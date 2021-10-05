#include "stdafx.h"
#include "../TaskExplorer.h"
#include "RpcView.h"
#include "../../../MiscHelpers/Common/Common.h"
#include "../../API/Windows/WindowsAPI.h"		
#include "../../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "../../../MiscHelpers/Common/Finder.h"

CRpcView::CRpcView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pRpcModel = new CRpcModel();
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pRpcModel);
	m_pSortProxy->setDynamicSortFilter(true);


	// Rpc List
	m_pRpcList = new QTreeViewEx();
	m_pRpcList->setItemDelegate(theGUI->GetItemDelegate());

	m_pRpcList->setModel(m_pSortProxy);

	m_pRpcList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pRpcList->setSortingEnabled(true);

	m_pRpcList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pRpcList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(theGUI, SIGNAL(ReloadPanels()), m_pRpcModel, SLOT(Clear()));

	m_pRpcList->setColumnReset(2);
	connect(m_pRpcList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	connect(m_pRpcList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	m_pMainLayout->addWidget(m_pRpcList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

	QByteArray Columns = theConf->GetBlob(objectName() + "/RpcView_Columns");
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pRpcList->restoreState(Columns);

	//m_pMenu = new QMenu();
	AddPanelItemsToMenu();

	connect(theAPI, SIGNAL(RpcListUpdated(QSet<QString>, QSet<QString>, QSet<QString>)), this, SLOT(OnRpcListUpdated(QSet<QString>, QSet<QString>, QSet<QString>)));
}


CRpcView::~CRpcView()
{
	theConf->SetBlob(objectName() + "/RpcView_Columns", m_pRpcList->saveState());
}

void CRpcView::OnResetColumns()
{
	for (int i = 0; i < m_pRpcModel->columnCount(); i++)
		m_pRpcList->SetColumnHidden(i, false);
}

void CRpcView::OnColumnsChanged()
{
	m_pRpcModel->Sync(m_RpcList);
}

void CRpcView::Refresh()
{
	QTimer::singleShot(0, theAPI, SLOT(UpdateRpcList()));
}

void CRpcView::OnRpcListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed)
{
	m_RpcList = ((CWindowsAPI*)theAPI)->GetRpcTableList();
	
	m_pRpcModel->Sync(m_RpcList);
}
