#include "stdafx.h"
#include "../TaskExplorer/GUI/TaskExplorer.h"
#include "JobView.h"
#include "../ProcessPicker.h"
#include "..\..\Common\SortFilterProxyModel.h"
#include "../Models\ProcessModel.h"

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
	m_pTerminate = new QPushButton(tr("Terminate"));
	connect(m_pTerminate, SIGNAL(pressed()), this, SLOT(OnTerminate()));
	m_pMainLayout->addWidget(m_pTerminate, row++, 2);

	m_pMainLayout->addWidget(new QLabel(tr("Processes in job:")), row, 0, 1 , 2);
	//m_pPermissions = new QPushButton(tr("Permissions"));
	//connect(m_pPermissions, SIGNAL(pressed()), this, SLOT(OnPermissions()));
	//m_pMainLayout->addWidget(m_pPermissions, row++, 2);
	m_pAddProcess = new QPushButton(tr("Add process"));
	connect(m_pAddProcess, SIGNAL(pressed()), this, SLOT(OnAddProcess()));
	m_pMainLayout->addWidget(m_pAddProcess, row++, 2);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);
	m_pMainLayout->addWidget(m_pSplitter, row++, 0, 1, 3);

	// Thread List
	m_pProcessModel = new CProcessModel();
	//connect(m_pProcessModel, SIGNAL(CheckChanged(quint64, bool)), this, SLOT(OnCheckChanged(quint64, bool)));
	//connect(m_pProcessModel, SIGNAL(Updated()), this, SLOT(OnUpdated()));

	m_pProcessModel->SetTree(false);

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pProcessModel);
	m_pSortProxy->setDynamicSortFilter(true);

	// setup default collumns
	m_pProcessModel->SetColumnEnabled(CProcessModel::ePID, true);
	m_pProcessModel->SetColumnEnabled(CProcessModel::eCPU, true);
	m_pProcessModel->SetColumnEnabled(CProcessModel::eIO_TotalRate, true);
	//m_pProcessModel->SetColumnEnabled(CProcessModel::eStaus, true);
	//m_pProcessModel->SetColumnEnabled(CProcessModel::ePrivateBytes, true);
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
	//m_pProcessModel->SetColumnEnabled(CProcessModel::eIO_ReadBytes, true);
	//m_pProcessModel->SetColumnEnabled(CProcessModel::eIO_WriteBytes, true);
	//m_pProcessModel->SetColumnEnabled(CProcessModel::eIO_OtherBytes, true);

	m_pProcessList = new QTreeViewEx();
	m_pProcessList->setItemDelegate(new CStyledGridItemDelegate(m_pProcessList->fontMetrics().height() + 3, this));

	m_pProcessList->setModel(m_pSortProxy);

	m_pProcessList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pProcessList->setSortingEnabled(true);

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

	// Limits....

	m_pSubWidget = new QWidget();
	m_pSubLayout = new QGridLayout();
	m_pSubLayout->setMargin(0);
	m_pSubWidget->setLayout(m_pSubLayout);
	m_pSplitter->addWidget(m_pSubWidget);

	m_pSubLayout->addWidget(new QLabel(tr("Job stats:")), row++, 0, 1 , 2);
	m_pJobStats = new CStatsView(CStatsView::eJob, this);
	m_pSubLayout->addWidget(m_pJobStats, row++, 0, 1, 3);

	m_pPermissions = new QPushButton(tr("Permissions"));
	connect(m_pPermissions, SIGNAL(pressed()), this, SLOT(OnPermissions()));
	m_pMainLayout->addWidget(m_pPermissions, row++, 2);

	//m_pMenu = new QMenu();
	
	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	m_pProcessList->header()->restoreState(theConf->GetBlob(objectName() + "/m_pJobList"));
}


CJobView::~CJobView()
{
	theConf->SetBlob(objectName() + "/JobView_Columns", m_pProcessList->header()->saveState());
}

void CJobView::ShowJob(const CProcessPtr& pProcess)
{
	if (m_pCurProcess != pProcess)
	{
		CWinProcess* pWinProc = qobject_cast<CWinProcess*>(pProcess.data());
		m_pCurJob = pWinProc->GetJob();

		this->setEnabled(!m_pCurJob.isNull());
		if (m_pCurJob.isNull())
			return;

		m_pJobName->setText(m_pCurJob->GetJobName());
		m_pCurProcess = pProcess;
	}

	m_pCurJob->UpdateDynamicData();

	m_pJobStats->ShowJob(m_pCurJob);

	QMap<quint64, CProcessPtr> ProcessList = m_pCurJob->GetProcesses();
	m_pProcessModel->Sync(ProcessList);
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