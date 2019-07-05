#include "stdafx.h"
#include "../TaskExplorer/GUI/TaskExplorer.h"
#include "TokenView.h"


CTokenView::CTokenView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QGridLayout();
	//m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	int row = 0;

	// Variables List
	m_TokenList = new QTreeWidgetEx();
	m_TokenList->setItemDelegate(new CStyledGridItemDelegate(m_TokenList->fontMetrics().height() + 3, this));
	m_TokenList->setHeaderLabels(tr("Name|Status|Description").split("|"));

	m_TokenList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_TokenList->setSortingEnabled(false);

	m_TokenList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_TokenList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(m_TokenList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemDoubleClicked(QTreeWidgetItem*, int)));
	m_pMainLayout->addWidget(m_TokenList);
	// 

	//m_pMenu = new QMenu();

	
	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	m_TokenList->header()->restoreState(theConf->GetBlob(objectName() + "/TokenView_Columns"));
}

CTokenView::~CTokenView()
{
	theConf->SetBlob(objectName() + "/TokenView_Columns", m_TokenList->header()->saveState());
}

void CTokenView::ShowToken(const CProcessPtr& pProcess)
{
	m_pCurProcess = pProcess;
}

void CTokenView::OnMenu(const QPoint &point)
{

	CPanelView::OnMenu(point);
}