#pragma once
#include <qwidget.h>
#include "../../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../../MiscHelpers/Common/PanelView.h"
#include "../../../API/ProcessInfo.h"
#include "../../../API/Windows/WinPoolEntry.h"
#include "../../Models/PoolModel.h"

class CPoolModel;
class QSortFilterProxyModel;

class CPoolView : public CPanelView
{
	Q_OBJECT

public:
	CPoolView(QWidget *parent = 0);
	virtual ~CPoolView();

	//void					OnMenu(const QPoint &point);

public slots:
	void					Refresh();

protected:
	//virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView()	{ return m_pPoolList; }
	virtual QAbstractItemModel* GetModel()	{ return m_pSortProxy; }

	QMap<quint64, CPoolEntryPtr> m_PoolList;

private slots:
	//void					OnResetColumns();
	void					OnColumnsChanged();

	void					OnPoolListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);

private:

	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pPoolList;
	CPoolModel*				m_pPoolModel;
	QSortFilterProxyModel*	m_pSortProxy;
};

