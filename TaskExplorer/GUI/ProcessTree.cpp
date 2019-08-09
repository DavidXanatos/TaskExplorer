#include "stdafx.h"
#include "TaskExplorer.h"
#include "ProcessTree.h"
#include "../Common/Common.h"
#include "Models/ProcessModel.h"
#include "../Common/SortFilterProxyModel.h"
#ifdef WIN32
#include "../API/Windows/WinProcess.h"
#include "../API/Windows/WindowsAPI.h"
#include "../API/Windows/ProcessHacker.h"
#endif
#include "../API/MemDumper.h"
#include "../Common/ProgressDialog.h"
#include "TaskInfo/TaskInfoWindow.h"
#include "RunAsDialog.h"
#include "../Common/Finder.h"

CProcessTree::CProcessTree(QWidget *parent)
	: CTaskView(parent)
{
	m_ExpandAll = false;

	this->ForceColumn(CProcessModel::eProcess);

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pProcessModel = new CProcessModel();
	//connect(m_pProcessModel, SIGNAL(CheckChanged(quint64, bool)), this, SLOT(OnCheckChanged(quint64, bool)));
	//connect(m_pProcessModel, SIGNAL(Updated()), this, SLOT(OnUpdated()));

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pProcessModel);
	m_pSortProxy->setDynamicSortFilter(true);


	m_pProcessList = new CSplitTreeView(m_pSortProxy);
	
	connect(m_pProcessList, SIGNAL(MenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(m_pProcessList, SIGNAL(TreeEnabled(bool)), this, SLOT(OnTreeEnabled(bool)));

	m_pProcessList->GetView()->header()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pProcessList->GetView()->header(), SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnHeaderMenu(const QPoint &)));

	m_pHeaderMenu = new QMenu(this);

	m_pProcessList->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	m_pProcessList->GetTree()->setItemDelegate(theGUI->GetItemDelegate());

	connect(theGUI, SIGNAL(ReloadAll()), m_pProcessModel, SLOT(Clear()));

	m_pMainLayout->addWidget(m_pProcessList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

#ifndef _DEBUG
	connect(m_pProcessList->GetView()->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(OnUpdateHistory()));
	connect(m_pProcessList->GetView(), SIGNAL(expanded(const QModelIndex &)), this, SLOT(OnUpdateHistory()));
#endif


	//m_pMenu = new QMenu();
	AddTaskItemsToMenu();
	//QAction*				m_pTerminateTree;
	m_pMenu->addSeparator();

	m_pOpenPath = m_pMenu->addAction(tr("Open Path"), this, SLOT(OnProcessAction()));

	m_pCreateDump = m_pMenu->addAction(tr("Create Crash Dump"), this, SLOT(OnCrashDump()));
	m_pDebug = m_pMenu->addAction(tr("Debug"), this, SLOT(OnProcessAction()));
	m_pDebug->setCheckable(true);
#ifdef WIN32
	m_pVirtualization = m_pMenu->addAction(tr("Virtualization"), this, SLOT(OnProcessAction()));
	m_pVirtualization->setCheckable(true);
	//QAction*				m_pWindows;
	//QAction*				m_pGDI_Handles;
	m_pCritical = m_pMenu->addAction(tr("Critical"), this, SLOT(OnProcessAction()));
	m_pCritical->setCheckable(true);
	m_pReduceWS = m_pMenu->addAction(tr("Reduce Working Set"), this, SLOT(OnProcessAction()));
	//QAction*				m_pUnloadModules;
	//QAction*				m_pWatchWS;
#endif
	m_pRunAsThis = m_pMenu->addAction(tr("Run as this User"), this, SLOT(OnRunAsThis()));

	AddPriorityItemsToMenu(eProcess, m_pMenu);
	AddPanelItemsToMenu(m_pMenu);



	//connect(m_pProcessList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	connect(m_pProcessList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));
	connect(m_pProcessList, SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));


	connect(theAPI, SIGNAL(ProcessListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), SLOT(OnProcessListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

	QByteArray Columns = theConf->GetBlob("MainWindow/ProcessTree_Columns");
	if (Columns.isEmpty())
	{
		for (int i = 1; i < m_pProcessModel->columnCount(); i++)
			m_pProcessList->GetView()->setColumnHidden(i, true);

		m_pProcessList->GetView()->setColumnHidden(CProcessModel::ePID, false);
		m_pProcessList->GetView()->setColumnHidden(CProcessModel::eCPU, false);
		m_pProcessList->GetView()->setColumnHidden(CProcessModel::eIO_TotalRate, false);
		m_pProcessList->GetView()->setColumnHidden(CProcessModel::eStaus, false);
		m_pProcessList->GetView()->setColumnHidden(CProcessModel::eWorkingSet, false); // thats what best represents the InMemory value
		m_pProcessList->GetView()->setColumnHidden(CProcessModel::eStartTime, false); // less refreshes as uptime 
#ifdef WIN32
		m_pProcessList->GetView()->setColumnHidden(CProcessModel::eServices, false);
#endif
		m_pProcessList->GetView()->setColumnHidden(CProcessModel::ePriorityClass, false);
		m_pProcessList->GetView()->setColumnHidden(CProcessModel::eHandles, false);
#ifdef WIN32
		m_pProcessList->GetView()->setColumnHidden(CProcessModel::eWND_Handles, false);
		m_pProcessList->GetView()->setColumnHidden(CProcessModel::eGDI_Handles, false);
		m_pProcessList->GetView()->setColumnHidden(CProcessModel::eUSER_Handles, false);
#endif
		m_pProcessList->GetView()->setColumnHidden(CProcessModel::eThreads, false);
		m_pProcessList->GetView()->setColumnHidden(CProcessModel::eCommandLine, false);
	}
	else
		m_pProcessList->restoreState(Columns);
	m_pProcessModel->SetColumnEnabled(0, true);
	for (int i = 1; i < m_pProcessModel->columnCount(); i++)
		m_pProcessModel->SetColumnEnabled(i, !m_pProcessList->GetView()->isColumnHidden(i));
}

CProcessTree::~CProcessTree()
{
	theConf->SetBlob("MainWindow/ProcessTree_Columns", m_pProcessList->saveState());
}

void CProcessTree::OnTreeEnabled(bool bEnable)
{
	if (m_pProcessModel->IsTree() != bEnable)
	{
		m_pProcessModel->SetTree(bEnable);

		if (bEnable)
			m_ExpandAll = true;
	}
}

void CProcessTree::AddHeaderSubMenu(QMenu* m_pHeaderMenu, const QString& Label, int from, int to)
{
	QMenu* pSubMenu = m_pHeaderMenu->addMenu(Label);

	for(int i = from; i <= to; i++)
	{
		QCheckBox *checkBox = new QCheckBox(m_pProcessModel->GetColumHeader(i), pSubMenu);
		connect(checkBox, SIGNAL(stateChanged(int)), this, SLOT(OnHeaderMenu()));
		QWidgetAction *pAction = new QWidgetAction(pSubMenu);
		pAction->setDefaultWidget(checkBox);
		pSubMenu->addAction(pAction);

		m_Columns[checkBox] = i;
	}
}

void CProcessTree::OnHeaderMenu(const QPoint &point)
{
	if(m_Columns.isEmpty())
	{
		//for(int i = 1; i < m_pProcessModel->MaxColumns(); i++)
		for(int i = CProcessModel::ePID; i <= CProcessModel::eCommandLine; i++)
		{
			QCheckBox *checkBox = new QCheckBox(m_pProcessModel->GetColumHeader(i), m_pHeaderMenu);
			connect(checkBox, SIGNAL(stateChanged(int)), this, SLOT(OnHeaderMenu()));
			QWidgetAction *pAction = new QWidgetAction(m_pHeaderMenu);
			pAction->setDefaultWidget(checkBox);
			m_pHeaderMenu->addAction(pAction);

			m_Columns[checkBox] = i;
		}

		m_pHeaderMenu->addSeparator();
		AddHeaderSubMenu(m_pHeaderMenu, tr("Graphs"), CProcessModel::eCPU_History, CProcessModel::eVMEM_History);
		m_pHeaderMenu->addSeparator();
		AddHeaderSubMenu(m_pHeaderMenu, tr("CPU"), CProcessModel::eCPU, CProcessModel::eCyclesDelta);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Memory"), CProcessModel::ePrivateBytes, CProcessModel::ePrivateBytesDelta);
		AddHeaderSubMenu(m_pHeaderMenu, tr("GPU"), CProcessModel::eGPU_Usage, CProcessModel::eGPU_Adapter);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Objects"), CProcessModel::eHandles, CProcessModel::eThreads);
		AddHeaderSubMenu(m_pHeaderMenu, tr("File Info"), CProcessModel::eFileName, CProcessModel::eFileSize);
#ifdef WIN32
		AddHeaderSubMenu(m_pHeaderMenu, tr("Protection"), CProcessModel::eIntegrity, CProcessModel::eCritical);
#endif
		AddHeaderSubMenu(m_pHeaderMenu, tr("Other"), CProcessModel::eSubsystem, CProcessModel::eSessionID);
		AddHeaderSubMenu(m_pHeaderMenu, tr("File I/O"), CProcessModel::eIO_TotalRate, CProcessModel::eIO_OtherRate);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Network I/O"), CProcessModel::eNet_TotalRate, CProcessModel::eSendRate);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Disk I/O"), CProcessModel::eDisk_TotalRate, CProcessModel::eWriteRate);
	}

	for(QMap<QCheckBox*, int>::iterator I = m_Columns.begin(); I != m_Columns.end(); I++)
		I.key()->setChecked(m_pProcessModel->IsColumnEnabled(I.value()));

	m_pHeaderMenu->popup(QCursor::pos());	
}

void CProcessTree::OnHeaderMenu()
{
	QCheckBox *checkBox = (QCheckBox*)sender();
	int Column = m_Columns.value(checkBox, -1);

	m_pProcessList->GetView()->setColumnHidden(Column, !checkBox->isChecked());
	m_pProcessModel->SetColumnEnabled(Column, checkBox->isChecked());
}

void CProcessTree::OnProcessListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	m_Processes = theAPI->GetProcessList();

	if (!theGUI->isVisible())
		return;

	m_pProcessModel->Sync(m_Processes);

	// If we are dsplaying a tree than always auto expand new items
	if (m_pProcessModel->IsTree())
	{
		QTimer::singleShot(100, this, [this, Added]() {
			if (m_ExpandAll)
			{
				m_ExpandAll = false;
				m_pProcessList->expandAll();
			}
			else
			{
				foreach(quint64 PID, Added) {
					m_pProcessList->expand(m_pSortProxy->mapFromSource(m_pProcessModel->FindIndex(PID)));
				}
			}
		});
	}

	OnUpdateHistory();
}

void CProcessTree::OnDoubleClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);

	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow(pProcess);
	pTaskInfoWindow->show();
}

void CProcessTree::OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);

	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	emit ProcessClicked(pProcess);
}

QList<CTaskPtr> CProcessTree::GetSellectedTasks()
{
	QList<CTaskPtr> List;
	foreach(const QModelIndex& Index, m_pProcessList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
		if(!pProcess.isNull())
			List.append(pProcess);
	}
	return List;
}

void CProcessTree::OnMenu(const QPoint &point)
{
	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	
	m_pCreateDump->setEnabled(m_pProcessList->selectedRows().count() == 1);
	m_pDebug->setEnabled(m_pProcessList->selectedRows().count() == 1);
	m_pDebug->setChecked(pProcess && pProcess->HasDebugger());

#ifdef WIN32
	QSharedPointer<CWinProcess> pWinProcess = pProcess.objectCast<CWinProcess>();
	CWinTokenPtr pToken = pWinProcess ? pWinProcess->GetToken() : NULL;

	m_pVirtualization->setEnabled(pToken && pToken->IsVirtualizationAllowed());
	m_pVirtualization->setChecked(pToken && pToken->IsVirtualizationEnabled());

	m_pCritical->setEnabled(!pWinProcess.isNull());
	m_pCritical->setChecked(pWinProcess && pWinProcess->IsCriticalProcess());

	m_pReduceWS->setEnabled(!pWinProcess.isNull());
#endif
	m_pRunAsThis->setEnabled(m_pProcessList->selectedRows().count() == 1);

	CTaskView::OnMenu(point);
}

void CProcessTree::OnCrashDump()
{
	QString DumpPath = QFileDialog::getSaveFileName(this, tr("Create dump"), "", tr("Dump files (*.dmp);;All files (*.*)"));
	if (DumpPath.isEmpty())
		return;

	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);

	CMemDumper* pDumper = CMemDumper::New();
	STATUS status = pDumper->PrepareDump(pProcess, DumpPath);
	if(status.IsError())
		QMessageBox::warning(this, "TaskExplorer", tr("Failed to create dump file, reason: %1").arg(status.GetText()));
	else
	{
		CProgressDialog* pDialog = new CProgressDialog(tr("Dumping %1").arg(pProcess->GetName()), this);
		pDialog->show();

		connect(pDumper, SIGNAL(ProgressMessage(const QString&, int)), pDialog, SLOT(OnProgressMessage(const QString&, int)));
		connect(pDumper, SIGNAL(StatusMessage(const QString&, int)), pDialog, SLOT(OnStatusMessage(const QString&, int)));
		connect(pDialog, SIGNAL(Cancel()), pDumper, SLOT(Cancel()));
		connect(pDumper, SIGNAL(finished()), pDialog, SLOT(OnFinished()));

		connect(pDumper, SIGNAL(finished()), pDumper, SLOT(deleteLater()));

		pDumper->start();
	}
}

void CProcessTree::OnProcessAction()
{
#ifdef WIN32
	QList<STATUS> Errors;
	int Force = -1;
	foreach(const QModelIndex& Index, m_pProcessList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
		QSharedPointer<CWinProcess> pWinProcess = pProcess.objectCast<CWinProcess>();
		if (!pWinProcess.isNull())
		{
		retry:
			STATUS Status = OK;
			if (sender() == m_pVirtualization)
			{
				CWinTokenPtr pToken = pWinProcess->GetToken();
				if (pToken)
					Status = pToken->SetVirtualizationEnabled(m_pVirtualization->isChecked());
				else
					Status = ERR(tr("This process does not have a token."), -1);
			}
			else if (sender() == m_pCritical)
				Status = pWinProcess->SetCriticalProcess(m_pCritical->isChecked(), Force == 1);
			else if (sender() == m_pReduceWS)
				Status = pWinProcess->ReduceWS();
			else if (sender() == m_pDebug)
			{
				if (m_pDebug->isChecked())
					pWinProcess->AttachDebugger();
				else
					pWinProcess->DetachDebugger();
			}
			else if (sender() == m_pOpenPath)
			{
				PPH_STRING phFileName = CastQString(pWinProcess->GetFileName());
				PhShellExecuteUserString(NULL, L"FileBrowseExecutable", phFileName->Buffer, FALSE, L"Make sure the Explorer executable file is present." );
				PhDereferenceObject(phFileName);
			}
			

			if (Status.IsError())
			{
				if (Status.GetStatus() == ERROR_CONFIRM)
				{
					if (Force == -1)
					{
						switch (QMessageBox("TaskExplorer", Status.GetText(), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel | QMessageBox::Default | QMessageBox::Escape).exec())
						{
						case QMessageBox::Yes:
							Force = 1;
							goto retry;
							break;
						case QMessageBox::No:
							Force = 0;
							break;
						case QMessageBox::Cancel:
							return;
						}
					}
				}
				else 
					Errors.append(Status);
			}
		}
	}

	CTaskExplorer::CheckErrors(Errors);
#endif
}

void CProcessTree::OnRunAsThis()
{
	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);

	CRunAsDialog* pWnd = new CRunAsDialog(pProcess->GetProcessId());
	pWnd->show();
}

void CProcessTree::UpdateIndexWidget(int HistoryColumn, int CellHeight, QMap<quint64, CHistoryGraph*>& Graphs, QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> >& History)
{
	QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> > OldMap;
	m_pProcessList->StartUpdatingWidgets(OldMap, History);
	//for(QModelIndex Index = m_pProcessList->GetView()->indexAt(QPoint(0,0)); Index.isValid(); Index = m_pProcessList->GetView()->indexBelow(Index))
	for(QModelIndex Index = m_pProcessList->GetView()->indexAt(QPoint(0,0)); Index.isValid(); Index = m_pProcessList->GetView()->indexBelow(Index))
	{
		Index = Index.sibling(Index.row(), HistoryColumn);
		if(!m_pProcessList->GetView()->viewport()->rect().intersects(m_pProcessList->GetView()->visualRect(Index)))
			break; // out of view
		
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		quint64 PID = m_pProcessModel->Data(ModelIndex, Qt::UserRole, CProcessModel::eProcess).toULongLong();

		CHistoryWidget* pGraph = OldMap.take(PID).first;
		if (!pGraph)
		{
			QModelIndex Index = m_pSortProxy->mapFromSource(ModelIndex);

			pGraph = new CHistoryWidget(Graphs[PID]);
			pGraph->setFixedHeight(CellHeight);
			History.insert(PID, qMakePair((QPointer<CHistoryWidget>)pGraph, QPersistentModelIndex(Index)));
			m_pProcessList->GetView()->setIndexWidget(Index, pGraph);
		}
	}
	m_pProcessList->EndUpdatingWidgets(OldMap, History);
}

void CProcessTree::OnUpdateHistory()
{
	float Div = (theConf->GetInt("Options/LinuxStyleCPU") == 1) ? theAPI->GetCpuCount() : 1.0f;

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eCPU_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eCPU_History);

		QMap<quint64, CHistoryGraph*> Old = m_CPU_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetProcessId();
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, Qt::white, this);
				pGraph->AddValue(0, Qt::green);
				pGraph->AddValue(1, Qt::red);
				m_CPU_Graphs.insert(PID, pGraph);
			}

			STaskStats Stats = pProcess->GetCpuStats();

			pGraph->SetValue(0, Stats.CpuUsage / Div);
			pGraph->SetValue(1, Stats.CpuKernelUsage / Div);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys())
			m_CPU_Graphs.take(TID)->deleteLater();

		UpdateIndexWidget(CProcessModel::eCPU_History, CellHeight, m_CPU_Graphs, m_CPU_History);
	}

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eGPU_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eGPU_History);

		QMap<quint64, CHistoryGraph*> Old = m_GPU_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetProcessId();
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, Qt::white, this);
				pGraph->AddValue(0, Qt::green);
				m_GPU_Graphs.insert(PID, pGraph);
			}

			pGraph->SetValue(0, pProcess->GetGpuUsage());
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys())
			m_GPU_Graphs.take(TID)->deleteLater();

		UpdateIndexWidget(CProcessModel::eGPU_History, CellHeight, m_GPU_Graphs, m_GPU_History);
	}

	quint64 TotalMemoryUsed = theAPI->GetCommitedMemory();

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eMEM_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eMEM_History);

		QMap<quint64, CHistoryGraph*> Old = m_MEM_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetProcessId();
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, Qt::white, this);
				pGraph->AddValue(0, QColor("#CCFF33"));
				m_MEM_Graphs.insert(PID, pGraph);
			}

			pGraph->SetValue(0, TotalMemoryUsed ? (float)pProcess->GetWorkingSetSize() / TotalMemoryUsed : 0);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys())
			m_MEM_Graphs.take(TID)->deleteLater();

		UpdateIndexWidget(CProcessModel::eMEM_History, CellHeight, m_MEM_Graphs, m_MEM_History);
	}

#ifdef WIN32
	CGpuMonitor* pGpuMonitor = ((CWindowsAPI*)theAPI)->GetGpuMonitor();

	CGpuMonitor::SGpuMemory GpuMemory = pGpuMonitor->GetGpuMemory();
	quint64 TotalShared = GpuMemory.SharedLimit;
	quint64 TotalDedicated = GpuMemory.DedicatedLimit;
#else
    // todo linux
    quint64 TotalShared = 0;
    quint64 TotalDedicated = 0;
#endif

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eVMEM_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eVMEM_History);

		QMap<quint64, CHistoryGraph*> Old = m_VMEM_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetProcessId();
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, Qt::white, this);
				pGraph->AddValue(0, QColor("#CCFF33"));
				m_VMEM_Graphs.insert(PID, pGraph);
			}

			float DedicatedMemory = TotalDedicated ? (float)pProcess->GetGpuDedicatedUsage() / TotalDedicated : 0;
			float SharedMemory = TotalShared ? (float)pProcess->GetGpuSharedUsage() / TotalShared : 0;

			pGraph->SetValue(0, qMax(DedicatedMemory, SharedMemory));
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys())
			m_VMEM_Graphs.take(TID)->deleteLater();

		UpdateIndexWidget(CProcessModel::eVMEM_History, CellHeight, m_VMEM_Graphs, m_VMEM_History);
	}


	SSysStats SysStats = theAPI->GetStats();
	quint64 TotalIO = SysStats.Io.ReadRate.Get() + SysStats.Io.WriteRate.Get() + SysStats.Io.OtherRate.Get();
	quint64 TotalDisk = SysStats.Disk.ReadRate.Get() + SysStats.Disk.WriteRate.Get();
	if (TotalDisk < TotalIO)
		TotalDisk = TotalIO;

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eIO_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eIO_History);

		QMap<quint64, CHistoryGraph*> Old = m_IO_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetProcessId();
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, Qt::white, this);
				pGraph->AddValue(0, Qt::green);
				pGraph->AddValue(1, Qt::red);
				pGraph->AddValue(2, Qt::blue);
				m_IO_Graphs.insert(PID, pGraph);
			}

			SProcStats Stats = pProcess->GetStats();

			pGraph->SetValue(0, TotalDisk ? (float)qMax(Stats.Disk.ReadRate.Get(), Stats.Io.ReadRate.Get()) / TotalDisk : 0);
			pGraph->SetValue(1, TotalDisk ? (float)qMax(Stats.Disk.WriteRate.Get(), Stats.Io.WriteRate.Get()) / TotalDisk : 0);
			pGraph->SetValue(2, TotalIO ? (float)Stats.Io.OtherRate.Get() / TotalIO : 0);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys())
			m_IO_Graphs.take(TID)->deleteLater();

		UpdateIndexWidget(CProcessModel::eIO_History, CellHeight, m_IO_Graphs, m_IO_History);
	}

	quint64 TotalNet = SysStats.Net.ReceiveRate.Get() + SysStats.Net.SendRate.Get();

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eNET_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eNET_History);

		QMap<quint64, CHistoryGraph*> Old = m_NET_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetProcessId();
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, Qt::white, this);
				pGraph->AddValue(0, Qt::green);
				pGraph->AddValue(1, Qt::red);
				m_NET_Graphs.insert(PID, pGraph);
			}

			SProcStats Stats = pProcess->GetStats();

			pGraph->SetValue(0, TotalNet ? (float)Stats.Net.ReceiveRate.Get() / TotalNet : 0);
			pGraph->SetValue(1, TotalNet ? (float)Stats.Net.SendRate.Get() / TotalNet : 0);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys())
			m_NET_Graphs.take(TID)->deleteLater();

		UpdateIndexWidget(CProcessModel::eNET_History, CellHeight, m_NET_Graphs, m_NET_History);
	}
}
