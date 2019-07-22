#pragma once

#include <QtWidgets/QMainWindow>
#include "ProcessTree.h"
#include "SystemInfo/SystemInfoView.h"
#include "TaskInfo/TaskInfoView.h"
#include "API/SystemAPI.h"
#include "Common/Settings.h"
#include "Common/FlexError.h"

#define VERSION_MJR		0
#define VERSION_MIN 	5
#define VERSION_REV 	0
#define VERSION_UPD 	0

class CGraphBar;
class CHistoryGraph;
class CCustomItemDelegate;

class CTaskExplorer : public QMainWindow
{
	Q_OBJECT

public:
	CTaskExplorer(QWidget *parent = Q_NULLPTR);
	virtual ~CTaskExplorer();

	QStyledItemDelegate*	GetItemDelegate();
	int					GetCellHeight();

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
	static QColor		GetColor(int Color);

	static void			CheckErrors(QList<STATUS> Errors);

	static QString		GetVersion();

signals:
	void				ReloadAll();

public slots:
	void				UpdateAll();

	void				UpdateStatus();

	void				UpdateOptions();

protected:
	void				timerEvent(QTimerEvent* pEvent);
	void				closeEvent(QCloseEvent *e);

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

	void				OnSystemInfo();

	void				OnFindHandle();
	void				OnFindDll();
	void				OnFindMemory();

	void				OnSettings();
	void				OnAutoRun();
	void				OnSkipUAC();

	void				OnCreateService();
	void				OnReloadService();
	void				OnSCMPermissions();
	void				OnMonitorETW();

	void				OnSysTray(QSystemTrayIcon::ActivationReason Reason);

	void				OnAbout();

	void				OnGraphsResized(int Size);

	void				OnSplitterMoved();

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

	QLabel*				m_pStausCPU;
	QLabel*				m_pStausGPU;
	QLabel*				m_pStausMEM;
	QLabel*				m_pStausIO;
	QLabel*				m_pStausNET;

	QMenu*				m_pMenuProcess;

	QMenu*				m_pMenuView;
	QMenu*				m_pMenuSysTabs;
#ifdef WIN32
	QAction*			m_pMenuKernelServices;
#endif
	QMenu*				m_pMenuTaskTabs;
	QAction*			m_pMenuSystemInfo;

	QMap<QAction*, int> m_Act2Tab;

	QAction*			m_pMenuPauseRefresh;
	QAction*			m_pMenuRefreshNow;

	QMenu*				m_pMenuFind;
	QAction*			m_pMenuFindHandle;
	QAction*			m_pMenuFindDll;
	QAction*			m_pMenuFindMemory;

	QMenu*				m_pMenuOptions;
	QAction*			m_pMenuSettings;
	QAction*			m_pMenuAutoRun;
#ifdef WIN32
	QAction*			m_pMenuUAC;
#endif

	QMenu*				m_pMenuTools;
	QMenu*				m_pMenuServices;
	QAction*			m_pMenuCreateService;
	QAction*			m_pMenuUpdateServices;
#ifdef WIN32
	QAction*			m_pMenuSCMPermissions;
	QAction*			m_pMenuETW;
#endif

	QMenu*				m_pMenuHelp;
	QAction*			m_pMenuAbout;
	QAction*			m_pMenuSupport;
#ifdef WIN32
	QAction*			m_pMenuAboutPH;
#endif
	QAction*			m_pMenuAboutQt;

	QSystemTrayIcon*	m_pTrayIcon;
	QMenu*				m_pTrayMenu;

	bool				m_bExit;

	CHistoryGraph*		m_pTrayGraph;

	CCustomItemDelegate* m_pCustomItemDelegate;
};

extern CSettings*		theConf;
extern CSystemAPI*		theAPI;
extern CTaskExplorer*	theGUI;

extern QIcon g_ExeIcon;
extern QIcon g_DllIcon;