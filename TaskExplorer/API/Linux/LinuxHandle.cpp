#include "stdafx.h"
#include "LinuxHandle.h"
#include "LinuxHelper.h"
#include "ProcFs.h"

#include <QFileInfo>

#include <fcntl.h>

//
// The open(2) flags CHandleInfo names are this platform's own.
//
static_assert(CHandleInfo::eOpenAccessMask   == O_ACCMODE,   "open access mask");
static_assert(CHandleInfo::eOpenReadOnly     == O_RDONLY,    "open read only");
static_assert(CHandleInfo::eOpenWriteOnly    == O_WRONLY,    "open write only");
static_assert(CHandleInfo::eOpenReadWrite    == O_RDWR,      "open read write");
static_assert(CHandleInfo::eOpenCloseOnExec  == O_CLOEXEC,   "open close on exec");
static_assert(CHandleInfo::eOpenAppend       == O_APPEND,    "open append");
static_assert(CHandleInfo::eOpenNonBlock     == O_NONBLOCK,  "open non-blocking");
static_assert(CHandleInfo::eOpenDSync        == O_DSYNC,     "open data sync");
static_assert(CHandleInfo::eOpenAsync        == O_ASYNC,     "open async");
static_assert(CHandleInfo::eOpenDirect       == O_DIRECT,    "open direct");
static_assert(CHandleInfo::eOpenDirectory    == O_DIRECTORY, "open directory");
static_assert(CHandleInfo::eOpenNoAtime      == O_NOATIME,   "open no atime");
static_assert(CHandleInfo::eOpenPath         == O_PATH,      "open path");
static_assert(CHandleInfo::eOpenSync         == O_SYNC,      "open sync");


CLinuxHandle::CLinuxHandle(QObject *parent)
	: CHandleInfo(parent)
{
	m_Type = eUnknown;
	m_Inode = 0;
	m_Flags = 0;
}

CLinuxHandle::~CLinuxHandle()
{
}

bool CLinuxHandle::InitStaticData(quint64 Pid, quint64 Fd)
{
	//
	// The fd symlink resolves either to a path (regular files, directories,
	// devices) or to a pseudo target naming the kernel object kind:
	//   socket:[12345]  pipe:[12345]  anon_inode:[eventfd]  anon_inode:inotify
	// The number in brackets is the inode, which is how a socket fd is later
	// joined against the sock_diag/proc/net tables.
	//
	const QString Target = ProcFs::ReadLink(ProcFs::ProcPath(Pid, QString("fd/%1").arg(Fd)));
	if (Target.isEmpty())
		return false; // closed already, or not permitted

	// fdinfo gives the current offset and the open flags.
	const QByteArray FdInfo = ProcFs::ReadFile(ProcFs::ProcPath(Pid, QString("fdinfo/%1").arg(Fd)));

	return InitStaticData(Pid, Fd, Target, FdInfo);
}

bool CLinuxHandle::InitStaticData(quint64 Pid, quint64 Fd, const QString& Target, const QByteArray& FdInfo)
{
	if (Target.isEmpty())
		return false;

	EHandleType Type = eUnknown;
	quint64 Inode = 0;

	auto BracketInode = [](const QString& Text) -> quint64 {
		const int Open = Text.indexOf('[');
		const int Close = Text.lastIndexOf(']');
		if (Open < 0 || Close <= Open)
			return 0;
		return Text.mid(Open + 1, Close - Open - 1).toULongLong();
	};

	if (Target.startsWith("socket:"))
	{
		Type = eSocket;
		Inode = BracketInode(Target);
	}
	else if (Target.startsWith("pipe:"))
	{
		Type = ePipe;
		Inode = BracketInode(Target);
	}
	else if (Target.startsWith("anon_inode:"))
	{
		Type = eAnonInode;
	}
	else
	{
		// A real path. Statting it says whether it is a directory or a device;
		// this can fail for a deleted file, which keeps eFile.
		Type = eFile;
		QFileInfo Info(Target);
		if (Info.isDir())
			Type = eDirectory;
		else if (Target.startsWith("/dev/"))
			Type = eCharDevice;
	}

	quint64 Position = 0;
	quint32 Flags = 0;
	for (const QByteArray& Line : FdInfo.split('\n'))
	{
		const int Sep = Line.indexOf(':');
		if (Sep < 0)
			continue;
		const QByteArray Key = Line.left(Sep).trimmed();
		const QByteArray Value = Line.mid(Sep + 1).trimmed();

		if (Key == "pos")
			Position = Value.toULongLong();
		else if (Key == "flags")
			Flags = Value.toUInt(nullptr, 8); // octal, as the kernel prints it
		else if (Key == "ino" && Inode == 0)
			Inode = Value.toULongLong();
	}

	QWriteLocker Locker(&m_Mutex);

	m_ProcessId = Pid;
	m_HandleId = Fd;
	m_FileName = Target;
	m_Type = Type;
	m_Inode = Inode;
	m_Flags = Flags;
	m_Position = Position;

	// Only meaningful for regular files; a socket or pipe has no size.
	if (Type == eFile)
	{
		QFileInfo Info(Target);
		if (Info.exists())
			m_Size = Info.size();
	}

	return true;
}

bool CLinuxHandle::UpdateDynamicData()
{
	const quint64 Pid = GetProcessId();
	const quint64 Fd = GetHandleId();

	return UpdateDynamicData(ProcFs::ReadFile(ProcFs::ProcPath(Pid, QString("fdinfo/%1").arg(Fd))));
}

bool CLinuxHandle::UpdateDynamicData(const QByteArray& FdInfo)
{
	quint64 Position = 0;
	if (FdInfo.isEmpty())
		return false;

	for (const QByteArray& Line : FdInfo.split('\n'))
	{
		if (!Line.startsWith("pos:"))
			continue;
		Position = Line.mid(4).trimmed().toULongLong();
		break;
	}

	QWriteLocker Locker(&m_Mutex);
	const bool bChanged = (m_Position != Position);
	m_Position = Position;
	return bChanged;
}

quint32 CLinuxHandle::GetTypeIndex() const
{
	QReadLocker Locker(&m_Mutex);
	return (quint32)m_Type;
}

QString CLinuxHandle::GetTypeName() const
{
	QReadLocker Locker(&m_Mutex);
	switch (m_Type)
	{
		case eFile:		return "File";
		case eDirectory:	return "Directory";
		case eSocket:		return "Socket";
		case ePipe:		return "Pipe";
		case eAnonInode:	return "AnonInode";
		case eCharDevice:	return "CharDevice";
		case eBlockDevice:	return "BlockDevice";
		default:		break;
	}
	return "Unknown";
}
quint32 CLinuxHandle::GetGrantedAccess() const
{
	QReadLocker Locker(&m_Mutex);
	return m_Flags;
}
STATUS CLinuxHandle::Close(bool bForce)
{
	// Linux has no equivalent of DuplicateHandle(DUPLICATE_CLOSE_SOURCE); an fd
	// belonging to another process can only be closed by ptrace-attaching and
	// issuing close() in its context.
	return ERR(TE_ClosingFileDesc);
}
