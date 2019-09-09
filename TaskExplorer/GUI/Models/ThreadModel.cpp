#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ThreadModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinThread.h"
#endif

CThreadModel::CThreadModel(QObject *parent)
:CListItemModel(parent)
{
	m_bExtThreadId = false;
}

CThreadModel::~CThreadModel()
{
}

void CThreadModel::Sync(QMap<quint64, CThreadPtr> ThreadList)
{
	QList<SListNode*> New;
	QHash<QVariant, SListNode*> Old = m_Map;

	bool bClearZeros = theConf->GetBool("Options/ClearZeros", true);

	foreach (const CThreadPtr& pThread, ThreadList)
	{
		QVariant ID = pThread->GetThreadId();

		int Row = -1;
		SThreadNode* pNode = static_cast<SThreadNode*>(Old[ID]);
		if(!pNode)
		{
			pNode = static_cast<SThreadNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pThread = pThread;
			pNode->IsBold = pThread->IsMainThread();
			New.append(pNode);
		}
		else
		{
			Old[ID] = NULL;
			Row = GetRow(pNode);
		}

#ifdef WIN32
		CWinThread* pWinThread = qobject_cast<CWinThread*>(pThread.data());
#endif

		int Col = 0;
		bool State = false;
		int Changed = 0;

		// Note: icons are loaded asynchroniusly
		if (m_bUseIcons && !pNode->Icon.isValid() && m_Columns.contains(eThread))
		{
			CProcessPtr pProcess = pNode->pThread->GetProcess().objectCast<CProcessInfo>();
			if (!pProcess.isNull())
			{
				QPixmap Icon = pProcess->GetModuleInfo()->GetFileIcon();
				if (!Icon.isNull()) {
					Changed = 1; // set change for first column
					pNode->Icon = Icon;
				}
			}
		}

		int RowColor = CTaskExplorer::eNone;
		if (pThread->IsMarkedForRemoval())		RowColor = CTaskExplorer::eToBeRemoved;
		else if (pThread->IsNewlyCreated())		RowColor = CTaskExplorer::eAdded;
#ifdef WIN32
		else if (pWinThread->IsGuiThread())		RowColor = CTaskExplorer::eGuiThread;
#endif
		
		if (pNode->iColor != RowColor) {
			pNode->iColor = RowColor;
			pNode->Color = CTaskExplorer::GetListColor(RowColor);
			Changed = 2;
		}

		if (pNode->IsGray != pThread->IsSuspended())
		{
			pNode->IsGray = pThread->IsSuspended();
			Changed = 2;
		}

		STaskStats CpuStats = pThread->GetCpuStats();

		for(int section = 0; section < columnCount(); section++)
		{
			if (!m_Columns.contains(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch(section)
			{
				case eThread:				Value = pThread->GetThreadId(); break;
				case eCPU_History:
				case eCPU:					Value = CpuStats.CpuUsage; break;
#ifdef WIN32
				case eStartAddress:			Value = pWinThread->GetStartAddressString(); break;
#endif
                case ePriority:				Value = (quint32)pThread->GetPriority(); break;
#ifdef WIN32
				case eService:				Value = pWinThread->GetServiceName(); break;
				case eName:					Value = pWinThread->GetThreadName(); break;
				case eType:					Value = pWinThread->IsMainThread() ? 2 : pWinThread->IsGuiThread() ? 1 : 0; break;
#endif
				case eCreated:				Value = pThread->GetCreateTimeStamp(); break;
#ifdef WIN32
				case eStartModule:			Value = pWinThread->GetStartAddressFileName(); break;
#endif
				case eContextSwitches:		Value = CpuStats.ContextSwitchesDelta.Value; break;
				case eContextSwitchesDelta:	Value = CpuStats.ContextSwitchesDelta.Delta; break;
                case eBasePriority:			Value = (quint32)pThread->GetBasePriority(); break;
                case ePagePriority:			Value = (quint32)pThread->GetPagePriority(); break;
                case eIOPriority:			Value = (quint32)pThread->GetIOPriority(); break;
				case eCycles:				Value = CpuStats.CycleDelta.Value; break;
				case eCyclesDelta:			Value = CpuStats.CycleDelta.Delta; break;
				case eState:				Value = pThread->GetWaitState(); break;
				case eKernelTime:			Value = CpuStats.CpuKernelDelta.Value;
				case eUserTime:				Value = CpuStats.CpuUserDelta.Value;
#ifdef WIN32
				case eIdealProcessor:		Value = pWinThread->GetIdealProcessor(); break;
				case eCritical:				Value = pWinThread->IsCriticalThread() ? tr("Critical") : ""; break;
#endif
#ifdef WIN32
				case eAppDomain:			Value = pWinThread->GetAppDomain(); break;
#endif
			}

			SThreadNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
					case eThread:				if (m_bExtThreadId)
													ColValue.Formated = tr("%1 (%2): %3").arg(pThread->GetName()).arg(pThread->GetProcessId()).arg(pThread->GetThreadId());
												break;
					//case eThread:				ColValue.Formated = "0x" + QString::number(pThread->GetThreadId()); break;
					case eCPU:					ColValue.Formated = (!bClearZeros || CpuStats.CpuUsage > 0.00004) ? QString::number(CpuStats.CpuUsage*100, 10, 2) + "%" : ""; break;
					case ePriority:				ColValue.Formated = pThread->GetPriorityString(); break;
					case eCreated:				ColValue.Formated = QDateTime::fromTime_t(Value.toULongLong()/1000).toString("dd.MM.yyyy hh:mm:ss"); break;
					case eType:					ColValue.Formated = pWinThread->GetTypeString(); break;
					case eState:				ColValue.Formated = pThread->GetStateString(); break;
					case eCycles:
					case eContextSwitches:
												ColValue.Formated = FormatNumber(Value.toULongLong()); break;
					case eCyclesDelta:
					case eContextSwitchesDelta:
												ColValue.Formated = FormatNumberEx(Value.toULongLong(), bClearZeros); break;
				}
			}

			if(State != (Changed != 0))
			{
				if(State && Row != -1)
					emit dataChanged(createIndex(Row, Col), createIndex(Row, section-1));
				State = (Changed != 0);
				Col = section;
			}
			if (Changed == 1)
				Changed = 0;
		}
		if(State && Row != -1)
			emit dataChanged(createIndex(Row, Col, pNode), createIndex(Row, columnCount()-1, pNode));

	}

	CListItemModel::Sync(New, Old);
}

CThreadPtr CThreadModel::GetThread(const QModelIndex &index) const
{
	if (!index.isValid())
        return CThreadPtr();

	SThreadNode* pNode = static_cast<SThreadNode*>(index.internalPointer());
	return pNode->pThread;
}

int CThreadModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CThreadModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eThread:				return tr("Thread");
			case eCPU_History:			return tr("CPU graph");
			case eCPU:					return tr("CPU");
			case eCyclesDelta:			return tr("Cycles delta");
#ifdef WIN32
			case eStartAddress:			return tr("Start address");
#endif
			case ePriority:				return tr("Priority");
#ifdef WIN32
			case eService:				return tr("Service");
			case eName:					return tr("Name");
			case eType:					return tr("Type");
#endif
			case eCreated:				return tr("Created");
#ifdef WIN32
			case eStartModule:			return tr("Start module");
#endif
			case eContextSwitches:		return tr("Context switches");
			case eContextSwitchesDelta:	return tr("Context switches delta");
			case eBasePriority:			return tr("Base priority");
			case ePagePriority:			return tr("Page priority");
			case eIOPriority:			return tr("I/O priority");
			case eCycles:				return tr("Cycles");
			case eState:				return tr("State");
			case eKernelTime:			return tr("Kernel time");
			case eUserTime:				return tr("User time");
#ifdef WIN32
			case eIdealProcessor:		return tr("Ideal processor");
			case eCritical:				return tr("Critical");
#endif
#ifdef WIN32
			case eAppDomain:			return tr("App Domain");
#endif
		}
	}
    return QVariant();
}

QVariant CThreadModel::GetDefaultIcon() const 
{ 
	return g_ExeIcon;
}