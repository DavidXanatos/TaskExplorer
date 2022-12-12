#include "stdafx.h"
#include "../TaskExplorer.h"
#include "DebugView.h"
#include "../../../MiscHelpers/Common/KeyValueInputDialog.h"
#include "../../../MiscHelpers/Common/Finder.h"
#include "../../API/Windows/WinProcess.h"
#include "../../API/Windows/ProcessHacker.h"
#include <algorithm>

CDebugView::CDebugView(QWidget *parent)
	:CPanelWidgetEx(parent)
{
	m_pTreeList->setItemDelegate(theGUI->GetItemDelegate());
	m_pTreeList->setHeaderLabels(tr("Process|Time stamp|Message").split("|"));
	m_pTreeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeList->setSortingEnabled(true);
	m_pTreeList->setMinimumHeight(60);
	m_pTreeList->setAutoFitMax(200);

	m_pTreeList->setSortingEnabled(false);
	m_pTreeList->setColumnReset(2);
	connect(m_pTreeList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));

	m_ViewMode = eNone;
	setObjectName(parent->objectName());
	SwitchView(eSingle);
}

CDebugView::~CDebugView()
{
	SwitchView(eNone);
}

void CDebugView::SwitchView(EView ViewMode)
{
	switch (m_ViewMode)
	{
		case eSingle:	theConf->SetBlob(objectName() + "/DebugView_Columns", m_pTreeList->header()->saveState()); break;
		case eMulti:	theConf->SetBlob(objectName() + "/DebugMultiView_Columns", m_pTreeList->header()->saveState()); break;
	}

	m_ViewMode = ViewMode;

	QByteArray Columns;
	switch (m_ViewMode)
	{
		case eSingle:	Columns = theConf->GetBlob(objectName() + "/DebugView_Columns"); break;
		case eMulti:	Columns = theConf->GetBlob(objectName() + "/DebugMultiView_Columns"); break;
		default:
			return;
	}
	
	if (Columns.isEmpty())
		OnResetColumns();
	else
		m_pTreeList->header()->restoreState(Columns);
}

void CDebugView::OnResetColumns()
{
	m_pTreeList->OnResetColumns();

	if(m_ViewMode == eSingle)
		m_pTreeList->setColumnHidden(CDebugView::eProcess, true);
}

void CDebugView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	if (m_Processes == Processes)
		return;

	m_Processes = Processes;

	SwitchView(m_Processes.size() > 1 ? eMulti : eSingle);

	m_pTreeList->clear();
	m_CounterMap.clear();

	Refresh();
}

void CDebugView::Refresh()
{
	struct SNewMessage
	{
		bool operator<(const SNewMessage& other) const {
			return TimeStamp < other.TimeStamp;
		}
		quint64 ProcessId;
		QString Name;
		QIcon Icon;
		QDateTime TimeStamp;
		QString Text;
	};

	QList<SNewMessage> NewMessages;

	foreach(const CProcessPtr& pProcess, m_Processes)
	{
		quint32 DebugMessageCount = 0;
		QList<CProcessInfo::SDebugMessage> Messages = pProcess->GetDebugMessages(&DebugMessageCount);

		quint64 ProcessId = pProcess->GetProcessId();
		if (DebugMessageCount > m_CounterMap[ProcessId])
		{
			int ToGo = DebugMessageCount - m_CounterMap[ProcessId];
			m_CounterMap[ProcessId] = DebugMessageCount;

			int Start = Messages.count() - ToGo;
			if (Start < 0) // we encountered a buffer overflow, we lost some lines
				Start = 0;

			for (int i = Start; i < Messages.count(); i++)
			{
				CProcessInfo::SDebugMessage& Message = Messages[i];

				CModulePtr pModule = pProcess->GetModuleInfo();
				QString Name = pProcess->GetName() + QString(" (%1)").arg(ProcessId);
				QIcon Icon = pModule->GetFileIcon();
				NewMessages.append(SNewMessage {ProcessId, Name, Icon.isNull() ? g_ExeIcon : Icon, Message.TimeStamp, Message.Text});
			}
		}
	};

	if (m_ViewMode == eMulti)
	{
		std::sort(NewMessages.begin(), NewMessages.end());
	}

	QDateTime OldThreshold = QDateTime::currentDateTime().addSecs(-theConf->GetInt("Options/ShortDateLimit", 12*60*60));

	QList<QTreeWidgetItem*> NewItems;
	foreach(const SNewMessage& Message, NewMessages)
	{
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		pItem->setData(eProcess, Message.ProcessId, Qt::UserRole);
		pItem->setText(eProcess, Message.Name);
		pItem->setIcon(eProcess, Message.Icon);
		if(OldThreshold >= Message.TimeStamp)
			pItem->setText(eTimeStamp, Message.TimeStamp.toString("dd.MM.yyyy hh:mm:ss.zzz"));
		else
			pItem->setText(eTimeStamp, Message.TimeStamp.toString("hh:mm:ss.zzz"));
		pItem->setText(eMessage, Message.Text);
		NewItems.append(pItem);
	}
	m_pTreeList->addTopLevelItems(NewItems);
}

