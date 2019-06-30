#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/PanelView.h"
#include "..\..\API\ProcessInfo.h"
#include "..\..\API\ServiceInfo.h"
#include "..\Models\ServiceModel.h"

class CServiceModel;
class QSortFilterProxyModel;

class CServicesView : public CPanelView
{
	Q_OBJECT

public:
	CServicesView(bool bAll, QWidget *parent = 0);
	virtual ~CServicesView();

	//void					OnMenu(const QPoint &point);

#ifdef WIN32
	void					SetShowKernelServices(bool bShow);
#endif

public slots:
	void					ShowServices(const CProcessPtr& pProcess);

protected:
	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView()	{ return m_pServiceList; }
	virtual QAbstractItemModel* GetModel()	{ return m_pServiceModel; }

private slots:
	void					OnServiceListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed);

	void					OnServiceAction();
	void					OnDoubleClicked(const QModelIndex& Index);

private:
	QHBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pServiceList;
	CServiceModel*			m_pServiceModel;
	QSortFilterProxyModel*	m_pSortProxy;


	QAction*				m_pMenuStart;
	QAction*				m_pMenuContinue;
	QAction*				m_pMenuPause;
	QAction*				m_pMenuStop;
	//QAction*				m_pMenuRestart;
	QAction*				m_pMenuDelete;
#ifdef WIN32
	QAction*				m_pMenuOpenKey;
#endif
};

