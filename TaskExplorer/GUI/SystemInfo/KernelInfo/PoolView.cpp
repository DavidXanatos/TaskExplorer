#include "stdafx.h"
#include "../../TaskExplorer.h"
#include "PoolView.h"
#include "../../../Common/Common.h"
#include "../../../Common/SortFilterProxyModel.h"
#include "../../../Common/Finder.h"
#ifdef WIN32
#include "../../../API/Windows/WindowsAPI.h"		
#endif


CPoolView::CPoolView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pPoolModel = new CPoolModel();
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pPoolModel);
	m_pSortProxy->setDynamicSortFilter(true);


	// Pool List
	m_pPoolList = new QTreeViewEx();
	m_pPoolList->setItemDelegate(theGUI->GetItemDelegate());

	m_pPoolList->setModel(m_pSortProxy);

	m_pPoolList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pPoolList->setSortingEnabled(true);

	m_pPoolList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pPoolList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(theGUI, SIGNAL(ReloadAll()), m_pPoolModel, SLOT(Clear()));

	m_pMainLayout->addWidget(m_pPoolList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

	QByteArray Columns = theConf->GetBlob(objectName() + "/PoolView_Columns");
	if (Columns.isEmpty())
	{
		for (int i = 0; i < m_pPoolModel->columnCount(); i++)
			m_pPoolList->SetColumnHidden(i, false);
	}
	else
		m_pPoolList->restoreState(Columns);

	//m_pMenu = new QMenu();
	AddPanelItemsToMenu();

	connect(theAPI, SIGNAL(PoolListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(OnPoolListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
}


CPoolView::~CPoolView()
{
	theConf->SetBlob(objectName() + "/PoolView_Columns", m_pPoolList->saveState());
}

void CPoolView::Refresh() 
{
	QTimer::singleShot(0, theAPI, SLOT(UpdatePoolTable()));
}

void CPoolView::OnPoolListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	QMap<quint64, CPoolEntryPtr> PoolList = ((CWindowsAPI*)theAPI)->GetPoolTableList();
	
	m_pPoolModel->Sync(PoolList);
}
