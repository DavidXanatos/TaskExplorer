#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../Models/ThreadModel.h"
#include "../../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "StackView.h"
#include "../TaskView.h"
#include "../../../MiscHelpers/Common/HistoryGraph.h"

class CThreadsView : public CTaskView
{
	Q_OBJECT
public:
	CThreadsView(QWidget *parent = 0);
	virtual ~CThreadsView();

public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					SellectThread(quint64 ThreadId);
	void					Refresh();

private slots:
	void					OnResetColumns();
	void					OnColumnsChanged();

	void					ShowThreads(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);

	void					OnUpdateHistory();

	//void					OnClicked(const QModelIndex& Index);
	void					OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

	//void					OnMenu(const QPoint &point);

	void					OnThreadAction();
	void					OnThreadToken();

	void					OnPermissions();

	void					ShowStack(const CStackTracePtr& StackTrace);

protected:
	virtual QList<CTaskPtr>		GetSellectedTasks();

	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pThreadList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }
	//virtual QAbstractItemModel* GetModel()				{ return m_pHandleModel; }
	//virtual QModelIndex			MapToSource(const QModelIndex& Model) { return m_pSortProxy->mapToSource(Model); }

	enum EView
	{
		eNone,
		eSingle,
		eMulti
	};

	virtual void			SwitchView(EView ViewMode);

	EView					m_ViewMode;

	QList<CProcessPtr>		m_Processes;
	//int						m_PendingUpdates;
	CThreadPtr				m_pCurThread;
	QMap<quint64, CThreadPtr> m_Threads;

	quint64					m_CurStackTraceJob;

private:
	QVBoxLayout*			m_pMainLayout;

	//QWidget*				m_pFilterWidget;

	QSplitter*				m_pSplitter;

	QTreeViewEx*			m_pThreadList;
	CThreadModel*			m_pThreadModel;
	QSortFilterProxyModel*	m_pSortProxy;

	QMap<quint64, CHistoryGraph*> m_CPU_Graphs;
	QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> > m_CPU_History;

	CStackView*				m_pStackView;

	//QMenu*					m_pMenu;

#ifdef WIN32
	QMenu*					m_pMiscMenu;
	QAction*				m_pCancelIO;
	//QAction*				m_pAnalyze;
	QAction*				m_pCritical;
	QAction*				m_pToken;
	QAction*				m_pPermissions;
#endif
	//QAction*				m_pWindows;


};
