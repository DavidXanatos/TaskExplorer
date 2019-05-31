#include "stdafx.h"
#include "SystemInfoView.h"


CSystemInfoView::CSystemInfoView()
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pTabs = new QTabWidget();
	m_pMainLayout->addWidget(m_pTabs);

	m_pSystemOverview = new CSystemOverview();
	m_pTabs->addTab(m_pSystemOverview, tr("System"));

	m_pSystemPerfMon = new CSystemPerfMon();
	m_pTabs->addTab(m_pSystemPerfMon, tr("Perf Mon"));

	m_pDriversView = new CDriversView();
	m_pTabs->addTab(m_pDriversView, tr("Drivers"));

	m_pAllFilesView = new CAllFilesView();
	m_pTabs->addTab(m_pAllFilesView, tr("All Files"));

	m_pAllSocketsView = new CSocketsView(true);
	m_pTabs->addTab(m_pAllSocketsView, tr("All Sockets"));

	m_pServicesView = new CServicesView();
	m_pTabs->addTab(m_pServicesView, tr("Services"));

}


CSystemInfoView::~CSystemInfoView()
{
}
