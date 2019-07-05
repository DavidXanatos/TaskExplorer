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
	m_pStatsList->setItemDelegate(new CStyledGridItemDelegate(m_pStatsList->fontMetrics().height() + 3, this));
	m_pStatsList->setHeaderLabels(tr("Name|Count|Size|Rate|Delta|Peak|Limit").split("|"));

	m_pStatsList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pStatsList->setSortingEnabled(false);

	m_pStatsList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pStatsList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	m_pMainLayout->addWidget(m_pStatsList);
	// 

	m_MonitorsETW = false;

	SetupTree();

	if (eView == eProcess || eView == eSystem)
		m_pStatsList->setMinimumHeight(450);

	//m_pMenu = new QMenu();
	AddPanelItemsToMenu(false);

	setObjectName(parent->parent()->objectName());
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
		m_pContextSwitches = new QTreeWidgetItem(tr("Context switches").split("|"));
		m_pCPU->addChild(m_pContextSwitches);
	}

	m_pMemory = new QTreeWidgetItem(tr("Memory").split("|"));
	m_pStatsList->addTopLevelItem(m_pMemory);
	if (m_eView == eSystem)
	{
		m_pCommitCharge = new QTreeWidgetItem(tr("Commit Charge").split("|"));
		m_pMemory->addChild(m_pCommitCharge);
		m_pVirtualSize = new QTreeWidgetItem(tr("Paged virtusl size").split("|"));
		m_pMemory->addChild(m_pVirtualSize);
		m_pWorkingSet = new QTreeWidgetItem(tr("Paged working set").split("|"));
		m_pMemory->addChild(m_pWorkingSet);
	}
	if (m_eView == eProcess) 
	{
		m_pPrivateBytes = new QTreeWidgetItem(tr("Private bytes").split("|"));
		m_pMemory->addChild(m_pPrivateBytes);
		m_pVirtualSize = new QTreeWidgetItem(tr("Virtusl size").split("|"));
		m_pMemory->addChild(m_pVirtualSize);
		m_pWorkingSet = new QTreeWidgetItem(tr("Working set").split("|"));
		m_pMemory->addChild(m_pWorkingSet);
	}
	m_pPageFaults = new QTreeWidgetItem(tr("Page faults").split("|"));
	m_pMemory->addChild(m_pPageFaults);
	if (m_eView == eProcess) 
	{
		m_pPrivateWS = new QTreeWidgetItem(tr("Private working set").split("|"));
		m_pMemory->addChild(m_pPrivateWS);
		m_pSharedWS = new QTreeWidgetItem(tr("Shared working set").split("|"));
		m_pMemory->addChild(m_pSharedWS);
		m_pPagedPool = new QTreeWidgetItem(tr("Paged pool").split("|"));
		m_pMemory->addChild(m_pPagedPool);
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
	if (m_eView == eProcess || m_eView == eSystem)
	{
		m_pNonPagedPool = new QTreeWidgetItem(tr("Non-Paged pool").split("|"));
		m_pMemory->addChild(m_pNonPagedPool);
	}

	m_pIO = new QTreeWidgetItem(tr("I/O").split("|"));
	m_pStatsList->addTopLevelItem(m_pIO);
	m_pIOReads = new QTreeWidgetItem(tr("I/O reads").split("|"));
	m_pIO->addChild(m_pIOReads);
	m_pIOWrites = new QTreeWidgetItem(tr("I/O writes").split("|"));
	m_pIO->addChild(m_pIOWrites);
	m_pIOOther = new QTreeWidgetItem(tr("Other I/O").split("|"));
	m_pIO->addChild(m_pIOOther);
	if (m_eView == eSystem)
	{
		m_pMMapIOReads = new QTreeWidgetItem(tr("Mem. I/O reads").split("|"));
		m_pIO->addChild(m_pMMapIOReads);
		m_pMMapIOWrites = new QTreeWidgetItem(tr("Mem. I/O writes").split("|"));
		m_pIO->addChild(m_pMMapIOWrites);
	}
	if (m_eView == eProcess || m_eView == eSystem)
	{
		if (m_MonitorsETW)
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
	}

	m_pOther = new QTreeWidgetItem(tr("Other").split("|"));
	m_pStatsList->addTopLevelItem(m_pOther);
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


	m_pStatsList->expandAll();
	//
}

#define CPU_TIME_DIVIDER (10 * 1000 * 1000) // the clock resolution is 100ns we need 1sec

void CStatsView::ShowProcess(const CProcessPtr& pProcess)
{
	if (m_MonitorsETW != ((CWindowsAPI*)theAPI)->IsMonitoringETW())
	{
		m_MonitorsETW = ((CWindowsAPI*)theAPI)->IsMonitoringETW();

		SetupTree();
	}

	STaskStatsEx CpuStats = pProcess->GetCpuStats();

	// CPU
	m_pCycles->setText(eCount, FormatUnit(CpuStats.CycleDelta.Value, 2));
	m_pCycles->setText(eDelta, FormatUnit(CpuStats.CycleDelta.Delta, 2));

	m_pKernelTime->setText(eCount, FormatTime(CpuStats.CpuKernelDelta.Value/CPU_TIME_DIVIDER));
	m_pKernelTime->setText(eDelta, FormatTime(CpuStats.CpuKernelDelta.Delta/CPU_TIME_DIVIDER));

	m_pUserTime->setText(eCount, FormatTime(CpuStats.CpuUserDelta.Value/CPU_TIME_DIVIDER));
	m_pUserTime->setText(eDelta, FormatTime(CpuStats.CpuUserDelta.Delta/CPU_TIME_DIVIDER));

	m_pTotalTime->setText(eCount, FormatTime((CpuStats.CpuKernelDelta.Value + CpuStats.CpuUserDelta.Value)/CPU_TIME_DIVIDER));
	m_pTotalTime->setText(eDelta, FormatTime((CpuStats.CpuKernelDelta.Delta + CpuStats.CpuUserDelta.Delta)/CPU_TIME_DIVIDER));

	m_pContextSwitches->setText(eSize, QString::number(CpuStats.ContextSwitchesDelta.Value));
	m_pContextSwitches->setText(eDelta, QString::number(CpuStats.ContextSwitchesDelta.Delta));

	// Memory
	m_pPrivateBytes->setText(eSize, FormatSize(CpuStats.PrivateBytesDelta.Value));
	m_pPrivateBytes->setText(eDelta, FormatSize(CpuStats.PrivateBytesDelta.Delta));

	m_pVirtualSize->setText(eSize, FormatSize(pProcess->GetVirtualSize()));
	m_pVirtualSize->setText(ePeak, FormatSize(pProcess->GetPeakVirtualSize()));
	
	m_pPageFaults->setText(eCount, QString::number(CpuStats.PageFaultsDelta.Value));
	m_pPageFaults->setText(eDelta, QString::number(CpuStats.PageFaultsDelta.Delta));

	m_pWorkingSet->setText(eSize, FormatSize(pProcess->GetWorkingSetSize()));

	m_pPrivateWS->setText(eSize, FormatSize(pProcess->GetPrivateWorkingSetSize()));

	m_pSharedWS->setText(eSize, FormatSize(pProcess->GetShareableWorkingSetSize()));
	
	m_pPagedPool->setText(eSize, FormatSize(pProcess->GetPagedPool()));
	m_pPagedPool->setText(ePeak, FormatSize(pProcess->GetPeakPagedPool()));

	m_pNonPagedPool->setText(eSize, FormatSize(pProcess->GetNonPagedPool()));
	m_pNonPagedPool->setText(ePeak, FormatSize(pProcess->GetPeakNonPagedPool()));

	// IO
	SProcStats Stats = pProcess->GetStats();
	ShowIoStats(Stats);

	// other
	m_pThreads->setText(eCount, QString::number(pProcess->GetNumberOfThreads()));
	m_pHandles->setText(eCount, QString::number(pProcess->GetNumberOfHandles()));
	//m_pHandles->setText(ePeak, QString::number(pProcess->GetpeakNumberOfHandles()));

#ifdef WIN32
	CWinProcess* pWinProc = qobject_cast<CWinProcess*>(pProcess.data());

	m_pGdiObjects->setText(eCount, QString::number(pWinProc->GetGdiHandles()));

	m_pUserObjects->setText(eCount, QString::number(pWinProc->GetUserHandles()));

	m_pWndObjects->setText(eCount, QString::number(pWinProc->GetWndHandles()));
#endif

	// todo: add uptime stats?
}

void CStatsView::ShowIoStats(const SProcStats& Stats)
{
	m_pIOReads->setText(eCount, QString::number(Stats.Io.ReadDelta.Value));
	m_pIOReads->setText(eSize, FormatSize(Stats.Io.ReadRawDelta.Value));
	m_pIOReads->setText(eRate, FormatSize(Stats.Io.ReadRate.Get()) + "/s");
	m_pIOReads->setText(eDelta, QString::number(Stats.Io.ReadDelta.Delta));

	m_pIOWrites->setText(eCount, QString::number(Stats.Io.WriteDelta.Value));
	m_pIOWrites->setText(eSize, FormatSize(Stats.Io.WriteRawDelta.Value));
	m_pIOWrites->setText(eRate, FormatSize(Stats.Io.WriteRate.Get()) + "/s");
	m_pIOWrites->setText(eDelta, QString::number(Stats.Io.WriteDelta.Delta));

	m_pIOOther->setText(eCount, QString::number(Stats.Io.OtherDelta.Value));
	m_pIOOther->setText(eSize, FormatSize(Stats.Io.OtherRawDelta.Value));
	m_pIOOther->setText(eRate, FormatSize(Stats.Io.OtherRate.Get()) + "/s");
	m_pIOOther->setText(eDelta, QString::number(Stats.Io.OtherDelta.Delta));

	if (m_MonitorsETW)
	{
		m_pDiskReads->setText(eCount, QString::number(Stats.Disk.ReadDelta.Value));
		m_pDiskReads->setText(eSize, FormatSize(Stats.Disk.ReadRawDelta.Value));
		m_pDiskReads->setText(eRate, FormatSize(Stats.Disk.ReadRate.Get()) + "/s");
		m_pDiskReads->setText(eDelta, QString::number(Stats.Disk.ReadDelta.Delta));

		m_pDiskWrites->setText(eCount, QString::number(Stats.Disk.WriteDelta.Value));
		m_pDiskWrites->setText(eSize, FormatSize(Stats.Disk.WriteRawDelta.Value));
		m_pDiskWrites->setText(eRate, FormatSize(Stats.Disk.WriteRate.Get()) + "/s");
		m_pDiskWrites->setText(eDelta, QString::number(Stats.Disk.WriteDelta.Delta));

		m_pNetSends->setText(eCount, QString::number(Stats.Net.SendDelta.Value));
		m_pNetSends->setText(eSize, FormatSize(Stats.Net.SendRawDelta.Value));
		m_pNetSends->setText(eRate, FormatSize(Stats.Net.SendRate.Get()) + "/s");
		m_pNetSends->setText(eDelta, QString::number(Stats.Net.SendDelta.Delta));

		m_pNetReceives->setText(eCount, QString::number(Stats.Net.ReceiveDelta.Value));
		m_pNetReceives->setText(eSize, FormatSize(Stats.Net.ReceiveRawDelta.Value));
		m_pNetReceives->setText(eRate, FormatSize(Stats.Net.ReceiveRate.Get()) + "/s");
		m_pNetReceives->setText(eDelta, QString::number(Stats.Net.ReceiveDelta.Delta));
	}
}

void CStatsView::ShowSystem()
{
	SCpuStats CpuStats = theAPI->GetCpuStats();

	// CPU


	// Memory
	m_pCommitCharge->setText(eSize, FormatSize(theAPI->GetCommitedMemory()));
	m_pCommitCharge->setText(ePeak, FormatSize(theAPI->GetCommitedMemoryPeak()));
	m_pCommitCharge->setText(eLimit, FormatSize(theAPI->GetMemoryLimit()));

	m_pVirtualSize->setText(eSize, FormatSize(theAPI->GetPagedMemory()));

	m_pWorkingSet->setText(eSize, FormatSize(theAPI->GetPersistentPagedMemory()));

	m_pPageFaults->setText(eCount, QString::number(CpuStats.PageFaultsDelta.Value));
	m_pPageFaults->setText(eDelta, QString::number(CpuStats.PageFaultsDelta.Delta));

	m_pNonPagedPool->setText(eSize, FormatSize(theAPI->GetNonPagedMemory()));

	// IO
	SSysStats Stats = theAPI->GetStats();
	ShowIoStats(Stats);

	m_pMMapIOReads->setText(eCount, QString::number(Stats.MMapIo.ReadDelta.Value));
	m_pMMapIOReads->setText(eSize, FormatSize(Stats.MMapIo.ReadRawDelta.Value));
	m_pMMapIOReads->setText(eRate, FormatSize(Stats.MMapIo.ReadRate.Get()) + "/s");
	m_pMMapIOReads->setText(eDelta, QString::number(Stats.MMapIo.ReadDelta.Delta));

	m_pMMapIOWrites->setText(eCount, QString::number(Stats.MMapIo.WriteDelta.Value));
	m_pMMapIOWrites->setText(eSize, FormatSize(Stats.MMapIo.WriteRawDelta.Value));
	m_pMMapIOWrites->setText(eRate, FormatSize(Stats.MMapIo.WriteRate.Get()) + "/s");
	m_pMMapIOWrites->setText(eDelta, QString::number(Stats.MMapIo.WriteDelta.Delta));

	// other
	m_pProcesses->setText(eCount, QString::number(theAPI->GetTotalProcesses()));

	m_pThreads->setText(eCount, QString::number(theAPI->GetTotalThreads()));

	m_pHandles->setText(eCount, QString::number(theAPI->GetTotalHandles()));
	//m_pHandles->setText(ePeak, FormatUnit(pProcess->GetpeakNumberOfHandles()));

#ifdef WIN32
	m_pGdiObjects->setText(eCount, QString::number(((CWindowsAPI*)theAPI)->GetTotalGuiObjects()));

	m_pUserObjects->setText(eCount, QString::number(((CWindowsAPI*)theAPI)->GetTotalUserObjects()));

	m_pWndObjects->setText(eCount, QString::number(((CWindowsAPI*)theAPI)->GetTotalWndObjects()));
#endif
}

#ifdef WIN32
void CStatsView::ShowJob(const CWinJobPtr& pCurJob)
{
	m_pProcesses->setText(eCount, QString::number(pCurJob->GetActiveProcesses()));
	m_pProcesses->setText(ePeak, QString::number(pCurJob->GetTotalProcesses())); // not quite peak but close enough ;)
	// GetTerminatedProcesses()
	
	SJobStats Stats = pCurJob->GetStats();

	// CPU
	m_pKernelTime->setText(eCount, FormatTime(Stats.KernelDelta.Value/CPU_TIME_DIVIDER));
	m_pKernelTime->setText(eDelta, FormatTime(Stats.KernelDelta.Delta/CPU_TIME_DIVIDER));

	m_pUserTime->setText(eCount, FormatTime(Stats.UserDelta.Value/CPU_TIME_DIVIDER));
	m_pUserTime->setText(eDelta, FormatTime(Stats.UserDelta.Delta/CPU_TIME_DIVIDER));

	// memory
	m_pPageFaults->setText(eCount, QString::number(Stats.PageFaultsDelta.Value));
	m_pPageFaults->setText(eDelta, QString::number(Stats.PageFaultsDelta.Delta));

	m_pPrivateWS->setText(eSize, FormatSize(pCurJob->GetPeakProcessMemoryUsed()));

	m_pSharedWS->setText(eSize, FormatSize(pCurJob->GetPeakJobMemoryUsed()));

	// IO
	m_pIOReads->setText(eCount, QString::number(Stats.Io.ReadDelta.Value));
	m_pIOReads->setText(eSize, FormatSize(Stats.Io.ReadRawDelta.Value));
	m_pIOReads->setText(eRate, FormatSize(Stats.Io.ReadRate.Get()) + "/s");
	m_pIOReads->setText(eDelta, QString::number(Stats.Io.ReadDelta.Delta));

	m_pIOWrites->setText(eCount, QString::number(Stats.Io.WriteDelta.Value));
	m_pIOWrites->setText(eSize, FormatSize(Stats.Io.WriteRawDelta.Value));
	m_pIOWrites->setText(eRate, FormatSize(Stats.Io.WriteRate.Get()) + "/s");
	m_pIOWrites->setText(eDelta, QString::number(Stats.Io.WriteDelta.Delta));

	m_pIOOther->setText(eCount, QString::number(Stats.Io.OtherDelta.Value));
	m_pIOOther->setText(eSize, FormatSize(Stats.Io.OtherRawDelta.Value));
	m_pIOOther->setText(eRate, FormatSize(Stats.Io.OtherRate.Get()) + "/s");
	m_pIOOther->setText(eDelta, QString::number(Stats.Io.OtherDelta.Delta));
}
#endif
