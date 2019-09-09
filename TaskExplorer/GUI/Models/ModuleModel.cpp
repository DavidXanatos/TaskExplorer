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
	m_Root = MkNode(QVariant());
}

CModuleModel::~CModuleModel()
{
}

QList<QVariant> CModuleModel::MakeModPath(const CModulePtr& pModule, const QMap<quint64, CModulePtr>& ModuleList)
{
	quint64 ParentID = pModule->GetParentBaseAddress();
	CModulePtr pParent = ModuleList.value(ParentID);

	QList<QVariant> Path;
	if (!pParent.isNull() && ParentID != pModule->GetBaseAddress())
	{
		Path = MakeModPath(pParent, ModuleList);
		Path.append(ParentID);
	}
	return Path;
}

bool CModuleModel::TestModPath(const QList<QVariant>& Path, const CModulePtr& pModule, const QMap<quint64, CModulePtr>& ModuleList, int Index)
{
	quint64 ParentID = pModule->GetParentBaseAddress();
	CModulePtr pParent = ModuleList.value(ParentID);

	if (!pParent.isNull() && ParentID != pModule->GetBaseAddress())
	{
		if(Index >= Path.size() || Path[Path.size() - Index - 1] != ParentID)
			return false;

		return TestModPath(Path, pParent, ModuleList, Index + 1);
	}

	return Path.size() == Index;
}

void CModuleModel::Sync(const QMap<quint64, CModulePtr>& ModuleList)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	for (QMap<quint64, CModulePtr>::const_iterator I = ModuleList.begin(); I != ModuleList.end(); ++I)
	{
		const CModulePtr& pModule = I.value();
		QVariant ID = I.key();

		QModelIndex Index;
		
		SModuleNode* pNode = static_cast<SModuleNode*>(Old.value(ID));
		if(!pNode || (m_bTree ? !TestModPath(pNode->Path, pModule, ModuleList) : !pNode->Path.isEmpty()))
		{
			pNode = static_cast<SModuleNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			if(m_bTree)
				pNode->Path = MakeModPath(pModule, ModuleList);
			pNode->pModule = pModule;
			pNode->IsBold = pModule->IsFirst();
			New[pNode->Path].append(pNode);
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
		int Changed = 0;

		CProcessPtr pProcess = (!m_bTree) ? pModule->GetProcess().objectCast<CProcessInfo>() : NULL;

		// Note: icons are loaded asynchroniusly
		if (m_bUseIcons && !pNode->Icon.isValid() && m_Columns.contains(eModule))
		{
			QPixmap Icon;
			if (!pProcess.isNull())
				Icon = pProcess->GetModuleInfo()->GetFileIcon();
			else
				Icon = pModule->GetFileIcon();

			if (!Icon.isNull()) {
				Changed = 1; // set change for first column
				pNode->Icon = Icon;
			}
		}

		if (pNode->IsGray != !pModule->IsLoaded())
		{
			pNode->IsGray = !pModule->IsLoaded();
			Changed = 2;
		}

#ifdef WIN32
		CWinModule* pWinModule = qobject_cast<CWinModule*>(pModule.data());
#endif

		for(int section = 0; section < columnCount(); section++)
		{
			if (!m_Columns.contains(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch(section)
			{
				case eModule:				if (!pProcess.isNull()) { Value = pProcess->GetName(); break; }
				case eModuleFile:			Value = pModule->GetName(); break;
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
				case eTimeStamp:			Value = pWinModule->GetTimeStamp(); break;
				case eCFGuard:				Value = pWinModule->GetCFGuardString(); break;
				case eLoadTime:				Value = pWinModule->GetLoadTime(); break;
				case eLoadReason:			Value = pWinModule->GetLoadReasonString(); break;
#endif
				case eFileModifiedTime:		Value = pModule->GetModificationTime(); break;
				case eFileSize:				Value = pModule->GetFileSize(); break;
#ifdef WIN32
				case eEntryPoint:			Value = pWinModule->GetEntryPoint();
				case eService:				Value = pWinModule->GetRefServices().join(", "); break;
#endif
				case eParentBaseAddress:	Value = pModule->GetParentBaseAddress(); break;
			}

			SModuleNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
					case eBaseAddress:
					case eParentBaseAddress:
#ifdef WIN32
					case eEntryPoint:
#endif
											ColValue.Formated = FormatAddress(ColValue.Raw.toULongLong()); break;
					case eSize:
					case eFileSize:			
											ColValue.Formated = FormatSize(ColValue.Raw.toULongLong()); break;
#ifdef WIN32
					case eTimeStamp:
					case eLoadTime:
#endif
					case eFileModifiedTime:	ColValue.Formated = QDateTime::fromTime_t(ColValue.Raw.toULongLong()).toString("dd.MM.yyyy hh:mm:ss"); break;
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
			case eService:				return tr("Ref. services");
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