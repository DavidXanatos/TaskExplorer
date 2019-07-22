#include "stdafx.h"
#include "LinuxAPI.h"

#include "../TaskExplorer/GUI/TaskExplorer.h"

ulong g_fileObjectTypeIndex = ULONG_MAX;

CLinuxAPI::CLinuxAPI(QObject *parent) : CSystemAPI(parent)
{

}

bool CLinuxAPI::Init()
{
    return true;
}

CLinuxAPI::~CLinuxAPI()
{

}

bool CLinuxAPI::RootAvaiable()
{
    return false;
}

bool CLinuxAPI::UpdateSysStats()
{



	QWriteLocker StatsLocker(&m_StatsMutex);
	m_Stats.UpdateStats();

	return true;
}

bool CLinuxAPI::UpdateProcessList()
{

	return true;
}

bool CLinuxAPI::UpdateSocketList()
{

	return true;
}

bool CLinuxAPI::UpdateOpenFileList()
{

	return true;
}

bool CLinuxAPI::UpdateServiceList(bool bRefresh)
{

	return true;
}

bool CLinuxAPI::UpdateDriverList()
{

	return true; 
}
