#include "stdafx.h"
#include "../TaskExplorer.h"
#include "HandlesView.h"
#include "../../Common/Common.h"
#include "..\Models\HandleModel.h"
#include "..\Models\SortFilterProxyModel.h"
#ifdef WIN32
#include "../../API/Windows/WinHandle.h"
#endif


CHandlesView::CHandlesView(bool bAll)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	m_pFilterWidget = new QWidget();
	m_pFilterWidget->setMinimumHeight(32);
	m_pMainLayout->addWidget(m_pFilterWidget);

	// Handle List
	m_pHandleModel = new CHandleModel();
	
	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pHandleModel);
	m_pSortProxy->setDynamicSortFilter(true);

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);
	m_pMainLayout->addWidget(m_pSplitter);

	m_pHandleList = new QTreeViewEx();
	m_pHandleList->setItemDelegate(new QStyledItemDelegateMaxH(m_pHandleList->fontMetrics().height() + 3, this));

	m_pHandleList->setModel(m_pSortProxy);

	m_pHandleList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pHandleList->setSortingEnabled(true);

	if (!bAll)
	{
		m_pHandleList->setColumnFixed(CHandleModel::eProcess, true);
		m_pHandleList->setColumnHidden(CHandleModel::eProcess, true);
	}
	else
	{
		m_pHandleList->setColumnFixed(CHandleModel::eType, true);
		m_pHandleList->setColumnHidden(CHandleModel::eType, true);
		m_pHandleList->setColumnFixed(CHandleModel::ePosition, true);
		m_pHandleList->setColumnHidden(CHandleModel::ePosition, true);
		m_pHandleList->setColumnFixed(CHandleModel::eSize, true);
		m_pHandleList->setColumnHidden(CHandleModel::eSize, true);
#ifdef WIN32
		m_pHandleList->setColumnFixed(CHandleModel::eAttributes, true);
		m_pHandleList->setColumnHidden(CHandleModel::eAttributes, true);
		m_pHandleList->setColumnFixed(CHandleModel::eOriginalName, true);
		m_pHandleList->setColumnHidden(CHandleModel::eOriginalName, true);
#endif
	}

	connect(m_pHandleList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));

	m_pSplitter->addWidget(m_pHandleList);
	m_pSplitter->setCollapsible(0, false);
	// 

	// No details for all files view
	if (bAll)
		return;

	m_pDetailsWidget = new QWidget();
	m_pDetailsLayout = new QHBoxLayout();
	m_pDetailsLayout->setMargin(0);
	m_pDetailsWidget->setLayout(m_pDetailsLayout);
	
	// Generic
	m_pGenericWidget = new QWidget();
	m_pGenericLayout = new QFormLayout();
	m_pGenericWidget->setLayout(m_pGenericLayout);
	m_pDetailsLayout->addWidget(m_pGenericWidget);

	m_pName = new QLabel();
	m_pName->setWordWrap(true);
	m_pName->setMaximumWidth(300);
	m_pGenericLayout->addRow(tr("Name"), m_pName);

	m_pType = new QLabel();
	m_pGenericLayout->addRow(tr("Type"), m_pType);

	m_pReferences = new QLabel();
	m_pGenericLayout->addRow(tr("References"), m_pReferences);

	m_pHandles = new QLabel();
	m_pGenericLayout->addRow(tr("Handles"), m_pHandles);

	m_pQuotaPaged = new QLabel();
	m_pGenericLayout->addRow(tr("Quota Paged"), m_pQuotaPaged);

	m_pQuotaSize = new QLabel();
	m_pGenericLayout->addRow(tr("Quota Virtual Size"), m_pQuotaSize);

	m_pGrantedAccess = new QLabel();
	m_pGenericLayout->addRow(tr("Granted Access"), m_pGrantedAccess);
	//

	m_pCustomWidget = new QWidget();
	m_pCustomLayout = new QStackedLayout();
	m_pCustomWidget->setLayout(m_pCustomLayout);
	m_pDetailsLayout->addWidget(m_pCustomWidget);
	m_pSplitter->addWidget(m_pDetailsWidget);


	// Port
	m_pPortWidget = new QWidget();
	m_pPortLayout = new QFormLayout();
	m_pPortWidget->setLayout(m_pPortLayout);
	m_pCustomLayout->addWidget(m_pPortWidget);

	m_pPortNumber = new QLabel();
	m_pPortLayout->addRow(tr("Sequence Number"), m_pPortNumber);


	m_pPortContext = new QLabel();
	m_pPortLayout->addRow(tr("Port Context"), m_pPortContext);
	//

	// File
	m_pFileWidget = new QWidget();
	m_pFileLayout = new QFormLayout();
	m_pFileWidget->setLayout(m_pFileLayout);
	m_pCustomLayout->addWidget(m_pFileWidget);

	m_pFileMode = new QLabel();
	m_pFileLayout->addRow(tr("File Mode"), m_pFileMode);
	//

	// Section
	m_pSectionWidget = new QWidget();
	m_pSectionLayout = new QFormLayout();
	m_pSectionWidget->setLayout(m_pSectionLayout);
	m_pCustomLayout->addWidget(m_pSectionWidget);

	m_pSectionType = new QLabel();
	m_pSectionLayout->addRow(tr("Section Type"), m_pSectionType);
	//

	// Mutant
	m_pMutantWidget = new QWidget();
	m_pMutantLayout = new QFormLayout();
	m_pMutantWidget->setLayout(m_pMutantLayout);
	m_pCustomLayout->addWidget(m_pMutantWidget);

	m_pMutantCount = new QLabel();
	m_pMutantLayout->addRow(tr("Count"), m_pMutantCount);

	m_pMutantAbandoned = new QLabel();
	m_pMutantLayout->addRow(tr("Abandoned"), m_pMutantAbandoned);

	m_pMutantOwner = new QLabel();
	m_pMutantLayout->addRow(tr("Owner"), m_pMutantOwner);

	//

	// Task
	m_pTaskWidget = new QWidget();
	m_pTaskLayout = new QFormLayout();
	m_pTaskWidget->setLayout(m_pTaskLayout);
	m_pCustomLayout->addWidget(m_pTaskWidget);

	m_pTaskCreated = new QLabel();
	m_pTaskLayout->addRow(tr("Created"), m_pTaskCreated);

	m_pTaskExited = new QLabel();
	m_pTaskLayout->addRow(tr("Exited"), m_pTaskExited);

	m_pTaskStatus = new QLabel();
	m_pTaskLayout->addRow(tr("Exit Status"), m_pTaskStatus);
	//

	/*

all: close, protect, inherit

Thread / Process: goto sub menu
		
Semaphore: Acquire/Release sub menu
		
Event: Set/Reset/Pulse sub menu
				
Token:  detail window

	*/
}


CHandlesView::~CHandlesView()
{
}

void CHandlesView::ShowHandles(const CProcessPtr& pProcess)
{
	m_pCurProcess = pProcess;

	m_pCurProcess->UpdateHandles();

	QMap<quint64, CHandlePtr> Handles = m_pCurProcess->GetHandleList();

	m_pHandleModel->Sync(Handles);
}

void CHandlesView::ShowAllFiles()
{
	theAPI->UpdateOpenFileList();

	QMap<quint64, CHandlePtr> Handles = theAPI->GetOpenFilesList();

	m_pHandleModel->Sync(Handles);
}

void CHandlesView::OnClicked(const QModelIndex& Index)
{
	CHandlePtr pHandle = m_pHandleModel->GetHandle(m_pSortProxy->mapToSource(Index));

	CWinHandle* pWinHandle = qobject_cast<CWinHandle*>(pHandle.data());
	
	m_pName->setText(pWinHandle->GetFileName());
	m_pType->setText(pWinHandle->GetTypeString());

	QVariantMap Details = pWinHandle->GetDetailedInfos();

	QVariantMap Refs = Details["References"].toMap();
	m_pReferences->setText(Refs["References"].toString());
	m_pHandles->setText(Refs["Handles"].toString());

	QVariantMap Quota = Details["Quota"].toMap();
	m_pQuotaPaged->setText(Quota["Paged"].toString());
	m_pQuotaSize->setText(Quota["VirtualSize"].toString());

	m_pGrantedAccess->setText(pWinHandle->GetGrantedAccessString());


	QString Type = pWinHandle->GetTypeString();

	if (Type == "ALPC Port")
    {
		m_pCustomLayout->setCurrentWidget(m_pPortWidget);
		QVariantMap Port = Details["ALPC_Port"].toMap();

		m_pPortNumber->setText(Port["SeqNumber"].toString());
		m_pPortContext->setText(Port["Context"].toString());
    }
    else if (Type == "File")
    {
		m_pCustomLayout->setCurrentWidget(m_pFileWidget);
		QVariantMap File = Details["FileInfo"].toMap();
		
		m_pFileMode->setText(File["Mode"].toString());
    }
    else if (Type == "Section")
    {
		m_pCustomLayout->setCurrentWidget(m_pSectionWidget);
		QVariantMap Section = Details["SectionInfo"].toMap();
		
		m_pSectionType->setText(Section["Type"].toString());
    }
    else if (Type == "Mutant")
    {
		m_pCustomLayout->setCurrentWidget(m_pMutantWidget);
		QVariantMap Mutant = Details["MutantInfo"].toMap();

		m_pMutantCount->setText(Mutant["Count"].toString());
		m_pMutantAbandoned->setText(Mutant["Abandoned"].toString());
		m_pMutantOwner->setText(Mutant["Owner"].toString());
    }
    else if (Type == "Process" || Type == "Thread")
    {
		m_pCustomLayout->setCurrentWidget(m_pTaskWidget);
		QVariantMap Task = (Type == "Thread") ? Details["ThreadInfo"].toMap() : Details["ProcessInfo"].toMap();

		m_pTaskCreated->setText(Task["Created"].toString());
		m_pTaskExited->setText(Task["Exited"].toString());
		m_pTaskStatus->setText(Task["ExitStatus"].toString());
    }
}