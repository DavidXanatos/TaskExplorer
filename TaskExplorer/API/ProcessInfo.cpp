#include "stdafx.h"
#include "ProcessInfo.h"


CProcessInfo::CProcessInfo(QObject *parent) : QObject(parent)
{
	// Basic
	m_ProcessId = -1;
	m_ParentProcessId = -1;

	// Dynamic
	m_PriorityClass = 0;
	m_KernelTime = 0;
	m_UserTime = 0;
	m_NumberOfThreads = 0;

	m_WorkingSetPrivateSize = 0;
	m_PeakNumberOfThreads = 0;
	m_HardFaultCount = 0;
}

CProcessInfo::~CProcessInfo()
{
}

void CProcessInfo::CleanUp()
{
	// TODO: clean up ald detailed infos like thread details, handles, etc.. if outdated
}

QString CProcessInfo::GetPriorityString() const
{
	switch (m_PriorityClass)
    {
	case PROCESS_PRIORITY_CLASS_REALTIME:		return tr("Real time");
    case PROCESS_PRIORITY_CLASS_HIGH:			return tr("High");
    case PROCESS_PRIORITY_CLASS_ABOVE_NORMAL:	return tr("Above normal");
    case PROCESS_PRIORITY_CLASS_NORMAL:			return tr("Normal");
	case PROCESS_PRIORITY_CLASS_BELOW_NORMAL:	return tr("Below normal");
    case PROCESS_PRIORITY_CLASS_IDLE:			return tr("Idle");
	default:									return tr("Unknown");
    }
}