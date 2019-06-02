#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "..\..\API\ProcessInfo.h"

class CHandleModel;
class QSortFilterProxyModel;

class CHandlesView : public QWidget
{
	Q_OBJECT
public:
	CHandlesView();
	virtual ~CHandlesView();

public slots:
	void					ShowHandles(const CProcessPtr& pProcess);

protected:
	CProcessPtr				m_pCurProcess;

private:
	QVBoxLayout*			m_pMainLayout;

	QWidget*				m_pFilterWidget;

	QTreeViewEx*			m_pHandleList;
	CHandleModel*			m_pHandleModel;
	QSortFilterProxyModel*	m_pSortProxy;

	QWidget*				m_pDetailsWidget;
	QStackedLayout*			m_pDetailsLayout;
};

