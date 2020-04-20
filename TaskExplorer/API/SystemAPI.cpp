#include "stdafx.h"
#include "SystemAPI.h"
#include "../Common/Settings.h"
#include "../Common/Xml.h"
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

	LoadPersistentPresets();

	QThread *pThread = new QThread();
	this->moveToThread(pThread);
	pThread->start();
}


CSystemAPI::~CSystemAPI()
{
	StorePersistentPresets();

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

void CSystemAPI::LoadPersistentPresets()
{
	QWriteLocker Locker(&m_PersistentMutex);

	m_PersistentPresets.clear();

	QVariantList Programs = CXml::Read(theConf->GetConfigDir() + "/Processes.xml").toList();
	foreach(const QVariant vMap, Programs)
	{
		CPersistentPresetPtr pPreset = CPersistentPresetPtr(new CPersistentPreset());
		if(pPreset->Load(vMap.toMap()))
			m_PersistentPresets.insert(pPreset->GetPattern().toLower(), pPreset);
	}
}

void CSystemAPI::StorePersistentPresets()
{
	QReadLocker Locker(&m_PersistentMutex);

	QVariantList Programs;
	foreach(const CPersistentPresetPtr& pPreset, m_PersistentPresets)
		Programs.append(pPreset->Store());

	CXml::Write(Programs, theConf->GetConfigDir() + "/Processes.xml");
}

void CSystemAPI::SetPersistentPresets(const QList<CPersistentPresetDataPtr>& PersistentPreset)
{
	QWriteLocker Locker(&m_PersistentMutex);
	QMap<QString, CPersistentPresetPtr>	OldPreset = m_PersistentPresets;

	foreach(const CPersistentPresetDataPtr& Preset, PersistentPreset)
	{
		CPersistentPresetPtr pPreset = OldPreset.take(Preset->sPattern.toLower());
		if (pPreset.isNull()) {
			pPreset = CPersistentPresetPtr(new CPersistentPreset());
			m_PersistentPresets.insert(Preset->sPattern.toLower(), pPreset);
		}
		pPreset->SetData(Preset);
	}

	foreach(const QString& key, OldPreset.keys())
		m_PersistentPresets.remove(key);

	QTimer::singleShot(0,this,SLOT(ApplyPersistentPresets()));
}

void CSystemAPI::ApplyPersistentPresets()
{
	StorePersistentPresets();

	foreach(const CProcessPtr& pProcess, theAPI->GetProcessList())
		pProcess->UpdatePresets();
}

QList<CPersistentPresetDataPtr> CSystemAPI::GetPersistentPresets() const
{
	QReadLocker Locker(&m_PersistentMutex);
	QList<CPersistentPresetDataPtr> Presets;
	foreach(const CPersistentPresetPtr& pPreset, m_PersistentPresets)
		Presets.append(pPreset->GetData());
	return Presets;
}

CPersistentPresetPtr CSystemAPI::FindPersistentPreset(const QString& FileName, const QString& CommandLine)
{
	QReadLocker Locker(&m_PersistentMutex);
	
	CPersistentPresetPtr FoundPreset;
	foreach(const CPersistentPresetPtr& pPreset, m_PersistentPresets)
	{
		if(!FoundPreset.isNull() && FoundPreset->GetPattern().length() > pPreset->GetPattern().length())
			continue; // pick the most exact match

		if (pPreset->Test(FileName, CommandLine))
			FoundPreset = pPreset;
	}
	return FoundPreset;
}

bool CSystemAPI::AddPersistentPreset(const QString& FileName)
{
	QWriteLocker Locker(&m_PersistentMutex);
	if(m_PersistentPresets.contains(FileName.toLower()))
		return false;

	CPersistentPresetPtr pPreset = CPersistentPresetPtr(new CPersistentPreset(FileName));
	m_PersistentPresets.insert(FileName.toLower(), pPreset);

	QTimer::singleShot(0,this,SLOT(ApplyPersistentPresets()));

	return true;
}

bool CSystemAPI::RemovePersistentPreset(const QString& FileName)
{
	QWriteLocker Locker(&m_PersistentMutex);
	
	return m_PersistentPresets.remove(FileName.toLower()) != 0;
}