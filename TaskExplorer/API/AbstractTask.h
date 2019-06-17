#pragma once
#include <qobject.h>
#include "..\Common\Common.h"
#include "../Common/FlexError.h"

struct STaskStats
{
	STaskStats()
	{
		CpuUsage = 0.0f;
		CpuKernelUsage = 0.0f;
		CpuUserUsage = 0.0f;
	}

	void			UpdateStats(quint64 sysTotalTime, quint64 sysTotalCycleTime = 0)
	{
		float newCpuUsage = 0.0f; // Below Windows 7, sum of kernel and user CPU usage; above Windows 7, cycle-based CPU usage.
		float kernelCpuUsage = 0.0f;
		float userCpuUsage = 0.0f;

		if (sysTotalCycleTime != 0)
		{
			float totalDelta;

			newCpuUsage = (float)CycleDelta.Delta / sysTotalCycleTime;

			// Calculate the kernel/user CPU usage based on the kernel/user time. If the kernel
			// and user deltas are both zero, we'll just have to use an estimate. Currently, we
			// split the CPU usage evenly across the kernel and user components, except when the
			// total user time is zero, in which case we assign it all to the kernel component.

			totalDelta = (float)(CpuKernelDelta.Delta + CpuUserDelta.Delta);

			if (totalDelta != 0)
			{
				kernelCpuUsage = newCpuUsage * ((float)CpuKernelDelta.Delta / totalDelta);
				userCpuUsage = newCpuUsage * ((float)CpuUserDelta.Delta / totalDelta);
			}
			else
			{
				if (CpuUserDelta.Value != 0)
				{
					kernelCpuUsage = newCpuUsage / 2;
					userCpuUsage = newCpuUsage / 2;
				}
				else
				{
					kernelCpuUsage = newCpuUsage;
					userCpuUsage = 0;
				}
			}
		}
		else if(sysTotalTime != 0)
		{
			kernelCpuUsage = (float)CpuKernelDelta.Delta / sysTotalTime;
			userCpuUsage = (float)CpuUserDelta.Delta / sysTotalTime;
			newCpuUsage = kernelCpuUsage + userCpuUsage;
		}

		CpuUsage = newCpuUsage;
		CpuKernelUsage = kernelCpuUsage;
		CpuUserUsage = userCpuUsage;
		//
	}

	SDelta64 		CpuKernelDelta;
	SDelta64 		CpuUserDelta;
	SDelta64 		CycleDelta;
	SDelta32 		ContextSwitchesDelta;

	float			CpuUsage; 
	float			CpuKernelUsage;
	float			CpuUserUsage;
};

class CAbstractTask: public QObject
{
	Q_OBJECT

public:
	CAbstractTask(QObject *parent = nullptr) : QObject(parent) 
	{
		m_KernelTime = 0;
		m_UserTime = 0;

		m_Priority = 0;
		m_BasePriority = 0;
		m_PagePriority = 0;
		m_IOPriority = 0;
	}
	
	virtual QDateTime GetCreateTime() const			{ QReadLocker Locker(&m_Mutex); return m_CreateTime; }
	virtual quint64 GetKernelTime()	const			{ QReadLocker Locker(&m_Mutex); return m_KernelTime; }
	virtual quint64 GetUserTime()	const			{ QReadLocker Locker(&m_Mutex); return m_UserTime; }

	virtual long GetPriority()	const				{ QReadLocker Locker(&m_Mutex); return m_Priority; }
	virtual QString GetPriorityString() const = 0;
	virtual STATUS SetPriority(long Value) = 0;
	virtual long GetBasePriority()	const			{ QReadLocker Locker(&m_Mutex); return m_BasePriority; }
	virtual STATUS SetBasePriority(long Value) = 0;
	virtual long GetPagePriority() const			{ QReadLocker Locker(&m_Mutex); return m_PagePriority; }
	virtual STATUS SetPagePriority(long Value) = 0;
	virtual long GetIOPriority() const				{ QReadLocker Locker(&m_Mutex); return m_IOPriority; }
	virtual STATUS SetIOPriority(long Value) = 0;

	virtual STATUS Terminate() = 0;

	virtual bool IsSuspended() const = 0;
	virtual STATUS Suspend() = 0;
	virtual STATUS Resume() = 0;

protected:

	QDateTime		m_CreateTime;
	quint64			m_KernelTime;
	quint64			m_UserTime;

	long			m_Priority;
    long			m_BasePriority;
	long			m_PagePriority;
	long			m_IOPriority;
	
	mutable QReadWriteLock		m_Mutex;

	mutable QReadWriteLock		m_StatsMutex;
};


typedef QSharedPointer<CAbstractTask> CTaskPtr;
typedef QWeakPointer<CAbstractTask> CTaskRef;