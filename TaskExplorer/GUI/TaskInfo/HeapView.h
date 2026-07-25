#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../Models/HeapModel.h"
#include "../../../MiscHelpers/Common/SortFilterProxyModel.h"

// Note: CHeapView is a windows only class don't include it on a unix build

class CHeapView : public CPanelView
{
	Q_OBJECT
public:
	CHeapView(QWidget *parent = 0);
	virtual ~CHeapView();

public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					Refresh() {} // only manual refresh

private slots:
	void					OnResetColumns();
	void					OnColumnsChanged();

	void					OnRefresh();
	void					OnFlush();	

	//void					OnMenu(const QPoint &point);
	//void					OnItemDoubleClicked(const QModelIndex& Index);

protected:
	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pHeapList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }

	CProcessPtr				m_pCurProcess;

	QMap<quint64, CHeapPtr>	m_HeapList;

private:

	QVBoxLayout*			m_pMainLayout;

	QWidget*				m_pFilterWidget;
	QHBoxLayout*			m_pFilterLayout;
	QPushButton*			m_pRefresh;
	QPushButton*			m_pFlush;

	QTreeViewEx*			m_pHeapList;
	CHeapModel*				m_pHeapModel;
	CSortFilterProxyModel*	m_pSortProxy;

	//QMenu*					m_pMenu;

};

