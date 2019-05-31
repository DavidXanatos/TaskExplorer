#include "stdafx.h"
#include "../TaskExplorer.h"
#include "SocketsView.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinSocket.h"		
#endif
#include "..\Models\SocketModel.h"
#include "..\Models\SortFilterProxyModel.h"


CSocketsView::CSocketsView(bool bAll)
{
	m_pMainLayout = new QHBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pSocketModel = new CSocketModel();
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pSocketModel);
	m_pSortProxy->setDynamicSortFilter(true);


	// Socket List
	m_pSocketList = new QTreeViewEx();
	m_pSocketList->setItemDelegate(new QStyledItemDelegateMaxH(m_pSocketList->fontMetrics().height() + 3, this));

	m_pSocketList->setModel(m_pSortProxy);

	m_pSocketList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pSocketList->setSortingEnabled(true);

	if (!bAll)
	{
		m_pSocketModel->SetProcessFilter(CProcessPtr());

		//Hide unneded columns
		m_pSocketList->setColumnFixed(CSocketModel::eProcess, true);
		m_pSocketList->setColumnHidden(CSocketModel::eProcess, true);
#ifdef WIN32
		m_pSocketList->setColumnFixed(CSocketModel::eOwner, true);
		m_pSocketList->setColumnHidden(CSocketModel::eOwner, true);
#endif
	}

	m_pMainLayout->addWidget(m_pSocketList);
	// 

	connect(theAPI, SIGNAL(SocketListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), SLOT(OnSocketListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
}


CSocketsView::~CSocketsView()
{

}

void CSocketsView::OnSocketListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	QMultiMap<quint64, CSocketPtr> SocketList = theAPI->GetSocketList();

	m_pSocketModel->Sync(SocketList);
}

void CSocketsView::ShowSockets(const CProcessPtr& pProcess)
{
	m_pSocketModel->SetProcessFilter(pProcess);

	OnSocketListUpdated(QSet<quint64>(),QSet<quint64>(),QSet<quint64>());
}
