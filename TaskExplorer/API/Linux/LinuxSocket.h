#pragma once
#include "../SocketInfo.h"
#include "ProcFs.h"

//
// A network endpoint. Sourced from the sock_diag netlink interface where
// available (it reports the owning socket inode and per-socket counters),
// falling back to /proc/net/{tcp,tcp6,udp,udp6}.
//
// Ownership is resolved by matching the socket inode against the inodes behind
// each process's /proc/<pid>/fd entries.
//
class CLinuxSocket : public CSocketInfo
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxSocket)
public:
	CLinuxSocket(QObject *parent = nullptr);
	virtual ~CLinuxSocket();

	virtual bool			InitStaticData(quint64 ProcessId, const ProcFs::SNetConnection& Conn);
	virtual bool			UpdateDynamicData(const ProcFs::SNetConnection& Conn);

	virtual void			LinkProcess(const QSharedPointer<QObject>& pProcess);

	virtual STATUS			Close();

	virtual quint64			GetInode() const	{ QReadLocker Locker(&m_Mutex); return m_Inode; }

protected:
	quint64					m_Inode;
	quint32					m_Uid;
};

typedef QSharedPointer<CLinuxSocket> CLinuxSocketPtr;
typedef QWeakPointer<CLinuxSocket> CLinuxSocketRef;
