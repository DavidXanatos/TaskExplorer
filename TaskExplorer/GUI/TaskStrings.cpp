#include "stdafx.h"
#include "TaskExplorer.h"
#include "TaskStrings.h"
#include "../API/SystemAPI.h"
#include "../API/Cluster.h"

//
// Why the tables here say QT_TRANSLATE_NOOP rather than QT_TR_NOOP.
//
// Almost everything in this file is a free function, and QT_TR_NOOP marks a
// string for translation *in the class it is written in*. Written outside one
// there is no class, so lupdate cannot tell what context the string belongs to
// - it says "tr() cannot be called without context" and then does not record
// the string at all. 325 of them were being dropped that way: every access
// right name, every open flag, every group attribute, present in the source
// and absent from every .ts file, so no translator ever saw one.
//
// QT_TRANSLATE_NOOP names the context outright, and the name given is the one
// the string is looked up under at runtime - these tables are read back with
// CTaskExplorer::tr(), so "CTaskExplorer" is what it has to be. Marker and
// reader have to agree; a mismatch is silent, and shows up only as a string
// that stays English however carefully it was translated.
//

//
// The sentences.
//
// This is the other half of API/TaskStatus.h: the core names a failure and
// hands over the values, and this turns the pair into something to read - in
// the language of whoever is reading, not of the machine that failed.
//
// A remote target therefore reports its troubles in the viewer's locale, and
// a translator only ever has to look here.
//
QString CTaskExplorer::FormatError(const STATUS& Error)
{
	if (!Error.IsError())
		return QString();

	//
	// A couple of messages show a value in a particular way. That is a
	// presentation choice, so it is made here rather than by whoever reported
	// it - the core passes the number.
	//
	switch (Error.GetMsgCode())
	{
	case TE_LoadKernelDriver:
		return tr("Unable to load the kernel driver, Error: 0x%1").arg(Error.GetArgs().value(0).toUInt(), 8, 16, QChar('0'));
	case TE_UnmapSectionView:
		return tr("Unable to unmap the section view at 0x%1").arg(Error.GetArgs().value(0).toULongLong(), 0, 16);

	case TE_DumpProcessingThread:
		return tr("Processing thread 0x%1...").arg(Error.GetArgs().value(0).toULongLong(), 0, 16);

	//
	// Byte counts are shown the way every other size in the program is.
	//
	case TE_DumpWritingMemory:
		return tr("Writing memory (%1 of %2)...")
			.arg(FormatSize(Error.GetArgs().value(0).toULongLong()))
			.arg(FormatSize(Error.GetArgs().value(1).toULongLong()));

	//
	// A finished core dump says how big it is, and what it is missing. Not
	// having stopped the threads is the more serious of the two, so it is the
	// one reported when both apply.
	//
	case TE_CoreDumpCompleted:
	{
		QString Message = tr("Core dump completed: %1").arg(FormatSize(Error.GetArgs().value(0).toULongLong()));

		const quint64 Unreadable = Error.GetArgs().value(2).toULongLong();
		if (!Error.GetArgs().value(1).toBool())
			Message += tr(" - without register state, as the threads could not be stopped.");
		else if (Unreadable)
			Message += tr(" - %1 could not be read and was written as zeros.").arg(FormatSize(Unreadable));

		return Message;
	}

	default: break;
	}

	QString Message;
	switch (Error.GetMsgCode())
	{
	case TE_Generic:
	case MH_Native:
		//
		// Nothing here can improve on what the platform said, so it is quoted
		// with the code beside it.
		//
		// Two codes, one rendering: TE_Generic is raised by the core when it has
		// nothing better to say, MH_Native by CStatus::Native() below it. Both
		// carry the platform's own wording as their first argument.
		//
		return tr("Error 0x%1: %2").arg((quint32)Error.GetStatus(), 8, 16, QChar('0'))
			.arg(Error.GetArgs().isEmpty() || Error.GetArgs().first().toString().isEmpty()
				? tr("no further information") : Error.GetArgs().first().toString().trimmed());

	case TE_NotSupported:	Message = tr("This is not supported here."); break;
	case TE_Message:		Message = "%1"; break;

	case TE_NoFileName:                     Message = tr("No file name is available for this object."); break;
	case TE_NoProcSelected:                 Message = tr("No process selected."); break;
	case TE_CreatingCoreDump:               Message = tr("Creating a core dump is not supported on this architecture."); break;
	case TE_CannotDumpSelf:                 Message = tr("TaskExplorer cannot dump its own process."); break;
	case TE_ProcExited:                     Message = tr("The process has exited."); break;
	case TE_KernelThreadNoAddrSpace:        Message = tr("Kernel threads have no user address space to dump."); break;
	case TE_Only64BitCoreDump:              Message = tr("This is a 32-bit process; only 64-bit core dumps are supported."); break;
	case TE_CannotWrite:                    Message = tr("Cannot write to %1: %2"); break;
	case TE_ClosingFileDesc:                Message = tr("Closing a file descriptor of another process is not supported on Linux."); break;
	case TE_NoTerminalEmulator:             Message = tr("No terminal emulator could be started. Looked for: %1."); break;
	case TE_RestartElevatedWayland:         Message = tr("Cannot restart elevated under a Wayland session: a compositor does not accept connections from a process running as another user. Run TaskExplorer from a terminal with 'sudo' instead."); break;
	case TE_RestartElevatedNoDisplay:       Message = tr("Cannot restart elevated: no X display is available."); break;
	case TE_RestartElevatedStarted:         Message = tr("Cannot restart elevated: %1 could not be started."); break;
	case TE_RestartElevatedNoGraphical:     Message = tr("Cannot restart elevated: no graphical privilege escalation helper was found. Install polkit (for pkexec), or run TaskExplorer from a terminal with 'sudo'."); break;
	case TE_ChangingMemoryProtection:       Message = tr("Changing memory protection of another process is not supported on Linux."); break;
	case TE_NoOutputFile:                   Message = tr("No output file."); break;
	case TE_OpenProcMemory:                 Message = tr("Failed to open process memory. This usually means ptrace access was denied; see /proc/sys/kernel/yama/ptrace_scope."); break;
	case TE_WriteDumpFile:                  Message = tr("Failed to write the dump file."); break;
	case TE_ReadProcMemory:                 Message = tr("Failed to read process memory."); break;
	case TE_FreeingMemoryProc:              Message = tr("Freeing memory of another process is not supported on Linux."); break;
	case TE_UnloadingModuleRunning:         Message = tr("Unloading a module from a running process is not supported on Linux."); break;
	case TE_CannotDebugSelf:                Message = tr("TaskExplorer cannot debug itself."); break;
	case TE_ProcAlreadyTraced:              Message = tr("This process is already being traced."); break;
	case TE_NoDebuggerInstalled:            Message = tr("No debugger is installed. Install gdb or lldb."); break;
	case TE_ProcTraced:                     Message = tr("This process is not being traced."); break;
	case TE_DebuggerItselfDetach:           Message = tr("Only the debugger itself can detach. This process is being traced by %1 (pid %2); quit it there, or terminate it."); break;
	case TE_PriorityBoostUnsupported:       Message = tr("Priority boost is not supported on Linux."); break;
	case TE_SettingSchedulingPolicy:        Message = tr("Setting the scheduling policy is not supported; use chrt(1)."); break;
	case TE_PagePriorityUnsupported:        Message = tr("Page priority is not supported on Linux."); break;
	case TE_OomAdjustmentRange:             Message = tr("The OOM adjustment must be between -1000 and 1000."); break;
	case TE_LoweringOomAdjustment:          Message = tr("Lowering the OOM adjustment requires root (CAP_SYS_RESOURCE)."); break;
	case TE_AffinityMaskEmpty:              Message = tr("The affinity mask must select at least one CPU."); break;
	case TE_EditingEnvironmentRunning:      Message = tr("Editing the environment of a running process is not supported on Linux."); break;
	case TE_FlushingHeapsUnsupported:       Message = tr("Flushing heaps is not supported on Linux."); break;
	case TE_ExecutablePathProc:             Message = tr("The executable path of this process is not readable."); break;
	case TE_LoadingModuleRunning:           Message = tr("Loading a module into a running process is not yet implemented on Linux."); break;
	case TE_AuthorisedNoInteractive:        Message = tr("Not authorised to %1 %2, and no interactive authentication agent is available. Run TaskExplorer from a desktop session with a polkit agent, or as root."); break;
	case TE_NotAuthorised:                  Message = tr("Not authorised to %1 %2."); break;
	case TE_UnitNoLonger:                   Message = tr("The unit %1 no longer exists."); break;
	case TE_UnitMaskedStarted:              Message = tr("The unit %1 is masked and cannot be started."); break;
	case TE_OperationUnsupportedRunning:    Message = tr("This operation is not supported by the running version of systemd."); break;
	case TE_TimedWaitingSystemd:            Message = tr("Timed out waiting for systemd to %1 %2."); break;
	case TE_UnitActionFailed:               Message = tr("Failed to %1 %2: %3"); break;
	case TE_ConnectSystemBus:               Message = tr("Cannot connect to the system bus."); break;
	case TE_JournalctlFoundSystem:          Message = tr("journalctl was not found; this system does not use the systemd journal."); break;
	case TE_DeletingSystemdUnit:            Message = tr("Deleting a systemd unit is not supported: the unit file usually belongs to a distribution package. Use 'systemctl disable %1' to stop it starting at boot, or 'systemctl mask %1' to prevent it from being started at all."); break;
	case TE_ClosingSocketYet:               Message = tr("Closing a socket is not yet implemented on Linux."); break;
	case TE_TerminatingIndividualThread:    Message = tr("Terminating an individual thread is not supported on Linux."); break;
	case TE_SuspendingIndividualThread:     Message = tr("Suspending an individual thread is not supported on Linux."); break;
	case TE_ResumingIndividualThread:       Message = tr("Resuming an individual thread is not supported on Linux."); break;
	case TE_ChangeWindowVisibility:         Message = tr("Failed to change the window visibility."); break;
	case TE_EnablingDisablingWindow:        Message = tr("Enabling or disabling a window is not supported on X11."); break;
	case TE_ChangeAlwaysTop:                Message = tr("Failed to change the always-on-top state."); break;
	case TE_SetWindowOpacity:               Message = tr("Failed to set the window opacity."); break;
	case TE_ActivateWindow:                 Message = tr("Failed to activate the window."); break;
	case TE_HighlightWindow:                Message = tr("Failed to highlight the window."); break;
	case TE_RestoreWindow:                  Message = tr("Failed to restore the window."); break;
	case TE_MinimizeWindow:                 Message = tr("Failed to minimize the window."); break;
	case TE_MaximizeWindow:                 Message = tr("Failed to maximize the window."); break;
	case TE_CloseWindow:                    Message = tr("Failed to close the window."); break;
	case TE_ReadMemoryProc:                 Message = tr("Cannot read memory of process %1"); break;
	case TE_KernelDriverFile:               Message = tr("The kernel driver file '%1' was not found."); break;
	case TE_AccessKernelDriver:             Message = tr("Unable to access the kernel driver, Error: %1"); break;
	case TE_MemoryOperation:                Message = tr("Memory operation failed."); break;
	case TE_KernelDriverConnected:          Message = tr("The kernel driver is not connected."); break;
	case TE_DeleteAtom:                     Message = tr("Failed to delete atom"); break;
	case TE_OpenProc2:                      Message = tr("Unable to open the process."); break;
	case TE_AccessDumpFile:                 Message = tr("Unable to access the dump file"); break;
	case TE_KProcessHackerUnavail:          Message = tr("KProcessHacker is not available"); break;
	case TE_SetHandleAttribute:             Message = tr("Failed to set handle attribute"); break;
	case TE_ConfirmCloseCriticalHandle:     Message = tr("You are about to close one or more handles for a critical process with strict handle checks enabled. This will shut down the operating system immediately!"); break;
	case TE_CloseHandle:                    Message = tr("Failed To close Handle"); break;
	case TE_OpenProcHandle:                 Message = tr("Unable to open process handle"); break;
	case TE_OpenDuplicateHandle:            Message = tr("Unable to open duplicate handle"); break;
	case TE_OpenJob:                        Message = tr("Failed to open job"); break;
	case TE_TerminateJob:                   Message = tr("Failed to terminate job"); break;
	case TE_JobFreezeUnavail:               Message = tr("Job freezing is only available on windows 8 and later"); break;
	case TE_UnFreezeJob:                    Message = tr("Failed to (un)freeze job"); break;
	case TE_AddProcJob:                     Message = tr("Unable to add the process to the job"); break;
	case TE_ChangeMemoryProtection:         Message = tr("Unable to change memory protection"); break;
	case TE_NotDumpableMemory:              Message = tr("Not dumpable memory item"); break;
	case TE_ConfirmUnloadModule:            Message = tr("Unloading a module may cause the process to crash."); break;
	case TE_FindModuleUnload:               Message = tr("Unable to find the module to unload."); break;
	case TE_UnloadModule:                   Message = tr("Unable to unload the module."); break;
	case TE_ConfirmUnloadDriver:            Message = tr("Unloading a driver may cause system instability."); break;
	case TE_UnloadDriver:                   Message = tr("Unable to unload driver."); break;
	case TE_ConfirmUnmapSection:            Message = tr("Unmapping a section view may cause the process to crash."); break;
	case TE_UnmapSectionView:               Message = tr("Unable to unmap the section view at 0x%1"); break;
	case TE_UnknownModuleType:              Message = tr("Unknown module type!"); break;
	case TE_SetProcExecution:               Message = tr("Failed to set Process execution required"); break;
	case TE_EnableWorkingSet:               Message = tr("Unable to enable working set watch."); break;
	case TE_ReadWorkingSet:                 Message = tr("Failed to read working set watch data."); break;
	case TE_ConfirmEditEnvSuspended:        Message = tr("Editing environment variable(s) of suspended processes is not supported."); break;
	case TE_SetEnvironmentVariable:         Message = tr("Unable to set the environment variable."); break;
	case TE_DeleteEnvironmentVariable:      Message = tr("Unable to delete the environment variable."); break;
	case TE_LocateDebugger:                 Message = tr("Unable to locate the debugger."); break;
	case TE_CreateDebuggerProc:             Message = tr("Failed to create debugger process"); break;
	case TE_ProcDebugged:                   Message = tr("The process is not being debugged."); break;
	case TE_DetachDebugger:                 Message = tr("Failed to detach debugger"); break;
	case TE_DriverFeatureUnsupported:       Message = tr("The loaded driver does not support this feature."); break;
	case TE_ClientNotVerified:              Message = tr("The client must be verifyed by the driver in order to unlock this feature."); break;
	case TE_ConfirmChangeProtection:        Message = tr("Changing Process Protection Flags may impact system stability!"); break;
	case TE_ClearProcProtection:            Message = tr("Failed to Clear Process Protection flag"); break;
	case TE_SetProcPriorityBoost:           Message = tr("Failed to set Process priority boost"); break;
	case TE_SetProcEfficiency:              Message = tr("Failed to set Process efficiency"); break;
	case TE_SetProcPriority:                Message = tr("Failed to set Process priority"); break;
	case TE_SetPagePriority:                Message = tr("Failed to set Page priority"); break;
	case TE_SetIoPriority:                  Message = tr("Failed to set I/O priority"); break;
	case TE_SetCpuAffinity:                 Message = tr("Failed to set CPU affinity"); break;
	case TE_ConfirmTerminateCriticalProc:   Message = tr("You are about to terminate one or more critical processes. This will shut down the operating system immediately."); break;
	case TE_TerminateProc:                  Message = tr("Failed to terminate process"); break;
	case TE_SuspendProc:                    Message = tr("Failed to suspend process"); break;
	case TE_ResumeProc:                     Message = tr("Failed to resume process"); break;
	case TE_ProcAlreadyFrozen:              Message = tr("Process already frozen"); break;
	case TE_FreezeProc:                     Message = tr("Failed to freeze process"); break;
	case TE_ProcFrozen:                     Message = tr("Process is not frozen"); break;
	case TE_UnFreezeProc:                   Message = tr("Failed to un-freeze process"); break;
	case TE_ConfirmCriticalProcShutdown:    Message = tr("If the process ends, the operating system will shut down immediately."); break;
	case TE_ChangeProcCritical:             Message = tr("Unable to change the process critical status."); break;
	case TE_ReduceWorkingSet:               Message = tr("Unable to reduce the working set of a process"); break;
	case TE_LoadDllInto:                    Message = tr("load the DLL into"); break;
	case TE_FlushHeaps:                     Message = tr("Failed Flush Heaps"); break;
	case TE_NoProgramGiven:                 Message = tr("No program was given."); break;
	case TE_ObjectExposeSecurity:           Message = tr("This object does not expose its security descriptor."); break;
	case TE_ChangingSecuritySam:            Message = tr("Changing the security of a SAM account is not implemented."); break;
	case TE_BuildSecurityDesc:              Message = tr("Failed to build a security descriptor."); break;
	case TE_AccountNotResolved:             Message = tr("One of the entries names an account that could not be resolved."); break;
	case TE_ObjectAccessControl:            Message = tr("This object has %1 access control entries of a kind this dialog does not understand, and saving would discard them."); break;
	case TE_NewOwnerResolved:               Message = tr("The new owner could not be resolved."); break;
	case TE_ObjectAuditEntries:             Message = tr("This object has %1 audit entries of a kind this dialog does not understand, and saving would discard them."); break;
	case TE_StartService:                   Message = tr("Failed to start service"); break;
	case TE_PauseService:                   Message = tr("Failed to pause service"); break;
	case TE_ContinueService:                Message = tr("Failed to continue service"); break;
	case TE_StopService:                    Message = tr("Failed to stop service"); break;
	case TE_ConfirmDeleteService:           Message = tr("Deleting a service can prevent the system from starting or functioning properly."); break;
	case TE_DeleteService:                  Message = tr("Failed to delete service"); break;
	case TE_UnsupportedTypeState:           Message = tr("Not supported type or state"); break;
	case TE_SetThreadPriorityBoost:         Message = tr("Failed to set Thread priority boost"); break;
	case TE_SetThreadPriority:              Message = tr("Failed to set Thread priority"); break;
	case TE_ConfirmTerminateCriticalThread: Message = tr("You are about to terminate one or more critical threads. This will shut down the operating system immediately."); break;
	case TE_TerminateThread:                Message = tr("Failed to terminate thread"); break;
	case TE_SuspendThread:                  Message = tr("Failed to suspend thread"); break;
	case TE_ResumeThread:                   Message = tr("Failed to resume thread"); break;
	case TE_ChangeThreadCritical:           Message = tr("Unable to change the thread critical status."); break;
	case TE_NoSynchronousIo:                Message = tr("There is no synchronous I/O to cancel."); break;
	case TE_CancelSynchronousIo:            Message = tr("Unable to cancel synchronous I/O"); break;
	case TE_SetProcVirtualization:          Message = tr("Failed to set process virtualization"); break;
	case TE_OpenToken:                      Message = tr("Could not open token."); break;
	case TE_SetTokenInfo:                   Message = tr("failed to Set Token Information"); break;
	case TE_ConfirmRemovePrivileges:        Message = tr("Removing privileges may reduce the functionality of the process, and is permanent for the lifetime of the process."); break;
	case TE_SetTokenPriv:                   Message = tr("Unable to Set Token Privilege"); break;
	case TE_SetTokenGroups:                 Message = tr("Unable to Set Token Groups"); break;
	case TE_MatchStringMin2:                Message = tr("Match String to short, min length 2"); break;
	case TE_MatchStringMin4:                Message = tr("Match String to short, min length 4"); break;
	case TE_AllocationError:                Message = tr("Allocation error"); break;
	case TE_BackupService:                  Message = tr("Unable to backup the service"); break;
	case TE_ProcHackerKernel:               Message = tr("The Process Hacker kernel driver '%1' was not found."); break;
	case TE_LoadKernelDriver:               Message = tr("Unable to load the kernel driver, Error: 0x%1"); break;

	case TE_UnsupportedWindowsVersion:      Message = tr("This version of Windows is not supported."); break;
	case TE_DriverActivateFailed:           Message = tr("Activating the kernel driver's dynamic data failed."); break;
	case TE_DriverNeedsAdmin:               Message = tr("The kernel driver requires administrative rights."); break;
	case TE_DriverOnly64Bit:                Message = tr("The kernel driver only supports 64-bit systems."); break;
	case TE_DriverNeedsReboot:              Message = tr("The last driver update requires a reboot."); break;
	case TE_DriverConnectFailed:            Message = tr("Connecting to the kernel driver failed."); break;
	case TE_DriverAccessDenied:             Message = tr("Unable to access the kernel driver: %1."); break;
	case TE_RestartSelfFailed:              Message = tr("Restarting TaskExplorer failed."); break;
	case TE_DriverServiceStopFailed:        Message = tr("Stopping the kernel driver service failed."); break;
	case TE_DebugMonitorFailed:             Message = tr("Debug output monitoring failed at %1."); break;

	//
	// An errno failure: what was attempted, then the system's own wording for
	// the number and the number itself.
	//
	case TE_SetProcessPriorityFailed:       Message = tr("Failed to set process priority: %1 (errno %2)"); break;
	case TE_SetThreadPriorityFailed:        Message = tr("Failed to set thread priority: %1 (errno %2)"); break;
	case TE_SetIoPriorityFailed:            Message = tr("Failed to set the I/O priority: %1 (errno %2)"); break;
	case TE_SetOomAdjustFailed:             Message = tr("Failed to set the OOM adjustment: %1 (errno %2)"); break;
	case TE_SetAffinityFailed:              Message = tr("Failed to set the affinity mask: %1 (errno %2)"); break;
	case TE_TerminateProcessFailed:         Message = tr("Failed to terminate process: %1 (errno %2)"); break;
	case TE_SuspendProcessFailed:           Message = tr("Failed to suspend process: %1 (errno %2)"); break;
	case TE_ResumeProcessFailed:            Message = tr("Failed to resume process: %1 (errno %2)"); break;

	case TE_HelperNotStarted:               Message = tr("The TaskHelper process could not be started, so stacks cannot be unwound."); break;
	case TE_HelperNoAnswer:                 Message = tr("The TaskHelper process did not answer."); break;
	case TE_StackPtraceDenied:              Message = tr("Cannot read this thread's stack: ptrace access was denied. "
	                                                     "Restart TaskExplorer elevated, or lower kernel.yama.ptrace_scope."); break;
	case TE_StackTraceFailed:               Message = tr("Stack trace failed: %1"); break;
	case TE_StackNoFrames:                  Message = tr("No stack frames were returned."); break;

	case TE_UnmapSectionViewFailed:         Message = tr("Unable to unmap the section view: %1"); break;
	case TE_FreeMemoryFailed:               Message = tr("Unable to free the memory region: %1"); break;
	case TE_DecommitMemoryFailed:           Message = tr("Unable to decommit the memory region: %1"); break;

	case TE_KsiNotRunningForObject:         Message = tr("The KTaskExplorer driver is not running, and this object can only be reached through it."); break;

	case TE_DumpProcessingModule:           Message = tr("Processing module %1..."); break;
	case TE_DumpProcessingMemory:           Message = tr("Processing memory regions"); break;
	case TE_DumpProcessingKernel:           Message = tr("Processing kernel minidump"); break;
	case TE_DumpSuspendingThreads:          Message = tr("Suspending threads..."); break;
	case TE_DumpHelperSuspending:           Message = tr("Asking the privileged helper to suspend threads..."); break;
	case TE_DumpHelper32Failed:             Message = tr("Failed to start a 32-bit TaskHelper. A 64-bit dump will be created instead."); break;
	case TE_DumpHelper32Started:            Message = tr("Started a 32-bit TaskHelper, to create a 32-bit dump file."); break;
	case TE_DumpHelper32Error:              Message = tr("The 32-bit TaskHelper failed to create the memory dump, Error: %1\r\n"
	                                                     "A 64-bit dump will be created instead."); break;
	case TE_DumpKernelNeedsAdmin:           Message = tr("Unable to create kernel minidump. Kernel minidumps of processes require administrative privileges."); break;
	case TE_DumpCompleted32:                Message = tr("32-bit memory dump completed."); break;
	case TE_DumpCompleted:                  Message = tr("Memory dump completed."); break;
	case TE_DumpFailed:                     Message = tr("Failed to create the dump."); break;
	case TE_DumpCanceled:                   Message = tr("The dump was canceled."); break;
	case TE_UserCanceled:                   Message = tr("The operation was canceled."); break;
	case TE_NotATaskServer:                 Message = tr("The far side is not a TaskServer."); break;
	case TE_ProtocolMismatch:               Message = tr("Protocol mismatch: the server speaks version %1, this viewer speaks version %2."); break;
	case TE_LoginRefused:                   Message = tr("The server refused the user name or password. Another attempt is allowed in %1 seconds."); break;
	case TE_LoginNoAnswer:                  Message = tr("The server did not answer the login."); break;
	case TE_HelperStartFailed:              Message = tr("The privileged helper could not be started."); break;
	case TE_ServerSetupNeedsAdmin:          Message = tr("Setting up the server on this machine needs administrator rights."); break;
	case TE_ServerBinaryMissing:            Message = tr("TaskServer was not found at %1; it has to be installed beside TaskExplorer."); break;
	case TE_ServerServiceFailed:            Message = tr("The server could not be set up: %1"); break;
	case TE_ServerKeyFileFailed:            Message = tr("The key file could not be written: %1"); break;
	case MH_CredStoreUnavailable:           Message = tr("Saved keys need encryption this build could not set up: %1"); break;
	case MH_CredStoreLocked:                Message = tr("The saved keys are locked."); break;
	case MH_CredStoreBadPassword:           Message = tr("That password did not open the saved keys."); break;
	case MH_CredStoreDamaged:               Message = tr("The saved keys could not be read: %1"); break;
	case MH_CredStoreWriteFailed:           Message = tr("The saved keys could not be written: %1"); break;
	case MH_CredStoreNoPlatformKey:         Message = tr("This machine cannot remember the password for you."); break;
	case MH_CredStoreVersion:               Message = tr("The saved keys were written by another version of this program (%1)."); break;
	case TE_MachineIdMismatch:              Message = tr("The machine at %1 is not the one this entry was saved for."); break;
	case TE_MachineNoAnswer:                Message = tr("The machine did not answer."); break;
	case TE_ActionNotPermitted:             Message = tr("This connection may watch this machine but not change it. "
	                                                     "Add \"Actions=true\" to this key in the server's key file to allow it."); break;
	case TE_ActionUnknown:                  Message = tr("The server does not know this action; it is probably older than this viewer."); break;
	case TE_KeyNotAdmin:                    Message = tr("This key may act on its own processes but not on the machine itself. "
	                                                     "Set \"Role=admin\" for this key in the server's key file to allow it."); break;
	case TE_NoRemoteModule:                 Message = tr("This installation cannot connect to other machines: the remote module "
	                                                     "is not installed. Add TaskRemote beside the program to enable it."); break;
	case TE_NotOverNetwork:                 Message = tr("This can only be done by a viewer running on that machine, not over a network connection."); break;
	case TE_NoCertificate:                  Message = tr("Watching another machine is a supporter feature, "
															"and this installation has no valid certificate. See the Support page "
															"in the settings."); break;
	case TE_DumpWriteFailed:                Message = tr("Failed to write the dump: %1"); break;
	case TE_DumpNoRegisterState:            Message = tr("Could not stop the process (%1). The dump will contain memory but no "
	                                                     "register state, so gdb will not be able to produce a backtrace. "
	                                                     "Enable the privileged helper or restart TaskExplorer elevated, or "
	                                                     "lower kernel.yama.ptrace_scope."); break;

	default:
	{
		//
		// A code this build has no sentence for - an older viewer against a
		// newer server, most likely. Shown as its group and number rather than
		// as one long decimal, because that is what it is: 'TE' or 'MH' and an
		// index within it. Legible enough to look up, which is the only useful
		// thing left to do with it.
		//
		const quint32 Code = Error.GetMsgCode();
		const quint32 Group = STATUS_GROUP(Code);
		const char Chars[3] = { (char)(Group >> 8), (char)Group, 0 };

		return tr("Unknown error %1:%2 (0x%3)")
			.arg(Group >= 0x2020 ? QString(Chars) : QString::number(Group))
			.arg(Code & 0xFFFF)
			.arg(Code, 8, 16, QChar('0'));
	}
	}

	//
	// The arguments are values, so they are placed rather than interpreted;
	// a message with fewer placeholders than arguments simply ignores the rest.
	//
	// A name among them may be one the target could not supply - the process
	// that is debugging another one, say - so each is offered to LocalizeName
	// on the way in. That costs one character comparison per argument.
	//
	foreach(const QVariant& Arg, Error.GetArgs())
		Message = Message.arg(LocalizeName(Arg.toString()));

	return Message;
}


//
// ---- priorities ----
//
// Everything below decides between two readings of the same number: the one
// the target's platform intends, and the one this machine happens to use. The
// target decides, so both mappings are here and neither is compiled away.
//

static bool IsWindowsTarget(const CAbstractInfo* pInfo)
{
	const CSystemPtr pSystem = pInfo ? pInfo->GetSystem() : CSystemPtr();
	return pSystem ? pSystem->GetOsType() == CSystemAPI::eOsWindows : true;
}

//
// Windows names six priority classes; the numbers are PROCESS_PRIORITY_CLASS_*.
//
QString GetPriorityClassString(qint32 Value)
{
	switch (Value)
	{
	case CProcessInfo::eProcessPriorityRealTime:	return CTaskExplorer::tr("Real time");
	case CProcessInfo::eProcessPriorityHigh:		return CTaskExplorer::tr("High");
	case CProcessInfo::eProcessPriorityAboveNormal:	return CTaskExplorer::tr("Above normal");
	case CProcessInfo::eProcessPriorityNormal:		return CTaskExplorer::tr("Normal");
	case CProcessInfo::eProcessPriorityBelowNormal:	return CTaskExplorer::tr("Below normal");
	case CProcessInfo::eProcessPriorityIdle:		return CTaskExplorer::tr("Idle");
	default:										return CTaskExplorer::tr("Unknown %1").arg(Value);
	}
}

//
// Linux has no priority classes, only a nice value from -20 to 19. Bucketing it
// onto the same six names keeps one column readable for both kinds of target;
// it is approximate by nature, which is why the number is kept alongside.
//
static QString NiceToString(qint32 Nice)
{
	if (Nice <= -15)	return CTaskExplorer::tr("Real time (%1)").arg(Nice);
	if (Nice <= -5)		return CTaskExplorer::tr("High (%1)").arg(Nice);
	if (Nice < 0)		return CTaskExplorer::tr("Above normal (%1)").arg(Nice);
	if (Nice == 0)		return CTaskExplorer::tr("Normal");
	if (Nice <= 9)		return CTaskExplorer::tr("Below normal (%1)").arg(Nice);
	return CTaskExplorer::tr("Idle (%1)").arg(Nice);
}

static QString SchedPolicyToString(qint32 Policy)
{
	switch (Policy)
	{
	case CAbstractTask::eSchedOther:	return CTaskExplorer::tr("Normal");
	case CAbstractTask::eSchedFifo:		return CTaskExplorer::tr("FIFO");
	case CAbstractTask::eSchedRr:		return CTaskExplorer::tr("Round robin");
	case CAbstractTask::eSchedBatch:	return CTaskExplorer::tr("Batch");
	case CAbstractTask::eSchedIdle:		return CTaskExplorer::tr("Idle");
	case CAbstractTask::eSchedDeadline:	return CTaskExplorer::tr("Deadline");
	default:							return CTaskExplorer::tr("Unknown");
	}
}

static QString WinPagePriorityToString(qint32 Value)
{
	switch (Value)
	{
	case CProcessInfo::ePagePriorityNormal:			return CTaskExplorer::tr("Normal");
	case CProcessInfo::ePagePriorityBelowNormal:	return CTaskExplorer::tr("Below normal");
	case CProcessInfo::ePagePriorityMedium:			return CTaskExplorer::tr("Medium");
	case CProcessInfo::ePagePriorityLow:			return CTaskExplorer::tr("Low");
	case CProcessInfo::ePagePriorityVeryLow:		return CTaskExplorer::tr("Very low");
	case CProcessInfo::ePagePriorityLowest:			return CTaskExplorer::tr("Lowest");
	default:										return CTaskExplorer::tr("Unknown %1").arg(Value);
	}
}

static QString WinIoPriorityToString(qint32 Value)
{
	switch (Value)
	{
	case CProcessInfo::eIoPriorityCritical:	return CTaskExplorer::tr("Critical");
	case CProcessInfo::eIoPriorityHigh:		return CTaskExplorer::tr("High");
	case CProcessInfo::eIoPriorityNormal:	return CTaskExplorer::tr("Normal");
	case CProcessInfo::eIoPriorityLow:		return CTaskExplorer::tr("Low");
	case CProcessInfo::eIoPriorityVeryLow:	return CTaskExplorer::tr("Very low");
	default:								return CTaskExplorer::tr("Unknown %1").arg(Value);
	}
}

//
// A Linux I/O priority is a class and a level packed together.
//
static QString LinuxIoPriorityToString(qint32 Value)
{
	if (Value < 0)
		return CTaskExplorer::tr("Unknown");

	const int Class = Value >> CAbstractTask::eIoPrioClassShift;
	const int Level = Value & CAbstractTask::eIoPrioLevelMask;

	switch (Class)
	{
	case CAbstractTask::eIoPrioNone:		return CTaskExplorer::tr("None");
	case CAbstractTask::eIoPrioRealtime:	return CTaskExplorer::tr("Real time (%1)").arg(Level);
	case CAbstractTask::eIoPrioBestEffort:	return CTaskExplorer::tr("Best effort (%1)").arg(Level);
	case CAbstractTask::eIoPrioIdle:		return CTaskExplorer::tr("Idle");
	}
	return CTaskExplorer::tr("Unknown");
}

//
// Windows names a thread's priority by how far it sits from its process's
// class, so the increment is what carries the meaning, not the absolute value.
// The numbers are THREAD_PRIORITY_*, repeated here because a viewer on another
// platform has no winnt.h to read them from.
//
static QString WinThreadPriorityToString(qint32 Increment, qint32 Priority)
{
	switch (Increment)
	{
	case 16:			// THREAD_BASE_PRIORITY_LOWRT + 1
	case 15:			// THREAD_BASE_PRIORITY_LOWRT
		return CTaskExplorer::tr("Time critical");
	case 2:				return CTaskExplorer::tr("Highest");
	case 1:				return CTaskExplorer::tr("Above normal");
	case 0:				return CTaskExplorer::tr("Normal");
	case -1:			return CTaskExplorer::tr("Below normal");
	case -2:			return CTaskExplorer::tr("Lowest");
	case -15:			// THREAD_BASE_PRIORITY_IDLE
	case -16:
		return CTaskExplorer::tr("Idle");
	case 0x7fffffff:	// THREAD_PRIORITY_ERROR_RETURN
		return QString();
	default:
		return QString::number(Priority);
	}
}


QString GetPriorityString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();
	return IsWindowsTarget(pProcess.data())
		? GetPriorityClassString(pProcess->GetPriority())
		: NiceToString(pProcess->GetPriority());
}

QString GetPriorityString(const CThreadPtr& pThread)
{
	if (pThread.isNull())
		return QString();
	return IsWindowsTarget(pThread.data())
		? WinThreadPriorityToString((qint32)pThread->GetBasePriorityIncrement(), pThread->GetPriority())
		: NiceToString(pThread->GetPriority());
}

//
// On Windows the base priority is a plain number - the class and the thread's
// offset already combined. On Linux the corresponding thing is the scheduling
// policy, which is a name.
//
QString GetBasePriorityString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();
	return IsWindowsTarget(pProcess.data())
		? QString::number(pProcess->GetBasePriority())
		: SchedPolicyToString(pProcess->GetSchedPolicy());
}

QString GetBasePriorityString(const CThreadPtr& pThread)
{
	if (pThread.isNull())
		return QString();
	return IsWindowsTarget(pThread.data())
		? QString::number(pThread->GetBasePriority())
		: SchedPolicyToString(pThread->GetSchedPolicy());
}

QString GetBasePriorityIncrementString(const CThreadPtr& pThread)
{
	if (pThread.isNull())
		return QString();
	return IsWindowsTarget(pThread.data())
		? QString::number((qint32)pThread->GetBasePriorityIncrement())
		: SchedPolicyToString(pThread->GetSchedPolicy());
}

//
// Linux has no page priority at all, so the column stays empty there rather
// than showing a number that means nothing.
//
QString GetPagePriorityString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull() || !IsWindowsTarget(pProcess.data()))
		return QString();
	return WinPagePriorityToString(pProcess->GetPagePriority());
}

QString GetPagePriorityString(const CThreadPtr& pThread)
{
	if (pThread.isNull() || !IsWindowsTarget(pThread.data()))
		return QString();
	return WinPagePriorityToString(pThread->GetPagePriority());
}

QString GetIOPriorityString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();
	return IsWindowsTarget(pProcess.data())
		? WinIoPriorityToString(pProcess->GetIOPriority())
		: LinuxIoPriorityToString(pProcess->GetIOPriority());
}

QString GetIOPriorityString(const CThreadPtr& pThread)
{
	if (pThread.isNull())
		return QString();
	return IsWindowsTarget(pThread.data())
		? WinIoPriorityToString(pThread->GetIOPriority())
		: LinuxIoPriorityToString(pThread->GetIOPriority());
}


//
// ---- process status ----
//

//
// In the order they are worth seeing, not the order of the bits: what is
// unusual about a process comes before what is ordinary about it.
//
QString GetStatusString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();

	//
	// A row the target sent only the outline of. Everything the state is read
	// from was withheld, so this column would otherwise be blank - and blank
	// reads as "nothing to say" rather than "not shown".
	//
	if (pProcess->IsRedacted())
		return CTaskExplorer::tr("Not your process");

	//
	// Wine first, because it is the thing about such a process that is least
	// obvious from anything else in the row: the name and command line look like
	// a Windows program's and the pid, user and state are a Linux process's.
	//
	// Joined to the state rather than replacing it - "Wine, Sleeping" - because
	// both are true and a task manager that dropped the state to make room for a
	// label would be trading a fact for a note.
	//
	// Read from the status flags, which the process *list* carries - see
	// eStatusWine. Asking GetWineInfo here would work only for the one process
	// whose detail has been fetched, which is not what a column needs.
	const quint32 StatusFlags = pProcess->GetStatusFlags();

	QString WinePrefix;
	if (StatusFlags & CProcessInfo::eStatusWine)
		WinePrefix = CTaskExplorer::tr("Wine");

	//
	// A Linux process has one run state where a Windows one has a set of flags -
	// but it has flags too, and this column used to show only the state. One
	// word, where the same column on the machine next to it says "Elevated,
	// Service, Packaged". The state goes last, because it is the one thing here
	// that changes from second to second.
	//
	if (!IsWindowsTarget(pProcess.data()))
	{
		QStringList Notes;
		if (!WinePrefix.isEmpty())									Notes.append(WinePrefix);
		if (StatusFlags & CProcessInfo::eStatusTerminated)			Notes.append(CTaskExplorer::tr("Zombie"));
		if (StatusFlags & CProcessInfo::eStatusSandboxed)			Notes.append(CTaskExplorer::tr("Container"));
		if (StatusFlags & CProcessInfo::eStatusService)				Notes.append(CTaskExplorer::tr("Daemon"));
		if (StatusFlags & CProcessInfo::eStatusSystemProcess)		Notes.append(CTaskExplorer::tr("System"));
		if (StatusFlags & CProcessInfo::eStatusElevated)			Notes.append(CTaskExplorer::tr("Elevated"));

		QString State;
		switch (pProcess->GetRunState())
		{
		case CProcessInfo::eStateRunning:		State = CTaskExplorer::tr("Running"); break;
		case CProcessInfo::eStateSleeping:		State = CTaskExplorer::tr("Sleeping"); break;
		case CProcessInfo::eStateDiskSleep:		State = CTaskExplorer::tr("Disk sleep"); break;
		case CProcessInfo::eStateZombie:		State = CTaskExplorer::tr("Zombie"); break;
		case CProcessInfo::eStateStopped:		State = CTaskExplorer::tr("Stopped"); break;
		case CProcessInfo::eStateTracingStop:	State = CTaskExplorer::tr("Tracing stop"); break;
		case CProcessInfo::eStateIdle:			State = CTaskExplorer::tr("Idle"); break;
		case CProcessInfo::eStateDead:			State = CTaskExplorer::tr("Dead"); break;
		case CProcessInfo::eStateWaking:		State = CTaskExplorer::tr("Waking"); break;
		case CProcessInfo::eStateParked:		State = CTaskExplorer::tr("Parked"); break;
		default:								State = CTaskExplorer::tr("Unknown"); break;
		}

		//
		// A zombie's run state is "Zombie" as well, and saying it twice reads as
		// a stutter rather than as emphasis.
		//
		if (!(StatusFlags & CProcessInfo::eStatusTerminated))
			Notes.append(State);

		return Notes.join(", ");
	}

	const quint32 Flags = StatusFlags;
	QStringList Status;

	//
	// A Windows machine has no Wine on it, so this only ever fires for a target
	// that reported one - but it is asked here too rather than assumed away.
	//
	if (!WinePrefix.isEmpty())							Status.append(WinePrefix);

	if (Flags & CProcessInfo::eStatusHidden)			Status.append(CTaskExplorer::tr("Hidden (!)"));
	if (Flags & CProcessInfo::eStatusTerminated)		Status.append(CTaskExplorer::tr("Terminated"));
	if (Flags & CProcessInfo::eStatusCritical)			Status.append(CTaskExplorer::tr("Critical"));
	if (Flags & CProcessInfo::eStatusSandboxed)			Status.append(CTaskExplorer::tr("Sandboxed"));
	if (Flags & CProcessInfo::eStatusDebugged)			Status.append(CTaskExplorer::tr("Debugged"));
	if (Flags & CProcessInfo::eStatusSuspended)			Status.append(CTaskExplorer::tr("Suspended"));
	if (Flags & CProcessInfo::eStatusHandleFiltered)	Status.append(CTaskExplorer::tr("Handle Filtered"));
	if (Flags & CProcessInfo::eStatusElevated)			Status.append(CTaskExplorer::tr("Elevated"));
	if (Flags & CProcessInfo::eStatusPico)				Status.append(CTaskExplorer::tr("Pico"));
	if (Flags & CProcessInfo::eStatusCrossSession)		Status.append(CTaskExplorer::tr("Cross Session"));
	if (Flags & CProcessInfo::eStatusFrozen)			Status.append(CTaskExplorer::tr("Frozen"));
	if (Flags & CProcessInfo::eStatusBackground)		Status.append(CTaskExplorer::tr("Background"));
	if (Flags & CProcessInfo::eStatusPackaged)			Status.append(CTaskExplorer::tr("Packaged (UWP)"));
	if (Flags & CProcessInfo::eStatusSecure)			Status.append(CTaskExplorer::tr("Secure"));
	if (Flags & CProcessInfo::eStatusImmersive)			Status.append(CTaskExplorer::tr("Immersive"));
	if (Flags & CProcessInfo::eStatusDotNet)			Status.append(CTaskExplorer::tr("DotNet"));
	if (Flags & CProcessInfo::eStatusPacked)			Status.append(CTaskExplorer::tr("Packed"));
	if (Flags & CProcessInfo::eStatusWow64)				Status.append(CTaskExplorer::tr("Wow64"));
	if (Flags & CProcessInfo::eStatusInSignificantJob)	Status.append(CTaskExplorer::tr("InSignificantJob"));
	if (Flags & CProcessInfo::eStatusReflected)			Status.append(CTaskExplorer::tr("Reflected"));
	if (Flags & CProcessInfo::eStatusSystemProcess)		Status.append(CTaskExplorer::tr("System Process"));
	if (Flags & CProcessInfo::eStatusSecureSystem)		Status.append(CTaskExplorer::tr("Secure System"));
	if (Flags & CProcessInfo::eStatusInJob)				Status.append(CTaskExplorer::tr("Job"));
	if (Flags & CProcessInfo::eStatusService)			Status.append(CTaskExplorer::tr("Service"));
	if (Flags & CProcessInfo::eStatusSystem)			Status.append(CTaskExplorer::tr("System"));
	if (Flags & CProcessInfo::eStatusOwned)				Status.append(CTaskExplorer::tr("Owned"));

	return Status.join(CTaskExplorer::tr(", "));
}


//
// ---- services ----
//

QString GetServiceTypeString(const CServicePtr& pService)
{
	if (pService.isNull())
		return QString();

	const quint32 Type = pService->GetType();
	if (Type == 0)
		return QString();

	switch (Type)
	{
	case CServiceInfo::eSvcKernelDriver:		return CTaskExplorer::tr("Driver");
	case CServiceInfo::eSvcFileSystemDriver:	return CTaskExplorer::tr("FS driver");
	case CServiceInfo::eSvcOwnProcess:			return CTaskExplorer::tr("Own process");
	case CServiceInfo::eSvcShareProcess:		return CTaskExplorer::tr("Share process");
	case CServiceInfo::eSvcOwnProcess | CServiceInfo::eSvcInteractive:
												return CTaskExplorer::tr("Own interactive process");
	case CServiceInfo::eSvcShareProcess | CServiceInfo::eSvcInteractive:
												return CTaskExplorer::tr("Share interactive process");
	case CServiceInfo::eSvcUserOwnProcess:		return CTaskExplorer::tr("User own process");
	case CServiceInfo::eSvcUserOwnProcess | CServiceInfo::eSvcUserServiceInstance:
												return CTaskExplorer::tr("User own process (instance)");
	case CServiceInfo::eSvcUserShareProcess:	return CTaskExplorer::tr("User share process");
	case CServiceInfo::eSvcUserShareProcess | CServiceInfo::eSvcUserServiceInstance:
												return CTaskExplorer::tr("User share process (instance)");
	default:									return CTaskExplorer::tr("Unknown %1").arg(Type);
	}
}

QString GetServiceStateString(const CServicePtr& pService)
{
	if (pService.isNull())
		return QString();

	//
	// A target that names its own states - systemd - has already said it in
	// words this side cannot improve on.
	//
	QString Name = pService->GetStateName();
	if (!Name.isEmpty())
		return Name;

	switch (pService->GetState())
	{
	case CServiceInfo::eSvcStopped:			return CTaskExplorer::tr("Stopped");
	case CServiceInfo::eSvcStartPending:	return CTaskExplorer::tr("Start pending");
	case CServiceInfo::eSvcStopPending:		return CTaskExplorer::tr("Stop pending");
	case CServiceInfo::eSvcRunning:			return CTaskExplorer::tr("Running");
	case CServiceInfo::eSvcContinuePending:	return CTaskExplorer::tr("Continue pending");
	case CServiceInfo::eSvcPausePending:	return CTaskExplorer::tr("Pause pending");
	case CServiceInfo::eSvcPaused:			return CTaskExplorer::tr("Paused");
	default:								return CTaskExplorer::tr("Unknown %1").arg(pService->GetState());
	}
}

QString GetServiceStartTypeString(const CServicePtr& pService)
{
	if (pService.isNull() || !IsWindowsTarget(pService.data()))
		return QString();

	switch (pService->GetStartType())
	{
	case CServiceInfo::eSvcDisabled:	return CTaskExplorer::tr("Disabled");
	case CServiceInfo::eSvcBootStart:	return CTaskExplorer::tr("Boot start");
	case CServiceInfo::eSvcSystemStart:	return CTaskExplorer::tr("System start");
	case CServiceInfo::eSvcAutoStart:	return CTaskExplorer::tr("Auto start");
	case CServiceInfo::eSvcDemandStart:	return CTaskExplorer::tr("Demand start");
	default:							return CTaskExplorer::tr("Unknown %1").arg(pService->GetStartType());
	}
}

QString GetServiceErrorControlString(const CServicePtr& pService)
{
	if (pService.isNull() || !IsWindowsTarget(pService.data()))
		return QString();

	switch (pService->GetErrorControl())
	{
	case CServiceInfo::eSvcErrorIgnore:		return CTaskExplorer::tr("Ignore");
	case CServiceInfo::eSvcErrorNormal:		return CTaskExplorer::tr("Normal");
	case CServiceInfo::eSvcErrorSevere:		return CTaskExplorer::tr("Severe");
	case CServiceInfo::eSvcErrorCritical:	return CTaskExplorer::tr("Critical");
	default:								return CTaskExplorer::tr("Unknown %1").arg(pService->GetErrorControl());
	}
}


//
// ---- sockets ----
//

QString GetProtocolString(const CSocketPtr& pSocket)
{
	if (pSocket.isNull())
		return QString();

	switch (pSocket->GetProtocolType())
	{
	case NET_TYPE_IPV4_TCP:	return CTaskExplorer::tr("TCP");
	case NET_TYPE_IPV6_TCP:	return CTaskExplorer::tr("TCP6");
	case NET_TYPE_IPV4_UDP:	return CTaskExplorer::tr("UDP");
	case NET_TYPE_IPV6_UDP:	return CTaskExplorer::tr("UDP6");

	//
	// Unix domain sockets are named after the socket type rather than after a
	// protocol, because there is no protocol: the kernel hands the bytes over
	// without one. Which is what ss, lsof and netstat all call them too.
	//
	case NET_TYPE_UNIX_STREAM:		return CTaskExplorer::tr("Unix");
	case NET_TYPE_UNIX_DGRAM:		return CTaskExplorer::tr("Unix datagram");
	case NET_TYPE_UNIX_SEQPACKET:	return CTaskExplorer::tr("Unix packet");

	default:				return CTaskExplorer::tr("Unknown");
	}
}

//
// What to put in the address columns.
//
// A socket is not always identified by an address: a unix socket is bound to a
// path, or to an abstract name, or to nothing at all. An anonymous socket is
// the ordinary case for the client end of a connected pair, and there the
// interesting thing is what it is connected *to* - which is why the peer is
// named as well.
//
QString GetSocketAddressString(const CSocketPtr& pSocket, bool bRemote)
{
	if (pSocket.isNull())
		return QString();

	if ((pSocket->GetProtocolType() & NET_TYPE_NETWORK_UNIX) != 0)
	{
		const QString Name = bRemote ? pSocket->GetRemoteName() : pSocket->GetLocalName();
		if (!Name.isEmpty())
			return Name;

		//
		// No name, so say who holds it instead.
		//
		// Both ends of a connected pair are usually anonymous - only a listening
		// socket carries the path - and a column of blanks says nothing. What a
		// person wants there is the program on the other side, which is what ss
		// shows for the same reason.
		//
		if (bRemote)
		{
			if (const quint64 Pid = pSocket->GetRemoteProcessId())
			{
				CSystemPtr pSystem = CCluster::GetViewSystem();
				CProcessPtr pProcess = pSystem.isNull() ? CProcessPtr() : pSystem->GetProcessByID(Pid);
				if (!pProcess.isNull())
					return CTaskExplorer::tr("%1 (%2)").arg(::LocalizeName(pProcess->GetName())).arg(Pid);
				return CTaskExplorer::tr("pid %1").arg(Pid);
			}
		}

		//
		// And blank where there is nothing to say: an unnamed end with no peer
		// this machine can see is a fact about the socket, and a placeholder
		// down every row would be noise.
		//
		return QString();
	}

	const QHostAddress Address = bRemote ? pSocket->GetRemoteAddress() : pSocket->GetLocalAddress();
	return Address.toString();
}

QString GetSocketStateString(const CSocketPtr& pSocket)
{
	if (pSocket.isNull())
		return QString();

	const quint32 State = pSocket->GetState();

	//
	// A unix socket's states are its own - it is connected to another socket on
	// this machine or it is not, and it never was on a network. Read before the
	// TCP branch because a unix stream socket carries the TCP protocol bit to
	// say it is connection oriented, which is true, but its state numbers are
	// not TCP's.
	//
	if ((pSocket->GetProtocolType() & NET_TYPE_NETWORK_UNIX) != 0)
	{
		switch ((int)State)
		{
		case eUnixListen:			return CTaskExplorer::tr("Listen");
		case eUnixConnected:		return CTaskExplorer::tr("Connected");
		case eUnixConnecting:		return CTaskExplorer::tr("Connecting");
		case eUnixDisconnecting:	return CTaskExplorer::tr("Disconnecting");
		case eUnixUnconnected:		return CTaskExplorer::tr("Unconnected");
		default:					return CTaskExplorer::tr("Unknown %1").arg(State);
		}
	}

	//
	// Only TCP has a connection state; for anything else the question is just
	// whether the socket is still there.
	//
	if ((pSocket->GetProtocolType() & NET_TYPE_PROTOCOL_TCP) == 0)
		return State == eTcpClosed ? CTaskExplorer::tr("Closed") : CTaskExplorer::tr("Open");

	switch ((int)State)
	{
	case eTcpClosed:		return CTaskExplorer::tr("Closed");
	case eTcpListen:		return CTaskExplorer::tr("Listen");
	case eTcpSynSent:		return CTaskExplorer::tr("SYN sent");
	case eTcpSynRcvd:		return CTaskExplorer::tr("SYN received");
	case eTcpEstablished:	return CTaskExplorer::tr("Established");
	case eTcpFinWait1:		return CTaskExplorer::tr("FIN wait 1");
	case eTcpFinWait2:		return CTaskExplorer::tr("FIN wait 2");
	case eTcpCloseWait:	return CTaskExplorer::tr("Close wait");
	case eTcpClosing:		return CTaskExplorer::tr("Closing");
	case eTcpLastAck:		return CTaskExplorer::tr("Last ACK");
	case eTcpTimeWait:		return CTaskExplorer::tr("Time wait");
	case eTcpDeleteTcb:	return CTaskExplorer::tr("Delete TCB");
	case eTcpBlocked:		return CTaskExplorer::tr("Blocked");
	default:							return CTaskExplorer::tr("Unknown %1").arg(State);
	}
}


//
// ---- modules ----
//

QString GetModuleTypeString(const CModulePtr& pModule)
{
	if (pModule.isNull())
		return QString();

	switch (pModule->GetType())
	{
	case CModuleInfo::eModuleDll:			return CTaskExplorer::tr("DLL");
	case CModuleInfo::eModuleMappedFile:	return CTaskExplorer::tr("Mapped file");
	case (quint64)-1:
	case CModuleInfo::eModuleMappedImage:	return CTaskExplorer::tr("Mapped image");
	case CModuleInfo::eModuleWow64Dll:		return CTaskExplorer::tr("WOW64 DLL");
	case CModuleInfo::eModuleKernel:		return CTaskExplorer::tr("Kernel module");
	case CModuleInfo::eModuleEnclave:		return CTaskExplorer::tr("Enclave module");
	default:								return CTaskExplorer::tr("Unknown %1").arg(pModule->GetType());
	}
}

QString GetLoadReasonString(const CModulePtr& pModule)
{
	if (pModule.isNull())
		return QString();

	const qint32 Reason = pModule->GetLoadReason();
	switch (Reason)
	{
	case CModuleInfo::eLoadReasonNotAvailable:			return QString();
	case CModuleInfo::eLoadStaticDependency:			return CTaskExplorer::tr("Static dependency");
	case CModuleInfo::eLoadStaticForwarderDependency:	return CTaskExplorer::tr("Static forwarder dependency");
	case CModuleInfo::eLoadDynamicForwarderDependency:	return CTaskExplorer::tr("Dynamic forwarder dependency");
	case CModuleInfo::eLoadDelayloadDependency:			return CTaskExplorer::tr("Delay load dependency");
	case CModuleInfo::eLoadDynamic:						return CTaskExplorer::tr("Dynamic");
	case CModuleInfo::eLoadAsImage:						return CTaskExplorer::tr("As image");
	case CModuleInfo::eLoadAsData:						return CTaskExplorer::tr("As data");
	case CModuleInfo::eLoadEnclavePrimary:				return CTaskExplorer::tr("Enclave");
	case CModuleInfo::eLoadEnclaveDependency:			return CTaskExplorer::tr("Enclave dependency");
	case CModuleInfo::eLoadPatchImage:					return CTaskExplorer::tr("Patch image");
	default:											return CTaskExplorer::tr("Unknown %1").arg(Reason);
	}
}

//
// ARM64X and CHPE images carry code for two architectures; the header says one
// thing and the loader another, so the version field is what distinguishes them.
//
QString GetImageMachineString(const CModulePtr& pModule)
{
	if (pModule.isNull())
		return QString();

	const bool bHybrid = pModule->GetImageCHPEVersion() != 0;
	switch (pModule->GetImageMachine())
	{
	case CModuleInfo::eMachineI386:		return bHybrid ? CTaskExplorer::tr("x86 (CHPE)") : CTaskExplorer::tr("x86");
	case CModuleInfo::eMachineAmd64:	return bHybrid ? CTaskExplorer::tr("x64 (ARM64X)") : CTaskExplorer::tr("x64");
	case CModuleInfo::eMachineArmNt:	return CTaskExplorer::tr("ARM");
	case CModuleInfo::eMachineArm64:	return bHybrid ? CTaskExplorer::tr("ARM64 (ARM64X)") : CTaskExplorer::tr("ARM64");
	default:							return QString();
	}
}

QString GetEnclaveTypeString(const CModulePtr& pModule)
{
	if (pModule.isNull())
		return QString();

	switch (pModule->GetEnclaveType())
	{
	case CModuleInfo::eEnclaveSgx:	return CTaskExplorer::tr("SGX");
	case CModuleInfo::eEnclaveSgx2:	return CTaskExplorer::tr("SGX2");
	case CModuleInfo::eEnclaveVbs:	return CTaskExplorer::tr("VBS");
	default:						return CTaskExplorer::tr("Unknown");
	}
}


//
// ---- heaps ----
//

QString GetHeapTypeString(const CHeapPtr& pHeap)
{
	if (pHeap.isNull())
		return QString();

	const int Kind = pHeap->GetHeapKind();
	if (Kind == CHeapInfo::eHeapUnknown)
		return CTaskExplorer::tr("Unknown Heap");

	//
	// The front end is what actually services small allocations, so it is worth
	// naming alongside the implementation.
	//
	switch (pHeap->GetFrontEndType())
	{
	case CHeapInfo::eFrontEndLookaside:
		return Kind == CHeapInfo::eHeapNt ? CTaskExplorer::tr("NT Heap (Lookaside)") : CTaskExplorer::tr("Segment Heap (Lookaside)");
	case CHeapInfo::eFrontEndLfh:
		return Kind == CHeapInfo::eHeapNt ? CTaskExplorer::tr("NT Heap (LFH)") : CTaskExplorer::tr("Segment Heap (LFH)");
	default:
		return Kind == CHeapInfo::eHeapNt ? CTaskExplorer::tr("NT Heap") : CTaskExplorer::tr("Segment Heap");
	}
}

QString GetHeapClassString(const CHeapPtr& pHeap)
{
	if (pHeap.isNull())
		return QString();

	switch (pHeap->GetClass())
	{
	case CHeapInfo::eHeapClassProcess:		return CTaskExplorer::tr("Process Heap");
	case CHeapInfo::eHeapClassPrivate:		return CTaskExplorer::tr("Private Heap");
	case CHeapInfo::eHeapClassKernel:		return CTaskExplorer::tr("Kernel Heap");
	case CHeapInfo::eHeapClassGdi:			return CTaskExplorer::tr("GDI Heap");
	case CHeapInfo::eHeapClassUser:			return CTaskExplorer::tr("User Heap");
	case CHeapInfo::eHeapClassConsole:		return CTaskExplorer::tr("Console Heap");
	case CHeapInfo::eHeapClassDesktop:		return CTaskExplorer::tr("Desktop Heap");
	case CHeapInfo::eHeapClassCsrShared:	return CTaskExplorer::tr("CSRSS Shared Heap");
	case CHeapInfo::eHeapClassCsrPort:		return CTaskExplorer::tr("CSRSS Port Heap");
	default:								return CTaskExplorer::tr("Unknown Heap");
	}
}

QString GetHeapFlagsString(const CHeapPtr& pHeap)
{
	if (pHeap.isNull())
		return QString();

	const quint32 Flags = pHeap->GetFlags();
	QStringList Info;

	if (Flags & CHeapInfo::eHeapNoSerialize)				Info.append(CTaskExplorer::tr("No serialize"));
	if (Flags & CHeapInfo::eHeapGrowable)					Info.append(CTaskExplorer::tr("Growable"));
	if (Flags & CHeapInfo::eHeapGenerateExceptions)			Info.append(CTaskExplorer::tr("Generate exceptions"));
	if (Flags & CHeapInfo::eHeapZeroMemory)					Info.append(CTaskExplorer::tr("Zero memory"));
	if (Flags & CHeapInfo::eHeapReallocInPlaceOnly)			Info.append(CTaskExplorer::tr("Realloc in-place"));
	if (Flags & CHeapInfo::eHeapTailChecking)				Info.append(CTaskExplorer::tr("Tail checking"));
	if (Flags & CHeapInfo::eHeapFreeChecking)				Info.append(CTaskExplorer::tr("Free checking"));
	if (Flags & CHeapInfo::eHeapDisableCoalesceOnFree)		Info.append(CTaskExplorer::tr("Coalesce on free"));
	if (Flags & CHeapInfo::eHeapCreateAlign16)				Info.append(CTaskExplorer::tr("Align 16"));
	if (Flags & CHeapInfo::eHeapCreateEnableTracing)		Info.append(CTaskExplorer::tr("Traceable"));
	if (Flags & CHeapInfo::eHeapCreateEnableExecute)		Info.append(CTaskExplorer::tr("Executable"));
	if (Flags & CHeapInfo::eHeapCreateSegmentHeap)			Info.append(CTaskExplorer::tr("Segment heap"));
	if (Flags & CHeapInfo::eHeapCreateHardened)				Info.append(CTaskExplorer::tr("Segment hardened"));

	return Info.join(", ");
}


//
// ---- memory ----
//

//
// Page protection reads as a short mnemonic - "RWX+G" - because a memory list
// shows one per row and the column has to stay narrow.
//
static QString PageProtectionToString(quint32 Protect)
{
	if (!Protect)
		return QString();

	QString Str;
	if (Protect & CMemoryInfo::ePageNoAccess)					Str = CTaskExplorer::tr("NA");
	else if (Protect & CMemoryInfo::ePageReadOnly)				Str = CTaskExplorer::tr("R");
	else if (Protect & CMemoryInfo::ePageReadWrite)				Str = CTaskExplorer::tr("RW");
	else if (Protect & CMemoryInfo::ePageWriteCopy)				Str = CTaskExplorer::tr("WC");
	else if (Protect & CMemoryInfo::ePageExecute)				Str = CTaskExplorer::tr("X");
	else if (Protect & CMemoryInfo::ePageExecuteRead)			Str = CTaskExplorer::tr("RX");
	else if (Protect & CMemoryInfo::ePageExecuteReadWrite)		Str = CTaskExplorer::tr("RWX");
	else if (Protect & CMemoryInfo::ePageExecuteWriteCopy)		Str = CTaskExplorer::tr("WCX");
	else														Str = CTaskExplorer::tr("?");

	if (Protect & CMemoryInfo::ePageGuard)			Str += CTaskExplorer::tr("+G");
	if (Protect & CMemoryInfo::ePageNoCache)		Str += CTaskExplorer::tr("+NC");
	if (Protect & CMemoryInfo::ePageWriteCombine)	Str += CTaskExplorer::tr("+WCM");

	return Str;
}

QString GetProtectionString(const CMemoryPtr& pMemory)
{
	return pMemory.isNull() ? QString() : PageProtectionToString(pMemory->GetProtection());
}

QString GetAllocProtectionString(const CMemoryPtr& pMemory)
{
	return pMemory.isNull() ? QString() : PageProtectionToString(pMemory->GetAllocProtection());
}

static QString MemoryKindToString(const CMemoryPtr& pMemory)
{
	const quint32 Type = pMemory->GetType();
	if (Type & CMemoryInfo::eMemPrivate)	return CTaskExplorer::tr("Private");
	if (Type & CMemoryInfo::eMemMapped)		return CTaskExplorer::tr("Mapped");
	if (Type & CMemoryInfo::eMemImage)		return CTaskExplorer::tr("Image");
	return CTaskExplorer::tr("Unknown");
}

static QString MemoryStateToString(const CMemoryPtr& pMemory)
{
	const quint32 State = pMemory->GetState();
	if (State & CMemoryInfo::eMemCommit)	return CTaskExplorer::tr("Commit");
	if (State & CMemoryInfo::eMemReserve)	return CTaskExplorer::tr("Reserved");
	if (State & CMemoryInfo::eMemFree)		return CTaskExplorer::tr("Free");
	return CTaskExplorer::tr("Unknown");
}

QString GetMemoryTypeString(const CMemoryPtr& pMemory)
{
	if (pMemory.isNull())
		return QString();

	if (pMemory->GetState() & CMemoryInfo::eMemFree)
	{
		//
		// An unusable hole is free but can never be allocated, which is worth
		// distinguishing from ordinary free space.
		//
		return pMemory->GetRegionType() == CMemoryInfo::eRegionUnusable
			? CTaskExplorer::tr("Free (Unusable)") : CTaskExplorer::tr("Free");
	}

	//
	// The allocation base names the whole reservation; the pages inside it are
	// described by what they are and what state they are in.
	//
	if (pMemory->IsAllocationBase())
		return MemoryKindToString(pMemory);

	return CTaskExplorer::tr("%1: %2").arg(MemoryKindToString(pMemory)).arg(MemoryStateToString(pMemory));
}

//
// What a region is for. Several kinds carry a payload - which thread, which
// heap, which file - so the wording is assembled from the value and that
// payload rather than handed over whole.
//
QString GetMemoryUseString(const CMemoryPtr& pMemory)
{
	if (pMemory.isNull())
		return QString();

	const int Type = pMemory->GetRegionType();
	switch (Type)
	{
	case CMemoryInfo::eRegionCustom:
	case CMemoryInfo::eRegionMappedFile:
		return pMemory->GetRegionText();

	case CMemoryInfo::eRegionUserSharedData:
		return CTaskExplorer::tr("USER_SHARED_DATA");
	case CMemoryInfo::eRegionHypervisorSharedData:
		return CTaskExplorer::tr("HYPERVISOR_SHARED_DATA");
	case CMemoryInfo::eRegionApiSetMap:
		return CTaskExplorer::tr("ApiSetMap");

	case CMemoryInfo::eRegionPeb:
	case CMemoryInfo::eRegionPeb32:
		return CTaskExplorer::tr("PEB%1").arg(Type == CMemoryInfo::eRegionPeb32 ? CTaskExplorer::tr(" 32-bit") : QString());

	case CMemoryInfo::eRegionTeb:
	case CMemoryInfo::eRegionTeb32:
		return CTaskExplorer::tr("TEB%1 (thread %2)")
			.arg(Type == CMemoryInfo::eRegionTeb32 ? CTaskExplorer::tr(" 32-bit") : QString())
			.arg(pMemory->GetRegionThreadId());

	case CMemoryInfo::eRegionStack:
	case CMemoryInfo::eRegionStack32:
		return CTaskExplorer::tr("Stack%1 (thread %2)")
			.arg(Type == CMemoryInfo::eRegionStack32 ? CTaskExplorer::tr(" 32-bit") : QString())
			.arg(pMemory->GetRegionThreadId());

	case CMemoryInfo::eRegionHeap:
	case CMemoryInfo::eRegionHeap32:
		return CTaskExplorer::tr("Heap%1 (ID %2)")
			.arg(Type == CMemoryInfo::eRegionHeap32 ? CTaskExplorer::tr(" 32-bit") : QString())
			.arg(pMemory->GetRegionIndex());

	case CMemoryInfo::eRegionHeapSegment:
	case CMemoryInfo::eRegionHeapSegment32:
		return CTaskExplorer::tr("Heap segment%1 (ID %2)")
			.arg(Type == CMemoryInfo::eRegionHeapSegment32 ? CTaskExplorer::tr(" 32-bit") : QString())
			.arg(pMemory->GetRegionIndex());

	case CMemoryInfo::eRegionCfgBitmap:
	case CMemoryInfo::eRegionCfgBitmap32:
		return CTaskExplorer::tr("CFG Bitmap%1").arg(Type == CMemoryInfo::eRegionCfgBitmap32 ? CTaskExplorer::tr(" 32-bit") : QString());
	}

	return QString();
}

QString GetRegionTypeExString(const CMemoryPtr& pMemory)
{
	if (pMemory.isNull())
		return QString();

	const quint32 Flags = pMemory->GetRegionTypeExFlags();
	if (!Flags)
		return QString();

	QStringList Types;
	if (Flags & CMemoryInfo::eRegionPrivate)			Types.append(CTaskExplorer::tr("Private"));
	if (Flags & CMemoryInfo::eRegionMappedDataFile)		Types.append(CTaskExplorer::tr("MappedDataFile"));
	if (Flags & CMemoryInfo::eRegionMappedImage)		Types.append(CTaskExplorer::tr("MappedImage"));
	if (Flags & CMemoryInfo::eRegionMappedPageFile)		Types.append(CTaskExplorer::tr("MappedPageFile"));
	if (Flags & CMemoryInfo::eRegionMappedPhysical)		Types.append(CTaskExplorer::tr("MappedPhysical"));
	if (Flags & CMemoryInfo::eRegionDirectMapped)		Types.append(CTaskExplorer::tr("DirectMapped"));
	if (Flags & CMemoryInfo::eRegionSoftwareEnclave)	Types.append(CTaskExplorer::tr("Software enclave"));
	if (Flags & CMemoryInfo::eRegionPageSize64K)		Types.append(CTaskExplorer::tr("PageSize64K"));
	if (Flags & CMemoryInfo::eRegionPlaceholder)		Types.append(CTaskExplorer::tr("Placeholder"));
	if (Flags & CMemoryInfo::eRegionMappedAwe)			Types.append(CTaskExplorer::tr("Mapped AWE"));
	if (Flags & CMemoryInfo::eRegionMappedWriteWatch)	Types.append(CTaskExplorer::tr("MappedWriteWatch"));
	if (Flags & CMemoryInfo::eRegionPageSizeLarge)		Types.append(CTaskExplorer::tr("PageSizeLarge"));
	if (Flags & CMemoryInfo::eRegionPageSizeHuge)		Types.append(CTaskExplorer::tr("PageSizeHuge"));

	return Types.join(CTaskExplorer::tr(", "));
}

//
// Several signing levels are vendor slots with no published meaning, so they
// all read as "Custom" rather than pretending to distinguish them.
//
QString GetSigningLevelString(const CMemoryPtr& pMemory)
{
	if (pMemory.isNull())
		return QString();

	switch (pMemory->GetSigningLevel())
	{
	case CMemoryInfo::eSignUnchecked:		return CTaskExplorer::tr("Unchecked");
	case CMemoryInfo::eSignUnsigned:		return CTaskExplorer::tr("Unsigned");
	case CMemoryInfo::eSignEnterprise:		return CTaskExplorer::tr("Enterprise");
	case CMemoryInfo::eSignDeveloper:		return CTaskExplorer::tr("Developer");
	case CMemoryInfo::eSignAuthenticode:	return CTaskExplorer::tr("Authenticode");
	case CMemoryInfo::eSignStore:			return CTaskExplorer::tr("StoreApp");
	case CMemoryInfo::eSignAntimalware:		return CTaskExplorer::tr("Antimalware");
	case CMemoryInfo::eSignMicrosoft:		return CTaskExplorer::tr("Microsoft");
	case CMemoryInfo::eSignDynamicCodegen:	return CTaskExplorer::tr("CodeGen");
	case CMemoryInfo::eSignWindows:			return CTaskExplorer::tr("Windows");
	case CMemoryInfo::eSignWindowsTcb:		return CTaskExplorer::tr("WinTcb");
	case CMemoryInfo::eSignCustom2:
	case CMemoryInfo::eSignCustom4:
	case CMemoryInfo::eSignCustom5:
	case CMemoryInfo::eSignCustom6:
	case CMemoryInfo::eSignCustom7:
		return CTaskExplorer::tr("Custom");
	}
	return QString();
}


//
// ---- process: image, protection, and the rest of the details page ----
//

//
// The machine an image was built for. These are the names people use, not the
// PE header's own spelling, and they are not translated - "x64" is "x64".
//
QString GetArchString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();

	const bool bArm64X = pProcess->IsArm64X();
	switch (pProcess->GetArchitecture())
	{
	case CModuleInfo::eMachineI386:		return "x86";
	case CModuleInfo::eMachineAmd64:	return bArm64X ? "x64 (ARM64X)" : "x64";
	case CModuleInfo::eMachineArmNt:	return "ARM";
	case CModuleInfo::eMachineArm64:	return bArm64X ? "ARM64 (ARM64X)" : "ARM64";
	}
	return QString();
}

QString GetSubsystemString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();

	switch (pProcess->GetSubsystem())
	{
	case CProcessInfo::eSubsystemUnknown:		return QString();
	case CProcessInfo::eSubsystemNative:		return CTaskExplorer::tr("Native");
	case CProcessInfo::eSubsystemWindowsGui:	return CTaskExplorer::tr("Windows");
	case CProcessInfo::eSubsystemWindowsCui:	return CTaskExplorer::tr("Windows console");
	case CProcessInfo::eSubsystemOs2Cui:		return CTaskExplorer::tr("OS/2");
	case CProcessInfo::eSubsystemPosixCui:		return CTaskExplorer::tr("POSIX");
	}
	return CTaskExplorer::tr("Unknown");
}

//
// The Windows version a process was told it is running on. An empty reading
// means it sees the real one, which is the ordinary case and not worth a word.
//
QString GetOsContextString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();

	const quint32 Context = pProcess->GetOsContextVersion();
	switch (Context)
	{
	case CProcessInfo::eOsContextNone:	return QString();
	case CProcessInfo::eOsContext10:	return CTaskExplorer::tr("10");
	case CProcessInfo::eOsContext81:	return CTaskExplorer::tr("8.1");
	case CProcessInfo::eOsContext8:		return CTaskExplorer::tr("8");
	case CProcessInfo::eOsContext7:		return CTaskExplorer::tr("7");
	case CProcessInfo::eOsContextVista:	return CTaskExplorer::tr("Vista");
	case CProcessInfo::eOsContextXp:	return CTaskExplorer::tr("XP");
	}
	return QString::number(Context);
}

QString GetDPIAwarenessString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();

	switch (pProcess->GetDPIAwareness())
	{
	case CProcessInfo::eDpiUnaware:			return CTaskExplorer::tr("Unaware");
	case CProcessInfo::eDpiSystemAware:		return CTaskExplorer::tr("System aware");
	case CProcessInfo::eDpiPerMonitorAware:	return CTaskExplorer::tr("Per-monitor aware");
	}
	return QString();
}

//
// A process with no main window has nothing to say here, which is different
// from a window that is answering normally.
//
QString GetWindowStatusString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();

	CWndPtr pWnd = pProcess->GetMainWindow();
	if (pWnd.isNull())
		return QString();

	return pWnd->IsHung() ? CTaskExplorer::tr("Not responding") : CTaskExplorer::tr("Running");
}

QString GetAccessMaskString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();
	return CTaskExplorer::tr("0x%1").arg(pProcess->GetAccessMask(), 0, 16);
}

//
// Thread-local storage in the two banks a process has: the fixed one, and the
// expansion bank that is only allocated once the first fills up. Showing both
// as a fraction of their size is what makes a nearly-exhausted process visible.
//
QString GetTlsBitmapCountString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();

	const quint16 Count = pProcess->GetTlsBitmapCount();
	if (Count == 0)
		return QString();

	if (Count > CProcessInfo::eTlsMinimumAvailable)
	{
		const int Expansion = Count - CProcessInfo::eTlsMinimumAvailable;
		return CTaskExplorer::tr("%1 (100%) | %2 (%3%)")
			.arg(CProcessInfo::eTlsMinimumAvailable)
			.arg(Expansion)
			.arg(Expansion * 100.f / CProcessInfo::eTlsExpansionSlots, 0, 'f', 2);
	}

	return CTaskExplorer::tr("%1 (%2%) | 0 (0%)")
		.arg(Count)
		.arg(Count * 100.f / CProcessInfo::eTlsMinimumAvailable, 0, 'f', 2);
}

//
// Which failures a process asked the system to handle without a dialog. The
// wording names the failure, not the suppression, since the whole list sits
// under a heading that already says what it is.
//
QString GetErrorModeString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();

	const quint32 Mode = pProcess->GetErrorMode();

	QStringList Modes;
	if (Mode & CProcessInfo::eErrModeFailCriticalErrors)		Modes.append(CTaskExplorer::tr("Fail critical"));
	if (Mode & CProcessInfo::eErrModeNoGpFaultErrorBox)			Modes.append(CTaskExplorer::tr("GP faults"));
	if (Mode & CProcessInfo::eErrModeNoAlignmentFaultExcept)	Modes.append(CTaskExplorer::tr("Alignment faults"));
	if (Mode & CProcessInfo::eErrModeNoOpenFileErrorBox)		Modes.append(CTaskExplorer::tr("Openfile faults"));

	return Modes.join(CTaskExplorer::tr(", "));
}

//
// The strict and audit variants supersede the plain ones rather than adding to
// them, so only one of each pair is named.
//
QString GetMitigationsString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();

	const quint32 Flags = pProcess->GetMitigationFlags();

	QStringList Mitigations;
	if (Flags & CProcessInfo::eMitigationAslr)			Mitigations.append(CTaskExplorer::tr("ASLR"));
	if (Flags & CProcessInfo::eMitigationDep)			Mitigations.append(CTaskExplorer::tr("DEP"));
	if (Flags & CProcessInfo::eMitigationCfg)			Mitigations.append(CTaskExplorer::tr("CFG"));

	if (Flags & CProcessInfo::eMitigationXfgAudit)		Mitigations.append(CTaskExplorer::tr("XFG Audit"));
	else if (Flags & CProcessInfo::eMitigationXfg)		Mitigations.append(CTaskExplorer::tr("XFG"));

	if (Flags & CProcessInfo::eMitigationCetStrict)		Mitigations.append(CTaskExplorer::tr("CET strict"));
	else if (Flags & CProcessInfo::eMitigationCet)		Mitigations.append(CTaskExplorer::tr("CET"));

	//
	// And the two a Linux process either has or has not. Strict seccomp is
	// worth naming separately because seeing it at all is remarkable: it allows
	// four system calls, and a program using it has been written around that.
	//
	if (Flags & CProcessInfo::eMitigationSeccompStrict)	Mitigations.append(CTaskExplorer::tr("seccomp strict"));
	else if (Flags & CProcessInfo::eMitigationSeccomp)	Mitigations.append(CTaskExplorer::tr("seccomp"));

	if (Flags & CProcessInfo::eMitigationNoNewPrivs)	Mitigations.append(CTaskExplorer::tr("no-new-privs"));

	//
	// What a sandbox granted, which belongs in this column for the same reason
	// the mitigations do: it is the list of what the system is enforcing on
	// this process. Three of them and a count, because a snap declares a dozen
	// and a column is not a details page - the full list is on the process's
	// own security page.
	//
	const QStringList Permissions = pProcess->GetSandboxPermissions();
	if (!Permissions.isEmpty())
	{
		QString Sandbox = CTaskExplorer::tr("sandbox: ") + Permissions.mid(0, 3).join(", ");
		if (Permissions.count() > 3)
			Sandbox += CTaskExplorer::tr(" +%1").arg(Permissions.count() - 3);
		Mitigations.append(Sandbox);
	}

	return Mitigations.join(CTaskExplorer::tr(", "));
}

//
// Who a protected process is protected on behalf of. Signer 0 means the kernel
// named no one, so the parentheses are left off rather than left empty.
//
static QString ProtectionSignerToString(quint8 Signer)
{
	switch (Signer)
	{
	case CProcessInfo::eSignerAuthenticode:	return CTaskExplorer::tr("(Authenticode)");
	case CProcessInfo::eSignerCodeGen:		return CTaskExplorer::tr("(CodeGen)");
	case CProcessInfo::eSignerAntimalware:	return CTaskExplorer::tr("(Antimalware)");
	case CProcessInfo::eSignerLsa:			return CTaskExplorer::tr("(Lsa)");
	case CProcessInfo::eSignerWindows:		return CTaskExplorer::tr("(Windows)");
	case CProcessInfo::eSignerWinTcb:		return CTaskExplorer::tr("(WinTcb)");
	case CProcessInfo::eSignerWinSystem:	return CTaskExplorer::tr("(WinSystem)");
	case CProcessInfo::eSignerStoreApp:		return CTaskExplorer::tr("(StoreApp)");
	}
	return QString();
}

QString GetPPLProtectionString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();

	const quint8 Type = pProcess->GetProtectionType();
	if (Type == CProcessInfo::eProtectionUnknown || Type == CProcessInfo::eProtectionNone)
		return QString();

	//
	// Before 8.1 there was one flag and nothing else to say about it.
	//
	if (Type == CProcessInfo::eProtectionLegacy)
		return CTaskExplorer::tr("Yes");

	const QString Signer = ProtectionSignerToString(pProcess->GetProtectionSigner());
	switch (Type)
	{
	case CProcessInfo::eProtectionLight:	return CTaskExplorer::tr("Light %1").arg(Signer);
	case CProcessInfo::eProtectionFull:		return CTaskExplorer::tr("Full %1").arg(Signer);
	}
	return CTaskExplorer::tr("Unknown %1").arg(Signer);
}

QString GetKPHProtectionString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();

	QString Level;
	switch (pProcess->GetKphLevel())
	{
	case CProcessInfo::eKphNotVerified:	return QString();
	case CProcessInfo::eKphMaximum:		Level = CTaskExplorer::tr("Max");	break;
	case CProcessInfo::eKphHigh:		Level = CTaskExplorer::tr("High");	break;
	case CProcessInfo::eKphMedium:		Level = CTaskExplorer::tr("Med");	break;
	case CProcessInfo::eKphLow:			Level = CTaskExplorer::tr("Low");	break;
	case CProcessInfo::eKphMinimum:		Level = CTaskExplorer::tr("Min");	break;
	default:							Level = CTaskExplorer::tr("None");	break;
	}

	QString Str = CTaskExplorer::tr("KPH %1").arg(Level);

#ifdef _DEBUG
	//
	// The observations behind that level, in the shorthand a developer reading
	// them wants. Several are stated as absences, so they are listed when the
	// flag is missing rather than when it is set.
	//
	const quint32 State = pProcess->GetKphState();

	QStringList Flags;
	if (State & CProcessInfo::eKphSecurelyCreated)					Flags.append("SecuC");
	if (State & CProcessInfo::eKphVerifiedProcess)					Flags.append("VProc");
	if (State & CProcessInfo::eKphProtectedProcess)
	{
		Flags.append("PProc");
		if (State & CProcessInfo::eKphNoUntrustedImages)			Flags.append("NoUnk");
	}
	if (!(State & CProcessInfo::eKphHasFileObject))					Flags.append("NoFile");
	if (!(State & CProcessInfo::eKphHasSectionObjectPointers))		Flags.append("NoSect");
	if (!(State & CProcessInfo::eKphNoUserWritableReferences))		Flags.append("WrRef");
	if (!(State & CProcessInfo::eKphNoFileTransaction))				Flags.append("FileTx");
	if (!(State & CProcessInfo::eKphNotBeingDebugged))				Flags.append("Dbg");

	Str += QString(" (%1)").arg(Flags.join(", "));
#endif

	return Str;
}

//
// The two protections a process can be under are independent; a process can
// have either, both, or neither.
//
QString GetProcessProtectionString(const CProcessPtr& pProcess)
{
	const QString Ppl = GetPPLProtectionString(pProcess);
	const QString Kph = GetKPHProtectionString(pProcess);

	if (!Ppl.isEmpty() && !Kph.isEmpty())
		return Ppl + " / " + Kph;

	return Ppl.isEmpty() ? Kph : Ppl;
}

//
// The mitigations an image asked for in its own header, which is a shorter
// list than what a running process ends up with.
//
QString GetMitigationsString(const CModulePtr& pModule)
{
	if (pModule.isNull())
		return QString();

	const quint32 Flags = pModule->GetMitigationFlags();

	QStringList Mitigations;
	if (Flags & CModuleInfo::eImageAslr)	Mitigations.append(CTaskExplorer::tr("ASLR"));
	if (Flags & CModuleInfo::eImageCfg)		Mitigations.append(CTaskExplorer::tr("CFG"));
	if (Flags & CModuleInfo::eImageCet)		Mitigations.append(CTaskExplorer::tr("CET"));

	return Mitigations.join(CTaskExplorer::tr(", "));
}

//
// ---- tokens ----
//

//
// Groups and privileges both say the same four things: whether they are on, and
// whether that differs from how the token was handed out. "Modified" is the
// part worth seeing - it means something changed the token after the fact.
//
QString GetPrivilegeAttributesString(quint32 Attributes)
{
	const bool bEnabled = (Attributes & CTokenInfo::ePrivilegeEnabled) != 0;
	const bool bDefault = (Attributes & CTokenInfo::ePrivilegeEnabledByDefault) != 0;

	if (bEnabled)
		return bDefault ? CTaskExplorer::tr("Enabled") : CTaskExplorer::tr("Enabled (modified)");

	return bDefault ? CTaskExplorer::tr("Disabled (modified)") : CTaskExplorer::tr("Disabled");
}

QString GetGroupStatusString(quint32 Attributes, bool bRestricted)
{
	QString Str;

	if ((Attributes & (CTokenInfo::eGroupIntegrity | CTokenInfo::eGroupIntegrityEnabled)) != 0)
	{
		//
		// An integrity group is on because of the level it stands for, so it is
		// never "modified" and a disabled one has nothing to say.
		//
		if (Attributes & CTokenInfo::eGroupEnabled)
			Str = CTaskExplorer::tr("Enabled (as a group)");
	}
	else
	{
		const bool bEnabled = (Attributes & CTokenInfo::eGroupEnabled) != 0;
		const bool bDefault = (Attributes & CTokenInfo::eGroupEnabledByDefault) != 0;

		if (bEnabled)
			Str = bDefault ? CTaskExplorer::tr("Enabled") : CTaskExplorer::tr("Enabled (modified)");
		else
			Str = bDefault ? CTaskExplorer::tr("Disabled (modified)") : CTaskExplorer::tr("Disabled");
	}

	if (bRestricted && !Str.isEmpty())
		Str += CTaskExplorer::tr(" (restricted)");

	return Str;
}

//
// What a group is for, as opposed to whether it is on. A group can be several
// of these at once.
//
QString GetGroupDescription(quint32 Attributes)
{
	static const struct { quint32 Mask; const char* Name; } Entries[] =
	{
		{ CTokenInfo::eGroupIntegrity | CTokenInfo::eGroupIntegrityEnabled,	QT_TRANSLATE_NOOP("CTaskExplorer", "Integrity") },
		{ CTokenInfo::eGroupLogonId,										QT_TRANSLATE_NOOP("CTaskExplorer", "Logon Id") },
		{ CTokenInfo::eGroupOwner,											QT_TRANSLATE_NOOP("CTaskExplorer", "Owner") },
		{ CTokenInfo::eGroupMandatory,										QT_TRANSLATE_NOOP("CTaskExplorer", "Mandatory") },
		{ CTokenInfo::eGroupUseForDenyOnly,									QT_TRANSLATE_NOOP("CTaskExplorer", "Use for deny only") },
		{ CTokenInfo::eGroupResource,										QT_TRANSLATE_NOOP("CTaskExplorer", "Resource") },
	};

	QStringList Names;
	for (size_t i = 0; i < sizeof(Entries) / sizeof(Entries[0]); i++)
	{
		if ((Attributes & Entries[i].Mask) == Entries[i].Mask)
			Names.append(CTaskExplorer::tr(Entries[i].Name));
	}
	return Names.join(CTaskExplorer::tr(", "));
}

QString GetSecurityAttributeTypeString(quint16 Type)
{
	switch (Type)
	{
	case CTokenInfo::eSecAttrInvalid:		return CTaskExplorer::tr("Invalid");
	case CTokenInfo::eSecAttrInt64:			return CTaskExplorer::tr("Int64");
	case CTokenInfo::eSecAttrUInt64:		return CTaskExplorer::tr("UInt64");
	case CTokenInfo::eSecAttrString:		return CTaskExplorer::tr("String");
	case CTokenInfo::eSecAttrFqbn:			return CTaskExplorer::tr("FQBN");
	case CTokenInfo::eSecAttrSid:			return CTaskExplorer::tr("SID");
	case CTokenInfo::eSecAttrBoolean:		return CTaskExplorer::tr("Boolean");
	case CTokenInfo::eSecAttrOctetString:	return CTaskExplorer::tr("Octet string");
	}
	return CTaskExplorer::tr("(Unknown)");
}

QString GetSecurityAttributeFlagsString(quint32 Flags)
{
	QStringList Names;
	if (Flags & CTokenInfo::eSecAttrMandatory)			Names.append(CTaskExplorer::tr("Mandatory"));
	if (Flags & CTokenInfo::eSecAttrDisabled)			Names.append(CTaskExplorer::tr("Disabled"));
	if (Flags & CTokenInfo::eSecAttrDisabledByDefault)	Names.append(CTaskExplorer::tr("Default disabled"));
	if (Flags & CTokenInfo::eSecAttrUseForDenyOnly)		Names.append(CTaskExplorer::tr("Use for deny only"));
	if (Flags & CTokenInfo::eSecAttrValueCaseSensitive)	Names.append(CTaskExplorer::tr("Case-sensitive"));
	if (Flags & CTokenInfo::eSecAttrNonInheritable)		Names.append(CTaskExplorer::tr("Non-inheritable"));
	if (Flags & CTokenInfo::eSecAttrCompareIgnore)		Names.append(CTaskExplorer::tr("Compare-ignore"));

	return Names.isEmpty() ? CTaskExplorer::tr("(None)") : Names.join(CTaskExplorer::tr(", "));
}

//
// Whether this is the elevated half of a split token. A token that was never
// split has no answer to give, which is not the same as "not elevated".
//
QString GetElevationString(const CTokenInfoPtr& pToken)
{
	if (pToken.isNull())
		return QString();

	switch (pToken->GetElevationType())
	{
	case CTokenInfo::eElevationFull:	return CTaskExplorer::tr("Yes");
	case CTokenInfo::eElevationLimited:	return CTaskExplorer::tr("No");
	}
	return CTaskExplorer::tr("N/A");
}

//
// The mandatory integrity level, named from the RID that stands for it. Levels
// between the named ones do exist, so an unrecognised one is reported as the
// number it is rather than swallowed.
//
QString GetIntegrityString(const CTokenInfoPtr& pToken)
{
	if (pToken.isNull())
		return QString();

	const quint32 Level = pToken->GetIntegrityLevel();
	switch (Level)
	{
	case CTokenInfo::eIntegrityUntrusted:	return CTaskExplorer::tr("Untrusted");
	case CTokenInfo::eIntegrityLow:			return CTaskExplorer::tr("Low");
	case CTokenInfo::eIntegrityMedium:		return CTaskExplorer::tr("Medium");
	case CTokenInfo::eIntegrityMediumPlus:	return CTaskExplorer::tr("Medium +");
	case CTokenInfo::eIntegrityHigh:		return CTaskExplorer::tr("High");
	case CTokenInfo::eIntegritySystem:		return CTaskExplorer::tr("System");
	case CTokenInfo::eIntegrityProtected:	return CTaskExplorer::tr("Protected");
	}

	if (Level == (quint32)-1)
		return QString();

	return CTaskExplorer::tr("Other (%1)").arg(Level);
}

//
// UAC file and registry virtualization: whether a process may redirect writes
// to protected locations, and whether it currently is.
//
QString GetVirtualizationString(const CTokenInfoPtr& pToken)
{
	if (pToken.isNull())
		return QString();

	if (pToken->IsVirtualizationEnabled())
		return CTaskExplorer::tr("Virtualized");
	if (pToken->IsVirtualizationAllowed())
		return CTaskExplorer::tr("Allowed");
	return CTaskExplorer::tr("Not allowed");
}

//
// ---- handles ----
//

//
// A handle's type, with the narrower one the platform gave in brackets when
// there is one - "File (Named pipe)".
//
QString GetHandleTypeString(const CHandlePtr& pHandle)
{
	if (pHandle.isNull())
		return QString();

	//
	// Qualified where the machine classifies handles in more than one way at
	// once. A Wine process's list holds both its descriptors and wineserver's
	// objects, and both tables use the word File for entirely different things:
	// one is a path the kernel gave out, the other an NT name inside the prefix.
	//
	CSystemPtr pSystem = CCluster::GetViewSystem();
	CSystemAPI::SHandleType Type;
	Type.Name = pHandle->GetTypeName();
	Type.Index = (int)pHandle->GetTypeIndex();
	Type.Group = pSystem.isNull() ? CSystemAPI::eHandleGroupNative : pSystem->GetHandleTypeGroup(Type.Index);

	const QString Name = GetHandleTypeLabel(Type);

	const QString SubType = pHandle->GetSubTypeName();
	return SubType.isEmpty() ? Name : Name + " (" + SubType + ")";
}

//
// What a descriptor was opened for, on a target that describes that with
// open(2) flags.
//
static QString OpenFlagsToString(quint32 Flags)
{
	QStringList Parts;

	//
	// O_PATH is checked first because it changes what the others mean: such a
	// descriptor refers to a location in the filesystem and cannot read or
	// write at all, yet it has O_RDONLY's value of 0 in the access-mode field.
	//
	if (Flags & CHandleInfo::eOpenPath)
	{
		Parts.append(CTaskExplorer::tr("Path only"));
	}
	else
	{
		//
		// The access mode is a two-bit field rather than a set of flags, so it
		// is switched on rather than tested.
		//
		switch (Flags & CHandleInfo::eOpenAccessMask)
		{
		case CHandleInfo::eOpenReadOnly:	Parts.append(CTaskExplorer::tr("Read")); break;
		case CHandleInfo::eOpenWriteOnly:	Parts.append(CTaskExplorer::tr("Write")); break;
		case CHandleInfo::eOpenReadWrite:	Parts.append(CTaskExplorer::tr("Read/Write")); break;
		}
	}

	//
	// The remaining file status flags, in the order most likely to matter when
	// looking at what a process has open. Flags that only affected the original
	// open() call and are not retained by the kernel (O_CREAT, O_EXCL, O_TRUNC,
	// O_NOCTTY, O_NOFOLLOW) never appear in fdinfo, so they are not listed.
	//
	static const struct { quint32 Flag; const char* Name; } Entries[] =
	{
		{ CHandleInfo::eOpenAppend,			QT_TRANSLATE_NOOP("CTaskExplorer", "Append")		},
		{ CHandleInfo::eOpenNonBlock,		QT_TRANSLATE_NOOP("CTaskExplorer", "Non-blocking")	},
		{ CHandleInfo::eOpenDirect,			QT_TRANSLATE_NOOP("CTaskExplorer", "Direct")		},	// bypasses the page cache
		{ CHandleInfo::eOpenSync,			QT_TRANSLATE_NOOP("CTaskExplorer", "Sync")			},	// implies O_DSYNC, so test it first
		{ CHandleInfo::eOpenDSync,			QT_TRANSLATE_NOOP("CTaskExplorer", "Data sync")		},
		{ CHandleInfo::eOpenAsync,			QT_TRANSLATE_NOOP("CTaskExplorer", "Async")			},	// SIGIO on readiness
		{ CHandleInfo::eOpenNoAtime,		QT_TRANSLATE_NOOP("CTaskExplorer", "No atime")		},
		{ CHandleInfo::eOpenDirectory,		QT_TRANSLATE_NOOP("CTaskExplorer", "Directory")		},
		{ CHandleInfo::eOpenCloseOnExec,	QT_TRANSLATE_NOOP("CTaskExplorer", "Close on exec")	},
	};

	for (size_t i = 0; i < sizeof(Entries) / sizeof(Entries[0]); i++)
	{
		//
		// O_SYNC is O_DSYNC|__O_SYNC, so a plain bit test would report
		// "Sync, Data sync" for a single flag. Matching the whole value avoids
		// that for any such composite.
		//
		if ((Flags & Entries[i].Flag) != Entries[i].Flag)
			continue;

		if (Entries[i].Flag == CHandleInfo::eOpenDSync && (Flags & CHandleInfo::eOpenSync) == CHandleInfo::eOpenSync)
			continue;

		Parts.append(CTaskExplorer::tr(Entries[i].Name));
	}

	return Parts.join(CTaskExplorer::tr(", "));
}

//
// What a handle is allowed to do. The two platforms describe this in genuinely
// different terms - an access mask against the object's type, or the flags a
// descriptor was opened with - so the target decides which is read.
//
QString GetGrantedAccessString(const CHandlePtr& pHandle)
{
	if (pHandle.isNull())
		return QString();

	return IsWindowsTarget(pHandle.data())
		? GetAccessRightsString(pHandle->GetGrantedAccessRights())
		: OpenFlagsToString(pHandle->GetOpenFlags());
}

//
// The same permission read coarsely, through the object type's generic
// mapping. A type with no mapping to apply has nothing to say.
//
QString GetGenericAccessString(const CHandlePtr& pHandle)
{
	if (pHandle.isNull())
		return QString();

	const quint32 Access = pHandle->GetGenericAccess();

	QStringList Rights;
	if (Access & CHandleInfo::eGenericRead)		Rights.append(CTaskExplorer::tr("Read"));
	if (Access & CHandleInfo::eGenericWrite)	Rights.append(CTaskExplorer::tr("Write"));
	if (Access & CHandleInfo::eGenericExecute)	Rights.append(CTaskExplorer::tr("Execute"));
	if (Access & CHandleInfo::eGenericAll)		Rights.append(CTaskExplorer::tr("All"));

	return Rights.isEmpty() ? CTaskExplorer::tr("N/A") : Rights.join(CTaskExplorer::tr(", "));
}

QString GetHandleAttributesString(const CHandlePtr& pHandle)
{
	if (pHandle.isNull())
		return QString();

	const quint32 Attributes = pHandle->GetAttributes();

	QStringList Names;
	if (Attributes & CHandleInfo::eObjProtectClose)	Names.append(CTaskExplorer::tr("Protected"));
	if (Attributes & CHandleInfo::eObjInherit)		Names.append(CTaskExplorer::tr("Inherit"));

	return Names.join(CTaskExplorer::tr(", "));
}

//
// What a file handle left open to others, as a three-slot mnemonic so the
// column stays narrow and every row lines up.
//
QString GetFileShareAccessString(const CHandlePtr& pHandle)
{
	if (pHandle.isNull())
		return QString();

	const quint32 Flags = pHandle->GetFileFlags();
	if (!(Flags & CHandleInfo::eShareMask))
		return "---";

	QString Str = "---";
	if (Flags & CHandleInfo::eShareRead)	Str[0] = 'R';
	if (Flags & CHandleInfo::eShareWrite)	Str[1] = 'W';
	if (Flags & CHandleInfo::eShareDelete)	Str[2] = 'D';
	return Str;
}

//
// What a section is backed by. The attributes are not exclusive in principle,
// so the first that applies is the one that names it.
//
QString GetSectionTypeString(quint32 Attributes)
{
	if (Attributes & CHandleInfo::eSecCommit)	return CTaskExplorer::tr("Commit");
	if (Attributes & CHandleInfo::eSecFile)		return CTaskExplorer::tr("File");
	if (Attributes & CHandleInfo::eSecImage)	return CTaskExplorer::tr("Module");
	if (Attributes & CHandleInfo::eSecReserve)	return CTaskExplorer::tr("Reserve");
	return CTaskExplorer::tr("Unknown");
}

//
// How an ALPC port was set up. Anything left over is shown as the bits it is,
// so a flag this build does not know about is visible rather than dropped.
//
QString GetAlpcPortFlagsString(quint32 Flags)
{
	static const struct { quint32 Flag; const char* Name; } Entries[] =
	{
		{ CHandleInfo::eAlpcLpcMode,				QT_TRANSLATE_NOOP("CTaskExplorer", "LPC mode")						},
		{ CHandleInfo::eAlpcAllowImpersonation,		QT_TRANSLATE_NOOP("CTaskExplorer", "Allow impersonation")			},
		{ CHandleInfo::eAlpcAllowLpcRequests,		QT_TRANSLATE_NOOP("CTaskExplorer", "Allow LPC requests")			},
		{ CHandleInfo::eAlpcWaitablePort,			QT_TRANSLATE_NOOP("CTaskExplorer", "Waitable")						},
		{ CHandleInfo::eAlpcAllowDupObject,			QT_TRANSLATE_NOOP("CTaskExplorer", "Allow object duplication")		},
		{ CHandleInfo::eAlpcSystemProcess,			QT_TRANSLATE_NOOP("CTaskExplorer", "System process only")			},
		{ CHandleInfo::eAlpcWakePolicy1,			QT_TRANSLATE_NOOP("CTaskExplorer", "Wake policy (1)")				},
		{ CHandleInfo::eAlpcWakePolicy2,			QT_TRANSLATE_NOOP("CTaskExplorer", "Wake policy (2)")				},
		{ CHandleInfo::eAlpcWakePolicy3,			QT_TRANSLATE_NOOP("CTaskExplorer", "Wake policy (3)")				},
		{ CHandleInfo::eAlpcDirectMessage,			QT_TRANSLATE_NOOP("CTaskExplorer", "No shared section (direct)")	},
		{ CHandleInfo::eAlpcAllowMultiHandleAttr,	QT_TRANSLATE_NOOP("CTaskExplorer", "Allow multi-handle attributes")	},
	};

	quint32 Remaining = Flags;
	QStringList Names;
	for (size_t i = 0; i < sizeof(Entries) / sizeof(Entries[0]); i++)
	{
		if (Remaining & Entries[i].Flag)
		{
			Names.append(CTaskExplorer::tr(Entries[i].Name));
			Remaining &= ~Entries[i].Flag;
		}
	}

	if (Remaining)
		Names.append(CTaskExplorer::tr("UNKNOWN: %1").arg(QString("%1").arg(Remaining, 8, 16, QChar('0'))));

	return Names.join(CTaskExplorer::tr(", "));
}

//
// ---- GDI objects ----
//
// GDI is a Windows notion with no counterpart elsewhere, and the type is read
// out of the handle by the Windows backend. The *wording* is built here all the
// same, on every platform, because the machine being described is not
// necessarily the machine describing it - a Linux viewer watching a Windows box
// has the numbers and needs the words for them.
//

QString GetGdiTypeString(const CGdiPtr& pGDI)
{
	if (pGDI.isNull())
		return QString();

	switch (pGDI->GetGdiType())
	{
	case CGdiInfo::eGdiAltDc:		return CTaskExplorer::tr("Alt. DC");
	case CGdiInfo::eGdiBitmap:		return CTaskExplorer::tr("Bitmap");
	case CGdiInfo::eGdiBrush:		return CTaskExplorer::tr("Brush");
	case CGdiInfo::eGdiClientObj:	return CTaskExplorer::tr("Client Object");
	case CGdiInfo::eGdiDibSection:	return CTaskExplorer::tr("DIB Section");
	case CGdiInfo::eGdiDc:			return CTaskExplorer::tr("DC");
	case CGdiInfo::eGdiExtPen:		return CTaskExplorer::tr("ExtPen");
	case CGdiInfo::eGdiFont:			return CTaskExplorer::tr("Font");
	case CGdiInfo::eGdiMetaDc16:		return CTaskExplorer::tr("Metafile DC");
	case CGdiInfo::eGdiMetafile:		return CTaskExplorer::tr("Enhanced Metafile");
	case CGdiInfo::eGdiMetafile16:	return CTaskExplorer::tr("Metafile");
	case CGdiInfo::eGdiPalette:		return CTaskExplorer::tr("Palette");
	case CGdiInfo::eGdiPen:			return CTaskExplorer::tr("Pen");
	case CGdiInfo::eGdiRegion:		return CTaskExplorer::tr("Region");
	}
	return CTaskExplorer::tr("Unknown");
}


//
// ---- GPU engines ----
//

//
// What a GPU engine is for. A driver that classified an engine as "other" gave
// it a name of its own, and a driver that reported nothing leaves the node to
// be named by its index.
//
QString GetGpuNodeString(const CGpuMonitor::SGpuNode& Node)
{
	switch (Node.EngineType)
	{
	case CGpuMonitor::eEngineOther:				return Node.FriendlyName;
	case CGpuMonitor::eEngine3D:				return CTaskExplorer::tr("3D");
	case CGpuMonitor::eEngineVideoDecode:		return CTaskExplorer::tr("Video Decode");
	case CGpuMonitor::eEngineVideoEncode:		return CTaskExplorer::tr("Video Encode");
	case CGpuMonitor::eEngineVideoProcessing:	return CTaskExplorer::tr("Video Processing");
	case CGpuMonitor::eEngineSceneAssembly:		return CTaskExplorer::tr("Scene Assembly");
	case CGpuMonitor::eEngineCopy:				return CTaskExplorer::tr("Copy");
	case CGpuMonitor::eEngineOverlay:			return CTaskExplorer::tr("Overlay");
	case CGpuMonitor::eEngineCrypto:			return CTaskExplorer::tr("Crypto");
	case CGpuMonitor::eEngineUnknown:			return CTaskExplorer::tr("Node: %1").arg(Node.Index);
	}
	return CTaskExplorer::tr("ERROR (%1)").arg(Node.EngineType);
}

//
// ---- threads: COM apartments and the last system call ----
//

QString GetApartmentTypeString(const CThreadPtr& pThread)
{
	if (pThread.isNull())
		return QString();

	QString Type;

	//
	// A thread in the neutral apartment is there as well as in its own, so the
	// two are read together.
	//
	if (pThread->IsInNeutralApartment())
		Type += CTaskExplorer::tr("NTA on ");

	switch (pThread->GetApartmentType())
	{
	case CThreadInfo::eApartmentNone:			return QString();
	case CThreadInfo::eApartmentSta:			Type += CTaskExplorer::tr("STA");			break;
	case CThreadInfo::eApartmentMainSta:		Type += CTaskExplorer::tr("Main STA");		break;
	case CThreadInfo::eApartmentApplicationSta:	Type += CTaskExplorer::tr("ASTA");			break;
	case CThreadInfo::eApartmentMta:			Type += CTaskExplorer::tr("MTA");			break;
	case CThreadInfo::eApartmentImplicitMta:	Type += CTaskExplorer::tr("Implicit MTA");	break;
	}

	//
	// Nested initializations are worth showing because an unbalanced one keeps
	// the apartment alive longer than its owner expects.
	//
	const quint32 Inits = pThread->GetComInitCount();
	if (Inits > 1)
		Type += CTaskExplorer::tr(" (x%1)").arg(Inits);

	return Type;
}

//
// The apartment's own flags. Several have no published meaning beyond their
// name, so those are spelled as they are rather than paraphrased.
//
QString GetApartmentFlagsString(const CThreadPtr& pThread)
{
	if (pThread.isNull())
		return QString();

	static const struct { quint32 Flag; const char* Name; } Entries[] =
	{
		{ CThreadInfo::eOleLocalTid,					QT_TRANSLATE_NOOP("CTaskExplorer", "Local TID")						},
		{ CThreadInfo::eOleUuidInitialized,				QT_TRANSLATE_NOOP("CTaskExplorer", "UUID initialized")				},
		{ CThreadInfo::eOleInThreadDetach,				QT_TRANSLATE_NOOP("CTaskExplorer", "Inside thread detach")			},
		{ CThreadInfo::eOleChannelThreadInitialized,	QT_TRANSLATE_NOOP("CTaskExplorer", "Channel thread initialized")	},
		{ CThreadInfo::eOleWowThread,					QT_TRANSLATE_NOOP("CTaskExplorer", "WOW Thread")					},
		{ CThreadInfo::eOleThreadUninitializing,		QT_TRANSLATE_NOOP("CTaskExplorer", "Thread Uninitializing")			},
		{ CThreadInfo::eOleDisableOle1Dde,				QT_TRANSLATE_NOOP("CTaskExplorer", "OLE1DDE disabled")				},
		{ CThreadInfo::eOleApartmentThreaded,			QT_TRANSLATE_NOOP("CTaskExplorer", "Single threaded (STA)")			},
		{ CThreadInfo::eOleMultiThreaded,				QT_TRANSLATE_NOOP("CTaskExplorer", "Multi threaded (MTA)")			},
		{ CThreadInfo::eOleImpersonating,				QT_TRANSLATE_NOOP("CTaskExplorer", "Impersonating")					},
		{ CThreadInfo::eOleDisableEventLogger,			QT_TRANSLATE_NOOP("CTaskExplorer", "Eventlogger disabled")			},
		{ CThreadInfo::eOleInNeutralApt,				QT_TRANSLATE_NOOP("CTaskExplorer", "Neutral threaded (NTA)")		},
		{ CThreadInfo::eOleDispatchThread,				QT_TRANSLATE_NOOP("CTaskExplorer", "Dispatch thread")				},
		{ CThreadInfo::eOleHostThread,					QT_TRANSLATE_NOOP("CTaskExplorer", "HOSTTHREAD")					},
		{ CThreadInfo::eOleAllowCoInit,					QT_TRANSLATE_NOOP("CTaskExplorer", "ALLOWCOINIT")					},
		{ CThreadInfo::eOlePendingUninit,				QT_TRANSLATE_NOOP("CTaskExplorer", "PENDINGUNINIT")					},
		{ CThreadInfo::eOleFirstMtaInit,				QT_TRANSLATE_NOOP("CTaskExplorer", "FIRSTMTAINIT")					},
		{ CThreadInfo::eOleFirstNtaInit,				QT_TRANSLATE_NOOP("CTaskExplorer", "FIRSTNTAINIT")					},
		{ CThreadInfo::eOleAptInitializing,				QT_TRANSLATE_NOOP("CTaskExplorer", "APTIN INITIALIZING")			},
		{ CThreadInfo::eOleUiMsgsInModalLoop,			QT_TRANSLATE_NOOP("CTaskExplorer", "UIMSGS IN MODAL LOOP")			},
		{ CThreadInfo::eOleMarshalingErrorObject,		QT_TRANSLATE_NOOP("CTaskExplorer", "Marshaling error object")		},
		{ CThreadInfo::eOleWinRtInitialize,				QT_TRANSLATE_NOOP("CTaskExplorer", "WinRT initialized")				},
		{ CThreadInfo::eOleApplicationSta,				QT_TRANSLATE_NOOP("CTaskExplorer", "ApplicationSTA")				},
		{ CThreadInfo::eOleInShutdownCallbacks,			QT_TRANSLATE_NOOP("CTaskExplorer", "IN_SHUTDOWN_CALLBACKS")			},
		{ CThreadInfo::eOlePointerInputBlocked,			QT_TRANSLATE_NOOP("CTaskExplorer", "POINTER_INPUT_BLOCKED")			},
		{ CThreadInfo::eOleInActivationFilter,			QT_TRANSLATE_NOOP("CTaskExplorer", "IN_ACTIVATION_FILTER")			},
		{ CThreadInfo::eOleAstaToAstaExemptQuirk,		QT_TRANSLATE_NOOP("CTaskExplorer", "ASTATOASTAEXEMPT_QUIRK")		},
		{ CThreadInfo::eOleAstaToAstaExemptProxy,		QT_TRANSLATE_NOOP("CTaskExplorer", "ASTATOASTAEXEMPT_PROXY")		},
		{ CThreadInfo::eOleAstaToAstaExemptIndoubt,		QT_TRANSLATE_NOOP("CTaskExplorer", "ASTATOASTAEXEMPT_INDOUBT")		},
		{ CThreadInfo::eOleDetectedUserInitialized,		QT_TRANSLATE_NOOP("CTaskExplorer", "DETECTED_USER_INITIALIZED")		},
		{ CThreadInfo::eOleBridgeSta,					QT_TRANSLATE_NOOP("CTaskExplorer", "BRIDGE_STA")					},
		{ CThreadInfo::eOleNaInitializing,				QT_TRANSLATE_NOOP("CTaskExplorer", "NA_INITIALIZING")				},
	};

	const quint32 Flags = pThread->GetApartmentFlags();

	QStringList Names;
	for (size_t i = 0; i < sizeof(Entries) / sizeof(Entries[0]); i++)
	{
		if (Flags & Entries[i].Flag)
			Names.append(CTaskExplorer::tr(Entries[i].Name));
	}
	return Names.join(CTaskExplorer::tr(", "));
}

//
// The call a thread is sitting in, with its number, its first argument, and
// how long it has been there.
//
QString GetLastSysCallInfoString(const CThreadPtr& pThread)
{
	if (pThread.isNull() || !pThread->HasLastSysCall())
		return QString();

	const quint32 Number = pThread->GetLastSysCallNumber();
	const QString Name = pThread->GetLastSysCallName();

	QString Info = Name.isEmpty()
		? CTaskExplorer::tr("0x%1").arg(QString::number(Number, 16))
		: CTaskExplorer::tr("%1 (0x%2)").arg(Name).arg(QString::number(Number, 16));

	Info += CTaskExplorer::tr(" (Arg0: 0x%1)").arg(QString::number(pThread->GetLastSysCallArgument(), 16));

	const quint64 WaitTime = pThread->GetLastSysCallWaitTime();
	if (WaitTime)
		Info += CTaskExplorer::tr(" - %1").arg(FormatTime(WaitTime, true));

	return Info;
}

//
// ---- memory: shared original pages ----
//

//
// How much of a mapped region is still what was mapped in, with the number of
// pages that have been copied since alongside. Only mapped, committed regions
// have anything to say.
//
QString GetOriginalPagesString(const CMemoryPtr& pMemory)
{
	if (pMemory.isNull() || !pMemory->IsMapped() || !(pMemory->GetState() & CMemoryInfo::eMemCommit))
		return QString();

	const quint32 PageSize = pMemory->GetPageSize();
	if (!PageSize)
		return QString();

	const quint64 Total = pMemory->GetRegionSize() / PageSize;
	if (!Total)
		return QString();

	const quint64 Shared = pMemory->GetSharedOriginalPages();
	const quint64 Modified = Total > Shared ? Total - Shared : 0;

	QString Result = CTaskExplorer::tr("%1%").arg((double)Shared * 100 / (double)Total, 0, 'f', 2);
	if (Modified)
		Result += CTaskExplorer::tr(" (%1)").arg(Modified);
	return Result;
}

//
// ---- DNS ----
//

//
// Record types are named by their RFC mnemonics, which are the same everywhere
// and are not translated.
//
QString GetDnsTypeString(quint16 Type)
{
	switch (Type)
	{
	case CDnsCacheEntry::eDnsA:		return "A";
	case CDnsCacheEntry::eDnsNs:	return "NS";
	case CDnsCacheEntry::eDnsCname:	return "CNAME";
	case CDnsCacheEntry::eDnsPtr:	return "PTR";
	case CDnsCacheEntry::eDnsMx:	return "MX";
	case CDnsCacheEntry::eDnsText:	return "TXT";
	case CDnsCacheEntry::eDnsAaaa:	return "AAAA";
	case CDnsCacheEntry::eDnsSrv:	return "SRV";
	}
	return CTaskExplorer::tr("UNKNOWN (%1)").arg(Type);
}

QString GetDnsTypeString(const CDnsCacheEntryPtr& pEntry)
{
	return pEntry.isNull() ? QString() : GetDnsTypeString(pEntry->GetType());
}

//
// ---- Sandboxie ----
//
// Sandboxie is a Windows product, so like GDI its decoders only exist where
// its backend does.
//

#ifdef WIN32

//
// Which program Sandboxie recognised a boxed process as. It matters because
// the box applies a different set of accommodations to each.
//
QString GetSbieImageTypeString(quint32 Type)
{
	switch (Type)
	{
	case CSandboxieAPI::eImageUnspecified:			return CTaskExplorer::tr("Generic");
	case CSandboxieAPI::eImageSandboxieRpcSs:		return CTaskExplorer::tr("Sbie RpcSs");
	case CSandboxieAPI::eImageSandboxieDcomLaunch:	return CTaskExplorer::tr("Sbie DcomLaunch");
	case CSandboxieAPI::eImageSandboxieCrypto:		return CTaskExplorer::tr("Sbie Crypto");
	case CSandboxieAPI::eImageSandboxieWuAu:		return CTaskExplorer::tr("Sbie WuAu Svc");
	case CSandboxieAPI::eImageSandboxieBits:		return CTaskExplorer::tr("Sbie BITS");
	case CSandboxieAPI::eImageSandboxieSbieSvc:		return CTaskExplorer::tr("Sbie Svc");
	case CSandboxieAPI::eImageMsiInstaller:			return CTaskExplorer::tr("Msi Installer");
	case CSandboxieAPI::eImageTrustedInstaller:		return CTaskExplorer::tr("Trusted Installer");
	case CSandboxieAPI::eImageWuaucLt:				return CTaskExplorer::tr("Windows Update");
	case CSandboxieAPI::eImageShellExplorer:		return CTaskExplorer::tr("Windows Explorer");
	case CSandboxieAPI::eImageInternetExplorer:		return CTaskExplorer::tr("Internet Explorer");
	case CSandboxieAPI::eImageMozillaFirefox:		return CTaskExplorer::tr("Mozilla Firefox (or derivative)");
	case CSandboxieAPI::eImageWindowsMediaPlayer:	return CTaskExplorer::tr("Windows Media Player");
	case CSandboxieAPI::eImageNullsoftWinamp:		return CTaskExplorer::tr("WinAmp");
	case CSandboxieAPI::eImagePandoraKmPlayer:		return CTaskExplorer::tr("KM Player");
	case CSandboxieAPI::eImageWindowsLiveMail:		return CTaskExplorer::tr("Windows Live Mail");
	case CSandboxieAPI::eImageServiceModelReg:		return CTaskExplorer::tr("Service Model Reg");
	case CSandboxieAPI::eImageRunDll32:				return CTaskExplorer::tr("RunDll32");
	case CSandboxieAPI::eImageDllHost:				return CTaskExplorer::tr("DllHost");
	case CSandboxieAPI::eImageDllHostWinInetCache:	return CTaskExplorer::tr("DllHost (WinInet Cache)");
	case CSandboxieAPI::eImageWispTis:				return CTaskExplorer::tr("Windows Ink Services");
	case CSandboxieAPI::eImageGoogleChrome:			return CTaskExplorer::tr("Google Chrome (or derivative)");
	case CSandboxieAPI::eImageGoogleUpdate:			return CTaskExplorer::tr("Google Updater");
	case CSandboxieAPI::eImageAcrobatReader:		return CTaskExplorer::tr("Acrobat Reader");
	case CSandboxieAPI::eImageOfficeOutlook:		return CTaskExplorer::tr("MS Outlook");
	case CSandboxieAPI::eImageOfficeExcel:			return CTaskExplorer::tr("MS Excel");
	case CSandboxieAPI::eImageFlashPlayerSandbox:	return CTaskExplorer::tr("Flash Player");
	case CSandboxieAPI::eImagePluginContainer:		return CTaskExplorer::tr("Firefox plugin container");
	case CSandboxieAPI::eImageOtherWebBrowser:		return CTaskExplorer::tr("Generic Web Browser");
	case CSandboxieAPI::eImageOtherMailClient:		return CTaskExplorer::tr("Generic Mail Client");
	}
	return CTaskExplorer::tr("Unknown");
}

//
// What the box decided about the process. These are short labels for a single
// line, so they stay terse.
//
QString GetSbieImageFlagsString(quint32 Flags)
{
	static const struct { quint32 Flag; const char* Name; } Entries[] =
	{
		{ CSandboxieAPI::eSbieValidProcess,			QT_TRANSLATE_NOOP("CTaskExplorer", "Valid")					},
		{ CSandboxieAPI::eSbieForcedProcess,		QT_TRANSLATE_NOOP("CTaskExplorer", "Forced")				},
		{ CSandboxieAPI::eSbieProcessIsStartExe,	QT_TRANSLATE_NOOP("CTaskExplorer", "Is StartExe")			},
		{ CSandboxieAPI::eSbieParentWasStartExe,	QT_TRANSLATE_NOOP("CTaskExplorer", "Started by StartExe")	},
		{ CSandboxieAPI::eSbieImageFromSbieDir,		QT_TRANSLATE_NOOP("CTaskExplorer", "From Sbie Dir")			},
		{ CSandboxieAPI::eSbieImageFromSandbox,		QT_TRANSLATE_NOOP("CTaskExplorer", "Image from Box")		},
		{ CSandboxieAPI::eSbieDropRights,			QT_TRANSLATE_NOOP("CTaskExplorer", "Drop Rights")			},
		{ CSandboxieAPI::eSbieRightsDropped,		QT_TRANSLATE_NOOP("CTaskExplorer", "Rights Dropped")		},
		{ CSandboxieAPI::eSbieOpenAllWinClass,		QT_TRANSLATE_NOOP("CTaskExplorer", "Win Class Open")		},
		{ CSandboxieAPI::eSbieProcessInPcaJob,		QT_TRANSLATE_NOOP("CTaskExplorer", "In PAC Job")			},
		{ CSandboxieAPI::eSbieCreateConsoleHide,	QT_TRANSLATE_NOOP("CTaskExplorer", "Cons Hide")				},
		{ CSandboxieAPI::eSbieCreateConsoleShow,	QT_TRANSLATE_NOOP("CTaskExplorer", "Cons Show")				},
		{ CSandboxieAPI::eSbieProtectedProcess,		QT_TRANSLATE_NOOP("CTaskExplorer", "Protected")				},
		{ CSandboxieAPI::eSbieHostInjectProcess,	QT_TRANSLATE_NOOP("CTaskExplorer", "Host Inject")			},
	};

	QStringList Names;
	for (size_t i = 0; i < sizeof(Entries) / sizeof(Entries[0]); i++)
	{
		if (Flags & Entries[i].Flag)
			Names.append(CTaskExplorer::tr(Entries[i].Name));
	}
	return Names.join(CTaskExplorer::tr(", "));
}

#endif

//
// ---- the last of it ----
//

QString GetFirewallStatusString(const CSocketPtr& pSocket)
{
	if (pSocket.isNull())
		return QString();

	switch (pSocket->GetFirewallStatus())
	{
	case CSocketInfo::eFirewallAllowedNotRestricted:	return CTaskExplorer::tr("Allowed, not restricted");
	case CSocketInfo::eFirewallAllowedRestricted:		return CTaskExplorer::tr("Allowed, restricted");
	case CSocketInfo::eFirewallNotAllowedNotRestricted:	return CTaskExplorer::tr("Not allowed, not restricted");
	case CSocketInfo::eFirewallNotAllowedRestricted:	return CTaskExplorer::tr("Not allowed, restricted");
	}
	return QString();
}

//
// Which protocols a process has been seen using. Serving on TCP is the more
// specific fact, so it stands in for plain TCP rather than joining it.
//
QString GetNetworkUsageString(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QString();

	const quint32 Flags = pProcess->GetNetworkUsageFlags();

	QStringList Usage;
	if (Flags & NET_TYPE_PROTOCOL_TCP_SRV)
		Usage.append(CTaskExplorer::tr("TCP/Server"));
	else if (Flags & NET_TYPE_PROTOCOL_TCP)
		Usage.append(CTaskExplorer::tr("TCP"));
	if (Flags & NET_TYPE_PROTOCOL_UDP)
		Usage.append(CTaskExplorer::tr("UDP"));

	return Usage.join(CTaskExplorer::tr(", "));
}

//
// Signature verification, collapsed to the three answers that change what a
// reader would do about it. The individual failure reasons are available from
// GetVerifyResult() for anyone who needs them.
//
QString GetVerifyResultString(const CModulePtr& pModule)
{
	if (pModule.isNull())
		return QString();

	switch (pModule->GetVerifyResult())
	{
	case CModuleInfo::VrTrusted:		return CTaskExplorer::tr("Trusted");
	case CModuleInfo::VrNoSignature:	return CTaskExplorer::tr("Un signed");
	case CModuleInfo::VrExpired:
	case CModuleInfo::VrRevoked:
	case CModuleInfo::VrDistrust:
	case CModuleInfo::VrBadSignature:	return CTaskExplorer::tr("Not trusted");
	}
	return CTaskExplorer::tr("Unknown");
}

//
// How much of a loaded image still matches the file it came from, as a
// percentage. A target that could not measure it reports -1.
//
QString GetImageCoherencyString(const CModulePtr& pModule)
{
	if (pModule.isNull())
		return QString();

	const float Coherency = pModule->GetImageCoherency();
	return Coherency != -1 ? QString::number(Coherency * 100.0F, 'f', 2) : QString();
}

QString GetAffinityMaskString(const CAbstractTask* pTask)
{
	if (!pTask)
		return QString();
	return CTaskExplorer::tr("0x%1").arg(QString::number(pTask->GetAffinityMask(), 16).toUpper());
}

QString GetStackUsageString(const CThreadPtr& pThread)
{
	if (pThread.isNull())
		return QString();

	return CTaskExplorer::tr("%1/%2 (%3 %)")
		.arg(FormatSize(pThread->GetStackUsage()))
		.arg(FormatSize(pThread->GetStackLimit()))
		.arg((double)pThread->GetStackUsagePercent(), 0, 'f', 2);
}

//
// A pool tag is four characters the driver author chose, stored as the machine
// word they pack into.
//
QString GetPoolTagString(quint32 Tag)
{
	return QString::fromLatin1((const char*)&Tag, 4);
}

//
// ---- threads: what one is doing ----
//

//
// The kernel's own names for the states a thread can be in, and for the reasons
// a waiting one is waiting. They are proper nouns rather than sentences, so
// they are not translated - but they are written here, so a client can name a
// remote target's threads without the kernel that produced the numbers.
//
static const char* const g_ThreadStateNames[CThreadInfo::eThreadStateCount] =
{
	"Initialized", "Ready", "Running", "Standby", "Terminated",
	"Waiting", "Transition", "DeferredReady", "GateWait", "WaitingForProcessInSwap",
};

static const char* const g_WaitReasonNames[CThreadInfo::eWaitReasonCount] =
{
	"Executive", "FreePage", "PageIn", "PoolAllocation", "DelayExecution",
	"Suspended", "UserRequest", "WrExecutive", "WrFreePage", "WrPageIn",
	"WrPoolAllocation", "WrDelayExecution", "WrSuspended", "WrUserRequest", "WrEventPair",
	"WrQueue", "WrLpcReceive", "WrLpcReply", "WrVirtualMemory", "WrPageOut",
	"WrRendezvous", "WrKeyedEvent", "WrTerminated", "WrProcessInSwap", "WrCpuRateControl",
	"WrCalloutStack", "WrKernel", "WrResource", "WrPushLock", "WrMutex",
	"WrQuantumEnd", "WrDispatchInt", "WrPreempted", "WrYieldExecution", "WrFastMutex",
	"WrGuardedMutex", "WrRundown", "WrAlertByThreadId", "WrDeferredPreempt", "WrPhysicalFault",
	"WrIoRing", "WrMdlCache", "WrRcu",
};

//
// Linux describes the same thing with a single letter from /proc, and its
// vocabulary does not overlap with the kernel states above at all - so, as
// everywhere else here, the target decides which reading applies.
//
static QString LinuxThreadStateToString(int State)
{
	switch (State)
	{
	case 'R': return CTaskExplorer::tr("Running");
	case 'S': return CTaskExplorer::tr("Sleeping");
	case 'D': return CTaskExplorer::tr("Disk Sleep");
	case 'Z': return CTaskExplorer::tr("Zombie");
	case 'T': return CTaskExplorer::tr("Stopped");
	case 't': return CTaskExplorer::tr("Tracing Stop");
	case 'X':
	case 'x': return CTaskExplorer::tr("Dead");
	case 'K': return CTaskExplorer::tr("Wakekill");
	case 'W': return CTaskExplorer::tr("Waking");
	case 'P': return CTaskExplorer::tr("Parked");
	case 'I': return CTaskExplorer::tr("Idle");
	}
	return CTaskExplorer::tr("Unknown");
}

QString GetThreadStateString(const CThreadPtr& pThread)
{
	if (pThread.isNull())
		return QString();

	if (!IsWindowsTarget(pThread.data()))
		return LinuxThreadStateToString(pThread->GetState());

	const int State = pThread->GetState();

	QString Str;
	if (State != CThreadInfo::eThreadWaiting)
	{
		Str = State >= 0 && State < CThreadInfo::eThreadStateCount
			? QString::fromLatin1(g_ThreadStateNames[State]) : CTaskExplorer::tr("Unknown");
	}
	else
	{
		const int Reason = pThread->GetWaitReason();
		Str = Reason >= 0 && Reason < CThreadInfo::eWaitReasonCount
			? CTaskExplorer::tr("Wait:") + QString::fromLatin1(g_WaitReasonNames[Reason])
			: CTaskExplorer::tr("Waiting");

		//
		// A thread suspended more than once stays suspended until every resume
		// has been matched, which is worth seeing.
		//
		if (Reason == CThreadInfo::eWaitSuspended)
		{
			const quint32 Count = pThread->GetSuspendCount();
			if (Count)
				Str += CTaskExplorer::tr(" (%1)").arg(Count);
		}
	}

	return Str;
}

//
// Whether a thread carries an impersonation token. A sandboxed thread is asked
// differently, because there the interesting question is whether the box gave
// it one at all.
//
QString GetTokenStateString(const CThreadPtr& pThread)
{
	if (pThread.isNull())
		return QString();

	if (pThread->IsSandboxed())
		return pThread->HasSandboxToken() ? CTaskExplorer::tr("Yes") : QString();

	switch (pThread->GetTokenState())
	{
	case CThreadInfo::eTokenStateAnonymous:	return CTaskExplorer::tr("Anonymous");
	case CThreadInfo::eTokenStatePresent:	return CTaskExplorer::tr("Yes");
	}
	return QString();
}

QString GetThreadTypeString(const CThreadPtr& pThread)
{
	if (pThread.isNull())
		return QString();

	if (pThread->IsMainThread())
		return CTaskExplorer::tr("Main");

	return pThread->IsGuiThread() ? CTaskExplorer::tr("GUI") : CTaskExplorer::tr("Normal");
}

//
// The status a thread's last call returned. Nothing is shown for success -
// which is the ordinary case and not news.
//
QString GetLastSysCallStatusString(const CThreadPtr& pThread)
{
	if (pThread.isNull() || !pThread->HasLastStatus())
		return QString();

	QString Info = CTaskExplorer::tr("0x%1").arg(QString::number(pThread->GetLastStatusValue(), 16));

	const QString Message = pThread->GetLastStatusMessage();
	if (!Message.isEmpty())
		Info += CTaskExplorer::tr(" (%1)").arg(Message);

	return Info;
}

//
// ---- what the core's structures carry as codes ----
//

//
// What kind of thing a SID names.
//
QString GetSidTypeString(quint8 Use)
{
	switch (Use)
	{
	case CTokenInfo::eSidNotResolved:		return QString();
	case CTokenInfo::eSidUser:				return CTaskExplorer::tr("User");
	case CTokenInfo::eSidGroup:				return CTaskExplorer::tr("Group");
	case CTokenInfo::eSidDomain:			return CTaskExplorer::tr("Domain");
	case CTokenInfo::eSidAlias:				return CTaskExplorer::tr("Alias");
	case CTokenInfo::eSidWellKnownGroup:	return CTaskExplorer::tr("Well Known Group");
	case CTokenInfo::eSidDeletedAccount:	return CTaskExplorer::tr("Deleted Account");
	case CTokenInfo::eSidInvalid:			return CTaskExplorer::tr("Invalid");
	case CTokenInfo::eSidComputer:			return CTaskExplorer::tr("Computer");
	case CTokenInfo::eSidLabel:				return CTaskExplorer::tr("Label");
	case CTokenInfo::eSidLogonSession:		return CTaskExplorer::tr("Logon Session");
	}
	return CTaskExplorer::tr("Unknown");
}

QString GetTokenTypeString(quint8 Type)
{
	switch (Type)
	{
	case CTokenInfo::eTokenTypePrimary:			return CTaskExplorer::tr("Primary");
	case CTokenInfo::eTokenTypeImpersonation:	return CTaskExplorer::tr("Impersonation");
	}
	return QString();
}

//
// A primary token has no impersonation level, which is a different thing from
// having one that could not be read.
//
QString GetImpersonationLevelString(qint8 Level)
{
	switch (Level)
	{
	case CTokenInfo::eImpersonationNone:			return CTaskExplorer::tr("N/A");
	case CTokenInfo::eImpersonationAnonymous:		return CTaskExplorer::tr("Anonymous");
	case CTokenInfo::eImpersonationIdentification:	return CTaskExplorer::tr("Identification");
	case CTokenInfo::eImpersonationImpersonation:	return CTaskExplorer::tr("Impersonation");
	case CTokenInfo::eImpersonationDelegation:		return CTaskExplorer::tr("Delegation");
	}
	return QString();
}

QString GetAppContainerSidTypeString(quint8 Type)
{
	switch (Type)
	{
	case CTokenInfo::eAppContainerSidParent:	return CTaskExplorer::tr("Parent");
	case CTokenInfo::eAppContainerSidChild:		return CTaskExplorer::tr("Child");
	}
	return CTaskExplorer::tr("Unknown");
}

//
// What a login session is doing. The two platforms describe this with
// different vocabularies that happen to agree on "active", so both are read
// from the one enum rather than dispatched on the target.
//
QString GetSessionStateString(const CSystemAPI::SUser& User)
{
	switch (User.State)
	{
	case CSystemAPI::eSessionActive:		return CTaskExplorer::tr("Active");
	case CSystemAPI::eSessionConnected:		return CTaskExplorer::tr("Connected");
	case CSystemAPI::eSessionConnectQuery:	return CTaskExplorer::tr("Connect query");
	case CSystemAPI::eSessionShadow:		return CTaskExplorer::tr("Shadow");
	case CSystemAPI::eSessionDisconnected:	return CTaskExplorer::tr("Disconnected");
	case CSystemAPI::eSessionIdle:			return CTaskExplorer::tr("Idle");
	case CSystemAPI::eSessionListen:		return CTaskExplorer::tr("Listen");
	case CSystemAPI::eSessionReset:			return CTaskExplorer::tr("Reset");
	case CSystemAPI::eSessionDown:			return CTaskExplorer::tr("Down");
	case CSystemAPI::eSessionInit:			return CTaskExplorer::tr("Init");
	case CSystemAPI::eSessionOnline:		return CTaskExplorer::tr("Online");
	case CSystemAPI::eSessionClosing:		return CTaskExplorer::tr("Closing");
	}

	//
	// No state to report. Where the platform names seats, which one the session
	// sits at is the next most useful thing to say about it.
	//
	return User.Seat.isEmpty() ? CTaskExplorer::tr("Remote") : User.Seat;
}

//
// ---- job limits ----
//

//
// Which limit a row of a job's limit table is about. The second group are
// prohibitions rather than caps, and read as "limited" with nothing to show.
//
QString GetJobLimitName(int Which)
{
	switch (Which)
	{
	case CJobInfo::SJobLimit::eLimitActiveProcesses:		return CTaskExplorer::tr("Active processes");
	case CJobInfo::SJobLimit::eLimitAffinity:				return CTaskExplorer::tr("Affinity");
	case CJobInfo::SJobLimit::eLimitBreakawayOk:			return CTaskExplorer::tr("Breakaway OK");
	case CJobInfo::SJobLimit::eLimitDieOnUnhandledException:return CTaskExplorer::tr("Die on unhandled exception");
	case CJobInfo::SJobLimit::eLimitJobMemory:				return CTaskExplorer::tr("Job memory");
	case CJobInfo::SJobLimit::eLimitJobTime:				return CTaskExplorer::tr("Job time");
	case CJobInfo::SJobLimit::eLimitKillOnJobClose:			return CTaskExplorer::tr("Kill on job close");
	case CJobInfo::SJobLimit::eLimitPriorityClass:			return CTaskExplorer::tr("Priority class");
	case CJobInfo::SJobLimit::eLimitProcessMemory:			return CTaskExplorer::tr("Process memory");
	case CJobInfo::SJobLimit::eLimitProcessTime:			return CTaskExplorer::tr("Process time");
	case CJobInfo::SJobLimit::eLimitSchedulingClass:		return CTaskExplorer::tr("Scheduling class");
	case CJobInfo::SJobLimit::eLimitSilentBreakawayOk:		return CTaskExplorer::tr("Silent breakaway OK");
	case CJobInfo::SJobLimit::eLimitWorkingSetMinimum:		return CTaskExplorer::tr("Working set minimum");
	case CJobInfo::SJobLimit::eLimitWorkingSetMaximum:		return CTaskExplorer::tr("Working set maximum");

	case CJobInfo::SJobLimit::eLimitDesktop:				return CTaskExplorer::tr("Desktop limited");
	case CJobInfo::SJobLimit::eLimitDisplaySettings:		return CTaskExplorer::tr("Display settings limited");
	case CJobInfo::SJobLimit::eLimitExitWindows:			return CTaskExplorer::tr("Exit windows limited");
	case CJobInfo::SJobLimit::eLimitGlobalAtoms:			return CTaskExplorer::tr("Global atoms limited");
	case CJobInfo::SJobLimit::eLimitHandles:				return CTaskExplorer::tr("Handles limited");
	case CJobInfo::SJobLimit::eLimitReadClipboard:			return CTaskExplorer::tr("Read clipboard limited");
	case CJobInfo::SJobLimit::eLimitSystemParameters:		return CTaskExplorer::tr("System parameters limited");
	case CJobInfo::SJobLimit::eLimitWriteClipboard:			return CTaskExplorer::tr("Write clipboard limited");
	}
	return CTaskExplorer::tr("Unknown");
}

//
// ---- the process tooltip ----
//

//
// What a block of the tooltip is about.
//
static QString ToolTipSectionHeading(int Kind)
{
	switch (Kind)
	{
	case CProcessInfo::SToolTipSection::eServiceGroup:	return CTaskExplorer::tr("Service group name:");
	case CProcessInfo::SToolTipSection::eRunDllTarget:	return CTaskExplorer::tr("Run DLL target file:");
	case CProcessInfo::SToolTipSection::eComTarget:		return CTaskExplorer::tr("COM target:");
	case CProcessInfo::SToolTipSection::eComTargetFile:	return CTaskExplorer::tr("COM target file:");
	case CProcessInfo::SToolTipSection::eServices:		return CTaskExplorer::tr("Services:");
	case CProcessInfo::SToolTipSection::eTasks:			return CTaskExplorer::tr("Tasks:");
	case CProcessInfo::SToolTipSection::eDrivers:		return CTaskExplorer::tr("Drivers:");
	case CProcessInfo::SToolTipSection::eEdgeRole:		return CTaskExplorer::tr("Edge:");
	case CProcessInfo::SToolTipSection::eWmiProviders:	return CTaskExplorer::tr("WMI Providers:");
	}
	return QString();
}

//
// Which part of Edge a process is.
//
static QString EdgeRoleToString(int Role)
{
	switch (Role)
	{
	case CProcessInfo::SToolTipSection::eEdgeManager:				return CTaskExplorer::tr("Microsoft Edge Manager");
	case CProcessInfo::SToolTipSection::eEdgeBrowserExtensions:		return CTaskExplorer::tr("Browser Extensions");
	case CProcessInfo::SToolTipSection::eEdgeUserInterfaceService:	return CTaskExplorer::tr("User Interface Service");
	case CProcessInfo::SToolTipSection::eEdgeChakraJitCompiler:		return CTaskExplorer::tr("Chakra Jit Compiler");
	case CProcessInfo::SToolTipSection::eEdgeFlashPlayer:			return CTaskExplorer::tr("Adobe Flash Player");
	case CProcessInfo::SToolTipSection::eEdgeBackgroundTabPool:		return CTaskExplorer::tr("Background Tab Pool");
	}
	return QString();
}

QStringList GetToolTipLines(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return QStringList();

	QStringList Lines;

	foreach (const CProcessInfo::SToolTipSection& Section, pProcess->GetToolTipSections())
	{
		Lines.append(ToolTipSectionHeading(Section.Kind));

		if (Section.Kind == CProcessInfo::SToolTipSection::eEdgeRole)
		{
			Lines.append(CTaskExplorer::tr("    %1").arg(EdgeRoleToString(Section.Role)));
			continue;
		}

		//
		// A version line reads as two words side by side; everything else pairs
		// a name with what it stands for.
		//
		const bool bVersion = Section.Kind == CProcessInfo::SToolTipSection::eRunDllTarget
						   || Section.Kind == CProcessInfo::SToolTipSection::eComTargetFile;

		typedef QPair<QString, QString> SItem;
		foreach (const SItem& Item, Section.Items)
		{
			//
			// A scheduled task's action or path may be one the target could not
			// read; every other item here is a name it did have.
			//
			const QString First = LocalizeName(Item.first);
			const QString Second = LocalizeName(Item.second);

			if (Second.isEmpty())
				Lines.append(CTaskExplorer::tr("    %1").arg(First));
			else if (bVersion)
				Lines.append(CTaskExplorer::tr("    %1 %2").arg(First).arg(Second));
			else
				Lines.append(CTaskExplorer::tr("    %1 (%2)").arg(First).arg(Second));
		}
	}

	return Lines;
}

//
// ---- mitigation details ----
//

//
// The mitigations the platform has no description for. Everything else on that
// page arrives already worded, from a table the platform owns.
//
QPair<QString, QString> GetMitigationDetail(const CProcessInfo::SMitigationDetail& Detail)
{
	switch (Detail.Extra)
	{
	case CProcessInfo::SMitigationDetail::eExtraLoaderIntegrity:
		return qMakePair(CTaskExplorer::tr("Loader Integrity"),
			CTaskExplorer::tr("OS signing levels for dependent module loads are enabled."));
	case CProcessInfo::SMitigationDetail::eExtraModuleTampering:
		return qMakePair(CTaskExplorer::tr("Module Tampering"),
			CTaskExplorer::tr("Module Tampering protection is enabled."));
	case CProcessInfo::SMitigationDetail::eExtraIndirectBranchPrediction:
		return qMakePair(CTaskExplorer::tr("Indirect branch prediction"),
			CTaskExplorer::tr("Protects against sibling hardware threads (hyperthreads) from interfering with indirect branch predictions."));
	case CProcessInfo::SMitigationDetail::eExtraDynamicCodeDowngrade:
		return qMakePair(CTaskExplorer::tr("Dynamic code (downgrade)"),
			CTaskExplorer::tr("Allows a broker to downgrade the dynamic code policy for a process."));
	case CProcessInfo::SMitigationDetail::eExtraSpeculativeStoreBypass:
		return qMakePair(CTaskExplorer::tr("Speculative store bypass"),
			CTaskExplorer::tr("Disables spectre mitigations for the process."));
	}

	return qMakePair(Detail.Name, Detail.Description);
}

//
// Where an environment variable comes from.
//
QString GetEnvVarTypeString(int Type)
{
	switch (Type)
	{
	case CProcessInfo::SEnvVar::eSystem:	return CTaskExplorer::tr("System");
	case CProcessInfo::SEnvVar::eUser:		return CTaskExplorer::tr("User");
	case CProcessInfo::SEnvVar::eProcess:	return CTaskExplorer::tr("Process");
	}
	return QString();
}

//
// How the kernel is filtering a process's system calls.
//
QString GetSeccompModeString(int Mode)
{
	switch (Mode)
	{
	case 0:	return CTaskExplorer::tr("Disabled");
	case 1:	return CTaskExplorer::tr("Strict");
	case 2:	return CTaskExplorer::tr("Filtered");
	}
	return CTaskExplorer::tr("Unknown (%1)").arg(Mode);
}

//
// ---- the security dialog's object title ----
//

//
// What kind of thing the dialog is editing. The type string is a stable
// identifier the core uses to pick an access-right table; naming it is separate.
//
QString GetSecurityObjectType(const QString& Type)
{
	if (Type == "Process")		return CTaskExplorer::tr("Process");
	if (Type == "Thread")		return CTaskExplorer::tr("Thread");
	if (Type == "Job")			return CTaskExplorer::tr("Job");
	if (Type == "Token")		return CTaskExplorer::tr("Token");
	if (Type == "TokenDefault")	return CTaskExplorer::tr("Default token");
	if (Type == "Handle")		return CTaskExplorer::tr("Handle");
	if (Type == "Service")		return CTaskExplorer::tr("Service");
	if (Type == "SCManager")	return CTaskExplorer::tr("Service Control Manager");
	if (Type == "LsaPolicy")	return CTaskExplorer::tr("Local LSA Policy");
	if (Type == "LsaAccount")	return CTaskExplorer::tr("LSA account");
	if (Type == "SamUser")		return CTaskExplorer::tr("User account");
	if (Type == "SamGroup")		return CTaskExplorer::tr("Group account");
	return Type;
}

//
// How the object is named in the title. Some kinds are identified by a name the
// platform gave, some by a number, and some only by what they are.
//
QString GetSecurityObjectName(const CSecurityEditablePtr& pObject)
{
	if (pObject.isNull())
		return QString();

	const QString Type = pObject->GetTypeName();
	const QString Name = pObject->GetName();

	if (Type == "Process")
		return Name.isEmpty() ? CTaskExplorer::tr("Process %1").arg(pObject->GetObjectId())
							  : CTaskExplorer::tr("%1 (%2)").arg(Name).arg(pObject->GetObjectId());

	if (Type == "Thread")
		return CTaskExplorer::tr("Thread %1").arg(pObject->GetObjectId());

	//
	// The rest either carry a name of their own or are named by their kind -
	// there is only ever one Service Control Manager to be looking at.
	//
	return Name.isEmpty() ? GetSecurityObjectType(Type) : Name;
}

//
// ---- small labels ----
//

//
// Which processor the scheduler prefers for a thread, as group:number. A
// platform that does not schedule by groups says nothing.
//
QString GetIdealProcessorString(const CThreadPtr& pThread)
{
	if (pThread.isNull())
		return QString();

	const int Group = pThread->GetIdealProcessorGroup();
	const int Number = pThread->GetIdealProcessorNumber();
	if (Group < 0 || Number < 0)
		return QString();

	return CTaskExplorer::tr("%1:%2").arg(Group).arg(Number);
}

//
// ---- the system, disks, and stack frames ----
//

//
// What the target calls itself. Windows keeps it as numbers, so the reading is
// made here; a platform that only has a release string reports that instead.
//
QString GetSystemVersionString(CSystemAPI* pSystem)
{
	if (!pSystem)
		return QString();

	if (!pSystem->GetSystemVersion().isEmpty())
		return pSystem->GetSystemVersion();

	const quint32 Major = pSystem->GetSystemMajorVersion();
	const quint32 Minor = pSystem->GetSystemMinorVersion();
	if (!Major)
		return QString();

	return Minor ? CTaskExplorer::tr("Windows %1.%2").arg(Major).arg(Minor)
				 : CTaskExplorer::tr("Windows %1").arg(Major);
}

QString GetSystemBuildString(CSystemAPI* pSystem)
{
	if (!pSystem)
		return QString();

	if (!pSystem->GetSystemBuild().isEmpty())
		return pSystem->GetSystemBuild();

	const quint32 Build = pSystem->GetSystemBuildNumber();
	if (!Build)
		return QString();

	//
	// The marketing name for the release, where there is one - "23H2 (22631)".
	//
	const QString Release = pSystem->GetSystemReleaseId();
	return Release.isEmpty() ? QString::number(Build)
							 : CTaskExplorer::tr("%1 (%2)").arg(Release).arg(Build);
}

//
// How a disk is labelled in the list: its number, where it is mounted, and
// what the driver calls it. A platform that only reports mount points shows
// those alone.
//
QString GetDiskLabel(const CDiskMonitor::SDiskInfo& Disk)
{
	if (Disk.DiskNumber == ULONG_MAX)
		return Disk.DeviceMountPoints;

	if (Disk.DeviceMountPoints.isEmpty())
		return CTaskExplorer::tr("Disk %1 [%2]").arg(Disk.DiskNumber).arg(Disk.DeviceDescription);

	return CTaskExplorer::tr("Disk %1 (%2) [%3]")
		.arg(Disk.DiskNumber).arg(Disk.DeviceMountPoints).arg(Disk.DeviceDescription);
}

//
// What a stack frame is executing. A managed frame names its method, with the
// native symbol it compiled down to alongside; a frame the unwinder had to
// guess at says so.
//
QString GetStackSymbolString(const CStackTrace::SStackFrame& Frame)
{
	QString Symbol = Frame.Symbol;

	if (!Frame.NativeSymbol.isEmpty())
	{
		if (Frame.ManagedDisplacement)
			Symbol += CTaskExplorer::tr(" + 0x%1").arg(Frame.ManagedDisplacement, 0, 16);
		Symbol += CTaskExplorer::tr(" <-- %1").arg(Frame.NativeSymbol);
	}

	if (Frame.bNoUnwindInfo)
		Symbol += CTaskExplorer::tr(" (No unwind info)");

	return Symbol;
}

//
// Where the frame is in the source. Without a line number the name is a module
// and an offset rather than a file, so it stands on its own.
//
QString GetStackFileInfoString(const CStackTrace::SStackFrame& Frame)
{
	if (Frame.FileName.isEmpty())
		return QString();

	return Frame.LineNumber
		? CTaskExplorer::tr("File: %1: line %2").arg(Frame.FileName).arg(Frame.LineNumber)
		: Frame.FileName;
}

//
// A security attribute's value. Most are plain numbers or strings; a
// fully-qualified binary name is a version and a name together, and a value
// that could not be read is absent.
//
QString GetSecurityAttributeValue(quint16 Type, const QVariant& Value)
{
	if (!Value.isValid())
	{
		return Type == CTokenInfo::eSecAttrSid ? CTaskExplorer::tr("(Invalid SID)")
											   : CTaskExplorer::tr("(Unknown)");
	}

	if (Type == CTokenInfo::eSecAttrFqbn)
	{
		const QVariantList Parts = Value.toList();
		if (Parts.count() >= 2)
			return CTaskExplorer::tr("Version %1: %2").arg(Parts[0].toULongLong()).arg(Parts[1].toString());
	}

	return Value.toString();
}

//
// Which thread owns a window, and which process that thread is in. A thread
// whose start address could not be resolved is still worth naming by its id.
//
QString GetWndThreadString(const CWndInfo::SWndInfo& WndInfo)
{
	const QString Start = WndInfo.ThreadStartAddress.isEmpty()
		? CTaskExplorer::tr("unknown") : WndInfo.ThreadStartAddress;

	return CTaskExplorer::tr("%1 (%2): %3")
		.arg(Start).arg(FormatID(WndInfo.ThreadProcessId)).arg(FormatID(WndInfo.ThreadId));
}

//
// ---- names the target could not supply ----
//

//
// A name on its way to the screen, with a placeholder read out if that is what
// it turned out to be.
//
// Meant to be called on every name displayed. The test is one character and
// almost always fails, so the cost of sprinkling this everywhere is a compare
// against '*' - see API/Placeholders.h for why no real name can start with one.
//
QString LocalizeName(const QString& Name)
{
	if (!IsPlaceholderName(Name))
		return Name;

	QString Which, Argument;
	if (!SplitPlaceholder(Name, Which, Argument))
		return Name;

	if (Which == QLatin1String(TE_NAME_UNKNOWN_PROCESS))
	{
		//
		// A pid narrows it down to a process that has gone, rather than one
		// that was never identified at all.
		//
		return Argument.isEmpty() ? CTaskExplorer::tr("Unknown process")
								  : CTaskExplorer::tr("Unknown process PID: %1").arg(Argument);
	}

	if (Which == QLatin1String(TE_NAME_SYSTEM_IDLE_PROCESS))	return CTaskExplorer::tr("System Idle Process");
	if (Which == QLatin1String(TE_NAME_WAITING_CONNECTIONS))	return CTaskExplorer::tr("Waiting connections");
	if (Which == QLatin1String(TE_NAME_UNKNOWN_TRACER))			return CTaskExplorer::tr("unknown");
	if (Which == QLatin1String(TE_NAME_RESOLVING))				return CTaskExplorer::tr("Resolving...");
	if (Which == QLatin1String(TE_NAME_NOT_RESOLVED))			return CTaskExplorer::tr("Not resolved...");
	if (Which == QLatin1String(TE_NAME_UNKNOWN_SID))			return CTaskExplorer::tr("[Unknown SID]");
	if (Which == QLatin1String(TE_NAME_UNKNOWN_FILE))			return CTaskExplorer::tr("Unknown file name");
	if (Which == QLatin1String(TE_NAME_UNKNOWN_ACTION))			return CTaskExplorer::tr("Unknown action");
	if (Which == QLatin1String(TE_NAME_UNKNOWN_PATH))			return CTaskExplorer::tr("Unknown path");

	//
	// A placeholder this build does not know - a newer target, most likely.
	// Showing it as it stands is more use than showing nothing.
	//
	return Name;
}

//
// ---- access rights ----
//
// The words for the rights the target reported. Two per right: the long one for
// the security dialog, which has room for it, and the short one for a column,
// which does not. Where the platform offers only one, both read the same.
//
static const struct { const char* Name; const char* ShortName; } g_AccessRightNames[] =
{
	{ NULL, NULL },	// eAccessNone

	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Synchronize"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Delete"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read permissions"), QT_TRANSLATE_NOOP("CTaskExplorer", "Read control") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Change permissions"), QT_TRANSLATE_NOOP("CTaskExplorer", "Write DAC") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Take ownership"), QT_TRANSLATE_NOOP("CTaskExplorer", "Write owner") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Full control"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Connect"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read events"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Assign processes"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Query information"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set information"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Write"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Execute"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Enumerate"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read objects"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Playback journals"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Write objects"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create windows"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create menus"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create window hooks"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Record journals"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Switch desktop"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Query"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Traverse"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create objects"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create subdirectories"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Notification"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read description"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create realtime"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create logfile"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "GUID enable"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Access kernel logger"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Log events"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Access realtime"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Register guids"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Join group"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Modify"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read & execute"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Traverse folder / execute file"), QT_TRANSLATE_NOOP("CTaskExplorer", "Execute") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "List folder / read data"), QT_TRANSLATE_NOOP("CTaskExplorer", "Read data") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read attributes"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read extended attributes"), QT_TRANSLATE_NOOP("CTaskExplorer", "Read EA") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create files / write data"), QT_TRANSLATE_NOOP("CTaskExplorer", "Write data") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create folders / append data"), QT_TRANSLATE_NOOP("CTaskExplorer", "Append data") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Write attributes"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Write extended attributes"), QT_TRANSLATE_NOOP("CTaskExplorer", "Write EA") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Delete subfolders and files"), QT_TRANSLATE_NOOP("CTaskExplorer", "Delete child") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set attributes"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set security attributes"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Terminate"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Enumerate subkeys"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Query values"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Notify"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set values"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create subkeys"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create links"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Wait"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Wake"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "View"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Adjust privileges"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Adjust quotas"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Adjust system access"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "View local information"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "View audit information"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Get private information"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Administer trust"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create account"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create secret"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create privilege"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set default quota limits"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set audit requirements"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Administer audit log"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Administer server"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Lookup names"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Get notifications"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set value"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Query value"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Query domain name"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Query controllers"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set controllers"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Query POSIX"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set POSIX"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Query authentication"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set authentication"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set quotas"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set session ID"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create threads"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create processes"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Modify memory"), QT_TRANSLATE_NOOP("CTaskExplorer", "VM operation") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read memory"), QT_TRANSLATE_NOOP("CTaskExplorer", "VM read") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Write memory"), QT_TRANSLATE_NOOP("CTaskExplorer", "VM write") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Duplicate handles"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Suspend / resume / set port"), QT_TRANSLATE_NOOP("CTaskExplorer", "Suspend/resume") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Query limited information"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set limited information"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Control"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read information"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Write account"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Add member"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Remove member"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "List members"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read password parameters"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Write password parameters"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read other parameters"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Write other parameters"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create user"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create group"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create alias"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Get alias membership"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "List accounts"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Lookup"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Shutdown"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Initialize"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create domain"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Enumerate domains"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Lookup domain"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read general"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read preferences"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Write preferences"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read logon"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read account"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Change password"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Force password change"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "List groups"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read group information"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Write group information"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Map for read"), QT_TRANSLATE_NOOP("CTaskExplorer", "Map read") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Map for write"), QT_TRANSLATE_NOOP("CTaskExplorer", "Map write") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Map for execute"), QT_TRANSLATE_NOOP("CTaskExplorer", "Map execute") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Map for execute (explicit)"), QT_TRANSLATE_NOOP("CTaskExplorer", "Map execute explicit") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Extend size"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Query status"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Query configuration"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Modify configuration"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Enumerate dependents"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Start"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Stop"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Pause / continue"), QT_TRANSLATE_NOOP("CTaskExplorer", "Pause/continue") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Interrogate"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "User-defined control"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create service"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Enumerate services"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Lock"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Modify boot config"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Query lock status"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Full control (extended)"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Get context"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set context"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Set token"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Alert"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Impersonate"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Direct impersonate"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Suspend / resume"), QT_TRANSLATE_NOOP("CTaskExplorer", "Suspend/resume") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Recover"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Subordinate rights"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Superior rights"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Enlist"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Register protocols"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Complete propagation"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Rename"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create resource manager"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Bind transactions"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Commit"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Rollback"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Propagate"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Adjust groups"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Adjust defaults"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Adjust session ID"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Assign as primary token"), QT_TRANSLATE_NOOP("CTaskExplorer", "Assign primary") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Duplicate"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Query source"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Release worker"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Ready worker"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Modify state"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Enable account"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Execute methods"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Full write"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Partial write"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Provider write"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Remote enable"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Enumerate desktops"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Read screen"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Access clipboard"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Access global atoms"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create desktop"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Exit windows"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create real-time logs"), QT_TRANSLATE_NOOP("CTaskExplorer", "Create real-time") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Create on disk logs"), QT_TRANSLATE_NOOP("CTaskExplorer", "Create on disk") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Enable provider GUIDs"), QT_TRANSLATE_NOOP("CTaskExplorer", "Enable GUIDs") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Access real-time events"), QT_TRANSLATE_NOOP("CTaskExplorer", "Access real-time") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Register provider GUIDs"), QT_TRANSLATE_NOOP("CTaskExplorer", "Register GUIDs") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Reset"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Virtual channels"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Remote control"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Logon"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Logoff"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Message"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Disconnect"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Guest access"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Guest access (current)"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "User access"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "User access (current)"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Execute local"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Execute remote"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Activate local"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "Activate remote"), NULL },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "FILE_FLAG_OVERLAPPED"), QT_TRANSLATE_NOOP("CTaskExplorer", "Asynchronous") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "FILE_FLAG_WRITE_THROUGH"), QT_TRANSLATE_NOOP("CTaskExplorer", "Write through") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "FILE_FLAG_SEQUENTIAL_SCAN"), QT_TRANSLATE_NOOP("CTaskExplorer", "Sequental") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "FILE_FLAG_NO_BUFFERING"), QT_TRANSLATE_NOOP("CTaskExplorer", "No buffering") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "FILE_SYNCHRONOUS_IO_ALERT"), QT_TRANSLATE_NOOP("CTaskExplorer", "Synchronous alert") },
	{ QT_TRANSLATE_NOOP("CTaskExplorer", "FILE_SYNCHRONOUS_IO_NONALERT"), QT_TRANSLATE_NOOP("CTaskExplorer", "Synchronous non-alert") },
};

QString GetAccessRightName(int Right, bool bShort)
{
	if (Right <= eAccessNone || Right >= eAccessRightCount)
		return QString();

	const char* Name = bShort && g_AccessRightNames[Right].ShortName
		? g_AccessRightNames[Right].ShortName : g_AccessRightNames[Right].Name;

	return Name ? CTaskExplorer::tr(Name) : QString();
}

QString GetAccessRightsString(const QList<int>& Rights)
{
	QStringList Names;
	foreach(int Right, Rights)
	{
		const QString Name = GetAccessRightName(Right, true);
		if (!Name.isEmpty())
			Names.append(Name);
	}
	return Names.join(CTaskExplorer::tr(", "));
}

//
// How a file handle was opened: the raw mode, and the flags it stands for.
//
QString GetFileAccessModeString(const CHandlePtr& pHandle, quint32 Mode)
{
	if (pHandle.isNull())
		return QString();

	return CTaskExplorer::tr("0x%1 (%2)")
		.arg(QString::number(Mode, 16))
		.arg(GetAccessRightsString(pHandle->GetFileAccessModeRights(Mode)));
}

//
// ---- icons ----
//

//
// An icon as the core reports it - the bytes of an image file - turned into
// something that can be drawn.
//
// This is the only place that happens. The collector deals in bytes because it
// may be running on another machine with no display at all; making a pixmap is
// the viewer's job, and has to happen on the thread that draws.
//
//
// Image bytes to a pixmap of a known size.
//
// Whoever produced the bytes had no idea what they would be drawn at, and the
// sizes that arrive are all over the place: a Windows executable carries 16, 32
// and 48 pixel icons and a shell extension may hand back 256; a freedesktop
// theme entry can resolve to a 512 pixel PNG or to an absolute path pointing at
// whatever the packager felt like shipping. Handed to an item view unchanged,
// each of those is drawn at its own size - which is what a 256 pixel icon in a
// 16 pixel row looks like, on both platforms.
//
// So the size is decided here, once, for every caller: the result is exactly
// Size logical pixels regardless of what came in.
//
// The scaling is done in device pixels and the ratio is recorded on the pixmap,
// which is the part that makes this a high-dpi fix rather than a resize. A
// pixmap with no ratio is drawn one image pixel per device pixel: on a 200%
// display that is a 32 pixel icon occupying 16 logical pixels - correct - and a
// 48 pixel icon occupying 24, which is the "some icons are too big" that shows
// up only when scaling is on. Scaling to Size * ratio and then declaring the
// ratio makes the two agree at any scale.
//
// One ratio for the whole application rather than per window. A pixmap cached
// in a model has no window to ask, and a mixed-dpi desktop would need the cache
// keyed by screen to do better - which is a great deal of machinery for an icon
// that is one pixel soft after being dragged to the other monitor.
//
QPixmap MakeIcon(const QByteArray& Bytes, int Size)
{
	if (Bytes.isEmpty())
		return QPixmap();

	QImage Image;
	if (!Image.loadFromData(Bytes) || Image.isNull())
		return QPixmap();

	const qreal Ratio = QGuiApplication::primaryScreen()
		? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
	const int Pixels = qMax(1, qRound(Size * Ratio));

	//
	// Both directions. Down is the case that prompted this; up matters just as
	// much on a scaled display, where a 16 pixel icon left alone is drawn at
	// half the size of everything around it.
	//
	if (Image.width() != Pixels || Image.height() != Pixels)
		Image = Image.scaled(Pixels, Pixels, Qt::KeepAspectRatio, Qt::SmoothTransformation);

	QPixmap Icon = QPixmap::fromImage(Image);
	Icon.setDevicePixelRatio(Ratio);
	return Icon;
}

//
// The CAP_* names in a Linux capability mask.
//
// Moved out of ProcFs, where it was a name table in the collector - and where a
// Windows viewer watching a Linux target could not have reached it.
//
QString GetHandleTypeLabel(const CSystemAPI::SHandleType& Type)
{
	switch (Type.Group)
	{
	case CSystemAPI::eHandleGroupWine:
		//
		// Qualified, because the two tables overlap: a Wine File is a
		// wineserver object and a Linux File is a descriptor the kernel gave
		// out, and a filter listing both unqualified would offer the same word
		// twice for two different things.
		//
		return QObject::tr("Wine %1").arg(Type.Name);
	default:
		return Type.Name;
	}
}

//
// One capability by number.
//
// Indexed by capability number, as defined in <linux/capability.h>. Listed
// literally rather than pulled from the header so that a kernel newer than the
// build machine's headers still names everything it reports - and a viewer on
// a different kernel than the target names what the target sent.
//
QString GetCapabilityName(int Bit)
{
	static const char* Names[] = {
		"CAP_CHOWN", "CAP_DAC_OVERRIDE", "CAP_DAC_READ_SEARCH", "CAP_FOWNER",
		"CAP_FSETID", "CAP_KILL", "CAP_SETGID", "CAP_SETUID",
		"CAP_SETPCAP", "CAP_LINUX_IMMUTABLE", "CAP_NET_BIND_SERVICE", "CAP_NET_BROADCAST",
		"CAP_NET_ADMIN", "CAP_NET_RAW", "CAP_IPC_LOCK", "CAP_IPC_OWNER",
		"CAP_SYS_MODULE", "CAP_SYS_RAWIO", "CAP_SYS_CHROOT", "CAP_SYS_PTRACE",
		"CAP_SYS_PACCT", "CAP_SYS_ADMIN", "CAP_SYS_BOOT", "CAP_SYS_NICE",
		"CAP_SYS_RESOURCE", "CAP_SYS_TIME", "CAP_SYS_TTY_CONFIG", "CAP_MKNOD",
		"CAP_LEASE", "CAP_AUDIT_WRITE", "CAP_AUDIT_CONTROL", "CAP_SETFCAP",
		"CAP_MAC_OVERRIDE", "CAP_MAC_ADMIN", "CAP_SYSLOG", "CAP_WAKE_ALARM",
		"CAP_BLOCK_SUSPEND", "CAP_AUDIT_READ", "CAP_PERFMON", "CAP_BPF",
		"CAP_CHECKPOINT_RESTORE",
	};
	static const int Count = (int)(sizeof(Names) / sizeof(Names[0]));

	if (Bit >= 0 && Bit < Count)
		return QString::fromLatin1(Names[Bit]);
	return QString("CAP_%1").arg(Bit);	// added since this table
}

QStringList GetCapabilityNames(quint64 Mask)
{
	QStringList Capabilities;
	for (int i = 0; i < 64; i++)
	{
		if (Mask & (1ULL << i))
			Capabilities.append(GetCapabilityName(i));
	}
	return Capabilities;
}


//
// The accounts every Windows machine has, by SID rather than by name.
//
// Only the ones that are genuinely universal - the three service accounts and
// the two anonymous ones. Anything with a machine or domain in it is not
// well known by definition and keeps whatever the target called it.
//
// Linux keys are numeric uids; root is the only one worth naming, since every
// other uid is whatever /etc/passwd on that machine says and the target has
// already resolved it.
//
QString GetMachineDisplayName(CSystemAPI* pSystem)
{
	if (!pSystem)
		return QString();

	QString Name;
	QString Address;
	const bool bRemote = theCluster && theCluster->GetTargetInfo(pSystem, &Name, &Address);

	QString Label = (bRemote && Name != Address) ? Name : pSystem->GetHostName();
	if (bRemote)
		Label += QString(" (%1)").arg(Address);
	else if (pSystem->IsLocal())
		Label += QString(" (%1)").arg(QObject::tr("local"));
	return Label;
}

QString GetMachineStateString(CSystemAPI* pSystem)
{
	switch (CCluster::GetTargetState(pSystem))
	{
	case STarget::eConnecting:	return CTaskExplorer::tr("Connecting...");

	//
	// "Unreachable" would be a lie for the one failure that is not about
	// reachability. A machine refused for its identity answered perfectly well,
	// and saying otherwise sends whoever reads it to look at the network, which
	// is the one place the problem is not.
	//
	case STarget::eFailed:
		return CCluster::GetTargetError(pSystem) == TE_MachineIdMismatch
			? CTaskExplorer::tr("Different machine")
			: CTaskExplorer::tr("Unreachable");

	//
	// The rows under this are real and were true when they were taken; they are
	// simply no longer current, and they go when the persistence window closes.
	//
	case STarget::eLost:		return CTaskExplorer::tr("Disconnected");

	default:					return QString();
	}
}

QString GetUserDisplayName(const QString& Key, const QString& Resolved)
{
	if (Key.isEmpty())
		return Resolved.isEmpty() ? QObject::tr("(unknown account)") : Resolved;

	static const struct { const char* Sid; const char* Name; } WellKnown[] = {
		{ "S-1-5-18", QT_TRANSLATE_NOOP("CTaskExplorer", "System") },
		{ "S-1-5-19", QT_TRANSLATE_NOOP("CTaskExplorer", "Local Service") },
		{ "S-1-5-20", QT_TRANSLATE_NOOP("CTaskExplorer", "Network Service") },
		{ "S-1-5-7",  QT_TRANSLATE_NOOP("CTaskExplorer", "Anonymous") },
		{ "S-1-1-0",  QT_TRANSLATE_NOOP("CTaskExplorer", "Everyone") },
		{ "S-1-5-32-544", QT_TRANSLATE_NOOP("CTaskExplorer", "Administrators") },
		{ "0",        QT_TRANSLATE_NOOP("CTaskExplorer", "root") },
	};

	for (size_t i = 0; i < sizeof(WellKnown) / sizeof(WellKnown[0]); i++)
	{
		if (Key == QLatin1String(WellKnown[i].Sid))
			return CTaskExplorer::tr(WellKnown[i].Name);
	}

	//
	// Through LocalizeName, because what the target sent may be a placeholder
	// rather than a name - "*UNKNOWN_SID*" is what a Windows collector reports
	// for an account it could not resolve, and the column beside this one has
	// always rendered it as "[Unknown SID]". A branch heading that showed the
	// raw marker would be the only place in the window still speaking the
	// protocol's language.
	//
	if (!Resolved.isEmpty())
		return LocalizeName(Resolved);

	//
	// A key with no name: the target could read the account but not resolve it,
	// which happens for a deleted account or one from a domain it cannot reach.
	// The key itself is more use than nothing.
	//
	return Key;
}
