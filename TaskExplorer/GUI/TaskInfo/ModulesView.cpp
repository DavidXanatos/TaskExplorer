#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ModulesView.h"
#include "../../Common/Common.h"
#include "..\Models\ModuleModel.h"
#include "..\Models\SortFilterProxyModel.h"

CModulesView::CModulesView()
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);


	// Module List
	m_pModuleModel = new CModuleModel();
	m_pModuleModel->SetTree(true);

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pModuleModel);
	m_pSortProxy->setDynamicSortFilter(true);


	m_pModuleList = new QTreeViewEx();
	m_pModuleList->setItemDelegate(new QStyledItemDelegateMaxH(m_pModuleList->fontMetrics().height() + 3, this));
	m_pModuleList->setModel(m_pSortProxy);

	m_pModuleList->setSelectionMode(QAbstractItemView::ExtendedSelection);
#ifdef WIN32
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pModuleList->setStyle(pStyle);
#endif
	m_pModuleList->setSortingEnabled(true);


	m_pMainLayout->addWidget(m_pModuleList);
	// 
}


CModulesView::~CModulesView()
{
}

void CModulesView::ShowModules(const CProcessPtr& pProcess)
{
	if(!m_pCurProcess.isNull())
		disconnect(m_pCurProcess.data(), SIGNAL(ModulesUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(OnModulesUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
	
	m_pCurProcess = pProcess;

	connect(m_pCurProcess.data(), SIGNAL(ModulesUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(OnModulesUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

	m_pCurProcess->UpdateModules();
}


void CModulesView::OnModulesUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	QMap<quint64, CModulePtr> Modules = m_pCurProcess->GetModleList();

	m_pModuleModel->Sync(Modules);

	QTimer::singleShot(100, this, [this, Added]() {
		foreach(quint64 ID, Added)
			m_pModuleList->expand(m_pSortProxy->mapFromSource(m_pModuleModel->FindIndex(ID)));
	});
}