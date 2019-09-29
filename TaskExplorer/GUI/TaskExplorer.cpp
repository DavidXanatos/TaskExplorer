#include "stdafx.h"
#include "TaskExplorer.h"
#ifdef WIN32
#include "../API/Windows/WindowsAPI.h"
#include "../API/Windows/ProcessHacker/RunAs.h"
#include "SecurityExplorer.h"
#include "DriverWindow.h"
#include "../API/Windows/WinAdmin.h"
extern "C" {
#include <winsta.h>
}
#endif
#include "../Common/ExitDialog.h"
#include "../Common/HistoryGraph.h"
#include "NewService.h"
#include "RunAsDialog.h"
#include "../SVC/TaskService.h"
#include "GraphBar.h"
#include "SettingsWindow.h"
#include "CustomItemDelegate.h"
#include "Search/HandleSearch.h"
#include "Search/ModuleSearch.h"
#include "Search/MemorySearch.h"
#include "SystemInfo/SystemInfoWindow.h"
#include "../Common/CheckableMessageBox.h"


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
	virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *result)
	{
		if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") 
		{
			MSG *msg = static_cast<MSG *>(message);

			//if(msg->message != 275 && msg->message != 1025)
			//	qDebug() << msg->message;
			if (msg->message == WM_NOTIFY) 
			{
				LRESULT ret;
				if (PhMwpOnNotify((NMHDR *)msg->lParam, &ret))
					*result = ret;
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
					theAPI->NotifyHardwareChanged();
					break;
				}
			}
		}
		return false;
	}
};
#endif


CTaskExplorer::CTaskExplorer(QWidget *parent)
	: QMainWindow(parent)
{
	theGUI = this;

	CSystemAPI::InitAPI();

	QString appTitle = tr("TaskExplorer v%1").arg(GetVersion());

#ifdef WIN32
	if (KphIsConnected())
	{
		if(KphIsVerified())
			appTitle.append(tr(" - [kPH]")); // full
		else
			appTitle.append(tr(" ~ [kPH]")); // limited
	}
#endif

	if (theAPI->RootAvaiable())
#ifdef WIN32
		appTitle.append(tr(" (Administrator)"));
#else
		appTitle.append(tr(" (root)"));
#endif
	this->setWindowTitle(appTitle);

#if defined(Q_OS_WIN)
	PhMainWndHandle = (HWND)QWidget::winId();

    QApplication::instance()->installNativeEventFilter(new CNativeEventFilter);
#endif

	m_bExit = false;

	// a shared item deleagate for all lists
	m_pCustomItemDelegate = new CCustomItemDelegate(GetCellHeight() + 1, this);

	LoadDefaultIcons();

	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout(m_pMainWidget);
	m_pMainLayout->setMargin(0);
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

	m_pMainSplitter = new QSplitter();
	m_pMainSplitter->setOrientation(Qt::Horizontal);
	m_pGraphSplitter->addWidget(m_pMainSplitter);
	m_pGraphSplitter->setStretchFactor(1, 1);

	m_pProcessTree = new CProcessTree(this);
	//m_pProcessTree->setMinimumSize(200 * theGUI->GetDpiScale(), 200 * theGUI->GetDpiScale());
	m_pMainSplitter->addWidget(m_pProcessTree);
	m_pMainSplitter->setCollapsible(0, false);

	m_pPanelSplitter = new QSplitter();
	m_pPanelSplitter->setOrientation(Qt::Vertical);
	m_pMainSplitter->addWidget(m_pPanelSplitter);

	m_pSystemInfo = new CSystemInfoView();
	//m_pSystemInfo->setMinimumSize(200 * theGUI->GetDpiScale(), 200 * theGUI->GetDpiScale());
	m_pPanelSplitter->addWidget(m_pSystemInfo);

	m_pTaskInfo = new CTaskInfoView();
	//m_pTaskInfo->setMinimumSize(200 * theGUI->GetDpiScale(), 200 * theGUI->GetDpiScale());
	m_pPanelSplitter->addWidget(m_pTaskInfo);
	m_pPanelSplitter->setCollapsible(1, false);

	m_pPanelSplitter->setMinimumHeight(100 * theGUI->GetDpiScale());

	connect(m_pMainSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(OnSplitterMoved()));
	connect(m_pPanelSplitter, SIGNAL(splitterMoved(int,int)), this, SLOT(OnSplitterMoved()));

	//connect(m_pProcessTree, SIGNAL(ProcessClicked(const CProcessPtr)), m_pTaskInfo, SLOT(ShowProcess(const CProcessPtr)));
	connect(m_pProcessTree, SIGNAL(ProcessesSelected(const QList<CProcessPtr>&)), m_pTaskInfo, SLOT(ShowProcesses(const QList<CProcessPtr>&)));


#ifdef WIN32
	connect(qobject_cast<CWindowsAPI*>(theAPI)->GetSymbolProvider(), SIGNAL(StatusMessage(const QString&)), this, SLOT(OnStatusMessage(const QString&)));
#endif


	m_pMenuProcess = menuBar()->addMenu(tr("&Tasks"));
		m_pMenuRun = m_pMenuProcess->addAction(MakeActionIcon(":/Actions/Run"), tr("Run..."), this, SLOT(OnRun()));
		m_pMenuRunAs = m_pMenuProcess->addAction(MakeActionIcon(":/Actions/RunAs"), tr("Run as..."), this, SLOT(OnRunAs()));
		// this can be done with the normal run command anyways
		//m_pMenuRunAsUser = m_pMenuProcess->addAction(MakeActionIcon(":/Actions/RunUser"), tr("Run as Limited User..."), this, SLOT(OnRunUser()));
		//m_pMenuRunAsAdmin = m_pMenuProcess->addAction(MakeActionIcon(":/Actions/RunRoot"), tr("Run as Administrator..."), this, SLOT(OnRunAdmin()));
#ifdef WIN32
		m_pMenuRunSys = m_pMenuProcess->addAction(MakeActionIcon(":/Actions/RunTI"), tr("Run as TrustedInstaller..."), this, SLOT(OnRunSys()));
#endif
		m_pMenuProcess->addSeparator();
		m_pMenuComputer = m_pMenuProcess->addMenu(MakeActionIcon(":/Actions/Shutdown"), tr("Computer"));
		m_pMenuUsers = m_pMenuProcess->addMenu(MakeActionIcon(":/Actions/Users"), tr("Users"));
		m_pMenuProcess->addSeparator();
		m_pMenuElevate = m_pMenuProcess->addAction(MakeActionIcon(":/Icons/Shield.png"), tr("Restart Elevated"), this, SLOT(OnElevate()));
		m_pMenuElevate->setVisible(!theAPI->RootAvaiable());
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
		m_pMenuHybridShutdown = m_pMenuComputer->addAction(MakeActionIcon(":/Actions/HybridShutdown"), tr("Hybid Shutdown"), this, SLOT(OnComputerAction()));



	m_pMenuView = menuBar()->addMenu(tr("&View"));
		m_pMenuSysTabs = m_pMenuView->addMenu(tr("System Tabs"));
		for (int i = 0; i < m_pSystemInfo->GetTabCount(); i++)
		{
			QAction* pAction = m_pMenuSysTabs->addAction(m_pSystemInfo->GetTabLabel(i), this, SLOT(OnSysTab()));
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

		m_pMenuTaskTabs = m_pMenuView->addMenu(tr("Task Tabs"));
		for (int i = 0; i < m_pTaskInfo->GetTabCount(); i++)
		{
			QAction* pAction = m_pMenuTaskTabs->addAction(m_pTaskInfo->GetTabLabel(i), this, SLOT(OnTaskTab()));
			pAction->setCheckable(true);
			pAction->setChecked(m_pTaskInfo->IsTabVisible(i));
			m_Act2Tab[pAction] = i;
		}

		m_pMenuView->addSeparator();
		m_pMenuSystemInfo = m_pMenuView->addAction(MakeActionIcon(":/Actions/SysInfo"), tr("System Info"), this, SLOT(OnSystemInfo()));
		m_pMenuView->addSeparator();
		m_pMenuPauseRefresh = m_pMenuView->addAction(MakeActionIcon(":/Actions/Pause"), tr("Pause Refresh"));
		m_pMenuPauseRefresh->setCheckable(true);
		m_pMenuRefreshNow = m_pMenuView->addAction(MakeActionIcon(":/Actions/Refresh"), tr("Refresh Now"), this, SLOT(UpdateAll()));
		m_pMenuResetAll = m_pMenuView->addAction(MakeActionIcon(":/Actions/Reset"), tr("Reset all Panels"), this, SLOT(ResetAll()));
		m_pMenuResetAll->setShortcut(QKeySequence::Refresh); // F5
		m_pMenuExpandAll = m_pMenuView->addAction(MakeActionIcon(":/Actions/Expand"), tr("Expand Process Tree"), m_pProcessTree, SLOT(OnExpandAll()));
		m_pMenuExpandAll->setShortcut(QKeySequence("Ctrl+E"));

	m_pMenuFind = menuBar()->addMenu(tr("&Find"));
		m_pMenuFindHandle = m_pMenuFind->addAction(MakeActionIcon(":/Actions/FindHandle"), tr("Find Handles"), this, SLOT(OnFindHandle()));
		m_pMenuFindDll = m_pMenuFind->addAction(MakeActionIcon(":/Actions/FindDLL"), tr("Find Module (dll)"), this, SLOT(OnFindDll()));
		m_pMenuFindMemory = m_pMenuFind->addAction(MakeActionIcon(":/Actions/FindString"), tr("Find String in Memory"), this, SLOT(OnFindMemory()));

	m_pMenuOptions = menuBar()->addMenu(tr("&Options"));
		m_pMenuSettings = m_pMenuOptions->addAction(MakeActionIcon(":/Actions/Settings"), tr("Settings"), this, SLOT(OnSettings()));
#ifdef WIN32
		m_pMenuDriverConf = m_pMenuOptions->addAction(MakeActionIcon(":/Actions/Driver"), tr("Driver Options"), this, SLOT(OnDriverConf()));
		m_pMenuDriverConf->setEnabled(!PhIsExecutingInWow64());

        m_pMenuOptions->addSeparator();
        m_pMenuAutoRun = m_pMenuOptions->addAction(tr("Auto Run"), this, SLOT(OnAutoRun()));
        m_pMenuAutoRun->setCheckable(true);
        m_pMenuAutoRun->setChecked(IsAutorunEnabled());
		m_pMenuUAC = m_pMenuOptions->addAction(tr("Skip UAC"), this, SLOT(OnSkipUAC()));
		m_pMenuUAC->setCheckable(true);
		m_pMenuUAC->setEnabled(theAPI->RootAvaiable());
		m_pMenuUAC->setChecked(SkipUacRun(true));
#endif

	m_pMenuTools = menuBar()->addMenu(tr("&Tools"));
		m_pMenuServices = m_pMenuTools->addMenu(MakeActionIcon(":/Actions/Services"), tr("&Services"));
			m_pMenuCreateService = m_pMenuServices->addAction(tr("Create new Service"), this, SLOT(OnCreateService()));
			m_pMenuCreateService->setEnabled(theAPI->RootAvaiable());
			m_pMenuUpdateServices = m_pMenuServices->addAction(tr("ReLoad all Service"), this, SLOT(OnReloadService()));
#ifdef WIN32
			m_pMenuSCMPermissions = m_pMenuServices->addAction(tr("Service Control Manager Permissions"), this, SLOT(OnSCMPermissions()));

		m_pMenuFree = m_pMenuTools->addMenu(MakeActionIcon(":/Actions/FreeMem"), tr("&Free Memory"));
			m_pMenuFreeWorkingSet = m_pMenuFree->addAction(tr("Empty Working set"), this, SLOT(OnFreeMemory()));
			m_pMenuFreeModPages = m_pMenuFree->addAction(tr("Empty Modified pages"), this, SLOT(OnFreeMemory()));
			m_pMenuFreeStandby = m_pMenuFree->addAction(tr("Empty Standby list"), this, SLOT(OnFreeMemory()));
			m_pMenuFreePriority0 = m_pMenuFree->addAction(tr("Empty Priority 0 list"), this, SLOT(OnFreeMemory()));
			m_pMenuFree->addSeparator();
			m_pMenuCombinePages = m_pMenuFree->addAction(tr("Combine Pages"), this, SLOT(OnFreeMemory()));
#endif
			
		m_pMenuFlushDns = m_pMenuTools->addAction(MakeActionIcon(":/Actions/Flush"), tr("Flush Dns Cache"), theAPI, SLOT(FlushDnsCache()));
#ifdef _WIN32
		m_pMenuSecurityExplorer = m_pMenuTools->addAction(MakeActionIcon(":/Actions/Security"), tr("Security Explorer"), this, SLOT(OnSecurityExplorer()));
#endif

		m_pMenuTools->addSeparator();
#ifdef WIN32
		m_pMenuMonitorETW = m_pMenuTools->addAction(MakeActionIcon(":/Actions/MonitorETW"), tr("Monitor ETW Events"), this, SLOT(OnMonitorETW()));
		m_pMenuMonitorETW->setCheckable(true);
		m_pMenuMonitorETW->setChecked(((CWindowsAPI*)theAPI)->IsMonitoringETW());
		//m_pMenuMonitorETW->setEnabled(theAPI->RootAvaiable());
		m_pMenuMonitorFW = m_pMenuTools->addAction(MakeActionIcon(":/Actions/MonitorFW"), tr("Monitor Windows Firewall"), this, SLOT(OnMonitorFW()));
		m_pMenuMonitorFW->setCheckable(true);
		m_pMenuMonitorFW->setChecked(((CWindowsAPI*)theAPI)->IsMonitoringFW());
		//m_pMenuMonitorFW->setEnabled(theAPI->RootAvaiable());
#endif

	m_pMenuHelp = menuBar()->addMenu(tr("&Help"));
		m_pMenuSupport = m_pMenuHelp->addAction(tr("Support TaskExplorer on Patreon"), this, SLOT(OnAbout()));
		m_pMenuHelp->addSeparator();
#ifdef WIN32
		m_pMenuAboutPH = m_pMenuHelp->addAction(tr("About ProcessHacker Library"), this, SLOT(OnAbout()));
#endif
		m_pMenuAboutQt = m_pMenuHelp->addAction(tr("About the Qt Framework"), this, SLOT(OnAbout()));
		//m_pMenuHelp->addSeparator();
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
	m_pRefreshButton->setToolTip(tr("Refresh Now"));
	m_pRefreshButton->setPopupMode(QToolButton::MenuButtonPopup);
	QMenu* pRefreshMenu = new QMenu(m_pRefreshButton);
	m_pRefreshGroup = new QActionGroup(pRefreshMenu);
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
	QObject::connect(m_pRefreshButton, SIGNAL(pressed()), this, SLOT(UpdateAll()));
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
	m_pToolBar->addAction(m_pMenuMonitorETW);
	m_pToolBar->addAction(m_pMenuMonitorFW);
	m_pToolBar->addSeparator();
	m_pToolBar->addAction(m_pMenuSystemInfo);
	m_pToolBar->addSeparator();
	
	m_pFindButton = new QToolButton();
	m_pFindButton->setIcon(MakeActionIcon(":/Actions/Find"));
	m_pFindButton->setToolTip(tr("Search..."));
	m_pFindButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_pFindButton->setMenu(m_pMenuFind);
	//QObject::connect(m_pFindButton, SIGNAL(triggered(QAction*)), , SLOT());
	QObject::connect(m_pFindButton, SIGNAL(pressed()), this, SLOT(OnFindHandle()));
	m_pToolBar->addWidget(m_pFindButton);
	m_pToolBar->addSeparator();

	m_pFreeButton = new QToolButton();
	m_pFreeButton->setIcon(MakeActionIcon(":/Actions/FreeMem"));
	m_pFreeButton->setToolTip(tr("Free memory"));
	m_pFreeButton->setPopupMode(QToolButton::MenuButtonPopup);
    m_pFreeButton->setMenu(m_pMenuFree);
	//QObject::connect(m_pFreeButton, SIGNAL(triggered(QAction*)), , SLOT());
	QObject::connect(m_pFreeButton, SIGNAL(pressed()), this, SLOT(OnFreeMemory()));
	m_pToolBar->addWidget(m_pFreeButton);
	m_pToolBar->addSeparator();
	
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

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	m_pToolBar->addAction(m_pMenuElevate);

	m_pToolBar->addSeparator();
	m_pToolBar->addWidget(new QLabel("        "));
	QLabel* pSupport = new QLabel("<a href=\"https://www.patreon.com/DavidXanatos\">Support TaskExplorer on Patreon</a>");
	pSupport->setTextInteractionFlags(Qt::TextBrowserInteraction);
	connect(pSupport, SIGNAL(linkActivated(const QString&)), this, SLOT(OnAbout()));
	m_pToolBar->addWidget(pSupport);
	m_pToolBar->addWidget(new QLabel("        "));

	

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

#ifdef WIN32
	if (KphIsConnected())
	{
		statusBar()->showMessage(tr("TaskExplorer with %1 (%2) driver is ready...").arg(((CWindowsAPI*)theAPI)->GetDriverFileName()).arg(KphIsVerified() ? tr("full") : tr("limited")), 30000);
	}
	else if (((CWindowsAPI*)theAPI)->HasDriverFailed() && theAPI->RootAvaiable())
	{
		QString Message = tr("Failed to load %1 driver, this could have various causes.\r\n"
			"The driver file may be missing, or is wrongfully detected as malicious by your anti-virus application and is being blocked.\r\n"
			"If this is the case you need to add an exception in your AV product for the xprocesshacker.sys file."
		).arg(((CWindowsAPI*)theAPI)->GetDriverFileName());

		bool State = false;
		CCheckableMessageBox::question(this, "TaskExplorer", Message
			, tr("Don't use the driver. WARNING: this will limit the aplications functionality!"), &State, QDialogButtonBox::Ok, QDialogButtonBox::Ok, QMessageBox::Warning);

		if (State)
			theConf->SetValue("Options/UseDriver", false);

		statusBar()->showMessage(tr("TaskExplorer failed to load driver %1").arg(((CWindowsAPI*)theAPI)->GetDriverFileName()), 180000);
	}
	else
#endif
		statusBar()->showMessage(tr("TaskExplorer is ready..."), 30000);

	ApplyOptions();

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

	theAPI->deleteLater();

	theGUI = NULL;
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

void CTaskExplorer::OnChangePersistence(QAction* pAction)
{
	quint64 Persistence = pAction->data().toULongLong();
	
	CAbstractInfoEx::SetPersistenceTime(Persistence);
}

void CTaskExplorer::closeEvent(QCloseEvent *e)
{
	if (!m_bExit)
	{
		if (m_pTrayIcon->isVisible() && theConf->GetBool("SysTray/CloseToTray", true))
		{
			hide();

			e->ignore();
			return;
		}
		else
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

void CTaskExplorer::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() != m_uTimerID)
		return;

	quint64 Interval = theConf->GetInt("Options/RefreshInterval", 1000);
	if (GetCurTick() - m_LastTimer < Interval / 2)
		return;

	UpdateUserMenu();

	m_pMenuExpandAll->setEnabled(m_pProcessTree->IsTree());

	foreach(QAction* pAction, m_pRefreshGroup->actions())
		pAction->setChecked(pAction->data().toULongLong() == Interval);

	quint64 Persistence = CAbstractInfoEx::GetPersistenceTime();
	foreach(QAction* pAction, m_pHoldGroup->actions())
		pAction->setChecked(pAction->data().toULongLong() == Persistence);
	m_pHoldButton->setChecked(m_pHoldAction->data().toULongLong() == Persistence);

	if(!m_pMenuPauseRefresh->isChecked())
		UpdateAll();

	//if(m_pMainSplitter->sizes()[0] > 0)
		m_pGraphBar->UpdateGraphs();

	if (m_pMainSplitter->sizes()[1] > 0 && m_pPanelSplitter->sizes()[0] > 0)
		m_pSystemInfo->UpdateGraphs();

	UpdateStatus();

	m_LastTimer = GetCurTick();
}

void CTaskExplorer::UpdateAll()
{
	QTimer::singleShot(0, theAPI, SLOT(UpdateProcessList()));
	QTimer::singleShot(0, theAPI, SLOT(UpdateSocketList()));

	QTimer::singleShot(0, theAPI, SLOT(UpdateSysStats()));

	QTimer::singleShot(0, theAPI, SLOT(UpdateServiceList()));

	if (!isVisible() || windowState().testFlag(Qt::WindowMinimized))
		return;

	if(m_pMainSplitter->sizes()[1] > 0)
		m_pTaskInfo->Refresh();

	if (m_pMainSplitter->sizes()[1] > 0 && m_pPanelSplitter->sizes()[0] > 0)
		m_pSystemInfo->Refresh();
}

void CTaskExplorer::UpdateStatus()
{
	m_pStausCPU->setText(tr("CPU: %1%    ").arg(int(100 * theAPI->GetCpuUsage())));
	m_pStausCPU->setToolTip(theAPI->GetCpuModel());

	QMap<QString, CGpuMonitor::SGpuInfo> GpuList = theAPI->GetGpuMonitor()->GetAllGpuList();

	QString GPU;
	QStringList GpuInfos;
	int i = 0;
	foreach(const CGpuMonitor::SGpuInfo &GpuInfo, GpuList)
	{
		GPU.append(tr("GPU-%1: %2%    ").arg(i).arg(int(100 * GpuInfo.TimeUsage)));
		GpuInfos.append(GpuInfo.Description);
		i++;
	}
	m_pStausGPU->setToolTip(GpuInfos.join("\r\n"));
	m_pStausGPU->setText(GPU);

	quint64 RamUsage = theAPI->GetPhysicalUsed();
	quint64 SwapedMemory = theAPI->GetSwapedOutMemory();
	quint64 CommitedMemory = theAPI->GetCommitedMemory();

	quint64 InstalledMemory = theAPI->GetInstalledMemory();
	quint64 TotalSwap = theAPI->GetTotalSwapMemory();

	quint64 TotalMemory = Max(theAPI->GetInstalledMemory(), theAPI->GetCommitedMemory()); // theAPI->GetMemoryLimit();

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


	SSysStats SysStats = theAPI->GetStats();

	QString IO;
	IO += tr("R: %1").arg(FormatRate(qMax(SysStats.Io.ReadRate.Get(), qMax(SysStats.MMapIo.ReadRate.Get(), SysStats.Disk.ReadRate.Get()))));
	IO += " ";
	IO += tr("W: %1").arg(FormatRate(qMax(SysStats.Io.WriteRate.Get(), qMax(SysStats.MMapIo.WriteRate.Get(), SysStats.Disk.WriteRate.Get()))));
	m_pStausIO->setText(IO + "    ");

	QStringList IOInfo;
	IOInfo.append(tr("FileIO; Read: %1; Write: %2; Other: %3").arg(FormatRate(SysStats.Io.ReadRate.Get())).arg(FormatRate(SysStats.Io.WriteRate.Get())).arg(FormatRate(SysStats.Io.OtherRate.Get())));
	IOInfo.append(tr("MMapIO; Read: %1; Write: %2").arg(FormatRate(SysStats.MMapIo.ReadRate.Get())).arg(FormatRate(SysStats.MMapIo.WriteRate.Get())));
#ifdef WIN32
	if(((CWindowsAPI*)theAPI)->HasExtProcInfo() || ((CWindowsAPI*)theAPI)->IsMonitoringETW())
		IOInfo.append(tr("DiskIO; Read: %1; Write: %2").arg(FormatRate(SysStats.Disk.ReadRate.Get())).arg(FormatRate(SysStats.Disk.WriteRate.Get())));
#endif
	m_pStausIO->setToolTip(IOInfo.join("\r\n"));

	CNetMonitor* pNetMonitor = theAPI->GetNetMonitor();

	CNetMonitor::SDataRates NetRates = pNetMonitor->GetTotalDataRate(CNetMonitor::eNet);
	CNetMonitor::SDataRates RasRates = pNetMonitor->GetTotalDataRate(CNetMonitor::eRas);

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

	QString TrayInfo = tr("Task Explorer\r\nCPU: %1%\r\nRam: %2%").arg(int(100 * theAPI->GetCpuUsage()))
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

			float used_x = hVal * theAPI->GetPhysicalUsed() / InstalledMemory;

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

			float used_x = hVal * theAPI->GetPhysicalUsed() / InstalledMemory;

			qp.setPen(QPen(Qt::cyan, 2));
			qp.drawLine(1, (hVal+1), 1, (hVal+1) - used_x);
		}
		else if(TotalMemory > 0) // TaskExplorer Mode
		{
			offset = 3;
			qp.fillRect(0, 0, offset, TrayIcon.height(), Qt::black);

			float used_x = hVal * RamUsage / TotalMemory;
			float virtual_x = hVal * (RamUsage + SwapedMemory) / TotalMemory;
			float commited_x = hVal * CommitedMemory / TotalMemory;

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
		m_pTrayGraph->SetValue(0, theAPI->GetCpuUsage());
		m_pTrayGraph->SetValue(1, theAPI->GetCpuKernelUsage());
		m_pTrayGraph->SetValue(2, theAPI->GetCpuDPCUsage());

		m_pTrayGraph->Update(TrayIcon.height(), TrayIcon.width() - offset);


		QImage TrayGraph = m_pTrayGraph->GetImage();
		qp.translate(TrayIcon.width() - TrayGraph.height(), TrayIcon.height());
		qp.rotate(270);
		qp.drawImage(0, 0, TrayGraph);
	}

	m_pTrayIcon->setIcon(QIcon(QPixmap::fromImage(TrayIcon)));
}

void CTaskExplorer::CheckErrors(QList<STATUS> Errors)
{
	if (Errors.isEmpty())
		return;

	// todo: xxx show window with error list

	QMessageBox::warning(NULL, "TaskExplorer", tr("Operation failed for %1 item(s).").arg(Errors.size()));
}

void CTaskExplorer::OnRun()
{
#ifdef WIN32
    SelectedRunAsMode = 0;
    PhShowRunFileDialog(PhMainWndHandle, NULL, NULL, NULL, NULL, RFF_OPTRUNAS);
#endif
}

void CTaskExplorer::OnRunAdmin()
{
#ifdef WIN32
    SelectedRunAsMode = RUNAS_MODE_ADMIN;
    PhShowRunFileDialog(PhMainWndHandle, NULL, NULL, NULL, L"Type the name of a program that will be opened under alternate credentials.", 0);
#endif
}

void CTaskExplorer::OnRunUser()
{
#ifdef WIN32
    SelectedRunAsMode = RUNAS_MODE_LIMITED;
    PhShowRunFileDialog(PhMainWndHandle, NULL, NULL, NULL, L"Type the name of a program that will be opened under standard user privileges.", 0);
#endif
}

void CTaskExplorer::OnRunAs()
{
	CRunAsDialog* pWnd = new CRunAsDialog();
	pWnd->show();
}

void CTaskExplorer::OnRunSys()
{
#ifdef WIN32
    SelectedRunAsMode = RUNAS_MODE_SYS;
    PhShowRunFileDialog(PhMainWndHandle, NULL, NULL, NULL, L"Type the name of a program that will be opened as system with the TrustedInstaller token.", 0);
#endif
}

void CTaskExplorer::OnElevate()
{
#ifdef WIN32
	if (PhShellProcessHackerEx(NULL, NULL, L"", SW_SHOW, PH_SHELL_EXECUTE_ADMIN, 0, 0, NULL))
		OnExit();
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

#ifdef WIN32
	bool bSuccess = false;

	if (sender() == m_pMenuLock || sender() == m_pComputerButton)
		bSuccess = LockWorkStation();
	else if (sender() == m_pMenuLogOff)
		bSuccess = ExitWindowsEx(EWX_LOGOFF, 0);
	else if(sender() == m_pMenuSleep)
		bSuccess = NT_SUCCESS(NtInitiatePowerAction(PowerActionSleep, PowerSystemSleeping1, 0, FALSE));
	else if(sender() == m_pMenuHibernate)
		bSuccess = NT_SUCCESS(NtInitiatePowerAction(PowerActionHibernate, PowerSystemSleeping1, 0, FALSE));
	else if(sender() == m_pMenuRestart)
		bSuccess = ExitWindowsEx(EWX_REBOOT, 0);
	else if (sender() == m_pMenuForceRestart)
	{
		switch(SoftForce)
		{
		case 1:	// Don't send the WM_QUERYENDSESSION message (This flag has no effect if terminal services is enabled.)
			bSuccess = ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 0); 
			break;
		case 2:	// Terminate processes not responding to the WM_QUERYENDSESSION or WM_ENDSESSION message.
			bSuccess = ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG, 0); 
			break;
		default:
			bSuccess = NtShutdownSystem(ShutdownReboot);
		}
	}
	else if(sender() == m_pMenuRestartEx)
		bSuccess = ExitWindowsEx(EWX_REBOOT | EWX_BOOTOPTIONS, 0);
	else if(sender() == m_pMenuShutdown)
		// Note: EWX_SHUTDOWN does not power off
		bSuccess = (ExitWindowsEx(EWX_POWEROFF, 0) || ExitWindowsEx(EWX_SHUTDOWN, 0));
	else if (sender() == m_pMenuForceShutdown)
	{
		switch (SoftForce)
		{
		case 1:	// Don't send the WM_QUERYENDSESSION message (This flag has no effect if terminal services is enabled.)
			bSuccess = ExitWindowsEx(EWX_POWEROFF | EWX_FORCE, 0) || ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, 0);
			break;
		case 2:	// Terminate processes not responding to the WM_QUERYENDSESSION or WM_ENDSESSION message.
			bSuccess = ExitWindowsEx(EWX_POWEROFF | EWX_FORCEIFHUNG, 0) || ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCEIFHUNG, 0);
			break;
		default:
			bSuccess = NtShutdownSystem(ShutdownPowerOff);
		}
	}
	else if (sender() == m_pMenuHybridShutdown)
		bSuccess = ExitWindowsEx(EWX_POWEROFF | EWX_HYBRID_SHUTDOWN, 0);

	if (!bSuccess)
		QMessageBox::critical(NULL, "TaskExplorer", tr("Failed to %1, due to: %2").arg(((QAction*)sender())->text()).arg(CastPhString(PhGetWin32Message(GetLastError()))));
#else
	// linux-todo:
#endif
}

void CTaskExplorer::UpdateUserMenu()
{
	QList<CSystemAPI::SUser> Users = theAPI->GetUsers();

	m_pMenuUsers->setTitle(tr("Users (%1)").arg(Users.size()));

	QMap<int, QMenu*> OldMenus = m_UserMenus;
	
	foreach(const CSystemAPI::SUser& User, Users)
	{
		QMenu* pMenu = OldMenus.take(User.SessionId);
		if (!pMenu)
		{
			pMenu = m_pMenuUsers->addMenu(MakeActionIcon(":/Actions/User"), QString());
			pMenu->setProperty("SessionId", User.SessionId);

			pMenu->addAction(MakeActionIcon(":/Actions/Connect"), tr("Connect"), this, SLOT(OnUserAction()))->setProperty("Action", eUserConnect);
			pMenu->addAction(MakeActionIcon(":/Actions/Disconnect"), tr("Disconnect"), this, SLOT(OnUserAction()))->setProperty("Action", eUserDisconnect);
			pMenu->addAction(MakeActionIcon(":/Actions/Logoff"), tr("Logoff"), this, SLOT(OnUserAction()))->setProperty("Action", eUserLogoff);
			/*pMenu->addSeparator();
			pMenu->addAction(MakeActionIcon(":/Actions/SendMsg"), tr("Send message..."), this, SLOT(OnUserAction()))->setProperty("Action", eSendMessage);
			pMenu->addAction(MakeActionIcon(":/Actions/UserInfo"), tr("Properties"), this, SLOT(OnUserAction()))->setProperty("Action", eUserInfo);*/

			m_UserMenus.insert(User.SessionId, pMenu);
		}

		pMenu->setTitle(tr("%1: %2 (%3)").arg(User.SessionId).arg(User.UserName).arg(User.Status));
	}

	foreach(int SessionId, OldMenus.keys())
		delete m_UserMenus.take(SessionId);
}

void CTaskExplorer::OnUserAction()
{
	QAction* pAction = (QAction*)sender();
	int SessionId = pAction->parent()->property("SessionId").toInt();

#ifdef WIN32
	bool bSuccess = false;

	switch (pAction->property("Action").toInt())
	{
		case eUserConnect:
		{
			// Try once with no password.
			if (WinStationConnectW(NULL, SessionId, LOGONID_CURRENT, L"", TRUE))
				bSuccess = true;
			else
			{
				QString Password = QInputDialog::getText(this, "TaskExplorer", tr("Connect to session, enter Password:"));
				if (!Password.isEmpty())
					bSuccess = WinStationConnectW(NULL, SessionId, LOGONID_CURRENT, (wchar_t*)Password.toStdWString().c_str(), TRUE);
			}
			break;
		}
		case eUserDisconnect:	
			bSuccess = WinStationDisconnect(NULL, SessionId, FALSE);
			break;
		case eUserLogoff:
			bSuccess = WinStationReset(NULL, SessionId, FALSE);
			break;
		/*case eSendMessage:
		{
			//bSuccess = WinStationSendMessageW(NULL, SessionId, title->Buffer, (ULONG)title->Length, text->Buffer, (ULONG)text->Length, icon, (ULONG)timeout, &response, TRUE);
			break;
		}
		case eUserInfo:	
		{

			break;
		}*/
	}

	if (!bSuccess)
		QMessageBox::critical(NULL, "TaskExplorer", tr("Failed to %1, due to: %2").arg(((QAction*)sender())->text()).arg(CastPhString(PhGetWin32Message(GetLastError()))));
#endif
}

void CTaskExplorer::OnSysTray(QSystemTrayIcon::ActivationReason Reason)
{
	switch(Reason)
	{
		case QSystemTrayIcon::Context:
			m_pTrayMenu->popup(QCursor::pos());	
			break;
		case QSystemTrayIcon::DoubleClick:
			if (isVisible())
			{
				hide();
				break;
			}
			
			show();
		case QSystemTrayIcon::Trigger:
#ifdef WIN32
		{
			WINDOWPLACEMENT placement = { sizeof(placement) };
			GetWindowPlacement(PhMainWndHandle, &placement);

			if (placement.showCmd == SW_MINIMIZE || placement.showCmd == SW_SHOWMINIMIZED)
				ShowWindowAsync(PhMainWndHandle, SW_RESTORE);
			else
				SetForegroundWindow(PhMainWndHandle);
		}
#endif
			break;
	}
}

void CTaskExplorer::OnShowHide()
{
	OnSysTray(QSystemTrayIcon::DoubleClick);
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
}

void CTaskExplorer::UpdateOptions()
{
	ApplyOptions();

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
	emit ReloadPanels();

	QTimer::singleShot(0, this, SLOT(UpdateAll()));
}

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
	QMetaObject::invokeMethod(theAPI, "UpdateServiceList", Qt::QueuedConnection, Q_ARG(bool, true));
}

#ifdef WIN32
NTSTATUS NTAPI CTaskExplorer_OpenServiceControlManager(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
{
    SC_HANDLE serviceHandle;
    if (serviceHandle = OpenSCManager(NULL, NULL, DesiredAccess))
    {
        *Handle = serviceHandle;
        return STATUS_SUCCESS;
    }
    return PhGetLastWin32ErrorAsNtStatus();
}
#endif

void CTaskExplorer::OnSCMPermissions()
{
#ifdef WIN32
	PhEditSecurity(NULL, L"Service Control Manager", L"SCManager", CTaskExplorer_OpenServiceControlManager, NULL, NULL);
#endif
}

void CTaskExplorer::OnSecurityExplorer()
{
#ifdef _WIN32
	CSecurityExplorer* pWnd = new CSecurityExplorer();
	pWnd->show();
#endif
}

void CTaskExplorer::OnFreeMemory()
{
#ifdef WIN32
	SYSTEM_MEMORY_LIST_COMMAND command = MemoryCommandMax;

	if (sender() == m_pMenuFreeWorkingSet)
		command = MemoryEmptyWorkingSets;
	else if (sender() == m_pMenuFreeModPages)
		command = MemoryFlushModifiedList;
	else if (sender() == m_pMenuFreeStandby)
		command = MemoryPurgeStandbyList;
	else if (sender() == m_pMenuFreePriority0)
		command = MemoryPurgeLowPriorityStandbyList;
	
	QApplication::setOverrideCursor(Qt::WaitCursor);

	NTSTATUS status;
	if (command == MemoryCommandMax)
	{
		MEMORY_COMBINE_INFORMATION_EX combineInfo = { 0 };
		status = NtSetSystemInformation(SystemCombinePhysicalMemoryInformation, &combineInfo, sizeof(MEMORY_COMBINE_INFORMATION_EX));
	}
	else
	{
		status = NtSetSystemInformation(SystemMemoryListInformation, &command, sizeof(SYSTEM_MEMORY_LIST_COMMAND));
		if (status == STATUS_PRIVILEGE_NOT_HELD)
		{
			QString SocketName = CTaskService::RunWorker();
			if (!SocketName.isEmpty())
			{
				QVariantMap Parameters;
				Parameters["Command"] = (int)command;

				QVariantMap Request;
				Request["Command"] = "FreeMemory";
				Request["Parameters"] = Parameters;

				status = CTaskService::SendCommand(SocketName, Request).toInt();
			}
		}
	}

	QApplication::restoreOverrideCursor();

	if (!NT_SUCCESS(status)) 
		QMessageBox::warning(NULL, "TaskExplorer", tr("Memory opertion failed; Error: %1").arg(status));
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

void CTaskExplorer::OnMonitorETW()
{
#ifdef WIN32
	if (m_pMenuMonitorETW->isChecked())
	{
		((CWindowsAPI*)theAPI)->MonitorETW(true);
		m_pMenuMonitorETW->setChecked(((CWindowsAPI*)theAPI)->IsMonitoringETW());
	}
	else
		((CWindowsAPI*)theAPI)->MonitorETW(false);
	theConf->SetValue("Options/MonitorETW", m_pMenuMonitorETW->isChecked());
#endif
}

void CTaskExplorer::OnMonitorFW()
{
#ifdef WIN32
	if (m_pMenuMonitorFW->isChecked())
	{
		((CWindowsAPI*)theAPI)->MonitorFW(true);
		m_pMenuMonitorFW->setChecked(((CWindowsAPI*)theAPI)->IsMonitoringETW());
	}
	else
		((CWindowsAPI*)theAPI)->MonitorFW(false);
	theConf->SetValue("Options/MonitorFirewall", m_pMenuMonitorFW->isChecked());
#endif
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

float CTaskExplorer::GetDpiScale()
{
	return QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.0;// *100.0;
}

int CTaskExplorer::GetCellHeight()
{
	QFontMetrics fontMetrics(QApplication::font());
	int fontHeight = fontMetrics.height();
	
	return (fontHeight + 3) * GetDpiScale();
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
	QString ColorStr;
	switch (Color)
	{
	case eGraphBack:	ColorStr = theConf->GetString("Colors/GraphBack", "#808080"); break;
	case eGraphFront:	ColorStr = theConf->GetString("Colors/GraphFront", "#FFFFFF"); break;

	case ePlotBack:		ColorStr = theConf->GetString("Colors/PlotBack", "#EFEFEF"); break; // QColor(0, 0, 64);
	case ePlotFront:	ColorStr = theConf->GetString("Colors/PlotFront", "#505050"); break; // QColor(187, 206, 239);
	case ePlotGrid:		ColorStr = theConf->GetString("Colors/PlotGrid", "#C7C7C7"); break; // QColor(46, 44, 119);
	}
	if (!ColorStr.isNull())
		return QColor(ColorStr);

	return GetListColor(Color);
}

QColor CTaskExplorer::GetListColor(int Color)
{
	QString ColorStr;
	switch (Color)
	{
	case eToBeRemoved:	ColorStr = theConf->GetString("Colors/ToBeRemoved", "#F08080"); break;
	case eAdded:		ColorStr = theConf->GetString("Colors/NewlyCreated", "#00FF7F"); break;
	
#ifdef WIN32
	case eDangerous:	ColorStr = theConf->GetString("Colors/DangerousProcess", "#FF0000"); break;
#endif
	case eSystem:		ColorStr = theConf->GetString("Colors/SystemProcess", "#AACCFF"); break;
	case eUser:			ColorStr = theConf->GetString("Colors/UserProcess", "#FFFF80"); break;
	case eService:		ColorStr = theConf->GetString("Colors/ServiceProcess", "#80FFFF"); break;
#ifdef WIN32
	case eJob:			ColorStr = theConf->GetString("Colors/JobProcess", "#D49C5C"); break;
	case ePico:			ColorStr = theConf->GetString("Colors/PicoProcess", "#42A0FF"); break;
	case eImmersive:	ColorStr = theConf->GetString("Colors/ImmersiveProcess", "#FFE6FF"); break;
	case eDotNet:		ColorStr = theConf->GetString("Colors/NetProcess", "#DCFF00"); break;
#endif
	case eElevated:		ColorStr = theConf->GetString("Colors/ElevatedProcess", "#FFBB30"); break;
#ifdef WIN32
	case eDriver:		ColorStr = theConf->GetString("Colors/KernelServices", "#FFC880"); break;
	case eGuiThread:	ColorStr = theConf->GetString("Colors/GuiThread", "#AACCFF"); break;
	case eIsInherited:	ColorStr = theConf->GetString("Colors/IsInherited", "#77FFFF"); break;
	case eIsProtected:	ColorStr = theConf->GetString("Colors/IsProtected", "#FF77FF"); break;
#endif
	case eExecutable:	ColorStr = theConf->GetString("Colors/Executable", "#FF90E0"); break;
	}

	StrPair ColorUse = Split2(ColorStr, ";");
	if (ColorUse.second.isEmpty() || ColorUse.second.compare("true", Qt::CaseInsensitive) == 0 || ColorUse.second.toInt() != 0)
		return QColor(ColorUse.first);

	return QColor(theConf->GetString("Colors/Background", "#FFFFFF"));
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
			"<p>by DavidXanatos</p>"
			"<p>Copyright (c) 2019</p>"
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
			"<p></p>"
			"<p></p>"
			"<p>Icons from <a href=\"https://icons8.com\">icons8.com</a></p>"
			"<p></p>"
		);
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
		PhShowAbout(this);
#endif
	else if (sender() == m_pMenuAboutQt)
		QMessageBox::aboutQt(this);
	else
		QDesktopServices::openUrl(QUrl("https://www.patreon.com/DavidXanatos"));
}
