#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../Models/HandleModel.h"
#include "../../../MiscHelpers/Common/SortFilterProxyModel.h"

/*class CHandleFilterModel: public CSortFilterProxyModel
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

				if (!m_ShowType.isEmpty() && !Type.contains(m_ShowType))
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
};*/

class CHandlesView : public CPanelView
{
	Q_OBJECT
public:
	CHandlesView(int iAll = 0, QWidget *parent = 0);
	virtual ~CHandlesView();

public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					Refresh();
	void					ShowHandles(const QMap<quint64, CHandlePtr>& Handles);

	void					UpdateFilter();
	void					UpdateFilter(const QString & filter);
	//void					OnShowDetails();

private slots:
	void					OnResetColumns();
	void					OnColumnsChanged();

	void					ShowHandles(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void					ShowOpenFiles(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void					OnItemSelected(const QModelIndex &current);
	void					OnDoubleClicked();

	//void					OnMenu(const QPoint &point);

	void					OnHandleAction();
	void					OnPermissions();

protected:
	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pHandleList; }
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

	int						m_ShowAllFiles;
	QList<CProcessPtr>		m_Processes;
	int						m_PendingUpdates;

	QMap<quint64, CHandlePtr> m_Handles;

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
	//CHandleFilterModel*		m_pSortProxy;
	CSortFilterProxyModel*	m_pSortProxy;

	QSplitter*				m_pSplitter;

	CPanelWidgetEx* m_pHandleDetails;

	//QMenu*					m_pMenu;
	QAction*				m_pClose;
	QAction*				m_pProtect;
	QAction*				m_pInherit;

	QAction*				m_pOpen;
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

