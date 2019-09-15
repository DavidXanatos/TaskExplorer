#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../../API/SocketInfo.h"
#include "../Models/MemoryModel.h"
#include "../../Common/SortFilterProxyModel.h"

class CMemoryFilterModel: public CSortFilterProxyModel
{
public:
	CMemoryFilterModel(bool bAlternate, QObject* parrent = 0) : CSortFilterProxyModel(bAlternate, parrent) 
	{
		m_bHideFree = false;
	}

	void SetFilter(bool bHideFree)
	{
		m_bHideFree = bHideFree;
		invalidateFilter();
	}

	bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const
	{
		if (m_bHideFree)
		{
			QModelIndex source_index = sourceModel()->index(source_row, CMemoryModel::eBaseAddress, source_parent);
			if (source_index.isValid())
			{
				CMemoryPtr pMemory = ((CMemoryModel*)sourceModel())->GetMemory(source_index);
				if (pMemory.isNull() || pMemory->IsFree())
					return false;
			}
		}

		// call parent if this filter passed
		return CSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
	}

protected:
	bool m_bHideFree;
};

class CMemoryView : public CPanelView
{
	Q_OBJECT
public:
	CMemoryView(QWidget *parent = 0);
	virtual ~CMemoryView();

public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					Refresh() {} // only manual refresh

private slots:
	void					OnResetColumns();
	void					OnColumnsChanged();

	void					OnDoubleClicked();
	void					OnRefresh();
	void					OnSearch();

	void					UpdateFilter();

	//void					OnMenu(const QPoint &point);
	void					OnSaveMemory();
	void					OnFreeMemory();
	void					OnProtectMemory();

protected:
	virtual void				OnMenu(const QPoint& Point);

	virtual QTreeView*			GetView() 				{ return m_pMemoryList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }
	//virtual QAbstractItemModel* GetModel()				{ return m_pSocketModel; }
	//virtual QModelIndex			MapToSource(const QModelIndex& Model) { return m_pSortProxy->mapToSource(Model); }
	
	CProcessPtr				m_pCurProcess;

	QMultiMap<quint64, CMemoryPtr> m_MemoryList;

private:

	QVBoxLayout*			m_pMainLayout;

	QWidget*				m_pFilterWidget;
	QHBoxLayout*			m_pFilterLayout;
	QCheckBox*				m_pHideFree;
	QPushButton*			m_pRefresh;
	QPushButton*			m_pSearch;

	QTreeViewEx*			m_pMemoryList;
	CMemoryModel*			m_pMemoryModel;
	CMemoryFilterModel*		m_pSortProxy;

	//QMenu*					m_pMenu;
	QAction*				m_pMenuEdit;
	QAction*				m_pMenuSave;
	QAction*				m_pMenuProtect;
	QAction*				m_pMenuFree;
	QAction*				m_pMenuDecommit;
};

