#include "stdafx.h"
#include "TaskExplorer.h"
#include "ProcessPicker.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "Models/ProcessModel.h"

CProcessPicker::CProcessPicker(QWidget* parent)
	: QDialog(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
 
	m_ProcessId = 0;

	// Thread List
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

	m_pProcessList->setModel(m_pSortProxy);

	m_pProcessList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pProcessList->setSortingEnabled(true);

	m_pProcessList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pProcessList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(theGUI, SIGNAL(ReloadPanels()), m_pProcessModel, SLOT(Clear()));

	//connect(m_pProcessList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	connect(m_pProcessList->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));

	m_pProcessList->setColumnReset(2);
	connect(m_pProcessList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	connect(m_pProcessList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	m_pMainLayout->addWidget(m_pProcessList);
	// 

	m_pButtonBox = new QDialogButtonBox();
	m_pButtonBox->setOrientation(Qt::Horizontal);
	m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
	m_pMainLayout->addWidget(m_pButtonBox);
 
	connect(m_pButtonBox,SIGNAL(accepted()),this,SLOT(accept()));
	connect(m_pButtonBox,SIGNAL(rejected()),this,SLOT(reject()));

	for (int i = 0; i < m_pProcessModel->columnCount(); i++)
	{
		if ((i >= CProcessModel::eCPU_History && i <= CProcessModel::eVMEM_History)
		 || (i >= CProcessModel::eIO_TotalRate && i <= CProcessModel::eIO_OtherRate)
		 || (i >= CProcessModel::eNet_TotalRate && i <= CProcessModel::eSendRate)
		 || (i >= CProcessModel::eDisk_TotalRate && i <= CProcessModel::eWriteRate)
		 || i == CProcessModel::eSharedWS || i == CProcessModel::eShareableWS 
		 || i == CProcessModel::eMinimumWS || i == CProcessModel::eMaximumWS)
		{
			m_pProcessList->SetColumnHidden(i, true, true);
		}
	}

	QByteArray Columns = theConf->GetBlob("ProcessPicker/Process_Columns");
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pProcessList->restoreState(Columns);

	m_ProcessList = theAPI->GetProcessList();

	Refresh();
}

CProcessPicker::~CProcessPicker()
{
	theConf->SetBlob("ProcessPicker/Process_Columns", m_pProcessList->saveState());
}

void CProcessPicker::OnResetColumns()
{
	for (int i = 0; i < m_pProcessModel->columnCount(); i++)
		m_pProcessList->SetColumnHidden(i, true);

	m_pProcessList->SetColumnHidden(CProcessModel::ePID, false);
	m_pProcessList->SetColumnHidden(CProcessModel::eCPU, false);
	m_pProcessList->SetColumnHidden(CProcessModel::eIO_TotalRate, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eStaus, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::ePrivateBytes, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::ePriorityClass, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eGDI_Handles, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eUSER_Handles, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eWND_Handles, false);
	m_pProcessList->SetColumnHidden(CProcessModel::eStartTime, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eIO_ReadRate, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eIO_WriteRate, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eIO_OtherRate, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eReceiveRate, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eSendRate, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eReadRate, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eWriteRate, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eIO_ReadBytes, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eIO_WriteBytes, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eIO_OtherBytes, false);
	m_pProcessList->SetColumnHidden(CProcessModel::eCommandLine, false);
}

void CProcessPicker::OnColumnsChanged()
{
	Refresh();
}

void CProcessPicker::OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);

	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	m_ProcessId = pProcess->GetProcessId();
}

void CProcessPicker::Refresh()
{
	m_pProcessModel->Sync(m_ProcessList);
}