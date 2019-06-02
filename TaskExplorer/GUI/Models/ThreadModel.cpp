#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ThreadModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinThread.h"
#endif

CThreadModel::CThreadModel(QObject *parent)
:QAbstractItemModel(parent)
{
}

CThreadModel::~CThreadModel()
{
	foreach(SThreadNode* pNode, m_List)
		delete pNode;
}

void CThreadModel::Sync(QMap<quint64, CThreadPtr> ThreadList)
{
	QList<SThreadNode*> New;
	QMap<quint64, SThreadNode*> Old = m_Map;

	foreach (const CThreadPtr& pThread, ThreadList)
	{
		quint64 UID = (quint64)pThread.data();

		int Row = -1;
		SThreadNode* pNode = Old[UID];
		if(!pNode)
		{
			pNode = new SThreadNode();
			pNode->Values.resize(columnCount());
			pNode->UID = UID;
			pNode->pThread = pThread;
			New.append(pNode);
		}
		else
		{
			Old[UID] = NULL;
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
				/*case eCPU:					return tr("CPU");
				case eCyclesDelta:			return tr("Cycles delta");*/
#ifdef WIN32
				case eStartAddress:			Value = pWinThread->GetStartAddressString(); break;
#endif
				case ePriority:				Value = pThread->GetPriority(); break;
				/*case eService:				return tr("Service");
				case eName:					return tr("Name");*/
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
					case eThread:				ColValue.Formated = "0x" + QString::number(pThread->GetThreadId(), 16); break;
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

	Sync(New, Old);
}

void CThreadModel::Sync(QList<SThreadNode*>& New, QMap<quint64, SThreadNode*>& Old)
{
	int Begin = -1;
	int End = -1;
	for(int i = m_List.count()-1; i >= -1; i--) 
	{
		quint64 ID = i >= 0 ? m_List[i]->UID : -1;
		if(ID != -1 && (Old.value(ID) != NULL)) // remove it
		{
			m_Map.remove(ID);
			if(End == -1)
				End = i;
		}
		else if(End != -1) // keep it and remove whatis to be removed at once
		{
			Begin = i + 1;

			beginRemoveRows(QModelIndex(), Begin, End);
			for(int j = End; j >= Begin; j--)
				delete m_List.takeAt(j);
			endRemoveRows();

			End = -1;
			Begin = -1;
		}
    }

	Begin = m_List.count();
	for(QList<SThreadNode*>::iterator I = New.begin(); I != New.end(); I++)
	{
		SThreadNode* pNode = *I;
		m_Map.insert(pNode->UID, pNode);
		m_List.append(pNode);
	}
	End = m_List.count();
	if(Begin < End)
	{
		beginInsertRows(QModelIndex(), Begin, End-1);
		endInsertRows();
	}
}

QModelIndex CThreadModel::FindIndex(quint64 SubID)
{
	if(SThreadNode* pNode = m_Map.value(SubID))
	{
		int row = m_List.indexOf(pNode);
		ASSERT(row != -1);
		return createIndex(row, eThread, pNode);
	}
	return QModelIndex();
}

void CThreadModel::Clear()
{
	//beginResetModel();
	beginRemoveRows(QModelIndex(), 0, rowCount());
	foreach(SThreadNode* pNode, m_List)
		delete pNode;
	m_List.clear();
	m_Map.clear();
	endRemoveRows();
	//endResetModel();
}

QVariant CThreadModel::data(const QModelIndex &index, int role) const
{
    return Data(index, role, index.column());
}

CThreadPtr CThreadModel::GetThread(const QModelIndex &index) const
{
	if (!index.isValid())
        return CThreadPtr();

	SThreadNode* pNode = static_cast<SThreadNode*>(index.internalPointer());
	return pNode->pThread;
}

QVariant CThreadModel::Data(const QModelIndex &index, int role, int section) const
{
	if (!index.isValid())
        return QVariant();

    //if(role == Qt::SizeHintRole )
    //    return QSize(64,16); // for fixing height

	SThreadNode* pNode = static_cast<SThreadNode*>(index.internalPointer());

	switch(role)
	{
		case Qt::DisplayRole:
		{
			SThreadNode::SValue& Value = pNode->Values[section];
			return Value.Formated.isValid() ? Value.Formated : Value.Raw;
		}
		case Qt::EditRole: // sort role
		{
			return pNode->Values[section].Raw;
		}
		case Qt::BackgroundRole:
		{
			//return QBrush(QColor(255,128,128));
			break;
		}
		case Qt::ForegroundRole:
		{
			/* QColor Color = Qt::black;
			return QBrush(Color); */
			break;
		}
		case Qt::UserRole:
		{
			switch(section)
			{
				case eThread:			return pNode->UID;
			}
			break;
		}
	}
	return QVariant();
}

Qt::ItemFlags CThreadModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex CThreadModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (parent.isValid()) 
        return QModelIndex();
	if(m_List.count() > row)
		return createIndex(row, column, m_List[row]);
	return QModelIndex();
}

QModelIndex CThreadModel::parent(const QModelIndex &index) const
{
	return QModelIndex();
}

int CThreadModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    if (parent.isValid())
        return 0;
	return m_List.count();
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
