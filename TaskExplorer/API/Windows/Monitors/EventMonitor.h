#pragma once

class CEventMonitor : public QThread
{
    Q_OBJECT

public:
	CEventMonitor(QObject *parent = nullptr);
    virtual ~CEventMonitor();

	bool		Init();

	bool		IsRunning() { return m_bRunning; }

signals:
	void		NetworkEvent(int Type, quint64 ProcessId, quint64 ThreadId, quint32 ProtocolType, quint32 TransferSize,
								const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort);
	void		DnsResEvent(quint64 ProcessId, quint64 ThreadId, const QString& HostName, const QStringList& Results);
	void		FileEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, const QString& FileName);
	void		DiskEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, quint32 IrpFlags, quint32 TransferSize, quint64 HighResResponseTime);
	void		ProcessEvent(int Type, quint32 ProcessId, QString CommandLine, QString FileName, quint32 ParentId, quint64 TimeStamp);

protected:
	virtual void run();

	bool		m_bRunning;
	bool		m_bRundownMode;

	CEventMonitor*	m_pMonitorRundown;

private:
	bool		StartSession();
	quint32		ControlSession(quint32 ControlCode);
	void		FlushSession();
	void		StopSession();

	struct SEventMonitor* m;
};
