#pragma once
#include "../SystemAPI.h"
#include "WinProcess.h"
#include "WinSocket.h"
#include "EventMonitor.h"
#include "SymbolProvider.h"

class CWindowsAPI : public CSystemAPI
{
	Q_OBJECT

public:
	CWindowsAPI(QObject *parent = nullptr);
	virtual ~CWindowsAPI();

	virtual bool UpdateProcessList();

	virtual bool UpdateSocketList();

	virtual CSymbolProviderPtr GetSymbolProvider() { return m_pSymbolProvider; }

public slots:
	void		OnNetworkEvent(int Type, quint64 ProcessId, quint64 ThreadId, ulong ProtocolType, ulong TransferSize,
								const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort);
	void		OnFileEvent(int Type, quint64 FileId, const QString& FileName);
	void		OnDiskEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, ulong IrpFlags, ulong TransferSize, quint64 HighResResponseTime);

protected:

	CEventMonitor*		m_pEventMonitor;

	QMap<quint64, QString> m_FileNames;

	CSymbolProviderPtr m_pSymbolProvider;
};


