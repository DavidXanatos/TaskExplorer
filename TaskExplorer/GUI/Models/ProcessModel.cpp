#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ProcessModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinProcess.h"
#endif



CProcessModel::CProcessModel(QObject *parent)
:QAbstractItemModel(parent)
{
	m_bTree = false;
	m_bUseDescr = true;
	m_Root = new SProcessNode(-1);
}

CProcessModel::~CProcessModel()
{
	delete m_Root;
}

QList<quint64> CProcessModel::MakeProcPath(const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList)
{
	quint64 ParentPID = pProcess->GetParentID();

	QList<quint64> list;
	if (ParentPID != pProcess->GetID())
	{
		CProcessPtr pParent = ProcessList.value(ParentPID);
		if (!pParent.isNull())
		{
			list = MakeProcPath(pParent, ProcessList);
			list.append(ParentPID);
		}
	}
	return list;
}

void CProcessModel::Sync(QMap<quint64, CProcessPtr> ProcessList)
{
	QMap<QList<quint64>, QList<SProcessNode*> > New;
	QMap<quint64, SProcessNode*> Old = m_Map;

	foreach (const CProcessPtr& pProcess, ProcessList)
	{
		quint64 ID = pProcess->GetID();

		QModelIndex Index;
		QList<quint64> Path;
		if(m_bTree)
			Path = MakeProcPath(pProcess, ProcessList);
		
		SProcessNode* pNode = Old.value(ID);
		if(!pNode || pNode->Path != Path)
		{
			pNode = new SProcessNode(ID);
			pNode->Values.resize(columnCount());
			pNode->Path = Path;
			pNode->pProcess = pProcess;
			New[Path].append(pNode);
		}
		else
		{
			Old[ID] = NULL;
			Index = Find(m_Root, pNode);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

		int Col = 0;
		bool State = false;
		bool Changed = false;

		// Note: icons are loaded asynchroniusly
		if (!pNode->Icon.isValid())
		{
			QPixmap Icon = pNode->pProcess->GetFileIcon();
			if (!Icon.isNull()) {
				Changed = true; // set change for first column
				pNode->Icon = Icon;
			}
		}

#ifdef WIN32
		CWinProcess* pWinProc = qobject_cast<CWinProcess*>(pProcess.data());
#endif

		for(int section = eProcess; section < columnCount(); section++)
		{
			QVariant Value;
			switch(section)
			{
				case eProcess:		
					if (m_bUseDescr)
					{
						QString Descr = pWinProc->GetDescription();
						if (!Descr.isEmpty())
						{
							Value =  Descr + " (" + pNode->pProcess->GetName() + ")";
							break;
						}
					}
									Value = pNode->pProcess->GetName(); break;
				case ePID:			Value = pNode->pProcess->GetID(); break;
				//case eCPU:			Value = ; break;
				//case eIO_Rate:		Value = ; break;
				case ePrivateBytes:	Value = pProcess->GetWorkingSetPrivateSize(); break;
#ifdef WIN32
				case eDescription:	Value = pWinProc->GetDescription(); break;
#endif
				case eStartTime:	Value = pProcess->GetCreateTime(); break;
				case eThreads:		Value = (quint32)pProcess->GetNumberOfThreads(); break;
				case ePriority:		Value = (quint32)pProcess->GetPriorityClass(); break;
#ifdef WIN32
				case eIntegrity:	Value = pWinProc->IntegrityLevel(); break;
				case eHandles:		Value = (quint32)pWinProc->GetNumberOfHandles(); break;
				case eOS_Ver:		Value = (quint32)pWinProc->GetOsContextVersion(); break;
				case eGDI:			Value = (quint32)pWinProc->GetGdiHandles(); break;
				case eUSER:			Value = (quint32)pWinProc->GetUserHandles(); break;
				case eSession:		Value = pWinProc->GetSessionID(); break;
#endif
				case eUserName:		Value = pProcess->GetUserName(); break;
				//case eIO_Reads:		Value = ; break;
				//case eIO_Writes:	Value = ; break;
				//case eIO_Other:		Value = ; break;
				case eFileName:		Value = pProcess->GetFileName(); break;
			}

			SProcessNode::SValue& ColValue = pNode->Values[section];

			bool Changed = false;
			if (ColValue.Raw != Value)
			{
				Changed = true;
				ColValue.Raw = Value;

				switch (section)
				{
					case ePrivateBytes:		ColValue.Formated = FormatSize(pProcess->GetWorkingSetPrivateSize()); break;
					case eStartTime:		ColValue.Formated = pProcess->GetCreateTime().toString("dd.MM.yyyy hh:mm:ss"); break;
					case ePriority:			ColValue.Formated = pProcess->GetPriorityString(); break;
#ifdef WIN32
					case eIntegrity:		ColValue.Formated = pWinProc->GetIntegrityString(); break;
					case eOS_Ver:			ColValue.Formated = pWinProc->GetOsContextString(); break;
#endif
				}
			}

			if(State != Changed)
			{
				if(State && Index.isValid())
					emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), section-1, pNode));
				State = Changed;
				Col = section;
			}
			Changed = false;
		}
		if(State && Index.isValid())
			emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), columnCount()-1, pNode));

	}

	Sync(New, Old);
}

void CProcessModel::Sync(QMap<QList<quint64>, QList<SProcessNode*> >& New, QMap<quint64, SProcessNode*>& Old)
{
	Purge(m_Root, QModelIndex(), Old);

	if(!New.isEmpty())
	{
		emit layoutAboutToBeChanged();

		//foreach(const QString& Path, New.uniqueKeys())
		for(QMap<QList<quint64>, QList<SProcessNode*> >::const_iterator I = New.begin(); I != New.end(); I++)
			Fill(m_Root, QModelIndex(), I.key(), 0, I.value(), I.key());

		emit layoutChanged();
	}

	emit Updated();
}

void CProcessModel::CountItems()
{
	CountItems(m_Root);
}

int CProcessModel::CountItems(SProcessNode* pRoot)
{
	if(pRoot->Children.isEmpty())
		return 1;
	
	int Counter = 0;
	foreach(SProcessNode* pChild, pRoot->Children)
		Counter += CountItems(pChild);
	//pRoot->AllChildren = Counter;
	return Counter;
}

void CProcessModel::Purge(SProcessNode* pParent, const QModelIndex &parent, QMap<quint64, SProcessNode*> &Old)
{
	int Removed = 0;

	int Begin = -1;
	int End = -1;
	for(int i = pParent->Children.count()-1; i >= -1; i--) 
	{
		SProcessNode* pNode = i >= 0 ? pNode = pParent->Children[i] : NULL;
		if(pNode)
			Purge(pNode, index(i, 0, parent), Old);

		bool bRemove = false;
		if(pNode && (pNode->ID == -1 || (bRemove = Old.value(pNode->ID) != NULL)) && pNode->Children.isEmpty()) // remove it
		{
			m_Map.remove(pNode->ID, pNode);
			if(End == -1)
				End = i;
		}
		else // keep it
		{
			if(bRemove)
			{
				ASSERT(!pNode->Children.isEmpty()); // we wanted to remove it but we have to keep it
				m_Map.remove(pNode->ID, pNode);
				pNode->ID = -1;
				pNode->Icon.clear();
			}

			if(End != -1) // remove whats to be removed at once
			{
				Begin = i + 1;

				beginRemoveRows(parent, Begin, End);
				//ASSERT(pParent->Children.count() > End);
				for(int j = End; j >= Begin; j--)
				{
					pNode = pParent->Children.takeAt(j);
					delete pNode;
					Removed++;
				}
				endRemoveRows();

				End = -1;
				Begin = -1;
			}
		}
    }

	if(Removed > 0)
	{
		pParent->Aux.clear();
		for(int i = pParent->Children.count()-1; i >= 0; i--) 
			pParent->Aux.insert(pParent->Children[i]->ID, i);
	}
}

void CProcessModel::Fill(SProcessNode* pParent, const QModelIndex &parent, const QList<quint64>& Paths, int PathsIndex, const QList<SProcessNode*>& New, const QList<quint64>& Path)
{
	if(Paths.size() > PathsIndex)
	{
		quint64 CurPath = Paths.at(PathsIndex);
		SProcessNode* pNode;
		int i = pParent->Aux.value(CurPath, -1);
		if(i != -1)
			pNode = pParent->Children[i];
		else
		{
			i = 0;
			pNode = new SProcessNode(-1);
			pNode->Parent = pParent;
			pNode->Values.resize(columnCount());

			//int Count = pParent->Children.count();
			//beginInsertRows(parent, Count, Count);
			pParent->Aux.insert(pNode->ID, pParent->Children.size());
			pParent->Children.append(pNode);
			//endInsertRows();
		}
		Fill(pNode, index(i, 0, parent), Paths, PathsIndex + 1, New, Path);
	}
	else
	{
		for(QList<SProcessNode*>::const_iterator I = New.begin(); I != New.end(); I++)
		{
			SProcessNode* pNode = *I;
			ASSERT(pNode);
			//ASSERT(!m_Map.contains(pNode->ID));
			m_Map.insert(pNode->ID, pNode);
			pNode->Parent = pParent;

			//int Count = pParent->Children.count();
			//beginInsertRows(parent, Count, Count);
			pParent->Aux.insert(pNode->ID, pParent->Children.size());
			pParent->Children.append(pNode);
			//endInsertRows();
		}
	}
}

QModelIndex CProcessModel::FindIndex(quint64 ID)
{
	if(SProcessNode* pNode = m_Map.value(ID))
		return Find(m_Root, pNode);
	return QModelIndex();
}

QModelIndex CProcessModel::Find(SProcessNode* pParent, SProcessNode* pNode)
{
	for(int i=0; i < pParent->Children.count(); i++)
	{
		if(pParent->Children[i] == pNode)
			return createIndex(i, eProcess, pNode);

		QModelIndex Index = Find(pParent->Children[i], pNode);
		if(Index.isValid())
			return Index;
	}
	return QModelIndex();
}

void CProcessModel::Clear()
{
	QMap<quint64, SProcessNode*> Old = m_Map;
	//beginResetModel();
	Purge(m_Root, QModelIndex(), Old);
	//endResetModel();
	ASSERT(m_Map.isEmpty());
}

CProcessPtr CProcessModel::GetProcess(const QModelIndex &index) const
{
	if (!index.isValid())
        return CProcessPtr();

	SProcessNode* pNode = static_cast<SProcessNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pProcess;
}

QVariant CProcessModel::data(const QModelIndex &index, int role) const
{
    return Data(index, role, index.column());
}

bool CProcessModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(index.column() == 0 && role == Qt::CheckStateRole)
	{
		SProcessNode* pNode = static_cast<SProcessNode*>(index.internalPointer());
		ASSERT(pNode);
		emit CheckChanged(pNode->ID, value.toInt() != Qt::Unchecked);
		return true;
	}
	return false;
}

QVariant CProcessModel::Data(const QModelIndex &index, int role, int section) const
{
	if (!index.isValid())
        return QVariant();

    //if(role == Qt::SizeHintRole)
    //    return QSize(64,16); // for fixing height

	SProcessNode* pNode = static_cast<SProcessNode*>(index.internalPointer());
	ASSERT(pNode);

    switch(role)
	{
		case Qt::DisplayRole:
		{
			SProcessNode::SValue& Value = pNode->Values[section];
			return Value.Formated.isValid() ? Value.Formated : Value.Raw;
		}
		case Qt::EditRole: // sort role
		{
			return pNode->Values[section].Raw;
		}
		case Qt::DecorationRole:
		{
			if (section == eProcess)
				return pNode->Icon.isValid() ? pNode->Icon : g_ExeIcon;
			break;
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
		case Qt::CheckStateRole:
		{
			/*if(section == eProcess)
			{
				if(pNode->...)
					return Qt::Unchecked;
				else
					return Qt::Checked;
			}*/
			break;
        }
		case Qt::UserRole:
		{
			switch(section)
			{
				case eProcess:			return pNode->ID;
			}
			break;
		}
	}
	return QVariant();
}

Qt::ItemFlags CProcessModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
	if(index.column() == 0)
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex CProcessModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    SProcessNode* pParent;
    if (!parent.isValid())
        pParent = m_Root;
    else
        pParent = static_cast<SProcessNode*>(parent.internalPointer());

	if(SProcessNode* pNode = pParent->Children.count() > row ? pParent->Children[row] : NULL)
        return createIndex(row, column, pNode);
    return QModelIndex();
}

QModelIndex CProcessModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    SProcessNode* pNode = static_cast<SProcessNode*>(index.internalPointer());
	ASSERT(pNode->Parent);
	SProcessNode* pParent = pNode->Parent;
    if (pParent == m_Root)
        return QModelIndex();

	int row = 0;
	if(pParent->Parent)
		row = pParent->Parent->Children.indexOf(pParent);
    return createIndex(row, 0, pParent);
}

int CProcessModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

	SProcessNode* pNode;
    if (!parent.isValid())
        pNode = m_Root;
    else
        pNode = static_cast<SProcessNode*>(parent.internalPointer());
	return pNode->Children.count();
}

int CProcessModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CProcessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eProcess:		return tr("Process");
			case ePID:			return tr("PID");
			case eCPU:			return tr("CPU");
			case eIO_Rate:		return tr("I/O total rate");
			case ePrivateBytes:	return tr("Private bytes");
#ifdef WIN32
			case eDescription:	return tr("Description");
#endif
			case eStartTime:	return tr("Start time");
			case eThreads:		return tr("Threads");
			case ePriority:		return tr("Priority");
#ifdef WIN32
			case eIntegrity:	return tr("Integrity");
			case eHandles:		return tr("Handles");
			case eOS_Ver:		return tr("OS ver.");
			case eGDI:			return tr("GDI");
			case eUSER:			return tr("USER");
			case eSession:		return tr("Session");
#endif
			case eUserName:		return tr("User name");
			case eIO_Reads:		return tr("I/O reads");
			case eIO_Writes:	return tr("I/O writes");
			case eIO_Other:		return tr("I/O other");
			case eFileName:		return tr("File name");
		}
	}
    return QVariant();
}
