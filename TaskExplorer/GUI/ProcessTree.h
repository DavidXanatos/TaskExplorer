#pragma once
#include <qwidget.h>
#include "../Common/SplitTreeView.h"
#include "../Common/HistoryGraph.h"
#include "../API/ProcessInfo.h"
#include "TaskView.h"

class CProcessModel;
class QSortFilterProxyModel;

class CProcessTree : public CTaskView
{
	Q_OBJECT
public:
	CProcessTree(QWidget *parent = 0);
	virtual ~CProcessTree();

	bool					IsTree() const { return m_pProcessList->IsTree(); }

signals:
	void					ProcessClicked(const CProcessPtr& pProcess);
	void					ProcessesSelected(const QList<CProcessPtr>& Processes);

public slots:
	void					OnExpandAll();

private slots:
	void					OnTreeEnabled(bool bEnabled);

	void					OnProcessListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);

	void					OnUpdateHistory();

	void					OnResetColumns();

	//void					OnClicked(const QModelIndex& Index);
	//void					OnDoubleClicked(const QModelIndex& Index);
	void					OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous);
	void					OnSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected);

	void					OnColumnsChanged();

	void					OnQuickRefresh();

	void					OnShowProperties();

	void					OnHeaderMenu(const QPoint& Point);
	void					OnHeaderMenu();
	
	void					OnToolTipCallback(const QVariant& ID, QString& ToolTip);

	//void					OnMenu(const QPoint& Point);

	void					OnCrashDump();
	void					OnProcessAction();
	void					OnWsWatch();
	void					OnWCT();
	void					OnRunAsThis();

	void					OnPermissions();

protected:
	template <class T>
	QList<T>					GetSellectedProcesses()
	{
		QList<T> List;
		foreach(const QModelIndex& Index, m_pProcessList->selectedRows())
		{
			QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
			CProcessPtr pProcess = m_pProcessModel->GetProcess(ModelIndex);
			if(!pProcess.isNull())
				List.append(pProcess);
		}
		return List;
	}
	virtual QList<CTaskPtr>		GetSellectedTasks();

	void						UpdateIndexWidget(int HistoryColumn, int CellHeight, QMap<quint64, CHistoryGraph*>& Graphs, QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> >& History);

	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pProcessList->GetView(); }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }
	//virtual QAbstractItemModel* GetModel()				{ return m_pHandleModel; }
	//virtual QModelIndex			MapToSource(const QModelIndex& Model) { return m_pSortProxy->mapToSource(Model); }

	QMap<quint64, CProcessPtr> m_Processes;

private:
	void					AddHeaderSubMenu(QMenu* m_pHeaderMenu, const QString& Label, int from, int to);

	void					QuickRefresh();

	bool					m_bQuickRefreshPending;

	QVBoxLayout*			m_pMainLayout;

	CProcessModel*			m_pProcessModel;
	QSortFilterProxyModel*	m_pSortProxy;
	CSplitTreeView*			m_pProcessList;

	QMap<quint64, CHistoryGraph*> m_CPU_Graphs;
	QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> > m_CPU_History;
	QMap<quint64, CHistoryGraph*> m_MEM_Graphs;
	QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> > m_MEM_History;
	QMap<quint64, CHistoryGraph*> m_IO_Graphs;
	QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> > m_IO_History;
	QMap<quint64, CHistoryGraph*> m_NET_Graphs;
	QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> > m_NET_History;
	QMap<quint64, CHistoryGraph*> m_GPU_Graphs;
	QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> > m_GPU_History;
	QMap<quint64, CHistoryGraph*> m_VMEM_Graphs;
	QMap<quint64, QPair<QPointer<CHistoryWidget>, QPersistentModelIndex> > m_VMEM_History;

	QMenu*					m_pHeaderMenu;
	QMap<QCheckBox*,int>	m_Columns;
	bool					m_ExpandAll;

	//QMenu*					m_pMenu;

	//QAction*				m_pTerminateTree;
	QAction*				m_pBringInFront;
	QAction*				m_pShowProperties;
	QAction*				m_pOpenPath;
	QAction*				m_pClose;
	QMenu*					m_pMiscMenu;
	QAction*				m_pQuit;
	QAction*				m_pRunAsThis;
	QAction*				m_pCreateDump;
	QAction*				m_pDebug; // []
#ifdef WIN32
	//QAction*				m_pVirtualization; // []
	QAction*				m_pCritical; // []
	QAction*				m_pReduceWS;
	QAction*				m_pWatchWS;
	QAction*				m_pWCT;
	QAction*				m_pPermissions;
#endif
};
