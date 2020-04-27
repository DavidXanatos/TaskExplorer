#include "stdafx.h"
#include "../TaskExplorer.h"
#include "MemoryModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinMemory.h"
#endif


CMemoryModel::CMemoryModel(QObject *parent)
:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());
}

CMemoryModel::~CMemoryModel()
{
}

QList<QVariant> CMemoryModel::MakeMemPath(const CMemoryPtr& pMemory, const QMap<quint64, CMemoryPtr>& ModuleList)
{
	QList<QVariant> Path;
	if (!pMemory->IsAllocationBase())
	{
		Path.append(pMemory->GetAllocationBase());
	}
	return Path;
}

void CMemoryModel::Sync(const QMap<quint64, CMemoryPtr>& ModuleList)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach (const CMemoryPtr& pMemory, ModuleList)
	{
		QVariant ID = pMemory->GetBaseAddress();

		QModelIndex Index;
		QList<QVariant> Path;
		if(m_bTree)
			Path = MakeMemPath(pMemory, ModuleList);
		
		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SMemoryNode* pNode = I != Old.end() ? static_cast<SMemoryNode*>(I.value()) : NULL;
		if(!pNode || pNode->Path != Path)
		{
			pNode = static_cast<SMemoryNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->Path = Path;
			pNode->pMemory = pMemory;
			New[Path].append(pNode);
		}
		else
		{
			I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		UpdateMemory(pMemory, pNode, Index);
	}

	CTreeItemModel::Sync(New, Old);
}

bool CMemoryModel::UpdateMemory(const CMemoryPtr& pMemory)
{
	QVariant ID = pMemory->GetBaseAddress();
	SMemoryNode* pNode = static_cast<SMemoryNode*>(m_Map.value(ID));
	if (!pNode)
		return false;
	QModelIndex Index = Find(m_Root, pNode);

	UpdateMemory(pMemory, pNode, Index);
	return true;
}

void CMemoryModel::UpdateMemory(const CMemoryPtr& pMemory, SMemoryNode* pNode, QModelIndex& Index)
{
	//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

#ifdef WIN32
	CWinMemory* pWinMemory = qobject_cast<CWinMemory*>(pMemory.data());
#endif

	int Col = 0;
	bool State = false;
	int Changed = 0;


	int RowColor = CTaskExplorer::eNone;
	if (pMemory->IsMarkedForRemoval() && CTaskExplorer::UseListColor(CTaskExplorer::eToBeRemoved))		RowColor = CTaskExplorer::eToBeRemoved;
	else if (pMemory->IsNewlyCreated() && CTaskExplorer::UseListColor(CTaskExplorer::eAdded))			RowColor = CTaskExplorer::eAdded;
	else if (pMemory->IsExecutable() && CTaskExplorer::UseListColor(CTaskExplorer::eExecutable))		RowColor = CTaskExplorer::eExecutable;
#ifdef WIN32
	else if (pWinMemory->IsBitmapRegion() && CTaskExplorer::UseListColor(CTaskExplorer::eElevated))		RowColor = CTaskExplorer::eElevated;
#endif
	else if (pMemory->IsPrivate() && CTaskExplorer::UseListColor(CTaskExplorer::eUser))					RowColor = CTaskExplorer::eUser;

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
			case eBaseAddress:			Value = pMemory->GetBaseAddress(); break;
			case eType:					Value = pMemory->GetTypeString(); break;
			case eSize:					Value = pMemory->GetRegionSize(); break;
			case eProtection:			Value = pMemory->GetProtectString(); break;
			case eUse:					Value = pMemory->GetUseString(); break;
			case eTotalWS:				Value = pMemory->GetTotalWorkingSet(); break;
			case ePrivateWS: 			Value = pMemory->GetPrivateWorkingSet(); break;
			case eShareableWS: 			Value = pMemory->GetSharedWorkingSet(); break;
			case eSharedWS: 			Value = pMemory->GetShareableWorkingSet(); break;
			case eLockedWS: 			Value = pMemory->GetLockedWorkingSet(); break;
			case eCommitted:			Value = pMemory->GetCommittedSize(); break;
			case ePrivate:				Value = pMemory->GetPrivateSize(); break;
		}

		SMemoryNode::SValue& ColValue = pNode->Values[section];

		if (ColValue.Raw != Value)
		{
			if(Changed == 0)
				Changed = 1;
			ColValue.Raw = Value;

			switch (section)
			{
				case eBaseAddress:	ColValue.Formated = FormatAddress(Value.toULongLong()); break;	

				case eSize:

				case eTotalWS:
				case ePrivateWS:
				case eShareableWS:
				case eSharedWS:
				case eLockedWS:

				case eCommitted:
				case ePrivate:		
									ColValue.Formated = FormatSize(Value.toULongLong()); break;	
			}
		}

		if(State != (Changed != 0))
		{
			if(State && Index.isValid())
				emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), section-1, pNode));
			State = (Changed != 0);
			Col = section;
		}
		if(Changed == 1)
			Changed = 0;
	}
	if(State && Index.isValid())
		emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), columnCount()-1, pNode));
}

CMemoryPtr CMemoryModel::GetMemory(const QModelIndex &index) const
{
	if (!index.isValid())
        return CMemoryPtr();

	SMemoryNode* pNode = static_cast<SMemoryNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pMemory;
}

int CMemoryModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CMemoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eBaseAddress: return tr("Base address");
			case eType: return tr("Type");
			case eSize: return tr("Size");
			case eProtection: return tr("Protection");
			case eUse: return tr("Use");
			case eTotalWS: return tr("Total WS");
			case ePrivateWS: return tr("Private WS");
			case eShareableWS: return tr("Shareable WS");
			case eSharedWS: return tr("Shared WS");
			case eLockedWS: return tr("Locked WS");
			case eCommitted: return tr("Committed");
			case ePrivate: return tr("Private");
		}
	}
    return QVariant();
}
