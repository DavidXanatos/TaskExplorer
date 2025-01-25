#include "stdafx.h"
#include "../TaskExplorer.h"
#include "HeapModel.h"
#include "../../../MiscHelpers/Common/Common.h"

CHeapModel::CHeapModel(QObject *parent)
:CListItemModel(parent)
{
}

CHeapModel::~CHeapModel()
{
}

void CHeapModel::Sync(QMap<quint64, CHeapPtr> List)
{
	QList<SListNode*> New;
	QHash<QVariant, SListNode*> Old = m_Map;

	foreach (const CHeapPtr& pHeap, List)
	{
		QVariant ID = pHeap->GetBaseAddress();

		int Row = -1;
		QHash<QVariant, SListNode*>::iterator I = Old.find(ID);
		SHeapNode* pNode = I != Old.end() ? static_cast<SHeapNode*>(I.value()) : NULL;
		if(!pNode)
		{
			pNode = static_cast<SHeapNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pHeap = pHeap;
			New.append(pNode);
		}
		else
		{
			I.value() = NULL;
			Row = GetRow(pNode);
		}

		int Col = 0;
		bool State = false;
		int Changed = 0;

		int RowColor = CTaskExplorer::eNone;
		if (pHeap->IsMarkedForRemoval() && CTaskExplorer::UseListColor(CTaskExplorer::eToBeRemoved))		RowColor = CTaskExplorer::eToBeRemoved;
		else if (pHeap->IsNewlyCreated() && CTaskExplorer::UseListColor(CTaskExplorer::eAdded))			RowColor = CTaskExplorer::eAdded;

		if (pNode->iColor != RowColor) {
			pNode->iColor = RowColor;
			pNode->Color = CTaskExplorer::GetListColor(RowColor);
			Changed = 2;
		}

		for(int section = 0; section < columnCount(); section++)
		{
			if (!m_Columns.contains(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch(section)
			{
				case eAddress:	Value = pHeap->GetBaseAddress(); break;	
				case eUsed:		Value = pHeap->GetBytesAllocated(); break;
				case eCommited: Value = pHeap->GetBytesCommitted(); break;
				case eEntries:	Value = pHeap->GetNumberOfEntries(); break;
				case eFlags:	Value = pHeap->GetFlags(); break;
				case eClass:	Value = pHeap->GetClass(); break;
				case eType:		Value = pHeap->GetType(); break;
			}

			SHeapNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
					case eAddress:	ColValue.Formated = FormatAddress(Value.toULongLong()); break;
					case eUsed:
					case eCommited:
									ColValue.Formated = FormatSize(Value.toULongLong()); break;	
					case eEntries:	ColValue.Formated = FormatNumber(Value.toULongLong()); break;
					case eFlags:	ColValue.Formated = pHeap->GetFlagsString(); break;
					case eClass:	ColValue.Formated = pHeap->GetClassString(); break;
					case eType:		ColValue.Formated = pHeap->GetTypeString(); break;
				}
			}

			if(State != (Changed != 0))
			{
				if(State && Row != -1)
					emit dataChanged(createIndex(Row, Col), createIndex(Row, section-1));
				State = (Changed != 0);
				Col = section;
			}
			if(Changed == 1)
				Changed = 0;
		}
		if(State && Row != -1)
			emit dataChanged(createIndex(Row, Col, pNode), createIndex(Row, columnCount()-1, pNode));

	}

	CListItemModel::Sync(New, Old);
}

CHeapPtr CHeapModel::GetHeap(const QModelIndex &index) const
{
	if (!index.isValid())
        return CHeapPtr();

	SHeapNode* pNode = static_cast<SHeapNode*>(index.internalPointer());
	return pNode->pHeap;
}

int CHeapModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CHeapModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eAddress: return tr("Address");
			case eUsed: return tr("Used");
			case eCommited: return tr("Commited");
			case eEntries: return tr("Entries");
			case eFlags: return tr("Flags");
			case eClass: return tr("Class");
			case eType: return tr("Type");
		}
	}
    return QVariant();
}
