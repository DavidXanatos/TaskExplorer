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
#ifdef WIN32
	m_ShowDriver = true;
#endif
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
#ifdef WIN32
		CWinService* pWinService = qobject_cast<CWinService*>(pService.data());

		if (!m_ShowDriver && pWinService->IsDriver())
			continue;
#endif

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
		int Changed = 0;

		// Note: icons are loaded asynchroniusly
#ifdef WIN32
		if (!pNode->Icon.isValid())
		{
			QPixmap Icon = pWinService->IsDriver() ? g_DllIcon.pixmap(16,16) : pService->GetModuleInfo()->GetFileIcon();
			if (!Icon.isNull()) {
				Changed = 1; // set change for first column
				pNode->Icon = Icon;
			}
		}
#endif

		int RowColor = CTaskExplorer::eNone;
		if (pService->IsMarkedForRemoval())			RowColor = CTaskExplorer::eToBeRemoved;
		else if (pService->IsNewlyCreated())		RowColor = CTaskExplorer::eAdded;
#ifdef WIN32
		else if (pWinService->IsDriver())			RowColor = CTaskExplorer::eElevated;
#endif

		if (pNode->iColor != RowColor) {
			pNode->iColor = RowColor;
			pNode->Color = CTaskExplorer::GetColor(RowColor);
			Changed = 2;
		}

		if (pNode->IsGray != pService->IsStopped())
		{
			pNode->IsGray = pService->IsStopped();
			Changed = 2;
		}


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
#endif
				case ePID:					Value = (int)pService->GetPID(); break;
				case eFileName:				Value = pService->GetFileName(); break;
#ifdef WIN32
				case eDescription:			Value = pService->GetModuleInfo()->GetFileInfo("Description"); break;
				case eCompanyName:			Value = pService->GetModuleInfo()->GetFileInfo("CompanyName"); break;
				case eVersion:				Value = pService->GetModuleInfo()->GetFileInfo("FileVersion"); break;
				case eErrorControl:			Value = pWinService->GetErrorControlString(); break;
				case eGroupe:				Value = pWinService->GetGroupeName(); break;
#endif
				case eBinaryPath:			Value = pService->GetBinaryPath(); break;
			}

			SServiceNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				/*switch (section)
				{
					case ePID:					ColValue.Formated = 
				}*/
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
#ifdef WIN32
			case eStartType:			return tr("Start type");
#endif
			case ePID:					return tr("PID");
			case eFileName:				return tr("File name");
#ifdef WIN32
			case eErrorControl:			return tr("Error control");
			case eGroupe:				return tr("Groupe");
			case eDescription:			return tr("Description");
			case eCompanyName:			return tr("Company name");
			case eVersion:				return tr("Version");
#endif
			case eBinaryPath:			return tr("Binary path");
		}
	}
    return QVariant();
}

QVariant CServiceModel::GetDefaultIcon() const 
{ 
	return g_ExeIcon;
}