#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/Linux/LinuxProcess.h"

//
// The control group a process belongs to.
//
// This is the Linux counterpart of the Windows Job view: a cgroup is a set of
// processes with shared resource accounting and shared limits, which is exactly
// what a job object is. On a systemd machine every process is in one, and the
// hierarchy - slices, scopes, services - is how the system is actually
// organised, so this often says more about a process than its parent does.
//
// Read only. Writing limits means either running as root or asking systemd to
// change the unit's properties, which is a different kind of operation from
// everything else in this view and is better done deliberately than from a
// properties tab.
//
class CCGroupView : public CPanelView
{
	Q_OBJECT

public:
	CCGroupView(QWidget *parent = 0);
	virtual ~CCGroupView();

public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					Refresh();

private slots:
	void					OnResetColumns();
	void					OnColumnsChanged();

protected:
	virtual void			OnMenu(const QPoint& Point);
	virtual QTreeView*		GetView()	{ return m_pList->GetView(); }
	virtual QAbstractItemModel* GetModel() { return nullptr; }

	QSharedPointer<CLinuxProcess>	m_pCurProcess;

private:
	// Adds or updates one row, keyed by name so the tree is rebuilt in place
	// rather than cleared - which would lose the selection on every refresh.
	void					SetValue(const QString& Group, const QString& Name, const QString& Value);
	void					PruneStale();

	QGridLayout*			m_pMainLayout;

	QLabel*					m_pPathLabel;
	QLineEdit*				m_pPath;

	CPanelWidgetEx*			m_pList;

	// Rows present after the current refresh; anything not in here is removed.
	QSet<QString>			m_LiveKeys;
	QMap<QString, QTreeWidgetItem*>	m_Items;
	QMap<QString, QTreeWidgetItem*>	m_Groups;
};
