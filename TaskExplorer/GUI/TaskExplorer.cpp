#include "stdafx.h"
#include "TaskExplorer.h"

CSystemAPI*	theAPI = NULL;

QIcon g_ExeIcon;

CTaskExplorer::CTaskExplorer(QWidget *parent)
	: QMainWindow(parent)
{
	//ui.setupUi(this);

	g_ExeIcon = QIcon(":/Icons/exe16.png");

	theAPI = CSystemAPI::New(this);

	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout(m_pMainWidget);
	m_pMainLayout->setMargin(0);
	this->setCentralWidget(m_pMainWidget);
	
	m_pGraphBar = new CGraphBar();
	m_pGraphBar->setMinimumHeight(60);
	m_pGraphBar->setMaximumHeight(120);
	m_pMainLayout->addWidget(m_pGraphBar);

	m_pMainSplitter = new QSplitter();
	m_pMainSplitter->setOrientation(Qt::Horizontal);
	m_pMainLayout->addWidget(m_pMainSplitter);

	m_pProcessTree = new CProcessTree();
	m_pProcessTree->setMinimumSize(200, 200);
	m_pMainSplitter->addWidget(m_pProcessTree);
	m_pMainSplitter->setCollapsible(0, false);

	m_pSubSplitter = new QSplitter();
	m_pSubSplitter->setOrientation(Qt::Vertical);
	m_pMainSplitter->addWidget(m_pSubSplitter);

	m_pSystemInfo = new CSystemInfoView();
	m_pSystemInfo->setMinimumSize(200, 200);
	m_pSubSplitter->addWidget(m_pSystemInfo);

	m_pTaskInfo = new CTaskInfoView();
	m_pTaskInfo->setMinimumSize(200, 200);
	m_pSubSplitter->addWidget(m_pTaskInfo);
	m_pSubSplitter->setCollapsible(1, false);

	connect(m_pProcessTree, SIGNAL(ProcessClicked(const CProcessPtr)), m_pTaskInfo, SLOT(ShowProcess(const CProcessPtr)));


	QMenu* m_pMenuProcess = menuBar()->addMenu(tr("&Process"));
		m_pMenuProcess->addAction(tr("Run..."), this, SLOT(OnRun()), QKeySequence("Ctrl+R"));
		m_pMenuProcess->addAction(tr("Run as Administrator..."), this, SLOT(OnRunAdmin()));
		m_pMenuProcess->addAction(tr("Run as limited User..."), this, SLOT(OnRunUser()));
		m_pMenuProcess->addAction(tr("Run as..."), this, SLOT(OnRunAs()));
		m_pMenuProcess->addAction(tr("Run as TrustedInstaller..."), this, SLOT(OnRunSys()));
		m_pMenuProcess->addSeparator();
		m_pMenuProcess->addAction(tr("Exit"), this, SLOT(OnExit()));

	QMenu* m_pMenuView = menuBar()->addMenu(tr("&View"));

	QMenu* m_pMenuOptions = menuBar()->addMenu(tr("&Options"));
		m_pMenuOptions->addAction(tr("Preferences"), this, SLOT(OnPrefs()));

	QMenu* m_pMenuTools = menuBar()->addMenu(tr("&Tools"));

	QMenu* m_pMenuHelp = menuBar()->addMenu(tr("&Help"));
		m_pMenuHelp->addAction(tr("About"), this, SLOT(OnAbout()));


	QLabel* m_pInfo = new QLabel(tr("test"));
	statusBar()->addPermanentWidget(m_pInfo);

	statusBar()->showMessage(tr("TaskExplorer is ready..."));

	//m_uTimerCounter = 0;
	m_uTimerID = startTimer(500);
}

CTaskExplorer::~CTaskExplorer()
{
	killTimer(m_uTimerID);

	delete theAPI;
	theAPI = NULL;
}

void CTaskExplorer::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() != m_uTimerID)
		return;

	QTimer::singleShot(0, theAPI, SLOT(UpdateProcessList()));
	QTimer::singleShot(0, theAPI, SLOT(UpdateSocketList()));
}

void CTaskExplorer::closeEvent(QCloseEvent *e)
{
	/*if (->IsEmbedded())
	{
		e->ignore();
		hide();
	}*/
}
