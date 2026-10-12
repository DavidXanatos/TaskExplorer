#include "stdafx.h"
#include "SystemAPI.h"
#include "../../MiscHelpers/Common/Settings.h"
#include "../../MiscHelpers/Common/Xml.h"

//
// Default true, which is what the viewer has always done: somebody sitting at
// the machine, who turned the monitor on from a menu and can turn it off
// again. The daemon overrides it from its own configuration before the system
// is created.
//
static bool g_ProcessBlockingAllowed = true;

void CSystemAPI::SetProcessBlockingAllowed(bool bSet)
{
	g_ProcessBlockingAllowed = bSet;
}

bool CSystemAPI::ProcessBlockingAllowed()
{
	return g_ProcessBlockingAllowed;
}

//
// The same answer, reachable from the ProcessHacker glue.
//
// That file is the seam with phlib and does not include SystemAPI.h - it
// reaches the rest of the program through plain externs, as it already does
// for g_KernelProcessMonitor. This follows the idiom rather than dragging the
// class into it.
//
bool TeProcessBlockingAllowed()
{
	return g_ProcessBlockingAllowed;
}
#ifdef WIN32
#include "Windows/WindowsAPI.h"
#else
#include "Linux/LinuxAPI.h"
#endif
#include "../version.h"
#include "../../MiscHelpers/Common/CredentialStore.h"
#include <QFileInfo>

CSettings*	theConf = NULL;

//
// Defined here rather than in main() so that it lives in the core, which is
// what reads it; the application assigns it during start-up.
//

CSystemPtr	theSystem;

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

	//
	// The three device monitors are created by the collector that has devices to
	// enumerate - CWindowsAPI and CLinuxAPI, in their Init. Nothing creates them
	// for a remote system, whose per-device lists arrive over the wire or not at
	// all, so the base has to null them or the getters hand out whatever was in
	// the heap. That is what crashed CDiskView::UpdateGraphs the moment the
	// panels started following a remote machine:
	//
	//     TaskExplorer!CDiskView::UpdateGraphs+0x88:
	//     mov rax,qword ptr [rsi]  ds:baadf00d`baadf00d=????????????????
	//
	// Every caller has to expect NULL. It is the honest answer for a machine
	// this process is not collecting from.
	//
	m_pGpuMonitor = NULL;
	m_pNetMonitor = NULL;
	m_pDiskMonitor = NULL;

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
}

//
// Puts the platform's wording behind CStatus::Native().
//
// CStatus lives in CoreHelpers, which is below the platform layer and cannot
// reach PhGetStatusMessage or strerror. So the formatter is installed rather
// than called: this is the one place that knows both, and it runs before
// anything can fail.
//
//
// Where this product's key store lives.
//
// One store per user, not one per program: the keys belong to whoever is
// sitting there, not to whichever of the three executables they happened to
// type. A viewer and a console that each kept their own would mean two files,
// two passwords, and a key saved in one that the other cannot see.
//
// A portable installation already shares one directory between all three, so
// there is nothing to redirect; only the per-application layout needs its last
// component replaced with the product's own name.
//
// Here rather than in CCredentialStore because it is a question about this
// product - CoreHelpers has neither a product nor an opinion about directories,
// which is what lets the store be reused.
//
void SetupCredentialStore()
{
	if (!theConf)
		return;

	//
	// Beside the configuration when that directory is ours outright - a portable
	// copy's, or one named outright by a service, which has the same meaning:
	// /etc/taskexplorer holds the daemon's settings and its key, and its
	// credentials belong there too rather than in an /etc/TaskExplorer invented
	// beside it.
	//
	const QString Dir = (theConf->IsPortable() || theConf->IsExplicitDir())
		? theConf->GetConfigDir()
		: QFileInfo(theConf->GetConfigDir()).absolutePath() + "/" MY_PRODUCT_NAME_STRING;

	CCredentialStore::Setup(Dir + "/" MY_PRODUCT_NAME_STRING "Creds.dat");
}

void InstallNativeStatusFormatter()
{
	CStatus::SetNativeFormatter(&FormatNativeStatus);
}

void CSystemAPI::InitLocalSystem()
{
	InstallNativeStatusFormatter();

	//
	// The system moves itself into its own thread in the constructor, so it has
	// to be destroyed there too - hence deleteLater as the deleter rather than
	// letting whichever thread drops the last reference call delete directly.
	//
#ifdef WIN32
	theSystem = CSystemPtr(new CWindowsAPI(), &QObject::deleteLater);
#else
	theSystem = CSystemPtr(new CLinuxAPI(), &QObject::deleteLater);
#endif
	QMetaObject::invokeMethod(theSystem.data(), "Init", Qt::BlockingQueuedConnection);
}

/*void CSystemAPI::UpdateStats()
{
	QWriteLocker Locker(&m_StatsMutex);
	m_Stats.UpdateStats();
}*/

//
// Conservative default: a facility is unavailable unless a backend says
// otherwise. Getting this wrong in this direction hides a working feature,
// which is recoverable; the other direction offers one that then fails.
//
bool CSystemAPI::HasCapability(ECapability Capability) const
{
	switch (Capability)
	{
	case eCapRoot:				return const_cast<CSystemAPI*>(this)->RootAvaiable();
	case eCapMemoryRead:		return true;
	case eCapServiceControl:	return true;
	default:					return false;
	}
}

QMap<quint64, CProcessPtr> CSystemAPI::GetProcessList()
{
	QReadLocker Locker(&m_ProcessMutex);
	return m_ProcessByPID;
}

QMap<SProcessUID, CProcessPtr> CSystemAPI::GetProcessMap()
{
	QReadLocker Locker(&m_ProcessMutex);
	return m_ProcessMap;
}

CProcessPtr CSystemAPI::GetProcessByID(quint64 ProcessId, bool bAddIfNew)
{
	QReadLocker Locker(&m_ProcessMutex);
	return m_ProcessByPID.value(ProcessId);
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
	return m_ServiceList.value(CanonicalServiceName(Name));
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
	m_FileListUpdateWatcher->setFuture(QtConcurrent::run([this] (){
		return CSystemAPI::UpdateOpenFileListAsync(this);
	}));
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

	foreach(const CProcessPtr& pProcess, GetProcessList())
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

void CSystemAPI::ResetAll()
{
	QWriteLocker Locker(&m_ProcessMutex);
	m_ProcessByPID.clear();
	m_ProcessMap.clear();
	m_ThreadMap.clear();
	Locker.unlock();

	QWriteLocker SocketLocker(&m_SocketMutex);
	m_SocketList.clear();
	SocketLocker.unlock();

	QWriteLocker OpenFilesLocker(&m_OpenFilesMutex);
	m_OpenFilesList.clear();
	OpenFilesLocker.unlock();

	QWriteLocker ServiceLocker(&m_ServiceMutex);
	m_ServiceList.clear();
	ServiceLocker.unlock();

	QWriteLocker DriverLocker(&m_DriverMutex);
	m_DriverList.clear();
	DriverLocker.unlock();

	UpdateAll();
}