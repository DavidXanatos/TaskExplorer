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
	void		FileEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, const QString& FileName);
	void		DiskEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, quint32 IrpFlags, quint32 TransferSize, quint64 HighResResponseTime);

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
