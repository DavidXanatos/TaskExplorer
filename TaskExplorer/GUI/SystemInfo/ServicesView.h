#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "..\..\API\ProcessInfo.h"
#include "..\..\API\ServiceInfo.h"

class CServiceModel;
class QSortFilterProxyModel;

class CServicesView : public QWidget
{
	Q_OBJECT

public:
	CServicesView(QWidget *parent = 0);
	virtual ~CServicesView();

private slots:
	void					OnServiceListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed);

private:

	QHBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pServiceList;
	CServiceModel*			m_pServiceModel;
	QSortFilterProxyModel*	m_pSortProxy;
};

