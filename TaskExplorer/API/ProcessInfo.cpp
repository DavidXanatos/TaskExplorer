#include "stdafx.h"
#include "ProcessInfo.h"


CProcessInfo::CProcessInfo(QObject *parent) : CAbstractTask(parent)
{
	// Basic
	m_ProcessId = -1;
	m_ParentProcessId = -1;

	// Dynamic
	m_NumberOfThreads = 0;
	m_NumberOfHandles = 0;

	m_WorkingSetPrivateSize = 0;
	m_PeakNumberOfThreads = 0;

	m_GpuDedicatedUsage = 0;
	m_GpuSharedUsage = 0;
}

CProcessInfo::~CProcessInfo()
{
}
