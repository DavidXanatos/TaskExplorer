#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../../../MiscHelpers/Common/ListItemModel.h"
#include "../../../MiscHelpers/Common/SortFilterProxyModel.h"

class CRunObjView : public CPanelView
{
	Q_OBJECT
public:
	CRunObjView(QWidget *parent = 0);
	virtual ~CRunObjView();

public slots:
	void					Refresh();

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	//void					OnMenu(const QPoint &point);
	//void					OnItemDoubleClicked(const QModelIndex& Index);

protected:
	//virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pRunObjList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }

	QList<QVariantMap>		m_RunObjs;

private:
	enum EColumns
	{
		eName = 0,
		eCount
	};

	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pRunObjList;
	CSimpleListModel*		m_pRunObjModel;
	CSortFilterProxyModel*	m_pSortProxy;

	//QMenu*					m_pMenu;
};

