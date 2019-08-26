#include "stdafx.h"
#include "ProcessInfo.h"
#include "HandleInfo.h"
#include "SystemAPI.h"


CHandleInfo::CHandleInfo(QObject *parent) : CAbstractInfoEx(parent)
{
	m_HandleId = -1;
	m_ProcessId = -1;

	m_Position = 0;
	m_Size = 0;
}

CHandleInfo::~CHandleInfo()
{
}

QSharedPointer<QObject>	CHandleInfo::GetProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	if (m_pProcess.isNull())
	{
		Locker.unlock();

		((CHandleInfo*)this)->SetProcess(theAPI->GetProcessByID(m_ProcessId));
		
		Locker.relock();
	}
	return m_pProcess;
}