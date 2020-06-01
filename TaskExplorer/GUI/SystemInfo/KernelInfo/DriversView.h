#pragma once
#include <qwidget.h>
#include "../../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../../MiscHelpers/Common/PanelView.h"
#include "../../../API/ProcessInfo.h"
#include "../../../API/DriverInfo.h"
#include "../../Models/DriverModel.h"

class CDriverModel;
class QSortFilterProxyModel;

class CDriversView : public CPanelView
{
	Q_OBJECT

public:
	CDriversView(QWidget *parent = 0);
	virtual ~CDriversView();

	//void					OnMenu(const QPoint &point);

public slots:
	void					Refresh();

protected:
	//virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView()	{ return m_pDriverList; }
	virtual QAbstractItemModel* GetModel()	{ return m_pSortProxy; }

	QMap<QString, CDriverPtr> m_DriverList;

private slots:
	void					OnResetColumns();
	void					OnColumnsChanged();

	void					OnDriverListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed);

private:

	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pDriverList;
	CDriverModel*			m_pDriverModel;
	QSortFilterProxyModel*	m_pSortProxy;
};

