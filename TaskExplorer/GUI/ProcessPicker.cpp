#include "stdafx.h"
#include "TaskExplorer.h"
#include "ProcessPicker.h"
#include "../Common/SortFilterProxyModel.h"
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

	// setup default collumns
	m_pProcessModel->SetColumnEnabled(CProcessModel::ePID, true);
	m_pProcessModel->SetColumnEnabled(CProcessModel::eCPU, true);
	//m_pProcessModel->SetColumnEnabled(CProcessModel::eIO_TotalRate, true);
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
	m_pProcessList->setItemDelegate(theGUI->GetItemDelegate());

	m_pProcessList->setModel(m_pSortProxy);

	m_pProcessList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pProcessList->setSortingEnabled(true);

	m_pMainLayout->addWidget(m_pProcessList);

	m_pProcessList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pProcessList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(theGUI, SIGNAL(ReloadAll()), m_pProcessModel, SLOT(Clear()));

	//connect(m_pProcessList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	connect(m_pProcessList->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));
	// 

	m_pButtonBox = new QDialogButtonBox();
	m_pButtonBox->setOrientation(Qt::Horizontal);
	m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
	m_pMainLayout->addWidget(m_pButtonBox);
 
	connect(m_pButtonBox,SIGNAL(accepted()),this,SLOT(accept()));
	connect(m_pButtonBox,SIGNAL(rejected()),this,SLOT(reject()));



	QMap<quint64, CProcessPtr> ProcessList = theAPI->GetProcessList();
	m_pProcessModel->Sync(ProcessList);
}

CProcessPicker::~CProcessPicker()
{
}

void CProcessPicker::OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);

	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	m_ProcessId = pProcess->GetProcessId();
}