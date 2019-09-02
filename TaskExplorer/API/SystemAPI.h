#pragma once
#include <qobject.h>
#include "MiscStats.h"
#include "ProcessInfo.h"
#include "SocketInfo.h"
#include "HandleInfo.h"
#include "ServiceInfo.h"
#include "DriverInfo.h"
#include "WndInfo.h"
#include "../Common/Common.h"
#include "Monitors/GpuMonitor.h"
#include "Monitors/NetMonitor.h"
#include "Monitors/DiskMonitor.h"

struct SCpuStats
{
	SCpuStats()
	{
		KernelUsage = 0.0f;
		UserUsage = 0.0f;
	}

	SDelta64 KernelDelta;
	SDelta64 UserDelta;
	SDelta64 IdleDelta;

	float KernelUsage;
	float UserUsage;
};

struct SCpuStatsEx : SCpuStats
{
	SDelta32 PageFaultsDelta;
	SDelta32 PageReadsDelta;
	SDelta32 PageFileWritesDelta;
	SDelta32 MappedWritesDelta;

	SDelta32 ContextSwitchesDelta;
	SDelta32 InterruptsDelta;
	SDelta64 DpcsDelta;
	SDelta32 SystemCallsDelta;
};

struct SPageFile // on linux swap partition
{
	SPageFile()
	{
		TotalSize = 0;
		TotalInUse = 0;
		PeakUsage = 0;
	}

	QString Path;
    quint64 TotalSize;
    quint64 TotalInUse;
    quint64 PeakUsage;
};

class CSystemAPI : public QObject
{
	Q_OBJECT

public:
	CSystemAPI(QObject *parent = nullptr);
	virtual ~CSystemAPI();

	static void InitAPI();

	virtual bool RootAvaiable() = 0;

	virtual QMap<quint64, CProcessPtr> GetProcessList();
	virtual CProcessPtr GetProcessByID(quint64 ProcessId);
	virtual CThreadPtr  GetThreadByID(quint64 ThreadId);
	virtual CProcessPtr GetProcessByThreadID(quint64 ThreadId);

	virtual QMultiMap<quint64, CSocketPtr> GetSocketList();

	virtual QMap<quint64, CHandlePtr> GetOpenFilesList();

	virtual QMap<QString, CServicePtr> GetServiceList();
	virtual CServicePtr			GetService(const QString& Name);

	virtual QMap<QString, CDriverPtr> GetDriverList();

	virtual SSysStats			GetStats()			{ QReadLocker Locker(&m_StatsMutex); return m_Stats; }

	virtual float GetCpuUsage() const				{ QReadLocker Locker(&m_StatsMutex); return m_CpuStats.KernelUsage + m_CpuStats.UserUsage; }
	virtual float GetCpuDPCUsage() const			{ QReadLocker Locker(&m_StatsMutex); return m_CpuStatsDPCUsage; }
	virtual float GetCpuKernelUsage() const			{ QReadLocker Locker(&m_StatsMutex); return m_CpuStats.KernelUsage; }
	virtual float GetCpuUserUsage() const			{ QReadLocker Locker(&m_StatsMutex); return m_CpuStats.UserUsage; }

	virtual quint64 GetInstalledMemory() const		{ QReadLocker Locker(&m_StatsMutex); return m_InstalledMemory; }
	virtual quint64 GetAvailableMemory() const		{ QReadLocker Locker(&m_StatsMutex); return m_AvailableMemory; }
	virtual quint64 GetCommitedMemory() const		{ QReadLocker Locker(&m_StatsMutex); return m_CommitedMemory; }
	virtual quint64 GetCommitedMemoryPeak() const	{ QReadLocker Locker(&m_StatsMutex); return m_CommitedMemoryPeak; }
	virtual quint64 GetMemoryLimit() const			{ QReadLocker Locker(&m_StatsMutex); return m_MemoryLimit; }
	virtual quint64 GetSwapedOutMemory() const		{ QReadLocker Locker(&m_StatsMutex); return m_SwapedOutMemory; }
	virtual quint64 GetTotalSwapMemory() const		{ QReadLocker Locker(&m_StatsMutex); return m_TotalSwapMemory; }
	virtual quint64 GetPagedPool() const			{ QReadLocker Locker(&m_StatsMutex); return m_PagedPool; }
	virtual quint64 GetPersistentPagedPool() const	{ QReadLocker Locker(&m_StatsMutex); return m_PersistentPagedPool; }
	virtual quint64 GetNonPagedPool() const			{ QReadLocker Locker(&m_StatsMutex); return m_NonPagedPool; }
	virtual quint64 GetPhysicalUsed() const			{ QReadLocker Locker(&m_StatsMutex); return m_PhysicalUsed; }
	virtual quint64 GetCacheMemory() const			{ QReadLocker Locker(&m_StatsMutex); return m_CacheMemory; }
	virtual quint64 GetKernelMemory() const			{ QReadLocker Locker(&m_StatsMutex); return m_KernelMemory; }
	virtual quint64 GetDriverMemory() const			{ QReadLocker Locker(&m_StatsMutex); return m_DriverMemory; }
	virtual quint64 GetReservedMemory() const		{ QReadLocker Locker(&m_StatsMutex); return m_ReservedMemory; }

	virtual quint64 GetTotalProcesses() const		{ QReadLocker Locker(&m_StatsMutex); return m_TotalProcesses; }
	virtual quint64 GetTotalThreads() const			{ QReadLocker Locker(&m_StatsMutex); return m_TotalThreads; }
	virtual quint64 GetTotalHandles() const			{ QReadLocker Locker(&m_StatsMutex); return m_TotalHandles; }

	virtual int GetPackageCount() const				{ QReadLocker Locker(&m_StatsMutex); return m_PackageCount; }
	virtual int GetNumaCount() const				{ QReadLocker Locker(&m_StatsMutex); return m_NumaCount; }
	virtual int GetCoreCount() const				{ QReadLocker Locker(&m_StatsMutex); return m_CoreCount; }
	virtual int GetCpuCount() const					{ QReadLocker Locker(&m_StatsMutex); return m_CpuCount; }
	virtual double GetCpuBaseClock() const			{ QReadLocker Locker(&m_StatsMutex); return m_CpuBaseClock; }
	virtual double GetCpuCurrentClock() const		{ QReadLocker Locker(&m_StatsMutex); return m_CpuCurrentClock; }

	virtual SCpuStatsEx GetCpuStats() const			{ QReadLocker Locker(&m_StatsMutex); return m_CpuStats; }
	virtual SCpuStats GetCpuStats(int Index) const	{ QReadLocker Locker(&m_StatsMutex); return m_CpusStats[Index]; }

	virtual QList<SPageFile> GetPageFiles() const	{ QReadLocker Locker(&m_StatsMutex); return m_PageFiles; }

	virtual QString GetCpuModel() const				{ QReadLocker Locker(&m_Mutex); return m_CPU_String; }

	virtual QPixmap GetSystemIcon() const			{ QReadLocker Locker(&m_Mutex); return m_SystemIcon; }
	virtual QString GetSystemName() const			{ QReadLocker Locker(&m_Mutex); return m_SystemName; }
	virtual QString GetSystemVersion() const		{ QReadLocker Locker(&m_Mutex); return m_SystemVersion; }
	virtual QString GetSystemBuild() const			{ QReadLocker Locker(&m_Mutex); return m_SystemBuild; }
	virtual quint64 GetUpTime() const = 0;
	virtual QString GetHostName() const				{ QReadLocker Locker(&m_Mutex); return m_HostName; }
	virtual QString GetUserName() const				{ QReadLocker Locker(&m_Mutex); return m_UserName; }
	virtual QString GetSystemDir() const			{ QReadLocker Locker(&m_Mutex); return m_SystemDir; }

	struct SUser
	{
		SUser()
		{
			SessionId = 0;
		}
		QString UserName;
		quint32 SessionId;
		QString Status;
	};

	virtual QList<SUser> GetUsers() const = 0;

	virtual CGpuMonitor* GetGpuMonitor()			{ return m_pGpuMonitor; }
	virtual CNetMonitor* GetNetMonitor()			{ return m_pNetMonitor; }
	virtual CDiskMonitor* GetDiskMonitor()			{ return m_pDiskMonitor; }

	void AddThread(CThreadPtr pThread);
	void ClearThread(quint64 ThreadId);

	virtual bool UpdateOpenFileListAsync();

	virtual void NotifyHardwareChanged();

public slots:
	virtual bool UpdateSysStats() = 0;
	virtual bool UpdateProcessList() = 0;
	virtual bool UpdateSocketList() = 0;
	virtual bool UpdateOpenFileList() = 0;
	virtual bool UpdateServiceList(bool bRefresh = false) = 0;
	virtual bool UpdateDriverList() = 0;

private slots:
	virtual bool Init() = 0;
	virtual void OnOpenFilesUpdated();
	virtual void OnHardwareChanged() = 0;

signals:
	void ProcessListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void SocketListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void OpenFileListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void ServiceListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed);
	void DriverListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed);

protected:
	//virtual void				UpdateStats();

	mutable QReadWriteLock		m_ProcessMutex;
	QMap<quint64, CProcessPtr>	m_ProcessList;

	mutable QReadWriteLock		m_SocketMutex;
	QMultiMap<quint64, CSocketPtr>	m_SocketList;

	mutable QReadWriteLock		m_OpenFilesMutex;
	QMap<quint64, CHandlePtr>	m_OpenFilesList;

	mutable QReadWriteLock		m_ServiceMutex;
	QMap<QString, CServicePtr>	m_ServiceList;

	mutable QReadWriteLock		m_DriverMutex;
	QMap<QString, CDriverPtr>	m_DriverList;

	// Guard it with m_ProcessMutex
	QMap<quint64, CThreadRef>	m_ThreadMap;

	mutable QReadWriteLock		m_Mutex;

	CGpuMonitor*				m_pGpuMonitor;
	CNetMonitor*				m_pNetMonitor;
	CDiskMonitor*				m_pDiskMonitor;

	// I/O stats
	mutable QReadWriteLock		m_StatsMutex;
	SSysStats					m_Stats;

	int							m_PackageCount;
	int							m_NumaCount;
	int							m_CoreCount;
	int							m_CpuCount;
	double						m_CpuBaseClock;
	double						m_CpuCurrentClock;

	SCpuStatsEx					m_CpuStats;
	float						m_CpuStatsDPCUsage;

	quint64						m_InstalledMemory;
	quint64						m_AvailableMemory;
	quint64						m_CommitedMemory;
	quint64						m_CommitedMemoryPeak;
	quint64						m_MemoryLimit;
	quint64						m_SwapedOutMemory;
	quint64						m_TotalSwapMemory;
	quint64						m_PagedPool;
	quint64						m_PersistentPagedPool;
	quint64						m_NonPagedPool;
	quint64						m_PhysicalUsed;
	quint64						m_CacheMemory;
	quint64						m_KernelMemory;
	quint64						m_DriverMemory;
	quint64						m_ReservedMemory;

	quint32						m_TotalProcesses;
	quint32						m_TotalThreads;
	quint32						m_TotalHandles;

	QVector<SCpuStats>			m_CpusStats;

	QList<SPageFile>			m_PageFiles;
	
	// guard this once with m_Mutex
	QString						m_CPU_String;
	QPixmap						m_SystemIcon;
	QString						m_SystemName;
	QString						m_SystemVersion;
	QString						m_SystemBuild;
	QString						m_HostName;
	QString						m_UserName;
	QString						m_SystemDir;

	volatile bool				m_HardwareChangePending;

	static bool					UpdateOpenFileListAsync(CSystemAPI* This);
	QFutureWatcher<bool>*		m_FileListUpdateWatcher;
};

extern CSystemAPI*		theAPI;