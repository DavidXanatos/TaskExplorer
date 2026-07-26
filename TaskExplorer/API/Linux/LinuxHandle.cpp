#include "stdafx.h"
#include "LinuxHandle.h"
#include "LinuxHelper.h"
#include "ProcFs.h"

#include <QFileInfo>

#include <fcntl.h>

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

QString CLinuxHandle::GetTypeString() const
{
	QReadLocker Locker(&m_Mutex);
	switch (m_Type)
	{
		case eFile:		return tr("File");
		case eDirectory:	return tr("Directory");
		case eSocket:		return tr("Socket");
		case ePipe:		return tr("Pipe");
		case eAnonInode:	return tr("Anonymous Inode");
		case eCharDevice:	return tr("Character Device");
		case eBlockDevice:	return tr("Block Device");
		default:		break;
	}
	return tr("Unknown");
}

quint32 CLinuxHandle::GetGrantedAccess() const
{
	QReadLocker Locker(&m_Mutex);
	return m_Flags;
}

QString CLinuxHandle::GetGrantedAccessString() const
{
	QReadLocker Locker(&m_Mutex);

	QStringList Parts;

	//
	// The access mode is a two-bit field rather than a set of flags, so it is
	// switched on rather than tested.
	//
	// O_PATH is checked first because it changes what the others mean: such a
	// descriptor refers to a location in the filesystem and cannot read or write
	// at all, yet it has O_RDONLY's value of 0 in the access-mode field.
	//
	if (m_Flags & O_PATH)
	{
		Parts.append(tr("Path only"));
	}
	else
	{
		switch (m_Flags & O_ACCMODE)
		{
			case O_RDONLY:	Parts.append(tr("Read")); break;
			case O_WRONLY:	Parts.append(tr("Write")); break;
			case O_RDWR:	Parts.append(tr("Read/Write")); break;
		}
	}

	//
	// The remaining file status flags, in the order most likely to matter when
	// looking at what a process has open. Flags that only affected the original
	// open() call and are not retained by the kernel (O_CREAT, O_EXCL, O_TRUNC,
	// O_NOCTTY, O_NOFOLLOW) never appear in fdinfo, so they are not listed.
	//
	struct { int Flag; const char* Name; } static const Flags[] =
	{
		{ O_APPEND,		QT_TR_NOOP("Append")		},
		{ O_NONBLOCK,	QT_TR_NOOP("Non-blocking")	},
		{ O_DIRECT,		QT_TR_NOOP("Direct")		},	// bypasses the page cache
		{ O_SYNC,		QT_TR_NOOP("Sync")			},	// implies O_DSYNC, so test it first
		{ O_DSYNC,		QT_TR_NOOP("Data sync")		},
		{ O_ASYNC,		QT_TR_NOOP("Async")			},	// SIGIO on readiness
		{ O_NOATIME,	QT_TR_NOOP("No atime")		},
		{ O_DIRECTORY,	QT_TR_NOOP("Directory")		},
		{ O_CLOEXEC,	QT_TR_NOOP("Close on exec")	},
	};

	for (size_t i = 0; i < sizeof(Flags) / sizeof(Flags[0]); i++)
	{
		//
		// O_SYNC is O_DSYNC|__O_SYNC on Linux, so a plain bit test would report
		// "Sync, Data sync" for a single flag. Matching the whole value avoids
		// that for any such composite.
		//
		if ((m_Flags & Flags[i].Flag) == Flags[i].Flag)
		{
			if (Flags[i].Flag == O_DSYNC && (m_Flags & O_SYNC) == O_SYNC)
				continue;
			Parts.append(tr(Flags[i].Name));
		}
	}

	return Parts.join(", ");
}

STATUS CLinuxHandle::Close(bool bForce)
{
	// Linux has no equivalent of DuplicateHandle(DUPLICATE_CLOSE_SOURCE); an fd
	// belonging to another process can only be closed by ptrace-attaching and
	// issuing close() in its context.
	return ERR(tr("Closing a file descriptor of another process is not supported on Linux."));
}
