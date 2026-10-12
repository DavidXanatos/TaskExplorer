#include "stdafx.h"
#include "ProcessView.h"
#include "../../API/SystemAPI.h"
#include "../TaskExplorer.h"
#include "../DesktopActions.h"
#include "../TaskStrings.h"
#include "../../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "../../../MiscHelpers/Common/CollapsibleGroupBox.h"
#include "../Models/ProcessModel.h"
#include "../SystemInfo/ServicesView.h"
#include "EnvironmentView.h"


CProcessView::CProcessView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout();
	//m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	/*
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
	*/

	m_pStackedWidget = new QWidget();
	m_pStackedLayout = new QStackedLayout();
	m_pStackedWidget->setLayout(m_pStackedLayout);
	//m_pInfoLayout->addWidget(m_pStackedWidget);
	m_pMainLayout->addWidget(m_pStackedWidget);
	m_pStackedWidget->setMaximumHeight(200);

	m_pOneProcWidget = new QWidget();
	m_pOneProcLayout = new QVBoxLayout();
	m_pOneProcLayout->setContentsMargins(0, 0, 0, 0);
	m_pOneProcWidget->setLayout(m_pOneProcLayout);
	//m_pInfoLayout->addWidget(m_pOneProcWidget);
	m_pStackedLayout->addWidget(m_pOneProcWidget);


	m_pFileBox = new QGroupBox(tr("File"));
	//m_pFileBox = new CCollapsibleGroupBox(this);
	m_pOneProcLayout->addWidget(m_pFileBox);

	m_pFileLayout = new QGridLayout();
	m_pFileLayout->setSpacing(2);
	m_pFileBox->setLayout(m_pFileLayout);
	int row = 0;

	m_pIcon = new QLabel();
	m_pIcon->setPixmap(g_ExeIcon.pixmap(32));
	m_pFileLayout->addWidget(m_pIcon, 0, 0, 2, 1);

	m_pProcessName = new QLabel();
	m_pProcessName->setSizePolicy(QSizePolicy::Ignored, m_pProcessName->sizePolicy().verticalPolicy());
	m_pFileLayout->addWidget(m_pProcessName, row++, 1, 1, 2);

	m_pCompanyName = new QLabel();
	m_pFileLayout->addWidget(m_pCompanyName, row++, 1, 1, 2);
	
	m_pFileLayout->addWidget(new QLabel(tr("Version:")), row, 0);
	m_pProcessVersion = new QLabel();
	m_pFileLayout->addWidget(m_pProcessVersion, row++, 1, 1, 2);

	//
	// Above the fields it explains, and hidden until there is something to
	// explain - the ordinary case must look exactly as it did.
	//
	m_pRedacted = new QLabel(tr("This process belongs to another user; the machine reports only its name."));
	m_pRedacted->setWordWrap(true);
	m_pRedacted->setVisible(false);
	m_pFileLayout->addWidget(m_pRedacted, row++, 0, 1, 3);

	m_pFileLayout->addWidget(new QLabel(tr("Image file name:")), row, 0, 1, 2);
	m_pSubSystem = new QLabel(tr("Subsystem:"));
	m_pSubSystem->setAlignment(Qt::AlignRight);
	m_pFileLayout->addWidget(m_pSubSystem, row++, 2, 1, 1);
	m_pFilePath = new QLineEdit();
	m_pFilePath->setReadOnly(true);
	m_pFileLayout->addWidget(m_pFilePath, row++, 0, 1, 3);
	//
	// What Windows calls this image, for a process running under Wine.
	//
	// Its own row rather than replacing the one above: both are true and they
	// name different things - the row above is the file on this machine's disk,
	// this is what the program itself sees. Hidden for everything else, and it
	// goes in this layout, beside the other paths, not in the process grid
	// further down - which does not exist yet at this point in construction.
	//
	m_pWineImageLabel = new QLabel(tr("Windows image:"));
	m_pFileLayout->addWidget(m_pWineImageLabel, row++, 0, 1, 2);
	m_pWineImage = new QLineEdit();
	m_pWineImage->setReadOnly(true);
	m_pFileLayout->addWidget(m_pWineImage, row++, 0, 1, 3);

	//
	// The NT path - \Device\HarddiskVolume3\Windows\... - is a Windows notion with
	// no counterpart elsewhere, so the label is kept rather than left anonymous:
	// both it and the field are hidden for a process on a machine that has no
	// such thing. See ShowProcess.
	//
	m_pFilePathNtLabel = new QLabel(tr("Image NT file name:"));
	m_pFileLayout->addWidget(m_pFilePathNtLabel, row++, 0, 1, 2);
	m_pFilePathNt = new QLineEdit();
	m_pFilePathNt->setReadOnly(true);
	m_pFileLayout->addWidget(m_pFilePathNt, row++, 0, 1, 3);
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
	pal.setColor(QPalette::Window, Qt::transparent);
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

	//
	// Desktop and DPI awareness are window-station concepts; where the target has
	// none, the row carries the user name instead - which on Windows comes from
	// the token shown further down.
	//
	// A row each, and they used to share one.
	//
	// Sharing was right while the desktop was a Windows-only notion and the user
	// name its non-Windows replacement - one row, one of the two shown. Linux
	// answers the desktop now (the display the process draws on, see
	// CLinuxProcess::GetUsedDesktop) and still wants the user name, so the two
	// are no longer alternatives.
	//
	// A hidden widget is empty as far as a QGridLayout is concerned, so the row a
	// platform does not use costs nothing but the spacing.
	//
	m_pDesktopLabel = new QLabel(tr("Used Desktop:"));
	m_pProcessLayout->addWidget(m_pDesktopLabel, row, 0);
	m_pDesktop = new QLineEdit();
	m_pDesktop->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pDesktop, row, 1, 1, 1);
	m_pDPIAware = new QLabel();
	m_pDPIAware->setMinimumWidth(100);
	m_pProcessLayout->addWidget(m_pDPIAware, row++, 2);

	m_pUserNameLabel = new QLabel(tr("User name:"));
	m_pProcessLayout->addWidget(m_pUserNameLabel, row, 0);
	m_pUserName = new QLineEdit();
	m_pUserName->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pUserName, row++, 1, 1, 2);

	m_pProcessLayout->addWidget(new QLabel(tr("PID/Parent PID:")), row, 0);
	m_pProcessId = new QLineEdit();
	m_pProcessId->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pProcessId, row++, 1, 1, 2);

	m_pProcessLayout->addWidget(new QLabel(tr("Started by:")), row, 0);
	m_pStartedBy = new QLineEdit();
	m_pStartedBy->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pStartedBy, row++, 1, 1, 2);
	//
	// The PEB is a Windows structure with no counterpart elsewhere. Greyed out
	// rather than hidden, because the image type sits beside it in this row and
	// removing the left of the row would leave that stranded on its own.
	//
	m_pPEBAddressLabel = new QLabel(tr("PEB address:"));
	m_pProcessLayout->addWidget(m_pPEBAddressLabel, row, 0);
	m_pPEBAddress = new QLineEdit();
	m_pPEBAddress->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pPEBAddress, row, 1, 1, 1);
	m_ImageType = new QLabel(tr("Image type:"));
	m_ImageType->setMinimumWidth(100);
	m_pProcessLayout->addWidget(m_ImageType, row++, 2);
	m_pProcessLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), row, 0);
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
	m_pSecurityLayout->addWidget(m_Protecetion, row++, 2, 1, 2);

	m_pMitigation = new CPanelWidgetEx();
	m_pMitigation->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	m_pMitigation->GetTree()->setHeaderLabels(tr("Name|Description").split("|"));

	m_pMitigation->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pMitigation->GetView()->setSortingEnabled(false);

	//m_pMitigation->setMinimumHeight(100);
	//m_pMitigation->GetTree()->setAutoFitMax(200);

	m_pSecurityLayout->addWidget(m_pMitigation, row++, 0, 1, 4);

	m_pPermissions = new QPushButton(tr("Permissions"));
	m_pPermissions->setEnabled(theSystem->HasCapability(CSystemAPI::eCapSecurityEditor));
	connect(m_pPermissions, SIGNAL(pressed()), this, SLOT(OnPermissions()));
	m_pSecurityLayout->addWidget(m_pPermissions, row, 3);

	QWidget* pPolicyWidget = new QWidget(this);
	QHBoxLayout* pPolicyLayout = new QHBoxLayout(pPolicyWidget);
	pPolicyLayout->setContentsMargins(0, 0, 0, 0);
	m_pSecurityLayout->addWidget(pPolicyWidget, row, 0, 1, 3);
	pPolicyLayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Minimum));
	m_pNoWriteUp = new QCheckBox(tr("No-Write-Up"));
	connect(m_pNoWriteUp, SIGNAL(clicked(bool)), this, SLOT(OnPolicy()));
	pPolicyLayout->addWidget(m_pNoWriteUp);
	m_pNoReadUp = new QCheckBox(tr("No-Read-Up"));
	connect(m_pNoReadUp, SIGNAL(clicked(bool)), this, SLOT(OnPolicy()));
	pPolicyLayout->addWidget(m_pNoReadUp);
	m_pNoExecuteUp = new QCheckBox(tr("No-Execute-Up"));
	connect(m_pNoExecuteUp, SIGNAL(clicked(bool)), this, SLOT(OnPolicy()));
	pPolicyLayout->addWidget(m_pNoExecuteUp);


	//
	// Packaged applications are a Windows concept; the box has nothing to show
	// against another target.
	//
	if (theSystem->GetOsType() == CSystemAPI::eOsWindows)
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
	m_pMultiProcWidget = new QWidget();
	m_pMultiProcLayout = new QVBoxLayout();
	m_pMultiProcLayout->setContentsMargins(0, 0, 0, 0);
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

	m_pSortProxy = new CSortFilterProxyModel(this);
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

	m_pTabWidget->addTab(m_pProcessArea, tr("Details"));
	m_pTabWidget->addTab(m_pStatsView, tr("Statistics"));
	//m_pTabWidget->addTab(m_pProcessBox, tr("Details"));

	//
	// Security here is the Windows token - integrity level, elevation, virtual-
	// isation - and App is the Store package. Neither has a Linux counterpart:
	// what confines a process there is capabilities, an LSM profile and a
	// control group, which have tabs of their own.
	//
	// Their indexes are kept so that ShowProcess can grey them out for a process
	// on a machine that has neither, rather than leaving two tabs that open onto
	// blank fields.
	//
	m_SecurityTab = m_pTabWidget->addTab(m_pSecurityBox, tr("Security"));
	if(m_pAppBox)
		m_AppTab = m_pTabWidget->addTab(m_pAppBox, tr("App"));
	m_pServiceView = new CServicesView(false, parent);
	m_pTabWidget->addTab(m_pServiceView, tr("Service"));
	m_pEnvironmentView = new CEnvironmentView(parent);
	m_pTabWidget->addTab(m_pEnvironmentView, tr("Environment"));

	/*QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pProcessLayout->addWidget(pSpacer, row, 1);*/
	//m_pInfoLayout->addWidget(pSpacer);

	for (int i = 0; i < m_pProcessModel->columnCount(); i++)
	{
		if ((i >= CProcessModel::eCPU_History && i <= CProcessModel::eVMEM_History)
		 || (i >= CProcessModel::eIntegrity && i <= CProcessModel::eCritical)
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
	m_pMitigation->GetTree()->header()->restoreState(theConf->GetBlob(objectName() + "/Mitigation_Columns"));
	m_pTabWidget->setCurrentIndex(theConf->GetValue(objectName() + "/Process_Tabs").toInt());
}


CProcessView::~CProcessView()
{
	theConf->SetBlob(objectName() + "/Processes_Columns", m_pProcessList->saveState());
	theConf->SetBlob(objectName() + "/Mitigation_Columns", m_pMitigation->GetTree()->header()->saveState());
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
	m_pProcessList->SetColumnHidden(CProcessModel::eVersion, false);
	m_pProcessList->SetColumnHidden(CProcessModel::eCompanyName, false);
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

			if (m_Processes.isEmpty())
				ClearProcess();

			if (m_Processes.count() == 1)
			{
				CProcessPtr pProcess = m_Processes.first();

				//
				// Most of what this panel shows is not in the process list -
				// see API_CMD_PROCDETAIL - so it is fetched when a process is
				// selected rather than for every process on every refresh.
				//
				// Asynchronously, because for a remote machine this is a round
				// trip and the selection changes on a click. ShowProcess draws
				// what is known now; the values land a moment later and the
				// next refresh draws them.
				//
				// The call holds its own reference; see CHandlesView::Refresh.
				const CProcessPtr pKeep = pProcess;
				QMetaObject::invokeMethod(pProcess.data(), [pKeep]() { pKeep->UpdateDetails(); },
					Qt::QueuedConnection);

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

void CProcessView::ClearProcess()
{
	//
	// Every value the panel writes, and none of the captions beside them. Listed
	// rather than swept out of the layout, because a caption is a QLabel too and
	// clearing those would leave a form of empty boxes with nothing to say what
	// they were.
	//
	QLabel* const Labels[] = { m_pProcessName, m_pCompanyName, m_pProcessVersion,
		m_pSubSystem, m_pRedacted, m_pDPIAware, m_ImageType, m_pVerification,
		m_pSigner, m_Protecetion };
	for (size_t i = 0; i < sizeof(Labels) / sizeof(Labels[0]); i++)
	{
		if (Labels[i])
			Labels[i]->clear();
	}

	QLineEdit* const Edits[] = { m_pFilePath, m_pWineImage, m_pFilePathNt, m_pCmdLine,
		m_pCurDir, m_pDesktop, m_pUserName, m_pProcessId, m_pStartedBy, m_pPEBAddress,
		m_pAppID, m_pPackageName };
	for (size_t i = 0; i < sizeof(Edits) / sizeof(Edits[0]); i++)
	{
		if (Edits[i])
			Edits[i]->clear();
	}

	if (m_pIcon)
		m_pIcon->clear();
}

void CProcessView::ShowProcess(const CProcessPtr& pProcess)
{
	CModulePtr pModule = pProcess->GetModuleInfo();

	QPixmap Icon;
	QString Description;
	if (pModule)
	{
		Icon = ::MakeIcon(pModule->GetFileIcon(true), 32);
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
		m_pProcessName->setText(Description + " (" + ::LocalizeName(pProcess->GetName()) + ")");
	else
		m_pProcessName->setText(::LocalizeName(pProcess->GetName()));
	// just in case its to long but we want to see it al
	m_pProcessName->setToolTip(m_pProcessName->text().length() > 50 ? m_pProcessName->text() : ""); 

	m_pRedacted->setVisible(pProcess->IsRedacted());

	m_pFilePath->setText(pProcess->GetFileName());

	//
	// What this machine has, and what it does not.
	//
	// Asked of the process's own system rather than of the one being viewed: a
	// selection can span machines, and what is drawn here belongs to this
	// process.
	//
	const SWineInfo WineInfo = pProcess->GetWineInfo();
	{
		//
		// Two questions, and they are not the same one.
		//
		// bWindows is "does this process have the shape Windows gives one" - a
		// PE image, an NT path, a security tab - and Wine gives it that shape,
		// so a Wine process answers yes. See CTaskInfoView::UpdateTabAvailability.
		//
		// bNativeWindows is "is this a Windows kernel's process", which is a
		// narrower thing and is what the two rows below actually need: the PEB
		// is read out of the process by the Windows collector, and the desktop
		// is a window-station a Linux machine has none of. Wine has both notions
		// internally and the bridge reads neither, so treating a Wine process as
		// Windows for those two left a labelled row that was permanently empty -
		// which reads as "this process has no PEB" rather than "nobody asked".
		//
		const bool bNativeWindows = pProcess->GetSystem()
			&& pProcess->GetSystem() && pProcess->GetSystem()->GetOsType() == CSystemAPI::eOsWindows;
		const bool bWindows = bNativeWindows || WineInfo.Valid;

		const QString NotHere = tr("This machine does not run Windows, which is where this comes from.");
		const QString NotFromWine = tr("Wine has this, but the bridge into the prefix does not read it.");
		const QString WhyNot = WineInfo.Valid ? NotFromWine : NotHere;

		//
		// The NT path, and only where there is one.
		//
		// CProcessInfo::GetFileNameNt returns GetFileName - only CWinProcess
		// overrides it with the real \Device\HarddiskVolumeN form, and
		// CRemoteProcess falls back to the same when the server sent nothing.
		// So off a Windows kernel this row is a verbatim copy of the image file
		// name two rows above it, which is worse than absent: a reader takes two
		// differently labelled rows to be two different facts and looks for the
		// difference.
		//
		// bNativeWindows and not bWindows, therefore - a Wine process has a
		// Windows image path, which is its own row, but no NT path: Wine has the
		// notion and the bridge does not report it. The day it does, this comes
		// back by reporting it rather than by changing anything here.
		//
		m_pFilePathNtLabel->setVisible(bNativeWindows);
		m_pFilePathNt->setVisible(bNativeWindows);

		//
		// Wine is a property of the process, not of the machine: the row next to
		// this one may be an ELF and this one a PE.
		//
		const SWineInfo& Wine = WineInfo;
		const bool bWine = Wine.Valid && !Wine.ImagePath.isEmpty();
		m_pWineImageLabel->setVisible(bWine);
		m_pWineImage->setVisible(bWine);
		//
		// The path, and only the path. The Windows pid used to be appended here
		// and has moved to the PID row below, where the other pid already is:
		// two numberings of one process belong beside each other, and a pid in
		// the row labelled with the image was somewhere nobody would look for it.
		//
		if (bWine)
			m_pWineImage->setText(Wine.ImagePath);

		//
		// The desktop, its DPI awareness and the user name share one row; which
		// of them belongs there is a property of the machine, not of the build.
		//
		//
		// The desktop is answered on both platforms now - a window station and
		// desktop on Windows, the display the process draws on elsewhere - so the
		// row is shown either way and says nothing when there is nothing to say,
		// which for a daemon is most of the time.
		//
		m_pDesktopLabel->setVisible(true);
		m_pDesktop->setVisible(true);

		//
		// DPI awareness has no meaning off Windows, and the user name is shown
		// here only where there is no token tab to carry it.
		//
		m_pDPIAware->setVisible(bNativeWindows);
		m_pUserNameLabel->setVisible(!bNativeWindows);
		m_pUserName->setVisible(!bNativeWindows);

		m_pPEBAddressLabel->setEnabled(bNativeWindows);
		m_pPEBAddress->setEnabled(bNativeWindows);
		m_pPEBAddressLabel->setToolTip(bNativeWindows ? QString() : WhyNot);
		m_pPEBAddress->setToolTip(bNativeWindows ? QString() : WhyNot);

		m_pTabWidget->setTabEnabled(m_SecurityTab, bWindows);
		m_pTabWidget->setTabToolTip(m_SecurityTab, bWindows ? QString() : NotHere);
		if (m_AppTab != -1)
		{
			m_pTabWidget->setTabEnabled(m_AppTab, bWindows);
			m_pTabWidget->setTabToolTip(m_AppTab, bWindows ? QString() : NotHere);
		}
	}

	m_pFilePathNt->setText(pProcess->GetFileNameNt());
	m_pCmdLine->setText(pProcess->GetCommandLineStr());
	m_pCurDir->setText(pProcess->GetWorkingDirectory());
	//
	// A Wine process has two pids and they are unrelated numbers - measured, 296
	// against 15336 for the same program. The Linux pair first, because that is
	// what every other row here, every list, and anything typed at a shell on
	// this machine means by "pid"; the Windows pair in brackets after it, and
	// only once the bridge has paired them - it is opt-in and absent more often
	// than not, and a zero there would read as process zero.
	//
	m_pProcessId->setText((WineInfo.Valid && WineInfo.WinPid)
		? tr("%1/%2  (Windows %3/%4)")
			.arg(pProcess->GetProcessId()).arg(pProcess->GetParentId())
			.arg(WineInfo.WinPid).arg(WineInfo.WinParentPid)
		: tr("%1/%2").arg(pProcess->GetProcessId()).arg(pProcess->GetParentId()));
	if (m_pUserName)
		m_pUserName->setText(::LocalizeName(pProcess->GetUserName()));
	//
	// The parent is looked up on the machine this process came from, which may
	// no longer be there: a panel holds its process by shared pointer, so the
	// object outlives the disconnected machine that collected it. See
	// CAbstractInfo::GetSystem - it answers null for exactly this.
	//
	CProcessPtr pParent;
	if (CSystemPtr pSystem = pProcess->GetSystem())
		pParent = pSystem->GetProcessByID(pProcess->GetParentId());
	if (!pProcess->ValidateParent(pParent.data()))
		pParent.clear();
	m_pStartedBy->setText(pParent.isNull() ? tr("N/A") : pParent->GetFileName());

	quint32 Subsystem = pProcess->GetSubsystem();
	bool bConsole = false;
	if (pProcess->GetOsContextVersion() != 0 && (Subsystem == CProcessInfo::eSubsystemWindowsGui || (bConsole = (Subsystem == CProcessInfo::eSubsystemWindowsCui))))
		m_pSubSystem->setText(tr("Subsystem: Windows %1%2").arg(::GetOsContextString(pProcess)).arg(bConsole ? tr(" console") : tr("")));
	else
		m_pSubSystem->setText(tr("Subsystem: %1").arg(::GetSubsystemString(pProcess)));

	m_pVerification->setText(::GetVerifyResultString(pModule));
	m_pSigner->setText(QString("<a href=\"%1\">%2</a>").arg(pProcess->GetFileName()).arg(pModule ? pModule->GetVerifySignerName() : ""));

	if (m_pDesktop)
		m_pDesktop->setText(pProcess->GetUsedDesktop());
	if (m_pDPIAware)
		m_pDPIAware->setText(tr("DPI Scaling: %1").arg(::GetDPIAwarenessString(pProcess)));

	const quint64 PebAddress = pProcess->GetPebBaseAddress();
	if (PebAddress == 0)
		m_pPEBAddress->setText("");
	else if (pProcess->IsWoW64())
		m_pPEBAddress->setText(tr("%1 (32-bit: %2)").arg(FormatAddress(PebAddress)).arg(FormatAddress(pProcess->GetPebBaseAddress(true))));
	else
		m_pPEBAddress->setText(FormatAddress(PebAddress));

	m_ImageType->setText(tr("Image type: %1").arg(::GetArchString(pProcess)));

	//m_pMitigation->setText(pProcess->GetMitigationString());
	QString Protection = ::GetProcessProtectionString(pProcess);
	m_Protecetion->setText(tr("Protection: %1").arg(Protection.isEmpty() ? tr("None") : Protection));

	m_pMitigation->GetTree()->clear();
	foreach (const CProcessInfo::SMitigationDetail& Detail, pProcess->GetMitigationDetails())
	{
		const QPair<QString, QString> Text = ::GetMitigationDetail(Detail);

		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		pItem->setText(0, Text.first);
		pItem->setText(1, Text.second);
		m_pMitigation->GetTree()->addTopLevelItem(pItem);
	}

	const quint32 MandatoryPolicy = pProcess->GetMandatoryPolicy();
	m_pNoWriteUp->setChecked((MandatoryPolicy & CProcessInfo::eNoWriteUp) != 0);
	m_pNoReadUp->setChecked((MandatoryPolicy & CProcessInfo::eNoReadUp) != 0);
	m_pNoExecuteUp->setChecked((MandatoryPolicy & CProcessInfo::eNoExecuteUp) != 0);

	if (m_pAppBox)
	{
		m_pAppID->setText(pProcess->GetAppID());
		m_pPackageName->setText(pProcess->GetPackageName());
		//m_pPackageDataDir->setText(pProcess->GetAppDataDirectory());
	}
	m_pEnvironmentView->ShowProcesses(QList<CProcessPtr>() << pProcess);
}

void CProcessView::SyncModel()
{
	QMap<SProcessUID, CProcessPtr> ProcessList;
	foreach(const CProcessPtr& pProcess, m_Processes)
		ProcessList.insert(pProcess->GetProcessUId(), pProcess);
	m_pProcessModel->Sync(QList<QMap<SProcessUID, CProcessPtr> >() << ProcessList);
}

void CProcessView::Refresh()
{
	if (m_Processes.count() > 1)
		SyncModel();
	else if (m_Processes.count() == 1)
	{
		//
		// Kept current while this tab is the one open. Most of these values
		// never change, but the working-set limits and the protection can, and
		// the panel is only refreshed while it is visible.
		//
		QTimer::singleShot(0, m_Processes.first().data(), SLOT(UpdateDetails()));
		ShowProcess(m_Processes.first());
	}

	m_pStatsView->ShowProcesses(m_Processes);
	m_pServiceView->ShowProcesses(m_Processes);
}

void CProcessView::OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);
	CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
	ShowProcess(pProcess);
}

void CProcessView::OnCertificate(const QString& Link)
{
	//
	// A packaged application's signature lives in the package catalog, so the
	// process itself has to open the dialog.
	//
	if (m_Processes.isEmpty())
		return;
	::ShowCertificate(m_Processes.first()->GetSystem().data(), Link);
}

CProcessPtr CProcessView::GetCurrentProcess()
{
	CProcessPtr pProcess;
	if (m_Processes.count() > 1) {
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(m_pProcessList->currentIndex());
		pProcess = m_pProcessModel->GetProcess(ModelIndex);
	}
	else
		pProcess = m_Processes.first();
	return pProcess;
}

void CProcessView::OnPolicy()
{
	CProcessPtr pProcess = GetCurrentProcess();
	if (pProcess.isNull())
		return;

	if (QMessageBox::question(this, "TaskExplorer", tr("Altering the integrity label for a process may produce undesirable results, instability or data corruption."), QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
		return;

	quint32 Policy = pProcess->GetMandatoryPolicy();

	quint32 Bit = 0;
	if (sender() == m_pNoWriteUp)			Bit = CProcessInfo::eNoWriteUp;
	else if (sender() == m_pNoReadUp)		Bit = CProcessInfo::eNoReadUp;
	else if (sender() == m_pNoExecuteUp)	Bit = CProcessInfo::eNoExecuteUp;
	else return;

	Policy ^= Bit;

	STATUS Status = pProcess->SetMandatoryPolicy(Policy);
	if (Status.IsError())
		CTaskExplorer::CheckErrors(QList<STATUS>() << Status);
}

void CProcessView::OnPermissions()
{
	if (CProcessPtr pProcess = GetCurrentProcess())
		CTaskExplorer::ShowSecurity(pProcess->GetSecurityObject(), this);
}