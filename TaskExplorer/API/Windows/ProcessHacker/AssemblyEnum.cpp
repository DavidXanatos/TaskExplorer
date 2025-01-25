/*
 * System Informer -
 *   qt port of asmpage.c (.NET Assemblies property page)
 *
 * Copyright (C) 2011-2015 wj32
 * Copyright (C) 2016-2018 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 * 
 */

#include "stdafx.h"
#include "AssemblyEnum.h"
#include "../ProcessHacker.h"

#include "../../../MiscHelpers/Common/Common.h"

#include <evntcons.h>

#include "clretw.h"


int _QMap_QVariant_QVariantMap_type = qRegisterMetaType<QMap<QVariant, QVariantMap>>("QMap<QVariant, QVariantMap>");

CAssemblyEnum::CAssemblyEnum(quint64 ProcessId, QObject* parent) : QThread(parent) 
{
	//m_bCancel = false;

	m_ProcessId = ProcessId;
}

CAssemblyEnum::~CAssemblyEnum() 
{
	/*m_bCancel = true;
	
	if(!wait(10*1000))
		terminate();

	DestroyDotNetTraceQuery(m);*/
}


#define DNATNC_STRUCTURE 0
#define DNATNC_ID 1
#define DNATNC_FLAGS 2
#define DNATNC_PATH 3
#define DNATNC_NATIVEPATH 4
#define DNATNC_MAXIMUM 5

#define DNA_TYPE_CLR 1
#define DNA_TYPE_APPDOMAIN 2
#define DNA_TYPE_ASSEMBLY 3

typedef struct _DNA_NODE
{
    PH_TREENEW_NODE Node;

    struct _DNA_NODE *Parent;
    PPH_LIST Children;

    PH_STRINGREF TextCache[DNATNC_MAXIMUM];

    ULONG Type;
    BOOLEAN IsFakeClr;

    union
    {
        struct
        {
            USHORT ClrInstanceID;
            ULONG StartupFlags;
            PPH_STRING DisplayName;
        } Clr;
        struct
        {
            ULONG64 AppDomainID;
            ULONG AppDomainFlags;
            PPH_STRING DisplayName;
        } AppDomain;
        struct
        {
            ULONG64 AssemblyID;
            ULONG AssemblyFlags;
            PPH_STRING FullyQualifiedAssemblyName;
        } Assembly;
    } u;

    PH_STRINGREF StructureText;
	UINT64 Id;
	//PPH_STRING IdText;
    PPH_STRING FlagsText;
    PPH_STRING PathText;
    PPH_STRING NativePathText;
} DNA_NODE, *PDNA_NODE;

typedef struct _ASMPAGE_QUERY_CONTEXT
{
    //HANDLE WindowHandle;

    HANDLE ProcessId;
    ULONG ClrVersions;
    PDNA_NODE ClrV2Node;

    BOOLEAN TraceClrV2;
    ULONG TraceResult;
    LONG TraceHandleActive;
    TRACEHANDLE TraceHandle;

    PPH_LIST NodeList;
    PPH_LIST NodeRootList;
} ASMPAGE_QUERY_CONTEXT, *PASMPAGE_QUERY_CONTEXT;

typedef struct _FLAG_DEFINITION
{
    PWSTR Name;
    ULONG Flag;
} FLAG_DEFINITION, *PFLAG_DEFINITION;

VOID DestroyDotNetTraceQuery(
    _In_ PASMPAGE_QUERY_CONTEXT Context
    );

INT_PTR CALLBACK DotNetAsmPageDlgProc(
    _In_ HWND hwndDlg,
    _In_ UINT uMsg,
    _In_ WPARAM wParam,
    _In_ LPARAM lParam
    );

static UNICODE_STRING DotNetLoggerName = RTL_CONSTANT_STRING(L"PhDnLogger");
static GUID ClrRuntimeProviderGuid = { 0xe13c0d23, 0xccbc, 0x4e12, { 0x93, 0x1b, 0xd9, 0xcc, 0x2e, 0xee, 0x27, 0xe4 } };
static GUID ClrRundownProviderGuid = { 0xa669021c, 0xc450, 0x4609, { 0xa0, 0x35, 0x5a, 0xf5, 0x9a, 0xf4, 0xdf, 0x18 } };

static FLAG_DEFINITION AppDomainFlagsMap[] =
{
    { L"Default", 0x1 },
    { L"Executable", 0x2 },
    { L"Shared", 0x4 }
};

static FLAG_DEFINITION AssemblyFlagsMap[] =
{
    { L"DomainNeutral", 0x1 },
    { L"Dynamic", 0x2 },
    { L"Native", 0x4 },
    { L"Collectible", 0x8 }
};

static FLAG_DEFINITION ModuleFlagsMap[] =
{
    { L"DomainNeutral", 0x1 },
    { L"Native", 0x2 },
    { L"Dynamic", 0x4 },
    { L"Manifest", 0x8 }
};

static FLAG_DEFINITION StartupModeMap[] =
{
    { L"ManagedExe", 0x1 },
    { L"HostedCLR", 0x2 },
    { L"IjwDll", 0x4 },
    { L"ComActivated", 0x8 },
    { L"Other", 0x10 }
};

static FLAG_DEFINITION StartupFlagsMap[] =
{
    { L"CONCURRENT_GC", 0x1 },
    { L"LOADER_OPTIMIZATION_SINGLE_DOMAIN", 0x2 },
    { L"LOADER_OPTIMIZATION_MULTI_DOMAIN", 0x4 },
    { L"LOADER_SAFEMODE", 0x10 },
    { L"LOADER_SETPREFERENCE", 0x100 },
    { L"SERVER_GC", 0x1000 },
    { L"HOARD_GC_VM", 0x2000 },
    { L"SINGLE_VERSION_HOSTING_INTERFACE", 0x4000 },
    { L"LEGACY_IMPERSONATION", 0x10000 },
    { L"DISABLE_COMMITTHREADSTACK", 0x20000 },
    { L"ALWAYSFLOW_IMPERSONATION", 0x40000 },
    { L"TRIM_GC_COMMIT", 0x80000 },
    { L"ETW", 0x100000 },
    { L"SERVER_BUILD", 0x200000 },
    { L"ARM", 0x400000 }
};

PPH_STRING FlagsToString(
    _In_ ULONG Flags,
    _In_ PFLAG_DEFINITION Map,
    _In_ ULONG SizeOfMap
    )
{
    PH_STRING_BUILDER sb;
    ULONG i;

    PhInitializeStringBuilder(&sb, 100);

    for (i = 0; i < SizeOfMap / sizeof(FLAG_DEFINITION); i++)
    {
        if (Flags & Map[i].Flag)
        {
            PhAppendStringBuilder2(&sb, Map[i].Name);
            PhAppendStringBuilder2(&sb, L", ");
        }
    }

    if (sb.String->Length != 0)
        PhRemoveEndStringBuilder(&sb, 2);

    return PhFinalStringBuilderString(&sb);
}

PDNA_NODE AddNode(
    _Inout_ PASMPAGE_QUERY_CONTEXT Context
    )
{
    PDNA_NODE node;

    node = (PDNA_NODE)PhAllocateZero(sizeof(DNA_NODE));
    PhInitializeTreeNewNode(&node->Node);

    memset(node->TextCache, 0, sizeof(PH_STRINGREF) * DNATNC_MAXIMUM);
    node->Node.TextCache = node->TextCache;
    node->Node.TextCacheSize = DNATNC_MAXIMUM;

    node->Children = PhCreateList(1);

    PhAddItemList(Context->NodeList, node);

    return node;
}

VOID DotNetAsmDestroyNode(
    _In_ PDNA_NODE Node
    )
{
    if (Node->Children) PhDereferenceObject(Node->Children);

    if (Node->Type == DNA_TYPE_CLR)
    {
        if (Node->u.Clr.DisplayName) PhDereferenceObject(Node->u.Clr.DisplayName);
    }
    else if (Node->Type == DNA_TYPE_APPDOMAIN)
    {
        if (Node->u.AppDomain.DisplayName) PhDereferenceObject(Node->u.AppDomain.DisplayName);
    }
    else if (Node->Type == DNA_TYPE_ASSEMBLY)
    {
        if (Node->u.Assembly.FullyQualifiedAssemblyName) PhDereferenceObject(Node->u.Assembly.FullyQualifiedAssemblyName);
    }

    //if (Node->IdText) PhDereferenceObject(Node->IdText);
    if (Node->FlagsText) PhDereferenceObject(Node->FlagsText);
    if (Node->PathText) PhDereferenceObject(Node->PathText);
    if (Node->NativePathText) PhDereferenceObject(Node->NativePathText);

    PhFree(Node);
}

PDNA_NODE AddFakeClrNode(
    _In_ PASMPAGE_QUERY_CONTEXT Context,
    _In_ PWSTR DisplayName
    )
{
    PDNA_NODE node;

    node = AddNode(Context);
    node->Type = DNA_TYPE_CLR;
    node->IsFakeClr = TRUE;
    node->u.Clr.ClrInstanceID = 0;
    node->u.Clr.DisplayName = NULL;
    PhInitializeStringRef(&node->StructureText, DisplayName);

    PhAddItemList(Context->NodeRootList, node);

    return node;
}

PDNA_NODE FindClrNode(
    _In_ PASMPAGE_QUERY_CONTEXT Context,
    _In_ USHORT ClrInstanceID
    )
{
    for (ULONG i = 0; i < Context->NodeRootList->Count; i++)
    {
        PDNA_NODE node = (PDNA_NODE)Context->NodeRootList->Items[i];

        if (!node->IsFakeClr && node->u.Clr.ClrInstanceID == ClrInstanceID)
            return node;
    }

    return NULL;
}

PDNA_NODE FindAppDomainNode(
    _In_ PDNA_NODE ClrNode,
    _In_ ULONG64 AppDomainID
    )
{
    for (ULONG i = 0; i < ClrNode->Children->Count; i++)
    {
        PDNA_NODE node = (PDNA_NODE)ClrNode->Children->Items[i];

        if (node->u.AppDomain.AppDomainID == AppDomainID)
            return node;
    }

    return NULL;
}

PDNA_NODE FindAssemblyNode(
    _In_ PDNA_NODE AppDomainNode,
    _In_ ULONG64 AssemblyID
    )
{
    for (ULONG i = 0; i < AppDomainNode->Children->Count; i++)
    {
        PDNA_NODE node = (PDNA_NODE)AppDomainNode->Children->Items[i];

        if (node->u.Assembly.AssemblyID == AssemblyID)
            return node;
    }

    return NULL;
}

PDNA_NODE FindAssemblyNode2(
    _In_ PDNA_NODE ClrNode,
    _In_ ULONG64 AssemblyID
    )
{
    for (ULONG i = 0; i < ClrNode->Children->Count; i++)
    {
        PDNA_NODE appDomainNode = (PDNA_NODE)ClrNode->Children->Items[i];

        for (ULONG j = 0; j < appDomainNode->Children->Count; j++)
        {
            PDNA_NODE assemblyNode = (PDNA_NODE)appDomainNode->Children->Items[j];

            if (assemblyNode->u.Assembly.AssemblyID == AssemblyID)
                return assemblyNode;
        }
    }

    return NULL;
}



static ULONG StartDotNetTrace(
    _Out_ PTRACEHANDLE SessionHandle,
    _Out_ PEVENT_TRACE_PROPERTIES *Properties
    )
{
    ULONG result;
    ULONG bufferSize;
    PEVENT_TRACE_PROPERTIES properties;
    TRACEHANDLE sessionHandle;

    bufferSize = sizeof(EVENT_TRACE_PROPERTIES) + DotNetLoggerName.Length + sizeof(WCHAR);
    properties = (PEVENT_TRACE_PROPERTIES)PhAllocateZero(bufferSize);

    properties->Wnode.BufferSize = bufferSize;
    properties->Wnode.ClientContext = 2; // System time clock resolution
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_USE_PAGED_MEMORY;
    properties->LogFileNameOffset = 0;
    properties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    result = StartTrace(&sessionHandle, DotNetLoggerName.Buffer, properties);

    if (result == ERROR_SUCCESS)
    {
        *SessionHandle = sessionHandle;
        *Properties = properties;

        return ERROR_SUCCESS;
    }
    else if (result == ERROR_ALREADY_EXISTS)
    {
        // Session already exists, so use that. Get the existing session handle.

        result = ControlTrace(0, DotNetLoggerName.Buffer, properties, EVENT_TRACE_CONTROL_QUERY);

        if (result != ERROR_SUCCESS)
        {
            PhFree(properties);
            return result;
        }

        *SessionHandle = properties->Wnode.HistoricalContext;
        *Properties = properties;

        return ERROR_SUCCESS;
    }
    else
    {
        PhFree(properties);

        return result;
    }
}

static ULONG NTAPI DotNetBufferCallback(
    _In_ PEVENT_TRACE_LOGFILE Buffer
    )
{
    return TRUE;
}

static VOID NTAPI DotNetEventCallback(
    _In_ PEVENT_RECORD EventRecord
    )
{
    PASMPAGE_QUERY_CONTEXT context = (PASMPAGE_QUERY_CONTEXT)EventRecord->UserContext;
    PEVENT_HEADER eventHeader = &EventRecord->EventHeader;
    PEVENT_DESCRIPTOR eventDescriptor = &eventHeader->EventDescriptor;

    if (UlongToHandle(eventHeader->ProcessId) == context->ProcessId)
    {
        // .NET 4.0+

        switch (eventDescriptor->Id)
        {
        case RuntimeInformationDCStart:
            {
                PRuntimeInformationRundown data = (PRuntimeInformationRundown)EventRecord->UserData;
                PDNA_NODE node;
                PPH_STRING startupFlagsString;
                PPH_STRING startupModeString;

                // Check for duplicates.
                if (FindClrNode(context, data->ClrInstanceID))
                    break;

                node = AddNode(context);
                node->Type = DNA_TYPE_CLR;
                node->u.Clr.ClrInstanceID = data->ClrInstanceID;
                node->u.Clr.StartupFlags = data->StartupFlags;
                node->u.Clr.DisplayName = PhFormatString(L"CLR v%u.%u.%u.%u", data->VMMajorVersion, data->VMMinorVersion, data->VMBuildNumber, data->VMQfeNumber);
                node->StructureText = node->u.Clr.DisplayName->sr;
                //node->IdText = PhFormatUInt64(data->ClrInstanceID, FALSE);
				node->Id = data->ClrInstanceID;

                startupFlagsString = FlagsToString(data->StartupFlags, StartupFlagsMap, sizeof(StartupFlagsMap));
                startupModeString = FlagsToString(data->StartupMode, StartupModeMap, sizeof(StartupModeMap));

                if (startupFlagsString->Length != 0 && startupModeString->Length != 0)
                {
                    node->FlagsText = PhConcatStrings(3, startupFlagsString->Buffer, L", ", startupModeString->Buffer);
                    PhDereferenceObject(startupFlagsString);
                    PhDereferenceObject(startupModeString);
                }
                else if (startupFlagsString->Length != 0)
                {
                    node->FlagsText = startupFlagsString;
                    PhDereferenceObject(startupModeString);
                }
                else if (startupModeString->Length != 0)
                {
                    node->FlagsText = startupModeString;
                    PhDereferenceObject(startupFlagsString);
                }

                if (data->CommandLine[0])
                    node->PathText = PhCreateString(data->CommandLine);

                PhAddItemList(context->NodeRootList, node);
            }
            break;
        case AppDomainDCStart_V1:
            {
                PAppDomainLoadUnloadRundown_V1 data = (PAppDomainLoadUnloadRundown_V1)EventRecord->UserData;
                SIZE_T appDomainNameLength;
                USHORT clrInstanceID;
                PDNA_NODE parentNode;
                PDNA_NODE node;

                appDomainNameLength = PhCountStringZ(data->AppDomainName) * sizeof(WCHAR);
                clrInstanceID = *(PUSHORT)PTR_ADD_OFFSET(data, FIELD_OFFSET(AppDomainLoadUnloadRundown_V1, AppDomainName) + appDomainNameLength + sizeof(WCHAR) + sizeof(ULONG));

                // Find the CLR node to add the AppDomain node to.
                parentNode = FindClrNode(context, clrInstanceID);

                if (parentNode)
                {
                    // Check for duplicates.
                    if (FindAppDomainNode(parentNode, data->AppDomainID))
                        break;

                    node = AddNode(context);
                    node->Type = DNA_TYPE_APPDOMAIN;
                    node->u.AppDomain.AppDomainID = data->AppDomainID;
                    node->u.AppDomain.AppDomainFlags = data->AppDomainFlags;
                    node->u.AppDomain.DisplayName = PhConcatStrings2(L"AppDomain: ", data->AppDomainName);
                    node->StructureText = node->u.AppDomain.DisplayName->sr;
                    //node->IdText = PhFormatUInt64(data->AppDomainID, FALSE);
					node->Id = data->AppDomainID;
                    node->FlagsText = FlagsToString(data->AppDomainFlags, AppDomainFlagsMap, sizeof(AppDomainFlagsMap));

                    PhAddItemList(parentNode->Children, node);
                }
            }
            break;
        case AssemblyDCStart_V1:
            {
                PAssemblyLoadUnloadRundown_V1 data = (PAssemblyLoadUnloadRundown_V1)EventRecord->UserData;
                SIZE_T fullyQualifiedAssemblyNameLength;
                USHORT clrInstanceID;
                PDNA_NODE parentNode;
                PDNA_NODE node;
                PH_STRINGREF remainingPart;

                fullyQualifiedAssemblyNameLength = PhCountStringZ(data->FullyQualifiedAssemblyName) * sizeof(WCHAR);
                clrInstanceID = *(PUSHORT)PTR_ADD_OFFSET(data, FIELD_OFFSET(AssemblyLoadUnloadRundown_V1, FullyQualifiedAssemblyName) + fullyQualifiedAssemblyNameLength + sizeof(WCHAR));

                // Find the AppDomain node to add the Assembly node to.

                parentNode = FindClrNode(context, clrInstanceID);

                if (parentNode)
                    parentNode = FindAppDomainNode(parentNode, data->AppDomainID);

                if (parentNode)
                {
                    // Check for duplicates.
                    if (FindAssemblyNode(parentNode, data->AssemblyID))
                        break;

                    node = AddNode(context);
                    node->Type = DNA_TYPE_ASSEMBLY;
                    node->u.Assembly.AssemblyID = data->AssemblyID;
                    node->u.Assembly.AssemblyFlags = data->AssemblyFlags;
                    node->u.Assembly.FullyQualifiedAssemblyName = PhCreateStringEx(data->FullyQualifiedAssemblyName, fullyQualifiedAssemblyNameLength);

                    // Display only the assembly name, not the whole fully qualified name.
                    if (!PhSplitStringRefAtChar(&node->u.Assembly.FullyQualifiedAssemblyName->sr, ',', &node->StructureText, &remainingPart))
                        node->StructureText = node->u.Assembly.FullyQualifiedAssemblyName->sr;

                    //node->IdText = PhFormatUInt64(data->AssemblyID, FALSE);
					node->Id = data->AssemblyID;
                    node->FlagsText = FlagsToString(data->AssemblyFlags, AssemblyFlagsMap, sizeof(AssemblyFlagsMap));

                    PhAddItemList(parentNode->Children, node);
                }
            }
            break;
        case ModuleDCStart_V1:
            {
                PModuleLoadUnloadRundown_V1 data = (PModuleLoadUnloadRundown_V1)EventRecord->UserData;
                PWSTR moduleILPath;
                SIZE_T moduleILPathLength;
                PWSTR moduleNativePath;
                SIZE_T moduleNativePathLength;
                USHORT clrInstanceID;
                PDNA_NODE node;

                moduleILPath = data->ModuleILPath;
                moduleILPathLength = PhCountStringZ(moduleILPath) * sizeof(WCHAR);
                moduleNativePath = (PWSTR)PTR_ADD_OFFSET(moduleILPath, moduleILPathLength + sizeof(WCHAR));
                moduleNativePathLength = PhCountStringZ(moduleNativePath) * sizeof(WCHAR);
                clrInstanceID = *(PUSHORT)PTR_ADD_OFFSET(moduleNativePath, moduleNativePathLength + sizeof(WCHAR));

                // Find the Assembly node to set the path on.

                node = FindClrNode(context, clrInstanceID);

                if (node)
                    node = FindAssemblyNode2(node, data->AssemblyID);

                if (node)
                {
                    PhMoveReference((PVOID*)&node->PathText, PhCreateStringEx(moduleILPath, moduleILPathLength));

                    if (moduleNativePathLength != 0)
                        PhMoveReference((PVOID*)&node->NativePathText, PhCreateStringEx(moduleNativePath, moduleNativePathLength));
                }
            }
            break;
        case DCStartComplete_V1:
            {
                if (_InterlockedExchange(&context->TraceHandleActive, 0) == 1)
                {
                    CloseTrace(context->TraceHandle);
                }
            }
            break;
        }

        // .NET 2.0

        if (eventDescriptor->Id == 0)
        {
            switch (eventDescriptor->Opcode)
            {
            case CLR_MODULEDCSTART_OPCODE:
                {
                    PModuleLoadUnloadRundown_V1 data = (PModuleLoadUnloadRundown_V1)EventRecord->UserData;
                    PWSTR moduleILPath;
                    SIZE_T moduleILPathLength;
                    PWSTR moduleNativePath;
                    SIZE_T moduleNativePathLength;
                    PDNA_NODE node;
                    ULONG_PTR indexOfBackslash;
                    ULONG_PTR indexOfLastDot;

                    moduleILPath = data->ModuleILPath;
                    moduleILPathLength = PhCountStringZ(moduleILPath) * sizeof(WCHAR);
                    moduleNativePath = (PWSTR)PTR_ADD_OFFSET(moduleILPath, moduleILPathLength + sizeof(WCHAR));
                    moduleNativePathLength = PhCountStringZ(moduleNativePath) * sizeof(WCHAR);

                    if (context->ClrV2Node && (moduleILPathLength != 0 || moduleNativePathLength != 0))
                    {
                        node = AddNode(context);
                        node->Type = DNA_TYPE_ASSEMBLY;
                        node->FlagsText = FlagsToString(data->ModuleFlags, ModuleFlagsMap, sizeof(ModuleFlagsMap));
                        node->PathText = PhCreateStringEx(moduleILPath, moduleILPathLength);

                        if (moduleNativePathLength != 0)
                            node->NativePathText = PhCreateStringEx(moduleNativePath, moduleNativePathLength);

                        // Use the name between the last backslash and the last dot for the structure column text.
                        // (E.g. C:\...\AcmeSoft.BigLib.dll -> AcmeSoft.BigLib)

                        indexOfBackslash = PhFindLastCharInString(node->PathText, 0, OBJ_NAME_PATH_SEPARATOR);
                        indexOfLastDot = PhFindLastCharInString(node->PathText, 0, '.');

                        if (indexOfBackslash != -1)
                        {
                            node->StructureText.Buffer = node->PathText->Buffer + indexOfBackslash + 1;

                            if (indexOfLastDot != -1 && indexOfLastDot > indexOfBackslash)
                            {
                                node->StructureText.Length = (indexOfLastDot - indexOfBackslash - 1) * sizeof(WCHAR);
                            }
                            else
                            {
                                node->StructureText.Length = node->PathText->Length - indexOfBackslash * sizeof(WCHAR) - sizeof(WCHAR);
                            }
                        }
                        else
                        {
                            node->StructureText = node->PathText->sr;
                        }

                        PhAddItemList(context->ClrV2Node->Children, node);
                    }
                }
                break;
            case CLR_METHODDC_DCSTARTCOMPLETE_OPCODE:
                {
                    if (_InterlockedExchange(&context->TraceHandleActive, 0) == 1)
                    {
                        CloseTrace(context->TraceHandle);
                    }
                }
                break;
            }
        }
    }
}

static ULONG ProcessDotNetTrace(
    _In_ PASMPAGE_QUERY_CONTEXT Context
    )
{
    ULONG result;
    TRACEHANDLE traceHandle;
    EVENT_TRACE_LOGFILE logFile;

    memset(&logFile, 0, sizeof(EVENT_TRACE_LOGFILE));
    logFile.LoggerName = DotNetLoggerName.Buffer;
    logFile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logFile.BufferCallback = DotNetBufferCallback;
    logFile.EventRecordCallback = DotNetEventCallback;
    logFile.Context = Context;

    traceHandle = OpenTrace(&logFile);

    if (traceHandle == INVALID_PROCESSTRACE_HANDLE)
        return GetLastError();

    Context->TraceHandleActive = 1;
    Context->TraceHandle = traceHandle;
    result = ProcessTrace(&traceHandle, 1, NULL, NULL);

    if (_InterlockedExchange(&Context->TraceHandleActive, 0) == 1)
    {
        CloseTrace(traceHandle);
    }

    return result;
}

NTSTATUS UpdateDotNetTraceInfoThreadStart(
    _In_ PVOID Parameter
    )
{
    PASMPAGE_QUERY_CONTEXT context = (PASMPAGE_QUERY_CONTEXT)Parameter;
    TRACEHANDLE sessionHandle;
    PEVENT_TRACE_PROPERTIES properties;
    PGUID guidToEnable;

    context->TraceResult = StartDotNetTrace(&sessionHandle, &properties);

    if (context->TraceResult != 0)
        return context->TraceResult;

    if (context->TraceClrV2)
        guidToEnable = &ClrRuntimeProviderGuid;
    else
        guidToEnable = &ClrRundownProviderGuid;

    EnableTraceEx(
        guidToEnable,
        NULL,
        sessionHandle,
        1,
        TRACE_LEVEL_INFORMATION,
        CLR_LOADER_KEYWORD | CLR_STARTENUMERATION_KEYWORD,
        0,
        0,
        NULL
        );

    context->TraceResult = ProcessDotNetTrace(context);

    ControlTrace(sessionHandle, NULL, properties, EVENT_TRACE_CONTROL_STOP);
    PhFree(properties);

    return context->TraceResult;
}

ULONG UpdateDotNetTraceInfoWithTimeout(
    _In_ PASMPAGE_QUERY_CONTEXT Context,
    _In_ BOOLEAN ClrV2,
    _In_opt_ PLARGE_INTEGER Timeout
    )
{
    HANDLE threadHandle;
    BOOLEAN timeout = FALSE;

    // ProcessDotNetTrace is not guaranteed to complete within any period of time, because
    // the target process might terminate before it writes the DCStartComplete_V1 event.
    // If the timeout is reached, the trace handle is closed, forcing ProcessTrace to stop
    // processing.

    Context->TraceClrV2 = ClrV2;
    Context->TraceResult = 0;
    Context->TraceHandleActive = 0;
    Context->TraceHandle = 0;

    if (!NT_SUCCESS(PhCreateThreadEx(&threadHandle, (PUSER_THREAD_START_ROUTINE)UpdateDotNetTraceInfoThreadStart, Context)))
        return ERROR_INVALID_HANDLE;

    if (NtWaitForSingleObject(threadHandle, FALSE, Timeout) != STATUS_WAIT_0)
    {
        // Timeout has expired. Stop the trace processing if it's still active.
        // BUG: This assumes that the thread is in ProcessTrace. It might still be
        // setting up though!
        if (_InterlockedExchange(&Context->TraceHandleActive, 0) == 1)
        {
            CloseTrace(Context->TraceHandle);
            timeout = TRUE;
        }

        NtWaitForSingleObject(threadHandle, FALSE, NULL);
    }

    NtClose(threadHandle);

    if (timeout)
        return ERROR_TIMEOUT;

    return Context->TraceResult;
}

VOID DestroyDotNetTraceQuery(
    _In_ PASMPAGE_QUERY_CONTEXT Context
    )
{
    if (Context->NodeRootList)
    {
        PhClearList(Context->NodeRootList);
        PhDereferenceObject(Context->NodeRootList);
    }

    if (Context->NodeList)
    {
        PhClearList(Context->NodeList);
        PhDereferenceObject(Context->NodeList);
    }

    PhFree(Context);
}

void CAssemblyEnum::AddNodes(CAssemblyListPtr& List, struct _PH_LIST* NodeList, quint64 ParrentId)
{
	for (ULONG i = 0; i < NodeList->Count; i++)
	{
		PDNA_NODE node = (PDNA_NODE)NodeList->Items[i];

		List->AddAssembly(node->Id, ParrentId, QString::fromWCharArray(node->StructureText.Buffer, node->StructureText.Length / sizeof(wchar_t)),
			CastPhString(node->PathText, false), CastPhString(node->FlagsText, false), CastPhString(node->NativePathText, false));

		if (node->Children)
			AddNodes(List, node->Children, node->Id);
	}
}

void CAssemblyEnum::run()
{
	PASMPAGE_QUERY_CONTEXT context;

    context = (PASMPAGE_QUERY_CONTEXT)PhAllocateZero(sizeof(ASMPAGE_QUERY_CONTEXT));
    //context->WindowHandle = Context->WindowHandle;
    context->ProcessId = (HANDLE)m_ProcessId;
    context->NodeList = PhCreateList(64);
    context->NodeRootList = PhCreateList(2);

	//if (isDotNet)
	{
		PhGetProcessIsDotNetEx(context->ProcessId, NULL, PH_CLR_USE_SECTION_CHECK, NULL, &context->ClrVersions);

		// DotNetTraceQueryThreadStart
		LARGE_INTEGER timeout;
		//PASMPAGE_QUERY_CONTEXT context = ;
		BOOLEAN timeoutReached = FALSE;
		BOOLEAN nonClrNode = FALSE;
		ULONG i;
		ULONG result = 0;

		if (context->ClrVersions & PH_CLR_VERSION_1_0)
		{
			AddFakeClrNode(context, L"CLR v1.0.3705"); // what PE displays
		}

		if (context->ClrVersions & PH_CLR_VERSION_1_1)
		{
			AddFakeClrNode(context, L"CLR v1.1.4322");
		}

		timeout.QuadPart = -(LONGLONG)UInt32x32To64(10, PH_TIMEOUT_SEC);

		if (context->ClrVersions & PH_CLR_VERSION_2_0)
		{
			context->ClrV2Node = AddFakeClrNode(context, L"CLR v2.0.50727");
			result = UpdateDotNetTraceInfoWithTimeout(context, TRUE, &timeout);

			if (result == ERROR_TIMEOUT)
			{
				timeoutReached = TRUE;
				result = ERROR_SUCCESS;
			}
		}

		if ((context->ClrVersions & PH_CLR_VERSION_4_ABOVE) || (context->ClrVersions & PH_CLR_CORELIB_PRESENT)) // PH_CLR_JIT_PRESENT CoreCLR support. (dmex)
		{
			result = UpdateDotNetTraceInfoWithTimeout(context, FALSE, &timeout);

			if (result == ERROR_TIMEOUT)
			{
				timeoutReached = TRUE;
				result = ERROR_SUCCESS;
			}
		}

		// If we reached the timeout, check whether we got any data back.
		if (timeoutReached)
		{
			for (i = 0; i < context->NodeList->Count; i++)
			{
				PDNA_NODE node = (PDNA_NODE)context->NodeList->Items[i];

				if (node->Type != DNA_TYPE_CLR)
				{
					nonClrNode = TRUE;
					break;
				}
			}

			if (!nonClrNode)
				result = ERROR_TIMEOUT;
		}

		//

		CAssemblyListPtr List = CAssemblyListPtr(new CAssemblyList());
		AddNodes(List, context->NodeRootList);

		emit Assemblies(List);
	}

	DestroyDotNetTraceQuery(context);

    // todo use this method
/*
    PCLR_PROCESS_SUPPORT support;
    PPH_LIST appdomainlist = NULL;
    BOOLEAN success = FALSE;

#ifdef _WIN64
    if (Context->IsWow64Process)
    {
        if (PhUiConnectToPhSvcEx(NULL, Wow64PhSvcMode, FALSE))
        {
            appdomainlist = CallGetClrAppDomainAssemblyList(Context->ProcessId);
            PhUiDisconnectFromPhSvc();
        }
    }
    else
#endif
    {
        if (support = CreateClrProcessSupport(Context->ProcessId))
        {
            appdomainlist = DnGetClrAppDomainAssemblyList(support);
            FreeClrProcessSupport(support);
        }
    }

    if (!appdomainlist)
        goto CleanupExit;

    for (ULONG i = 0; i < appdomainlist->Count; i++)
    {
        PDN_PROCESS_APPDOMAIN_ENTRY entry = appdomainlist->Items[i];
        PDNA_NODE parentNode;

        //if (!entry->AssemblyList)
        //    continue;

        parentNode = AddNode(Context);
        parentNode->Type = DNA_TYPE_APPDOMAIN;
        parentNode->u.AppDomain.AppDomainID = entry->AppDomainID;
        parentNode->u.AppDomain.AppDomainType = entry->AppDomainType;
        parentNode->u.AppDomain.DisplayName = PhConcatStrings2(L"AppDomain: ", entry->AppDomainName->Buffer);
        parentNode->StructureText = parentNode->u.AppDomain.DisplayName->sr;
        parentNode->IdText = FormatToHexString(entry->AppDomainID);
        parentNode->RootNode = TRUE;
        PhAddItemList(Context->NodeRootList, parentNode);

        if (entry->AssemblyList)
        {
            for (ULONG j = 0; j < entry->AssemblyList->Count; j++)
            {
                PDN_DOTNET_ASSEMBLY_ENTRY assembly = entry->AssemblyList->Items[j];
                PDNA_NODE childNode;

                //if (FindAssemblyNode3(Context, assembly->AssemblyID))
                //    continue;

                childNode = AddNode(Context);
                childNode->Type = DNA_TYPE_ASSEMBLY;
                childNode->u.Assembly.AssemblyID = assembly->AssemblyID;
                PhSetReference(&childNode->u.Assembly.DisplayName, assembly->DisplayName);
                PhSetReference(&childNode->u.Assembly.FullyQualifiedAssemblyName, assembly->ModuleName);
                childNode->u.Assembly.BaseAddress = assembly->BaseAddress;
                childNode->StructureText = PhGetStringRef(assembly->DisplayName);
                PhSetReference(&childNode->PathText, assembly->ModuleName);
                PhSetReference(&childNode->NativePathText, assembly->NativeFileName);
                childNode->MvidText = PhFormatGuid(&assembly->Mvid);
                childNode->IdText = FormatToHexString(assembly->AssemblyID);

                if (assembly->IsDynamicAssembly || assembly->ModuleFlag & CLRDATA_MODULE_IS_DYNAMIC || assembly->IsReflection)
                {
                    childNode->u.Assembly.AssemblyFlags = 0x2;
                }
                else if (!PhIsNullOrEmptyString(assembly->NativeFileName))
                {
                    childNode->u.Assembly.AssemblyFlags = 0x4;
                }

                childNode->FlagsText = FlagsToString(
                    childNode->u.Assembly.AssemblyFlags,
                    AssemblyFlagsMap,
                    sizeof(AssemblyFlagsMap)
                    );

                if (assembly->BaseAddress)
                {
                    WCHAR value[PH_INT64_STR_LEN_1];
                    PhPrintPointer(value, assembly->BaseAddress);
                    childNode->BaseAddressText = PhCreateString(value);
                }

                PhAddItemList(parentNode->Children, childNode);
            }
        }
    }

    DnDestroyProcessDotNetAppDomainList(appdomainlist);

    // Check whether we got any data.
    {
        for (ULONG i = 0; i < Context->NodeList->Count; i++)
        {
            PDNA_NODE node = Context->NodeList->Items[i];

            if (node->Type != DNA_TYPE_CLR)
            {
                success = TRUE;
                break;
            }
        }

        if (success && !Context->PageContext->CancelQueryContext && Context->PageContext->WindowHandle)
        {
            SendMessage(Context->PageContext->WindowHandle, DN_ASM_UPDATE_MSG, 0, (LPARAM)Context);
        }
    }

CleanupExit:
    if (!success && !Context->PageContext->CancelQueryContext && Context->PageContext->WindowHandle)
    {
        SendMessage(Context->PageContext->WindowHandle, DN_ASM_UPDATE_ERROR, 0, (LPARAM)Context);
    }

    PhDereferenceObject(Context);
*/

	emit Finished();
}