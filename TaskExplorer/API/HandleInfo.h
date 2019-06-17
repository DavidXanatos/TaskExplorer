#pragma once
#include <qobject.h>
#include "..\Common\FlexError.h"

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


	virtual QString			GetProcessName()		{ QReadLocker Locker(&m_Mutex); return m_ProcessName; }
	virtual QSharedPointer<QObject>	GetProcess()	{ QReadLocker Locker(&m_Mutex); return m_pProcess; }

	virtual STATUS		Close(bool bForce = false) = 0;

protected:
	quint64				m_HandleId;
	quint64				m_ProcessId;

	QString				m_FileName;
	quint64				m_Position;
	quint64				m_Size;

	QString				m_ProcessName;
	QSharedPointer<QObject>	m_pProcess;

	mutable QReadWriteLock		m_Mutex;
};

typedef QSharedPointer<CHandleInfo> CHandlePtr;
typedef QWeakPointer<CHandleInfo> CHandleRef;