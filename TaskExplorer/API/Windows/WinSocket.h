#pragma once
#include "../SocketInfo.h"
#include "../DnsEntry.h"

#define PH_NETWORK_OWNER_INFO_SIZE 16

class CWinSocket : public CSocketInfo
{
	Q_OBJECT
public:
	CWinSocket(QObject *parent = nullptr);
	virtual ~CWinSocket();

	//static QHostAddress PH2QAddress(struct _PH_IP_ADDRESS* addr);

	virtual QString			GetOwnerServiceName()	{ QReadLocker Locker(&m_Mutex); return m_OwnerService; }
	//virtual bool			IsSubsystemProcess()	{ QReadLocker Locker(&m_Mutex); return m_SubsystemProcess; }

	virtual int				GetFirewallStatus();
	virtual QString			GetFirewallStatusString();

	virtual quint64			GetIdleTime() const;

	virtual bool			HasStaticDataEx() const { QReadLocker Locker(&m_Mutex); return m_HasStaticDataEx; }

	virtual STATUS			Close();

	struct SSocket
	{
		struct SEndPoint
		{
			QHostAddress Address;
			quint16 Port;
		};
		quint32 ProtocolType;
		SEndPoint LocalEndpoint;
		SEndPoint RemoteEndpoint;
		quint32 State;
		quint64 ProcessId;
		quint64 CreateTime;
		quint64 OwnerInfo[PH_NETWORK_OWNER_INFO_SIZE];
		quint32 LocalScopeId; // Ipv6
		quint32 RemoteScopeId; // Ipv6
	};

	static QVector<SSocket> GetNetworkConnections();

public slots:
	void			OnHostResolved(const QHostAddress& Address, const QString& HostName);

protected:
	friend class CWindowsAPI;

	bool InitStaticData(quint64 ProcessId, quint32 ProtocolType,
		const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort);

	bool InitStaticDataEx(SSocket* connection, bool IsNew);

	bool UpdateDynamicData(SSocket* connection);

	void AddNetworkIO(int Type, quint32 TransferSize);

	void			ProcessSetNetworkFlag();

	QString			m_OwnerService;
	//bool			m_SubsystemProcess;

	quint64			m_LastActivity;

	bool			m_HasStaticDataEx;

private:
	struct SWinSocket* m;
};

QVariant SvcApiCloseSocket(const QVariantMap& Parameters);