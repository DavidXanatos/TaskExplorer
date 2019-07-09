#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/TreeWidgetEx.h"
#include "../../Common/PanelView.h"
#include "..\..\API\ProcessInfo.h"

class CEnvironmentView : public CPanelView
{
	Q_OBJECT
public:
	CEnvironmentView(QWidget *parent = 0);
	virtual ~CEnvironmentView();

public slots:
	void					ShowProcess(const CProcessPtr& pProcess);
	void					Refresh();

private slots:
	//void					OnMenu(const QPoint &point);
	void					OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column);

	void					OnEdit();
	void					OnAdd();
	void					OnDelete();

protected:
	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pVariablesList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pVariablesList->model(); }

	CProcessPtr				m_pCurProcess;

private:
	enum EStackColumns
	{
		eName = 0,
		eType,
		eValue,
		eCount
	};

	QVBoxLayout*			m_pMainLayout;

	QTreeWidgetEx*			m_pVariablesList;

	//QMenu*					m_pMenu;
	QAction*				m_pEdit;
	QAction*				m_pAdd;
	QAction*				m_pDelete;
};

