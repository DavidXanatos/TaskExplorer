#include "stdafx.h"
#include "ThreadInfo.h"

#include "../TaskExplorer/GUI/TaskExplorer.h"

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