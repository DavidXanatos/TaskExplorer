#pragma once
#include <qobject.h>
#include "AbstractInfo.h"

class CDnsLogEntry: public CAbstractInfoEx
{
	Q_OBJECT

public:
	CDnsLogEntry(const QString& HostName, const QList<QHostAddress>& Addresses);
	virtual ~CDnsLogEntry() {}

	virtual QList<QHostAddress> UpdateAddresses(const QList<QHostAddress>& Addresses);

	virtual QString GetHostName() const							{ QReadLocker Locker(&m_Mutex); return m_HostName; }
	virtual QList<QHostAddress> GetAddresses() const			{ QReadLocker Locker(&m_Mutex); return m_Addresses; }

protected:
	QString					m_HostName;
	QList<QHostAddress>		m_Addresses;
};

typedef QSharedPointer<CDnsLogEntry> CDnsLogEntryPtr;
typedef QWeakPointer<CDnsLogEntry> CDnsLogEntryRef;

/*
class CDnsProcRecord : public QObject
{
public:
	CDnsProcRecord(const QString& ProcessName, quint64 ProcessId)
	{
		m_ProcessName = ProcessName;
		m_ProcessId = ProcessId;

		m_LastSeen = 0;
		m_Counter = 0;
	}
	virtual ~CDnsProcRecord() {}

	virtual quint64 GetProcessId() const				{ QReadLocker Locker(&m_Mutex); return m_ProcessId; }
	virtual QString GetProcessName() const				{ QReadLocker Locker(&m_Mutex); return m_ProcessName; }
	virtual QWeakPointer<QObject> GetProcess() const	{ QReadLocker Locker(&m_Mutex); return m_pProcess; }
	virtual quint64 GetLastSeen() const					{ QReadLocker Locker(&m_Mutex); return m_LastSeen; }
	virtual quint32 GetCounter() const					{ QReadLocker Locker(&m_Mutex); return m_Counter; }

	virtual void Update(const QWeakPointer<QObject>& pProcess, bool bUpdate)
	{
		QWriteLocker Locker(&m_Mutex);
		if(pProcess)
			m_pProcess = pProcess;

		if (bUpdate && m_LastSeen != 0)
			return;

		m_LastSeen = GetTime() * 1000; // in ms
		m_Counter++;
	}

protected:
	mutable QReadWriteLock	m_Mutex;

	quint64					m_ProcessId;
	QString					m_ProcessName;
	QWeakPointer<QObject>	m_pProcess;
	quint64					m_LastSeen;
	quint32					m_Counter;
};

typedef QSharedPointer<CDnsProcRecord> CDnsProcRecordPtr;
*/

class CDnsCacheEntry: public CAbstractInfoEx
{
	Q_OBJECT

public:
	CDnsCacheEntry(const QString& HostName, quint16 Type, const QHostAddress& Address, const QString& ResolvedString = QString(), QObject *parent = nullptr);
	virtual ~CDnsCacheEntry() {}

	virtual QString GetHostName() const				{ QReadLocker Locker(&m_Mutex); return m_HostName; }
	virtual QString GetResolvedString() const		{ QReadLocker Locker(&m_Mutex); return m_ResolvedString; }
	virtual QHostAddress GetAddress() const			{ QReadLocker Locker(&m_Mutex); return m_Address; }
	virtual quint16 GetType() const					{ QReadLocker Locker(&m_Mutex); return m_Type; }
	virtual QString GetTypeString() const;
	static QString GetTypeString(quint16 Type);

	virtual quint64 GetTTL() const					{ QReadLocker Locker(&m_Mutex); return m_TTL > 0 ? m_TTL : 0; }
	virtual quint64 GetDeadTime() const				{ QReadLocker Locker(&m_Mutex); return m_TTL < 0 ? -m_TTL : 0; }
	virtual void SetTTL(quint64 TTL);
	virtual void SubtractTTL(quint64 Delta);
	
	virtual quint64 GetQueryCounter() const			{ QReadLocker Locker(&m_Mutex); return m_QueryCounter; }

	/*virtual void RecordProcess(const QString& ProcessName, quint64 ProcessId, const QWeakPointer<QObject>& pProcess, bool bUpdate = false);
	virtual QMap<QPair<QString, quint64>, CDnsProcRecordPtr> GetProcessRecords() const	{ QReadLocker Locker(&m_Mutex); return m_ProcessRecords; }*/

protected:

	QString			m_HostName;
	QString			m_ResolvedString;
	QHostAddress	m_Address;
	quint16			m_Type;
	qint64			m_TTL;

	quint32			m_QueryCounter;

	//QMap<QPair<QString, quint64>, CDnsProcRecordPtr>	m_ProcessRecords;
};

typedef QSharedPointer<CDnsCacheEntry> CDnsCacheEntryPtr;
typedef QWeakPointer<CDnsCacheEntry> CDnsCacheEntryRef;