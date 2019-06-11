#include "stdafx.h"
#include "ListItemModel.h"

#define FIRST_COLUMN 0

CListItemModel::CListItemModel(QObject *parent)
:QAbstractItemModel(parent)
{
}

CListItemModel::~CListItemModel()
{
	foreach(SListNode* pNode, m_List)
		delete pNode;
}

void CListItemModel::Sync(QList<QVariantMap> List)
{
	QList<SListNode*> New;
	QMap<QVariant, SListNode*> Old = m_Map;

	foreach (const QVariantMap& Cur, List)
	{
		QVariant ID = Cur["ID"];

		int Row = -1;
		SListNode* pNode = static_cast<SListNode*>(Old[ID]);
		if(!pNode)
		{
			pNode = static_cast<SListNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->IsBold = Cur["IsBold"].toBool();
			pNode->Icon = Cur["Icon"];
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

		QVariantList Values = Cur["Values"].toList();
		for(int section = FIRST_COLUMN; section < columnCount(); section++)
		{
			QVariant Value = Values[section];

			SListNode::SValue& ColValue = pNode->Values[section];

			bool Changed = false;
			if (ColValue.Raw != Value)
			{
				Changed = true;
				ColValue.Raw = Value;

				//ColValue.Formated = 
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

void CListItemModel::Sync(QList<SListNode*>& New, QMap<QVariant, SListNode*>& Old)
{
	int Begin = -1;
	int End = -1;
	for(int i = m_List.count()-1; i >= -1; i--) 
	{
		QVariant ID = i >= 0 ? m_List[i]->ID : QVariant();
		if(ID.isNull() && (Old.value(ID) != NULL)) // remove it
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
	for(QList<SListNode*>::iterator I = New.begin(); I != New.end(); I++)
	{
		SListNode* pNode = *I;
		m_Map.insert(pNode->ID, pNode);
		m_List.append(pNode);
	}
	End = m_List.count();
	if(Begin < End)
	{
		beginInsertRows(QModelIndex(), Begin, End-1);
		endInsertRows();
	}
}

QModelIndex CListItemModel::FindIndex(const QString& SubID)
{
	if(SListNode* pNode = m_Map.value(SubID))
	{
		int row = m_List.indexOf(pNode);
		ASSERT(row != -1);
		return createIndex(row, FIRST_COLUMN, pNode);
	}
	return QModelIndex();
}

void CListItemModel::Clear()
{
	//beginResetModel();
	beginRemoveRows(QModelIndex(), 0, rowCount());
	foreach(SListNode* pNode, m_List)
		delete pNode;
	m_List.clear();
	m_Map.clear();
	endRemoveRows();
	//endResetModel();
}

QVariant CListItemModel::data(const QModelIndex &index, int role) const
{
    return Data(index, role, index.column());
}

QVariant CListItemModel::Data(const QModelIndex &index, int role, int section) const
{
	if (!index.isValid())
        return QVariant();

    //if(role == Qt::SizeHintRole )
    //    return QSize(64,16); // for fixing height

	SListNode* pNode = static_cast<SListNode*>(index.internalPointer());

	switch(role)
	{
		case Qt::DisplayRole:
		{
			SListNode::SValue& Value = pNode->Values[section];
			return Value.Formated.isValid() ? Value.Formated : Value.Raw;
		}
		case Qt::EditRole: // sort role
		{
			return pNode->Values[section].Raw;
		}
		case Qt::DecorationRole:
		{
			if (section == FIRST_COLUMN)
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
				case FIRST_COLUMN:			return pNode->ID;
			}
			break;
		}
	}
	return QVariant();
}

Qt::ItemFlags CListItemModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return 0;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex CListItemModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    if (parent.isValid()) 
        return QModelIndex();
	if(m_List.count() > row)
		return createIndex(row, column, m_List[row]);
	return QModelIndex();
}

QModelIndex CListItemModel::parent(const QModelIndex &index) const
{
	return QModelIndex();
}

int CListItemModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    if (parent.isValid())
        return 0;
	return m_List.count();
}

int CListItemModel::columnCount(const QModelIndex &parent) const
{
	return 1;
}

QVariant CListItemModel::headerData(int section, Qt::Orientation orientation, int role) const
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