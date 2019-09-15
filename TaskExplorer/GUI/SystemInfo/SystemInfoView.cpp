#include "stdafx.h"
#include "SystemInfoView.h"
#include "../TaskExplorer.h"

#include "SystemView.h"
//#include "DriversView.h"
#include "KernelInfo/KernelView.h"
#include "../TaskInfo/HandlesView.h"
#include "../TaskInfo/SocketsView.h"
#include "ServicesView.h"
#include "CPUView.h"
#include "RAMView.h"
#include "DiskView.h"
#include "NetworkView.h"
#include "GPUView.h"
#include "DnsCacheView.h"


CSystemInfoView::CSystemInfoView(QWidget* patent) 
	: CTabPanel(patent)
{
	setObjectName("SystemPanel");

	InitializeTabs();

	int ActiveTab = theConf->GetValue(objectName() + "/Tabs_Active").toInt();
	QStringList VisibleTabs = theConf->GetStringList(objectName() + "/Tabs_Visible");
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
	m_pSystemView = new CSystemView(this);
	AddTab(m_pSystemView, tr("System"));

	m_pCPUView = new CCPUView(this);
	AddTab(m_pCPUView, tr("CPU"));

	m_pRAMView = new CRAMView(this);
	AddTab(m_pRAMView, tr("Memory"));

	m_pDiskView = new CDiskView(this);
	AddTab(m_pDiskView, tr("Disk/IO"));

	m_pAllFilesView = new CHandlesView(true, this);
	AddTab(m_pAllFilesView, tr("Open Files"));

	m_pNetworkView = new CNetworkView(this);
	AddTab(m_pNetworkView, tr("Network"));

	m_pAllSocketsView = new CSocketsView(true, this);
	AddTab(m_pAllSocketsView, tr("Open Sockets"));

	m_pDnsCacheView = new CDnsCacheView(true, this);
	AddTab(m_pDnsCacheView, tr("Dns Cache"));

	m_pGPUView = new CGPUView(this);
	AddTab(m_pGPUView, tr("GPU"));

	//m_pDriversView = new CDriversView(this);
	//AddTab(m_pDriversView, tr("Drivers"));
	
	m_pKernelView = new CKernelView(this);
	AddTab(m_pKernelView, tr("Kernel Objects"));

	m_pServicesView = new CServicesView(true, this);
	AddTab(m_pServicesView, tr("Services"));


	connect(m_pTabs, SIGNAL(currentChanged(int)), this, SLOT(OnTab(int)));
}

void CSystemInfoView::OnTab(int tabIndex)
{
	Refresh();
}

void CSystemInfoView::Refresh()
{
	QTimer::singleShot(0, m_pTabs->currentWidget(), SLOT(Refresh()));
}

void CSystemInfoView::UpdateGraphs()
{
	// todo: dont update hidden tabs
	m_pCPUView->UpdateGraphs();
	m_pRAMView->UpdateGraphs();
	m_pDiskView->UpdateGraphs();
	m_pNetworkView->UpdateGraphs();
	m_pGPUView->UpdateGraphs();
}
