#include "stdafx.h"
#include "ThreadInfo.h"

#include "../TaskExplorer/GUI/TaskExplorer.h"

CThreadInfo::CThreadInfo(QObject *parent) : QObject(parent)
{
	m_ThreadId = -1;
	m_ProcessId = -1;

	m_KernelTime = 0;
	m_UserTime = 0;

	m_Priority = 0;
    m_BasePriority = 0;
	m_State = 0;
	m_WaitReason = 0;

	m_IsMainThread = false;

	m_CpuUsage = 0.0f;
	m_CpuKernelUsage = 0.0f;
	m_CpuUserUsage = 0.0f;
}

CThreadInfo::~CThreadInfo()
{
	theAPI->ClearThread(m_ThreadId);
}