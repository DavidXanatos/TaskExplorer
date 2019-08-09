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


	for (int i = 0; i < m_pProcessModel->columnCount(); i++)
	{
		if ((i >= CProcessModel::eCPU_History && i <= CProcessModel::eVMEM_History)
		 || (i >= CProcessModel::eIO_TotalRate && i <= CProcessModel::eIO_OtherRate)
		 || (i >= CProcessModel::eNet_TotalRate && i <= CProcessModel::eSendRate)
		 || (i >= CProcessModel::eDisk_TotalRate && i <= CProcessModel::eWriteRate)
		 || i == CProcessModel::eSharedWS || i == CProcessModel::eShareableWS 
		 || i == CProcessModel::eMinimumWS || i == CProcessModel::eMaximumWS)
		{
			m_pProcessList->setColumnFixed(i, true);
			m_pProcessList->setColumnHidden(i, true);
			m_pProcessModel->SetColumnEnabled(i, false);
		}
		else
			m_pProcessModel->SetColumnEnabled(i, true);
	}

	QByteArray Columns = theConf->GetBlob("ProcessPicker/Process_Columns");
	if (Columns.isEmpty())
	{
		for (int i = 0; i < m_pProcessModel->columnCount(); i++)
			m_pProcessList->setColumnHidden(i, true);

		m_pProcessList->setColumnHidden(CProcessModel::ePID, false);
		m_pProcessList->setColumnHidden(CProcessModel::eCPU, false);
		m_pProcessList->setColumnHidden(CProcessModel::eIO_TotalRate, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eStaus, false);
		//m_pProcessList->setColumnHidden(CProcessModel::ePrivateBytes, false);
		//m_pProcessList->setColumnHidden(CProcessModel::ePriorityClass, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eGDI_Handles, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eUSER_Handles, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eWND_Handles, false);
		m_pProcessList->setColumnHidden(CProcessModel::eStartTime, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eIO_ReadRate, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eIO_WriteRate, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eIO_OtherRate, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eReceiveRate, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eSendRate, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eReadRate, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eWriteRate, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eIO_ReadBytes, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eIO_WriteBytes, false);
		//m_pProcessList->setColumnHidden(CProcessModel::eIO_OtherBytes, false);
		m_pProcessList->setColumnHidden(CProcessModel::eCommandLine, false);
	}
	else
		m_pProcessList->header()->restoreState(Columns);
}

CProcessPicker::~CProcessPicker()
{
	theConf->SetBlob("ProcessPicker/Process_Columns", m_pProcessList->header()->saveState());
}

void CProcessPicker::OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);

	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	m_ProcessId = pProcess->GetProcessId();
}