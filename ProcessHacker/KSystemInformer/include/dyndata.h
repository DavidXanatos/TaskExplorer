/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     jxy-s   2022-2023
 *
 */

#pragma once

#ifdef EXT
#undef EXT
#endif

#ifdef _DYNDATA_PRIVATE
KPH_PROTECTED_DATA_SECTION_PUSH();
#define EXT
#define OFFDEFAULT = ULONG_MAX
#define DYNIMPORTDEFAULT = NULL
#define DYNPTRDEFAULT = NULL
#else
#define EXT extern
#define OFFDEFAULT
#define DYNIMPORTDEFAULT
#define DYNPTRDEFAULT
#endif

EXT ULONG KphDynEgeGuid OFFDEFAULT;
EXT ULONG KphDynEpObjectTable OFFDEFAULT;
EXT ULONG KphDynEreGuidEntry OFFDEFAULT;
EXT ULONG KphDynHtHandleContentionEvent OFFDEFAULT;
EXT ULONG KphDynOtName OFFDEFAULT;
EXT ULONG KphDynOtIndex OFFDEFAULT;
EXT ULONG KphDynObDecodeShift OFFDEFAULT;
EXT ULONG KphDynObAttributesShift OFFDEFAULT;
EXT ULONG KphDynAlpcCommunicationInfo OFFDEFAULT;
EXT ULONG KphDynAlpcOwnerProcess OFFDEFAULT;
EXT ULONG KphDynAlpcConnectionPort OFFDEFAULT;
EXT ULONG KphDynAlpcServerCommunicationPort OFFDEFAULT;
EXT ULONG KphDynAlpcClientCommunicationPort OFFDEFAULT;
EXT ULONG KphDynAlpcHandleTable OFFDEFAULT;
EXT ULONG KphDynAlpcHandleTableLock OFFDEFAULT;
EXT ULONG KphDynAlpcAttributes OFFDEFAULT;
EXT ULONG KphDynAlpcAttributesFlags OFFDEFAULT;
EXT ULONG KphDynAlpcPortContext OFFDEFAULT;
EXT ULONG KphDynAlpcPortObjectLock OFFDEFAULT;
EXT ULONG KphDynAlpcSequenceNo OFFDEFAULT;
EXT ULONG KphDynAlpcState OFFDEFAULT;
EXT ULONG KphDynKtReadOperationCount OFFDEFAULT;
EXT ULONG KphDynKtWriteOperationCount OFFDEFAULT;
EXT ULONG KphDynKtOtherOperationCount OFFDEFAULT;
EXT ULONG KphDynKtReadTransferCount OFFDEFAULT;
EXT ULONG KphDynKtWriteTransferCount OFFDEFAULT;
EXT ULONG KphDynKtOtherTransferCount OFFDEFAULT;
EXT ULONG KphDynLxPicoProc OFFDEFAULT;
EXT ULONG KphDynLxPicoProcInfo OFFDEFAULT;
EXT ULONG KphDynLxPicoProcInfoPID OFFDEFAULT;
EXT ULONG KphDynLxPicoThrdInfo OFFDEFAULT;
EXT ULONG KphDynLxPicoThrdInfoTID OFFDEFAULT;
EXT ULONG KphDynMmSectionControlArea OFFDEFAULT;
EXT ULONG KphDynMmControlAreaListHead OFFDEFAULT;
EXT ULONG KphDynMmControlAreaLock OFFDEFAULT;

EXT PPS_SET_LOAD_IMAGE_NOTIFY_ROUTINE_EX KphDynPsSetLoadImageNotifyRoutineEx DYNIMPORTDEFAULT;
EXT PPS_SET_CREATE_PROCESS_NOTIFY_ROUTINE_EX2 KphDynPsSetCreateProcessNotifyRoutineEx2 DYNIMPORTDEFAULT;
EXT PCI_FREE_POLICY_INFO KphDynCiFreePolicyInfo DYNIMPORTDEFAULT;
EXT PCI_VERIFY_HASH_IN_CATALOG KphDynCiVerifyHashInCatalog DYNIMPORTDEFAULT;
EXT PCI_CHECK_SIGNED_FILE KphDynCiCheckSignedFile DYNIMPORTDEFAULT;
EXT PCI_VERIFY_HASH_IN_CATALOG_EX KphDynCiVerifyHashInCatalogEx DYNIMPORTDEFAULT;
EXT PCI_CHECK_SIGNED_FILE_EX KphDynCiCheckSignedFileEx DYNIMPORTDEFAULT;
EXT PMM_PROTECT_DRIVER_SECTION KphDynMmProtectDriverSection DYNIMPORTDEFAULT;
EXT PLXP_THREAD_GET_CURRENT KphDynLxpThreadGetCurrent DYNIMPORTDEFAULT;

EXT ULONG KphDynDisableImageLoadProtection OFFDEFAULT;

#ifdef _DYNDATA_PRIVATE
KPH_PROTECTED_DATA_SECTION_POP();
#endif

EXT PUNICODE_STRING KphDynPortName DYNPTRDEFAULT;
EXT PUNICODE_STRING KphDynAltitude DYNPTRDEFAULT;

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphOpenParametersKey(
    _In_ PUNICODE_STRING RegistryPath,
    _Out_ PHANDLE KeyHandle
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphDynamicDataInitialization(
    _In_ PUNICODE_STRING RegistryPath
    );

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID KphDynamicDataCleanup(
    VOID
    );
