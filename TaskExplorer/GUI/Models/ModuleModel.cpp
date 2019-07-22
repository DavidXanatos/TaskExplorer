#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ModuleModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinModule.h"
#endif
#include "../../API/ProcessInfo.h"


CModuleModel::CModuleModel(QObject *parent)
:CTreeItemModel(parent)
{
}

CModuleModel::~CModuleModel()
{
}

QList<QVariant> CModuleModel::MakeModPath(const CModulePtr& pModule, const QMap<quint64, CModulePtr>& ModuleList)
{
	quint64 ParentID = pModule->GetParentBaseAddress();

	QList<QVariant> list;
	if (ParentID != pModule->GetBaseAddress())
	{
		CModulePtr pParent = ModuleList.value(ParentID);
		if (!pParent.isNull())
		{
			list = MakeModPath(pParent, ModuleList);
			list.append(ParentID);
		}
	}
	return list;
}

void CModuleModel::Sync(const QMap<quint64, CModulePtr>& ModuleList)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QMap<QVariant, STreeNode*> Old = m_Map;

	for (QMap<quint64, CModulePtr>::const_iterator I = ModuleList.begin(); I != ModuleList.end(); ++I)
	{
		const CModulePtr& pModule = I.value();
		QVariant ID = I.key();

		QModelIndex Index;
		QList<QVariant> Path;
		if(m_bTree)
			Path = MakeModPath(pModule, ModuleList);
		
		SModuleNode* pNode = static_cast<SModuleNode*>(Old.value(ID));
		if(!pNode || pNode->Path != Path)
		{
			pNode = static_cast<SModuleNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->Path = Path;
			pNode->pModule = pModule;
			pNode->IsBold = pModule->IsFirst();
			New[Path].append(pNode);
		}
		else
		{
			Old[ID] = NULL;
			Index = Find(m_Root, pNode);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

		int Col = 0;
		bool State = false;
		bool Changed = false;

		CProcessPtr pProcess = (!m_bTree) ? pNode->pModule->GetProcess().objectCast<CProcessInfo>() : NULL;

		// Note: icons are loaded asynchroniusly
		if (m_bUseIcons && !pNode->Icon.isValid())
		{
			QPixmap Icon;
			if (!pProcess.isNull())
				Icon = pProcess->GetModuleInfo()->GetFileIcon();
			else
				Icon = pNode->pModule->GetFileIcon();

			if (!Icon.isNull()) {
				Changed = 1; // set change for first column
				pNode->Icon = Icon;
			}
		}

#ifdef WIN32
		CWinModule* pWinModule = qobject_cast<CWinModule*>(pModule.data());
#endif

		for(int section = eModule; section < columnCount(); section++)
		{
			QVariant Value;
			switch(section)
			{
				case eModule:				if (!pProcess.isNull()) { Value = pProcess->GetName(); break; }
				case eModuleFile:			Value = pNode->pModule->GetName(); break;
				case eBaseAddress:			Value = pModule->GetBaseAddress(); break;
				case eSize:					Value = pModule->GetSize(); break;
#ifdef WIN32
				case eDescription:			Value = pModule->GetFileInfo("Description"); break;

				case eCompanyName:			Value = pModule->GetFileInfo("CompanyName"); break;
				case eVersion:				Value = pModule->GetFileInfo("FileVersion"); break;
#endif
				case eFileName:				Value = pModule->GetFileName(); break;
#ifdef WIN32
				case eType:					Value = pWinModule->GetTypeString(); break;
				case eLoadCount:			Value = pWinModule->GetLoadCount(); break;
				case eVerificationStatus:	Value = pWinModule->GetVerifyResultString(); break;
				case eVerifiedSigner:		Value = pWinModule->GetVerifySignerName(); break;
				case eASLR:					Value = pWinModule->GetASLRString(); break;
				case eTimeStamp:			Value = pWinModule->GetImageTimeDateStamp(); break;
				case eCFGuard:				Value = pWinModule->GetCFGuardString(); break;
				case eLoadTime:				Value = pWinModule->GetLoadTime(); break;
				case eLoadReason:			Value = pWinModule->GetLoadReasonString(); break;
#endif
				case eFileModifiedTime:		Value = pModule->GetModificationTime(); break;
				case eFileSize:				Value = pModule->GetFileSize(); break;
#ifdef WIN32
				case eEntryPoint:			Value = pWinModule->GetEntryPoint();
#endif
				case eParentBaseAddress:	Value = pModule->GetParentBaseAddress(); break;
			}

			SModuleNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				Changed = true;
				ColValue.Raw = Value;

				switch (section)
				{
					case eBaseAddress:		ColValue.Formated = "0x" + QString::number(pModule->GetBaseAddress(), 16); break;
					case eSize:				ColValue.Formated = FormatSize(pModule->GetSize()); break;
					case eParentBaseAddress:ColValue.Formated = "0x" + QString::number(pModule->GetParentBaseAddress(), 16); break;
#ifdef WIN32
					case eTimeStamp:		ColValue.Formated = pWinModule->GetImageTimeDateStamp().toString("dd.MM.yyyy hh:mm:ss"); break;
					case eLoadTime:			ColValue.Formated = pWinModule->GetLoadTime().toString("dd.MM.yyyy hh:mm:ss"); break;
#endif
					case eFileSize:			ColValue.Formated = FormatSize(pModule->GetFileSize()); break;
					case eFileModifiedTime:	ColValue.Formated = pModule->GetModificationTime().toString("dd.MM.yyyy hh:mm:ss"); break;
#ifdef WIN32
					case eEntryPoint:		ColValue.Formated = "0x" + QString::number(pWinModule->GetEntryPoint(), 16); break;
#endif
				}
			}

			if(State != Changed)
			{
				if(State && Index.isValid())
					emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), section-1, pNode));
				State = Changed;
				Col = section;
			}
			Changed = false;
		}
		if(State && Index.isValid())
			emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), columnCount()-1, pNode));

	}

	CTreeItemModel::Sync(New, Old);
}

CModulePtr CModuleModel::GetModule(const QModelIndex &index) const
{
	if (!index.isValid())
        return CModulePtr();

	SModuleNode* pNode = static_cast<SModuleNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pModule;
}

int CModuleModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CModuleModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eModule:				if(!m_bTree) return tr("Process");
			case eModuleFile:			return tr("Name");
			case eBaseAddress:			return tr("Base address");
			case eSize:					return tr("Size");
#ifdef WIN32
			case eDescription:			return tr("Description");

			case eCompanyName:			return tr("Company name");
			case eVersion:				return tr("Version");
#endif
			case eFileName:				return tr("File name");
#ifdef WIN32
			case eType:					return tr("Type");
			case eLoadCount:			return tr("Load count");
			case eVerificationStatus:	return tr("Verification status");
			case eVerifiedSigner:		return tr("Verified signer");
			case eASLR:					return tr("ASLR");
			case eTimeStamp:			return tr("Time stamp");
			case eCFGuard:				return tr("CF Guard");
			case eLoadTime:				return tr("Load time");
			case eLoadReason:			return tr("Load reason");
#endif
			case eFileModifiedTime:		return tr("File modified time");
			case eFileSize:				return tr("File size");
#ifdef WIN32
			case eEntryPoint:			return tr("Entry point");
#endif
			case eParentBaseAddress:	return tr("Parent base address");
		}
	}
    return QVariant();
}

QVariant CModuleModel::GetDefaultIcon() const 
{ 
	if(m_bTree)
		return g_DllIcon;
	return g_ExeIcon;
}