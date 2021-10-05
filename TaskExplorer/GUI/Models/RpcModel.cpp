#include "stdafx.h"
#include "../TaskExplorer.h"
#include "RpcModel.h"
#include "../../../MiscHelpers/Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinDriver.h"
#endif

CRpcModel::CRpcModel(QObject *parent)
:CListItemModel(parent)
{
}

CRpcModel::~CRpcModel()
{
}

void CRpcModel::Sync(const QMap<QString, CRpcEndpointPtr>& RpcEndpointList)
{
	QList<SListNode*> New;
	QHash<QVariant, SListNode*> Old = m_Map;

	foreach (const CRpcEndpointPtr & pRpcEndpoint, RpcEndpointList)
	{
		QVariant ID = pRpcEndpoint->GetIfId() + " " + pRpcEndpoint->GetBinding();

		int Row = -1;
		QHash<QVariant, SListNode*>::iterator I = Old.find(ID);
		SRpcEndpointNode* pNode = I != Old.end() ? static_cast<SRpcEndpointNode*>(I.value()) : NULL;
		if(!pNode)
		{
			pNode = static_cast<SRpcEndpointNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pRpcEndpoint = pRpcEndpoint;
			New.append(pNode);
		}
		else
		{
			I.value() = NULL;
			Row = GetRow(pNode);
		}

		int Col = 0;
		bool State = false;
		bool Changed = false;

		for(int section = 0; section < columnCount(); section++)
		{
			if (!m_Columns.contains(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch(section)
			{
				case eIfId:				Value = pRpcEndpoint->GetIfId(); break;
				case eDescription:		Value = pRpcEndpoint->GetDescription(); break;
				case eBinding:			Value = pRpcEndpoint->GetBinding(); break;
			}

			SRpcEndpointNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				Changed = true;
				ColValue.Raw = Value;

				/*switch (section)
				{
      
				}*/
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

CRpcEndpointPtr CRpcModel::GetRpcEndpoint(const QModelIndex &index) const
{
	if (!index.isValid())
        return CRpcEndpointPtr();

	SRpcEndpointNode* pNode = static_cast<SRpcEndpointNode*>(index.internalPointer());
	return pNode->pRpcEndpoint;
}

int CRpcModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CRpcModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eIfId:				return tr("IfId");
			case eDescription:		return tr("Description");
			case eBinding:			return tr("Binding");
		}
	}
    return QVariant();
}
