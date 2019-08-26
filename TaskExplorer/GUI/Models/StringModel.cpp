#include "stdafx.h"
#include "../TaskExplorer.h"
#include "StringModel.h"
#include "../../Common/Common.h"
#include "../../API/MemoryInfo.h"


CStringModel::CStringModel(QObject *parent)
:CListItemModel(parent)
{
}

CStringModel::~CStringModel()
{
}

void CStringModel::Sync(const QMap<quint64, CStringInfoPtr>& StringList)
{
	QList<SListNode*> New;
	QHash<QVariant, SListNode*> Old = m_Map;

	for (QMap<quint64, CStringInfoPtr>::const_iterator I = StringList.begin(); I != StringList.end(); ++I)
	{
		const CStringInfoPtr& pString = I.value();
		QVariant ID = I.key();

		int Row = -1;
		SStringNode* pNode = static_cast<SStringNode*>(Old[ID]);
		if(!pNode)
		{
			pNode = static_cast<SStringNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pString = pString;
			New.append(pNode);
		}
		else
		{
			Old[ID] = NULL;
			Row = GetRow(pNode);
		}

		int Col = 0;
		bool State = false;
		int Changed = 0;

		CProcessPtr pProcess = pString->GetProcess();

		// Note: icons are loaded asynchroniusly
		if (m_bUseIcons && !pNode->Icon.isValid() && m_Columns.contains(eProcess))
		{
			if (!pProcess.isNull())
			{
				QPixmap Icon = pProcess->GetModuleInfo()->GetFileIcon();
				if (!Icon.isNull()) {
					Changed = 1; // set change for first column
					pNode->Icon = Icon;
				}
			}
		}

		for(int section = eProcess; section < columnCount(); section++)
		{
			if (!m_Columns.contains(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch(section)
			{
				case eProcess:		Value = pProcess ? pProcess->GetName() : ""; break;
				case eAddress:		Value = pString->GetAddress(); break;
				case eBaseAddress:	Value = pString->GetBaseAddress(); break;
				case eLength:		Value = pString->GetString().length(); break;
				case eResult:		Value = pString->GetString(); break;
			}

			SStringNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
					case eAddress:
					case eBaseAddress:	
										ColValue.Formated = FormatAddress(Value.toULongLong()); break;	
					case eLength:
										ColValue.Formated = FormatNumber(Value.toULongLong()); break;	
				}
			}

			if(State != (Changed != 0))
			{
				if(State)
					emit dataChanged(createIndex(Row, Col, pNode), createIndex(Row, section-1, pNode));
				State = (Changed != 0);
				Col = section;
			}
			if(Changed == 1)
				Changed = 0;
		}
		if(State)
			emit dataChanged(createIndex(Row, Col, pNode), createIndex(Row, columnCount()-1, pNode));

	}

	CListItemModel::Sync(New, Old);
}

CStringInfoPtr CStringModel::GetString(const QModelIndex &index) const
{
	if (!index.isValid())
        return CStringInfoPtr();

	SStringNode* pNode = static_cast<SStringNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pString;
}

int CStringModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CStringModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eProcess: return tr("Process");
			case eAddress: return tr("Address");
			case eBaseAddress: return tr("Base address");
			case eLength: return tr("Length");
			case eResult: return tr("Result");
		}
	}
    return QVariant();
}

QVariant CStringModel::GetDefaultIcon() const 
{ 
	return g_ExeIcon;
}