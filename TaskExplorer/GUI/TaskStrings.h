#pragma once
#include "../API/ProcessInfo.h"
#include "../API/ThreadInfo.h"
#include "../API/ServiceInfo.h"
#include "../API/SocketInfo.h"
#include "../API/ModuleInfo.h"
#include "../API/HeapInfo.h"
#include "../API/MemoryInfo.h"
#include "../API/TokenInfo.h"
#include "../API/HandleInfo.h"
#include "../API/AccessRights.h"
#include "../API/DNSEntry.h"
#include "../API/JobInfo.h"
#include "../API/SystemAPI.h"
#include "../API/Monitors/GpuMonitor.h"
#include "../API/Monitors/DiskMonitor.h"
#include "../API/StackTrace.h"
#include "../API/WndInfo.h"
#ifdef WIN32
#include "../API/Windows/WinGDI.h"
#include "../API/Windows/SandboxieAPI.h"
#endif

//
// Values from the core, read out in the viewer's language.
//
// The core reports priorities as its platform numbers them and says nothing
// about what they mean - see the note on CAbstractTask. What a number means
// depends on the machine it came from, so every function here asks the task
// which target it belongs to and picks the reading accordingly.
//
// That is the point of doing it on this side: a Windows viewer showing a Linux
// machine must read nice values, and a Linux viewer showing a Windows machine
// must read priority classes. Both mappings are compiled everywhere.
//

//
// What is notable about a process. Windows reports a set of flags, Linux a
// single run state; the target decides which is read.
//
QString GetStatusString(const CProcessPtr& pProcess);

// The CAP_* names contained in a Linux capability mask.
QString GetCapabilityName(int Bit);
QStringList GetCapabilityNames(quint64 Mask);

//
// What to call one kind of handle in a filter that shows more than one machine's
// worth of them at once.
//
// A machine can classify handles in two ways at the same time - a Linux box
// running Wine has the kernel's descriptors and wineserver's objects - and both
// tables use the word File for entirely different things. The target reports
// which group a type is in; the word that says so is chosen here.
//
QString GetHandleTypeLabel(const CSystemAPI::SHandleType& Type);

//
// Services. A Windows service answers with numbers, a systemd unit with a name
// of its own, so the target decides which is read.
//
//
// Sockets. The protocol and, for TCP, the connection state.
//
// And what belongs in the address columns, which is not always an address: a
// unix socket is bound to a path or to nothing at all.
//
QString GetSocketAddressString(const CSocketPtr& pSocket, bool bRemote);
//
//
// Modules.
//
//
// Heaps.
//
//
// Memory regions.
//
QString GetProtectionString(const CMemoryPtr& pMemory);
QString GetAllocProtectionString(const CMemoryPtr& pMemory);
QString GetMemoryTypeString(const CMemoryPtr& pMemory);
QString GetMemoryUseString(const CMemoryPtr& pMemory);
QString GetRegionTypeExString(const CMemoryPtr& pMemory);
QString GetSigningLevelString(const CMemoryPtr& pMemory);

QString GetHeapTypeString(const CHeapPtr& pHeap);
QString GetHeapClassString(const CHeapPtr& pHeap);
QString GetHeapFlagsString(const CHeapPtr& pHeap);

QString GetMitigationsString(const CModulePtr& pModule);
QString GetModuleTypeString(const CModulePtr& pModule);
QString GetLoadReasonString(const CModulePtr& pModule);
QString GetImageMachineString(const CModulePtr& pModule);
QString GetEnclaveTypeString(const CModulePtr& pModule);

QString GetProtocolString(const CSocketPtr& pSocket);
QString GetSocketStateString(const CSocketPtr& pSocket);

QString GetServiceTypeString(const CServicePtr& pService);
QString GetServiceStateString(const CServicePtr& pService);
QString GetServiceStartTypeString(const CServicePtr& pService);
QString GetServiceErrorControlString(const CServicePtr& pService);

QString GetPriorityString(const CProcessPtr& pProcess);
QString GetPriorityString(const CThreadPtr& pThread);

QString GetBasePriorityString(const CProcessPtr& pProcess);
QString GetBasePriorityString(const CThreadPtr& pThread);

//
// How far a thread sits from its process's priority class. Windows reports it
// as a signed offset; Linux has no such notion and shows its policy instead.
//
QString GetBasePriorityIncrementString(const CThreadPtr& pThread);

QString GetPagePriorityString(const CProcessPtr& pProcess);
QString GetPagePriorityString(const CThreadPtr& pThread);

QString GetIOPriorityString(const CProcessPtr& pProcess);
QString GetIOPriorityString(const CThreadPtr& pThread);

//
// A Windows process priority class on its own - the job limits report one
// without a process to ask.
//
QString GetPriorityClassString(qint32 Value);

//
// Processes: what the image is, what shields it, and what its PEB says.
//
QString GetArchString(const CProcessPtr& pProcess);
QString GetSubsystemString(const CProcessPtr& pProcess);
QString GetOsContextString(const CProcessPtr& pProcess);
QString GetDPIAwarenessString(const CProcessPtr& pProcess);
QString GetWindowStatusString(const CProcessPtr& pProcess);
QString GetAccessMaskString(const CProcessPtr& pProcess);
QString GetTlsBitmapCountString(const CProcessPtr& pProcess);
QString GetErrorModeString(const CProcessPtr& pProcess);
QString GetMitigationsString(const CProcessPtr& pProcess);

QString GetPPLProtectionString(const CProcessPtr& pProcess);
QString GetKPHProtectionString(const CProcessPtr& pProcess);
QString GetProcessProtectionString(const CProcessPtr& pProcess);

//
// Tokens: the attribute masks read out, and the token's own standing.
//
QString GetPrivilegeAttributesString(quint32 Attributes);
QString GetGroupStatusString(quint32 Attributes, bool bRestricted = false);
QString GetGroupDescription(quint32 Attributes);
QString GetSecurityAttributeTypeString(quint16 Type);
QString GetSecurityAttributeFlagsString(quint32 Flags);

QString GetElevationString(const CTokenInfoPtr& pToken);
QString GetIntegrityString(const CTokenInfoPtr& pToken);
QString GetVirtualizationString(const CTokenInfoPtr& pToken);

//
// Handles: what one refers to, and what it is allowed to do with it.
//
QString GetHandleTypeString(const CHandlePtr& pHandle);
QString GetGrantedAccessString(const CHandlePtr& pHandle);
QString GetGenericAccessString(const CHandlePtr& pHandle);
QString GetHandleAttributesString(const CHandlePtr& pHandle);
QString GetFileShareAccessString(const CHandlePtr& pHandle);

QString GetSectionTypeString(quint32 Attributes);
QString GetAlpcPortFlagsString(quint32 Flags);

//
// A GDI object's type. Windows-only as a *concept*, but the wording is built
// here like every other, and a Linux viewer watching a Windows machine needs
// it - see CTaskInfoView::InitializeTabs, where the guard moved from build
// time to the target's operating system.
//
QString GetGdiTypeString(const CGdiPtr& pGDI);

QString GetGpuNodeString(const CGpuMonitor::SGpuNode& Node);

//
// Threads: the COM apartment, and the call one is sitting in.
//
QString GetApartmentTypeString(const CThreadPtr& pThread);
QString GetApartmentFlagsString(const CThreadPtr& pThread);
QString GetLastSysCallInfoString(const CThreadPtr& pThread);

QString GetOriginalPagesString(const CMemoryPtr& pMemory);

QString GetDnsTypeString(quint16 Type);
QString GetDnsTypeString(const CDnsCacheEntryPtr& pEntry);

#ifdef WIN32
QString GetSbieImageTypeString(quint32 Type);
QString GetSbieImageFlagsString(quint32 Flags);
#endif

QString GetFirewallStatusString(const CSocketPtr& pSocket);
QString GetNetworkUsageString(const CProcessPtr& pProcess);

QString GetVerifyResultString(const CModulePtr& pModule);
QString GetImageCoherencyString(const CModulePtr& pModule);

QString GetAffinityMaskString(const CAbstractTask* pTask);
QString GetStackUsageString(const CThreadPtr& pThread);
QString GetPoolTagString(quint32 Tag);

QString GetThreadStateString(const CThreadPtr& pThread);
QString GetTokenStateString(const CThreadPtr& pThread);
QString GetThreadTypeString(const CThreadPtr& pThread);
QString GetLastSysCallStatusString(const CThreadPtr& pThread);

//
// Codes carried inside the core's structures rather than returned from a
// getter - a group's SID kind, a token's type, a session's state, a job limit.
//
QString GetSidTypeString(quint8 Use);
QString GetTokenTypeString(quint8 Type);
QString GetImpersonationLevelString(qint8 Level);
QString GetAppContainerSidTypeString(quint8 Type);

QString GetSessionStateString(const CSystemAPI::SUser& User);
QString GetJobLimitName(int Which);

QStringList GetToolTipLines(const CProcessPtr& pProcess);
QPair<QString, QString> GetMitigationDetail(const CProcessInfo::SMitigationDetail& Detail);

QString GetEnvVarTypeString(int Type);
QString GetSeccompModeString(int Mode);

QString GetSecurityObjectType(const QString& Type);
QString GetSecurityObjectName(const CSecurityEditablePtr& pObject);
QString GetIdealProcessorString(const CThreadPtr& pThread);

QString GetSystemVersionString(CSystemAPI* pSystem);
QString GetSystemBuildString(CSystemAPI* pSystem);
QString GetDiskLabel(const CDiskMonitor::SDiskInfo& Disk);
QString GetStackSymbolString(const CStackTrace::SStackFrame& Frame);
QString GetStackFileInfoString(const CStackTrace::SStackFrame& Frame);
QString GetSecurityAttributeValue(quint16 Type, const QVariant& Value);

QString GetWndThreadString(const CWndInfo::SWndInfo& WndInfo);

//
// A name from the target, read out if it turned out to be a placeholder rather
// than a name. Cheap enough to call on every name displayed.
//
QString LocalizeName(const QString& Name);

QString GetFileAccessModeString(const CHandlePtr& pHandle, quint32 Mode);

//
// What an access right is called. The long name is for a dialog that has room
// for it; the short one for a column that does not.
//
QString GetAccessRightName(int Right, bool bShort = false);
QString GetAccessRightsString(const QList<int>& Rights);

//
// An icon the core reported as image-file bytes, ready to draw.
//
//
// Size is in logical pixels and is what the result will measure, whatever the
// bytes contained. 16 is a row in a list; the process page asks for 32 and the
// system panel for its logo.
//
QPixmap MakeIcon(const QByteArray& Bytes, int Size = 16);

//
// What to call the account a process runs as, given the key the target sent and
// the name it resolved.
//
// The well-known accounts are named here, in *this* machine's language, because
// the target's answer is in the target's: a German Windows resolves the system
// account to "NT-AUTORITAET\\SYSTEM" and an English one to "NT AUTHORITY\\SYSTEM",
// and a viewer that showed whichever arrived would relabel its own tree
// depending on which machine it happened to be watching. Everything else falls
// back to the resolved name, which is the best anyone can do for an account
// only the target knows about.
//
QString GetUserDisplayName(const QString& Key, const QString& Resolved);

//
// What a machine is called on screen.
//
// The name the user gave it when they connected, falling back to the host name
// the machine reports when they gave none - and the address in brackets after
// it, because otherwise nothing says whether this is the machine in front of
// you read directly or another one read through a server. Two boxes that both
// call themselves DESKTOP-XXXX can be told apart this way; by host name alone
// they cannot.
//
// One function, so the branch in the tree and the title bar cannot drift apart.
//
QString GetMachineDisplayName(class CSystemAPI* pSystem);

//
// Whether a machine is answering. Empty for one that is, so that in the ordinary
// case the column says nothing rather than saying "fine" on every row.
//
QString GetMachineStateString(class CSystemAPI* pSystem);
