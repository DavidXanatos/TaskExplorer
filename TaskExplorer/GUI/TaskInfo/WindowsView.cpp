#include "stdafx.h"
#include "../TaskExplorer.h"
#include "WindowsView.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinWnd.h"
#endif
#include "..\Models\WindowModel.h"
#include "..\..\Common\SortFilterProxyModel.h"


CWindowsView::CWindowsView(QWidget *parent)
	:CPanelView(parent)
{
	m_LockValue = false;

	m_pMainLayout = new QHBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);
	
	m_pWindowModel = new CWindowModel();
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pWindowModel);
	m_pSortProxy->setDynamicSortFilter(true);


	// Window List
	m_pWindowList = new QTreeViewEx();
	m_pWindowList->setItemDelegate(new CStyledGridItemDelegate(m_pWindowList->fontMetrics().height() + 3, this));

	m_pWindowList->setModel(m_pSortProxy);

	m_pWindowList->setSelectionMode(QAbstractItemView::ExtendedSelection);
#ifdef WIN32
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pWindowList->setStyle(pStyle);
#endif
	m_pWindowList->setSortingEnabled(true);

	m_pWindowList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pWindowList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	m_pMainLayout->addWidget(m_pWindowList);
	// 

	//m_pMenu = new QMenu();
	m_pBringToFront = m_pMenu->addAction(tr("Bring to front"), this, SLOT(OnWindowAction()));
	m_pHighlight = m_pMenu->addAction(tr("Highlight"), this, SLOT(OnWindowAction()));

	m_pMenu->addSeparator();
	m_pRestore = m_pMenu->addAction(tr("Restore"), this, SLOT(OnWindowAction()));
	m_pMinimize = m_pMenu->addAction(tr("Minimize"), this, SLOT(OnWindowAction()));
	m_pMaximize = m_pMenu->addAction(tr("Maximize"), this, SLOT(OnWindowAction()));
	m_pClose = m_pMenu->addAction(tr("Close"), this, SLOT(OnWindowAction()));

	m_pMenu->addSeparator();
	m_pVisible = m_pMenu->addAction(tr("Visible"), this, SLOT(OnWindowAction()));
	m_pVisible->setCheckable(true);
	m_pEnabled = m_pMenu->addAction(tr("Enabled"), this, SLOT(OnWindowAction()));
	m_pEnabled->setCheckable(true);
	m_pOpacity = new QSpinBox(this);
	m_pOpacity->setRange(0, 100);
	m_pOpacity->setSingleStep(10);
	//m_pOpacity->setSpecialValueText(tr("100"));
	m_pOpacity->setSuffix("%");
	CMenuAction* pOpacity = new CMenuAction(m_pOpacity, tr("Opacity:"));
	connect(m_pOpacity, SIGNAL(valueChanged(int)), this, SLOT(OnWindowAction()));
	m_pMenu->addAction(pOpacity);
	m_pOnTop = m_pMenu->addAction(tr("Always on top"), this, SLOT(OnWindowAction()));

	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	m_pWindowList->header()->restoreState(theConf->GetBlob(objectName() + "/WindowsView_Columns"));
	//m_pSplitter->restoreState(theConf->GetBlob(objectName() + "/WindowsView_Splitter"));
}


CWindowsView::~CWindowsView()
{
	theConf->SetBlob(objectName() + "/WindowsView_Columns", m_pWindowList->header()->saveState());
	//theConf->SetBlob(objectName() + "/WindowsView_Splitter",m_pSplitter->saveState());
}

void CWindowsView::ShowWindows(const CProcessPtr& pProcess)
{
	if(!m_pCurProcess.isNull())
		disconnect(m_pCurProcess.data(), SIGNAL(WindowsUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(OnWindowsUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
	
	m_pCurProcess = pProcess;

	connect(m_pCurProcess.data(), SIGNAL(WindowsUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(OnWindowsUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

	m_pCurProcess->UpdateWindows();
}

void CWindowsView::OnWindowsUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	QMap<quint64, CWndPtr> Windows = m_pCurProcess->GetWindowList();

	m_pWindowModel->Sync(Windows);


	QTimer::singleShot(100, this, [this, Added]() {
		foreach(quint64 ID, Added)
			m_pWindowList->expand(m_pSortProxy->mapFromSource(m_pWindowModel->FindIndex(ID)));
	});
}

void CWindowsView::OnMenu(const QPoint &point)
{
	int Count = 0;
	int Visible = 0;
	int Enabled = 0;
	int OnTop = 0;
	int Opacity = 255;
	foreach(const QModelIndex& Index, m_pWindowList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CWndPtr pWindow = m_pWindowModel->GetWindow(ModelIndex);
		if (pWindow.isNull())
			continue;

		Count++;
		if (pWindow->IsVisible())
			Visible++;
		if (pWindow->IsEnabled())
			Enabled++;
		if (pWindow->IsAlwaysOnTop())
			OnTop++;

		Opacity = pWindow->GetWindowAlpha(); // just take the last one
	}

	m_LockValue = true;
	
	m_pBringToFront->setEnabled(Count == 1);
	m_pHighlight->setEnabled(Count == 1);

	m_pRestore->setEnabled(Count >= 1);
	m_pMinimize->setEnabled(Count >= 1);
	m_pMaximize->setEnabled(Count >= 1);
	m_pClose->setEnabled(Count >= 1);

	m_pVisible->setEnabled(Count >= 1);
	m_pVisible->setChecked(Visible > 0);
	m_pEnabled->setEnabled(Count >= 1);
	m_pEnabled->setChecked(Enabled > 0);
	m_pOpacity->setEnabled(Count == 1);
	m_pOpacity->setValue(Opacity*100/255);
	m_pOnTop->setEnabled(Count != 1);
	m_pOnTop->setChecked(OnTop > 0);
	
	m_LockValue = false;

	CPanelView::OnMenu(point);
}


void CWindowsView::OnWindowAction()
{
	if (m_LockValue)
		return;

	// QList<STATUS> Errors;
	foreach(const QModelIndex& Index, m_pWindowList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CWndPtr pWindow = m_pWindowModel->GetWindow(ModelIndex);
		if (!pWindow.isNull())
		{
			STATUS Status = OK;
			if (sender() == m_pBringToFront)
				Status = pWindow->BringToFront();
			else if (sender() == m_pHighlight)
				Status = pWindow->Highlight();

			else if (sender() == m_pRestore)
				Status = pWindow->Restore();
			else if (sender() == m_pMinimize)
				Status = pWindow->Minimize();
			else if (sender() == m_pMaximize)
				Status = pWindow->Maximize();
			else if (sender() == m_pClose)
				Status = pWindow->Close();


			else if (sender() == m_pVisible)
				Status = pWindow->SetVisible(m_pVisible->isChecked());
			else if (sender() == m_pEnabled)
				Status = pWindow->SetEnabled(m_pEnabled->isChecked());
			else if (sender() == m_pOpacity)
				Status = pWindow->SetWindowAlpha(m_pOpacity->value() * 255 / 100);
			else if (sender() == m_pOnTop)
				Status = pWindow->SetAlwaysOnTop(m_pOnTop->isChecked());

			//if(Status.IsError())
			//	Errors.append(Status);
		}
	}

	//CTaskExplorer::CheckErrors(Errors);
}