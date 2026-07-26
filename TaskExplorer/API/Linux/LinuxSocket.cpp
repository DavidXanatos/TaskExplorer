#include "stdafx.h"
#include "LinuxSocket.h"
#include "LinuxHelper.h"
#include "../ProcessInfo.h"

CLinuxSocket::CLinuxSocket(QObject *parent)
	: CSocketInfo(parent)
{
	m_Inode = 0;
	m_Uid = 0;
}

CLinuxSocket::~CLinuxSocket()
{
}

bool CLinuxSocket::InitStaticData(quint64 ProcessId, const ProcFs::SNetConnection& Conn)
{
	QWriteLocker Locker(&m_Mutex);

	m_ProcessId = ProcessId;
	m_ProtocolType = Conn.ProtocolType;
	m_LocalAddress = Conn.LocalAddress;
	m_LocalPort = Conn.LocalPort;
	m_RemoteAddress = Conn.RemoteAddress;
	m_RemotePort = Conn.RemotePort;

	m_Inode = Conn.Inode;
	m_Uid = Conn.Uid;

	// The hash keys the socket list; it must be derived from exactly the same
	// tuple that FindSocketEntry() looks the socket up by.
	m_HashID = MkHash(ProcessId, Conn.ProtocolType, Conn.LocalAddress, Conn.LocalPort,
	                  Conn.RemoteAddress, Conn.RemotePort);

	return true;
}

void CLinuxSocket::LinkProcess(const QSharedPointer<QObject>& pProcess)
{
	QWriteLocker Locker(&m_Mutex);
	m_pProcess = pProcess.toWeakRef();
	if (CProcessInfo* pInfo = qobject_cast<CProcessInfo*>(pProcess.data()))
		m_ProcessName = pInfo->GetName();
}

bool CLinuxSocket::UpdateDynamicData(const ProcFs::SNetConnection& Conn)
{
	QWriteLocker Locker(&m_Mutex);

	const bool bChanged = (m_State != Conn.State);
	m_State = Conn.State;

	Locker.unlock();

	//
	// Cumulative counters from sock_diag's tcp_info. SNetStats turns them into
	// deltas and rates, so they must be set before UpdateStats().
	//
	// Only TCP sockets carry a tcp_info, and the /proc/net fallback carries no
	// counters at all - in either case bHaveCounters is false and the totals
	// are left alone, so the rates stay at zero rather than being computed from
	// a bogus baseline.
	//
	if (Conn.bHaveCounters)
	{
		QWriteLocker StatsLocker(&m_StatsMutex);
		m_Stats.Net.SetReceive(Conn.BytesReceived, Conn.SegmentsReceived);
		m_Stats.Net.SetSend(Conn.BytesSent, Conn.SegmentsSent);
	}

	UpdateStats();

	return bChanged;
}

STATUS CLinuxSocket::Close()
{
	// linux-todo: SOCK_DESTROY via netlink closes a TCP socket, but only for
	// root and only for TCP.
	return ERR(tr("Closing a socket is not yet implemented on Linux."));
}
