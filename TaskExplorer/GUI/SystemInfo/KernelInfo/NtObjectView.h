#pragma once
#include <qwidget.h>
#include "../../../Common/TreeViewEx.h"
#include "../../../Common/PanelView.h"
#include "../../../API/ProcessInfo.h"
#include "../../Models/NtObjectModel.h"

class CNtObjectModel;
class QSortFilterProxyModel;

class CNtObjectView : public CPanelView
{
	Q_OBJECT

public:
	CNtObjectView(QWidget *parent = 0);
	virtual ~CNtObjectView();

	//void					OnMenu(const QPoint &point);

public slots:
	void					Refresh();

protected:
	//virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView()	{ return m_pNtObjectList; }
	virtual QAbstractItemModel* GetModel()	{ return m_pSortProxy; }

private:

	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pNtObjectList;
	CNtObjectModel*			m_pNtObjectModel;
	QSortFilterProxyModel*	m_pSortProxy;
};

