#pragma once
#include "../Common/TreeViewEx.h"
#include "../API/ProcessInfo.h"


class CProcessModel;
class QSortFilterProxyModel;

class CProcessPicker : public QDialog
{
public:
	CProcessPicker(QWidget* parent = 0);
	virtual ~CProcessPicker();

	quint64		GetProcessId() const { return m_ProcessId; }

private slots:
	void					OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

protected:
	void reject()
	{
		hide();
	}

	void closeEvent(QCloseEvent *e)
	{
		hide();
		e->ignore();
	}

	quint64					m_ProcessId;

private:
	QVBoxLayout*			m_pMainLayout;

	CProcessModel*			m_pProcessModel;
	QSortFilterProxyModel*	m_pSortProxy;
	QTreeViewEx*			m_pProcessList;

	QDialogButtonBox *		m_pButtonBox;
};
