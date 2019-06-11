#include "stdafx.h"
#include "TaskExplorer.h"
#include "ProcessTree.h"
#include "../Common/Common.h"
#include "Models\ProcessModel.h"
#include "Models\SortFilterProxyModel.h"

CProcessTree::CProcessTree()
{
	m_ExpandAll = false;

	m_pMainLayout = new QHBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pProcessModel = new CProcessModel();
	//connect(m_pProcessModel, SIGNAL(CheckChanged(quint64, bool)), this, SLOT(OnCheckChanged(quint64, bool)));
	//connect(m_pProcessModel, SIGNAL(Updated()), this, SLOT(OnUpdated()));
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pProcessModel);
	m_pSortProxy->setDynamicSortFilter(true);


	m_pProcessList = new CSplitTreeView(m_pSortProxy);
	
	connect(m_pProcessList, SIGNAL(TreeResized(int)), this, SLOT(OnTreeResized(int)));

	m_pProcessList->header()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pProcessList->header(), SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnShowHeaderMenu(const QPoint &)));

	m_pMenu = new QMenu(this);

	m_pMainLayout->addWidget(m_pProcessList);
	// 

//#ifdef _DEBUG
	/*for (int i = 1; i < m_pProcessList->colorCount(); i++)
		m_pProcessList->setColumnHidden(i, true);*/

	m_pProcessModel->setColumn(CProcessModel::ePID, true);
	m_pProcessModel->setColumn(CProcessModel::eCPU, true);
	m_pProcessModel->setColumn(CProcessModel::eIO_TotalRate, true);
	m_pProcessModel->setColumn(CProcessModel::ePrivateBytes, true);
	//m_pProcessModel->setColumn(CProcessModel::ePriorityClass, true);
	//m_pProcessModel->setColumn(CProcessModel::eGDI_Handles, true);
	//m_pProcessModel->setColumn(CProcessModel::eUSER_Handles, true);
	m_pProcessModel->setColumn(CProcessModel::eStartTime, true);
	//m_pProcessModel->setColumn(CProcessModel::eIO_ReadRate, true);
	//m_pProcessModel->setColumn(CProcessModel::eIO_WriteRate, true);
	//m_pProcessModel->setColumn(CProcessModel::eIO_OtherRate, true);
	//m_pProcessModel->setColumn(CProcessModel::eReceiveRate, true);
	//m_pProcessModel->setColumn(CProcessModel::eSendRate, true);
	//m_pProcessModel->setColumn(CProcessModel::eReadRate, true);
	//m_pProcessModel->setColumn(CProcessModel::eWriteRate, true);
//#endif

	connect(m_pProcessList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));


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

void CProcessTree::OnTreeResized(int Width)
{
	if (m_pProcessModel->IsTree() != (Width > 0))
	{
		m_pProcessModel->SetTree(Width > 0);

		if (Width > 0)
			m_ExpandAll = true;
	}
}

void CProcessTree::OnShowHeaderMenu(const QPoint &point)
{
	if(m_Columns.isEmpty())
	{
		for(int i=0; i < m_pProcessModel->maxColumn(); i++)
		{
			QAction* pAction = new QAction(m_pProcessModel->getColumn(i), m_pMenu);
			pAction->setCheckable(true);
			connect(pAction, SIGNAL(triggered()), this, SLOT(OnHeaderMenu()));
			m_pMenu->addAction(pAction);
			m_Columns[pAction] = i;
		}
	}

	for(QMap<QAction*, int>::iterator I = m_Columns.begin(); I != m_Columns.end(); I++)
		I.key()->setChecked(m_pProcessModel->testColumn(I.value()));

	m_pMenu->popup(QCursor::pos());	
}

void CProcessTree::OnHeaderMenu()
{
	QAction* pAction = (QAction*)sender();
	int Column = m_Columns.value(pAction, -1);
	m_pProcessModel->setColumn(Column, pAction->isChecked());
}


void CProcessTree::OnProcessListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	QMap<quint64, CProcessPtr> ProcessList = theAPI->GetProcessList();

	m_pProcessModel->Sync(ProcessList);

	// If we are dsplaying a tree than always auto expand new items
	if (m_pProcessModel->IsTree())
	{
		QTimer::singleShot(100, this, [this, Added]() {
			if (m_ExpandAll)
			{
				m_ExpandAll = false;
				m_pProcessList->expandAll();
			}
			else
			{
				foreach(quint64 PID, Added) {
					m_pProcessList->expand(m_pSortProxy->mapFromSource(m_pProcessModel->FindIndex(PID)));
				}
			}
		});
	}
}

void CProcessTree::OnClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	emit ProcessClicked(pProcess);
}