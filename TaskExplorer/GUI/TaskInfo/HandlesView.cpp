#include "stdafx.h"
#include "../TaskExplorer.h"
#include "HandlesView.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinHandle.h"
#endif


CHandlesView::CHandlesView(bool bAll, QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	// todo: files only etc
	/*m_pFilterWidget = new QWidget();
	m_pFilterWidget->setMinimumHeight(32);
	m_pMainLayout->addWidget(m_pFilterWidget);*/

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
	m_pHandleList->setItemDelegate(new CStyledGridItemDelegate(m_pHandleList->fontMetrics().height() + 3, this));

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

	m_pHandleList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pHandleList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	m_pSplitter->addWidget(m_pHandleList);
	m_pSplitter->setCollapsible(0, false);
	// 


	//m_pMenu = new QMenu();
	m_pClose = m_pMenu->addAction(tr("Close"), this, SLOT(OnHandleAction()));
#ifdef WIN32
	m_pProtect = m_pMenu->addAction(tr("Protect"), this, SLOT(OnHandleAction()));
	m_pProtect->setCheckable(true);
	m_pInherit = m_pMenu->addAction(tr("Inherit"), this, SLOT(OnHandleAction()));
	m_pInherit->setCheckable(true);
#endif
	//m_pMenu->addSeparator();
#ifdef WIN32
	// todo: Token:  detail window
	m_pTokenInfo = m_pMenu->addAction(tr("Token Info"), this, SLOT(OnTokenInfo()));

	m_pSemaphore = m_pMenu->addMenu(tr("Semaphore"));
	m_pSemaphoreAcquire = m_pSemaphore->addAction(tr("Acquire"), this, SLOT(OnHandleAction()));
	m_pSemaphoreRelease = m_pSemaphore->addAction(tr("Release"), this, SLOT(OnHandleAction()));

	m_pEvent = m_pMenu->addMenu(tr("Event"));
	m_pEventSet = m_pEvent->addAction(tr("Set"), this, SLOT(OnHandleAction()));
	m_pEventReset = m_pEvent->addAction(tr("Reset"), this, SLOT(OnHandleAction()));
	m_pEventPulse = m_pEvent->addAction(tr("Pulse"), this, SLOT(OnHandleAction()));

	// todo: Thread / Process: goto
	m_pTask = m_pMenu->addMenu(tr("Task"));

	m_pMenu->addSeparator();
	m_pPermissions = m_pMenu->addAction(tr("Permissions"), this, SLOT(OnPermissions()));
#endif

	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	m_pHandleList->header()->restoreState(theConf->GetBlob(objectName() + "/HandlesView_Columns"));
	m_pSplitter->restoreState(theConf->GetBlob(objectName() + "/HandlesView_Splitter"));

	// No details for all files view
	if (bAll)
		return;

	//connect(m_pHandleList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnClicked(const QModelIndex&)));
	connect(m_pHandleList->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));

	m_pDetailsArea = new QScrollArea();
	m_pSplitter->addWidget(m_pDetailsArea);
	QList<int> sizes = m_pSplitter->sizes();
	sizes[0] += sizes[1];
	sizes[1] = 0;
	m_pSplitter->setSizes(sizes);

	m_pDetailsWidget = new QWidget();
	m_pDetailsLayout = new QVBoxLayout();
	m_pDetailsLayout->setMargin(0);
	m_pDetailsWidget->setLayout(m_pDetailsLayout);
	//m_pSplitter->addWidget(m_pDetailsWidget);
	m_pDetailsArea->setWidgetResizable(true);
	m_pDetailsArea->setWidget(m_pDetailsWidget);
	
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


	// other
	m_pOtherWidget = new QWidget();
	m_pOtherLayout = new QFormLayout();
	m_pOtherWidget->setLayout(m_pOtherLayout);
	m_pCustomLayout->addWidget(m_pOtherWidget);


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
}


CHandlesView::~CHandlesView()
{
	theConf->SetBlob(objectName() + "/HandlesView_Columns", m_pHandleList->header()->saveState());
	theConf->SetBlob(objectName() + "/HandlesView_Splitter",m_pSplitter->saveState());
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
	theAPI->UpdateOpenFileListAsync();

	QMap<quint64, CHandlePtr> Handles = theAPI->GetOpenFilesList();

	m_pHandleModel->Sync(Handles);
}

void CHandlesView::OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);

	CHandlePtr pHandle = m_pHandleModel->GetHandle(ModelIndex);
	if (pHandle.isNull())
		return;

#ifdef WIN32
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
	else
	{
		m_pCustomLayout->setCurrentWidget(m_pOtherWidget);
	}
#endif
}


void CHandlesView::OnMenu(const QPoint &point)
{
	QModelIndex Index = m_pHandleList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CHandlePtr pHandle = m_pHandleModel->GetHandle(ModelIndex);

	m_pClose->setEnabled(!pHandle.isNull());

#ifdef WIN32
	QSharedPointer<CWinHandle> pWinHandle = pHandle.objectCast<CWinHandle>();

	m_pProtect->setEnabled(!pHandle.isNull() && theAPI->RootAvaiable());
	m_pProtect->setChecked(pWinHandle && pWinHandle->IsProtected());
	m_pInherit->setEnabled(!pHandle.isNull() && theAPI->RootAvaiable());
	m_pInherit->setChecked(pWinHandle && pWinHandle->IsInherited());

	QString Type = pWinHandle ? pWinHandle->GetTypeString() : "";

	m_pTokenInfo->setVisible(Type == "Token");
	m_pSemaphore->menuAction()->setVisible(Type == "Semaphore");
	m_pEvent->menuAction()->setVisible(Type == "Event");
	m_pTask->menuAction()->setVisible(Type == "Process" || Type == "Thread");

	m_pPermissions->setEnabled(m_pHandleList->selectedRows().count() == 1);
#endif
	
	CPanelView::OnMenu(point);
}

void CHandlesView::OnHandleAction()
{
	if (sender() == m_pClose)
	{
		if (QMessageBox("TaskExplorer", tr("Do you want to close the selected handle(s)"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
			return;
	}

	QList<STATUS> Errors;
	int Force = -1;
	foreach(const QModelIndex& Index, m_pHandleList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CHandlePtr pHandle = m_pHandleModel->GetHandle(ModelIndex);
#ifdef WIN32
		QSharedPointer<CWinHandle> pWinHandle = pHandle.objectCast<CWinHandle>();
#endif
		if (!pHandle.isNull())
		{
			STATUS Status = OK;
retry:
			if (sender() == m_pClose)
				Status = pHandle->Close(Force == 1);
#ifdef WIN32
			else if (sender() == m_pProtect)
				Status = pWinHandle->SetProtected(m_pProtect->isChecked());
			else if (sender() == m_pInherit)
				Status = pWinHandle->SetInherited(m_pInherit->isChecked());
			else 

			if (sender() == m_pSemaphoreAcquire)
				Status = pWinHandle->DoHandleAction(CWinHandle::eSemaphoreAcquire);
			else if (sender() == m_pSemaphoreRelease)
				Status = pWinHandle->DoHandleAction(CWinHandle::eSemaphoreRelease);
			else 

			if (sender() == m_pEventSet)
				Status = pWinHandle->DoHandleAction(CWinHandle::eEventSet);
			else if (sender() == m_pEventReset)
				Status = pWinHandle->DoHandleAction(CWinHandle::eEventReset);
			else if (sender() == m_pEventPulse)
				Status = pWinHandle->DoHandleAction(CWinHandle::eEventPulse);
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


void CHandlesView::OnPermissions()
{
#ifdef WIN32
	QModelIndex Index = m_pHandleList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CHandlePtr pHandle = m_pHandleModel->GetHandle(ModelIndex);

	QSharedPointer<CWinHandle> pWinHandle = pHandle.objectCast<CWinHandle>();
	pWinHandle->OpenPermissions();
#endif
}