#pragma once
#include <qobject.h>
#include "..\Common\Common.h"
#include "../Common/FlexError.h"
#include "AbstractInfo.h"

struct STaskStats
{
	STaskStats()
	{
		CpuUsage = 0.0f;
		CpuKernelUsage = 0.0f;
		CpuUserUsage = 0.0f;
	}

	void			UpdateStats(quint64 sysTotalTime, quint64 sysTotalCycleTime = 0);

	SDelta64 		CpuKernelDelta;
	SDelta64 		CpuUserDelta;
	SDelta64 		CycleDelta;
	SDelta32 		ContextSwitchesDelta;

	float			CpuUsage; 
	float			CpuKernelUsage;
	float			CpuUserUsage;
};

class CAbstractTask : public CAbstractInfoEx
{
	Q_OBJECT

public:
	CAbstractTask(QObject *parent = nullptr);
	virtual ~CAbstractTask();
	
	virtual quint64 GetKernelTime()	const			{ QReadLocker Locker(&m_Mutex); return m_KernelTime; }
	virtual quint64 GetUserTime()	const			{ QReadLocker Locker(&m_Mutex); return m_UserTime; }

	virtual QString GetName() const = 0;
	virtual long GetPriority()	const				{ QReadLocker Locker(&m_Mutex); return m_Priority; }
	virtual QString GetPriorityString() const = 0;
	virtual STATUS SetPriority(long Value) = 0;
	virtual long GetBasePriority()	const			{ QReadLocker Locker(&m_Mutex); return m_BasePriority; }
	virtual STATUS SetBasePriority(long Value) = 0;
	virtual long GetPagePriority() const			{ QReadLocker Locker(&m_Mutex); return m_PagePriority; }
	virtual STATUS SetPagePriority(long Value) = 0;
	virtual long GetIOPriority() const				{ QReadLocker Locker(&m_Mutex); return m_IOPriority; }
	virtual STATUS SetIOPriority(long Value) = 0;

	virtual quint64 GetAffinityMask() const				{ QReadLocker Locker(&m_Mutex); return m_AffinityMask; }
	virtual STATUS SetAffinityMask(quint64 Value) = 0;

	virtual STATUS Terminate() = 0;

	virtual bool IsSuspended() const = 0;
	virtual STATUS Suspend() = 0;
	virtual STATUS Resume() = 0;


protected:
	quint64						m_KernelTime;
	quint64						m_UserTime;

	long						m_Priority;
    long						m_BasePriority;
	long						m_PagePriority;
	long						m_IOPriority;

	quint64						m_AffinityMask;

	mutable QReadWriteLock		m_StatsMutex;
};


typedef QSharedPointer<CAbstractTask> CTaskPtr;
typedef QWeakPointer<CAbstractTask> CTaskRef;