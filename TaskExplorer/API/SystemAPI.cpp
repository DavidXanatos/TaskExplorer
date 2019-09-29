#include "stdafx.h"
#include "SystemAPI.h"
#ifdef WIN32
#include "Windows/WindowsAPI.h"
#else
#include "Linux/LinuxAPI.h"
#endif

CSystemAPI*	theAPI = NULL;

int _QHostAddress_type = qRegisterMetaType<QHostAddress>("QHostAddress");

int _QSet_qquint64ype = qRegisterMetaType<QSet<quint64>>("QSet<quint64>");
int _QSet_QString_type = qRegisterMetaType<QSet<QString>>("QSet<QString>");

// When running this in a separate thread, QObject parent must be NULL
CSystemAPI::CSystemAPI(QObject *parent) 
{
	m_PackageCount = 0;
	m_NumaCount = 0;
	m_CoreCount = 0;
	m_CpuCount = 0;
	m_CpuBaseClock = 0;
	m_CpuCurrentClock = 0;

	m_CpuStatsDPCUsage = 0;

	m_InstalledMemory = 0;
	m_AvailableMemory = 0;
	m_CommitedMemory = 0;
	m_CommitedMemoryPeak = 0;
	m_MemoryLimit = 0;
	m_SwapedOutMemory = 0;
	m_TotalSwapMemory = 0;
	m_PagedPool = 0;
	m_PersistentPagedPool = 0;
	m_NonPagedPool = 0;
	m_PhysicalUsed = 0;
	m_CacheMemory = 0;
	m_KernelMemory = 0;
	m_DriverMemory = 0;
	m_ReservedMemory = 0;

	m_TotalProcesses = 0;
	m_TotalThreads = 0;
	m_TotalHandles = 0;

	m_HardwareChangePending = false;

	m_FileListUpdateWatcher = NULL;

	QThread *pThread = new QThread();
	this->moveToThread(pThread);
	pThread->start();
}


CSystemAPI::~CSystemAPI()
{
	this->thread()->quit();

	/*if (m_FileListUpdateWatcher) 
		m_FileListUpdateWatcher->isRunning();*/

	theAPI = NULL;
}

void CSystemAPI::InitAPI()
{
#ifdef WIN32
	theAPI = new CWindowsAPI();
#else
    theAPI = new CLinuxAPI();
#endif
	QMetaObject::invokeMethod(theAPI, "Init", Qt::BlockingQueuedConnection);
}

/*void CSystemAPI::UpdateStats()
{
	QWriteLocker Locker(&m_StatsMutex);
	m_Stats.UpdateStats();
}*/

QMap<quint64, CProcessPtr> CSystemAPI::GetProcessList()
{
	QReadLocker Locker(&m_ProcessMutex);
	return m_ProcessList;
}

CProcessPtr CSystemAPI::GetProcessByID(quint64 ProcessId, bool bAddIfNew)
{
	QReadLocker Locker(&m_ProcessMutex);
	return m_ProcessList.value(ProcessId);
}

QMultiMap<quint64, CSocketPtr> CSystemAPI::GetSocketList()
{
	QReadLocker Locker(&m_SocketMutex);
	return m_SocketList;
}

QMap<quint64, CHandlePtr> CSystemAPI::GetOpenFilesList()
{
	QReadLocker Locker(&m_OpenFilesMutex);
	return m_OpenFilesList;
}

QMap<QString, CServicePtr> CSystemAPI::GetServiceList()
{
	QReadLocker Locker(&m_ServiceMutex);
	return m_ServiceList;
}

CServicePtr CSystemAPI::GetService(const QString& Name)
{
	QReadLocker Locker(&m_ServiceMutex);
	return m_ServiceList.value(Name.toLower());
}

QMap<QString, CDriverPtr> CSystemAPI::GetDriverList()
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

bool CSystemAPI::UpdateOpenFileListAsync(CSystemAPI* This)
{
	return This->UpdateOpenFileList();
}

bool CSystemAPI::UpdateOpenFileListAsync()
{
	if (m_FileListUpdateWatcher)
		return false;

	m_FileListUpdateWatcher = new QFutureWatcher<bool>();
	connect(m_FileListUpdateWatcher, SIGNAL(finished()), this, SLOT(OnOpenFilesUpdated()));
	m_FileListUpdateWatcher->setFuture(QtConcurrent::run(CSystemAPI::UpdateOpenFileListAsync, this));
	return true;
}

void CSystemAPI::OnOpenFilesUpdated()
{
	m_FileListUpdateWatcher->deleteLater();
	m_FileListUpdateWatcher = NULL;
}

void CSystemAPI::NotifyHardwareChanged()
{
	if(!m_HardwareChangePending)
	{
		m_HardwareChangePending = true;
		QTimer::singleShot(100,this,SLOT(OnHardwareChanged())); // OnHardwareChanged must first do m_HardwareChangePending = false;
	}
}