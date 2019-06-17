#include "stdafx.h"
#include "TaskExplorer.h"
#include "ProcessTree.h"
#include "../Common/Common.h"
#include "Models\ProcessModel.h"
#include "Models\SortFilterProxyModel.h"
#ifdef WIN32
#include "../API/Windows/WinProcess.h"
#endif
#include "TaskInfo/TaskInfoWindow.h"


CProcessTree::CProcessTree(QWidget *parent)
	: CTaskView(parent)
{
	m_ExpandAll = false;

	this->ForceColumn(CProcessModel::eProcess);

	m_pMainLayout = new QHBoxLayout();
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

	connect(m_pProcessList, SIGNAL(TreeResized(int)), this, SLOT(OnTreeResized(int)));

	m_pProcessList->GetView()->header()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pProcessList->GetView()->header(), SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnHeaderMenu(const QPoint &)));

	m_pHeaderMenu = new QMenu(this);

	m_pMainLayout->addWidget(m_pProcessList);
	// 



	//m_pMenu = new QMenu();
	AddTaskItemsToMenu();
	//QAction*				m_pTerminateTree;
	m_pMenu->addSeparator();

	m_pCreateDump = m_pMenu->addAction(tr("Create Crash Dump"), this, SLOT(OnCrashDump()));
	m_pDebug = m_pMenu->addAction(tr("Debug"), this, SLOT(OnDebug()));
	m_pDebug->setCheckable(true);
#ifdef WIN32
	m_pVirtualization = m_pMenu->addAction(tr("Virtualization"), this, SLOT(OnVirtualization()));
	m_pVirtualization->setCheckable(true);
	//QAction*				m_pWindows;
	//QAction*				m_pGDI_Handles;
	m_pCritical = m_pMenu->addAction(tr("Critical"), this, SLOT(OnCritical()));
	m_pCritical->setCheckable(true);
	m_pReduceWS = m_pMenu->addAction(tr("Reduce Working Set"), this, SLOT(OnReduceWS()));
	//QAction*				m_pUnloadModules;
	//QAction*				m_pWatchWS;
#endif

	AddPriorityItemsToMenu(eProcess, m_pMenu);
	AddPanelItemsToMenu(m_pMenu);



	//connect(m_pProcessList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	connect(m_pProcessList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));
	connect(m_pProcessList, SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));


	connect(theAPI, SIGNAL(ProcessListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), SLOT(OnProcessListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

	QStringList EnabledColumns = theConf->GetValue("MainWindow/ProcessTree_EnabledColumns").toStringList();
	if (EnabledColumns.count() > 1)
	{
		foreach(const QString& Column, EnabledColumns)
			m_pProcessModel->SetColumnEnabled(Column.toInt(), true);
	}
	else
	{
		// setup default collumns
		m_pProcessModel->SetColumnEnabled(CProcessModel::ePID, true);
		m_pProcessModel->SetColumnEnabled(CProcessModel::eCPU, true);
		m_pProcessModel->SetColumnEnabled(CProcessModel::eIO_TotalRate, true);
		m_pProcessModel->SetColumnEnabled(CProcessModel::eStaus, true);
		m_pProcessModel->SetColumnEnabled(CProcessModel::ePrivateBytes, true);
		//m_pProcessModel->SetColumnEnabled(CProcessModel::ePriorityClass, true);
		//m_pProcessModel->SetColumnEnabled(CProcessModel::eGDI_Handles, true);
		//m_pProcessModel->SetColumnEnabled(CProcessModel::eUSER_Handles, true);
		//m_pProcessModel->SetColumnEnabled(CProcessModel::eWND_Handles, true);
		m_pProcessModel->SetColumnEnabled(CProcessModel::eStartTime, true);
		//m_pProcessModel->SetColumnEnabled(CProcessModel::eIO_ReadRate, true);
		//m_pProcessModel->SetColumnEnabled(CProcessModel::eIO_WriteRate, true);
		//m_pProcessModel->SetColumnEnabled(CProcessModel::eIO_OtherRate, true);
		//m_pProcessModel->SetColumnEnabled(CProcessModel::eReceiveRate, true);
		//m_pProcessModel->SetColumnEnabled(CProcessModel::eSendRate, true);
		//m_pProcessModel->SetColumnEnabled(CProcessModel::eReadRate, true);
		//m_pProcessModel->SetColumnEnabled(CProcessModel::eWriteRate, true);
		m_pProcessModel->SetColumnEnabled(CProcessModel::eIO_ReadBytes, true);
		m_pProcessModel->SetColumnEnabled(CProcessModel::eIO_WriteBytes, true);
		m_pProcessModel->SetColumnEnabled(CProcessModel::eIO_OtherBytes, true);
	}


	m_pProcessList->GetSplitter()->restoreState(theConf->GetValue("MainWindow/ProcessTree_Splitter").toByteArray());
	m_pProcessList->GetView()->header()->restoreState(theConf->GetValue("MainWindow/ProcessTree_Columns").toByteArray());
}

CProcessTree::~CProcessTree()
{
	theConf->SetValue("MainWindow/ProcessTree_Splitter",m_pProcessList->GetSplitter()->saveState());
	theConf->SetValue("MainWindow/ProcessTree_Columns", m_pProcessList->GetView()->header()->saveState());

	QStringList EnabledColumns;
	foreach(int column, m_pProcessModel->GetColumns())
		EnabledColumns.append(QString::number(column));
	theConf->SetValue("MainWindow/ProcessTree_EnabledColumns", EnabledColumns);
}

void CProcessTree::OnTreeResized(int Width)
{
	if (m_pProcessModel->IsTree() != (Width > 0))
	{
		m_pProcessModel->SetTree(Width > 0);

		if (Width > 0)
			m_ExpandAll = true;
	}
}

void CProcessTree::OnHeaderMenu(const QPoint &point)
{
	if(m_Columns.isEmpty())
	{
		for(int i=1; i < m_pProcessModel->MaxColumns(); i++)
		{
			QCheckBox *checkBox = new QCheckBox(m_pProcessModel->GetColumn(i), m_pHeaderMenu);
			connect(checkBox, SIGNAL(stateChanged(int)), this, SLOT(OnHeaderMenu()));
			QWidgetAction *pAction = new QWidgetAction(m_pHeaderMenu);
			pAction->setDefaultWidget(checkBox);
			m_pHeaderMenu->addAction(pAction);

			m_Columns[checkBox] = i;
		}
	}

	for(QMap<QCheckBox*, int>::iterator I = m_Columns.begin(); I != m_Columns.end(); I++)
		I.key()->setChecked(m_pProcessModel->IsColumnEnabled(I.value()));

	m_pHeaderMenu->popup(QCursor::pos());	
}

void CProcessTree::OnHeaderMenu()
{
	QCheckBox *checkBox = (QCheckBox*)sender();
	int Column = m_Columns.value(checkBox, -1);
	m_pProcessModel->SetColumnEnabled(Column, checkBox->isChecked());
}

void CProcessTree::OnProcessListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	QMap<quint64, CProcessPtr> ProcessList = theAPI->GetProcessList();

	m_pProcessModel->Sync(ProcessList);

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
}

void CProcessTree::OnDoubleClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);

	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow();
	pTaskInfoWindow->ShowProcess(pProcess);
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
	
	m_pCreateDump->setEnabled(!pProcess.isNull() && m_pProcessList->selectedRows().count() == 1);
	m_pDebug->setEnabled(!pProcess.isNull());
	m_pDebug->setChecked(pProcess && pProcess->HasDebugger());

#ifdef WIN32
	QSharedPointer<CWinProcess> pWinProcess = pProcess.objectCast<CWinProcess>();

	m_pVirtualization->setEnabled(!pWinProcess.isNull() && pWinProcess->IsVirtualizationAllowed());
	m_pVirtualization->setChecked(pWinProcess && pWinProcess->IsVirtualizationEnabled());

	m_pCritical->setEnabled(!pWinProcess.isNull());
	m_pCritical->setChecked(pWinProcess && pWinProcess->IsCriticalProcess());

	m_pReduceWS->setEnabled(!pWinProcess.isNull());
#endif

	CTaskView::OnMenu(point);
}

void CProcessTree::OnCrashDump()
{
	QString DumpPath = ""; // todo get file path open file open dialog

	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);

	STATUS status = pProcess->CreateDump(DumpPath);
	if(!status)
		QMessageBox::warning(this, "TaskExplorer", tr("Failed to create dump file, reason: %1").arg(status.GetText()));
}

void CProcessTree::OnVirtualization()
{
#ifdef WIN32
	int ErrorCount = 0;
	foreach(const QModelIndex& Index, m_pProcessList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
		QSharedPointer<CWinProcess> pWinProcess = pProcess.objectCast<CWinProcess>();
		if (!pWinProcess.isNull())
		{
			if (!pWinProcess->SetVirtualizationEnabled(m_pVirtualization->isChecked()))
				ErrorCount++;
		}
	}

	if (ErrorCount > 0)
		QMessageBox::warning(this, "TaskExplorer", tr("Failed to set virtualization for %1 processes.").arg(ErrorCount));
#endif
}

void CProcessTree::OnCritical()
{
#ifdef WIN32
	int ErrorCount = 0;
	int Force = -1;
	foreach(const QModelIndex& Index, m_pProcessList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
		QSharedPointer<CWinProcess> pWinProcess = pProcess.objectCast<CWinProcess>();
		if (!pWinProcess.isNull())
		{
		retry:
			STATUS Status = pWinProcess->SetCriticalProcess(m_pCritical->isChecked(), Force == 1);
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
		QMessageBox::warning(this, "TaskExplorer", tr("Failed to set %1 process critical").arg(ErrorCount));
#endif
}

void CProcessTree::OnReduceWS()
{
#ifdef WIN32
	int ErrorCount = 0;
	foreach(const QModelIndex& Index, m_pProcessList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
		QSharedPointer<CWinProcess> pWinProcess = pProcess.objectCast<CWinProcess>();
		if (!pWinProcess.isNull())
		{
			if (!pWinProcess->ReduceWS())
				ErrorCount++;
		}
	}

	if (ErrorCount > 0)
		QMessageBox::warning(this, "TaskExplorer", tr("Failed to reduce working set of %1 processes.").arg(ErrorCount));
#endif
}
