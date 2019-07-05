#include "stdafx.h"
#include "MemoryInfo.h"


CMemoryInfo::CMemoryInfo(QObject *parent) : CAbstractInfoEx(parent)
{
	m_ProcessId = 0;

	m_BaseAddress = 0;
	m_AllocationBase = 0;
    m_AllocationProtect = 0;
    m_RegionSize = 0;
    m_State = 0;
    m_Protect = 0;
    m_Type = 0;

	m_CommittedSize = 0;
	m_PrivateSize = 0;

	m_RegionType = 0;

	m_TotalWorkingSet = 0;
    m_PrivateWorkingSet = 0;
    m_SharedWorkingSet = 0;
    m_ShareableWorkingSet = 0;
    m_LockedWorkingSet = 0;
}

CMemoryInfo::~CMemoryInfo()
{
}

bool CMemoryInfo::IsAllocationBase() const
{
	QReadLocker Locker(&m_Mutex);
	return m_AllocationBaseItem.isNull() || m_AllocationBaseItem.data() == this;
}