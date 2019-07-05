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

#ifdef WIN32
	if (!bAll)
	{
		m_pFilterWidget = new QWidget();
		m_pMainLayout->addWidget(m_pFilterWidget);

		m_pFilterLayout = new QHBoxLayout();
		m_pFilterLayout->setMargin(3);
		m_pFilterWidget->setLayout(m_pFilterLayout);

		m_pFilterLayout->addWidget(new QLabel(tr("Types:")));
		m_pShowType = new QComboBox();
		m_pFilterLayout->addWidget(m_pShowType);

		m_pShowType->addItem(tr("All"), "");
		m_pShowType->addItem(tr("File"), "File");
		m_pShowType->addItem(tr("Keys"), "Key");
		m_pShowType->addItem(tr("ALPC Ports"), "ALPC Port");
		m_pShowType->addItem(tr("Sections"), "Section");
		m_pShowType->addItem(tr("Mutants"), "Mutant");
		m_pShowType->addItem(tr("Processes"), "Process");
		m_pShowType->addItem(tr("Threads"), "Thread");
		m_pShowType->addItem(tr("Tokens"), "Token");
		m_pShowType->addItem(tr("Desktops"), "Desktop");
		m_pShowType->addItem(tr("Events"), "Event");
		m_pShowType->addItem(tr("Event Pairs"), "EventPair");
		m_pShowType->addItem(tr("Jobs"), "Job");
		m_pShowType->addItem(tr("Semaphores"), "Semaphore");
		m_pShowType->addItem(tr("Timers"), "Timer");
		m_pShowType->addItem(tr("Worker Factories"), "TpWorkerFactory");
		m_pShowType->setEditable(true); // just in case we forgot a type

		m_pHideUnnamed = new QCheckBox(tr("Hide Unnamed"));
		m_pFilterLayout->addWidget(m_pHideUnnamed);
		
		m_pHideETW = new QCheckBox(tr("Hide ETW"));
		m_pFilterLayout->addWidget(m_pHideETW);
		
		//m_pShowDetails = new QPushButton(tr("Details"));
		//m_pShowDetails->setCheckable(true);
		////m_pShowDetails->setStyleSheet("QPushButton {color:white;} QPushButton:checked{background-color: rgb(80, 80, 80); border: none;} QPushButton:hover{background-color: grey; border-style: outset;}");
		////m_pShowDetails->setStyleSheet("QPushButton:checked{color:white; background-color: rgb(80, 80, 80); border-style: outset;}");
//#ifdef WIN32
		//QStyle* pStyle = QStyleFactory::create("fusion");
		//m_pShowDetails->setStyle(pStyle);
//#endif
		//m_pFilterLayout->addWidget(m_pShowDetails);

		m_pShowType->setCurrentIndex(m_pShowType->findData(theConf->GetString("HandleView/ShowType", "")));
		m_pHideUnnamed->setChecked(theConf->GetBool("HandleView/HideUnNamed", false));
		m_pHideETW->setChecked(theConf->GetBool("HandleView/HideETW", true));
		//m_pShowDetails->setChecked(theConf->GetBool("HandleView/ShowDetails", false));

		connect(m_pShowType, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
		connect(m_pShowType, SIGNAL(editTextChanged(const QString &)), this, SLOT(UpdateFilter(const QString &)));
		connect(m_pHideUnnamed, SIGNAL(stateChanged(int)), this, SLOT(UpdateFilter()));
		connect(m_pHideETW, SIGNAL(stateChanged(int)), this, SLOT(UpdateFilter()));
		//connect(m_pShowDetails, SIGNAL(toggled(bool)), this, SLOT(OnShowDetails()));

		m_pFilterLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
	}
#endif

	// Handle List
	m_pHandleModel = new CHandleModel();
	
	m_pSortProxy = new CHandleFilterModel(false, this);
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
		UpdateFilter();

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

	if (!bAll)
	{
		connect(m_pHandleList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnItemSelected(const QModelIndex&)));
		connect(m_pHandleList->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)), this, SLOT(OnItemSelected(QModelIndex)));

		// Handle Details
		m_pHandleDetails = new CPanelWidget<QTreeWidgetEx>();

		m_pHandleDetails->GetView()->setItemDelegate(new CStyledGridItemDelegate(m_pHandleDetails->fontMetrics().height() + 3, this));
		((QTreeWidgetEx*)m_pHandleDetails->GetView())->setHeaderLabels(tr("Name|Value").split("|"));

		m_pHandleDetails->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
		m_pHandleDetails->GetView()->setSortingEnabled(false);

		m_pSplitter->addWidget(m_pHandleDetails);
		//m_pSplitter->setCollapsible(1, false);
		//m_pHandleDetails->setVisible(m_pShowDetails->isChecked());
		//

		m_pHandleDetails->GetView()->header()->restoreState(theConf->GetBlob(objectName() + "/HandlesDetail_Columns"));
	}
	else
		m_pHandleDetails = NULL;

	setObjectName(parent->objectName());
	m_pHandleList->header()->restoreState(theConf->GetBlob(objectName() + "/HandlesView_Columns"));
	m_pSplitter->restoreState(theConf->GetBlob(objectName() + "/HandlesView_Splitter"));



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
	
#ifdef _DEBUG
	// todo: Token:  detail window // todo: xxx
	m_pTokenInfo = m_pMenu->addAction(tr("Token Info"), this, SLOT(OnTokenInfo()));
	m_pJobInfo = m_pMenu->addAction(tr("Job Info"), this, SLOT(OnJobInfo()));
#endif

	m_pSemaphore = m_pMenu->addMenu(tr("Semaphore"));
		m_pSemaphoreAcquire = m_pSemaphore->addAction(tr("Acquire"), this, SLOT(OnHandleAction()));
		m_pSemaphoreRelease = m_pSemaphore->addAction(tr("Release"), this, SLOT(OnHandleAction()));

	m_pEvent = m_pMenu->addMenu(tr("Event"));
		m_pEventSet = m_pEvent->addAction(tr("Set"), this, SLOT(OnHandleAction()));
		m_pEventReset = m_pEvent->addAction(tr("Reset"), this, SLOT(OnHandleAction()));
		m_pEventPulse = m_pEvent->addAction(tr("Pulse"), this, SLOT(OnHandleAction()));

	m_pEventPair = m_pMenu->addMenu(tr("Event Pair"));
		m_pEventSetLow = m_pEventPair->addAction(tr("Set Low"), this, SLOT(OnHandleAction()));
		m_pEventSetHigh = m_pEventPair->addAction(tr("Set High"), this, SLOT(OnHandleAction()));

	m_pTimer = m_pMenu->addMenu(tr("Timer"));
	m_pTimerCancel = m_pTimer->addAction(tr("Cancel"), this, SLOT(OnHandleAction()));

	m_pTask = m_pMenu->addMenu(tr("Task"));
		m_pTerminate = m_pTask->addAction(tr("Terminate"), this, SLOT(OnHandleAction()));
		m_pSuspend = m_pTask->addAction(tr("Suspend"), this, SLOT(OnHandleAction()));
		m_pResume = m_pTask->addAction(tr("Resume"), this, SLOT(OnHandleAction()));
		// todo: goto thread/process

	m_pMenu->addSeparator();
	m_pPermissions = m_pMenu->addAction(tr("Permissions"), this, SLOT(OnPermissions()));
#endif

	AddPanelItemsToMenu();
}


CHandlesView::~CHandlesView()
{
	theConf->SetBlob(objectName() + "/HandlesView_Columns", m_pHandleList->header()->saveState());
	theConf->SetBlob(objectName() + "/HandlesView_Splitter",m_pSplitter->saveState());
	if(m_pHandleDetails)
		theConf->SetBlob(objectName() + "/HandlesDetail_Columns", m_pHandleDetails->GetView()->header()->saveState());
}

void CHandlesView::ShowHandles(const CProcessPtr& pProcess)
{
	if (m_pCurProcess != pProcess)
	{
		disconnect(this, SLOT(ShowHandles(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

		m_pCurProcess = pProcess;

		connect(m_pCurProcess.data(), SIGNAL(HandlesUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(ShowHandles(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
	}

	QTimer::singleShot(0, m_pCurProcess.data(), SLOT(UpdateHandles()));
}

void CHandlesView::ShowHandles(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	QMap<quint64, CHandlePtr> Handles = m_pCurProcess->GetHandleList();

	m_pHandleModel->Sync(Handles);
}

void CHandlesView::ShowAllFiles()
{
	theAPI->UpdateOpenFileListAsync();

	QMap<quint64, CHandlePtr> Handles = theAPI->GetOpenFilesList();

	m_pHandleModel->Sync(Handles);
}

void CHandlesView::UpdateFilter()
{
	UpdateFilter(m_pShowType->currentData().toString());
}

void CHandlesView::UpdateFilter(const QString & filter)
{
	theConf->SetValue("HandleView/ShowType", filter);
	theConf->SetValue("HandleView/HideUnNamed", m_pHideUnnamed->isChecked());
	theConf->SetValue("HandleView/HideETW", m_pHideETW->isChecked());
	m_pSortProxy->SetFilter(filter, m_pHideUnnamed->isChecked(), m_pHideETW->isChecked());
}

//void CHandlesView::OnShowDetails()
//{
//	theConf->SetValue("HandleView/ShowDetails", m_pShowDetails->isChecked());
//	m_pHandleDetails->setVisible(m_pShowDetails->isChecked());
//}

void CHandlesView::OnItemSelected(const QModelIndex &current)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);

	CHandlePtr pHandle = m_pHandleModel->GetHandle(ModelIndex);
	if (pHandle.isNull())
		return;

	QTreeWidget* pDetails = (QTreeWidget*)m_pHandleDetails->GetView();
	// Note: we don't auto refresh this infos
	pDetails->clear();

#ifdef WIN32
	CWinHandle* pWinHandle = qobject_cast<CWinHandle*>(pHandle.data());

	QString TypeName = pWinHandle->GetTypeName();
	CWinHandle::SHandleInfo HandleInfo = pWinHandle->GetHandleInfo();
	
	QTreeWidgetItem* pBasicInfo = new QTreeWidgetItem(QStringList(tr("Basic informations")));
	pDetails->addTopLevelItem(pBasicInfo);

	QTreeWidgetEx::AddSubItem(pBasicInfo, tr("Name"), pWinHandle->GetOriginalName());
	QTreeWidgetEx::AddSubItem(pBasicInfo, tr("Type"), pWinHandle->GetTypeString());
	QTreeWidgetEx::AddSubItem(pBasicInfo, tr("Object address"), "0x" + QString::number(pWinHandle->GetObjectAddress(), 16));
	QTreeWidgetEx::AddSubItem(pBasicInfo, tr("Granted access"), pWinHandle->GetGrantedAccessString());


	QTreeWidgetItem* pReferences = new QTreeWidgetItem(QStringList(tr("References")));
	pDetails->addTopLevelItem(pReferences);

	QTreeWidgetEx::AddSubItem(pReferences, tr("Ref. count"), QString::number(HandleInfo.References));
	QTreeWidgetEx::AddSubItem(pReferences, tr("Handles"), QString::number(HandleInfo.Handles));


	QTreeWidgetItem* pQuota = new QTreeWidgetItem(QStringList(tr("Quota charges")));
	pDetails->addTopLevelItem(pQuota);

	QTreeWidgetEx::AddSubItem(pQuota, tr("Paged"), QString::number(HandleInfo.Paged));
	QTreeWidgetEx::AddSubItem(pQuota, tr("Virtual Size"), QString::number(HandleInfo.VirtualSize));


	QTreeWidgetItem* pExtendedInfo = new QTreeWidgetItem(QStringList(tr("Extended informations")));
	pDetails->addTopLevelItem(pExtendedInfo);

	if(TypeName == "ALPC Port")
	{
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Sequence number"), QString::number(HandleInfo.Port.SeqNumber));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Port context"), QString::number(HandleInfo.Port.Context));
	}
	else if(TypeName == "File")
	{
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Is directory"), HandleInfo.File.IsDir ? tr("True") : tr("False"));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("File mode"), CWinHandle::GetFileAccessMode(HandleInfo.File.Mode));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("File size"), FormatSize(HandleInfo.File.Size));
	}
	else if(TypeName == "Section")
	{
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Section type"), CWinHandle::GetSectionType(HandleInfo.Section.Attribs));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Size"), FormatSize(HandleInfo.Section.Size));
	}
	else if(TypeName == "Mutant")
	{
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Count"), CWinHandle::GetFileAccessMode(HandleInfo.Mutant.Count));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Abandoned"), HandleInfo.Mutant.Abandoned ? tr("True") : tr("False"));
		CThreadPtr pThread = theAPI->GetThreadByID(HandleInfo.Task.TID);
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Owner"), tr("%1 (%2): %3").arg(QString(pThread ? pThread->GetName() : tr("unknown"))).arg(HandleInfo.Task.PID).arg(HandleInfo.Task.TID));
	}
	else if(TypeName == "Process" || TypeName == "Thread")
	{
		QString Name;
		if (TypeName == "Process")
		{
			CProcessPtr pProcess = theAPI->GetProcessByID(HandleInfo.Task.PID);
			Name = tr("%1 (%2)").arg(QString(pProcess ? pProcess->GetName() : tr("unknown"))).arg(HandleInfo.Task.PID);
		}
		else
		{
			CThreadPtr pThread = theAPI->GetThreadByID(HandleInfo.Task.TID);
			Name = tr("%1 (%2): %3").arg(QString(pThread ? pThread->GetName() : tr("unknown"))).arg(HandleInfo.Task.PID).arg(HandleInfo.Task.TID);
		}

		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Name"), Name);
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Created"), QDateTime::fromTime_t(HandleInfo.Task.Created).toString("dd.MM.yyyy hh:mm:ss"));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Exited"), QDateTime::fromTime_t(HandleInfo.Task.Exited).toString("dd.MM.yyyy hh:mm:ss"));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("ExitStatus"), QString::number(HandleInfo.Task.ExitStatus));
	}
	else if(TypeName == "Timer")
	{
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Remaining"), QString::number(HandleInfo.Timer.Remaining)); // todo
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Signaled"), HandleInfo.Timer.Signaled ? tr("True") : tr("False"));
	}
	else
		delete pExtendedInfo;

	pDetails->expandAll();
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

	QString Type = pWinHandle ? pWinHandle->GetTypeName() : "";

#ifdef _DEBUG
	// todo:
	m_pTokenInfo->setVisible(Type == "Token");
	m_pJobInfo->setVisible(Type == "Job");
#endif

	m_pSemaphore->menuAction()->setVisible(Type == "Semaphore");
	m_pEvent->menuAction()->setVisible(Type == "Event");
	m_pEventPair->menuAction()->setVisible(Type == "EventPair");
	m_pTimer->menuAction()->setVisible(Type == "Timer");

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
			else

			if (sender() == m_pEventSetLow)
				Status = pWinHandle->DoHandleAction(CWinHandle::eSetLow);
			else if (sender() == m_pEventSetHigh)
				Status = pWinHandle->DoHandleAction(CWinHandle::eSetHigh);
			else
				
			if (sender() == m_pTimerCancel)
				Status = pWinHandle->DoHandleAction(CWinHandle::eCancelTimer);
			else 

			if (sender() == m_pTerminate || sender() == m_pSuspend || sender() == m_pResume)
			{
				QString TypeName = pWinHandle->GetTypeName();
				CWinHandle::SHandleInfo HandleInfo = pWinHandle->GetHandleInfo();

				QSharedPointer<CAbstractTask> pTask;
				if (TypeName == "Thread")
					pTask = theAPI->GetThreadByID(HandleInfo.Task.TID);
				else if (TypeName == "Process")
					pTask = theAPI->GetProcessByID(HandleInfo.Task.PID);

				if (!pTask)
					Status = ERR("Not Found");
				if (sender() == m_pTerminate)
					Status = pTask->Terminate();
				else if (sender() == m_pSuspend)
					Status = pTask->Suspend();
				else if (sender() == m_pResume)
					Status = pTask->Resume();
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