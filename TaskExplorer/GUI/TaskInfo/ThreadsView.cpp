#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ThreadsView.h"
#include "../../../MiscHelpers/Common/Common.h"
#include "../WaitChainDialog.h"
#include "../../../MiscHelpers/Common/Finder.h"
#include "TaskInfoWindow.h"
#include "TokenView.h"

CThreadsView::CThreadsView(QWidget *parent)
	: CTaskView(parent)
{
	//m_PendingUpdates = 0;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	//m_pFilterWidget = new QWidget();
	//m_pFilterWidget->setMinimumHeight(32);
	//m_pMainLayout->addWidget(m_pFilterWidget);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);
	m_pMainLayout->addWidget(m_pSplitter);

	// Thread List
	m_pThreadModel = new CThreadModel();
	
	m_pSortProxy = new CSortFilterProxyModel(this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pThreadModel);
	m_pSortProxy->setDynamicSortFilter(true);

	m_pThreadList = new QTreeViewEx();
	m_pThreadList->setItemDelegate(theGUI->GetItemDelegate());

	m_pThreadList->setModel(m_pSortProxy);

	m_pThreadList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pThreadList->setSortingEnabled(true);

	m_pThreadList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pThreadList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(theGUI, SIGNAL(ReloadPanels()), m_pThreadModel, SLOT(Clear()));

	//connect(m_pThreadList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	connect(m_pThreadList->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));

	m_pThreadList->setColumnReset(2);
	connect(m_pThreadList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	connect(m_pThreadList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	//
	// Replace QTreeViewEx's built-in flat column menu with a grouped one, like
	// the process view: the thread list carries a lot of columns and a single
	// flat list is unwieldy. The header already has a CustomContextMenu policy
	// and is wired to QTreeViewEx::OnMenuRequested in its constructor; drop that
	// wiring and point the header at our own grouped builder instead.
	//
	disconnect(m_pThreadList->header(), SIGNAL(customContextMenuRequested(const QPoint&)), m_pThreadList, 0);
	connect(m_pThreadList->header(), SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(OnHeaderMenu(const QPoint&)));
	m_pHeaderMenu = new QMenu(this);

	m_pSplitter->addWidget(CFinder::AddFinder(m_pThreadList, m_pSortProxy));
	m_pSplitter->setCollapsible(0, false);
	// 


	// Stack List
	m_pStackView = new CStackView(this);
	
	m_pSplitter->addWidget(m_pStackView);
	//m_pSplitter->setCollapsible(1, false);
	// 

	m_CurStackTraceJob = 0;
	m_pStackTraceSystem = NULL;

	//m_pMenu = new QMenu();
	AddTaskItemsToMenu();
	m_pMenu->addSeparator();

	m_pMiscMenu = m_pMenu->addMenu(tr("Miscellaneous"));
	m_pCancelIO = m_pMiscMenu->addAction(tr("Cancel I/O"), this, SLOT(OnThreadAction()));
	//m_pAnalyze;
	m_pWCT = m_pMiscMenu->addAction(tr("Wait Chain Traversal"), this, SLOT(OnWCT()));
	m_pMiscMenu->addSeparator();
	m_pCritical = m_pMiscMenu->addAction(tr("Critical Thread Flag"), this, SLOT(OnThreadAction()));
	m_pCritical->setCheckable(true);

	m_pToken = m_pMenu->addAction(tr("Impersonation Token"), this, SLOT(OnThreadToken()));
	m_pToken2 = m_pMenu->addAction(tr("Original Impersonation Token"), this, SLOT(OnThreadToken()));
	m_pPermissions = m_pMenu->addAction(tr("Permissions"), this, SLOT(OnPermissions()));
	m_pMenu->addSeparator();

	AddPriorityItemsToMenu(eThread);

	AddPanelItemsToMenu();

	m_ViewMode = eNone;
	setObjectName(parent->objectName());
	SwitchView(eSingle);
}

CThreadsView::~CThreadsView()
{
	SwitchView(eNone);
}

void CThreadsView::SwitchView(EView ViewMode)
{
	//
	// Saved header state is keyed by column index, so it only makes sense against
	// the EColumns order it was written with. Bump this whenever that enum is
	// reordered or entries are inserted anywhere but the end; a stale version is
	// ignored and the columns reset, rather than restoring a scrambled layout.
	//
	const int ColumnLayoutVersion = 2;

	switch (m_ViewMode)
	{
		case eSingle:	theConf->SetBlob(objectName() + "/ThreadView_Columns", m_pThreadList->saveState());
						theConf->SetValue(objectName() + "/ThreadView_ColumnsVersion", ColumnLayoutVersion); break;
		case eMulti:	theConf->SetBlob(objectName() + "/ThreadMultiView_Columns", m_pThreadList->saveState());
						theConf->SetValue(objectName() + "/ThreadMultiView_ColumnsVersion", ColumnLayoutVersion); break;
	}

	m_ViewMode = ViewMode;

	QByteArray Columns;
	switch (m_ViewMode)
	{
		case eSingle:	if (theConf->GetValue(objectName() + "/ThreadView_ColumnsVersion", 1).toInt() == ColumnLayoutVersion)
							Columns = theConf->GetBlob(objectName() + "/ThreadView_Columns");
						break;
		case eMulti:	if (theConf->GetValue(objectName() + "/ThreadMultiView_ColumnsVersion", 1).toInt() == ColumnLayoutVersion)
							Columns = theConf->GetBlob(objectName() + "/ThreadMultiView_Columns");
						break;
		default:
			return;
	}

	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pThreadList->restoreState(Columns);
}

void CThreadsView::OnResetColumns()
{
	for (int i = 0; i < m_pThreadModel->columnCount(); i++)
		m_pThreadList->SetColumnHidden(i, true);

	m_pThreadList->SetColumnHidden(CThreadModel::eThread, false);
	m_pThreadList->SetColumnHidden(CThreadModel::eCPU, false);
	if(m_ViewMode == eSingle)
		m_pThreadList->SetColumnHidden(CThreadModel::eCPU_History, false);
	m_pThreadList->SetColumnHidden(CThreadModel::eStartAddress, false);
	m_pThreadList->SetColumnHidden(CThreadModel::ePriority, false);
	m_pThreadList->SetColumnHidden(CThreadModel::eService, false);
	m_pThreadList->SetColumnHidden(CThreadModel::eState, false);
	m_pThreadList->SetColumnHidden(CThreadModel::eType, false);
	m_pThreadList->SetColumnHidden(CThreadModel::eCreated, false);
}

void CThreadsView::OnColumnsChanged()
{
	m_pThreadModel->Sync(m_Threads);
}

void CThreadsView::AddHeaderColumn(QMenu* pMenu, int Column)
{
	// Columns a backend does not fill carry an empty header - skip them, exactly
	// as QTreeViewEx's built-in menu does, so the list stays honest per platform.
	QString Label = m_pThreadModel->headerData(Column, Qt::Horizontal, Qt::DisplayRole).toString();
	if (Label.isEmpty())
		return;

	QCheckBox *checkBox = new QCheckBox(Label, pMenu);
	connect(checkBox, SIGNAL(stateChanged(int)), this, SLOT(OnHeaderMenu()));
	QWidgetAction *pAction = new QWidgetAction(pMenu);
	pAction->setDefaultWidget(checkBox);
	pMenu->addAction(pAction);

	m_Columns[checkBox] = Column;
}

void CThreadsView::AddHeaderSubMenu(QMenu* pMenu, const QString& Label, int from, int to)
{
	QMenu* pSubMenu = pMenu->addMenu(Label);
	for (int i = from; i <= to; i++)
		AddHeaderColumn(pSubMenu, i);
}

void CThreadsView::OnHeaderMenu(const QPoint &point)
{
	if (m_Columns.isEmpty())
	{
		//
		// One submenu per contiguous enum group, naming only its first and last
		// entry - the thread EColumns is ordered for exactly this (see the note
		// there). Common columns sit at the top level; everything that is not CPU,
		// Scheduling or I/O is folded into one "Other" group.
		//
		for (int i = CThreadModel::eThread; i <= CThreadModel::eCreated; i++)
			AddHeaderColumn(m_pHeaderMenu, i);

		m_pHeaderMenu->addSeparator();
		AddHeaderSubMenu(m_pHeaderMenu, tr("CPU"), CThreadModel::eContextSwitches, CThreadModel::eUserTime);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Scheduling"), CThreadModel::ePriority, CThreadModel::ePriorityBoost);
		AddHeaderSubMenu(m_pHeaderMenu, tr("I/O"), CThreadModel::eIO_Reads, CThreadModel::eIO_OtherRate);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Other"), CThreadModel::eStartModule, CThreadModel::eRPC_Usage);

		m_pHeaderMenu->addSeparator();
		QAction* pAction = m_pHeaderMenu->addAction(tr("Reset columns"));
		connect(pAction, SIGNAL(triggered()), this, SLOT(OnResetColumns()));
	}

	for (QMap<QCheckBox*, int>::iterator I = m_Columns.begin(); I != m_Columns.end(); I++)
		I.key()->setChecked(!m_pThreadList->isColumnHidden(I.value()));

	m_pHeaderMenu->popup(QCursor::pos());
}

void CThreadsView::OnHeaderMenu()
{
	QCheckBox *checkBox = (QCheckBox*)sender();
	int Column = m_Columns.value(checkBox, -1);
	if (Column < 0)
		return;

	// SetColumnHidden flips the view, updates the model's enabled flag and emits
	// ColumnChanged - which drives OnColumnsChanged (and the graph column).
	m_pThreadList->SetColumnHidden(Column, !checkBox->isChecked());
}

void CThreadsView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	if (m_Processes != Processes)
	{
		disconnect(this, SLOT(ShowThreads(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

		m_Processes = Processes;
		//m_PendingUpdates = 0;

		m_pThreadModel->SetUseIcons(m_Processes.size() > 1);
		m_pThreadModel->SetExtThreadId(m_Processes.size() > 1);
		m_pThreadModel->Clear();

		SwitchView(m_Processes.size() > 1 ? eMulti : eSingle);

		foreach(const CProcessPtr& pProcess, m_Processes)
			connect(pProcess.data(), SIGNAL(ThreadsUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(ShowThreads(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
	}

	Refresh();
}

void CThreadsView::SelectThread(quint64 ThreadId)
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
	if(!m_pCurThread.isNull() && m_CurStackTraceJob == 0)
	{
		m_pStackTraceSystem = m_pCurThread->GetSystem().data();
		m_CurStackTraceJob = m_pCurThread->TraceStack();
	}

	//if (m_PendingUpdates > 0)
	//	return;

	//m_PendingUpdates = 0;
	foreach(const CProcessPtr& pProcess, m_Processes)
	{
		//m_PendingUpdates++;
		// The call holds its own reference; see CHandlesView::Refresh.
		const CProcessPtr pKeep = pProcess;
		QMetaObject::invokeMethod(pProcess.data(), [pKeep]() { pKeep->UpdateThreads(); },
			Qt::QueuedConnection);
	}

	// Note: See coment in CWinProcess::UpdateThreads(), ... so windows we just call ShowThreads right away.
	ShowThreads(QSet<quint64>(), QSet<quint64>(), QSet<quint64>());
}

void CThreadsView::ShowThreads(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	if(m_Processes.count() == 1)
	{
		//m_PendingUpdates = 0;

		m_Threads = m_Processes.first()->GetThreadList();
	}
	else // merge the maps
	{
		//if (--m_PendingUpdates > 0)
		//	return;

		m_Threads.clear();
		foreach(const CProcessPtr& pProcess, m_Processes) {
			QMap<quint64, CThreadPtr> Threads = pProcess->GetThreadList();
			for (QMap<quint64, CThreadPtr>::iterator I = Threads.begin(); I != Threads.end(); I++)
			{
				ASSERT(!m_Threads.contains(I.key()));
				m_Threads.insert(I.key(), I.value());
			}
		}
	}

	m_pThreadModel->Sync(m_Threads);

	OnUpdateHistory();
}

void CThreadsView::OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	//
	// Windows only: this configures DbgHelp's symbol *server* search path, so
	// it has no meaning on Linux - eu-stack resolves symbols out of the
	// binaries themselves. Asking anyway just interrupts the user with a modal
	// dialog the first time they select a thread.
	//
	// linux-todo: the nearest equivalent is debuginfod (DEBUGINFOD_URLS), which
	// eu-stack already honours when it is configured in the environment.
	//
	if (theConf->GetInt("Options/DbgHelpSearch", 2) == 2)
	{
		bool Ret = QMessageBox("TaskExplorer", tr("Do you want to download debug symbols of the internet?\nYou can change this option later on in the settings."), QMessageBox::Question, QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes;
		theConf->SetValue("Options/DbgHelpSearch", Ret ? 1 : 0);
	}
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);
	m_pCurThread = m_pThreadModel->GetThread(ModelIndex);

	if (m_CurStackTraceJob)
	{
		// cancel on the system the job was issued to, not on the selected one
		m_pStackTraceSystem->CancelSymbolJob(m_CurStackTraceJob);
		m_CurStackTraceJob = 0;
		m_pStackTraceSystem = NULL;
	}

	disconnect(m_pStackView, SLOT(ShowStack(const CStackTracePtr&)));

	if (m_pSplitter->sizes()[1] == 0)
		return;

	//m_pStackView->Clear();
	m_pStackView->Invalidate();

	if (!m_pCurThread.isNull()) 
	{
		connect(m_pCurThread.data(), SIGNAL(StackTraced(const CStackTracePtr&)), this, SLOT(ShowStack(const CStackTracePtr&)));

		m_pStackTraceSystem = m_pCurThread->GetSystem().data();
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
	m_pStackTraceSystem = NULL;

	m_pStackView->ShowStack(StackTrace);
}

QList<CTaskPtr> CThreadsView::GetSelectedTasks()
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

	QModelIndexList selectedRows = m_pThreadList->selectedRows();
	m_pCancelIO->setEnabled(!pThread.isNull());

	//
	// Hidden rather than left to fail for a machine this viewer is not on:
	// CWaitChainDialog asks the local Win32 wait chain API about the selected
	// pid and tid, so pointed elsewhere it answers about a local process that
	// happens to share the number. See the longer note in CProcessTree::OnMenu.
	//
	const CSystemPtr pViewed = pThread.isNull() ? CSystemPtr() : pThread->GetSystem();
	m_pWCT->setVisible(!pViewed.isNull() && pViewed->IsLocal());
	m_pWCT->setEnabled(selectedRows.count() == 1);

	m_pCritical->setEnabled(!pThread.isNull());
	m_pCritical->setChecked(pThread && pThread->IsCriticalThread());

	m_pToken->setEnabled(pThread && pThread->GetTokenState() == CThreadInfo::eTokenStatePresent);
	m_pToken2->setVisible(pThread && pThread->HasToken2());

	m_pPermissions->setEnabled(selectedRows.count() == 1 && theSystem->HasCapability(CSystemAPI::eCapSecurityEditor));
	CTaskView::OnMenu(point);
}

void CThreadsView::OnThreadAction()
{
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
		if (!pThread.isNull())
		{
		retry:
			STATUS Status = OK;
			if (sender() == m_pCancelIO)
				Status = pThread->CancelIO();
			else if(sender() == m_pCritical)
				Status = pThread->SetCriticalThread(m_pCritical->isChecked(), Force == 1);

			if (Status.IsError())
			{
				if (Status.GetStatus() == ERROR_CONFIRM)
				{
					if (Force == -1)
					{
						switch (QMessageBox("TaskExplorer", CTaskExplorer::FormatError(Status), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel | QMessageBox::Default | QMessageBox::Escape).exec())
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
}

void CThreadsView::OnUpdateHistory()
{
	QColor PlotBackground = Qt::white;
	if(theGUI->GetTheme()->IsDarkTheme())
		PlotBackground = Qt::black;

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
				pGraph = new CHistoryGraph(true, PlotBackground, this);
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

		const int ViewportHeight = m_pThreadList->viewport()->rect().height();

		//
		// Iterate on column 0 and cull by VERTICAL position only - the same fix
		// as CProcessTree::UpdateIndexWidget. Testing the history column's full
		// rect for intersection broke the loop on the first row whenever that
		// column was scrolled horizontally out of view, so the thread graphs
		// rendered nothing; and walking indexBelow() from a history-column index
		// stopped after the first row. The thread graphs are keyed by thread id,
		// which is also the model's node id (UserRole/eThread), so unlike the
		// process tree there is no key mismatch to fix here.
		//
		for(QModelIndex Row = m_pThreadList->indexAt(QPoint(0,0)); Row.isValid(); Row = m_pThreadList->indexBelow(Row))
		{
			QModelIndex Index = Row.sibling(Row.row(), HistoryColumn);
			if (m_pThreadList->visualRect(Index).top() >= ViewportHeight)
				break; // below the visible area

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

void CThreadsView::OnThreadToken()
{
	QList<CTaskPtr>	Tasks = GetSelectedTasks();
	if (Tasks.count() != 1)
		return;
	CThreadPtr pThread = Tasks.first().staticCast<CThreadInfo>();
	if (pThread.isNull())
		return;

#ifdef WIN32	// this view is in TE_GUI_WIN; phase 4 gives it a portable API
	CTokenInfoPtr pToken = (sender() == m_pToken2) ? pThread->GetOriginalToken() : pThread->GetToken();
	if (pToken)
	{
		CTokenView* pTokenView = new CTokenView();
		CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow(pTokenView, tr("Token"));
		pTokenView->ShowToken(pToken);
		pTaskInfoWindow->show();
	}
#endif
}

void CThreadsView::OnWCT()
{
	QList<CTaskPtr>	Tasks = GetSelectedTasks();
	if (Tasks.count() != 1)
		return;
	CThreadPtr pThread = Tasks.first().staticCast<CThreadInfo>();
	if (pThread.isNull())
		return;

#ifdef WIN32	// this view is in TE_GUI_WIN; phase 4 gives it a portable API
	CWaitChainDialog* pWnd = new CWaitChainDialog(pThread);
	pWnd->show();
#endif
}

void CThreadsView::OnPermissions()
{
	QList<CTaskPtr>	Tasks = GetSelectedTasks();
	if (Tasks.count() != 1)
		return;

	if (CThreadPtr pThread = Tasks.first().staticCast<CThreadInfo>())
		CTaskExplorer::ShowSecurity(pThread->GetSecurityObject(), this);
}
