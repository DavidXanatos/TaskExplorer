#include "stdafx.h"
#include "../TaskExplorer.h"
#include "JobView.h"
#include "../ProcessPicker.h"
#include "../../Common/SortFilterProxyModel.h"
#include "../Models/ProcessModel.h"

CJobView::CJobView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QGridLayout();
	//m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);
	int row = 0;

	m_pMainLayout->addWidget(new QLabel(tr("Job name:")), row, 0);
	m_pJobName = new QLineEdit();
	m_pJobName->setSizePolicy(QSizePolicy::Expanding, m_pJobName->sizePolicy().verticalPolicy());
	m_pMainLayout->addWidget(m_pJobName, row, 1);

	m_pMainLayout->addWidget(new QLabel(tr("Job object Id:")), row, 2);
	m_pJobId = new QLabel();
	m_pMainLayout->addWidget(m_pJobId, row, 3);

	m_pTerminate = new QPushButton(tr("Terminate"));
	connect(m_pTerminate, SIGNAL(pressed()), this, SLOT(OnTerminate()));
	m_pMainLayout->addWidget(m_pTerminate, row++, 4);

	m_pMainLayout->addWidget(new QLabel(tr("Processes in job:")), row, 0, 1, 4);
	//m_pPermissions = new QPushButton(tr("Permissions"));
	//connect(m_pPermissions, SIGNAL(pressed()), this, SLOT(OnPermissions()));
	//m_pMainLayout->addWidget(m_pPermissions, row++, 2);
	m_pAddProcess = new QPushButton(tr("Add process"));
	connect(m_pAddProcess, SIGNAL(pressed()), this, SLOT(OnAddProcess()));
	m_pMainLayout->addWidget(m_pAddProcess, row++, 4);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);
	m_pMainLayout->addWidget(m_pSplitter, row++, 0, 1, 5);

	// Process List
	m_pProcessModel = new CProcessModel();
	//connect(m_pProcessModel, SIGNAL(CheckChanged(quint64, bool)), this, SLOT(OnCheckChanged(quint64, bool)));
	//connect(m_pProcessModel, SIGNAL(Updated()), this, SLOT(OnUpdated()));

	m_pProcessModel->SetTree(false);

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pProcessModel);
	m_pSortProxy->setDynamicSortFilter(true);


	m_pProcessList = new QTreeViewEx();
	m_pProcessList->setItemDelegate(theGUI->GetItemDelegate());
	m_pProcessList->setMinimumHeight(50 * theGUI->GetDpiScale());

	m_pProcessList->setModel(m_pSortProxy);

	m_pProcessList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pProcessList->setSortingEnabled(true);

	connect(theGUI, SIGNAL(ReloadPanels()), m_pProcessModel, SLOT(Clear()));

	m_pProcessList->setColumnReset(2);
	connect(m_pProcessList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	connect(m_pProcessList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	//m_pMainLayout->addWidget(m_pProcessList, row++, 0, 1, 3);
	m_pSplitter->addWidget(m_pProcessList);

	//m_pProcessList->setContextMenuPolicy(Qt::CustomContextMenu);
	//connect(m_pProcessList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	//connect(m_pProcessList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	//connect(m_pProcessList->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)))
	//

	//m_pAddProcess = new QPushButton(tr("Add process"));
	//connect(m_pAddProcess, SIGNAL(pressed()), this, SLOT(OnAddProcess()));
	//m_pMainLayout->addWidget(m_pAddProcess, row, 2);

	m_pSubWidget = new QWidget();
	m_pSubLayout = new QGridLayout();
	m_pSubLayout->setMargin(0);
	m_pSubWidget->setLayout(m_pSubLayout);
	m_pSplitter->addWidget(m_pSubWidget);

	m_pAdvancedTabs = new QTabWidget();
	//m_pAdvancedTabs->setTabPosition(QTabWidget::South);
	m_pSubLayout->addWidget(m_pAdvancedTabs, row++, 0, 1, 3);

	m_pLimits = new CPanelWidgetEx();

	m_pLimits->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pLimits->GetView())->setHeaderLabels(tr("Name|Value").split("|"));

	m_pLimits->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pLimits->GetView()->setSortingEnabled(false);

	m_pAdvancedTabs->addTab(m_pLimits, tr("Limits"));


	//m_pSubLayout->addWidget(new QLabel(tr("Job stats:")), row++, 0, 1 , 2);
	m_pJobStats = new CStatsView(CStatsView::eJob, this);
	//m_pSubLayout->addWidget(m_pJobStats, row++, 0, 1, 3);
	m_pAdvancedTabs->addTab(m_pJobStats, tr("Statistics"));

	m_pPermissions = new QPushButton(tr("Permissions"));
	connect(m_pPermissions, SIGNAL(pressed()), this, SLOT(OnPermissions()));
	m_pMainLayout->addWidget(m_pPermissions, row++, 4);

	//m_pMenu = new QMenu();
	
	AddPanelItemsToMenu();

	for (int i = 0; i < m_pProcessModel->columnCount(); i++)
	{
		if ((i >= CProcessModel::eCPU_History && i <= CProcessModel::eVMEM_History)
		 || (i >= CProcessModel::eFileName && i <= CProcessModel::eFileSize)
#ifdef WIN32
		 || (i >= CProcessModel::eIntegrity && i <= CProcessModel::eCritical)
#endif
		 || (i >= CProcessModel::eCPU && i <= CProcessModel::eCyclesDelta)
		 || (i >= CProcessModel::ePrivateBytes && i <= CProcessModel::ePrivateBytesDelta)
		 || (i >= CProcessModel::eGPU_Usage && i <= CProcessModel::eGPU_Adapter)
		 || (i >= CProcessModel::ePriorityClass && i <= CProcessModel::eIO_Priority)
		 || (i >= CProcessModel::eHandles && i <= CProcessModel::ePeakThreads)
		 || (i >= CProcessModel::eSubsystem && i <= CProcessModel::eSessionID && i != CProcessModel::eJobObjectID)
		 || (i >= CProcessModel::eIO_TotalRate && i <= CProcessModel::eIO_OtherRate)
		 || (i >= CProcessModel::eNet_TotalRate && i <= CProcessModel::eSendRate)
		 || (i >= CProcessModel::eDisk_TotalRate && i <= CProcessModel::eWriteRate)
		 || i == CProcessModel::eSharedWS || i == CProcessModel::eShareableWS)
		{
			m_pProcessList->SetColumnHidden(i, true, true);
		}
	}

	setObjectName(parent ? parent->objectName() : "InfoWindow");
	QByteArray Columns = theConf->GetBlob(objectName() + "/JobProcess_Columns");
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pProcessList->restoreState(Columns);

	m_pLimits->GetView()->header()->restoreState(theConf->GetBlob(objectName() + "/JobLimits_Columns"));
}


CJobView::~CJobView()
{
	theConf->SetBlob(objectName() + "/JobProcess_Columns", m_pProcessList->saveState());
	theConf->SetBlob(objectName() + "/JobLimits_Columns", m_pLimits->GetView()->header()->saveState());
}

void CJobView::OnResetColumns()
{
	for (int i = 0; i < m_pProcessModel->columnCount(); i++)
		m_pProcessList->SetColumnHidden(i, true);

	m_pProcessList->SetColumnHidden(CProcessModel::eProcess, false);
	m_pProcessList->SetColumnHidden(CProcessModel::ePID, false);
	m_pProcessList->SetColumnHidden(CProcessModel::eCPU, false);
	m_pProcessList->SetColumnHidden(CProcessModel::eStartTime, false);
	m_pProcessList->SetColumnHidden(CProcessModel::eCommandLine, false);
}

void CJobView::OnColumnsChanged()
{
	m_pProcessModel->Sync(m_ProcessList);
}

void CJobView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	CProcessPtr pProcess;
	if (Processes.count() > 1)
	{
		setEnabled(false);
	}
	else if(!Processes.isEmpty())
	{
		setEnabled(true);
		pProcess = Processes.first();
	}

	if (m_pCurProcess == pProcess)
	{
		Refresh();
		return;
	}
	
	m_pCurProcess = pProcess.objectCast<CWinProcess>();
		
	if(m_pCurProcess)
		ShowJob(m_pCurProcess->GetJob());	
}

void CJobView::ShowJob(const CWinJobPtr& pJob)
{
	setEnabled(!pJob.isNull());

	if (m_pCurJob != pJob)
	{
		m_pCurJob = pJob;

		if (m_pCurJob.isNull())
			return;

		m_pJobName->setText(m_pCurJob->GetJobName());
		if(m_pCurProcess)
			m_pJobId->setText(QString::number(m_pCurProcess->GetJobObjectID()));
	}

	Refresh();
}

void CJobView::Refresh()
{
	if (!m_pCurJob)
		return;

	m_pCurJob->UpdateDynamicData();

	QList<CWinJob::SJobLimit> Limits = m_pCurJob->GetLimits();


	QMap<QString, QTreeWidgetItem*> OldLimits;
	for(int i = 0; i < m_pLimits->GetTree()->topLevelItemCount(); ++i) 
	{
		QTreeWidgetItem* pItem = m_pLimits->GetTree()->topLevelItem(i);
		QString Name = pItem->data(0, Qt::UserRole).toString();
		Q_ASSERT(!OldLimits.contains(Name));
		OldLimits.insert(Name,pItem);
	}

	for(QList<CWinJob::SJobLimit>::iterator I = Limits.begin(); I != Limits.end(); ++I)
	{
		QTreeWidgetItem* pItem = OldLimits.take(I->Name);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, I->Name);
			pItem->setText(0, I->Name);
			m_pLimits->GetTree()->addTopLevelItem(pItem);
		}

		switch (I->Type)
		{
		case CWinJob::SJobLimit::eString:	pItem->setText(1, I->Value.toString()); break;
		case CWinJob::SJobLimit::eSize:		pItem->setText(1, FormatSize(I->Value.toULongLong())); break;
		case CWinJob::SJobLimit::eTimeMs:	pItem->setText(1, FormatTime(I->Value.toULongLong(), true)); break;
		case CWinJob::SJobLimit::eAddress:	pItem->setText(1, FormatAddress(I->Value.toULongLong())); break;
		case CWinJob::SJobLimit::eNumber:	pItem->setText(1, QString::number(I->Value.toUInt())); break;
		case CWinJob::SJobLimit::eEnabled:	pItem->setText(1, I->Value.toBool() ? tr("Enabled") : tr("Disabled")); break;
		case CWinJob::SJobLimit::eLimited:	pItem->setText(1, I->Value.toBool() ? tr("Limited") : tr("Unlimited")); break;
		}
	}

	foreach(QTreeWidgetItem* pItem, OldLimits)
		delete pItem;

	m_pJobStats->ShowJob(m_pCurJob);

	m_ProcessList = m_pCurJob->GetProcesses();
	m_pProcessModel->Sync(m_ProcessList);
}

void CJobView::OnMenu(const QPoint &point)
{

	CPanelView::OnMenu(point);
}

void CJobView::OnPermissions()
{
	if (m_pCurJob)
		m_pCurJob->OpenPermissions();
}

void CJobView::OnTerminate()
{
	if(QMessageBox("TaskExplorer", tr("Do you to terminate the job?"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

	if (m_pCurJob)
	{
		if(!m_pCurJob->Terminate())
			QMessageBox::warning(this, "TaskExplorer", tr("Failed to terminate job."));
	}
}

void CJobView::OnAddProcess()
{
	CProcessPicker ProcessPicker;
	if (!ProcessPicker.exec())
		return;
	
	if (m_pCurJob)
		m_pCurJob->AddProcess(ProcessPicker.GetProcessId());
}