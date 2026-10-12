#include "stdafx.h"
#include "TaskExplorer.h"
#include "DesktopActions.h"
#include "TaskStrings.h"
#include "ProcessTree.h"
#include "../API/Cluster.h"
#include "../API/RemoteApi.h"
#include "../../MiscHelpers/Common/Common.h"
#include "Models/ProcessModel.h"
#include "../API/SystemAPI.h"
#ifdef WIN32
// Only for the two dialogs below that are not yet portable.
#include "WsWatchDialog.h"
#include "WaitChainDialog.h"
#endif
#include "../API/MemDumper.h"
#include "../../MiscHelpers/Common/ProgressDialog.h"
#include "TaskInfo/TaskInfoWindow.h"
#include "RunAsDialog.h"
#include "../../MiscHelpers/Common/Finder.h"
#include "PersistenceConfig.h"
//#include "../../MiscHelpers/Common/qzlib.h"
#include "Filters/ProcessFilterModel.h"

CProcessTree::CProcessTree(QWidget *parent)
	: CTaskView(parent)
{
	m_ExpandAll = false;
	m_ExpandLocal = true;

	m_bQuickRefreshPending = false;

	this->ForceColumn(CProcessModel::eProcess);

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	m_pProcessModel = new CProcessModel();
	m_pProcessModel->SetUseDescr(theConf->GetInt("Options/ShowProcessDescr", 1));
	//connect(m_pProcessModel, SIGNAL(CheckChanged(quint64, bool)), this, SLOT(OnCheckChanged(quint64, bool)));
	//connect(m_pProcessModel, SIGNAL(Updated()), this, SLOT(OnUpdated()));

	m_pSortProxy = new CProcessFilterModel(this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pProcessModel);
	m_pSortProxy->setDynamicSortFilter(true);


	//
	// Restored before the first refresh, so the first list the user sees is
	// already grouped the way they left it rather than regrouping a moment
	// later.
	//
	m_pProcessModel->SetMultiUser(theConf->GetBool("Options/MultiUser", false));
	m_pProcessModel->SetMultiMachine(CCluster::ClusterModeWanted());

	m_pProcessList = new CSplitTreeView(m_pSortProxy);
	
	connect(m_pProcessList, SIGNAL(MenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	//
	// The two branch menus. Built once here rather than per click, so that the
	// actions keep their identity and can be enabled and disabled in place.
	//
	m_pMenuSystem = NULL;

	m_pMachineMenu = new QMenu(this);

	//
	// The reason an entry is greyed out has to be readable, or a disabled item
	// is indistinguishable from a broken one.
	//
	m_pMachineMenu->setToolTipsVisible(true);

	m_pMachineRefresh = m_pMachineMenu->addAction(MakeActionIcon(":/Actions/Reset"), tr("Refresh"), this, SLOT(OnMachineRefresh()));
	m_pMachineMenu->addSeparator();

	//
	// The same three things the Tasks menu offers, aimed at the machine the
	// branch stands for. The Computer submenu is inserted per popup rather than
	// kept here, because it is theGUI's own menu and only belongs on this menu
	// while the branch is the local machine - see OnMenu.
	//
	m_pMachineRun = m_pMachineMenu->addAction(MakeActionIcon(":/Actions/Run"), tr("Run..."), this, SLOT(OnMachineRun()));
	m_pMachineRunAs = m_pMachineMenu->addAction(MakeActionIcon(":/Actions/RunAs"), tr("Run as..."), this, SLOT(OnMachineRunAs()));
	m_pMachineSeparator = m_pMachineMenu->addSeparator();

	m_pMachineDisconnect = m_pMachineMenu->addAction(MakeActionIcon(":/Actions/Disconnect"), tr("Disconnect"), this, SLOT(OnMachineDisconnect()));
	m_pMachineForget = m_pMachineMenu->addAction(MakeActionIcon(":/Actions/Remove"), tr("Remove from the machine list"), this, SLOT(OnMachineForget()));

	m_pUserMenu = new QMenu(this);
	m_pUserMenu->setToolTipsVisible(true);
	m_pUserRunAs = m_pUserMenu->addAction(tr("Run as this user..."), this, SLOT(OnUserRunAs()));

	connect(m_pProcessList, SIGNAL(TreeEnabled(bool)), this, SLOT(OnTreeEnabled(bool)));

	m_pProcessList->GetView()->header()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pProcessList->GetView()->header(), SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnHeaderMenu(const QPoint &)));

	m_pHeaderMenu = new QMenu(this);

	m_pProcessList->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	m_pProcessList->GetTree()->setItemDelegate(theGUI->GetItemDelegate());

	connect(m_pProcessModel, SIGNAL(ToolTipCallback(const QVariant&, QString&)), this, SLOT(OnToolTipCallback(const QVariant&, QString&)), Qt::DirectConnection);

	connect(theGUI, SIGNAL(ReloadPanels()), this, SLOT(OnClear()));

	m_pMainLayout->addWidget(m_pProcessList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

#ifndef _DEBUG
	connect(m_pProcessList->GetView()->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(OnUpdateHistory()));
	connect(m_pProcessList->GetView(), SIGNAL(expanded(const QModelIndex &)), this, SLOT(OnUpdateHistory()));
#endif


	//m_pMenu = new QMenu();
	m_pShowProperties = m_pMenu->addAction(tr("Properties"), this, SLOT(OnShowProperties()));
	m_pOpenPath = m_pMenu->addAction(tr("Open Path"), this, SLOT(OnProcessAction()));
	m_pViewPE = m_pMenu->addAction(tr("View PE info"), this, SLOT(OnProcessAction()));
	//
	// PE is the Windows executable format and the viewer is peview.exe, so the
	// entry only makes sense against a Windows target.
	//
	m_pViewPE->setVisible(theSystem->GetOsType() == CSystemAPI::eOsWindows);
	m_pMenu->addSeparator();

	m_pStop = m_pMenu->addAction(tr("Stop"), this, SLOT(OnProcessAction()));

	AddTaskItemsToMenu();

	m_pFreeze = m_pMenu->addAction(tr("Freeze"), this, SLOT(OnProcessAction()));
	m_pUnFreeze = m_pMenu->addAction(tr("UnFreeze"), this, SLOT(OnProcessAction()));

	//QAction*				m_pTerminateTree;

	m_pMenu->addSeparator();
	m_pWindowMenu = m_pMenu->addMenu(tr("Main Window"));
		m_pBringInFront = m_pWindowMenu->addAction(tr("Bring in front"), this, SLOT(OnProcessAction()));
		m_pWindowMenu->addSeparator();
		m_pRestore = m_pWindowMenu->addAction(tr("Restore"), this, SLOT(OnProcessAction()));
		m_pMinimize = m_pWindowMenu->addAction(tr("Minimize"), this, SLOT(OnProcessAction()));
		m_pMaximize = m_pWindowMenu->addAction(tr("Maximize"), this, SLOT(OnProcessAction()));
		m_pWindowMenu->addSeparator();
		m_pClose = m_pWindowMenu->addAction(tr("Close (WM_CLOSE)"), this, SLOT(OnProcessAction()));
		m_pQuit = m_pWindowMenu->addAction(tr("Quit (WM_QUIT)"), this, SLOT(OnProcessAction()));

	m_pMiscMenu = m_pMenu->addMenu(tr("Miscellaneous"));
		// Windows power throttling; shown when the *target* is Windows - see OnMenu.
		m_pEfficiency = m_pMiscMenu->addAction(tr("Efficiency Mode"), this, SLOT(OnProcessAction()));
		m_pEfficiency->setCheckable(true);

		m_pExecRequired = m_pMiscMenu->addAction(tr("Execution Required"), this, SLOT(OnProcessAction()));
		m_pExecRequired->setCheckable(true);

		m_pRunAsThis = m_pMiscMenu->addAction(tr("Run as this User"), this, SLOT(OnRunAsThis()));
		m_pDumpMenu = m_pMiscMenu->addMenu(tr("Create Crash Dump"));
			m_pMinimalDump = m_pDumpMenu->addAction(tr("Minimal"), this, SLOT(OnCrashDump()));
			m_pLimitedDump = m_pDumpMenu->addAction(tr("Limited"), this, SLOT(OnCrashDump()));
			m_pNormalDump = m_pDumpMenu->addAction(tr("Normal"), this, SLOT(OnCrashDump()));
			m_pFullDump = m_pDumpMenu->addAction(tr("Full"), this, SLOT(OnCrashDump()));
			//m_pCustomDump = m_pDumpMenu->addAction(tr("Custom"), this, SLOT(OnCrashDump()));

		m_pDebug = m_pMiscMenu->addAction(tr("Debug"), this, SLOT(OnProcessAction()));
		m_pDebug->setCheckable(true);
		m_pReduceWS = m_pMiscMenu->addAction(tr("Reduce Working Set"), this, SLOT(OnProcessAction()));
#ifdef WIN32	// the two that need a dialog this viewer does not have off Windows
		m_pWatchWS = m_pMiscMenu->addAction(tr("Working Set Watch"), this, SLOT(OnWsWatch()));
		m_pWCT = m_pMiscMenu->addAction(tr("Wait Chain Traversal"), this, SLOT(OnWCT()));
#endif
		//m_pVirtualization = m_pMiscMenu->addAction(tr("Virtualization"), this, SLOT(OnProcessAction()));
		//m_pVirtualization->setCheckable(true);
		m_pMiscMenu->addSeparator();
		m_pCritical = m_pMiscMenu->addAction(tr("Critical Process Flag"), this, SLOT(OnProcessAction()));
		m_pCritical->setCheckable(true);
		// todo: xxxx si
		//m_pProtected = m_pMiscMenu->addAction(tr("Protected Process"), this, SLOT(OnProcessAction()));
		//m_pProtected->setCheckable(true);

	m_pPermissions = m_pMenu->addAction(tr("Permissions"), this, SLOT(OnPermissions()));

	m_pMenu->addSeparator();

	m_pPreset = m_pMenu->addAction(tr("Persistent Preset"), this, SLOT(OnPresetAction()));
	m_pPreset->setCheckable(true); 

	AddPriorityItemsToMenu(eProcess);

	AddPanelItemsToMenu(m_pMenu);



	//connect(m_pProcessList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	//connect(m_pProcessList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));
	connect(m_pProcessList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnShowProperties()));
	connect(m_pProcessList, SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));
	connect(m_pProcessList, SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(OnSelectionChanged(QItemSelection,QItemSelection)));

	connect(theSystem.data(), SIGNAL(ProcessListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), SLOT(OnProcessListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

	//
	// The saved layout is a QHeaderView::saveState() blob keyed by column
	// index, so it only means anything against the EColumns order it was
	// written with. Reordering the enum silently scrambles it - a saved
	// "Working set" would come back as whatever now sits at that index - so the
	// layout carries the order's version and is discarded when it does not
	// match.
	//
	// Bump this whenever EColumns is reordered or entries are inserted anywhere
	// but the end.
	//
	const int ColumnLayoutVersion = 2;

	QByteArray Columns;
	if (theConf->GetInt("MainWindow/ProcessTree_ColumnsVersion", 1) == ColumnLayoutVersion)
		Columns = theConf->GetBlob("MainWindow/ProcessTree_Columns");

	if (Columns.isEmpty())
		OnResetColumns();
	else
	{
		m_pProcessList->restoreState(Columns);

		m_pProcessModel->SetColumnEnabled(0, true);
		for (int i = 1; i < m_pProcessModel->columnCount(); i++)
			m_pProcessModel->SetColumnEnabled(i, !m_pProcessList->GetView()->isColumnHidden(i));
	}

	m_PlotBackground = Qt::white;
	if(theGUI->GetTheme()->IsDarkTheme())
		m_PlotBackground = Qt::black;

	//connect(m_pProcessList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	OnColumnsChanged();
}

CProcessTree::~CProcessTree()
{
	theConf->SetBlob("MainWindow/ProcessTree_Columns", m_pProcessList->saveState());
	theConf->SetValue("MainWindow/ProcessTree_ColumnsVersion", 2);
}

void CProcessTree::OnResetColumns()
{
	for (int i = 1; i < m_pProcessModel->columnCount(); i++)
		m_pProcessList->GetView()->setColumnHidden(i, true);

	//
	// The default set.
	//
	// Every column now exists against every target, so the defaults have to be
	// ones that mean something on any of them - a Linux target would otherwise
	// open with three permanently blank object-count columns. The Windows-only
	// and Linux-only extras stay one click away in the header menu.
	//
	// Ordered so that the widest, least structured column is last: a command
	// line has no bound, and anywhere but the end it pushes everything else
	// off screen.
	//
	static const int DefaultColumns[] =
	{
		CProcessModel::ePID,
		CProcessModel::eDescription,	// how anyone identifies an unfamiliar process
		CProcessModel::eUserName,
		CProcessModel::eStatus,
		CProcessModel::eCPU,
		CProcessModel::eWorkingSet,		// closest thing to "in memory"
		CProcessModel::eDisk_TotalRate,
		CProcessModel::eNet_TotalRate,
		CProcessModel::eThreads,
		CProcessModel::eHandles,
		CProcessModel::eStartTime,		// refreshes less often than up time
		CProcessModel::eCommandLine,
	};

	for (size_t i = 0; i < sizeof(DefaultColumns) / sizeof(DefaultColumns[0]); i++)
		m_pProcessList->GetView()->setColumnHidden(DefaultColumns[i], false);

	m_pProcessModel->SetColumnEnabled(0, true);
	for (int i = 1; i < m_pProcessModel->columnCount(); i++)
		m_pProcessModel->SetColumnEnabled(i, !m_pProcessList->GetView()->isColumnHidden(i));
}

void CProcessTree::OnColumnsChanged()
{
	// automatically set the setting based on wether the columns are checked or not

	/*theConf->SetValue("Options/GPUStatsGetPerProcess", 
		m_pProcessModel->IsColumnEnabled(CProcessModel::eGPU_History)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eVMEM_History)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eGPU_Usage)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eGPU_Shared)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eGPU_Dedicated)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eGPU_Adapter)
	);*/

	theConf->SetValue("Options/MonitorTokenChange", 
		m_pProcessModel->IsColumnEnabled(CProcessModel::eElevation)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eVirtualized)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eIntegrity)
	);

	//
	// Recomputed here and applied on the next refresh - see
	// OnProcessListUpdated, which is the one place that runs for every machine
	// including one that connects later. See API_REQ_FIELDS.
	//
	m_WantedFields = m_pProcessModel->GetWantedFields();

	QuickRefresh();
}

void CProcessTree::QuickRefresh()
{
	if (m_bQuickRefreshPending)
		return;
	m_bQuickRefreshPending = true;
	QTimer::singleShot(250, this, SLOT(OnQuickRefresh()));
}

void CProcessTree::OnQuickRefresh()
{
	m_bQuickRefreshPending = false;

	OnProcessListUpdated(QSet<quint64>(), QSet<quint64>(), QSet<quint64>());
}

void CProcessTree::OnTreeEnabled(bool bEnable)
{
	if (m_pProcessModel->IsTree() != bEnable)
	{
		m_pProcessModel->SetTree(bEnable);

		//
		// Also when the tree is turned *off* while grouping by user, because
		// the account branches remain and a collapsed set of them shows
		// nothing at all.
		//
		if (bEnable || m_pProcessModel->IsMultiUser())
			m_ExpandAll = true;
	}
}

void CProcessTree::OnClear()
{
	m_pProcessModel->SetUseDescr(theConf->GetInt("Options/ShowProcessDescr", 1));
	m_pProcessModel->Clear();


	QColor PlotBackground = Qt::white;
	if(theGUI->GetTheme()->IsDarkTheme())
		PlotBackground = Qt::black;

	if(m_PlotBackground != PlotBackground)
	{
		m_PlotBackground = PlotBackground;

		foreach(CHistoryGraph* pGraph, m_CPU_Graphs)
			pGraph->deleteLater();
		m_CPU_Graphs.clear();

		foreach(CHistoryGraph* pGraph, m_MEM_Graphs)
			pGraph->deleteLater();
		m_MEM_Graphs.clear();

		foreach(CHistoryGraph* pGraph, m_IO_Graphs)
			pGraph->deleteLater();
		m_IO_Graphs.clear();

		foreach(CHistoryGraph* pGraph, m_NET_Graphs)
			pGraph->deleteLater();
		m_NET_Graphs.clear();

		foreach(CHistoryGraph* pGraph, m_GPU_Graphs)
			pGraph->deleteLater();
		m_GPU_Graphs.clear();

		foreach(CHistoryGraph* pGraph, m_VMEM_Graphs)
			pGraph->deleteLater();
		m_VMEM_Graphs.clear();
	}
}

void CProcessTree::AddHeaderSubMenu(QMenu* m_pHeaderMenu, const QString& Label, int from, int to)
{
	QMenu* pSubMenu = m_pHeaderMenu->addMenu(Label);

	for(int i = from; i <= to; i++)
	{
		QCheckBox *checkBox = new QCheckBox(m_pProcessModel->GetColumHeader(i), pSubMenu);
		connect(checkBox, SIGNAL(stateChanged(int)), this, SLOT(OnHeaderMenu()));
		QWidgetAction *pAction = new QWidgetAction(pSubMenu);
		pAction->setDefaultWidget(checkBox);
		pSubMenu->addAction(pAction);

		m_Columns[checkBox] = i;
	}
}

void CProcessTree::OnHeaderMenu(const QPoint &point)
{
	if(m_Columns.isEmpty())
	{
		//for(int i = 1; i < m_pProcessModel->MaxColumns(); i++)
		for(int i = CProcessModel::ePID; i <= CProcessModel::eCommandLine; i++)
		{
			QCheckBox *checkBox = new QCheckBox(m_pProcessModel->GetColumHeader(i), m_pHeaderMenu);
			connect(checkBox, SIGNAL(stateChanged(int)), this, SLOT(OnHeaderMenu()));
			QWidgetAction *pAction = new QWidgetAction(m_pHeaderMenu);
			pAction->setDefaultWidget(checkBox);
			m_pHeaderMenu->addAction(pAction);

			m_Columns[checkBox] = i;
		}

		m_pHeaderMenu->addSeparator();
		//
		// One submenu per enum group, in enum order. The ranges have to track
		// EColumns: each is the group's first and last entry.
		//
		AddHeaderSubMenu(m_pHeaderMenu, tr("CPU"), CProcessModel::eCPU, CProcessModel::eCyclesDelta);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Memory"), CProcessModel::ePrivateBytes, CProcessModel::eTLS);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Disk I/O"), CProcessModel::eDisk_TotalRate, CProcessModel::eWriteRate);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Network I/O"), CProcessModel::eNet_TotalRate, CProcessModel::eSendRate);
		AddHeaderSubMenu(m_pHeaderMenu, tr("File I/O"), CProcessModel::eIO_TotalRate, CProcessModel::eIO_OtherRate);
		AddHeaderSubMenu(m_pHeaderMenu, tr("GPU"), CProcessModel::eGPU_Usage, CProcessModel::eGPU_Adapter);
		m_pHeaderMenu->addSeparator();
		AddHeaderSubMenu(m_pHeaderMenu, tr("Objects"), CProcessModel::eHandles, CProcessModel::eUSER_Handles);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Scheduling"), CProcessModel::ePriorityClass, CProcessModel::eAffinity);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Lifetime"), CProcessModel::eRunningTime, CProcessModel::ePowerThrottling);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Graphs"), CProcessModel::eCPU_History, CProcessModel::eVMEM_History);
		m_pHeaderMenu->addSeparator();
		AddHeaderSubMenu(m_pHeaderMenu, tr("File Info"), CProcessModel::eFileName, CProcessModel::eFileSize);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Security"), CProcessModel::eIntegrity, CProcessModel::eOomScoreAdj);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Platform"), CProcessModel::ePID_LXSS, CProcessModel::eInotifyWatches);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Other"), CProcessModel::eArchitecture, CProcessModel::eStartKey);


		m_pHeaderMenu->addSeparator();
		QAction* pAction = m_pHeaderMenu->addAction(tr("Reset columns"));
		connect(pAction, SIGNAL(triggered()), this, SLOT(OnResetColumns()));
	}

	for(QMap<QCheckBox*, int>::iterator I = m_Columns.begin(); I != m_Columns.end(); I++)
		I.key()->setChecked(m_pProcessModel->IsColumnEnabled(I.value()));

	m_pHeaderMenu->popup(QCursor::pos());	
}

void CProcessTree::OnHeaderMenu()
{
	QCheckBox *checkBox = (QCheckBox*)sender();
	int Column = m_Columns.value(checkBox, -1);

	m_pProcessList->GetView()->setColumnHidden(Column, !checkBox->isChecked());
	m_pProcessModel->SetColumnEnabled(Column, checkBox->isChecked());
	OnColumnsChanged();
}

void CProcessTree::OnProcessListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	if (!theGUI->isVisible() || theGUI->windowState().testFlag(Qt::WindowMinimized))
		return;

	//
	// Every machine in view - which is not the same as every machine connected.
	//
	// With the machine layer on that is all of them, each under its own branch.
	// With it off there is no branch to tell them apart, so the tree shows the
	// one machine the window is about and the others stay connected but
	// undrawn. Merging them into one flat list would put two machines' processes
	// side by side with nothing saying which was which.
	//
	// CCluster::GetSystems() answers with the single local system when no
	// cluster exists, so this is the same code path in every case - which is the
	// point: there is one model underneath and the settings only change how it
	// is drawn.
	//
	// The map is keyed by SProcessUID, which is unique on one machine and not
	// across several - two machines can easily hold the same pid at the same
	// creation time. Only the model's node ids have to be globally unique, and
	// those come from CAbstractInfo::GetObjectUid, which is a counter in this
	// process. So a collision here costs a row, and only in the rare case; a
	// collision there would merge two machines' processes.
	//
	//
	// Said to every connected machine, not only the drawn ones: they are all
	// still being refreshed. SetWantedFields returns at once when nothing
	// changed, which is every round but the few after a column is toggled.
	//
	foreach(const CSystemPtr& pSystem, CCluster::GetSystems())
	{
		if (!pSystem.isNull())
			pSystem->SetWantedFields(m_WantedFields);
	}

	QList<QMap<SProcessUID, CProcessPtr> > Lists;
	m_Processes.clear();

	QList<CSystemPtr> Systems;
	if (m_pProcessModel->IsMultiMachine())
		Systems = CCluster::GetSystems();
	else
		Systems.append(CCluster::GetActiveSystem());

	foreach(const CSystemPtr& pSystem, Systems)
	{
		if (pSystem.isNull())
			continue;

		QMap<SProcessUID, CProcessPtr> List = pSystem->GetProcessMap();
		Lists.append(List);

		//
		// And a merged copy for the menus below, which only ever walk the
		// values. A key collision there costs nothing; in the model it would
		// cost a row, which is why the model gets the lists unmerged.
		//
		m_Processes.insert(List);
	}

	Added = m_pProcessModel->Sync(Lists);

	//
	// The local machine's branch, opened the first time it is drawn.
	//
	// Not folded into the expand-all above: that one is about a tree that has
	// just been rebuilt and applies to every row, and it is off in list mode,
	// where machine branches exist just the same and would be just as shut.
	//
	if (m_ExpandLocal && m_pProcessModel->IsMultiMachine())
	{
		QTimer::singleShot(100, this, [this]() {
			const QModelIndex Index = m_pSortProxy->mapFromSource(m_pProcessModel->FindMachineBranch(theSystem.data()));
			if (Index.isValid())
			{
				m_ExpandLocal = false;
				m_pProcessList->expand(Index);
			}
		});
	}

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

	OnUpdateHistory();
}

void CProcessTree::OnExpandAll()
{
	m_pProcessList->expandAll();
}

void CProcessTree::SetTree(bool bSet)
{
	m_pProcessList->SetTree(bSet);
}

bool CProcessTree::IsMultiUser() const
{
	return m_pProcessModel->IsMultiUser();
}

bool CProcessTree::IsMultiMachine() const
{
	return m_pProcessModel->IsMultiMachine();
}

void CProcessTree::SetMultiMachine(bool bSet)
{
	if (m_pProcessModel->IsMultiMachine() == bSet)
		return;

	m_pProcessModel->SetMultiMachine(bSet);

	OnProcessListUpdated(QSet<quint64>(), QSet<quint64>(), QSet<quint64>());
	m_ExpandAll = true;
}

void CProcessTree::SetMultiUser(bool bSet)
{
	if (m_pProcessModel->IsMultiUser() == bSet)
		return;

	m_pProcessModel->SetMultiUser(bSet);
	theConf->SetValue("Options/MultiUser", bSet);

	//
	// The model threw everything away, so nothing is on screen until the next
	// refresh - which is a second away and would look like a stall. Rebuild
	// now, and expand, because a collapsed set of user branches shows nothing
	// at all and looks broken rather than grouped.
	//
	OnProcessListUpdated(QSet<quint64>(), QSet<quint64>(), QSet<quint64>());
	m_ExpandAll = true;
}

void CProcessTree::OnShowProperties()
{
	CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow(GetSelectedProcesses<CProcessPtr>());
	pTaskInfoWindow->show();
}

void CProcessTree::OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);

	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);

	//
	// The system panels follow the selection: a machine or an account branch
	// names its machine outright, a process names the one it was observed on.
	//
	// A selection that is neither - the row was cleared, or the tree is being
	// rebuilt - leaves the panels where they are rather than snapping back to
	// the local machine, because every refresh briefly has no current row and
	// the panels would flicker between two machines once a second.
	//
	// The strong reference is kept in a local of its own rather than folded into
	// the branch case: a branch answers with a raw pointer it owns, a process
	// answers with one that has to be held to stay valid, and mixing the two in
	// a ternary would drop the second the moment the expression ended.
	//
	const CSystemPtr pOwn = pProcess.isNull() ? CSystemPtr() : pProcess->GetSystem();
	CSystemAPI* pSystem = pProcess.isNull() ? m_pProcessModel->GetBranchSystem(ModelIndex) : pOwn.data();
	if (pSystem)
		CCluster::SetViewSystem(pSystem);

	emit ProcessClicked(pProcess);
}

void CProcessTree::OnSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected)
{
	emit ProcessesSelected(GetSelectedProcesses<CProcessPtr>());
}

QList<CTaskPtr> CProcessTree::GetSelectedTasks() 
{
	return GetSelectedProcesses<CTaskPtr>(); 
}

void CProcessTree::OnToolTipCallback(const QVariant& ID, QString& ToolTip)
{
	//
	// Through the model, not through theSystem.
	//
	// The id is the node's, and a node id is CAbstractInfo::GetObjectUid - a
	// counter in this process, unique across every connected machine. It is not
	// a pid, so GetProcessByID would answer with whatever process happened to
	// carry that number, or with nothing. Asking the model also means a machine
	// or account branch answers null and simply has no tooltip.
	//
	CProcessPtr pProcess = m_pProcessModel->GetProcess(m_pProcessModel->FindIndex(ID));
	if (pProcess.isNull())
		return;

	//
	// Nothing is written for a value the target did not answer.
	//
	// A tooltip is built by appending lines, and an empty value appended is
	// still a line - an indent with nothing after it. It seldom showed on
	// Windows, where the description, the version and the company all come out
	// of one version resource and a binary carrying any of them usually carries
	// all three. Linux has no such resource, so every process drew two blank
	// lines under its path, and a kernel thread - no command line and no
	// executable on disk - drew a tooltip that was almost entirely empty.
	//
	QStringList InfoLines;

	QString CommandLine = pProcess->GetCommandLineStr();
	if (!CommandLine.isEmpty())
	{
		// This is necessary because the tooltip control seems to use some kind of O(n^9999) word-wrapping algorithm.
		for (int i = 100; i < CommandLine.length(); i += 101)
			CommandLine.insert(i, "\n");
		InfoLines.append(CommandLine);
	}

	CModulePtr pModule = pProcess->GetModuleInfo();

	//
	// Gathered before the heading, so that "File:" is written only when there
	// is something to write under it.
	//
	{
		QStringList FileLines;

		const QString FileName = pProcess->GetFileName();
		if (!FileName.isEmpty())
			FileLines.append(tr("    %1").arg(FileName));

		if (pModule)
		{
			//
			// The description and the version read as one line where both are
			// answered, and as whichever one is answered where only one is.
			//
			const QString Description = pModule->GetFileInfo("Description");
			const QString Version = pModule->GetFileInfo("FileVersion");
			const QString Named = (Description + " " + Version).trimmed();
			if (!Named.isEmpty())
				FileLines.append(tr("    %1").arg(Named));

			const QString Company = pModule->GetFileInfo("CompanyName");
			if (!Company.isEmpty())
				FileLines.append(tr("    %1").arg(Company));
		}

		if (!FileLines.isEmpty())
		{
			InfoLines.append(tr("File:"));
			InfoLines.append(FileLines);
		}
	}

	// Whatever the backend can say about what this process is actually hosting.
	InfoLines.append(::GetToolTipLines(pProcess));

    // Notes
    {
		QStringList Notes;

		QString SandBoxName = pProcess->GetSandBoxName();
        if (!SandBoxName.isEmpty())
			Notes.append(tr("    Sandboxed in: %1").arg(SandBoxName));

		if (pModule)
		{
			switch (pModule->GetVerifyResult())
			{
			case CModuleInfo::VrTrusted:
			{
				QString Signer = pModule->GetVerifySignerName();
				if (Signer.isEmpty())
					Notes.append(tr("    Signer: %1").arg(Signer));
				else
					Notes.append(tr("    Signed"));
				break;
			}
			case CModuleInfo::VrNoSignature:
				Notes.append(tr("    Signature invalid"));
				break;
			}

			if (pModule->IsPacked())
			{
				Notes.append(tr("    Image is probably packed (%1 imports over %2 modules).").arg(pModule->GetImportFunctions()).arg(pModule->GetImportModules()));
			}
		}

		quint64 ConsoleHostProcessId = pProcess->GetConsoleHostId();
        if (ConsoleHostProcessId & ~3)
        {
            QString Description = tr("Console host");
            if (ConsoleHostProcessId & 2)
                Description = tr("Console application");
			quint64 PID = (ConsoleHostProcessId & ~3);
			CProcessPtr pHostProcess;
			if (CSystemPtr pHostSystem = pProcess->GetSystem())
				pHostProcess = pHostSystem->GetProcessByID(PID);
			Notes.append(tr("    %1: %2 (%3)").arg(Description).arg(pHostProcess ? ::LocalizeName(pHostProcess->GetName()) : tr("Non-existent process")).arg(PID));
        }

		QString PackageFullName = pProcess->GetPackageName();
        if (!PackageFullName.isEmpty())
			Notes.append(tr("    Package name: %1").arg(PackageFullName));

        if (pProcess->IsNetProcess())
            Notes.append(tr("    Process is managed (.NET)."));
        if (pProcess->IsElevated())
            Notes.append(tr("    Process is elevated."));
        if (pProcess->IsImmersiveProcess())
            Notes.append(tr("    Process is a Modern UI app."));
        if (pProcess->IsInJob())
            Notes.append(tr("    Process is in a job."));
        if (pProcess->IsWoW64())
            Notes.append(tr("    Process is 32-bit (WOW64)."));

        if (!Notes.isEmpty())
        {
			InfoLines.append(tr("Notes:"));
			InfoLines.append(Notes);
        }
    }

	ToolTip = InfoLines.join("\n");
}

void CProcessTree::OnMenu(const QPoint &point)
{
	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);

	//
	// A branch row gets its own menu and nothing else. Answered before the
	// process menu is prepared at all, because none of what follows applies.
	//
	QString Key;
	switch (m_pProcessModel->GetRowKind(ModelIndex, &Key))
	{
		case CProcessModel::eMachineRow:
		{
			m_pMenuSystem = m_pProcessModel->GetBranchSystem(ModelIndex);

			//
			// The local machine is always there and cannot be dropped; only a
			// connection can. Refreshing on demand is worth having for a remote
			// one that is being watched at a slower cadence than the timer.
			//
			const bool bRemote = m_pMenuSystem && !m_pMenuSystem->IsLocal();
			m_pMachineDisconnect->setEnabled(bRemote);
			m_pMachineDisconnect->setVisible(bRemote);
			m_pMachineForget->setVisible(bRemote);
			m_pMachineSeparator->setVisible(bRemote);
			m_pMachineRefresh->setEnabled(m_pMenuSystem != NULL);

			//
			// Starting a program on the branch's machine, which is now a thing
			// that can happen: the dialogs take the machine they were opened
			// for and the request goes there.
			//
			m_pMachineRun->setEnabled(m_pMenuSystem != NULL);
			m_pMachineRunAs->setEnabled(m_pMenuSystem != NULL);

			//
			// theGUI's own Computer and Users submenus, borrowed for the length
			// of one popup. Not copies: CTaskExplorer::OnComputerAction
			// identifies the action by sender(), and both slots act on whatever
			// machine the menu was handed - which is what the argument here
			// says, and what makes offering them on a remote branch safe rather
			// than the worst kind of wrong.
			//
			QMenu* pComputer = theGUI->GetComputerMenu(m_pMenuSystem);
			m_pMachineMenu->removeAction(pComputer->menuAction());
			m_pMachineMenu->insertMenu(m_pMachineSeparator, pComputer);

			QMenu* pUsers = theGUI->GetUsersMenu(m_pMenuSystem);
			m_pMachineMenu->removeAction(pUsers->menuAction());
			m_pMachineMenu->insertMenu(m_pMachineSeparator, pUsers);

			m_pMachineMenu->popup(QCursor::pos());
			return;
		}
		case CProcessModel::eUserRow:
		{
			m_MenuUserKey = Key;

			//
			// The name as shown, so that whatever the menu says matches the
			// branch it was opened on.
			//
			m_MenuUserName = m_pProcessModel->data(ModelIndex, Qt::DisplayRole).toString();
			m_pUserRunAs->setText(tr("Run as %1...").arg(m_MenuUserName));

			//
			// The machine the account branch hangs under, for the same reason
			// as above: running something as this user has to happen where the
			// branch is, and only the local machine can be asked yet.
			//
			//
			// The machine the account branch hangs under: running something as
			// this user has to happen where the branch is, and now it can.
			//
			m_pMenuSystem = m_pProcessModel->GetBranchSystem(ModelIndex);
			m_pUserRunAs->setEnabled(m_pMenuSystem != NULL);

			m_pUserMenu->popup(QCursor::pos());
			return;
		}
		default:
			break;
	}

	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	
	QModelIndexList selectedRows = m_pProcessList->selectedRows();

	QList<CWndPtr> Windows;
	bool HasService = false;
	bool EfficiencyMode = false;
	bool ExecRequired = false;
	int Frozen = 0;

	foreach(const QModelIndex& Index, m_pProcessList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CProcessPtr pCurProcess = m_pProcessModel->GetProcess(ModelIndex);

		//
		// A machine or account branch is a row with no process behind it, so
		// every one of these would dereference nothing. Skipped rather than
		// refused: selecting a branch and some processes is a reasonable thing
		// to do, and the processes in that selection still count.
		//
		if (pCurProcess.isNull())
			continue;

		Windows.append(pCurProcess->GetWindows());
		if (pCurProcess->IsServiceProcess())
			HasService = true;

		//
		// These three asked pProcess - the row under the cursor - rather than
		// the row being examined, so with more than one selected they answered
		// about the same process every time round.
		//
		if (pCurProcess->IsPowerThrottled())
			EfficiencyMode = true;
		if (pCurProcess->IsExecutionRequired())
			ExecRequired = true;
		if (pCurProcess->IsFrozen())
			Frozen++;
	}

	m_pStop->setVisible(HasService);

	//
	// Rows that are actually processes, not just rows. A selection of nothing
	// but machine or account branches would otherwise offer Properties and open
	// a window about nothing.
	//
	m_pShowProperties->setEnabled(!GetSelectedProcesses<CProcessPtr>().isEmpty());

	m_pWindowMenu->setEnabled(!Windows.isEmpty());

	m_pBringInFront->setEnabled(Windows.count() == 1);
	m_pClose->setEnabled(!HasService && !Windows.isEmpty());
	m_pQuit->setEnabled(!Windows.isEmpty());
	
	//
	// Everything below asks *the machine the selection is on*, not the machine
	// this viewer runs on.
	//
	// That is the whole of this rule: an action is greyed out where the target
	// could never support it - Windows against Linux - and left enabled
	// everywhere else, where it goes through and returns an error if it cannot
	// be done. A viewer that hid every action it had not implemented yet would
	// be indistinguishable from one watching a machine that cannot do them.
	//
	// The local machine answers for itself, so this is the same code path as
	// before when nothing is connected.
	//
	const CSystemPtr pViewed = pProcess.isNull() ? CCluster::GetViewSystem() : pProcess->GetSystem();
	CSystemAPI* pTarget = pViewed.data();
	const bool bWinTarget = pTarget && pTarget->GetOsType() == CSystemAPI::eOsWindows;

	//
	// Freezing is offered only where the system has a per-process notion of it.
	// The nearest Linux equivalent, the cgroup v2 freezer, acts on a whole
	// cgroup - freezing the cgroup of a desktop-launched application would take
	// the whole session with it, which is not what clicking "Freeze" on one row
	// asks for. The per-process equivalent there is SIGSTOP/SIGCONT, which is
	// what Suspend and Resume already do.
	//
	const bool bCanFreeze = pTarget && pTarget->HasCapability(CSystemAPI::eCapProcessFreeze);
	m_pFreeze->setVisible(bCanFreeze && selectedRows.count() > Frozen);
	m_pUnFreeze->setVisible(bCanFreeze && Frozen > 0);

	m_pPreset->setEnabled(selectedRows.count() == 1);
	m_pPreset->setChecked(!pProcess.isNull() && !pProcess->GetPresets().isNull());

	//
	// Offered only where the target has the notion; enabled only when
	// something is selected.
	//
	m_pEfficiency->setVisible(bWinTarget);
	m_pEfficiency->setEnabled(selectedRows.count() > 0);
	m_pEfficiency->setChecked(EfficiencyMode);

	m_pExecRequired->setVisible(bWinTarget);
	m_pExecRequired->setEnabled(selectedRows.count() > 0);
	m_pExecRequired->setChecked(ExecRequired);

#ifdef WIN32	// the dialogs, not the actions - see the note in the header
	m_pWatchWS->setVisible(bWinTarget);
	m_pWatchWS->setEnabled(selectedRows.count() == 1);

	//
	// The exception to the rule above, and the reason the rule is worth
	// stating: this one is hidden rather than left to fail.
	//
	// CWaitChainDialog does not go through the API at all. It hands the
	// selected pid and tid to the Win32 wait chain API *on the machine this
	// viewer runs on*, so pointed at another machine it does not come back
	// empty - it comes back about whatever local process happens to hold that
	// pid, which is an answer to a question nobody asked and looks like an
	// answer to the one they did.
	//
	// Wrong is not the same as unimplemented, and only wrong is worth hiding.
	//
	const bool bLocalTarget = pTarget && pTarget->IsLocal();
	m_pWCT->setVisible(bWinTarget && bLocalTarget);
	m_pWCT->setEnabled(selectedRows.count() == 1);
#endif

	m_pRunAsThis->setEnabled(selectedRows.count() == 1);
	m_pDumpMenu->setEnabled(selectedRows.count() == 1);
	m_pDebug->setEnabled(selectedRows.count() == 1);
	m_pDebug->setChecked(pProcess && pProcess->HasDebugger());

	//CTokenInfoPtr pToken = pProcess ? pProcess->GetToken() : NULL;
	//m_pVirtualization->setEnabled(pToken && pToken->IsVirtualizationAllowed());
	//m_pVirtualization->setChecked(pToken && pToken->IsVirtualizationEnabled());

	m_pCritical->setVisible(bWinTarget);
	m_pCritical->setEnabled(selectedRows.count() > 0);
	m_pCritical->setChecked(!pProcess.isNull() && pProcess->IsCriticalProcess());

	// todo: xxxx si
	//m_pProtected->setEnabled(selectedRows.count() > 0);
	//m_pProtected->setChecked(pWinProcess && pProcess->GetProtection());

	//
	// Trimming a working set is a Windows operation; Linux reclaims on its own
	// and offers nothing to ask for.
	//
	m_pReduceWS->setVisible(bWinTarget);
	m_pReduceWS->setEnabled(selectedRows.count() > 0);

	//
	// A capability rather than an operating system: whether access lists can be
	// read and written is something the target reports, and it can be false on a
	// Windows machine too - see API_HS_CAPS.
	//
	const bool bCanEditSecurity = pTarget && pTarget->HasCapability(CSystemAPI::eCapSecurityEditor);
	m_pPermissions->setVisible(bCanEditSecurity);
	m_pPermissions->setEnabled(selectedRows.count() == 1);

	CTaskView::OnMenu(point);
}

void CProcessTree::OnCrashDump()
{
	CMemDumper::EDumpPreset Preset;
	if (sender() == m_pMinimalDump)
		Preset = CMemDumper::eDumpMinimal;
	else if (sender() == m_pLimitedDump)
		Preset = CMemDumper::eDumpLimited;
	else if (sender() == m_pNormalDump)
		Preset = CMemDumper::eDumpNormal;
	else if (sender() == m_pFullDump)
		Preset = CMemDumper::eDumpFull;
	/*else if (sender() == m_pCustomDump)
	{
		QMessageBox::warning(this, "TaskExplorer", "Not implemented yet.");
		return;
	}*/
	else
		return;

	const quint32 DumpType = CMemDumper::GetPresetFlags(Preset);

	//
	// A Windows target yields a minidump; a Linux one an ELF core file, which
	// is what gdb and the rest of that toolchain expect to see.
	//
	const QString DumpFilter = (theSystem->GetOsType() == CSystemAPI::eOsWindows)
		? tr("Dump files (*.dmp);;All files (*.*)")
		: tr("Core dumps (*.core);;All files (*.*)");
	QString DumpPath = QFileDialog::getSaveFileName(this, tr("Create dump"), "", DumpFilter);
	if (DumpPath.isEmpty())
		return;

	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);

	CMemDumper* pDumper = CMemDumper::New();
	STATUS status = pDumper->PrepareDump(pProcess, DumpType, DumpPath);
	if(status.IsError())
		QMessageBox::warning(this, "TaskExplorer", tr("Failed to create dump file, reason: %1").arg(CTaskExplorer::FormatError(status)));
	else
	{
		CProgressDialog Dialog(tr("Dumping %1").arg(::LocalizeName(pProcess->GetName())), this);
		Dialog.show();

		//
		// The dumper reports codes; CProgressDialog is a shared widget that
		// shows text, so the wording happens here, on the way through.
		//
		// The connects are made by pointer rather than by SIGNAL()/SLOT() name:
		// an earlier version named slots CProgressDialog does not have, and the
		// string-based form compiles either way, so the dialog silently showed
		// no progress and no explanation of a failed dump.
		//
		connect(pDumper, &CMemDumper::ProgressMessage, &Dialog, [&Dialog](const CStatus& Message, int Progress) {
			Dialog.ShowProgress(CTaskExplorer::FormatError(Message), Progress);
		});
		connect(pDumper, &CMemDumper::StatusMessage, &Dialog, [&Dialog](const CStatus& Message) {
			Dialog.ShowStatus(CTaskExplorer::FormatError(Message), (int)Message.GetStatus());
		});
		connect(&Dialog, SIGNAL(Cancel()), pDumper, SLOT(Cancel()));
		connect(pDumper, SIGNAL(finished()), &Dialog, SLOT(OnFinished()));

		connect(pDumper, SIGNAL(finished()), pDumper, SLOT(deleteLater()));

		pDumper->start();

		Dialog.exec();
	}
}

void CProcessTree::OnProcessAction()
{
	QList<STATUS> Errors;
	int Force = -1;
	foreach(const QModelIndex& Index, m_pProcessList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
		if (pProcess.isNull())
			continue;

	retry:
		STATUS Status = OK;

		if (sender() == m_pCritical)
			Status = pProcess->SetCriticalProcess(m_pCritical->isChecked(), Force == 1);
		else if (sender() == m_pEfficiency)
			Status = pProcess->SetPowerThrottled(m_pEfficiency->isChecked());
		else if (sender() == m_pExecRequired)
			Status = pProcess->SetExecutionRequired(!pProcess->IsExecutionRequired());
		else if (sender() == m_pFreeze)
			Status = pProcess->Freeze();
		else if (sender() == m_pUnFreeze)
			Status = pProcess->UnFreeze();
		else if (sender() == m_pReduceWS)
			Status = pProcess->ReduceWS();
		else if (sender() == m_pDebug)
		{
			if (m_pDebug->isChecked())
				Status = pProcess->AttachDebugger();
			else
				Status = pProcess->DetachDebugger();
		}
		else if (sender() == m_pOpenPath)
			Status = ::ExploreFile(pProcess->GetSystem().data(), pProcess->GetFileName());
		else if (sender() == m_pStop)
		{
			QMap<QString, CServicePtr> AllServices = theSystem->GetServiceList();
			foreach(const QString& ServiceName, pProcess->GetServiceList())
			{
				CServicePtr pService = AllServices[ServiceName.toLower()];
				if (pService.isNull())
					continue;
				STATUS StopStatus = pService->Stop();
				if (StopStatus.IsError())
					Errors.append(StopStatus);
			}
		}
		else if (sender() == m_pViewPE)
		{
			//
			// peview ships with TaskExplorer and reads PE files, so it is only
			// meaningful against a Windows target.
			//
			if (theSystem->GetOsType() == CSystemAPI::eOsWindows)
				QProcess::startDetached(QApplication::applicationDirPath() + "/peview.exe", QStringList(pProcess->GetFileName()));
		}
		else if (sender() == m_pClose || sender() == m_pQuit)
		{
			//
			// Close asks each window to go away; Quit asks the application to
			// exit. Where a platform draws no distinction, Quit falls back to
			// Close.
			//
			foreach(const CWndPtr& pWnd, pProcess->GetWindows())
			{
				if (pWnd.isNull())
					continue;
				STATUS WndStatus = (sender() == m_pQuit) ? pWnd->Quit() : pWnd->Close();
				if (WndStatus.IsError())
					Errors.append(WndStatus);
			}
		}
		else
		{
			CWndPtr pWnd = pProcess->GetMainWindow();
			//
			// A null window here only happens if it closed between the menu
			// opening and the action running; the entries are disabled for a
			// process that has none.
			//
			if (!pWnd.isNull())
			{
				if (sender() == m_pBringInFront)	Status = pWnd->BringToFront();
				else if (sender() == m_pRestore)	Status = pWnd->Restore();
				else if (sender() == m_pMinimize)	Status = pWnd->Minimize();
				else if (sender() == m_pMaximize)	Status = pWnd->Maximize();
			}
		}

		if (Status.IsError())
		{
			if (Status.GetStatus() == ERROR_CONFIRM)
			{
				if (Force == -1)
				{
					switch (QMessageBox("TaskExplorer", CTaskExplorer::FormatError(Status), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel | QMessageBox::Default | QMessageBox::Escape).exec())
					{
					case QMessageBox::Yes:
						Force = 1;
						goto retry;
						break;
					case QMessageBox::No:
						Force = 0;
						break;
					case QMessageBox::Cancel:
						return;
					}
				}
			}
			else
				Errors.append(Status);
		}
	}

	CTaskExplorer::CheckErrors(Errors);
}

void CProcessTree::OnPresetAction()
{
	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	if (!pProcess)
		return;
	
	if (pProcess->GetPresets().isNull())
	{
		theSystem->AddPersistentPreset(pProcess->GetFileName());
	}
	else if(!theSystem->RemovePersistentPreset(pProcess->GetFileName()))
	{
		// remove fails for wildcard entries, hence show the dialog
		CPersistenceConfig dialog;
		dialog.exec();
	}
}

void CProcessTree::OnWsWatch()
{
	//
	// Still compile-time gated: CWsWatchDialog and CWaitChainDialog are
	// themselves Windows-only classes that are not built on Linux yet. They
	// need the same treatment as the views before this can become a runtime
	// check; until then the menu entries are hidden for non-Windows targets in
	// OnMenu rather than doing nothing when clicked.
	//
#ifdef WIN32
	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	if (!pProcess)
		return;
	
	CWsWatchDialog* pWnd = new CWsWatchDialog(pProcess);
	pWnd->show();
#endif
}

void CProcessTree::OnWCT()
{
	//
	// Still compile-time gated: CWsWatchDialog and CWaitChainDialog are
	// themselves Windows-only classes that are not built on Linux yet. They
	// need the same treatment as the views before this can become a runtime
	// check; until then the menu entries are hidden for non-Windows targets in
	// OnMenu rather than doing nothing when clicked.
	//
#ifdef WIN32
	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	if (!pProcess)
		return;
	
	CWaitChainDialog* pWnd = new CWaitChainDialog(pProcess);
	pWnd->show();
#endif
}

void CProcessTree::OnRunAsThis()
{
	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	if (!pProcess)
		return;
		
	//
	// On the machine the process is on, which for a remote one is not this
	// computer - "run as the parent of that process" only means anything where
	// that process is.
	//
	CRunAsDialog* pWnd = new CRunAsDialog(pProcess->GetSystem().data(), pProcess->GetProcessId());
	pWnd->show();
}

void CProcessTree::OnPermissions()
{
	QList<CTaskPtr>	Tasks = GetSelectedTasks();
	if (Tasks.count() != 1)
		return;

	if (CProcessPtr pProcess = Tasks.first().staticCast<CProcessInfo>())
		CTaskExplorer::ShowSecurity(pProcess->GetSecurityObject(), this);
}


void CProcessTree::UpdateIndexWidget(int HistoryColumn, int CellHeight, QMap<quint64, CHistoryGraph*>& Graphs, QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> >& History)
{
	QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> > OldMap;
	m_pProcessList->StartUpdatingWidgets(OldMap, History);

	QTreeView* pView = m_pProcessList->GetView();
	const int ViewportHeight = pView->viewport()->rect().height();

	//
	// Iterate on column 0: indexBelow() walks rows from a column-0 index. Walking
	// it from a history-column index returned invalid after the first row, which
	// left every row below the top one without a graph widget.
	//
	for(QModelIndex Row = pView->indexAt(QPoint(0,0)); Row.isValid(); Row = pView->indexBelow(Row))
	{
		QModelIndex Index = Row.sibling(Row.row(), HistoryColumn);

		//
		// Cull by VERTICAL position only: stop once a row sits below the visible
		// area (every row after it is lower still). The history column usually
		// lives far to the right of a narrow tree, so its cells are horizontally
		// off-screen at rest; testing the full rect for intersection here made
		// the very first row fail and broke the loop before placing anything -
		// which is why the graph columns rendered nothing. The widget is placed
		// regardless of the column's horizontal scroll position, so it is already
		// there when the user scrolls across to it.
		//
		if (pView->visualRect(Index).top() >= ViewportHeight)
			break; // below the visible area

		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		//
		// Key the graph by the model's node id (object uid), which is what the
		// eProcess column exposes through UserRole - not the pid. OnUpdateHistory
		// stores the graphs under the same object uid; keying this lookup by pid
		// instead handed every widget a missing (null) graph, so they painted
		// nothing and the raw numeric value showed through.
		//
		quint64 NodeId = m_pProcessModel->Data(ModelIndex, Qt::UserRole, CProcessModel::eProcess).toULongLong();

		CHistoryWidget* pGraph = OldMap.take(NodeId).first;
		if (!pGraph)
		{
			QModelIndex Index = m_pSortProxy->mapFromSource(ModelIndex);

			pGraph = new CHistoryWidget(Graphs[NodeId]);
			pGraph->setFixedHeight(CellHeight);
			History.insert(NodeId, qMakePair((QPointer<CHistoryWidget>)pGraph, QPersistentModelIndex(Index)));
			pView->setIndexWidget(Index, pGraph);
		}
	}
	m_pProcessList->EndUpdatingWidgets(OldMap, History);
}

void CProcessTree::OnUpdateHistory()
{
	float Div = (theConf->GetInt("Options/LinuxStyleCPU") == 1) ? theSystem->GetCpuCount() : 1.0f;

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eCPU_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eCPU_History);

		QMap<quint64, CHistoryGraph*> Old = m_CPU_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetObjectUid(); // key by object uid, not pid: that is the model's node id (UserRole/eProcess) that UpdateIndexWidget looks graphs up by
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, m_PlotBackground, this);
				pGraph->AddValue(0, Qt::green);
				pGraph->AddValue(1, Qt::red);
				m_CPU_Graphs.insert(PID, pGraph);
			}

			STaskStats CpuStats = pProcess->GetCpuStats();

			pGraph->SetValue(0, CpuStats.CpuUsage / Div);
			pGraph->SetValue(1, CpuStats.CpuKernelUsage / Div);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys()) {
			auto graph = m_CPU_Graphs.take(TID);
			if(graph) graph->deleteLater();
		}

		UpdateIndexWidget(CProcessModel::eCPU_History, CellHeight, m_CPU_Graphs, m_CPU_History);
	}

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eGPU_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eGPU_History);

		QMap<quint64, CHistoryGraph*> Old = m_GPU_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetObjectUid(); // key by object uid, not pid: that is the model's node id (UserRole/eProcess) that UpdateIndexWidget looks graphs up by
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, m_PlotBackground, this);
				pGraph->AddValue(0, Qt::green);
				m_GPU_Graphs.insert(PID, pGraph);
			}

			SGpuStats GpuStats = pProcess->GetGpuStats();

			pGraph->SetValue(0, GpuStats.GpuTimeUsage.Usage);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys()) {
			auto graph = m_GPU_Graphs.take(TID);
			if(graph) graph->deleteLater();
		}

		UpdateIndexWidget(CProcessModel::eGPU_History, CellHeight, m_GPU_Graphs, m_GPU_History);
	}

	quint64 TotalMemoryUsed = theSystem->GetCommitedMemory();

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eMEM_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eMEM_History);

		QMap<quint64, CHistoryGraph*> Old = m_MEM_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetObjectUid(); // key by object uid, not pid: that is the model's node id (UserRole/eProcess) that UpdateIndexWidget looks graphs up by
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, m_PlotBackground, this);
				pGraph->AddValue(0, QColor("#CCFF33"));
				m_MEM_Graphs.insert(PID, pGraph);
			}

			pGraph->SetValue(0, TotalMemoryUsed ? (float)pProcess->GetWorkingSetSize() / TotalMemoryUsed : 0);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys()) {
			auto graph = m_MEM_Graphs.take(TID);
			if(graph) graph->deleteLater();
		}

		UpdateIndexWidget(CProcessModel::eMEM_History, CellHeight, m_MEM_Graphs, m_MEM_History);
	}

	//
	// GetGpuMonitor is on the base API; both backends provide one, and a
	// backend with no GPU accounting reports zero limits.
	//
	CGpuMonitor::SGpuMemory GpuMemory = theSystem->GetGpuMonitor()->GetGpuMemory();
	quint64 TotalShared = GpuMemory.SharedLimit;
	quint64 TotalDedicated = GpuMemory.DedicatedLimit;

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eVMEM_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eVMEM_History);

		QMap<quint64, CHistoryGraph*> Old = m_VMEM_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetObjectUid(); // key by object uid, not pid: that is the model's node id (UserRole/eProcess) that UpdateIndexWidget looks graphs up by
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, m_PlotBackground, this);
				pGraph->AddValue(0, QColor("#CCFF33"));
				m_VMEM_Graphs.insert(PID, pGraph);
			}

			SGpuStats GpuStats = pProcess->GetGpuStats();

			float DedicatedMemory = TotalDedicated ? (float)GpuStats.GpuDedicatedUsage / TotalDedicated : 0;
			float SharedMemory = TotalShared ? (float)GpuStats.GpuSharedUsage / TotalShared : 0;

			pGraph->SetValue(0, qMax(DedicatedMemory, SharedMemory));
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys()) {
			auto graph = m_VMEM_Graphs.take(TID);
			if(graph) graph->deleteLater();
		}

		UpdateIndexWidget(CProcessModel::eVMEM_History, CellHeight, m_VMEM_Graphs, m_VMEM_History);
	}


	SSysStats SysStats = theSystem->GetStats();
	quint64 TotalIO = SysStats.Io.ReadRate.Get() + SysStats.Io.WriteRate.Get() + SysStats.Io.OtherRate.Get();
	quint64 TotalDisk = SysStats.Disk.ReadRate.Get() + SysStats.Disk.WriteRate.Get();
	if (TotalDisk < TotalIO)
		TotalDisk = TotalIO;

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eIO_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eIO_History);

		QMap<quint64, CHistoryGraph*> Old = m_IO_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetObjectUid(); // key by object uid, not pid: that is the model's node id (UserRole/eProcess) that UpdateIndexWidget looks graphs up by
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, m_PlotBackground, this);
				pGraph->AddValue(0, Qt::green);
				pGraph->AddValue(1, Qt::red);
				pGraph->AddValue(2, Qt::blue);
				m_IO_Graphs.insert(PID, pGraph);
			}

			SProcStats IoStats = pProcess->GetStats();

			pGraph->SetValue(0, TotalDisk ? (float)qMax(IoStats.Disk.ReadRate.Get(), IoStats.Io.ReadRate.Get()) / TotalDisk : 0);
			pGraph->SetValue(1, TotalDisk ? (float)qMax(IoStats.Disk.WriteRate.Get(), IoStats.Io.WriteRate.Get()) / TotalDisk : 0);
			pGraph->SetValue(2, TotalIO ? (float)IoStats.Io.OtherRate.Get() / TotalIO : 0);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys()) {
			auto graph = m_IO_Graphs.take(TID);
			if(graph) graph->deleteLater();
		}

		UpdateIndexWidget(CProcessModel::eIO_History, CellHeight, m_IO_Graphs, m_IO_History);
	}

	quint64 TotalNet = SysStats.Net.ReceiveRate.Get() + SysStats.Net.SendRate.Get();

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eNET_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eNET_History);

		QMap<quint64, CHistoryGraph*> Old = m_NET_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetObjectUid(); // key by object uid, not pid: that is the model's node id (UserRole/eProcess) that UpdateIndexWidget looks graphs up by
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, m_PlotBackground, this);
				pGraph->AddValue(0, Qt::green);
				pGraph->AddValue(1, Qt::red);
				m_NET_Graphs.insert(PID, pGraph);
			}

			SProcStats IoStats = pProcess->GetStats();

			pGraph->SetValue(0, TotalNet ? (float)IoStats.Net.ReceiveRate.Get() / TotalNet : 0);
			pGraph->SetValue(1, TotalNet ? (float)IoStats.Net.SendRate.Get() / TotalNet : 0);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys()) {
			auto graph = m_NET_Graphs.take(TID);
			if(graph) graph->deleteLater();
		}

		UpdateIndexWidget(CProcessModel::eNET_History, CellHeight, m_NET_Graphs, m_NET_History);
	}
}


//
// ---- the branch menus ----
//

//
// One reason, written once. An entry that is off because the machine is remote
// says so on itself; one that is on carries no tooltip at all, so an empty
// tooltip is not mistaken for a missing explanation.
//
void CProcessTree::SetRemoteTodo(QAction* pAction, bool bEnabled)
{
	pAction->setEnabled(bEnabled);
	pAction->setToolTip(bEnabled ? QString() : tr("Actions on a remote machine are not supported yet."));
}

void CProcessTree::OnMachineRefresh()
{
	if (m_pMenuSystem)
		QTimer::singleShot(0, m_pMenuSystem, SLOT(UpdateAll()));
}

void CProcessTree::OnMachineDisconnect()
{
	if (!theCluster || !m_pMenuSystem)
		return;

	//
	// By name, because that is what CCluster is keyed by. Found from the system
	// rather than from the branch label, which is the host name and need not be
	// unique.
	//
	foreach(const STarget& Target, theCluster->GetTargets())
	{
		if (Target.pSystem.data() == m_pMenuSystem) {
			theCluster->Disconnect(Target.Name);
			break;
		}
	}

	m_pMenuSystem = NULL;
	OnProcessListUpdated(QSet<quint64>(), QSet<quint64>(), QSet<quint64>());
}

void CProcessTree::OnMachineForget()
{
	if (!theCluster || !m_pMenuSystem)
		return;

	QString Name;
	if (!theCluster->GetTargetInfo(m_pMenuSystem, &Name, NULL))
		return;

	if (QMessageBox::question(this, "TaskExplorer", tr("Remove %1 from the list of machines?").arg(Name),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		return;

	//
	// Disconnects first if it is up - CCluster::RemoveTarget does that itself -
	// so that "remove" needs one confirmation rather than two steps.
	//
	theCluster->RemoveTarget(Name);
	m_pMenuSystem = NULL;
	OnProcessListUpdated(QSet<quint64>(), QSet<quint64>(), QSet<quint64>());
}

//
// The three run entries on the branch menus.
//
// Each names the machine before opening the dialog. The Computer submenu does
// it too, on the way up - but the account branch has no Computer submenu, and a
// dialog that inherited whichever machine was last right-clicked would start a
// program on the wrong computer without ever saying so.
//
void CProcessTree::OnMachineRun()
{
	theGUI->SetActionSystem(m_pMenuSystem);

	// Through the meta-object, since OnRun is a private slot of the window.
	QMetaObject::invokeMethod(theGUI, "OnRun");
}

void CProcessTree::OnMachineRunAs()
{
	theGUI->SetActionSystem(m_pMenuSystem);
	QMetaObject::invokeMethod(theGUI, "OnRunAs");
}

void CProcessTree::OnUserRunAs()
{
	//
	// Straight to the ordinary Run As dialog. Pre-filling it with the account
	// the branch stands for wants an argument CTaskExplorer::OnRunAs does not
	// take yet; until it does, this at least starts from the right place
	// rather than pretending the entry is not there.
	//
	theGUI->SetActionSystem(m_pMenuSystem);

	// Through the meta-object, since OnRunAs is a private slot of the window.
	QMetaObject::invokeMethod(theGUI, "OnRunAs");
}
