#include "stdafx.h"
#include "../../API/Cluster.h"
#include "../TaskExplorer.h"
#include "../DesktopActions.h"
#include "../TaskStrings.h"
#include "HandlesView.h"
#include "../../../MiscHelpers/Common/Common.h"
#include "../../API/SystemAPI.h"
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

	// These are only created on some paths below; null them so a stray use is a
	// null dereference rather than a jump through uninitialised memory.
	m_pFilterWidget = nullptr;
	m_pFilterLayout = nullptr;
	m_pShowType = nullptr;
	m_pHideUnnamed = nullptr;
	m_pHideETW = nullptr;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	//
	// The type filter is built from whatever kinds of object the target
	// reports. A system with no such classification returns an empty list and
	// the whole filter row stays hidden.
	//
	//
	// Built for the per-process view whether or not the machine in front of us
	// right now reports any types: which machine that is changes, and the row
	// is filled in - and hidden when empty - by RebuildTypeFilter.
	//
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

		//
		// One model for the life of the view, refilled when the machine changes
		// rather than replaced. Swapping a combo's model means deciding who owns
		// the one being dropped - and the first one is the combo's own, which
		// must not be deleted from under it.
		//
		m_pTypeModel = new QStandardItemModel(this);
		CHandleSortModel* pTypeProxy = new CHandleSortModel();
		pTypeProxy->setParent(this);
		pTypeProxy->setSourceModel(m_pTypeModel);
		pTypeProxy->sort(0);
		m_pShowType->setModel(pTypeProxy);



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
	{
		RebuildTypeFilter();
		UpdateFilter();
	}
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

		{
		new CViewSystemLink(this, SIGNAL(OpenFileListUpdated(QSet<quint64>, QSet<quint64>, QSet<quint64>)), SLOT(ShowOpenFiles(QSet<quint64>, QSet<quint64>, QSet<quint64>)));
		connect(theGUI, SIGNAL(ViewSystemChanged()), this, SLOT(OnViewSystemChanged()));
	}
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
		m_pHandleList->SetColumnHidden(CHandleModel::eAttributes, true, true);
		m_pHandleList->SetColumnHidden(CHandleModel::eObjectAddress, true, true);
		m_pHandleList->SetColumnHidden(CHandleModel::eOriginalName, true, true);
	}

	m_ViewMode = eNone;
	setObjectName(parent->objectName());
	SwitchView(eSingle);

	OnColumnsChanged();

	//m_pMenu = new QMenu();
	m_pOpen = m_pMenu->addAction(tr("Open Handle"), this, SLOT(OnOpenHandle()));
	
	m_pMenu->addSeparator();

	m_pClose = m_pMenu->addAction(tr("Close"), this, SLOT(OnHandleAction()));
	m_pClose->setShortcut(QKeySequence::Delete);
	m_pClose->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pClose);
	m_pProtect = m_pMenu->addAction(tr("Protect"), this, SLOT(OnHandleAction()));
	m_pProtect->setCheckable(true);
	m_pInherit = m_pMenu->addAction(tr("Inherit"), this, SLOT(OnHandleAction()));
	m_pInherit->setCheckable(true);
	m_pMenu->addSeparator();
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
	m_pHandleList->SetColumnHidden(CHandleModel::eFileShareAccess, false);
}

//
// Fill the type filter from the machine now being looked at.
//
// The types are that machine's own numbering - a Windows object table is built
// when its drivers load, a Linux fd is one of seven fixed kinds - so this cannot
// be decided once at construction. It used to be, which meant a Windows viewer
// watching a Linux box offered Windows object types, none of which any
// descriptor there could ever match.
//
// A machine that reports none hides the row rather than showing an empty combo.
//
void CHandlesView::RebuildTypeFilter()
{
	if (!m_pShowType || !m_pFilterWidget)
		return;

	CSystemPtr pSystem = CCluster::GetViewSystem();
	const QList<CSystemAPI::SHandleType> All = pSystem.isNull()
		? QList<CSystemAPI::SHandleType>() : pSystem->GetHandleTypes();

	//
	// A machine can classify handles in more than one way at once. A Linux
	// machine with Wine on it reports the kernel's descriptor kinds and
	// wineserver's object kinds, and which of them a process can hold depends on
	// the process: only one inside a prefix has the second sort. Offering all of
	// them for every process would mean a filter with entries that can never
	// match anything, which reads as a broken list rather than as an empty one.
	//
	bool bWine = false;
	foreach(const CProcessPtr& pProcess, m_Processes)
	{
		if (pProcess->GetStatusFlags() & CProcessInfo::eStatusWine)
			bWine = true;
	}

	QList<CSystemAPI::SHandleType> HandleTypes;
	foreach(const CSystemAPI::SHandleType& Type, All)
	{
		if (Type.Group == 0 || bWine)
			HandleTypes.append(Type);
	}

	//
	// Two of the filters only mean something on some machines.
	//
	// ETW registrations are a Windows object kind: a Linux machine has none, and
	// neither has Wine, which stubs the tracing API rather than implementing it.
	// Unnamed handles are the same shape of question - every descriptor on Linux
	// resolves to something, a path or a socket:[n], so nothing there is ever
	// unnamed, while a Wine process's events and mutexes mostly are. Disabled
	// rather than hidden, so the row does not change shape as the selection
	// moves, and read as disabled by the filter itself rather than only looking it.
	//
	if (m_pHideETW)
		m_pHideETW->setEnabled(!pSystem.isNull() && pSystem->GetEtwHandleTypeIndex() != -1);
	if (m_pHideUnnamed)
		m_pHideUnnamed->setEnabled(bWine || (!pSystem.isNull() && pSystem->GetOsType() == CSystemAPI::eOsWindows));

	//
	// The filtered list is what is remembered, so that moving between a Wine
	// process and an ordinary one on the same machine counts as a change - the
	// machine has not changed, but what it can show for this process has.
	//
	if (HandleTypes == m_HandleTypes)
		return;		// same machine, or two that classify alike
	m_HandleTypes = HandleTypes;


	m_pFilterWidget->setVisible(!HandleTypes.isEmpty());
	if (HandleTypes.isEmpty())
		return;

	//
	// The selection is kept across the rebuild where the new machine has a type
	// of the same name. Keeping the *index* would be wrong - the same number
	// means something else on the other machine.
	//
	const QString Selected = m_pShowType->currentText();

	m_pTypeModel->clear();

	auto pAll = new QStandardItem(tr("[All]"));
	pAll->setData(-1, Qt::UserRole);
	m_pTypeModel->appendRow(pAll);

	foreach (const CSystemAPI::SHandleType& Type, HandleTypes)
	{
		auto pItem = new QStandardItem(GetHandleTypeLabel(Type));
		pItem->setData(Type.Index, Qt::UserRole);
		m_pTypeModel->appendRow(pItem);
	}

	const int Index = m_pShowType->findText(Selected);
	m_pShowType->setCurrentIndex(Index >= 0 ? Index : 0);
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

		//
		// Emptied the moment the selection moves, before anything is asked for
		// the new process.
		//
		// What is on screen belongs to one process, and an update that fails -
		// another user's process on a viewer with no privilege, a process that
		// exited between the click and the read - emits nothing at all. Without
		// this the previous process's list simply stayed there, under the new
		// process's name, which is worse than showing nothing: it is wrong and
		// it looks right. CThreadsView and CWindowsView already did this.
		//
		m_Handles.clear();
		m_pHandleModel->Clear();

		//
		// The selection may have moved to a machine that classifies handles
		// differently; the filter has to follow it.
		//
		if (m_ShowAllFiles == 0)
			RebuildTypeFilter();

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
		CCluster::GetViewSystem()->UpdateOpenFileListAsync();
	}
	else
	{
		if (m_PendingUpdates > 0)
			return;

		m_PendingUpdates = 0;
		foreach(const CProcessPtr& pProcess, m_Processes)
		{
			m_PendingUpdates++;

			//
			// The call holds its own reference.
			//
			// This is posted to the collector's thread and enumerating a
			// process's handles takes long enough to matter - and meanwhile
			// this thread may be told to show something else, or a machine may
			// go away and take its rows with it. A bare receiver pointer owns
			// nothing, so the object could be freed here while the slot was
			// running over there; that is a crash inside UpdateHandles with a
			// "this" that still looks valid, and it is what a machine
			// disappearing used to produce.
			//
			const CProcessPtr pKeep = pProcess;
			QMetaObject::invokeMethod(pProcess.data(), [pKeep]() { pKeep->UpdateHandles(); },
				Qt::QueuedConnection);
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

		//
		// Defaults for the "all files" variant of this view, which has no
		// filter row of its own. -1 means every type; 0 would match only
		// eUnknown and hide the entire list.
		//
		const int EtwTypeIndex = CCluster::GetViewSystem()->GetEtwHandleTypeIndex();
		int ShowType = CCluster::GetViewSystem()->GetFileHandleTypeIndex();
		bool HideUnnamed = true;
		bool HideETW = true;

		if (m_ShowAllFiles == 0)
		{
			if (m_pShowType)
			{
				ShowType = m_pShowType->currentData().toInt();
				//
				// A disabled box is not a checked one, whatever it was left on
				// by the last machine that had the notion.
				//
				HideUnnamed = m_pHideUnnamed->isEnabled() && m_pHideUnnamed->isChecked();
				HideETW = m_pHideETW->isEnabled() && m_pHideETW->isChecked();
			}
			else
			{
				ShowType = -1;
				HideUnnamed = false;
				HideETW = false;
			}
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
				//
				// A busy process can hold thousands of ETW registrations; the
				// filter exists to keep them out of the way.
				//
				if (HideETW && EtwTypeIndex != -1 && EtwTypeIndex == (int)pHandle->GetTypeIndex())
					continue;

				ASSERT(!AllHandles.contains(I.key()));
				AllHandles.insert(I.key(), I.value());
			}
		}
		ShowHandles(AllHandles);
	}
}

void CHandlesView::OnViewSystemChanged()
{
	m_Handles.clear();
	m_pHandleModel->Clear();
}

void CHandlesView::ShowOpenFiles(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed)
{
	bool bGetDanymicData = theConf->GetBool("Options/OpenFileGetPosition", false);

	m_pHandleModel->SetSizePosNA(!bGetDanymicData);

	ShowHandles(CCluster::GetViewSystem()->GetOpenFilesList());
}

void CHandlesView::ShowHandles(const QMap<quint64, CHandlePtr>& Handles)
{
	m_Handles = Handles;

	m_pHandleModel->Sync(m_Handles);
}

void CHandlesView::UpdateFilter()
{
	if (m_pShowType)
		UpdateFilter(m_pShowType->currentText());
}

void CHandlesView::UpdateFilter(const QString & filter)
{
	if (m_pShowType)
	{
		theConf->SetValue("HandleView/ShowType", filter);
		theConf->SetValue("HandleView/HideUnNamed", m_pHideUnnamed->isChecked());
		theConf->SetValue("HandleView/HideETW", m_pHideETW->isChecked());
	}
	//m_pSortProxy->SetFilter(filter, m_pHideUnnamed->isChecked(), m_pHideETW->isChecked());
}

//void CHandlesView::OnShowDetails()
//{
//	theConf->SetValue("HandleView/ShowDetails", m_pShowDetails->isChecked());
//	m_pHandleDetails->setVisible(m_pShowDetails->isChecked());
//}

//
// What a handle points at, on the machine the handle came from.
//
// Asked of that machine and not of the global, because a viewer watching two
// machines would otherwise resolve one's pids against the other's. And guarded,
// because the machine may be gone: a panel holds its handle by shared pointer,
// so the object outlives a disconnected system and GetSystem() answers null.
// Nothing found is the honest answer then, and every caller here already draws
// "unknown" for it.
//
static CProcessPtr ProcessOnItsMachine(const CHandlePtr& pHandle, quint64 Pid)
{
	const CSystemPtr pSystem = pHandle.isNull() ? CSystemPtr() : pHandle->GetSystem();
	return pSystem ? pSystem->GetProcessByID(Pid) : CProcessPtr();
}

static CThreadPtr ThreadOnItsMachine(const CHandlePtr& pHandle, quint64 Tid)
{
	const CSystemPtr pSystem = pHandle.isNull() ? CSystemPtr() : pHandle->GetSystem();
	return pSystem ? pSystem->GetThreadByID(Tid) : CThreadPtr();
}


void CHandlesView::OnItemSelected(const QModelIndex &current)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);

	CHandlePtr pHandle = m_pHandleModel->GetHandle(ModelIndex);
	if (pHandle.isNull())
		return;

	QTreeWidget* pDetails = (QTreeWidget*)m_pHandleDetails->GetView();
	// Note: we don't auto refresh this infos
	pDetails->clear();

	QString TypeName = pHandle->GetTypeName();
	QVariantMap HandleInfo = pHandle->GetHandleInfo();
	
	QTreeWidgetItem* pBasicInfo = new QTreeWidgetItem(QStringList(tr("Basic information")));
	pDetails->addTopLevelItem(pBasicInfo);

	QTreeWidgetEx::AddSubItem(pBasicInfo, tr("Name"), pHandle->GetFileName()); // pHandle->GetOriginalName()
	//if (!pHandle->GetOriginalName().isEmpty())
	//	QTreeWidgetEx::AddSubItem(pBasicInfo, tr("Original Name"), pHandle->GetOriginalName());
	QTreeWidgetEx::AddSubItem(pBasicInfo, tr("Type"), ::GetHandleTypeString(pHandle));
	QTreeWidgetEx::AddSubItem(pBasicInfo, tr("Object address"), FormatAddress(pHandle->GetObjectAddress()));

	QTreeWidgetItem* pSecInfo = new QTreeWidgetItem(QStringList(tr("Security information")));
	pDetails->addTopLevelItem(pSecInfo);
	QTreeWidgetEx::AddSubItem(pSecInfo, tr("Granted access"), ::GetGrantedAccessString(pHandle));
	QTreeWidgetEx::AddSubItem(pSecInfo, tr("Granted access (generic)"), ::GetGenericAccessString(pHandle));
	QTreeWidgetEx::AddSubItem(pSecInfo, tr("Granted access (mask)"), tr("0x%1").arg(pHandle->GetGrantedAccess(), 8, 16, QChar('0')));
	QTreeWidgetEx::AddSubItem(pSecInfo, tr("SDDL"), pHandle->GetSecurityDescriptorSddl());

	QTreeWidgetItem* pReferences = new QTreeWidgetItem(QStringList(tr("References")));
	pDetails->addTopLevelItem(pReferences);

	QTreeWidgetEx::AddSubItem(pReferences, tr("Ref. count"), HandleInfo["References"].toString());
	QTreeWidgetEx::AddSubItem(pReferences, tr("Handles"), HandleInfo["Handles"].toString());


	QTreeWidgetItem* pQuota = new QTreeWidgetItem(QStringList(tr("Quota charges")));
	pDetails->addTopLevelItem(pQuota);

	QTreeWidgetEx::AddSubItem(pQuota, tr("Paged"), HandleInfo["Paged"].toString());
	QTreeWidgetEx::AddSubItem(pQuota, tr("Virtual Size"), HandleInfo["VirtualSize"].toString());


	QTreeWidgetItem* pExtendedInfo = new QTreeWidgetItem(QStringList(tr("Extended information")));
	pDetails->addTopLevelItem(pExtendedInfo);

	//
	// ALPC port flags, decoded by the backend that knows what they mean.
	//
	if (TypeName == "ALPC Port")
	{
		// The flag word is decoded by the backend that knows what its bits mean.
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Flags"), ::GetAlpcPortFlagsString(HandleInfo["Flags"].toUInt()));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Sequence number"), HandleInfo["SeqNumber"].toString());
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Port context"), HandleInfo["Context"].toString());

		const CSystemPtr pSystem = pHandle->GetSystem();
		auto GetProcCon = [HandleInfo, pSystem](const QString& PidName, const QString& PortName) {
			CProcessPtr pProcess = pSystem->GetProcessByID(HandleInfo[PidName].toULongLong());
			QString Name = tr("%1 (%2)").arg(::LocalizeName(pProcess ? pProcess->GetName() : QString())).arg(theGUI->FormatID(HandleInfo[PidName].toULongLong()));
			QString Port = HandleInfo[PortName].toString();
			if (Port.isEmpty())
				return Name;
			return Name + " - " + Port;
		};

		if (HandleInfo.contains("ConnectionPID")) QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Connection"), GetProcCon("ConnectionPID", "ConnectionPort"));
		if (HandleInfo.contains("ServerComPID")) QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Server"), GetProcCon("ServerComPID", "ServerComPort"));
		if (HandleInfo.contains("ClientComPID")) QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Client"), GetProcCon("ClientComPID", "ClientComPort"));
	}
	else if(TypeName == "File")
	{
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Is directory"), HandleInfo["IsDir"].toBool() ? tr("True") : tr("False"));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("File mode"), ::GetFileAccessModeString(pHandle, HandleInfo["Mode"].toUInt()));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("File size"), FormatSize(HandleInfo["Size"].toULongLong()));

		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Driver Device"), HandleInfo["DrvDevice"].toString());
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Driver Image"), HandleInfo["DrvImage"].toString());
	}
	else if(TypeName == "Section")
	{
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Section type"), ::GetSectionTypeString(HandleInfo["Attribs"].toUInt()));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Size"), FormatSize(HandleInfo["Size"].toULongLong()));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("File"), HandleInfo["File"].toString());
	}
	else if(TypeName == "Mutant")
	{
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Count"), HandleInfo["Count"].toString());
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Abandoned"), HandleInfo["Abandoned"].toBool() ? tr("True") : tr("False"));
		CThreadPtr pThread = ThreadOnItsMachine(pHandle, HandleInfo["TID"].toULongLong());
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Owner"), tr("%1 (%2): %3").arg(pThread ? pThread->GetName() : tr("unknown")).arg(theGUI->FormatID(HandleInfo["PID"].toULongLong())).arg(theGUI->FormatID(HandleInfo["TID"].toULongLong())));
	}
	else if(TypeName == "Process" || TypeName == "Thread")
	{
		QString Name;
		if (TypeName == "Process")
		{
			CProcessPtr pProcess = ProcessOnItsMachine(pHandle, HandleInfo["PID"].toULongLong());
			Name = tr("%1 (%2)").arg(::LocalizeName(pProcess ? pProcess->GetName() : QString())).arg(theGUI->FormatID(HandleInfo["PID"].toULongLong()));
		}
		else
		{
			CThreadPtr pThread = ThreadOnItsMachine(pHandle, HandleInfo["TID"].toULongLong());
			Name = tr("%1 (%2): %3").arg(pThread ? pThread->GetName() : tr("unknown")).arg(theGUI->FormatID(HandleInfo["PID"].toULongLong())).arg(theGUI->FormatID(HandleInfo["TID"].toULongLong()));
		}

		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Name"), Name);
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Created"), QDateTime::fromSecsSinceEpoch(HandleInfo["Created"].toULongLong()).toString("dd.MM.yyyy hh:mm:ss"));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Exited"), QDateTime::fromSecsSinceEpoch(HandleInfo["Exited"].toULongLong()).toString("dd.MM.yyyy hh:mm:ss"));
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("ExitStatus"), HandleInfo["ExitStatus"].toString());
	}
	else if(TypeName == "Timer")
	{
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Remaining"), HandleInfo["Remaining"].toString());
		QTreeWidgetEx::AddSubItem(pExtendedInfo, tr("Signaled"), HandleInfo["Signaled"].toBool() ? tr("True") : tr("False"));
	}
	else
		delete pExtendedInfo;

	pDetails->expandAll();
}


void CHandlesView::OnMenu(const QPoint &point)
{
	QModelIndex Index = m_pHandleList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CHandlePtr pHandle = m_pHandleModel->GetHandle(ModelIndex);

	QModelIndexList selectedRows = m_pHandleList->selectedRows();

	m_pClose->setEnabled(!pHandle.isNull());
	//
	// Changing a handle's flags needs the kernel driver; without it these are
	// shown but not offered.
	//
	const bool bCanEditHandle = CCluster::GetViewSystem()->HasCapability(CSystemAPI::eCapKernelDriver);
	m_pProtect->setEnabled(!pHandle.isNull() && bCanEditHandle);
	m_pProtect->setChecked(!pHandle.isNull() && pHandle->IsProtected());
	m_pInherit->setEnabled(!pHandle.isNull() && bCanEditHandle);
	m_pInherit->setChecked(!pHandle.isNull() && pHandle->IsInherited());

	QString Type = !pHandle.isNull() ? pHandle->GetTypeName() : "";

	m_pOpen->setVisible(Type == "Token" || Type == "File" || Type == "Mapped file" || Type == "DLL" || Type == "Mapped image" || Type == "Job" || Type == "Process" || Type == "Thread" || Type == "Section");

	m_pSemaphore->menuAction()->setVisible(Type == "Semaphore");
	m_pEvent->menuAction()->setVisible(Type == "Event");
	m_pEventPair->menuAction()->setVisible(Type == "EventPair");
	m_pTimer->menuAction()->setVisible(Type == "Timer");

	m_pTask->menuAction()->setVisible(Type == "Process" || Type == "Thread");

	m_pPermissions->setEnabled(selectedRows.count() == 1 && CCluster::GetViewSystem()->HasCapability(CSystemAPI::eCapSecurityEditor));
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
		if (!pHandle.isNull())
		{
			STATUS Status = OK;
retry:
			if (sender() == m_pClose)
				Status = pHandle->Close(Force == 1);
			else if (sender() == m_pProtect)
				Status = pHandle->SetProtected(m_pProtect->isChecked());
			else if (sender() == m_pInherit)
				Status = pHandle->SetInherited(m_pInherit->isChecked());
			else 

			if (sender() == m_pSemaphoreAcquire)
				Status = pHandle->DoHandleAction(CHandleInfo::eSemaphoreAcquire);
			else if (sender() == m_pSemaphoreRelease)
				Status = pHandle->DoHandleAction(CHandleInfo::eSemaphoreRelease);
			else 

			if (sender() == m_pEventSet)
				Status = pHandle->DoHandleAction(CHandleInfo::eEventSet);
			else if (sender() == m_pEventReset)
				Status = pHandle->DoHandleAction(CHandleInfo::eEventReset);
			else if (sender() == m_pEventPulse)
				Status = pHandle->DoHandleAction(CHandleInfo::eEventPulse);
			else

			if (sender() == m_pEventSetLow)
				Status = pHandle->DoHandleAction(CHandleInfo::eSetLow);
			else if (sender() == m_pEventSetHigh)
				Status = pHandle->DoHandleAction(CHandleInfo::eSetHigh);
			else
				
			if (sender() == m_pTimerCancel)
				Status = pHandle->DoHandleAction(CHandleInfo::eCancelTimer);
			else 

			if (sender() == m_pTerminate || sender() == m_pSuspend || sender() == m_pResume)
			{
				QString TypeName = pHandle->GetTypeName();
				QVariantMap HandleInfo = pHandle->GetHandleInfo();

				QSharedPointer<CAbstractTask> pTask;
				if (TypeName == "Thread")
					pTask = ThreadOnItsMachine(pHandle, HandleInfo["TID"].toULongLong());
				else if (TypeName == "Process")
					pTask = ProcessOnItsMachine(pHandle, HandleInfo["PID"].toULongLong());

				if (!pTask)
					Status = ERR(TE_Message, QVariantList() << tr("Not found."));
				else if (sender() == m_pTerminate)
					Status = pTask->Terminate(Force == 1);
				else if (sender() == m_pSuspend)
					Status = pTask->Suspend();
				else if (sender() == m_pResume)
					Status = pTask->Resume();
			}
			if (Status.IsError())
			{
				if (Status.GetStatus() == ERROR_CONFIRM)
				{
					if (Force == -1)
					{
						switch (QMessageBox("TaskExplorer", CTaskExplorer::FormatError(Status), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel | QMessageBox::Default | QMessageBox::Escape).exec())
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
	QModelIndex Index = m_pHandleList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CHandlePtr pHandle = m_pHandleModel->GetHandle(ModelIndex);
	if (!pHandle)
		return;
	CTaskExplorer::ShowSecurity(pHandle->GetSecurityObject(), this);
}

void CHandlesView::OnDoubleClicked()
{
	if (m_ShowAllFiles != 0)
	{
		QModelIndex Index = m_pHandleList->currentIndex();
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CHandlePtr pHandle = m_pHandleModel->GetHandle(ModelIndex);
		if (pHandle)
		{
			CProcessPtr pProcess = pHandle->GetProcess().objectCast<CProcessInfo>();
			if (pProcess)
			{
				CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow(QList<CProcessPtr>() << pProcess);
				pTaskInfoWindow->show();
			}
		}
	}
	else
		OnOpenHandle();
}

void CHandlesView::OnOpenHandle()
{
	QModelIndex Index = m_pHandleList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CHandlePtr pHandle = m_pHandleModel->GetHandle(ModelIndex);
	if (!pHandle)
		return;

	const QString Type = pHandle->GetTypeName();
	if (Type == "Token")
	{
#ifdef WIN32	// this view is in TE_GUI_WIN; phase 4 gives it a portable API
		CTokenInfoPtr pToken = pHandle->GetToken();
		if (pToken)
		{
			CTokenView* pTokenView = new CTokenView();
			CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow(pTokenView, tr("Token"));
			pTokenView->ShowToken(pToken);
			pTaskInfoWindow->show();
		}
#endif
	}
	else if (Type == "Job")
	{
#ifdef WIN32	// this view is in TE_GUI_WIN; phase 4 gives it a portable API
		CJobInfoPtr pJob = pHandle->GetJob();
		if (pJob)
		{
			CJobView* pJobView = new CJobView();
			CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow(pJobView, tr("Job"));
			pJobView->ShowJob(pJob);
			pTaskInfoWindow->show();
		}
#endif
	}
	else if (Type == "Section")
	{
		// Read/Write &memory
		QIODevice* pDevice = pHandle->OpenDevice();
		if (!pDevice) {
			QMessageBox("TaskExplorer", tr("This memory region can not be edited"), QMessageBox::Warning, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton).exec();
			return;
		}

		CMemoryEditor* pEditor = new CMemoryEditor();
		if(CProcessPtr pProcess = pHandle->GetProcess().staticCast<CProcessInfo>())
			pEditor->setWindowTitle(tr("Memory Editor: %1 (%2)").arg(::LocalizeName(pProcess->GetName())).arg(pProcess->GetParentId()));
		pEditor->setDevice(pDevice);
		pEditor->show();
	}
	else if (Type == "File" || Type == "Mapped file" || Type == "DLL" || Type == "Mapped image")
	{
		//if(Type == "File") // file properties
		// PhShellProperties(hWnd, Info->BestObjectName->Buffer);
           
		::ExploreFile(pHandle->GetSystem().data(), pHandle->GetFileName());
	}
	else if (Type == "Key")
	{
		::OpenRegistryKey(pHandle->GetSystem().data(), pHandle->GetFileName());
	}
	else
	if (Type == "Process" || Type == "Thread")
	{
		CProcessPtr pProcess = ProcessOnItsMachine(pHandle, pHandle->GetProcessId());

		quint64 ThreadId = 0;
		if (Type == "Thread")
		{
			QVariantMap HandleInfo = pHandle->GetHandleInfo();
			ThreadId = HandleInfo["TID"].toULongLong();
		}

		CTaskInfoWindow* pTaskInfoWindow = new CTaskInfoWindow(QList<CProcessPtr>() << pProcess, ThreadId);
		pTaskInfoWindow->show();
	}
}
