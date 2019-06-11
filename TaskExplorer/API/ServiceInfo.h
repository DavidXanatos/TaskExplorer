#pragma once
#include <qobject.h>

#include "ModuleInfo.h"

class CServiceInfo: public QObject
{
	Q_OBJECT

public:
	CServiceInfo(QObject *parent = nullptr);
	virtual ~CServiceInfo();

	virtual QString GetName() const					{ QReadLocker Locker(&m_Mutex); return m_SvcName; }
	virtual QString GetFileName() const				{ QReadLocker Locker(&m_Mutex); return m_FileName; }

	virtual QString GetStateString() const = 0;

	virtual CModulePtr GetModuleInfo()			{ QReadLocker Locker(&m_Mutex); return m_pModuleInfo; }

protected:

	QString							m_SvcName;
	QString							m_FileName;

	CModulePtr					m_pModuleInfo;

	mutable QReadWriteLock			m_Mutex;
};

typedef QSharedPointer<CServiceInfo> CServicePtr;
typedef QWeakPointer<CServiceInfo> CServiceRef;