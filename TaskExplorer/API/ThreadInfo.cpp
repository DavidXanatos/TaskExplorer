#include "stdafx.h"
#include "ThreadInfo.h"
#include "SystemAPI.h"

CThreadInfo::CThreadInfo(QObject *parent) : CAbstractTask(parent)
{
	m_ThreadId = -1;
	m_ProcessId = -1;

	m_State = 0;
	m_WaitReason = 0;

	m_IsMainThread = false;

}

CThreadInfo::~CThreadInfo()
{
	theAPI->ClearThread(m_ThreadId);
}

QSharedPointer<QObject>	CThreadInfo::GetProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	if (m_pProcess.isNull())
	{
		Locker.unlock();

		((CThreadInfo*)this)->SetProcess(theAPI->GetProcessByID(m_ProcessId));
		
		Locker.relock();
	}
	return m_pProcess;
}