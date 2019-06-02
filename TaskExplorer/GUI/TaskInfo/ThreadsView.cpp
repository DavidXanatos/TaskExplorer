#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ThreadsView.h"
#include "../../Common/Common.h"
#include "..\Models\ThreadModel.h"
#include "..\Models\SortFilterProxyModel.h"


CThreadsView::CThreadsView()
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pFilterWidget = new QWidget();
	m_pFilterWidget->setMinimumHeight(32);
	m_pMainLayout->addWidget(m_pFilterWidget);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);
	m_pMainLayout->addWidget(m_pSplitter);

	// Thread List
	m_pThreadModel = new CThreadModel();
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pThreadModel);
	m_pSortProxy->setDynamicSortFilter(true);

	m_pThreadList = new QTreeViewEx();
	m_pThreadList->setItemDelegate(new QStyledItemDelegateMaxH(m_pThreadList->fontMetrics().height() + 3, this));

	m_pThreadList->setModel(m_pSortProxy);

	m_pThreadList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pThreadList->setSortingEnabled(true);

	m_pSplitter->addWidget(m_pThreadList);

	connect(m_pThreadList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	// 

	// Stack List
	m_pStackList = new QTreeWidgetEx();
	m_pStackList->setItemDelegate(new QStyledItemDelegateMaxH(m_pStackList->fontMetrics().height() + 3, this));
	m_pStackList->setHeaderLabels(tr("#|Name|Stack address|Frame address|Control address|Return address|Stack parameters").split("|"));

	m_pStackList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pStackList->setSortingEnabled(false);

	m_pSplitter->addWidget(m_pStackList);
	// 
}


CThreadsView::~CThreadsView()
{
}

void CThreadsView::ShowThreads(const CProcessPtr& pProcess)
{
	m_pCurProcess = pProcess;

	m_pCurProcess->UpdateThreads();

	QMap<quint64, CThreadPtr> Threads = m_pCurProcess->GetThreadList();

	m_pThreadModel->Sync(Threads);

	if(!m_pCurThread.isNull())
		m_pCurThread->TraceStack();
}

void CThreadsView::OnClicked(const QModelIndex& Index) 
{
	m_pCurThread = m_pThreadModel->GetThread(m_pSortProxy->mapToSource(Index));

	if (!m_pCurThread.isNull()) {
		disconnect(this, SLOT(OnStackTraced(const CStackTracePtr&)));

		connect(m_pCurThread.data(), SIGNAL(StackTraced(const CStackTracePtr&)), this, SLOT(OnStackTraced(const CStackTracePtr&)));

		m_pCurThread->TraceStack();
	}
}

void CThreadsView::OnStackTraced(const CStackTracePtr& StackTrace)
{
	int i = 0;
	for (; i < StackTrace->GetCount(); i++)
	{
		auto& StackFrame = StackTrace->GetFrame(i);
		
		QTreeWidgetItem* pItem;
		if (i >= m_pStackList->topLevelItemCount())
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(eStack, Qt::UserRole, i);
			pItem->setText(eStack, QString::number(i));
			m_pStackList->addTopLevelItem(pItem);
		}
		else
			pItem = m_pStackList->topLevelItem(i);

		pItem->setText(eName, StackFrame.Symbol);
		pItem->setText(eStackAddress, "0x" + QString::number(StackFrame.StackAddress, 16));
		pItem->setText(eFrameAddress, "0x" + QString::number(StackFrame.FrameAddress, 16));
		pItem->setText(eControlAddress, "0x" + QString::number(StackFrame.PcAddress, 16));
		pItem->setText(eReturnAddress, "0x" + QString::number(StackFrame.ReturnAddress, 16));
		pItem->setText(eStackParameter, tr("0x%1 0x%2 0x%3 0x%4").arg(StackFrame.Params[0], 0, 16).arg(StackFrame.Params[1], 0, 16).arg(StackFrame.Params[2], 0, 16).arg(StackFrame.Params[3], 0, 16));
	}

	for (; i < m_pStackList->topLevelItemCount(); )
		delete m_pStackList->topLevelItem(i);
}