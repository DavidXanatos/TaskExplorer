#include "stdafx.h"
#include "ThreadInfo.h"

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
}

CThreadInfo::~CThreadInfo()
{
}