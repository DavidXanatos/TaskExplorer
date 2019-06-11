#include "stdafx.h"
#include "SystemAPI.h"
#include "Windows\WindowsAPI.h"


CSystemAPI::CSystemAPI(QObject *parent): QObject(parent)
{
	m_CpuStatsDPCUsage = 0;

	m_InstalledMemory = 0;
	m_AvailableMemory = 0;
	m_CommitedMemory = 0;
	m_MemoryLimit = 0;
	m_PagedMemory = 0;
	m_PhysicalUsed = 0;
	m_CacheMemory = 0;
	m_ReservedMemory = 0;

	m_TotalProcesses = 0;
	m_TotalThreads = 0;
	m_TotalHandles = 0;
}


CSystemAPI::~CSystemAPI()
{
}

CSystemAPI* CSystemAPI::New(QObject *parent)
{
#ifdef WIN32
	return new CWindowsAPI(parent);
#else
	// todo: implement other systems liek Linux
#endif
}

void CSystemAPI::UpdateStats()
{
	QWriteLocker Locker(&m_StatsMutex);
	m_Stats.UpdateStats();
}

QMap<quint64, CProcessPtr> CSystemAPI::GetProcessList()
{
	QReadLocker Locker(&m_ProcessMutex);
	return m_ProcessList;
}

CProcessPtr CSystemAPI::GetProcessByID(quint64 ProcessId)
{
	QReadLocker Locker(&m_ProcessMutex);
	return m_ProcessList.value(ProcessId);
}

QMultiMap<quint64, CSocketPtr> CSystemAPI::GetSocketList()
{
	QReadLocker Locker(&m_SocketMutex);
	return m_SocketList;
}

QMultiMap<quint64, CHandlePtr> CSystemAPI::GetOpenFilesList()
{
	QReadLocker Locker(&m_OpenFilesMutex);
	return m_OpenFilesList;
}

QMultiMap<QString, CServicePtr> CSystemAPI::GetServiceList()
{
	QReadLocker Locker(&m_ServiceMutex);
	return m_ServiceList;
}


QMultiMap<QString, CServicePtr> CSystemAPI::GetDriverList()
{
	QReadLocker Locker(&m_DriverMutex);
	return m_DriverList;
}

void CSystemAPI::AddThread(CThreadPtr pThread)
{
	QWriteLocker Locker(&m_ProcessMutex);
	m_ThreadMap.insert(pThread->GetThreadId(), pThread);
}

void CSystemAPI::ClearThread(quint64 ThreadId)
{
	QWriteLocker Locker(&m_ProcessMutex);
	m_ThreadMap.remove(ThreadId);
}

CThreadPtr CSystemAPI::GetThreadByID(quint64 ThreadId)
{
	QReadLocker Locker(&m_ProcessMutex);
	return m_ThreadMap.value(ThreadId);
}

CProcessPtr CSystemAPI::GetProcessByThreadID(quint64 ThreadId)
{
	CThreadPtr pThread = GetThreadByID(ThreadId);
	if (pThread.isNull())
		return CProcessPtr();
	return GetProcessByID(pThread->GetProcessId());
}
