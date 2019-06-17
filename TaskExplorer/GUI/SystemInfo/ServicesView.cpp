#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ServicesView.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinService.h"		
#endif
#include "..\Models\ServiceModel.h"
#include "..\Models\SortFilterProxyModel.h"


CServicesView::CServicesView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QHBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pServiceModel = new CServiceModel();
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pServiceModel);
	m_pSortProxy->setDynamicSortFilter(true);


	// Service List
	m_pServiceList = new QTreeViewEx();
	m_pServiceList->setItemDelegate(new QStyledItemDelegateMaxH(m_pServiceList->fontMetrics().height() + 3, this));

	m_pServiceList->setModel(m_pSortProxy);

	m_pServiceList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pServiceList->setSortingEnabled(true);

	m_pServiceList->setColumnHidden(CServiceModel::eGroupe, true);
	m_pServiceList->setColumnFixed(CServiceModel::eGroupe, true);

	m_pMainLayout->addWidget(m_pServiceList);
	// 

	connect(theAPI, SIGNAL(ServiceListUpdated(QSet<QString>, QSet<QString>, QSet<QString>)), this, SLOT(OnServiceListUpdated(QSet<QString>, QSet<QString>, QSet<QString>)));
}

CServicesView::~CServicesView()
{
}

void CServicesView::OnServiceListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed)
{
	QMap<QString, CServicePtr> ServiceList = theAPI->GetServiceList();

	m_pServiceModel->Sync(ServiceList);
}