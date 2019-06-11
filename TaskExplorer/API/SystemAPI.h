#pragma once
#include <qobject.h>
#include "IOStats.h"
#include "ProcessInfo.h"
#include "SocketInfo.h"
#include "HandleInfo.h"
#include "ServiceInfo.h"
#include "../Common/Common.h"

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

class CSystemAPI : public QObject
{
	Q_OBJECT

public:
	CSystemAPI(QObject *parent = nullptr);
	virtual ~CSystemAPI();

	static CSystemAPI* New(QObject *parent = nullptr);

	virtual QMap<quint64, CProcessPtr> GetProcessList();
	virtual CProcessPtr GetProcessByID(quint64 ProcessId);
	virtual CThreadPtr  GetThreadByID(quint64 ThreadId);
	virtual CProcessPtr GetProcessByThreadID(quint64 ThreadId);

	virtual QMultiMap<quint64, CSocketPtr> GetSocketList();

	virtual QMultiMap<quint64, CHandlePtr> GetOpenFilesList();

	virtual QMultiMap<QString, CServicePtr> GetServiceList();

	virtual QMultiMap<QString, CServicePtr> GetDriverList();

	virtual SProcStats			GetStats()			{ QReadLocker Locker(&m_StatsMutex); return m_Stats; }
	virtual SSambaStats			GetSambaStats()		{ QReadLocker Locker(&m_StatsMutex); return m_SambaStats; }

	virtual float GetCpuUsage() const				{ QReadLocker Locker(&m_StatsMutex); return m_CpuStats.KernelUsage + m_CpuStats.UserUsage; }
	virtual float GetCpuDPCUsage() const			{ QReadLocker Locker(&m_StatsMutex); return m_CpuStatsDPCUsage; }
	virtual float GetCpuKernelUsage() const			{ QReadLocker Locker(&m_StatsMutex); return m_CpuStats.KernelUsage; }
	virtual float GetCpuUserUsage() const			{ QReadLocker Locker(&m_StatsMutex); return m_CpuStats.UserUsage; }

	virtual quint64 GetInstalledMemory() const		{ QReadLocker Locker(&m_StatsMutex); return m_InstalledMemory; }
	virtual quint64 GetAvailableMemory() const		{ QReadLocker Locker(&m_StatsMutex); return m_AvailableMemory; }
	virtual quint64 GetCommitedMemory() const		{ QReadLocker Locker(&m_StatsMutex); return m_CommitedMemory; }
	virtual quint64 GetMemoryLimit() const			{ QReadLocker Locker(&m_StatsMutex); return m_MemoryLimit; }
	virtual quint64 GetPagedMemory() const			{ QReadLocker Locker(&m_StatsMutex); return m_PagedMemory; }
	virtual quint64 GetPhysicalUsed() const			{ QReadLocker Locker(&m_StatsMutex); return m_PhysicalUsed; }
	virtual quint64 GetCacheMemory() const			{ QReadLocker Locker(&m_StatsMutex); return m_CacheMemory; }
	virtual quint64 GetReservedMemory() const		{ QReadLocker Locker(&m_StatsMutex); return m_ReservedMemory; }

	virtual quint64 GetTotalProcesses() const		{ QReadLocker Locker(&m_StatsMutex); return m_TotalProcesses; }
	virtual quint64 GetTotalThreads() const			{ QReadLocker Locker(&m_StatsMutex); return m_TotalThreads; }
	virtual quint64 GetTotalHandles() const			{ QReadLocker Locker(&m_StatsMutex); return m_TotalHandles; }


	void AddThread(CThreadPtr pThread);
	void ClearThread(quint64 ThreadId);

public slots:
	virtual bool UpdateProcessList() = 0;
	virtual bool UpdateSocketList() = 0;
	virtual bool UpdateOpenFileList() = 0;
	virtual bool UpdateServiceList() = 0;
	virtual bool UpdateDriverList() = 0;

signals:
	void ProcessListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void SocketListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void OpenFileListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void ServiceListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed);
	void DriverListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed);

protected:
	virtual void				UpdateStats();

	mutable QReadWriteLock		m_ProcessMutex;
	QMap<quint64, CProcessPtr>	m_ProcessList;

	mutable QReadWriteLock		m_SocketMutex;
	QMultiMap<quint64, CSocketPtr>	m_SocketList;

	mutable QReadWriteLock		m_OpenFilesMutex;
	QMultiMap<quint64, CHandlePtr>	m_OpenFilesList;

	mutable QReadWriteLock		m_ServiceMutex;
	QMultiMap<QString, CServicePtr>	m_ServiceList;

	mutable QReadWriteLock		m_DriverMutex;
	QMultiMap<QString, CServicePtr>	m_DriverList;

	// Guard it with m_ProcessMutex
	QMap<quint64, CThreadRef>	m_ThreadMap;


	// I/O stats
	mutable QReadWriteLock		m_StatsMutex;
	SProcStats					m_Stats;
	SSambaStats					m_SambaStats;

	SCpuStats					m_CpuStats;
	float						m_CpuStatsDPCUsage;

	quint64						m_InstalledMemory;
	quint64						m_AvailableMemory;
	quint64						m_CommitedMemory;
	quint64						m_MemoryLimit;
	quint64						m_PagedMemory;
	quint64						m_PhysicalUsed;
	quint64						m_CacheMemory;
	quint64						m_ReservedMemory;

	quint32						m_TotalProcesses;
	quint32						m_TotalThreads;
	quint32						m_TotalHandles;

	QVector<SCpuStats>			m_CpusStats;
};
