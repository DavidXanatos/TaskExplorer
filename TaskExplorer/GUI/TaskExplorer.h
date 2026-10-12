#pragma once
#include "../API/SecurityInfo.h"

#include <QtWidgets/QMainWindow>
#include "ProcessTree.h"
#include "SystemInfo/SystemInfoView.h"
#include "TaskInfo/TaskInfoView.h"
#include "API/SystemAPI.h"
#include "../MiscHelpers/Common/Settings.h"
#include "../MiscHelpers/Common/Status.h"
#include "../MiscHelpers/Common/CustomTheme.h"
#include "../MiscHelpers/Common/ProgressDialog.h"
#include "../API/StatusEx.h"


class CGraphBar;
class CHistoryGraph;
class CCustomItemDelegate;
class COnlineUpdater;
class CCluster;

//
// Keeps one subscription pointed at the machine the panels are showing.
//
// A list view learns that new data has arrived from a signal on the system that
// collected it, and which system that is moves with the selection in the tree.
// Rather than have every such view remember what it is currently attached to,
// this does it: it connects on construction and reconnects whenever the view
// system changes. Parented to the receiver, so it lives exactly as long as the
// view that wanted it.
//
class CViewSystemLink : public QObject
{
	Q_OBJECT

public:
	CViewSystemLink(QObject* pReceiver, const char* pSignal, const char* pSlot);

private slots:
	void				Relink();

private:
	QObject*			m_pReceiver;
	QByteArray			m_Signal;
	QByteArray			m_Slot;
	QPointer<QObject>	m_pLinked;
};

class CTaskExplorer : public QMainWindow
{
	Q_OBJECT

public:
	CTaskExplorer(QWidget *parent = Q_NULLPTR);
	virtual ~CTaskExplorer();

	void SetUITheme();

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

		//
		// Not guarded, deliberately. Each of these is chosen by a predicate that
		// is portable and answers false where the notion does not apply - and
		// several do apply on both: .NET runs on Linux, a job is a cgroup there,
		// a driver is a kernel module. Guarding them would also have made
		// eColorCount differ between platforms, so a settings file could not
		// travel.
		//
		eSandBoxed,
		eJob,
		ePico,
		eImmersive,
		eDotNet,
		eElevated,
		eDriver,

		//
		// Running under Wine, which is neither a Windows process nor an ordinary
		// Linux one. Appended at the end of the list rather than beside the
		// others it belongs with: the enum is what a saved colour scheme is
		// indexed by, so inserting in the middle would silently shift every
		// colour a user has chosen.
		//
		eWine,
		eGuiThread,
		eIsInherited,
		eIsProtected,

		eExecutable,
		eColorCount
	};

	struct SColor
	{
		SColor() : Enabled(false) {}
		SColor(const QString& name, const QString& description, const QString& def_value)
		{
			Name = name;
			Description = description;
			Default = def_value;
			Value = QColor(def_value);
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

	CCustomTheme* GetTheme() { return &m_CustomTheme; }

	COnlineUpdater*		GetOnlineUpdater() { return m_pUpdater; }

	static int			GetGraphLimit(bool bLong = false);

	static bool			CheckErrors(QList<STATUS> Errors);

	//
	// Wording for the kernel-driver values the API reports as numbers and flags.
	//
	static QString		GetKernelLevelString(int Level);

	//
	// The short badge the title bar shows for a driver - [KTE++] and its lesser
	// forms. Empty when there is no driver to describe, so a caller can append
	// it without asking first.
	//
	// Here rather than beside the title bar, because the daemon panel shows the
	// same badge for the *server's* driver and two spellings of one convention
	// would drift.
	//
	static QString		GetKernelBadge(const struct CSystemAPI::SKernelDriver& Driver);
	static QStringList	GetKernelWeaknessStrings(quint32 Weaknesses);

	static STATUS		UpdateDynData(const QString& AppDir);

	// the platform layer's own credits dialog
	static void			ShowPlatformAbout(QWidget* parent);

	//
	// Open the permissions dialog on whatever the object hands back. A null
	// object means the target cannot offer one, which is said rather than
	// silently doing nothing.
	//
	static void			ShowSecurity(const CSecurityEditablePtr& pObject, QWidget* parent = NULL);

	//
	// Turn what the core reported into a sentence, in this viewer's language.
	// See GUI/TaskStrings.cpp.
	//
	static QString		FormatError(const STATUS& Error);

	QString				FormatID(quint64 ID) const;

	static QString		GetVersion();

	//
	// Handed out so that a machine branch can offer the very same power actions
	// the Tasks menu does. The menu itself, not a copy: CTaskExplorer::
	// OnComputerAction tells the actions apart by sender(), so a duplicate set
	// would land on none of the branches of that if.
	//
	//
	// ---- which machine a menu action applies to ----
	//
	// The menus that act on a *machine* - shut it down, end a session, start a
	// program - are built once and borrowed by whoever opens them. So the
	// machine cannot be baked into the actions; it is whatever the last opened
	// menu said, and every one of those menus says it on the way up.
	//
	// The alternative was a second set of menus per connected machine, which is
	// the same thing with more objects and one more place for the two copies to
	// disagree.
	//
	// Null means this computer, which is what an unqualified "the machine"
	// meant before any of this and still does.
	//
	CSystemAPI*			GetActionSystem() const;
	void				SetActionSystem(CSystemAPI* pSystem) { m_pActionSystem = pSystem; }

	QMenu*				GetComputerMenu(CSystemAPI* pSystem = NULL);
	QMenu*				GetUsersMenu(CSystemAPI* pSystem = NULL);

signals:
	void				ReloadPanels();
	void				ReloadPlots();

	//
	// The panels are now looking at a different machine. Relayed from CCluster,
	// which owns the state, because theGUI is the one object every view already
	// has and can connect to in its constructor - theCluster may not exist yet
	// when the views are built.
	//
	void				ViewSystemChanged();
	void				ActiveSystemChanged();

public slots:
	void				UpdateAll();

	void				RefreshAll();

	void				UpdateStatus();

	void				UpdateOptions();

	void				UpdateUserMenu();

	void				UpdateLabel();

	void				OpenTaskInfoWnd(quint64 PID);

protected:
	void				timerEvent(QTimerEvent* pEvent);
	void				closeEvent(QCloseEvent *e);
	//quint16				m_uTimerCounter;
	int					m_uTimerID;
	quint64				m_LastTimer;

	CCustomTheme		m_CustomTheme;
	double				m_DefaultFontSize;

	QMap<EColor, SColor> m_Colors;

private slots:	
	void				OnMessage(const QString&);

	void				ApplyOptions();

	void				OnStatusMessage(const QString& Message);

	//
	// The saved machines being connected at startup. See CCluster::ConnectingTo.
	//
	void				OnConnectingTo(const QString& Name, int Index, int Count);
	void				OnConnectRunFinished(int Connected, int Count, const QString& Name);

	void				OnRun();
	void				OnRunAs();
	void				OnRunSys();
	void				OnComputerAction();
	void				OnUserAction();
	void				OnTaskMenu();
#ifdef WIN32
	// Implemented in WndFinder.cpp, which drags the window under the cursor out
	// of the Win32 window manager. No portable equivalent yet.
	void				OnWndFinder();
#endif
#ifndef WIN32
	void				OnUseHelper();
#endif
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
	void				OnMultiUserButton();
	void				OnClusterModeButton();
	void				OnMachineBoxChanged(int Index);
	void				OnShowUnixSockets();
	void				OnConnect();
	void				OnSwitchMode();
	void				OnDisconnect();
	void				OnViewSystemChanged();
	void				OnActiveSystemChanged();
	void				OnNodeRemoved(const CSystemPtr& pSystem);

	//
	// Re-read the tab names into the View menu, for the tab whose name belongs
	// to the machine being looked at rather than to the program.
	//
	void				UpdateTabMenus();
	//
	// A tab's name for the View menu, which is not always the tab's own name.
	// See the definition.
	//
	QString				TabMenuLabel(class CTabPanel* pPanel, int Index);
	void				UpdateMachineLabels();
	void				UpdateMachineBox();
	void				UpdateTitle();
	void				UpdateTargetMenu();
	void				ResetAll();

	void				OnFindProcess();
	void				OnFindHandle();
	void				OnFindDll();
	void				OnFindMemory();

	void				OnSettings();
	void				OnDriverConf();
	//void				OnUseDriver();
	void				OnAutoRun();
	void				OnSkipUAC();

	void				OnCreateService();
	void				OnReloadService();
	void				OnSCMPermissions();
	void				OnPersistenceOptions();
	void				OnSecurityExplorer();
	void				OnFreeMemory();
	void				OnMonitorSys();
	void				OnMonitorETW();
	void				OnMonitorFW();
	void				OnMonitorDbg();

	void				OnViewFilter();

	void				OnSysTray(QSystemTrayIcon::ActivationReason Reason);

	void				OnAbout();
	void				OnHelp();
	void				OnCheckForUpdates();

	void				OnGraphsResized(int Size);

	void				OnSplitterMoved();

private:
	//
	// Creates theCluster if it is not there yet and wires it up. Cluster mode
	// and a connection both need it, and both used to build it themselves.
	//
	CCluster*			EnsureCluster();

	//
	// Look for a daemon on this machine and, unless told otherwise, read the
	// machine through it. See the implementation for why "unless told
	// otherwise" is a command-line switch and not a setting.
	//
	void				TryLocalDaemon();

	//
	// Whether this run was told to collect for itself, and whether there was a
	// daemon to decline. Two questions, because self-contained with no daemon on
	// the machine is not a choice anybody made and the window says nothing about
	// it.
	//
	bool				m_bSelfContained;
	bool				m_bDaemonPresent;

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
	//
	// The machine the machine-level menus act on; null for this computer.
	// A raw pointer, checked against the cluster before it is used - see
	// GetActionSystem, which is what makes a stale one harmless.
	//
	CSystemAPI*			m_pActionSystem = NULL;

	QAction*			m_pMenuRun;
	QAction*			m_pMenuRunAsUser;
	QAction*			m_pMenuRunAsAdmin;
	QAction*			m_pMenuRunAs;
	QAction*			m_pMenuRunSys;
	QAction*			m_pMenuFindWnd;
	QAction*			m_pMenuElevate;
#ifndef WIN32
	QAction*			m_pMenuUseHelper;
#endif
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
	//
	// Keyed by CSystemAPI::SUser::SessionKey - what the platform calls the
	// session - and not by the numeric id.
	//
	// It was the number, and on Linux the number is a lossy reading of a string:
	// logind calls a greeter "c1", toUInt says 0, and a second such session says
	// 0 as well. Two entries then shared a key, the second overwrote the first in
	// this map while both stayed in the menu, and the one that was overwritten
	// could never be found again to delete - so a duplicate appeared on every
	// refresh, once a second, for ever. See NEXT.md 5.47.
	//
	QMap<QString, QMenu*>	m_UserMenus;
	// the menu carries the API's own action enum, so there is nothing to map

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
	QAction*			m_pMenuMultiUser;
	QAction*			m_pMenuClusterMode;
	QAction*			m_pMenuShowUnixSockets;
	QAction*			m_pMenuConnect;
	QAction*			m_pMenuDisconnect;

	//
	// "Restart self-contained" or "Restart using the local daemon", depending
	// on which of the two this run is.
	//
	QAction*			m_pMenuSwitchMode;
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
	//QAction*			m_pMenuUseDriver;
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
	QAction*			m_pMenuMonitorSYS;
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
	QAction*			m_pMenuForum;
	QAction*			m_pMenuCheckUpdates;
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

	//
	// Which machine the window is about. Shown only while there is more than
	// one to choose between - see UpdateMachineBox.
	//
	class QComboBox*	m_pMachineBox;
	QAction*			m_pMachineBoxAction;

	//
	// Which machine the status line and the graph bar above it are reporting.
	// Shown only while more than one is in view - see OnViewSystemChanged.
	//
	QLabel*				m_pStausMachine;

	QLabel*				m_pStausCPU;
	QLabel*				m_pStausGPU;
	QLabel*				m_pStausMEM;
	QLabel*				m_pStausIO;
	QLabel*				m_pStausNET;

	QLabel*				m_pUpdateLabel;

	//
	// The separator and the padding that belong to that label, so they can be
	// taken out of the toolbar with it. Actions, not widgets - see where they
	// are added.
	//
	QList<QAction*>		m_UpdateLabelItems;

	bool				m_bExit;

	CHistoryGraph*		m_pTrayGraph;

	CCustomItemDelegate* m_pCustomItemDelegate;

	COnlineUpdater*		m_pUpdater;

	void				LoadLanguage();
	void				LoadLanguage(const QString& Lang, const QString& Module, int Index);
	QTranslator			m_Translator[2];
	QString				m_Language;
	//quint32				m_LanguageId;
};

extern CTaskExplorer*	theGUI;

extern QIcon g_ExeIcon;
extern QIcon g_DllIcon;
