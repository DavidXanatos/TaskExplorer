#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/TreeWidgetEx.h"
#include "../../Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../../Common/TreeItemModel.h"
#include "../../Common/SortFilterProxyModel.h"
#include "../../API/AssemblyList.h"

class CDotNetView : public CPanelView
{
	Q_OBJECT
public:
	CDotNetView(QWidget *parent = 0);
	virtual ~CDotNetView();

public slots:
	void					ShowProcess(const CProcessPtr& pProcess);
	void					Refresh();

private slots:
	//void					OnMenu(const QPoint &point);
	void					OnDoubleClicked();

	void					OnRefresh();

	void					OnAssemblies(const CAssemblyListPtr& List);

protected:
	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pAssemblyList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }

	CProcessPtr				m_pCurProcess;

private:
	enum EColumns
	{
		eStructure = 0,
		eFileName,
		eFlags,
		eID,
		eNativePath,
		eCount
	};

	QVBoxLayout*			m_pMainLayout;

	QWidget*				m_pFilterWidget;
	QHBoxLayout*			m_pFilterLayout;
	QPushButton*			m_pRefresh;
	
	QTreeViewEx*			m_pAssemblyList;
	CSimpleTreeModel*		m_pAssemblyModel;
	CSortFilterProxyModel*	m_pSortProxy;

	QSplitter*				m_pSplitter;

	CPanelWidget<QTreeWidgetEx>* m_pPerfStats;
	QMap<int, QTreeWidgetItem*> m_PerfCounters;

	//QMenu*					m_pMenu;
	QAction*				m_pOpen;
};

