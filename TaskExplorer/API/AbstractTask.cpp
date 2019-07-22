#include "stdafx.h"
#include "AbstractTask.h"
#include "../GUI/TaskExplorer.h"


void STaskStats::UpdateStats(quint64 sysTotalTime, quint64 sysTotalCycleTime )
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

CAbstractTask::CAbstractTask(QObject *parent)
	: CAbstractInfoEx(parent)
{
	m_KernelTime = 0;
	m_UserTime = 0;

	m_Priority = 0;
	m_BasePriority = 0;
	m_PagePriority = 0;
	m_IOPriority = 0;

	m_AffinityMask = 0;
}

CAbstractTask::~CAbstractTask()
{
}

