#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/TreeWidgetEx.h"
#include "../../Common/PanelView.h"
#include "..\..\API\ProcessInfo.h"
#include "..\Models\ThreadModel.h"
#include "..\..\Common\SortFilterProxyModel.h"
#include "StackView.h"
#include "../TaskView.h"
#include "../Common/HistoryGraph.h"

class CThreadsView : public CTaskView
{
	Q_OBJECT
public:
	CThreadsView(QWidget *parent = 0);
	virtual ~CThreadsView();

public slots:
	void					ShowThreads(const CProcessPtr& pProcess);

private slots:
	void					OnUpdateHistory();

	//void					OnClicked(const QModelIndex& Index);
	void					OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

	//void					OnMenu(const QPoint &point);

	void					OnThreadAction();

protected:
	virtual QList<CTaskPtr>		GetSellectedTasks();

	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pThreadList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }
	//virtual QAbstractItemModel* GetModel()				{ return m_pHandleModel; }
	//virtual QModelIndex			MapToSource(const QModelIndex& Model) { return m_pSortProxy->mapToSource(Model); }

	CProcessPtr				m_pCurProcess;
	CThreadPtr				m_pCurThread;

private:
	QVBoxLayout*			m_pMainLayout;

	QWidget*				m_pFilterWidget;

	QSplitter*				m_pSplitter;

	QTreeViewEx*			m_pThreadList;
	CThreadModel*			m_pThreadModel;
	QSortFilterProxyModel*	m_pSortProxy;
	QMap<quint64, QPair<QPointer<CHistoryGraph>, QPersistentModelIndex> > m_CPU_History;

	CStackView*				m_pStackView;

	//QMenu*					m_pMenu;

#ifdef WIN32
	QAction*				m_pCancelIO;
	//QAction*				m_pAnalyze;
	QAction*				m_pCritical;
	//QAction*				m_pPermissions;
	//QAction*				m_pToken;
#endif
	//QAction*				m_pWindows;


};
