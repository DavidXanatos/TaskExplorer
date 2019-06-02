#include "stdafx.h"
#include "ProcessInfo.h"
#include "HandleInfo.h"


CHandleInfo::CHandleInfo(QObject *parent) : QObject(parent)
{
	m_HandleId = -1;
	m_ProcessId = -1;

	m_Position = 0;
	m_Size = 0;
}

CHandleInfo::~CHandleInfo()
{
}