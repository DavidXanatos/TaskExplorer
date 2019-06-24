#include "stdafx.h"
#include "ServiceInfo.h"


CServiceInfo::CServiceInfo(QObject *parent) : CAbstractInfoEx(parent)
{
	m_ProcessId = 0;
}

CServiceInfo::~CServiceInfo()
{
}
