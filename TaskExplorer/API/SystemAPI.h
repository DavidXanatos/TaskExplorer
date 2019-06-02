#pragma once
#include <qobject.h>
#include "ProcessInfo.h"
#include "SocketInfo.h"

class CSystemAPI : public QObject
{
	Q_OBJECT

public:
	CSystemAPI(QObject *parent = nullptr);
	virtual ~CSystemAPI();

	static CSystemAPI* New(QObject *parent = nullptr);

	virtual QMap<quint64, CProcessPtr> GetProcessList();
	virtual CProcessPtr GetProcessByID(quint64 ProcessId);

	virtual QMultiMap<quint64, CSocketPtr> GetSocketList();


public slots:
	virtual bool UpdateProcessList() = 0;
	virtual bool UpdateSocketList() = 0;

signals:
	void ProcessListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void SocketListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);

protected:

	mutable QReadWriteLock		m_ProcessMutex;
	QMap<quint64, CProcessPtr>	m_ProcessList;

	mutable QReadWriteLock		m_SocketMutex;
	QMultiMap<quint64, CSocketPtr>	m_SocketList;
};

