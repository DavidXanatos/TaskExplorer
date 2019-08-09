#include "stdafx.h"
#include "../TaskExplorer.h"
#include "GDIView.h"
#include "../../Common/KeyValueInputDialog.h"
#include "../../Common/Finder.h"
#include "../../API/Windows/WinProcess.h"


CGDIView::CGDIView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	// GDI List
	m_pGDIModel = new CGDIModel();

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pGDIModel);
	m_pSortProxy->setDynamicSortFilter(true);

	m_pGDIList = new QTreeViewEx();
	m_pGDIList->setItemDelegate(theGUI->GetItemDelegate());
	
	m_pGDIList->setModel(m_pSortProxy);

	m_pGDIList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pGDIList->setSortingEnabled(true);

	m_pGDIList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pGDIList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	//connect(m_pGDIList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnItemDoubleClicked(const QModelIndex&)));
	m_pMainLayout->addWidget(m_pGDIList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	m_pGDIList->header()->restoreState(theConf->GetBlob(objectName() + "/GDIView_Columns"));
}

CGDIView::~CGDIView()
{
	theConf->SetBlob(objectName() + "/GDIView_Columns", m_pGDIList->header()->saveState());
}

void CGDIView::ShowProcess(const CProcessPtr& pProcess)
{
	if (m_pCurProcess == pProcess)
		return;

	m_pCurProcess = pProcess;

	m_GDIList.clear();

	Refresh();
}

void CGDIView::Refresh()
{
	if (!m_pCurProcess)
		return;

	qobject_cast<CWinProcess*>(m_pCurProcess.data())->UpdateGDIList(m_GDIList);

	m_pGDIModel->Sync(m_GDIList);
}

void CGDIView::OnMenu(const QPoint &point)
{
	CPanelView::OnMenu(point);
}
