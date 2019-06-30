#pragma once
#include <qwidget.h>
#include "../../Common/TreeWidgetEx.h"
#include "../../Common/TreeViewEx.h"
#include "../../Common/PanelView.h"
#include "..\..\API\ProcessInfo.h"
#include "..\Models\HandleModel.h"
#include "..\..\Common\SortFilterProxyModel.h"

class CHandleFilterModel: public CSortFilterProxyModel
{
public:
	CHandleFilterModel(bool bAlternate, QObject* parrent = 0) : CSortFilterProxyModel(bAlternate, parrent) 
	{
		m_bHideUnnmed = false;
		m_bHideEtwReg = false;
	}

	void SetFilter(const QString& Type, bool bHideUnnmed, bool bHideEtwReg)
	{
		m_ShowType = Type;
		m_bHideUnnmed = bHideUnnmed;
		m_bHideEtwReg = bHideEtwReg;
		invalidateFilter();
	}

	bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const
	{
		if (m_bHideUnnmed)
		{
			QModelIndex source_index = sourceModel()->index(source_row, CHandleModel::eName, source_parent);
			if (source_index.isValid())
			{
				QString Name = sourceModel()->data(source_index, filterRole()).toString();
				if (Name.isEmpty())
					return false;
			}
		}

		if (!m_ShowType.isEmpty() || m_bHideEtwReg)
		{
			QModelIndex source_index = sourceModel()->index(source_row, CHandleModel::eType, source_parent);
			if (source_index.isValid())
			{
				QString Type = sourceModel()->data(source_index, filterRole()).toString();

				if (m_bHideEtwReg && Type == "EtwRegistration")
					return false;

				if (!m_ShowType.isEmpty() && Type != m_ShowType)
					return false;
			}
		}

		// call parent if this filter passed
		return CSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
	}

protected:
	QString m_ShowType;
	bool m_bHideUnnmed;
	bool m_bHideEtwReg;
};

class CHandlesView : public CPanelView
{
	Q_OBJECT
public:
	CHandlesView(bool bAll = false, QWidget *parent = 0);
	virtual ~CHandlesView();

public slots:
	void					ShowHandles(const CProcessPtr& pProcess);
	void					UpdateFilter();
	void					UpdateFilter(const QString & filter);
	//void					OnShowDetails();
	void					ShowAllFiles();

private slots:
	void					ShowHandles(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void					OnItemSelected(const QModelIndex &current);

	//void					OnMenu(const QPoint &point);

	void					OnHandleAction();
	void					OnPermissions();

protected:
	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pHandleList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }
	//virtual QAbstractItemModel* GetModel()				{ return m_pHandleModel; }
	//virtual QModelIndex			MapToSource(const QModelIndex& Model) { return m_pSortProxy->mapToSource(Model); }

	CProcessPtr				m_pCurProcess;

private:
	QVBoxLayout*			m_pMainLayout;

	QWidget*				m_pFilterWidget;
	QHBoxLayout*			m_pFilterLayout;
	QComboBox*				m_pShowType;
	QCheckBox*				m_pHideUnnamed;
	QCheckBox*				m_pHideETW;
	//QPushButton*			m_pShowDetails;

	QTreeViewEx*			m_pHandleList;
	CHandleModel*			m_pHandleModel;
	CHandleFilterModel*		m_pSortProxy;

	QSplitter*				m_pSplitter;

	CPanelWidget<QTreeWidgetEx>* m_pHandleDetails;

	//QMenu*					m_pMenu;
	QAction*				m_pClose;
	QAction*				m_pProtect;
	QAction*				m_pInherit;

	QAction*				m_pTokenInfo;
	QAction*				m_pJobInfo;
	QMenu*					m_pSemaphore;
	QAction*				m_pSemaphoreAcquire;
	QAction*				m_pSemaphoreRelease;
	QMenu*					m_pEvent;
	QAction*				m_pEventSet;
	QAction*				m_pEventReset;
	QAction*				m_pEventPulse;
	QMenu*					m_pEventPair;
	QAction*				m_pEventSetLow;
	QAction*				m_pEventSetHigh;
	QMenu*					m_pTimer;
	QAction*				m_pTimerCancel;
	QMenu*					m_pTask;
	QAction*				m_pTerminate;
	QAction*				m_pSuspend;
	QAction*				m_pResume;
	QAction*				m_pPermissions;
};

