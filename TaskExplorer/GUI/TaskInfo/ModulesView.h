#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/PanelView.h"
#include "..\..\API\ProcessInfo.h"
#include "..\Models\ModuleModel.h"
#include "..\Models\SortFilterProxyModel.h"

class CModulesView : public CPanelView
{
	Q_OBJECT
public:
	CModulesView(QWidget *parent = 0);
	virtual ~CModulesView();

public slots:
	void					ShowModules(const CProcessPtr& pProcess);

private slots:
	void					OnModulesUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);

	void					OnMenu(const QPoint &point);

	void					OnUnload();


protected:
	virtual QTreeView*			GetView() 				{ return m_pModuleList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }
	//virtual QAbstractItemModel* GetModel()				{ return m_pModuleModel; }
	//virtual QModelIndex			MapToSource(const QModelIndex& Model) { return m_pSortProxy->mapToSource(Model); }

	CProcessPtr				m_pCurProcess;

private:
	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pModuleList;
	CModuleModel*			m_pModuleModel;
	QSortFilterProxyModel*	m_pSortProxy;

	//QMenu*					m_pMenu;
	QAction*				m_pUnload;
};

