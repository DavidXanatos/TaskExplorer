#include "stdafx.h"
#include "../TaskExplorer.h"
#include "WindowModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinWnd.h"
#endif


CWindowModel::CWindowModel(QObject *parent)
:CTreeItemModel(parent)
{
}

CWindowModel::~CWindowModel()
{
}

QList<QVariant> CWindowModel::MakeWndPath(const CWndPtr& pWindow, const QMap<quint64, CWndPtr>& ModuleList)
{
	quint64 ParentID = pWindow->GetParentWnd();

	QList<QVariant> list;
	if (ParentID != pWindow->GetHWnd())
	{
		CWndPtr pParent = ModuleList.value(ParentID);
		if (!pParent.isNull())
		{
			list = MakeWndPath(pParent, ModuleList);
			list.append(ParentID);
		}
	}
	return list;
}

void CWindowModel::Sync(const QMap<quint64, CWndPtr>& ModuleList)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QMap<QVariant, STreeNode*> Old = m_Map;

	foreach (const CWndPtr& pWindow, ModuleList)
	{
		QVariant ID = pWindow->GetHWnd();

		QModelIndex Index;
		QList<QVariant> Path;
		if(m_bTree)
			Path = MakeWndPath(pWindow, ModuleList);
		
		SWindowNode* pNode = static_cast<SWindowNode*>(Old.value(ID));
		if(!pNode || pNode->Path != Path)
		{
			pNode = static_cast<SWindowNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->Path = Path;
			pNode->pWindow = pWindow;
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
		int Changed = 0;

		// Note: icons are loaded asynchroniusly
		/*if (m_bUseIcons && !pNode->Icon.isValid())
		{
			QPixmap Icon = pNode->pWindow->GetFileIcon();
			if (!Icon.isNull()) {
				Changed = true; // set change for first column
				pNode->Icon = Icon;
			}
		}*/

		if (pNode->IsGray != !pWindow->IsVisible())
		{
			pNode->IsGray = !pWindow->IsVisible();
			Changed = 2; // update all columns for this item
		}

#ifdef WIN32
		CWinWnd* pWinWnd = qobject_cast<CWinWnd*>(pWindow.data());
#endif

		for(int section = eHandle; section < columnCount(); section++)
		{
			QVariant Value;
			switch(section)
			{
				case eHandle:		Value = pWindow->GetHWnd(); break;
#ifdef WIN32
				case eClass:		Value = pWinWnd->GetWindowClass(); break;
#endif
				case eText:			Value = pWinWnd->GetWindowTitle(); break;
				case eThread:		Value = pWinWnd->GetThreadId(); break;
#ifdef WIN32
				case eModule:		Value = pWinWnd->GetModuleString(); break;
#endif
			}

			SWindowNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
					case eHandle:		ColValue.Formated = "0x" + QString::number(Value.toULongLong(), 16); break;
					case eThread:		break; // todo
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

CWndPtr CWindowModel::GetWindow(const QModelIndex &index) const
{
	if (!index.isValid())
        return CWndPtr();

	SWindowNode* pNode = static_cast<SWindowNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pWindow;
}

int CWindowModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CWindowModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eHandle:				return tr("Handle");
#ifdef WIN32
			case eClass:				return tr("Class");
#endif
			case eText:					return tr("Text");
			case eThread:				return tr("Thread");
#ifdef WIN32
			case eModule:				return tr("Module");
#endif
		}
	}
    return QVariant();
}

QVariant CWindowModel::GetDefaultIcon() const 
{ 
	return g_ExeIcon;
}