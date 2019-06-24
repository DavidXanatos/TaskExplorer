#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/PanelView.h"
#include "..\..\API\ProcessInfo.h"
#include "..\..\API\DriverInfo.h"
#include "..\Models\DriverModel.h"

class CDriverModel;
class QSortFilterProxyModel;

class CDriversView : public CPanelView
{
	Q_OBJECT

public:
	CDriversView(QWidget *parent = 0);
	virtual ~CDriversView();

	//void					OnMenu(const QPoint &point);

protected:
	//virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView()	{ return m_pDriverList; }
	virtual QAbstractItemModel* GetModel()	{ return m_pDriverModel; }

private slots:
	void					OnDriverListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed);

private:

	QHBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pDriverList;
	CDriverModel*			m_pDriverModel;
	QSortFilterProxyModel*	m_pSortProxy;
};

