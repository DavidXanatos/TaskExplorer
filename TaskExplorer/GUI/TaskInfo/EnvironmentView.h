#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../../../MiscHelpers/Common/ListItemModel.h"
#include "../../../MiscHelpers/Common/SortFilterProxyModel.h"

class CEnvironmentView : public CPanelView
{
	Q_OBJECT
public:
	CEnvironmentView(QWidget *parent = 0);
	virtual ~CEnvironmentView();

public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					Refresh();

private slots:
	//void					OnResetColumns();
	void					OnColumnsChanged();

	//void					OnMenu(const QPoint &point);
	void					OnItemDoubleClicked(const QModelIndex& Index);

	void					OnEdit();
	void					OnAdd();
	void					OnDelete();

protected:
	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pVariablesList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }

	CProcessPtr				m_pCurProcess;

	QList<QVariantMap>		m_Variables;

private:
	enum EColumns
	{
		eName = 0,
		eType,
		eValue,
		eCount
	};

	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pVariablesList;
	CSimpleListModel*		m_pVariablesModel;
	CSortFilterProxyModel*	m_pSortProxy;

	//QMenu*					m_pMenu;
	QAction*				m_pEdit;
	QAction*				m_pAdd;
	QAction*				m_pDelete;
};

