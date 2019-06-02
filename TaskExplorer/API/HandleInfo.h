#pragma once
#include <qobject.h>

class CHandleInfo: public QObject
{
	Q_OBJECT

public:
	CHandleInfo(QObject *parent = nullptr);
	virtual ~CHandleInfo();

	virtual quint64 GetHandleId() const				{ QReadLocker Locker(&m_Mutex); return m_HandleId; }
	virtual quint64 GetProcessId()	const			{ QReadLocker Locker(&m_Mutex); return m_ProcessId; }

	virtual QString GetTypeString() const = 0;
	virtual QString GetFileName() const				{ QReadLocker Locker(&m_Mutex); return m_FileName; }
	virtual quint64 GetPosition() const				{ QReadLocker Locker(&m_Mutex); return m_Position; }
	virtual quint64 GetSize()	const				{ QReadLocker Locker(&m_Mutex); return m_Size; }
	virtual QString GetGrantedAccessString() const = 0;

	virtual QVariantMap GetDetailedInfos() const	{ return QVariantMap(); };

protected:
	quint64				m_HandleId;
	quint64				m_ProcessId;

	QString				m_FileName;
	quint64				m_Position;
	quint64				m_Size;

	mutable QReadWriteLock		m_Mutex;
};

typedef QSharedPointer<CHandleInfo> CHandlePtr;
typedef QWeakPointer<CHandleInfo> CHandleRef;