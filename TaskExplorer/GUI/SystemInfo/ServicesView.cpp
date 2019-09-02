#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ServicesView.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinService.h"	
#include "../../API/Windows/ProcessHacker.h"	
#include "WinSvcWindow.h"
#include "../../API/Windows/WinProcess.h"
#endif
#include "../../Common/SortFilterProxyModel.h"
#include "../../Common/Finder.h"

CServicesView::CServicesView(bool bAll, QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pServiceModel = new CServiceModel();
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pServiceModel);
	m_pSortProxy->setDynamicSortFilter(true);


	// Service List
	m_pServiceList = new QTreeViewEx();
	m_pServiceList->setItemDelegate(theGUI->GetItemDelegate());

	m_pServiceList->setModel(m_pSortProxy);

	m_pServiceList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pServiceList->setSortingEnabled(true);

	m_pServiceList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pServiceList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(theGUI, SIGNAL(ReloadAll()), m_pServiceModel, SLOT(Clear()));

	m_pMainLayout->addWidget(m_pServiceList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

	if (!bAll)
	{
		//m_pServiceList->SetColumnHidden(CServiceModel::ePID, true, true);
#ifdef WIN32
		m_pServiceList->SetColumnHidden(CServiceModel::eGroupe, true, true);
#endif
		m_pServiceList->SetColumnHidden(CServiceModel::eFileName, true, true);
#ifdef WIN32
		m_pServiceList->SetColumnHidden(CServiceModel::eDescription, true, true);
		m_pServiceList->SetColumnHidden(CServiceModel::eCompanyName, true, true);
		m_pServiceList->SetColumnHidden(CServiceModel::eVersion, true, true);
#endif
		//m_pServiceList->setColumnHidden(CServiceModel::eBinaryPath, true, true);
	}

	//connect(m_pServiceList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	connect(m_pServiceList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));

	QByteArray Columns = theConf->GetBlob(objectName() + "/ServicesView_Columns");
	if (Columns.isEmpty())
	{
		for (int i = 0; i < m_pServiceModel->columnCount(); i++)
			m_pServiceList->SetColumnHidden(i, true);

		m_pServiceList->SetColumnHidden(CServiceModel::eService, false);
#ifdef WIN32
		m_pServiceList->SetColumnHidden(CServiceModel::eDisplayName, false);
		m_pServiceList->SetColumnHidden(CServiceModel::eType, false);
#endif
		m_pServiceList->SetColumnHidden(CServiceModel::eStatus, false);
#ifdef WIN32
		m_pServiceList->SetColumnHidden(CServiceModel::eStartType, false);
#endif
		if (bAll) {
			m_pServiceList->SetColumnHidden(CServiceModel::ePID, false);
			m_pServiceList->SetColumnHidden(CServiceModel::eBinaryPath, false);
		}
	}
	else
		m_pServiceList->restoreState(Columns);

	//m_pMenu = new QMenu();
	m_pMenuStart = m_pMenu->addAction(tr("Start"), this, SLOT(OnServiceAction()));
	m_pMenuContinue = m_pMenu->addAction(tr("Continue"), this, SLOT(OnServiceAction()));
	m_pMenuPause = m_pMenu->addAction(tr("Pause"), this, SLOT(OnServiceAction()));
	m_pMenuStop = m_pMenu->addAction(tr("Stop"), this, SLOT(OnServiceAction()));
	//m_MenuRestart = m_pMenu->addAction(tr("Restart"), this, SLOT(OnServiceAction()));
	m_pMenu->addSeparator();
	m_pMenuDelete = m_pMenu->addAction(tr("Delete"), this, SLOT(OnServiceAction()));
#ifdef WIN32
	m_pMenuOpenKey = m_pMenu->addAction(tr("Open key"), this, SLOT(OnServiceAction()));
	if (bAll)
	{
		m_pMenu->addSeparator();
		m_pMenuKernelServices = m_pMenu->addAction(tr("Show Kernel Services"));
		m_pMenuKernelServices->setCheckable(true);
		m_pMenuKernelServices->setChecked(theConf->GetValue(objectName() + "/ShowKernelServices", true).toBool());
	}
	else
		m_pMenuKernelServices = NULL;
#endif

	AddPanelItemsToMenu();

	if(bAll)
		connect(theAPI, SIGNAL(ServiceListUpdated(QSet<QString>, QSet<QString>, QSet<QString>)), this, SLOT(OnServiceListUpdated(QSet<QString>, QSet<QString>, QSet<QString>)));
}

CServicesView::~CServicesView()
{
	theConf->SetBlob(objectName() + "/ServicesView_Columns", m_pServiceList->saveState());
	if (m_pMenuKernelServices)
	{
		theConf->SetValue(objectName() + "/ShowKernelServices", m_pMenuKernelServices->isChecked());
	}
}

void CServicesView::OnServiceListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed)
{
	QMap<QString, CServicePtr> ServiceList = theAPI->GetServiceList();

#ifdef WIN32
	if (m_pMenuKernelServices)
	{
		m_pServiceModel->SetShowKernelServices(m_pMenuKernelServices->isChecked());
	}
	else
	{
		m_pServiceModel->SetShowKernelServices(false);
	}
#endif

	m_pServiceModel->Sync(ServiceList);
}

void CServicesView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
#ifdef WIN32
	QMap<QString, CServicePtr> AllServices = theAPI->GetServiceList();
	QMap<QString, CServicePtr> Services;

	foreach(const CProcessPtr& pProcess, Processes)
	{
		foreach(const QString& ServiceName, pProcess.objectCast<CWinProcess>()->GetServiceList())
		{
			CServicePtr pService = AllServices[ServiceName.toLower()];
			if (!pService)
				pService = CServicePtr(new CWinService(ServiceName));
			Services.insert(ServiceName.toLower(), pService);
		}
	}

	m_pServiceModel->Sync(Services);
#endif
}

void CServicesView::OnMenu(const QPoint &point)
{
	QModelIndexList selectedRows = m_pServiceList->selectedRows();

	int CanStart = 0;
	int CanStop = 0;
	int CanPause = 0;
	int CanContinue = 0;
	foreach(const QModelIndex& Index, selectedRows)
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
#ifdef WIN32
		QSharedPointer<CWinService> pService = m_pServiceModel->GetService(ModelIndex).objectCast<CWinService>();
		if (pService.isNull())
			continue;

		switch (pService->GetState())
        {
			case SERVICE_RUNNING:
				if (pService->GetControlsAccepted() & SERVICE_ACCEPT_PAUSE_CONTINUE)
					CanPause++;
				if (pService->GetControlsAccepted() & SERVICE_ACCEPT_STOP)
					CanStop++;
            break;
			case SERVICE_PAUSED:
				if (pService->GetControlsAccepted() & SERVICE_ACCEPT_PAUSE_CONTINUE)
					CanContinue++;
				if (pService->GetControlsAccepted() & SERVICE_ACCEPT_STOP)
					CanStop++;
            break;
			case SERVICE_STOPPED:
				CanStart++;
            break;
			case SERVICE_START_PENDING:
			case SERVICE_CONTINUE_PENDING:
			case SERVICE_PAUSE_PENDING:
			case SERVICE_STOP_PENDING:
            break;
        }
#endif
	}

	m_pMenuStart->setEnabled(CanStart > 0);
	m_pMenuContinue->setEnabled(CanContinue > 0);
	m_pMenuPause->setEnabled(CanPause > 0);
	m_pMenuStop->setEnabled(CanStop > 0);

	m_pMenuDelete->setEnabled(selectedRows.count() >= 1);
#ifdef WIN32
	m_pMenuOpenKey->setEnabled(selectedRows.count() == 1);
#endif

	CPanelView::OnMenu(point);
}

void CServicesView::OnServiceAction()
{
	QList<STATUS> Errors;
	int Force = -1;
	foreach(const QModelIndex& Index, m_pServiceList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CServicePtr pService = m_pServiceModel->GetService(ModelIndex);
		if (!pService.isNull())
		{
retry:
			STATUS Status = OK;
			if (sender() == m_pMenuStart)
				Status = pService->Start();
			else if (sender() == m_pMenuStop)
				Status = pService->Stop();
			else if (sender() == m_pMenuPause)
				Status = pService->Pause();
			else if (sender() == m_pMenuContinue)
				Status = pService->Continue();
			else if (sender() == m_pMenuDelete)
				Status = pService->Delete(Force == 1);
#ifdef WIN32
			else if (sender() == m_pMenuOpenKey)
			{
				PPH_STRING phRegKey = CastQString("HKLM\\System\\CurrentControlSet\\Services\\" + pService->GetName());
				PhShellOpenKey2(NULL, phRegKey);
				PhDereferenceObject(phRegKey);
			}
#endif

			if (Status.IsError())
			{
				if (Status.GetStatus() == ERROR_CONFIRM)
				{
					if (Force == -1)
					{
						switch (QMessageBox("TaskExplorer", Status.GetText(), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel | QMessageBox::Default | QMessageBox::Escape).exec())
						{
						case QMessageBox::Yes:
							Force = 1;
							goto retry;
							break;
						case QMessageBox::No:
							Force = 0;
							break;
						case QMessageBox::Cancel:
							return;
						}
					}
				}
				else
					Errors.append(Status);
			}
		}
	}

	CTaskExplorer::CheckErrors(Errors);
}


void CServicesView::OnDoubleClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
#ifdef WIN32
	QSharedPointer<CWinService> pService = m_pServiceModel->GetService(ModelIndex).objectCast<CWinService>();

	CWinSvcWindow* pWnd = new CWinSvcWindow(pService);
	connect(pWnd, SIGNAL(ServicesChanged()), theGUI, SLOT(OnReloadService()));
	pWnd->show();
#endif
}
