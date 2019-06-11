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
}

CThreadModel::~CThreadModel()
{
}

void CThreadModel::Sync(QMap<quint64, CThreadPtr> ThreadList)
{
	QList<SListNode*> New;
	QMap<QVariant, SListNode*> Old = m_Map;

	foreach (const CThreadPtr& pThread, ThreadList)
	{
		QVariant ID = (quint64)pThread.data();

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
			Row = m_List.indexOf(pNode);
		}

		int Col = 0;
		bool State = false;
		bool Changed = false;

#ifdef WIN32
		CWinThread* pWinThread = qobject_cast<CWinThread*>(pThread.data());
#endif

		for(int section = eThread; section < columnCount(); section++)
		{
			QVariant Value;
			switch(section)
			{
				case eThread:				Value = pThread->GetThreadId(); break;
				case eCPU:					Value = pThread->GetCpuUsage(); break;
				//case eCyclesDelta:			return tr("Cycles delta");*/
#ifdef WIN32
				case eStartAddress:			Value = pWinThread->GetStartAddressString(); break;
#endif
				case ePriority:				Value = pThread->GetPriority(); break;
#ifdef WIN32
				case eService:				Value = pWinThread->GetServiceName(); break;
				case eName:					Value = pWinThread->GetThreadName(); break;
				case eType:					Value = pWinThread->GetTypeString(); break;
#endif
				case eCreated:				Value = pThread->GetCreateTime(); break;
				/*case eStartModule:			return tr("Start module");
				case eContextSwitches:		return tr("Context switches");*/
				case eBasePriority:			Value = pThread->GetBasePriority(); break;
				/*case ePagePriority:			return tr("Page priority");
				case eIOPriority:			return tr("I/O priority");
				case eCycles:				return tr("Cycles");*/
				case eState:				Value = pThread->GetStateString(); break;
				/*case eKernelTime:			return tr("Kernel time");
				case eUserTime:				return tr("User time");
				case eIdealProcessor:		return tr("Ideal processor");
				case eCritical:				return tr("Critical");*/
			}

			SThreadNode::SValue& ColValue = pNode->Values[section];

			bool Changed = false;
			if (ColValue.Raw != Value)
			{
				Changed = true;
				ColValue.Raw = Value;

				switch (section)
				{
					//case eThread:				ColValue.Formated = "0x" + QString::number(pThread->GetThreadId(), 16); break;
					case eCPU:					ColValue.Formated = QString::number(pThread->GetCpuUsage()*100, 10,2); break;
					case ePriority:				ColValue.Formated = pThread->GetPriorityString(); break;
					case eCreated:				ColValue.Formated = pThread->GetCreateTime().toString("dd.MM.yyyy hh:mm:ss"); break;
				}
			}

			if(State != Changed)
			{
				if(State && Row != -1)
					emit dataChanged(createIndex(Row, Col), createIndex(Row, section-1));
				State = Changed;
				Col = section;
			}
			Changed = false;
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
			case eCPU:					return tr("CPU");
			case eCyclesDelta:			return tr("Cycles delta");
			case eStartAddress:			return tr("Start address");
			case ePriority:				return tr("Priority");
			case eService:				return tr("Service");
			case eName:					return tr("Name");
			case eType:					return tr("Type");
			case eCreated:				return tr("Created");
			case eStartModule:			return tr("Start module");
			case eContextSwitches:		return tr("Context switches");
			case eBasePriority:			return tr("Base priority");
			case ePagePriority:			return tr("Page priority");
			case eIOPriority:			return tr("I/O priority");
			case eCycles:				return tr("Cycles");
			case eState:				return tr("State");
			case eKernelTime:			return tr("Kernel time");
			case eUserTime:				return tr("User time");
			case eIdealProcessor:		return tr("Ideal processor");
			case eCritical:				return tr("Critical");
		}
	}
    return QVariant();
}
