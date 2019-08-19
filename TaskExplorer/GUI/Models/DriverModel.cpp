#include "stdafx.h"
#include "../TaskExplorer.h"
#include "DriverModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinDriver.h"
#endif

CDriverModel::CDriverModel(QObject *parent)
:CListItemModel(parent)
{
}

CDriverModel::~CDriverModel()
{
}

void CDriverModel::Sync(QMap<QString, CDriverPtr> DriverList)
{
	QList<SListNode*> New;
	QMap<QVariant, SListNode*> Old = m_Map;

	foreach (const CDriverPtr& pDriver, DriverList)
	{
		QVariant ID = pDriver->GetBinaryPath();

		int Row = -1;
		SDriverNode* pNode = static_cast<SDriverNode*>(Old[ID]);
		if(!pNode)
		{
			pNode = static_cast<SDriverNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pDriver = pDriver;
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
		CWinDriver* pWinDriver = qobject_cast<CWinDriver*>(pDriver.data());
#endif

		for(int section = eDriver; section < columnCount(); section++)
		{
			QVariant Value;
			switch(section)
			{
				case eDriver:				Value = pDriver->GetFileName(); break;
#ifdef WIN32
				case eImageBase:			Value = pWinDriver->GetImageBase(); break;
				case eImageSize:			Value = pWinDriver->GetImageSize(); break;
				case eDescription:			Value = pDriver->GetModuleInfo()->GetFileInfo("Description"); break;
				case eCompanyName:			Value = pDriver->GetModuleInfo()->GetFileInfo("CompanyName"); break;
				case eVersion:				Value = pDriver->GetModuleInfo()->GetFileInfo("FileVersion"); break;
#endif
				case eBinaryPath:			Value = pDriver->GetBinaryPath(); break;
			}

			SDriverNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				Changed = true;
				ColValue.Raw = Value;

				switch (section)
				{
                    case eDriver: break;
#ifdef WIN32
					case eImageBase:	ColValue.Formated = FormatAddress(Value.toULongLong()); break;
					case eImageSize:	ColValue.Formated = FormatSize(Value.toULongLong()); break;			
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

CDriverPtr CDriverModel::GetDriver(const QModelIndex &index) const
{
	if (!index.isValid())
        return CDriverPtr();

	SDriverNode* pNode = static_cast<SDriverNode*>(index.internalPointer());
	return pNode->pDriver;
}

int CDriverModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CDriverModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eDriver:				return tr("Driver");
#ifdef WIN32
			case eImageBase:			return tr("Image base");
			case eImageSize:			return tr("Image size");
			case eDescription:			return tr("Description");
			case eCompanyName:			return tr("Company name");
			case eVersion:				return tr("Version");
#endif
			case eBinaryPath:			return tr("Binary path");
		}
	}
    return QVariant();
}
