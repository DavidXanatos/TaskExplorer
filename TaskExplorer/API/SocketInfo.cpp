#include "stdafx.h"
#include "SocketInfo.h"
#include "../GUI/TaskExplorer.h"


CSocketInfo::CSocketInfo(QObject *parent) : CAbstractInfoEx(parent)
{
	m_HashID = -1;

	m_ProtocolType = 0;;
	m_LocalPort = 0;
	m_RemotePort = 0;
	m_State = 0;
	m_ProcessId = -1;

}

CSocketInfo::~CSocketInfo()
{
}

bool CSocketInfo::Match(quint64 ProcessId, ulong ProtocolType, const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort, bool bStrict)
{
	QReadLocker Locker(&m_Mutex); 

	if (m_ProcessId != ProcessId)
		return false;

	if (m_ProtocolType != ProtocolType)
		return false;

	if ((m_ProtocolType & (PH_TCP_PROTOCOL_TYPE | PH_UDP_PROTOCOL_TYPE)) != 0)
	{
		if (m_LocalPort != LocalPort)
			return false;
	}

	// a socket may be bount to all adapters than it has a local null address
	if (bStrict || m_LocalAddress != QHostAddress::AnyIPv4)
	{
		if(m_LocalAddress != LocalAddress)
			return false;
	}

	// don't test the remote endpoint if this is a udp socket
	if (bStrict || (m_ProtocolType & PH_TCP_PROTOCOL_TYPE) != 0)
	{
		if ((m_ProtocolType & (PH_TCP_PROTOCOL_TYPE | PH_UDP_PROTOCOL_TYPE)) != 0)
		{
			if (m_RemotePort != RemotePort)
				return false;
		}

		if (m_RemoteAddress != RemoteAddress)
			return false;
	}

	return true;
}

quint64 CSocketInfo::MkHash(quint64 ProcessId, ulong ProtocolType, const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort)
{
	if ((ProtocolType & PH_UDP_PROTOCOL_TYPE) != 0)
		RemotePort = 0;

	quint64 HashID = ((quint64)LocalPort << 0) | ((quint64)RemotePort << 16) | (quint64)(((quint32*)&ProcessId)[0] ^ ((quint32*)&ProcessId)[1]) << 32;

	return HashID;
}

void CSocketInfo::UpdateStats()
{
	QWriteLocker Locker(&m_StatsMutex);
	m_Stats.UpdateStats();
}

QString CSocketInfo::GetProtocolString()
{
	QReadLocker Locker(&m_Mutex);
    switch (m_ProtocolType)
    {
		case PH_TCP4_NETWORK_PROTOCOL:	return tr("TCP");
		case PH_TCP6_NETWORK_PROTOCOL:	return tr("TCP6");
		case PH_UDP4_NETWORK_PROTOCOL:	return tr("UDP");
		case PH_UDP6_NETWORK_PROTOCOL:	return tr("UDP6");
		default:						return tr("Unknown");
    }
}

#ifndef MIB_TCP_STATE
typedef enum {
    MIB_TCP_STATE_CLOSED     =  1,
    MIB_TCP_STATE_LISTEN     =  2,
    MIB_TCP_STATE_SYN_SENT   =  3,
    MIB_TCP_STATE_SYN_RCVD   =  4,
    MIB_TCP_STATE_ESTAB      =  5,
    MIB_TCP_STATE_FIN_WAIT1  =  6,
    MIB_TCP_STATE_FIN_WAIT2  =  7,
    MIB_TCP_STATE_CLOSE_WAIT =  8,
    MIB_TCP_STATE_CLOSING    =  9,
    MIB_TCP_STATE_LAST_ACK   = 10,
    MIB_TCP_STATE_TIME_WAIT  = 11,
    MIB_TCP_STATE_DELETE_TCB = 12,
    //
    // Extra TCP states not defined in the MIB
    //
    MIB_TCP_STATE_RESERVED      = 100
} MIB_TCP_STATE;
#endif

QString CSocketInfo::GetStateString()
{
	QReadLocker Locker(&m_Mutex);

	if ((m_ProtocolType & PH_TCP_PROTOCOL_TYPE) == 0)
		return tr("Open");

	// all these are TCP states
    switch (m_State)
    {
    case MIB_TCP_STATE_CLOSED:		return tr("Closed");
    case MIB_TCP_STATE_LISTEN:		return tr("Listen");
    case MIB_TCP_STATE_SYN_SENT:	return tr("SYN sent");
    case MIB_TCP_STATE_SYN_RCVD:	return tr("SYN received");
    case MIB_TCP_STATE_ESTAB:		return tr("Established");
    case MIB_TCP_STATE_FIN_WAIT1:	return tr("FIN wait 1");
    case MIB_TCP_STATE_FIN_WAIT2:	return tr("FIN wait 2");
    case MIB_TCP_STATE_CLOSE_WAIT:	return tr("Close wait");
    case MIB_TCP_STATE_CLOSING:		return tr("Closing");
    case MIB_TCP_STATE_LAST_ACK:	return tr("Last ACK");
    case MIB_TCP_STATE_TIME_WAIT:	return tr("Time wait");
    case MIB_TCP_STATE_DELETE_TCB:	return tr("Delete TCB");
    default:						return tr("Unknown");
    }
}
