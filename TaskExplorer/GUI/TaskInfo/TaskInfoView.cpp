#include "stdafx.h"
#include "TaskInfoView.h"


CTaskInfoView::CTaskInfoView()
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pTabs = new QTabWidget();
	m_pMainLayout->addWidget(m_pTabs);

	
	m_pTaskOverview = new CTaskOverview();
	m_pTabs->addTab(m_pTaskOverview, tr("General"));

	m_pTaskPerfMon = new CTaskPerfMon();
	m_pTabs->addTab(m_pTaskPerfMon, tr("Perf Mon"));

	m_pHandlesView = new CHandlesView();
	m_pTabs->addTab(m_pHandlesView, tr("Handles"));

	m_pSocketsView = new CSocketsView();
	m_pTabs->addTab(m_pSocketsView, tr("Sockets"));

	m_pThreadsView = new CThreadsView();
	m_pTabs->addTab(m_pThreadsView, tr("Threads"));

	m_pModulesView = new CModulesView();
	m_pTabs->addTab(m_pModulesView, tr("Modules"));

	m_pMemoryView = new CMemoryView();
	m_pTabs->addTab(m_pMemoryView, tr("Memory"));

	m_pTokensView = new CTokensView();
	m_pTabs->addTab(m_pTokensView, tr("Tokens"));

	m_pWindowsView = new CWindowsView();
	m_pTabs->addTab(m_pWindowsView, tr("Windows"));

	m_pEnvironmentView = new CEnvironmentView();
	m_pTabs->addTab(m_pEnvironmentView, tr("Environment"));
}


CTaskInfoView::~CTaskInfoView()
{
}

void CTaskInfoView::ShowProcess(const CProcessPtr& pProcess)
{
	m_pSocketsView->ShowSockets(pProcess);
}
