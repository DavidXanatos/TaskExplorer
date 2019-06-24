#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/PanelView.h"
#include "..\..\API\ProcessInfo.h"
#include "..\..\API\SocketInfo.h"
#include "..\Models\SocketModel.h"
#include "..\..\Common\SortFilterProxyModel.h"

class CSocketsView : public CPanelView
{
	Q_OBJECT
public:
	CSocketsView(bool bAll = false, QWidget *parent = 0);
	virtual ~CSocketsView();

public slots:
	void					ShowSockets(const CProcessPtr& pProcess);

private slots:
	void					OnSocketListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);

	//void					OnMenu(const QPoint &point);

	void					OnClose();

protected:
	virtual void				OnMenu(const QPoint& Point);

	virtual QTreeView*			GetView() 				{ return m_pSocketList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }
	//virtual QAbstractItemModel* GetModel()				{ return m_pSocketModel; }
	//virtual QModelIndex			MapToSource(const QModelIndex& Model) { return m_pSortProxy->mapToSource(Model); }
	
private:

	QHBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pSocketList;
	CSocketModel*			m_pSocketModel;
	QSortFilterProxyModel*	m_pSortProxy;

	//QMenu*					m_pMenu;
	QAction*				m_pClose;
};

