/*
 * Process Hacker -
 *   qt wrapper and runas.c functions
 *
 * Copyright (C) 2009-2016 wj32
 * Copyright (C) 2018
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains Process Hacker code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */


/*
 * The run-as mechanism has three stages:
 * 1. The user enters the information into the dialog box. Here it is decided whether the run-as
 *    service is needed. If it is not, PhCreateProcessAsUser is called directly. Otherwise,
 *    PhExecuteRunAsCommand2 is called for stage 2.
 * 2. PhExecuteRunAsCommand2 creates a random service name and tries to create the service and
 *    execute it (using PhExecuteRunAsCommand). If the process has insufficient permissions, an
 *    elevated instance of phsvc is started and PhSvcCallExecuteRunAsCommand is called.
 * 3. The service is started, and sets up an instance of phsvc with the same random service name as
 *    its port name. Either the original or elevated Process Hacker instance then calls
 *    PhSvcCallInvokeRunAsService to complete the operation.
 */

/*
 *
 * ProcessHacker.exe (user, limited privileges)
 *   *                       | ^
 *   |                       | | phsvc API (LPC)
 *   |                       | |
 *   |                       v |
 *   ProcessHacker.exe (user, full privileges)
 *         | ^                    | ^
 *         | | SCM API (RPC)      | |
 *         | |                    | |
 *         v |                    | | phsvc API (LPC)
 * services.exe                   | |
 *   *                            | |
 *   |                            | |
 *   |                            | |
 *   |                            v |
 *   ProcessHacker.exe (NT AUTHORITY\SYSTEM)
 *     *
 *     |
 *     |
 *     |
 *     program.exe
 */

#include "stdafx.h"
#include "RunAs.h"
#include "../../../SVC/TaskService.h"
#include <shellapi.h>
extern "C" {
#include <winsta.h>
}
#include <lm.h>

BOOLEAN PhShellProcessHackerEx(
    _In_opt_ HWND hWnd,
    _In_opt_ PWSTR FileName,
    _In_opt_ PWSTR Parameters,
    _In_ ULONG ShowWindowType,
    _In_ ULONG Flags,
    _In_ ULONG AppFlags,
    _In_opt_ ULONG Timeout,
    _Out_opt_ PHANDLE ProcessHandle
    )
{
    BOOLEAN result;
    PPH_STRING applicationFileName;

    if (!(applicationFileName = PhGetApplicationFileName()))
        return FALSE;

    result = PhShellExecuteEx(
        hWnd,
        FileName ? FileName : PhGetString(applicationFileName),
        Parameters,
        ShowWindowType,
        Flags,
        Timeout,
        ProcessHandle
        );


    PhDereferenceObject(applicationFileName);

    return result;
}

NTSTATUS PhExecuteRunAsCommand3(
	_In_ HWND hWnd,
	_In_ PWSTR Program,
	_In_opt_ PWSTR UserName,
	_In_opt_ PWSTR Password,
	_In_opt_ ULONG LogonType,
	_In_opt_ HANDLE ProcessIdWithToken,
	_In_ ULONG SessionId,
	_In_ PWSTR DesktopName,
	_In_ BOOLEAN UseLinkedToken,
	_In_ BOOLEAN CreateSuspendedProcess
)
{
	NTSTATUS status = STATUS_SUCCESS;
	//PH_RUNAS_SERVICE_PARAMETERS parameters;
	WCHAR serviceName[32];
	PPH_STRING portName;
	UNICODE_STRING portNameUs;

	QString ServiceName = CTaskService::RunService();
	if (ServiceName.isEmpty())
		return STATUS_ACCESS_DENIED;

	QVariantMap Parameters;
	Parameters["ProcessId"] = (quint64)ProcessIdWithToken;
	Parameters["UserName"] = QString::fromWCharArray(UserName);
	Parameters["Password"] = QString::fromWCharArray(Password);
	Parameters["LogonType"] = (quint32)LogonType;
	Parameters["SessionId"] = (quint32)SessionId;
	Parameters["CommandLine"] = QString::fromWCharArray(Program);
	Parameters["DesktopName"] = QString::fromWCharArray(DesktopName);
	Parameters["UseLinkedToken"] = UseLinkedToken;
	Parameters["CreateSuspendedProcess"] = CreateSuspendedProcess;
	Parameters["ServiceName"] = ServiceName;

	QVariantMap Request;
	Request["Command"] = "SvcInvokeRunAsService";
	Request["Parameters"] = Parameters;

	QVariant Response = CTaskService::SendCommand(ServiceName, Request);

	if (Response.isValid())
		return Response.toUInt();
	return STATUS_PORT_DISCONNECTED;
}

NTSTATUS PhSvcpValidateRunAsServiceParameters(_In_ PPH_RUNAS_SERVICE_PARAMETERS Parameters)
{
    if ((!Parameters->UserName || !Parameters->Password) && !Parameters->ProcessId)
        return STATUS_INVALID_PARAMETER_MIX;
    if (!Parameters->FileName && !Parameters->CommandLine)
        return STATUS_INVALID_PARAMETER_MIX;
    if (!Parameters->ServiceName)
        return STATUS_INVALID_PARAMETER;

    return STATUS_SUCCESS;
}

VOID PhpSplitUserName(
    _In_ PWSTR UserName,
    _Out_opt_ PPH_STRING *DomainPart,
    _Out_opt_ PPH_STRING *UserPart
    )
{
    PH_STRINGREF userName;
    PH_STRINGREF domainPart;
    PH_STRINGREF userPart;

    PhInitializeStringRefLongHint(&userName, UserName);

    if (PhSplitStringRefAtChar(&userName, OBJ_NAME_PATH_SEPARATOR, &domainPart, &userPart))
    {
        if (DomainPart)
            *DomainPart = PhCreateString2(&domainPart);
        if (UserPart)
            *UserPart = PhCreateString2(&userPart);
    }
    else
    {
        if (DomainPart)
            *DomainPart = NULL;
        if (UserPart)
            *UserPart = PhCreateString2(&userName);
    }
}

NTSTATUS PhInvokeRunAsService(
    _In_ PPH_RUNAS_SERVICE_PARAMETERS Parameters
    )
{
    NTSTATUS status;
    PPH_STRING domainName;
    PPH_STRING userName;
    PH_CREATE_PROCESS_AS_USER_INFO createInfo;
    ULONG flags;

    if (Parameters->UserName)
    {
        PhpSplitUserName(Parameters->UserName, &domainName, &userName);
    }
    else
    {
        domainName = NULL;
        userName = NULL;
    }

    memset(&createInfo, 0, sizeof(PH_CREATE_PROCESS_AS_USER_INFO));
    createInfo.ApplicationName = Parameters->FileName;
    createInfo.CommandLine = Parameters->CommandLine;
    createInfo.CurrentDirectory = Parameters->CurrentDirectory;
    createInfo.DomainName = PhGetString(domainName);
    createInfo.UserName = PhGetString(userName);
    createInfo.Password = Parameters->Password;
    createInfo.LogonType = Parameters->LogonType;
    createInfo.SessionId = Parameters->SessionId;
    createInfo.DesktopName = Parameters->DesktopName;

    flags = PH_CREATE_PROCESS_SET_SESSION_ID;

    if (Parameters->ProcessId)
    {
        createInfo.ProcessIdWithToken = UlongToHandle(Parameters->ProcessId);
        flags |= PH_CREATE_PROCESS_USE_PROCESS_TOKEN;
    }

    if (Parameters->UseLinkedToken)
        flags |= PH_CREATE_PROCESS_USE_LINKED_TOKEN;
    if (Parameters->CreateSuspendedProcess)
        flags |= PH_CREATE_PROCESS_SUSPENDED;

    status = PhCreateProcessAsUser(
        &createInfo,
        flags,
        NULL,
        NULL,
        NULL
        );

    if (domainName) PhDereferenceObject(domainName);
    if (userName) PhDereferenceObject(userName);

    return status;
}


quint32 PhSvcApiInvokeRunAsService(const QVariantMap& Parameters)
{
	NTSTATUS status = STATUS_INVALID_PARAMETER;
	PH_RUNAS_SERVICE_PARAMETERS parameters;

	parameters.ProcessId = Parameters["ProcessId"].toULongLong();
	wstring userName = Parameters["UserName"].toString().toStdWString();
	parameters.UserName = userName.size() != 0 ? (wchar_t*)userName.c_str() : NULL;
	wstring password = Parameters["Password"].toString().toStdWString();
    parameters.Password = userName.size() != 0 ? (wchar_t*)password.c_str() : NULL; // if we have a username we also must have a password, even if its empty
	parameters.LogonType = Parameters["LogonType"].toUInt();
    parameters.SessionId = Parameters["SessionId"].toUInt();
	wstring currentDirectory = Parameters["CurrentDirectory"].toString().toStdWString();
    parameters.CurrentDirectory = currentDirectory.size() != 0 ? (wchar_t*)currentDirectory.c_str() : NULL;
	wstring commandLine = Parameters["CommandLine"].toString().toStdWString();
    parameters.CommandLine = commandLine.size() != 0 ? (wchar_t*)commandLine.c_str() : NULL;
	wstring fileName = Parameters["FileName"].toString().toStdWString();
    parameters.FileName = fileName.size() != 0 ? (wchar_t*)fileName.c_str() : NULL;
	wstring desktopName = Parameters["DesktopName"].toString().toStdWString();
    parameters.DesktopName = desktopName.size() != 0 ? (wchar_t*)desktopName.c_str() : NULL;
	parameters.UseLinkedToken = Parameters["UseLinkedToken"].toBool();
	wstring serviceName = Parameters["ServiceName"].toString().toStdWString();
	parameters.ServiceName = serviceName.size() != 0 ? (wchar_t*)serviceName.c_str() : NULL;
    parameters.CreateSuspendedProcess = Parameters["CreateSuspendedProcess"].toBool();

    if (NT_SUCCESS(status = PhSvcpValidateRunAsServiceParameters(&parameters)))
    {
        status = PhInvokeRunAsService(&parameters);
    }

	return status;
}

/**
 * Sets the access control lists of the current window station
 * and desktop to allow all access.
 */
VOID PhSetDesktopWinStaAccess(
    VOID
    )
{
    static SID_IDENTIFIER_AUTHORITY appPackageAuthority = SECURITY_APP_PACKAGE_AUTHORITY;

    HWINSTA wsHandle;
    HDESK desktopHandle;
    ULONG allocationLength;
    PSECURITY_DESCRIPTOR securityDescriptor;
    PACL dacl;
    CHAR allAppPackagesSidBuffer[FIELD_OFFSET(SID, SubAuthority) + sizeof(ULONG) * 2];
    PSID allAppPackagesSid;

    // TODO: Set security on the correct window station and desktop.

    allAppPackagesSid = (PISID)allAppPackagesSidBuffer;
    RtlInitializeSid(allAppPackagesSid, &appPackageAuthority, SECURITY_BUILTIN_APP_PACKAGE_RID_COUNT);
    *RtlSubAuthoritySid(allAppPackagesSid, 0) = SECURITY_APP_PACKAGE_BASE_RID;
    *RtlSubAuthoritySid(allAppPackagesSid, 1) = SECURITY_BUILTIN_PACKAGE_ANY_PACKAGE;

    // We create a DACL that allows everyone to access everything.

    allocationLength = SECURITY_DESCRIPTOR_MIN_LENGTH +
        (ULONG)sizeof(ACL) +
        (ULONG)sizeof(ACCESS_ALLOWED_ACE) +
        RtlLengthSid(&PhSeEveryoneSid) +
        (ULONG)sizeof(ACCESS_ALLOWED_ACE) +
        RtlLengthSid(allAppPackagesSid);
    securityDescriptor = PhAllocate(allocationLength);
    dacl = (PACL)PTR_ADD_OFFSET(securityDescriptor, SECURITY_DESCRIPTOR_MIN_LENGTH);

    RtlCreateSecurityDescriptor(securityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    RtlCreateAcl(dacl, allocationLength - SECURITY_DESCRIPTOR_MIN_LENGTH, ACL_REVISION);
    RtlAddAccessAllowedAce(dacl, ACL_REVISION, GENERIC_ALL, &PhSeEveryoneSid);

    if (WindowsVersion >= WINDOWS_8)
    {
        RtlAddAccessAllowedAce(dacl, ACL_REVISION, GENERIC_ALL, allAppPackagesSid);
    }

    RtlSetDaclSecurityDescriptor(securityDescriptor, TRUE, dacl, FALSE);

    if (wsHandle = OpenWindowStation(
        L"WinSta0",
        FALSE,
        WRITE_DAC
        ))
    {
        PhSetObjectSecurity(wsHandle, DACL_SECURITY_INFORMATION, securityDescriptor);
        CloseWindowStation(wsHandle);
    }

    if (desktopHandle = OpenDesktop(
        L"Default",
        0,
        FALSE,
        WRITE_DAC | DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS
        ))
    {
        PhSetObjectSecurity(desktopHandle, DACL_SECURITY_INFORMATION, securityDescriptor);
        CloseDesktop(desktopHandle);
    }

    PhFree(securityDescriptor);
}

/**
 * Starts a program as another user.
 *
 * \param hWnd A handle to the parent window.
 * \param Program The command line of the program to start.
 * \param UserName The user to start the program as. The user
 * name should be specified as: domain\\name. This parameter
 * can be NULL if \a ProcessIdWithToken is specified.
 * \param Password The password for the specified user. If there
 * is no password, specify an empty string. This parameter
 * can be NULL if \a ProcessIdWithToken is specified.
 * \param LogonType The logon type for the specified user. This
 * parameter can be 0 if \a ProcessIdWithToken is specified.
 * \param ProcessIdWithToken The ID of a process from which
 * to duplicate the token.
 * \param SessionId The ID of the session to run the program
 * under.
 * \param DesktopName The window station and desktop to run the
 * program under.
 * \param UseLinkedToken Uses the linked token if possible.
 *
 * \retval STATUS_CANCELLED The user cancelled the operation.
 *
 * \remarks This function will cause another instance of
 * Process Hacker to be executed if the current security context
 * does not have sufficient system access. This is done
 * through a UAC elevation prompt.
 */
NTSTATUS PhExecuteRunAsCommand2(
    _In_ HWND hWnd,
    _In_ PWSTR Program,
    _In_opt_ PWSTR UserName,
    _In_opt_ PWSTR Password,
    _In_opt_ ULONG LogonType,
    _In_opt_ HANDLE ProcessIdWithToken,
    _In_ ULONG SessionId,
    _In_ PWSTR DesktopName,
    _In_ BOOLEAN UseLinkedToken
    )
{
    return PhExecuteRunAsCommand3(hWnd, Program, UserName, Password, LogonType, ProcessIdWithToken, SessionId, DesktopName, UseLinkedToken, FALSE);
}

NTSTATUS RunAsTrustedInstaller(PWSTR CommandLine)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    SERVICE_STATUS_PROCESS serviceStatus = { 0 };
    SC_HANDLE serviceHandle = NULL;
    HANDLE processHandle = NULL;
    HANDLE tokenHandle = NULL;
    PTOKEN_USER tokenUser = NULL;
    PPH_STRING userName = NULL;
    PPH_STRING commandLine;
    ULONG bytesNeeded = 0;

    //commandLine = PhConcatStrings2(CommandLine, L""); // todo:
	commandLine = PhConcatStrings2(USER_SHARED_DATA->NtSystemRoot, L"\\System32\\cmd.exe");

    if (!(serviceHandle = PhOpenService(L"TrustedInstaller", SERVICE_QUERY_STATUS | SERVICE_START)))
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        goto CleanupExit;
    }

    if (!QueryServiceStatusEx(
        serviceHandle,
        SC_STATUS_PROCESS_INFO,
        (PBYTE)&serviceStatus,
        sizeof(SERVICE_STATUS_PROCESS),
        &bytesNeeded
        ))
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        goto CleanupExit;
    }

    if (serviceStatus.dwCurrentState == SERVICE_RUNNING)
    {
        status = STATUS_SUCCESS;
    }
    else
    {
        ULONG attempts = 5;

        StartService(serviceHandle, 0, NULL);

        do
        {
            if (QueryServiceStatusEx(
                serviceHandle,
                SC_STATUS_PROCESS_INFO,
                (PBYTE)&serviceStatus,
                sizeof(SERVICE_STATUS_PROCESS),
                &bytesNeeded
                ))
            {
                if (serviceStatus.dwCurrentState == SERVICE_RUNNING)
                {
                    status = STATUS_SUCCESS;
                    break;
                }
            }

            Sleep(1000);

        } while (--attempts != 0);
    }

    if (!NT_SUCCESS(status))
    {
        status = STATUS_SERVICES_FAILED_AUTOSTART; // One or more services failed to start.
        goto CleanupExit;
    }

    if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, ProcessQueryAccess, UlongToHandle(serviceStatus.dwProcessId))))
        goto CleanupExit;

    if (!NT_SUCCESS(status = NtOpenProcessToken(processHandle, TOKEN_QUERY, &tokenHandle)))
        goto CleanupExit;

    if (!NT_SUCCESS(status = PhGetTokenUser(tokenHandle, &tokenUser)))
        goto CleanupExit;

    if (!(userName = PhGetSidFullName(tokenUser->User.Sid, TRUE, NULL)))
    {
        status = STATUS_INVALID_SID; // the SID structure is not valid.
        goto CleanupExit;
    }

    status = PhExecuteRunAsCommand2(
        PhMainWndHandle,
        PhGetStringOrEmpty(commandLine),
        PhGetStringOrEmpty(userName),
        L"",
        LOGON32_LOGON_SERVICE,
        UlongToHandle(serviceStatus.dwProcessId),
        NtCurrentPeb()->SessionId,
        NULL,
        FALSE
        );

CleanupExit:

    if (commandLine)
        PhDereferenceObject(commandLine);

    if (userName)
        PhDereferenceObject(userName);

    if (tokenUser)
        PhFree(tokenUser);

    if (tokenHandle)
        NtClose(tokenHandle);

    if (processHandle)
        NtClose(processHandle);

    if (serviceHandle)
        CloseServiceHandle(serviceHandle);

    return status;
}


ULONG SelectedRunAsMode = ULONG_MAX;
HWND PhMainWndHandle = NULL;

BOOLEAN PhMwpOnNotify(
    _In_ NMHDR *Header,
    _Out_ LRESULT *Result
    )
{
    if (Header->code == RFN_VALIDATE)
    {
        LPNMRUNFILEDLG runFileDlg = (LPNMRUNFILEDLG)Header;

        if (SelectedRunAsMode == RUNAS_MODE_ADMIN)
        {
            PH_STRINGREF string;
            PH_STRINGREF fileName;
            PH_STRINGREF arguments;
            PPH_STRING fullFileName;
            PPH_STRING argumentsString;

            PhInitializeStringRefLongHint(&string, runFileDlg->lpszFile);
            PhParseCommandLineFuzzy(&string, &fileName, &arguments, &fullFileName);

            if (!fullFileName)
                fullFileName = PhCreateString2(&fileName);

            argumentsString = PhCreateString2(&arguments);

            if (PhShellExecuteEx(
                PhMainWndHandle,
                fullFileName->Buffer,
                argumentsString->Buffer,
                runFileDlg->ShowCmd,
                PH_SHELL_EXECUTE_ADMIN,
                0,
                NULL
                ))
            {
                *Result = RF_CANCEL;
            }
            else
            {
                *Result = RF_RETRY;
            }

            PhDereferenceObject(fullFileName);
            PhDereferenceObject(argumentsString);

            return TRUE;
        }
		else if (SelectedRunAsMode == RUNAS_MODE_SYS)
		{
			NTSTATUS status = STATUS_SUCCESS;
			//NTSTATUS status = RunAsTrustedInstaller(runFileDlg->lpszFile);

			QStringList Arguments;
			Arguments.append("-runasti");
			Arguments.append(QString::fromWCharArray(runFileDlg->lpszFile));
			QProcess::execute(QCoreApplication::applicationFilePath(), Arguments);

            if (NT_SUCCESS(status))
            {
                *Result = RF_CANCEL;
            }
            else
            {
                PhShowStatus(PhMainWndHandle, L"Unable to execute the program.", status, 0);
                *Result = RF_RETRY;
            }

            return TRUE;
		}
        else if (SelectedRunAsMode == RUNAS_MODE_LIMITED)
        {
            NTSTATUS status;
            HANDLE tokenHandle;
            HANDLE newTokenHandle;

            if (NT_SUCCESS(status = PhOpenProcessToken(
                NtCurrentProcess(),
                TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_GROUPS |
                TOKEN_ADJUST_DEFAULT | READ_CONTROL | WRITE_DAC,
                &tokenHandle
                )))
            {
                if (NT_SUCCESS(status = PhFilterTokenForLimitedUser(
                    tokenHandle,
                    &newTokenHandle
                    )))
                {
                    status = PhCreateProcessWin32(
                        NULL,
                        runFileDlg->lpszFile,
                        NULL,
                        NULL,
                        0,
                        newTokenHandle,
                        NULL,
                        NULL
                        );

                    NtClose(newTokenHandle);
                }

                NtClose(tokenHandle);
            }

            if (NT_SUCCESS(status))
            {
                *Result = RF_CANCEL;
            }
            else
            {
                PhShowStatus(PhMainWndHandle, L"Unable to execute the program.", status, 0);
                *Result = RF_RETRY;
            }

            return TRUE;
        }
    }
    else if (Header->code == RFN_LIMITEDRUNAS)
    {
        LPNMRUNFILEDLG runFileDlg = (LPNMRUNFILEDLG)Header;
        PVOID WdcLibraryHandle;
        ULONG (WINAPI* WdcRunTaskAsInteractiveUser_I)(
            _In_ PCWSTR CommandLine,
            _In_ PCWSTR CurrentDirectory,
            _In_ ULONG Reserved
            );


        // dmex: Task Manager uses RFF_OPTRUNAS and RFN_LIMITEDRUNAS to show the 'Create this task with administrative privileges' checkbox
        // on the RunFileDlg when the current process is elevated. Task Manager also uses the WdcRunTaskAsInteractiveUser function to launch processes
        // as the interactive user from an elevated token. The WdcRunTaskAsInteractiveUser function
        // invokes the "\Microsoft\Windows\Task Manager\Interactive" Task Scheduler task for launching the process but 
        // doesn't return error information and we need to perform some sanity checks before invoking the task. 
        // Ideally, we should use our own task but for now just re-use the existing task and do what Task Manager does...

        if (WdcLibraryHandle = LoadLibrary(L"wdc.dll"))
        {
            if (WdcRunTaskAsInteractiveUser_I = (ULONG (WINAPI* )(_In_ PCWSTR CommandLine, _In_ PCWSTR CurrentDirectory, _In_ ULONG Reserved)) PhGetDllBaseProcedureAddress(WdcLibraryHandle, "WdcRunTaskAsInteractiveUser", 0))
            {
                PH_STRINGREF string;
                PPH_STRING commandlineString;
                PPH_STRING executeString = NULL;
                INT cmdlineArgCount;
                PWSTR* cmdlineArgList;

                PhInitializeStringRefLongHint(&string, (PWSTR)runFileDlg->lpszFile);
                commandlineString = PhCreateString2(&string);

                // Extract the filename.
                if (cmdlineArgList = CommandLineToArgvW(commandlineString->Buffer, &cmdlineArgCount))
                {
                    PPH_STRING fileName = PhCreateString(cmdlineArgList[0]);

                    if (fileName && !RtlDoesFileExists_U(fileName->Buffer))
                    {
                        PPH_STRING filePathString;

                        // The user typed a name without a path so attempt to locate the executable.
                        if (filePathString = PhSearchFilePath(fileName->Buffer, L".exe"))
                            PhMoveReference((PVOID*)&fileName, filePathString);
                        else
                            PhClearReference((PVOID*)&fileName);
                    }

                    if (fileName)
                    {
                        // Escape the filename.
                        PhMoveReference((PVOID*)&fileName, PhConcatStrings(3, L"\"", fileName->Buffer, L"\""));

                        if (cmdlineArgCount == 2)
                        {
                            PPH_STRING fileArgs = PhCreateString(cmdlineArgList[1]);

                            // Escape the parameters.
                            PhMoveReference((PVOID*)&fileArgs, PhConcatStrings(3, L"\"", fileArgs->Buffer, L"\""));

                            // Create the escaped execute string.
                            PhMoveReference((PVOID*)&executeString, PhConcatStrings(3, fileName->Buffer, L" ", fileArgs->Buffer));

                            // Cleanup.
                            PhDereferenceObject(fileArgs);
                        }
                        else
                        {
                            // Create the escaped execute string.
                            executeString = (PPH_STRING)PhReferenceObject(fileName);
                        }

                        PhDereferenceObject(fileName);
                    }

                    LocalFree(cmdlineArgList);
                }

                if (!PhIsNullOrEmptyString(executeString))
                {
                    if (WdcRunTaskAsInteractiveUser_I(executeString->Buffer, NULL, 0) == 0)
                        *Result = RF_CANCEL;
                    else
                        *Result = RF_RETRY;
                }
                else
                {
                    *Result = RF_RETRY;
                }

                if (executeString)
                    PhDereferenceObject(executeString);
                PhDereferenceObject(commandlineString);
            }

            FreeLibrary((HMODULE)WdcLibraryHandle);
        }
        return TRUE;
    }

    return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef  ULONG (WINAPI *pNetUserEnum)(
    _In_ PCWSTR servername,
    _In_ ULONG level,
    _In_ ULONG filter,
    _Out_ PBYTE *bufptr,
    _In_ ULONG prefmaxlen,
    _Out_ PULONG entriesread,
    _Out_ PULONG totalentries,
    _Inout_ PULONG resume_handle
    );
static pNetUserEnum NetUserEnum_I;

typedef  ULONG (WINAPI *pNetApiBufferFree)(
    _Frees_ptr_opt_ PVOID Buffer
    );
static pNetApiBufferFree NetApiBufferFree_I;

PPH_STRING GetCurrentWinStaName(
    VOID
    )
{
    PPH_STRING string;

    string = PhCreateStringEx(NULL, 0x200);

    if (GetUserObjectInformation(
        GetProcessWindowStation(),
        UOI_NAME,
        string->Buffer,
        (ULONG)string->Length + sizeof(UNICODE_NULL),
        NULL
        ))
    {
        PhTrimToNullTerminatorString(string);
        return string;
    }
    else
    {
        PhDereferenceObject(string);
        return PhCreateString(L"WinSta0"); // assume the current window station is WinSta0
    }
}

PPH_STRING GetCurrentDesktopName(
    VOID
    )
{
    PPH_STRING string;

    string = PhCreateStringEx(NULL, 0x200);

    if (GetUserObjectInformation(
        GetThreadDesktop(HandleToULong(NtCurrentThreadId())),
        UOI_NAME,
        string->Buffer,
        (ULONG)string->Length + sizeof(UNICODE_NULL),
        NULL
        ))
    {
        PhTrimToNullTerminatorString(string);
        return string;
    }
    else
    {
        PhDereferenceObject(string);
        return PhCreateString(L"Default");
    }
}

PPH_STRING PhpGetCurrentDesktopInfo(
    VOID
    )
{
    static PH_STRINGREF seperator = PH_STRINGREF_INIT(L"\\"); // OBJ_NAME_PATH_SEPARATOR
    PPH_STRING desktopInfo = NULL;
    PPH_STRING winstationName = NULL;
    PPH_STRING desktopName = NULL;

    winstationName = GetCurrentWinStaName();
    desktopName = GetCurrentDesktopName();

    if (winstationName && desktopName)
    {
        desktopInfo = PhConcatStringRef3(&winstationName->sr, &seperator, &desktopName->sr);
    }

    if (PhIsNullOrEmptyString(desktopInfo))
    {
        PH_STRINGREF desktopInfoSr;

        PhUnicodeStringToStringRef(&NtCurrentPeb()->ProcessParameters->DesktopInfo, &desktopInfoSr);

        PhMoveReference((PVOID*)&desktopInfo, PhCreateString2(&desktopInfoSr));
    }

    if (winstationName)
        PhDereferenceObject(winstationName);
    if (desktopName)
        PhDereferenceObject(desktopName);

    return desktopInfo;
}

BOOLEAN PhpInitializeNetApi(VOID)
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static HMODULE netapiModuleHandle = NULL;

    if (PhBeginInitOnce(&initOnce))
    {
        if (netapiModuleHandle = LoadLibrary(L"netapi32.dll"))
        {
            NetUserEnum_I = (pNetUserEnum)PhGetDllBaseProcedureAddress(netapiModuleHandle, "NetUserEnum", 0);
            NetApiBufferFree_I = (pNetApiBufferFree)PhGetDllBaseProcedureAddress(netapiModuleHandle, "NetApiBufferFree", 0);
        }

        if (netapiModuleHandle && !NetUserEnum_I && !NetApiBufferFree_I)
        {
            FreeLibrary(netapiModuleHandle);
            netapiModuleHandle = NULL;
        }

        PhEndInitOnce(&initOnce);
    }

    if (netapiModuleHandle)
        return TRUE;

    return FALSE;
}

PPH_STRING ntAuthoritySystem = NULL;
PPH_STRING ntAuthorityLocal = NULL;
PPH_STRING ntAuthorityNetwork = NULL;

BOOLEAN InitNtAuthority()
{
	static PH_INITONCE initOnce = PH_INITONCE_INIT;

    if (PhBeginInitOnce(&initOnce))
    {
		// that may like a mutex, but sonc all subsequent access is read only its imho fine without one.
        ntAuthoritySystem = PhGetSidFullName(&PhSeLocalSystemSid, TRUE, NULL);
		ntAuthorityLocal = PhGetSidFullName(&PhSeLocalServiceSid, TRUE, NULL);
		ntAuthorityNetwork = PhGetSidFullName(&PhSeNetworkServiceSid, TRUE, NULL);

        PhEndInitOnce(&initOnce);
    }

	return ntAuthoritySystem != NULL && ntAuthorityLocal != NULL && ntAuthorityNetwork != NULL;
}

BOOLEAN IsServiceAccount(
    _In_ PPH_STRING UserName
    )
{
	// these strings depand on the system language...
	if (!InitNtAuthority())
		return FALSE;

    if (
        PhEqualString2(UserName, ntAuthoritySystem->sr.Buffer, TRUE) ||
        PhEqualString2(UserName, ntAuthorityLocal->sr.Buffer, TRUE) ||
        PhEqualString2(UserName, ntAuthorityNetwork->sr.Buffer, TRUE)
        )
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

BOOLEAN IsCurrentUserAccount(
    _In_ PPH_STRING UserName
    )
{
    PPH_STRING userName;

    if (userName = PhGetTokenUserString(PhGetOwnTokenAttributes().TokenHandle, TRUE))
    {
        if (PhEndsWithString(userName, UserName, TRUE))
        {
            PhDereferenceObject(userName);
            return TRUE;
        }

        PhDereferenceObject(userName);
    }

    return FALSE;
}

void AddAccountsToComboBox(QComboBox* pComboBox)
{
	NET_API_STATUS status;
    LPUSER_INFO_0 userinfoArray = NULL;
    ULONG userinfoMaxLength = MAX_PREFERRED_LENGTH;
    ULONG userinfoEntriesRead = 0;
    ULONG userinfoTotalEntries = 0;
    ULONG userinfoResumeHandle = 0;

	pComboBox->clear();

	if (!InitNtAuthority())
		return;

	pComboBox->addItem(CastPhString(ntAuthoritySystem, false));
    pComboBox->addItem(CastPhString(ntAuthorityLocal, false));
    pComboBox->addItem(CastPhString(ntAuthorityNetwork, false));

    if (!PhpInitializeNetApi())
        return;

    NetUserEnum_I(
        NULL,
        0,
        FILTER_NORMAL_ACCOUNT,
        (PBYTE*)&userinfoArray,
        userinfoMaxLength,
        &userinfoEntriesRead,
        &userinfoTotalEntries,
        &userinfoResumeHandle
        );

    if (userinfoArray)
    {
        NetApiBufferFree_I(userinfoArray);
        userinfoArray = NULL;
    }

    status = NetUserEnum_I(
        NULL,
        0,
        FILTER_NORMAL_ACCOUNT,
        (PBYTE*)&userinfoArray,
        userinfoMaxLength,
        &userinfoEntriesRead,
        &userinfoTotalEntries,
        &userinfoResumeHandle
        );

    if (status == NERR_Success)
    {
        PPH_STRING username;
        PPH_STRING userDomainName = NULL;

        if (username = PhGetSidFullName(PhGetOwnTokenAttributes().TokenSid, TRUE, NULL))
        {
            PhpSplitUserName(username->Buffer, &userDomainName, NULL);
            PhDereferenceObject(username);
        }

        for (ULONG i = 0; i < userinfoEntriesRead; i++)
        {
            LPUSER_INFO_0 entry = (LPUSER_INFO_0)PTR_ADD_OFFSET(userinfoArray, sizeof(USER_INFO_0) * i);

            if (entry->usri0_name)
            {
                if (userDomainName)
                {
                    PPH_STRING usernameString;

                    usernameString = PhConcatStrings(
                        3, 
                        userDomainName->Buffer, 
                        L"\\", 
                        entry->usri0_name
                        );

					pComboBox->addItem(QString::fromWCharArray(usernameString->Buffer));
                    PhDereferenceObject(usernameString);
                }
                else
                {
                    pComboBox->addItem(QString::fromWCharArray(entry->usri0_name));
                }
            }
        }

        if (userDomainName)
            PhDereferenceObject(userDomainName);
    }

    if (userinfoArray)
        NetApiBufferFree_I(userinfoArray);

    //LSA_HANDLE policyHandle;
    //LSA_ENUMERATION_HANDLE enumerationContext = 0;
    //PLSA_ENUMERATION_INFORMATION buffer;
    //ULONG count;
    //PPH_STRING name;
    //SID_NAME_USE nameUse;
    //
    //if (NT_SUCCESS(PhOpenLsaPolicy(&policyHandle, POLICY_VIEW_LOCAL_INFORMATION, NULL)))
    //{
    //    while (NT_SUCCESS(LsaEnumerateAccounts(
    //        policyHandle,
    //        &enumerationContext,
    //        &buffer,
    //        0x100,
    //        &count
    //        )))
    //    {
    //        for (i = 0; i < count; i++)
    //        {
    //            name = PhGetSidFullName(buffer[i].Sid, TRUE, &nameUse);
    //            if (name)
    //            {
    //                if (nameUse == SidTypeUser)
    //                    ComboBox_AddString(ComboBoxHandle, name->Buffer);
    //                PhDereferenceObject(name);
    //            }
    //        }
    //        LsaFreeMemory(buffer);
    //    }
    //
    //    LsaClose(policyHandle);
    //}
}

void AddSessionsToComboBox(QComboBox* pComboBox)
{
	PSESSIONIDW sessions;
    ULONG numberOfSessions;
    ULONG i;

	pComboBox->clear();

    if (WinStationEnumerateW(NULL, &sessions, &numberOfSessions))
    {
        for (i = 0; i < numberOfSessions; i++)
        {
            PPH_STRING menuString;
            WINSTATIONINFORMATION winStationInfo;
            ULONG returnLength;

            if (!WinStationQueryInformationW(
                NULL,
                sessions[i].SessionId,
                WinStationInformation,
                &winStationInfo,
                sizeof(WINSTATIONINFORMATION),
                &returnLength
                ))
            {
                winStationInfo.Domain[0] = UNICODE_NULL;
                winStationInfo.UserName[0] = UNICODE_NULL;
            }

            if (
                winStationInfo.UserName[0] != UNICODE_NULL &&
                sessions[i].WinStationName[0] != UNICODE_NULL
                )
            {
                menuString = PhFormatString(L"%lu: %s (%s\\%s)",
                    sessions[i].SessionId,
                    sessions[i].WinStationName,
                    winStationInfo.Domain,
                    winStationInfo.UserName
                    );
            }
            else if (winStationInfo.UserName[0] != UNICODE_NULL)
            {
                menuString = PhFormatString(L"%lu: %s\\%s",
                    sessions[i].SessionId,
                    winStationInfo.Domain,
                    winStationInfo.UserName
                    );
            }
            else if (sessions[i].WinStationName[0] != UNICODE_NULL)
            {
                menuString = PhFormatString(L"%lu: %s",
                    sessions[i].SessionId,
                    sessions[i].WinStationName
                    );
            }
            else
            {
                menuString = PhFormatString(L"%lu", sessions[i].SessionId);
            }

			pComboBox->addItem(CastPhString(menuString), (quint64)sessions[i].SessionId);
        }

        WinStationFreeMemory(sessions);
    }
}

typedef struct _RUNAS_DIALOG_DESKTOP_CALLBACK
{
    PPH_LIST DesktopList;
    PPH_STRING WinStaName;
} RUNAS_DIALOG_DESKTOP_CALLBACK, *PRUNAS_DIALOG_DESKTOP_CALLBACK;

static BOOL CALLBACK EnumDesktopsCallback(
    _In_ PWSTR DesktopName,
    _In_ LPARAM Context
    )
{
    PRUNAS_DIALOG_DESKTOP_CALLBACK context = (PRUNAS_DIALOG_DESKTOP_CALLBACK)Context;

    PhAddItemList(context->DesktopList, PhConcatStrings(
        3,
        PhGetString(context->WinStaName),
        L"\\",
        DesktopName
        ));

    return TRUE;
}

void AddDesktopsToComboBox(QComboBox* pComboBox)
{
	ULONG i;
    RUNAS_DIALOG_DESKTOP_CALLBACK callback;

	pComboBox->clear();

    callback.DesktopList = PhCreateList(10);
    callback.WinStaName = GetCurrentWinStaName();

    EnumDesktops(GetProcessWindowStation(), EnumDesktopsCallback, (LPARAM)&callback);

    for (i = 0; i < callback.DesktopList->Count; i++)
    {
		pComboBox->addItem(CastPhString((PPH_STRING)callback.DesktopList->Items[i]));
    }

    PhDereferenceObject(callback.DesktopList);
    PhDereferenceObject(callback.WinStaName);
}

VOID SetDefaultSessionEntry(QComboBox* pComboBox)
{
    INT sessionCount = pComboBox->count();
    ULONG currentSessionId = 0;

    if (!NT_SUCCESS(PhGetProcessSessionId(NtCurrentProcess(), &currentSessionId)))
        return;

    for (INT i = 0; i < sessionCount; i++)
    {
        if (pComboBox->itemData(i).toUInt() == currentSessionId)
        {
			pComboBox->setCurrentIndex(i);
            break;
        }
    }
}

VOID SetDefaultDesktopEntry(QComboBox* pComboBox)
{
    INT sessionCount = pComboBox->count();
    PPH_STRING desktopName;

    if (!(desktopName = PhpGetCurrentDesktopInfo()))
        return;

    for (INT i = 0; i < sessionCount; i++)
    {
        if (pComboBox->itemText(i) == CastPhString(desktopName, false))
        {
            pComboBox->setCurrentIndex(i);
            break;
        }
    }

    PhDereferenceObject(desktopName);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

typedef INT (CALLBACK *MRUSTRINGCMPPROC)(PCWSTR pString1, PCWSTR pString2);
typedef INT (CALLBACK *MRUINARYCMPPROC)(LPCVOID pString1, LPCVOID pString2, ULONG length);

#define MRU_STRING 0x0000 // list will contain strings.
#define MRU_BINARY 0x0001 // list will contain binary data.
#define MRU_CACHEWRITE 0x0002 // only save list order to reg. is FreeMRUList.

typedef struct _MRUINFO
{
    ULONG cbSize;
    UINT uMaxItems;
    UINT uFlags;
    HKEY hKey;
    LPCTSTR lpszSubKey;
    MRUSTRINGCMPPROC lpfnCompare;
} MRUINFO, *PMRUINFO;

typedef HANDLE (WINAPI *pCreateMRUList)(
    _In_ PMRUINFO lpmi
    );
static pCreateMRUList CreateMRUList_I;

typedef INT (WINAPI *pAddMRUString)(
    _In_ HANDLE hMRU,
    _In_ PWSTR szString
    );
static pAddMRUString AddMRUString_I;

typedef INT (WINAPI *pEnumMRUList)(
    _In_ HANDLE hMRU,
    _In_ INT nItem,
    _Out_ PVOID lpData,
    _In_ UINT uLen
    );
static pEnumMRUList EnumMRUList_I;

typedef INT (WINAPI *pFreeMRUList)(
    _In_ HANDLE hMRU
    );
static pFreeMRUList FreeMRUList_I;

BOOLEAN PhpInitializeMRUList(VOID)
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static HMODULE comctl32ModuleHandle = NULL;

    if (PhBeginInitOnce(&initOnce))
    {
        if (comctl32ModuleHandle = LoadLibrary(L"comctl32.dll"))
        {
            CreateMRUList_I = (pCreateMRUList)PhGetDllBaseProcedureAddress(comctl32ModuleHandle, "CreateMRUListW", 0);
            AddMRUString_I = (pAddMRUString)PhGetDllBaseProcedureAddress(comctl32ModuleHandle, "AddMRUStringW", 0);
            EnumMRUList_I = (pEnumMRUList)PhGetDllBaseProcedureAddress(comctl32ModuleHandle, "EnumMRUListW", 0);
            FreeMRUList_I = (pFreeMRUList)PhGetDllBaseProcedureAddress(comctl32ModuleHandle, "FreeMRUList", 0);
        }

        if (!CreateMRUList_I && !AddMRUString_I && !EnumMRUList_I && !FreeMRUList_I && comctl32ModuleHandle)
        {
            FreeLibrary(comctl32ModuleHandle);
            comctl32ModuleHandle = NULL;
        }

        PhEndInitOnce(&initOnce);
    }

    if (comctl32ModuleHandle)
        return TRUE;

    return FALSE;
}

HANDLE PhpCreateRunMRUList(
    VOID
    )
{
    MRUINFO info;

    if (!CreateMRUList_I)
        return NULL;

    memset(&info, 0, sizeof(MRUINFO));
    info.cbSize = sizeof(MRUINFO);
    info.uMaxItems = UINT_MAX;
    info.hKey = HKEY_CURRENT_USER;
    info.lpszSubKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\RunMRU";

    return CreateMRUList_I(&info);
}

VOID PhpAddRunMRUListEntry(
    _In_ PPH_STRING CommandLine
    )
{
    static PH_STRINGREF prefixSr = PH_STRINGREF_INIT(L"\\1");
    HANDLE listHandle;
    PPH_STRING commandString;

    if (!(listHandle = PhpCreateRunMRUList()))
        return;

    commandString = PhConcatStringRef2(&CommandLine->sr, &prefixSr);
    AddMRUString_I(listHandle, commandString->Buffer);
    PhDereferenceObject(commandString);

    FreeMRUList_I(listHandle);
}

void AddProgramsToComboBox(QComboBox* pComboBox)
{
    static PH_STRINGREF prefixSr = PH_STRINGREF_INIT(L"\\1");
    HANDLE listHandle;
    INT listCount;

    if (!PhpInitializeMRUList())
        return;
    if (!(listHandle = PhpCreateRunMRUList()))
        return;

    listCount = EnumMRUList_I(
        listHandle,
        MAXINT,
        NULL,
        0
        );

    for (INT i = 0; i < listCount; i++)
    {
        PPH_STRING programName;
        PH_STRINGREF nameSr;
        PH_STRINGREF firstPart;
        PH_STRINGREF remainingPart;
        WCHAR entry[MAX_PATH];

        if (!EnumMRUList_I(
            listHandle,
            i,
            entry,
            RTL_NUMBER_OF(entry)
            ))
        {
            break;
        }

        PhInitializeStringRefLongHint(&nameSr, entry);

        if (!PhSplitStringRefAtString(&nameSr, &prefixSr, TRUE, &firstPart, &remainingPart))
        {
			pComboBox->addItem(QString::fromWCharArray(entry));
            continue;
        }

        programName = PhCreateString2(&firstPart);
		pComboBox->addItem(CastPhString(programName));
    }

    FreeMRUList_I(listHandle);
}