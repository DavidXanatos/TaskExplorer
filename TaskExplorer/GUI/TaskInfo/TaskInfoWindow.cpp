#include "stdafx.h"
#include "TaskInfoWindow.h"
#include "../TaskExplorer.h"


CTaskInfoWindow::CTaskInfoWindow(const QList<CProcessPtr>& Processes, quint64 ThreaId, QWidget *parent) 
	: QMainWindow(parent)
{
	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout();
	m_pMainWidget->setLayout(m_pMainLayout);
	this->setCentralWidget(m_pMainWidget);

	CTaskInfoView* pTaskInfo = new CTaskInfoView(true);
	m_pMainLayout->addWidget(pTaskInfo);
	m_pTaskInfo = pTaskInfo;

	m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok /*| QDialogButtonBox::Cancel*/, Qt::Horizontal, this);
	QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(close()));
	//QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(close()));
	m_pMainLayout->addWidget(m_pButtonBox);

	QStringList Names;
	foreach(const CProcessPtr& pProcess, Processes)
	{
		Names.append(tr("%1 (%2)").arg(pProcess->GetName()).arg(pProcess->GetProcessId()));
	}

	this->setWindowTitle(tr("Task Infos of %1").arg(Names.join(tr(", "))));

	pTaskInfo->ShowProcesses(Processes);
	if (ThreaId)
		pTaskInfo->SellectThread(ThreaId);

	restoreGeometry(theConf->GetBlob("InfoWindow/Window_Geometry"));

	m_uTimerID = startTimer(500);
}

CTaskInfoWindow::CTaskInfoWindow(QWidget* pSingleTab, const QString& TabName, QWidget *parent)
	: QMainWindow(parent)
{
	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout();
	m_pMainWidget->setLayout(m_pMainLayout);
	this->setCentralWidget(m_pMainWidget);

	QTabWidget* pTabWidget = new QTabWidget(); // this is just decoration ;)
	pTabWidget->addTab(pSingleTab, TabName);
	m_pMainLayout->addWidget(pTabWidget);
	m_pTaskInfo = pSingleTab;

	m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok /*| QDialogButtonBox::Cancel*/, Qt::Horizontal, this);
	QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(close()));
	//QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(close()));
	m_pMainLayout->addWidget(m_pButtonBox);

	restoreGeometry(theConf->GetBlob("InfoWindow/Window_Geometry"));

	m_uTimerID = startTimer(500);
}

CTaskInfoWindow::~CTaskInfoWindow()
{
	theConf->SetBlob("InfoWindow/Window_Geometry",saveGeometry());

	killTimer(m_uTimerID);
}

void CTaskInfoWindow::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

void CTaskInfoWindow::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() != m_uTimerID)
		return;

	QMetaObject::invokeMethod(m_pTaskInfo, "Refresh", Qt::AutoConnection);
}
