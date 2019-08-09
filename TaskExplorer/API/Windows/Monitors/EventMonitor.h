#pragma once

enum ET_ETW_EVENT_TYPE
{
	EtEtwDiskReadType = 1,
	EtEtwDiskWriteType,
	EtEtwFileNameType,
	EtEtwFileCreateType,
	EtEtwFileDeleteType,
	EtEtwFileRundownType,
	EtEtwNetworkReceiveType,
	EtEtwNetworkSendType,
	EtEtwUnknow = ULONG_MAX
};

class CEventMonitor : public QThread
{
    Q_OBJECT

public:
	CEventMonitor(QObject *parent = nullptr);
    virtual ~CEventMonitor();

	bool		Init();

	bool		IsRunning() { return m_bRunning; }

signals:
	void		NetworkEvent(int Type, quint64 ProcessId, quint64 ThreadId, ulong ProtocolType, ulong TransferSize,
								const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort);
	void		FileEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, const QString& FileName);
	void		DiskEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, ulong IrpFlags, ulong TransferSize, quint64 HighResResponseTime);

protected:
	virtual void run();

	bool		m_bRunning;
	bool		m_bRundownMode;

	CEventMonitor*	m_pMonitorRundown;

private:
	bool		StartSession();
	ulong		ControlSession(ulong ControlCode);
	void		FlushSession();
	void		StopSession();

	struct SEventMonitor* m;
};
