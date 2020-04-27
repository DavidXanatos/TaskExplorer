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
	m_SizePosNA = false;
}

CHandleModel::~CHandleModel()
{
}

void CHandleModel::Sync(QMap<quint64, CHandlePtr> HandleList)
{
	QList<SListNode*> New;
	QHash<QVariant, SListNode*> Old = m_Map;
	
	bool bClearZeros = theConf->GetBool("Options/ClearZeros", true);

	foreach (const CHandlePtr& pHandle, HandleList)
	{
		QVariant ID = (quint64)pHandle.data();

		int Row = -1;
		QHash<QVariant, SListNode*>::iterator I = Old.find(ID);
		SHandleNode* pNode = I != Old.end() ? static_cast<SHandleNode*>(I.value()) : NULL;
		if(!pNode)
		{
			pNode = static_cast<SHandleNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pHandle = pHandle;
			New.append(pNode);
		}
		else
		{
			I.value() = NULL;
			Row = GetRow(pNode);
		}

#ifdef WIN32
		CWinHandle* pWinHandle = qobject_cast<CWinHandle*>(pHandle.data());
#endif

		int Col = 0;
		bool State = false;
		int Changed = 0;

		CProcessPtr pProcess = pNode->pHandle->GetProcess().staticCast<CProcessInfo>();

		// Note: icons are loaded asynchroniusly
		if (m_bUseIcons && !pNode->Icon.isValid() && m_Columns.contains(eProcess))
		{
			CModulePtr pModule = pProcess ? pProcess->GetModuleInfo() : CModulePtr();
			if (pModule)
			{
				QPixmap Icon = pModule->GetFileIcon();
				if (!Icon.isNull()) {
					Changed = 1; // set change for first column
					pNode->Icon = Icon;
				}
			}
		}

		int RowColor = CTaskExplorer::eNone;
		if (pHandle->IsMarkedForRemoval() && CTaskExplorer::UseListColor(CTaskExplorer::eToBeRemoved))		RowColor = CTaskExplorer::eToBeRemoved;
		else if (pHandle->IsNewlyCreated() && CTaskExplorer::UseListColor(CTaskExplorer::eAdded))			RowColor = CTaskExplorer::eAdded;
#ifdef WIN32
		else if (pWinHandle->IsInherited() && CTaskExplorer::UseListColor(CTaskExplorer::eIsInherited))		RowColor = CTaskExplorer::eIsInherited;
		else if (pWinHandle->IsProtected() && CTaskExplorer::UseListColor(CTaskExplorer::eIsProtected))		RowColor = CTaskExplorer::eIsProtected;
#endif

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
				case eProcess:			Value = pHandle->GetProcessId(); break;	
				case eHandle:			Value = pHandle->GetHandleId(); break;
				case eType:				Value = pHandle->GetTypeName(); break;
				case eName:				Value = pHandle->GetFileName(); break;
				case ePosition:			Value = pHandle->GetPosition(); break;	
				case eSize:				Value = pHandle->GetSize(); break;	
				case eGrantedAccess:	Value = (quint32)pHandle->GetGrantedAccess(); break;
#ifdef WIN32
				case eFileShareAccess:	Value = (quint32)pWinHandle->GetFileFlags(); break;	
				case eAttributes:		Value = (quint32)pWinHandle->GetAttributes(); break;	
				case eObjectAddress:	Value = pWinHandle->GetObjectAddress(); break;	
				case eOriginalName:		Value = pWinHandle->GetOriginalName(); break;	
#endif
			}

			SHandleNode::SValue& ColValue = pNode->Values[section];

			if (m_SizePosNA && (section == ePosition || section == eSize))
				Value = tr("N/A");

			if (ColValue.Raw != Value)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
					case eProcess:			ColValue.Formated = tr("%1 (%2)").arg(pProcess.isNull() ? tr("Unknown process") : pProcess->GetName()).arg(pHandle->GetProcessId()); break;	
					case eHandle:			ColValue.Formated = "0x" + QString::number(pHandle->GetHandleId(), 16); break;
					case eType:				ColValue.Formated = pHandle->GetTypeString(); break;
					case eGrantedAccess:	ColValue.Formated = pHandle->GetGrantedAccessString(); break;
#ifdef WIN32
					case eAttributes:		ColValue.Formated = pWinHandle->GetAttributesString(); break;	
					case eFileShareAccess:	ColValue.Formated = pWinHandle->GetFileShareAccessString(); break;	
					case eObjectAddress:	ColValue.Formated = FormatAddress(pWinHandle->GetObjectAddress()); break;	
#endif
					case eSize:
					case ePosition:			if(Value.type() != QVariant::String) ColValue.Formated = FormatNumberEx(Value.toULongLong(), bClearZeros);
				}
			}

			if(State != (Changed != 0))
			{
				if(State && Row != -1)
					emit dataChanged(createIndex(Row, Col), createIndex(Row, section-1));
				State = Changed;
				Col = (Changed != 0);
			}
			if(Changed == 1)
				Changed = 0;
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