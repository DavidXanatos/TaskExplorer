/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     wj32    2016
 *     jxy-s   2020-2022
 *
 */

#include <kph.h>
#include <dyndata.h>

#include <trace.h>

typedef enum _KPH_KEY_TYPE
{
    KphKeyTypeTest,
    KphKeyTypeProd,
} KPH_KEY_TYPE, *PKPH_KEY_TYPE;

#define KPH_KEY_MATERIAL_SIZE ((ULONG)72)

typedef struct _KPH_KEY
{
    KPH_KEY_TYPE Type;
    BYTE Material[KPH_KEY_MATERIAL_SIZE];
} KPH_KEY, *PKPH_KEY;
typedef const KPH_KEY* PCKPH_KEY;

KPH_PROTECTED_DATA_SECTION_RO_PUSH();
static const KPH_KEY KphpPublicKeys[] =
{
    {
        KphKeyTypeProd,
        {
    0x45, 0x43, 0x53, 0x31, 0x20, 0x00, 0x00, 0x00,
    0x05, 0x7A, 0x12, 0x5A, 0xF8, 0x54, 0x01, 0x42,
    0xDB, 0x19, 0x87, 0xFC, 0xC4, 0xE3, 0xD3, 0x8D,
    0x46, 0x7B, 0x74, 0x01, 0x12, 0xFC, 0x78, 0xEB,
    0xEF, 0x7F, 0xF6, 0xAF, 0x4D, 0x9A, 0x3A, 0xF6,
    0x64, 0x90, 0xDB, 0xE3, 0x48, 0xAB, 0x3E, 0xA7,
    0x2F, 0xC1, 0x18, 0x32, 0xBD, 0x23, 0x02, 0x9D,
    0x3F, 0xF3, 0x27, 0x86, 0x71, 0x45, 0x26, 0x14,
    0x14, 0xF5, 0x19, 0xAA, 0x2D, 0xEE, 0x50, 0x10,
        }
    }
};
KPH_PROTECTED_DATA_SECTION_RO_POP();

KPH_PROTECTED_DATA_SECTION_PUSH();
static BCRYPT_ALG_HANDLE KphpBCryptSigningProvider = NULL;
static BCRYPT_KEY_HANDLE KphpPublicKeyHandles[ARRAYSIZE(KphpPublicKeys)] = { 0 };
static ULONG KphpPublicKeyHandlesCount = 0;
C_ASSERT(ARRAYSIZE(KphpPublicKeyHandles) == ARRAYSIZE(KphpPublicKeys));
KPH_PROTECTED_DATA_SECTION_POP();


PAGED_FILE();

static UNICODE_STRING KphpSigExtension = RTL_CONSTANT_STRING(L".sig");

/**
 * \brief Initializes verification infrastructure.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphInitializeVerify(
    VOID
    )
{
    NTSTATUS status;
    BOOLEAN testSigning;

    PAGED_CODE_PASSIVE();

    status = BCryptOpenAlgorithmProvider(&KphpBCryptSigningProvider,
                                         BCRYPT_ECDSA_P256_ALGORITHM,
                                         NULL,
                                         0);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_ERROR,
                      VERIFY,
                      "BCryptOpenAlgorithmProvider failed: %!STATUS!",
                      status);

        KphpBCryptSigningProvider = NULL;
        return status;
    }

    testSigning = KphTestSigningEnabled();

    for (ULONG i = 0; i < ARRAYSIZE(KphpPublicKeys); i++)
    {
        PCKPH_KEY key;
        BCRYPT_KEY_HANDLE keyHandle;

        key = &KphpPublicKeys[i];

        if (!testSigning && (key->Type == KphKeyTypeTest))
        {
            continue;
        }

        status = BCryptImportKeyPair(KphpBCryptSigningProvider,
                                     NULL,
                                     BCRYPT_ECCPUBLIC_BLOB,
                                     &keyHandle,
                                     (PUCHAR)key->Material,
                                     KPH_KEY_MATERIAL_SIZE,
                                     0);
        if (!NT_SUCCESS(status))
        {
            KphTracePrint(TRACE_LEVEL_ERROR,
                          VERIFY,
                          "BCryptImportKeyPair failed: %!STATUS!",
                          status);

            return status;
        }

        KphpPublicKeyHandles[KphpPublicKeyHandlesCount++] = keyHandle;
    }

    return status;
}

/**
 * \brief Cleans up verification infrastructure.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID KphCleanupVerify(
    VOID
    )
{
    PAGED_CODE_PASSIVE();

    for (ULONG i = 0; i < KphpPublicKeyHandlesCount; i++)
    {
        BCryptDestroyKey(KphpPublicKeyHandles[i]);
    }

    if (KphpBCryptSigningProvider)
    {
        BCryptCloseAlgorithmProvider(KphpBCryptSigningProvider, 0);
    }
}

/**
 * \brief Verifies that the specified signature matches the specified hash by
 * using one of the known public keys.
 *
 * \param[in] Hash The hash to verify.
 * \param[in] Signature The signature to check.
 * \param[in] SignatureLength The length of the signature.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphpVerifyHash(
    _In_ PKPH_HASH Hash,
    _In_ PBYTE Signature,
    _In_ ULONG SignatureLength
    )
{
    NTSTATUS status;

    PAGED_CODE_PASSIVE();

    status = STATUS_UNSUCCESSFUL;

    for (ULONG i = 0; i < KphpPublicKeyHandlesCount; i++)
    {
        status = BCryptVerifySignature(KphpPublicKeyHandles[i],
                                       NULL,
                                       Hash->Buffer,
                                       Hash->Size,
                                       Signature,
                                       SignatureLength,
                                       0);
        if (NT_SUCCESS(status))
        {
            return status;
        }
    }

    KphTracePrint(TRACE_LEVEL_VERBOSE,
                  VERIFY,
                  "BCryptVerifySignature failed: %!STATUS!",
                  status);

    return status;
}

/**
 * \brief Verifies a buffer matches the provided signature.
 *
 * \param[in] Buffer The buffer to verify.
 * \param[in] BufferLength The length of the buffer.
 * \param[in] Signature The signature to check.
 * \param[in] SignatureLength The length of the signature.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphVerifyBuffer(
    _In_ PBYTE Buffer,
    _In_ ULONG BufferLength,
    _In_ PBYTE Signature,
    _In_ ULONG SignatureLength
    )
{
    NTSTATUS status;
    KPH_HASH hash;

    PAGED_CODE_PASSIVE();

    status = KphHashBuffer(Buffer, BufferLength, CALG_SHA_256, &hash);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      VERIFY,
                      "KphHashBuffer failed: %!STATUS!",
                      status);

        goto Exit;
    }

    status = KphpVerifyHash(&hash, Signature, SignatureLength);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      VERIFY,
                      "KphpVerifyHash failed: %!STATUS!",
                      status);

        goto Exit;
    }

    status = STATUS_SUCCESS;

Exit:

    return status;
}

/**
 * \brief Verifies that a file matches the provided signature.
 *
 * \param[in] FileName File name of file to verify.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphVerifyFile(
    _In_ PUNICODE_STRING FileName
    )
{
    NTSTATUS status;
    KPH_HASH hash;
    UNICODE_STRING signatureFileName;
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE signatureFileHandle;
    IO_STATUS_BLOCK ioStatusBlock;
    BYTE signature[MINCRYPT_MAX_HASH_LEN];

    PAGED_CODE_PASSIVE();

    signatureFileHandle = NULL;
    RtlZeroMemory(&signatureFileName, sizeof(signatureFileName));

    if ((FileName->Length <= KphpSigExtension.Length) ||
        (FileName->Buffer[0] != L'\\'))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      VERIFY,
                      "File name is invalid \"%wZ\"",
                      FileName);

        status = STATUS_OBJECT_NAME_INVALID;
        goto Exit;
    }

    status = RtlDuplicateUnicodeString(0, FileName, &signatureFileName);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      VERIFY,
                      "RtlDuplicateUnicodeString failed: %!STATUS!",
                      status);

        RtlZeroMemory(&signatureFileName, sizeof(signatureFileName));
        goto Exit;
    }

    //
    // Replace the file extension with ".sig". Our verification requires that
    // the signature file be placed alongside the file we're verifying.
    //
    if (signatureFileName.Length < KphpSigExtension.Length)
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      VERIFY,
                      "Signature file name length invalid \"%wZ\"",
                      &signatureFileName);

        status = STATUS_OBJECT_NAME_INVALID;
        goto Exit;
    }

    signatureFileName.Length -= KphpSigExtension.Length;

    status = RtlAppendUnicodeStringToString(&signatureFileName,
                                            &KphpSigExtension);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      VERIFY,
                      "RtlAppendUnicodeStringToString failed: %!STATUS!",
                      status);

        goto Exit;
    }

    InitializeObjectAttributes(&objectAttributes,
                               &signatureFileName,
                               OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);

    status = KphCreateFile(&signatureFileHandle,
                           FILE_READ_ACCESS | SYNCHRONIZE,
                           &objectAttributes,
                           &ioStatusBlock,
                           NULL,
                           FILE_ATTRIBUTE_NORMAL,
                           FILE_SHARE_READ,
                           FILE_OPEN,
                           FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                           NULL,
                           0,
                           IO_IGNORE_SHARE_ACCESS_CHECK,
                           KernelMode);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      VERIFY,
                      "Failed to open signature file \"%wZ\": %!STATUS!",
                      &signatureFileName,
                      status);

        signatureFileHandle = NULL;
        goto Exit;
    }

    status = ZwReadFile(signatureFileHandle,
                        NULL,
                        NULL,
                        NULL,
                        &ioStatusBlock,
                        signature,
                        ARRAYSIZE(signature),
                        NULL,
                        NULL);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      VERIFY,
                      "Failed to read signature file \"%wZ\": %!STATUS!",
                      &signatureFileName,
                      status);

#ifdef SIG_NO_SECURITY
        if (status == STATUS_END_OF_FILE)
        {
            status = STATUS_SUCCESS;
        }
#endif

        goto Exit;
    }

    NT_ASSERT(ioStatusBlock.Information <= ARRAYSIZE(signature));

    status = KphHashFileByName(FileName, CALG_SHA_256, &hash);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      VERIFY,
                      "KphHashFileByName failed: %!STATUS!",
                      status);

        goto Exit;
    }

    status = KphpVerifyHash(&hash, signature, (ULONG)ioStatusBlock.Information);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      VERIFY,
                      "KphpVerifyHash failed: %!STATUS!",
                      status);

        goto Exit;
    }

    status = STATUS_SUCCESS;

Exit:

    RtlFreeUnicodeString(&signatureFileName);

    if (signatureFileHandle)
    {
        ObCloseHandle(signatureFileHandle, KernelMode);
    }

    return status;
}

/**
 * \brief Performs a domination check between a calling process and a target process.
 *
 * \details A process dominates the other when the protected level of the
 * process exceeds the other. This domination check is not ideal, it is overly
 * strict and lacks enough information from the kernel to fully understand the
 * protected process state.
 *
 * \param[in] Process - Calling process.
 * \param[in] ProcessTarget - Target process to check that the calling process
 * dominates.
 * \param[in] AccessMode - Access mode of the request, if KernelMode the
 * domination check is bypassed.
 *
 * \return Appropriate status:
 * STATUS_SUCCESS - The calling process dominates the target, or the target is not protected
 * STATUS_ACCESS_DENIED - The calling process does not dominate the target.
*/
_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS KphDominationCheck(
    _In_ PEPROCESS Process,
    _In_ PEPROCESS ProcessTarget,
    _In_ KPROCESSOR_MODE AccessMode
    )
{
    PS_PROTECTION processProtection;
    PS_PROTECTION targetProtection;

    PAGED_CODE();

    if (AccessMode == KernelMode)
    {
        //
        // Give the kernel what it wants...
        //
        return STATUS_SUCCESS;
    }

    //
    // Until Microsoft gives us more insight into protected process domination
    // we'll do a very strict check here:
    //

    processProtection = PsGetProcessProtection(Process);
    targetProtection = PsGetProcessProtection(ProcessTarget);

    if ((targetProtection.Type != PsProtectedTypeNone) &&
        (targetProtection.Type >= processProtection.Type))
    {
        //
        // Calling process protection does not dominate the other, deny access.
        // We could do our own domination check/mapping here with the signing
        // level, but it won't be great and Microsoft might change it, so we'll
        // do this strict check until Microsoft exports:
        // PsTestProtectedProcessIncompatibility
        // RtlProtectedAccess/RtlTestProtectedAccess
        //
        return STATUS_ACCESS_DENIED;
    }

    return STATUS_SUCCESS;
}

/**
 * \brief Retrieves the process state mask from a process.
 *
 * \param[in] Process The process to get the state from.
 *
 * \return State mask describing what state the process is in.
 */
_IRQL_requires_max_(APC_LEVEL)
KPH_PROCESS_STATE KphGetProcessState(
    _In_ PKPH_PROCESS_CONTEXT Process
    )
{
    KPH_PROCESS_STATE processState;

    PAGED_CODE();

    if (KphProtectionsSuppressed())
    {
        //
        // This ultimately permits low state callers into the driver. But still
        // check for verification. We still want to exercise the code below,
        // regardless.
        //
        processState = ~KPH_PROCESS_VERIFIED_PROCESS;
    }
    else
    {
        processState = 0;
    }

    if (Process->SecurelyCreated)
    {
        processState |= KPH_PROCESS_SECURELY_CREATED;
    }

    if (Process->VerifiedProcess)
    {
        processState |= KPH_PROCESS_VERIFIED_PROCESS;
    }

    if (Process->Protected)
    {
        processState |= KPH_PROCESS_PROTECTED_PROCESS;
    }

    if (Process->NumberOfUntrustedImageLoads == 0)
    {
        processState |= KPH_PROCESS_NO_UNTRUSTED_IMAGES;
    }

    if (!PsIsProcessBeingDebugged(Process->EProcess))
    {
        processState |= KPH_PROCESS_NOT_BEING_DEBUGGED;
    }

    if (!Process->FileObject)
    {
        return processState;
    }

    processState |= KPH_PROCESS_HAS_FILE_OBJECT;

    if (!IoGetTransactionParameterBlock(Process->FileObject))
    {
        processState |= KPH_PROCESS_NO_FILE_TRANSACTION;
    }

    if (!Process->FileObject->SectionObjectPointer)
    {
        return processState;
    }

    processState |= KPH_PROCESS_HAS_SECTION_OBJECT_POINTERS;

    if (!MmDoesFileHaveUserWritableReferences(Process->FileObject->SectionObjectPointer))
    {
        processState |= KPH_PROCESS_NO_USER_WRITABLE_REFERENCES;
    }

    return processState;
}
