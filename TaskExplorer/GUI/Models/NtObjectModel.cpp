#include "stdafx.h"
#include "../TaskExplorer.h"
#include "NtObjectModel.h"
#include "../../../MiscHelpers/Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/ProcessHacker.h"
#endif


CNtObjectModel::CNtObjectModel(QObject *parent)
:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());
	((SNtObjectNode*)m_Root)->ObjectPath = "\\";
	m_Root->Values.resize(columnCount());
	m_Root->Values[eName].Raw = "";
	m_Root->Values[eType].Raw = "Root";

	m_Icons.insert("ALPC Port", QPixmap(":/NtObjects/Port"));
	m_Icons.insert("Device", QPixmap(":/NtObjects/Device"));
	m_Icons.insert("Driver", QPixmap(":/NtObjects/Driver"));
	m_Icons.insert("Event", QPixmap(":/NtObjects/Event"));
	m_Icons.insert("Key", QPixmap(":/NtObjects/Key"));
	m_Icons.insert("Mutant", QPixmap(":/NtObjects/Mutant"));
	m_Icons.insert("Section", QPixmap(":/NtObjects/Section"));
	m_Icons.insert("Session", QPixmap(":/NtObjects/Session"));
	m_Icons.insert("SymbolicLink", QPixmap(":/NtObjects/Link"));
	m_Icons.insert("Directory", QPixmap(":/NtObjects/Folder"));
	m_DefaultIcon = QPixmap(":/NtObjects/Unknown");

}

CNtObjectModel::~CNtObjectModel()
{
}

CNtObjectModel::SNtObjectNode* CNtObjectModel::GetNode(const QModelIndex &index) const
{
    if (index.column() > 0)
        return NULL;

	SNtObjectNode* pNode;
    if (!index.isValid())
        pNode = (SNtObjectNode*)m_Root;
    else
        pNode = static_cast<SNtObjectNode*>(index.internalPointer());
	return pNode;
}

struct SNtObjectInfo
{
	QString Name;
	QString Type;
};

typedef struct _DIRECTORY_ENUM_CONTEXT
{
	QList<SNtObjectInfo> FoundObjects;

} DIRECTORY_ENUM_CONTEXT, *PDIRECTORY_ENUM_CONTEXT;

static BOOLEAN NTAPI EnumDirectoryObjectsCallback(_In_ PPH_STRINGREF Name, _In_ PPH_STRINGREF TypeName, _In_opt_ PVOID Context)
{
    PDIRECTORY_ENUM_CONTEXT context = (PDIRECTORY_ENUM_CONTEXT)Context;

	SNtObjectInfo NtObject;
	NtObject.Name = QString::fromWCharArray(Name->Buffer, Name->Length / sizeof(wchar_t));
	NtObject.Type = QString::fromWCharArray(TypeName->Buffer, TypeName->Length / sizeof(wchar_t));
	context->FoundObjects.append(NtObject);   

    return TRUE;
}

QList<SNtObjectInfo> EnumDirectoryObjects(const QString& ObjectPath)
{
	DIRECTORY_ENUM_CONTEXT enumContext;

	std::wstring Name = ObjectPath.toStdWString();

    HANDLE directoryHandle;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING name;
	name.Buffer = (wchar_t*)Name.c_str();
	name.Length = Name.length() * sizeof(wchar_t);
	name.MaximumLength = (Name.length() + 1) * sizeof(wchar_t);

    InitializeObjectAttributes(&oa, &name, 0, NULL, NULL);

	if (!NT_SUCCESS(NtOpenDirectoryObject(&directoryHandle, DIRECTORY_QUERY, &oa)))
		return enumContext.FoundObjects;

    PhEnumDirectoryObjects(directoryHandle, EnumDirectoryObjectsCallback, &enumContext);

    NtClose(directoryHandle);

	return enumContext.FoundObjects;
}

void CNtObjectModel::FillNode(const struct SNtObjectInfo* pNtObject, SNtObjectNode* pChildNode)
{
	pChildNode->Values.resize(columnCount());

	pChildNode->Values[eName].Raw = pNtObject->Name;
	if (pNtObject->Type == "Directory")
	{
		pChildNode->Values[eType].Raw = QVariant(); // sort directories first
		pChildNode->Values[eType].Formated = pNtObject->Type;
	}
	else
	{
		pChildNode->Values[eType].Raw = pNtObject->Type;
		pChildNode->State = -1;
	}
	pChildNode->Icon = m_Icons.value(pNtObject->Type);
}

void CNtObjectModel::fetchMore(const QModelIndex &parent)
{
	SNtObjectNode* pNode = GetNode(parent);
	if (pNode->State != 0)
		return;

	QList<SNtObjectInfo> FoundObjects = EnumDirectoryObjects(pNode->ObjectPath);

	QMap<QList<QVariant>, QList<STreeNode*> > New;

	QString ObjectPrefix = pNode->ObjectPath;
	if (pNode->ObjectPath != "\\")
		ObjectPrefix += "\\";

	foreach(const SNtObjectInfo& NtObject, FoundObjects)
	{
		QString ObjectPath = ObjectPrefix + NtObject.Name;

		SNtObjectNode* pChildNode = static_cast<SNtObjectNode*>(MkNode(ObjectPath));
		pChildNode->ObjectPath = ObjectPath;
		if(pNode->Parent)
		{
			pChildNode->Path = pNode->Path;
			pChildNode->Path.append(pNode->ObjectPath);
		}
		//pChildNode->pNtObject = 
		New[pChildNode->Path].append(pChildNode);

		FillNode(&NtObject, pChildNode);
	}

	pNode->State = 1;

	if(!New.isEmpty())
	{
		beginInsertRows(parent, 0, FoundObjects.size()-1);
		for(QMap<QList<QVariant>, QList<STreeNode*> >::const_iterator I = New.begin(); I != New.end(); I++)
			Fill(m_Root, QModelIndex(), I.key(), 0, I.value(), I.key(), NULL);
		endInsertRows();
	}
}

void CNtObjectModel::Refresh()
{ 
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	Refresh((SNtObjectNode*)m_Root, New, Old); 

	CTreeItemModel::Sync(New, Old);
}

void CNtObjectModel::Refresh(SNtObjectNode* pNode, QMap<QList<QVariant>, QList<STreeNode*> >& New, QHash<QVariant, STreeNode*>& Old)
{
	QList<SNtObjectInfo> FoundObjects = EnumDirectoryObjects(pNode->ObjectPath);

	QString ObjectPrefix = pNode->ObjectPath;
	if (pNode->ObjectPath != "\\")
		ObjectPrefix += "\\";

	foreach(const SNtObjectInfo& NtObject, FoundObjects)
	{
		QString ObjectPath = ObjectPrefix + NtObject.Name;

		//QModelIndex Index;
		
		SNtObjectNode* pChildNode = static_cast<SNtObjectNode*>(Old.value(ObjectPath));
		if(!pChildNode)
		{
			pChildNode = static_cast<SNtObjectNode*>(MkNode(ObjectPath));
			pChildNode->ObjectPath = ObjectPath;
			if(pNode->Parent)
			{
				pChildNode->Path = pNode->Path;
				pChildNode->Path.append(pNode->ObjectPath);
			}
			//pChildNode->pNtObject = 
			New[pChildNode->Path].append(pChildNode);

			FillNode(&NtObject, pChildNode);
		}
		else
		{
			Old[ObjectPath] = NULL;
			//Index = Find(m_Root, pChildNode);

			if (pChildNode->State == 1)
				Refresh(pChildNode, New, Old);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pChildNode), createIndex(Index.row(), columnCount()-1, pChildNode));
	}
}

bool CNtObjectModel::canFetchMore(const QModelIndex &parent) const
{
	SNtObjectNode* pNode = GetNode(parent);
	if (!pNode || pNode->State != 0)
		return false;
	return true;
}

bool CNtObjectModel::hasChildren(const QModelIndex &parent) const
{
	SNtObjectNode* pNode = GetNode(parent);
	if (pNode && pNode->State == 0)
		return true;
	return CTreeItemModel::hasChildren(parent);
}

int CNtObjectModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CNtObjectModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eName:				return tr("Name");
			case eType:				return tr("Type");
		}
	}
    return QVariant();
}
