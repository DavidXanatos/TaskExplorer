#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "..\..\API\Process.h"
#include "..\..\API\SocketInfo.h"

class CSocketModel;
class QSortFilterProxyModel;

class CSocketsView : public QWidget
{
	Q_OBJECT
public:
	CSocketsView(bool bAll = false);
	virtual ~CSocketsView();

public slots:
	void					ShowSockets(const CProcessPtr& pProcess);

private slots:
	void					OnSocketListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);

private:

	QHBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pSocketList;
	CSocketModel*			m_pSocketModel;
	QSortFilterProxyModel*	m_pSortProxy;
};

