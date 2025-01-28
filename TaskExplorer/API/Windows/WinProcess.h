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

	virtual void SetParentId(quint64 PID)				{ QWriteLocker Locker(&m_Mutex); m_ParentProcessId = PID; }
	virtual void SetName(const QString& Name)			{ QWriteLocker Locker(&m_Mutex); m_ProcessName = Name; }
	virtual void SetFileName(const QString& FileName, const QString& FileNameNt);
	virtual QString GetFileNameNt() const				{ QReadLocker Locker(&m_Mutex); return m_FileNameNt; }
	virtual void SetCommandLineStr(const QString& CommandLine)	{ QWriteLocker Locker(&m_Mutex); m_CommandLine = CommandLine; }
	virtual void MarkAsHidden();

	virtual quint64 GetStartKey() const					{ QReadLocker Locker(&m_Mutex); return m_StartKey; }

	virtual bool IsFullyInitialized() const				{ QReadLocker Locker(&m_Mutex); return m_IsFullyInitialized; }

	// Basic
	virtual void* GetQueryHandle() const;
	virtual bool IsWoW64() const;
	virtual QString GetArchString() const;
	virtual quint64 GetSessionID() const;
	virtual CWinTokenPtr GetToken() const				{ QReadLocker Locker(&m_Mutex); return m_pToken; }
	virtual quint16 GetSubsystem() const;
	virtual QString GetSubsystemString() const;
	virtual void SetRawCreateTime(quint64 TimeStamp);
	virtual quint64 GetRawCreateTime() const;
	virtual QString GetWorkingDirectory() const;
	//virtual QString GetAppDataDirectory() const;

	// Flags
	virtual bool IsSubsystemProcess() const;

	// Dynamic
	virtual quint64 GetPagedPool() const			{ QReadLocker Locker(&m_Mutex); return m_QuotaPagedPoolUsage; }
	virtual quint64 GetPeakPagedPool() const		{ QReadLocker Locker(&m_Mutex); return m_QuotaPeakPagedPoolUsage; }
	virtual quint64 GetNonPagedPool() const			{ QReadLocker Locker(&m_Mutex); return m_QuotaNonPagedPoolUsage; }
	virtual quint64 GetPeakNonPagedPool() const		{ QReadLocker Locker(&m_Mutex); return m_QuotaPeakNonPagedPoolUsage; }

	virtual quint64 GetSharedWorkingSetSize() const;
	virtual quint64 GetShareableWorkingSetSize() const;
	virtual quint64 GetMinimumWS() const;
	virtual quint64 GetMaximumWS() const;
	virtual quint64 GetShareableCommitSize() const;

	virtual quint32	GetPeakNumberOfHandles() const;
	virtual quint64 GetUpTime() const;
	virtual quint64 GetSuspendTime() const;
	virtual int		GetHangCount() const;
	virtual int		GetGhostCount() const;

	virtual QString GetPriorityString() const		{ return GetPriorityString(GetPriority()); }
	virtual QString GetBasePriorityString() const	{ return GetBasePriorityString(GetBasePriority()); }
	virtual QString GetPagePriorityString() const	{ return GetPagePriorityString(GetPagePriority()); }
	virtual QString GetIOPriorityString() const		{ return GetIOPriorityString(GetIOPriority()); }
	static QString GetPriorityString(quint32 value);
	static QString GetBasePriorityString(quint32 value);
	static QString GetPagePriorityString(quint32 value);
	static QString GetIOPriorityString(quint32 value);
	virtual STATUS SetPriority(qint32 Value);
	virtual STATUS SetBasePriority(qint32 Value)		{ return ERR(); }
	virtual STATUS SetPagePriority(qint32 Value);
	virtual STATUS SetIOPriority(qint32 Value);

	virtual bool HasPriorityBoost() const			{ QReadLocker Locker(&m_Mutex); return m_PriorityBoost; }
	virtual STATUS SetPriorityBoost(bool Value);
	virtual bool IsPowerThrottled() const;			//{ QReadLocker Locker(&m_Mutex); return m_IsPowerThrottled; }
	virtual STATUS SetPowerThrottled(bool Value);

	virtual quint16 GetCodePage() const;
	virtual quint16 GetTlsBitmapCount() const; 
	virtual QString GetTlsBitmapCountString() const; 
	virtual quint32 GetErrorMode() const; 
	virtual QString GetErrorModeString() const; 

	virtual quint32 GetReferenceCount();
	virtual quint32 GetAccessMask();
	virtual QString GetAccessMaskString();

	virtual STATUS SetAffinityMask(quint64 Value);

	virtual STATUS Terminate(bool bForce);

	virtual bool IsSuspended() const;
	virtual STATUS Suspend();
	virtual STATUS Resume();

	virtual bool IsFrozen() const;
	virtual STATUS Freeze();
	virtual STATUS UnFreeze();

	virtual bool IsReflectedProcess() const;

	virtual void AddService(const QString& Name)	{ QWriteLocker Locker(&m_Mutex); if(!m_ServiceList.contains(Name)) m_ServiceList.append(Name); }
	virtual void RemoveService(const QString& Name)	{ QWriteLocker Locker(&m_Mutex); m_ServiceList.removeAll(Name); }
	virtual QStringList GetServiceList() const		{ QReadLocker Locker(&m_Mutex); return m_ServiceList; }

	// GDI, USER handles
	virtual quint32 GetGdiHandles() const			{ QReadLocker Locker(&m_Mutex); return m_GdiHandles; }
	virtual quint32 GetUserHandles() const			{ QReadLocker Locker(&m_Mutex); return m_UserHandles; }
	virtual void SetWndHandles(quint32 WndHandles)  { QWriteLocker Locker(&m_Mutex); m_WndHandles = WndHandles; m_pMainWnd.clear(); } // invalidate cached main window pointer
	virtual quint32 GetWndHandles() const			{ QReadLocker Locker(&m_Mutex); return m_WndHandles; }

	virtual QString GetWindowTitle() const;
	virtual QString GetWindowStatusString() const;

	// OS context
	virtual quint32 GetOsContextVersion() const;
	virtual QString GetOsContextString() const;

	virtual QString GetMitigationsString() const;

	// Other Fields
	virtual QString GetUserName() const;
	virtual quint64 GetProcessSequenceNumber() const;

	virtual QMap<QString, SEnvVar>	GetEnvVariables() const;
	virtual STATUS					DeleteEnvVariable(const QString& Name) { return EditEnvVariable(Name, QString()); }
	virtual STATUS					EditEnvVariable(const QString& Name, const QString& Value);

	virtual QString GetStatusString() const;

	virtual bool HasDebugger() const;
	virtual STATUS AttachDebugger();
	virtual STATUS DetachDebugger();

	virtual int GetKnownProcessType() const;
	virtual bool IsWindowsProcess() const;
	virtual bool IsSystemProcess() const;
	virtual bool IsServiceProcess() const;
	virtual bool IsUserProcess() const;
	virtual bool IsElevated() const;
	virtual bool TokenHasChanged() const;
	virtual bool IsHiddenProcess() const;
	virtual bool CheckIsRunning() const;
	//virtual bool IsJobProcess() const;
	virtual bool IsInJob() const;
	virtual bool IsImmersiveProcess() const;
	virtual bool IsNetProcess() const;
	virtual bool IsPackagedProcess() const;
	virtual quint64 GetConsoleHostId() const;

	virtual QString GetPackageName() const; 
	virtual QString GetAppID() const; 
	virtual quint32 GetDPIAwareness() const;
	virtual QString GetDPIAwarenessString() const;


	virtual quint64 GetJobObjectID() const;

	virtual quint8 GetProtection() const;
	virtual QString GetPPLProtectionString() const;
	virtual QString GetKPHProtectionString() const;
	virtual QString GetProtectionString() const;
	virtual STATUS SetProtectionFlag(quint8 Flag, bool bForce = false);
	virtual QList<QPair<QString, QString>> GetMitigationDetails() const;

	virtual QString GetUsedDesktop() const {QReadLocker Locker(&m_Mutex); return m_UsedDesktop;}

	virtual bool IsCriticalProcess() const;
	virtual STATUS SetCriticalProcess(bool bSet, bool bForce = false);
	virtual STATUS ReduceWS();

	virtual bool IsSandBoxed() const;
	virtual QString GetSandBoxName() const;

	virtual STATUS LoadModule(const QString& Path);

	virtual void OpenPermissions();

	virtual CWinJobPtr		GetJob() const;

	virtual QMap<quint64, CMemoryPtr> GetMemoryMap() const;
	virtual QMap<quint64, CHeapPtr> GetHeapList() const;
	virtual STATUS FlushHeaps();

	virtual QList<CWndPtr> GetWindows() const;
	virtual CWndPtr	GetMainWindow() const;

	virtual void UpdateDns(const QString& HostName, const QList<QHostAddress>& Addresses);

	virtual void CloseHandle();

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
	virtual bool	UpdateModules()				{ return UpdateModulesList(false); }
	virtual bool	UpdateModulesAndModPages()	{ return UpdateModulesList(true); }
	virtual bool	UpdateWindows();

	void			OnAsyncDataDone(bool IsPacked, quint32 ImportFunctions, quint32 ImportModules);

private slots:
	//bool UpdateDynamicDataExt();

protected:
	friend class CWindowsAPI;
	friend class CWinGpuMonitor;

	bool InitStaticData(quint64 ProcessId);
	bool InitStaticData(struct _SYSTEM_PROCESS_INFORMATION* process, bool bFullProcessInfo);
	bool InitStaticData(bool bLoadFileName = true); // *NOT Thread Safe* internal function to be called by other InitStaticData's
	void UpdateModuleInfo(); // *NOT Thread Safe* internal function
	bool IsHandleValid();
	bool UpdateDynamicData(struct _SYSTEM_PROCESS_INFORMATION* process, bool bFullProcessInfo, quint64 sysTotalTime, quint64 sysTotalTimePerCPU);
	bool UpdateThreadData(struct _SYSTEM_PROCESS_INFORMATION* process, bool bFullProcessInfo, quint64 sysTotalTime, quint64 sysTotalCycleTime);
	bool UpdateTokenData(bool MonitorChange);
	void UpdateCPUCycles(quint64 sysTotalTime, quint64 sysTotalCycleTime);
	void UnInit();

	bool UpdateModulesList(bool bWithModPages);

	void AddNetworkIO(int Type, quint32 TransferSize);
	void AddDiskIO(int Type, quint32 TransferSize);

	QString							m_FileNameNt;

	union {
		quint32 m_MiscStates;
		struct {
			quint32 
				m_PriorityBoost : 1,
				m_Reserved : 31;
		};
	};

	// Other fields
	QList<QString>					m_ServiceList;

	// Dynamic
	quint64							m_QuotaPagedPoolUsage;
	quint64							m_QuotaPeakPagedPoolUsage;
	quint64							m_QuotaNonPagedPoolUsage;
	quint64							m_QuotaPeakNonPagedPoolUsage;

	// GDI, USER handles
    quint32							m_GdiHandles;
    quint32							m_UserHandles;
	quint32							m_WndHandles;

	bool							m_IsCritical;

	quint64							m_StartKey;	

	// Token
	CWinTokenPtr					m_pToken;

	// MainWindow
	CWndRef							m_pMainWnd;
	QString							m_UsedDesktop;

private:
	//volatile quint64 m_lastExtUpdate;
	//void	UpdateExtDataIfNeeded() const;
	void UpdateWsCounters() const;

	bool							m_IsFullyInitialized;
	quint64							m_LastUpdateThreads;
	quint64							m_LastUpdateHandles;

	struct SWinProcess* m;
};

QVariantList GetProcessUnloadedDlls(quint64 ProcessId);
QVariantList GetProcessHeaps(quint64 ProcessId);