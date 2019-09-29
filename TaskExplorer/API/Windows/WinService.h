#pragma once
#include "../ServiceInfo.h"

class CWinService : public CServiceInfo
{
	Q_OBJECT
public:
	CWinService(QObject *parent = nullptr);
	CWinService(const QString& Name, QObject *parent = nullptr);
	virtual ~CWinService();

	virtual quint32 GetType() const					{ QReadLocker Locker(&m_Mutex); return m_Type; }
	virtual QString GetTypeString() const;
	virtual quint32 GetState() const				{ QReadLocker Locker(&m_Mutex); return m_State; }
	virtual quint32 GetControlsAccepted() const		{ QReadLocker Locker(&m_Mutex); return m_ControlsAccepted; }
	virtual quint32 GetFlags() const				{ QReadLocker Locker(&m_Mutex); return m_Flags; }	

	virtual bool IsStopped() const;
	virtual bool IsRunning(bool bStrict = false) const;
	virtual bool IsPaused() const;
	virtual QString GetStateString() const;

	virtual bool IsDriver() const;

	virtual quint32 GetStartType() const			{ QReadLocker Locker(&m_Mutex); return m_StartType; }
	virtual QString GetStartTypeString() const;
	virtual quint32 GetErrorControl() const			{ QReadLocker Locker(&m_Mutex); return m_ErrorControl; }
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
	void			OnAsyncDataDone(bool IsPacked, quint32 ImportFunctions, quint32 ImportModules);

protected:
	friend class CWindowsAPI;

	bool InitStaticData(struct _ENUM_SERVICE_STATUS_PROCESSW* service);
	bool UpdatePID(struct _ENUM_SERVICE_STATUS_PROCESSW* service);
	bool UpdateDynamicData(void* pscManagerHandle, struct _ENUM_SERVICE_STATUS_PROCESSW* service, bool bRefresh);
	void UnInit();

	quint32							m_Type;
	quint32							m_State;
	quint32							m_ControlsAccepted;
	quint32							m_Flags;

	quint32							m_StartType;
	quint32							m_ErrorControl;

	QString							m_GroupeName;
	QString							m_Description;

	quint64							m_KeyLastWriteTime;

	struct SWinService* m;
};
