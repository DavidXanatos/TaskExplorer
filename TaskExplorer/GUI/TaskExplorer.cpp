#include "stdafx.h"
#include "TaskExplorer.h"
#ifdef WIN32
#include "../API/Windows/WindowsAPI.h"
#include "../API/Windows/ProcessHacker/RunAs.h"
#include "../API/Windows/WinAdmin.h"
#endif
#include "../Common/ExitDialog.h"
#include "../Common/HistoryGraph.h"
#include "NewService.h"
#include "RunAsDialog.h"
#include "../SVC/TaskService.h"
#include "GraphBar.h"
#include "SettingsWindow.h"
#include "CustomItemDelegate.h"

CSystemAPI*	theAPI = NULL;

QIcon g_ExeIcon;
QIcon g_DllIcon;

CSettings* theConf = NULL;
CTaskExplorer* theGUI = NULL;


#if defined(Q_OS_WIN)
#include <wtypes.h>
#include <QAbstractNativeEventFilter>

class CNativeEventFilter : public QAbstractNativeEventFilter
{
public:
	virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *result)
	{
		if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
			MSG *msg = static_cast<MSG *>(message);
			if (msg->message == WM_NOTIFY) {
				LRESULT ret;
				if (PhMwpOnNotify((NMHDR *)msg->lParam, &ret))
					*result = ret;
				return true;
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

	this->setWindowTitle(tr("TaskExplorer v%1").arg(GetVersion()));

	theAPI = CSystemAPI::New();

	connect(theAPI, SIGNAL(InitDone()), this, SLOT(OnInitDone()));

#if defined(Q_OS_WIN)
	PhMainWndHandle = (HWND)QWidget::winId();

    QApplication::instance()->installNativeEventFilter(new CNativeEventFilter);
#endif

	if (g_ExeIcon.isNull())
	{
		g_ExeIcon = QIcon(":/Icons/exe16.png");
		g_ExeIcon.addFile(":/Icons/exe32.png");
		g_ExeIcon.addFile(":/Icons/exe48.png");
		g_ExeIcon.addFile(":/Icons/exe64.png");
	}

	if (g_ExeIcon.isNull())
	{
		g_DllIcon = QIcon(":/Icons/dll16.png");
		g_DllIcon.addFile(":/Icons/dll32.png");
		g_DllIcon.addFile(":/Icons/dll48.png");
		g_DllIcon.addFile(":/Icons/dll64.png");
	}

	m_bExit = false;

	// a shared item deleagate for all lists
	m_pCustomItemDelegate = new CCustomItemDelegate(GetCellHeight() + 1, this);

	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout(m_pMainWidget);
	m_pMainLayout->setMargin(0);
	this->setCentralWidget(m_pMainWidget);

	m_pGraphSplitter = new QSplitter();
	m_pGraphSplitter->setOrientation(Qt::Vertical);
	m_pMainLayout->addWidget(m_pGraphSplitter);

	
	
	m_pGraphBar = new CGraphBar();
	//m_pMainLayout->addWidget(m_pGraphBar);
	m_pGraphSplitter->addWidget(m_pGraphBar);
	m_pGraphSplitter->setStretchFactor(0, 0);
	m_pGraphSplitter->setSizes(QList<int>() << 80); // default size of 80
	connect(m_pGraphBar, SIGNAL(Resized(int)), this, SLOT(OnGraphsResized(int)));

	m_pMainSplitter = new QSplitter();
	m_pMainSplitter->setOrientation(Qt::Horizontal);
	//m_pMainLayout->addWidget(m_pMainSplitter);
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

	connect(m_pProcessTree, SIGNAL(ProcessClicked(const CProcessPtr)), m_pTaskInfo, SLOT(ShowProcess(const CProcessPtr)));


	m_pMenuProcess = menuBar()->addMenu(tr("&Process"));
		m_pMenuProcess->addAction(tr("Run..."), this, SLOT(OnRun()));
		m_pMenuProcess->addAction(tr("Run as Administrator..."), this, SLOT(OnRunAdmin()));
		m_pMenuProcess->addAction(tr("Run as Limited User..."), this, SLOT(OnRunUser()));
		QAction* m_pMenuRunAs = m_pMenuProcess->addAction(tr("Run as..."), this, SLOT(OnRunAs()));
#ifdef WIN32
		QAction* m_pMenuRunSys = m_pMenuProcess->addAction(tr("Run as TrustedInstaller..."), this, SLOT(OnRunSys()));
#endif
		QAction* m_pElevate = m_pMenuProcess->addAction(tr("Restart Elevated"), this, SLOT(OnElevate()));
		m_pElevate->setIcon(QIcon(":/Icons/Shield.png"));
		m_pMenuProcess->addSeparator();
		m_pMenuProcess->addAction(tr("Exit"), this, SLOT(OnExit()));

#ifdef WIN32
		m_pElevate->setVisible(!PhGetOwnTokenAttributes().Elevated);
#endif

	m_pMenuView = menuBar()->addMenu(tr("&View"));
		m_pMenuSysTabs = m_pMenuView->addMenu(tr("System Tabs"));
		for (int i = 0; i < m_pSystemInfo->GetTabCount(); i++)
		{
			QAction* pAction = m_pMenuSysTabs->addAction(m_pSystemInfo->GetTabLabel(i), this, SLOT(OnSysTab()));
			pAction->setCheckable(true);
			pAction->setChecked(m_pSystemInfo->IsTabVisible(i));
			m_Act2Tab[pAction] = i;
		}

#ifdef WIN32
		m_pMenuSysTabs->addSeparator();
		m_pMenuKernelServices = m_pMenuSysTabs->addAction(tr("Show Kernel Services"), this, SLOT(OnKernelServices()));
		m_pMenuKernelServices->setCheckable(true);
		m_pMenuKernelServices->setChecked(theConf->GetBool("MainWindow/ShowDrivers", true));
		OnKernelServices();
#endif

		m_pMenuTaskTabs = m_pMenuView->addMenu(tr("Task Tabs"));
		for (int i = 0; i < m_pTaskInfo->GetTabCount(); i++)
		{
			QAction* pAction = m_pMenuTaskTabs->addAction(m_pTaskInfo->GetTabLabel(i), this, SLOT(OnTaskTab()));
			pAction->setCheckable(true);
			pAction->setChecked(m_pTaskInfo->IsTabVisible(i));
			m_Act2Tab[pAction] = i;
		}

		m_pMenuView->addSeparator();
		m_pMenuPauseRefresh = m_pMenuView->addAction(tr("Pause Refresh"));
		m_pMenuPauseRefresh->setCheckable(true);
		m_pMenuRefreshNow = m_pMenuView->addAction(tr("Refresh Now"), this, SLOT(UpdateAll()));

	m_pMenuOptions = menuBar()->addMenu(tr("&Options"));
		m_pMenuSettings = m_pMenuOptions->addAction(tr("Settings"), this, SLOT(OnSettings()));
		m_pMenuOptions->addSeparator();
		m_pMenuAutoRun = m_pMenuOptions->addAction(tr("Auto Run"), this, SLOT(OnAutoRun()));
		m_pMenuAutoRun->setCheckable(true);
		m_pMenuAutoRun->setChecked(IsAutorunEnabled());
#ifdef WIN32
		m_pMenuUAC = m_pMenuOptions->addAction(tr("Skip UAC"), this, SLOT(OnSkipUAC()));
		m_pMenuUAC->setCheckable(true);
		m_pMenuUAC->setEnabled(PhGetOwnTokenAttributes().Elevated);
		m_pMenuUAC->setChecked(SkipUacRun(true));
#endif

	m_pMenuTools = menuBar()->addMenu(tr("&Tools"));
		m_pMenuServices = m_pMenuTools->addMenu(tr("&Services"));
			m_pMenuCreateService = m_pMenuServices->addAction(tr("Create new Service"), this, SLOT(OnCreateService()));
			m_pMenuCreateService->setEnabled(PhGetOwnTokenAttributes().Elevated);
			m_pMenuUpdateServices = m_pMenuServices->addAction(tr("ReLoad all Service"), this, SLOT(OnReloadService()));
#ifdef WIN32
			m_pMenuSCMPermissions = m_pMenuServices->addAction(tr("Service Control Manager Permissions"), this, SLOT(OnSCMPermissions()));
#endif
		m_pMenuTools->addSeparator();
#ifdef WIN32
		m_pMenuETW = m_pMenuTools->addAction(tr("Monitor ETW Events"), this, SLOT(OnMonitorETW()));
		m_pMenuETW->setCheckable(true);
#endif

	m_pMenuHelp = menuBar()->addMenu(tr("&Help"));
		m_pMenuSupport = m_pMenuHelp->addAction(tr("Support TaskExplorer on Patreon"), this, SLOT(OnAbout()));
		m_pMenuHelp->addSeparator();
#ifdef WIN32
		m_pMenuAboutPH = m_pMenuHelp->addAction(tr("About ProcessHacker Library"), this, SLOT(OnAbout()));
#endif
		m_pMenuAboutQt = m_pMenuHelp->addAction(tr("About the Qt Framework"), this, SLOT(OnAbout()));
		m_pMenuHelp->addSeparator();
		m_pMenuAbout = m_pMenuHelp->addAction(QIcon(":/TaskExplorer.png"), tr("About TaskExplorer"), this, SLOT(OnAbout()));


	restoreGeometry(theConf->GetBlob("MainWindow/Window_Geometry"));
	m_pMainSplitter->restoreState(theConf->GetBlob("MainWindow/Window_Splitter"));
	m_pPanelSplitter->restoreState(theConf->GetBlob("MainWindow/Panel_Splitter"));
	m_pGraphSplitter->restoreState(theConf->GetBlob("MainWindow/Graph_Splitter"));



	bool bAutoRun = QApplication::arguments().contains("-autorun");

	QIcon Icon;
	Icon.addFile(":/TaskExplorer.png");
	m_pTrayIcon = new QSystemTrayIcon(Icon, this);
	m_pTrayIcon->setToolTip("TaskExplorer");
	connect(m_pTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(OnSysTray(QSystemTrayIcon::ActivationReason)));
	//m_pTrayIcon->setContextMenu(m_pNeoMenu);

	m_pTrayMenu = new QMenu();
	m_pTrayMenu->addAction(tr("Exit"), this, SLOT(OnExit()));

	m_pTrayIcon->show(); // Note: qt bug; without a first show hide does not work :/
	if(!bAutoRun && !theConf->GetBool("SysTray/Show", true))
		m_pTrayIcon->hide();

	//QLabel* m_pInfo = new QLabel(tr("test"));
	//statusBar()->addPermanentWidget(m_pInfo);

	m_pCustomItemDelegate->m_Grid = theConf->GetBool("Options/ShowGrid", true);
	m_pCustomItemDelegate->m_Color = QColor(theConf->GetString("Colors/GridColor", "#808080"));

	m_pGraphBar->UpdateLengths();

	//m_uTimerCounter = 0;
	m_uTimerID = startTimer(theConf->GetInt("Options/RefreshInterval", 1000));

	if (!bAutoRun)
		show();

	statusBar()->showMessage(tr("TaskExplorer is ready..."));

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

	delete theAPI;

	theGUI = NULL;
}

void CTaskExplorer::OnInitDone()
{
	m_pMenuETW->setChecked(((CWindowsAPI*)theAPI)->IsMonitoringETW());
}

void CTaskExplorer::OnGraphsResized(int Size)
{
	QList<int> Sizes = m_pGraphSplitter->sizes();
	Sizes[1] += Sizes[0] - Size;
	Sizes[0] = Size;
	m_pGraphSplitter->setSizes(Sizes);
}

void CTaskExplorer::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() != m_uTimerID)
		return;

	if(!m_pMenuPauseRefresh->isChecked())
		UpdateAll();
}

void CTaskExplorer::closeEvent(QCloseEvent *e)
{
	if (m_bExit)
		return;

	if(m_pTrayIcon->isVisible() && theConf->GetBool("SysTray/CloseToTray", true))
	{
		hide();
		e->ignore();
	}
	else
	{
		CExitDialog ExitDialog(tr("Do you want to close TaskExplorer?"));
		if(ExitDialog.exec())
			return;

		e->ignore();
	}
}

void CTaskExplorer::UpdateAll()
{
	QTimer::singleShot(0, theAPI, SLOT(UpdateProcessList()));
	QTimer::singleShot(0, theAPI, SLOT(UpdateSocketList()));

	QTimer::singleShot(0, theAPI, SLOT(UpdateSysStats()));

	QTimer::singleShot(0, theAPI, SLOT(UpdateServiceList()));
	QTimer::singleShot(0, theAPI, SLOT(UpdateDriverList()));


	m_pGraphBar->UpdateGraphs();
	m_pTaskInfo->Refresh();
	m_pSystemInfo->Refresh();

	UpdateTray();
}

void CTaskExplorer::UpdateTray()
{
	if (!m_pTrayIcon->isVisible())
		return;

	quint64 RamUsage = theAPI->GetPhysicalUsed();
	quint64 SwapedMemory = theAPI->GetSwapedOutMemory();
	quint64 CommitedMemory = theAPI->GetCommitedMemory();

	quint64 InstalledMemory = theAPI->GetInstalledMemory();
	quint64 TotalSwap = theAPI->GetTotalSwapMemory();

	quint64 TotalMemory = Max(theAPI->GetInstalledMemory(), theAPI->GetCommitedMemory()); // theAPI->GetMemoryLimit();

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
		if (!m_TrayGraph.isNull()) 
		{
			m_TrayGraph = QImage();

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

		QMap<int, CHistoryGraph::SValue> TrayValues;
		TrayValues[0] = { theAPI->GetCpuUsage(), Qt::green };
		TrayValues[1] = { theAPI->GetCpuKernelUsage(), Qt::red };
		TrayValues[2] = { theAPI->GetCpuDPCUsage(), Qt::blue };
		// Note: we may add an cuttof show 0 below 10%
		CHistoryGraph::Update(m_TrayGraph, QColor(0, 128, 0), TrayValues, TrayIcon.height(), TrayIcon.width() - offset);

		qp.translate(TrayIcon.width() - m_TrayGraph.height(), TrayIcon.height());
		qp.rotate(270);
		qp.drawImage(0, 0, m_TrayGraph);
	}

	m_pTrayIcon->setIcon(QIcon(QPixmap::fromImage(TrayIcon)));
}

void CTaskExplorer::CheckErrors(QList<STATUS> Errors)
{
	if (Errors.isEmpty())
		return;

	// todo : show window with error list

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
    PhShowRunFileDialog(PhMainWndHandle, NULL, NULL, NULL, L"Type the name of a program that will be opened as system with the TrustedInstaller's token.", 0);
	/*STATUS status = RunAsTrustedInstaller(L"");
	if(!NT_SUCCESS(status))
		QMessageBox::warning(NULL, "TaskExplorer", tr("Run As TristedInstaller failed, Error:").arg(status));*/
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

void CTaskExplorer::OnSysTray(QSystemTrayIcon::ActivationReason Reason)
{
	switch(Reason)
	{
		case QSystemTrayIcon::Context:
			m_pTrayMenu->popup(QCursor::pos());	
			break;
		case QSystemTrayIcon::DoubleClick:
			show();
			break;
	}
}

void CTaskExplorer::OnSysTab()
{
	QAction* pAction = (QAction*)sender();
	int Index = m_Act2Tab.value(pAction);
	m_pSystemInfo->ShowTab(Index, pAction->isChecked());
}

void CTaskExplorer::OnKernelServices()
{
	theConf->SetValue("MainWindow/ShowDrivers", m_pMenuKernelServices->isChecked());
	m_pSystemInfo->SetShowKernelServices(m_pMenuKernelServices->isChecked());
}

void CTaskExplorer::OnTaskTab()
{
	QAction* pAction = (QAction*)sender();
	int Index = m_Act2Tab.value(pAction);
	m_pTaskInfo->ShowTab(Index, pAction->isChecked());
}

void CTaskExplorer::OnSettings()
{
	CSettingsWindow* pSettingsWindow = new CSettingsWindow();
	connect(pSettingsWindow, SIGNAL(OptionsChanged()), this, SLOT(UpdateOptions()));
	pSettingsWindow->show();
}

void CTaskExplorer::UpdateOptions()
{
	m_pCustomItemDelegate->m_Grid = theConf->GetBool("Options/ShowGrid", true);
	m_pCustomItemDelegate->m_Color = QColor(theConf->GetString("Colors/GridColor", "#808080"));

	m_pGraphBar->UpdateLengths();

	killTimer(m_uTimerID);
	m_uTimerID = startTimer(theConf->GetInt("Options/RefreshInterval", 1000));

	emit ReloadAll();

	QTimer::singleShot(0, this, SLOT(UpdateAll()));
}

void CTaskExplorer::OnAutoRun()
{
	AutorunEnable(m_pMenuAutoRun->isChecked());
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
NTSTATUS PhpOpenServiceControlManager(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
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
	PhEditSecurity(NULL, L"Service Control Manager", L"SCManager", (PPH_OPEN_OBJECT)PhpOpenServiceControlManager, NULL, NULL);
#endif
}

void CTaskExplorer::OnMonitorETW()
{
#ifdef WIN32
	theConf->SetValue("Options/MonitorETW", m_pMenuETW->isChecked());
	((CWindowsAPI*)theAPI)->MonitorETW(m_pMenuETW->isChecked());
#endif
}

QStyledItemDelegate* CTaskExplorer::GetItemDelegate() 
{
	return m_pCustomItemDelegate; 
}

int CTaskExplorer::GetCellHeight()
{
	return 16;
}

QColor CTaskExplorer::GetColor(int Color)
{
	QString ColorStr;
	switch (Color)
	{
	case eToBeRemoved:	ColorStr = theConf->GetString("Colors/ToBeRemoved", "#F08080"); break;
	case eAdded:		ColorStr = theConf->GetString("Colors/NewlyCreated", "#00FF7F"); break;
	
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
	else if (sender() == m_pMenuSupport)
	{
		QDesktopServices::openUrl(QUrl("https://www.patreon.com/DavidXanatos"));
	}
#ifdef WIN32
	else if (sender() == m_pMenuAboutPH)
		PhShowAbout(this);
#endif
	else //if (sender() == m_pMenuAboutQt)
		QMessageBox::aboutQt(this);
}