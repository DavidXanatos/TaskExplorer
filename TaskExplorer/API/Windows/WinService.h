#pragma once
#include "../ServiceInfo.h"

class CWinService : public CServiceInfo
{
	Q_OBJECT
public:
	CWinService(QObject *parent = nullptr);
	virtual ~CWinService();

	virtual ulong GetType() const					{ QReadLocker Locker(&m_Mutex); return m_Type; }
	virtual QString GetTypeString() const;
	virtual ulong GetState() const					{ QReadLocker Locker(&m_Mutex); return m_State; }
	virtual QString GetStateString() const;

	virtual bool IsDriver() const;

	virtual ulong GetPID() const					{ QReadLocker Locker(&m_Mutex); return m_ProcessId; }

	virtual ulong GetStartType() const				{ QReadLocker Locker(&m_Mutex); return m_StartType; }
	virtual QString GetStartTypeString() const;
	virtual ulong GetErrorControl() const			{ QReadLocker Locker(&m_Mutex); return m_ErrorControl; }
	virtual QString GetErrorControlString() const;

	virtual QString GetDisplayName() const			{ QReadLocker Locker(&m_Mutex); return m_DisplayName; }
	virtual QString GetGroupeName() const			{ QReadLocker Locker(&m_Mutex); return m_GroupeName; }
	virtual QString GetBinaryPath() const			{ QReadLocker Locker(&m_Mutex); return m_BinaryPath; }

public slots:
	void			OnAsyncDataDone(bool IsPacked, ulong ImportFunctions, ulong ImportModules);

protected:
	friend class CWindowsAPI;

	bool InitStaticData(void* pscManagerHandle, struct _ENUM_SERVICE_STATUS_PROCESSW* service);
	bool UpdateDynamicData(void* pscManagerHandle, struct _ENUM_SERVICE_STATUS_PROCESSW* service);
	void UnInit();

	QString							m_BinaryPath;

	ulong							m_Type;
	ulong							m_State;
	ulong							m_ControlsAccepted;
	ulong							m_Flags;
	ulong							m_ProcessId;

	ulong							m_StartType;
	ulong							m_ErrorControl;

	QString							m_DisplayName;
	QString							m_GroupeName;

	QDateTime						m_KeyLastWriteTime;

	struct SWinService* m;
};
