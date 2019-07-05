#pragma once

#include <QtWidgets/QMainWindow>
#include "GraphBar.h"
#include "ProcessTree.h"
#include "SystemInfo\SystemInfoView.h"
#include "TaskInfo\TaskInfoView.h"
#include "API\SystemAPI.h"
#include "Common/Settings.h"
#include "Common/FlexError.h"

#define VERSION_MJR		0
#define VERSION_MIN 	2
#define VERSION_REV 	0
#define VERSION_UPD 	0

class CTaskExplorer : public QMainWindow
{
	Q_OBJECT

public:
	CTaskExplorer(QWidget *parent = Q_NULLPTR);
	virtual ~CTaskExplorer();

	enum EColor {
		eNone = 0,
		eAdded,
		eToBeRemoved,
		eSystem,
		eUser,
		eService, // daemon
#ifdef WIN32
		eJob,
		ePico,
		eImmersive,
		eDotNet,
#endif
		eElevated,
#ifdef WIN32
		eGuiThread,
		eIsInherited,
		eIsProtected,
#endif
		eExecutable,
		eColorCount
	};
	static QColor	GetColor(int Color);

	static void		CheckErrors(QList<STATUS> Errors);

public slots:
	void				UpdateTray();


protected:
	void				timerEvent(QTimerEvent* pEvent);
	void				closeEvent(QCloseEvent *e);

	void				UpdateAll();

	//quint16				m_uTimerCounter;
	int					m_uTimerID;

private slots:
	void				OnInitDone();

	void				OnRun();
	void				OnRunAdmin();
	void				OnRunUser();
	void				OnRunAs();
	void				OnRunSys();
	void				OnElevate();

	void				OnExit();

	void				OnSysTab();
	void				OnKernelServices();
	void				OnTaskTab();

	void				OnPreferences();
	void				OnAutoRun();
	void				OnSkipUAC();

	void				OnCreateService();
	void				OnSCMPermissions();
	void				OnMonitorETW();

	void				OnSysTray(QSystemTrayIcon::ActivationReason Reason);

	void				OnAbout();

private:

	QWidget*			m_pMainWidget;
	QVBoxLayout*		m_pMainLayout;
	QSplitter*			m_pGraphSplitter;

	CGraphBar*			m_pGraphBar;

	QSplitter*			m_pMainSplitter;

	CProcessTree*		m_pProcessTree;

	QSplitter*			m_pPanelSplitter;

	CSystemInfoView*	m_pSystemInfo;

	CTaskInfoView*		m_pTaskInfo;

	QMenu*				m_pMenuProcess;

	QMenu*				m_pMenuView;
	QMenu*				m_pMenuSysTabs;
#ifdef WIN32
	QAction*			m_pMenuKernelServices;
#endif
	QMenu*				m_pMenuTaskTabs;

	QMap<QAction*, int> m_Act2Tab;

	QMenu*				m_pMenuOptions;
	QAction*			m_pMenuConf;
	QAction*			m_pMenuAutoRun;
#ifdef WIN32
	QAction*			m_pMenuUAC;
#endif

	QMenu*				m_pMenuTools;
	QMenu*				m_pMenuServices;
	QAction*			m_pMenuCreateService;
#ifdef WIN32
	QAction*			m_pMenuSCMPermissions;
	QAction*			m_pMenuETW;
#endif

	QMenu*				m_pMenuHelp;
	QAction*			m_pMenuAbout;
#ifdef WIN32
	QAction*			m_pMenuAboutPH;
#endif
	QAction*			m_pMenuAboutQt;

	QSystemTrayIcon*	m_pTrayIcon;
	QMenu*				m_pTrayMenu;

	bool				m_bExit;

	QImage				m_TrayGraph;
};

extern CSettings*	theConf;
extern CSystemAPI*	theAPI;

extern QIcon g_ExeIcon;
extern QIcon g_DllIcon;