#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../../API/Windows/RpcEndpoint.h"
#include "../Models/RpcModel.h"

class CRpcModel;
class QSortFilterProxyModel;

class CRpcView : public CPanelView
{
	Q_OBJECT

public:
	CRpcView(QWidget *parent = 0);
	virtual ~CRpcView();

	//void					OnMenu(const QPoint &point);

public slots:
	void					Refresh();

protected:
	//virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView()	{ return m_pRpcList; }
	virtual QAbstractItemModel* GetModel()	{ return m_pSortProxy; }

	QMap<QString, CRpcEndpointPtr> m_RpcList;

private slots:
	void					OnResetColumns();
	void					OnColumnsChanged();

	void					OnRpcListUpdated(QSet<QString> Added, QSet<QString> Changed, QSet<QString> Removed);

private:

	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pRpcList;
	CRpcModel*				m_pRpcModel;
	QSortFilterProxyModel*	m_pSortProxy;
};

