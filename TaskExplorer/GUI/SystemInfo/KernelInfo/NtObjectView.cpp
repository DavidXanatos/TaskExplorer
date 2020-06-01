#include "stdafx.h"
#include "../../TaskExplorer.h"
#include "NtObjectView.h"
#include "../../../../MiscHelpers/Common/Common.h"
#include "../../../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "../../../../MiscHelpers/Common/Finder.h"
#ifdef WIN32
#include "../../../API/Windows/WindowsAPI.h"		
#endif


CNtObjectView::CNtObjectView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pNtObjectModel = new CNtObjectModel();
	m_pNtObjectModel->SetUseIcons(true);
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pNtObjectModel);
	m_pSortProxy->setDynamicSortFilter(true);


	// NtObject List
	m_pNtObjectList = new QTreeViewEx();
	m_pNtObjectList->setItemDelegate(theGUI->GetItemDelegate());

	m_pNtObjectList->setModel(m_pSortProxy);

	m_pNtObjectList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pNtObjectList->setSortingEnabled(true);

	m_pNtObjectList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pNtObjectList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(theGUI, SIGNAL(ReloadPanels()), m_pNtObjectModel, SLOT(Clear()));

	m_pMainLayout->addWidget(m_pNtObjectList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

	QByteArray Columns = theConf->GetBlob(objectName() + "/NtObjectView_Columns");
	if (Columns.isEmpty())
		m_pNtObjectList->OnResetColumns();
	else
		m_pNtObjectList->restoreState(Columns);

	//m_pMenu = new QMenu();
	AddPanelItemsToMenu();
}

CNtObjectView::~CNtObjectView()
{
	theConf->SetBlob(objectName() + "/NtObjectView_Columns", m_pNtObjectList->saveState());
}

void CNtObjectView::Refresh() 
{
	m_pNtObjectModel->Refresh();
}
