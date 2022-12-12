#include "stdafx.h"
#include "../TaskExplorer.h"
#include "HandlesView.h"
#include "../../../MiscHelpers/Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinHandle.h"
#include "../../API/Windows/ProcessHacker.h"
#include "../../API/Windows/WinMemIO.h"
#include "../../API/Windows/WindowsAPI.h"
#include "../../API/Windows/ProcessHacker/appsup.h"	
#endif
#include "TaskInfoWindow.h"
#include "TokenView.h"
#include "JobView.h"
#include "../MemoryEditor.h"
#include "../../../MiscHelpers/Common/Finder.h"

class CHandleSortModel: public QSortFilterProxyModel
{
public:
    CHandleSortModel(QObject *parent = NULL) : QSortFilterProxyModel(parent) {}

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const
    {
        QString leftData = sourceModel()->data(left).toString();
        QString rightData = sourceModel()->data(right).toString();
        
		if ((leftData.mid(0, 1) == "[") != (rightData.mid(0, 1) == "["))
			return leftData.mid(0, 1) == "[";

        return QString::localeAwareCompare(leftData, rightData) < 0;
    }
};

CHandlesView::CHandlesView(int iAll, QWidget *parent)
	:CPanelView(parent)
{
	m_PendingUpdates = 0;

	m_ShowAllFiles = iAll;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

#ifdef WIN32
	if (m_ShowAllFiles == 0)
	{
		m_pFilterWidget = new QWidget();
		m_pMainLayout->addWidget(m_pFilterWidget);

		m_pFilterLayout = new QHBoxLayout();
		m_pFilterLayout->setContentsMargins(3, 3, 3, 3);
		m_pFilterWidget->setLayout(m_pFilterLayout);

		m_pFilterLayout->addWidget(new QLabel(tr("Types:")));
		m_pShowType = new QComboBox();
		m_pFilterLayout->addWidget(m_pShowType);

		QStandardItemModel* model = new QStandardItemModel(this);

		//m_pShowType->addItem(tr("All"), "");
		//m_pShowType->addItem(tr("All"), -1);
		auto itemAll = new QStandardItem(tr("[All]"));
		itemAll->setData(-1, Qt::UserRole);
		model->appendRow(itemAll);

		POBJECT_TYPES_INFORMATION objectTypes;
		if (NT_SUCCESS(PhEnumObjectTypes(&objectTypes)))
		{
			POBJECT_TYPE_INFORMATION objectType = (POBJECT_TYPE_INFORMATION)PH_FIRST_OBJECT_TYPE(objectTypes);
			for (ULONG i = 0; i < objectTypes->NumberOfTypes; i++)
			{
				QString Type = QString::fromWCharArray(objectType->TypeName.Buffer, objectType->TypeName.Length / sizeof(wchar_t));

				//m_pShowType->addItem(Type, Type);

				int objectIndex;
				if (WindowsVersion >= WINDOWS_8_1)
					objectIndex = objectType->TypeIndex;
				else
					objectIndex = i + 2;

				//m_pShowType->addItem(Type, objectIndex);
				auto item = new QStandardItem(Type);
				item->setData(objectIndex, Qt::UserRole);
				model->appendRow(item);

				objectType = (POBJECT_TYPE_INFORMATION)PH_NEXT_OBJECT_TYPE(objectType);
			}
			PhFree(objectTypes);
		}

		auto proxy1 = new CHandleSortModel();
		proxy1->setSourceModel(model);
		proxy1->sort(0);
		m_pShowType->setModel(proxy1);

		//m_pShowType->setEditable(true); // just in case we forgot a type


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

		m_pShowType->setCurrentIndex(m_pShowType->findText(theConf->GetString("HandleView/ShowType", "")));
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

	m_pSplitter = new QSplitter();
	m_pSplitter->setOrientation(Qt::Vertical);
	m_pMainLayout->addWidget(m_pSplitter);

	// Handle List
	m_pHandleModel = new CHandleModel();
	
	//m_pSortProxy = new CHandleFilterModel(false, this);
	m_pSortProxy = new CSortFilterProxyModel(this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pHandleModel);
	m_pSortProxy->setDynamicSortFilter(true);

	m_pHandleList = new QTreeViewEx();
	m_pHandleList->setItemDelegate(theGUI->GetItemDelegate());

	m_pHandleList->setModel(m_pSortProxy);

	m_pHandleList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pHandleList->setSortingEnabled(true);

	connect(theGUI, SIGNAL(ReloadPanels()), m_pHandleModel, SLOT(Clear()));

	if (m_ShowAllFiles == 0)
		UpdateFilter();
	m_pHandleModel->SetUseIcons(true);

	m_pHandleList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pHandleList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));
	connect(m_pHandleList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked()));

	m_pHandleList->setColumnReset(2);
	connect(m_pHandleList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	connect(m_pHandleList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	m_pSplitter->addWidget(CFinder::AddFinder(m_pHandleList, m_pSortProxy));
	m_pSplitter->setCollapsible(0, false);
	// 

	if (m_ShowAllFiles == 1)
	{
		m_pHandleDetails = NULL;

		connect(theAPI, SIGNAL(OpenFileListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(ShowOpenFiles(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
	}
	else if (m_ShowAllFiles == 3)
	{
		m_pHandleDetails = NULL;
	}
	else
	{
		connect(m_pHandleList, SIGNAL(clicked(const QModelIndex&)), this, SLOT(OnItemSelected(const QModelIndex&)));
		connect(m_pHandleList->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)), this, SLOT(OnItemSelected(QModelIndex)));

		// Handle Details
		m_pHandleDetails = new CPanelWidgetEx();

		m_pHandleDetails->GetView()->setItemDelegate(theGUI->GetItemDelegate());
		((QTreeWidgetEx*)m_pHandleDetails->GetView())->setHeaderLabels(tr("Name|Value").split("|"));

		m_pHandleDetails->GetView()->setSelectionMode(QAbstractItemView::ExtendedSelection);
		m_pHandleDetails->GetView()->setSortingEnabled(false);

		m_pSplitter->addWidget(m_pHandleDetails);
		//m_pSplitter->setCollapsible(1, false);
		//m_pHandleDetails->setVisible(m_pShowDetails->isChecked());
		//

		m_pHandleDetails->GetView()->header()->restoreState(theConf->GetBlob(objectName() + "/HandlesDetail_Columns"));
		m_pSplitter->restoreState(theConf->GetBlob(objectName() + "/HandlesView_Splitter"));
	}

	//if (m_ShowAllFiles != 0)
	if (m_ShowAllFiles == 2)
	{
		m_pHandleList->SetColumnHidden(CHandleModel::ePosition, true, true);
		m_pHandleList->SetColumnHidden(CHandleModel::eSize, true, true);
	}

	if (m_ShowAllFiles == 1 || m_ShowAllFiles == 3)
	{
		m_pHandleList->SetColumnHidden(CHandleModel::eType, true, true);
#ifdef WIN32
		m_pHandleList->SetColumnHidden(CHandleModel::eAttributes, true, true);
		m_pHandleList->SetColumnHidden(CHandleModel::eObjectAddress, true, true);
		m_pHandleList->SetColumnHidden(CHandleModel::eOriginalName, true, true);
#endif
	}

	m_ViewMode = eNone;
	setObjectName(parent->objectName());
	SwitchView(eSingle);

	OnColumnsChanged();

	//m_pMenu = new QMenu();
	m_pOpen = m_pMenu->addAction(tr("Open"), this, SLOT(OnDoubleClicked()));
	
	m_pMenu->addSeparator();

	m_pClose = m_pMenu->addAction(tr("Close"), this, SLOT(OnHandleAction()));
	m_pClose->setShortcut(QKeySequence::Delete);
	m_pClose->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pClose);

#ifdef WIN32
	m_pProtect = m_pMenu->addAction(tr("Protect"), this, SLOT(OnHandleAction()));
	m_pProtect->setCheckable(true);
	m_pInherit = m_pMenu->addAction(tr("Inherit"), this, SLOT(OnHandleAction()));
	m_pInherit->setCheckable(true);

#endif
	
	m_pMenu->addSeparator();

#ifdef WIN32
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
	SwitchView(eNone);
	if (m_pHandleDetails)
	{
		theConf->SetBlob(objectName() + "/HandlesView_Splitter",m_pSplitter->saveState());
		theConf->SetBlob(objectName() + "/HandlesDetail_Columns", m_pHandleDetails->GetView()->header()->saveState());
	}
}

void CHandlesView::SwitchView(EView ViewMode)
{
	QString Name = "Handles";
	if (m_ShowAllFiles == 1)
		Name = "AllFiles";
	else if (m_ShowAllFiles == 2)
		Name = "HandleSearch";
	else if (m_ShowAllFiles == 3)
		Name = "Files";

	switch (m_ViewMode)
	{
		case eSingle:	theConf->SetBlob(objectName() + "/" + Name + "View_Columns", m_pHandleList->saveState()); break;
		case eMulti:	theConf->SetBlob(objectName() + "/" + Name + "MultiView_Columns", m_pHandleList->saveState()); break;
	}

	m_ViewMode = ViewMode;

	QByteArray Columns;
	switch (m_ViewMode)
	{
		case eSingle:	Columns = theConf->GetBlob(objectName() + "/" + Name + "View_Columns"); break;
		case eMulti:	Columns = theConf->GetBlob(objectName() + "/" + Name + "MultiView_Columns"); break;
		default:
			return;
	}
	
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pHandleList->restoreState(Columns);
}

void CHandlesView::OnResetColumns()
{
	for (int i = 0; i < m_pHandleModel->columnCount(); i++)
		m_pHandleList->SetColumnHidden(i, true);

	if (m_ViewMode == eMulti)
		m_pHandleList->SetColumnHidden(CHandleModel::eProcess, false);
	m_pHandleList->SetColumnHidden(CHandleModel::eHandle, false);
	m_pHandleList->SetColumnHidden(CHandleModel::eType, false);
	m_pHandleList->SetColumnHidden(CHandleModel::eName, false);
	if (m_ShowAllFiles == 0 || m_ShowAllFiles == 3)
	{
		m_pHandleList->SetColumnHidden(CHandleModel::ePosition, false);
		m_pHandleList->SetColumnHidden(CHandleModel::eSize, false);
	}
	//m_pHandleList->SetColumnHidden(CHandleModel::eRefs, false);
	m_pHandleList->SetColumnHidden(CHandleModel::eGrantedAccess, false);
#ifdef WIN32
	m_pHandleList->SetColumnHidden(CHandleModel::eFileShareAccess, false);
#endif
}

void CHandlesView::OnColumnsChanged()
{
	if (m_ShowAllFiles == 1)
	{
		// automatically set the setting based on wether the columns are checked or not
		theConf->SetValue("Options/OpenFileGetPosition",
			m_pHandleModel->IsColumnEnabled(CHandleModel::ePosition)
			|| m_pHandleModel->IsColumnEnabled(CHandleModel::eSize)
		);
	}

	m_pHandleModel->Sync(m_Handles);
}

void CHandlesView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	if (m_ShowAllFiles != 0 && m_ShowAllFiles != 3) {
		ASSERT(0);
		return;
	}

	if (m_Processes != Processes)
	{
		disconnect(this, SLOT(ShowHandles(QSet<quint64>, QSet<quint64>, QSet<quint64>)));

		m_Processes = Processes;
		m_PendingUpdates = 0;

		SwitchView(m_Processes.size() > 1 ? eMulti : eSingle);

		foreach(const CProcessPtr& pProcess, m_Processes)
			connect(pProcess.data(), SIGNAL(HandlesUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), this, SLOT(ShowHandles(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
	}

	Refresh();
}

void CHandlesView::Refresh()
{
	if (m_ShowAllFiles == 1)
	{
		theAPI->UpdateOpenFileListAsync();
	}
	else
	{
		if (m_PendingUpdates > 0)
			return;

		m_PendingUpdates = 0;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			m_PendingUpdates++;
			QTimer::singleShot(0, pProcess.data(), SLOT(UpdateHandles()));
		}
	}
}

void CHandlesView::ShowHandles(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	/*if(m_Processes.count() == 1)
	{
		m_PendingUpdates = 0;

		ShowHandles(m_Processes.first()->GetHandleList());
	}
	else*/
	{
		if (--m_PendingUpdates != 0)
			return;

#ifdef WIN32
		int ShowType = g_fileObjectTypeIndex;
#else
        int ShowType = 0;
#endif
		bool HideUnnamed = true;
		bool HideETW = true;

		if (m_ShowAllFiles == 0)
		{
			ShowType = m_pShowType->currentData().toInt();
			HideUnnamed = m_pHideUnnamed->isChecked();
			HideETW = m_pHideETW->isChecked();
		}

		QMap<quint64, CHandlePtr> AllHandles;
		foreach(const CProcessPtr& pProcess, m_Processes) {
			QMap<quint64, CHandlePtr> Handles = pProcess->GetHandleList();
			for (QMap<quint64, CHandlePtr>::iterator I = Handles.begin(); I != Handles.end(); I++)
			{
				const CHandlePtr& pHandle = I.value();
				if (ShowType != -1 && ShowType != pHandle->GetTypeIndex())
					continue;
				if (HideUnnamed && pHandle->GetFileName().isEmpty())
					continue;
#ifdef WIN32
				if(HideETW && g_EtwRegistrationTypeIndex == pHandle->GetTypeIndex())
					continue;
#endif

				ASSERT(!AllHandles.contains(I.key()));
				AllHandles.insert(I.key(), I.value());
			}
		}
		ShowHandles(AllHandles);
	}
}

void CHandlesView::ShowOpenFiles(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	bool bGetDanymicData = theConf->GetBool("Options/OpenFileGetPosition", false);

	m_pHandleModel->SetSizePosNA(!bGetDanymicData);

	ShowHandles(theAPI->GetOpenFilesList());
}

void CHandlesView::ShowHandles(const QMap<quint64, CHandlePtr>& Handles)
{
	m_Handles = Handles;

	m_pHandleModel->Sync(m_Handles);
}

void CHandlesView::UpdateFilter()
{
#ifdef WIN32
    UpdateFilter(m_pShowType->currentText());
#endif
}

void CHandlesView::UpdateFilter(const QString & filter)
{
#ifdef WIN32
	theConf->SetValue("HandleView/ShowType", filter);
	theConf->SetValue("HandleView/HideUnNamed", m_pHideUnnamed->isChecked());
	theConf->SetValue("HandleView/HideETW", m_pHideETW->isChecked());
	//m_pSortProxy->SetFilter(filter, m_pHideUnnamed->isChecked(), m_pHideETW->isChecked());
#endif
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
	QTreeWidgetEx::AddSubItem(pBasicInfo, tr("Object address"), FormatAddress(pWinHandle->GetObjectAddress()));
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
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Owner"), tr("%1 (%2): %3").arg(pThread ? pThread->GetName() : tr("unknown")).arg(HandleInfo.Task.PID).arg(HandleInfo.Task.TID));
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
			Name = tr("%1 (%2): %3").arg(pThread ? pThread->GetName() : tr("unknown")).arg(HandleInfo.Task.PID).arg(HandleInfo.Task.TID);
		}

		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Name"), Name);
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Created"), QDateTime::fromSecsSinceEpoch(HandleInfo.Task.Created).toString("dd.MM.yyyy hh:mm:ss"));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Exited"), QDateTime::fromSecsSinceEpoch(HandleInfo.Task.Exited).toString("dd.MM.yyyy hh:mm:ss"));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("ExitStatus"), QString::number(HandleInfo.Task.ExitStatus));
	}
	else if(TypeName == "Timer")
	{
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Remaining"), QString::number(HandleInfo.Timer.Remaining));
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

	QModelIndexList selectedRows = m_pHandleList->selectedRows();

	m_pClose->setEnabled(!pHandle.isNull());

#ifdef WIN32
	QSharedPointer<CWinHandle> pWinHandle = pHandle.staticCast<CWinHandle>();

	m_pProtect->setEnabled(!pHandle.isNull() && KphCommsIsConnected());
	m_pProtect->setChecked(pWinHandle && pWinHandle->IsProtected());
	m_pInherit->setEnabled(!pHandle.isNull() && KphCommsIsConnected());
	m_pInherit->setChecked(pWinHandle && pWinHandle->IsInherited());

	QString Type = pWinHandle ? pWinHandle->GetTypeName() : "";

	m_pOpen->setVisible(Type == "Token" || Type == "File" || Type == "Mapped file" || Type == "DLL" || Type == "Mapped image" || Type == "Job" || Type == "Process" || Type == "Thread" || Type == "Section");

	m_pSemaphore->menuAction()->setVisible(Type == "Semaphore");
	m_pEvent->menuAction()->setVisible(Type == "Event");
	m_pEventPair->menuAction()->setVisible(Type == "EventPair");
	m_pTimer->menuAction()->setVisible(Type == "Timer");

	m_pTask->menuAction()->setVisible(Type == "Process" || Type == "Thread");

	m_pPermissions->setEnabled(selectedRows.count() == 1);
#endif
	
	CPanelView::OnMenu(point);
}

void CHandlesView::OnHandleAction()
{
	if (sender() == m_pClose)
	{
		if (QMessageBox("TaskExplorer", tr("Do you want to close the selected handle(s)"), QMessageBox::Question, QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
			return;
	}

	QList<STATUS> Errors;
	int Force = -1;
	foreach(const QModelIndex& Index, m_pHandleList->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CHandlePtr pHandle = m_pHandleModel->GetHandle(ModelIndex);
#ifdef WIN32
		QSharedPointer<CWinHandle> pWinHandle = pHandle.staticCast<CWinHandle>();
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
				else if (sender() == m_pTerminate)
					Status = pTask->Terminate(Force == 1);
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
	if (!pHandle)
		return;

	QSharedPointer<CWinHandle> pWinHandle = pHandle.staticCast<CWinHandle>();
	pWinHandle->OpenPermissions();
#endif
}

void CHandlesView::OnDoubleClicked()
{
	QModelIndex Index = m_pHandleList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CHandlePtr pHandle = m_pHandleModel->GetHandle(ModelIndex);
	if (!pHandle)
		return;

	QString Type;
#ifdef WIN32
	CWinHandle* pWinHandle = qobject_cast<CWinHandle*>(pHandle.data());
	Type = pWinHandle->GetTypeName();
#else
	// unix
#endif

#ifdef WIN32
	if (Type == "Token")
	{
		CWinToken* pToken = CWinToken::TokenFromHandle(pHandle->GetProcessId(), pHandle->GetHandleId());
		if (pToken)
		{
			CTokenView* pTokenView = new CTokenView();
			CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow(pTokenView, tr("Token"));
			pTokenView->ShowToken(CWinTokenPtr(pToken));
			pTaskInfoWindow->show();
		}
	}
	else if (Type == "Job")
	{
		CWinJob* pJob = CWinJob::JobFromHandle(pHandle->GetProcessId(), pHandle->GetHandleId());
		if (pJob)
		{
			CJobView* pJobView = new CJobView();
			CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow(pJobView, tr("Job"));
			pJobView->ShowJob(CWinJobPtr(pJob));
			pTaskInfoWindow->show();
		}
	}
	else if (Type == "Section")
	{
		// Read/Write &memory
		CWinMemIO* pDevice = CWinMemIO::FromHandle(pHandle->GetProcessId(), pHandle->GetHandleId());
		if (!pDevice) {
			QMessageBox("TaskExplorer", tr("This memory region can not be edited"), QMessageBox::Warning, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton).exec();
			return;
		}

		CMemoryEditor* pEditor = new CMemoryEditor();
		if(CProcessPtr pProcess = pHandle->GetProcess().staticCast<CProcessInfo>())
			pEditor->setWindowTitle(tr("Memory Editor: %1 (%2)").arg(pProcess->GetName()).arg(pProcess->GetParentId()));
		pEditor->setDevice(pDevice);
		pEditor->show();
	}
	else if (Type == "File" || Type == "Mapped file" || Type == "DLL" || Type == "Mapped image")
	{
		//if(Type == "File") // file properties
		// PhShellProperties(hWnd, Info->BestObjectName->Buffer);
           
		PPH_STRING phFileName = CastQString(pHandle->GetFileName());
		PhShellExecuteUserString(NULL, L"FileBrowseExecutable", phFileName->Buffer, FALSE, L"Make sure the Explorer executable file is present." );
		PhDereferenceObject(phFileName);
	}
	else if (Type == "Key")
	{
		PPH_STRING phRegKey = CastQString(pHandle->GetFileName());
		PhShellOpenKey2(NULL, phRegKey);
		PhDereferenceObject(phRegKey);
	}
	else
#endif
	if (Type == "Process" || Type == "Thread")
	{
		CProcessPtr pProcess = theAPI->GetProcessByID(pHandle->GetProcessId());

		quint64 ThreadId = 0;
#ifdef WIN32
		if (Type == "Thread")
		{
			CWinHandle::SHandleInfo HandleInfo = pWinHandle->GetHandleInfo();
			ThreadId = HandleInfo.Task.TID;
		}
#else
		// unix
#endif

		CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow(QList<CProcessPtr>() << pProcess, ThreadId);
		pTaskInfoWindow->show();
	}
}
