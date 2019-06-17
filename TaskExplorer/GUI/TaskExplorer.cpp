#include "stdafx.h"
#include "TaskExplorer.h"
#ifdef WIN32
#include "../API/Windows/ProcessHacker.h"
#endif
#include "../Common/ExitDialog.h"

CSystemAPI*	theAPI = NULL;

QIcon g_ExeIcon;
QIcon g_DllIcon;

CSettings* theConf = NULL;


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
	g_ExeIcon = QIcon(":/Icons/exe16.png");
	g_ExeIcon.addFile(":/Icons/exe32.png");
	g_ExeIcon.addFile(":/Icons/exe48.png");
	g_ExeIcon.addFile(":/Icons/exe64.png");
	g_DllIcon = QIcon(":/Icons/dll16.png");
	g_DllIcon.addFile(":/Icons/dll32.png");
	g_DllIcon.addFile(":/Icons/dll48.png");
	g_DllIcon.addFile(":/Icons/dll64.png");

	theAPI = CSystemAPI::New();

#if defined(Q_OS_WIN)
	PhMainWndHandle = (HWND)QWidget::winId();

    QApplication::instance()->installNativeEventFilter(new CNativeEventFilter);
#endif

	m_bExit = false;

	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout(m_pMainWidget);
	m_pMainLayout->setMargin(0);
	this->setCentralWidget(m_pMainWidget);
	
	m_pGraphBar = new CGraphBar();
	m_pMainLayout->addWidget(m_pGraphBar);

	m_pMainSplitter = new QSplitter();
	m_pMainSplitter->setOrientation(Qt::Horizontal);
	m_pMainLayout->addWidget(m_pMainSplitter);

	m_pProcessTree = new CProcessTree(this);
	//m_pProcessTree->setMinimumSize(200, 200);
	m_pMainSplitter->addWidget(m_pProcessTree);
	m_pMainSplitter->setCollapsible(0, false);

	m_pSubSplitter = new QSplitter();
	m_pSubSplitter->setOrientation(Qt::Vertical);
	m_pMainSplitter->addWidget(m_pSubSplitter);

	m_pSystemInfo = new CSystemInfoView();
	//m_pSystemInfo->setMinimumSize(200, 200);
	m_pSubSplitter->addWidget(m_pSystemInfo);

	m_pTaskInfo = new CTaskInfoView();
	//m_pTaskInfo->setMinimumSize(200, 200);
	m_pSubSplitter->addWidget(m_pTaskInfo);
	m_pSubSplitter->setCollapsible(1, false);

	connect(m_pProcessTree, SIGNAL(ProcessClicked(const CProcessPtr)), m_pTaskInfo, SLOT(ShowProcess(const CProcessPtr)));


	m_pMenuProcess = menuBar()->addMenu(tr("&Process"));
		m_pMenuProcess->addAction(tr("Run..."), this, SLOT(OnRun()));
		m_pMenuProcess->addAction(tr("Run as Administrator..."), this, SLOT(OnRunAdmin()));
		m_pMenuProcess->addAction(tr("Run as limited User..."), this, SLOT(OnRunUser()));
		QAction* m_pMenuRunAs = m_pMenuProcess->addAction(tr("Run as..."), this, SLOT(OnRunAs()));
		m_pMenuRunAs->setEnabled(false); // todo
		QAction* m_pMenuRunSys = m_pMenuProcess->addAction(tr("Run as TrustedInstaller..."), this, SLOT(OnRunSys()));
		m_pMenuRunSys->setEnabled(false); // todo
		QAction* m_pElevate = m_pMenuProcess->addAction(tr("Restart Elevated"), this, SLOT(OnElevate()));
		m_pElevate->setEnabled(false); // todo
		m_pMenuProcess->addSeparator();
		m_pMenuProcess->addAction(tr("Exit"), this, SLOT(OnExit()));

#ifdef WIN32
	m_pElevate->setVisible(!PhGetOwnTokenAttributes().Elevated);
#endif

	QMenu* m_pMenuView = menuBar()->addMenu(tr("&View"));
		QMenu* m_pMenuSysTabs = m_pMenuView->addMenu(tr("System Tabs"));
		for (int i = 0; i < m_pSystemInfo->GetTabCount(); i++)
		{
			QAction* pAction = m_pMenuSysTabs->addAction(m_pSystemInfo->GetTabLabel(i), this, SLOT(OnSysTab()));
			pAction->setCheckable(true);
			pAction->setChecked(m_pSystemInfo->IsTabVisible(i));
			m_Act2Tab[pAction] = i;
		}
		QMenu* m_pMenuTaskTabs = m_pMenuView->addMenu(tr("Task Tabs"));
		for (int i = 0; i < m_pTaskInfo->GetTabCount(); i++)
		{
			QAction* pAction = m_pMenuTaskTabs->addAction(m_pSystemInfo->GetTabLabel(i), this, SLOT(OnTaskTab()));
			pAction->setCheckable(true);
			pAction->setChecked(m_pSystemInfo->IsTabVisible(i));
			m_Act2Tab[pAction] = i;
		}



	QMenu* m_pMenuOptions = menuBar()->addMenu(tr("&Options"));
		QAction* m_pMenuConf = m_pMenuOptions->addAction(tr("Preferences"), this, SLOT(OnPrefs()));
		m_pMenuConf->setEnabled(false); // todo

	//QMenu* m_pMenuTools = menuBar()->addMenu(tr("&Tools")); // todo

	m_pMenuHelp = menuBar()->addMenu(tr("&Help"));
	m_pMenuAbout = m_pMenuHelp->addAction(tr("About TaskExplorer"), this, SLOT(OnAbout()));
	m_pMenuHelp->addSeparator();
#ifdef WIN32
	m_pMenuAboutPH = m_pMenuHelp->addAction(tr("About PHlib"), this, SLOT(OnAbout()));
#endif
	m_pMenuAboutQt = m_pMenuHelp->addAction(tr("About Qt"), this, SLOT(OnAbout()));

		

	QIcon Icon;
	Icon.addFile(":/TaskExplorer.png");
	m_pTrayIcon = new QSystemTrayIcon(Icon, this);
	m_pTrayIcon->setToolTip(tr("TaskExplorer"));
	connect(m_pTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(OnSysTray(QSystemTrayIcon::ActivationReason)));
	//m_pTrayIcon->setContextMenu(m_pNeoMenu);

	if(theConf->GetValue("SysTray/Show", true).toBool())
		m_pTrayIcon->show();

	m_pTrayMenu = new QMenu();
	m_pTrayMenu->addAction(tr("Exit"), this, SLOT(OnExit()));

	//QLabel* m_pInfo = new QLabel(tr("test"));
	//statusBar()->addPermanentWidget(m_pInfo);

	restoreGeometry(theConf->GetValue("MainWindow/Window_Geometry").toByteArray());
	m_pMainSplitter->restoreState(theConf->GetValue("MainWindow/Window_Splitter").toByteArray());
	m_pSubSplitter->restoreState(theConf->GetValue("MainWindow/Panel_Splitter").toByteArray());

	//m_uTimerCounter = 0;
	m_uTimerID = startTimer(1000); // todo add setting

	statusBar()->showMessage(tr("TaskExplorer is ready..."));

	UpdateAll();
}

CTaskExplorer::~CTaskExplorer()
{
	killTimer(m_uTimerID);

	m_pTrayIcon->hide();

	theConf->SetValue("MainWindow/Window_Geometry",saveGeometry());
	theConf->SetValue("MainWindow/Window_Splitter",m_pMainSplitter->saveState());
	theConf->SetValue("MainWindow/Panel_Splitter",m_pSubSplitter->saveState());

	delete theAPI;
}

void CTaskExplorer::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() != m_uTimerID)
		return;

	UpdateAll();
}

void CTaskExplorer::closeEvent(QCloseEvent *e)
{
	if (m_bExit)
		return;

	if(m_pTrayIcon->isVisible() && theConf->GetValue("SysTray/CloseToTray", false).toBool())
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

	QTimer::singleShot(0, theAPI, SLOT(UpdateServiceList()));
	QTimer::singleShot(0, theAPI, SLOT(UpdateDriverList()));

	m_pGraphBar->UpdateGraphs();
	m_pTaskInfo->Refresh();
	m_pSystemInfo->Refresh();
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
	// todo: run as ...
}

void CTaskExplorer::OnRunSys()
{
	// todo: run as trusted installer
}

void CTaskExplorer::OnElevate()
{
	// todo restart elevated
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

void CTaskExplorer::OnTaskTab()
{
	QAction* pAction = (QAction*)sender();
	int Index = m_Act2Tab.value(pAction);
	m_pTaskInfo->ShowTab(Index, pAction->isChecked());
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

		QString Version = QString::number(VERSION_MJR) + "." + QString::number(VERSION_MIN) //.rightJustified(2, '0')
#if VERSION_REV > 0
			+ "." + QString::number(VERSION_REV)
#endif
#if VERSION_UPD > 0
			+ QString::number('a' + VERSION_UPD - 1)
#endif
			;

		QString AboutCaption = tr(
			"<h3>About TaskExplorer</h3>"
			"<p>Version %1</p>"
			"<p>by DavidXanatos</p>"
			"<p>Copyright (c) 2019</p>"
		).arg(Version);
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
#ifdef WIN32
	else if (sender() == m_pMenuAboutPH)
		PhShowAbout(this);
#endif
	else //if (sender() == m_pMenuAboutQt)
		QMessageBox::aboutQt(this);
}