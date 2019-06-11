#include "stdafx.h"
#include "../TaskExplorer.h"
#include "HandleModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinHandle.h"
#endif

CHandleModel::CHandleModel(QObject *parent)
:CListItemModel(parent)
{
}

CHandleModel::~CHandleModel()
{
}

void CHandleModel::Sync(QMap<quint64, CHandlePtr> HandleList)
{
	QList<SListNode*> New;
	QMap<QVariant, SListNode*> Old = m_Map;

	foreach (const CHandlePtr& pHandle, HandleList)
	{
		QVariant ID = (quint64)pHandle.data();

		int Row = -1;
		SHandleNode* pNode = static_cast<SHandleNode*>(Old[ID]);
		if(!pNode)
		{
			pNode = static_cast<SHandleNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pHandle = pHandle;
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

		// Note: icons are loaded asynchroniusly
		if (!pNode->Icon.isValid())
		{
			CProcessPtr pProcess = pNode->pHandle->GetProcess().objectCast<CProcessInfo>();

			if (!pProcess.isNull())
			{
				QPixmap Icon = pProcess->GetModuleInfo()->GetFileIcon();
				if (!Icon.isNull()) {
					Changed = true; // set change for first column
					pNode->Icon = Icon;
				}
			}
		}

#ifdef WIN32
		CWinHandle* pWinHandle = qobject_cast<CWinHandle*>(pHandle.data());
#endif

		for(int section = eProcess; section < columnCount(); section++)
		{
			QVariant Value;
			switch(section)
			{
				case eProcess:			Value = pHandle->GetProcessName(); break;	
				case eHandle:			Value = pHandle->GetHandleId(); break;
				case eType:				Value = pHandle->GetTypeString(); break;
				case eName:				Value = pHandle->GetFileName(); break;
				case ePosition:			Value = pHandle->GetPosition(); break;	
				case eSize:				Value = pHandle->GetSize(); break;	
				case eGrantedAccess:	Value = pHandle->GetGrantedAccessString(); break;
#ifdef WIN32
				case eFileShareAccess:	Value = pWinHandle->GetFileShareAccessString(); break;	
				case eAttributes:		Value = pWinHandle->GetAttributesString(); break;	
				case eObjectAddress:	Value = pWinHandle->GetObjectAddress(); break;	
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
					//case eHandle:			ColValue.Formated = "0x" + QString::number(pHandle->GetHandleId(), 16); break;
#ifdef WIN32
					case eObjectAddress:	ColValue.Formated = "0x" + QString::number(pWinHandle->GetObjectAddress(), 16); break;	
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

	CListItemModel::Sync(New, Old);
}

CHandlePtr CHandleModel::GetHandle(const QModelIndex &index) const
{
	if (!index.isValid())
        return CHandlePtr();

	SHandleNode* pNode = static_cast<SHandleNode*>(index.internalPointer());
	return pNode->pHandle;
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
			case eProcess:				return tr("Process");
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

QVariant CHandleModel::GetDefaultIcon() const 
{ 
	return g_ExeIcon;
}