#pragma once
#include "DotNetPerf.h"
#include "SecurityInfo.h"
#include "AssemblyList.h"
#include "../taskcore_global.h"
#include <qobject.h>

#include "ModuleInfo.h"
#include "ThreadInfo.h"
#include "HandleInfo.h"
#include "WndInfo.h"
#include "AbstractTask.h"
#include "MiscStats.h"
#include "MemoryInfo.h"
#include "HeapInfo.h"
#include "SocketInfo.h"
#include "DNSEntry.h"
#include "PersistentPreset.h"
#include "TokenInfo.h"
#include "JobInfo.h"
#include "CGroupInfo.h"
#include "ProcessSecurity.h"
#include "WineInfo.h"
#include "GdiInfo.h"

#ifdef WIN32
#undef GetUserName
#endif

struct SProcessUID
{
	SProcessUID() {}
	SProcessUID(quint64 uPid, quint64 msTime);
	__inline quint64 Get() const { return PUID; }
	__inline void Set(quint64 UID) { PUID = UID; }
	quint64 PUID = 0;

	bool operator==(const SProcessUID& other) const { return PUID == other.PUID; }
	bool operator!=(const SProcessUID& other) const { return PUID != other.PUID; }
	bool operator<(const SProcessUID& other) const { return PUID < other.PUID; }
	bool operator<=(const SProcessUID& other) const { return PUID <= other.PUID; }
	bool operator>(const SProcessUID& other) const { return PUID > other.PUID; }
	bool operator>=(const SProcessUID& other) const { return PUID >= other.PUID; }
};

struct STaskStatsEx : STaskStats
{
	SDelta32_64 	PageFaultsDelta;
	SDelta32_64 	HardFaultsDelta;
	SDelta64 		PrivateBytesDelta;
};

struct SGpuStats
{
	STimeUsage	GpuTimeUsage;
	quint64		GpuDedicatedUsage;
	quint64		GpuSharedUsage;
	QString		GpuAdapter;
};

class TASKCORE_EXPORT CProcessInfo: public CAbstractTask
{
	Q_OBJECT

	TRACK_OBJECT(CProcessInfo)
public:
	CProcessInfo(QObject *parent = nullptr);
	virtual ~CProcessInfo();

	// Basic
	virtual quint64 GetProcessId() const				{ QReadLocker Locker(&m_Mutex); return m_ProcessId; }
	virtual quint64 GetParentId() const					{ QReadLocker Locker(&m_Mutex); return m_ParentProcessId; }
	virtual void SetParentUId(SProcessUID PID) 			{ QWriteLocker Locker(&m_Mutex); m_ParentProcessUId = PID; }
	virtual SProcessUID GetProcessUId() const			{ QReadLocker Locker(&m_Mutex); return m_ProcessUId; }
	virtual SProcessUID GetParentUId() const			{ QReadLocker Locker(&m_Mutex); return m_ParentProcessUId; }
	virtual QString GetName() const						{ QReadLocker Locker(&m_Mutex); return m_ProcessName; }

	virtual bool ValidateParent(CProcessInfo* pParent) const = 0;

	//
	// The machine an image was built for - the same numbering CModuleInfo uses,
	// since it is the same field in the same header. A binary that carries both
	// an ARM64 and an x64 view of itself is flagged separately; the machine
	// alone cannot say so.
	//
	virtual quint16 GetArchitecture() const				{ return 0; }
	virtual bool    IsArm64X() const					{ return false; }
	virtual quint64 GetSessionID() const = 0;

	//
	// PE subsystem values, as they appear in an image header. Named here so the
	// process view can special-case a GUI or console image without a Windows
	// header; other formats report 0.
	//
	enum ESubsystem
	{
		eSubsystemUnknown	= 0,
		eSubsystemNative	= 1,
		eSubsystemWindowsGui= 2,
		eSubsystemWindowsCui= 3,
		eSubsystemOs2Cui	= 5,
		eSubsystemPosixCui	= 7,
	};
	virtual quint16 GetSubsystem() const = 0;

	// Parameters
	virtual QString GetFileName() const					{ QReadLocker Locker(&m_Mutex); return m_FileName; }
	// The path in the kernel's own namespace, where the platform has one.
	virtual QString GetFileNameNt() const				{ return GetFileName(); }
	virtual QString GetCommandLineStr() const			{ QReadLocker Locker(&m_Mutex); return m_CommandLine; }
	virtual QString GetWorkingDirectory() const = 0;

	// Other fields
	virtual QString GetUserName() const					{ QReadLocker Locker(&m_Mutex); return m_UserName; }

	//
	// A stable key for the account this process runs as - the SID in string
	// form on Windows, the numeric uid on Linux.
	//
	// GetUserName() is the *resolved* form and cannot be grouped by: it is
	// localised, so on a German Windows the same account reads
	// "NT-AUTORITAET\\SYSTEM", and two viewers watching one machine in different
	// languages would build different branches from it. Grouping the process
	// tree by user is the reason this exists; see CProcessModel.
	//
	// Empty where the collector could not read it, which is a real answer -
	// a process whose token cannot be opened has no account this side can name.
	//
	virtual QString GetUserKey() const					{ QReadLocker Locker(&m_Mutex); return m_UserKey; }

	// Dynamic
	virtual quint32 GetNumberOfThreads() const			{ QReadLocker Locker(&m_Mutex); return m_NumberOfThreads; }
	virtual quint32 GetNumberOfHandles() const			{ QReadLocker Locker(&m_Mutex); return m_NumberOfHandles; }

	virtual quint32 GetPeakNumberOfThreads() const		{ QReadLocker Locker(&m_Mutex); return m_PeakNumberOfThreads; }
	virtual quint32 GetPeakNumberOfHandles() const = 0;

	virtual quint64 GetPeakPrivateBytes() const			{ QReadLocker Locker(&m_Mutex); return m_PeakPagefileUsage; }
	virtual quint64 GetWorkingSetSize() const			{ QReadLocker Locker(&m_Mutex); return m_WorkingSetSize; }
	virtual quint64 GetPeakWorkingSetSize() const		{ QReadLocker Locker(&m_Mutex); return m_PeakWorkingSetSize; }
	virtual quint64 GetPrivateWorkingSetSize() const	{ QReadLocker Locker(&m_Mutex); return m_WorkingSetPrivateSize; }
	virtual quint64 GetVirtualSize() const				{ QReadLocker Locker(&m_Mutex); return m_VirtualSize; }
	virtual quint64 GetPeakVirtualSize() const			{ QReadLocker Locker(&m_Mutex); return m_PeakVirtualSize; }
	//quint32 GetPageFaultCount() const					{ QReadLocker Locker(&m_Mutex); return m_PageFaultCount; }

	virtual quint64 GetSharedWorkingSetSize() const = 0;
	virtual quint64 GetShareableWorkingSetSize() const = 0;
	virtual quint64 GetMinimumWS() const = 0;
	virtual quint64 GetMaximumWS() const = 0;

	virtual STaskStatsEx GetCpuStats() const			{ QReadLocker Locker(&m_StatsMutex); return m_CpuStats; }
	virtual STaskStats GetCpuStats2() const				{ QReadLocker Locker(&m_StatsMutex); return m_CpuStats2; }
	virtual SGpuStats GetGpuStats() const				{ QReadLocker Locker(&m_StatsMutex); m_GpuUpdateCounter = 0; return m_GpuStats; }


	virtual bool HasDebugger() const = 0;
	virtual STATUS AttachDebugger() = 0;
	virtual STATUS DetachDebugger() = 0;

	virtual bool IsSystemProcess() const = 0;
	virtual bool IsServiceProcess() const = 0;
	virtual bool IsUserProcess() const = 0;
	virtual bool IsElevated() const = 0;
	virtual bool IsPowerThrottled() const = 0; 

	virtual void SetNetworkUsageFlag(quint64 uFlag)			{ QWriteLocker Locker(&m_StatsMutex); m_NetworkUsageFlags |= uFlag; }
	virtual int GetNetworkUsageFlags() const				{ QReadLocker Locker(&m_StatsMutex); return m_NetworkUsageFlags; }

	virtual void UpdateDns(const QString& HostName, const QList<QHostAddress>& Addresses);
	virtual QString GetHostName(const QHostAddress& Address);

	virtual void UpdatePresets();

	virtual SProcStats	GetStats() const					{ QReadLocker Locker(&m_StatsMutex); return m_Stats; }

	virtual CModulePtr GetModuleInfo() const				{ QReadLocker Locker(&m_Mutex); return m_pModuleInfo; }

	//
	// What to say about this process beyond its name.
	//
	// On Windows that is the image's own description, out of its version
	// resources - "Firefox" for firefox.exe - and the default here reads it
	// from the main module, which is where it has always come from.
	//
	// A platform where a process can rename itself answers differently: on
	// Linux the interesting thing about the fifth firefox is not that its image
	// is firefox, which the name column already says, but that it calls itself
	// "Web Content". See CLinuxProcess.
	//
	// A virtual on the process rather than a reach through the module, because
	// the module is the file and this is about the process - two processes of
	// one image can have different answers - and because the module list is
	// filled on demand, so on the machine that has not been asked for it there
	// is no module to reach through.
	//
	virtual QString GetDescription() const
	{
		CModulePtr pModule = GetModuleInfo();
		return pModule.isNull() ? QString() : pModule->GetFileInfo("Description");
	}

	// Threads
	virtual QMap<quint64, CThreadPtr>	GetThreadList() const	{ QReadLocker Locker(&m_ThreadMutex); return m_ThreadList; }

	// Handles
	virtual QMap<quint64, CHandlePtr>	GetHandleList() const	{ QReadLocker Locker(&m_HandleMutex); return m_HandleList; }
	
	// Modules
	virtual QMap<quint64, CModulePtr>	GetModuleList() const	{ QReadLocker Locker(&m_ModuleMutex); return m_ModuleList; }

	// Windows
	virtual QMap<quint64, CWndPtr>		GetWindowList() const	{ QReadLocker Locker(&m_WindowMutex); return m_WindowList; }

	//
	// One window of this process, by the handle it reports.
	//
	// Not the same as looking the handle up in the map above: what a window is
	// filed under is this collector's business and need not be the handle it
	// shows. A Linux process running under Wine holds two kinds at once and
	// keeps them apart by their key, while both report the handle their own
	// world knows them by - which is the one that crosses the wire and comes
	// back in an action.
	//
	virtual CWndPtr						GetWindow(quint64 hWnd) const
										{ QReadLocker Locker(&m_WindowMutex); return m_WindowList.value(hWnd); }
	virtual CWndPtr						GetWindowByHwnd(quint64 hwnd) const { QReadLocker Locker(&m_WindowMutex); return m_WindowList.value(hwnd); }

	// Sockets
	virtual void						AddSocket(const CSocketPtr& pSocket) { QWriteLocker Locker(&m_SocketMutex); m_SocketList.insert((quint64)pSocket.data(), pSocket.toWeakRef()); }
	virtual void						RemoveSocket(const CSocketPtr& pSocket) { QWriteLocker Locker(&m_SocketMutex); m_SocketList.remove((quint64)pSocket.data()); }
	//
	// For a collector that rebuilds this list rather than maintaining it. See
	// CRemoteSystem::ApplySocketDelta, which projects it out of the machine's
	// socket list once a round instead of filing into it as sockets arrive.
	//
	virtual void						ClearSockets()							{ QWriteLocker Locker(&m_SocketMutex); m_SocketList.clear(); }
	virtual QMap<quint64, CSocketRef>	GetSocketList() const	{ QReadLocker Locker(&m_SocketMutex); return m_SocketList; }

	// Debug Output
	virtual void						AddDebugMessage(const QString& Text, const QDateTime& TimeStamp);
	struct SDebugMessage
	{
		QDateTime TimeStamp;
		QString Text;
	};
	virtual QList<SDebugMessage>		GetDebugMessages(quint32* pDebugMessageCount = NULL) const;
	virtual quint32						GetDebugMessageCount() const {QReadLocker Locker(&m_DebugMutex);  return m_DebugMessageCount; }
	virtual void						ClearDebugMessages();

	struct SEnvVar
	{
		enum EType
		{
			eSystem,
			eUser,
			eProcess
		};
		SEnvVar() { Type = eProcess; }
		QString Name;
		QString Value;
		EType	Type;
		//
		// A stable key for the variable, used to match rows across refreshes.
		// Built from the type's number rather than its name, so it does not
		// change when the interface language does.
		//
		QString GetTypeName() const { return QString::number((int)Type) + "_" + Name; }
	};

	virtual QMap<QString, SEnvVar>	GetEnvVariables() const = 0;
	virtual STATUS					DeleteEnvVariable(const QString& Name) = 0;
	virtual STATUS					EditEnvVariable(const QString& Name, const QString& Value) = 0;

	virtual QMap<quint64, CMemoryPtr> GetMemoryMap() const = 0;
	virtual QMap<quint64, CHeapPtr> GetHeapList() const = 0;
	virtual STATUS FlushHeaps() = 0;

	virtual QList<CWndPtr> GetWindows() const = 0;
	virtual CWndPtr	GetMainWindow() const = 0;
	

	virtual STATUS LoadModule(const QString& Path) = 0;

	virtual void ClearPersistence();

	virtual CPersistentPresetPtr GetPresets() const;

	//
	// ---- platform surface ----
	//
	// Everything either platform can report about a process is declared here,
	// so the GUI never needs to know which one it is looking at. An
	// implementation that has no such concept does not override, and the
	// default below is what a caller sees.
	//
	// A default is not a placeholder for "unimplemented": it is the honest
	// answer for a system that has no such notion. Whether a column or tab
	// built on one of these is worth showing at all is decided separately, by
	// CSystemAPI::GetOsType() and HasCapability().
	//

	// The security context. Null where the platform has none to report, or
	// where this process's could not be opened - both already happen on
	// Windows, so callers null-check today.
	virtual CTokenInfoPtr GetToken() const			{ return CTokenInfoPtr(); }

	// The token this process was created with, before any restriction was
	// applied to it. Null where the platform has no such notion, or where it
	// could not be opened.
	virtual CTokenInfoPtr GetOriginalToken() const	{ return CTokenInfoPtr(); }

	// -- Windows: identity and lineage --
	virtual quint64 GetLXSSProcessId() const		{ return 0; }		// pid inside WSL
	virtual quint64 GetConsoleHostId() const		{ return 0; }
	virtual quint64 GetStartKey() const				{ return 0; }
	virtual quint64 GetProcessSequenceNumber() const{ return 0; }
	virtual QStringList GetServiceList() const		{ return QStringList(); }

	// -- Windows: classification --
	virtual bool IsWoW64() const					{ return false; }
	virtual bool IsSubsystemProcess() const			{ return false; }	// pico / WSL
	// Part of the operating system itself rather than something a user started.
	virtual bool IsWindowsProcess() const			{ return false; }
	virtual bool IsImmersiveProcess() const			{ return false; }	// packaged / UWP
	//
	// Priority values, as the platform numbers them. A preset stores these and
	// they go over the wire, so they carry the native values rather than a dense
	// range of their own; the labels belong to the combo box that shows them.
	//
	enum EProcessPriority
	{
		eProcessPriorityUnknown		= 0,
		eProcessPriorityIdle		= 1,
		eProcessPriorityNormal		= 2,
		eProcessPriorityHigh		= 3,
		eProcessPriorityRealTime	= 4,
		eProcessPriorityBelowNormal	= 5,
		eProcessPriorityAboveNormal	= 6,
	};

	enum EIoPriority
	{
		eIoPriorityVeryLow		= 0,
		eIoPriorityLow			= 1,
		eIoPriorityNormal		= 2,
		eIoPriorityHigh			= 3,
		eIoPriorityCritical		= 4,
	};

	enum EPagePriority
	{
		ePagePriorityLowest		= 0,
		ePagePriorityVeryLow	= 1,
		ePagePriorityLow		= 2,
		ePagePriorityMedium		= 3,
		ePagePriorityBelowNormal= 4,
		ePagePriorityNormal		= 5,
	};

	virtual bool IsNetProcess() const				{ return false; }	// .NET runtime present

	//
	// A worker that reports this process's loaded CLR assemblies, or null where
	// the target cannot walk them. The caller owns it and starts it.
	//
	virtual CAssemblyEnumerator* GetAssemblyEnumerator(QObject* parent = nullptr) const
													{ Q_UNUSED(parent); return nullptr; }

	//
	// The CLR performance counters as values - see DotNetPerf.h. Empty when
	// the process is not managed, or the block could not be read.
	//
	virtual SDotNetCounters GetDotNetPerfCounters() const	{ return SDotNetCounters(); }
	//
	// What is notable about this process right now, as a set of flags.
	//
	// Windows has many of these and shows them together; Linux has a single run
	// state instead, reported by GetRunState below. Neither is turned into words
	// here - see GUI/TaskStrings.cpp. Bit values are protocol: append, never
	// renumber.
	//
	enum EProcessStatus
	{
		eStatusHidden			= 0x00000001,
		eStatusTerminated		= 0x00000002,
		eStatusCritical			= 0x00000004,
		eStatusSandboxed		= 0x00000008,
		eStatusDebugged			= 0x00000010,
		eStatusSuspended		= 0x00000020,
		eStatusHandleFiltered	= 0x00000040,
		eStatusElevated			= 0x00000080,
		eStatusPico				= 0x00000100,
		eStatusCrossSession		= 0x00000200,
		eStatusFrozen			= 0x00000400,
		eStatusBackground		= 0x00000800,
		eStatusPackaged			= 0x00001000,
		eStatusSecure			= 0x00002000,
		eStatusImmersive		= 0x00004000,
		eStatusDotNet			= 0x00008000,
		eStatusPacked			= 0x00010000,
		eStatusWow64			= 0x00020000,
		eStatusInSignificantJob	= 0x00040000,
		eStatusReflected		= 0x00080000,
		eStatusSystemProcess	= 0x00100000,
		eStatusSecureSystem		= 0x00200000,
		eStatusInJob			= 0x00400000,
		eStatusService			= 0x00800000,
		eStatusSystem			= 0x01000000,
		eStatusOwned			= 0x02000000,

		//
		// Running under Wine, which is not a Windows notion at all - it is a
		// Linux process pretending to be a Windows one. It lives here rather
		// than in the detail block because the status *column* shows it, and a
		// column is drawn for every row while the detail is fetched only for the
		// process somebody selected.
		//
		eStatusWine				= 0x04000000,
	};
	virtual quint32 GetStatusFlags() const			{ return 0; }

	//
	// The run state, which is what Linux reports instead. The letters are the
	// ones /proc/[pid]/stat uses, kept as their own numbering.
	//
	enum EProcessState
	{
		eStateUnknown = 0,
		eStateRunning,
		eStateSleeping,
		eStateDiskSleep,
		eStateZombie,
		eStateStopped,
		eStateTracingStop,
		eStateIdle,
		eStateDead,
		eStateWaking,
		eStateParked,
	};
	virtual int GetRunState() const					{ return eStateUnknown; }

	virtual bool IsSandBoxed() const				{ return false; }	// Sandboxie
	virtual bool IsReflectedProcess() const			{ return false; }
	virtual bool IsCriticalProcess() const			{ return false; }
	virtual bool IsInJob() const					{ return false; }
	virtual bool TokenHasChanged() const			{ return false; }

	// -- Windows: pool quotas --
	virtual quint64 GetPagedPool() const			{ return 0; }
	virtual quint64 GetPeakPagedPool() const		{ return 0; }
	virtual quint64 GetNonPagedPool() const			{ return 0; }
	virtual quint64 GetPeakNonPagedPool() const		{ return 0; }
	virtual quint64 GetShareableCommitSize() const	{ return 0; }

	// -- Windows: user-mode object counts --
	virtual quint32 GetGdiHandles() const			{ return 0; }
	virtual quint32 GetUserHandles() const			{ return 0; }
	virtual quint32 GetWndHandles() const			{ return 0; }

	// -- Windows: protection and mitigation --
	//
	// The mandatory-integrity policy: which accesses from lower integrity are
	// blocked. Windows expresses it as three independent bits; a system with no
	// integrity model reports none set and refuses to change them.
	//
	enum EMandatoryPolicy
	{
		eNoWriteUp		= 0x01,
		eNoReadUp		= 0x02,
		eNoExecuteUp	= 0x04,
	};
	virtual quint32 GetMandatoryPolicy() const		{ return 0; }
	virtual STATUS  SetMandatoryPolicy(quint32 Policy) { Q_UNUSED(Policy); return ERR(TE_NotSupported); }


	//
	// Windows shields some processes from tampering, at one of two levels and on
	// behalf of one of a handful of signers. Both come out of a single byte the
	// kernel reports; when it reports none, the type is unknown rather than
	// none - an unprotected process and a process we could not ask about are
	// different things.
	//
	enum EProtectionType
	{
		eProtectionNone		= 0,	// PsProtectedTypeNone
		eProtectionLight	= 1,	// PsProtectedTypeProtectedLight
		eProtectionFull		= 2,	// PsProtectedTypeProtected
		eProtectionLegacy	= 0xFE,	// before 8.1: protected, with no type to report
		eProtectionUnknown	= 0xFF,
	};

	enum EProtectionSigner
	{
		eSignerNone			= 0,
		eSignerAuthenticode	= 1,
		eSignerCodeGen		= 2,
		eSignerAntimalware	= 3,
		eSignerLsa			= 4,
		eSignerWindows		= 5,
		eSignerWinTcb		= 6,
		eSignerWinSystem	= 7,
		eSignerStoreApp		= 8,
	};

	virtual quint8  GetProtection() const			{ return 0; }
	virtual quint8  GetProtectionType() const		{ return eProtectionUnknown; }
	virtual quint8  GetProtectionSigner() const		{ return eSignerNone; }

	//
	// What the kernel driver established about a process, as a level from none
	// to maximum. Which combination of the observations below adds up to which
	// level is the driver's own business and changes between its versions, so
	// the level is reported rather than left for a viewer to recompute.
	//
	enum EKphLevel
	{
		eKphNotVerified	= -1,
		eKphNone		= 0,
		eKphMinimum		= 1,
		eKphLow			= 2,
		eKphMedium		= 3,
		eKphHigh		= 4,
		eKphMaximum		= 5,
	};
	virtual qint8   GetKphLevel() const				{ return eKphNotVerified; }

	// The individual observations behind that level.
	enum EKphState
	{
		eKphSecurelyCreated				= 0x0001,
		eKphVerifiedProcess				= 0x0002,
		eKphProtectedProcess			= 0x0004,
		eKphNoUntrustedImages			= 0x0008,
		eKphHasFileObject				= 0x0010,
		eKphHasSectionObjectPointers	= 0x0020,
		eKphNoUserWritableReferences	= 0x0040,
		eKphNoFileTransaction			= 0x0080,
		eKphNotBeingDebugged			= 0x0100,
	};
	virtual quint32 GetKphState() const				{ return 0; }

	//
	// Exploit mitigations that are on for this process. DEP being permanent is
	// a separate fact from DEP being on, and the strict and audit variants of
	// CET and XFG likewise sit alongside the plain ones.
	//
	enum EMitigation
	{
		eMitigationAslr			= 0x0001,
		eMitigationDep			= 0x0002,
		eMitigationDepPermanent	= 0x0004,
		eMitigationCfg			= 0x0008,
		eMitigationXfg			= 0x0010,
		eMitigationXfgAudit		= 0x0020,
		eMitigationCet			= 0x0040,
		eMitigationCetStrict	= 0x0080,

		//
		// Linux. The same column answers the same question - what has this
		// process asked the kernel to enforce on it - with the two things a
		// Linux process either has or has not, both one line of
		// /proc/<pid>/status.
		//
		// Above the Windows bits rather than among them, because these values
		// cross the wire: a viewer of one platform reads the flags of the other
		// and must not find a bit meaning two things.
		//
		eMitigationSeccomp		= 0x0100,
		eMitigationSeccompStrict= 0x0200,
		eMitigationNoNewPrivs	= 0x0400,
	};
	virtual quint32 GetMitigationFlags() const		{ return 0; }

	//
	// One row of the mitigation details page.
	//
	// Most rows come from the platform's own policy descriptions - a table it
	// owns, in its own words, the same as the access-right names. A handful are
	// not in that table and are recognised here instead; those carry a code and
	// are worded by the viewer.
	//
	struct SMitigationDetail
	{
		enum EExtra
		{
			eExtraNone = 0,		// the platform described it; Name and Description are filled

			eExtraLoaderIntegrity,
			eExtraModuleTampering,
			eExtraIndirectBranchPrediction,
			eExtraDynamicCodeDowngrade,
			eExtraSpeculativeStoreBypass,
		};

		int Extra = eExtraNone;

		// Only when Extra is eExtraNone.
		QString Name;
		QString Description;
	};

	virtual QList<SMitigationDetail> GetMitigationDetails() const { return QList<SMitigationDetail>(); }

	// Address of the process environment block, or its 32-bit view under WoW64.
	virtual quint64 GetPebBaseAddress(bool bWow64 = false) const { Q_UNUSED(bWow64); return 0; }
	virtual quint32 GetAccessMask()					{ return 0; }
	virtual quint32 GetReferenceCount()				{ return 0; }

	// -- Windows: desktop integration --
	virtual QString GetWindowTitle() const			{ return QString(); }
	virtual QString GetUsedDesktop() const			{ return QString(); }

	enum EDpiAwareness
	{
		eDpiUnknown			= 0,
		eDpiUnaware			= 1,
		eDpiSystemAware		= 2,
		eDpiPerMonitorAware	= 3,
	};
	virtual quint32 GetDPIAwareness() const			{ return eDpiUnknown; }

	// -- Windows: packaging --
	virtual QString GetPackageName() const			{ return QString(); }
	virtual QString GetAppID() const				{ return QString(); }

	// -- Windows: job objects --
	virtual quint64 GetJobObjectID() const			{ return 0; }

	// The job or cgroup this process belongs to, null when it is in none.
	virtual CJobInfoPtr GetJob() const				{ return CJobInfoPtr(); }

	// -- Windows: process environment block details --
	virtual quint16 GetCodePage() const				{ return 0; }
	// Which failures a process has asked the system to handle silently.
	enum EErrorMode
	{
		eErrModeFailCriticalErrors		= 0x0001,	// SEM_FAILCRITICALERRORS
		eErrModeNoGpFaultErrorBox		= 0x0002,	// SEM_NOGPFAULTERRORBOX
		eErrModeNoAlignmentFaultExcept	= 0x0004,	// SEM_NOALIGNMENTFAULTEXCEPT
		eErrModeNoOpenFileErrorBox		= 0x8000,	// SEM_NOOPENFILEERRORBOX
	};
	virtual quint32 GetErrorMode() const			{ return 0; }

	//
	// Thread-local storage comes in two banks: a small one every process has,
	// and an expansion bank allocated on demand. A count above the first bank's
	// size means the expansion bank is in use, which is what makes the two
	// sizes worth naming rather than leaving in the reading.
	//
	enum ETlsSlots
	{
		eTlsMinimumAvailable	= 64,	// TLS_MINIMUM_AVAILABLE
		eTlsExpansionSlots		= 1024,	// TLS_EXPANSION_SLOTS
	};
	virtual quint16 GetTlsBitmapCount() const		{ return 0; }

	//
	// The Windows version a process was told it is running on, when it asked to
	// be lied to. The numbering is not Windows's own - it is a dense ordering
	// of releases, so that "at least 8.1" is a comparison rather than a table.
	//
	enum EOsContext
	{
		eOsContextNone		= 0,	// no context: the process sees the real version
		eOsContextXp		= 51,
		eOsContextVista		= 60,
		eOsContext7			= 61,
		eOsContext8			= 62,
		eOsContext81		= 63,
		eOsContext10		= 100,
	};
	virtual quint32 GetOsContextVersion() const		{ return eOsContextNone; }

	// -- state control --
	//
	// Freezing suspends a process and tells the system it is intentionally
	// idle, which is different from a plain suspend; Windows exposes it for
	// packaged apps. Systems without the notion report never frozen.
	//
	virtual bool IsFrozen() const					{ return false; }
	virtual STATUS Freeze()							{ return ERR(TE_NotSupported); }
	virtual STATUS UnFreeze()						{ return ERR(TE_NotSupported); }

	virtual STATUS SetPowerThrottled(bool Value)	{ Q_UNUSED(Value); return ERR(TE_NotSupported); }
	virtual STATUS SetCriticalProcess(bool bSet, bool bForce = false)
													{ Q_UNUSED(bSet); Q_UNUSED(bForce); return ERR(TE_NotSupported); }
	//virtual STATUS SetProtectionFlag(quint8 Flag, bool bForce = false)
	//												{ Q_UNUSED(Flag); Q_UNUSED(bForce); return ERR(TE_NotSupported); }
	virtual STATUS ReduceWS()						{ return ERR(TE_NotSupported); }

	// Opens the platform's own security editor for this process.
	//
	// The object's access control, for the security dialog. Null when the
	// target cannot offer one - which is what a viewer greys the button on.
	//
	virtual CSecurityEditablePtr GetSecurityObject() const	{ return CSecurityEditablePtr(); }

	//
	// Whether the process is holding the machine awake. Windows calls this
	// "execution required"; there is no single Linux equivalent, so the
	// default reports off and refuses to change it.
	//
	virtual bool IsExecutionRequired() const		{ return false; }
	virtual STATUS SetExecutionRequired(bool bSet)	{ Q_UNUSED(bSet); return ERR(TE_NotSupported); }

	//
	// Reveal the process's executable in the system's file manager. The
	// mechanism differs per platform - Explorer can select the file itself,
	// most Linux file managers can only be pointed at the folder - so the
	// backend decides how.
	//

	//
	// Working-set watch.
	//
	// Once enabled the kernel records the instruction pointer of every page
	// fault in the process; enabling is one-way, it runs until the process
	// exits. GetWsWatchFaults() drains what has accumulated since the previous
	// call, one entry per fault - the same address twice means it faulted
	// twice - and reports through bEnabled whether the watch is running at all,
	// which is how a caller learns it still has to be turned on.
	//
	virtual STATUS EnableWsWatch()					{ return ERR(TE_NotSupported); }
	virtual STATUS GetWsWatchFaults(QList<quint64>& Faults, bool& bEnabled)
													{ Q_UNUSED(Faults); bEnabled = false; return ERR(TE_NotSupported); }

	//
	// Extra descriptive lines for the process tree's tooltip - what a svchost
	// is hosting, what a rundll is running, which COM object a surrogate
	// serves. Decoding needs the machine's own registry and images, so the
	// backend produces the text and the view only displays it.
	//
	//
	// One block of what the process tree says about a process in its tooltip.
	//
	// Working out that a process is a svchost group, a rundll target or a COM
	// surrogate means reading its command line against the machine's own
	// registry and image files, so that stays with the collector. What comes
	// back is which kind of block it is and the values that fill it; the
	// headings and the layout are the viewer's.
	//
	struct SToolTipSection
	{
		enum EKind
		{
			eServiceGroup,		// Items: the group name
			eRunDllTarget,		// Items: (description, version), (company, -)
			eComTarget,			// Items: the class name, if any; then its GUID
			eComTargetFile,		// Items: (description, version), (company, -)
			eServices,			// Items: (service name, display name)
			eTasks,				// Items: (task name, path)
			eDrivers,			// Items: (driver name, path)
			eEdgeRole,			// Role, no items
			eWmiProviders,		// Items: (provider name, file)
		};

		//
		// Which part of Edge a process is. Recognised by the AppContainer SID it
		// runs under, which is a fixed list Microsoft ships.
		//
		enum EEdgeRole
		{
			eEdgeNone = 0,
			eEdgeManager,
			eEdgeBrowserExtensions,
			eEdgeUserInterfaceService,
			eEdgeChakraJitCompiler,
			eEdgeFlashPlayer,
			eEdgeBackgroundTabPool,
		};

		SToolTipSection(EKind kind = eServiceGroup) { Kind = kind; Role = eEdgeNone; }

		EKind Kind;
		int Role;

		// The second half is empty where a line carries only one value.
		QList<QPair<QString, QString>> Items;
	};

	virtual QList<SToolTipSection> GetToolTipSections() const { return QList<SToolTipSection>(); }

	// -- classification --
	virtual int GetKnownProcessType() const			{ return 0; }
	virtual QString GetSandBoxName() const			{ return QString(); }

	//
	// Things hosted inside a process rather than being one: scheduled tasks in
	// a svchost, user-mode drivers, WMI providers. Queried on demand for the
	// process tree's tooltip, so an implementation with nothing to report just
	// returns an empty list.
	//
	struct STask
	{
		QString Name;
		QString Path;
	};
	virtual QList<STask> GetTasks() const			{ return QList<STask>(); }

	struct SDriver
	{
		QString Name;
		QString Path;
	};
	virtual QList<SDriver> GetUmdfDrivers() const	{ return QList<SDriver>(); }

	struct SWmiProvider
	{
		QString ProviderName;
		QString NamespacePath;
		QString FileName;
		QString UserName;
	};
	virtual QList<SWmiProvider> QueryWmiProviders() const { return QList<SWmiProvider>(); }

	//
	// Graphics objects this process owns. Windows fills it from the GDI handle
	// table; a backend without one returns nothing and the view stays empty.
	//
	virtual QMap<quint64, CGdiPtr> GetGdiList() const	{ return QMap<quint64, CGdiPtr>(); }

	// -- lifetime and responsiveness --
	virtual quint64 GetUpTime() const				{ return 0; }
	virtual quint64 GetSuspendTime() const			{ return 0; }
	virtual int     GetHangCount() const			{ return 0; }
	virtual int     GetGhostCount() const			{ return 0; }

	// -- Linux: resource control and confinement --
	virtual QString GetCGroupPath() const			{ return QString(); }
	//
	// The accounting for that group, and how long it spent stalled waiting for
	// a resource ("cpu", "memory" or "io").
	//
	// Read by whoever is doing the collecting, because that is the only side
	// that has the files. The cgroup view used to call ProcFs itself, which
	// meant it read the machine the window was on rather than the machine the
	// process was on.
	//
	virtual SCGroupStats GetCGroupStats() const		{ return SCGroupStats(); }
	virtual SResourcePressure GetCGroupPressure(const QString& Resource) const
													{ Q_UNUSED(Resource); return SResourcePressure(); }
	virtual QString GetConfinement() const			{ return QString(); }	// AppArmor / SELinux
	virtual QString GetContainer() const			{ return QString(); }	// docker / snap / flatpak

	//
	// What a sandboxed process was granted, in the sandbox's own words -
	// "filesystem=home", "share=network", "x11". Values out of a manifest
	// rather than sentences of ours, so a permission this build has never heard
	// of still arrives intact and still means something to whoever reads it.
	//
	// Empty for anything not in a flatpak or a snap, which is most things.
	//
	virtual QStringList GetSandboxPermissions() const { return QStringList(); }

	//
	// The numeric owner. GetUserName() is the resolved form and is what a view
	// should normally show; these are for the places that want the raw id.
	//
	//
	// Wine, where the process is running under it.
	//
	// Not an operating system of its own and not a property of the machine: two
	// processes on the same Linux box can differ, one being an ELF and the next
	// a PE under Wine. So it is asked of the process, and a view that offers
	// Windows detail for it checks SWineInfo::Valid rather than GetOsType.
	//
	virtual SWineInfo GetWineInfo() const			{ return SWineInfo(); }

	virtual quint32 GetUid() const					{ return 0; }
	virtual quint32 GetGid() const					{ return 0; }

	//
	// Capability sets, seccomp and the LSM label; and which namespaces this
	// process is in. Read by the collector - the security view used to call
	// ProcFs itself, which asked the wrong machine's /proc about a pid that may
	// not be on it.
	//
	// To decide whether a namespace is the host's, compare against
	// CSystemAPI::GetHostNamespaces() rather than reading pid 1 locally.
	//
	virtual SProcessSecurity GetProcessSecurity() const	{ return SProcessSecurity(); }
	virtual SProcessNamespaces GetNamespaces() const	{ return SProcessNamespaces(); }
	virtual int     GetOomScore() const				{ return 0; }
	virtual int     GetOomScoreAdj() const			{ return 0; }
	virtual quint64 GetInotifyWatches() const		{ return 0; }

signals:
	void			ThreadsUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void			HandlesUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void			ModulesUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void			WindowsUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);

public slots:
	//
	// Fetch what the list does not carry - see API_CMD_PROCDETAIL.
	//
	// Nothing for a local collector, which fills every field during its own
	// enumeration and has nowhere to fetch from. A view that wants the details
	// calls this and reads the getters afterwards, and gets the same answer
	// either way.
	//
	//
	// Whether the target sent only this process's outline.
	//
	// False everywhere a viewer collects for itself - it sees what it can see,
	// and what it cannot it simply does not have. It is only a remote answer
	// that can be *deliberately* partial, and a view wants to tell that apart
	// from a machine that failed to answer. See API_PROC_REDACTED.
	//
	virtual bool	IsRedacted() const { return false; }

	virtual bool	UpdateDetails() { return true; }

	virtual bool	UpdateThreads() = 0;
	virtual bool	UpdateHandles() = 0;
	virtual bool	UpdateModules() = 0;
	virtual bool	UpdateWindows() = 0;

	virtual void	ApplyPresets();

protected:
	void InitPresets(); // *NOT Thread Safe* internal function

	// Basic
	quint64							m_ProcessId;
	quint64							m_ParentProcessId;
	SProcessUID						m_ProcessUId;
	SProcessUID						m_ParentProcessUId;
	QString							m_ProcessName;

	// Parameters
	QString							m_FileName;
	QString							m_CommandLine;
	//QString							m_WorkingDirectory;
	
	// Other fields
	QString							m_UserName;
	QString							m_UserKey;

	// Dynamic
	quint32							m_NumberOfThreads;
	quint32							m_NumberOfHandles;

	quint32							m_PeakNumberOfThreads;

	quint64							m_PeakPagefileUsage;
	quint64							m_WorkingSetSize;
	quint64							m_PeakWorkingSetSize;
	quint64							m_WorkingSetPrivateSize;
	quint64							m_VirtualSize;
	quint64							m_PeakVirtualSize;
	//quint32							m_PageFaultCount;

	quint32							m_NetworkUsageFlags;

	// I/O stats
	mutable QReadWriteLock			m_StatsMutex;
	SProcStats						m_Stats;
	STaskStatsEx					m_CpuStats;
	STaskStats						m_CpuStats2;
	SGpuStats						m_GpuStats;
	volatile mutable quint32		m_GpuUpdateCounter;


	// module info
	CModulePtr						m_pModuleInfo;

	// Threads
	mutable QReadWriteLock			m_ThreadMutex;
	QMap<quint64, CThreadPtr>		m_ThreadList;

	// Handles
	mutable QReadWriteLock			m_HandleMutex;
	QMap<quint64, CHandlePtr>		m_HandleList;

	// Modules
	mutable QReadWriteLock			m_ModuleMutex;
	QMap<quint64, CModulePtr>		m_ModuleList;

	// Window
	mutable QReadWriteLock			m_WindowMutex;
	QMap<quint64, CWndPtr>			m_WindowList;

	// Sockets
	mutable QReadWriteLock			m_SocketMutex;
	QMap<quint64, CSocketRef>		m_SocketList;

	// Dns Log
	mutable QReadWriteLock			m_DnsMutex;
	QMap<QString, CDnsLogEntryPtr>		m_DnsLog;
	QMultiMap<QHostAddress, QString>	m_DnsRevLog;

	// Debug Messages
	mutable QReadWriteLock			m_DebugMutex;
	QList<SDebugMessage>			m_DebugMessages;
	quint32							m_DebugMessageCount;
	
	CPersistentPresetRef			m_PersistentPreset;
};

typedef QSharedPointer<CProcessInfo> CProcessPtr;
typedef QWeakPointer<CProcessInfo> CProcessRef;
