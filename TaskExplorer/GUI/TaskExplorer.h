#pragma once

#include <QtWidgets/QMainWindow>
#include "GraphBar.h"
#include "ProcessTree.h"
#include "SystemInfo\SystemInfoView.h"
#include "TaskInfo\TaskInfoView.h"
#include "API\SystemAPI.h"
#include "Common/Settings.h"

#define VERSION_MJR		0
#define VERSION_MIN 	0
#define VERSION_REV 	8
#define VERSION_UPD 	0

class CTaskExplorer : public QMainWindow
{
	Q_OBJECT

public:
	CTaskExplorer(QWidget *parent = Q_NULLPTR);
	virtual ~CTaskExplorer();

protected:
	void				timerEvent(QTimerEvent* pEvent);
	void				closeEvent(QCloseEvent *e);

	void				UpdateAll();

	//quint16				m_uTimerCounter;
	int					m_uTimerID;

private slots:
	void				OnRun();
	void				OnRunAdmin();
	void				OnRunUser();
	void				OnRunAs();
	void				OnRunSys();
	void				OnElevate();

	void				OnExit();

	void				OnSysTab();
	void				OnTaskTab();

	void				OnSysTray(QSystemTrayIcon::ActivationReason Reason);

	void				OnAbout();

private:

	QWidget*			m_pMainWidget;
	QVBoxLayout*		m_pMainLayout;

	CGraphBar*			m_pGraphBar;

	QSplitter*			m_pMainSplitter;

	CProcessTree*		m_pProcessTree;

	QSplitter*			m_pSubSplitter;

	CSystemInfoView*	m_pSystemInfo;

	CTaskInfoView*		m_pTaskInfo;

	QMenu*				m_pMenuProcess;
	QMap<QAction*, int> m_Act2Tab;

	QMenu*				m_pMenuHelp;
	QAction*			m_pMenuAbout;
#ifdef WIN32
	QAction*			m_pMenuAboutPH;
#endif
	QAction*			m_pMenuAboutQt;

	QSystemTrayIcon*	m_pTrayIcon;
	QMenu*				m_pTrayMenu;

	bool				m_bExit;
};

extern CSettings*	theConf;
extern CSystemAPI*	theAPI;

extern QIcon g_ExeIcon;
extern QIcon g_DllIcon;