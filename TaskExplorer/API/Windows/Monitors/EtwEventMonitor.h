#pragma once

class CEtwEventMonitor : public QObject
{
    Q_OBJECT

public:
	CEtwEventMonitor(QObject *parent = nullptr);
    virtual ~CEtwEventMonitor();

	bool		Init();

signals:
	void		NetworkEvent(int Type, quint64 ProcessId, quint64 ThreadId, quint32 ProtocolType, quint32 TransferSize,
								const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort);
	void		DnsResEvent(quint64 ProcessId, quint64 ThreadId, const QString& HostName, const QStringList& Results);
	void		FileEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, const QString& FileName);
	void		DiskEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, quint32 IrpFlags, quint32 TransferSize, quint64 HighResResponseTime);
	void		ProcessEvent(int Type, quint32 ProcessId, QString CommandLine, QString FileName, quint32 ParentId, quint64 TimeStamp);

protected:

private:
	struct SEtwEventMonitor* m;
};
