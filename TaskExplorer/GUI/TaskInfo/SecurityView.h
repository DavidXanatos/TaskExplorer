#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/Linux/LinuxProcess.h"

//
// What a process is allowed to do - the Linux counterpart of the Windows Token
// view.
//
// A Windows token carries the user, the groups and the privileges in one
// object. Linux spreads the same information across several places: the
// credentials in /proc/<pid>/status, the five capability sets, whichever LSM is
// active (AppArmor or SELinux), and the seccomp state. This view puts them
// together, because the question a person is asking - "how privileged is this
// thing" - is the same one.
//
class CSecurityView : public CPanelView
{
	Q_OBJECT

public:
	CSecurityView(QWidget *parent = 0);
	virtual ~CSecurityView();

public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					Refresh();

protected:
	virtual void			OnMenu(const QPoint& Point);
	virtual QTreeView*		GetView()	{ return m_pList->GetView(); }
	virtual QAbstractItemModel* GetModel() { return nullptr; }

	QSharedPointer<CLinuxProcess>	m_pCurProcess;

private:
	void					SetValue(const QString& Group, const QString& Name, const QString& Value);
	void					PruneStale();

	QGridLayout*			m_pMainLayout;
	CPanelWidgetEx*			m_pList;

	QSet<QString>			m_LiveKeys;
	QMap<QString, QTreeWidgetItem*>	m_Items;
	QMap<QString, QTreeWidgetItem*>	m_Groups;
};
