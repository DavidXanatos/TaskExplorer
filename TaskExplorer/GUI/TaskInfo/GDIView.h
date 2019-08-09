#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/TreeWidgetEx.h"
#include "../../Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../Models/GDIModel.h"
#include "../../Common/SortFilterProxyModel.h"

// Note: CGDIView is a windows only class don't include it on a unix build

class CGDIView : public CPanelView
{
	Q_OBJECT
public:
	CGDIView(QWidget *parent = 0);
	virtual ~CGDIView();

public slots:
	void					ShowProcess(const CProcessPtr& pProcess);
	void					Refresh();

private slots:
	//void					OnMenu(const QPoint &point);
	//void					OnItemDoubleClicked(const QModelIndex& Index);

protected:
	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pGDIList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }

	CProcessPtr				m_pCurProcess;

	QMap<quint64, CWinGDIPtr>	m_GDIList;

private:

	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pGDIList;
	CGDIModel*				m_pGDIModel;
	CSortFilterProxyModel*	m_pSortProxy;

	//QMenu*					m_pMenu;

};

