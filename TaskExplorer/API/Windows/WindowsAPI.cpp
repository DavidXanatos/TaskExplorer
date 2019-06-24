#include "stdafx.h"
#include "WindowsAPI.h"
#include "ProcessHacker.h"
#include "WinHandle.h"
#include <lm.h>
#include <ras.h>
#include <raserror.h>


#include "../TaskExplorer/GUI/TaskExplorer.h"

int _QHostAddress_type = qRegisterMetaType<QHostAddress>("QHostAddress");

int _QSet_qquint64ype = qRegisterMetaType<QSet<quint64>>("QSet<quint64>");
int _QSet_QString_type = qRegisterMetaType<QSet<QString>>("QSet<QString>");

ulong g_fileObjectTypeIndex = ULONG_MAX;

struct SWindowsAPI
{
	SWindowsAPI()
	{
		//PowerInformation = new PROCESSOR_POWER_INFORMATION[PhSystemBasicInformation.NumberOfProcessors];
		//memset(&PowerInformation, 0, sizeof(PROCESSOR_POWER_INFORMATION));

		CpuIdleCycleTime = new LARGE_INTEGER[PhSystemBasicInformation.NumberOfProcessors];
		memset(CpuIdleCycleTime, 0, sizeof(LARGE_INTEGER)*PhSystemBasicInformation.NumberOfProcessors);
	
		CpuSystemCycleTime = new LARGE_INTEGER[PhSystemBasicInformation.NumberOfProcessors];
		memset(CpuSystemCycleTime, 0, sizeof(LARGE_INTEGER)*PhSystemBasicInformation.NumberOfProcessors);

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
		delete[] CpuIdleCycleTime;
		delete[] CpuSystemCycleTime;
	}

	//PPROCESSOR_POWER_INFORMATION PowerInformation;

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

	m_InstalledMemory = 0;
	m_AvailableMemory = 0;
	m_ReservedMemory = 0;

	m_pEventMonitor = NULL;
	m_pSymbolProvider = NULL;

	m = new SWindowsAPI();
}

bool CWindowsAPI::Init()
{
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
		m_CoreCount = m_CpuCount = PhSystemBasicInformation.NumberOfProcessors;
	}

	m_InstalledMemory = ::GetInstalledMemory();
	m_AvailableMemory = UInt32x32To64(PhSystemBasicInformation.NumberOfPhysicalPages, PAGE_SIZE);
	m_ReservedMemory = m_InstalledMemory - UInt32x32To64(PhSystemBasicInformation.NumberOfPhysicalPages, PAGE_SIZE);

	WCHAR brandString[49];
	PhSipGetCpuBrandString(brandString);

	m_CPU_String = QString::fromWCharArray(brandString);

	m_pEventMonitor = new CEventMonitor();

	//NtPowerInformation(ProcessorInformation, NULL, 0, m->PowerInformation, sizeof(PROCESSOR_POWER_INFORMATION) * PhSystemBasicInformation.NumberOfProcessors));
    

	connect(m_pEventMonitor, SIGNAL(NetworkEvent(int, quint64, quint64, ulong, ulong, const QHostAddress&, quint16, const QHostAddress&, quint16)), this, SLOT(OnNetworkEvent(int, quint64, quint64, ulong, ulong, const QHostAddress&, quint16, const QHostAddress&, quint16)));
	connect(m_pEventMonitor, SIGNAL(FileEvent(int, quint64, quint64, quint64, const QString&)), this, SLOT(OnFileEvent(int, quint64, quint64, quint64, const QString&)));
	connect(m_pEventMonitor, SIGNAL(DiskEvent(int, quint64, quint64, quint64, ulong, ulong, quint64)), this, SLOT(OnDiskEvent(int, quint64, quint64, quint64, ulong, ulong, quint64)));

	m_pEventMonitor->Init();

	m_pSymbolProvider = CSymbolProviderPtr(new CSymbolProvider());
	m_pSymbolProvider->Init();

	return true;
}

CWindowsAPI::~CWindowsAPI()
{
	delete m_pEventMonitor;

	delete m;
}

bool CWindowsAPI::RootAvaiable()
{
	return KphIsConnected();
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

	m_CommitedMemory = UInt32x32To64(PerfInformation.CommittedPages, PAGE_SIZE);
	m_CommitedMemoryPeak = UInt32x32To64(PerfInformation.PeakCommitment, PAGE_SIZE);
	m_MemoryLimit = UInt32x32To64(PerfInformation.CommitLimit, PAGE_SIZE);
	m_PagedMemory = UInt32x32To64(PerfInformation.PagedPoolPages, PAGE_SIZE);
	m_PersistentPagedMemory = UInt32x32To64(PerfInformation.ResidentPagedPoolPage, PAGE_SIZE);
	m_NonPagedMemory = UInt32x32To64(PerfInformation.NonPagedPoolPages, PAGE_SIZE);
	m_PhysicalUsed = UInt32x32To64(PhSystemBasicInformation.NumberOfPhysicalPages - PerfInformation.AvailablePages, PAGE_SIZE);
	m_CacheMemory = UInt32x32To64(PerfInformation.ResidentSystemCachePage, PAGE_SIZE);
}

void CWindowsAPI::UpdateNetStats()
{
	MIB_IF_TABLE2* pIfTable = NULL;
	if (GetIfTable2(&pIfTable) == NO_ERROR)
	{
		quint64 uRecvTotal = 0;
		quint64 uRecvCount = 0;
		quint64 uSentTotal = 0;
		quint64 uSentCount = 0;
		for (int i = 0; i < pIfTable->NumEntries; i++)
		{
			MIB_IF_ROW2* pIfRow = (MIB_IF_ROW2*)& pIfTable->Table[i];

			if (pIfRow->InterfaceAndOperStatusFlags.FilterInterface)
				continue;
			//qDebug() << QString::fromWCharArray(pIfRow->Description);

			uRecvTotal += pIfRow->InOctets;
			uRecvCount += pIfRow->InUcastPkts + pIfRow->InNUcastPkts;

			uSentTotal += pIfRow->OutOctets;
			uSentCount += pIfRow->OutUcastPkts + pIfRow->OutNUcastPkts;
		}

		FreeMibTable(pIfTable);

		QWriteLocker Locker(&m_StatsMutex);

		m_Stats.NetIf.ReceiveRaw = uRecvTotal;
		m_Stats.NetIf.ReceiveCount = uSentCount;

		m_Stats.NetIf.SendRaw = uSentTotal;
		m_Stats.NetIf.SendCount = uRecvCount;
	}


	/////////////////////////////////////////////////////////////////////
	// ras connections

	LPRASCONN lpRasConn = new RASCONN[1];
	DWORD cb = sizeof(RASCONN);
	lpRasConn->dwSize = sizeof(RASCONN);
	DWORD dwConnections = 0;
	DWORD dwRet = RasEnumConnections(lpRasConn, &cb, &dwConnections);
	if (dwRet == ERROR_BUFFER_TOO_SMALL)
	{
		delete[] lpRasConn;
		lpRasConn = new RASCONN[dwConnections];
		lpRasConn->dwSize = sizeof(RASCONN);
		dwRet = RasEnumConnections(lpRasConn, &cb, &dwConnections);
	}

	if (dwRet == ERROR_SUCCESS)
	{
		QWriteLocker Locker(&m_StatsMutex);

		// given that nowadays ras is almost exclusivly used for VPN's there is no point in differentiating here by type

		//quint64 uVpnRecvTotal = 0;
		//quint64 uVpnRecvCount = 0;
		//quint64 uVpnSentTotal = 0;
		//quint64 uVpnSentCount = 0;

		quint64 uRasRecvTotal = 0;
		quint64 uRasRecvCount = 0;
		quint64 uRasSentTotal = 0;
		quint64 uRasSentCount = 0;

		QSet<QString> OldRasCons = m_RasCons.keys().toSet();

		for (DWORD i = 0; i < dwConnections; i++)
		{
			LPRASCONN pRasConn = &lpRasConn[i];

			QString EntryName = QString::fromWCharArray(pRasConn->szEntryName);

			SWinRasCon* pRasCon = NULL;
			QMap<QString, SWinRasCon>::iterator I = m_RasCons.find(EntryName);
			if (I == m_RasCons.end())
			{
				pRasCon = &m_RasCons.insert(EntryName, SWinRasCon()).value();
				pRasCon->EntryName = EntryName;
				pRasCon->DeviceName = QString::fromWCharArray(pRasConn->szDeviceName);
			}
			else
			{
				pRasCon = &I.value();
				OldRasCons.remove(EntryName);
			}

			RAS_STATS Statistics;
			Statistics.dwSize = sizeof(RAS_STATS);
			if (RasGetConnectionStatistics(pRasConn->hrasconn, &Statistics) != ERROR_SUCCESS)
				continue;

			//if (_wcsicmp(pRasConn->szDeviceType, RASDT_Vpn) == 0)
			//{
			//	uVpnRecvTotal += pRasCon->BytesRecv.FixValue(Statistics.dwBytesRcved);
			//	uVpnRecvCount += pRasCon->RecvCount = Statistics.dwFramesRcved;
			//
			//	uVpnSentTotal += pRasCon->BytesSent.FixValue(Statistics.dwBytesXmited);
			//	uVpnSentCount += pRasCon->SentCount = Statistics.dwFramesXmited;
			//}
			//else
			{
				uRasRecvTotal += pRasCon->BytesRecv.FixValue(Statistics.dwBytesRcved);
				uRasRecvCount += pRasCon->RecvCount = Statistics.dwFramesRcved;

				uRasSentTotal += pRasCon->BytesSent.FixValue(Statistics.dwBytesXmited);
				uRasSentCount += pRasCon->SentCount = Statistics.dwFramesXmited;
			}
		}

		foreach(const QString& EntryName, OldRasCons)
			m_RasCons.remove(EntryName);


		//m_Stats.NetVpn.ReceiveRaw = uVpnRecvTotal;
		//m_Stats.NetVpn.ReceiveCount = uVpnSentCount;
		//
		//m_Stats.NetVpn.SendRaw = uVpnSentTotal;
		//m_Stats.NetVpn.SendCount = uVpnRecvCount;

		m_Stats.NetRas.ReceiveRaw = uRasRecvTotal;
		m_Stats.NetRas.ReceiveCount = uRasSentCount;

		m_Stats.NetRas.SendRaw = uRasSentTotal;
		m_Stats.NetRas.SendCount = uRasRecvCount;
	}

	delete[] lpRasConn;


	/////////////////////////////////////////////////////////////////////
	// samba traffic

	LARGE_INTEGER ClientBytesReceived;
	LARGE_INTEGER ClientSendRaw;
	STAT_WORKSTATION_0* wrkStat = NULL;
	if (NT_SUCCESS(NetStatisticsGet(NULL, L"LanmanWorkstation", 0, 0, (LPBYTE*)&wrkStat)))
	{
		ClientBytesReceived = wrkStat->BytesReceived;
		//ClientReceiveCount = wrkStat->SmbsReceived;
		
		ClientSendRaw = wrkStat->BytesTransmitted;
		//ClientSendCount = wrkStat->SmbsTransmitted;

		NetApiBufferFree(wrkStat);
	}

	LARGE_INTEGER ServerBytesReceived;
	LARGE_INTEGER ServerSendRaw;
	_STAT_SERVER_0* srvStat = NULL;
	if (NT_SUCCESS(NetStatisticsGet(NULL, L"LanmanServer", 0, 0, (LPBYTE*)&srvStat)))
	{
		ServerBytesReceived.HighPart = srvStat->sts0_bytesrcvd_high;
		ServerBytesReceived.LowPart = srvStat->sts0_bytesrcvd_low;
	
		ServerSendRaw.HighPart = srvStat->sts0_bytessent_high;
		ServerSendRaw.LowPart = srvStat->sts0_bytessent_low;
	
		NetApiBufferFree(srvStat);
	}

	QWriteLocker Locker(&m_StatsMutex);

	m_Stats.SambaClient.ReceiveRaw = ClientBytesReceived.QuadPart;
	//m_Stats.SambaClient.ReceiveCount = ClientReceiveCount.QuadPart;
		
	m_Stats.SambaClient.SendRaw = ClientSendRaw.QuadPart;
	//m_Stats.SambaClient.SendCount = ClientSendCount.QuadPart;

	m_Stats.SambaServer.ReceiveRaw = ServerBytesReceived.QuadPart;
	//m_Stats.SambaServer.ReceiveCount = 

	m_Stats.SambaServer.SendRaw =ServerSendRaw.QuadPart;
	//m_Stats.SambaServer.SendCount = 
}

bool CWindowsAPI::UpdateSysStats()
{
	UpdatePerfStats();

	UpdateNetStats();

	QWriteLocker StatsLocker(&m_StatsMutex);
	m_Stats.UpdateStats();

	return true;
}

bool CWindowsAPI::UpdateProcessList()
{
	quint64 sysTotalTime; // total time for this update period
    quint64 sysTotalCycleTime = 0; // total cycle time for this update period
    quint64 sysIdleCycleTime = 0; // total idle cycle time for this update period

	bool PhEnableCycleCpuUsage = false; // todo add settings

    if (PhEnableCycleCpuUsage)
    {
        sysTotalTime = UpdateCpuStats(false);
        sysIdleCycleTime = UpdateCpuCycleStats();

		// sysTotalCycleTime = todo count from all processes what a mess
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
	if (!NT_SUCCESS(PhEnumProcesses(&processes)))
		return false;

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
			bAdd = pProcess->InitStaticData(process);
			QWriteLocker Locker(&m_ProcessMutex);
			ASSERT(!m_ProcessList.contains(ProcessID));
			m_ProcessList.insert(ProcessID, pProcess);
		}

		bool bChanged = false;
		bChanged = pProcess->UpdateDynamicData(process, iLinuxStyleCPU == 1 ? sysTotalTimePerCPU : sysTotalTime, sysTotalCycleTime);

		pProcess->UpdateThreadData(process, iLinuxStyleCPU ? sysTotalTimePerCPU : sysTotalTime, sysTotalCycleTime); // a thread can ever only use one CPU to use linux style ylways

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

	m_CpuStatsDPCUsage = CpuStatsDPCUsage;

	return true;
}

void EnumChildWindows(HWND hwnd, QMap<quint64, QMultiMap<quint64, CWindowsAPI::SWndInfo> >& WindowMap, quint32& WndCount)
{
    HWND childWindow = NULL;
    ULONG i = 0;

    // We use FindWindowEx because EnumWindows doesn't return Metro app windows.
    // Set a reasonable limit to prevent infinite loops.
    while (i < 0x800 && (childWindow = FindWindowEx(hwnd, childWindow, NULL, NULL)))
    {
		WndCount++;

        ULONG processId;
        ULONG threadId;

        threadId = GetWindowThreadProcessId(childWindow, &processId);

		CWindowsAPI::SWndInfo WndInfo;
		WndInfo.hwnd = (quint64)childWindow;
		WndInfo.parent = (quint64)hwnd;
		WndInfo.processId = processId;
		WndInfo.threadId = threadId;

		WindowMap[processId].insertMulti(threadId, WndInfo);

		EnumChildWindows(childWindow, WindowMap, WndCount);

        i++;
    }
}

quint32 CWindowsAPI::EnumWindows()
{
	quint32 WndCount = 0;
	QMap<quint64, QMultiMap<quint64, SWndInfo> > WindowMap; // QMap<ProcessId, QMultiMap<ThreadId, HWND> > 

	// When enumerating windows in windows we have only very little ability to filter the results
	// so to avoind going throuhgh the full list over and over agian we just cache ist in a double multi map
	// this way we can asily look up all windows of a process or all all windows belinging to a given thread

	EnumChildWindows(GetDesktopWindow(), WindowMap, WndCount);

	QWriteLocker Locker(&m_WindowMutex);
	m_WindowMap = WindowMap;

	return WndCount;
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

    PPH_NETWORK_CONNECTION connections;
    ulong numberOfConnections;

    if (!NetworkImportDone)
    {
        WSADATA wsaData;
        // Make sure WSA is initialized. (wj32)
        WSAStartup(WINSOCK_VERSION, &wsaData);
        NetworkImportDone = TRUE;
    }

	if (!PhGetNetworkConnections(&connections, &numberOfConnections))
		return false;

	//QWriteLocker Locker(&m_SocketMutex);
	
	// Copy the socket map Map
	QMultiMap<quint64, CSocketPtr> OldSockets = GetSocketList();

	for (ulong i = 0; i < numberOfConnections; i++)
	{
		QSharedPointer<CWinSocket> pSocket;

		QHostAddress LocalAddress = CWinSocket::PH2QAddress(&connections[i].LocalEndpoint.Address);
		quint16 LocalPort = connections[i].LocalEndpoint.Port;
		QHostAddress RemoteAddress = CWinSocket::PH2QAddress(&connections[i].RemoteEndpoint.Address);
		quint16  RemotePort = connections[i].RemoteEndpoint.Port;

		bool bAdd = false;
		QMultiMap<quint64, CSocketPtr>::iterator I = FindSocketEntry(OldSockets, (quint64)connections[i].ProcessId, (quint64)connections[i].ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort, true);
		if (I == OldSockets.end())
		{
			pSocket = QSharedPointer<CWinSocket>(new CWinSocket());
			bAdd = pSocket->InitStaticData((quint64)connections[i].ProcessId, (quint64)connections[i].ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort);
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

	PhFree(connections);

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
								const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort)
{
    // Note: there is always the possibility of us receiving the event too early,
    // before the process item or network item is created. 

	AddNetworkIO(Type, TransferSize);

	// Hack to get samba stats - check for system using Samba port 445 + (NetBIOS 137, 138, and 139)
	// the propper way has been found
	/*if (ProcessId == (quint64)SYSTEM_PROCESS_ID)
	{
		if (RemotePort == 445)// || RemotePort == 137 || RemotePort == 138 || RemotePort == 139) // Samba client
		{
			QWriteLocker Locker(&m_StatsMutex);
			switch (Type)
			{
			case EtEtwNetworkReceiveType:	m_Stats.SambaClient.AddReceive(TransferSize); break;
			case EtEtwNetworkSendType:		m_Stats.SambaClient.AddSend(TransferSize); break;
			}
		}
		else if (LocalPort == 445)// || LocalPort == 137 || LocalPort == 138 || LocalPort == 139) // Samba server
		{
			QWriteLocker Locker(&m_StatsMutex);
			switch (Type)
			{
			case EtEtwNetworkReceiveType:	m_Stats.SambaServer.AddReceive(TransferSize); break;
			case EtEtwNetworkSendType:		m_Stats.SambaServer.AddSend(TransferSize); break;
			}
		}
	}*/

	QSharedPointer<CWinSocket> pSocket = FindSocket(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort, false).objectCast<CWinSocket>();
	bool bAdd = false;
	if (pSocket.isNull())
	{
		//qDebug() << ProcessId  << Type << LocalAddress.toString() << LocalPort << RemoteAddress.toString() << RemotePort;

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

	/*CHandlePtr FileHandle = m_HandleByObject.value(FileId); // does not work :-(
	if (!FileHandle.isNull())
		qDebug() << FileHandle->GetFileName() << FileName;*/

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

    }
}

QString CWindowsAPI::GetFileNameByID(quint64 FileId) const
{
	QReadLocker Locker(&m_FileNameMutex);
	return m_FileNames.value(FileId, tr("Unknown file name"));;
}

void CWindowsAPI::OnDiskEvent(int Type, quint64 FileId, quint64 ProcessId, quint64 ThreadId, ulong IrpFlags, ulong TransferSize, quint64 HighResResponseTime)
{
	QString FileName = GetFileNameByID(FileId);

	AddDiskIO(Type, TransferSize);

	QSharedPointer<CWinProcess> pProcess;
	if (ProcessId != -1)
		pProcess = GetProcessByID(ProcessId).objectCast<CWinProcess>();
	else
		pProcess = GetProcessByThreadID(ThreadId).objectCast<CWinProcess>();

	if (!pProcess.isNull())
		pProcess->AddDiskIO(Type, TransferSize);

	// todo: FileName ist often unknown and FileId doesn't seam to have relations to open handled
	//			we want to be able to show for eadch open file the i/o that must be doable somehow...

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

	// Note: we coudl do without the driver but we dont want to
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
					continue; // ToDo: whyt are there duplicate results in the handle list?!
				m_OpenFilesList.insert(handle->HandleValue, pWinHandle);
				//m_HandleByObject.insertMulti(pWinHandle->GetObject(), pWinHandle);
			}
		}
		// ToDo: do we want to start it right away and dont wait?
		//else if (pWinHandle->UpdateDynamicData(handle,(quint64)ProcessHandle))
		//	Changed.insert(handle->HandleValue);

		if (pWinHandle->m_pProcess.isNull())
		{
			CProcessPtr pProcess = GetProcessByID(handle->UniqueProcessId);
			if (!pProcess.isNull()) {
				pWinHandle->m_pProcess = pProcess;
				pWinHandle->m_ProcessName = pProcess->GetName();
			}
			else
				pWinHandle->m_ProcessName = tr("Unknown Process");
		}
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

bool CWindowsAPI::UpdateServiceList()
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

		QString Name = QString::fromWCharArray(service->lpServiceName);

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

		bChanged = pService->UpdateDynamicData(&scManagerHandle, service);

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

	return m_CoreCount > 0 && m_CpuCount > 0;
}