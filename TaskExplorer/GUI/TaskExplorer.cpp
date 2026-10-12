#include "stdafx.h"
#include "TaskExplorer.h"
#include "../../TaskCommon/Support.h"
#include "../API/Cluster.h"
#include <QInputDialog>
#include "TaskStrings.h"
#include "version.h"
#ifdef WIN32
#include "../API/Windows/WindowsAPI.h"
#include "SecurityExplorer.h"
#include "DriverWindow.h"
#include "../API/Windows/WinAdmin.h"
extern "C" {
}
#else
// For OnElevate(): relaunching through a graphical privilege escalation helper.
#include "../API/Linux/LinuxHelper.h"
#include <signal.h>
#include <errno.h>
#endif
#include "../../MiscHelpers/Common/ExitDialog.h"
#include "../../MiscHelpers/Common/HistoryGraph.h"
#include "NewService.h"
#include "SecurityDialog.h"
#include "RunDialog.h"
#include "RunAsDialog.h"
#include "ConnectDialog.h"
#include "../API/RemoteApi.h"
#include "UnlockDialog.h"
#include "../../MiscHelpers/Common/CredentialStore.h"
#include "../SVC/TaskService.h"
#include "GraphBar.h"
#include "SettingsWindow.h"
#include "CustomItemDelegate.h"
#include "Search/HandleSearch.h"
#include "Search/ModuleSearch.h"
#include "Search/MemorySearch.h"
#include "SystemInfo/SystemInfoWindow.h"
#include "../../MiscHelpers/Common/CheckableMessageBox.h"
#include "MultiErrorDialog.h"
#include "PersistenceConfig.h"
#include "Filters/ProcessFilterModel.h"
#ifdef WIN32
#include "../../MiscHelpers/Archive/Archive.h"
#include "../../MiscHelpers/Archive/ArchiveFS.h"
#endif
#include "TaskInfo/TaskInfoWindow.h"
#include "OnlineUpdater.h"
#include "API/AssemblyList.h"


QIcon g_ExeIcon;
QIcon g_DllIcon;

CTaskExplorer* theGUI = NULL;

#if defined(Q_OS_WIN)
#include <wtypes.h>
#include <QAbstractNativeEventFilter>
#include <dbt.h>

class CNativeEventFilter : public QAbstractNativeEventFilter
{
public:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	virtual bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
#else
	virtual bool nativeEventFilter(const QByteArray& eventType, void* message, long* result)
#endif
	{
		if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") 
		{
			MSG *msg = static_cast<MSG *>(message);

			//if(msg->message != 275 && msg->message != 1025)
			//	qDebug() << msg->message;
			if (msg->message == WM_NOTIFY) 
			{
				if (theSystem->HandleNativeNotify((void*)msg->lParam, result))
					return true;
				return true;
			}
			else if (msg->message == WM_DEVICECHANGE) 
			{
				switch (msg->wParam)
				{
				/*case DBT_DEVICEARRIVAL: // Drive letter added
				case DBT_DEVICEREMOVECOMPLETE: // Drive letter removed
					{
						DEV_BROADCAST_HDR* deviceBroadcast = (DEV_BROADCAST_HDR*)msg->lParam;

						if (deviceBroadcast->dbch_devicetype == DBT_DEVTYP_VOLUME)
						{
							
						}
					}
					break;*/
				case DBT_DEVNODES_CHANGED: // hardware changed
					theSystem->NotifyHardwareChanged();
					break;
				}
			}
		}
		return false;
	}
};
#endif


CViewSystemLink::CViewSystemLink(QObject* pReceiver, const char* pSignal, const char* pSlot)
	: QObject(pReceiver), m_pReceiver(pReceiver), m_Signal(pSignal), m_Slot(pSlot)
{
	connect(theGUI, SIGNAL(ViewSystemChanged()), this, SLOT(Relink()));
	Relink();
}

void CViewSystemLink::Relink()
{
	QObject* pSystem = CCluster::GetViewSystem().data();
	if (pSystem == m_pLinked)
		return;

	//
	// Through a QPointer, because the machine this was attached to may have
	// been disconnected and deleted between the two calls - which is precisely
	// when this runs.
	//
	if (!m_pLinked.isNull())
		QObject::disconnect(m_pLinked, m_Signal.constData(), m_pReceiver, m_Slot.constData());

	m_pLinked = pSystem;

	if (pSystem)
		QObject::connect(pSystem, m_Signal.constData(), m_pReceiver, m_Slot.constData());
}

CTaskExplorer::CTaskExplorer(QWidget *parent)
	: QMainWindow(parent)
{
	theGUI = this;

	//
	// Before the title is built, which asks about both.
	//
	m_bSelfContained = false;
	m_bDaemonPresent = false;
	m_pMenuSwitchMode = NULL;

	qRegisterMetaType<CStatus>("CStatus");
	qRegisterMetaType<CAssemblyListPtr>("CAssemblyListPtr");
	qRegisterMetaType<QList<QSharedPointer<QObject>>>("QList<QSharedPointer<QObject>>");
	qRegisterMetaType<CStackTracePtr>("CStackTracePtr");
	qRegisterMetaType<QHostAddress>("QHostAddress");
	qRegisterMetaType<QSet<quint64>>("QSet<quint64>");
	qRegisterMetaType<QSet<QString>>("QSet<QString>");
	qRegisterMetaType<QMap<QVariant, QVariantMap>>("QMap<QVariant, QVariantMap>");

	m_DefaultFontSize = QApplication::font().pointSizeF();

	LoadLanguage();

	CFinder::m_CaseInsensitiveIcon = QIcon(":/Actions/CaseSensitive");
	CFinder::m_RegExpStrIcon = QIcon(":/Actions/RegExp");
	CFinder::m_HighlightIcon = QIcon(":/Actions/Highlight");

	CSystemAPI::InitLocalSystem();

	UpdateTitle();

#if defined(Q_OS_WIN)
	theSystem->SetMainWindow((quint64)QWidget::winId());

    QApplication::instance()->installNativeEventFilter(new CNativeEventFilter);
#endif

	m_bExit = false;

	// a shared item deleagate for all lists
	m_pCustomItemDelegate = new CCustomItemDelegate(GetCellHeight() + 1, this);

	// Initialize progress dialog for async operations

	LoadDefaultIcons();
	InitColors();

	m_pGraphBar = NULL;

	//
	// Null until the toolbar is built, which happens after the menus that
	// UpdateMachineLabels can already be reached from.
	//
	m_pMenuClusterMode = NULL;
	m_pMachineBox = NULL;
	m_pMachineBoxAction = NULL;

	SetUITheme();

	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout(m_pMainWidget);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	m_pMainLayout->setSpacing(0);
	this->setCentralWidget(m_pMainWidget);

	m_pGraphSplitter = new QSplitter();
	m_pGraphSplitter->setOrientation(Qt::Vertical);
	m_pMainLayout->addWidget(m_pGraphSplitter);

	
	
	m_pGraphBar = new CGraphBar();
	m_pGraphSplitter->addWidget(m_pGraphBar);
	m_pGraphSplitter->setStretchFactor(0, 0);
	m_pGraphSplitter->setSizes(QList<int>() << 80); // default size of 80
	connect(m_pGraphBar, SIGNAL(Resized(int)), this, SLOT(OnGraphsResized(int)));
	m_pGraphBar->SetDarkMode(m_CustomTheme.IsDarkTheme());

	m_pMainSplitter = new QSplitter();
	m_pMainSplitter->setOrientation(Qt::Horizontal);
	m_pGraphSplitter->addWidget(m_pMainSplitter);
	m_pGraphSplitter->setStretchFactor(1, 1);

	m_pProcessTree = new CProcessTree(this);
	//m_pProcessTree->setMinimumSize(200, 200);
	m_pMainSplitter->addWidget(m_pProcessTree);
	m_pMainSplitter->setCollapsible(0, false);

	m_pPanelSplitter = new QSplitter();
	m_pPanelSplitter->setOrientation(Qt::Vertical);
	m_pMainSplitter->addWidget(m_pPanelSplitter);

	m_pSystemInfo = new CSystemInfoView();
	//m_pSystemInfo->setMinimumSize(200, 200);
	m_pPanelSplitter->addWidget(m_pSystemInfo);

	m_pTaskInfo = new CTaskInfoView();
	//m_pTaskInfo->setMinimumSize(200, 200);
	m_pPanelSplitter->addWidget(m_pTaskInfo);
	m_pPanelSplitter->setCollapsible(1, false);

	m_pPanelSplitter->setMinimumHeight(100);

	connect(m_pMainSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(OnSplitterMoved()));
	connect(m_pPanelSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(OnSplitterMoved()));

	//connect(m_pProcessTree, SIGNAL(ProcessClicked(const CProcessPtr)), m_pTaskInfo, SLOT(ShowProcess(const CProcessPtr)));
	connect(m_pProcessTree, SIGNAL(ProcessesSelected(const QList<CProcessPtr>&)), m_pTaskInfo, SLOT(ShowProcesses(const QList<CProcessPtr>&)));


	connect(theSystem.data(), SIGNAL(StatusMessage(const QString&)), this, SLOT(OnStatusMessage(const QString&)));


	m_pMenuProcess = menuBar()->addMenu(tr("&Tasks"));
	connect(m_pMenuProcess, SIGNAL(aboutToShow()), this, SLOT(OnTaskMenu()));
		m_pMenuRun = m_pMenuProcess->addAction(MakeActionIcon(":/Actions/Run"), tr("Run..."), this, SLOT(OnRun()));
		m_pMenuRun->setShortcut(QKeySequence("Ctrl+R"));
		m_pMenuRunAs = m_pMenuProcess->addAction(MakeActionIcon(":/Actions/RunAs"), tr("Run as..."), this, SLOT(OnRunAs()));
		m_pMenuRunAs->setShortcut(QKeySequence("Alt+R"));
#ifdef WIN32
		m_pMenuRunSys = m_pMenuProcess->addAction(MakeActionIcon(":/Actions/RunTI"), tr("Run as TrustedInstaller..."), this, SLOT(OnRunSys()));
		m_pMenuRunSys->setShortcut(QKeySequence("Ctrl+Alt+R"));
#endif
		m_pMenuProcess->addSeparator();

		//
		// Connecting to another machine sits under Tasks beside the other
		// things one *does*, rather than under View, because it changes what is
		// being watched and not how it is drawn.
		//
		m_pMenuConnect = m_pMenuProcess->addAction(MakeActionIcon(":/Actions/Connect"), tr("Connect to machine..."), this, SLOT(OnConnect()));
		m_pMenuDisconnect = m_pMenuProcess->addAction(MakeActionIcon(":/Actions/Disconnect"), tr("Disconnect machine..."), this, SLOT(OnDisconnect()));
		m_pMenuDisconnect->setEnabled(false);

		//
		// Greyed rather than hidden when this installation has no TaskRemote,
		// with the reason in the tooltip.
		//
		// Hidden would be tidier and is wrong: somebody who has used this
		// before, or read that it exists, would look for it and conclude the
		// build is broken. An entry that is there and says why it cannot be
		// used answers the question it raises.
		//
		if (!CRemoteLoader::IsAvailable())
		{
			m_pMenuConnect->setEnabled(false);
			m_pMenuConnect->setToolTip(tr("This installation cannot connect to other machines: %1")
				.arg(CRemoteLoader::GetError()));
			m_pMenuProcess->setToolTipsVisible(true);
		}

		//
		// Whether this machine is read directly or through its own daemon is
		// decided at startup, so changing it means starting again. Said in the
		// entry rather than implied - a menu item that silently closes the
		// window and opens another one is worse than one that says it will.
		//
		m_pMenuSwitchMode = m_pMenuProcess->addAction(QString(), this, SLOT(OnSwitchMode()));
		m_pMenuSwitchMode->setVisible(false);

		m_pMenuProcess->addSeparator();
		m_pMenuComputer = m_pMenuProcess->addMenu(MakeActionIcon(":/Actions/Computer"), tr("Computer"));
		m_pMenuUsers = m_pMenuProcess->addMenu(MakeActionIcon(":/Actions/Users"), tr("Users"));
		m_pMenuProcess->addSeparator();
#ifdef WIN32
		m_pMenuFindWnd = m_pMenuProcess->addAction(MakeActionIcon(":/Actions/Finder"), tr("Window Finder"), this, SLOT(OnWndFinder()));
		m_pMenuProcess->addSeparator();
#endif
		m_pMenuElevate = m_pMenuProcess->addAction(MakeActionIcon(":/Icons/Shield.png"), tr("Restart Elevated"), this, SLOT(OnElevate()));
		m_pMenuElevate->setVisible(!theSystem->RootAvaiable());
#ifndef WIN32
		//
		// The lighter alternative to restarting the whole GUI as root: keep this
		// process unprivileged and let a small helper do the few things that
		// genuinely need privileges - reading other users' I/O counters and open
		// files, unwinding their stacks, writing their core dumps.
		//
		// Off by default and offered as an explicit toggle rather than turning
		// itself on when something is refused, because switching it on raises an
		// authentication prompt. That has to be the answer to a question the user
		// asked, not a surprise triggered by scrolling the process list.
		//
		m_pMenuUseHelper = m_pMenuProcess->addAction(MakeActionIcon(":/Icons/Shield.png"), tr("Use Privileged Helper"), this, SLOT(OnUseHelper()));
		m_pMenuUseHelper->setCheckable(true);
		m_pMenuUseHelper->setChecked(theConf->GetBool("Options/UseTaskHelper", false));
		m_pMenuUseHelper->setVisible(!theSystem->RootAvaiable());
		m_pMenuUseHelper->setToolTip(tr("Ask an elevated helper process for the details this user is not allowed to read, instead of running all of Task Explorer as root."));
#endif
		m_pMenuExit = m_pMenuProcess->addAction(MakeActionIcon(":/Actions/Exit"), tr("Exit"), this, SLOT(OnExit()));

		
		m_pMenuLock = m_pMenuComputer->addAction(MakeActionIcon(":/Actions/Lock"), tr("Lock"), this, SLOT(OnComputerAction()));
		m_pMenuLogOff = m_pMenuComputer->addAction(MakeActionIcon(":/Actions/Logoff"), tr("Logout"), this, SLOT(OnComputerAction()));
		m_pMenuComputer->addSeparator();
		m_pMenuSleep = m_pMenuComputer->addAction(MakeActionIcon(":/Actions/StandBy"), tr("Standby"), this, SLOT(OnComputerAction()));
		m_pMenuHibernate = m_pMenuComputer->addAction(MakeActionIcon(":/Actions/Hibernate"), tr("Hibernate"), this, SLOT(OnComputerAction()));
		m_pMenuComputer->addSeparator();
		m_pMenuRestart = m_pMenuComputer->addAction(MakeActionIcon(":/Actions/Reboot"), tr("Restart"), this, SLOT(OnComputerAction()));
		m_pMenuForceRestart = m_pMenuComputer->addAction(MakeActionIcon(":/Actions/HardReboot"), tr("Force Restart"), this, SLOT(OnComputerAction()));
		m_pMenuRestartEx = m_pMenuComputer->addAction(MakeActionIcon(":/Actions/RebootMenu"), tr("Restart to Boot Menu"), this, SLOT(OnComputerAction()));
		m_pMenuComputer->addSeparator();
		m_pMenuShutdown = m_pMenuComputer->addAction(MakeActionIcon(":/Actions/Shutdown"), tr("Shutdown"), this, SLOT(OnComputerAction()));
		m_pMenuForceShutdown = m_pMenuComputer->addAction(MakeActionIcon(":/Actions/HardShutdown"), tr("Force Shutdown"), this, SLOT(OnComputerAction()));
		m_pMenuHybridShutdown = m_pMenuComputer->addAction(MakeActionIcon(":/Actions/HybridShutdown"), tr("Hybrid Shutdown"), this, SLOT(OnComputerAction()));



	m_pMenuView = menuBar()->addMenu(tr("&View"));
		m_pMenuSysTabs = m_pMenuView->addMenu(tr("System Tabs"));
		for (int i = 0; i < m_pSystemInfo->GetTabCount(); i++)
		{
			QAction* pAction = m_pMenuSysTabs->addAction(TabMenuLabel(m_pSystemInfo, i), this, SLOT(OnSysTab()));
			pAction->setCheckable(true);
			pAction->setChecked(m_pSystemInfo->IsTabVisible(i));
			m_Act2Tab[pAction] = i;
		}

/*#ifdef WIN32
		m_pMenuSysTabs->addSeparator();
		m_pMenuKernelServices = m_pMenuSysTabs->addAction(tr("Show Kernel Services"), this, SLOT(OnKernelServices()));
		m_pMenuKernelServices->setCheckable(true);
		m_pMenuKernelServices->setChecked(theConf->GetBool("MainWindow/ShowDrivers", true));
		OnKernelServices();
#endif*/

		//
		// A tab whose name belongs to the machine being looked at can be renamed
		// later - the service list is called Services or Daemons depending on
		// the target. These actions carry the same words, so they follow.
		//
		connect(m_pSystemInfo, SIGNAL(TabLabelsChanged()), this, SLOT(UpdateTabMenus()));

		m_pMenuTaskTabs = m_pMenuView->addMenu(tr("Task Tabs"));
		for (int i = 0; i < m_pTaskInfo->GetTabCount(); i++)
		{
			QAction* pAction = m_pMenuTaskTabs->addAction(TabMenuLabel(m_pTaskInfo, i), this, SLOT(OnTaskTab()));
			pAction->setCheckable(true);
			pAction->setChecked(m_pTaskInfo->IsTabVisible(i));
			m_Act2Tab[pAction] = i;
		}

		m_pMenuView->addSeparator();
		m_pMenuSystemInfo = m_pMenuView->addAction(MakeActionIcon(":/Actions/SysInfo"), tr("System Info"), this, SLOT(OnSystemInfo()));
		m_pMenuSystemInfo->setShortcut(QKeySequence("Ctrl+S"));
		m_pMenuView->addSeparator();
		m_pMenuPauseRefresh = m_pMenuView->addAction(MakeActionIcon(":/Actions/Pause"), tr("Pause Refresh"));
		m_pMenuPauseRefresh->setCheckable(true);
		m_pMenuRefreshNow = m_pMenuView->addAction(MakeActionIcon(":/Actions/Refresh"), tr("Refresh Now"), this, SLOT(UpdateAll()));
		m_pMenuResetAll = m_pMenuView->addAction(MakeActionIcon(":/Actions/Reset"), tr("Reset all Panels"), this, SLOT(ResetAll()));
		m_pMenuResetAll->setShortcut(QKeySequence::Refresh); // F5
		m_pMenuShowTree = m_pMenuView->addAction(MakeActionIcon(":/Actions/Tree"), tr("Tree/List"), this, SLOT(OnTreeButton()));
		m_pMenuShowTree->setCheckable(true);
		m_pMenuShowTree->setShortcut(QKeySequence("Ctrl+T"));
		//
		// The icons are the ones the branches themselves are drawn with - see
		// CProcessModel::data - so a button says which kind of branch it makes
		// rather than being told apart by its position.
		//
		m_pMenuMultiUser = m_pMenuView->addAction(MakeActionIcon(":/Actions/Users"), tr("Group by User"), this, SLOT(OnMultiUserButton()));
		m_pMenuMultiUser->setCheckable(true);
		m_pMenuMultiUser->setShortcut(QKeySequence("Ctrl+U"));

		//
		// Cluster mode lives here rather than in the settings, next to the other
		// switch that changes how the tree is arranged. It is not a preference
		// that is set once - it is the answer to "one machine or all of them",
		// which is asked while looking at the tree, not while in a dialog.
		//
		// Without TaskRemote it is disabled rather than hidden: the stored value
		// is left alone so that adding the module later restores the choice, and
		// a greyed switch says the feature exists and this build cannot reach it,
		// which an absent one does not - see CCluster::ClusterModeWanted.
		//
		m_pMenuClusterMode = m_pMenuView->addAction(MakeActionIcon(":/Actions/Computer"), tr("Cluster Mode"), this, SLOT(OnClusterModeButton()));
		m_pMenuClusterMode->setCheckable(true);
		m_pMenuClusterMode->setToolTip(tr("Show a branch per machine, so that several connected machines can be watched in one tree. The local machine gets a branch of its own like any other."));
		m_pMenuClusterMode->setEnabled(CRemoteLoader::IsAvailable());
		//
		// The socket lists are network views, as they are on Windows. A Linux
		// machine also has unix domain sockets - hundreds of them - and this
		// says whether they belong there too. Offered on every platform because
		// what matters is the machine being watched, not the one watching.
		//
		m_pMenuShowUnixSockets = m_pMenuView->addAction(tr("Show Unix Sockets"), this, SLOT(OnShowUnixSockets()));
		m_pMenuShowUnixSockets->setCheckable(true);
		m_pMenuShowUnixSockets->setChecked(theConf->GetBool("Options/ShowUnixSockets", false));
		m_pMenuExpandAll = m_pMenuView->addAction(MakeActionIcon(":/Actions/Expand"), tr("Expand Process Tree"), m_pProcessTree, SLOT(OnExpandAll()));
		m_pMenuExpandAll->setShortcut(QKeySequence("Ctrl+E"));
		m_pMenuView->addSeparator();
		m_pMenuFilter = m_pMenuView->addAction(MakeActionIcon(":/Actions/Filter"), tr("Filter Processes"), this, SLOT(OnViewFilter()));
		m_pMenuFilter->setCheckable(true);
		m_pMenuFilterMenu = m_pMenuView->addMenu(MakeActionIcon(":/Actions/Filter2"), tr("Select Filters"));
			m_pMenuFilterWindows = MakeActionCheck(m_pMenuFilterMenu, tr("Windows Processes"), QVariant(), true);
			connect(m_pMenuFilterWindows, SIGNAL(triggered(bool)), this, SLOT(OnViewFilter()));
			m_pMenuFilterSystem = MakeActionCheck(m_pMenuFilterMenu, tr("System Processes"), QVariant(), true);
			connect(m_pMenuFilterSystem, SIGNAL(triggered(bool)), this, SLOT(OnViewFilter()));
			// Named to match what the target calls them; the same processes either way.
			m_pMenuFilterService = MakeActionCheck(m_pMenuFilterMenu, theSystem->GetOsType() == CSystemAPI::eOsWindows
				? tr("Service Processes") : tr("Daemon Processes"), QVariant(), true);
			connect(m_pMenuFilterService, SIGNAL(triggered(bool)), this, SLOT(OnViewFilter()));
			m_pMenuFilterOther = MakeActionCheck(m_pMenuFilterMenu, tr("Processes of Other Logged-In Users"), QVariant(), true);
			connect(m_pMenuFilterOther, SIGNAL(triggered(bool)), this, SLOT(OnViewFilter()));
			m_pMenuFilterOwn = MakeActionCheck(m_pMenuFilterMenu, tr("Processes of the Current User"), QVariant(), true);
			connect(m_pMenuFilterOwn, SIGNAL(triggered(bool)), this, SLOT(OnViewFilter()));



	m_pMenuFind = menuBar()->addMenu(tr("&Find"));
		m_pMenuFindProcess = m_pMenuFind->addAction(MakeActionIcon(":/Actions/Eye"), tr("Find Hidden Processes"), this, SLOT(OnFindProcess()));
		m_pMenuFind->addSeparator();
		m_pMenuFindHandle = m_pMenuFind->addAction(MakeActionIcon(":/Actions/FindHandle"), tr("Find Handles"), this, SLOT(OnFindHandle()));
		m_pMenuFindHandle->setShortcut(QKeySequence("Ctrl+H"));
		m_pMenuFindDll = m_pMenuFind->addAction(MakeActionIcon(":/Actions/FindDLL"), tr("Find Module (dll)"), this, SLOT(OnFindDll()));
		m_pMenuFindMemory = m_pMenuFind->addAction(MakeActionIcon(":/Actions/FindString"), tr("Find String in Memory"), this, SLOT(OnFindMemory()));

	m_pMenuOptions = menuBar()->addMenu(tr("&Options"));
		m_pMenuSettings = m_pMenuOptions->addAction(MakeActionIcon(":/Actions/Settings"), tr("Settings"), this, SLOT(OnSettings()));
#ifdef WIN32
		m_pMenuDriverConf = m_pMenuOptions->addAction(MakeActionIcon(":/Actions/Driver"), tr("Driver Options"), this, SLOT(OnDriverConf()));
		m_pMenuDriverConf->setEnabled(theSystem->RootAvaiable());

		//m_pMenuUseDriver = m_pMenuOptions->addAction(tr("Use KSystemInformer"), this, SLOT(OnUseDriver()));
		//m_pMenuUseDriver->setEnabled(theSystem->RootAvaiable());
		//m_pMenuUseDriver->setCheckable(true);
		//m_pMenuUseDriver->setChecked(theConf->GetBool("OptionsKSI/KsiEnable", true));

        m_pMenuOptions->addSeparator();
        m_pMenuAutoRun = m_pMenuOptions->addAction(tr("Auto Run"), this, SLOT(OnAutoRun()));
        m_pMenuAutoRun->setCheckable(true);
        m_pMenuAutoRun->setChecked(IsAutorunEnabled());
		m_pMenuUAC = m_pMenuOptions->addAction(tr("Skip UAC"), this, SLOT(OnSkipUAC()));
		m_pMenuUAC->setCheckable(true);
		m_pMenuUAC->setEnabled(theSystem->RootAvaiable());
		m_pMenuUAC->setChecked(SkipUacRun(true));
#endif

	m_pMenuTools = menuBar()->addMenu(tr("&Tools"));
		m_pMenuServices = m_pMenuTools->addMenu(MakeActionIcon(":/Actions/Services"), tr("&Services"));
			m_pMenuCreateService = m_pMenuServices->addAction(tr("Create new Service"), this, SLOT(OnCreateService()));
			m_pMenuCreateService->setEnabled(theSystem->RootAvaiable());
			m_pMenuUpdateServices = m_pMenuServices->addAction(tr("ReLoad all Service"), this, SLOT(OnReloadService()));
#ifdef WIN32
			m_pMenuSCMPermissions = m_pMenuServices->addAction(tr("Service Control Manager Permissions"), this, SLOT(OnSCMPermissions()));
			m_pMenuSCMPermissions->setEnabled(theSystem->HasCapability(CSystemAPI::eCapSecurityEditor));

		m_pMenuFree = m_pMenuTools->addMenu(MakeActionIcon(":/Actions/FreeMem"), tr("&Free Memory"));
			m_pMenuFreeWorkingSet = m_pMenuFree->addAction(tr("Empty Working set"), this, SLOT(OnFreeMemory()));
			m_pMenuFreeModPages = m_pMenuFree->addAction(tr("Empty Modified pages"), this, SLOT(OnFreeMemory()));
			m_pMenuFreeStandby = m_pMenuFree->addAction(tr("Empty Standby std::list"), this, SLOT(OnFreeMemory()));
			m_pMenuFreePriority0 = m_pMenuFree->addAction(tr("Empty Priority 0 std::list"), this, SLOT(OnFreeMemory()));
			m_pMenuFree->addSeparator();
			m_pMenuCombinePages = m_pMenuFree->addAction(tr("Combine Pages"), this, SLOT(OnFreeMemory()));
#endif
		
		m_pMenuPersistence = m_pMenuTools->addAction(MakeActionIcon(":/Actions/Persistence"), tr("Persistence Options"), this, SLOT(OnPersistenceOptions()));
		m_pMenuPersistence->setShortcut(QKeySequence("Ctrl+P"));

		m_pMenuFlushDns = m_pMenuTools->addAction(MakeActionIcon(":/Actions/Flush"), tr("Flush Dns Cache"), theSystem.data(), SLOT(FlushDnsCache()));
#ifdef WIN32
		m_pMenuSecurityExplorer = m_pMenuTools->addAction(MakeActionIcon(":/Actions/Security"), tr("Security Explorer"), this, SLOT(OnSecurityExplorer()));
#endif

		m_pMenuTools->addSeparator();
#ifdef WIN32

		m_pMenuMonitorSYS = m_pMenuTools->addAction(MakeActionIcon(":/Actions/MonitorSys"), tr("Use Driver to Monitor System"), this, SLOT(OnMonitorSys()));
		m_pMenuMonitorSYS->setCheckable(true);

		m_pMenuMonitorETW = m_pMenuTools->addAction(MakeActionIcon(":/Actions/MonitorETW"), tr("Monitor ETW Events"), this, SLOT(OnMonitorETW()));
		m_pMenuMonitorETW->setCheckable(true);
		m_pMenuMonitorETW->setChecked(theSystem->IsMonitoringETW());
		m_pMenuMonitorETW->setEnabled(theSystem->RootAvaiable());

		m_pMenuMonitorFW = m_pMenuTools->addAction(MakeActionIcon(":/Actions/MonitorFW"), tr("Monitor Windows Firewall"), this, SLOT(OnMonitorFW()));
		m_pMenuMonitorFW->setCheckable(true);
		m_pMenuMonitorFW->setChecked(theSystem->IsMonitoringFW());
		//m_pMenuMonitorFW->setEnabled(theSystem->RootAvaiable());

		int DbgMode = theSystem->GetDebugMonitor();
		m_pMenuMonitorDbgMenu = m_pMenuTools->addMenu(MakeActionIcon(":/Actions/MonitorDbg"), tr("Monitor Debug Output"));
		m_pMenuMonitorDbgLocal = m_pMenuMonitorDbgMenu->addAction("Local", this, SLOT(OnMonitorDbg()));
		m_pMenuMonitorDbgLocal->setCheckable(true);
		m_pMenuMonitorDbgLocal->setChecked((DbgMode & CSystemAPI::eDbgLocal) != 0);
		m_pMenuMonitorDbgLocal->setProperty("Mode", (int)CSystemAPI::eDbgLocal);
		m_pMenuMonitorDbgGlobal = m_pMenuMonitorDbgMenu->addAction("Global", this, SLOT(OnMonitorDbg()));
		m_pMenuMonitorDbgGlobal->setCheckable(true);
		m_pMenuMonitorDbgGlobal->setChecked((DbgMode & CSystemAPI::eDbgGlobal) != 0);
		m_pMenuMonitorDbgGlobal->setProperty("Mode", (int)CSystemAPI::eDbgGlobal);
		m_pMenuMonitorDbgGlobal->setEnabled(theSystem->RootAvaiable());
		m_pMenuMonitorDbgKernel = m_pMenuMonitorDbgMenu->addAction("Kernel", this, SLOT(OnMonitorDbg()));
		m_pMenuMonitorDbgKernel->setCheckable(true);
		m_pMenuMonitorDbgKernel->setChecked((DbgMode & CSystemAPI::eDbgKernel) != 0);
		m_pMenuMonitorDbgKernel->setProperty("Mode", (int)CSystemAPI::eDbgKernel);
#endif

	m_pMenuHelp = menuBar()->addMenu(tr("&Help"));
		m_pMenuSupport = m_pMenuHelp->addAction(MakeActionIcon(":/Actions/Support"), tr("Support TaskExplorer on Patreon"), this, SLOT(OnHelp()));
		m_pMenuForum = m_pMenuHelp->addAction(MakeActionIcon(":/Actions/Forum"), tr("Visit Support Forum"), this, SLOT(OnHelp()));
		m_pMenuHelp->addSeparator();
		m_pMenuCheckUpdates = m_pMenuHelp->addAction(MakeActionIcon(":/Actions/Refresh"), tr("Check for Updates"), this, SLOT(OnCheckForUpdates()));
		m_pMenuHelp->addSeparator();
#ifdef WIN32
		m_pMenuAboutPH = m_pMenuHelp->addAction(tr("About ProcessHacker Library"), this, SLOT(OnAbout()));
#endif
		m_pMenuAboutQt = m_pMenuHelp->addAction(tr("About the Qt Framework"), this, SLOT(OnAbout()));
		m_pMenuAbout = m_pMenuHelp->addAction(QIcon(":/TaskExplorer.png"), tr("About TaskExplorer"), this, SLOT(OnAbout()));

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);
	m_pToolBar->addAction(m_pMenuSettings);

	m_pToolBar->addAction(m_pMenuSettings);
	m_pToolBar->addSeparator();
	m_pToolBar->addAction(m_pMenuPauseRefresh);

	//m_pToolBar->addAction(m_pMenuRefreshNow);
	m_pRefreshButton = new QToolButton();
	m_pRefreshButton->setIcon(MakeActionIcon(":/Actions/Refresh"));
	m_pRefreshButton->setToolTip(tr("Refresh Now/Reset Hold"));
	m_pRefreshButton->setPopupMode(QToolButton::MenuButtonPopup);
	QMenu* pRefreshMenu = new QMenu(m_pRefreshButton);
	m_pRefreshGroup = new QActionGroup(pRefreshMenu);
	//MakeAction(m_pRefreshGroup, pRefreshMenu, tr("Extremly fast (60Hz)"), 17);
	MakeAction(m_pRefreshGroup, pRefreshMenu, tr("Extremly fast (30Hz)"), 33);
	MakeAction(m_pRefreshGroup, pRefreshMenu, tr("Ultra fast (0.1s)"), 100);
	MakeAction(m_pRefreshGroup, pRefreshMenu, tr("Very fast (0.25s)"), 250);
	MakeAction(m_pRefreshGroup, pRefreshMenu, tr("Fast (0.5s)"), 500);
	MakeAction(m_pRefreshGroup, pRefreshMenu, tr("Normal (1s)"), 1000);
	MakeAction(m_pRefreshGroup, pRefreshMenu, tr("Slow (2s)"), 2000);
	MakeAction(m_pRefreshGroup, pRefreshMenu, tr("Very slow (5s)"), 5000);
	MakeAction(m_pRefreshGroup, pRefreshMenu, tr("Extremely slow (10s)"), 10000);
	connect(m_pRefreshGroup, SIGNAL(triggered(QAction*)), this, SLOT(OnChangeInterval(QAction*)));
    m_pRefreshButton->setMenu(pRefreshMenu);
	//QObject::connect(m_pRefreshButton, SIGNAL(triggered(QAction*)), , SLOT());
	QObject::connect(m_pRefreshButton, SIGNAL(pressed()), this, SLOT(RefreshAll()));
	m_pToolBar->addWidget(m_pRefreshButton);

	//m_pToolBar->addAction(m_pMenuHoldAll);
	m_pHoldButton = new QToolButton();
	m_pHoldButton->setIcon(MakeActionIcon(":/Actions/Hibernate"));
	m_pHoldButton->setToolTip(tr("Hold ALL removed items"));
	m_pHoldButton->setCheckable(true);
	m_pHoldButton->setPopupMode(QToolButton::MenuButtonPopup);
	QMenu* pHoldMenu = new QMenu(m_pHoldButton);
	m_pHoldGroup = new QActionGroup(pHoldMenu);
	MakeAction(m_pHoldGroup, pHoldMenu, tr("Short persistence (2.5s)"), 2500);
	MakeAction(m_pHoldGroup, pHoldMenu, tr("Normal persistence (5s)"), 5*1000);
	MakeAction(m_pHoldGroup, pHoldMenu, tr("Long persistence (10s)"), 10*1000);
	MakeAction(m_pHoldGroup, pHoldMenu, tr("Very long persistence (60s)"), 60*1000);
	MakeAction(m_pHoldGroup, pHoldMenu, tr("Extremely long persistence (5m)"), 5*60*1000);
	m_pHoldAction = MakeAction(m_pHoldGroup, pHoldMenu, tr("Pseudo static persistence (1h)"), 60*60*1000);
	connect(m_pHoldGroup, SIGNAL(triggered(QAction*)), this, SLOT(OnChangePersistence(QAction*)));
    m_pHoldButton->setMenu(pHoldMenu);
	//QObject::connect(m_pHoldButton, SIGNAL(triggered(QAction*)), , SLOT());
	QObject::connect(m_pHoldButton, SIGNAL(pressed()), this, SLOT(OnStaticPersistence()));
	m_pToolBar->addWidget(m_pHoldButton);

	m_pToolBar->addSeparator();
	m_pToolBar->addAction(m_pMenuShowTree);
	m_pToolBar->addAction(m_pMenuMultiUser);

	m_pMenuFilterButton = new QToolButton();
	m_pMenuFilterButton->setIcon(MakeActionIcon(":/Actions/Filter"));
	m_pMenuFilterButton->setToolTip(tr("Filter Processes"));
	m_pMenuFilterButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_pMenuFilterButton->setMenu(m_pMenuFilterMenu);
	//QObject::connect(m_pMenuFilterButton, SIGNAL(triggered(QAction*)), , SLOT());
	QObject::connect(m_pMenuFilterButton, SIGNAL(pressed()), this, SLOT(OnViewFilter()));
	m_pMenuFilterButton->setCheckable(true);
	//m_pMenuFilterButton->setChecked();
	m_pToolBar->addWidget(m_pMenuFilterButton);

	m_pToolBar->addSeparator();
#ifdef WIN32
	m_pToolBar->addAction(m_pMenuMonitorSYS);
	m_pToolBar->addAction(m_pMenuMonitorETW);
	m_pToolBar->addAction(m_pMenuMonitorFW);

	m_pMenuMonitorDbgButton = new QToolButton();
	m_pMenuMonitorDbgButton->setIcon(MakeActionIcon(":/Actions/MonitorDbg"));
	m_pMenuMonitorDbgButton->setToolTip(tr("Monitor Debug Output"));
	m_pMenuMonitorDbgButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_pMenuMonitorDbgButton->setMenu(m_pMenuMonitorDbgMenu);
	//QObject::connect(m_pMenuMonitorDbgButton, SIGNAL(triggered(QAction*)), , SLOT());
	QObject::connect(m_pMenuMonitorDbgButton, SIGNAL(pressed()), this, SLOT(OnMonitorDbg()));
	m_pMenuMonitorDbgButton->setCheckable(true);
	m_pMenuMonitorDbgButton->setChecked((DbgMode & CSystemAPI::eDbgAll) != 0);
	m_pToolBar->addWidget(m_pMenuMonitorDbgButton);

	m_pToolBar->addSeparator();
#endif
	m_pToolBar->addAction(m_pMenuSystemInfo);
	m_pToolBar->addSeparator();
	
	//m_pToolBar->addAction(m_pMenuFindProcess);

	m_pFindButton = new QToolButton();
	m_pFindButton->setIcon(MakeActionIcon(":/Actions/Find"));
	m_pFindButton->setToolTip(tr("Search..."));
	m_pFindButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_pFindButton->setMenu(m_pMenuFind);
	//QObject::connect(m_pFindButton, SIGNAL(triggered(QAction*)), , SLOT());
	QObject::connect(m_pFindButton, SIGNAL(pressed()), this, SLOT(OnFindHandle()));
	m_pToolBar->addWidget(m_pFindButton);
	m_pToolBar->addSeparator();

	/*
#ifdef WIN32
	m_pFreeButton = new QToolButton();
	m_pFreeButton->setIcon(MakeActionIcon(":/Actions/FreeMem"));
	m_pFreeButton->setToolTip(tr("Free memory"));
	m_pFreeButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_pFreeButton->setMenu(m_pMenuFree);
	//QObject::connect(m_pFreeButton, SIGNAL(triggered(QAction*)), , SLOT());
	QObject::connect(m_pFreeButton, SIGNAL(pressed()), this, SLOT(OnFreeMemory()));
	m_pToolBar->addWidget(m_pFreeButton);
	m_pToolBar->addSeparator();
#endif

	m_pComputerButton = new QToolButton();
	m_pComputerButton->setIcon(MakeActionIcon(":/Actions/Shutdown"));
	m_pComputerButton->setToolTip(tr("Lock, Shutdown/Reboot, etc..."));
	m_pComputerButton->setPopupMode(QToolButton::MenuButtonPopup);
	m_pComputerButton->setMenu(m_pMenuComputer);
	//QObject::connect(m_pComputerButton, SIGNAL(triggered(QAction*)), , SLOT());
#ifndef _DEBUG
	QObject::connect(m_pComputerButton, SIGNAL(pressed()), this, SLOT(OnComputerAction()));
#endif
	m_pToolBar->addWidget(m_pComputerButton);
	m_pToolBar->addSeparator();
	*/

	m_pToolBar->addAction(m_pMenuPersistence);

	m_pToolBar->addSeparator();
	m_pToolBar->addAction(m_pMenuClusterMode);

	//
	// Which machine the window is about, and the only place it can be said
	// outside cluster mode - there the tree holds one machine and has no row
	// to click. In cluster mode the tree still holds them all and this moves
	// what the graph bar and the system tabs report.
	//
	// Hidden while there is only the local machine. A list of one is not a
	// choice, and a control that cannot be used is worse than no control.
	//
	//
	// A hair of air, so the box does not read as part of the button.
	//
	QWidget* pMachineGap = new QWidget();
	pMachineGap->setFixedWidth(4);
	m_pToolBar->addWidget(pMachineGap);

	m_pMachineBox = new QComboBox();
	m_pMachineBox->setToolTip(tr("The machine the graphs, the system tabs and the status line report on."));
	m_pMachineBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	m_pMachineBox->setMinimumWidth(120);
	connect(m_pMachineBox, SIGNAL(activated(int)), this, SLOT(OnMachineBoxChanged(int)));
	m_pMachineBoxAction = m_pToolBar->addWidget(m_pMachineBox);
	m_pMachineBoxAction->setVisible(false);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	m_pToolBar->addAction(m_pMenuElevate);

	//
	// The separator, the padding either side and the label itself are kept as
	// four actions rather than four widgets, because hiding an action is what
	// takes something out of a toolbar's layout - hiding the widget alone
	// leaves its slot behind, and a separator with nothing after it is a line
	// at the end of the bar with no reason to be there.
	//
	m_UpdateLabelItems.append(m_pToolBar->addSeparator());
	m_UpdateLabelItems.append(m_pToolBar->addWidget(new QLabel("        ")));

	m_pUpdateLabel = new QLabel();
	m_pUpdateLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
	m_UpdateLabelItems.append(m_pToolBar->addWidget(m_pUpdateLabel));

	m_UpdateLabelItems.append(m_pToolBar->addWidget(new QLabel("        ")));

	//
	// Filled in rather than built with a message in it, so that a supported
	// copy never shows the appeal for the moment before the first update.
	//
	UpdateLabel();

	

	restoreGeometry(theConf->GetBlob("MainWindow/Window_Geometry"));
	m_pMainSplitter->restoreState(theConf->GetBlob("MainWindow/Window_Splitter"));
	m_pPanelSplitter->restoreState(theConf->GetBlob("MainWindow/Panel_Splitter"));
	m_pGraphSplitter->restoreState(theConf->GetBlob("MainWindow/Graph_Splitter"));

	OnSplitterMoved();


	bool bAutoRun = QApplication::arguments().contains("-autorun");

	QIcon Icon;
	Icon.addFile(":/TaskExplorer.png");
	m_pTrayIcon = new QSystemTrayIcon(Icon, this);
	m_pTrayIcon->setToolTip("TaskExplorer");
	connect(m_pTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(OnSysTray(QSystemTrayIcon::ActivationReason)));
	//m_pTrayIcon->setContextMenu(m_pNeoMenu);

	m_pTrayMenu = new QMenu();
	m_pTrayMenu->addAction(m_pMenuRun);
	m_pTrayMenu->addAction(m_pMenuRunAs);
	m_pTrayMenu->addSeparator();
	m_pTrayMenu->addAction(m_pMenuSystemInfo);
	m_pTrayMenu->addSeparator();
	m_pTrayMenu->addMenu(m_pMenuComputer);
	m_pTrayMenu->addMenu(m_pMenuUsers);
	m_pTrayMenu->addSeparator();
	m_pTrayMenu->addAction(m_pMenuExit);

	m_pTrayIcon->show(); // Note: qt bug; without a first show hide does not work :/
	if(!bAutoRun && !theConf->GetBool("SysTray/Show", true))
		m_pTrayIcon->hide();

	m_pTrayGraph = NULL;

	//
	// First, so it sits at the left of the permanent group and reads as a
	// heading for the numbers that follow - which are the local machine's, as
	// is the graph bar above them, whatever the panels are showing.
	//
	m_pStausMachine = new QLabel();
	statusBar()->addPermanentWidget(m_pStausMachine);

	m_pStausCPU	= new QLabel();
	statusBar()->addPermanentWidget(m_pStausCPU);
	m_pStausGPU	= new QLabel();
	statusBar()->addPermanentWidget(m_pStausGPU);
	m_pStausMEM	= new QLabel();
	statusBar()->addPermanentWidget(m_pStausMEM);
	m_pStausIO	= new QLabel();
	statusBar()->addPermanentWidget(m_pStausIO);
	m_pStausNET	= new QLabel();
	statusBar()->addPermanentWidget(m_pStausNET);


	if (!bAutoRun)
		show();

	if (theSystem->GetKernelDriver().Connected)
	{
		statusBar()->showMessage(tr("TaskExplorer with kernel driver is ready..."), 30000);
	}
	//else if (((CWindowsAPI*)theSystem.data())->HasDriverFailed() && theSystem->RootAvaiable())
	//{
	//	QString Message = tr("Failed to load %1 driver, this could have various causes.\r\n"
	//		"Currently the driver is not signed, pelase enable test signing (bcdedit /set testsigning on) to use kernel features."
	//	).arg(((CWindowsAPI*)theSystem.data())->GetDriverFileName());

	//	bool State = false;
	//	CCheckableMessageBox::question(this, "TaskExplorer", Message
	//		, tr("Don't use the driver. WARNING: this will limit the aplications functionality!"), &State, QDialogButtonBox::Ok, QDialogButtonBox::Ok, QMessageBox::Warning);

	//	if (State)
	//		theConf->SetValue("Options/UseDriver", false);

	//	statusBar()->showMessage(tr("TaskExplorer failed to load driver %1").arg(((CWindowsAPI*)theSystem.data())->GetDriverFileName()), 180000);
	//}
	else
		statusBar()->showMessage(tr("TaskExplorer is ready..."), 30000);

	ApplyOptions();

	//
	// After ApplyOptions, because whether the machine layer is drawn decides
	// whether a daemon for *this* machine has anywhere sensible to go.
	//
	TryLocalDaemon();

	// Initialize Online Updater
	m_pUpdater = new COnlineUpdater(this);
	connect(m_pUpdater, SIGNAL(StateChanged()), this, SLOT(UpdateLabel()));
	UpdateLabel(); // Initial label update

	m_LastTimer = 0;
	//m_uTimerCounter = 0;
	m_uTimerID = startTimer(theConf->GetInt("Options/RefreshInterval", 1000));

	UpdateAll();
}

CTaskExplorer::~CTaskExplorer()
{
	killTimer(m_uTimerID);

	m_pTrayIcon->hide();

	theConf->SetBlob("MainWindow/Window_Geometry",saveGeometry());
	theConf->SetBlob("MainWindow/Window_Splitter",m_pMainSplitter->saveState());
	theConf->SetBlob("MainWindow/Panel_Splitter",m_pPanelSplitter->saveState());
	theConf->SetBlob("MainWindow/Graph_Splitter",m_pGraphSplitter->saveState());

	theSystem.clear(); // the deleter posts the actual destruction to its own thread

	theGUI = NULL;
}

void CTaskExplorer::SetUITheme()
{
	int iDark = theConf->GetInt("MainWindow/DarkTheme", 2);
	int iFusion = theConf->GetInt("MainWindow/UseFusionTheme", 2);
	bool bDark = iDark == 2 ? m_CustomTheme.IsSystemDark() : (iDark == 1);
	m_CustomTheme.SetUITheme(bDark, iFusion);
	//CPopUpWindow::SetDarkMode(bDark);

	if(m_pGraphBar)
		m_pGraphBar->SetDarkMode(bDark);
	CTreeItemModel::SetDarkMode(bDark);
	CListItemModel::SetDarkMode(bDark);

	QFont font = QApplication::font();
	QString customFontStr = theConf->GetString("Options/UIFont", "");
	if (customFontStr != "") {
		font.setFamily(customFontStr);
		QApplication::setFont(font);
	}
	double newFontSize = m_DefaultFontSize * theConf->GetInt("Options/FontScaling", 100) / 100.0;
	if (newFontSize != font.pointSizeF()) {
		font.setPointSizeF(newFontSize);
		QApplication::setFont(font);
	}
}

void CTaskExplorer::OnGraphsResized(int Size)
{
	QList<int> Sizes = m_pGraphSplitter->sizes();
	Sizes[1] += Sizes[0] - Size;
	Sizes[0] = Size;
	m_pGraphSplitter->setSizes(Sizes);
}

void CTaskExplorer::OnChangeInterval(QAction* pAction)
{
	quint64 Interval = pAction->data().toULongLong();
	
	theConf->SetValue("Options/RefreshInterval", Interval);

	killTimer(m_uTimerID);
	m_uTimerID = startTimer(Interval);

	emit ReloadPlots();
}

void CTaskExplorer::OnStaticPersistence()
{
	if(m_pHoldButton->isChecked())
		CAbstractInfoEx::SetPersistenceTime(theConf->GetUInt64("Options/PersistenceTime", 5000));
	else 
		OnChangePersistence(m_pHoldAction);
}

void CTaskExplorer::OnTreeButton()
{
	m_pProcessTree->SetTree(m_pMenuShowTree->isChecked());
}

void CTaskExplorer::OnMultiUserButton()
{
	m_pProcessTree->SetMultiUser(m_pMenuMultiUser->isChecked());
}

//
// Turning the machine layer on or off while running.
//
// Through the stored value and ApplyOptions rather than by calling
// CCluster::SetClusterMode here, because the switch is three things at once -
// the cluster has to exist, the tree has to gain or lose its machine level,
// and the menus have to be re-asked what they apply to - and that sequence is
// already written down in one place.
//
void CTaskExplorer::OnClusterModeButton()
{
	theConf->SetValue("Options/ClusterMode", m_pMenuClusterMode->isChecked());
	ApplyOptions();
}

//
// The machine picked in the toolbar becomes the one the window is about.
//
// The same thing clicking a row in the tree does, and it goes through the same
// call, so whichever way it was said the other agrees with it.
//
void CTaskExplorer::OnMachineBoxChanged(int Index)
{
	if (Index < 0)
		return;

	//
	// Looked up in the live list rather than trusted: the box is rebuilt
	// whenever a machine joins or leaves, and what it holds is the address a
	// machine had when the row was written. A machine that has since gone
	// simply matches nothing, which is the right answer.
	//
	const quintptr Chosen = (quintptr)m_pMachineBox->itemData(Index).toULongLong();
	foreach(const CSystemPtr& pSystem, CCluster::GetSystems())
	{
		if ((quintptr)pSystem.data() == Chosen)
		{
			//
			// In cluster mode this moves the graphs and nothing else: the tree
			// holds every machine and the panels follow the row that is
			// selected there, which is a separate question with a separate
			// answer. Out of cluster mode there is one machine on screen and
			// the box chooses it outright - panels, tree and graphs together.
			//
			if (CCluster::IsClusterMode())
				CCluster::SetGraphSystem(pSystem.data());
			else
				CCluster::SetViewSystem(pSystem.data());
			return;
		}
	}
}

void CTaskExplorer::OnShowUnixSockets()
{
	theConf->SetValue("Options/ShowUnixSockets", m_pMenuShowUnixSockets->isChecked());

	//
	// The lists rebuild themselves on the next round either way - what is no
	// longer given to Sync is dropped, exactly as a closed socket would be - so
	// nothing has to be torn down here.
	//
}

//
// Connect to a TaskServer.
//
// The address doubles as the entry's name for now - a pipe name on one machine,
// host:port once the network listener exists. A proper dialog with a display
// name of its own belongs here when there is more to fill in than one field.
//
void CTaskExplorer::OnConnect()
{
	//
	// The cluster has to exist before the dialog does: it is what knows the
	// machines already saved, which is what the dialog offers to pick from.
	// Creating it is not the same as turning cluster mode on - that is the tree
	// switch below - it only means the target list is loaded.
	//
	EnsureCluster();

	//
	// Switched on here as well as in the settings dialog, because this is the
	// other place somebody expresses an interest in other machines - and the
	// setting may have been made in an earlier session, when there was no
	// cluster to start it on.
	//
	if (theConf->GetBool("Options/Discover", false) && !theCluster->IsDiscovering())
	{
		QString Error;
		theCluster->StartDiscovery(&Error);
	}

	//
	// The dialog is asked again after every failure, and it is the same dialog.
	//
	// A wrong password or a mistyped address used to close it, put the reason in
	// a message box, and leave whoever was connecting to open it again and fill
	// in all four fields from memory - which is the moment people give up. exec()
	// on the same instance brings back exactly what was typed, so the fix is one
	// field and Return.
	//
	// Cancel still cancels, so there is always a way out; nothing here loops
	// without somebody pressing something.
	//
	CConnectDialog Dialog(this);
	for (;;)
	{
	if (Dialog.exec() != QDialog::Accepted)
		return;

	const QString Name = Dialog.GetName();
	const QString Address = Dialog.GetAddress();

	const SCredentials Cred = Dialog.GetCredentials();

	STATUS Status;
	CSystemPtr pSystem = theCluster->Connect(Name, Address, &Status, Cred);

	//
	// Something answered, and it is not the machine this entry was saved for.
	//
	// Asked rather than decided, and asked with both ids on screen, because
	// there are two entirely ordinary reasons for it and they want opposite
	// answers: the machine was rebuilt or the daemon reinstalled, in which case
	// yes; or this address now reaches something else, in which case no, and
	// quietly attaching the saved name to it is how a person ends up reading one
	// machine while believing they are looking at another.
	//
	// Defaulting to No: an unread dialog dismissed with Return should leave
	// things as they were.
	//
	if (pSystem.isNull() && Status.GetMsgCode() == TE_MachineIdMismatch)
	{
		const QVariantList Args = Status.GetArgs();
		const QString Was = Args.count() > 1 ? Args[1].toString() : QString();
		const QString Now = Args.count() > 2 ? Args[2].toString() : QString();

		const QString Question = tr(
			"%1 answered, but it is not the machine saved as \"%2\".\n\n"
			"Saved machine:   %3\n"
			"Answered now:    %4\n\n"
			"This is what you would expect if the machine was rebuilt or the "
			"server reinstalled. If it was not, something else is now reachable "
			"at this address.\n\n"
			"Accept this machine under that name?")
			.arg(Address).arg(Name)
			.arg(Was)
			.arg(Now.isEmpty() ? tr("did not say") : Now);

		//
		// Back to the dialog rather than out of it: somebody who refuses this
		// machine under that name usually wants to correct the name or the
		// address, not to abandon connecting.
		//
		if (QMessageBox::warning(this, "TaskExplorer", Question,
				QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			continue;

		//
		// A second call rather than a flag on the first: accepting is a decision
		// about *this* machine that somebody has just made, and it should not be
		// possible to express it before the question has been asked.
		//
		pSystem = theCluster->Connect(Name, Address, &Status, Cred, CCluster::eAcceptNew);
	}

	if (pSystem.isNull())
	{
		CheckErrors(QList<STATUS>() << Status);
		continue;
	}

	//
	// Saved only now, and only because it worked.
	//
	// Storing what was typed before the connection was tried would fill the
	// store with keys that do not open anything, and the first symptom would be
	// a reconnection that fails for a reason nobody could see - the field would
	// look right, because it would be exactly what was typed.
	//
	if (Dialog.GetRemember() && Cred.IsValid())
	{
		if (CUnlockDialog::Open(this))
		{
			STATUS Saved = CCredentialStore::Instance()->Set(Name, Cred);
			if (Saved.IsError())
				CheckErrors(QList<STATUS>() << Saved);
		}
	}

	//
	// Without the machine layer the window becomes this machine's - the tree
	// shows its processes, the panels its detail, the title bar its name. That
	// is CCluster::Connect's doing: it made the new node the view system, which
	// is what everything here follows.
	//
	UpdateTargetMenu();
	UpdateAll();
	return;
	}
}

CCluster* CTaskExplorer::EnsureCluster()
{
	if (!theCluster)
	{
		theCluster = new CCluster(this);
		theCluster->LoadTargets();

		//
		// And connect what was saved.
		//
		// TryUnlock first, which is the silent half: where the platform can hold
		// the master key - DPAPI on Windows, the keyring on Linux - the store
		// opens without asking anybody anything, and the machines come back on
		// their own. Where it cannot, this does nothing and the entries sit in
		// the list until the store is opened, which is the honest behaviour:
		// nothing can connect without a key.
		//
		// Loading the list was never the missing part - see ConnectSavedTargets.
		//
		CCredentialStore::Instance()->TryUnlock();

		//
		// Connected before the machines are, so the first one being tried is
		// already on screen rather than reported after the fact.
		//
		connect(theCluster, SIGNAL(ConnectingTo(const QString&, int, int)),
			this, SLOT(OnConnectingTo(const QString&, int, int)));
		connect(theCluster, SIGNAL(ConnectRunFinished(int, int, const QString&)),
			this, SLOT(OnConnectRunFinished(int, int, const QString&)));

		theCluster->ConnectSavedTargets();
		connect(theCluster, SIGNAL(TargetsChanged()), this, SLOT(UpdateTargetMenu()));

		//
		// Relayed rather than connected to directly by the views: theCluster
		// may not exist when they are built, theGUI always does.
		//
		connect(theCluster, SIGNAL(ViewSystemChanged()), this, SLOT(OnViewSystemChanged()));
		connect(theCluster, SIGNAL(ActiveSystemChanged()), this, SLOT(OnActiveSystemChanged()));

		//
		// A machine going away, which was emitted and listened to by nobody.
		//
		// The panels hold their selection by shared pointer, so a process from a
		// disconnected machine stays alive in whatever is showing it and goes on
		// being drawn - a frozen page claiming to be a machine that is not there.
		// Worse, it used to be a crash: see NEXT.md 5.44 and
		// CAbstractInfo::GetSystem, which is why holding one is now merely wrong
		// rather than fatal. This is what makes it not wrong either.
		//
		connect(theCluster, SIGNAL(NodeRemoved(const CSystemPtr&)),
			this, SLOT(OnNodeRemoved(const CSystemPtr&)));
	}
	return theCluster;
}

//
// A machine has gone. Anything still showing it stops.
//
// Only the task panel is told: it is the one place that holds a selection
// across refreshes. The process tree rebuilds itself from the cluster every
// round, so a departed machine's rows leave on their own - and the system panel
// follows CCluster::GetViewSystem, which Disconnect() has already changed.
//
// Not a crash any more either way - see CAbstractInfo::GetSystem - but a panel
// that goes on drawing a machine nobody is connected to is a lie that looks
// like data.
//
void CTaskExplorer::OnNodeRemoved(const CSystemPtr& pSystem)
{
	if (m_pTaskInfo)
		m_pTaskInfo->DropSystem(pSystem);
}

//
// The panels are now looking at a different machine.
//
// Order matters. The views re-point their subscriptions on ViewSystemChanged,
// then ReloadPanels throws away what they were holding, and only then is
// anything asked again - the other way round would refill the models from the
// machine that is being left.
//
//
// Re-read the tab names into the View menu.
//
// Only the text: which action maps to which tab is fixed at construction and
// does not move, so m_Act2Tab stays as it is.
//
//
// What one tab is called in the View menu.
//
// The tab's own name, and for a tab that is about the other kind of system, the
// name of that system after it.
//
// Every tab is built on every platform, because a viewer watching another
// machine needs the tabs that machine has - so this menu offers a Windows
// viewer the control group and security tabs, which will be blank on everything
// it can see locally. Somebody turning one on should be told that before they
// wonder why, and the answer is not "you cannot have it" but "this is for the
// other sort of machine": there is nothing wrong with turning it on if what you
// are here for is a Linux box across the network.
//
// Only in the menu. The tab keeps its own name - the panel is already showing
// whichever machine is selected, and its title bar is not the place to argue
// about platforms.
//
QString CTaskExplorer::TabMenuLabel(CTabPanel* pPanel, int Index)
{
	const QString Name = pPanel->GetTabLabel(Index);

	const int Platform = pPanel->GetTabPlatform(Index);
	if (Platform == CTabPanel::eAnyPlatform)
		return Name;

	//
	// Against the platform this program runs on rather than the machine being
	// looked at: the menu is a lasting choice about which tabs exist, while the
	// selection moves from row to row. A note that changed as the user clicked
	// about would say nothing.
	//
#ifdef WIN32
	const int OwnPlatform = CSystemAPI::eOsWindows;
#else
	const int OwnPlatform = CSystemAPI::eOsLinux;
#endif
	if (Platform == OwnPlatform)
		return Name;

	return tr("%1 (%2)").arg(Name).arg(Platform == CSystemAPI::eOsWindows ? tr("Windows") : tr("Linux"));
}

void CTaskExplorer::UpdateTabMenus()
{
	for (QMap<QAction*, int>::const_iterator I = m_Act2Tab.begin(); I != m_Act2Tab.end(); ++I)
	{
		CTabPanel* pPanel = (I.key()->parentWidget() == m_pMenuSysTabs)
			? (CTabPanel*)m_pSystemInfo : (CTabPanel*)m_pTaskInfo;
		I.key()->setText(TabMenuLabel(pPanel, I.value()));
	}
}

//
// The graphs are about a different machine now.
//
// Deliberately much less than OnViewSystemChanged does: nothing that holds a
// list has to throw anything away, because no list is about this machine. The
// graph bar clears itself on the relayed signal, the status line is rewritten
// on the next tick anyway, and the labels say which machine is meant.
//
void CTaskExplorer::OnActiveSystemChanged()
{
	emit ActiveSystemChanged();
	UpdateMachineLabels();
}

void CTaskExplorer::OnViewSystemChanged()
{
	//
	// Not ReloadPanels. That one also clears the process tree - which holds
	// *every* machine, not the one being looked at - and rebuilding it drops
	// the selection, which is what moved the view system in the first place.
	// The panels that hold machine-scoped lists clear themselves on this
	// signal instead.
	//
	emit ViewSystemChanged();

	UpdateMachineLabels();
	UpdateAll();
}

//
// Read this machine directly, or through a daemon running on it.
//
// The point of the daemon is that it can be privileged where the viewer is not,
// so when one is there it is the better source and is used. What stops that
// being a surprise is the title bar, which says which of the two is in force.
//
// The escape hatch is a command-line switch rather than a setting, because the
// choice is made once at startup and cannot be changed while running: the
// window is either reading a local collector or a socket, and everything from
// the process tree to the graph bar is built on that. A setting would suggest
// otherwise.
//
// Only without the machine layer. With it on, this machine already has a branch
// of its own read directly, and a daemon for the same machine would be a second
// branch for it - two rows for one computer, disagreeing about the details.
//
void CTaskExplorer::TryLocalDaemon()
{
	m_bSelfContained = QCoreApplication::arguments().contains("-self-contained");
	m_bDaemonPresent = false;

	if (CCluster::IsClusterMode())
		return;

	m_bDaemonPresent = CCluster::IsLocalDaemonPresent();
	if (!m_bDaemonPresent || m_bSelfContained)
	{
		UpdateTargetMenu();
		UpdateTitle();
		return;
	}

	STATUS Status;
	if (EnsureCluster()->ConnectLocalDaemon(&Status).isNull())
	{
		//
		// Something is listening on the endpoint but it is not a daemon this
		// viewer can talk to. Said in the status bar rather than in a box: the
		// window works perfectly well without it, and a modal error before the
		// window is even shown is a poor way to start.
		//
		m_bSelfContained = true;
		statusBar()->showMessage(tr("Could not use the local daemon: %1").arg(FormatError(Status)), 30000);
	}

	//
	// Both of these ran before the probe did - ApplyOptions calls them - so the
	// Tasks entry offering the other mode does not exist yet and the title does
	// not know which mode this is. Nothing calls them again on its own when
	// nothing connected.
	//
	UpdateTargetMenu();
	UpdateTitle();
}

void CTaskExplorer::OnSwitchMode()
{
	const bool bToSelfContained = CCluster::IsLocalDaemon(CCluster::GetActiveSystem().data());

	if (QMessageBox::question(this, "TaskExplorer",
		bToSelfContained ? tr("Restart TaskExplorer without the local daemon?")
		                 : tr("Restart TaskExplorer using the local daemon?"),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		return;

	//
	// -multi because this instance has not exited yet, and without it the new
	// process would signal this one and quit. The same reason OnElevate passes
	// it on Linux.
	//
	QStringList Args;
	Args << "-multi";
	if (bToSelfContained)
		Args << "-self-contained";

	if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), Args))
	{
		QMessageBox::warning(this, "TaskExplorer", tr("Could not start a new instance."));
		return;
	}

	OnExit();
}

//
// The window's name for itself.
//
// Rebuilt rather than set once, because what the window is about can change
// while it is open: with the machine layer off, connecting to a machine makes
// the whole window that machine's, and the name goes first because it is the
// one thing that says so at a glance.
//
// The kernel-driver badge and the elevation suffix stay behind in that case.
// Both describe *this* process - what its own driver can do, and what account
// it is running as - and neither says anything about the machine being watched,
// so they would read as claims about the wrong computer.
//
void CTaskExplorer::UpdateTitle()
{
	CSystemPtr pActive = CCluster::GetActiveSystem();

	//
	// Never a machine name in cluster mode. The window holds all of them there,
	// and naming the one whose graphs happen to be shown would claim the whole
	// window is about it. Out of cluster mode there is exactly one machine on
	// screen and the title is where it is said.
	//
	const bool bRemote = !CCluster::IsClusterMode() && !pActive.isNull() && pActive != theSystem;

	//
	// The daemon is a remote system by every mechanism, but it is not another
	// machine - it is this one, seen through something else. Naming the machine
	// would put this computer's own name in the title as though it were
	// somewhere else, so the mode is named instead.
	//
	const bool bDaemon = bRemote && CCluster::IsLocalDaemon(pActive.data());

	QString appTitle;
	if (bRemote && !bDaemon)
		appTitle = tr("%1 - TaskExplorer v%2").arg(::GetMachineDisplayName(pActive.data())).arg(GetVersion());
	else
	{
		appTitle = tr("TaskExplorer v%1").arg(GetVersion());

		//
		// Which of the two ways this machine is being read. Said only when
		// there is a choice to have made: with no daemon on the machine there
		// is one way to do it, and naming it would be answering a question
		// nobody asked.
		//
		if (bDaemon)
			appTitle.append(tr(" - [local daemon]"));
		else if (m_bDaemonPresent)
			appTitle.append(tr(" - [self-contained]"));

		//
		// The driver's trust level, as a short badge in the title bar - and
		// only while this process is the one doing the collecting. In daemon
		// mode the driver that matters is the daemon's, which this viewer has
		// not asked about; showing its own would describe something nobody is
		// looking at.
		//
		if (!bDaemon)
		{
			const QString Badge = GetKernelBadge(theSystem->GetKernelDriver());
			if (!Badge.isEmpty())
				appTitle.append(" - " + Badge);
		}

		if (theSystem->RootAvaiable())
			appTitle.append(theSystem->GetOsType() == CSystemAPI::eOsWindows ? tr(" (Administrator)") : tr(" (root)"));
	}

	this->setWindowTitle(appTitle);
}

//
// Who is who, on screen.
//
// The status line reports the local machine whatever the panels are showing, as
// does the graph bar directly above it, so it says which one that is - but only
// while there is another machine it could be confused with. The panel names the
// one it is showing for the same reason.
//
//
// Refills the toolbar's machine list and points it at the current one.
//
// Rebuilt rather than patched, because the two things that would have to be
// kept in step - which machines exist and which one is selected - both change
// from outside this window, and a list of at most a handful of rows is not
// worth the bookkeeping.
//
// Signals are blocked while it happens: setCurrentIndex on a box being rebuilt
// would otherwise report a selection nobody made, and moving the view system
// is not a thing to do by accident.
//
void CTaskExplorer::UpdateMachineBox()
{
	if (!m_pMachineBox || !m_pMachineBoxAction)
		return;

	const QList<CSystemPtr> Systems = CCluster::GetSystems();

	//
	// One machine is not a choice. The title bar already names it.
	//
	m_pMachineBoxAction->setVisible(Systems.count() > 1);
	if (Systems.count() < 2) {
		m_pMachineBox->clear();
		return;
	}

	const CSystemPtr pActive = CCluster::GetActiveSystem();

	const bool bWas = m_pMachineBox->blockSignals(true);
	m_pMachineBox->clear();
	foreach(const CSystemPtr& pSystem, Systems)
	{
		if (pSystem.isNull())
			continue;

		//
		// The same name the machine branch is drawn with, so that the box and
		// the tree are plainly talking about the same computer.
		//
		m_pMachineBox->addItem(::GetMachineDisplayName(pSystem.data()), (qulonglong)(quintptr)pSystem.data());
		if (pSystem == pActive)
			m_pMachineBox->setCurrentIndex(m_pMachineBox->count() - 1);
	}
	m_pMachineBox->blockSignals(bWas);
}

void CTaskExplorer::UpdateMachineLabels()
{
	//
	// Only in cluster mode. With the machine layer off there is one machine on
	// screen and the title bar names it, so a second label would be repeating
	// what the window is already called.
	//
	const bool bNeeded = CCluster::IsClusterMode() && CCluster::GetSystems().count() > 1;

	//
	// The machine the bar above it is plotting, which since the graphs began
	// following the selection is not always the local one - see
	// CCluster::GetActiveSystem.
	//
	if (m_pStausMachine)
		m_pStausMachine->setText(bNeeded ? CCluster::GetActiveSystem()->GetHostName() + "    " : QString());

	UpdateMachineBox();
	if (m_pSystemInfo)
		m_pSystemInfo->UpdateMachineLabel();

	UpdateTitle();
}

void CTaskExplorer::OnDisconnect()
{
	if (!theCluster)
		return;

	QStringList Names;
	foreach(const STarget& Target, theCluster->GetTargets())
	{
		if (Target.State == STarget::eConnected)
			Names.append(Target.Name);
	}
	if (Names.isEmpty())
		return;

	bool bOk = false;
	QString Name = Names.count() == 1 ? Names.first()
		: QInputDialog::getItem(this, tr("Disconnect"), tr("Machine to disconnect:"), Names, 0, false, &bOk);
	if (Names.count() > 1 && !bOk)
		return;

	theCluster->Disconnect(Name);
	UpdateTargetMenu();
	UpdateAll();
}

//
// Whether disconnecting is something that can be done at all.
//
void CTaskExplorer::UpdateTargetMenu()
{
	int Connected = 0;
	if (theCluster)
	{
		foreach(const STarget& Target, theCluster->GetTargets())
			if (Target.State == STarget::eConnected)
				Connected++;
	}
	//
	// Whether the machines have to be named at all is a property of how many
	// are in view, not of which one is selected - so it is answered here, where
	// a machine joining or leaving is already being reacted to, as well as when
	// the selection moves.
	//
	UpdateMachineLabels();

	//
	// The other way of reading this machine, offered only when there is one -
	// which means a daemon has to be there, and the machine layer has to be off
	// for it to have anywhere to go.
	//
	if (m_pMenuSwitchMode)
	{
		//
		// Which way round it reads follows what is in force *now*, not what was
		// asked for at startup - the daemon can also be dropped from the
		// Disconnect entry, and after that this run is self-contained whatever
		// it set out to be.
		//
		const bool bUsingDaemon = CCluster::IsLocalDaemon(CCluster::GetActiveSystem().data());

		m_pMenuSwitchMode->setVisible(m_bDaemonPresent && !CCluster::IsClusterMode());
		m_pMenuSwitchMode->setText(bUsingDaemon ? tr("Restart self-contained...")
		                                        : tr("Restart using the local daemon..."));
	}

	if (m_pMenuDisconnect)
	{
		//
		// Hidden entirely in cluster mode rather than merely disabled: with
		// several machines drawn side by side a single global "disconnect" has
		// no obvious subject, and the answer - which machine? - is already
		// asked and answered by right-clicking the one meant. Outside cluster
		// mode there is one connection and the global entry is the only place
		// to end it.
		//
		const bool bCluster = m_pProcessTree && m_pProcessTree->IsMultiMachine();
		m_pMenuDisconnect->setVisible(!bCluster);
		m_pMenuDisconnect->setEnabled(Connected > 0);
	}
}

void CTaskExplorer::OnChangePersistence(QAction* pAction)
{
	quint64 Persistence = pAction->data().toULongLong();
	
	CAbstractInfoEx::SetPersistenceTime(Persistence);
}

void CTaskExplorer::closeEvent(QCloseEvent *e)
{
	if (!m_bExit)
	{
		QString OnClose = theConf->GetString("Options/OnClose", "ToTray");
		if (m_pTrayIcon->isVisible() && OnClose.compare("ToTray", Qt::CaseInsensitive) == 0)
		{
			hide();

			e->ignore();
			return;
		}
		else if(OnClose.compare("Prompt", Qt::CaseInsensitive) == 0)
		{
			CExitDialog ExitDialog(tr("Do you want to close TaskExplorer?"));
			if (!ExitDialog.exec())
			{
				e->ignore();
				return;
			}
		}
	}

	QApplication::quit();
}

#ifdef _DEBUG
size_t getHeapUsage() 
{
	_HEAPINFO heapInfo;
	heapInfo._pentry = nullptr;
	size_t usedMemory = 0;

	while (_heapwalk(&heapInfo) == _HEAPOK) {
		if (heapInfo._useflag == _USEDENTRY) {
			usedMemory += heapInfo._size;
		}
	}

	return usedMemory;
}


#endif

void CTaskExplorer::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() != m_uTimerID)
		return;

#ifdef _DEBUG
	static quint64 LastObjectDump = GetTickCount64();
	if (LastObjectDump + 6 * 1000 < GetTickCount64()) 
	{
		LastObjectDump = GetTickCount64();
		ObjectTrackerBase::PrintCounts();


		//size_t memoryUsed = getHeapUsage();
		//DbgPrint("USED MEMORY: %llu bytes\n", memoryUsed);



		theSystem->DumpObjectCounts();
	}
#endif

	quint64 Interval = theConf->GetInt("Options/RefreshInterval", 1000);
	if (GetCurTick() - m_LastTimer < Interval / 2)
		return;

	UpdateUserMenu();

	m_pMenuShowTree->setChecked(m_pProcessTree->IsTree());
	m_pMenuMultiUser->setChecked(m_pProcessTree->IsMultiUser());
	//
	// Expanding is worth offering whenever there is anything to expand, and
	// grouping by user makes branches even in list mode.
	//
	m_pMenuExpandAll->setEnabled(m_pProcessTree->IsTree() || m_pProcessTree->IsMultiUser());

#ifdef WIN32	// the driver-backed system monitor
	// The system monitor toggle is driven by the kernel driver, where there is one.
	if (theSystem->GetKernelDriver().Connected) {
		m_pMenuMonitorSYS->setEnabled(true);
		m_pMenuMonitorSYS->setChecked(theSystem->IsSystemMonitorOn());
	}
	else if (m_pMenuMonitorSYS->isEnabled()) {
		m_pMenuMonitorSYS->setEnabled(false);
		m_pMenuMonitorSYS->setChecked(false);
	}
#endif

	foreach(QAction* pAction, m_pRefreshGroup->actions())
		pAction->setChecked(pAction->data().toULongLong() == Interval);

	quint64 Persistence = CAbstractInfoEx::GetPersistenceTime();
	foreach(QAction* pAction, m_pHoldGroup->actions())
		pAction->setChecked(pAction->data().toULongLong() == Persistence);
	m_pHoldButton->setChecked(m_pHoldAction->data().toULongLong() == Persistence);
	
	if(!m_pMenuPauseRefresh->isChecked())
		UpdateAll();

	// Process online updater
	m_pUpdater->Process();

	//if(m_pMainSplitter->sizes()[0] > 0)
		m_pGraphBar->UpdateGraphs();

	if (m_pMainSplitter->sizes()[1] > 0 && m_pPanelSplitter->sizes()[0] > 0)
		m_pSystemInfo->UpdateGraphs();

	UpdateStatus();

	m_LastTimer = GetCurTick();
}

void CTaskExplorer::UpdateAll()
{
	//
	// Every connected machine, not only the local one. Each refreshes in its own
	// thread, so the slow one does not hold up the others.
	//
	foreach(const CSystemPtr& pSystem, CCluster::GetSystems())
		QTimer::singleShot(0, pSystem.data(), SLOT(UpdateAll()));

	//
	// And one attempt at any whose connection died. Here because this is the one
	// place that runs on a clock; the attempts themselves are posted and do not
	// hold this up.
	//
	CCluster::RetryLostNodes();

	if (!isVisible() || windowState().testFlag(Qt::WindowMinimized))
		return;

	if(m_pMainSplitter->sizes()[1] > 0)
		m_pTaskInfo->Refresh();

	if (m_pMainSplitter->sizes()[1] > 0 && m_pPanelSplitter->sizes()[0] > 0)
		m_pSystemInfo->Refresh();
}

void CTaskExplorer::RefreshAll()
{
	if(m_pHoldButton->isChecked())
		QTimer::singleShot(0, theSystem.data(), SLOT(ClearPersistence()));

	UpdateAll();
}

void CTaskExplorer::OnViewFilter()
{
	CProcessFilterModel* pFilter = (CProcessFilterModel*)m_pProcessTree->GetModel();

	int Value = 0;

	if(sender() == m_pMenuFilter)
		pFilter->SetEnabled(m_pMenuFilter->isChecked());
	else if(sender() == m_pMenuFilterButton)
		pFilter->SetEnabled(!m_pMenuFilterButton->isChecked());
	else
	{
		QCheckBox* pCheck = qobject_cast<QCheckBox*>(((QWidgetAction*)sender())->defaultWidget());
		Value = pCheck->checkState();

#ifdef WIN32
		if(sender() == m_pMenuFilterWindows)
			pFilter->SetFilterWindows(Value);
		else
#endif
		if(sender() == m_pMenuFilterSystem)
			pFilter->SetFilterSystem(Value);
		else if(sender() == m_pMenuFilterService)
			pFilter->SetFilterService(Value);
		else if(sender() == m_pMenuFilterOther)
			pFilter->SetFilterOther(Value);
		else if(sender() == m_pMenuFilterOwn)
			pFilter->SetFilterOwn(Value);
	}

	if(sender() != m_pMenuFilterButton)
	{
		if(Value != 0) // if one was enabled enable
			pFilter->SetEnabled(true);
		m_pMenuFilterButton->setChecked(pFilter->IsEnabled());
	}
	m_pMenuFilter->setChecked(pFilter->IsEnabled());
}

void CTaskExplorer::UpdateStatus()
{
	m_pStausCPU->setText(tr("CPU: %1%    ").arg(int(100 * CCluster::GetActiveSystem()->GetCpuUsage())));
	m_pStausCPU->setToolTip(CCluster::GetActiveSystem()->GetCpuModel());

	//
	// A machine this process is not collecting from has no device monitors -
	// only aggregate counters cross the wire - so these read empty rather than
	// wrong. See CSystemAPI's constructor.
	//
	QString GPU;
	QStringList GpuInfos;
	if (CGpuMonitor* pGpuMonitor = CCluster::GetActiveSystem()->GetGpuMonitor())
	{
		int i = 0;
		foreach(const CGpuMonitor::SGpuInfo &GpuInfo, pGpuMonitor->GetAllGpuList())
		{
			GPU.append(tr("GPU-%1: %2%    ").arg(i).arg(int(100 * GpuInfo.TimeUsage)));
			GpuInfos.append(GpuInfo.Description);
			i++;
		}
	}
	m_pStausGPU->setToolTip(GpuInfos.join("\r\n"));
	m_pStausGPU->setText(GPU);

	quint64 RamUsage = CCluster::GetActiveSystem()->GetPhysicalUsed();
	quint64 SwapedMemory = CCluster::GetActiveSystem()->GetSwapedOutMemory();
	quint64 CommitedMemory = CCluster::GetActiveSystem()->GetCommitedMemory();

	quint64 InstalledMemory = CCluster::GetActiveSystem()->GetInstalledMemory();
	quint64 TotalSwap = CCluster::GetActiveSystem()->GetTotalSwapMemory();

	//
	// How the commit charge relates to installed memory differs by system.
	//
	// On Linux Committed_AS counts virtual reservations and routinely exceeds
	// installed RAM under the default overcommit policy; folding it into the
	// scale would make Max() pick the commit charge itself, so the commit bar
	// would divide by its own value and sit permanently at 100%.
	//
	quint64 TotalMemory;
	quint64 CommitScale;
	if (CCluster::GetActiveSystem()->GetOsType() == CSystemAPI::eOsWindows)
	{
		TotalMemory = Max(CCluster::GetActiveSystem()->GetInstalledMemory(), CCluster::GetActiveSystem()->GetCommitedMemory());
		CommitScale = TotalMemory;
	}
	else
	{
		//
		// The physical bars are scaled against installed RAM and the commit bar
		// against the commit limit, which is what it is actually bounded by -
		// the same way the memory graph in GraphBar.cpp already scales it.
		//
		// Assigned, not declared. Declaring them here again made two more
		// variables of the same name that died with this block, leaving the
		// ones read further down uninitialised. It never showed while the
		// status line could only ever report the local, Windows machine.
		//
		TotalMemory = CCluster::GetActiveSystem()->GetInstalledMemory();
		CommitScale = CCluster::GetActiveSystem()->GetMemoryLimit();
		if (CommitScale == 0)
			CommitScale = TotalMemory;
	}

	if(TotalSwap > 0)
		m_pStausMEM->setText(tr("Memory: %1/%2/(%3 + %4)    ").arg(FormatSize(RamUsage)).arg(FormatSize(CommitedMemory)).arg(FormatSize(InstalledMemory)).arg(FormatSize(TotalSwap)));
	else
		m_pStausMEM->setText(tr("Memory: %1/%2/%3    ").arg(FormatSize(RamUsage)).arg(FormatSize(CommitedMemory)).arg(FormatSize(InstalledMemory)));

	QStringList MemInfo;
	MemInfo.append(tr("Installed: %1").arg(FormatSize(InstalledMemory)));
	MemInfo.append(tr("Swap: %1").arg(FormatSize(TotalSwap)));
	MemInfo.append(tr("Commited: %1").arg(FormatSize(CommitedMemory)));
	MemInfo.append(tr("Physical: %1").arg(FormatSize(RamUsage)));
	m_pStausMEM->setToolTip(MemInfo.join("\r\n"));


	SSysStats SysStats = CCluster::GetActiveSystem()->GetStats();

	QString IO;
	IO += tr("R: %1").arg(FormatRate(qMax(SysStats.Io.ReadRate.Get(), qMax(SysStats.MMapIo.ReadRate.Get(), SysStats.Disk.ReadRate.Get()))));
	IO += " ";
	IO += tr("W: %1").arg(FormatRate(qMax(SysStats.Io.WriteRate.Get(), qMax(SysStats.MMapIo.WriteRate.Get(), SysStats.Disk.WriteRate.Get()))));
	m_pStausIO->setText(IO + "    ");

	QStringList IOInfo;
	IOInfo.append(tr("FileIO; Read: %1; Write: %2; Other: %3").arg(FormatRate(SysStats.Io.ReadRate.Get())).arg(FormatRate(SysStats.Io.WriteRate.Get())).arg(FormatRate(SysStats.Io.OtherRate.Get())));
	IOInfo.append(tr("MMapIO; Read: %1; Write: %2").arg(FormatRate(SysStats.MMapIo.ReadRate.Get())).arg(FormatRate(SysStats.MMapIo.WriteRate.Get())));
#ifdef WIN32
	if(CCluster::GetActiveSystem()->HasCapability(CSystemAPI::eCapExtProcInfo) || CCluster::GetActiveSystem()->IsMonitoringETW())
		IOInfo.append(tr("DiskIO; Read: %1; Write: %2").arg(FormatRate(SysStats.Disk.ReadRate.Get())).arg(FormatRate(SysStats.Disk.WriteRate.Get())));
#endif
	m_pStausIO->setToolTip(IOInfo.join("\r\n"));

	CNetMonitor* pNetMonitor = CCluster::GetActiveSystem()->GetNetMonitor();

	CNetMonitor::SDataRates NetRates;
	CNetMonitor::SDataRates RasRates;
	if (pNetMonitor)
	{
		NetRates = pNetMonitor->GetTotalDataRate(CNetMonitor::eNet);
		RasRates = pNetMonitor->GetTotalDataRate(CNetMonitor::eRas);
	}
	else
	{
		//
		// No adapter list from a machine this process does not collect from, but
		// its total throughput does cross the wire - see API_SYS_NETRECVBYTES.
		// A different measurement of the same thing, and closer to the truth
		// than a flat zero next to a machine that is plainly busy. RAS has no
		// such counter and stays empty.
		//
		NetRates.ReceiveRate = SysStats.Net.ReceiveRate.Get();
		NetRates.SendRate = SysStats.Net.SendRate.Get();
	}

	QString Net;
	Net += tr("D: %1").arg(FormatRate(NetRates.ReceiveRate));
	Net += " ";
	Net += tr("U: %1").arg(FormatRate(NetRates.SendRate));
	m_pStausNET->setText(Net + "    ");

	QStringList NetInfo;
	NetInfo.append(tr("TCP/IP; Download: %1; Upload: %2").arg(FormatRate(NetRates.ReceiveRate)).arg(FormatRate(NetRates.SendRate)));
	NetInfo.append(tr("VPN/RAS; Download: %1; Upload: %2").arg(FormatRate(RasRates.ReceiveRate)).arg(FormatRate(RasRates.SendRate)));
	m_pStausNET->setToolTip(NetInfo.join("\r\n"));


	
	if (!m_pTrayIcon->isVisible())
		return;

	QString TrayInfo = tr("Task Explorer\r\nCPU: %1%\r\nRam: %2%").arg(int(100 * CCluster::GetActiveSystem()->GetCpuUsage()))
		.arg(InstalledMemory > 0 ? (int)100 * RamUsage / InstalledMemory : 0);
	if (TotalSwap > 0)
		TrayInfo.append(tr("\r\nSwap: %1%").arg((int)100 * SwapedMemory / TotalSwap));

	m_pTrayIcon->setToolTip(TrayInfo);

	QString TrayGraphMode = theConf->GetString("SysTray/GraphMode", "CpuMem");

	int MemMode = 0;
	if (TrayGraphMode.compare("Cpu", Qt::CaseInsensitive) == 0)
		;
	else if (TrayGraphMode.compare("CpuMem", Qt::CaseInsensitive) == 0)
		MemMode = 3; // All in one bar
	else if (TrayGraphMode.compare("CpuMem1", Qt::CaseInsensitive) == 0)
		MemMode = 1; // ram only
	else if (TrayGraphMode.compare("CpuMem2", Qt::CaseInsensitive) == 0)
		MemMode = 2; // ram and swap in two columns
	else
	{
		if (m_pTrayGraph) 
		{
			m_pTrayGraph->deleteLater();
			m_pTrayGraph = NULL;

			QIcon Icon;
			Icon.addFile(":/TaskExplorer.png");
			m_pTrayIcon->setIcon(Icon);
		}
		return;
	}
	
	QImage TrayIcon = QImage(16, 16, QImage::Format_RGB32);
	
	{
		QPainter qp(&TrayIcon);

		int offset = 0;

		float hVal = TrayIcon.height();

		if (MemMode == 2 && InstalledMemory > 0 && TotalSwap > 0) // if mode == 2 but TotalSwap == 0 default to mode == 1
		{
			offset = 6;
			qp.fillRect(0, 0, offset, TrayIcon.height(), Qt::black);

			float used_x = hVal * CCluster::GetActiveSystem()->GetPhysicalUsed() / InstalledMemory;

			qp.setPen(QPen(Qt::cyan, 2));
			qp.drawLine(3, (hVal+1), 3, (hVal+1) - used_x);

			float swaped_x = hVal * SwapedMemory / TotalSwap;

			qp.setPen(QPen(Qt::yellow, 2));
			qp.drawLine(1, (hVal+1), 1, (hVal+1) - swaped_x);
		}
		else if (MemMode != 3 && InstalledMemory > 0)
		{
			offset = 3;
			qp.fillRect(0, 0, offset, TrayIcon.height(), Qt::black);

			float used_x = hVal * CCluster::GetActiveSystem()->GetPhysicalUsed() / InstalledMemory;

			qp.setPen(QPen(Qt::cyan, 2));
			qp.drawLine(1, (hVal+1), 1, (hVal+1) - used_x);
		}
		else if(TotalMemory > 0) // TaskExplorer Mode
		{
			offset = 3;
			qp.fillRect(0, 0, offset, TrayIcon.height(), Qt::black);

			float used_x = hVal * RamUsage / TotalMemory;
			float virtual_x = hVal * (RamUsage + SwapedMemory) / TotalMemory;
			float commited_x = hVal * CommitedMemory / CommitScale;

			// RAM + swapped can exceed installed RAM, so clamp rather than
			// drawing past the top of the icon.
			if (virtual_x > hVal) virtual_x = hVal;
			if (commited_x > hVal) commited_x = hVal;

			qp.setPen(QPen(Qt::yellow, 2));
			qp.drawLine(1, (hVal+1), 1, (hVal+1) - commited_x);

			qp.setPen(QPen(Qt::red, 2));
			qp.drawLine(1, (hVal+1), 1, (hVal+1) - virtual_x);

			qp.setPen(QPen(Qt::cyan, 2));
			qp.drawLine(1, (hVal+1), 1, (hVal+1) - used_x);
		}

		ASSERT(TrayIcon.width() > offset);

		if (m_pTrayGraph == NULL)
		{
			m_pTrayGraph = new CHistoryGraph(true, QColor(0, 128, 0), this);
			m_pTrayGraph->AddValue(0, Qt::green);
			m_pTrayGraph->AddValue(1, Qt::red);
			m_pTrayGraph->AddValue(2, Qt::blue);
		}

		// Note: we may add an cuttof show 0 below 10%
		m_pTrayGraph->SetValue(0, CCluster::GetActiveSystem()->GetCpuUsage());
		m_pTrayGraph->SetValue(1, CCluster::GetActiveSystem()->GetCpuKernelUsage());
		m_pTrayGraph->SetValue(2, CCluster::GetActiveSystem()->GetCpuDPCUsage());

		m_pTrayGraph->Update(TrayIcon.height(), TrayIcon.width() - offset);


		QImage TrayGraph = m_pTrayGraph->GetImage();
		qp.translate(TrayIcon.width() - TrayGraph.height(), TrayIcon.height());
		qp.rotate(270);
		qp.drawImage(0, 0, TrayGraph);
	}

	// todo memory leak???
	m_pTrayIcon->setIcon(QIcon(QPixmap::fromImage(TrayIcon)));
}

bool CTaskExplorer::CheckErrors(QList<STATUS> Errors)
{
	//
	// Keep only what actually went wrong.
	//
	// This used to test whether the list was empty, which is not the same
	// question: callers pass the result of an operation, not a list of
	// failures, so the list was never empty and a successful call raised an
	// error box reporting "0x00000000 STATUS_SUCCESS" with no message. Doing it
	// here rather than at each call site because three of the six callers hand
	// over an unfiltered status.
	//
	// A cancellation goes too. The user declining a prompt is an answer, and
	// telling them what they just chose is noise; see TE_UserCanceled.
	//
	QList<STATUS> Real;
	foreach(const STATUS& Status, Errors)
	{
		if (Status.IsError() && Status.GetMsgCode() != TE_UserCanceled)
			Real.append(Status);
	}

	if (Real.isEmpty())
		return true;

	//
	// One failure is a sentence, not a table.
	//
	// The list earns its place when several things went wrong and have to be
	// compared - which of five processes refused, and why each. For a single
	// status it is a header row, one line, and an empty grid around it, with
	// the message itself cut off at whatever width the column happened to
	// take. A message box says the same thing and says it plainly.
	//
	if (Real.count() == 1)
	{
		const STATUS& Error = Real.first();
		QString Text = FormatError(Error);

		//
		// And the platform's own words underneath, where there are any at all.
		// ERROR_UNDEFINED means the failure was decided here rather than
		// reported by the machine - see CMultiErrorDialog, which draws its two
		// native columns on the same condition.
		//
		if (Error.GetStatus() != ERROR_UNDEFINED)
		{
			const QString Native = theSystem->GetStatusMessage(Error.GetStatus());
			Text += "\n\n" + tr("0x%1: %2")
				.arg((quint32)Error.GetStatus(), 8, 16, QChar('0')).arg(Native);
		}

		QMessageBox::warning(theGUI, tr("TaskExplorer - Error"), Text);
		return true;
	}

	CMultiErrorDialog Dialog(tr("Operation failed for %1 item(s).").arg(Real.size()), Real);
	return !!Dialog.exec();
}

QString CTaskExplorer::FormatID(quint64 ID) const
{
	return ::FormatID(ID);
}

//
// The API reports the driver's trust level as a number and its complaints as
// flags; the wording lives here so it is in the reader's language rather than
// the collector's.
//
//
// [KTE++] and its lesser forms.
//
// The badge says two things at once: how far the driver trusts this process,
// and whether it knows this kernel build well enough to be useful. A driver
// that loaded but could not be given the version-specific offsets answers a
// fraction of what it otherwise would, which is worth a word rather than a
// silently smaller number.
//
QString CTaskExplorer::GetKernelBadge(const CSystemAPI::SKernelDriver& Driver)
{
	if (!Driver.Connected)
		return QString();

	static const char* Badges[] = { "---", "--", "-", "~", "+", "++" };
	const QString Level = Badges[qBound(0, Driver.Level, 5)];
	return tr("[%1KTE%2]").arg(Driver.DynDataLoaded ? QString() : tr("Limited ")).arg(Level);
}

QString CTaskExplorer::GetKernelLevelString(int Level)
{
	switch (Level)
	{
	case CSystemAPI::eKernelLevelNone:	return tr("None");
	case CSystemAPI::eKernelLevelMin:	return tr("Minimal");
	case CSystemAPI::eKernelLevelLow:	return tr("Low");
	case CSystemAPI::eKernelLevelMed:	return tr("Medium");
	case CSystemAPI::eKernelLevelHigh:	return tr("High");
	case CSystemAPI::eKernelLevelMax:	return tr("Maximum");
	}
	return tr("N/A");
}

QStringList CTaskExplorer::GetKernelWeaknessStrings(quint32 Weaknesses)
{
	QStringList Info;
	if (Weaknesses & CSystemAPI::eKsiNotSecurelyCreated)	Info.append(tr("not securely created"));
	if (Weaknesses & CSystemAPI::eKsiUnverifiedImage)	Info.append(tr("unverified primary image"));
	if (Weaknesses & CSystemAPI::eKsiInactiveProtections) Info.append(tr("inactive protections"));
	if (Weaknesses & CSystemAPI::eKsiUntrustedImages)	Info.append(tr("unsigned images (likely an unsigned plugin)"));
	if (Weaknesses & CSystemAPI::eKsiBeingDebugged)		Info.append(tr("process is being debugged"));
	if (Weaknesses & CSystemAPI::eKsiWritableFileObject)Info.append(tr("writable file object"));
	if (Weaknesses & CSystemAPI::eKsiNoCreateNotification) Info.append(tr("missing create notification"));
	if (Weaknesses & CSystemAPI::eKsiTamperedImage)		Info.append(tr("tampered primary image"));
	return Info;
}


//
// The machine the machine-level menus act on.
//
// Checked against the cluster rather than trusted: a connection can be dropped
// while its menu is open, and a pointer to a system that has gone would be a
// crash where an obvious fallback exists. The local machine is always there.
//
CSystemAPI* CTaskExplorer::GetActionSystem() const
{
	if (m_pActionSystem)
	{
		foreach(const CSystemPtr& pSystem, CCluster::GetSystems())
		{
			if (pSystem.data() == m_pActionSystem)
				return m_pActionSystem;
		}
	}
	return theSystem.data();
}

QMenu* CTaskExplorer::GetComputerMenu(CSystemAPI* pSystem)
{
	SetActionSystem(pSystem);
	return m_pMenuComputer;
}

QMenu* CTaskExplorer::GetUsersMenu(CSystemAPI* pSystem)
{
	SetActionSystem(pSystem);

	//
	// Rebuilt here rather than left to the refresh timer, because the menu is
	// about to be shown for a machine it may not currently be describing.
	//
	UpdateUserMenu();
	return m_pMenuUsers;
}

//
// The Tasks menu is this computer's, so opening it says so.
//
// Without this the target would still be whichever machine branch was last
// right-clicked, and "Shutdown" in the menu bar would shut down that one - the
// exact confusion the branch menu used to avoid by hiding itself.
//
void CTaskExplorer::OnTaskMenu()
{
	SetActionSystem(NULL);
	UpdateUserMenu();
}

void CTaskExplorer::OnRun()
{
	CRunDialog* pWnd = new CRunDialog(GetActionSystem());
	pWnd->show();
}

void CTaskExplorer::OnRunAs()
{
	CRunAsDialog* pWnd = new CRunAsDialog(GetActionSystem());
	pWnd->show();
}

void CTaskExplorer::OnRunSys()
{
	CheckErrors(QList<STATUS>() << theSystem->ShowRunDialog(CSystemAPI::eRunAsTrustedInstaller));
}

#ifndef WIN32
//
// Turns the privileged helper on or off.
//
// Enabling it authenticates straight away rather than waiting for the next
// refresh to need something, so that the password prompt appears while the user
// still has the menu click in mind. If authentication fails or is cancelled the
// toggle goes back off - leaving it on would mean re-prompting on every refresh.
//
void CTaskExplorer::OnUseHelper()
{
	const bool bEnable = m_pMenuUseHelper->isChecked();
	theConf->SetValue("Options/UseTaskHelper", bEnable);

	if (!bEnable)
	{
		CTaskService::TerminateWorkers();
		return;
	}

	//
	// The prompt is modal and can sit there for as long as the user needs, so
	// the wait cursor is the honest indication that something is pending.
	//
	QApplication::setOverrideCursor(Qt::WaitCursor);
	const bool bOk = !CTaskService::RunWorker(true).isEmpty();
	QApplication::restoreOverrideCursor();

	if (!bOk)
	{
		m_pMenuUseHelper->setChecked(false);
		theConf->SetValue("Options/UseTaskHelper", false);
		QMessageBox::warning(this, "TaskExplorer", tr("The privileged helper could not be started."));
	}
}
#endif

void CTaskExplorer::OnElevate()
{
#ifdef WIN32
	STATUS Status = theSystem->RestartElevated();
	if (Status.IsError())
	{
		//
		// Declining the UAC prompt is an answer, not a fault, and a box telling
		// the user what they just chose is noise. Anything else is worth saying
		// out loud.
		//
		if (Status.GetMsgCode() != TE_UserCanceled)
			QMessageBox::warning(this, "TaskExplorer", CTaskExplorer::FormatError(Status));
		return;
	}
	OnExit();
#else
	//
	// Relaunch ourselves through a graphical privilege escalation helper.
	//
	// -multi is passed because the running instance has not exited yet, and
	// without it the new process would just signal this one and quit.
	//
	qint64 Pid = 0;
	STATUS Status = LinuxRunElevated(QCoreApplication::applicationFilePath(),
	                                 QStringList() << "-multi", &Pid);
	if (Status.IsError())
	{
		QMessageBox::warning(this, "TaskExplorer", CTaskExplorer::FormatError(Status));
		return;
	}

	//
	// A pid of zero means elevation went through a terminal, so what started was
	// the terminal and the elevated instance only exists once a password has been
	// typed. There is nothing to test the liveness of, and closing this instance
	// on the strength of it could leave the user with neither.
	//
	if (Pid == 0)
	{
		QMessageBox::information(this, "TaskExplorer",
			tr("A terminal has been opened to ask for your password. Once the elevated "
			   "Task Explorer appears you can close this one."));
		return;
	}

	//
	// Wait for the authentication to succeed before closing this instance, and do
	// not guess at how long that takes.
	//
	// pkexec names its *parent* as the polkit subject, so exiting while the prompt
	// is still on screen invalidates the whole request: the prompt vanishes and no
	// elevated process is ever started. A fixed short delay did exactly that to
	// anyone who took longer than a couple of seconds to type a password - both
	// windows disappeared and nothing came back.
	//
	// "Still alive" cannot be the signal either, since that is equally true of a
	// process sitting on an unanswered prompt. What is observable is the uid: see
	// LinuxElevationChildIsElevated.
	//
	const qint64 Deadline = QDateTime::currentMSecsSinceEpoch() + 180 * 1000;

	QTimer* pWait = new QTimer(this);
	pWait->setInterval(400);

	connect(pWait, &QTimer::timeout, this, [this, pWait, Pid, Deadline]() {
		int ExitCode = 0;
		const bool bAlive = LinuxElevationChildAlive(Pid, &ExitCode);

		if (bAlive && LinuxElevationChildIsElevated(Pid))
		{
			// Authenticated and running as root; this instance is redundant.
			pWait->stop();
			pWait->deleteLater();
			OnExit();
			return;
		}

		if (!bAlive)
		{
			pWait->stop();
			pWait->deleteLater();

			//
			// It died without ever becoming root: cancelled, refused, or unable to
			// start. The helper's own stderr says which, so it is shown rather
			// than guessed at.
			//
			QString Message = tr("Could not restart elevated. The authentication prompt was cancelled, or the "
			                     "elevated process failed to start.");

			const QString Reason = LinuxLastElevationError();
			if (!Reason.isEmpty())
				Message += "\n\n" + Reason;
			else if (ExitCode != 0)
				Message += "\n\n" + tr("The helper exited with code %1.").arg(ExitCode);

			QMessageBox::warning(this, "TaskExplorer", Message);
			return;
		}

		//
		// Still waiting on the prompt. Eventually stop watching rather than poll
		// for ever, and leave this instance running - it is the only one there is.
		//
		if (QDateTime::currentMSecsSinceEpoch() > Deadline)
		{
			pWait->stop();
			pWait->deleteLater();

			QMessageBox::information(this, "TaskExplorer",
				tr("Still waiting for authentication. If the elevated Task Explorer starts, "
				   "you can close this one."));
		}
	});

	pWait->start();
#endif
}

void CTaskExplorer::OnExit()
{
	m_bExit = true;
	close();
}

void CTaskExplorer::OnComputerAction()
{
	if (sender() != m_pMenuLock && sender() != m_pComputerButton)
	{
		if (QMessageBox("TaskExplorer", tr("Do you really want to %1?").arg(((QAction*)sender())->text()), QMessageBox::Warning, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
			return;
	}

	int SoftForce = theConf->GetInt("Options/UseSoftForce", 2);
	if (QApplication::keyboardModifiers() & Qt::ControlModifier)
		SoftForce = 0;

	//
	// Which action, and how hard to push it; the platform layer does the rest.
	//
	CSystemAPI::EPowerAction Action;
	bool bForce = false;

	if (sender() == m_pMenuLock || sender() == m_pComputerButton)
		Action = CSystemAPI::ePowerLock;
	else if (sender() == m_pMenuLogOff)
		Action = CSystemAPI::ePowerLogOff;
	else if (sender() == m_pMenuSleep)
		Action = CSystemAPI::ePowerSleep;
	else if (sender() == m_pMenuHibernate)
		Action = CSystemAPI::ePowerHibernate;
	else if (sender() == m_pMenuRestart)
		Action = CSystemAPI::ePowerRestart;
	else if (sender() == m_pMenuForceRestart)
		{ Action = CSystemAPI::ePowerRestart; bForce = true; }
	else if (sender() == m_pMenuRestartEx)
		Action = CSystemAPI::ePowerRestartToOptions;
	else if (sender() == m_pMenuShutdown)
		Action = CSystemAPI::ePowerShutdown;
	else if (sender() == m_pMenuForceShutdown)
		{ Action = CSystemAPI::ePowerShutdown; bForce = true; }
	else if (sender() == m_pMenuHybridShutdown)
		Action = CSystemAPI::ePowerHybridShutdown;
	else
		return;

	STATUS Status = GetActionSystem()->PowerAction(Action, bForce, SoftForce);
	if (Status.IsError())
		QMessageBox::critical(NULL, "TaskExplorer", tr("Failed to %1, due to: %2")
			.arg(((QAction*)sender())->text()).arg(CTaskExplorer::FormatError(Status)));
}

void CTaskExplorer::UpdateUserMenu()
{
	//
	// Of whichever machine the menu currently stands for. The submenu is
	// borrowed by the machine branches in the tree, so between one being opened
	// and the Tasks menu being opened again this is somebody else's session
	// list - which is the point.
	//
	CSystemAPI* pSystem = GetActionSystem();

	QList<CSystemAPI::SUser> Users = pSystem->GetUsers();
	QSet<QString> UserNames;

	m_pMenuUsers->setTitle(tr("Users (%1)").arg(Users.size()));

	QMap<QString, QMenu*> OldMenus = m_UserMenus;
	m_UserMenus.clear();

	foreach(const CSystemAPI::SUser& User, Users)
	{
		UserNames.insert(User.UserName);

		//
		// What the platform calls this session, falling back to the number for a
		// server too old to send it. Suffixed with the name, so that even two
		// sessions a platform reports identically get one entry each rather than
		// one entry twice.
		//
		const QString Key = (User.SessionKey.isEmpty()
			? QString::number(User.SessionId) : User.SessionKey) + "/" + User.UserName;

		//
		// Rebuilt into a fresh map rather than edited in place, so a key that
		// somehow repeats cannot overwrite a menu that is still in the widget and
		// leave it untracked - which is exactly how this leaked one menu per
		// refresh. A repeat now reuses the entry and the list stays the length
		// the machine says it is.
		//
		QMenu* pMenu = OldMenus.take(Key);
		if (!pMenu)
			pMenu = m_UserMenus.value(Key);
		if (!pMenu)
		{
			pMenu = m_pMenuUsers->addMenu(MakeActionIcon(":/Actions/User"), QString());

			pMenu->addAction(MakeActionIcon(":/Actions/Connect"), tr("Connect"), this, SLOT(OnUserAction()))->setProperty("Action", CSystemAPI::eUserConnect);
			pMenu->addAction(MakeActionIcon(":/Actions/Disconnect"), tr("Disconnect"), this, SLOT(OnUserAction()))->setProperty("Action", CSystemAPI::eUserDisconnect);
			pMenu->addAction(MakeActionIcon(":/Actions/Logoff"), tr("Logoff"), this, SLOT(OnUserAction()))->setProperty("Action", CSystemAPI::eUserLogoff);
			/*pMenu->addSeparator();
			pMenu->addAction(MakeActionIcon(":/Actions/SendMsg"), tr("Send message..."), this, SLOT(OnUserAction()))->setProperty("Action", eSendMessage);
			pMenu->addAction(MakeActionIcon(":/Actions/UserInfo"), tr("Properties"), this, SLOT(OnUserAction()))->setProperty("Action", eUserInfo);*/

		}

		m_UserMenus.insert(Key, pMenu);

		//
		// Set every round, not only when the menu is made: it is what
		// UserSessionAction is given, and a reused menu whose session was
		// renumbered would otherwise act on the number it had when it was first
		// seen.
		//
		pMenu->setProperty("SessionId", User.SessionId);

		//
		// Titled with what the platform calls the session, not with the number:
		// "c1" is the answer on a machine that says c1, and 0 is not.
		//
		pMenu->setTitle(tr("%1: %2 (%3)")
			.arg(User.SessionKey.isEmpty() ? QString::number(User.SessionId) : User.SessionKey)
			.arg(User.UserName).arg(::GetSessionStateString(User)));
	}

	//
	// Whatever was not claimed above belongs to a session that has gone.
	//
	foreach(QMenu* pOld, OldMenus)
		delete pOld;

	//
	// The filter is about the tree, which is about this computer's accounts
	// whatever the menu happens to be showing - so it is fed from theSystem and
	// not from the target.
	//
	QSet<QString> LocalNames;
	foreach(const CSystemAPI::SUser& User, theSystem->GetUsers())
		LocalNames.insert(User.UserName);

	CProcessFilterModel* pFilter = (CProcessFilterModel*)m_pProcessTree->GetModel();
	pFilter->UpdateUsers(LocalNames);
}

void CTaskExplorer::OnUserAction()
{
	QAction* pAction = (QAction*)sender();
	int SessionId = pAction->parent()->property("SessionId").toInt();

	CSystemAPI::EUserAction Action = (CSystemAPI::EUserAction)pAction->property("Action").toInt();

	//
	// Connecting is tried without a password first; most of the time there is
	// none, and asking for one that is not needed is worse than one retry.
	//
	CSystemAPI* pSystem = GetActionSystem();

	STATUS Status = pSystem->UserSessionAction(SessionId, Action);
	if (Status.IsError() && Action == CSystemAPI::eUserConnect)
	{
		QString Password = QInputDialog::getText(this, "TaskExplorer", tr("Connect to session, enter Password:"), QLineEdit::Password);
		if (!Password.isEmpty())
			Status = pSystem->UserSessionAction(SessionId, Action, Password);
	}

	if (Status.IsError())
		QMessageBox::critical(NULL, "TaskExplorer", tr("Failed to %1, due to: %2")
			.arg(pAction->text()).arg(CTaskExplorer::FormatError(Status)));
}

void CTaskExplorer::OnSysTray(QSystemTrayIcon::ActivationReason Reason)
{
	static bool TriggerSet = false;
	static bool NullifyTrigger = false;
	switch(Reason)
	{
		case QSystemTrayIcon::Context:
			m_pTrayMenu->popup(QCursor::pos());	
			break;
		case QSystemTrayIcon::DoubleClick:
#ifndef WIN32
			//
			// Ignored on Linux - the Trigger case below does the work.
			//
			// A StatusNotifierItem tray (Plasma, and anything else using the
			// modern D-Bus protocol) never delivers DoubleClick at all: Qt only
			// emits Trigger for a left click on an SNI item. Relying on
			// DoubleClick therefore left no way to get the window back.
			//
			// On a tray that does deliver it, Trigger arrives first, so acting
			// on both would toggle the window twice and leave it as it was.
			//
			break;
#else
			if (isVisible())
			{
				if(TriggerSet)
					NullifyTrigger = true;
				hide();
				break;
			}
			show();
#endif
		case QSystemTrayIcon::Trigger:
#ifdef WIN32
			if (isVisible() && !TriggerSet)
			{
				TriggerSet = true;
				QTimer::singleShot(100, [this]() {
					TriggerSet = false;
					if (NullifyTrigger) {
						NullifyTrigger = false;
						return;
					}
					setWindowState(Qt::WindowActive);
					SetForegroundWindow((HWND)winId());
				} );
			}
#else
			//
			// Single click toggles, which is the convention on Linux desktops
			// and the only activation an SNI tray reports.
			//
			if (isVisible() && !isMinimized())
			{
				hide();
			}
			else
			{
				show();
				// Clear the minimized bit explicitly; show() alone restores a
				// hidden window but not a minimized one.
				setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
				raise();
				activateWindow();
			}
#endif
			break;
	}
}

void CTaskExplorer::OnSysTab()
{
	QAction* pAction = (QAction*)sender();
	int Index = m_Act2Tab.value(pAction);
	m_pSystemInfo->ShowTab(Index, pAction->isChecked());
}

/*void CTaskExplorer::OnKernelServices()
{
#ifdef WIN32
	theConf->SetValue("MainWindow/ShowDrivers", m_pMenuKernelServices->isChecked());
	m_pSystemInfo->SetShowKernelServices(m_pMenuKernelServices->isChecked());
#endif
}*/

void CTaskExplorer::OnTaskTab()
{
	QAction* pAction = (QAction*)sender();
	int Index = m_Act2Tab.value(pAction);
	m_pTaskInfo->ShowTab(Index, pAction->isChecked());
}

void CTaskExplorer::OnSystemInfo()
{
	CSystemInfoWindow* pSystemInfoWindow = new CSystemInfoWindow();
	pSystemInfoWindow->show();
}

void CTaskExplorer::OnSettings()
{
	CSettingsWindow* pSettingsWindow = new CSettingsWindow();
	connect(pSettingsWindow, SIGNAL(OptionsChanged()), this, SLOT(UpdateOptions()));
	pSettingsWindow->show();
}

void CTaskExplorer::OnDriverConf()
{
#ifdef WIN32
	CDriverWindow* pDriverWindow = new CDriverWindow();
	pDriverWindow->show();
#endif
}

void CTaskExplorer::OnMessage(const QString& Message)
{
	if (Message == "ShowWnd")
	{
		if (!isVisible())
			show();
		setWindowState(Qt::WindowActive);
#ifdef WIN32
		SetForegroundWindow((HWND)winId());
#else
		raise();
		activateWindow();
#endif
	}
}

void CTaskExplorer::ApplyOptions()
{
	if (theConf->GetBool("Options/ShowGrid", true))
		m_pCustomItemDelegate->SetGridColor(QColor(theConf->GetString("Colors/GridColor", "#808080")));
	else
		m_pCustomItemDelegate->SetGridColor(Qt::transparent);

	CAbstractInfoEx::SetHighlightTime(theConf->GetUInt64("Options/HighlightTime", 2500));
	CAbstractInfoEx::SetPersistenceTime(theConf->GetUInt64("Options/PersistenceTime", 5000));

	CPanelView::SetSimpleFormat(theConf->GetBool("Options/PanelCopySimple", false));
	CPanelView::SetMaxCellWidth(theConf->GetInt("Options/PanelCopyMaxCellWidth", 0));
	CPanelView::SetCellSeparator(UnEscape(theConf->GetString("Options/PanelCopyCellSeparator", "\\t")));

	//
	// The view modes, here rather than in UpdateOptions so that they also apply
	// at startup - UpdateOptions calls this first.
	//
	m_pProcessTree->SetMultiUser(theConf->GetBool("Options/MultiUser", false));

	//
	// Cluster mode is two things at once: the tree gains a machine level, and
	// theCluster starts existing so that GetSystems() can answer with more than
	// the local machine. Turning it off does not disconnect anything - the
	// connections stay, they simply stop being drawn separately - so that
	// flipping the switch is not a destructive act.
	//
	//
	// The mode before the connections, because CCluster::Connect asks for it:
	// without the machine layer a new connection is what the window is now
	// about, with it on the selection is left alone. EnsureCluster reconnects
	// everything saved, so doing it the other way round meant every machine
	// restored at startup moved the selection - which in cluster mode it is
	// not supposed to do, and which left the graph bar and the status line
	// reporting whichever machine happened to connect last.
	//
	const bool bCluster = CCluster::ClusterModeWanted();
	CCluster::SetClusterMode(bCluster);
	if (bCluster)
		EnsureCluster();
	m_pProcessTree->SetMultiMachine(bCluster);

	//
	// The button shows what is in force, not what was asked for. Without
	// TaskRemote the two differ - the stored value is kept and ignored - and
	// a switch that stays down while nothing happened would be a lie.
	//
	if (m_pMenuClusterMode)
	{
		m_pMenuClusterMode->setChecked(bCluster);
		m_pMenuClusterMode->setEnabled(CRemoteLoader::IsAvailable());
	}

	UpdateTargetMenu();

	//
	// The status line's machine label and the panel's follow whether there is
	// more than one machine to tell apart, which this may just have changed.
	//
	OnViewSystemChanged();
}

void CTaskExplorer::UpdateOptions()
{
	ApplyOptions();

	//
	// The same switch as the View menu entry, so whichever one was used the
	// other agrees with it. SetMultiUser does nothing when the value has not
	// changed, which is what keeps a settings apply from rebuilding the tree
	// for no reason.
	//
	if (m_pMenuShowUnixSockets)
		m_pMenuShowUnixSockets->setChecked(theConf->GetBool("Options/ShowUnixSockets", false));


	ReloadColors();

	SetUITheme();

	if(theConf->GetBool("SysTray/Show", true))
		m_pTrayIcon->show();
	else
		m_pTrayIcon->hide();

	killTimer(m_uTimerID);
	m_uTimerID = startTimer(theConf->GetInt("Options/RefreshInterval", 1000));

	emit ReloadPlots();

	ResetAll();
}

void CTaskExplorer::ResetAll()
{
	//
	// Every machine, not only the one this window started with.
	//
	// CCluster::GetSystems answers with the local system alone when there is no
	// cluster, so this is the same call in both cases - but with machines
	// connected, a reset that only reached the local one left every remote panel
	// showing what it already had. See CRemoteSystem::ResetAll for what a remote
	// machine has to do about it beyond clearing its own lists.
	//
	foreach(const CSystemPtr& pSystem, CCluster::GetSystems())
		pSystem->ResetAll();

	emit ReloadPanels();

	QTimer::singleShot(0, this, SLOT(UpdateAll()));
}

//void CTaskExplorer::OnUseDriver()
//{
//	theConf->SetValue("OptionsKSI/KsiEnable", m_pMenuUseDriver->isChecked());
//}

void CTaskExplorer::OnAutoRun()
{
#ifdef WIN32
	AutorunEnable(m_pMenuAutoRun->isChecked());
#endif
}

void CTaskExplorer::OnSkipUAC()
{
#ifdef WIN32
	SkipUacEnable(m_pMenuUAC->isChecked());
#endif
}

void CTaskExplorer::OnCreateService()
{
	CNewService* pWnd = new CNewService();
	pWnd->show();
}

void CTaskExplorer::OnReloadService()
{
	QMetaObject::invokeMethod(theSystem.data(), "UpdateServiceList", Qt::QueuedConnection, Q_ARG(bool, true));
}

void CTaskExplorer::OnSCMPermissions()
{
	ShowSecurity(theSystem->GetSecurityObject(CSystemAPI::eSecServiceManager, QString()), this);
}

void CTaskExplorer::OnPersistenceOptions()
{
	CPersistenceConfig dialog;
	dialog.exec();
}

void CTaskExplorer::OnSecurityExplorer()
{
#ifdef WIN32
	CSecurityExplorer* pWnd = new CSecurityExplorer();
	pWnd->show();
#endif
}

void CTaskExplorer::OnFreeMemory()
{
	CSystemAPI::EMemoryCommand Command = CSystemAPI::eMemCombinePages;

#ifdef WIN32	// freeing memory goes through the driver
	if (sender() == m_pMenuFreeWorkingSet)
		Command = CSystemAPI::eMemEmptyWorkingSets;
	else if (sender() == m_pMenuFreeModPages)
		Command = CSystemAPI::eMemFlushModifiedList;
	else if (sender() == m_pMenuFreeStandby)
		Command = CSystemAPI::eMemPurgeStandbyList;
	else if (sender() == m_pMenuFreePriority0)
		Command = CSystemAPI::eMemPurgeLowPriorityStandby;
#endif

	QApplication::setOverrideCursor(Qt::WaitCursor);
	STATUS Status = theSystem->MemoryCommand(Command);
	QApplication::restoreOverrideCursor();

	if (Status.IsError())
		QMessageBox::warning(NULL, "TaskExplorer", tr("Memory operation failed; Error: %1").arg(CTaskExplorer::FormatError(Status)));
}

void CTaskExplorer::OnFindProcess()
{
	/*m_pHoldButton->setChecked(true);
	CAbstractInfoEx::SetPersistenceTime(60*60*1000);*/

#ifdef WIN32
	int count = theSystem->FindHiddenProcesses();
	if(count > 0)
		QMessageBox::warning(NULL, "TaskExplorer", tr("Found %1 hidden processes and added them to the process std::list.").arg(count));
	else
		QMessageBox::information(NULL, "TaskExplorer", tr("No hidden processes found."));
#endif
}

void CTaskExplorer::OnFindHandle()
{
	CHandleSearch* pHandleSearch = new CHandleSearch();
	pHandleSearch->show();
}

void CTaskExplorer::OnFindDll()
{
	CModuleSearch* pModuleSearch = new CModuleSearch();
	pModuleSearch->show();
}

void CTaskExplorer::OnFindMemory()
{
	CMemorySearch* pMemorySearch = new CMemorySearch();
	pMemorySearch->show();
}

void CTaskExplorer::OnMonitorSys()
{
#ifdef WIN32	// the menu entry that reaches this only exists where the driver does
	if (theSystem->SetSystemMonitor(m_pMenuMonitorSYS->isChecked()).IsError())
		return;

	theConf->SetValue("Options/MonitorSys", m_pMenuMonitorSYS->isChecked());
#endif
}

void CTaskExplorer::OnMonitorETW()
{
#ifdef WIN32
	if (m_pMenuMonitorETW->isChecked())
	{
		theSystem->MonitorETW(true);
		m_pMenuMonitorETW->setChecked(theSystem->IsMonitoringETW());
	}
	else
		theSystem->MonitorETW(false);
	theConf->SetValue("Options/MonitorETW", m_pMenuMonitorETW->isChecked());
#endif
}

void CTaskExplorer::OnMonitorFW()
{
#ifdef WIN32
	if (m_pMenuMonitorFW->isChecked())
	{
		theSystem->MonitorFW(true);
		m_pMenuMonitorFW->setChecked(theSystem->IsMonitoringFW());
	}
	else
		theSystem->MonitorFW(false);
	theConf->SetValue("Options/MonitorFirewall", m_pMenuMonitorFW->isChecked());
#endif
}

void CTaskExplorer::OnMonitorDbg()
{
#ifdef WIN32
	bool bChecked = false;
	int Mode = CSystemAPI::eDbgAll;
	if (sender() == m_pMenuMonitorDbgButton)
	{
		bChecked = !m_pMenuMonitorDbgButton->isChecked();
		if (bChecked)
		{
			Mode = CSystemAPI::eDbgLocal;
			if (theSystem->RootAvaiable())
				Mode |= CSystemAPI::eDbgGlobal;
			if (theSystem->GetKernelDriver().Connected)
				Mode |= CSystemAPI::eDbgKernel;
		}
	}
	else
	{
		QAction* pAction = (QAction*)sender();
		bChecked = pAction->isChecked();
		Mode = pAction->property("Mode").toInt();
	}

	int NewMode = theSystem->GetDebugMonitor();
	if (bChecked)
		NewMode |= Mode;
	else
		NewMode &= ~Mode;

	STATUS Status = theSystem->SetDebugMonitor(NewMode);
	if(Status.IsError())
		CTaskExplorer::CheckErrors(QList<STATUS>() << Status);

	int DbgMode = theSystem->GetDebugMonitor();
	if (sender() != m_pMenuMonitorDbgButton)
		m_pMenuMonitorDbgButton->setChecked((DbgMode & CSystemAPI::eDbgAll) != 0);
	m_pMenuMonitorDbgLocal->setChecked((DbgMode & CSystemAPI::eDbgLocal) != 0);
	m_pMenuMonitorDbgGlobal->setChecked((DbgMode & CSystemAPI::eDbgGlobal) != 0);
	m_pMenuMonitorDbgKernel->setChecked((DbgMode & CSystemAPI::eDbgKernel) != 0);

	m_Act2Tab.key(CTaskInfoView::eDebugView)->setChecked(DbgMode != CSystemAPI::eDbgNone);
	m_pTaskInfo->ShowTab(CTaskInfoView::eDebugView, DbgMode != CSystemAPI::eDbgNone);

	theConf->SetValue("Options/MonitorDbg", DbgMode);
#endif
}

void CTaskExplorer::OpenTaskInfoWnd(quint64 PID)
{
	CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow(QList<CProcessPtr>() << theSystem->GetProcessByID(PID));
	pTaskInfoWindow->show();
}

void CTaskExplorer::OnSplitterMoved()
{
	m_pMenuTaskTabs->setEnabled(m_pMainSplitter->sizes()[1] > 0);
	m_pMenuSysTabs->setEnabled(m_pMainSplitter->sizes()[1] > 0 && m_pPanelSplitter->sizes()[0] > 0);

	//m_pPanelSplitter->setVisible(m_pMainSplitter->sizes()[1] > 0);
}

QStyledItemDelegate* CTaskExplorer::GetItemDelegate() 
{
	return m_pCustomItemDelegate; 
}

/*float CTaskExplorer::GetDpiScale()
{
	return QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.0;// *100.0;
}*/

int CTaskExplorer::GetCellHeight()
{
	QFontMetrics fontMetrics(QApplication::font());
	int fontHeight = fontMetrics.height();
	
	return (fontHeight + 3);// *GetDpiScale();
}

void CTaskExplorer::LoadDefaultIcons()
{
	if (g_ExeIcon.isNull())
	{
		g_ExeIcon = QIcon(":/Icons/exe16.png");
		g_ExeIcon.addFile(":/Icons/exe32.png");
		g_ExeIcon.addFile(":/Icons/exe48.png");
		g_ExeIcon.addFile(":/Icons/exe64.png");
	}

	if (g_DllIcon.isNull())
	{
		g_DllIcon = QIcon(":/Icons/dll16.png");
		g_DllIcon.addFile(":/Icons/dll32.png");
		g_DllIcon.addFile(":/Icons/dll48.png");
		g_DllIcon.addFile(":/Icons/dll64.png");
	}
}

QVector<QColor> CTaskExplorer::GetPlotColors()
{
	static QVector<QColor> Colors;
	if (Colors.isEmpty())
	{
		Colors.append(Qt::red);
		Colors.append(Qt::green);
		Colors.append(Qt::blue);
		Colors.append(Qt::yellow);

		Colors.append("#f58231"); // Orange
		Colors.append("#911eb4"); // Purple
		Colors.append("#42d4f4"); // Cyan
		Colors.append("#f032e6"); // Magenta
		Colors.append("#bfef45"); // Lime
		Colors.append("#fabebe"); // Pink
		Colors.append("#469990"); // Teal
		Colors.append("#e6beff"); // Lavender
		Colors.append("#9A6324"); // Brown
		Colors.append("#fffac8"); // Beige
		Colors.append("#800000"); // Maroon
		Colors.append("#aaffc3"); // Mint
		Colors.append("#808000"); // Olive
		Colors.append("#ffd8b1"); // Acricot
		Colors.append("#000075"); // Navy

		Colors.append("#e6194B"); // Red
		Colors.append("#3cb44b"); // Green
		Colors.append("#4363d8"); // Yellow
		Colors.append("#ffe119"); // Blue
	}

	return Colors;
}

QColor CTaskExplorer::GetColor(int Color)
{
	return m_Colors.value((EColor)Color).Value;
}

QColor CTaskExplorer::GetListColor(int Color)
{
	return theGUI->m_Colors.value((EColor)Color).Value;
}

bool CTaskExplorer::UseListColor(int Color)
{
	return theGUI->m_Colors.value((EColor)Color).Enabled;
}

void CTaskExplorer::InitColors()
{
	// plot colors:
	m_Colors.insert(eGraphBack, SColor("GraphBack", tr("Graph background"), "#808080"));
	m_Colors.insert(eGraphFront, SColor("GraphFront", tr("Graph text"), "#FFFFFF"));

	m_Colors.insert(ePlotBack, SColor("PlotBack", tr("Plot background"), "#EFEFEF"));
	m_Colors.insert(ePlotFront, SColor("PlotFront", tr("Plot text"), "#505050"));
	m_Colors.insert(ePlotGrid, SColor("PlotGrid", tr("Plot grid"), "#C7C7C7"));

	m_Colors.insert(eGridColor, SColor("GridColor", tr("List grid color"), "#808080"));
	m_Colors.insert(eBackground, SColor("Background", tr("Default background"), "#FFFFFF"));

	// std::list colors:
	m_Colors.insert(eAdded, SColor("NewlyCreated", tr("New items"), "#00FF7F"));
	m_Colors.insert(eToBeRemoved, SColor("ToBeRemoved", tr("Removed items"), "#F08080"));

#ifdef WIN32
	m_Colors.insert(eDangerous, SColor("DangerousProcess", tr("Dangerous process"), "#FF0000"));
#endif
	m_Colors.insert(eSystem, SColor("SystemProcess", tr("System processes"), "#AACCFF"));
	m_Colors.insert(eUser, SColor("UserProcess", tr("Current user processes"), "#FFFF80"));
	// The settings key stays "ServiceProcess" on both platforms so an existing
	// colour choice survives; only the label differs.
#ifdef WIN32
	m_Colors.insert(eService, SColor("ServiceProcess", tr("Service processes"), "#80FFFF"));
#else
	m_Colors.insert(eService, SColor("ServiceProcess", tr("Daemon processes"), "#80FFFF"));
#endif
	m_Colors.insert(eSandBoxed, SColor("SandBoxed", tr("Sandboxed processes"), "#FFFF00"));
	m_Colors.insert(eJob, SColor("JobProcess", tr("Job processes"), "#D49C5C"));
	m_Colors.insert(ePico, SColor("PicoProcess", tr("Pico processes"), "#42A0FF"));
	m_Colors.insert(eImmersive, SColor("ImmersiveProcess", tr("Immersive processes"), "#FFE6FF"));
	m_Colors.insert(eDotNet, SColor("NetProcess", tr(".NET processes"), "#DCFF00"));
	m_Colors.insert(eElevated, SColor("ElevatedProcess", tr("Elevated processes"), "#FFBB30"));

	//
	// The settings keys stay as they are on both platforms so an existing
	// colour choice survives; only the labels differ where the same idea has
	// two names.
	//
#ifdef WIN32
	m_Colors.insert(eDriver, SColor("KernelServices", tr("Kernel Services (Driver)"), "#FFC880"));
#else
	m_Colors.insert(eDriver, SColor("KernelServices", tr("Kernel modules"), "#FFC880"));
#endif
	//
	// A violet, because nothing else in this list is one. It has to be legible
	// with black text like the rest, so it is a light one rather than the colour
	// the name suggests.
	//
	m_Colors.insert(eWine, SColor("WineProcess", tr("Wine processes"), "#D0B0E8"));

	m_Colors.insert(eGuiThread, SColor("GuiThread", tr("Gui threads"), "#AACCFF"));
	m_Colors.insert(eIsInherited, SColor("IsInherited", tr("Inherited handles"), "#77FFFF"));
	m_Colors.insert(eIsProtected, SColor("IsProtected", tr("Protected handles/Critical tasks"), "#FF77FF"));

	m_Colors.insert(eExecutable, SColor("Executable", tr("Executable memory"), "#FF90E0"));
	//

	ReloadColors();
}

void CTaskExplorer::ReloadColors()
{
	for(QMap<EColor, SColor>::iterator I = m_Colors.begin(); I!= m_Colors.end(); ++I)
	{
		CTaskExplorer::SColor& Color = I.value();

		StrPair ColorUse = Split2(theConf->GetString("Colors/" + Color.Name, Color.Default), ";");

		if (Color.Name != "Background"
 		 && Color.Name != "GraphBack" && Color.Name != "GraphFront"
		 && Color.Name != "PlotBack" && Color.Name != "PlotFront" && Color.Name != "PlotGrid")
			Color.Enabled = ColorUse.second.isEmpty() || ColorUse.second.compare("true", Qt::CaseInsensitive) == 0 || ColorUse.second.toInt() != 0;
		else
			Color.Enabled = true;

		Color.Value = QColor(ColorUse.first);
	}
}

int CTaskExplorer::GetGraphLimit(bool bLong)
{
	int interval = theConf->GetInt("Options/RefreshInterval", 1000); // 5 minutes default
	int limit = interval ? theConf->GetInt("Options/GraphLength", 300) * 1000 / interval : 300;

	return limit;
}

void CTaskExplorer::OnStatusMessage(const QString& Message)
{
	statusBar()->showMessage(Message, 5000); // show for 5 seconds
}

//
// What the startup is waiting for, in the status line.
//
// Without a timeout: each of these replaces the one before it and the run ends
// with a message that has one, so the line is never left saying that something
// is being connected which finished a minute ago. The count is only shown when
// there is more than one machine - "(1 of 1)" is noise.
//
void CTaskExplorer::OnConnectingTo(const QString& Name, int Index, int Count)
{
	statusBar()->showMessage(Count > 1
		? tr("Connecting to %1 ... (%2 of %3)").arg(Name).arg(Index + 1).arg(Count)
		: tr("Connecting to %1 ...").arg(Name));
}

//
// And what came of it, for thirty seconds - the same span the other startup
// messages use, long enough to be read by somebody who looked away while the
// machines were being tried.
//
void CTaskExplorer::OnConnectRunFinished(int Connected, int Count, const QString& Name)
{
	QString Message;
	if (Count == 1)
		Message = Connected == 1 ? tr("Connected to %1").arg(Name)
								 : tr("Could not connect to %1").arg(Name);
	else if (Connected == Count)
		Message = tr("Connected to all %1 machines").arg(Count);
	else
		Message = tr("Connected to %1 of %2 machines").arg(Connected).arg(Count);

	statusBar()->showMessage(Message, 30000);
}

void CTaskExplorer::LoadLanguage()
{
	m_Language = theConf->GetString("General/Language");
	if(m_Language.isEmpty())
		m_Language = QLocale::system().name();

	if (m_Language.compare("native", Qt::CaseInsensitive) == 0)
#ifdef _DEBUG
		m_Language = "en";
#else
		m_Language.clear();
#endif

	//m_LanguageId = LocaleNameToLCID(m_Language.toStdWString().c_str(), 0);
	//if (!m_LanguageId)
	//	m_LanguageId = 1033; // default to English

	LoadLanguage(m_Language, "taskexplorer", 0);
	LoadLanguage(m_Language, "qt", 1);

	QTreeViewEx::m_ResetColumns = tr("Reset Columns");
	CPanelView::m_CopyCell = tr("Copy Cell");
	CPanelView::m_CopyRow = tr("Copy Row");
	CPanelView::m_CopyPanel = tr("Copy Panel");
	CFinder::m_CaseInsensitive = tr("Case Sensitive");
	CFinder::m_RegExpStr = tr("RegExp");
	CFinder::m_Highlight = tr("Highlight");
	CFinder::m_CloseStr = tr("Close");
	CFinder::m_FindStr = tr("&Find ...");
	CFinder::m_AllColumns = tr("All columns");
}

void CTaskExplorer::LoadLanguage(const QString& Lang, const QString& Module, int Index)
{
	qApp->removeTranslator(&m_Translator[Index]);

	if (Lang.isEmpty())
		return;

	QString LangAux = Lang; // Short version as fallback
	LangAux.truncate(LangAux.lastIndexOf('_'));

	QString LangDir;
#ifdef WIN32
	// Translations may ship inside translations.7z; the loose directory is the
	// fallback. The 7-Zip file engine is Windows-only for now (MiscHelpers'
	// Archive subtree builds on the Win32 COM shims), so Linux always takes the
	// directory path.
	C7zFileEngineHandler LangFS("lang", this);
	if (LangFS.Open(QApplication::applicationDirPath() + "/translations.7z"))
		LangDir = LangFS.Prefix() + "/";
	else
#endif
		LangDir = QApplication::applicationDirPath() + "/translations/";

	bool bOk = false;
	QString LangPath = LangDir + Module + "_";
	bool bAux = false;
	if (QFile::exists(LangPath + Lang + ".qm") || (bAux = QFile::exists(LangPath + LangAux + ".qm")))
	{
		if(m_Translator[Index].load(LangPath + (bAux ? LangAux : Lang) + ".qm", LangDir))
			bOk = qApp->installTranslator(&m_Translator[Index]);
	}
}

void CTaskExplorer::OnCheckForUpdates()
{
	m_pUpdater->CheckForUpdates(true);
}

//
// The one line of the toolbar that is not about a process.
//
// It carries whichever of two things is worth saying, and sometimes neither: an
// update is waiting, or this copy is not supported yet. Somebody who *has*
// supported it should not be asked again every time they look at the toolbar -
// so with a valid certificate the label is empty unless there is an update, and
// the space it took goes back to the toolbar.
//
void CTaskExplorer::UpdateLabel()
{
	QString PendingUpdate = theConf->GetString("Updater/PendingUpdate");

	//
	// Supported already and nothing to announce: the label, its padding and the
	// separator in front of it all go, so the bar closes up rather than ending
	// in a line with a gap after it.
	//
	SCertInfo Cert;
	Cert.State = CRemoteLoader::CertificateState();
	const bool bShow = !PendingUpdate.isEmpty() || !Cert.active;
	foreach(QAction* pItem, m_UpdateLabelItems)
		pItem->setVisible(bShow);

	if (!PendingUpdate.isEmpty()) {
		// An update is waiting - that is worth the space whoever is running this.
		m_pUpdateLabel->setText(tr("<a href=\"#update\">Update to TaskExplorer %1 available!</a>").arg(PendingUpdate));
		m_pUpdateLabel->disconnect();
		connect(m_pUpdateLabel, SIGNAL(linkActivated(const QString&)), this, SLOT(OnCheckForUpdates()));
	}
	else if (!bShow) {
		m_pUpdateLabel->clear();
		m_pUpdateLabel->disconnect();
	}
	else {
		// Show default Patreon support message
		m_pUpdateLabel->setText("<a href=\"https://xanasoft.com/go.php?to=patreon\">Support TaskExplorer on Patreon</a>");
		m_pUpdateLabel->disconnect();
		connect(m_pUpdateLabel, SIGNAL(linkActivated(const QString&)), this, SLOT(OnHelp()));
	}
}

QString CTaskExplorer::GetVersion()
{
	QString Version = QString::number(VERSION_MJR) + "." + QString::number(VERSION_MIN) //.rightJustified(2, '0')
#if VERSION_REV > 0
		+ "." + QString::number(VERSION_REV)
#endif
#if VERSION_UPD > 0
		+ QString('a' + VERSION_UPD - 1)
#endif
		;
	return Version;
}

void CTaskExplorer::OnHelp()
{
	if (sender() == m_pMenuForum)
		QDesktopServices::openUrl(QUrl("https://xanasoft.com/go.php?to=forum"));
	else
		QDesktopServices::openUrl(QUrl("https://xanasoft.com/go.php?to=patreon"));
}

void CTaskExplorer::ShowSecurity(const CSecurityEditablePtr& pObject, QWidget* parent)
{
	if (!pObject)
	{
		QMessageBox::information(parent, "TaskExplorer",
			tr("Permissions cannot be shown for this object."));
		return;
	}

	CSecurityDialog* pWnd = new CSecurityDialog(pObject, parent);
	pWnd->show();
}

void CTaskExplorer::OnAbout()
{
	if (sender() == m_pMenuAbout)
	{
#ifdef Q_WS_MAC
		static QPointer<QMessageBox> oldMsgBox;

		if (oldMsgBox) {
			oldMsgBox->show();
			oldMsgBox->raise();
			oldMsgBox->activateWindow();
			return;
		}
#endif

		QString AboutCaption = tr(
			"<h3>About TaskExplorer</h3>"
			"<p>Version %1</p>"
			"<p>Copyright (C) 2019-2026 David Xanatos (xanasoft.com)</p>"
		).arg(GetVersion());
		QString AboutText = tr(
			"<p>TaskExplorer is a powerfull multi-purpose Task Manager that helps you monitor system resources, debug software and detect malware.</p>"
			"<p></p>"
#ifdef WIN32
			"<p>On Windows TaskExplorer is powered by the ProsessHacker Library.</p>"
			"<p></p>"
#endif
			"<p>Visit <a href=\"https://github.com/DavidXanatos/TaskExplorer\">TaskExplorer on github</a> for more information.</p>"
			"<p></p>"
			"<p>Config Dir: %1</p>"
			"<p></p>"
			"<p>Icons from <a href=\"https://icons8.com\">icons8.com</a></p>"
			"<p></p>"
		).arg(theConf->GetConfigDir());
		QMessageBox *msgBox = new QMessageBox(this);
		msgBox->setAttribute(Qt::WA_DeleteOnClose);
		msgBox->setWindowTitle(tr("About TaskExplorer"));
		msgBox->setText(AboutCaption);
		msgBox->setInformativeText(AboutText);

		QIcon ico(QLatin1String(":/TaskExplorer.png"));
		msgBox->setIconPixmap(ico.pixmap(128, 128));
#if defined(Q_WS_WINCE)
		msgBox->setDefaultButton(msgBox->addButton(QMessageBox::Ok));
#endif

		// should perhaps be a style hint
#ifdef Q_WS_MAC
		oldMsgBox = msgBox;
		msgBox->show();
#else
		msgBox->exec();
#endif
	}
#ifdef WIN32
	else if (sender() == m_pMenuAboutPH)
	{
		QString AboutCaption = QString(
			"<h3>System Informer</h3>"
			"<p>Licensed Under the MIT License</p>"
			"<p>Copyright (c) 2022</p>"
		);
		QString AboutText = QString(
			"<p>Thanks to:<br>"
			"    <a href=\"https://github.com/wj32\">wj32</a> - Wen Jia Liu<br>"
			"    <a href=\"https://github.com/dmex\">dmex</a> - Steven G<br>"
			"    <a href=\"https://github.com/jxy-s\">jxy-s</a> - Johnny Shaw<br>"
			"    <a href=\"https://github.com/ionescu007\">ionescu007</a> - Alex Ionescu<br>"
			"    <a href=\"https://github.com/yardenshafir\">yardenshafir</a> - Yarden Shafir<br>"
			"    <a href=\"https://github.com/winsiderss/systeminformer/graphs/contributors\">Contributors</a> - thank you for your additions!<br>"
			"    Donors - thank you for your support!</p>"
			"<p>System Informer uses the following components:<br>"
			"    <a href=\"https://github.com/michaelrsweet/mxml\">Mini-XML</a> by Michael Sweet<br>"
			"    <a href=\"https://www.pcre.org\">PCRE</a><br>"
			"    <a href=\"https://github.com/json-c/json-c\">json-c</a><br>"
			"    MD5 code by Jouni Malinen<br>"
			"    SHA1 code by Filip Navara, based on code by Steve Reid<br>"
			"    <a href=\"http://www.famfamfam.com/lab/icons/silk\">Silk icons</a><br>"
			"    <a href=\"https://www.fatcow.com/free-icons\">Farm-fresh web icons</a><br></p>"
			"<p></p>"
			"<p>Visit <a href=\"https://github.com/winsiderss/systeminformer\">System Informer on github</a> for more information.</p>"
		);
		QMessageBox *msgBox = new QMessageBox(this);
		msgBox->setAttribute(Qt::WA_DeleteOnClose);
		msgBox->setWindowTitle(QString("About ProcessHacker Library"));
		msgBox->setText(AboutCaption);
		msgBox->setInformativeText(AboutText);

		QIcon ico(QLatin1String(":/ProcessHacker.png"));
		msgBox->setIconPixmap(ico.pixmap(64, 64));
#if defined(Q_WS_WINCE)
		msgBox->setDefaultButton(msgBox->addButton(QMessageBox::Ok));
#endif

		msgBox->exec();
	}
#endif
	else if (sender() == m_pMenuAboutQt)
		QMessageBox::aboutQt(this);
}
