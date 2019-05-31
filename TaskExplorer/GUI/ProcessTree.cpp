#include "stdafx.h"
#include "TaskExplorer.h"
#include "ProcessTree.h"
#include "../Common/Common.h"
#include "Models\ProcessModel.h"
#include "Models\SortFilterProxyModel.h"


CProcessTree::CProcessTree()
{
	m_pMainLayout = new QHBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);


	m_pProcessSplitter = new QSplitter();
	m_pProcessSplitter->setOrientation(Qt::Horizontal);
	m_pMainLayout->addWidget(m_pProcessSplitter);

	m_pProcessModel = new CProcessModel();
	//connect(m_pProcessModel, SIGNAL(CheckChanged(quint64, bool)), this, SLOT(OnCheckChanged(quint64, bool)));
	//connect(m_pProcessModel, SIGNAL(Updated()), this, SLOT(OnUpdated()));
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pProcessModel);
	m_pSortProxy->setDynamicSortFilter(true);



	// Process Tree
	m_pProcessTree = new QTreeView();
	m_pProcessTree->setItemDelegate(new QStyledItemDelegateMaxH(m_pProcessTree->fontMetrics().height() + 3, this));

	/*m_pProxyModel = new CFilterProxyModel(this);
	m_pProxyModel->setSourceModel(m_pSortProxy);
	m_pProcessTree->setModel(m_pProxyModel);*/
	m_pProcessTree->setModel(m_pSortProxy);

	m_pProcessTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
#ifdef WIN32
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pProcessTree->setStyle(pStyle);
#endif
	m_pProcessTree->setSortingEnabled(true);
	

	m_pProcessSplitter->addWidget(m_pProcessTree);
	//m_pProcessSplitter->setCollapsible(0, false);
	m_pProcessSplitter->setStretchFactor(0, 0);
	//

	// Process List
	m_pProcessList = new QTreeViewEx();
	m_pProcessList->setItemDelegate(new QStyledItemDelegateMaxH(m_pProcessList->fontMetrics().height() + 3, this));

	m_pProcessList->setModel(m_pSortProxy);

	m_pProcessList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pProcessList->setSortingEnabled(true);

	m_pProcessSplitter->addWidget(m_pProcessList);
	m_pProcessSplitter->setCollapsible(1, false);
	m_pProcessSplitter->setStretchFactor(1, 1);
	// 

	connect(m_pProcessSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(OnSplitterMoved(int,int)));


	// Link selections
	m_pProcessTree->setSelectionModel(m_pProcessList->selectionModel());

	// Modify columns
	for (int i = 1; i < m_pProcessTree->colorCount(); i++)
		m_pProcessTree->setColumnHidden(i, true);

	m_bTreeHidden = false; // todo
	m_pProcessModel->SetTree(!m_bTreeHidden);
	m_pProcessList->setColumnFixed(0, true);
	m_pProcessList->setColumnHidden(0, !m_bTreeHidden);

	// link expansion
	connect(m_pProcessTree, SIGNAL(expanded(const QModelIndex)), m_pProcessList, SLOT(expand(const QModelIndex)));
	connect(m_pProcessTree, SIGNAL(collapsed(const QModelIndex)), m_pProcessList, SLOT(collapse(const QModelIndex)));
	connect(m_pProcessList, SIGNAL(expanded(const QModelIndex)), m_pProcessTree, SLOT(expand(const QModelIndex)));
	connect(m_pProcessList, SIGNAL(collapsed(const QModelIndex)), m_pProcessTree, SLOT(collapse(const QModelIndex)));

	// link kscrollbars
	m_pProcessTree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_pProcessTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	m_pProcessList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	connect(m_pProcessList->verticalScrollBar(), SIGNAL(valueChanged(int)), m_pProcessTree->verticalScrollBar(), SLOT(setValue(int)));
	connect(m_pProcessTree->verticalScrollBar(), SIGNAL(valueChanged(int)), m_pProcessList->verticalScrollBar(), SLOT(setValue(int)));

	connect(m_pProcessList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	connect(m_pProcessTree, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));

	/*m_pProcessTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pProcessTree, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));

	connect(m_pProcessTree->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(OnSelectionChanged(QItemSelection,QItemSelection)));
	connect(m_pProcessTree, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));
	*/


	connect(theAPI, SIGNAL(ProcessListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), SLOT(OnProcessListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
}

CProcessTree::~CProcessTree()
{

}

void CProcessTree::OnSplitterMoved(int pos, int index)
{
	if ((pos == 0) == m_bTreeHidden)
		return;
	m_bTreeHidden = (pos == 0);

	m_pProcessModel->SetTree(!m_bTreeHidden);
	m_pProcessList->setColumnHidden(0, !m_bTreeHidden);
}

void CProcessTree::OnProcessListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	QMap<quint64, CProcessPtr> ProcessList = theAPI->GetProcessList();

	m_pProcessModel->Sync(ProcessList);


	QTimer::singleShot(100, this, [this, Added]() {
		foreach(quint64 PID, Added)
			m_pProcessTree->expand(m_pSortProxy->mapFromSource(m_pProcessModel->FindIndex(PID)));
	});
}


void CProcessTree::OnClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	emit ProcessClicked(pProcess);
}