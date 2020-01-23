#include "stdafx.h"
#include "WindowsAPI.h"
#include "ProcessHacker.h"
#include "ProcessHacker/pooltable.h"
#include "ProcessHacker/syssccpu.h"
#include "WinHandle.h"
#include <lm.h>
#include "Monitors/WinGpuMonitor.h"
#include "Monitors/WinNetMonitor.h"
#include "Monitors/WinDiskMonitor.h"
#include "../SVC/TaskService.h"
#include "../Common/Settings.h"

extern "C" {
#include <winsta.h>
}

quint32 g_fileObjectTypeIndex = ULONG_MAX;
quint32 g_EtwRegistrationTypeIndex = ULONG_MAX;

struct SWindowsAPI
{
	SWindowsAPI()
	{
		Processes = NULL;
		bFullProcessInfo = false;
		sysTotalTime = 0;
		sysTotalCycleTime = 0;

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

		memset(&PoolTableDB, 0, sizeof(PoolTableDB));
		
		PoolStatsOk = false;
	}

	~SWindowsAPI()
	{
		if (Processes)
			PhFree(Processes);

		delete[] PowerInformation;
		delete[] InterruptInformation;
		if (CurrentPerformanceDistribution)
			PhFree(CurrentPerformanceDistribution);
		if (PreviousPerformanceDistribution)
			PhFree(PreviousPerformanceDistribution);

		delete[] CpuIdleCycleTime;

		delete[] CpuSystemCycleTime;
	}

	QReadWriteLock ProcLocker;
	PVOID Processes;
	bool bFullProcessInfo;
	quint64 sysTotalTime;
	quint64 sysTotalCycleTime;

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

	POOLTAG_CONTEXT PoolTableDB;
	bool PoolStatsOk;
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
	m_pFirewallMonitor = NULL;
	m_pGpuMonitor = NULL;
	m_pSymbolProvider = NULL;
	m_pSidResolver = NULL;
	m_pDnsResolver = NULL;

	m_bTestSigning = false;
	m_uDriverStatus = 0;
	m_uDriverFeatures = 0;

	m = new SWindowsAPI();
}

bool CWindowsAPI::Init()
{
	//if(WindowsVersion >= WINDOWS_10_RS1)
	//	SetThreadDescription(GetCurrentThread(), L"Process Enumerator");

	InitPH();

	SYSTEM_CODEINTEGRITY_INFORMATION sci = {sizeof(SYSTEM_CODEINTEGRITY_INFORMATION)};
	if(NT_SUCCESS(NtQuerySystemInformation(SystemCodeIntegrityInformation, &sci, sizeof(sci), NULL)))
	{
		bool bCodeIntegrityEnabled = !!(sci.CodeIntegrityOptions & /*CODEINTEGRITY_OPTION_ENABLED*/ 0x1);
		bool bTestSigningEnabled = !!(sci.CodeIntegrityOptions & /*CODEINTEGRITY_OPTION_TESTSIGN*/ 0x2);

		m_bTestSigning = (!bCodeIntegrityEnabled || bTestSigningEnabled);
	}
	
	if (!PhIsExecutingInWow64() && theConf->GetBool("Options/UseDriver", true))
	{
		QPair<QString, QString> Driver = SellectDriver();
		int SecurityLevel = theConf->GetInt("Options/DriverSecurityLevel", KphSecurityPrivilegeCheck);
		InitDriver(Driver.first, Driver.second, SecurityLevel);
	}

	static PH_INITONCE initOnce = PH_INITONCE_INIT;
	if (PhBeginInitOnce(&initOnce))
    {
        UNICODE_STRING fileTypeName = RTL_CONSTANT_STRING(L"File");
        g_fileObjectTypeIndex = PhGetObjectTypeNumber(&fileTypeName);

		UNICODE_STRING EtwRegistrationTypeName = RTL_CONSTANT_STRING(L"EtwRegistration");
        g_EtwRegistrationTypeIndex = PhGetObjectTypeNumber(&EtwRegistrationTypeName);
		
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

	LoadPoolTagDatabase(&m->PoolTableDB);

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

	if (theConf->GetBool("Options/MonitorFirewall", false))
		MonitorFW(true);

	m_pGpuMonitor = new CWinGpuMonitor();
	m_pGpuMonitor->Init();

	m_pNetMonitor = new CWinNetMonitor();
	m_pNetMonitor->Init();

	m_pDiskMonitor = new CWinDiskMonitor();
	m_pDiskMonitor->Init();

	m_pSymbolProvider = new CSymbolProvider();
	m_pSymbolProvider->Init();

	m_pSidResolver = new CSidResolver();
	m_pSidResolver->Init();

	m_pDnsResolver = new CDnsResolver();
	connect(m_pDnsResolver, SIGNAL(DnsCacheUpdated()), this, SIGNAL(DnsCacheUpdated()));
	m_pDnsResolver->Init();

	return true;
}

QPair<QString, QString> CWindowsAPI::SellectDriver()
{
	QString DeviceName;
	QString FileName = theConf->GetString("Options/DriverFile");
	if (!FileName.isEmpty())
		DeviceName = theConf->GetString("Options/DriverDevice");
	else
	{
		if (QFile::exists(QApplication::applicationDirPath() + "/xprocesshacker.sys"))
		{
			DeviceName = "XProcessHacker3";
			FileName = "xprocesshacker.sys";
		}
		else
		{
			DeviceName = QString::fromWCharArray(KPH_DEVICE_SHORT_NAME);
			FileName = "kprocesshacker.sys";
		}
	}
	return qMakePair(DeviceName, FileName);
}

STATUS CWindowsAPI::InitDriver(QString DeviceName, QString FileName, int SecurityLevel)
{
	if (KphIsConnected())
		return OK;

	m_DriverFileName = FileName;
	m_DriverDeviceName = DeviceName;

	STATUS Status = InitKPH(DeviceName, FileName, SecurityLevel);
	if (!Status.IsError())
	{
		CLIENT_ID clientId;

		clientId.UniqueProcess = NtCurrentProcessId();
		clientId.UniqueThread = NULL;

		// Note: when KphSecuritySignatureCheck is enabled the connection does not fail only the later atempt to use teh driver,
		//			hence we have to test if the driver is usable and if not disconnect.
		HANDLE ProcessHandle = NULL;
		NTSTATUS status = KphOpenProcess(&ProcessHandle, PROCESS_QUERY_INFORMATION, &clientId);
		if (NT_SUCCESS(status))
			NtClose(ProcessHandle);
		else
		{
			Status = ERR(QObject::tr("Unable to access the kernel driver, Error: %1").arg(CastPhString(PhGetNtMessage(status))), status);

			KphDisconnect();
		}
	}

	m_uDriverStatus = Status.GetStatus();
	if (Status.IsError())
		qDebug() << Status.GetText();
	else
	{
		ULONG Features = 0;
		KphGetFeatures(&Features);
		m_uDriverFeatures = Features;
	}

	return Status;
}

CWindowsAPI::~CWindowsAPI()
{
	delete m_pEventMonitor;
	delete m_pFirewallMonitor;

	delete m_pGpuMonitor;
	delete m_pNetMonitor;
	delete m_pDiskMonitor;

	delete m_pSymbolProvider;
	delete m_pSidResolver;
	delete m_pDnsResolver;

	FreePoolTagDatabase(&m->PoolTableDB);

	delete m;
}

bool CWindowsAPI::RootAvaiable()
{
	return PhGetOwnTokenAttributes().Elevated;
}


void CWindowsAPI::MonitorETW(bool bEnable)
{
	if (bEnable == (m_pEventMonitor != NULL))
		return;

	if (!RootAvaiable())
		return;

	if (bEnable)
	{
		//m_pEventMonitor = new CEventMonitor();
		m_pEventMonitor = new CEtwEventMonitor();

		connect(m_pEventMonitor, SIGNAL(NetworkEvent(int, quint64, quint64, quint32, quint32, const QHostAddress&, quint16, const QHostAddress&, quint16)), this, SLOT(OnNetworkEvent(int, quint64, quint64, quint32, quint32, QHostAddress, quint16, QHostAddress, quint16)));
		connect(m_pEventMonitor, SIGNAL(DnsResEvent(quint64, quint64, const QString&, const QStringList&)), this, SLOT(OnDnsResEvent(quint64, quint64, const QString&, const QStringList&)));

		connect(m_pEventMonitor, SIGNAL(FileEvent(int, quint64, quint64, quint64, const QString&)), this, SLOT(OnFileEvent(int, quint64, quint64, quint64, const QString&)));
		connect(m_pEventMonitor, SIGNAL(DiskEvent(int, quint64, quint64, quint64, quint32, quint32, quint64)), this, SLOT(OnDiskEvent(int, quint64, quint64, quint64, quint32, quint32, quint64)));

		connect(m_pEventMonitor, SIGNAL(ProcessEvent(int, quint32, QString, QString, quint32, quint64)), this, SLOT(OnProcessEvent(int, quint32, QString, QString, quint32, quint64)));

		if (m_pEventMonitor->Init())
			return;
	}

	delete m_pEventMonitor;
	m_pEventMonitor = NULL;
}

void CWindowsAPI::MonitorFW(bool bEnable)
{
	if (bEnable == (m_pFirewallMonitor != NULL))
		return;

	if (bEnable)
	{
		// Note: the FwpmNetEventSubscribe mechanism, does not seam to provide a PID, WTF?!
		//			So instead we wil enable the firewall auditing policy and monitor the event log :p
		//m_pFirewallMonitor = new CFirewallMonitor();
		m_pFirewallMonitor = new CFwEventMonitor();

		connect(m_pFirewallMonitor, SIGNAL(NetworkEvent(int, quint64, quint64, quint32, quint32, const QHostAddress&, quint16, const QHostAddress&, quint16)), this, SLOT(OnNetworkEvent(int, quint64, quint64, quint32, quint32, QHostAddress, quint16, QHostAddress, quint16)));

		if (m_pFirewallMonitor->Init())
			return;
	}

	delete m_pFirewallMonitor;
	m_pFirewallMonitor = NULL;
}

// PhpUpdateCpuInformation
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
//

// PhpUpdateCpuCycleInformation
quint64 CWindowsAPI::UpdateCpuCycleStats()
{			
	QWriteLocker Locker(&m_StatsMutex);

    // Idle

    // We need to query this separately because the idle cycle time in SYSTEM_PROCESS_INFORMATION doesn't give us data for individual processors.
    NtQuerySystemInformation(SystemProcessorIdleCycleTimeInformation, m->CpuIdleCycleTime, sizeof(LARGE_INTEGER) * PhSystemBasicInformation.NumberOfProcessors, NULL);

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
//

// PhpUpdateCpuCycleUsageInformation
void CWindowsAPI::UpdateCPUCycles(quint64 TotalCycleTime, quint64 IdleCycleTime)
{
    FLOAT baseCpuUsage;
    FLOAT totalTimeDelta;
    ULONG64 totalTime;

    // Cycle time is not only lacking for kernel/user components, but also for individual
    // processors. We can get the total idle cycle time for individual processors but
    // without knowing the total cycle time for individual processors, this information
    // is useless.
    //
    // We'll start by calculating the total CPU usage, then we'll calculate the kernel/user
    // components. In the event that the corresponding CPU time deltas are zero, we'll split
    // the CPU usage evenly across the kernel/user components. CPU usage for individual
    // processors is left untouched, because it's too difficult to provide an estimate.
    //
    // Let I_1, I_2, ..., I_n be the idle cycle times and T_1, T_2, ..., T_n be the
    // total cycle times. Let I'_1, I'_2, ..., I'_n be the idle CPU times and T'_1, T'_2, ...,
    // T'_n be the total CPU times.
    // We know all I'_n, T'_n and I_n, but we only know sigma(T). The "real" total CPU usage is
    // sigma(I)/sigma(T), and the "real" individual CPU usage is I_n/T_n. The problem is that
    // we don't know T_n; we only know sigma(T). Hence we need to find values i_1, i_2, ..., i_n
    // and t_1, t_2, ..., t_n such that:
    // sigma(i)/sigma(t) ~= sigma(I)/sigma(T), and
    // i_n/t_n ~= I_n/T_n
    //
    // Solution 1: Set i_n = I_n and t_n = sigma(T)*T'_n/sigma(T'). Then:
    // sigma(i)/sigma(t) = sigma(I)/(sigma(T)*sigma(T')/sigma(T')) = sigma(I)/sigma(T), and
    // i_n/t_n = I_n/T'_n*sigma(T')/sigma(T) ~= I_n/T_n since I_n/T'_n ~= I_n/T_n and sigma(T')/sigma(T) ~= 1.
    // However, it is not guaranteed that i_n/t_n <= 1, which may lead to CPU usages over 100% being displayed.
    //
    // Solution 2: Set i_n = I'_n and t_n = T'_n. Then:
    // sigma(i)/sigma(t) = sigma(I')/sigma(T') ~= sigma(I)/sigma(T) since I'_n ~= I_n and T'_n ~= T_n.
    // i_n/t_n = I'_n/T'_n ~= I_n/T_n as above.
    // Not scaling at all is currently the best solution, since it's fast, simple and guarantees that i_n/t_n <= 1.

    baseCpuUsage = TotalCycleTime ? 1 - (FLOAT)IdleCycleTime / TotalCycleTime : 0;
    totalTimeDelta = (FLOAT)(m_CpuStats.KernelDelta.Delta + m_CpuStats.UserDelta.Delta);

    if (totalTimeDelta != 0)
    {
        m_CpuStats.KernelUsage = totalTimeDelta != 0 ? baseCpuUsage * ((FLOAT)m_CpuStats.KernelDelta.Delta / totalTimeDelta) : 0;
        m_CpuStats.UserUsage = totalTimeDelta != 0 ? baseCpuUsage * ((FLOAT)m_CpuStats.UserDelta.Delta / totalTimeDelta) : 0;
    }
    else
    {
        m_CpuStats.KernelUsage = baseCpuUsage / 2;
        m_CpuStats.UserUsage = baseCpuUsage / 2;
    }

    for (ulong i = 0; i < (ulong)PhSystemBasicInformation.NumberOfProcessors; i++)
    {
		SCpuStats& CpuStats = m_CpusStats[i];

        totalTime = CpuStats.KernelDelta.Delta + CpuStats.UserDelta.Delta + CpuStats.IdleDelta.Delta;

        if (totalTime != 0)
        {
            CpuStats.KernelUsage = (FLOAT)CpuStats.KernelDelta.Delta / totalTime;
            CpuStats.UserUsage = (FLOAT)CpuStats.UserDelta.Delta / totalTime;
        }
        else
        {
            CpuStats.KernelUsage = 0;
            CpuStats.UserUsage = 0;
        }
    }
}
//

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

	if (PerfInformation.PagedPoolAllocs || PerfInformation.PagedPoolFrees || PerfInformation.NonPagedPoolAllocs || PerfInformation.NonPagedPoolFrees)
	{
		m->PoolStatsOk = true;
		m_CpuStats.PagedAllocsDelta.Update(PerfInformation.PagedPoolAllocs);
		m_CpuStats.PagedFreesDelta.Update(PerfInformation.PagedPoolFrees);
		m_CpuStats.NonPagedAllocsDelta.Update(PerfInformation.NonPagedPoolAllocs);
		m_CpuStats.NonPagedFreesDelta.Update(PerfInformation.NonPagedPoolFrees);
	}

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
	bool EnableCycleCpuUsage = theConf->GetBool("Options/EnableCycleCpuUsage", true);
	int iLinuxStyleCPU = theConf->GetInt("Options/LinuxStyleCPU", 2);

	quint64 sysTotalTime = UpdateCpuStats(!EnableCycleCpuUsage); // total time for this update period
	quint64 sysTotalCycleTime = 0; // total cycle time for this update period
    quint64 sysIdleCycleTime = EnableCycleCpuUsage ? UpdateCpuCycleStats() : 0; // total idle cycle time for this update period
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

	// Comment from: procprv.c
    // Add the fake processes to the PID list.
    //
    // On Windows 7 the two fake processes are merged into "Interrupts" since we can only get cycle
    // time information both DPCs and Interrupts combined.

    if (EnableCycleCpuUsage)
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

	bool MonitorTokenChange = theConf->GetBool("Options/MonitorTokenChange", false);

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
		QSharedPointer<CWinProcess> pProcess = OldProcesses.take(ProcessID).staticCast<CWinProcess>();
		bool bAdd = false;
		if (pProcess.isNull())
		{
			pProcess = QSharedPointer<CWinProcess>(new CWinProcess());
			bAdd = pProcess->InitStaticData(process, bFullProcessInfo);
			QWriteLocker Locker(&m_ProcessMutex);
			ASSERT(!m_ProcessList.contains(ProcessID));
			m_ProcessList.insert(ProcessID, pProcess);
		}
		else if(!pProcess->IsFullyInitialized())
		{
			bAdd = true;
			pProcess->InitStaticData(process, bFullProcessInfo);
		}

		bool bChanged = false;
		bChanged = pProcess->UpdateDynamicData(process, bFullProcessInfo, EnableCycleCpuUsage ? 0 : (iLinuxStyleCPU == 1 ? sysTotalTimePerCPU : sysTotalTime));

		if (EnableCycleCpuUsage)
			sysTotalCycleTime += pProcess->GetCpuStats().CycleDelta.Delta;

		//pProcess->UpdateThreadData(process, bFullProcessInfo, EnableCycleCpuUsage ? 0 : (iLinuxStyleCPU ? sysTotalTimePerCPU : sysTotalTime)); // a thread can ever only use one CPU to use linux style ylways

		pProcess->UpdateTokenData(MonitorTokenChange);

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
                if (EnableCycleCpuUsage)
                    process = &m->InterruptsProcessInformation;
                else
                    process = &m->DpcsProcessInformation;
            }
        }
	}

	QMap<quint64, CProcessPtr>	Processes = GetProcessList();

	if (EnableCycleCpuUsage)
	{
		quint64 sysTotalCycleTimePerCPU = sysTotalCycleTime / m_CpuCount;

		foreach(const CProcessPtr& pProcess, Processes)
		{
			if (pProcess->IsMarkedForRemoval())
				continue;

			pProcess.staticCast<CWinProcess>()->UpdateCPUCycles(iLinuxStyleCPU == 1 ? sysTotalTimePerCPU : sysTotalTime, iLinuxStyleCPU == 1 ? sysTotalCycleTimePerCPU : sysTotalCycleTime);
		}

		UpdateCPUCycles(sysTotalCycleTime, sysIdleCycleTime);
	}

	// parent retention
	QMap<quint64, int> ChildCount;
	if (theConf->GetBool("Options/EnableParrentRetention", true))
	{
		foreach(const CProcessPtr& pProcess, Processes) {
			CProcessPtr pParent = Processes.value(pProcess->GetParentId());
			if (!pParent.isNull() && pProcess->ValidateParent(pParent.data()))
				ChildCount[pProcess->GetParentId()]++;
		}
	}

	// purle all processes left as thay are not longer running

	QWriteLocker Locker(&m_ProcessMutex);
	foreach(quint64 ProcessID, OldProcesses.keys())
	{
		QSharedPointer<CWinProcess> pProcess = m_ProcessList.value(ProcessID).staticCast<CWinProcess>();
		if (pProcess->CanBeRemoved() && !ChildCount.contains(ProcessID))
		{
			m_ProcessList.remove(ProcessID);
			Removed.insert(ProcessID);
		}
		else if (!pProcess->IsMarkedForRemoval())
		{
			if (pProcess->IsHiddenProcess() && pProcess->CheckIsRunning())
				continue;

			pProcess->MarkForRemoval();
			pProcess->UnInit();
			Changed.insert(ProcessID);
		}
	}
	Locker.unlock();

	// Cache processes for later use when updating threads
	QWriteLocker ProcLocker(&m->ProcLocker);
	if (m->Processes)
		PhFree(m->Processes);
	m->Processes = processes;
	m->bFullProcessInfo = bFullProcessInfo;
	m->sysTotalTime = sysTotalTime;
	m->sysTotalCycleTime = sysTotalCycleTime;
	ProcLocker.unlock();

	// PhFree(processes);

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

int CWindowsAPI::FindHiddenProcesses()
{
	int count = 0;

	PVOID processes; // get the most up do date list
	if (!NT_SUCCESS(PhEnumProcesses(&processes)))
		return count; 

	HANDLE processHandle = NULL;
	for (;;)
	{
		HANDLE enumProcessHandle;
		if (!NT_SUCCESS(NtGetNextProcess(processHandle, PROCESS_QUERY_LIMITED_INFORMATION, 0, 0, &enumProcessHandle)))
			break;
		if(processHandle != NULL)
			NtClose(processHandle);
		processHandle = enumProcessHandle;

		PROCESS_EXTENDED_BASIC_INFORMATION basicInfo;
		if (!NT_SUCCESS(PhGetProcessExtendedBasicInformation(processHandle, &basicInfo)))
			continue; // wa cant do anythign with this one
		
		if (PhFindProcessInformation(processes, basicInfo.BasicInfo.UniqueProcessId))
			continue; // already known
			
		QSharedPointer<CWinProcess> pProcess = GetProcessByID((quint64)basicInfo.BasicInfo.UniqueProcessId, true).staticCast<CWinProcess>();
		if (!pProcess)
			continue; // should not happen but in case
		
		if (!basicInfo.IsProcessDeleting) {
			pProcess->MarkAsHidden();
			count++;
		}

		if (!pProcess->IsFullyInitialized())
		{
			pProcess->SetParentId((quint64)basicInfo.BasicInfo.InheritedFromUniqueProcessId);

			KERNEL_USER_TIMES times;
			if (NT_SUCCESS(PhGetProcessTimes(processHandle, &times)))
				pProcess->SetRawCreateTime(times.CreateTime.QuadPart);

			if (!pProcess->IsHandleValid())
			{
				PPH_STRING fileName;
				if (NT_SUCCESS(PhGetProcessImageFileName(processHandle, &fileName)))
				{
					QString FileName = CastPhString(fileName);
					int pos = FileName.lastIndexOf("\\");
					pProcess->SetName(FileName.mid(pos + 1));
					pProcess->SetFileName(FileName);
				}
			}
		}
	}
	if(processHandle != NULL)
		NtClose(processHandle);

    PhFree(processes);

	return count;
}

bool CWindowsAPI::UpdateThreads(CWinProcess* pProcess)
{
	HANDLE ProcessId = (HANDLE)pProcess->GetProcessId();

	QReadLocker Locker(&m->ProcLocker);
	PSYSTEM_PROCESS_INFORMATION process = m->Processes ? PhFindProcessInformation(m->Processes, ProcessId) : NULL;
	if (process == NULL)
		return false;

	bool EnableCycleCpuUsage = theConf->GetBool("Options/EnableCycleCpuUsage", true);
	int iLinuxStyleCPU = theConf->GetInt("Options/LinuxStyleCPU", 2);

	return pProcess->UpdateThreadData(process, m->bFullProcessInfo, iLinuxStyleCPU ? (m->sysTotalTime / m_CpuCount) : m->sysTotalTime, EnableCycleCpuUsage ? (iLinuxStyleCPU ? (m->sysTotalCycleTime / m_CpuCount) : m->sysTotalCycleTime) : 0);
}

CProcessPtr CWindowsAPI::GetProcessByID(quint64 ProcessId, bool bAddIfNew)
{
	QSharedPointer<CWinProcess> pProcess = CSystemAPI::GetProcessByID(ProcessId, false).staticCast<CWinProcess>();
	if (!pProcess && bAddIfNew)
	{
		QWriteLocker Locker(&m_ProcessMutex);
		pProcess = m_ProcessList.value(ProcessId).staticCast<CWinProcess>();
		if(pProcess) // just in case between CSystemAPI::GetProcessByID and QWriteLocker something happened
			return pProcess;

		pProcess = QSharedPointer<CWinProcess>(new CWinProcess());
		pProcess->moveToThread(theAPI->thread());
		pProcess->InitStaticData(ProcessId);
		//ASSERT(!m_ProcessList.contains(ProcessId)); 
		m_ProcessList.insert(ProcessId, pProcess);
	}
	return pProcess;
}

struct CWindowsAPI__WndEnumStruct
{
	QHash<quint64, QPair<quint64, quint64> > OldWindows; // QMap<HWND, QPair<PID, TID> >

	QReadWriteLock* pWindowMutex;
	QHash<quint64, QMultiMap<quint64, quint64> >* pWindowMap; // QMap<PID, HWND>
	QHash<quint64, QPair<quint64, quint64> >* pWindowRevMap; // QMap<HWND, QPair<PID, TID> >
};

void CWindowsAPI__WndEnumProc(quint64 hWnd, /*quint64 hParent,*/ void* Param)
{
	CWindowsAPI__WndEnumStruct* pContext = (CWindowsAPI__WndEnumStruct*)Param;

	if (!pContext->OldWindows.remove(hWnd))
	{
		ULONG processId;
		ULONG threadId = GetWindowThreadProcessId((HWND)hWnd, &processId);

		QWriteLocker Locker(pContext->pWindowMutex);
		pContext->pWindowMap->operator[](processId).insertMulti(threadId, hWnd);
		pContext->pWindowRevMap->insert(hWnd, qMakePair((quint64)processId, (quint64)threadId));
	}
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
	for (QHash<quint64, QPair<quint64, quint64> >::iterator I = Context.OldWindows.begin(); I != Context.OldWindows.end(); ++I)
	{
		quint64 hWnd = I.key();
		QPair<quint64, quint64> PidTid = Context.pWindowRevMap->take(hWnd);

		QHash<quint64, QMultiMap<quint64, quint64> >::iterator J = Context.pWindowMap->find(PidTid.first);
		if (J != Context.pWindowMap->end())
		{
			J->remove(PidTid.second, hWnd);
			if (J->isEmpty())
				Context.pWindowMap->erase(J);
		}
	}

	return m_WindowRevMap.size();
}

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

QMultiMap<quint64, CSocketPtr>::iterator FindSocketEntry(QMultiMap<quint64, CSocketPtr> &Sockets, quint64 ProcessId, quint32 ProtocolType, 
							const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort, CSocketInfo::EMatchMode Mode)
{
	quint64 HashID = CSocketInfo::MkHash(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort);

	for (QMultiMap<quint64, CSocketPtr>::iterator I = Sockets.find(HashID); I != Sockets.end() && I.key() == HashID; I++)
	{
		if (I.value().staticCast<CWinSocket>()->Match(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort, Mode))
			return I;
	}

	// no matching entry
	return Sockets.end();
}

bool CWindowsAPI::UpdateSocketList()
{
	if (!NetworkImportDone)
    {
        WSADATA wsaData;
        // Make sure WSA is initialized. (wj32)
        WSAStartup(WINSOCK_VERSION, &wsaData);
        NetworkImportDone = TRUE;
    }

	// Note: Important; first we must update the DNS cache to be able to properly assign DNS entries to the sockets!!!
	if(theConf->GetBool("Options/MonitorDnsCache", false))
		UpdateDnsCache();


	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	QVector<CWinSocket::SSocket> connections = CWinSocket::GetNetworkConnections();

	//QWriteLocker Locker(&m_SocketMutex);
	
	// Copy the socket map Map
	QMultiMap<quint64, CSocketPtr> OldSockets = GetSocketList();

	for (ulong i = 0; i < connections.size(); i++)
	{
		QSharedPointer<CWinSocket> pSocket;

		bool bAdd = false;
		QMultiMap<quint64, CSocketPtr>::iterator I = FindSocketEntry(OldSockets, (quint64)connections[i].ProcessId, (quint64)connections[i].ProtocolType, 
			connections[i].LocalEndpoint.Address, connections[i].LocalEndpoint.Port, connections[i].RemoteEndpoint.Address, connections[i].RemoteEndpoint.Port, CSocketInfo::eStrict);
		if (I == OldSockets.end())
		{
			pSocket = QSharedPointer<CWinSocket>(new CWinSocket());
			bAdd = pSocket->InitStaticData((quint64)connections[i].ProcessId, (quint64)connections[i].ProtocolType, 
				connections[i].LocalEndpoint.Address, connections[i].LocalEndpoint.Port, connections[i].RemoteEndpoint.Address, connections[i].RemoteEndpoint.Port);

			if ((quint64)connections[i].ProcessId)
			{
				CProcessPtr pProcess = theAPI->GetProcessByID((quint64)connections[i].ProcessId, true); // Note: this will add the process and load some basic data if it does not already exist
				pSocket->LinkProcess(pProcess);
				pProcess->AddSocket(pSocket);
			}

			QWriteLocker Locker(&m_SocketMutex);
			m_SocketList.insertMulti(pSocket->m_HashID, pSocket);
		}
		else
		{
			pSocket = I.value().staticCast<CWinSocket>();
			OldSockets.erase(I);
		}

		bool bChanged = false;
		if (bAdd || !pSocket->HasStaticDataEx()) { // on network events we may add new, yet un enumerated, sockets 
			pSocket->InitStaticDataEx(&(connections[i]), bAdd);
			bChanged = true;
		}
		bChanged = pSocket->UpdateDynamicData(&(connections[i]));

		if (bAdd)
			Added.insert(pSocket->m_HashID);
		else if (bChanged)
			Changed.insert(pSocket->m_HashID);
	}

	quint64 UdpPersistenceTime = theConf->GetUInt64("Options/UdpPersistenceTime", 3000);
	// For UDP Traffic keep the pseudo connections listed for a while
	for (QMultiMap<quint64, CSocketPtr>::iterator I = OldSockets.begin(); I != OldSockets.end(); )
	{
		QSharedPointer<CWinSocket> pSocket = I.value().staticCast<CWinSocket>();
		bool bIsUDPPseudoCon = (pSocket->GetProtocolType() & NET_TYPE_PROTOCOL_UDP) != 0 && pSocket->GetRemotePort() != 0;

		if (!pSocket->HasStaticDataEx())
		{
			// this is eider a UDP pseudo connection bIsUDPPseudoCon == true or
			// this was a real socket, but it was closed before we could get its data from the connection Table
			pSocket->InitStaticDataEx(NULL, false);
		}
		
		if (bIsUDPPseudoCon && pSocket->GetIdleTime() < UdpPersistenceTime) // Active UDP pseudo connection
		{
			pSocket->UpdateStats();
			I = OldSockets.erase(I);
		}
		else // closed connection
		{
			quint32 State = pSocket->GetState();
			if (State != -1 && State != MIB_TCP_STATE_CLOSED)
				pSocket->SetClosed();
			I++;
		}
	}

	QWriteLocker Locker(&m_SocketMutex);
	// purge all sockets left as thay are not longer running
	for(QMultiMap<quint64, CSocketPtr>::iterator I = OldSockets.begin(); I != OldSockets.end(); ++I)
	{
		QSharedPointer<CWinSocket> pSocket = I.value().staticCast<CWinSocket>();
		if (pSocket->CanBeRemoved())
		{
			if(CProcessPtr pProcess = pSocket->GetProcess().toStrongRef().staticCast<CProcessInfo>())
				pProcess->RemoveSocket(pSocket);
			m_SocketList.remove(I.key(), I.value());
			Removed.insert(I.key());
		}
		else if (!pSocket->IsMarkedForRemoval()) 
		{
			pSocket->MarkForRemoval();
		}
	}
	Locker.unlock();

	emit SocketListUpdated(Added, Changed, Removed);

	return true;
}

CSocketPtr CWindowsAPI::FindSocket(quint64 ProcessId, quint32 ProtocolType,
	const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort, CSocketInfo::EMatchMode Mode)
{
	QReadLocker Locker(&m_SocketMutex);

	QMultiMap<quint64, CSocketPtr>::iterator I = FindSocketEntry(m_SocketList, ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort, Mode);
	if (I == m_SocketList.end())
		return CSocketPtr();
	return I.value();
}

void CWindowsAPI::AddNetworkIO(int Type, quint32 TransferSize)
{
	QWriteLocker Locker(&m_StatsMutex);

	switch (Type)
	{
	case EtwNetworkReceiveType:	m_Stats.Net.AddReceive(TransferSize); break;
	case EtwNetworkSendType:	m_Stats.Net.AddSend(TransferSize); break;
	}
}

void CWindowsAPI::AddDiskIO(int Type, quint32 TransferSize)
{
	QWriteLocker Locker(&m_StatsMutex);

	switch (Type)
	{
	case EtwDiskReadType:		m_Stats.Disk.AddRead(TransferSize); break;
	case EtwDiskWriteType:		m_Stats.Disk.AddWrite(TransferSize); break;
	}
}

void CWindowsAPI::OnNetworkEvent(int Type, quint64 ProcessId, quint64 ThreadId, quint32 ProtocolType, quint32 TransferSize, // note ThreadId is always -1
								QHostAddress LocalAddress, quint16 LocalPort, QHostAddress RemoteAddress, quint16 RemotePort)
{
    // Note: there is always the possibility of us receiving the event too early,
    // before the process item or network item is created. 

	AddNetworkIO(Type, TransferSize);

	CSocketInfo::EMatchMode Mode = CSocketInfo::eFuzzy;

	if ((ProtocolType & NET_TYPE_PROTOCOL_UDP) != 0 && theConf->GetBool("Options/UseUDPPseudoConnectins", false))
		Mode = CSocketInfo::eStrict;

	QSharedPointer<CWinSocket> pSocket = FindSocket(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort, Mode).staticCast<CWinSocket>();
	bool bAdd = false;
	if (pSocket.isNull())
	{
		//qDebug() << ProcessId  << Type << LocalAddress.toString() << LocalPort << RemoteAddress.toString() << RemotePort;

		pSocket = QSharedPointer<CWinSocket>(new CWinSocket());
		bAdd = pSocket->InitStaticData(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort);

		if (ProcessId)
		{
			CProcessPtr pProcess = theAPI->GetProcessByID(ProcessId, true); // Note: this will add the process and load some basic data if it does not already exist
			pSocket->LinkProcess(pProcess);
			pProcess->AddSocket(pSocket);
		}

		if (Type == EvlFirewallBlocked)
			pSocket->SetBlocked();
		QWriteLocker Locker(&m_SocketMutex);
		m_SocketList.insertMulti(pSocket->m_HashID, pSocket);
	}

	QSharedPointer<CWinProcess> pProcess = pSocket->GetProcess().toStrongRef().staticCast<CWinProcess>();
    if (!pProcess.isNull())
		pProcess->AddNetworkIO(Type, TransferSize);

	pSocket->AddNetworkIO(Type, TransferSize);
}

void CWindowsAPI::OnDnsResEvent(quint64 ProcessId, quint64 ThreadId, const QString& HostName, const QStringList& Results)
{
	if (Results.size() == 1 && Results.first() == HostName)
		return;
	/*
	"192.168.163.1" "192.168.163.1;"
	"localhost" "[::1]:8307;127.0.0.1:8307;" <- wtf is this why is there a port?!
	"DESKTOP" "fe80::189a:f1c3:3e87:be81%12;192.168.10.12;"
	"telemetry.malwarebytes.com" "54.149.69.204;54.200.191.52;54.70.191.27;54.149.66.105;54.244.17.248;54.148.98.86;"
	"web.whatsapp.com" "31.13.84.51;"
	*/
	//qDebug() << HostName << Results.join(";");

	QList<QHostAddress> Addresses;
	foreach(const QString& Result, Results)
	{
		QHostAddress Address(Result);
		if (Address.isNull())
		{
			QUrl url("bla://" + Result);
			Address = QHostAddress(url.host());
		}

		if (!Address.isNull())
			Addresses.append(Address);
	}
	if (Addresses.isEmpty())
		return;

	CProcessPtr pProcess = theAPI->GetProcessByID(ProcessId, true); // Note: this will add the process and load some basic data if it does not already exist
	if (!pProcess.isNull())
		pProcess->UpdateDns(HostName, Addresses);
}

void CWindowsAPI::OnFileEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, const QString& FileName)
{
	// Since Windows 8, we no longer get the correct process/thread IDs in the
	// event headers for file events. 

	/*
	QWriteLocker Locker(&m_FileNameMutex);

	switch (Type)
    {
    case EtwFileNameType: // Name
        break;
    case EtwFileCreateType: // FileCreate
	case EtwFileRundownType:
		m_FileNames.insert(FileId, FileName);
        break;
    case EtwFileDeleteType: // FileDelete
		m_FileNames.remove(FileId);
        break;
    }*/
}

/*QString CWindowsAPI::GetFileNameByID(quint64 FileId) const
{
	QReadLocker Locker(&m_FileNameMutex);
	return m_FileNames.value(FileId, tr("Unknown file name"));;
}*/

void CWindowsAPI::OnDiskEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, quint32 IrpFlags, quint32 TransferSize, quint64 HighResResponseTime)
{
	//if (m_UseDiskCounters == eUseForSystem)
	if (m_UseDiskCounters)
		return;

	AddDiskIO(Type, TransferSize);

	//if (m_UseDiskCounters == eForProgramsOnly)
	//	return;

	QSharedPointer<CWinProcess> pProcess;
	if (ProcessId != -1)
		pProcess = GetProcessByID(ProcessId).staticCast<CWinProcess>();
	else
		pProcess = GetProcessByThreadID(ThreadId).staticCast<CWinProcess>();

	if (!pProcess.isNull())
		pProcess->AddDiskIO(Type, TransferSize);

	// todo: FileName ist often unknown and FileId doesn't seam to have relations to open handled
	//			we want to be able to show for eadch open file the i/o that must be doable somehow...

	//QString FileName = GetFileNameByID(FileId);
	//pHandle->AddDiskIO(Type, TransferSize);

}

void CWindowsAPI::OnProcessEvent(int Type, quint32 ProcessId, QString CommandLine, QString FileName, quint32 ParentId, quint64 TimeStamp)
{
	if (Type != EtwProcessStarted)
		return;

	QSharedPointer<CWinProcess> pProcess = GetProcessByID(ProcessId, true).staticCast<CWinProcess>();
	if (pProcess)
	{
		// Note: the etw mechanism is a bit slow so the process may be terminates already 
		//			so try at list to fill in what little we know from the event metadata.
		if (!pProcess->IsFullyInitialized())
		{
			pProcess->SetParentId(ParentId);
			pProcess->SetRawCreateTime(TimeStamp);

			if (!pProcess->IsHandleValid()) // check if we failed to pen the process (as it already terminated)
			{
				pProcess->SetName(FileName);
				
				QString FilePath = GetPathFromCmd(CommandLine, ProcessId, FileName, ParentId);
				if (!FilePath.isEmpty())
					pProcess->SetFileName(FilePath);
			}
		}

		if(pProcess->GetCommandLineStr().isEmpty())
			pProcess->SetCommandLineStr(CommandLine);
	}
}

bool CWindowsAPI::UpdateOpenFileList()
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

    PSYSTEM_HANDLE_INFORMATION_EX handleInfo;
    if (!NT_SUCCESS(PhEnumHandlesEx(&handleInfo)))
        return false;

	bool bGetDanymicData = theConf->GetBool("Options/OpenFileGetPosition", false);

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

		quint64 HandleID = CWinHandle::MakeID(handle->HandleValue, handle->UniqueProcessId);

		QSharedPointer<CWinHandle> pWinHandle = OldHandles.take(HandleID).staticCast<CWinHandle>();

		bool WasReused = false;
		// Comment from: hndlprv.c
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
			Removed.insert(HandleID);

		HANDLE &ProcessHandle = ProcessHandles[handle->UniqueProcessId];

		if (bAdd || WasReused || bGetDanymicData)
		{
			if (ProcessHandle == NULL)
			{
				if (!NT_SUCCESS(PhOpenProcess(&ProcessHandle, PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, (HANDLE)handle->UniqueProcessId)))
					continue;
			}
		}

		if (bAdd || WasReused)
		{
			pWinHandle->InitStaticData(handle, TimeStamp);
            pWinHandle->InitExtData(handle,(quint64)ProcessHandle);	

			// ignore results without a valid file Name
			if (pWinHandle->GetFileName().isEmpty())
				continue; 

			if (bAdd)
			{
				Added.insert(HandleID);
				QWriteLocker Locker(&m_OpenFilesMutex);
				ASSERT(!m_OpenFilesList.contains(HandleID));
				m_OpenFilesList.insert(HandleID, pWinHandle);
				//m_HandleByObject.insertMulti(pWinHandle->GetObject(), pWinHandle);
			}
		}
		
		if (bGetDanymicData)
		{
			if (pWinHandle->UpdateDynamicData(handle, (quint64)ProcessHandle))
				Changed.insert(HandleID);
		}

		//if (pWinHandle->GetProcess().isNull())
		//	pWinHandle->SetProcess(GetProcessByID(handle->UniqueProcessId));
	}

	foreach(HANDLE ProcessHandle, ProcessHandles)
		NtClose(ProcessHandle);
	ProcessHandles.clear();

	PhFree(handleInfo);

	QWriteLocker Locker(&m_OpenFilesMutex);
	// purle all handles left as thay are not longer valid
	foreach(quint64 HandleID, OldHandles.keys())
	{
		QSharedPointer<CWinHandle> pWinHandle = OldHandles.value(HandleID).staticCast<CWinHandle>();
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

VOID PhpWorkaroundWindows10ServiceTypeBug(_Inout_ LPENUM_SERVICE_STATUS_PROCESS ServieEntry)
{
    // https://github.com/processhacker2/processhacker/issues/120 (dmex)
    if (ServieEntry->ServiceStatusProcess.dwServiceType == SERVICE_WIN32)
        ServieEntry->ServiceStatusProcess.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
    if (ServieEntry->ServiceStatusProcess.dwServiceType == (SERVICE_WIN32 | SERVICE_USER_SHARE_PROCESS | SERVICE_USERSERVICE_INSTANCE))
        ServieEntry->ServiceStatusProcess.dwServiceType = SERVICE_USER_SHARE_PROCESS | SERVICE_USERSERVICE_INSTANCE;
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

		QSharedPointer<CWinService> pService = OldService.take(Name).staticCast<CWinService>();

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

		quint64 uOldServicePID = pService->GetPID();
		if (pService->UpdatePID(service)) // true if changed
		{
			QWriteLocker Locker(&m_ServiceMutex);
			if (uOldServicePID != 0)
			{
				QList<QString>* pList = &m_ServiceByPID[uOldServicePID];
				pList->removeAll(Name);
				if (pList->isEmpty())
					m_ServiceByPID.remove(uOldServicePID);
			}

			quint64 uServicePID = pService->GetPID();
			if (uServicePID != 0)
			{
				QList<QString>* pList = &m_ServiceByPID[uServicePID];
				if (!pList->contains(Name)) // just in case
					pList->append(Name);
			}
			Locker.unlock();

			if (uOldServicePID)
			{
				QSharedPointer<CWinProcess> pProcess = GetProcessByID(uOldServicePID, false).staticCast<CWinProcess>();
				if(pProcess)
					pProcess->RemoveService(Name);
			}

			if (uServicePID)
			{
				QSharedPointer<CWinProcess> pProcess = GetProcessByID(uServicePID, true).staticCast<CWinProcess>();
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
		QSharedPointer<CWinService> pService = m_ServiceList.value(Name).staticCast<CWinService>();
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

		QSharedPointer<CWinDriver> pWinDriver = OldDrivers.take(BinaryPath).staticCast<CWinDriver>();

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
		QSharedPointer<CWinDriver> pDriver = m_DriverList.value(Name).staticCast<CWinDriver>();
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

bool CWindowsAPI::UpdatePoolTable()
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	PSYSTEM_POOLTAG_INFORMATION poolTagTable;
    if (!NT_SUCCESS(EnumPoolTagTable((void**)&poolTagTable)))
    {
        return false;
    }

	QMap<quint64, CPoolEntryPtr> OldPoolTable = GetPoolTableList();

    quint64 PagedPoolAllocs = 0;
    quint64 PagedPoolFrees = 0;
    quint64 NonPagedPoolAllocs = 0;
    quint64 NonPagedPoolFrees = 0;

	for (ULONG i = 0; i < poolTagTable->Count; i++)
	{
		SYSTEM_POOLTAG poolTagInfo = poolTagTable->TagInfo[i];
		quint64 TagName = poolTagInfo.TagUlong;

		CPoolEntryPtr pPoolEntry = OldPoolTable.take(TagName);
		
		bool bAdd = false;
		if (pPoolEntry.isNull())
		{
			pPoolEntry = CPoolEntryPtr(new CWinPoolEntry());
			QPair<QString, QString> Info = UpdatePoolTagBinaryName(&m->PoolTableDB, TagName);
			bAdd = pPoolEntry->InitStaticData(TagName, Info.first, Info.second);
			QWriteLocker Locker(&m_PoolTableMutex);
			ASSERT(!m_PoolTableList.contains(TagName));
			m_PoolTableList.insert(TagName, pPoolEntry);
		}

		bool bChanged = false;
		bChanged = pPoolEntry->UpdateDynamicData(&poolTagInfo);
		
		PagedPoolAllocs += poolTagInfo.PagedAllocs;
		PagedPoolFrees += poolTagInfo.PagedFrees;
		NonPagedPoolAllocs += poolTagInfo.NonPagedAllocs;
		NonPagedPoolFrees += poolTagInfo.NonPagedFrees;

		if (bAdd)
			Added.insert(TagName);
		else if (bChanged)
			Changed.insert(TagName);
	}

	QWriteLocker Locker(&m_PoolTableMutex);
	foreach(quint64 TagName, OldPoolTable.keys())
	{
		CPoolEntryPtr pPoolEntry = m_PoolTableList.value(TagName);
		if (pPoolEntry->CanBeRemoved())
		{
			m_PoolTableList.remove(TagName);
			Removed.insert(TagName);
		}
		else if (!pPoolEntry->IsMarkedForRemoval())
			pPoolEntry->MarkForRemoval();
	}
	Locker.unlock();

	PhFree(poolTagTable);

	emit PoolListUpdated(Added, Changed, Removed);

	if (!m->PoolStatsOk)
	{
		// Note: Winsows 10 starting with 1809 does not longer seam to provide a value for:
		//			PagedPoolAllocs, PagedPoolFrees, NonPagedPoolAllocs, NonPagedPoolFrees
		QWriteLocker Locker(&m_StatsMutex);
		m_CpuStats.PagedAllocsDelta.Update64(PagedPoolAllocs);
		m_CpuStats.PagedFreesDelta.Update64(PagedPoolFrees);
		m_CpuStats.NonPagedAllocsDelta.Update64(NonPagedPoolAllocs);
		m_CpuStats.NonPagedFreesDelta.Update64(NonPagedPoolFrees);
	}

	return true; 
}

bool CWindowsAPI::InitWindowsInfo()
{
	//bool bServer = false;
	QString ReleaseId;
	HANDLE keyHandle;
	static PH_STRINGREF currentVersion = PH_STRINGREF_INIT(L"Software\\Microsoft\\Windows NT\\CurrentVersion");
    if (NT_SUCCESS(PhOpenKey(&keyHandle, KEY_READ, PH_KEY_LOCAL_MACHINE, &currentVersion, 0)))
    {
		m_SystemName = CastPhString(PhQueryRegistryString(keyHandle, L"ProductName"));
		m_SystemType = CastPhString(PhQueryRegistryString(keyHandle, L"InstallationType"));
		//bServer = m_SystemType == "Server";
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

QMultiMap<QString, CDnsCacheEntryPtr> CWindowsAPI::GetDnsEntryList() const
{
	return m_pDnsResolver->GetEntryList();
}

bool CWindowsAPI::UpdateDnsCache()
{
	return m_pDnsResolver->UpdateDnsCache();
}

void CWindowsAPI::FlushDnsCache()
{
	m_pDnsResolver->FlushDnsCache();
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

// MSDN: FILETIME Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).

quint64 FILETIME2ms(quint64 fileTime)
{
	if (fileTime < 116444736000000000ULL)
		return 0;
	return (fileTime - 116444736000000000ULL) / 10000ULL;
}

time_t FILETIME2time(quint64 fileTime)
{
	return FILETIME2ms(fileTime) / 1000ULL;
}


