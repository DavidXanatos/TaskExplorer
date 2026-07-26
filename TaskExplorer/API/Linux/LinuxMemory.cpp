#include "stdafx.h"
#include "LinuxMemory.h"
#include "LinuxMemIO.h"
#include "LinuxHelper.h"

CLinuxMemory::CLinuxMemory(QObject *parent)
	: CMemoryInfo(parent)
{
	m_Readable = false;
	m_Writable = false;
	m_Executable = false;
	m_Shared = false;
}

CLinuxMemory::~CLinuxMemory()
{
}

bool CLinuxMemory::InitStaticData(quint64 Pid, const ProcFs::SMapEntry& Entry)
{
	QWriteLocker Locker(&m_Mutex);

	m_ProcessId = Pid;
	m_BaseAddress = Entry.Start;
	m_RegionSize = Entry.End - Entry.Start;
	m_Path = Entry.Path;

	m_Readable = Entry.Read;
	m_Writable = Entry.Write;
	m_Executable = Entry.Exec;
	m_Shared = Entry.Shared;

	if (Entry.Path == "[heap]")
		m_RegionType = eHeap;
	else if (Entry.Path == "[stack]")
		m_RegionType = eStack;
	else if (Entry.Path == "[vdso]" || Entry.Path == "[vsyscall]" || Entry.Path == "[vvar]")
		m_RegionType = eVdso;
	else if (Entry.Inode != 0)
		m_RegionType = Entry.Exec ? eImage : eMapped;
	else
		m_RegionType = ePrivate;

	// Working set fields are filled in by SetDetail() when smaps is read.
	return true;
}

void CLinuxMemory::SetDetail(const ProcFs::SMapDetail& Detail)
{
	QWriteLocker Locker(&m_Mutex);

	m_TotalWorkingSet = Detail.Rss;
	m_PrivateWorkingSet = Detail.PrivateClean + Detail.PrivateDirty;
	m_SharedWorkingSet = Detail.SharedClean + Detail.SharedDirty;
	// What is resident but not exclusively ours.
	m_ShareableWorkingSet = (Detail.Rss > m_PrivateWorkingSet) ? (Detail.Rss - m_PrivateWorkingSet) : 0;
	m_LockedWorkingSet = Detail.Locked;

	//
	// Committed and private size have no exact Linux counterpart. Rss is what
	// is actually backed by physical memory, which is the closest useful
	// meaning for the "committed" column.
	//
	m_CommittedSize = Detail.Rss;
	m_PrivateSize = m_PrivateWorkingSet;
}

QString CLinuxMemory::GetTypeString() const
{
	QReadLocker Locker(&m_Mutex);
	switch (m_RegionType)
	{
		case eImage:	return tr("Image");
		case eMapped:	return tr("Mapped");
		case ePrivate:	return tr("Private");
		case eHeap:	return tr("Heap");
		case eStack:	return tr("Stack");
		case eVdso:	return tr("Kernel");
		default:	break;
	}
	return tr("Unknown");
}

QString CLinuxMemory::GetProtectionString() const
{
	QReadLocker Locker(&m_Mutex);

	QString Protection;
	Protection += m_Readable   ? "r" : "-";
	Protection += m_Writable   ? "w" : "-";
	Protection += m_Executable ? "x" : "-";
	Protection += m_Shared     ? "s" : "p";
	return Protection;
}

QString CLinuxMemory::GetAllocProtectionString() const
{
	// Linux does not record the protection a mapping was originally created
	// with, only its current one.
	return GetProtectionString();
}

bool CLinuxMemory::IsFree() const
{
	// /proc/<pid>/maps only lists mapped regions, so nothing enumerated here is
	// free. Gaps between entries are synthesised by the caller if needed.
	return false;
}

bool CLinuxMemory::IsExecutable() const
{
	QReadLocker Locker(&m_Mutex);
	return m_Executable;
}

bool CLinuxMemory::IsMapped() const
{
	QReadLocker Locker(&m_Mutex);
	return m_RegionType == eImage || m_RegionType == eMapped;
}

bool CLinuxMemory::IsPrivate() const
{
	QReadLocker Locker(&m_Mutex);
	return !m_Shared;
}

QString CLinuxMemory::GetUseString() const
{
	QReadLocker Locker(&m_Mutex);
	return m_Path;
}

STATUS CLinuxMemory::SetProtect(quint32 Protect)
{
	// mprotect only acts on the calling process; changing another process's
	// protection needs ptrace.
	return ERR(tr("Changing memory protection of another process is not supported on Linux."));
}

STATUS CLinuxMemory::DumpMemory(QIODevice* pFile)
{
	if (!pFile)
		return ERR(tr("No output file."));

	QReadLocker Locker(&m_Mutex);
	const quint64 BaseAddress = m_BaseAddress;
	const quint64 RegionSize = m_RegionSize;
	const quint64 ProcessId = m_ProcessId;
	Locker.unlock();

	CLinuxMemIO Reader(BaseAddress, RegionSize, ProcessId);
	if (!Reader.open(QIODevice::ReadOnly))
		return ERR(tr("Failed to open process memory. This usually means ptrace access was denied; see /proc/sys/kernel/yama/ptrace_scope."));

	// Copied in chunks so a large region does not have to be held in memory.
	const qint64 ChunkSize = 64 * 1024;
	QByteArray Chunk;
	quint64 Total = 0;

	while (Total < RegionSize)
	{
		Chunk = Reader.read(ChunkSize);
		if (Chunk.isEmpty())
			break; // hit an unreadable page, or the end

		if (pFile->write(Chunk) != Chunk.size())
			return ERR(tr("Failed to write the dump file."));

		Total += Chunk.size();
	}

	if (Total == 0)
		return ERR(tr("Failed to read process memory."));

	return OK;
}

STATUS CLinuxMemory::FreeMemory(bool Free)
{
	return ERR(tr("Freeing memory of another process is not supported on Linux."));
}

QIODevice* CLinuxMemory::MkDevice()
{
	QReadLocker Locker(&m_Mutex);
	return new CLinuxMemIO(m_BaseAddress, m_RegionSize, m_ProcessId);
}
