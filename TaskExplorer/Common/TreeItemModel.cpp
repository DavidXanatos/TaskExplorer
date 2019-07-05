#include "stdafx.h"
#include "TreeItemModel.h"


#define FIRST_COLUMN 0


CTreeItemModel::CTreeItemModel(QObject *parent)
:QAbstractItemModel(parent)
{
	m_bTree = true;
	m_bUseIcons = false;
	m_Root = MkNode(QVariant());
}

CTreeItemModel::~CTreeItemModel()
{
	delete m_Root;
}

QList<QVariant> CTreeItemModel::MakePath(const QVariantMap& Cur, const QMap<QVariant, QVariantMap>& List)
{
	QVariant ParentID = Cur["ParentID"];

	QList<QVariant> list;
	QVariantMap Parent = List.value(ParentID);
	if (!Parent.isEmpty())
	{
		list = MakePath(Parent, List);
		list.append(ParentID);
	}
	return list;
}

void CTreeItemModel::Sync(const QMap<QVariant, QVariantMap>& List)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QMap<QVariant, STreeNode*> Old = m_Map;

	foreach (const QVariantMap& Cur, List)
	{
		QVariant ID = Cur["ID"];

		QModelIndex Index;
		QList<QVariant> Path;
		if(m_bTree)
			Path = MakePath(Cur, List);
		
		STreeNode* pNode = static_cast<STreeNode*>(Old.value(ID));
		if(!pNode || pNode->Path != Path)
		{
			pNode = static_cast<STreeNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->Path = Path;
			pNode->IsBold = Cur["IsBold"].toBool();
			pNode->Icon = Cur["Icon"];
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

		QVariantList Values = Cur["Values"].toList();
		for(int section = FIRST_COLUMN; section < columnCount(); section++)
		{
			QVariant Value = Values[section];

			STreeNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				Changed = true;
				ColValue.Raw = Value;

				//ColValue.Formated = 
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

	CTreeItemModel::Sync(New, Old);
}

void CTreeItemModel::Sync(QMap<QList<QVariant>, QList<STreeNode*> >& New, QMap<QVariant, STreeNode*>& Old)
{
	Purge(m_Root, QModelIndex(), Old);

	if(!New.isEmpty())
	{
		emit layoutAboutToBeChanged();

		//foreach(const QString& Path, New.uniqueKeys())
		for(QMap<QList<QVariant>, QList<STreeNode*> >::const_iterator I = New.begin(); I != New.end(); I++)
			Fill(m_Root, QModelIndex(), I.key(), 0, I.value(), I.key());

		emit layoutChanged();
	}

	emit Updated();
}

void CTreeItemModel::CountItems()
{
	CountItems(m_Root);
}

int CTreeItemModel::CountItems(STreeNode* pRoot)
{
	if(pRoot->Children.isEmpty())
		return 1;
	
	int Counter = 0;
	foreach(STreeNode* pChild, pRoot->Children)
		Counter += CountItems(pChild);
	//pRoot->AllChildren = Counter;
	return Counter;
}

void CTreeItemModel::Purge(STreeNode* pParent, const QModelIndex &parent, QMap<QVariant, STreeNode*> &Old)
{
	int Removed = 0;

	int Begin = -1;
	int End = -1;
	for(int i = pParent->Children.count()-1; i >= -1; i--) 
	{
		STreeNode* pNode = i >= 0 ? pNode = pParent->Children[i] : NULL;
		if(pNode)
			Purge(pNode, index(i, 0, parent), Old);

		bool bRemove = false;
		if(pNode && (pNode->ID.isNull() || (bRemove = Old.value(pNode->ID) != NULL)) && pNode->Children.isEmpty()) // remove it
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
				pNode->ID = QVariant();
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

void CTreeItemModel::Fill(STreeNode* pParent, const QModelIndex &parent, const QList<QVariant>& Paths, int PathsIndex, const QList<STreeNode*>& New, const QList<QVariant>& Path)
{
	if(Paths.size() > PathsIndex)
	{
		QVariant CurPath = Paths.at(PathsIndex);
		STreeNode* pNode;
		int i = pParent->Aux.value(CurPath, -1);
		if(i != -1)
			pNode = pParent->Children[i];
		else
		{
			i = 0;
			pNode = MkNode(QVariant());
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
		for(QList<STreeNode*>::const_iterator I = New.begin(); I != New.end(); I++)
		{
			STreeNode* pNode = *I;
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

QModelIndex CTreeItemModel::FindIndex(const QVariant& ID)
{
	if(STreeNode* pNode = m_Map.value(ID))
		return Find(m_Root, pNode);
	return QModelIndex();
}

QModelIndex CTreeItemModel::Find(STreeNode* pParent, STreeNode* pNode)
{
	for(int i=0; i < pParent->Children.count(); i++)
	{
		if(pParent->Children[i] == pNode)
			return createIndex(i, FIRST_COLUMN, pNode);

		QModelIndex Index = Find(pParent->Children[i], pNode);
		if(Index.isValid())
			return Index;
	}
	return QModelIndex();
}

void CTreeItemModel::Clear()
{
	QMap<QVariant, STreeNode*> Old = m_Map;
	//beginResetModel();
	Purge(m_Root, QModelIndex(), Old);
	//endResetModel();
	ASSERT(m_Map.isEmpty());
}

void CTreeItemModel::RemoveIndex(const QModelIndex &index)
{
	if (!index.isValid())
        return;

	STreeNode* pNode = static_cast<STreeNode*>(index.internalPointer());
	ASSERT(pNode);

	QMap<QVariant, STreeNode*> Old;
	Old[pNode->ID] = pNode;

	Purge(m_Root, QModelIndex(), Old);
}

QVariant CTreeItemModel::data(const QModelIndex &index, int role) const
{
    return Data(index, role, index.column());
}

bool CTreeItemModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(index.column() == FIRST_COLUMN && role == Qt::CheckStateRole)
	{
		STreeNode* pNode = static_cast<STreeNode*>(index.internalPointer());
		ASSERT(pNode);
		emit CheckChanged(pNode->ID, value.toInt() != Qt::Unchecked);
		return true;
	}
	return false;
}

QVariant CTreeItemModel::Data(const QModelIndex &index, int role, int section) const
{
	if (!index.isValid())
        return QVariant();

    //if(role == Qt::SizeHintRole)
    //    return QSize(64,16); // for fixing height

	STreeNode* pNode = static_cast<STreeNode*>(index.internalPointer());
	ASSERT(pNode);

	if (pNode->Values.size() <= section)
		return QVariant();

    switch(role)
	{
		case Qt::DisplayRole:
		{
			STreeNode::SValue& Value = pNode->Values[section];
			return Value.Formated.isValid() ? Value.Formated : Value.Raw;
		}
		case Qt::EditRole: // sort role
		{
			return pNode->Values[section].Raw;
		}
		case Qt::DecorationRole:
		{
			if (m_bUseIcons && section == FIRST_COLUMN)
				return pNode->Icon.isValid() ? pNode->Icon : GetDefaultIcon();
			break;
		}
		case Qt::FontRole:
		{
			if (section == FIRST_COLUMN && pNode->IsBold)
			{
				QFont fnt;
				fnt.setBold(true);
				return fnt;
			}
			break;
		}
		case Qt::BackgroundRole:
		{
			return pNode->Color.isValid() ? pNode->Color : QVariant();
			break;
		}
		case Qt::ForegroundRole:
		{
			if (pNode->IsGray)
			{
				QColor Color = Qt::gray;
				return QBrush(Color);
			}
			break;
		}
		case Qt::CheckStateRole:
		{
			/*if(section == eModule)
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
				case FIRST_COLUMN:			return pNode->ID;
			}
			break;
		}
	}
	return QVariant();
}

Qt::ItemFlags CTreeItemModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
	if(index.column() == 0)
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex CTreeItemModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    STreeNode* pParent;
    if (!parent.isValid())
        pParent = m_Root;
    else
        pParent = static_cast<STreeNode*>(parent.internalPointer());

	if(STreeNode* pNode = pParent->Children.count() > row ? pParent->Children[row] : NULL)
        return createIndex(row, column, pNode);
    return QModelIndex();
}

QModelIndex CTreeItemModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    STreeNode* pNode = static_cast<STreeNode*>(index.internalPointer());
	ASSERT(pNode->Parent);
	STreeNode* pParent = pNode->Parent;
    if (pParent == m_Root)
        return QModelIndex();

	int row = 0;
	if(pParent->Parent)
		row = pParent->Parent->Children.indexOf(pParent);
    return createIndex(row, 0, pParent);
}

int CTreeItemModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

	STreeNode* pNode;
    if (!parent.isValid())
        pNode = m_Root;
    else
        pNode = static_cast<STreeNode*>(parent.internalPointer());
	return pNode->Children.count();
}

int CTreeItemModel::columnCount(const QModelIndex &parent) const
{
	return 1;
}

QVariant CTreeItemModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case FIRST_COLUMN:		return tr("Value");
			// ...
		}
	}
    return QVariant();
}