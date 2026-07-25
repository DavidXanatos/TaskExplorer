#include "stdafx.h"
#include "../TaskExplorer.h"
#include "HeapView.h"
#include "../../../MiscHelpers/Common/KeyValueInputDialog.h"
#include "../../../MiscHelpers/Common/Finder.h"
#include "../../API/Windows/WinProcess.h"
#include "../../API/Windows/ProcessHacker.h"


CHeapView::CHeapView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	m_pFilterWidget = new QWidget();
	m_pMainLayout->addWidget(m_pFilterWidget);

	m_pFilterLayout = new QHBoxLayout();
	m_pFilterLayout->setContentsMargins(3, 3, 3, 3);
	m_pFilterWidget->setLayout(m_pFilterLayout);

	m_pRefresh = new QPushButton(tr("Refresh"));
	connect(m_pRefresh, SIGNAL(pressed()), this, SLOT(OnRefresh()));
	m_pFilterLayout->addWidget(m_pRefresh);

	m_pFlush = new QPushButton(tr("Flush"));
	connect(m_pFlush, SIGNAL(pressed()), this, SLOT(OnFlush()));
	m_pFilterLayout->addWidget(m_pFlush);

	m_pFilterLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));

	// Heap List
	m_pHeapModel = new CHeapModel();

	m_pSortProxy = new CSortFilterProxyModel(this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pHeapModel);
	m_pSortProxy->setDynamicSortFilter(true);

	m_pHeapList = new QTreeViewEx();
	m_pHeapList->setItemDelegate(theGUI->GetItemDelegate());
	
	m_pHeapList->setModel(m_pSortProxy);

	m_pHeapList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pHeapList->setSortingEnabled(true);

	m_pHeapList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pHeapList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(m_pHeapList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	//connect(m_pHeapList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnItemDoubleClicked(const QModelIndex&)));
	m_pMainLayout->addWidget(m_pHeapList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	QByteArray Columns = theConf->GetBlob(objectName() + "/HeapView_Columns");
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pHeapList->restoreState(Columns);
}

CHeapView::~CHeapView()
{
	theConf->SetBlob(objectName() + "/HeapView_Columns", m_pHeapList->saveState());
}

void CHeapView::OnResetColumns()
{
	for (int i = 0; i < m_pHeapModel->columnCount(); i++)
		m_pHeapList->SetColumnHidden(i, false);
}

void CHeapView::OnColumnsChanged()
{
	m_pHeapModel->Sync(m_HeapList);
}

void CHeapView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	CProcessPtr pProcess;
	if (Processes.count() > 1)
	{
		setEnabled(false);
	}
	else if(!Processes.isEmpty())
	{
		setEnabled(true);
		pProcess = Processes.first();
	}

	m_pCurProcess = pProcess;

	OnRefresh();

	m_pHeapList->expandAll();
}

void CHeapView::OnRefresh()
{
	if (!m_pCurProcess)
		return;

	m_HeapList = m_pCurProcess->GetHeapList();

	m_pHeapModel->Sync(m_HeapList);
}

void CHeapView::OnFlush()
{
	if (!m_pCurProcess)
		return;
	m_pCurProcess->FlushHeaps();
	OnRefresh();
}

void CHeapView::OnMenu(const QPoint &point)
{
	CPanelView::OnMenu(point);
}
