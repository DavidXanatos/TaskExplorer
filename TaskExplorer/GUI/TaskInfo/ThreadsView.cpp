#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ThreadsView.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinThread.h"
#endif

CThreadsView::CThreadsView(QWidget *parent)
	: CTaskView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pFilterWidget = new QWidget();
	m_pFilterWidget->setMinimumHeight(32);
	m_pMainLayout->addWidget(m_pFilterWidget);

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
	m_pThreadList->setItemDelegate(new QStyledItemDelegateMaxH(m_pThreadList->fontMetrics().height() + 3, this));

	m_pThreadList->setModel(m_pSortProxy);

	m_pThreadList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pThreadList->setSortingEnabled(true);

	m_pSplitter->addWidget(m_pThreadList);
	m_pSplitter->setCollapsible(0, false);

	m_pThreadList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pThreadList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	//connect(m_pThreadList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	connect(m_pThreadList->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));
	// 

	// Stack List
	m_pStackView = new CStackView(this);
	
	m_pSplitter->addWidget(m_pStackView);
	m_pSplitter->setCollapsible(1, false);
	// 

	//m_pMenu = new QMenu();
	AddTaskItemsToMenu();
	m_pMenu->addSeparator();
#ifdef WIN32
	m_pCancelIO = m_pMenu->addAction(tr("Cancel I/O"), this, SLOT(OnCancelIO()));
	//m_pAnalyze;
	m_pCritical = m_pMenu->addAction(tr("Critical"), this, SLOT(OnCritical()));
	m_pCritical->setCheckable(true);
	//m_pPermissions;
	//m_pToken;
#endif
	//m_pWindows;
	AddPriorityItemsToMenu(eThread);
	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	m_pThreadList->header()->restoreState(theConf->GetValue(objectName() + "/ThreadView_Columns").toByteArray());
}


CThreadsView::~CThreadsView()
{
	theConf->SetValue(objectName() + "/ThreadView_Columns", m_pThreadList->header()->saveState());
}

void CThreadsView::ShowThreads(const CProcessPtr& pProcess)
{
	m_pCurProcess = pProcess;

	m_pCurProcess->UpdateThreads();

	QMap<quint64, CThreadPtr> Threads = m_pCurProcess->GetThreadList();

	m_pThreadModel->Sync(Threads);

	if(!m_pCurThread.isNull())
		m_pCurThread->TraceStack();
}

void CThreadsView::OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);

	m_pCurThread = m_pThreadModel->GetThread(ModelIndex);

	if (!m_pCurThread.isNull()) 
	{
		disconnect(m_pStackView, SLOT(ShowStack(const CStackTracePtr&)));

		m_pStackView->Clear();

		connect(m_pCurThread.data(), SIGNAL(StackTraced(const CStackTracePtr&)), m_pStackView, SLOT(ShowStack(const CStackTracePtr&)));

		m_pCurThread->TraceStack();
	}
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
#endif

	CPanelView::OnMenu(point);
}

void CThreadsView::OnCancelIO()
{
#ifdef WIN32
	if(QMessageBox("TaskExplorer", tr("Do you want to cancel I/O for the selected thread(s)?"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

	int ErrorCount = 0;
	foreach(const QModelIndex& Index, m_pThreadList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CThreadPtr pThread = m_pThreadModel->GetThread(ModelIndex);
		QSharedPointer<CWinThread> pWinThread = pThread.objectCast<CWinThread>();
		if (!pWinThread.isNull())
		{
			if (!pWinThread->CancelIO())
				ErrorCount++;
		}
	}

	if (ErrorCount > 0)
		QMessageBox::warning(this, "TaskExplorer", tr("Failed to cancel IO for %1 threads.").arg(ErrorCount));
#endif
}

void CThreadsView::OnCritical()
{
#ifdef WIN32
	int ErrorCount = 0;
	int Force = -1;
	foreach(const QModelIndex& Index, m_pThreadList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CThreadPtr pThread = m_pThreadModel->GetThread(ModelIndex);
		QSharedPointer<CWinThread> pWinThread = pThread.objectCast<CWinThread>();
		if (!pWinThread.isNull())
		{
		retry:
			STATUS Status = pWinThread->SetCriticalThread(m_pCritical->isChecked(), Force == 1);
			if (Status.IsError())
			{
				if (Force == -1 && Status.GetStatus() == ERROR_CONFIRM)
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

				ErrorCount++;
			}
		}
	}

	if (ErrorCount > 0)
		QMessageBox::warning(this, "TaskExplorer", tr("Failed to set %1 thread critical").arg(ErrorCount));
#endif
}