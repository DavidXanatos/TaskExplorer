#pragma once
#include <qobject.h>

#include "ModuleInfo.h"
#include "ThreadInfo.h"
#include "HandleInfo.h"
#include "../Common/Common.h"
#include "IOStats.h"

#ifndef PROCESS_PRIORITY_CLASS_UNKNOWN
#define PROCESS_PRIORITY_CLASS_UNKNOWN 0
#define PROCESS_PRIORITY_CLASS_IDLE 1
#define PROCESS_PRIORITY_CLASS_NORMAL 2
#define PROCESS_PRIORITY_CLASS_HIGH 3
#define PROCESS_PRIORITY_CLASS_REALTIME 4
#define PROCESS_PRIORITY_CLASS_BELOW_NORMAL 5
#define PROCESS_PRIORITY_CLASS_ABOVE_NORMAL 6
#endif

class CProcessInfo: public QObject
{
	Q_OBJECT

public:
	CProcessInfo(QObject *parent = nullptr);
	virtual ~CProcessInfo();

	// Basic
	virtual quint64 GetID() const					{ QReadLocker Locker(&m_Mutex); return m_ProcessId; }
	virtual quint64 GetParentID() const				{ QReadLocker Locker(&m_Mutex); return m_ParentProcessId; }
	virtual QString GetName() const					{ QReadLocker Locker(&m_Mutex); return m_ProcessName; }

	virtual QDateTime GetCreateTime() const			{ QReadLocker Locker(&m_Mutex); return m_CreateTime; }

	virtual quint64 GetSessionID() const = 0;

	// Parameters
	virtual QString GetFileName() const				{ QReadLocker Locker(&m_Mutex); return m_FileName; }
	virtual QString GetCommandLine() const			{ QReadLocker Locker(&m_Mutex); return m_CommandLine; }

	// Other fields
	virtual QString GetUserName() const				{ QReadLocker Locker(&m_Mutex); return m_UserName; }

	// Dynamic
	virtual ulong GetPriorityClass() const			{ QReadLocker Locker(&m_Mutex); return m_PriorityClass; }
	virtual QString GetPriorityString() const;
	virtual ulong GetKernelTime() const				{ QReadLocker Locker(&m_Mutex); return m_KernelTime; }
	virtual qint64 GetUserTime() const				{ QReadLocker Locker(&m_Mutex); return m_UserTime; }
	virtual ulong GetNumberOfThreads() const		{ QReadLocker Locker(&m_Mutex); return m_NumberOfThreads; }
	virtual ulong GetNumberOfHandles() const		{ QReadLocker Locker(&m_Mutex); return m_NumberOfHandles; }

	virtual size_t GetWorkingSetPrivateSize() const	{ QReadLocker Locker(&m_Mutex); return m_WorkingSetPrivateSize; }
	virtual ulong GetPeakNumberOfThreads() const	{ QReadLocker Locker(&m_Mutex); return m_PeakNumberOfThreads; }
	virtual ulong GetHardFaultCount() const			{ QReadLocker Locker(&m_Mutex); return m_HardFaultCount; }

	virtual float GetCpuUsage() const				{ QReadLocker Locker(&m_Mutex); return m_CpuUsage; }
	virtual float GetCpuKernelUsage() const			{ QReadLocker Locker(&m_Mutex); return m_CpuKernelUsage; }
	virtual float GetCpuUserUsage() const			{ QReadLocker Locker(&m_Mutex); return m_CpuUserUsage; }

	virtual SProcStats	GetStats()					{ QReadLocker Locker(&m_StatsMutex); return m_Stats; }

	virtual CModulePtr GetModuleInfo()				{ QReadLocker Locker(&m_Mutex); return m_pModuleInfo; }

	// Threads
	virtual QMap<quint64, CThreadPtr> GetThreadList()	{ QReadLocker Locker(&m_ThreadMutex); return m_ThreadList; }

	// Handles
	virtual QMap<quint64, CHandlePtr> GetHandleList()	{ QReadLocker Locker(&m_HandleMutex); return m_HandleList; }
	
	// Handles
	virtual QMap<quint64, CModulePtr> GetModleList()	{ QReadLocker Locker(&m_ModuleMutex); return m_ModuleList; }

signals:
	void			ThreadsUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void			HandlesUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void			ModulesUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);

public slots:
	virtual bool	UpdateThreads() = 0;
	virtual bool	UpdateHandles() = 0;
	virtual bool	UpdateModules() = 0;

protected:
	// Basic
	quint64							m_ProcessId;
	quint64							m_ParentProcessId;
	QString							m_ProcessName;

	QDateTime						m_CreateTime;

	// Parameters
	QString							m_FileName;
	QString							m_CommandLine;
	
	// Other fields
	QString							m_UserName;

	// Dynamic
	ulong							m_PriorityClass;
	qint64							m_KernelTime;
	qint64							m_UserTime;
	ulong							m_NumberOfThreads;
	ulong							m_NumberOfHandles;

	size_t							m_WorkingSetPrivateSize;
	ulong							m_PeakNumberOfThreads;
	ulong							m_HardFaultCount;

	float							m_CpuUsage; 
	float							m_CpuKernelUsage;
	float							m_CpuUserUsage;

	SDelta64 						m_CpuKernelDelta;
	SDelta64 						m_CpuUserDelta;
	SDelta64 						m_CycleTimeDelta; // since WIN7

	SDelta32 						m_ContextSwitchesDelta;
	SDelta32 						m_PageFaultsDelta;
	SDelta64 						m_PrivateBytesDelta;

		

	// I/O stats
	mutable QReadWriteLock			m_StatsMutex;
	SProcStats						m_Stats;

	// module info
	CModulePtr						m_pModuleInfo;

	mutable QReadWriteLock			m_Mutex;

	// Threads
	mutable QReadWriteLock			m_ThreadMutex;
	QMap<quint64, CThreadPtr>		m_ThreadList;

	// Handles
	mutable QReadWriteLock			m_HandleMutex;
	QMap<quint64, CHandlePtr>		m_HandleList;

	// Modules
	mutable QReadWriteLock			m_ModuleMutex;
	QMap<quint64, CModulePtr>		m_ModuleList;

};

typedef QSharedPointer<CProcessInfo> CProcessPtr;
typedef QWeakPointer<CProcessInfo> CProcessRef;