#include "stdafx.h"
#include "../TaskExplorer.h"
#include "DnsCacheView.h"
#include "../../Common/Common.h"
#include "../../API/SystemAPI.h"
#include "../../Common/SortFilterProxyModel.h"
#include "../../Common/Finder.h"

CDnsCacheView::CDnsCacheView(bool bAll, QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pDnsModel = new CDnsModel();
	/*if (!bAll)
		m_pDnsModel->SetProcessFilter(QList<QSharedPointer<QObject> >());*/
	m_pDnsModel->SetUseIcons(true);
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pDnsModel);
	m_pSortProxy->setDynamicSortFilter(true);


	// Dns List
	m_pDnsList = new QTreeViewEx();
	m_pDnsList->setItemDelegate(theGUI->GetItemDelegate());

	m_pDnsList->setModel(m_pSortProxy);

	m_pDnsList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pDnsList->setSortingEnabled(true);

	m_pDnsList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pDnsList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(theGUI, SIGNAL(ReloadPanels()), m_pDnsModel, SLOT(Clear()));

	m_pDnsList->setColumnReset(2);
	connect(m_pDnsList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	connect(m_pDnsList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	m_pMainLayout->addWidget(m_pDnsList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

	//m_pMenu = new QMenu();
	AddPanelItemsToMenu();

	connect(theAPI, SIGNAL(DnsCacheUpdated()), this, SLOT(OnDnsCacheUpdated()));

	setObjectName(parent->objectName());
	//m_ViewMode = eNone;
	//SwitchView(bAll ? eMulti : eSingle);
	QByteArray Columns = theConf->GetBlob(objectName() + "/DnsCacheView_Columns");
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pDnsList->restoreState(Columns);
}

CDnsCacheView::~CDnsCacheView()
{
	//SwitchView(eNone);
	theConf->SetBlob(objectName() + "/DnsCacheView_Columns", m_pDnsList->saveState());
}

/*void CDnsCacheView::SwitchView(EView ViewMode)
{
	switch (m_ViewMode)
	{
		case eSingle:	theConf->SetBlob(objectName() + "/DnsCacheView_Columns", m_pDnsList->saveState()); break;
		case eMulti:	theConf->SetBlob(objectName() + "/DnsCacheMultiView_Columns", m_pDnsList->saveState()); break;
	}

	m_ViewMode = ViewMode;

	QByteArray Columns;
	switch (m_ViewMode)
	{
		case eSingle:	Columns = theConf->GetBlob(objectName() + "/DnsCacheView_Columns"); break;
		case eMulti:	Columns = theConf->GetBlob(objectName() + "/DnsCacheMultiView_Columns"); break;
		default:
			return;
	}
	
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pDnsList->restoreState(Columns);
}*/

void CDnsCacheView::OnResetColumns()
{

	for (int i = 0; i < m_pDnsModel->columnCount(); i++)
		m_pDnsList->SetColumnHidden(i, false);

	/*if (m_ViewMode == eSingle)
	{
		m_pDnsList->SetColumnHidden(CDnsModel::eProcess, true);
	}*/

	m_pDnsList->SetColumnHidden(CDnsModel::eTTL, true);
}

void CDnsCacheView::OnColumnsChanged()
{
	m_pDnsModel->Sync(m_DnsCacheList);
}

void CDnsCacheView::OnDnsCacheUpdated()
{
	m_DnsCacheList = theAPI->GetDnsEntryList();

	m_pDnsModel->Sync(m_DnsCacheList);
}


/*void CDnsCacheView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	SwitchView(Processes.size() > 1 ? eMulti : eSingle);

	QList<QSharedPointer<QObject> > processes;
	foreach(const CProcessPtr& pProcess, Processes) // todo-later: improve that
		processes.append(pProcess);

	m_pDnsModel->SetProcessFilter(processes);

	OnDnsCacheUpdated();
}*/

void CDnsCacheView::Refresh()
{
	// Note: if SmartHostNameResolution is enabled the cache is always updated
	if(!theConf->GetBool("Options/SmartHostNameResolution", false))
		theAPI->UpdateDnsCache();
}