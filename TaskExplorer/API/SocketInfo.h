#pragma once
#include <qobject.h>
#include "ProcessInfo.h"
#include "../Common/Common.h"
#include "AbstractInfo.h"

#ifndef PH_NO_NETWORK_PROTOCOL
#define PH_IPV4_NETWORK_TYPE 0x1
#define PH_IPV6_NETWORK_TYPE 0x2
#define PH_NETWORK_TYPE_MASK 0x3

#define PH_TCP_PROTOCOL_TYPE 0x10
#define PH_UDP_PROTOCOL_TYPE 0x20
#define PH_PROTOCOL_TYPE_MASK 0x30

#define PH_NO_NETWORK_PROTOCOL 0x0
#define PH_TCP4_NETWORK_PROTOCOL (PH_IPV4_NETWORK_TYPE | PH_TCP_PROTOCOL_TYPE)
#define PH_TCP6_NETWORK_PROTOCOL (PH_IPV6_NETWORK_TYPE | PH_TCP_PROTOCOL_TYPE)
#define PH_UDP4_NETWORK_PROTOCOL (PH_IPV4_NETWORK_TYPE | PH_UDP_PROTOCOL_TYPE)
#define PH_UDP6_NETWORK_PROTOCOL (PH_IPV6_NETWORK_TYPE | PH_UDP_PROTOCOL_TYPE)
#endif

class CSocketInfo: public CAbstractInfoEx
{
	Q_OBJECT

public:
	CSocketInfo(QObject *parent = nullptr);
	virtual ~CSocketInfo();

	virtual quint64				GetHashID() const			{ QReadLocker Locker(&m_Mutex); return m_HashID; }

	virtual ulong				GetProtocolType() const		{ QReadLocker Locker(&m_Mutex); return m_ProtocolType; }
	virtual QString				GetProtocolString();
	virtual QHostAddress		GetLocalAddress() const		{ QReadLocker Locker(&m_Mutex); return m_LocalAddress; }
	virtual quint16				GetLocalPort() const		{ QReadLocker Locker(&m_Mutex); return m_LocalPort; }
	virtual QHostAddress		GetRemoteAddress() const	{ QReadLocker Locker(&m_Mutex); return m_RemoteAddress; }
	virtual quint16				GetRemotePort() const		{ QReadLocker Locker(&m_Mutex); return m_RemotePort; }
	virtual ulong				GetState() const			{ QReadLocker Locker(&m_Mutex); return m_State; }
	virtual QString				GetStateString();
	virtual quint64				GetProcessId() const		{ QReadLocker Locker(&m_Mutex); return m_ProcessId; }

	virtual QString				GetProcessName() const		{ QReadLocker Locker(&m_Mutex); return m_ProcessName; }
	virtual CProcessPtr			GetProcess() const			{ QReadLocker Locker(&m_Mutex); return m_pProcess; }
	
	virtual SSockStats			GetStats() const			{ QReadLocker Locker(&m_StatsMutex); return m_Stats; }


	virtual bool				Match(quint64 ProcessId, ulong ProtocolType, const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort, bool bStrict);

	static quint64				MkHash(quint64 ProcessId, ulong ProtocolType, const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort);

	virtual STATUS				Close() = 0;

protected:
	virtual void				UpdateStats();

	quint64						m_HashID;

	ulong						m_ProtocolType;
	QHostAddress				m_LocalAddress;
	quint16						m_LocalPort;
	QHostAddress				m_RemoteAddress;
	quint16						m_RemotePort;
	ulong						m_State;
	quint64						m_ProcessId;

	QString						m_ProcessName;
	CProcessRef					m_pProcess;


	// I/O stats
	mutable QReadWriteLock		m_StatsMutex;
	SSockStats					m_Stats;
};

typedef QSharedPointer<CSocketInfo> CSocketPtr;
typedef QWeakPointer<CSocketInfo> CSocketRef;