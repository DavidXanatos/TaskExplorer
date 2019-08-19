#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ThreadsView.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinThread.h"
#include "../../API/Windows/WindowsAPI.h"
#endif
#include "../../Common/Finder.h"

CThreadsView::CThreadsView(QWidget *parent)
	: CTaskView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	//m_pFilterWidget = new QWidget();
	//m_pFilterWidget->setMinimumHeight(32);
	//m_pMainLayout->addWidget(m_pFilterWidget);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);
	m_pMainLayout->addWidget(m_pSplitter);

	// Thread List
	m_pThreadModel = new CThreadModel();
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pThreadModel);
	m_pSortProxy->setDynamicSortFilter(true);

	m_pThreadList = new QTreeViewEx();
	m_pThreadList->setItemDelegate(theGUI->GetItemDelegate());

	m_pThreadList->setModel(m_pSortProxy);

	m_pThreadList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pThreadList->setSortingEnabled(true);

	m_pSplitter->addWidget(CFinder::AddFinder(m_pThreadList, m_pSortProxy));
	m_pSplitter->setCollapsible(0, false);

	m_pThreadList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pThreadList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(theGUI, SIGNAL(ReloadAll()), m_pThreadModel, SLOT(Clear()));

	//connect(m_pThreadList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	connect(m_pThreadList->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));
	// 


	// Stack List
	m_pStackView = new CStackView(this);
	
	m_pSplitter->addWidget(m_pStackView);
	m_pSplitter->setCollapsible(1, false);
	// 

	m_CurStackTraceJob = 0;

	//m_pMenu = new QMenu();
	AddTaskItemsToMenu();

#ifdef WIN32
	m_pMiscMenu = m_pMenu->addMenu(tr("Miscellaneous"));
	m_pCancelIO = m_pMiscMenu->addAction(tr("Cancel I/O"), this, SLOT(OnThreadAction()));
	//m_pAnalyze;
	m_pCritical = m_pMiscMenu->addAction(tr("Critical"), this, SLOT(OnThreadAction()));
	m_pCritical->setCheckable(true);	
#endif

	AddPriorityItemsToMenu(eThread);

#ifdef WIN32
	m_pMenu->addSeparator();

	//m_pToken;
	m_pPermissions = m_pMenu->addAction(tr("Permissions"), this, SLOT(OnPermissions()));
#endif

	AddPanelItemsToMenu();


	setObjectName(parent->objectName());
	QByteArray Columns = theConf->GetBlob(objectName() + "/ThreadView_Columns");
	if (Columns.isEmpty())
	{
		for (int i = 0; i < m_pThreadModel->columnCount(); i++)
			m_pThreadList->setColumnHidden(i, true);

		m_pThreadList->setColumnHidden(CThreadModel::eThread, false);
		m_pThreadList->setColumnHidden(CThreadModel::eCPU, false);
		m_pThreadList->setColumnHidden(CThreadModel::eCPU_History, false);
#ifdef WIN32
		m_pThreadList->setColumnHidden(CThreadModel::eStartAddress, false);
#endif
		m_pThreadList->setColumnHidden(CThreadModel::ePriority, false);
#ifdef WIN32
		m_pThreadList->setColumnHidden(CThreadModel::eService, false);
#endif
		m_pThreadList->setColumnHidden(CThreadModel::eState, false);
#ifdef WIN32
		m_pThreadList->setColumnHidden(CThreadModel::eType, false);
#endif
		m_pThreadList->setColumnHidden(CThreadModel::eCreated, false);
	}
	else
		m_pThreadList->header()->restoreState(Columns);
}


CThreadsView::~CThreadsView()
{
	theConf->SetBlob(objectName() + "/ThreadView_Columns", m_pThreadList->header()->saveState());
}

void CThreadsView::ShowProcess(const CProcessPtr& pProcess)
{
	if (m_pCurProcess != pProcess)
	{
		disconnect(this, SLOT(ShowThreads(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

		m_pCurProcess = pProcess;

		connect(m_pCurProcess.data(), SIGNAL(ThreadsUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(ShowThreads(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
	}

	Refresh();
}

void CThreadsView::SellectThread(quint64 ThreadId)
{
	QModelIndex Index = m_pThreadModel->FindIndex(ThreadId);
	QModelIndex ModelIndex = m_pSortProxy->mapFromSource(Index);
		
	QModelIndex ModelL = m_pSortProxy->index(ModelIndex.row(), 0, ModelIndex.parent());
	QModelIndex ModelR = m_pSortProxy->index(ModelIndex.row(), m_pSortProxy->columnCount()-1, ModelIndex.parent());
	
	QItemSelection SelectedItems;
	SelectedItems.append(QItemSelectionRange(ModelL, ModelR));

	m_pThreadList->selectionModel()->select(SelectedItems, QItemSelectionModel::ClearAndSelect);
}

void CThreadsView::Refresh()
{
	if (!m_pCurProcess)
		return;

	QTimer::singleShot(0, m_pCurProcess.data(), SLOT(UpdateThreads()));

#ifdef WIN32
	// Note: See coment in CWinProcess::UpdateThreads(), ... so windows we just call ShowThreads right away.
	ShowThreads(QSet<quint64>(), QSet<quint64>(), QSet<quint64>());
#endif

	if(!m_pCurThread.isNull() && m_CurStackTraceJob == 0)
		m_CurStackTraceJob = m_pCurThread->TraceStack();
}

void CThreadsView::ShowThreads(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	m_Threads = m_pCurProcess->GetThreadList();

	m_pThreadModel->Sync(m_Threads);

	OnUpdateHistory();
}

void CThreadsView::OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);
	m_pCurThread = m_pThreadModel->GetThread(ModelIndex);

	if (m_CurStackTraceJob)
	{
		qobject_cast<CWindowsAPI*>(theAPI)->GetSymbolProvider()->CancelJob(m_CurStackTraceJob);
		m_CurStackTraceJob = 0;
	}

	disconnect(m_pStackView, SLOT(ShowStack(const CStackTracePtr&)));

	//m_pStackView->Clear();
	m_pStackView->Invalidate();

	if (!m_pCurThread.isNull()) 
	{
		connect(m_pCurThread.data(), SIGNAL(StackTraced(const CStackTracePtr&)), this, SLOT(ShowStack(const CStackTracePtr&)));

		m_CurStackTraceJob = m_pCurThread->TraceStack();
	}
}

void CThreadsView::ShowStack(const CStackTracePtr& StackTrace)
{
	if (!m_pCurThread)
		return;
	if (StackTrace->GetProcessId() != m_pCurThread->GetProcessId())
		return;
	if (StackTrace->GetThreadId() != m_pCurThread->GetThreadId())
		return;

	m_CurStackTraceJob = 0;

	m_pStackView->ShowStack(StackTrace);
}

QList<CTaskPtr> CThreadsView::GetSellectedTasks()
{
	QList<CTaskPtr> List;
	foreach(const QModelIndex& Index, m_pThreadList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CThreadPtr pThread = m_pThreadModel->GetThread(ModelIndex);
		if(!pThread.isNull())
			List.append(pThread);
	}
	return List;
}

void CThreadsView::OnMenu(const QPoint &point)
{
	QModelIndex Index = m_pThreadList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CThreadPtr pThread = m_pThreadModel->GetThread(ModelIndex);

#ifdef WIN32
	QSharedPointer<CWinThread> pWinThread = pThread.objectCast<CWinThread>();

	m_pCancelIO->setEnabled(!pThread.isNull());

	m_pCritical->setEnabled(!pWinThread.isNull());
	m_pCritical->setChecked(pWinThread && pWinThread->IsCriticalThread());

	m_pPermissions->setEnabled(m_pThreadList->selectedRows().count() == 1);
#endif

	CTaskView::OnMenu(point);
}

void CThreadsView::OnThreadAction()
{
#ifdef WIN32
	if (sender() == m_pCancelIO)
	{
		if (QMessageBox("TaskExplorer", tr("Do you want to cancel I/O for the selected thread(s)?"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
			return;
	}

	QList<STATUS> Errors;
	int Force = -1;
	foreach(const QModelIndex& Index, m_pThreadList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CThreadPtr pThread = m_pThreadModel->GetThread(ModelIndex);
		QSharedPointer<CWinThread> pWinThread = pThread.objectCast<CWinThread>();
		if (!pWinThread.isNull())
		{
		retry:
			STATUS Status = OK;
			if (sender() == m_pCancelIO)
				Status = pWinThread->CancelIO();
			else if(sender() == m_pCritical)
				Status = pWinThread->SetCriticalThread(m_pCritical->isChecked(), Force == 1);

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

void CThreadsView::OnUpdateHistory()
{
	if (!m_pThreadList->isColumnHidden(CThreadModel::eCPU_History))
	{
		int HistoryColumn = CThreadModel::eCPU_History;
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pThreadList->columnWidth(HistoryColumn);

		QMap<quint64, CHistoryGraph*> Old = m_CPU_Graphs;
		foreach(const CThreadPtr& pThread, m_Threads)
		{
			quint64 TID = pThread->GetThreadId();
			CHistoryGraph* pGraph = Old.take(TID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, Qt::white, this);
				pGraph->AddValue(0, Qt::green);
				pGraph->AddValue(1, Qt::red);
				m_CPU_Graphs.insert(TID, pGraph);
			}

			STaskStats Stats = pThread->GetCpuStats();

			pGraph->SetValue(0, Stats.CpuUsage);
			pGraph->SetValue(1, Stats.CpuKernelUsage);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys())
			m_CPU_Graphs.take(TID)->deleteLater();


		QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> > OldMap;
		m_pThreadList->StartUpdatingWidgets(OldMap, m_CPU_History);
		for(QModelIndex Index = m_pThreadList->indexAt(QPoint(0,0)); Index.isValid(); Index = m_pThreadList->indexBelow(Index))
		{
			Index = Index.sibling(Index.row(), HistoryColumn);
			if(!m_pThreadList->viewport()->rect().intersects(m_pThreadList->visualRect(Index)))
				break; // out of view
		
			QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
			quint64 TID = m_pThreadModel->Data(ModelIndex, Qt::UserRole, CThreadModel::eThread).toULongLong();

			CHistoryWidget* pGraph = OldMap.take(TID).first;
			if (!pGraph)
			{
				QModelIndex Index = m_pSortProxy->mapFromSource(ModelIndex);

				pGraph = new CHistoryWidget(m_CPU_Graphs[TID]);
				pGraph->setFixedHeight(CellHeight);
				m_CPU_History.insert(TID, qMakePair((QPointer<CHistoryWidget>)pGraph, QPersistentModelIndex(Index)));
				m_pThreadList->setIndexWidget(Index, pGraph);
			}
		}
		m_pThreadList->EndUpdatingWidgets(OldMap, m_CPU_History);
	}
}

void CThreadsView::OnPermissions()
{
#ifdef WIN32
	QList<CTaskPtr>	Tasks = GetSellectedTasks();
	if (Tasks.count() != 1)
		return;

	if (QSharedPointer<CWinThread> pWinThread = Tasks.first().objectCast<CWinThread>())
	{
		pWinThread->OpenPermissions();
	}
#endif
}
