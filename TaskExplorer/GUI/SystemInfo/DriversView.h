#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "..\..\API\ProcessInfo.h"
#include "..\..\API\ServiceInfo.h"

class CServiceModel;
class QSortFilterProxyModel;

class CDriversView : public QWidget
{
	Q_OBJECT

public:
	CDriversView(QWidget *parent = 0);
	virtual ~CDriversView();

private slots:
	void					OnDriverListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed);

private:

	QHBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pDriverList;
	CServiceModel*			m_pDriverModel;
	QSortFilterProxyModel*	m_pSortProxy;
};

