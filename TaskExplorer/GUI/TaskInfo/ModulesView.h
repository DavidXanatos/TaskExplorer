#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "..\..\API\ProcessInfo.h"

class CModuleModel;
class QSortFilterProxyModel;

class CModulesView : public QWidget
{
	Q_OBJECT
public:
	CModulesView();
	virtual ~CModulesView();

public slots:
	void					ShowModules(const CProcessPtr& pProcess);

private slots:
	void					OnModulesUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);

protected:
	CProcessPtr				m_pCurProcess;

private:
	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pModuleList;
	CModuleModel*			m_pModuleModel;
	QSortFilterProxyModel*	m_pSortProxy;
};

