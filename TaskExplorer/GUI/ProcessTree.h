#pragma once
#include <qwidget.h>
#include "../Common/TreeWidgetEx.h"
#include "../Common/TreeViewEx.h"
#include "..\API\Process.h"

class CProcessModel;
class QSortFilterProxyModel;

class CProcessTree : public QWidget
{
	Q_OBJECT
public:
	CProcessTree();
	virtual ~CProcessTree();

signals:
	void				ProcessClicked(const CProcessPtr& pProcess);

private slots:

	void				OnSplitterMoved(int pos, int index);

	void				OnProcessListUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);

	void				OnClicked(const QModelIndex& Index);

private:

	QHBoxLayout*			m_pMainLayout;

	bool					m_bTreeHidden;
	QSplitter*				m_pProcessSplitter;
	QTreeView*				m_pProcessTree;
	QTreeViewEx*			m_pProcessList;
	CProcessModel*			m_pProcessModel;
	QSortFilterProxyModel*	m_pSortProxy;
};
