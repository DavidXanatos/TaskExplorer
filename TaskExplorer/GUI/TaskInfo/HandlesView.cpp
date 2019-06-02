#include "stdafx.h"
#include "../TaskExplorer.h"
#include "HandlesView.h"
#include "../../Common/Common.h"
#include "..\Models\HandleModel.h"
#include "..\Models\SortFilterProxyModel.h"


CHandlesView::CHandlesView()
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pFilterWidget = new QWidget();
	m_pFilterWidget->setMinimumHeight(32);
	m_pMainLayout->addWidget(m_pFilterWidget);

	// Handle List
	m_pHandleModel = new CHandleModel();
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pHandleModel);
	m_pSortProxy->setDynamicSortFilter(true);


	m_pHandleList = new QTreeViewEx();
	m_pHandleList->setItemDelegate(new QStyledItemDelegateMaxH(m_pHandleList->fontMetrics().height() + 3, this));

	m_pHandleList->setModel(m_pSortProxy);

	m_pHandleList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pHandleList->setSortingEnabled(true);

	m_pMainLayout->addWidget(m_pHandleList);
	// 

	m_pDetailsWidget = new QWidget();
	m_pDetailsLayout = new QStackedLayout();
	m_pDetailsWidget->setLayout(m_pDetailsLayout);
	m_pDetailsWidget->setMinimumHeight(32);
	m_pMainLayout->addWidget(m_pDetailsWidget);

	/*
All:
    References
    Handles
    
    Quota Paged
    Quota Virtual size

	Granted access Numeric
    
File:
		Mode
		
ALPC Port:
		Sequence Number
		Port Context
    
Mutant:
		Count
		Abandoned
		Owner
		
Section:
		Commit
		
Thread / Process: goto sub menu
		Name
		Created
		Exited
		Exit Status
		
Semaphore: Acquire/Release sub menu
		
Event: Set/Reset/Pulse sub menu
				
Token:  detail window

	*/
}


CHandlesView::~CHandlesView()
{
}

void CHandlesView::ShowHandles(const CProcessPtr& pProcess)
{
	m_pCurProcess = pProcess;

	m_pCurProcess->UpdateHandles();

	QMap<quint64, CHandlePtr> Handles = m_pCurProcess->GetHandleList();

	m_pHandleModel->Sync(Handles);
}