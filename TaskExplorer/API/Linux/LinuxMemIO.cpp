#include "stdafx.h"
#include "LinuxMemIO.h"
#include "LinuxHelper.h"
#include "ProcFs.h"
#include "../../../MiscHelpers/Common/Settings.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <unistd.h>

CLinuxMemIO::CLinuxMemIO(quint64 BaseAddress, quint64 RegionSize, quint64 ProcessId, QObject* parent)
	: QIODevice(parent)
{
	m_BaseAddress = BaseAddress;
	m_RegionSize = RegionSize;
	m_ProcessId = ProcessId;
	m_MemFd = -1;
	m_bUseHelper = false;
}

CLinuxMemIO::~CLinuxMemIO()
{
	close();
}

bool CLinuxMemIO::open(OpenMode flags)
{
	//
	// Two mechanisms, tried in order:
	//
	//   /proc/<pid>/mem supports pread/pwrite at arbitrary offsets and is the
	//   more capable of the two, but opening it needs PTRACE_MODE_ATTACH, which
	//   Yama's ptrace_scope=1 (the Ubuntu default) restricts to descendants.
	//
	//   process_vm_readv/writev need only PTRACE_MODE_ATTACH_REALCREDS and work
	//   without an open fd, so they are the fallback. They cannot write to a
	//   read-only mapping, where /proc/<pid>/mem can.
	//
	// Failing to open the fd is therefore not an error; readData() falls back.
	//
	const QByteArray Path = ProcFs::ProcPath(m_ProcessId, "mem").toLocal8Bit();

	int OpenFlags = O_RDONLY;
	if (flags & QIODevice::WriteOnly)
		OpenFlags = (flags & QIODevice::ReadOnly) ? O_RDWR : O_WRONLY;

	m_MemFd = ::open(Path.constData(), OpenFlags | O_CLOEXEC);

	if (m_MemFd == -1)
	{
		// Probe the fallback; if even a single byte cannot be read, this process
		// is out of local reach entirely.
		char Probe = 0;
		iovec Local = { &Probe, 1 };
		iovec Remote = { (void*)(uintptr_t)m_BaseAddress, 1 };
		const bool bLocalReadable = process_vm_readv((pid_t)m_ProcessId, &Local, 1, &Remote, 1, 0) >= 0;

		if (!bLocalReadable)
		{
			//
			// Out of reach locally. An elevated helper can still read it, but it
			// cannot write - so a write request fails here rather than appearing
			// to open and then failing on the first write.
			//
			if ((flags & QIODevice::WriteOnly) || !LinuxHelperNeeded()
			 || !theConf->GetBool("Options/UseTaskHelper", false))
				return false;

			m_bUseHelper = true;
		}
		else if (flags & QIODevice::WriteOnly)
		{
			//
			// Readable through process_vm_readv, which means process_vm_writev is
			// permitted too - but it cannot write a read-only mapping, where the
			// fd could have. Nothing more to check here; the write itself reports
			// that case.
			//
		}
	}

	return QIODevice::open(flags);
}

void CLinuxMemIO::close()
{
	if (m_MemFd != -1)
	{
		::close(m_MemFd);
		m_MemFd = -1;
	}
	QIODevice::close();
}

qint64 CLinuxMemIO::size() const
{
	return (qint64)m_RegionSize;
}

bool CLinuxMemIO::seek(qint64 pos)
{
	if (pos < 0 || pos > (qint64)m_RegionSize)
		return false;
	return QIODevice::seek(pos);
}

qint64 CLinuxMemIO::readData(char *data, qint64 maxlen)
{
	if (maxlen <= 0)
		return 0;

	// Never read past the end of the region we were handed.
	const qint64 Remaining = (qint64)m_RegionSize - pos();
	if (Remaining <= 0)
		return 0;
	if (maxlen > Remaining)
		maxlen = Remaining;

	const quint64 Address = m_BaseAddress + (quint64)pos();

	if (m_bUseHelper)
	{
		const QByteArray Read = LinuxHelperReadMemory(m_ProcessId, Address, (quint64)maxlen);
		if (Read.isEmpty())
			return -1;

		const qint64 Length = qMin<qint64>(maxlen, Read.size());
		memcpy(data, Read.constData(), (size_t)Length);
		return Length;
	}

	if (m_MemFd != -1)
	{
		const ssize_t Length = pread(m_MemFd, data, (size_t)maxlen, (off_t)Address);
		if (Length >= 0)
			return Length;
		// Fall through: an unreadable page (a PROT_NONE guard) fails here but
		// process_vm_readv reports it identically, so there is nothing to gain
		// by retrying - return the error.
		return -1;
	}

	iovec Local = { data, (size_t)maxlen };
	iovec Remote = { (void*)(uintptr_t)Address, (size_t)maxlen };
	const ssize_t Length = process_vm_readv((pid_t)m_ProcessId, &Local, 1, &Remote, 1, 0);
	return (Length < 0) ? -1 : Length;
}

qint64 CLinuxMemIO::writeData(const char *data, qint64 len)
{
	if (len <= 0)
		return 0;

	const qint64 Remaining = (qint64)m_RegionSize - pos();
	if (Remaining <= 0)
		return 0;
	if (len > Remaining)
		len = Remaining;

	const quint64 Address = m_BaseAddress + (quint64)pos();

	if (m_MemFd != -1)
	{
		const ssize_t Length = pwrite(m_MemFd, data, (size_t)len, (off_t)Address);
		return (Length < 0) ? -1 : Length;
	}

	iovec Local = { (void*)data, (size_t)len };
	iovec Remote = { (void*)(uintptr_t)Address, (size_t)len };
	const ssize_t Length = process_vm_writev((pid_t)m_ProcessId, &Local, 1, &Remote, 1, 0);
	return (Length < 0) ? -1 : Length;
}
