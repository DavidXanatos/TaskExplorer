#include "stdafx.h"
#include "TaskInfoWindow.h"


CTaskInfoWindow::CTaskInfoWindow(QWidget *parent) 
	: QMainWindow(parent)
{
	this->setWindowTitle(tr("Task Infos"));

	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout();
	m_pMainWidget->setLayout(m_pMainLayout);
	this->setCentralWidget(m_pMainWidget);


	m_pTaskInfo = new CTaskInfoView(true);
	m_pMainLayout->addWidget(m_pTaskInfo);

	m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok /*| QDialogButtonBox::Cancel*/, Qt::Horizontal, this);
	QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(close()));
	//QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(close()));
	m_pMainLayout->addWidget(m_pButtonBox);

	m_uTimerID = startTimer(500);
}


CTaskInfoWindow::~CTaskInfoWindow()
{
	killTimer(m_uTimerID);
}

void CTaskInfoWindow::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

void CTaskInfoWindow::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() == m_uTimerID)
		m_pTaskInfo->Refresh();
}

void CTaskInfoWindow::ShowProcess(const CProcessPtr& pProcess)
{
	this->setWindowTitle(tr("Task Infos of %1 (%2)").arg(pProcess->GetName()).arg(pProcess->GetParentId()));

	m_pTaskInfo->ShowProcess(pProcess);
}
