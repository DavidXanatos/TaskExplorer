#pragma once

class CFirewallMonitor : public QObject//QThread
{
    Q_OBJECT

public:
	CFirewallMonitor(QObject *parent = nullptr);
    virtual ~CFirewallMonitor();

	bool		Init();

	//bool		IsRunning() { return m_bRunning; }

signals:
	void		NetworkEvent(int Type, quint64 ProcessId, quint64 ThreadId, quint32 ProtocolType, quint32 TransferSize,
								const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort);

protected:
	friend void __stdcall FwMonEventCallback(void* FwContext, const struct FWPM_NET_EVENT5_* FwEvent);
	//virtual void run();

	struct SFirewallMonitor* M() { return m; }

	bool		m_bRunning;

private:
	bool StartFwMonitor();
	void StopFwMonitor();

	struct SFirewallMonitor* m;
};
