#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/TreeWidgetEx.h"
#include "..\..\API\ProcessInfo.h"

class CThreadModel;
class QSortFilterProxyModel;


class CThreadsView : public QWidget
{
	Q_OBJECT
public:
	CThreadsView();
	virtual ~CThreadsView();

public slots:
	void					ShowThreads(const CProcessPtr& pProcess);

private slots:
	void					OnClicked(const QModelIndex& Index);

	void					OnStackTraced(const CStackTracePtr& StackTrace);

protected:
	CProcessPtr				m_pCurProcess;
	CThreadPtr				m_pCurThread;

private:
	enum EStackColumns
	{
		eStack = 0,
		eName,
		eStackAddress,
		eFrameAddress,
		eControlAddress,
		eReturnAddress,
		eStackParameter,
		eCount
	};

	QVBoxLayout*			m_pMainLayout;

	QWidget*				m_pFilterWidget;

	QSplitter*				m_pSplitter;

	QTreeViewEx*			m_pThreadList;
	CThreadModel*			m_pThreadModel;
	QSortFilterProxyModel*	m_pSortProxy;

	QTreeWidgetEx*			m_pStackList;
};

