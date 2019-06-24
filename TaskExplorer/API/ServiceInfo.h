#pragma once
#include <qobject.h>
#include "AbstractInfo.h"

#include "ModuleInfo.h"

class CServiceInfo: public CAbstractInfoEx
{
	Q_OBJECT

public:
	CServiceInfo(QObject *parent = nullptr);
	virtual ~CServiceInfo();

	virtual QString GetName() const					{ QReadLocker Locker(&m_Mutex); return m_SvcName; }
	virtual QString GetFileName() const				{ QReadLocker Locker(&m_Mutex); return m_FileName; }
	virtual QString GetBinaryPath() const			{ QReadLocker Locker(&m_Mutex); return m_BinaryPath; }

	virtual quint64 GetPID() const					{ QReadLocker Locker(&m_Mutex); return m_ProcessId; }


	virtual bool IsStopped() const = 0;
	virtual bool IsRunning() const = 0;
	virtual bool IsPaused() const = 0;
	virtual QString GetStateString() const = 0;

	virtual CModulePtr GetModuleInfo()			{ QReadLocker Locker(&m_Mutex); return m_pModuleInfo; }

	virtual STATUS Start() = 0;
	virtual STATUS Pause() = 0;
	virtual STATUS Continue() = 0;
	virtual STATUS Stop() = 0;
	virtual STATUS Delete(bool bForce = false) = 0;

protected:

	QString							m_SvcName;
	QString							m_FileName;
	QString							m_BinaryPath;

	quint64							m_ProcessId;

	CModulePtr						m_pModuleInfo;
};

typedef QSharedPointer<CServiceInfo> CServicePtr;
typedef QWeakPointer<CServiceInfo> CServiceRef;