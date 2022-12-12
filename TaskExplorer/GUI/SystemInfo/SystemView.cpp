#include "stdafx.h"
#include "SystemView.h"
#include "../TaskExplorer.h"
#include "../StatsView.h"
#ifdef WIN32
#include "./KernelInfo/PoolView.h"
#include "./KernelInfo/DriversView.h"
#include "./KernelInfo/NtObjectView.h"
#include "./KernelInfo/AtomView.h"
#include "./KernelInfo/RunObjView.h"
#endif

CSystemView::CSystemView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	m_pScrollArea = new QScrollArea();
	m_pMainLayout->addWidget(m_pScrollArea);

	m_pInfoWidget = new QWidget();
	m_pScrollArea->setFrameShape(QFrame::NoFrame);
	m_pScrollArea->setWidgetResizable(true);
	m_pScrollArea->setWidget(m_pInfoWidget);
	QPalette pal = m_pScrollArea->palette();
	pal.setColor(QPalette::Window, Qt::transparent);
	m_pScrollArea->setPalette(pal);

	m_pInfoLayout = new QVBoxLayout();
	m_pInfoWidget->setLayout(m_pInfoLayout);

	m_pSystemBox = new QGroupBox(tr("System"));
	m_pInfoLayout->addWidget(m_pSystemBox);

	m_pSystemLayout = new QGridLayout();
	m_pSystemLayout->setSpacing(2);
	m_pSystemBox->setLayout(m_pSystemLayout);

	int row = 0;

	m_pIcon = new QLabel();
	m_pSystemLayout->addWidget(m_pIcon, 0, 0, 4, 1);

	m_pSystemName = new QLabel();
	m_pSystemName->setSizePolicy(QSizePolicy::Expanding, m_pSystemName->sizePolicy().verticalPolicy());
	m_pSystemLayout->addWidget(m_pSystemName, row++, 1, 1, 2);
	
	m_pSystemLayout->addWidget(new QLabel(tr("Type:")), row, 1);
	m_pSystemType = new QLabel();
	m_pSystemLayout->addWidget(m_pSystemType, row++, 2);

	m_pSystemLayout->addWidget(new QLabel(tr("Version:")), row, 1);
	m_pSystemVersion = new QLabel();
	m_pSystemVersion->setSizePolicy(QSizePolicy::Expanding, m_pSystemVersion->sizePolicy().verticalPolicy());
	m_pSystemLayout->addWidget(m_pSystemVersion, row++, 2);

	m_pSystemLayout->addWidget(new QLabel(tr("Build:")), row, 1);
	m_pSystemBuild = new QLabel();
	m_pSystemLayout->addWidget(m_pSystemBuild, row++, 2);


	//m_pSystemLayout->addWidget(new QLabel(tr("Up time:")), row, 0);
	//m_pUpTime = new QLabel();
	//m_pSystemLayout->addWidget(m_pUpTime, row++, 1, 1, 2);

	//m_pSystemLayout->addWidget(new QLabel(tr("Host name:")), row, 0);
	//m_pHostName = new QLabel();
	//m_pSystemLayout->addWidget(m_pHostName, row++, 1, 1, 2);

	//m_pSystemLayout->addWidget(new QLabel(tr("User name:")), row, 0);
	//m_pUserName = new QLabel();
	//m_pSystemLayout->addWidget(m_pUserName, row++, 1, 1, 2);

	//m_pSystemLayout->addWidget(new QLabel(tr("System directory:")), row++, 0, 1, 3);
	//m_pSystemDir = new QLineEdit();
	//m_pSystemDir->setReadOnly(true);
	//m_pSystemLayout->addWidget(m_pSystemDir, row++, 0, 1, 3);

	//m_pSystemLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), row, 0);

	//m_pStatsView = new CStatsView(CStatsView::eSystem, this);
	//m_pStatsView->setSizePolicy(m_pStatsView->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
	//m_pInfoLayout->addWidget(m_pStatsView);

	/*QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pProcessLayout->addWidget(pSpacer, row, 1);*/
	//m_pInfoLayout->addWidget(pSpacer);

	m_pTabs = new QTabWidget();
	//m_pTabs->setDocumentMode(true);
	//m_pTabs->setTabPosition(QTabWidget::South);
	//m_pTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
	m_pInfoLayout->addWidget(m_pTabs);
	
	m_pStatsView = new CStatsView(CStatsView::eSystem, this);
	m_pTabs->addTab(m_pStatsView, tr("Statistics"));

#ifdef WIN32
	m_pPoolView = new CPoolView(this);
	m_pTabs->addTab(m_pPoolView, tr("Pool Table"));

	m_pDriversView = new CDriversView(this);
	m_pTabs->addTab(m_pDriversView, tr("Drivers"));

	m_pNtObjectView = new CNtObjectView(this);
	m_pTabs->addTab(m_pNtObjectView, tr("Nt Objects"));

	m_pAtomView = new CAtomView(this);
	m_pTabs->addTab(m_pAtomView, tr("Atom Table"));

	m_pRunObjView = new CRunObjView(this);
	m_pTabs->addTab(m_pRunObjView, tr("Running Objects"));
#endif

	m_pTabs->setCurrentIndex(theConf->GetValue(objectName() + "/SystemView_Tab").toInt());
}

CSystemView::~CSystemView()
{
	theConf->SetValue(objectName() + "/SystemView_Tab", m_pTabs->currentIndex());
}


void CSystemView::Refresh()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	if(m_pIcon->pixmap() == NULL)
#else
	if(m_pIcon->pixmap().isNull())
#endif
		m_pIcon->setPixmap(theAPI->GetSystemIcon().scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

	m_pSystemName->setText(theAPI->GetSystemName());
	m_pSystemType->setText(theAPI->GetSystemType());
	m_pSystemVersion->setText(theAPI->GetSystemVersion());
	m_pSystemBuild->setText(theAPI->GetSystemBuild());

	//m_pUpTime->setText(FormatTime(theAPI->GetUpTime()));
	//m_pHostName->setText(theAPI->GetHostName());
	//m_pUserName->setText(theAPI->GetUserName());
	//m_pSystemDir->setText(theAPI->GetSystemDir());

	m_pStatsView->ShowSystem();

#ifdef WIN32
	//if(m_pTabs->currentWidget() == m_pPoolView)
		m_pPoolView->Refresh(); // needed for the allocs on win 10
	//else 
    if(m_pTabs->currentWidget() == m_pDriversView)
		m_pDriversView->Refresh();
	else if(m_pTabs->currentWidget() == m_pNtObjectView)
		m_pNtObjectView->Refresh();
	else if(m_pTabs->currentWidget() == m_pAtomView)
		m_pAtomView->Refresh();
	else if(m_pTabs->currentWidget() == m_pRunObjView)
		m_pRunObjView->Refresh();
#endif
}
