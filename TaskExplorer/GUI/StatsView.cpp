#include "stdafx.h"
#include "TaskExplorer.h"
#include "StatsView.h"
#include "../Common/Common.h"
#ifdef WIN32
#include "../API/Windows/WinProcess.h"
#include "../API/Windows/WindowsAPI.h"
#endif


CStatsView::CStatsView(EView eView, QWidget *parent)
	:CPanelView(parent)
{
	m_eView = eView;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	// Stack List
	m_pStatsList = new QTreeWidgetEx();
	m_pStatsList->setItemDelegate(theGUI->GetItemDelegate());
	m_pStatsList->setHeaderLabels(tr("Name|Count|Size|Rate|Delta|Peak").split("|"));

	m_pStatsList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pStatsList->setSortingEnabled(false);

	m_pStatsList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pStatsList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	m_pMainLayout->addWidget(CFinder::AddFinder(m_pStatsList, this, true));
	// 

	m_MonitorsETW = false;

	SetupTree();

	if (/*eView == eProcess ||*/ eView == eSystem)
		m_pStatsList->setAutoFitMax(800 * theGUI->GetDpiScale());
	else
		m_pStatsList->setMinimumHeight(50 * theGUI->GetDpiScale());

	//m_pMenu = new QMenu();
	AddPanelItemsToMenu();

	setObjectName(parent->parent() ? parent->parent()->objectName() : "InfoWindow");
	m_pStatsList->header()->restoreState(theConf->GetBlob(objectName() + "/StatsView_Columns"));
}

CStatsView::~CStatsView()
{
	theConf->SetBlob(objectName() + "/StatsView_Columns", m_pStatsList->header()->saveState());
}

void CStatsView::SetupTree()
{
	m_pStatsList->clear();

	//data
	m_pCPU = new QTreeWidgetItem(tr("CPU").split("|"));
	m_pStatsList->addTopLevelItem(m_pCPU);
	if (m_eView == eProcess)
	{
		m_pCycles = new QTreeWidgetItem(tr("Cycles").split("|"));
		m_pCPU->addChild(m_pCycles);
	}
	if (m_eView == eProcess
#ifdef WIN32
		|| m_eView == eJob
#endif
	 ) {
		m_pKernelTime = new QTreeWidgetItem(tr("Kernel time").split("|"));
		m_pCPU->addChild(m_pKernelTime);
		m_pUserTime = new QTreeWidgetItem(tr("User time").split("|"));
		m_pCPU->addChild(m_pUserTime);
	}
	if (m_eView == eProcess) 
	{
		m_pTotalTime = new QTreeWidgetItem(tr("Total time").split("|"));
		m_pCPU->addChild(m_pTotalTime);
	}
	if (m_eView == eProcess || m_eView == eSystem)
	{
		m_pContextSwitches = new QTreeWidgetItem(tr("Context switches").split("|"));
		m_pCPU->addChild(m_pContextSwitches);
	}
	if (m_eView == eSystem)
	{
		m_pInterrupts = new QTreeWidgetItem(tr("Interrupts").split("|"));
		m_pCPU->addChild(m_pInterrupts);
		m_pDPCs = new QTreeWidgetItem(tr("DPCs").split("|"));
		m_pCPU->addChild(m_pDPCs);
		m_pSysCalls = new QTreeWidgetItem(tr("System calls").split("|"));
		m_pCPU->addChild(m_pSysCalls);
	}

	m_pMemory = new QTreeWidgetItem(tr("Memory").split("|"));
	m_pStatsList->addTopLevelItem(m_pMemory);
	/*if (m_eView == eSystem)
	{
		m_pCommitCharge = new QTreeWidgetItem(tr("Commit Charge").split("|"));
		m_pMemory->addChild(m_pCommitCharge);
		m_pVirtualSize = new QTreeWidgetItem(tr("Paged pool virtual size").split("|"));
		m_pMemory->addChild(m_pVirtualSize);
		m_pWorkingSet = new QTreeWidgetItem(tr("Paged pool working set").split("|"));
		m_pMemory->addChild(m_pWorkingSet);
		m_pNonPagedPool = new QTreeWidgetItem(tr("Non-Paged pool usage").split("|"));
		m_pMemory->addChild(m_pNonPagedPool);
	}*/
	if (m_eView == eProcess) 
	{
		m_pPrivateBytes = new QTreeWidgetItem(tr("Private bytes").split("|"));
		m_pMemory->addChild(m_pPrivateBytes);
		m_pVirtualSize = new QTreeWidgetItem(tr("Virtual size").split("|"));
		m_pMemory->addChild(m_pVirtualSize);
		m_pWorkingSet = new QTreeWidgetItem(tr("Working set").split("|"));
		m_pMemory->addChild(m_pWorkingSet);
	}
	m_pPageFaults = new QTreeWidgetItem(tr("Page faults").split("|"));
	m_pMemory->addChild(m_pPageFaults);
	if (m_eView == eProcess) 
	{
		m_pHardFaults = new QTreeWidgetItem(tr("Hard faults").split("|"));
		m_pMemory->addChild(m_pHardFaults);
		m_pPrivateWS = new QTreeWidgetItem(tr("Private working set").split("|"));
		m_pMemory->addChild(m_pPrivateWS);
		m_pShareableWS = new QTreeWidgetItem(tr("Shareable working set").split("|"));
		m_pMemory->addChild(m_pShareableWS);
		m_pSharedWS = new QTreeWidgetItem(tr("Shared working set").split("|"));
		m_pMemory->addChild(m_pSharedWS);
#ifdef WIN32
		m_pPagedPool = new QTreeWidgetItem(tr("Paged pool usage").split("|"));
		m_pMemory->addChild(m_pPagedPool);
		m_pNonPagedPool = new QTreeWidgetItem(tr("Non-Paged pool usage").split("|"));
		m_pMemory->addChild(m_pNonPagedPool);
#endif
	}
#ifdef WIN32
	else if(m_eView == eJob)
	{
		m_pPrivateWS = new QTreeWidgetItem(tr("Peak process usage").split("|"));
		m_pMemory->addChild(m_pPrivateWS);
		m_pSharedWS = new QTreeWidgetItem(tr("Peak job usage").split("|"));
		m_pMemory->addChild(m_pSharedWS);
	}
#endif

	m_pIO = new QTreeWidgetItem(tr("I/O").split("|"));
	m_pStatsList->addTopLevelItem(m_pIO);
	m_pIOReads = new QTreeWidgetItem(tr("File I/O reads").split("|"));
	m_pIO->addChild(m_pIOReads);
	m_pIOWrites = new QTreeWidgetItem(tr("File I/O writes").split("|"));
	m_pIO->addChild(m_pIOWrites);
	m_pIOOther = new QTreeWidgetItem(tr("Other I/O").split("|"));
	m_pIO->addChild(m_pIOOther);
	if (m_eView == eSystem)
	{
		m_pMMapIOReads = new QTreeWidgetItem(tr("Mapped I/O reads").split("|"));
		m_pIO->addChild(m_pMMapIOReads);
		m_pMMapIOWrites = new QTreeWidgetItem(tr("Mapped I/O writes").split("|"));
		m_pIO->addChild(m_pMMapIOWrites);
	}
	if (m_eView == eProcess || m_eView == eSystem)
	{
		m_pDiskReads = new QTreeWidgetItem(tr("Disk reads").split("|"));
		m_pIO->addChild(m_pDiskReads);
		m_pDiskWrites = new QTreeWidgetItem(tr("Disk writes").split("|"));
		m_pIO->addChild(m_pDiskWrites);
		m_pNetSends = new QTreeWidgetItem(tr("Network Sends").split("|"));
		m_pIO->addChild(m_pNetSends);
		m_pNetReceives = new QTreeWidgetItem(tr("Network Receive").split("|"));
		m_pIO->addChild(m_pNetReceives);
	}

	m_pOther = new QTreeWidgetItem(tr("Other").split("|"));
	m_pStatsList->addTopLevelItem(m_pOther);

	if (m_eView == eSystem)
	{
		m_pUpTime = new QTreeWidgetItem(tr("Up time").split("|"));
		m_pOther->addChild(m_pUpTime);
	}
	if (m_eView == eProcess)
	{
		m_pUpTime = new QTreeWidgetItem(tr("Start time").split("|"));
		m_pOther->addChild(m_pUpTime);
	}

	if (m_eView == eSystem
#ifdef WIN32
		|| m_eView == eJob
#endif
	 ) {
		m_pProcesses = new QTreeWidgetItem(tr("Processes").split("|"));
		m_pOther->addChild(m_pProcesses);
	}
	if (m_eView == eProcess || m_eView == eSystem)
	{
		m_pThreads = new QTreeWidgetItem(tr("Threads").split("|"));
		m_pOther->addChild(m_pThreads);
		m_pHandles = new QTreeWidgetItem(tr("Handles").split("|"));
		m_pOther->addChild(m_pHandles);
		m_pGdiObjects = new QTreeWidgetItem(tr("Gdi objects").split("|"));
		m_pOther->addChild(m_pGdiObjects);
		m_pUserObjects = new QTreeWidgetItem(tr("User objects").split("|"));
		m_pOther->addChild(m_pUserObjects);
		m_pWndObjects = new QTreeWidgetItem(tr("Windows").split("|"));
		m_pOther->addChild(m_pWndObjects);
	}
#ifdef WIN32
	if (m_eView == eProcess)
	{
		m_pRunningTime = new QTreeWidgetItem(tr("Running time").split("|"));
		m_pOther->addChild(m_pRunningTime);
		m_pSuspendTime = new QTreeWidgetItem(tr("Suspended time").split("|"));
		m_pOther->addChild(m_pSuspendTime);
		m_pHangCount = new QTreeWidgetItem(tr("Hang count").split("|"));
		m_pOther->addChild(m_pHangCount);
		m_pGhostCount = new QTreeWidgetItem(tr("Ghost count").split("|"));
		m_pOther->addChild(m_pGhostCount);
	}
#endif

	m_pStatsList->expandAll();
	//
}

void CStatsView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
#ifdef WIN32
	if (m_MonitorsETW != ((CWindowsAPI*)theAPI)->IsMonitoringETW())
	{
		m_MonitorsETW = ((CWindowsAPI*)theAPI)->IsMonitoringETW();

		SetupTree();
	}
#endif

	if (Processes.isEmpty())
		return;

	enum EFormat
	{
		eUndefined = 0,
		eFormatSize,
		eFormatRate,
		eFormatTime,
		eFormatNumber
	};

	struct SAccStat
	{
		struct SColStat
		{
			SColStat(): Format(eUndefined), Value(0) { }

			EFormat Format;
			quint64 Value;

			void AddSumm(EFormat format, quint64 value)
			{
				Format = format;
				Value += value;
			}
		};

		void AddSumm(int column, EFormat format, quint64 value)
		{
			Columns[column].AddSumm(format, value);
		}

		QMap<int, SColStat> Columns;
	};

	QMap<QTreeWidgetItem*, SAccStat> AccStats;

	foreach(const CProcessPtr& pProcess, Processes)
	{
		STaskStatsEx CpuStats = pProcess->GetCpuStats();

		// CPU
		AccStats[m_pCycles].AddSumm(eCount, eFormatNumber, CpuStats.CycleDelta.Value);
		AccStats[m_pCycles].AddSumm(eDelta, eFormatNumber, CpuStats.CycleDelta.Delta);

		AccStats[m_pKernelTime].AddSumm(eCount, eFormatTime, CpuStats.CpuKernelDelta.Value / CPU_TIME_DIVIDER);
		AccStats[m_pKernelTime].AddSumm(eDelta, eFormatTime, CpuStats.CpuKernelDelta.Delta / CPU_TIME_DIVIDER);

		AccStats[m_pUserTime].AddSumm(eCount, eFormatTime, CpuStats.CpuUserDelta.Value / CPU_TIME_DIVIDER);
		AccStats[m_pUserTime].AddSumm(eDelta, eFormatTime, CpuStats.CpuUserDelta.Delta / CPU_TIME_DIVIDER);

		AccStats[m_pTotalTime].AddSumm(eCount, eFormatTime, (CpuStats.CpuKernelDelta.Value + CpuStats.CpuUserDelta.Value) / CPU_TIME_DIVIDER);
		AccStats[m_pTotalTime].AddSumm(eDelta, eFormatTime, (CpuStats.CpuKernelDelta.Delta + CpuStats.CpuUserDelta.Delta) / CPU_TIME_DIVIDER);

		AccStats[m_pContextSwitches].AddSumm(eCount, eFormatNumber, CpuStats.ContextSwitchesDelta.Value);
		AccStats[m_pContextSwitches].AddSumm(eDelta, eFormatNumber, CpuStats.ContextSwitchesDelta.Delta);

		// Memory
		AccStats[m_pPrivateBytes].AddSumm(eSize, eFormatSize, CpuStats.PrivateBytesDelta.Value);
		AccStats[m_pPrivateBytes].AddSumm(eDelta, eFormatSize, CpuStats.PrivateBytesDelta.Delta);

		AccStats[m_pVirtualSize].AddSumm(eSize, eFormatSize, pProcess->GetVirtualSize());
		AccStats[m_pVirtualSize].AddSumm(ePeak, eFormatSize, pProcess->GetPeakVirtualSize());

		AccStats[m_pPageFaults].AddSumm(eCount, eFormatNumber, CpuStats.PageFaultsDelta.Value);
		AccStats[m_pPageFaults].AddSumm(eDelta, eFormatNumber, CpuStats.PageFaultsDelta.Delta);

		AccStats[m_pHardFaults].AddSumm(eCount, eFormatNumber, CpuStats.HardFaultsDelta.Value);
		AccStats[m_pHardFaults].AddSumm(eDelta, eFormatNumber, CpuStats.HardFaultsDelta.Delta);

		AccStats[m_pWorkingSet].AddSumm(eSize, eFormatSize, pProcess->GetWorkingSetSize());

		AccStats[m_pPrivateWS].AddSumm(eSize, eFormatSize, pProcess->GetPrivateWorkingSetSize());

		AccStats[m_pShareableWS].AddSumm(eSize, eFormatSize, pProcess->GetShareableWorkingSetSize());
		AccStats[m_pSharedWS].AddSumm(eSize, eFormatSize, pProcess->GetSharedWorkingSetSize());

		// IO
		SProcStats Stats = pProcess->GetStats();

		AccStats[m_pIOReads].AddSumm(eCount, eFormatNumber, Stats.Io.ReadDelta.Value);
		AccStats[m_pIOReads].AddSumm(eSize, eFormatSize, Stats.Io.ReadRawDelta.Value);
		AccStats[m_pIOReads].AddSumm(eRate, eFormatRate, Stats.Io.ReadRate.Get());
		AccStats[m_pIOReads].AddSumm(eDelta, eFormatNumber, Stats.Io.ReadDelta.Delta);

		AccStats[m_pIOWrites].AddSumm(eCount, eFormatNumber, Stats.Io.WriteDelta.Value);
		AccStats[m_pIOWrites].AddSumm(eSize, eFormatSize, Stats.Io.WriteRawDelta.Value);
		AccStats[m_pIOWrites].AddSumm(eRate, eFormatRate, Stats.Io.WriteRate.Get());
		AccStats[m_pIOWrites].AddSumm(eDelta, eFormatNumber, Stats.Io.WriteDelta.Delta);

		AccStats[m_pIOOther].AddSumm(eCount, eFormatNumber, Stats.Io.OtherDelta.Value);
		AccStats[m_pIOOther].AddSumm(eSize, eFormatSize, Stats.Io.OtherRawDelta.Value);
		AccStats[m_pIOOther].AddSumm(eRate, eFormatRate, Stats.Io.OtherRate.Get());
		AccStats[m_pIOOther].AddSumm(eDelta, eFormatNumber, Stats.Io.OtherDelta.Delta);

		AccStats[m_pDiskReads].AddSumm(eCount, eFormatNumber, Stats.Disk.ReadDelta.Value);
		AccStats[m_pDiskReads].AddSumm(eSize, eFormatSize, Stats.Disk.ReadRawDelta.Value);
		AccStats[m_pDiskReads].AddSumm(eRate, eFormatRate, Stats.Disk.ReadRate.Get());
		AccStats[m_pDiskReads].AddSumm(eDelta, eFormatNumber, Stats.Disk.ReadDelta.Delta);

		AccStats[m_pDiskWrites].AddSumm(eCount, eFormatNumber, Stats.Disk.WriteDelta.Value);
		AccStats[m_pDiskWrites].AddSumm(eSize, eFormatSize, Stats.Disk.WriteRawDelta.Value);
		AccStats[m_pDiskWrites].AddSumm(eRate, eFormatRate, Stats.Disk.WriteRate.Get());
		AccStats[m_pDiskWrites].AddSumm(eDelta, eFormatNumber, Stats.Disk.WriteDelta.Delta);

		if (m_MonitorsETW)
		{
			AccStats[m_pNetSends].AddSumm(eCount, eFormatNumber, Stats.Net.SendDelta.Value);
			AccStats[m_pNetSends].AddSumm(eSize, eFormatSize, Stats.Net.SendRawDelta.Value);
			AccStats[m_pNetSends].AddSumm(eRate, eFormatRate, Stats.Net.SendRate.Get());
			AccStats[m_pNetSends].AddSumm(eDelta, eFormatNumber, Stats.Net.SendDelta.Delta);

			AccStats[m_pNetReceives].AddSumm(eCount, eFormatNumber, Stats.Net.ReceiveDelta.Value);
			AccStats[m_pNetReceives].AddSumm(eSize, eFormatSize, Stats.Net.ReceiveRawDelta.Value);
			AccStats[m_pNetReceives].AddSumm(eRate, eFormatRate, Stats.Net.ReceiveRate.Get());
			AccStats[m_pNetReceives].AddSumm(eDelta, eFormatNumber, Stats.Net.ReceiveDelta.Delta);
		}

		// other
		AccStats[m_pThreads].AddSumm(eCount, eFormatNumber, pProcess->GetNumberOfThreads());
		AccStats[m_pThreads].AddSumm(ePeak, eFormatNumber, pProcess->GetPeakNumberOfThreads());
		AccStats[m_pHandles].AddSumm(eCount, eFormatNumber, pProcess->GetNumberOfHandles());
		AccStats[m_pHandles].AddSumm(ePeak, eFormatNumber, pProcess->GetPeakNumberOfHandles());

#ifdef WIN32
		CWinProcess* pWinProc = qobject_cast<CWinProcess*>(pProcess.data());

		AccStats[m_pPagedPool].AddSumm(eSize, eFormatSize, pWinProc->GetPagedPool());
		AccStats[m_pPagedPool].AddSumm(ePeak, eFormatSize, pWinProc->GetPeakPagedPool());

		AccStats[m_pNonPagedPool].AddSumm(eSize, eFormatSize, pWinProc->GetNonPagedPool());
		AccStats[m_pNonPagedPool].AddSumm(ePeak, eFormatSize, pWinProc->GetPeakNonPagedPool());

		AccStats[m_pGdiObjects].AddSumm(eCount, eFormatNumber, pWinProc->GetGdiHandles());

		AccStats[m_pUserObjects].AddSumm(eCount, eFormatNumber, pWinProc->GetUserHandles());

		AccStats[m_pWndObjects].AddSumm(eCount, eFormatNumber, pWinProc->GetWndHandles());


		AccStats[m_pRunningTime].AddSumm(eCount, eFormatTime, pWinProc->GetUpTime());
		AccStats[m_pSuspendTime].AddSumm(eCount, eFormatTime, pWinProc->GetSuspendTime());
		AccStats[m_pHangCount].AddSumm(eCount, eFormatNumber, pWinProc->GetHangCount());
		AccStats[m_pGhostCount].AddSumm(eCount, eFormatNumber, pWinProc->GetGhostCount());
#endif
	}
	
	for (QMap<QTreeWidgetItem*, SAccStat>::iterator I = AccStats.begin(); I != AccStats.end(); I++)
	{
		for (QMap<int, SAccStat::SColStat>::iterator J = I->Columns.begin(); J != I->Columns.end(); J++)
		switch (J->Format)
		{
		case eFormatSize:	I.key()->setText(J.key(), FormatSize(J->Value)); break;
		case eFormatRate:	I.key()->setText(J.key(), FormatRate(J->Value)); break;
		case eFormatTime:	I.key()->setText(J.key(), FormatTime(J->Value)); break;
		case eFormatNumber:	I.key()->setText(J.key(), FormatNumber(J->Value)); break;
		}
	}

	if (Processes.count() == 1)
	{
		CProcessPtr pProcess = Processes.first();

		m_pUpTime->setText(eCount, QDateTime::fromTime_t(pProcess->GetCreateTimeStamp() / 1000).toString("dd.MM.yyyy hh:mm:ss"));
	}
	else
	{
		m_pUpTime->setText(eCount, "");
	}
}

void CStatsView::ShowIoStats(const SProcStats& Stats)
{
	m_pIOReads->setText(eCount, FormatNumber(Stats.Io.ReadDelta.Value));
	m_pIOReads->setText(eSize, FormatSize(Stats.Io.ReadRawDelta.Value));
	m_pIOReads->setText(eRate, FormatRate(Stats.Io.ReadRate.Get()));
	m_pIOReads->setText(eDelta, FormatNumber(Stats.Io.ReadDelta.Delta));

	m_pIOWrites->setText(eCount, FormatNumber(Stats.Io.WriteDelta.Value));
	m_pIOWrites->setText(eSize, FormatSize(Stats.Io.WriteRawDelta.Value));
	m_pIOWrites->setText(eRate, FormatRate(Stats.Io.WriteRate.Get()));
	m_pIOWrites->setText(eDelta, FormatNumber(Stats.Io.WriteDelta.Delta));

	m_pIOOther->setText(eCount, FormatNumber(Stats.Io.OtherDelta.Value));
	m_pIOOther->setText(eSize, FormatSize(Stats.Io.OtherRawDelta.Value));
	m_pIOOther->setText(eRate, FormatRate(Stats.Io.OtherRate.Get()));
	m_pIOOther->setText(eDelta, FormatNumber(Stats.Io.OtherDelta.Delta));

	m_pDiskReads->setText(eCount, FormatNumber(Stats.Disk.ReadDelta.Value));
	m_pDiskReads->setText(eSize, FormatSize(Stats.Disk.ReadRawDelta.Value));
	m_pDiskReads->setText(eRate, FormatRate(Stats.Disk.ReadRate.Get()));
	m_pDiskReads->setText(eDelta, FormatNumber(Stats.Disk.ReadDelta.Delta));

	m_pDiskWrites->setText(eCount, FormatNumber(Stats.Disk.WriteDelta.Value));
	m_pDiskWrites->setText(eSize, FormatSize(Stats.Disk.WriteRawDelta.Value));
	m_pDiskWrites->setText(eRate, FormatRate(Stats.Disk.WriteRate.Get()));
	m_pDiskWrites->setText(eDelta, FormatNumber(Stats.Disk.WriteDelta.Delta));

	if (m_MonitorsETW)
	{
		m_pNetSends->setText(eCount, FormatNumber(Stats.Net.SendDelta.Value));
		m_pNetSends->setText(eSize, FormatSize(Stats.Net.SendRawDelta.Value));
		m_pNetSends->setText(eRate, FormatRate(Stats.Net.SendRate.Get()));
		m_pNetSends->setText(eDelta, FormatNumber(Stats.Net.SendDelta.Delta));

		m_pNetReceives->setText(eCount, FormatNumber(Stats.Net.ReceiveDelta.Value));
		m_pNetReceives->setText(eSize, FormatSize(Stats.Net.ReceiveRawDelta.Value));
		m_pNetReceives->setText(eRate, FormatRate(Stats.Net.ReceiveRate.Get()));
		m_pNetReceives->setText(eDelta, FormatNumber(Stats.Net.ReceiveDelta.Delta));
	}
}

void CStatsView::ShowSystem()
{
	SCpuStatsEx CpuStats = theAPI->GetCpuStats();

	// CPU
	m_pContextSwitches->setText(eCount, FormatNumber(CpuStats.ContextSwitchesDelta.Value));
	m_pContextSwitches->setText(eDelta, FormatNumber(CpuStats.ContextSwitchesDelta.Delta));

	m_pInterrupts->setText(eCount, FormatNumber(CpuStats.InterruptsDelta.Value));
	m_pInterrupts->setText(eDelta, FormatNumber(CpuStats.InterruptsDelta.Delta));

	m_pDPCs->setText(eCount, FormatNumber(CpuStats.DpcsDelta.Value));
	m_pDPCs->setText(eDelta, FormatNumber(CpuStats.DpcsDelta.Delta));

	m_pSysCalls->setText(eCount, FormatNumber(CpuStats.SystemCallsDelta.Value));
	m_pSysCalls->setText(eDelta, FormatNumber(CpuStats.SystemCallsDelta.Delta));


	// Memory
	/*m_pCommitCharge->setText(eSize, FormatSize(theAPI->GetCommitedMemory()));
	m_pCommitCharge->setText(ePeak, FormatSize(theAPI->GetCommitedMemoryPeak()));
	//m_pCommitCharge->setText(eLimit, FormatSize(theAPI->GetMemoryLimit()));

	m_pVirtualSize->setText(eSize, FormatSize(theAPI->GetPagedPool()));

	m_pWorkingSet->setText(eSize, FormatSize(theAPI->GetPersistentPagedPool()));

	m_pNonPagedPool->setText(eSize, FormatSize(theAPI->GetNonPagedPool()));*/

	m_pPageFaults->setText(eCount, FormatNumber(CpuStats.PageFaultsDelta.Value));
	m_pPageFaults->setText(eDelta, FormatNumber(CpuStats.PageFaultsDelta.Delta));

	// IO
	SSysStats SysStats = theAPI->GetStats();
	ShowIoStats(SysStats);

	m_pMMapIOReads->setText(eCount, FormatNumber(SysStats.MMapIo.ReadDelta.Value));
	m_pMMapIOReads->setText(eSize, FormatSize(SysStats.MMapIo.ReadRawDelta.Value));
	m_pMMapIOReads->setText(eRate, FormatRate(SysStats.MMapIo.ReadRate.Get()));
	m_pMMapIOReads->setText(eDelta, FormatNumber(SysStats.MMapIo.ReadDelta.Delta));

	m_pMMapIOWrites->setText(eCount, FormatNumber(SysStats.MMapIo.WriteDelta.Value));
	m_pMMapIOWrites->setText(eSize, FormatSize(SysStats.MMapIo.WriteRawDelta.Value));
	m_pMMapIOWrites->setText(eRate, FormatRate(SysStats.MMapIo.WriteRate.Get()));
	m_pMMapIOWrites->setText(eDelta, FormatNumber(SysStats.MMapIo.WriteDelta.Delta));

	// other
	m_pUpTime->setText(eCount, FormatTime(theAPI->GetUpTime()));

	m_pProcesses->setText(eCount, FormatNumber(theAPI->GetTotalProcesses()));

	m_pThreads->setText(eCount, FormatNumber(theAPI->GetTotalThreads()));

	m_pHandles->setText(eCount, FormatNumber(theAPI->GetTotalHandles()));
	//m_pHandles->setText(ePeak, FormatNumber(pProcess->GetpeakNumberOfHandles()));

#ifdef WIN32
	m_pGdiObjects->setText(eCount, FormatNumber(((CWindowsAPI*)theAPI)->GetTotalGuiObjects()));

	m_pUserObjects->setText(eCount, FormatNumber(((CWindowsAPI*)theAPI)->GetTotalUserObjects()));

	m_pWndObjects->setText(eCount, FormatNumber(((CWindowsAPI*)theAPI)->GetTotalWndObjects()));
#endif
}

#ifdef WIN32
void CStatsView::ShowJob(const CWinJobPtr& pCurJob)
{
	m_pProcesses->setText(eCount, FormatNumber(pCurJob->GetActiveProcesses()));
	m_pProcesses->setText(ePeak, FormatNumber(pCurJob->GetTotalProcesses())); // not quite peak but close enough ;)
	// GetTerminatedProcesses()
	
	SJobStats Stats = pCurJob->GetStats();

	// CPU
	m_pKernelTime->setText(eCount, FormatTime(Stats.KernelDelta.Value/CPU_TIME_DIVIDER));
	m_pKernelTime->setText(eDelta, FormatTime(Stats.KernelDelta.Delta/CPU_TIME_DIVIDER));

	m_pUserTime->setText(eCount, FormatTime(Stats.UserDelta.Value/CPU_TIME_DIVIDER));
	m_pUserTime->setText(eDelta, FormatTime(Stats.UserDelta.Delta/CPU_TIME_DIVIDER));

	// memory
	m_pPageFaults->setText(eCount, FormatNumber(Stats.PageFaultsDelta.Value));
	m_pPageFaults->setText(eDelta, FormatNumber(Stats.PageFaultsDelta.Delta));

	m_pPrivateWS->setText(eSize, FormatSize(pCurJob->GetPeakProcessMemoryUsed()));

	m_pSharedWS->setText(eSize, FormatSize(pCurJob->GetPeakJobMemoryUsed()));

	// IO
	m_pIOReads->setText(eCount, FormatNumber(Stats.Io.ReadDelta.Value));
	m_pIOReads->setText(eSize, FormatSize(Stats.Io.ReadRawDelta.Value));
	m_pIOReads->setText(eRate, FormatRate(Stats.Io.ReadRate.Get()));
	m_pIOReads->setText(eDelta, FormatNumber(Stats.Io.ReadDelta.Delta));

	m_pIOWrites->setText(eCount, FormatNumber(Stats.Io.WriteDelta.Value));
	m_pIOWrites->setText(eSize, FormatSize(Stats.Io.WriteRawDelta.Value));
	m_pIOWrites->setText(eRate, FormatRate(Stats.Io.WriteRate.Get()));
	m_pIOWrites->setText(eDelta, FormatNumber(Stats.Io.WriteDelta.Delta));

	m_pIOOther->setText(eCount, FormatNumber(Stats.Io.OtherDelta.Value));
	m_pIOOther->setText(eSize, FormatSize(Stats.Io.OtherRawDelta.Value));
	m_pIOOther->setText(eRate, FormatRate(Stats.Io.OtherRate.Get()));
	m_pIOOther->setText(eDelta, FormatNumber(Stats.Io.OtherDelta.Delta));
}
#endif

void CStatsView::SetFilter(const QRegExp& Exp, bool bHighLight, int Col)
{
	CPanelWidgetEx::ApplyFilter(m_pStatsList, Exp/*, bHighLight, Col*/);
}
