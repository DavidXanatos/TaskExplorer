#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ServiceModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinService.h"
#endif

CServiceModel::CServiceModel(QObject *parent)
:CListItemModel(parent)
{
}

CServiceModel::~CServiceModel()
{
}

void CServiceModel::Sync(QMap<QString, CServicePtr> ServiceList)
{
	QList<SListNode*> New;
	QMap<QVariant, SListNode*> Old = m_Map;

	foreach (const CServicePtr& pService, ServiceList)
	{
		QVariant ID = pService->GetName();

		int Row = -1;
		SServiceNode* pNode = static_cast<SServiceNode*>(Old[ID]);
		if(!pNode)
		{
			pNode = static_cast<SServiceNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pService = pService;
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
		CWinService* pWinService = qobject_cast<CWinService*>(pService.data());
#endif

		for(int section = eService; section < columnCount(); section++)
		{
			QVariant Value;
			switch(section)
			{
				case eService:				Value = pService->GetName(); break;
#ifdef WIN32
				case eDisplayName:			Value = pWinService->GetDisplayName(); break;
				case eType:					Value = pWinService->GetTypeString(); break;
#endif
				case eStatus:				Value = pService->GetStateString(); break;
#ifdef WIN32
				case eStartType:			Value = pWinService->GetStartTypeString(); break;
				case ePID:					Value = (int)pWinService->GetPID(); break;
#endif
				case eFileName:				Value = pService->GetFileName(); break;
#ifdef WIN32
				case eErrorControl:			Value = pWinService->GetErrorControlString(); break;
				case eGroupe:				Value = pWinService->GetGroupeName(); break;
				case eBinaryPath:			Value = pWinService->GetBinaryPath(); break;
#endif
			}

			SServiceNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				Changed = true;
				ColValue.Raw = Value;

				/*switch (section)
				{
					case ePID:					ColValue.Formated = 
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

CServicePtr CServiceModel::GetService(const QModelIndex &index) const
{
	if (!index.isValid())
        return CServicePtr();

	SServiceNode* pNode = static_cast<SServiceNode*>(index.internalPointer());
	return pNode->pService;
}

int CServiceModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CServiceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eService:				return tr("Service");
#ifdef WIN32
			case eDisplayName:			return tr("Display Name");
			case eType:					return tr("Type");
#endif
			case eStatus:				return tr("Status");
			case eStartType:			return tr("Start type");
			case ePID:					return tr("PID");
			case eFileName:				return tr("File name");
			case eErrorControl:			return tr("Error control");
			case eGroupe:				return tr("Groupe");
			case eBinaryPath:			return tr("Binary path");
		}
	}
    return QVariant();
}
