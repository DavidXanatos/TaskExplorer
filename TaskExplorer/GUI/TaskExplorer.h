#pragma once

#include <QtWidgets/QMainWindow>
#include "ProcessTree.h"
#include "SystemInfo/SystemInfoView.h"
#include "TaskInfo/TaskInfoView.h"
#include "API/SystemAPI.h"
#include "../MiscHelpers/Common/Settings.h"
#include "../MiscHelpers/Common/FlexError.h"

#define VERSION_MJR		1
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

	void SetDarkTheme(bool bDark);

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

		eGridColor,
		eBackground,

		eAdded,
		eDangerous,
		eToBeRemoved,
		eSystem,
		eUser,
		eService, // daemon
#ifdef WIN32
		eSandBoxed,
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

	struct SColor
	{
		SColor() : Enabled(false) {}
		SColor(const QString& name, const QString& description, const QString& default)
		{
			Name = name;
			Description = description;
			Default = default;
			Value = QColor(default);
			Enabled = false;
		}

		QString Name;
		QString Description;
		QString Default;
		QColor	Value;
		bool	Enabled;
	};

	QColor				GetColor(int Color);
	static QColor		GetListColor(int Color);
	static bool			UseListColor(int Color);
	void				InitColors();
	void				ReloadColors();
	QList<SColor>		GetAllColors() { return m_Colors.values(); }

	static int			GetGraphLimit(bool bLong = false);

	static void			CheckErrors(QList<STATUS> Errors);

	static QString		GetVersion();

signals:
	void				ReloadPanels();
	void				ReloadPlots();

public slots:
	void				UpdateAll();

	void				RefreshAll();

	void				UpdateStatus();

	void				UpdateOptions();

	void				UpdateUserMenu();

protected:
	void				timerEvent(QTimerEvent* pEvent);
	void				closeEvent(QCloseEvent *e);
	//quint16				m_uTimerCounter;
	int					m_uTimerID;
	quint64				m_LastTimer;

	QString				m_DefaultStyle;
	QPalette			m_DefaultPalett;

	QMap<EColor, SColor> m_Colors;

private slots:	
	void				OnMessage(const QString&);

	void				ApplyOptions();

	void				OnStatusMessage(const QString& Message);

	void				OnRun();
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
	void				OnChangePersistence(QAction* pAction);
	void				OnStaticPersistence();
	void				OnTreeButton();
	void				ResetAll();

	void				OnFindProcess();
	void				OnFindHandle();
	void				OnFindDll();
	void				OnFindMemory();

	void				OnSettings();
	void				OnDriverConf();
	void				OnAutoRun();
	void				OnSkipUAC();

	void				OnCreateService();
	void				OnReloadService();
	void				OnSCMPermissions();
	void				OnPersistenceOptions();
	void				OnSecurityExplorer();
	void				OnFreeMemory();
	void				OnMonitorETW();
	void				OnMonitorFW();
	void				OnMonitorDbg();

	void				OnViewFilter();

	void				OnSysTray(QSystemTrayIcon::ActivationReason Reason);

	void				OnAbout();

	void				OnGraphsResized(int Size);

	void				OnSplitterMoved();

private:
	void				LoadDefaultIcons();

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
	QAction*			m_pMenuResetAll;
	QAction*			m_pMenuShowTree;
	QAction*			m_pMenuExpandAll;

	QMenu*				m_pMenuFind;
	QAction*			m_pMenuFindProcess;
	QAction*			m_pMenuFindHandle;
	QAction*			m_pMenuFindDll;
	QAction*			m_pMenuFindMemory;

	QMenu*				m_pMenuOptions;
	QAction*			m_pMenuSettings;
#ifdef WIN32
	QAction*			m_pMenuDriverConf;
	QAction*			m_pMenuAutoRun;
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
#endif
	QAction*			m_pMenuPersistence;
	QAction*			m_pMenuFlushDns;
#ifdef WIN32
	QAction*			m_pMenuSecurityExplorer;
#endif

	QAction*			m_pMenuFilter;
	QMenu*				m_pMenuFilterMenu;
	QToolButton*		m_pMenuFilterButton;
	QAction*			m_pMenuFilterWindows;
	QAction*			m_pMenuFilterSystem;
	QAction*			m_pMenuFilterService;
	QAction*			m_pMenuFilterOther;
	QAction*			m_pMenuFilterOwn;


#ifdef WIN32
	QAction*			m_pMenuMonitorETW;
	QAction*			m_pMenuMonitorFW;
	QMenu*				m_pMenuMonitorDbgMenu;
	QToolButton*		m_pMenuMonitorDbgButton;
	QAction*			m_pMenuMonitorDbgLocal;
	QAction*			m_pMenuMonitorDbgGlobal;
	QAction*			m_pMenuMonitorDbgKernel;
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

	QToolButton*		m_pHoldButton;
	QActionGroup*		m_pHoldGroup;
	QAction*			m_pHoldAction;

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

	void				LoadLanguage();
	QTranslator			m_Translator;
	QByteArray			m_Translation;
};

extern CTaskExplorer*	theGUI;

extern QIcon g_ExeIcon;
extern QIcon g_DllIcon;
