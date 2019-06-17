#include "stdafx.h"
#include "SystemInfoView.h"
#include "../TaskExplorer.h"


CSystemInfoView::CSystemInfoView(QWidget* patent) 
	: CTabPanel(patent)
{
	setObjectName("SystemPanel");

	InitializeTabs();

	int ActiveTab = theConf->GetValue(objectName() + "/Tabs_Active").toInt();
	QStringList VisibleTabs = theConf->GetValue(objectName() + "/Tabs_Visible").toStringList();
	RebuildTabs(ActiveTab, VisibleTabs);
}


CSystemInfoView::~CSystemInfoView()
{
	int ActiveTab = 0;
	QStringList VisibleTabs;
	SaveTabs(ActiveTab, VisibleTabs);
	theConf->SetValue(objectName() + "/Tabs_Active", ActiveTab);
	theConf->SetValue(objectName() + "/Tabs_Visible", VisibleTabs);
}

void CSystemInfoView::InitializeTabs()
{
	// todo
	m_pSystemView = new CSystemView(this);
	AddTab(m_pSystemView, tr("System"));

	m_pDriversView = new CDriversView(this);
	AddTab(m_pDriversView, tr("Drivers"));

	m_pAllFilesView = new CHandlesView(true, this);
	AddTab(m_pAllFilesView, tr("All Files"));

	m_pAllSocketsView = new CSocketsView(true, this);
	AddTab(m_pAllSocketsView, tr("All Sockets"));

	m_pServicesView = new CServicesView(this);
	AddTab(m_pServicesView, tr("Services"));

	connect(m_pTabs, SIGNAL(currentChanged(int)), this, SLOT(OnTab(int)));
}

void CSystemInfoView::OnTab(int tabIndex)
{
	Refresh();
}

void CSystemInfoView::Refresh()
{
	if (m_pTabs->currentWidget() == m_pAllFilesView)
		m_pAllFilesView->ShowAllFiles();
}
