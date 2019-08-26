#include "stdafx.h"
#include "AbstractTask.h"


void STaskStats::UpdateStats(quint64 sysTotalTime, quint64 sysTotalCycleTime)
{

	if (sysTotalCycleTime != 0)
	{
		// Below Windows 7, sum of kernel and user CPU usage; above Windows 7, cycle-based CPU usage.
		float newCpuUsage = (float)CycleDelta.Delta / sysTotalCycleTime;

		// Calculate the kernel/user CPU usage based on the kernel/user time. If the kernel
		// and user deltas are both zero, we'll just have to use an estimate. Currently, we
		// split the CPU usage evenly across the kernel and user components, except when the
		// total user time is zero, in which case we assign it all to the kernel component.

		float  totalDelta = (float)(CpuKernelDelta.Delta + CpuUserDelta.Delta);

		if (totalDelta != 0)
		{
			CpuKernelUsage = newCpuUsage * ((float)CpuKernelDelta.Delta / totalDelta);
			CpuUsage = newCpuUsage * ((float)CpuUserDelta.Delta / totalDelta);
		}
		else if (CpuUserDelta.Value != 0)
		{
			CpuKernelUsage = newCpuUsage / 2;
			CpuUsage = newCpuUsage / 2;
		}
		else
		{
			CpuKernelUsage = newCpuUsage;
			CpuUsage = 0;
		}
	}
	else if(sysTotalTime != 0)
	{
		CpuKernelUsage = (float)CpuKernelDelta.Delta / sysTotalTime;
		CpuUserUsage = (float)CpuUserDelta.Delta / sysTotalTime;
		CpuUsage = CpuKernelUsage + CpuUserUsage;
	}
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

