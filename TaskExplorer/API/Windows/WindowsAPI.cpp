#include "stdafx.h"
#include "WindowsAPI.h"
#include "ProcessHacker.h"
#include "WinHandle.h"
#include <lm.h>
#include "Monitors/WinGpuMonitor.h"
#include "Monitors/WinNetMonitor.h"
#include "Monitors/WinDiskMonitor.h"

#include "../GUI/TaskExplorer.h"
#include "../SVC/TaskService.h"

extern "C" {
#include <winsta.h>
}

ulong g_fileObjectTypeIndex = ULONG_MAX;
struct SWindowsAPI
{
	SWindowsAPI()
	{
		PowerInformation = NULL;
		InterruptInformation = NULL;
		CurrentPerformanceDistribution = NULL;
		PreviousPerformanceDistribution = NULL;

		CpuIdleCycleTime = NULL;
		CpuSystemCycleTime = NULL;

		memset(&DpcsProcessInformation, 0, sizeof(SYSTEM_PROCESS_INFORMATION));
		RtlInitUnicodeString(&DpcsProcessInformation.ImageName, L"DPCs");
		DpcsProcessInformation.UniqueProcessId = DPCS_PROCESS_ID;
		DpcsProcessInformation.InheritedFromUniqueProcessId = SYSTEM_IDLE_PROCESS_ID;

		memset(&InterruptsProcessInformation, 0, sizeof(SYSTEM_PROCESS_INFORMATION));
		RtlInitUnicodeString(&InterruptsProcessInformation.ImageName, L"Interrupts");
		InterruptsProcessInformation.UniqueProcessId = INTERRUPTS_PROCESS_ID;
		InterruptsProcessInformation.InheritedFromUniqueProcessId = SYSTEM_IDLE_PROCESS_ID;
	}

	~SWindowsAPI()
	{
		delete[] PowerInformation;
		delete[] InterruptInformation;
		if (CurrentPerformanceDistribution)
			PhFree(CurrentPerformanceDistribution);
		if (PreviousPerformanceDistribution)
			PhFree(PreviousPerformanceDistribution);

		delete[] CpuIdleCycleTime;

		delete[] CpuSystemCycleTime;
	}

	PPROCESSOR_POWER_INFORMATION PowerInformation;
	PSYSTEM_INTERRUPT_INFORMATION InterruptInformation;
	PSYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION CurrentPerformanceDistribution;
	PSYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION PreviousPerformanceDistribution;

	SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION CpuTotals;

	PLARGE_INTEGER CpuIdleCycleTime;
	PLARGE_INTEGER CpuSystemCycleTime;

	SDelta64 CpuIdleCycleDelta;
	SDelta64 CpuSystemCycleDelta;

	SYSTEM_PROCESS_INFORMATION DpcsProcessInformation;
	SYSTEM_PROCESS_INFORMATION InterruptsProcessInformation;

	// ensure we handle overflows of this coutners properly
	SUnOverflow PageReadCount;
	SUnOverflow CacheReadCount;

	SUnOverflow MappedPagesWriteCount;
	SUnOverflow DirtyPagesWriteCount;
	SUnOverflow CcLazyWritePages;

};

quint64 GetInstalledMemory()
{
	static BOOL (WINAPI *getPhysicallyInstalledSystemMemory)(PULONGLONG) = NULL;

	if (!getPhysicallyInstalledSystemMemory) 
		getPhysicallyInstalledSystemMemory = (BOOL(WINAPI*)(PULONGLONG))PhGetDllProcedureAddress(L"kernel32.dll", "GetPhysicallyInstalledSystemMemory", 0);

	ULONGLONG InstalledMemory = 0;
    if (getPhysicallyInstalledSystemMemory && getPhysicallyInstalledSystemMemory(&InstalledMemory))
    {
		InstalledMemory *= 1024;
    }
    else
    {
		InstalledMemory = UInt32x32To64(PhSystemBasicInformation.NumberOfPhysicalPages, PAGE_SIZE);
    }

	return InstalledMemory;
}

CWindowsAPI::CWindowsAPI(QObject *parent) : CSystemAPI(parent)
{
	m_TotalGuiObjects = 0;
	m_TotalUserObjects = 0;
	m_TotalWndObjects = 0;

	//m_UseDiskCounters = eDontUse;
	m_UseDiskCounters = false;

	m_InstalledMemory = 0;
	m_AvailableMemory = 0;
	m_ReservedMemory = 0;

	m_pEventMonitor = NULL;
	m_pGpuMonitor = NULL;
	m_pSymbolProvider = NULL;

	m_bTestSigning = false;
	m_bDriverFailed = false;

	m = new SWindowsAPI();
}

bool CWindowsAPI::Init()
{
	InitPH();

	SYSTEM_CODEINTEGRITY_INFORMATION sci = {sizeof(SYSTEM_CODEINTEGRITY_INFORMATION)};
	if(NT_SUCCESS(NtQuerySystemInformation(SystemCodeIntegrityInformation, &sci, sizeof(sci), NULL)))
	{
		bool bCoseIntegrityEnabled = !!(sci.CodeIntegrityOptions & /*CODEINTEGRITY_OPTION_ENABLED*/ 0x1);
		bool bTestSigningEnabled = !!(sci.CodeIntegrityOptions & /*CODEINTEGRITY_OPTION_TESTSIGN*/ 0x2);

		m_bTestSigning = (!bCoseIntegrityEnabled || bTestSigningEnabled);
	}

	int iUseDriver = theConf->GetInt("Options/UseDriver", 1);
	if (!PhIsExecutingInWow64() && iUseDriver != 0)
	{
		m_DriverFileName = theConf->GetString("Options/DriverFile");
		QString DeviceName;
		if (!m_DriverFileName.isEmpty())
			DeviceName = theConf->GetString("Options/DriverDevice");
		else
		{
			if (QFile::exists(QApplication::applicationDirPath() + "/xprocesshacker.sys"))
			{
				DeviceName = "XProcessHacker3";
				m_DriverFileName = "xprocesshacker.sys";
			}
			else
			{
				DeviceName = QString::fromWCharArray(KPH_DEVICE_SHORT_NAME);
				m_DriverFileName = "kprocesshacker.sys";
			}
		}

		STATUS Status = InitKPH(DeviceName, m_DriverFileName, iUseDriver == 1);

		if (Status.IsError())
		{
			qDebug() << Status.GetText();
			m_bDriverFailed = true;
		}
		else
			qDebug() << tr("Process Hacker kernel driver connected.");
	}

	static PH_INITONCE initOnce = PH_INITONCE_INIT;
	if (PhBeginInitOnce(&initOnce))
    {
        UNICODE_STRING fileTypeName = RTL_CONSTANT_STRING(L"File");

        g_fileObjectTypeIndex = PhGetObjectTypeNumber(&fileTypeName);

        PhEndInitOnce(&initOnce);
    }

	m_CpusStats.resize(PhSystemBasicInformation.NumberOfProcessors);

	if (!InitCpuCount())
	{
		m_NumaCount = 1;
		m_PackageCount = 1;
		m_CoreCount = PhSystemBasicInformation.NumberOfProcessors;
	}
	// Note: we must ensure m_CpuCount is the same as NumberOfProcessors
	m_CpuCount = PhSystemBasicInformation.NumberOfProcessors;

	m->CpuIdleCycleTime = new LARGE_INTEGER[PhSystemBasicInformation.NumberOfProcessors];
	memset(m->CpuIdleCycleTime, 0, sizeof(LARGE_INTEGER)*PhSystemBasicInformation.NumberOfProcessors);
	
	m->CpuSystemCycleTime = new LARGE_INTEGER[PhSystemBasicInformation.NumberOfProcessors];
	memset(m->CpuSystemCycleTime, 0, sizeof(LARGE_INTEGER)*PhSystemBasicInformation.NumberOfProcessors);

	m_InstalledMemory = ::GetInstalledMemory();
	m_AvailableMemory = UInt32x32To64(PhSystemBasicInformation.NumberOfPhysicalPages, PAGE_SIZE);
	m_ReservedMemory = m_InstalledMemory - UInt32x32To64(PhSystemBasicInformation.NumberOfPhysicalPages, PAGE_SIZE);

	m->PowerInformation =  new PROCESSOR_POWER_INFORMATION[PhSystemBasicInformation.NumberOfProcessors];
	if (!NT_SUCCESS(NtPowerInformation(ProcessorInformation, NULL, 0, m->PowerInformation, sizeof(PROCESSOR_POWER_INFORMATION) * PhSystemBasicInformation.NumberOfProcessors)))
		memset(m->PowerInformation, 0, sizeof(PROCESSOR_POWER_INFORMATION)*PhSystemBasicInformation.NumberOfProcessors);

	m->InterruptInformation = new SYSTEM_INTERRUPT_INFORMATION[PhSystemBasicInformation.NumberOfProcessors];


	CHAR brandString[49];
    NtQuerySystemInformation(SystemProcessorBrandString, brandString, sizeof(brandString), NULL);
	m_CPU_String = QString(brandString);

	InitWindowsInfo();
	
	wchar_t machine[UNCLEN + 1];
	DWORD machine_len = UNCLEN + 1;
    GetComputerName(machine, &machine_len);
	m_HostName = QString::fromWCharArray(machine);

	wchar_t user[UNLEN + 1];
	DWORD user_len = UNLEN + 1;
	GetUserName(user, &user_len);
	m_UserName = QString::fromWCharArray(user);

	m_SystemDir = CastPhString(PhGetSystemDirectory());

	if (theConf->GetBool("Options/MonitorETW", false))
		MonitorETW(true);

	m_pGpuMonitor = new CWinGpuMonitor();
	m_pGpuMonitor->Init();

	m_pNetMonitor = new CWinNetMonitor();
	m_pNetMonitor->Init();

	m_pDiskMonitor = new CWinDiskMonitor();
	m_pDiskMonitor->Init();

	m_pSymbolProvider = CSymbolProviderPtr(new CSymbolProvider());
	m_pSymbolProvider->Init();

	return true;
}

void CWindowsAPI::MonitorETW(bool bEnable)
{
	if (bEnable == (m_pEventMonitor != NULL))
		return;

	if (bEnable)
	{
		m_pEventMonitor = new CEventMonitor();

		connect(m_pEventMonitor, SIGNAL(NetworkEvent(int, quint64, quint64, ulong, ulong, const QHostAddress&, quint16, const QHostAddress&, quint16)), this, SLOT(OnNetworkEvent(int, quint64, quint64, ulong, ulong, QHostAddress, quint16, QHostAddress, quint16)));
		connect(m_pEventMonitor, SIGNAL(FileEvent(int, quint64, quint64, quint64, const QString&)), this, SLOT(OnFileEvent(int, quint64, quint64, quint64, const QString&)));
		connect(m_pEventMonitor, SIGNAL(DiskEvent(int, quint64, quint64, quint64, ulong, ulong, quint64)), this, SLOT(OnDiskEvent(int, quint64, quint64, quint64, ulong, ulong, quint64)));

		if (m_pEventMonitor->Init())
			return;
	}

	delete m_pEventMonitor;
	m_pEventMonitor = NULL;
}

CWindowsAPI::~CWindowsAPI()
{
	delete m_pEventMonitor;

	delete m_pGpuMonitor;
	delete m_pNetMonitor;
	delete m_pDiskMonitor;

	delete m;
}

bool CWindowsAPI::RootAvaiable()
{
	return PhGetOwnTokenAttributes().Elevated;
}

quint64 CWindowsAPI::UpdateCpuStats(bool SetCpuUsage)
{
	QWriteLocker Locker(&m_StatsMutex);

	PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION CpuInformation = new SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION[PhSystemBasicInformation.NumberOfProcessors];
    NtQuerySystemInformation(SystemProcessorPerformanceInformation, CpuInformation, sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION) * (ULONG)PhSystemBasicInformation.NumberOfProcessors, NULL);

	memset(&m->CpuTotals, 0, sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION));
	

	quint64 totalTime;
    for (ulong i = 0; i < (ulong)PhSystemBasicInformation.NumberOfProcessors; i++)
    {
        PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION cpuInfo = &CpuInformation[i];

        cpuInfo->KernelTime.QuadPart -= cpuInfo->IdleTime.QuadPart;

        m->CpuTotals.DpcTime.QuadPart += cpuInfo->DpcTime.QuadPart;
        m->CpuTotals.IdleTime.QuadPart += cpuInfo->IdleTime.QuadPart;
        m->CpuTotals.InterruptCount += cpuInfo->InterruptCount;
        m->CpuTotals.InterruptTime.QuadPart += cpuInfo->InterruptTime.QuadPart;
        m->CpuTotals.KernelTime.QuadPart += cpuInfo->KernelTime.QuadPart;
        m->CpuTotals.UserTime.QuadPart += cpuInfo->UserTime.QuadPart;

		SCpuStats& CpuStats = m_CpusStats[i];

		CpuStats.KernelDelta.Update(cpuInfo->KernelTime.QuadPart);
		CpuStats.UserDelta.Update(cpuInfo->UserTime.QuadPart);
		CpuStats.IdleDelta.Update(cpuInfo->IdleTime.QuadPart);

        if (SetCpuUsage)
        {
            totalTime = CpuStats.KernelDelta.Delta + CpuStats.UserDelta.Delta + CpuStats.IdleDelta.Delta;

			CpuStats.KernelUsage = totalTime != 0 ? (float)CpuStats.KernelDelta.Delta / totalTime : 0.0f;
			CpuStats.UserUsage = totalTime != 0 ? (float)CpuStats.UserDelta.Delta / totalTime : 0.0f;
        }
    }

	m_CpuStats.KernelDelta.Update(m->CpuTotals.KernelTime.QuadPart);
	m_CpuStats.UserDelta.Update(m->CpuTotals.UserTime.QuadPart);
	m_CpuStats.IdleDelta.Update(m->CpuTotals.IdleTime.QuadPart);

    totalTime = m_CpuStats.KernelDelta.Delta + m_CpuStats.UserDelta.Delta + m_CpuStats.IdleDelta.Delta;

    if (SetCpuUsage)
    {
		totalTime = m_CpuStats.KernelDelta.Delta + m_CpuStats.UserDelta.Delta + m_CpuStats.IdleDelta.Delta;

		m_CpuStats.KernelUsage = totalTime != 0 ? (float)m_CpuStats.KernelDelta.Delta / totalTime : 0.0f;
		m_CpuStats.UserUsage = totalTime != 0 ? (float)m_CpuStats.UserDelta.Delta / totalTime : 0.0f;
    }

	delete[] CpuInformation;


	//////////////////////////////
	// Update CPU Clock speeds

    if (m->PreviousPerformanceDistribution)
        PhFree(m->PreviousPerformanceDistribution);
    m->PreviousPerformanceDistribution = m->CurrentPerformanceDistribution;
    m->CurrentPerformanceDistribution = NULL;
    PhSipQueryProcessorPerformanceDistribution((PVOID*)&m->CurrentPerformanceDistribution);


	// ToDo: don't just look on [0] use all cpus

	double cpuGhz = 0;
	double cpuFraction = 0;
    if (PhSipGetCpuFrequencyFromDistribution(m->CurrentPerformanceDistribution, m->PreviousPerformanceDistribution, &cpuFraction))
        cpuGhz = (double)m->PowerInformation[0].MaxMhz * cpuFraction / 1000;
	else
		cpuGhz = (double)m->PowerInformation[0].CurrentMhz / 1000;
   
	m_CpuBaseClock = (double)m->PowerInformation[0].MaxMhz / 1000;
	m_CpuCurrentClock = cpuGhz;

    return totalTime;
}

quint64 CWindowsAPI::UpdateCpuCycleStats()
{			
	QWriteLocker Locker(&m_StatsMutex);

    // Idle

    // We need to query this separately because the idle cycle time in SYSTEM_PROCESS_INFORMATION
    // doesn't give us data for individual processors.

    NtQuerySystemInformation(SystemProcessorIdleCycleTimeInformation, &m->CpuIdleCycleTime, sizeof(LARGE_INTEGER) * PhSystemBasicInformation.NumberOfProcessors, NULL);

	quint64 totalIdle = 0;
    for (ulong i = 0; i < (ulong)PhSystemBasicInformation.NumberOfProcessors; i++)
        totalIdle += m->CpuIdleCycleTime[i].QuadPart;

	m->CpuIdleCycleDelta.Update(totalIdle);


    // System

	
    NtQuerySystemInformation(SystemProcessorCycleTimeInformation, m->CpuSystemCycleTime, sizeof(LARGE_INTEGER) * PhSystemBasicInformation.NumberOfProcessors, NULL);

    quint64 totalSys = 0;
    for (ulong i = 0; i < (ulong)PhSystemBasicInformation.NumberOfProcessors; i++)
        totalSys += m->CpuSystemCycleTime[i].QuadPart;

	m->CpuSystemCycleDelta.Update(totalSys);	


	return m->CpuIdleCycleDelta.Delta;
}

void CWindowsAPI::UpdatePerfStats()
{
	SYSTEM_PERFORMANCE_INFORMATION PerfInformation;
	NtQuerySystemInformation(SystemPerformanceInformation, &PerfInformation, sizeof(SYSTEM_PERFORMANCE_INFORMATION), NULL);

	QWriteLocker Locker(&m_StatsMutex);

	m_Stats.Io.SetRead(PerfInformation.IoReadTransferCount.QuadPart, PerfInformation.IoReadOperationCount);
	m_Stats.Io.SetWrite(PerfInformation.IoWriteTransferCount.QuadPart, PerfInformation.IoWriteOperationCount);
	m_Stats.Io.SetOther(PerfInformation.IoOtherTransferCount.QuadPart, PerfInformation.IoOtherOperationCount);

	//SYSTEM_INFO SystemInfo;
	//GetSystemInfo(&SystemInfo);
	//SystemInfo.dwPageSize // todo: use this instead of PAGE_SIZE

	//m_Stats.MMapIo.ReadRaw = m->PageReadCount.FixValue(PerfInformation.PageReadCount) * SystemInfo.dwPageSize;
	m_Stats.MMapIo.ReadRaw = (m->PageReadCount.FixValue(PerfInformation.PageReadCount) + m->CacheReadCount.FixValue(PerfInformation.CacheReadCount)) * PAGE_SIZE;
	m_Stats.MMapIo.ReadCount = (quint64)PerfInformation.PageReadIoCount + (quint64)PerfInformation.CacheIoCount;
	
	//m_Stats.MMapIo.WriteRaw = ((quint64)PerfInformation.MappedPagesWriteCount + (quint64)PerfInformation.DirtyPagesWriteCount + (quint64)PerfInformation.CcLazyWritePages) * SystemInfo.dwPageSize;
	m_Stats.MMapIo.WriteRaw = (m->MappedPagesWriteCount.FixValue(PerfInformation.MappedPagesWriteCount) + m->DirtyPagesWriteCount.FixValue(PerfInformation.DirtyPagesWriteCount) + m->CcLazyWritePages.FixValue(PerfInformation.CcLazyWritePages)) * PAGE_SIZE;
	m_Stats.MMapIo.WriteCount = (quint64)PerfInformation.MappedWriteIoCount + (quint64)PerfInformation.DirtyWriteIoCount + (quint64)PerfInformation.CcLazyWriteIos;

	m_CpuStats.PageFaultsDelta.Update(PerfInformation.PageFaultCount);
	m_CpuStats.PageReadsDelta.Update(PerfInformation.PageReadCount);
	m_CpuStats.PageFileWritesDelta.Update(PerfInformation.DirtyPagesWriteCount);
	m_CpuStats.MappedWritesDelta.Update(PerfInformation.MappedPagesWriteCount);

	m_CommitedMemory = UInt32x32To64(PerfInformation.CommittedPages, PAGE_SIZE);
	m_CommitedMemoryPeak = UInt32x32To64(PerfInformation.PeakCommitment, PAGE_SIZE);
	m_MemoryLimit = UInt32x32To64(PerfInformation.CommitLimit, PAGE_SIZE);
	m_PagedPool = UInt32x32To64(PerfInformation.PagedPoolPages, PAGE_SIZE);
	m_PersistentPagedPool = UInt32x32To64(PerfInformation.ResidentPagedPoolPage, PAGE_SIZE);
	m_NonPagedPool = UInt32x32To64(PerfInformation.NonPagedPoolPages, PAGE_SIZE);
	m_PhysicalUsed = UInt32x32To64(PhSystemBasicInformation.NumberOfPhysicalPages - PerfInformation.AvailablePages, PAGE_SIZE);
	m_CacheMemory = UInt32x32To64(PerfInformation.ResidentSystemCachePage, PAGE_SIZE);
	m_KernelMemory = UInt32x32To64(PerfInformation.ResidentSystemCodePage, PAGE_SIZE);
	m_DriverMemory = UInt32x32To64(PerfInformation.ResidentSystemDriverPage, PAGE_SIZE);

	quint64 TotalSwapMemory = 0;
	quint64 SwapedOutMemory = 0;

	PVOID PageFiles;
	if (NT_SUCCESS(PhEnumPagefiles(&PageFiles)))
    {
		m_PageFiles.clear();

		for (PSYSTEM_PAGEFILE_INFORMATION pagefile = PH_FIRST_PAGEFILE(PageFiles); pagefile; pagefile = PH_NEXT_PAGEFILE(pagefile))
		{
			TotalSwapMemory += UInt32x32To64(pagefile->TotalSize, PAGE_SIZE);
			SwapedOutMemory += UInt32x32To64(pagefile->TotalInUse, PAGE_SIZE);

	        PPH_STRING fileName = PhCreateStringFromUnicodeString(&pagefile->PageFileName);
		    PPH_STRING newFileName = PhGetFileName(fileName);
			PhDereferenceObject(fileName);

			SPageFile PageFile;
			PageFile.Path = CastPhString(newFileName);
			PageFile.TotalSize = UInt32x32To64(pagefile->TotalSize, PAGE_SIZE);
			PageFile.TotalInUse = UInt32x32To64(pagefile->TotalInUse, PAGE_SIZE);
			PageFile.PeakUsage = UInt32x32To64(pagefile->PeakUsage, PAGE_SIZE);
			m_PageFiles.append(PageFile);
		}

        PhFree(PageFiles);
    }

	m_TotalSwapMemory = TotalSwapMemory;
	m_SwapedOutMemory = SwapedOutMemory;

    ULONG64 dpcCount = 0;
    if (NT_SUCCESS(NtQuerySystemInformation(SystemInterruptInformation, m->InterruptInformation, sizeof(SYSTEM_INTERRUPT_INFORMATION) * PhSystemBasicInformation.NumberOfProcessors, NULL)))
    {
        for (int i = 0; i < PhSystemBasicInformation.NumberOfProcessors; i++)
            dpcCount += m->InterruptInformation[i].DpcCount;
    }

	m_CpuStats.ContextSwitchesDelta.Update(PerfInformation.ContextSwitches);
	m_CpuStats.InterruptsDelta.Update(m->CpuTotals.InterruptCount);
	m_CpuStats.DpcsDelta.Update(dpcCount);
	m_CpuStats.SystemCallsDelta.Update(PerfInformation.SystemCalls);	
}

void CWindowsAPI::UpdateSambaStats()
{
	QWriteLocker Locker(&m_StatsMutex);

	STAT_WORKSTATION_0* wrkStat = NULL;
	if (NT_SUCCESS(NetStatisticsGet(NULL, L"LanmanWorkstation", 0, 0, (LPBYTE*)&wrkStat)) && wrkStat != NULL)
	{
		m_Stats.SambaClient.SetReceive(wrkStat->BytesReceived.QuadPart, 0 /*wrkStat->SmbsReceived*/);
		m_Stats.SambaClient.SetSend(wrkStat->BytesTransmitted.QuadPart, 0 /*wrkStat->SmbsTransmitted*/);

		NetApiBufferFree(wrkStat);
	}

	_STAT_SERVER_0* srvStat = NULL;
	if (NT_SUCCESS(NetStatisticsGet(NULL, L"LanmanServer", 0, 0, (LPBYTE*)&srvStat)) && srvStat != NULL)
	{
		m_Stats.SambaServer.SetReceive((LARGE_INTEGER { srvStat->sts0_bytesrcvd_low, (LONG)srvStat->sts0_bytesrcvd_high }).QuadPart, 0 /*???*/);
		m_Stats.SambaServer.SetSend((LARGE_INTEGER { srvStat->sts0_bytessent_low, (LONG)srvStat->sts0_bytessent_high }).QuadPart, 0 /*???*/);

		NetApiBufferFree(srvStat);
	}
}

bool CWindowsAPI::UpdateSysStats()
{
	m_pGpuMonitor->UpdateGpuStats();
	m_pNetMonitor->UpdateNetStats();
	m_pDiskMonitor->UpdateDiskStats();

	UpdatePerfStats();

	UpdateSambaStats();

	QWriteLocker StatsLocker(&m_StatsMutex);
	m_Stats.UpdateStats();

	return true;
}

bool CWindowsAPI::UpdateProcessList()
{
	quint64 sysTotalTime; // total time for this update period
    quint64 sysTotalCycleTime = 0; // total cycle time for this update period
    quint64 sysIdleCycleTime = 0; // total idle cycle time for this update period

	bool PhEnableCycleCpuUsage = false; // todo: add settings

    if (PhEnableCycleCpuUsage)
    {
        sysTotalTime = UpdateCpuStats(false);
        sysIdleCycleTime = UpdateCpuCycleStats();

		// sysTotalCycleTime = todo: count from all processes what a mess
    }
    else
    {
        sysTotalTime = UpdateCpuStats(true);
    }

	int iLinuxStyleCPU = theConf->GetInt("Options/LinuxStyleCPU", 2);

	quint64 sysTotalTimePerCPU = sysTotalTime / m_CpuCount;


	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	PVOID processes;
	bool bFullProcessInfo = (WindowsVersion >= WINDOWS_8) && RootAvaiable();
	if (!bFullProcessInfo || !NT_SUCCESS(PhEnumProcessesEx(&processes, SystemFullProcessInformation)))
	{
		bFullProcessInfo = false; // API not supported
		if (!NT_SUCCESS(PhEnumProcesses(&processes)))
			return false;
	}

    // Add the fake processes to the PID list.
    //
    // On Windows 7 the two fake processes are merged into "Interrupts" since we can only get cycle
    // time information both DPCs and Interrupts combined.

    if (PhEnableCycleCpuUsage)
    {
        m->InterruptsProcessInformation.KernelTime.QuadPart = m->CpuTotals.DpcTime.QuadPart + m->CpuTotals.InterruptTime.QuadPart;
        m->InterruptsProcessInformation.CycleTime = m->CpuSystemCycleDelta.Value;
        sysTotalCycleTime += m->CpuSystemCycleDelta.Delta;
    }
    else
    {
        m->DpcsProcessInformation.KernelTime = m->CpuTotals.DpcTime;
        m->InterruptsProcessInformation.KernelTime = m->CpuTotals.InterruptTime;
    }
	float CpuStatsDPCUsage = 0;

	quint32 newTotalProcesses = 0;
	quint32 newTotalThreads = 0;
	quint32 newTotalHandles = 0;

	quint32 newTotalGuiObjects = 0;
	quint32 newTotalUserObjects = 0;
	quint32 newTotalWndObjects = EnumWindows();

	/*if (!HasExtProcInfo() || theConf->GetBool("Options/DiskAlwaysUseETW", false))
		m_UseDiskCounters = eDontUse;
	else if (m_pDiskMonitor->AllDisksSupported())
		m_UseDiskCounters = eUseForSystem;
	else
		m_UseDiskCounters = eForProgramsOnly;*/

	m_UseDiskCounters = HasExtProcInfo() && m_pDiskMonitor->AllDisksSupported() && !theConf->GetBool("Options/DiskAlwaysUseETW", false);

	PROCESS_DISK_COUNTERS DiskCounters;
	memset(&DiskCounters, 0, sizeof(DiskCounters));

	// Copy the process Map
	QMap<quint64, CProcessPtr>	OldProcesses = GetProcessList();

	for (PSYSTEM_PROCESS_INFORMATION process = PH_FIRST_PROCESS(processes); process != NULL; )
	{
		quint64 ProcessID = (quint64)process->UniqueProcessId;

		// take all running processes out of the copyed map
		QSharedPointer<CWinProcess> pProcess = OldProcesses.take(ProcessID).objectCast<CWinProcess>();
		bool bAdd = false;
		if (pProcess.isNull())
		{
			pProcess = QSharedPointer<CWinProcess>(new CWinProcess());
			bAdd = pProcess->InitStaticData(process, bFullProcessInfo);
			QWriteLocker Locker(&m_ProcessMutex);
			ASSERT(!m_ProcessList.contains(ProcessID));
			m_ProcessList.insert(ProcessID, pProcess);
		}

		bool bChanged = false;
		bChanged = pProcess->UpdateDynamicData(process, bFullProcessInfo, iLinuxStyleCPU == 1 ? sysTotalTimePerCPU : sysTotalTime, sysTotalCycleTime);

		pProcess->UpdateThreadData(process, bFullProcessInfo, iLinuxStyleCPU ? sysTotalTimePerCPU : sysTotalTime, sysTotalCycleTime); // a thread can ever only use one CPU to use linux style ylways

		pProcess->SetWndHandles(GetWindowByPID((quint64)process->UniqueProcessId).count());

		if (bAdd)
			Added.insert(ProcessID);
		else if (bChanged)
			Changed.insert(ProcessID);

		newTotalProcesses++;
        newTotalThreads += process->NumberOfThreads;
        newTotalHandles += process->HandleCount;
		
		newTotalGuiObjects += pProcess->GetGdiHandles();
		newTotalUserObjects += pProcess->GetUserHandles();

		//if (m_UseDiskCounters == eUseForSystem && PH_IS_REAL_PROCESS_ID(process->UniqueProcessId))
		if (m_UseDiskCounters && PH_IS_REAL_PROCESS_ID(process->UniqueProcessId))
		{
			PSYSTEM_PROCESS_INFORMATION_EXTENSION processExtension = bFullProcessInfo ? PH_EXTENDED_PROCESS_EXTENSION(process) : PH_PROCESS_EXTENSION(process);
			DiskCounters.BytesRead += processExtension->DiskCounters.BytesRead;
			DiskCounters.BytesWritten += processExtension->DiskCounters.BytesWritten;
			DiskCounters.FlushOperationCount += processExtension->DiskCounters.FlushOperationCount;
			DiskCounters.ReadOperationCount += processExtension->DiskCounters.ReadOperationCount;
			DiskCounters.WriteOperationCount += processExtension->DiskCounters.WriteOperationCount;
		}

        // Trick ourselves into thinking that the fake processes
        // are on the list.
        if (process == &m->InterruptsProcessInformation)
        {
			CpuStatsDPCUsage += pProcess->GetCpuStats().CpuUsage;
            process = NULL;
        }
        else if (process == &m->DpcsProcessInformation)
        {
			CpuStatsDPCUsage += pProcess->GetCpuStats().CpuUsage;
            process = &m->InterruptsProcessInformation;
        }
        else
        {
            process = PH_NEXT_PROCESS(process);

            if (process == NULL)
            {
                if (PhEnableCycleCpuUsage)
                    process = &m->InterruptsProcessInformation;
                else
                    process = &m->DpcsProcessInformation;
            }
        }
	}

	// purle all processes left as thay are not longer running

	QWriteLocker Locker(&m_ProcessMutex);
	foreach(quint64 ProcessID, OldProcesses.keys())
	{
		QSharedPointer<CWinProcess> pProcess = m_ProcessList.value(ProcessID).objectCast<CWinProcess>();
		if (pProcess->CanBeRemoved())
		{
			m_ProcessList.remove(ProcessID);
			Removed.insert(ProcessID);
		}
		else if (!pProcess->IsMarkedForRemoval())
		{
			pProcess->MarkForRemoval();
			pProcess->UnInit();
			Changed.insert(ProcessID);
		}
	}
	Locker.unlock();

	PhFree(processes);

	emit ProcessListUpdated(Added, Changed, Removed);

	QWriteLocker StatsLocker(&m_StatsMutex);

	m_TotalProcesses = newTotalProcesses;
    m_TotalThreads = newTotalThreads;
    m_TotalHandles = newTotalHandles;

	m_TotalGuiObjects = newTotalGuiObjects;
	m_TotalUserObjects = newTotalUserObjects;
	m_TotalWndObjects = newTotalWndObjects;

	m_CpuStatsDPCUsage = CpuStatsDPCUsage; // & interrupts

	//if (m_UseDiskCounters == eUseForSystem)
	if (m_UseDiskCounters)
	{
		m_Stats.Disk.SetRead(DiskCounters.BytesRead, DiskCounters.ReadOperationCount);
		m_Stats.Disk.SetWrite(DiskCounters.BytesWritten, DiskCounters.WriteOperationCount);
		// DiskCounters.FlushOperationCount // todo
	}

	return true;
}

#if 0
void EnumChildWindows(HWND hwnd, QMap<quint64, QMultiMap<quint64, quint64> >& WindowMap, quint32& WndCount)
{
    HWND childWindow = NULL;
    ULONG i = 0;

    // We use FindWindowEx because EnumWindows doesn't return Metro app windows.
    // Set a reasonable limit to prevent infinite loops.
    while (i < 0x800 && (childWindow = FindWindowEx(hwnd, childWindow, NULL, NULL)))
    {
		WndCount++;

        ULONG processId;
        ULONG threadId = GetWindowThreadProcessId(childWindow, &processId);

		WindowMap[processId].insertMulti(threadId, (quint64)childWindow);

		EnumChildWindows(childWindow, WindowMap, WndCount);

        i++;
    }
}

quint32 CWindowsAPI::EnumWindows()
{
	quint32 WndCount = 0;
	QMap<quint64, QMultiMap<quint64, quint64> > WindowMap; // QMap<ProcessId, QMultiMap<ThreadId, HWND> > 

	// Using FindWindowEx we have only very little ability to filter the results
	// so to avoind going throuhgh the full list over and over agian we just cache ist in a double multi map
	// this way we can asily look up all windows of a process or all all windows belinging to a given thread

	EnumChildWindows(GetDesktopWindow(), WindowMap, WndCount);

	QWriteLocker Locker(&m_WindowMutex);
	m_WindowMap = WindowMap;

	return WndCount;
}
#else

struct CWindowsAPI__WndEnumStruct
{
	QMap<quint64, QPair<quint64, quint64> > OldWindows;

	QReadWriteLock* pWindowMutex;
	QMap<quint64, QMultiMap<quint64, quint64> >* pWindowMap;
	QMap<quint64, QPair<quint64, quint64> >* pWindowRevMap;
};

void CWindowsAPI__WndEnumProc(quint64 hWnd, /*quint64 hParent,*/ void* Param)
{
	CWindowsAPI__WndEnumStruct* pContext = (CWindowsAPI__WndEnumStruct*)Param;

	QMap<quint64, QPair<quint64, quint64> >::iterator I = pContext->OldWindows.find(hWnd);
	if (I == pContext->OldWindows.end())
	{
		ULONG processId;
		ULONG threadId = GetWindowThreadProcessId((HWND)hWnd, &processId);

		QWriteLocker Locker(pContext->pWindowMutex);
		pContext->pWindowMap->operator[](processId).insertMulti(threadId, hWnd);
		pContext->pWindowRevMap->insert(hWnd, qMakePair((quint64)processId, (quint64)threadId));
	}
	else
		pContext->OldWindows.erase(I);
}

quint32 CWindowsAPI::EnumWindows()
{
	CWindowsAPI__WndEnumStruct Context;
	Context.OldWindows = m_WindowRevMap;

	Context.pWindowMutex = &m_WindowMutex;
	Context.pWindowMap = &m_WindowMap;
	Context.pWindowRevMap = &m_WindowRevMap;

	CWinWnd::EnumAllWindows(CWindowsAPI__WndEnumProc, &Context);

	QWriteLocker Locker(Context.pWindowMutex);
	foreach(quint64 hWnd, Context.OldWindows.keys())
	{
		QPair<quint64, quint64> PidTid = Context.pWindowRevMap->take(hWnd);

		QMap<quint64, QMultiMap<quint64, quint64> >::iterator I = Context.pWindowMap->find(PidTid.first);
		if (I != Context.pWindowMap->end())
		{
			I->remove(PidTid.second, hWnd);
			if (I->isEmpty())
				Context.pWindowMap->erase(I);
		}
	}

	return m_WindowRevMap.size();
}
#endif

void CWindowsAPI::OnHardwareChanged()
{
	m_HardwareChangePending = false;

	m_pGpuMonitor->UpdateAdapters();
	m_pNetMonitor->UpdateAdapters();
	m_pDiskMonitor->UpdateDisks();
}

quint64	CWindowsAPI::GetCpuIdleCycleTime(int index)
{
	QReadLocker Locker(&m_StatsMutex);

	if (index < PhSystemBasicInformation.NumberOfProcessors)
		return m->CpuIdleCycleTime[index].QuadPart;
	return 0;
}

static BOOLEAN NetworkImportDone = FALSE;

QMultiMap<quint64, CSocketPtr>::iterator FindSocketEntry(QMultiMap<quint64, CSocketPtr> &Sockets, quint64 ProcessId, ulong ProtocolType, 
							const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort, bool bStrict)
{
	quint64 HashID = CSocketInfo::MkHash(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort);

	for (QMultiMap<quint64, CSocketPtr>::iterator I = Sockets.find(HashID); I != Sockets.end() && I.key() == HashID; I++)
	{
		if (I.value().objectCast<CWinSocket>()->Match(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort, bStrict))
			return I;
	}

	// no matching entry
	return Sockets.end();
}

bool CWindowsAPI::UpdateSocketList()
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

    if (!NetworkImportDone)
    {
        WSADATA wsaData;
        // Make sure WSA is initialized. (wj32)
        WSAStartup(WINSOCK_VERSION, &wsaData);
        NetworkImportDone = TRUE;
    }

	QVector<CWinSocket::SSocket> connections = CWinSocket::GetNetworkConnections();

	//QWriteLocker Locker(&m_SocketMutex);
	
	// Copy the socket map Map
	QMultiMap<quint64, CSocketPtr> OldSockets = GetSocketList();

	for (ulong i = 0; i < connections.size(); i++)
	{
		QSharedPointer<CWinSocket> pSocket;

		bool bAdd = false;
		QMultiMap<quint64, CSocketPtr>::iterator I = FindSocketEntry(OldSockets, (quint64)connections[i].ProcessId, (quint64)connections[i].ProtocolType, 
			connections[i].LocalEndpoint.Address, connections[i].LocalEndpoint.Port, connections[i].RemoteEndpoint.Address, connections[i].RemoteEndpoint.Port, true);
		if (I == OldSockets.end())
		{
			pSocket = QSharedPointer<CWinSocket>(new CWinSocket());
			bAdd = pSocket->InitStaticData((quint64)connections[i].ProcessId, (quint64)connections[i].ProtocolType, 
				connections[i].LocalEndpoint.Address, connections[i].LocalEndpoint.Port, connections[i].RemoteEndpoint.Address, connections[i].RemoteEndpoint.Port);
			QWriteLocker Locker(&m_SocketMutex);
			m_SocketList.insertMulti(pSocket->m_HashID, pSocket);
		}
		else
		{
			pSocket = I.value().objectCast<CWinSocket>();
			OldSockets.erase(I);
		}

		bool bChanged = false;
		if (bAdd || pSocket->GetCreateTimeStamp() == 0) { // on network events we may add new, yet un enumerated, sockets 
			pSocket->InitStaticDataEx(&(connections[i]));
			bChanged = true;
		}
		bChanged = pSocket->UpdateDynamicData(&(connections[i]));

		if (bAdd)
			Added.insert(pSocket->m_HashID);
		else if (bChanged)
			Changed.insert(pSocket->m_HashID);
	}

	// For UDP Traffic keep the pseudo connections listed for a while
	for (QMultiMap<quint64, CSocketPtr>::iterator I = OldSockets.begin(); I != OldSockets.end(); )
	{
		if ((I.value()->GetProtocolType() & PH_UDP_PROTOCOL_TYPE) != 0 && I.value()->GetRemotePort() != 0 &&
			I.value().objectCast<CWinSocket>()->GetIdleTime() < theConf->GetUInt64("Options/UdpPersistenceTime", 3000))
		{
			I.value()->UpdateStats();
			I = OldSockets.erase(I);
		}
		else
			I++;
	}

	QWriteLocker Locker(&m_SocketMutex);
	// purle all sockets left as thay are not longer running
	for(QMultiMap<quint64, CSocketPtr>::iterator I = OldSockets.begin(); I != OldSockets.end(); ++I)
	{
		QSharedPointer<CWinSocket> pSocket = I.value().objectCast<CWinSocket>();
		if (pSocket->CanBeRemoved())
		{
			m_SocketList.remove(I.key(), I.value());
			Removed.insert(I.key());
		}
		else if (!pSocket->IsMarkedForRemoval())
			pSocket->MarkForRemoval();
	}
	Locker.unlock();

	emit SocketListUpdated(Added, Changed, Removed);
	return true;
}

CSocketPtr CWindowsAPI::FindSocket(quint64 ProcessId, ulong ProtocolType,
	const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort, bool bStrict)
{
	QReadLocker Locker(&m_SocketMutex);

	QMultiMap<quint64, CSocketPtr>::iterator I = FindSocketEntry(m_SocketList, ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort, bStrict);
	if (I == m_SocketList.end())
		return CSocketPtr();
	return I.value();
}

void CWindowsAPI::AddNetworkIO(int Type, ulong TransferSize)
{
	QWriteLocker Locker(&m_StatsMutex);

	switch (Type)
	{
	case EtEtwNetworkReceiveType:	m_Stats.Net.AddReceive(TransferSize); break;
	case EtEtwNetworkSendType:		m_Stats.Net.AddSend(TransferSize); break;
	}
}

void CWindowsAPI::AddDiskIO(int Type, ulong TransferSize)
{
	QWriteLocker Locker(&m_StatsMutex);

	switch (Type)
	{
	case EtEtwDiskReadType:			m_Stats.Disk.AddRead(TransferSize); break;
	case EtEtwDiskWriteType:		m_Stats.Disk.AddWrite(TransferSize); break;
	}
}

void CWindowsAPI::OnNetworkEvent(int Type, quint64 ProcessId, quint64 ThreadId, ulong ProtocolType, ulong TransferSize, // note ThreadId is always -1
								QHostAddress LocalAddress, quint16 LocalPort, QHostAddress RemoteAddress, quint16 RemotePort)
{
    // Note: there is always the possibility of us receiving the event too early,
    // before the process item or network item is created. 

	AddNetworkIO(Type, TransferSize);

	bool bStrict = false;

	if ((ProtocolType & PH_UDP_PROTOCOL_TYPE) != 0 && theConf->GetBool("Options/UseUDPPseudoConnectins", false))
		bStrict = true;

	QSharedPointer<CWinSocket> pSocket = FindSocket(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort, bStrict).objectCast<CWinSocket>();
	bool bAdd = false;
	if (pSocket.isNull())
	{
		qDebug() << ProcessId  << Type << LocalAddress.toString() << LocalPort << RemoteAddress.toString() << RemotePort;

		pSocket = QSharedPointer<CWinSocket>(new CWinSocket());
		bAdd = pSocket->InitStaticData(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort);
		QWriteLocker Locker(&m_SocketMutex);
		m_SocketList.insertMulti(pSocket->m_HashID, pSocket);
	}

	QSharedPointer<CWinProcess> pProcess = pSocket->GetProcess().objectCast<CWinProcess>();
    if (!pProcess.isNull())
		pProcess->AddNetworkIO(Type, TransferSize);

	pSocket->AddNetworkIO(Type, TransferSize);
}

void CWindowsAPI::OnFileEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, const QString& FileName)
{
	// Since Windows 8, we no longer get the correct process/thread IDs in the
	// event headers for file events. 

	/*
	QWriteLocker Locker(&m_FileNameMutex);

	switch (Type)
    {
    case EtEtwFileNameType: // Name
        break;
    case EtEtwFileCreateType: // FileCreate
	case EtEtwFileRundownType:
		m_FileNames.insert(FileId, FileName);
        break;
    case EtEtwFileDeleteType: // FileDelete
		m_FileNames.remove(FileId);
        break;
    }*/
}

/*QString CWindowsAPI::GetFileNameByID(quint64 FileId) const
{
	QReadLocker Locker(&m_FileNameMutex);
	return m_FileNames.value(FileId, tr("Unknown file name"));;
}*/

void CWindowsAPI::OnDiskEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, ulong IrpFlags, ulong TransferSize, quint64 HighResResponseTime)
{
	//if (m_UseDiskCounters == eUseForSystem)
	if (m_UseDiskCounters)
		return;

	AddDiskIO(Type, TransferSize);

	//if (m_UseDiskCounters == eForProgramsOnly)
	//	return;

	QSharedPointer<CWinProcess> pProcess;
	if (ProcessId != -1)
		pProcess = GetProcessByID(ProcessId).objectCast<CWinProcess>();
	else
		pProcess = GetProcessByThreadID(ThreadId).objectCast<CWinProcess>();

	if (!pProcess.isNull())
		pProcess->AddDiskIO(Type, TransferSize);

	// todo: FileName ist often unknown and FileId doesn't seam to have relations to open handled
	//			we want to be able to show for eadch open file the i/o that must be doable somehow...

	//QString FileName = GetFileNameByID(FileId);
	//pHandle->AddDiskIO(Type, TransferSize);

}


bool CWindowsAPI::UpdateOpenFileList()
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

    PSYSTEM_HANDLE_INFORMATION_EX handleInfo;
    if (!NT_SUCCESS(PhEnumHandlesEx(&handleInfo)))
        return false;

	quint64 TimeStamp = GetTime() * 1000;

	// Copy the handle Map
	QMap<quint64, CHandlePtr> OldHandles = GetOpenFilesList();

	//if (!KphIsConnected())
	//{
	//	PhFree(handleInfo);
	//	return false;
	//}

	QMap<quint64, HANDLE> ProcessHandles;

	for (int i = 0; i < handleInfo->NumberOfHandles; i++)
	{
		PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handle = &handleInfo->Handles[i];

		// Note: to make this much mroe efficient we would need to add a call to the PH driver that only executes NTSTATUS KphQueryNameObject
		//			without the need of having a valid process handle

		// Only list fiel handles
		if (handle->ObjectTypeIndex != g_fileObjectTypeIndex)
			continue;

		QSharedPointer<CWinHandle> pWinHandle = OldHandles.take(handle->HandleValue).objectCast<CWinHandle>();

		bool WasReused = false;
		// Also compare the object pointers to make sure a
        // different object wasn't re-opened with the same
        // handle value. This isn't 100% accurate as pool
        // addresses may be re-used, but it works well.
		if (handle->Object && !pWinHandle.isNull() && (quint64)handle->Object != pWinHandle->GetObjectAddress())
			WasReused = true;

		bool bAdd = false;
		if (pWinHandle.isNull())
		{
			pWinHandle = QSharedPointer<CWinHandle>(new CWinHandle());
			bAdd = true;
		}
		
		if (WasReused)
			Removed.insert(handle->HandleValue);

		if (bAdd || WasReused)
		{
			HANDLE &ProcessHandle = ProcessHandles[handle->UniqueProcessId];	
			if (ProcessHandle == NULL)
			{
				if (!NT_SUCCESS(PhOpenProcess(&ProcessHandle, PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, (HANDLE)handle->UniqueProcessId)))
					continue;
			}

			pWinHandle->InitStaticData(handle, TimeStamp);
            pWinHandle->InitExtData(handle,(quint64)ProcessHandle);	

			// ignore results without a valid file Name
			if (pWinHandle->m_FileName.isEmpty())
				continue; 

			if (bAdd)
			{
				Added.insert(handle->HandleValue);
				QWriteLocker Locker(&m_OpenFilesMutex);
				if (m_OpenFilesList.contains(handle->HandleValue))
					continue; // ToDo: why are there duplicate results in the handle list?!
				m_OpenFilesList.insert(handle->HandleValue, pWinHandle);
				//m_HandleByObject.insertMulti(pWinHandle->GetObject(), pWinHandle);
			}
		}

		if (pWinHandle->m_pProcess.isNull())
			pWinHandle->m_pProcess = GetProcessByID(handle->UniqueProcessId);
	}

	foreach(HANDLE ProcessHandle, ProcessHandles)
		NtClose(ProcessHandle);
	ProcessHandles.clear();

	PhFree(handleInfo);

	QWriteLocker Locker(&m_OpenFilesMutex);
	// purle all handles left as thay are not longer valid
	foreach(quint64 HandleID, OldHandles.keys())
	{
		QSharedPointer<CWinHandle> pWinHandle = OldHandles.value(HandleID).objectCast<CWinHandle>();
		if (pWinHandle->CanBeRemoved())
		{
			//m_HandleByObject.remove(pWinHandle->GetObject(), pWinHandle);
			m_OpenFilesList.remove(HandleID);
			Removed.insert(HandleID);
		}
		else if (!pWinHandle->IsMarkedForRemoval())
		{
			pWinHandle->MarkForRemoval();
			Changed.insert(HandleID); 
		}
	}
	Locker.unlock();
    
	emit OpenFileListUpdated(Added, Changed, Removed);

	return true;
}

bool CWindowsAPI::UpdateServiceList(bool bRefresh)
{
	QSet<QString> Added;
	QSet<QString> Changed;
	QSet<QString> Removed;

    SC_HANDLE scManagerHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE);

	if (!scManagerHandle)
		return false;

	ULONG numberOfServices;
    LPENUM_SERVICE_STATUS_PROCESS services = (LPENUM_SERVICE_STATUS_PROCESS)PhEnumServices(scManagerHandle, 0, 0, &numberOfServices);

	if (!services) {
		CloseServiceHandle(scManagerHandle);
		return false;
	}

	QMap<QString, CServicePtr> OldService = GetServiceList();

	for (ULONG i = 0; i < numberOfServices; i++)
	{
		LPENUM_SERVICE_STATUS_PROCESS service = &services[i];

        if (WindowsVersion >= WINDOWS_10_RS2)
            PhpWorkaroundWindows10ServiceTypeBug(service);

		QString Name = QString::fromWCharArray(service->lpServiceName).toLower(); // its not case sensitive

		QSharedPointer<CWinService> pService = OldService.take(Name).objectCast<CWinService>();

		bool bAdd = false;
		if (pService.isNull())
		{
			pService = QSharedPointer<CWinService>(new CWinService());
			bAdd = pService->InitStaticData(service);
			QWriteLocker Locker(&m_ServiceMutex);
			ASSERT(!m_ServiceList.contains(Name));
			m_ServiceList.insert(Name, pService);
		}

		bool bChanged = false;

		if (pService->UpdatePID(service))
		{
			quint64 uServicePID = pService->GetPID();
			QWriteLocker Locker(&m_ServiceMutex);
			if(uServicePID != 0)
				m_ServiceByPID.insertMulti(uServicePID, Name);
			else
				m_ServiceByPID.remove(uServicePID, Name);
			Locker.unlock();
			// if the pid just changed we couldn't have set it on the process, so do it now
			if (uServicePID)
			{
				QSharedPointer<CWinProcess> pProcess = GetProcessByID(uServicePID).objectCast<CWinProcess>();
				if(pProcess)
					pProcess->AddService(Name);
			}
		}

		bChanged = pService->UpdateDynamicData(&scManagerHandle, service, bRefresh);

		if (bAdd)
			Added.insert(Name);
		else if (bChanged)
			Changed.insert(Name);
	}

	QWriteLocker Locker(&m_ServiceMutex);
	foreach(const QString& Name, OldService.keys())
	{
		QSharedPointer<CWinService> pService = m_ServiceList.value(Name).objectCast<CWinService>();
		pService->UnInit();
		if (pService->CanBeRemoved())
		{
			m_ServiceList.remove(Name);
			Removed.insert(Name);
		}
		else if (!pService->IsMarkedForRemoval())
			pService->MarkForRemoval();
	}
	Locker.unlock();

	PhFree(services);

	CloseServiceHandle(scManagerHandle);
    
	emit ServiceListUpdated(Added, Changed, Removed);

	return true;
}

bool CWindowsAPI::UpdateDriverList() 
{
	QSet<QString> Added;
	QSet<QString> Changed;
	QSet<QString> Removed;

	PRTL_PROCESS_MODULES ModuleInfo = NULL;
	ULONG len = 0;
	NTSTATUS status = NtQuerySystemInformation(SystemModuleInformation, NULL, 0, &len);
	if (status == STATUS_INFO_LENGTH_MISMATCH)
		ModuleInfo = (PRTL_PROCESS_MODULES)malloc(len);
	
	status = NtQuerySystemInformation(SystemModuleInformation, ModuleInfo, len, NULL);
    if(!NT_SUCCESS(status))
    {
		free(ModuleInfo);
        return false;
    }
 
	QMap<QString, CDriverPtr> OldDrivers = GetDriverList();

    for(int i=0;i<ModuleInfo->NumberOfModules;i++)
    {
		PRTL_PROCESS_MODULE_INFORMATION Module = &ModuleInfo->Modules[i];
		QString BinaryPath = (char*)Module->FullPathName;

		QSharedPointer<CWinDriver> pWinDriver = OldDrivers.take(BinaryPath).objectCast<CWinDriver>();

		bool bAdd = false;
		if (pWinDriver.isNull())
		{
			pWinDriver = QSharedPointer<CWinDriver>(new CWinDriver());
			bAdd = pWinDriver->InitStaticData(Module);
			QWriteLocker Locker(&m_DriverMutex);
			ASSERT(!m_DriverList.contains(BinaryPath));
			m_DriverList.insert(BinaryPath, pWinDriver);
		}

		bool bChanged = false;
		//bChanged = pWinDriver->UpdateDynamicData();
			
		if (bAdd)
			Added.insert(BinaryPath);
		else if (bChanged)
			Changed.insert(BinaryPath);
    }

	QWriteLocker Locker(&m_DriverMutex);
	foreach(const QString& Name, OldDrivers.keys())
	{
		QSharedPointer<CWinDriver> pDriver = m_DriverList.value(Name).objectCast<CWinDriver>();
		if (pDriver->CanBeRemoved())
		{
			m_DriverList.remove(Name);
			Removed.insert(Name);
		}
		else if (!pDriver->IsMarkedForRemoval())
			pDriver->MarkForRemoval();
	}
	Locker.unlock();
 
    free(ModuleInfo);

	emit DriverListUpdated(Added, Changed, Removed);

	return true; 
}

bool CWindowsAPI::InitWindowsInfo()
{
	bool bServer = false;
	QString ReleaseId;
	HANDLE keyHandle;
	static PH_STRINGREF currentVersion = PH_STRINGREF_INIT(L"Software\\Microsoft\\Windows NT\\CurrentVersion");
    if (NT_SUCCESS(PhOpenKey(&keyHandle, KEY_READ, PH_KEY_LOCAL_MACHINE, &currentVersion, 0)))
    {
		m_SystemName = CastPhString(PhQueryRegistryString(keyHandle, L"ProductName"));
		bServer = CastPhString(PhQueryRegistryString(keyHandle, L"InstallationType")) == "Server";
		ReleaseId = CastPhString(PhQueryRegistryString(keyHandle, L"ReleaseId"));

        NtClose(keyHandle);
    }

	if (WindowsVersion == WINDOWS_NEW)
		m_SystemIcon = QPixmap::fromImage(QImage(":/WinLogos/WinNew"));
	// Windows 2000
	else if (PhOsVersion.dwMajorVersion == 5 && PhOsVersion.dwMinorVersion == 0)
		m_SystemIcon = QPixmap::fromImage(QImage(":/WinLogos/Win2k"));
	// Windows XP, Windows Server 2003
	else if (PhOsVersion.dwMajorVersion == 5 && PhOsVersion.dwMinorVersion > 0)
		m_SystemIcon = QPixmap::fromImage(QImage(":/WinLogos/WinXP"));
	// Windows Vista, Windows Server 2008
	else if (PhOsVersion.dwMajorVersion == 6 && PhOsVersion.dwMinorVersion == 0)
		m_SystemIcon = QPixmap::fromImage(QImage(":/WinLogos/Win6"));
    // Windows 7, Windows Server 2008 R2
	else if (PhOsVersion.dwMajorVersion == 6 && PhOsVersion.dwMinorVersion == 1)
		m_SystemIcon = QPixmap::fromImage(QImage(":/WinLogos/Win7"));
    // Windows 8, Windows Server 2012
	else if (PhOsVersion.dwMajorVersion == 6 && PhOsVersion.dwMinorVersion == 2)
		m_SystemIcon = QPixmap::fromImage(QImage(":/WinLogos/Win8"));
    // Windows 8.1, Windows Server 2012 R2
	else if (PhOsVersion.dwMajorVersion == 6 && PhOsVersion.dwMinorVersion == 3)
		m_SystemIcon = QPixmap::fromImage(QImage(":/WinLogos/Win8"));
    // Windows 10, Windows Server 2016
    else if (PhOsVersion.dwMajorVersion == 10 && PhOsVersion.dwMinorVersion == 0)
        m_SystemIcon = QPixmap::fromImage(QImage(":/WinLogos/Win10"));
	else
		m_SystemIcon = QPixmap::fromImage(QImage(":/WinLogos/WinOld"));

	m_SystemVersion = tr("Windows %1.%2").arg(PhOsVersion.dwMajorVersion).arg(PhOsVersion.dwMinorVersion);
	if(!ReleaseId.isEmpty())
		m_SystemBuild = tr("%1 (%2)").arg(ReleaseId).arg(PhOsVersion.dwBuildNumber);
	else
		m_SystemBuild = tr("%1").arg(PhOsVersion.dwBuildNumber);

	return true;
}

quint64 CWindowsAPI::GetUpTime() const
{
	return GetTickCount64() / 1000;
}

QList<CSystemAPI::SUser> CWindowsAPI::GetUsers() const
{
	QList<SUser> List;

	PSESSIONIDW sessions;
    ULONG numberOfSessions;
    if (WinStationEnumerateW(NULL, &sessions, &numberOfSessions))
    {
        for (ULONG i = 0; i < numberOfSessions; i++)
        {
			ULONG returnLength;
			WINSTATIONINFORMATION winStationInfo;
            if (!WinStationQueryInformationW(NULL, sessions[i].SessionId, WinStationInformation, &winStationInfo, sizeof(WINSTATIONINFORMATION), &returnLength))
            {
                winStationInfo.Domain[0] = UNICODE_NULL;
                winStationInfo.UserName[0] = UNICODE_NULL;
            }

            if (winStationInfo.Domain[0] == UNICODE_NULL || winStationInfo.UserName[0] == UNICODE_NULL)
                continue; // Probably the Services or RDP-Tcp session.

			SUser User;
			User.UserName = QString::fromWCharArray(winStationInfo.Domain) + "\\" + QString::fromWCharArray(winStationInfo.UserName);
			User.SessionId = sessions[i].SessionId;

			switch (sessions[i].State)
			{
				case State_Active:			User.Status = tr("Active"); break;
				case State_Connected:		User.Status = tr("Connected"); break;
				case State_ConnectQuery:	User.Status = tr("Connect query"); break;
				case State_Shadow:			User.Status = tr("Shadow"); break;
				case State_Disconnected:	User.Status = tr("Disconnected"); break;
				case State_Idle:			User.Status = tr("Idle"); break;
				case State_Listen:			User.Status = tr("Listen"); break;
				case State_Reset:			User.Status = tr("Reset"); break;
				case State_Down:			User.Status = tr("Down"); break;
				case State_Init:			User.Status = tr("Init"); break;
			}

			List.append(User);
        }

        WinStationFreeMemory(sessions);
    }

	return List;
}

bool CWindowsAPI::HasExtProcInfo() const
{
	return (WindowsVersion >= WINDOWS_10_RS3 && !PhIsExecutingInWow64());
}

//////////////////////////////////////////////////
//

typedef BOOL (WINAPI *LPFN_GLPI)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);

// Helper function to count set bits in the processor mask.
DWORD CountSetBits(ULONG_PTR bitMask)
{
    DWORD LSHIFT = sizeof(ULONG_PTR)*8 - 1;
    DWORD bitSetCount = 0;
    ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;    
    DWORD i;
    
    for (i = 0; i <= LSHIFT; ++i)
    {
        bitSetCount += ((bitMask & bitTest)?1:0);
        bitTest/=2;
    }

    return bitSetCount;
}

bool CWindowsAPI::InitCpuCount()
{
	LPFN_GLPI getLogicalProcessorInformation = (LPFN_GLPI)GetProcAddress(GetModuleHandle(L"kernel32"), "GetLogicalProcessorInformation");
	if (NULL == getLogicalProcessorInformation)
	{
		qDebug() << "GetLogicalProcessorInformation is not supported.";
		return false;
	}

	DWORD logicalProcessorCount = 0;
	DWORD numaNodeCount = 0;
	DWORD processorCoreCount = 0;
	DWORD processorL1CacheCount = 0;
	DWORD processorL2CacheCount = 0;
	DWORD processorL3CacheCount = 0;
	DWORD processorPackageCount = 0;

	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
	DWORD returnLength = 0;
	DWORD rc = getLogicalProcessorInformation(buffer, &returnLength);
	buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);
	rc = getLogicalProcessorInformation(buffer, &returnLength);

	if (!NT_SUCCESS(rc))
	{
		qDebug() << "GetLogicalProcessorInformation Error:" << GetLastError();
		free(buffer);
		return false;
	}

	DWORD byteOffset = 0;
	for (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = buffer; byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength; ptr++, byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION))
	{
		switch (ptr->Relationship)
		{
		case RelationNumaNode:
			// Non-NUMA systems report a single record of this type.
			numaNodeCount++;
			break;

		case RelationProcessorCore:
			processorCoreCount++;
			// A hyperthreaded core supplies more than one logical processor.
			logicalProcessorCount += CountSetBits(ptr->ProcessorMask);
			break;

		case RelationCache:
		{
			// Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache. 
			PCACHE_DESCRIPTOR Cache = &ptr->Cache;
			if (Cache->Level == 1)
				processorL1CacheCount++;
			else if (Cache->Level == 2)
				processorL2CacheCount++;
			else if (Cache->Level == 3)
				processorL3CacheCount++;
			break;
		}

		case RelationProcessorPackage:
			// Logical processors share a physical package.
			processorPackageCount++;
			break;

		default:
			qDebug() << "Error: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value.\n";
		}
	}

	free(buffer);

	m_CoreCount = processorCoreCount;
	m_CpuCount = logicalProcessorCount;
	m_NumaCount = numaNodeCount;
	m_PackageCount = processorPackageCount;

	return m_CoreCount > 0 && m_CpuCount > 0;
}

