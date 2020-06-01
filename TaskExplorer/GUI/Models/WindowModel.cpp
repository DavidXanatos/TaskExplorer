#include "stdafx.h"
#include "../TaskExplorer.h"
#include "WindowModel.h"
#include "../../../MiscHelpers/Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinWnd.h"
#endif


CWindowModel::CWindowModel(QObject *parent)
:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bExtThreadId = false;
}

CWindowModel::~CWindowModel()
{
}

QList<QVariant> CWindowModel::MakeWndPath(const CWndPtr& pWindow, const QHash<quint64, CWndPtr>& WindowList)
{
	quint64 ParentID = pWindow->GetParentWnd();
	CWndPtr pParent = WindowList.value(ParentID);

	QList<QVariant> Path;
	if (!pParent.isNull() && ParentID != pWindow->GetHWnd())
	{
		Path = MakeWndPath(pParent, WindowList);
		Path.append(ParentID);
	}
	return Path;
}

bool CWindowModel::TestWndPath(const QList<QVariant>& Path, const CWndPtr& pWindow, const QHash<quint64, CWndPtr>& WindowList, int Index)
{
	quint64 ParentID = pWindow->GetParentWnd();
	CWndPtr pParent = WindowList.value(ParentID);

	if (!pParent.isNull() && ParentID != pWindow->GetHWnd())
	{
		if(Index >= Path.size() || Path[Path.size() - Index - 1] != ParentID)
			return false;

		return TestWndPath(Path, pParent, WindowList, Index + 1);
	}

	return Path.size() == Index;
}

QSet<quint64> CWindowModel::Sync(const QHash<quint64, CWndPtr>& WindowList)
{
	QSet<quint64> Added;
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach (const CWndPtr& pWindow, WindowList)
	{
		quint64 ID = pWindow->GetHWnd();

		QModelIndex Index;
		
		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SWindowNode* pNode = I != Old.end() ? static_cast<SWindowNode*>(I.value()) : NULL;
		if(!pNode || (m_bTree ? !TestWndPath(pNode->Path, pWindow, WindowList) : !pNode->Path.isEmpty()))
		{
			pNode = static_cast<SWindowNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			if(m_bTree)
				pNode->Path = MakeWndPath(pWindow, WindowList);
			pNode->pWindow = pWindow;
			New[pNode->Path].append(pNode);
			Added.insert(ID);
		}
		else
		{
			I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

		int Col = 0;
		bool State = false;
		int Changed = 0;

		// Note: icons are loaded asynchroniusly
		/*if (m_bUseIcons && !pNode->Icon.isValid() && m_Columns.contains(eHandle))
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

		for(int section = 0; section < columnCount(); section++)
		{
			if (!m_Columns.contains(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch(section)
			{
				case eHandle:		Value = pWindow->GetHWnd(); break;
#ifdef WIN32
				case eClass:		Value = pWinWnd->GetWindowClass(); break;
#endif
                case eText:			Value = pWindow->GetWindowTitle(); break;
                case eThread:		Value = pWindow->GetThreadId(); break;
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
					case eThread:		if (m_bExtThreadId)
											ColValue.Formated = tr("%1 (%2): %3").arg(pWindow->GetProcessName()).arg(pWindow->GetProcessId()).arg(pWindow->GetThreadId());
										break;
					case eHandle:		ColValue.Formated = "0x" + QString::number(Value.toULongLong(), 16); break;
					//case eThread:		ColValue.Formated = "0x" + QString::number(Value.toULongLong(), 16); break;
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

	return Added;
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
