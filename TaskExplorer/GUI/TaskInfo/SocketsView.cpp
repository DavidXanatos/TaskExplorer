#include "stdafx.h"
#include "../TaskExplorer.h"
#include "SocketsView.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinSocket.h"		
#endif
#include "../../Common/Finder.h"


CSocketsView::CSocketsView(bool bAll, QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pSocketModel = new CSocketModel();
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pSocketModel);
	m_pSortProxy->setDynamicSortFilter(true);


	// Socket List
	m_pSocketList = new QTreeViewEx();
	m_pSocketList->setItemDelegate(theGUI->GetItemDelegate());

	m_pSocketList->setModel(m_pSortProxy);

	m_pSocketList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pSocketList->setSortingEnabled(true);

	if (!bAll)
		m_pSocketModel->SetProcessFilter(QList<CProcessPtr>());
	m_pSocketModel->SetUseIcons(true);

	m_pSocketList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pSocketList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(theGUI, SIGNAL(ReloadPanels()), m_pSocketModel, SLOT(Clear()));

	m_pSocketList->setColumnReset(2);
	connect(m_pSocketList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	connect(m_pSocketList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	m_pMainLayout->addWidget(m_pSocketList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));


	connect(theAPI, SIGNAL(SocketListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(OnSocketListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

	//m_pMenu = new QMenu();
	m_pClose = m_pMenu->addAction(tr("Close"), this, SLOT(OnClose()));
	m_pClose->setShortcut(QKeySequence::Delete);
	m_pClose->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pClose);

	AddPanelItemsToMenu();

	m_ViewMode = eNone;
	setObjectName(parent->objectName());
	SwitchView(bAll ? eMulti : eSingle);
}


CSocketsView::~CSocketsView()
{
	SwitchView(eNone);
}

void CSocketsView::SwitchView(EView ViewMode)
{
	switch (m_ViewMode)
	{
		case eSingle:	theConf->SetBlob(objectName() + "/SocketsView_Columns", m_pSocketList->saveState()); break;
		case eMulti:	theConf->SetBlob(objectName() + "/SocketsMultiView_Columns", m_pSocketList->saveState()); break;
	}

	m_ViewMode = ViewMode;

	QByteArray Columns;
	switch (m_ViewMode)
	{
		case eSingle:	Columns = theConf->GetBlob(objectName() + "/SocketsView_Columns"); break;
		case eMulti:	Columns = theConf->GetBlob(objectName() + "/SocketsMultiView_Columns"); break;
		default:
			return;
	}
	
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pSocketList->restoreState(Columns);
}

void CSocketsView::OnResetColumns()
{
	for (int i = 0; i < m_pSocketModel->columnCount(); i++)
		m_pSocketList->SetColumnHidden(i, true);

	if (m_ViewMode == eMulti)
	{
		m_pSocketList->SetColumnHidden(CSocketModel::eProcess, false);
#ifdef WIN32
		m_pSocketList->SetColumnHidden(CSocketModel::eOwnerService, false);
#endif
	}
	m_pSocketList->SetColumnHidden(CSocketModel::eProtocol, false);
	m_pSocketList->SetColumnHidden(CSocketModel::eState, false);
	m_pSocketList->SetColumnHidden(CSocketModel::eLocalAddress, false);
	m_pSocketList->SetColumnHidden(CSocketModel::eLocalPort, false);
	m_pSocketList->SetColumnHidden(CSocketModel::eRemoteAddress, false);
	m_pSocketList->SetColumnHidden(CSocketModel::eRemotePort, false);
	m_pSocketList->SetColumnHidden(CSocketModel::eSendRate, false);
	m_pSocketList->SetColumnHidden(CSocketModel::eReceiveRate, false);
}

void CSocketsView::OnColumnsChanged()
{
	m_pSocketModel->Sync(m_SocketList);
}

void CSocketsView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	SwitchView(Processes.size() > 1 ? eMulti : eSingle);

	m_pSocketModel->SetProcessFilter(Processes);

	OnSocketListUpdated(QSet<quint64>(),QSet<quint64>(),QSet<quint64>());
}

void CSocketsView::Refresh()
{
	// noting
}

void CSocketsView::OnSocketListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	m_SocketList = theAPI->GetSocketList();

	m_pSocketModel->Sync(m_SocketList);
}

void CSocketsView::OnMenu(const QPoint &point)
{
	QModelIndex Index = m_pSocketList->currentIndex();
	
	m_pClose->setEnabled(Index.isValid());
	
	CPanelView::OnMenu(point);
}

void CSocketsView::OnClose()
{
	if(QMessageBox("TaskExplorer", tr("Do you want to close the selected socket(s)"), QMessageBox::Question, QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

	QList<STATUS> Errors;
	foreach(const QModelIndex& Index, m_pSocketList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CSocketPtr pSocket = m_pSocketModel->GetSocket(ModelIndex);
		if (!pSocket.isNull())
		{
			STATUS Status = pSocket->Close();
				
			if(Status.IsError())
				Errors.append(Status);
		}
	}

	CTaskExplorer::CheckErrors(Errors);
}
