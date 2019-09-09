#pragma once

class CFwEventMonitor : public QObject//QThread
{
    Q_OBJECT

public:
	CFwEventMonitor(QObject *parent = nullptr);
    virtual ~CFwEventMonitor();

	bool		Init();

	//bool		IsRunning() { return m_bRunning; }

signals:
	void		NetworkEvent(int Type, quint64 ProcessId, quint64 ThreadId, quint32 ProtocolType, quint32 TransferSize,
								const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort);

protected:
	//virtual void run();

	struct SFwEventMonitor* M() { return m; }

	bool		m_bRunning;

private:
	bool StartLogMonitor();
	void StopLogMonitor();

	struct SFwEventMonitor* m;
};
