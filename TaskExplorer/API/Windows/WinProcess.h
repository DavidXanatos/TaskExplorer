#pragma once
#include "../ProcessInfo.h"
#include "WinJob.h"
#include "WinToken.h"
#include "WinGDI.h"

class CWinProcess : public CProcessInfo
{
	Q_OBJECT

public:
	CWinProcess(QObject *parent = nullptr);
	virtual ~CWinProcess();

	virtual bool ValidateParent(CProcessInfo* pParent) const;

	// Basic
	virtual void* GetQueryHandle() const;
	virtual bool IsWoW64() const;
	virtual QString GetArchString() const;
	virtual quint64 GetSessionID() const;
	virtual CWinTokenPtr GetToken() const			{ QReadLocker Locker(&m_Mutex); return m_pToken; }
	virtual ulong GetSubsystem() const;
	virtual QString GetSubsystemString() const;
	virtual quint64 GetRawCreateTime() const;
	virtual QString GetWorkingDirectory() const;

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
	//virtual quint32 GetPageFaultCount() const;
	virtual quint64 GetPagedPool() const;
	virtual quint64 GetPeakPagedPool() const;
	virtual quint64 GetNonPagedPool() const;
	virtual quint64 GetPeakNonPagedPool() const;
	virtual quint64 GetMinimumWS() const;
	virtual quint64 GetMaximumWS() const;

	virtual ulong	GetPeakNumberOfHandles() const;
	virtual quint64 GetUpTime() const;
	virtual quint64 GetSuspendTime() const;
	virtual int		GetHangCount() const;
	virtual int		GetGhostCount() const;

	virtual QString GetPriorityString() const;
	virtual STATUS SetPriority(long Value);
	virtual STATUS SetBasePriority(long Value)		{ return ERR(); }
	virtual STATUS SetPagePriority(long Value);
	virtual STATUS SetIOPriority(long Value);

	virtual STATUS SetAffinityMask(quint64 Value);

	virtual STATUS Terminate();

	virtual bool IsSuspended() const;
	virtual STATUS Suspend();
	virtual STATUS Resume();

	virtual void AddService(const QString& Name)	{ QWriteLocker Locker(&m_Mutex); if(!m_ServiceList.contains(Name)) m_ServiceList.append(Name); }
	virtual QStringList GetServiceList() const		{ QReadLocker Locker(&m_Mutex); return m_ServiceList; }

	// GDI, USER handles
	virtual ulong GetGdiHandles() const				{ QReadLocker Locker(&m_Mutex); return m_GdiHandles; }
	virtual ulong GetUserHandles() const			{ QReadLocker Locker(&m_Mutex); return m_UserHandles; }
	virtual void SetWndHandles(ulong WndHandles) 	{ QWriteLocker Locker(&m_Mutex); m_WndHandles = WndHandles; }
	virtual ulong GetWndHandles() const				{ QReadLocker Locker(&m_Mutex); return m_WndHandles; }


	// OS context
	virtual ulong GetOsContextVersion() const;
	virtual QString GetOsContextString() const;

	virtual QString GetDEPStatusString() const;

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

	virtual bool IsSystemProcess() const;
	virtual bool IsServiceProcess() const;
	virtual bool IsUserProcess() const;
	virtual bool IsElevated() const;
	//virtual bool IsJobProcess() const;
	virtual bool IsInJob() const;
	virtual bool IsPicoProcess() const;
	virtual bool IsImmersiveProcess() const;
	virtual bool IsNetProcess() const;
	virtual quint64 GetConsoleHostId() const;

	virtual QString GetPackageName() const; 
	virtual QString GetAppID() const; 
	virtual ulong GetDPIAwareness() const;
	virtual QString GetDPIAwarenessString() const;


	virtual quint64 GetJobObjectID() const;

	virtual ulong GetProtection() const;
	virtual QString GetProtectionString() const;
	virtual QString GetMitigationString() const;

	virtual QString GetDesktopInfo() const;
	virtual bool IsCriticalProcess() const;
	virtual STATUS SetCriticalProcess(bool bSet, bool bForce = false);
	virtual STATUS ReduceWS();

	virtual STATUS LoadModule(const QString& Path);

	virtual void OpenPermissions();

	virtual quint64 GetPebBaseAddress(bool bWow64 = false) const;

	virtual CWinJobPtr		GetJob() const;

	virtual QMap<quint64, CMemoryPtr> GetMemoryMap() const;

	virtual CWndPtr GetMainWindowHwnd() const;

	virtual bool UpdateGDIList(QMap<quint64, CWinGDIPtr>& List) const;

	struct STask
	{
		QString Name;
		QString Path;
	};
	virtual QList<STask>	GetTasks() const;

	struct SDriver
	{
		QString Name;
		QString Path;
	};
	virtual QList<SDriver>	GetUmdfDrivers() const;

	struct SWmiProvider
	{
		QString ProviderName;
		QString NamespacePath;
		QString FileName;
		QString UserName;
	};
	virtual QList<SWmiProvider>	QueryWmiProviders() const;

public slots:
	virtual bool	UpdateThreads();
	virtual bool	UpdateHandles();
	virtual bool	UpdateModules();
	virtual bool	UpdateWindows();

	void			OnAsyncDataDone(bool IsPacked, ulong ImportFunctions, ulong ImportModules);

private slots:
	//bool UpdateDynamicDataExt();

protected:
	friend class CWindowsAPI;

	bool InitStaticData(struct _SYSTEM_PROCESS_INFORMATION* process, bool bFullProcessInfo);
	bool UpdateDynamicData(struct _SYSTEM_PROCESS_INFORMATION* process, bool bFullProcessInfo, quint64 sysTotalTime, quint64 sysTotalCycleTime);
	bool UpdateThreadData(struct _SYSTEM_PROCESS_INFORMATION* process, bool bFullProcessInfo, quint64 sysTotalTime, quint64 sysTotalCycleTime);
	void UnInit();

	void AddNetworkIO(int Type, ulong TransferSize);
	void AddDiskIO(int Type, ulong TransferSize);

	// Other fields
	QList<QString>					m_ServiceList;

	// Dynamic

	// GDI, USER handles
    ulong m_GdiHandles;
    ulong m_UserHandles;
	ulong m_WndHandles;

	bool m_IsCritical;

	// Threads
	//QMap<quint64, >

	// Token
	CWinTokenPtr					m_pToken;

private:
	//volatile quint64 m_lastExtUpdate;
	//void	UpdateExtDataIfNeeded() const;
	void UpdateWsCounters() const;

	quint64 m_LastUpdateHandles;

	struct SWinProcess* m;
};

