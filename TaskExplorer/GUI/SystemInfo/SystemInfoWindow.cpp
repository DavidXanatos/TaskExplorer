#include "stdafx.h"
#include "SystemInfoWindow.h"
#include "../TaskExplorer.h"


CSystemInfoWindow::CSystemInfoWindow(QWidget *parent) 
	: QMainWindow(parent)
{
	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout();
	m_pMainWidget->setLayout(m_pMainLayout);
	this->setCentralWidget(m_pMainWidget);

	CSystemInfoView* pSystemInfo = new CSystemInfoView();
	m_pMainLayout->addWidget(pSystemInfo);
	m_pSystemInfo = pSystemInfo;

	m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok /*| QDialogButtonBox::Cancel*/, Qt::Horizontal, this);
	QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(close()));
	//QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(close()));
	m_pMainLayout->addWidget(m_pButtonBox);

	this->setWindowTitle(tr("System Info"));

	restoreGeometry(theConf->GetBlob("SystemWindow/Window_Geometry"));

	m_uTimerID = startTimer(500);
}

CSystemInfoWindow::~CSystemInfoWindow()
{
	theConf->SetBlob("SystemWindow/Window_Geometry",saveGeometry());

	killTimer(m_uTimerID);
}

void CSystemInfoWindow::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

void CSystemInfoWindow::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() != m_uTimerID)
		return;

	QMetaObject::invokeMethod(m_pSystemInfo, "Refresh", Qt::AutoConnection);
}
