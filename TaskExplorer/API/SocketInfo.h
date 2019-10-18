#pragma once
#include <qobject.h>
#include "../Common/Common.h"
#include "../Common/FlexError.h"
#include "AbstractInfo.h"
#include "MiscStats.h"

#define NET_TYPE_NETWORK_IPV4		0x1
#define NET_TYPE_NETWORK_IPV6		0x2
#define NET_TYPE_NETWORK_MASK		0x3

#define NET_TYPE_PROTOCOL_TCP		0x10
#define NET_TYPE_PROTOCOL_UDP		0x20
#define NET_TYPE_PROTOCOL_MASK		0x30
#define NET_TYPE_PROTOCOL_TCP_SRV	0x40
#define NET_TYPE_PROTOCOL_OTHER		0x80

#define NET_TYPE_NONE 0x0
#define NET_TYPE_IPV4_TCP (NET_TYPE_NETWORK_IPV4 | NET_TYPE_PROTOCOL_TCP)
#define NET_TYPE_IPV6_TCP (NET_TYPE_NETWORK_IPV6 | NET_TYPE_PROTOCOL_TCP)
#define NET_TYPE_IPV4_UDP (NET_TYPE_NETWORK_IPV4 | NET_TYPE_PROTOCOL_UDP)
#define NET_TYPE_IPV6_UDP (NET_TYPE_NETWORK_IPV6 | NET_TYPE_PROTOCOL_UDP)

class CSocketInfo: public CAbstractInfoEx
{
	Q_OBJECT

public:
	CSocketInfo(QObject *parent = nullptr);
	virtual ~CSocketInfo();

	virtual quint64				GetHashID() const			{ QReadLocker Locker(&m_Mutex); return m_HashID; }

	virtual quint32				GetProtocolType() const		{ QReadLocker Locker(&m_Mutex); return m_ProtocolType; }
	virtual QString				GetProtocolString();
	virtual QHostAddress		GetLocalAddress() const		{ QReadLocker Locker(&m_Mutex); return m_LocalAddress; }
	virtual quint16				GetLocalPort() const		{ QReadLocker Locker(&m_Mutex); return m_LocalPort; }
	virtual QHostAddress		GetRemoteAddress() const	{ QReadLocker Locker(&m_Mutex); return m_RemoteAddress; }
	virtual quint16				GetRemotePort() const		{ QReadLocker Locker(&m_Mutex); return m_RemotePort; }
	virtual quint32				GetState() const			{ QReadLocker Locker(&m_Mutex); return m_State; }
	virtual void				SetClosed();
	virtual void				SetBlocked()				{ QWriteLocker Locker(&m_Mutex); m_State = -1; }
	virtual bool				WasBlocked() const			{ QReadLocker Locker(&m_Mutex); return m_State == -1; }
	virtual QString				GetStateString();
	virtual quint64				GetProcessId() const		{ QReadLocker Locker(&m_Mutex); return m_ProcessId; }

	virtual QString				GetRemoteHostName() const	{ QReadLocker Locker(&m_Mutex); return m_RemoteHostName; }

	virtual QString				GetProcessName() const		{ QReadLocker Locker(&m_Mutex); return m_ProcessName; }
	virtual QWeakPointer<QObject> GetProcess() const		{ QReadLocker Locker(&m_Mutex); return m_pProcess; }
	
	virtual SSockStats			GetStats() const			{ QReadLocker Locker(&m_StatsMutex); return m_Stats; }

	enum EMatchMode
	{
		eFuzzy = 0,
		eStrict,
	};

	virtual bool				Match(quint64 ProcessId, quint32 ProtocolType, const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort, EMatchMode Mode);

	static quint64				MkHash(quint64 ProcessId, quint32 ProtocolType, const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort);

	virtual STATUS				Close() = 0;

	virtual void				UpdateStats();

protected:
	quint64						m_HashID;

	quint32						m_ProtocolType;
	QHostAddress				m_LocalAddress;
	quint16						m_LocalPort;
	QHostAddress				m_RemoteAddress;
	quint16						m_RemotePort;
	quint32						m_State;
	quint64						m_ProcessId;

	QString						m_ProcessName;
	QWeakPointer<QObject>		m_pProcess;

	QString						m_RemoteHostName;

	// I/O stats
	mutable QReadWriteLock		m_StatsMutex;
	SSockStats					m_Stats;
};

typedef QSharedPointer<CSocketInfo> CSocketPtr;
typedef QWeakPointer<CSocketInfo> CSocketRef;