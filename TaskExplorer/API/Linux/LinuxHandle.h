#pragma once
#include "../HandleInfo.h"

//
// An open file descriptor, from /proc/<pid>/fd/<n> and fdinfo/<n>.
//
// "Handle type" on Linux is derived from what the fd symlink resolves to:
// a path for regular files/directories, or an anon_inode:/socket:/pipe: prefix
// for the kernel object kinds.
//
class CLinuxHandle : public CHandleInfo
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxHandle)
public:
	CLinuxHandle(QObject *parent = nullptr);
	virtual ~CLinuxHandle();

	enum EHandleType
	{
		eUnknown = 0,
		eFile,
		eDirectory,
		eSocket,
		ePipe,
		eAnonInode,
		eCharDevice,
		eBlockDevice,
		eMax
	};

	virtual bool			InitStaticData(quint64 Pid, quint64 Fd);
	virtual bool			UpdateDynamicData();

	//
	// The same two, for fd data that was fetched by an elevated TaskHelper rather
	// than read here.
	//
	// Splitting the reading from the interpreting keeps one copy of the
	// classification rules; the helper only ever ships back the raw symlink target
	// and the raw fdinfo text, exactly as /proc presents them.
	//
	virtual bool			InitStaticData(quint64 Pid, quint64 Fd, const QString& Target, const QByteArray& FdInfo);
	virtual bool			UpdateDynamicData(const QByteArray& FdInfo);

	virtual quint32			GetTypeIndex() const;
	virtual QString			GetTypeName() const;
	virtual QString			GetTypeString() const;
	virtual quint32			GetGrantedAccess() const;
	virtual QString			GetGrantedAccessString() const;

	virtual STATUS			Close(bool bForce = false);

	// The inode behind a socket fd, used to join /proc/<pid>/fd against
	// /proc/net/* and the sock_diag netlink results.
	virtual quint64			GetInode() const	{ QReadLocker Locker(&m_Mutex); return m_Inode; }

protected:
	EHandleType				m_Type;
	quint64					m_Inode;
	quint32					m_Flags;	// O_RDONLY/O_WRONLY/... from fdinfo
};

typedef QSharedPointer<CLinuxHandle> CLinuxHandlePtr;
typedef QWeakPointer<CLinuxHandle> CLinuxHandleRef;
