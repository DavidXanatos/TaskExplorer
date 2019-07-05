#include "stdafx.h"
#include "WinMemIO.h"

#include "ProcessHacker.h"

struct SWinMemIO
{
	SWinMemIO()
	{
		BaseAddress = 0;
		RegionSize = 0;
		MemPosition = 0;
		ProcessId = NULL;
		ProcessHandle = NULL;
	}

	quint64 BaseAddress;
	quint64 RegionSize;
	quint64 MemPosition;
	HANDLE ProcessId;
	HANDLE ProcessHandle;
};

CWinMemIO::CWinMemIO(CWinMemory* pMemory, QObject* parent)
	: QIODevice(parent)
{
	m = new SWinMemIO;

	m->BaseAddress = pMemory->GetBaseAddress();
	m->RegionSize = pMemory->GetRegionSize();
	m->ProcessId = (HANDLE)pMemory->GetProcessId();
}

CWinMemIO::~CWinMemIO()
{
	if (m->ProcessHandle)
		close();
	delete m;
}

bool CWinMemIO::open(OpenMode flags)
{
	ACCESS_MASK DesiredAccess = 0;
	if (flags & QIODevice::ReadOnly)
		DesiredAccess = PROCESS_VM_READ;
	if (flags & QIODevice::WriteOnly)
		DesiredAccess = PROCESS_VM_READ | PROCESS_VM_WRITE;

	NTSTATUS status;
	if (!NT_SUCCESS(status = PhOpenProcess(&m->ProcessHandle, DesiredAccess, m->ProcessId))) {
		qDebug() << QString("CWinMemIO::open failed: 0x%1").arg((quint32)status, 8, 16, QChar('0'));
		return false;
	}

	m->MemPosition = 0;
	return QIODevice::open(flags);
}

void CWinMemIO::close()
{
	if (m->ProcessHandle) {
		NtClose(m->ProcessHandle);
		m->ProcessHandle = NULL;
	}
	QIODevice::close();
}

qint64 CWinMemIO::size() const
{
	return m->RegionSize;
}

bool CWinMemIO::seek(qint64 pos)
{
	m->MemPosition = pos;
	return QIODevice::seek(pos);
}

qint64 CWinMemIO::readData(char *data, qint64 maxlen)
{
	if (m->MemPosition + maxlen > m->RegionSize)
		maxlen = m->RegionSize - m->MemPosition;

	NTSTATUS status;
	if (!NT_SUCCESS(status = NtReadVirtualMemory(m->ProcessHandle, PTR_ADD_OFFSET((PVOID)m->BaseAddress, m->MemPosition), data, maxlen, NULL))) {
		qDebug() << QString("CWinMemIO::readData failed: 0x%1").arg((quint32)status, 8, 16, QChar('0'));
		return -1;
	}

	m->MemPosition += maxlen;

	return maxlen;
}

qint64 CWinMemIO::writeData(const char *data, qint64 len)
{
	NTSTATUS status;
	if (!NT_SUCCESS(status = NtWriteVirtualMemory(m->ProcessHandle, PTR_ADD_OFFSET((PVOID)m->BaseAddress, m->MemPosition), (char*)data, len, NULL))) {
		qDebug() << QString("CWinMemIO::writeData failed: 0x%1").arg((quint32)status, 8, 16, QChar('0'));
		return -1;
	}
	
	m->MemPosition += len;

	return len;
}