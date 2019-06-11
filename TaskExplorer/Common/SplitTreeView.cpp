#include "stdafx.h"
#include "SplitTreeView.h"

CSplitTreeView::CSplitTreeView(QAbstractItemModel* pModel, QWidget *parent) : QWidget(parent)
{
	m_pModel = pModel;

	m_pMainLayout = new QHBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);


	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Horizontal);
	m_pMainLayout->addWidget(m_pSplitter);
	
#ifdef WIN32
	QStyle* pStyle = QStyleFactory::create("windows");
#endif


	// Tree
	m_pTree = new QTreeView();
	m_pTree->setItemDelegate(new QStyledItemDelegateMaxH(m_pTree->fontMetrics().height() + 3, this));

	m_pOneModel = new COneColumnModel();
	m_pOneModel->setSourceModel(m_pModel);
	m_pTree->setModel(m_pOneModel);
	//m_pTree->setModel(m_pSortProxy);

	m_pTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
#ifdef WIN32
	m_pTree->setStyle(pStyle);
#endif
	m_pTree->setSortingEnabled(true);
	

	m_pSplitter->addWidget(m_pTree);
	//m_pSplitter->setCollapsible(0, false);
	m_pSplitter->setStretchFactor(0, 0);
	//


	// List
	// Note: It would be convinient to use QTreeViewEx here but qt does not scale well when there are too many columns
	//			hence we will add and remove columns at the model level directly.
	//			This way we can get out operational CPU usage to be quite comparable with TaskInfo na other advanced task managers
	//		 Plus there are to many columns to cram them into one simple context menu :-)
	//m_pList = new QTreeViewEx();
	m_pList = new QTreeView();
	m_pList->setItemDelegate(new QStyledItemDelegateMaxH(m_pList->fontMetrics().height() + 3, this));

	m_pList->setModel(m_pModel);

	m_pList->setSelectionMode(QAbstractItemView::ExtendedSelection);
#ifdef WIN32
	m_pList->setStyle(pStyle);
#endif
	m_pList->setSortingEnabled(true);

	m_pSplitter->addWidget(m_pList);
	m_pSplitter->setCollapsible(1, false);
	m_pSplitter->setStretchFactor(1, 1);
	// 

	connect(m_pSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(OnSplitterMoved(int,int)));

	// Link selections
	//m_pTree->setSelectionModel(m_pList->selectionModel()); // todo link selection

	m_bTreeHidden = false;
	m_pList->setColumnHidden(0, true);

	// link expansion
	connect(m_pTree, SIGNAL(expanded(const QModelIndex)), this, SLOT(OnExpandTree(const QModelIndex)));
	connect(m_pTree, SIGNAL(collapsed(const QModelIndex)), this, SLOT(OnCollapseTree(const QModelIndex)));
	//connect(m_pList, SIGNAL(expanded(const QModelIndex)), this, SLOT(expand(const QModelIndex)));
	//connect(m_pList, SIGNAL(collapsed(const QModelIndex)), this, SLOT(collapse(const QModelIndex)));

	// link scrollbars
	m_pTree->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_pTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	m_pList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	connect(m_pList->verticalScrollBar(), SIGNAL(valueChanged(int)), m_pTree->verticalScrollBar(), SLOT(setValue(int)));
	connect(m_pTree->verticalScrollBar(), SIGNAL(valueChanged(int)), m_pList->verticalScrollBar(), SLOT(setValue(int)));

	connect(m_pTree, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClickedTree(const QModelIndex&)));
	connect(m_pList, SIGNAL(clicked(const QModelIndex&)), this, SIGNAL(clicked(const QModelIndex&)));

	QTimer::singleShot(0, this, [this]() {
		emit TreeResized(GetTreeWidth());
	});
}

CSplitTreeView::~CSplitTreeView()
{

}

int CSplitTreeView::GetTreeWidth() const
{
	QList<int> sizes = m_pSplitter->sizes();
	return sizes[0];
}

void CSplitTreeView::SetTreeWidth(int Width)
{
	QList<int> sizes = m_pSplitter->sizes();
	int total = sizes[0] + sizes[1];
	sizes[0] = Width;
	sizes[1] = total - Width;
}

void CSplitTreeView::OnSplitterMoved(int pos, int index)
{
	if ((pos == 0) == m_bTreeHidden)
		return;
	m_bTreeHidden = (pos == 0);

	m_pList->setColumnHidden(0, !m_bTreeHidden);

	emit TreeResized(pos);
}

void CSplitTreeView::OnExpandTree(const QModelIndex& index)
{
	m_pList->expand(m_pOneModel->mapToSource(index));
}

void CSplitTreeView::OnCollapseTree(const QModelIndex& index)
{
	m_pList->collapse(m_pOneModel->mapToSource(index));
}

void CSplitTreeView::OnClickedTree(const QModelIndex& Index)
{
	emit clicked(m_pOneModel->mapToSource(Index));
}

void CSplitTreeView::expand(const QModelIndex &index) 
{ 
	m_pTree->expand(m_pOneModel->mapFromSource(index)); 
}

void CSplitTreeView::collapse(const QModelIndex &index) 
{ 
	m_pTree->collapse(m_pOneModel->mapFromSource(index)); 
}