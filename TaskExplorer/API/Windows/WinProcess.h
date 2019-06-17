#pragma once
#include "../ProcessInfo.h"


class CWinProcessPrivate;

class CWinProcess : public CProcessInfo
{
	Q_OBJECT

public:
	CWinProcess(QObject *parent = nullptr);
	virtual ~CWinProcess();

	virtual bool ValidateParent(CProcessInfo* pParent) const;

	// Basic
	virtual QString GetArchString() const;
	virtual quint64 GetSessionID() const;
	virtual QString GetElevationString() const;
	virtual ulong GetSubsystem() const;
	virtual QString GetSubsystemString() const;
	virtual quint64 GetRawCreateTime() const;

	// Flags
	virtual bool IsSubsystemProcess() const;

	// Dynamic
	virtual quint64 GetPeakPrivateBytes() const;
	virtual quint64 GetWorkingSetSize() const;
	virtual quint64 GetPeakWorkingSetSize() const;
	virtual quint64 GetPrivateWorkingSetSize() const;
	virtual quint64 GetSharedWorkingSetSize() const;
	virtual quint64 GetShareableWorkingSetSize() const;
	virtual quint64 GetVirtualSize() const;
	virtual quint64 GetPeakVirtualSize() const;
	virtual quint32 GetPageFaultCount() const;
	virtual quint64 GetPagedPool() const;
	virtual quint64 GetPeakPagedPool() const;
	virtual quint64 GetNonPagedPool() const;
	virtual quint64 GetPeakNonPagedPool() const;
	virtual quint64 GetMinimumWS() const;
	virtual quint64 GetMaximumWS() const;

	virtual QString GetPriorityString() const;
	virtual STATUS SetPriority(long Value);
	virtual STATUS SetBasePriority(long Value) { return ERR(); }
	virtual STATUS SetPagePriority(long Value);
	virtual STATUS SetIOPriority(long Value);

	virtual STATUS Terminate();

	virtual bool IsSuspended() const;
	virtual STATUS Suspend();
	virtual STATUS Resume();

	// Security
	virtual int IntegrityLevel() const;
	virtual QString GetIntegrityString() const;

	// GDI, USER handles
	virtual ulong GetGdiHandles() const				{ QReadLocker Locker(&m_Mutex); return m_GdiHandles; }
	virtual ulong GetUserHandles() const			{ QReadLocker Locker(&m_Mutex); return m_UserHandles; }
	virtual void SetWndHandles(ulong WndHandles) 	{ QWriteLocker Locker(&m_Mutex); m_WndHandles = WndHandles; }
	virtual ulong GetWndHandles() const				{ QReadLocker Locker(&m_Mutex); return m_WndHandles; }


	// OS context
	virtual ulong GetOsContextVersion() const;
	virtual QString GetOsContextString() const;

	virtual QString GetDEPStatusString() const;

	virtual bool IsVirtualizationAllowed() const;
	virtual bool IsVirtualizationEnabled() const;
	virtual STATUS SetVirtualizationEnabled(bool bSet);
	virtual QString GetVirtualizedString() const;

	virtual QString GetCFGuardString() const;

	virtual QDateTime GetTimeStamp() const;

	// Other Fields
	virtual quint64 GetProcessSequenceNumber() const;

	virtual QMap<QString, SEnvVar>	GetEnvVariables() const;
	virtual STATUS					DeleteEnvVariable(const QString& Name) { return EditEnvVariable(Name, QString()); }
	virtual STATUS					EditEnvVariable(const QString& Name, const QString& Value);

	virtual QString GetStatusString() const;

	virtual bool HasDebugger() const;
	virtual STATUS AttachDebugger();
	virtual STATUS DetachDebugger();
	virtual STATUS CreateDump(const QString& DumpPath);

	virtual QString GetPackageName() const; 
	virtual QString GetAppID() const; 
	virtual ulong GetDPIAwareness() const;
	virtual QString GetDPIAwarenessString() const;


	virtual quint64 GetJobObjectID() const;

	virtual ulong GetProtection() const;
	virtual QString GetProtectionString() const;
	virtual QString GetMitigationString() const;

	virtual QString GetDesktopInfo() const;
	virtual bool IsCriticalProcess() const				{ QReadLocker Locker(&m_Mutex); return m_IsCritical; }
	virtual STATUS SetCriticalProcess(bool bSet, bool bForce = false);
	virtual STATUS ReduceWS();

	virtual void OpenPermissions();

	virtual bool	IsWow64() const;
	virtual quint64 GetPebBaseAddress(bool bWow64 = false) const;

public slots:
	virtual bool	UpdateThreads();
	virtual bool	UpdateHandles();
	virtual bool	UpdateModules();
	virtual bool	UpdateWindows();

	void			OnAsyncDataDone(bool IsPacked, ulong ImportFunctions, ulong ImportModules);

protected:
	friend class CWindowsAPI;

	bool InitStaticData(struct _SYSTEM_PROCESS_INFORMATION* process);
	bool UpdateDynamicData(struct _SYSTEM_PROCESS_INFORMATION* process, quint64 sysTotalTime, quint64 sysTotalCycleTime);
	bool UpdateThreadData(struct _SYSTEM_PROCESS_INFORMATION* process, quint64 sysTotalTime, quint64 sysTotalCycleTime);
	void UnInit();

	void AddNetworkIO(int Type, ulong TransferSize);
	void AddDiskIO(int Type, ulong TransferSize);

	// Dynamic

	// GDI, USER handles
    ulong m_GdiHandles;
    ulong m_UserHandles;
	ulong m_WndHandles;

	bool m_IsCritical;

	// Threads
	//QMap<quint64, >

private:

	struct SWinProcess* m;
};

// CWinProcess Helper Functions

QString GetSidFullNameCached(const vector<char>& Sid);