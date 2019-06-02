#include "stdafx.h"
#include "../TaskExplorer.h"
#include "HandleModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinHandle.h"
#endif

CHandleModel::CHandleModel(QObject *parent)
:QAbstractItemModel(parent)
{
}

CHandleModel::~CHandleModel()
{
	foreach(SHandleNode* pNode, m_List)
		delete pNode;
}

void CHandleModel::Sync(QMap<quint64, CHandlePtr> HandleList)
{
	QList<SHandleNode*> New;
	QMap<quint64, SHandleNode*> Old = m_Map;

	foreach (const CHandlePtr& pHandle, HandleList)
	{
		quint64 UID = (quint64)pHandle.data();

		int Row = -1;
		SHandleNode* pNode = Old[UID];
		if(!pNode)
		{
			pNode = new SHandleNode();
			pNode->Values.resize(columnCount());
			pNode->UID = UID;
			pNode->pHandle = pHandle;
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
		CWinHandle* pWinHandle = qobject_cast<CWinHandle*>(pHandle.data());
#endif

		for(int section = eHandle; section < columnCount(); section++)
		{
			QVariant Value;
			switch(section)
			{
				case eHandle:			Value = pHandle->GetHandleId(); break;
				case eType:				Value = pHandle->GetTypeString(); break;
				case eName:				Value = pHandle->GetFileName(); break;
				case ePosition:			Value = pHandle->GetPosition(); break;	
				case eSize:				Value = pHandle->GetSize(); break;	
				case eGrantedAccess:	Value = pHandle->GetGrantedAccessString(); break;
#ifdef WIN32
				case eFileShareAccess:	Value = pWinHandle->GetFileShareAccessString(); break;	
				case eAttributes:		Value = pWinHandle->GetAttributesString(); break;	
				case eObjectAddress:	Value = pWinHandle->GetObject(); break;	
				case eOriginalName:		Value = pWinHandle->GetOriginalName(); break;	
#endif
			}

			SHandleNode::SValue& ColValue = pNode->Values[section];

			bool Changed = false;
			if (ColValue.Raw != Value)
			{
				Changed = true;
				ColValue.Raw = Value;

				switch (section)
				{
					case eHandle:			ColValue.Formated = "0x" + QString::number(pHandle->GetHandleId(), 16); break;
#ifdef WIN32
					case eObjectAddress:	ColValue.Formated = "0x" + QString::number(pWinHandle->GetObject(), 16); break;	
#endif
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

void CHandleModel::Sync(QList<SHandleNode*>& New, QMap<quint64, SHandleNode*>& Old)
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
	for(QList<SHandleNode*>::iterator I = New.begin(); I != New.end(); I++)
	{
		SHandleNode* pNode = *I;
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

QModelIndex CHandleModel::FindIndex(quint64 SubID)
{
	if(SHandleNode* pNode = m_Map.value(SubID))
	{
		int row = m_List.indexOf(pNode);
		ASSERT(row != -1);
		return createIndex(row, eHandle, pNode);
	}
	return QModelIndex();
}

void CHandleModel::Clear()
{
	//beginResetModel();
	beginRemoveRows(QModelIndex(), 0, rowCount());
	foreach(SHandleNode* pNode, m_List)
		delete pNode;
	m_List.clear();
	m_Map.clear();
	endRemoveRows();
	//endResetModel();
}

QVariant CHandleModel::data(const QModelIndex &index, int role) const
{
    return Data(index, role, index.column());
}

CHandlePtr CHandleModel::GetHandle(const QModelIndex &index) const
{
	if (!index.isValid())
        return CHandlePtr();

	SHandleNode* pNode = static_cast<SHandleNode*>(index.internalPointer());
	return pNode->pHandle;
}

QVariant CHandleModel::Data(const QModelIndex &index, int role, int section) const
{
	if (!index.isValid())
        return QVariant();

    //if(role == Qt::SizeHintRole )
    //    return QSize(64,16); // for fixing height

	SHandleNode* pNode = static_cast<SHandleNode*>(index.internalPointer());

	switch(role)
	{
		case Qt::DisplayRole:
		{
			SHandleNode::SValue& Value = pNode->Values[section];
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
				case eHandle:			return pNode->UID;
			}
			break;
		}
	}
	return QVariant();
}

Qt::ItemFlags CHandleModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex CHandleModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (parent.isValid()) 
        return QModelIndex();
	if(m_List.count() > row)
		return createIndex(row, column, m_List[row]);
	return QModelIndex();
}

QModelIndex CHandleModel::parent(const QModelIndex &index) const
{
	return QModelIndex();
}

int CHandleModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    if (parent.isValid())
        return 0;
	return m_List.count();
}

int CHandleModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CHandleModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eHandle:				return tr("Handle");
			case eType:					return tr("Type");
			case eName:					return tr("File Name");
			case ePosition:				return tr("Position");
			case eSize:					return tr("Size");
			case eGrantedAccess:		return tr("Granted access");
#ifdef WIN32
			case eFileShareAccess:		return tr("File share access");
			case eAttributes:			return tr("Attributes");
			case eObjectAddress:		return tr("Object address");
			case eOriginalName:			return tr("Original name");
#endif
		}
	}
    return QVariant();
}
