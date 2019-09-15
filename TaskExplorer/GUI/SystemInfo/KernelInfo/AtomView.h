#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/TreeWidgetEx.h"
#include "../../Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../../Common/ListItemModel.h"
#include "../../Common/SortFilterProxyModel.h"

class CAtomView : public CPanelView
{
	Q_OBJECT
public:
	CAtomView(QWidget *parent = 0);
	virtual ~CAtomView();

public slots:
	void					Refresh();

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	//void					OnMenu(const QPoint &point);
	//void					OnItemDoubleClicked(const QModelIndex& Index);

	void					OnDelete();

protected:
	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pAtomList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }

	QList<QVariantMap>		m_Atoms;

private:
	enum EColumns
	{
		eName = 0,
		eRefCount,
		eCount
	};

	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pAtomList;
	CSimpleListModel*		m_pAtomModel;
	CSortFilterProxyModel*	m_pSortProxy;

	//QMenu*					m_pMenu;
	QAction*				m_pDelete;
};

