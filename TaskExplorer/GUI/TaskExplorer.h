#pragma once

#include <QtWidgets/QMainWindow>
#include "ProcessTree.h"
#include "SystemInfo/SystemInfoView.h"
#include "TaskInfo/TaskInfoView.h"
#include "API/SystemAPI.h"
#include "Common/Settings.h"
#include "Common/FlexError.h"

#define VERSION_MJR		0
#define VERSION_MIN 	8
#define VERSION_REV 	5
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
	int						GetCellHeight();
	float					GetDpiScale();
	QVector<QColor>			GetPlotColors();

	enum EColor {
		eNone = 0,

		eGraphBack,
		eGraphFront,

		ePlotBack,
		ePlotFront,
		ePlotGrid,

		eAdded,
#ifdef WIN32
		eDange,
#endif
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
		eDriver,
		eGuiThread,
		eIsInherited,
		eIsProtected,
#endif
		eExecutable,
		eColorCount
	};
	static QColor		GetColor(int Color);
	static QColor		GetListColor(int Color);

	static int			GetGraphLimit(bool bLong = false);

	static void			CheckErrors(QList<STATUS> Errors);

	static QString		GetVersion();

signals:
	void				ReloadAll();

public slots:
	void				UpdateAll();

	void				UpdateStatus();

	void				UpdateOptions();

	void				UpdateUserMenu();

protected:
	void				timerEvent(QTimerEvent* pEvent);
	void				closeEvent(QCloseEvent *e);
	//quint16				m_uTimerCounter;
	int					m_uTimerID;
	quint64				m_LastTimer;

private slots:	
	void				OnStatusMessage(const QString& Message);

	void				OnRun();
	void				OnRunAdmin();
	void				OnRunUser();
	void				OnRunAs();
	void				OnRunSys();
	void				OnComputerAction();
	void				OnUserAction();
	void				OnElevate();
	void				OnExit();

	void				OnSysTab();
	//void				OnKernelServices();
	void				OnTaskTab();

	void				OnSystemInfo();

	void				OnChangeInterval(QAction* pAction);

	void				OnFindHandle();
	void				OnFindDll();
	void				OnFindMemory();

	void				OnSettings();
	void				OnAutoRun();
	void				OnSkipUAC();

	void				OnCreateService();
	void				OnReloadService();
	void				OnSCMPermissions();
	void				OnFreeMemory();
	void				OnMonitorETW();

	void				OnSysTray(QSystemTrayIcon::ActivationReason Reason);
	void				OnShowHide();

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


	QMenu*				m_pMenuProcess;
	QAction*			m_pMenuRun;
	QAction*			m_pMenuRunAsUser;
	QAction*			m_pMenuRunAsAdmin;
	QAction*			m_pMenuRunAs;
	QAction*			m_pMenuRunSys;
	QAction*			m_pMenuElevate;
	QAction*			m_pMenuExit;

	QMenu*				m_pMenuComputer;
	QAction*			m_pMenuLock;
	QAction*			m_pMenuLogOff;
	QAction*			m_pMenuSleep;
	QAction*			m_pMenuHibernate;
	QAction*			m_pMenuRestart;
	QAction*			m_pMenuForceRestart;
	QAction*			m_pMenuRestartEx;
	QAction*			m_pMenuShutdown;
	QAction*			m_pMenuForceShutdown;
	QAction*			m_pMenuHybridShutdown;

	QMenu*				m_pMenuUsers;
	QMap<int, QMenu*>	m_UserMenus;
	enum EUserCommands
	{
		eUserConnect,
		eUserDisconnect,
		eUserLogoff,
		/*eRemoteControll,
		eSendMessage,
		eUserInfo*/
	};

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
	QAction*			m_pMenuExpandAll;

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
	QMenu*				m_pMenuFree;
	QAction*			m_pMenuFreeWorkingSet;
	QAction*			m_pMenuFreeModPages;
	QAction*			m_pMenuFreeStandby;
	QAction*			m_pMenuFreePriority0;
	QAction*			m_pMenuCombinePages;
	QAction*			m_pMenuETW;
#endif

	QMenu*				m_pMenuHelp;
	QAction*			m_pMenuAbout;
	QAction*			m_pMenuSupport;
#ifdef WIN32
	QAction*			m_pMenuAboutPH;
#endif
	QAction*			m_pMenuAboutQt;

	QToolButton*		m_pRefreshButton;
	QActionGroup*		m_pRefreshGroup;
	QToolButton*		m_pFindButton;
	QToolButton*		m_pFreeButton;
	QToolButton*		m_pComputerButton;


	QSystemTrayIcon*	m_pTrayIcon;
	QMenu*				m_pTrayMenu;

	QToolBar*			m_pToolBar;

	QLabel*				m_pStausCPU;
	QLabel*				m_pStausGPU;
	QLabel*				m_pStausMEM;
	QLabel*				m_pStausIO;
	QLabel*				m_pStausNET;


	bool				m_bExit;

	CHistoryGraph*		m_pTrayGraph;

	CCustomItemDelegate* m_pCustomItemDelegate;
};

extern CTaskExplorer*	theGUI;

extern QIcon g_ExeIcon;
extern QIcon g_DllIcon;