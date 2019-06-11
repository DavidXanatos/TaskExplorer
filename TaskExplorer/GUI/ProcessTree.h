#pragma once
#include <qwidget.h>
#include "../Common/SplitTreeView.h"
#include "..\API\ProcessInfo.h"

class CProcessModel;
class QSortFilterProxyModel;

class CProcessTree : public QWidget
{
	Q_OBJECT
public:
	CProcessTree();
	virtual ~CProcessTree();

signals:
	void					ProcessClicked(const CProcessPtr& pProcess);

private slots:

	void					OnTreeResized(int Width);

	void					OnProcessListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);

	void					OnClicked(const QModelIndex& Index);
	
	void					OnShowHeaderMenu(const QPoint& Point);
	void					OnHeaderMenu();

private:

	QHBoxLayout*			m_pMainLayout;

	CProcessModel*			m_pProcessModel;
	QSortFilterProxyModel*	m_pSortProxy;
	CSplitTreeView*			m_pProcessList;

	QMenu*					m_pMenu;
	QMap<QAction*,int>		m_Columns;
	bool					m_ExpandAll;
};
