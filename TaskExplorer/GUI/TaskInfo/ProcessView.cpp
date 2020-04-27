#include "stdafx.h"
#include "ProcessView.h"
#include "../TaskExplorer.h"
#include "../../Common/SortFilterProxyModel.h"
#include "../Models/ProcessModel.h"
#ifdef WIN32
#include "../../API/Windows/WinProcess.h"
#include "../../API/Windows/WinModule.h"
#include "../../API/Windows/ProcessHacker.h"
#endif


CProcessView::CProcessView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout();
	//m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	/*
	m_pScrollArea = new QScrollArea();
	m_pMainLayout->addWidget(m_pScrollArea);

	m_pInfoWidget = new QWidget();
	m_pScrollArea->setFrameShape(QFrame::NoFrame);
	m_pScrollArea->setWidgetResizable(true);
	m_pScrollArea->setWidget(m_pInfoWidget);
	QPalette pal = m_pScrollArea->palette();
	pal.setColor(QPalette::Background, Qt::transparent);
	m_pScrollArea->setPalette(pal);

	m_pInfoLayout = new QVBoxLayout();
	m_pInfoWidget->setLayout(m_pInfoLayout);
	*/

	m_pStackedWidget = new QWidget();
	m_pStackedLayout = new QStackedLayout();
	m_pStackedWidget->setLayout(m_pStackedLayout);
	//m_pInfoLayout->addWidget(m_pStackedWidget);
	m_pMainLayout->addWidget(m_pStackedWidget);
	m_pStackedWidget->setMaximumHeight(120);

	m_pOneProcWidget = new QWidget();
	m_pOneProcLayout = new QVBoxLayout();
	m_pOneProcLayout->setMargin(0);
	m_pOneProcWidget->setLayout(m_pOneProcLayout);
	//m_pInfoLayout->addWidget(m_pOneProcWidget);
	m_pStackedLayout->addWidget(m_pOneProcWidget);


	m_pFileBox = new QGroupBox(tr("File"));
	m_pOneProcLayout->addWidget(m_pFileBox);

	m_pFileLayout = new QGridLayout();
	m_pFileLayout->setSpacing(2);
	m_pFileBox->setLayout(m_pFileLayout);
	int row = 0;

	m_pIcon = new QLabel();
	m_pFileLayout->addWidget(m_pIcon, 0, 0, 2, 1);

	m_pProcessName = new QLabel();
	m_pProcessName->setSizePolicy(QSizePolicy::Expanding, m_pProcessName->sizePolicy().verticalPolicy());
	m_pFileLayout->addWidget(m_pProcessName, row++, 1);

	m_pCompanyName = new QLabel();
	m_pFileLayout->addWidget(m_pCompanyName, row++, 1);
	
	m_pFileLayout->addWidget(new QLabel(tr("Version:")), row, 0);
	m_pProcessVersion = new QLabel();
	m_pFileLayout->addWidget(m_pProcessVersion, row++, 1);

	m_pFileLayout->addWidget(new QLabel(tr("Image file name:")), row, 0, 1, 1);
	m_pSubSystem = new QLabel(tr("Subsystem:"));
	m_pSubSystem->setAlignment(Qt::AlignRight);
	m_pFileLayout->addWidget(m_pSubSystem, row++, 1, 1, 1);
	m_pFilePath = new QLineEdit();
	m_pFilePath->setReadOnly(true);
	m_pFileLayout->addWidget(m_pFilePath, row++, 0, 1, 2);

	m_pFileLayout->addItem(new QSpacerItem(20, 30, QSizePolicy::Expanding, QSizePolicy::Expanding), row++, 1);

	m_pTabWidget = new QTabWidget();
	m_pMainLayout->addWidget(m_pTabWidget);


	//m_pProcessBox = new QGroupBox(tr("Process"));
	m_pProcessBox = new QWidget();
	//m_pProcessBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	//m_pOneProcLayout->addWidget(m_pProcessBox);


	m_pProcessArea = new QScrollArea();

	m_pProcessArea->setFrameShape(QFrame::NoFrame);
	m_pProcessArea->setWidgetResizable(true);
	m_pProcessArea->setWidget(m_pProcessBox);
	QPalette pal = m_pProcessArea->palette();
	pal.setColor(QPalette::Background, Qt::transparent);
	m_pProcessArea->setPalette(pal);


	m_pProcessLayout = new QGridLayout();
	m_pProcessLayout->setSpacing(2);
	m_pProcessBox->setLayout(m_pProcessLayout);
	row = 0;

	m_pProcessLayout->addWidget(new QLabel(tr("Command line:")), row, 0);
	m_pCmdLine = new QLineEdit();
	//m_pCmdLine->setSizePolicy(QSizePolicy::Expanding, m_pCmdLine->sizePolicy().verticalPolicy());
	m_pCmdLine->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pCmdLine, row++, 1, 1, 2);

	m_pProcessLayout->addWidget(new QLabel(tr("Current directory:")), row, 0);
	m_pCurDir = new QLineEdit();
	m_pCurDir->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pCurDir, row++, 1, 1, 2);

#ifdef WIN32
	m_pProcessLayout->addWidget(new QLabel(tr("Used Desktop:")), row, 0);
	m_pDesktop = new QLineEdit();
	m_pDesktop->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pDesktop, row, 1, 1, 1);
	m_pDPIAware = new QLabel();
	m_pDPIAware->setMinimumWidth(100);
	m_pProcessLayout->addWidget(m_pDPIAware, row++, 2);
#else
	m_pProcessLayout->addWidget(new QLabel(tr("User name:")), row, 0);
	m_pUserName = new QLineEdit();
	m_pUserName->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pUserName, row++, 1, 1, 2);
#endif

	m_pProcessLayout->addWidget(new QLabel(tr("PID/Parent PID:")), row, 0);
	m_pProcessId = new QLineEdit();
	m_pProcessId->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pProcessId, row++, 1, 1, 2);

	m_pProcessLayout->addWidget(new QLabel(tr("Started by:")), row, 0);
	m_pStartedBy = new QLineEdit();
	m_pStartedBy->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pStartedBy, row++, 1, 1, 2);

#ifdef WIN32
	m_pProcessLayout->addWidget(new QLabel(tr("PEB address:")), row, 0);
	m_pPEBAddress = new QLineEdit();
	m_pPEBAddress->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pPEBAddress, row, 1, 1, 1);
	m_ImageType = new QLabel(tr("Image type:"));
	m_ImageType->setMinimumWidth(100);
	m_pProcessLayout->addWidget(m_ImageType, row++, 2);
#endif

	m_pProcessLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), row, 0);

#ifdef WIN32
	//m_pSecurityBox = new QGroupBox(tr("Security"));
	m_pSecurityBox = new QWidget();
	//m_pSecurityBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	//m_pOneProcLayout->addWidget(m_pSecurityBox);

	m_pSecurityLayout = new QGridLayout();
	m_pSecurityLayout->setSpacing(2);
	m_pSecurityBox->setLayout(m_pSecurityLayout);
	row = 0;


	m_pSecurityLayout->addWidget(new QLabel(tr("Verification: ")), row, 0);
	m_pVerification = new QLabel();
	m_pVerification->setSizePolicy(QSizePolicy::Expanding, m_pVerification->sizePolicy().verticalPolicy());
	m_pSecurityLayout->addWidget(m_pVerification, row++, 1);

	m_pSecurityLayout->addWidget(new QLabel(tr("Signer: ")), row, 0);
	m_pSigner = new QLabel();
	m_pSigner->setTextInteractionFlags(Qt::TextBrowserInteraction);
	connect(m_pSigner, SIGNAL(linkActivated(const QString&)), this, SLOT(OnCertificate(const QString&)));
	m_pSecurityLayout->addWidget(m_pSigner, row++, 1);


	QLabel* pMitigation = new QLabel(tr("Mitigation policies:"));
	pMitigation->setFixedHeight(20);
	m_pSecurityLayout->addWidget(pMitigation, row, 0);

	m_Protecetion = new QLabel();
	m_pSecurityLayout->addWidget(m_Protecetion, row++, 2);

	m_pMitigation = new CPanelWidgetEx();
	m_pMitigation->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	m_pMitigation->GetTree()->setHeaderLabels(tr("Name|Description").split("|"));

	m_pMitigation->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pMitigation->GetView()->setSortingEnabled(false);

	//m_pMitigation->setMinimumHeight(100);
	//m_pMitigation->GetTree()->setAutoFitMax(200);

	m_pSecurityLayout->addWidget(m_pMitigation, row++, 0, 1, 3);


	if (WindowsVersion >= WINDOWS_8)
	{
		//m_pAppBox = new QGroupBox(tr("App"));
		m_pAppBox = new QWidget();
		//m_pOneProcLayout->addWidget(m_pAppBox);

		m_pAppLayout = new QGridLayout();
		m_pAppLayout->setSpacing(2);
		m_pAppBox->setLayout(m_pAppLayout);
		row = 0;

		m_pAppLayout->addWidget(new QLabel(tr("App ID:")), row, 0);
		m_pAppID = new QLineEdit();
		m_pAppID->setReadOnly(true);
		m_pAppLayout->addWidget(m_pAppID, row++, 1, 1, 1);

		m_pAppLayout->addWidget(new QLabel(tr("Package Name:")), row, 0);
		m_pPackageName = new QLineEdit();
		m_pPackageName->setReadOnly(true);
		m_pAppLayout->addWidget(m_pPackageName, row++, 1, 1, 1);

		/*m_pAppLayout->addWidget(new QLabel(tr("Data Directory:")), row, 0);
		m_pPackageDataDir = new QLineEdit();
		m_pPackageDataDir->setReadOnly(true);
		m_pAppLayout->addWidget(m_pPackageDataDir, row++, 1, 1, 1);*/

		m_pAppLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), row, 0);
	}
	else
		m_pAppBox = NULL;
#endif

	m_pMultiProcWidget = new QWidget();
	m_pMultiProcLayout = new QVBoxLayout();
	m_pMultiProcLayout->setMargin(0);
	m_pMultiProcWidget->setLayout(m_pMultiProcLayout);
	//m_pInfoLayout->addWidget(m_pMultiProcWidget);
	m_pMainLayout->addWidget(m_pMultiProcWidget);
	//m_pMultiProcWidget->setVisible(false);
	m_pStackedLayout->addWidget(m_pMultiProcWidget);


	// Process List
	m_pProcessModel = new CProcessModel();
	//connect(m_pProcessModel, SIGNAL(CheckChanged(quint64, bool)), this, SLOT(OnCheckChanged(quint64, bool)));
	//connect(m_pProcessModel, SIGNAL(Updated()), this, SLOT(OnUpdated()));

	m_pProcessModel->SetTree(false);

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pProcessModel);
	m_pSortProxy->setDynamicSortFilter(true);


	m_pProcessList = new QTreeViewEx();
	m_pProcessList->setItemDelegate(theGUI->GetItemDelegate());
	m_pProcessList->setMinimumHeight(50);

	m_pProcessList->setModel(m_pSortProxy);

	m_pProcessList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pProcessList->setSortingEnabled(true);

	connect(theGUI, SIGNAL(ReloadPanels()), m_pProcessModel, SLOT(Clear()));

	//connect(m_pProcessList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	connect(m_pProcessList->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));

	m_pProcessList->setColumnReset(2);
	connect(m_pProcessList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	connect(m_pProcessList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	m_pMultiProcLayout->addWidget(m_pProcessList);
	///


	m_pStatsView = new CStatsView(CStatsView::eProcess, this);
	m_pStatsView->setSizePolicy(m_pStatsView->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
	//m_pInfoLayout->addWidget(m_pStatsView);

	m_pTabWidget->addTab(m_pProcessArea, "Details");
	m_pTabWidget->addTab(m_pStatsView, "Statistics");
	//m_pTabWidget->addTab(m_pProcessBox, "Details");
#ifdef WIN32
	m_pTabWidget->addTab(m_pSecurityBox, "Security");
	if(m_pAppBox)
		m_pTabWidget->addTab(m_pAppBox, "App");
#endif

	/*QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pProcessLayout->addWidget(pSpacer, row, 1);*/
	//m_pInfoLayout->addWidget(pSpacer);

	for (int i = 0; i < m_pProcessModel->columnCount(); i++)
	{
		if ((i >= CProcessModel::eCPU_History && i <= CProcessModel::eVMEM_History)
#ifdef WIN32
		 || (i >= CProcessModel::eIntegrity && i <= CProcessModel::eCritical)
#endif
		 || (i >= CProcessModel::eCPU && i <= CProcessModel::eCyclesDelta)
		 || (i >= CProcessModel::ePrivateBytes && i <= CProcessModel::ePrivateBytesDelta)
		 || (i >= CProcessModel::eGPU_Usage && i <= CProcessModel::eGPU_Adapter)
		 || (i >= CProcessModel::ePriorityClass && i <= CProcessModel::eIO_Priority)
		 || (i >= CProcessModel::eHandles && i <= CProcessModel::ePeakThreads)
		 || (i >= CProcessModel::eSubsystem && i <= CProcessModel::eSessionID)
		 || (i >= CProcessModel::eIO_TotalRate && i <= CProcessModel::eIO_OtherRate)
		 || (i >= CProcessModel::eNet_TotalRate && i <= CProcessModel::eSendRate)
		 || (i >= CProcessModel::eDisk_TotalRate && i <= CProcessModel::eWriteRate)
		 || i == CProcessModel::eSharedWS || i == CProcessModel::eShareableWS)
		{
			m_pProcessList->SetColumnHidden(i, true, true);
		}
	}

	setObjectName(parent ? parent->objectName() : "InfoWindow");
	QByteArray Columns = theConf->GetBlob(objectName() + "/Processes_Columns");
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pProcessList->restoreState(Columns);
#ifdef WIN32
	m_pMitigation->GetTree()->header()->restoreState(theConf->GetBlob(objectName() + "/Mitigation_Columns"));
#endif
	m_pTabWidget->setCurrentIndex(theConf->GetValue(objectName() + "/Process_Tabs").toInt());
}


CProcessView::~CProcessView()
{
	theConf->SetBlob(objectName() + "/Processes_Columns", m_pProcessList->saveState());
#ifdef WIN32
	theConf->SetBlob(objectName() + "/Mitigation_Columns", m_pMitigation->GetTree()->header()->saveState());
#endif
	theConf->SetValue(objectName() + "/Process_Tabs", m_pTabWidget->currentIndex());
}

void CProcessView::OnResetColumns()
{
	for (int i = 0; i < m_pProcessModel->columnCount(); i++)
		m_pProcessList->setColumnHidden(i, true);

	m_pProcessList->SetColumnHidden(CProcessModel::eProcess, false);
	m_pProcessList->SetColumnHidden(CProcessModel::ePID, false);
	//m_pProcessList->SetColumnHidden(CProcessModel::eCPU, false);
	m_pProcessList->SetColumnHidden(CProcessModel::eUserName, false);
#ifdef WIN32
	m_pProcessList->SetColumnHidden(CProcessModel::eVersion, false);
	m_pProcessList->SetColumnHidden(CProcessModel::eCompanyName, false);
#endif
	m_pProcessList->SetColumnHidden(CProcessModel::eCommandLine, false);
	//current directory
	m_pProcessList->SetColumnHidden(CProcessModel::eFileName, false);
	// started by
}

void CProcessView::OnColumnsChanged()
{
	SyncModel();
}

void CProcessView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	if (m_Processes != Processes)
	{
		m_Processes = Processes;

		if(m_Processes.count() <= 1)
		{
			//m_pMultiProcWidget->setVisible(false);
			//m_pOneProcWidget->setVisible(true);
			m_pStackedLayout->setCurrentWidget(m_pOneProcWidget);

			if (m_Processes.count() == 1)
			{
				CProcessPtr pProcess = m_Processes.first();

				ShowProcess(pProcess);
			}
		}
		else
		{
			//m_pMultiProcWidget->setVisible(true);
			//m_pOneProcWidget->setVisible(false);
			m_pStackedLayout->setCurrentWidget(m_pMultiProcWidget);

			m_pTabWidget->setCurrentIndex(0);
		}
	}

	Refresh();
}

void CProcessView::ShowProcess(const CProcessPtr pProcess)
{
	CModulePtr pModule = pProcess->GetModuleInfo();

	QPixmap Icon;
	QString Description;
	if (pModule)
	{
		Icon = pModule->GetFileIcon(true);
		Description = pModule->GetFileInfo("Description");
		m_pCompanyName->setText(pModule->GetFileInfo("CompanyName"));
		m_pProcessVersion->setText(pModule->GetFileInfo("FileVersion"));
	}
	else
	{
		m_pCompanyName->setText("");
		m_pProcessVersion->setText("");
	}

	m_pIcon->setPixmap(Icon.isNull() ? g_ExeIcon.pixmap(32) : Icon);
	if (!Description.isEmpty())
		m_pProcessName->setText(Description + " (" + pProcess->GetName() + ")");
	else
		m_pProcessName->setText(pProcess->GetName());
	m_pFilePath->setText(pProcess->GetFileName());

	m_pCmdLine->setText(pProcess->GetCommandLineStr());
	m_pCurDir->setText(pProcess->GetWorkingDirectory());
	m_pProcessId->setText(tr("%1/%2").arg(pProcess->GetProcessId()).arg(pProcess->GetParentId()));
#ifndef WIN32
	m_pUserName->setText(pProcess->GetUserName());
#endif

	CProcessPtr pParent = theAPI->GetProcessByID(pProcess->GetParentId());
	if (!pProcess->ValidateParent(pParent.data()))
		pParent.clear();
	m_pStartedBy->setText(pParent.isNull() ? tr("N/A") : pParent->GetFileName());


#ifdef WIN32
	CWinProcess* pWinProc = qobject_cast<CWinProcess*>(pProcess.data());

	CWinMainModule* pWinModule = qobject_cast<CWinMainModule*>(pModule.data());

	quint32 Subsystem = pWinProc->GetSubsystem();
	bool bConsole = false;
	if (pWinProc->GetOsContextVersion() != 0 && (Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI || (bConsole = (Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI))))
		m_pSubSystem->setText(tr("Subsystem: Windows %1%2").arg(pWinProc->GetOsContextString()).arg(bConsole ? tr(" console") : tr("")));
	else
		m_pSubSystem->setText(tr("Subsystem: %1").arg(pWinProc->GetSubsystemString()));

	m_pVerification->setText(pWinModule ? pWinModule->GetVerifyResultString() : "");
	m_pSigner->setText(QString("<a href=\"%1\">%2</a>").arg(pProcess->GetFileName()).arg((pWinModule ? pWinModule->GetVerifySignerName() : "")));

	m_pDesktop->setText(pWinProc->GetUsedDesktop());
	m_pDPIAware->setText(tr("DPI Scaling: %1").arg(pWinProc->GetDPIAwarenessString()));

	if (!pWinModule)
		m_pPEBAddress->setText("");
	else if (pWinProc->IsWoW64())
		m_pPEBAddress->setText(tr("%1 (32-bit: %2)").arg(FormatAddress(pWinModule->GetPebBaseAddress())).arg(FormatAddress(pWinModule->GetPebBaseAddress(true))));
	else
		m_pPEBAddress->setText(FormatAddress(pWinModule->GetPebBaseAddress()));

	m_ImageType->setText(tr("Image type: %1").arg(pProcess->GetArchString()));

	//m_pMitigation->setText(pWinProc->GetMitigationString());
	m_Protecetion->setText(tr("Protection: %1").arg(pWinProc->GetProtectionString()));

	m_pMitigation->GetTree()->clear();
	QList<QPair<QString, QString>> List = pWinProc->GetMitigationDetails();
	for (int i = 0; i < List.count(); i++)
	{
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		pItem->setText(0, List[i].first);
		pItem->setText(1, List[i].second);
		m_pMitigation->GetTree()->addTopLevelItem(pItem);
	}

	if (m_pAppBox)
	{
		m_pAppID->setText(pWinProc->GetAppID());
		m_pPackageName->setText(pWinProc->GetPackageName());
		//m_pPackageDataDir->setText(pWinProc->GetAppDataDirectory());
	}
#endif
}

void CProcessView::SyncModel()
{
	QMap<quint64, CProcessPtr> ProcessList;
	foreach(const CProcessPtr& pProcess, m_Processes)
		ProcessList.insert(pProcess->GetProcessId(), pProcess);
	m_pProcessModel->Sync(ProcessList);
}

void CProcessView::Refresh()
{
	if (m_Processes.count() > 1)
		SyncModel();

	m_pStatsView->ShowProcesses(m_Processes);
}

void CProcessView::OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	ShowProcess(pProcess);
}

void CProcessView::OnCertificate(const QString& Link)
{
#ifdef WIN32
	wstring fileName = Link.toStdWString();

	PH_VERIFY_FILE_INFO info;
    memset(&info, 0, sizeof(PH_VERIFY_FILE_INFO));
    info.FileName = (wchar_t*)fileName.c_str();
    info.Flags = PH_VERIFY_VIEW_PROPERTIES;
    info.hWnd = NULL;

	PPH_STRING packageFullName = m_Processes.isEmpty() ? NULL : CastQString(m_Processes.first().staticCast<CWinProcess>()->GetPackageName());

    PhVerifyFileWithAdditionalCatalog(&info, packageFullName, NULL);

	PhDereferenceObject(packageFullName);
#endif
}
