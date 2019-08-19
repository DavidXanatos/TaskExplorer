#include "stdafx.h"
#include "../TaskExplorer.h"
#include "WindowsView.h"
#include "../../Common/Common.h"
#include "../Models/WindowModel.h"
#include "../../Common/SortFilterProxyModel.h"
#include "../../Common/Finder.h"
#ifdef WIN32
#include "../../API/Windows/ProcessHacker.h"
#include "../../API/Windows/WinWnd.h"
#undef IsMinimized
#undef IsMaximized
#endif


CWindowsView::CWindowsView(QWidget *parent)
	:CPanelView(parent)
{
	m_LockValue = false;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);
	
	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);
	m_pMainLayout->addWidget(m_pSplitter);

	m_pWindowModel = new CWindowModel();
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pWindowModel);
	m_pSortProxy->setDynamicSortFilter(true);


	// Window List
	m_pWindowList = new QTreeViewEx();
	m_pWindowList->setItemDelegate(theGUI->GetItemDelegate());

	m_pWindowList->setModel(m_pSortProxy);

	m_pWindowList->setSelectionMode(QAbstractItemView::ExtendedSelection);
#ifdef WIN32
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pWindowList->setStyle(pStyle);
#endif
	m_pWindowList->setSortingEnabled(true);

	m_pWindowList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pWindowList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(theGUI, SIGNAL(ReloadAll()), m_pWindowModel, SLOT(Clear()));

	m_pSplitter->addWidget(CFinder::AddFinder(m_pWindowList, m_pSortProxy));
	m_pSplitter->setCollapsible(0, false);
	// 



	connect(m_pWindowList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnItemSelected(const QModelIndex&)));
	connect(m_pWindowList->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)), this, SLOT(OnItemSelected(QModelIndex)));

	// Handle Details
	m_pWindowDetails = new CPanelWidget<QTreeWidgetEx>();

	m_pWindowDetails->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	((QTreeWidgetEx*)m_pWindowDetails->GetView())->setHeaderLabels(tr("Name|Value").split("|"));

	m_pWindowDetails->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pWindowDetails->GetView()->setSortingEnabled(false);

	m_pSplitter->addWidget(m_pWindowDetails);
	//m_pSplitter->setCollapsible(1, false);
	//

	setObjectName(parent->objectName());

	QByteArray Columns = theConf->GetBlob(objectName() + "/WindowsView_Columns");
	if (Columns.isEmpty())
	{
		//
	}
	else
		m_pWindowList->header()->restoreState(Columns);
	m_pSplitter->restoreState(theConf->GetBlob(objectName() + "/WindowsView_Splitter"));
	m_pWindowDetails->GetView()->header()->restoreState(theConf->GetBlob(objectName() + "/WindowsDetail_Columns"));


	//m_pMenu = new QMenu();
	m_pBringToFront = m_pMenu->addAction(tr("Bring to front"), this, SLOT(OnWindowAction()));
	m_pHighlight = m_pMenu->addAction(tr("Highlight"), this, SLOT(OnWindowAction()));

	m_pMenu->addSeparator();
	m_pRestore = m_pMenu->addAction(tr("Restore"), this, SLOT(OnWindowAction()));
	m_pMinimize = m_pMenu->addAction(tr("Minimize"), this, SLOT(OnWindowAction()));
	m_pMaximize = m_pMenu->addAction(tr("Maximize"), this, SLOT(OnWindowAction()));
	m_pClose = m_pMenu->addAction(tr("Close"), this, SLOT(OnWindowAction()));
	m_pClose->setShortcut(QKeySequence::Delete);
	m_pClose->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pClose);

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
}


CWindowsView::~CWindowsView()
{
	theConf->SetBlob(objectName() + "/WindowsView_Columns", m_pWindowList->header()->saveState());
	theConf->SetBlob(objectName() + "/WindowsView_Splitter",m_pSplitter->saveState());
	theConf->SetBlob(objectName() + "/WindowsDetail_Columns", m_pWindowDetails->GetView()->header()->saveState());
}

void CWindowsView::ShowProcess(const CProcessPtr& pProcess)
{
	if (m_pCurProcess != pProcess)
	{
		disconnect(this, SLOT(OnWindowsUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

		m_pCurProcess = pProcess;

		connect(m_pCurProcess.data(), SIGNAL(WindowsUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(OnWindowsUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
	}

	Refresh();
}

void CWindowsView::Refresh()
{
	if (!m_pCurProcess)
		return;

	QTimer::singleShot(0, m_pCurProcess.data(), SLOT(UpdateWindows()));
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
	int Normal = 0;
	int Mimimized = 0;
	int Maximized = 0;
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
		if (pWindow->IsNormal())
			Normal++;
		if (pWindow->IsMinimized())
			Mimimized++;
		if (pWindow->IsMaximized())
			Maximized++;

		Opacity = pWindow->GetWindowAlpha(); // just take the last one
	}

	m_LockValue = true;
	
	m_pBringToFront->setEnabled(Count == 1);
	m_pHighlight->setEnabled(Count == 1);

	m_pRestore->setEnabled(Normal < Count);
	m_pMinimize->setEnabled(Mimimized < Count);
	m_pMaximize->setEnabled(Maximized < Count);
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

	if (sender() == m_pClose)
	{
		if (QMessageBox("TaskExplorer", tr("Do you want to close the selected window(s)"), QMessageBox::Question, QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
			return;
	}

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

QString QRect2Str(const QRect& rect)
{
	return QString("(%1, %2) - (%3, %4) [%5x%6]").arg(rect.left()).arg(rect.top()).arg(rect.right()).arg(rect.bottom()).arg(rect.width()).arg(rect.height());
}

void CWindowsView::OnItemSelected(const QModelIndex &current)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);

	CWndPtr pWindow = m_pWindowModel->GetWindow(ModelIndex);
	if (pWindow.isNull())
		return;

	QTreeWidget* pDetails = (QTreeWidget*)m_pWindowDetails->GetView();
	// Note: we don't auto refresh this infos
	pDetails->clear();

#ifdef WIN32
	CWinWnd::SWndInfo WndInfo = pWindow.objectCast<CWinWnd>()->GetWndInfo();

	QTreeWidgetItem* pGeneral = new QTreeWidgetItem(QStringList(tr("General")));
	pDetails->addTopLevelItem(pGeneral);

	QTreeWidgetEx::AddSubItem(pGeneral, tr("AppID"), WndInfo.AppID);
	QTreeWidgetEx::AddSubItem(pGeneral, tr("Text"), WndInfo.Text);
	QTreeWidgetEx::AddSubItem(pGeneral, tr("Thread"), WndInfo.Thread);
	QTreeWidgetEx::AddSubItem(pGeneral, tr("Rectangle"), QRect2Str(WndInfo.Rect));
	QTreeWidgetEx::AddSubItem(pGeneral, tr("Normal rectangle"), QRect2Str(WndInfo.NormalRect));
	QTreeWidgetEx::AddSubItem(pGeneral, tr("Client rectangle"), QRect2Str(WndInfo.ClientRect));
	QTreeWidgetEx::AddSubItem(pGeneral, tr("Instance handle"), WndInfo.InstanceString);
	QTreeWidgetEx::AddSubItem(pGeneral, tr("Menu handle"), QString::number(WndInfo.MenuHandle));
	QTreeWidgetEx::AddSubItem(pGeneral, tr("User data"), QString::number(WndInfo.UserdataHandle));
	QTreeWidgetEx::AddSubItem(pGeneral, tr("Unicode"), WndInfo.IsUnicode ? tr("Yes") : tr("No"));
	QTreeWidgetEx::AddSubItem(pGeneral, tr("Dialog control ID"), QString::number(WndInfo.WindowId));
	QTreeWidgetEx::AddSubItem(pGeneral, tr("Font"), WndInfo.Font);
	QTreeWidgetEx::AddSubItem(pGeneral, tr("Styles"), WndInfo.Styles);
	QTreeWidgetEx::AddSubItem(pGeneral, tr("Extended styles"), WndInfo.StylesEx);


	QTreeWidgetItem* pClass = new QTreeWidgetItem(QStringList(tr("Class")));
	pDetails->addTopLevelItem(pClass);

	QTreeWidgetEx::AddSubItem(pClass, tr("Class name"), WndInfo.ClassName);
	QTreeWidgetEx::AddSubItem(pClass, tr("Atom"), QString::number(WndInfo.Atom));
	QTreeWidgetEx::AddSubItem(pClass, tr("Styles"), WndInfo.StylesClass);
	QTreeWidgetEx::AddSubItem(pClass, tr("Instance handle"), WndInfo.InstanceString2);
	QTreeWidgetEx::AddSubItem(pClass, tr("Large icon handle"), QString::number(WndInfo.hIcon));
	QTreeWidgetEx::AddSubItem(pClass, tr("Small icon handle"), QString::number(WndInfo.hIconSm));
	QTreeWidgetEx::AddSubItem(pClass, tr("Cursor handle"), QString::number(WndInfo.hCursor));
	QTreeWidgetEx::AddSubItem(pClass, tr("Background brush"), QString::number(WndInfo.hbrBackground));
	QTreeWidgetEx::AddSubItem(pClass, tr("Menu name"), QString::number(WndInfo.lpszMenuName));
	

	QTreeWidgetItem* pProps = new QTreeWidgetItem(QStringList(tr("Properties")));
	pDetails->addTopLevelItem(pProps);

	for(QMap<QString, QString>::iterator I = WndInfo.Properties.begin(); I != WndInfo.Properties.end(); ++I)
		QTreeWidgetEx::AddSubItem(pProps, I.key(), I.value());

	//QTreeWidgetItem* pPropStor = new QTreeWidgetItem(QStringList(tr("Property Storage")));
	//pDetails->addTopLevelItem(pPropStor);

	// ToDo:

	pDetails->expandAll();
#endif
}