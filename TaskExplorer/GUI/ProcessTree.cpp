#include "stdafx.h"
#include "TaskExplorer.h"
#include "ProcessTree.h"
#include "../Common/Common.h"
#include "Models/ProcessModel.h"
#include "../Common/SortFilterProxyModel.h"
#ifdef WIN32
#include "../API/Windows/WinProcess.h"
#include "../API/Windows/WinModule.h"
#include "../API/Windows/WindowsAPI.h"
#include "../API/Windows/ProcessHacker.h"
#include "../API/Windows/ProcessHacker/appsup.h"
#include "WsWatchDialog.h"
#include "WaitChainDialog.h"
#endif
#include "../API/MemDumper.h"
#include "../Common/ProgressDialog.h"
#include "TaskInfo/TaskInfoWindow.h"
#include "RunAsDialog.h"
#include "../Common/Finder.h"

CProcessTree::CProcessTree(QWidget *parent)
	: CTaskView(parent)
{
	m_ExpandAll = false;

	m_bQuickRefreshPending = false;

	this->ForceColumn(CProcessModel::eProcess);

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pProcessModel = new CProcessModel();
	//connect(m_pProcessModel, SIGNAL(CheckChanged(quint64, bool)), this, SLOT(OnCheckChanged(quint64, bool)));
	//connect(m_pProcessModel, SIGNAL(Updated()), this, SLOT(OnUpdated()));

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pProcessModel);
	m_pSortProxy->setDynamicSortFilter(true);


	m_pProcessList = new CSplitTreeView(m_pSortProxy);
	
	connect(m_pProcessList, SIGNAL(MenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(m_pProcessList, SIGNAL(TreeEnabled(bool)), this, SLOT(OnTreeEnabled(bool)));

	m_pProcessList->GetView()->header()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pProcessList->GetView()->header(), SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnHeaderMenu(const QPoint &)));

	m_pHeaderMenu = new QMenu(this);

	m_pProcessList->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	m_pProcessList->GetTree()->setItemDelegate(theGUI->GetItemDelegate());

	connect(m_pProcessModel, SIGNAL(ToolTipCallback(const QVariant&, QString&)), this, SLOT(OnToolTipCallback(const QVariant&, QString&)), Qt::DirectConnection);

	connect(theGUI, SIGNAL(ReloadPanels()), m_pProcessModel, SLOT(Clear()));

	m_pMainLayout->addWidget(m_pProcessList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

#ifndef _DEBUG
	connect(m_pProcessList->GetView()->verticalScrollBar(), SIGNAL(valueChanged(int)), this, SLOT(OnUpdateHistory()));
	connect(m_pProcessList->GetView(), SIGNAL(expanded(const QModelIndex &)), this, SLOT(OnUpdateHistory()));
#endif


	//m_pMenu = new QMenu();
	m_pShowProperties = m_pMenu->addAction(tr("Properties"), this, SLOT(OnShowProperties()));
	m_pOpenPath = m_pMenu->addAction(tr("Open Path"), this, SLOT(OnProcessAction()));
	m_pBringInFront = m_pMenu->addAction(tr("Bring in front"), this, SLOT(OnProcessAction()));
	m_pMenu->addSeparator();

	m_pClose = m_pMenu->addAction(tr("Close"), this, SLOT(OnProcessAction()));

	AddTaskItemsToMenu();
	//QAction*				m_pTerminateTree;
	
	AddPriorityItemsToMenu(eProcess, m_pMenu);


	m_pMenu->addSeparator();
	m_pMiscMenu = m_pMenu->addMenu(tr("Miscellaneous"));
	m_pRunAsThis = m_pMiscMenu->addAction(tr("Run as this User"), this, SLOT(OnRunAsThis()));
	m_pQuit = m_pMiscMenu->addAction(tr("Quit (WM_QUIT)"), this, SLOT(OnProcessAction()));
	m_pCreateDump = m_pMiscMenu->addAction(tr("Create Crash Dump"), this, SLOT(OnCrashDump()));
	m_pDebug = m_pMiscMenu->addAction(tr("Debug"), this, SLOT(OnProcessAction()));
	m_pDebug->setCheckable(true);
#ifdef WIN32
	m_pReduceWS = m_pMiscMenu->addAction(tr("Reduce Working Set"), this, SLOT(OnProcessAction()));
	m_pWatchWS = m_pMiscMenu->addAction(tr("Working Set Watch"), this, SLOT(OnWsWatch()));
	m_pWCT = m_pMiscMenu->addAction(tr("Wait Chain Traversal"), this, SLOT(OnWCT()));
	//m_pVirtualization = m_pMiscMenu->addAction(tr("Virtualization"), this, SLOT(OnProcessAction()));
	//m_pVirtualization->setCheckable(true);
	m_pCritical = m_pMiscMenu->addAction(tr("Critical Process Flag"), this, SLOT(OnProcessAction()));
	m_pCritical->setCheckable(true);

	m_pPermissions = m_pMenu->addAction(tr("Permissions"), this, SLOT(OnPermissions()));
#endif

	AddPanelItemsToMenu(m_pMenu);



	//connect(m_pProcessList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	//connect(m_pProcessList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));
	connect(m_pProcessList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnShowProperties()));
	connect(m_pProcessList, SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));
	connect(m_pProcessList, SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(OnSelectionChanged(QItemSelection,QItemSelection)));

	connect(theAPI, SIGNAL(ProcessListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), SLOT(OnProcessListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

	QByteArray Columns = theConf->GetBlob("MainWindow/ProcessTree_Columns");
	if (Columns.isEmpty())
		OnResetColumns();
	else
	{
		m_pProcessList->restoreState(Columns);

		m_pProcessModel->SetColumnEnabled(0, true);
		for (int i = 1; i < m_pProcessModel->columnCount(); i++)
			m_pProcessModel->SetColumnEnabled(i, !m_pProcessList->GetView()->isColumnHidden(i));
	}

	//connect(m_pProcessList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	OnColumnsChanged();
}

CProcessTree::~CProcessTree()
{
	theConf->SetBlob("MainWindow/ProcessTree_Columns", m_pProcessList->saveState());
}

void CProcessTree::OnResetColumns()
{
	for (int i = 1; i < m_pProcessModel->columnCount(); i++)
		m_pProcessList->GetView()->setColumnHidden(i, true);

	m_pProcessList->GetView()->setColumnHidden(CProcessModel::ePID, false);
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eCPU, false);
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eIO_TotalRate, false);
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eStaus, false);
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eWorkingSet, false); // thats what best represents the InMemory value
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eStartTime, false); // less refreshes as uptime 
#ifdef WIN32
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eServices, false);
#endif
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::ePriorityClass, false);
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eHandles, false);
#ifdef WIN32
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eWND_Handles, false);
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eGDI_Handles, false);
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eUSER_Handles, false);
#endif
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eThreads, false);
	//m_pProcessList->GetView()->setColumnHidden(CProcessModel::eCommandLine, false);
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eNet_TotalRate, false);
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eDisk_TotalRate, false);
	m_pProcessList->GetView()->setColumnHidden(CProcessModel::eNetUsage, false);

	m_pProcessModel->SetColumnEnabled(0, true);
	for (int i = 1; i < m_pProcessModel->columnCount(); i++)
		m_pProcessModel->SetColumnEnabled(i, !m_pProcessList->GetView()->isColumnHidden(i));
}

void CProcessTree::OnColumnsChanged()
{
	// automatically set the setting based on wether the columns are checked or not

	/*theConf->SetValue("Options/GPUStatsGetPerProcess", 
		m_pProcessModel->IsColumnEnabled(CProcessModel::eGPU_History)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eVMEM_History)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eGPU_Usage)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eGPU_Shared)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eGPU_Dedicated)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eGPU_Adapter)
	);*/

	theConf->SetValue("Options/MonitorTokenChange", 
		m_pProcessModel->IsColumnEnabled(CProcessModel::eElevation)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eVirtualized)
	 || m_pProcessModel->IsColumnEnabled(CProcessModel::eIntegrity)
	);

	QuickRefresh();
}

void CProcessTree::QuickRefresh()
{
	if (m_bQuickRefreshPending)
		return;
	m_bQuickRefreshPending = true;
	QTimer::singleShot(250, this, SLOT(OnQuickRefresh()));
}

void CProcessTree::OnQuickRefresh()
{
	m_bQuickRefreshPending = false;

	OnProcessListUpdated(QSet<quint64>(), QSet<quint64>(), QSet<quint64>());
}

void CProcessTree::OnTreeEnabled(bool bEnable)
{
	if (m_pProcessModel->IsTree() != bEnable)
	{
		m_pProcessModel->SetTree(bEnable);

		if (bEnable)
			m_ExpandAll = true;
	}
}

void CProcessTree::AddHeaderSubMenu(QMenu* m_pHeaderMenu, const QString& Label, int from, int to)
{
	QMenu* pSubMenu = m_pHeaderMenu->addMenu(Label);

	for(int i = from; i <= to; i++)
	{
		QCheckBox *checkBox = new QCheckBox(m_pProcessModel->GetColumHeader(i), pSubMenu);
		connect(checkBox, SIGNAL(stateChanged(int)), this, SLOT(OnHeaderMenu()));
		QWidgetAction *pAction = new QWidgetAction(pSubMenu);
		pAction->setDefaultWidget(checkBox);
		pSubMenu->addAction(pAction);

		m_Columns[checkBox] = i;
	}
}

void CProcessTree::OnHeaderMenu(const QPoint &point)
{
	if(m_Columns.isEmpty())
	{
		//for(int i = 1; i < m_pProcessModel->MaxColumns(); i++)
		for(int i = CProcessModel::ePID; i <= CProcessModel::eCommandLine; i++)
		{
			QCheckBox *checkBox = new QCheckBox(m_pProcessModel->GetColumHeader(i), m_pHeaderMenu);
			connect(checkBox, SIGNAL(stateChanged(int)), this, SLOT(OnHeaderMenu()));
			QWidgetAction *pAction = new QWidgetAction(m_pHeaderMenu);
			pAction->setDefaultWidget(checkBox);
			m_pHeaderMenu->addAction(pAction);

			m_Columns[checkBox] = i;
		}

		m_pHeaderMenu->addSeparator();
		AddHeaderSubMenu(m_pHeaderMenu, tr("Graphs"), CProcessModel::eCPU_History, CProcessModel::eVMEM_History);
		m_pHeaderMenu->addSeparator();
		AddHeaderSubMenu(m_pHeaderMenu, tr("CPU"), CProcessModel::eCPU, CProcessModel::eCyclesDelta);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Memory"), CProcessModel::ePrivateBytes, CProcessModel::ePrivateBytesDelta);
		AddHeaderSubMenu(m_pHeaderMenu, tr("GPU"), CProcessModel::eGPU_Usage, CProcessModel::eGPU_Adapter);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Priority"), CProcessModel::ePriorityClass, CProcessModel::eIO_Priority);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Objects"), CProcessModel::eHandles, CProcessModel::ePeakThreads);
		AddHeaderSubMenu(m_pHeaderMenu, tr("File Info"), CProcessModel::eFileName, CProcessModel::eFileSize);
#ifdef WIN32
		AddHeaderSubMenu(m_pHeaderMenu, tr("Protection"), CProcessModel::eIntegrity, CProcessModel::eCritical);
#endif
		AddHeaderSubMenu(m_pHeaderMenu, tr("Other"), CProcessModel::eSubsystem, CProcessModel::eSessionID);
		AddHeaderSubMenu(m_pHeaderMenu, tr("File I/O"), CProcessModel::eIO_TotalRate, CProcessModel::eIO_OtherRate);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Network I/O"), CProcessModel::eNet_TotalRate, CProcessModel::eSendRate);
		AddHeaderSubMenu(m_pHeaderMenu, tr("Disk I/O"), CProcessModel::eDisk_TotalRate, CProcessModel::eWriteRate);

		m_pHeaderMenu->addSeparator();
		QAction* pAction = m_pHeaderMenu->addAction(tr("Reset columns"));
		connect(pAction, SIGNAL(triggered()), this, SLOT(OnResetColumns()));
	}

	for(QMap<QCheckBox*, int>::iterator I = m_Columns.begin(); I != m_Columns.end(); I++)
		I.key()->setChecked(m_pProcessModel->IsColumnEnabled(I.value()));

	m_pHeaderMenu->popup(QCursor::pos());	
}

void CProcessTree::OnHeaderMenu()
{
	QCheckBox *checkBox = (QCheckBox*)sender();
	int Column = m_Columns.value(checkBox, -1);

	m_pProcessList->GetView()->setColumnHidden(Column, !checkBox->isChecked());
	m_pProcessModel->SetColumnEnabled(Column, checkBox->isChecked());
	OnColumnsChanged();
}

void CProcessTree::OnProcessListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	if (!theGUI->isVisible() || theGUI->windowState().testFlag(Qt::WindowMinimized))
		return;

	m_Processes = theAPI->GetProcessList();

	Added = m_pProcessModel->Sync(m_Processes);

	// If we are dsplaying a tree than always auto expand new items
	if (m_pProcessModel->IsTree())
	{
		QTimer::singleShot(100, this, [this, Added]() {
			if (m_ExpandAll)
			{
				m_ExpandAll = false;
				m_pProcessList->expandAll();
			}
			else
			{
				foreach(quint64 PID, Added) {
					m_pProcessList->expand(m_pSortProxy->mapFromSource(m_pProcessModel->FindIndex(PID)));
				}
			}
		});
	}

	OnUpdateHistory();
}

void CProcessTree::OnExpandAll()
{
	m_pProcessList->expandAll();
}

void CProcessTree::OnShowProperties()
{
	CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow(GetSellectedProcesses<CProcessPtr>());
	pTaskInfoWindow->show();
}

void CProcessTree::OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);

	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	emit ProcessClicked(pProcess);
}

void CProcessTree::OnSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected)
{
	emit ProcessesSelected(GetSellectedProcesses<CProcessPtr>());
}

QList<CTaskPtr> CProcessTree::GetSellectedTasks() 
{
	return GetSellectedProcesses<CTaskPtr>(); 
}

void CProcessTree::OnToolTipCallback(const QVariant& ID, QString& ToolTip)
{
	CProcessPtr pProcess = theAPI->GetProcessByID(ID.toULongLong());
	if (pProcess.isNull())
		return;

	QStringList InfoLines;

	QString CommandLine = pProcess->GetCommandLineStr();
	// This is necessary because the tooltip control seems to use some kind of O(n^9999) word-wrapping algorithm.
	for (int i = 100; i < CommandLine.length(); i += 101)
		CommandLine.insert(i, "\n");
	InfoLines.append(CommandLine);

	InfoLines.append(tr("File:"));
	InfoLines.append(tr("    %1").arg(pProcess->GetFileName()));

#ifdef WIN32
	QSharedPointer<CWinProcess> pWinProcess = pProcess.staticCast<CWinProcess>();
	QSharedPointer<CWinModule> pWinModule = pProcess->GetModuleInfo().staticCast<CWinModule>();

	if (pWinModule)
	{
		InfoLines.append(tr("    %1 %2").arg(pWinModule->GetFileInfo("Description")).arg(pWinModule->GetFileInfo("FileVersion")));
		InfoLines.append(tr("    %1").arg(pWinModule->GetFileInfo("CompanyName")));
	}

	PH_KNOWN_PROCESS_TYPE KnownProcessType = PhGetProcessKnownTypeEx(pProcess->GetProcessId(), pProcess->GetFileName());

	// Known command line information
    PH_KNOWN_PROCESS_COMMAND_LINE knownCommandLine;
	if (KnownProcessType != UnknownProcessType)
	{
		PH_AUTO_POOL autoPool;
		PhInitializeAutoPool(&autoPool);

		PPH_STRING commandLine = (PPH_STRING)PH_AUTO(CastQString(pProcess->GetCommandLineStr()));

		if (PhaGetProcessKnownCommandLine(commandLine, KnownProcessType, &knownCommandLine))
		{
			switch (KnownProcessType & KnownProcessTypeMask)
			{
				case ServiceHostProcessType:
				{
					InfoLines.append(tr("Service group name:"));
					InfoLines.append(tr("    %1").arg(CastPhString(knownCommandLine.ServiceHost.GroupName, false)));
					break;
				}
				case RunDllAsAppProcessType:
				{
					PH_IMAGE_VERSION_INFO versionInfo;
					if (PhInitializeImageVersionInfo(&versionInfo, knownCommandLine.RunDllAsApp.FileName->Buffer))
					{
						InfoLines.append(tr("Run DLL target file:"));
						InfoLines.append(tr("    %1 %2").arg(CastPhString(versionInfo.FileDescription, false)).arg(CastPhString(versionInfo.FileVersion, false)));
						InfoLines.append(tr("    %1").arg(CastPhString(versionInfo.CompanyName, false)));

						PhDeleteImageVersionInfo(&versionInfo);
					}
					break;
				}
				case ComSurrogateProcessType:
				{
					InfoLines.append(tr("COM target:"));

					if (knownCommandLine.ComSurrogate.Name)
						InfoLines.append(tr("    %1").arg(CastPhString(knownCommandLine.ComSurrogate.Name, false)));

					PPH_STRING guidString = PhFormatGuid(&knownCommandLine.ComSurrogate.Guid);
					if (guidString)
						InfoLines.append(tr("    %1").arg(CastPhString(guidString)));

					PH_IMAGE_VERSION_INFO versionInfo;
					if (knownCommandLine.ComSurrogate.FileName && PhInitializeImageVersionInfo(&versionInfo, knownCommandLine.ComSurrogate.FileName->Buffer))
					{
						InfoLines.append(tr("COM target file:"));
						InfoLines.append(tr("    %1 %2").arg(CastPhString(versionInfo.FileDescription, false)).arg(CastPhString(versionInfo.FileVersion, false)));
						InfoLines.append(tr("    %1").arg(CastPhString(versionInfo.CompanyName, false)));
					}
					break;
				}
			}
		}

		PhDeleteAutoPool(&autoPool);
	}

	// Services
	QStringList ServiceList = pWinProcess->GetServiceList();
    if (!ServiceList.isEmpty())
    {
		InfoLines.append(tr("Services:"));

		foreach(const QString& Service, ServiceList)
		{
			CServicePtr pService = theAPI->GetService(Service);
			if(pService)
				InfoLines.append(tr("    %1 (%2)").arg(Service).arg(pService->GetDisplayName()));
			else
				InfoLines.append(tr("    %1").arg(Service));
		}
    }

	// Tasks, Drivers
    switch (KnownProcessType & KnownProcessTypeMask)
    {
		case TaskHostProcessType:
        {
			QList<CWinProcess::STask> Tasks = pWinProcess->GetTasks();
			if (!Tasks.isEmpty())
			{
				InfoLines.append(tr("Tasks:"));
				foreach(const CWinProcess::STask& Task, Tasks)
					InfoLines.append(tr("    %1 (%2)").arg(Task.Name).arg(Task.Path));
			}
        }
        break;
		case UmdfHostProcessType:
        {
			QList<CWinProcess::SDriver> Drivers = pWinProcess->GetUmdfDrivers();
			if (!Drivers.isEmpty())
			{
				InfoLines.append(tr("Drivers:"));
				foreach(const CWinProcess::SDriver& Driver, Drivers)
					InfoLines.append(tr("    %1 (%2)").arg(Driver.Name).arg(Driver.Path));
			}
        }
        break;
		case EdgeProcessType:
        {
			CWinTokenPtr pToken = pWinProcess->GetToken();
			if (!pToken)
				break;
			
			QString SidString = pToken->GetSidString();

			QString EdgeInfo;
			if (SidString.compare("S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194", Qt::CaseInsensitive))
				EdgeInfo = tr("Microsoft Edge Manager");
			else if (SidString.compare("S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-1206159417-1570029349-2913729690-1184509225", Qt::CaseInsensitive))
				EdgeInfo = tr("Browser Extensions");
			else if (SidString.compare("S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-3513710562-3729412521-1863153555-1462103995", Qt::CaseInsensitive))
				EdgeInfo = tr("User Interface Service");
			else if (SidString.compare("S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-1821068571-1793888307-623627345-1529106238", Qt::CaseInsensitive))
				EdgeInfo = tr("Chakra Jit Compiler");
			else if (SidString.compare("S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-3859068477-1314311106-1651661491-1685393560", Qt::CaseInsensitive))
				EdgeInfo = tr("Adobe Flash Player");
			else if (SidString.compare("S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-4256926629-1688279915-2739229046-3928706915", Qt::CaseInsensitive) 
				  || SidString.compare("S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-2385269614-3243675-834220592-3047885450", Qt::CaseInsensitive)
				  || SidString.compare("S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-355265979-2879959831-980936148-1241729999", Qt::CaseInsensitive))
				EdgeInfo = tr("Background Tab Pool");

			if (!EdgeInfo.isEmpty())
			{
				InfoLines.append(tr("Edge:"));
				InfoLines.append(tr("    %1").arg(EdgeInfo));
			}
        }
        break;
		case WmiProviderHostType:
        {
			QList<CWinProcess::SWmiProvider> Providers = pWinProcess->QueryWmiProviders();
			if (!Providers.isEmpty())
			{
				InfoLines.append(tr("WMI Providers:"));
				foreach(const CWinProcess::SWmiProvider& Provider, Providers)
					InfoLines.append(tr("    %1 (%2)").arg(Provider.ProviderName).arg(Provider.FileName));
			}
        }      
        break;
    }

    // Notes
    {
		QStringList Notes;

		if (pWinModule)
		{
			switch (pWinModule->GetVerifyResult())
			{
			case VrTrusted:
			{
				QString Signer = pWinModule->GetVerifySignerName();
				if (Signer.isEmpty())
					Notes.append(tr("    Signer: %1").arg(Signer));
				else
					Notes.append(tr("    Signed"));
				break;
			}
			case VrNoSignature:
				Notes.append(tr("    Signature invalid"));
				break;
			}

			if (pWinModule->IsPacked())
			{
				Notes.append(tr("    Image is probably packed (%1 imports over %2 modules).").arg(pWinModule->GetImportFunctions()).arg(pWinModule->GetImportModules()));
			}
		}

		quint64 ConsoleHostProcessId = pWinProcess->GetConsoleHostId();
        if (ConsoleHostProcessId & ~3)
        {
            QString Description = tr("Console host");
            if (ConsoleHostProcessId & 2)
                Description = tr("Console application");
			quint64 PID = (ConsoleHostProcessId & ~3);
			CProcessPtr pHostProcess = theAPI->GetProcessByID(PID);
			Notes.append(tr("    %1: %2 (%3)").arg(Description).arg(pHostProcess ? pHostProcess->GetName() : tr("Non-existent process")).arg(PID));
        }

		QString PackageFullName = pWinProcess->GetPackageName();
        if (!PackageFullName.isEmpty())
			Notes.append(tr("    Package name: %1").arg(PackageFullName));

        if (pWinProcess->IsNetProcess())
            Notes.append(tr("    Process is managed (.NET)."));
        if (pWinProcess->IsElevated())
            Notes.append(tr("    Process is elevated."));
        if (pWinProcess->IsImmersiveProcess())
            Notes.append(tr("    Process is a Modern UI app."));
        if (pWinProcess->IsInJob())
            Notes.append(tr("    Process is in a job."));
        if (pWinProcess->IsWoW64())
            Notes.append(tr("    Process is 32-bit (WOW64)."));

        if (!Notes.isEmpty())
        {
			InfoLines.append(tr("Notes:"));
			InfoLines.append(Notes);
        }
    }
#endif

	ToolTip = InfoLines.join("\n");
}

void CProcessTree::OnMenu(const QPoint &point)
{
	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	
	QModelIndexList selectedRows = m_pProcessList->selectedRows();

	QList<CWndPtr> Windows;
	if (selectedRows.count() >= 1)
		Windows = pProcess->GetWindows();

	m_pShowProperties->setEnabled(selectedRows.count() > 0);
	m_pBringInFront->setEnabled(selectedRows.count() == 1 && !Windows.isEmpty());
	m_pClose->setEnabled(!Windows.isEmpty());

	m_pQuit->setEnabled(!Windows.isEmpty());
	m_pRunAsThis->setEnabled(selectedRows.count() == 1);
	m_pCreateDump->setEnabled(selectedRows.count() == 1);
	m_pDebug->setEnabled(selectedRows.count() == 1);
	m_pDebug->setChecked(pProcess && pProcess->HasDebugger());

#ifdef WIN32
	QSharedPointer<CWinProcess> pWinProcess = pProcess.staticCast<CWinProcess>();

	//CWinTokenPtr pToken = pWinProcess ? pWinProcess->GetToken() : NULL;
	//m_pVirtualization->setEnabled(pToken && pToken->IsVirtualizationAllowed());
	//m_pVirtualization->setChecked(pToken && pToken->IsVirtualizationEnabled());

	m_pCritical->setEnabled(selectedRows.count() > 0);
	m_pCritical->setChecked(pWinProcess && pWinProcess->IsCriticalProcess());

	m_pReduceWS->setEnabled(selectedRows.count() > 0);
	m_pWatchWS->setEnabled(selectedRows.count() == 1);
	m_pWCT->setEnabled(selectedRows.count() == 1);

	m_pPermissions->setEnabled(selectedRows.count() == 1);
#endif

	CTaskView::OnMenu(point);
}

void CProcessTree::OnCrashDump()
{
	QString DumpPath = QFileDialog::getSaveFileName(this, tr("Create dump"), "", tr("Dump files (*.dmp);;All files (*.*)"));
	if (DumpPath.isEmpty())
		return;

	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);

	CMemDumper* pDumper = CMemDumper::New();
	STATUS status = pDumper->PrepareDump(pProcess, DumpPath);
	if(status.IsError())
		QMessageBox::warning(this, "TaskExplorer", tr("Failed to create dump file, reason: %1").arg(status.GetText()));
	else
	{
		CProgressDialog* pDialog = new CProgressDialog(tr("Dumping %1").arg(pProcess->GetName()), this);
		pDialog->show();

		connect(pDumper, SIGNAL(ProgressMessage(const QString&, int)), pDialog, SLOT(OnProgressMessage(const QString&, int)));
		connect(pDumper, SIGNAL(StatusMessage(const QString&, int)), pDialog, SLOT(OnStatusMessage(const QString&, int)));
		connect(pDialog, SIGNAL(Cancel()), pDumper, SLOT(Cancel()));
		connect(pDumper, SIGNAL(finished()), pDialog, SLOT(OnFinished()));

		connect(pDumper, SIGNAL(finished()), pDumper, SLOT(deleteLater()));

		pDumper->start();
	}
}

void CProcessTree::OnProcessAction()
{
#ifdef WIN32
	QList<STATUS> Errors;
	int Force = -1;
	foreach(const QModelIndex& Index, m_pProcessList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
		QSharedPointer<CWinProcess> pWinProcess = pProcess.staticCast<CWinProcess>();
		if (!pWinProcess.isNull())
		{
		retry:
			STATUS Status = OK;
			/*if (sender() == m_pVirtualization)
			{
				CWinTokenPtr pToken = pWinProcess->GetToken();
				if (pToken)
					Status = pToken->SetVirtualizationEnabled(m_pVirtualization->isChecked());
				else
					Status = ERR(tr("This process does not have a token."), -1);
			}
			else*/ if (sender() == m_pCritical)
				Status = pWinProcess->SetCriticalProcess(m_pCritical->isChecked(), Force == 1);
			else if (sender() == m_pReduceWS)
				Status = pWinProcess->ReduceWS();
			else if (sender() == m_pDebug)
			{
				if (m_pDebug->isChecked())
					pWinProcess->AttachDebugger();
				else
					pWinProcess->DetachDebugger();
			}
			else if (sender() == m_pClose || sender() == m_pQuit)
			{
				QList<CWndPtr> Windows = pProcess->GetWindows();
				foreach(const CWndPtr& pWnd, Windows)
				{
					if (QSharedPointer<CWinWnd> pWinWnd = pWnd.staticCast<CWinWnd>())
						pWinWnd->PostWndMessage(sender() == m_pQuit ? WM_QUIT : WM_CLOSE);
				}
			}
			else if (sender() == m_pBringInFront)
			{
				CWndPtr pWnd = pProcess->GetMainWindow();
				if (!pWnd.isNull())
					pWnd->BringToFront();
			}
			else if (sender() == m_pOpenPath)
			{
				PPH_STRING phFileName = CastQString(pWinProcess->GetFileName());
				PhShellExecuteUserString(NULL, L"FileBrowseExecutable", phFileName->Buffer, FALSE, L"Make sure the Explorer executable file is present." );
				PhDereferenceObject(phFileName);
			}
			

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
#endif
}

void CProcessTree::OnWsWatch()
{
#ifdef _WIN32
	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	if (pProcess)
	{
		CWsWatchDialog* pWnd = new CWsWatchDialog(pProcess);
		pWnd->show();
	}
#endif
}

void CProcessTree::OnWCT()
{
#ifdef _WIN32
	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	if (pProcess)
	{
		CWaitChainDialog* pWnd = new CWaitChainDialog(pProcess);
		pWnd->show();
	}
#endif
}

void CProcessTree::OnRunAsThis()
{
	QModelIndex Index = m_pProcessList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	if (pProcess)
	{
		CRunAsDialog* pWnd = new CRunAsDialog(pProcess->GetProcessId());
		pWnd->show();
	}
}

void CProcessTree::OnPermissions()
{
#ifdef WIN32
	QList<CTaskPtr>	Tasks = GetSellectedTasks();
	if (Tasks.count() != 1)
		return;

	if (QSharedPointer<CWinProcess> pWinProcess = Tasks.first().staticCast<CWinProcess>())
	{
		pWinProcess->OpenPermissions();
	}
#endif
}


void CProcessTree::UpdateIndexWidget(int HistoryColumn, int CellHeight, QMap<quint64, CHistoryGraph*>& Graphs, QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> >& History)
{
	QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> > OldMap;
	m_pProcessList->StartUpdatingWidgets(OldMap, History);
	//for(QModelIndex Index = m_pProcessList->GetView()->indexAt(QPoint(0,0)); Index.isValid(); Index = m_pProcessList->GetView()->indexBelow(Index))
	for(QModelIndex Index = m_pProcessList->GetView()->indexAt(QPoint(0,0)); Index.isValid(); Index = m_pProcessList->GetView()->indexBelow(Index))
	{
		Index = Index.sibling(Index.row(), HistoryColumn);
		if(!m_pProcessList->GetView()->viewport()->rect().intersects(m_pProcessList->GetView()->visualRect(Index)))
			break; // out of view
		
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		quint64 PID = m_pProcessModel->Data(ModelIndex, Qt::UserRole, CProcessModel::eProcess).toULongLong();

		CHistoryWidget* pGraph = OldMap.take(PID).first;
		if (!pGraph)
		{
			QModelIndex Index = m_pSortProxy->mapFromSource(ModelIndex);

			pGraph = new CHistoryWidget(Graphs[PID]);
			pGraph->setFixedHeight(CellHeight);
			History.insert(PID, qMakePair((QPointer<CHistoryWidget>)pGraph, QPersistentModelIndex(Index)));
			m_pProcessList->GetView()->setIndexWidget(Index, pGraph);
		}
	}
	m_pProcessList->EndUpdatingWidgets(OldMap, History);
}

void CProcessTree::OnUpdateHistory()
{
	float Div = (theConf->GetInt("Options/LinuxStyleCPU") == 1) ? theAPI->GetCpuCount() : 1.0f;

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eCPU_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eCPU_History);

		QMap<quint64, CHistoryGraph*> Old = m_CPU_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetProcessId();
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, Qt::white, this);
				pGraph->AddValue(0, Qt::green);
				pGraph->AddValue(1, Qt::red);
				m_CPU_Graphs.insert(PID, pGraph);
			}

			STaskStats CpuStats = pProcess->GetCpuStats();

			pGraph->SetValue(0, CpuStats.CpuUsage / Div);
			pGraph->SetValue(1, CpuStats.CpuKernelUsage / Div);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys())
			m_CPU_Graphs.take(TID)->deleteLater();

		UpdateIndexWidget(CProcessModel::eCPU_History, CellHeight, m_CPU_Graphs, m_CPU_History);
	}

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eGPU_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eGPU_History);

		QMap<quint64, CHistoryGraph*> Old = m_GPU_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetProcessId();
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, Qt::white, this);
				pGraph->AddValue(0, Qt::green);
				m_GPU_Graphs.insert(PID, pGraph);
			}

			SGpuStats GpuStats = pProcess->GetGpuStats();

			pGraph->SetValue(0, GpuStats.GpuTimeUsage.Usage);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys())
			m_GPU_Graphs.take(TID)->deleteLater();

		UpdateIndexWidget(CProcessModel::eGPU_History, CellHeight, m_GPU_Graphs, m_GPU_History);
	}

	quint64 TotalMemoryUsed = theAPI->GetCommitedMemory();

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eMEM_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eMEM_History);

		QMap<quint64, CHistoryGraph*> Old = m_MEM_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetProcessId();
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, Qt::white, this);
				pGraph->AddValue(0, QColor("#CCFF33"));
				m_MEM_Graphs.insert(PID, pGraph);
			}

			pGraph->SetValue(0, TotalMemoryUsed ? (float)pProcess->GetWorkingSetSize() / TotalMemoryUsed : 0);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys())
			m_MEM_Graphs.take(TID)->deleteLater();

		UpdateIndexWidget(CProcessModel::eMEM_History, CellHeight, m_MEM_Graphs, m_MEM_History);
	}

#ifdef WIN32
	CGpuMonitor* pGpuMonitor = ((CWindowsAPI*)theAPI)->GetGpuMonitor();

	CGpuMonitor::SGpuMemory GpuMemory = pGpuMonitor->GetGpuMemory();
	quint64 TotalShared = GpuMemory.SharedLimit;
	quint64 TotalDedicated = GpuMemory.DedicatedLimit;
#else
	// linux-todo:
    quint64 TotalShared = 0;
    quint64 TotalDedicated = 0;
#endif

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eVMEM_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eVMEM_History);

		QMap<quint64, CHistoryGraph*> Old = m_VMEM_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetProcessId();
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, Qt::white, this);
				pGraph->AddValue(0, QColor("#CCFF33"));
				m_VMEM_Graphs.insert(PID, pGraph);
			}

			SGpuStats GpuStats = pProcess->GetGpuStats();

			float DedicatedMemory = TotalDedicated ? (float)GpuStats.GpuDedicatedUsage / TotalDedicated : 0;
			float SharedMemory = TotalShared ? (float)GpuStats.GpuSharedUsage / TotalShared : 0;

			pGraph->SetValue(0, qMax(DedicatedMemory, SharedMemory));
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys())
			m_VMEM_Graphs.take(TID)->deleteLater();

		UpdateIndexWidget(CProcessModel::eVMEM_History, CellHeight, m_VMEM_Graphs, m_VMEM_History);
	}


	SSysStats SysStats = theAPI->GetStats();
	quint64 TotalIO = SysStats.Io.ReadRate.Get() + SysStats.Io.WriteRate.Get() + SysStats.Io.OtherRate.Get();
	quint64 TotalDisk = SysStats.Disk.ReadRate.Get() + SysStats.Disk.WriteRate.Get();
	if (TotalDisk < TotalIO)
		TotalDisk = TotalIO;

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eIO_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eIO_History);

		QMap<quint64, CHistoryGraph*> Old = m_IO_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetProcessId();
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, Qt::white, this);
				pGraph->AddValue(0, Qt::green);
				pGraph->AddValue(1, Qt::red);
				pGraph->AddValue(2, Qt::blue);
				m_IO_Graphs.insert(PID, pGraph);
			}

			SProcStats IoStats = pProcess->GetStats();

			pGraph->SetValue(0, TotalDisk ? (float)qMax(IoStats.Disk.ReadRate.Get(), IoStats.Io.ReadRate.Get()) / TotalDisk : 0);
			pGraph->SetValue(1, TotalDisk ? (float)qMax(IoStats.Disk.WriteRate.Get(), IoStats.Io.WriteRate.Get()) / TotalDisk : 0);
			pGraph->SetValue(2, TotalIO ? (float)IoStats.Io.OtherRate.Get() / TotalIO : 0);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys())
			m_IO_Graphs.take(TID)->deleteLater();

		UpdateIndexWidget(CProcessModel::eIO_History, CellHeight, m_IO_Graphs, m_IO_History);
	}

	quint64 TotalNet = SysStats.Net.ReceiveRate.Get() + SysStats.Net.SendRate.Get();

	if (m_pProcessModel->IsColumnEnabled(CProcessModel::eNET_History))
	{
		int CellHeight = theGUI->GetCellHeight();
		int CellWidth = m_pProcessList->GetView()->columnWidth(CProcessModel::eNET_History);

		QMap<quint64, CHistoryGraph*> Old = m_NET_Graphs;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			quint64 PID = pProcess->GetProcessId();
			CHistoryGraph* pGraph = Old.take(PID);
			if (!pGraph)
			{
				pGraph = new CHistoryGraph(true, Qt::white, this);
				pGraph->AddValue(0, Qt::green);
				pGraph->AddValue(1, Qt::red);
				m_NET_Graphs.insert(PID, pGraph);
			}

			SProcStats IoStats = pProcess->GetStats();

			pGraph->SetValue(0, TotalNet ? (float)IoStats.Net.ReceiveRate.Get() / TotalNet : 0);
			pGraph->SetValue(1, TotalNet ? (float)IoStats.Net.SendRate.Get() / TotalNet : 0);
			pGraph->Update(CellHeight, CellWidth);
		}
		foreach(quint64 TID, Old.keys())
			m_NET_Graphs.take(TID)->deleteLater();

		UpdateIndexWidget(CProcessModel::eNET_History, CellHeight, m_NET_Graphs, m_NET_History);
	}
}
