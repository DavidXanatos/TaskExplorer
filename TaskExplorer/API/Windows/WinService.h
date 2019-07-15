#pragma once
#include "../ServiceInfo.h"

class CWinService : public CServiceInfo
{
	Q_OBJECT
public:
	CWinService(QObject *parent = nullptr);
	CWinService(const QString& Name, QObject *parent = nullptr);
	virtual ~CWinService();

	virtual ulong GetType() const					{ QReadLocker Locker(&m_Mutex); return m_Type; }
	virtual QString GetTypeString() const;
	virtual ulong GetState() const					{ QReadLocker Locker(&m_Mutex); return m_State; }
	virtual ulong GetControlsAccepted() const		{ QReadLocker Locker(&m_Mutex); return m_ControlsAccepted; }
	virtual ulong GetFlags() const					{ QReadLocker Locker(&m_Mutex); return m_Flags; }	

	virtual bool IsStopped() const;
	virtual bool IsRunning() const;
	virtual bool IsPaused() const;
	virtual QString GetStateString() const;

	virtual bool IsDriver() const;

	virtual ulong GetStartType() const				{ QReadLocker Locker(&m_Mutex); return m_StartType; }
	virtual QString GetStartTypeString() const;
	virtual ulong GetErrorControl() const			{ QReadLocker Locker(&m_Mutex); return m_ErrorControl; }
	virtual QString GetErrorControlString() const;

	virtual QString GetGroupeName() const			{ QReadLocker Locker(&m_Mutex); return m_GroupeName; }
	virtual QString GetDescription() const			{ QReadLocker Locker(&m_Mutex); return m_Description; }

	virtual STATUS Start();
	virtual STATUS Pause();
	virtual STATUS Continue();
	virtual STATUS Stop();
	virtual STATUS Delete(bool bForce = false);

	virtual void OpenPermissions();

public slots:
	void			OnAsyncDataDone(bool IsPacked, ulong ImportFunctions, ulong ImportModules);

protected:
	friend class CWindowsAPI;

	bool InitStaticData(struct _ENUM_SERVICE_STATUS_PROCESSW* service);
	bool UpdatePID(struct _ENUM_SERVICE_STATUS_PROCESSW* service);
	bool UpdateDynamicData(void* pscManagerHandle, struct _ENUM_SERVICE_STATUS_PROCESSW* service, bool bRefresh);
	void UnInit();

	ulong							m_Type;
	ulong							m_State;
	ulong							m_ControlsAccepted;
	ulong							m_Flags;

	ulong							m_StartType;
	ulong							m_ErrorControl;

	QString							m_GroupeName;
	QString							m_Description;

	QDateTime						m_KeyLastWriteTime;

	struct SWinService* m;
};
