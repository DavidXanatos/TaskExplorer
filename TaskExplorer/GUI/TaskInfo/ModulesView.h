#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../Models/ModuleModel.h"
#include "../../../MiscHelpers/Common/SortFilterProxyModel.h"

class CModulesView : public CPanelView
{
	Q_OBJECT
public:
	CModulesView(bool bGlobal, QWidget *parent = 0);
	virtual ~CModulesView();

public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					Refresh();
	void					ShowModules(const QMap<quint64, CModulePtr>& Modules);

private slots:
	void					OnResetColumns();
	void					OnColumnsChanged();

	void					OnModulesUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void					OnDoubleClicked();

	void					OnMenu(const QPoint &point);

	void					OnUnload();
	void					OnLoad();

protected:
	virtual QTreeView*			GetView() 				{ return m_pModuleList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }
	//virtual QAbstractItemModel* GetModel()				{ return m_pModuleModel; }
	//virtual QModelIndex			MapToSource(const QModelIndex& Model) { return m_pSortProxy->mapToSource(Model); }

	enum EView
	{
		eNone,
		eSingle,
		eMulti
	};

	virtual void			SwitchView(EView ViewMode);

	EView					m_ViewMode;

	bool					m_bGlobal;
	CProcessPtr				m_pCurProcess;

	QMap<quint64, CModulePtr> m_Modules;

private:
	QVBoxLayout*			m_pMainLayout;

	QWidget*				m_pFilterWidget;
	QHBoxLayout*			m_pFilterLayout;
	QPushButton*			m_pLoadModule;
	QCheckBox*				m_pShowModPages;

	QTreeViewEx*			m_pModuleList;
	CModuleModel*			m_pModuleModel;
	QSortFilterProxyModel*	m_pSortProxy;

	//QMenu*					m_pMenu;
	QAction*				m_pOpen;
	QAction*				m_pUnload;
};

