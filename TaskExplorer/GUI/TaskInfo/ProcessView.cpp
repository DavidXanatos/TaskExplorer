#include "stdafx.h"
#include "ProcessView.h"
#include "../TaskExplorer.h"
#ifdef WIN32
#include "../../API/Windows/WinProcess.h"
#endif


CProcessView::CProcessView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pScrollArea = new QScrollArea();
	m_pMainLayout->addWidget(m_pScrollArea);

	QWidget* m_pInfoWidget = new QWidget();
	m_pScrollArea->setWidgetResizable(true);
	m_pScrollArea->setWidget(m_pInfoWidget);

	m_pInfoLayout = new QVBoxLayout();
	m_pInfoWidget->setLayout(m_pInfoLayout);

	m_pFileBox = new QGroupBox(tr("File"));
	m_pInfoLayout->addWidget(m_pFileBox);

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

	m_pFileLayout->addWidget(new QLabel(tr("Image file name:")), row++, 0, 1, 2);
	m_pFilePath = new QLineEdit();
	m_pFilePath->setReadOnly(true);
	m_pFileLayout->addWidget(m_pFilePath, row++, 0, 1, 2);


	m_pProcessBox = new QGroupBox(tr("Process"));
	//m_pProcessBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pInfoLayout->addWidget(m_pProcessBox);

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

	m_pProcessLayout->addWidget(new QLabel(tr("User name:")), row, 0);
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


#ifdef WIN32
	m_pProcessLayout->addWidget(new QLabel(tr("PEB address:")), row, 0);
	m_pPEBAddress = new QLineEdit();
	m_pPEBAddress->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pPEBAddress, row, 1, 1, 1);
	m_ImageType = new QLabel();
	m_ImageType->setMinimumWidth(100);
	m_pProcessLayout->addWidget(m_ImageType, row++, 2);

	m_pProcessLayout->addWidget(new QLabel(tr("Mitigation policies:")), row, 0);
	m_pMitigation = new QLineEdit();
	m_pMitigation->setReadOnly(true);
	m_pProcessLayout->addWidget(m_pMitigation, row, 1, 1, 1);
	m_Protecetion = new QLabel();
	m_pProcessLayout->addWidget(m_Protecetion, row++, 2);
#endif

	/*m_pStatsList = new QTreeWidgetEx();

	m_pStatsList->setItemDelegate(theGUI->GetItemDelegate());
	m_pStatsList->setHeaderLabels(tr("Name|Value").split("|"));
	m_pStatsList->header()->hide();

	m_pStatsList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pStatsList->setSortingEnabled(false);

	for (int i = 0; i < 100; i++) {
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		//pItem->setData(0, Qt::UserRole, i);
		pItem->setText(0, QString::number(i));
		m_pStatsList->addTopLevelItem(pItem);
	}

	m_pStatsList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pStatsList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	m_pInfoLayout->addWidget(m_pStatsList);*/

	m_pStatsView = new CStatsView(CStatsView::eProcess, this);
	m_pStatsView->setSizePolicy(m_pStatsView->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);
	m_pInfoLayout->addWidget(m_pStatsView);

	/*QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pProcessLayout->addWidget(pSpacer, row, 1);*/
	//m_pInfoLayout->addWidget(pSpacer);
}


CProcessView::~CProcessView()
{
}

void CProcessView::ShowProcess(const CProcessPtr& pProcess)
{
	if (m_pCurProcess != pProcess)
	{
		m_pCurProcess = pProcess;

		CModulePtr pModule = m_pCurProcess->GetModuleInfo();

		QPixmap Icon = pModule->GetFileIcon(true);
		m_pIcon->setPixmap(Icon.isNull() ? g_ExeIcon.pixmap(32) : Icon);
		QString Description = pModule->GetFileInfo("Description");
		if (!Description.isEmpty())
			m_pProcessName->setText(Description + " (" + m_pCurProcess->GetName() + ")");
		else
			m_pProcessName->setText(m_pCurProcess->GetName());

		m_pCompanyName->setText(pModule->GetFileInfo("CompanyName"));
		m_pProcessVersion->setText(pModule->GetFileInfo("FileVersion"));
		m_pFilePath->setText(m_pCurProcess->GetFileName());

		m_pCmdLine->setText(m_pCurProcess->GetCommandLine());
		m_pCurDir->setText(m_pCurProcess->GetWorkingDirectory());
		m_pProcessId->setText(tr("%1/%2").arg(m_pCurProcess->GetProcessId()).arg(m_pCurProcess->GetParentId()));
		m_pUserName->setText(m_pCurProcess->GetUserName());


		CProcessPtr pParent = theAPI->GetProcessByID(m_pCurProcess->GetParentId());
		if (!pProcess->ValidateParent(pParent.data()))
			pParent.clear();
		m_pStartedBy->setText(pParent.isNull() ? tr("N/A") : pParent->GetFileName());


#ifdef WIN32
		CWinProcess* pWinProc = qobject_cast<CWinProcess*>(pProcess.data());

		if (pWinProc->IsWow64())
			m_pPEBAddress->setText(tr("0x%1 (32-bit: 0x%2)").arg(pWinProc->GetPebBaseAddress(), 8, 16, QChar('0')).arg(pWinProc->GetPebBaseAddress(true), 8, 16, QChar('0')));
		else
			m_pPEBAddress->setText(tr("0x%1").arg(pWinProc->GetPebBaseAddress(), 8, 16, QChar('0')));
		m_ImageType->setText(tr("Image type: %1").arg(m_pCurProcess->GetArchString()));

		m_pMitigation->setText(pWinProc->GetMitigationString());
		m_Protecetion->setText(tr("Protection: %1").arg(pWinProc->GetProtectionString()));
#endif
	}

	Refresh();
}

void CProcessView::Refresh()
{
	if (!m_pCurProcess)
		return;

	m_pStatsView->ShowProcess(m_pCurProcess);
}