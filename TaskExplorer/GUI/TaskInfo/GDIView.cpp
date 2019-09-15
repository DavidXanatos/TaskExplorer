#include "stdafx.h"
#include "../TaskExplorer.h"
#include "GDIView.h"
#include "../../Common/KeyValueInputDialog.h"
#include "../../Common/Finder.h"
#include "../../API/Windows/WinProcess.h"
#include "../../API/Windows/ProcessHacker.h"


CGDIView::CGDIView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	// GDI List
	m_pGDIModel = new CGDIModel();

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pGDIModel);
	m_pSortProxy->setDynamicSortFilter(true);

	m_pGDIList = new QTreeViewEx();
	m_pGDIList->setItemDelegate(theGUI->GetItemDelegate());
	
	m_pGDIList->setModel(m_pSortProxy);

	m_pGDIList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pGDIList->setSortingEnabled(true);

	m_pGDIList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pGDIList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	m_pGDIList->setColumnReset(2);
	connect(m_pGDIList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	connect(m_pGDIList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	//connect(m_pGDIList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnItemDoubleClicked(const QModelIndex&)));
	m_pMainLayout->addWidget(m_pGDIList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

	AddPanelItemsToMenu();

	m_ViewMode = eNone;
	setObjectName(parent->objectName());
	SwitchView(eSingle);
}

CGDIView::~CGDIView()
{
	SwitchView(eNone);
}

void CGDIView::SwitchView(EView ViewMode)
{
	switch (m_ViewMode)
	{
		case eSingle:	theConf->SetBlob(objectName() + "/GDIView_Columns", m_pGDIList->saveState()); break;
		case eMulti:	theConf->SetBlob(objectName() + "/GDIMultiView_Columns", m_pGDIList->saveState()); break;
	}

	m_ViewMode = ViewMode;

	QByteArray Columns;
	switch (m_ViewMode)
	{
		case eSingle:	Columns = theConf->GetBlob(objectName() + "/GDIView_Columns"); break;
		case eMulti:	Columns = theConf->GetBlob(objectName() + "/GDIMultiView_Columns"); break;
		default:
			return;
	}
	
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pGDIList->restoreState(Columns);
}

void CGDIView::OnResetColumns()
{
	for (int i = 0; i < m_pGDIModel->columnCount(); i++)
		m_pGDIList->SetColumnHidden(i, false);

	if(m_ViewMode == eSingle)
		m_pGDIList->SetColumnHidden(CGDIModel::eProcess, true);
}

void CGDIView::OnColumnsChanged()
{
	m_pGDIModel->Sync(m_GDIList);
}

void CGDIView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	if (m_Processes == Processes)
		return;

	m_Processes = Processes;

	SwitchView(m_Processes.size() > 1 ? eMulti : eSingle);

	m_GDIList.clear();

	Refresh();
}

void CGDIView::Refresh()
{
	bool bInitTimeStamp = !m_GDIList.isEmpty();

	QMap<quint64, CWinGDIPtr> OldList = m_GDIList;

	foreach(const CProcessPtr& pProcess, m_Processes)
	{
		QString ProcessName = pProcess->GetName();

		PGDI_SHARED_MEMORY gdiShared = (PGDI_SHARED_MEMORY)NtCurrentPeb()->GdiSharedHandleTable;
		USHORT processId = (USHORT)pProcess->GetProcessId();

		for (ulong i = 0; i < GDI_MAX_HANDLE_COUNT; i++)
		{
			PWSTR typeName;
			INT lvItemIndex;
			WCHAR pointer[PH_PTR_STR_LEN_1];

			PGDI_HANDLE_ENTRY handle = &gdiShared->Handles[i];

			if (handle->Owner.ProcessId != processId)
				continue;

			CWinGDIPtr pWinGDI = OldList.take(GDI_MAKE_HANDLE(i, handle->Unique));
			if (!pWinGDI)
			{
				pWinGDI = CWinGDIPtr(new CWinGDI());
				if (bInitTimeStamp)
					pWinGDI->InitTimeStamp();
				pWinGDI->InitData(i, handle, ProcessName);
				m_GDIList.insert(pWinGDI->GetHandleId(), pWinGDI);
			}
		}
	}

	foreach(quint64 HandleId, OldList.keys())
	{
		CWinGDIPtr pWinGDI = m_GDIList.value(HandleId);
		if (pWinGDI->CanBeRemoved())
			m_GDIList.remove(HandleId);
		else if (!pWinGDI->IsMarkedForRemoval())
			pWinGDI->MarkForRemoval();
	}


	m_pGDIModel->Sync(m_GDIList);
}

void CGDIView::OnMenu(const QPoint &point)
{
	CPanelView::OnMenu(point);
}
