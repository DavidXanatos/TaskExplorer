#include "stdafx.h"
#include "../TaskExplorer.h"
#include "GDIModel.h"
#include "../../Common/Common.h"

CGDIModel::CGDIModel(QObject *parent)
:CListItemModel(parent)
{
}

CGDIModel::~CGDIModel()
{
}

void CGDIModel::Sync(QMap<quint64, CWinGDIPtr> List)
{
	QList<SListNode*> New;
	QHash<QVariant, SListNode*> Old = m_Map;

	foreach (const CWinGDIPtr& pGDI, List)
	{
		QVariant ID = (quint32)pGDI->GetHandleId();

		int Row = -1;
		QHash<QVariant, SListNode*>::iterator I = Old.find(ID);
		SGDINode* pNode = I != Old.end() ? static_cast<SGDINode*>(I.value()) : NULL;
		if(!pNode)
		{
			pNode = static_cast<SGDINode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pGDI = pGDI;
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
		if (pGDI->IsMarkedForRemoval() && CTaskExplorer::UseListColor(CTaskExplorer::eToBeRemoved))		RowColor = CTaskExplorer::eToBeRemoved;
		else if (pGDI->IsNewlyCreated() && CTaskExplorer::UseListColor(CTaskExplorer::eAdded))			RowColor = CTaskExplorer::eAdded;

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
				case eHandle:			Value = (quint32)pGDI->GetHandleId(); break;
				case eProcess:			Value = pGDI->GetProcessId();  break;
				case eType:				Value = pGDI->GetTypeString(); break;
				case eObject:			Value = pGDI->GetObject(); break;
				case eInformation:		Value = pGDI->GetInformations(); break;
			}

			SGDINode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
					case eProcess:	ColValue.Formated = tr("%1 (%2)").arg(pGDI->GetProcessName()).arg(pGDI->GetProcessId()); 
					case eHandle:	ColValue.Formated = "0x" + QString::number(Value.toUInt(), 16); break;
                    case eObject:	ColValue.Formated = FormatAddress(Value.toULongLong()); break;
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

CWinGDIPtr CGDIModel::GetGDI(const QModelIndex &index) const
{
	if (!index.isValid())
        return CWinGDIPtr();

	SGDINode* pNode = static_cast<SGDINode*>(index.internalPointer());
	return pNode->pGDI;
}

int CGDIModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CGDIModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eHandle:			return tr("Handle");
			case eProcess:			return tr("Process");
			case eType:				return tr("Type");
			case eObject:			return tr("Object");
			case eInformation:		return tr("Informations");
		}
	}
    return QVariant();
}
