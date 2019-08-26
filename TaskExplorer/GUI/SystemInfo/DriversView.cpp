#include "stdafx.h"
#include "../TaskExplorer.h"
#include "DriversView.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinDriver.h"		
#endif
#include "../../Common/SortFilterProxyModel.h"
#include "../../Common/Finder.h"

CDriversView::CDriversView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pDriverModel = new CDriverModel();
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pDriverModel);
	m_pSortProxy->setDynamicSortFilter(true);


	// Driver List
	m_pDriverList = new QTreeViewEx();
	m_pDriverList->setItemDelegate(theGUI->GetItemDelegate());

	m_pDriverList->setModel(m_pSortProxy);

	m_pDriverList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pDriverList->setSortingEnabled(true);

	m_pDriverList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pDriverList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(theGUI, SIGNAL(ReloadAll()), m_pDriverModel, SLOT(Clear()));

	m_pMainLayout->addWidget(m_pDriverList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

	QByteArray Columns = theConf->GetBlob(objectName() + "/DriversView_Columns");
	if (Columns.isEmpty())
	{
		for (int i = 0; i < m_pDriverModel->columnCount(); i++)
			m_pDriverList->SetColumnHidden(i, true);

		m_pDriverList->SetColumnHidden(CDriverModel::eDriver, false);
#ifdef WIN32
		m_pDriverList->SetColumnHidden(CDriverModel::eDescription, false);
#endif
		m_pDriverList->SetColumnHidden(CDriverModel::eBinaryPath, false);

	}
	else
		m_pDriverList->restoreState(Columns);

	//m_pMenu = new QMenu();
	AddPanelItemsToMenu();

	connect(theAPI, SIGNAL(DriverListUpdated(QSet<QString>, QSet<QString>, QSet<QString>)), this, SLOT(OnDriverListUpdated(QSet<QString>, QSet<QString>, QSet<QString>)));
}


CDriversView::~CDriversView()
{
	theConf->SetBlob(objectName() + "/DriversView_Columns", m_pDriverList->saveState());
}


void CDriversView::OnDriverListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed)
{
	QMap<QString, CDriverPtr> DriverList = theAPI->GetDriverList();
	
	m_pDriverModel->Sync(DriverList);
}
