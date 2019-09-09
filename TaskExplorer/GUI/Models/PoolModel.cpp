#include "stdafx.h"
#include "../TaskExplorer.h"
#include "PoolModel.h"
#include "../../Common/Common.h"

CPoolModel::CPoolModel(QObject *parent)
:CListItemModel(parent)
{
}

CPoolModel::~CPoolModel()
{
}

void CPoolModel::Sync(QMap<quint64, CPoolEntryPtr> PoolEntryList)
{
	bool bClearZeros = theConf->GetBool("Options/ClearZeros", true);

	QList<SListNode*> New;
	QHash<QVariant, SListNode*> Old = m_Map;

	foreach (const CPoolEntryPtr& pPoolEntry, PoolEntryList)
	{
		QVariant ID = (quint64)pPoolEntry.data();

		int Row = -1;
		SPoolEntryNode* pNode = static_cast<SPoolEntryNode*>(Old[ID]);
		if(!pNode)
		{
			pNode = static_cast<SPoolEntryNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pPoolEntry = pPoolEntry;
			New.append(pNode);
		}
		else
		{
			Old[ID] = NULL;
			Row = GetRow(pNode);
		}

		int Col = 0;
		bool State = false;
		bool Changed = false;

		SPoolEntryStats EntryStats = pPoolEntry->GetEntryStats();

		for(int section = 0; section < columnCount(); section++)
		{
			if (!m_Columns.contains(section))
				continue; // ignore columns which are hidden

			// todo: xxx use the deltas show delta / value ?

			QVariant Value;
			switch(section)
			{
    			case eTagName:			Value = pPoolEntry->GetTagString(); break;
    			case eDriver:			Value = pPoolEntry->GetDriver(); break;
    			case eDescription:		Value = pPoolEntry->GetDescription(); break;
    			case ePagedAllocs:		Value = EntryStats.PagedAllocsDelta.Value; break;
    			case ePagedFrees:		Value = EntryStats.PagedFreesDelta.Value; break;
    			case ePagedCurrent:		Value = EntryStats.PagedCurrentDelta.Value; break;
    			case ePagedBytes:		Value = EntryStats.PagedTotalSizeDelta.Value; break;
    			case eNonPagedAllocs:	Value = EntryStats.NonPagedAllocsDelta.Value; break;
    			case eNonPagedFrees:	Value = EntryStats.NonPagedFreesDelta.Value; break;
    			case eNonPagedCurrent:	Value = EntryStats.NonPagedCurrentDelta.Value; break;
    			case eNonPagedBytes:	Value = EntryStats.NonPagedTotalSizeDelta.Value; break;
			}

			SPoolEntryNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				Changed = true;
				ColValue.Raw = Value;

				switch (section)
				{
    			case ePagedAllocs:		
    			case ePagedFrees:		
    			case ePagedCurrent:		
    			case eNonPagedAllocs:	
    			case eNonPagedFrees:	
    			case eNonPagedCurrent:	
										ColValue.Formated = FormatNumberEx(Value.toULongLong(), bClearZeros); break;

				case ePagedBytes:		
    			case eNonPagedBytes:	
										ColValue.Formated = FormatSizeEx(Value.toULongLong(), bClearZeros); break;
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

CPoolEntryPtr CPoolModel::GetPoolEntry(const QModelIndex &index) const
{
	if (!index.isValid())
        return CPoolEntryPtr();

	SPoolEntryNode* pNode = static_cast<SPoolEntryNode*>(index.internalPointer());
	return pNode->pPoolEntry;
}

int CPoolModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CPoolModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
    		case eTagName:			return tr("Tag Name");
    		case eDriver:			return tr("Driver");
    		case eDescription:		return tr("Description");
    		case ePagedAllocs:		return tr("Paged Allocations");
    		case ePagedFrees:		return tr("Paged Frees");
    		case ePagedCurrent:		return tr("Paged Current");
    		case ePagedBytes:		return tr("Paged Bytes Total");
    		case eNonPagedAllocs:	return tr("Non-paged Allocations");
    		case eNonPagedFrees:	return tr("Non-paged Frees");
    		case eNonPagedCurrent:	return tr("Non-paged Current");
    		case eNonPagedBytes:	return tr("Non-paged Bytes Total");
		}
	}
    return QVariant();
}

