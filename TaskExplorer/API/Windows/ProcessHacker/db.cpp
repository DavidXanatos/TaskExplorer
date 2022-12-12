/*
 * System Informer -
 *   functions from Pool Table Plugin
 *
 * Copyright (C) 2016 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 * 
 */

#include "stdafx.h"
#include "../ProcessHacker.h"
#include "pooltable.h"

PPH_STRING PoolTrimString(
    _In_ PPH_STRING String
    )
{
    static PH_STRINGREF whitespace = PH_STRINGREF_INIT(L" \t");
    PH_STRINGREF sr = String->sr;
    PhTrimStringRef(&sr, &whitespace, 0);
    return PhCreateString2(&sr);
}

PPH_STRING FindPoolTagFilePath(
    VOID
    )
{
    static struct
    {
        PH_STRINGREF AppendPath;
    } locations[] =
    {
#ifdef _WIN64
        { PH_STRINGREF_INIT(L"%ProgramFiles(x86)%\\Windows Kits\\10\\Debuggers\\x64\\triage\\pooltag.txt") },
        { PH_STRINGREF_INIT(L"%ProgramFiles(x86)%\\Windows Kits\\8.1\\Debuggers\\x64\\triage\\pooltag.txt") },
        { PH_STRINGREF_INIT(L"%ProgramFiles(x86)%\\Windows Kits\\8.0\\Debuggers\\x64\\triage\\pooltag.txt") },
        { PH_STRINGREF_INIT(L"%ProgramFiles%\\Debugging Tools for Windows (x64)\\triage\\pooltag.txt") }
#else
        { PH_STRINGREF_INIT(L"%ProgramFiles%\\Windows Kits\\10\\Debuggers\\x86\\triage\\pooltag.txt") },
        { PH_STRINGREF_INIT(L"%ProgramFiles%\\Windows Kits\\8.1\\Debuggers\\x86\\triage\\pooltag.txt") },
        { PH_STRINGREF_INIT(L"%ProgramFiles%\\Windows Kits\\8.0\\Debuggers\\x86\\triage\\pooltag.txt") },
        { PH_STRINGREF_INIT(L"%ProgramFiles%\\Debugging Tools for Windows (x86)\\triage\\pooltag.txt") }
#endif
    };
 
    PPH_STRING path;
    ULONG i;
   
    for (i = 0; i < RTL_NUMBER_OF(locations); i++)
    {
        if (path = PhExpandEnvironmentStrings(&locations[i].AppendPath))
        {
            if (RtlDoesFileExists_U(path->Buffer))
                return path;

            PhDereferenceObject(path);
        }
    }

    return NULL;
}


BOOLEAN NTAPI PmPoolTagListHashtableEqualFunction(
    _In_ PVOID Entry1,
    _In_ PVOID Entry2
    )
{
    PPOOL_TAG_LIST_ENTRY poolTagNode1 = *(PPOOL_TAG_LIST_ENTRY *)Entry1;
    PPOOL_TAG_LIST_ENTRY poolTagNode2 = *(PPOOL_TAG_LIST_ENTRY *)Entry2;

    return poolTagNode1->TagUlong == poolTagNode2->TagUlong;
}

ULONG NTAPI PmPoolTagListHashtableHashFunction(
    _In_ PVOID Entry
    )
{
    return (*(PPOOL_TAG_LIST_ENTRY*)Entry)->TagUlong;
}

PPOOL_TAG_LIST_ENTRY FindPoolTagListEntry(
    _In_ PPOOLTAG_CONTEXT Context,
    _In_ ULONG PoolTag
    )
{
    POOL_TAG_LIST_ENTRY lookupWindowNode;
    PPOOL_TAG_LIST_ENTRY lookupWindowNodePtr = &lookupWindowNode;
    PPOOL_TAG_LIST_ENTRY *windowNode;

    lookupWindowNode.TagUlong = PoolTag;

    windowNode = (PPOOL_TAG_LIST_ENTRY*)PhFindEntryHashtable(
        Context->PoolTagDbHashtable,
        &lookupWindowNodePtr
        );

    if (windowNode)
        return *windowNode;
    else
        return NULL;
}

VOID LoadPoolTagDatabase(
    _In_ PPOOLTAG_CONTEXT Context
    )
{
    static PH_STRINGREF skipPoolTagFileHeader = PH_STRINGREF_INIT(L"\r\n\r\n");
    static PH_STRINGREF skipPoolTagFileLine = PH_STRINGREF_INIT(L"\r\n");

    PPH_STRING poolTagFilePath;
    HANDLE fileHandle = NULL;
    LARGE_INTEGER fileSize;
    ULONG stringBufferLength;
    PSTR stringBuffer;
    PPH_STRING utf16StringBuffer = NULL;
    IO_STATUS_BLOCK isb;

    Context->PoolTagDbList = PhCreateList(100);
    Context->PoolTagDbHashtable = PhCreateHashtable(
        sizeof(PPOOL_TAG_LIST_ENTRY),
        PmPoolTagListHashtableEqualFunction,
        PmPoolTagListHashtableHashFunction,
        100
        );

	poolTagFilePath = poolTagFilePath = FindPoolTagFilePath();

	if (poolTagFilePath == NULL)
	{
		PPH_STRING applicationDirectory;
		if (applicationDirectory = PhGetApplicationDirectory())
		{
			poolTagFilePath = PhConcatStringRefZ(&applicationDirectory->sr, L"pooltag.txt");
			PhDereferenceObject(applicationDirectory);

			if (!RtlDoesFileExists_U(poolTagFilePath->Buffer))
			{
				PhDereferenceObject(poolTagFilePath);
				poolTagFilePath = NULL;
			}
		}
	}

    if (poolTagFilePath != NULL)
    {
        if (!NT_SUCCESS(PhCreateFileWin32(
            &fileHandle,
            poolTagFilePath->Buffer,
            FILE_GENERIC_READ,
            0,
            FILE_SHARE_READ | FILE_SHARE_DELETE,
            FILE_OPEN,
            FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
            )))
        {
            PhDereferenceObject(poolTagFilePath);
            return;
        }

        if (!NT_SUCCESS(PhGetFileSize(fileHandle, &fileSize)))
        {
            NtClose(fileHandle);
            PhDereferenceObject(poolTagFilePath);
            return;
        }

        if (fileSize.QuadPart == 0)
        {
            NtClose(fileHandle);
            PhDereferenceObject(poolTagFilePath);
            return;
        }

        stringBufferLength = (ULONG)fileSize.QuadPart + 1;
        stringBuffer = (PSTR)PhAllocate(stringBufferLength);
        memset(stringBuffer, 0, stringBufferLength);

        if (NT_SUCCESS(NtReadFile(
            fileHandle,
            NULL,
            NULL,
            NULL,
            &isb,
            stringBuffer,
            (ULONG)fileSize.QuadPart,
            NULL,
            NULL
            )))
        {
            utf16StringBuffer = PhZeroExtendToUtf16(stringBuffer);
        }

        PhFree(stringBuffer);
        NtClose(fileHandle);
        PhDereferenceObject(poolTagFilePath);
    }
    /*else
    {
        HRSRC resourceHandle;

        // Find the resource
        if (resourceHandle = FindResource(
            PluginInstance->DllBase,
            MAKEINTRESOURCE(IDR_TXT_POOLTAGS),
            L"TXT")
            )
        {
            ULONG resourceLength;
            HGLOBAL resourceBuffer;
            PSTR utf16Buffer;

            // Get the resource length
            resourceLength = SizeofResource(PluginInstance->DllBase, resourceHandle);

            // Load the resource
            if (resourceBuffer = LoadResource(PluginInstance->DllBase, resourceHandle))
            {
                utf16Buffer = (PSTR)LockResource(resourceBuffer);

                utf16StringBuffer = PhZeroExtendToUtf16Ex(utf16Buffer, resourceLength);
            }

            FreeResource(resourceBuffer);
        }
    }*/

    if (utf16StringBuffer)
    {
        PH_STRINGREF firstPart;
        PH_STRINGREF remainingPart;
        PH_STRINGREF poolTagPart;
        PH_STRINGREF binaryNamePart;
        PH_STRINGREF descriptionPart;

        remainingPart = utf16StringBuffer->sr;

        PhSplitStringRefAtString(&remainingPart, &skipPoolTagFileHeader, TRUE, &firstPart, &remainingPart);

        while (remainingPart.Length != 0)
        {
            PhSplitStringRefAtString(&remainingPart, &skipPoolTagFileLine, TRUE, &firstPart, &remainingPart);

            if (firstPart.Length != 0)
            {
                PPOOL_TAG_LIST_ENTRY entry;
                PPH_STRING poolTagString;
                PPH_STRING poolTag;
                PPH_STRING binaryName;
                PPH_STRING description;

                if (!PhSplitStringRefAtChar(&firstPart, '-', &poolTagPart, &firstPart))
                    continue;
                if (!PhSplitStringRefAtChar(&firstPart, '-', &binaryNamePart, &firstPart))
                    continue;
                // Note: Some entries don't have descriptions
                PhSplitStringRefAtChar(&firstPart, '-', &descriptionPart, &firstPart);

                poolTag = PhCreateString2(&poolTagPart);
                binaryName = PhCreateString2(&binaryNamePart);
                description = PhCreateString2(&descriptionPart);

                entry = (PPOOL_TAG_LIST_ENTRY)PhAllocate(sizeof(POOL_TAG_LIST_ENTRY));
                memset(entry, 0, sizeof(POOL_TAG_LIST_ENTRY));

                // Strip leading/trailing space characters.
                poolTagString = PoolTrimString(poolTag);
                entry->BinaryNameString = PoolTrimString(binaryName);
                entry->DescriptionString = PoolTrimString(description);

                // Convert the poolTagString to ULONG
                PhConvertUtf16ToUtf8Buffer(
                    (char*)entry->Tag,
                    sizeof(entry->Tag),
                    NULL,
                    poolTagString->Buffer,
                    poolTagString->Length
                    );

                PhAcquireQueuedLockExclusive(&Context->PoolTagListLock);
                PhAddEntryHashtable(Context->PoolTagDbHashtable, &entry);
                PhAddItemList(Context->PoolTagDbList, entry);
                PhReleaseQueuedLockExclusive(&Context->PoolTagListLock);

                PhDereferenceObject(description);
                PhDereferenceObject(binaryName);
                PhDereferenceObject(poolTag);
                PhDereferenceObject(poolTagString);
            }
        }

        PhDereferenceObject(utf16StringBuffer);
    }
}

VOID FreePoolTagDatabase(
    _In_ PPOOLTAG_CONTEXT Context
    )
{
    PhAcquireQueuedLockExclusive(&Context->PoolTagListLock);

    for (ULONG i = 0; i < Context->PoolTagDbList->Count; i++)
    {
        PPOOL_TAG_LIST_ENTRY entry = (PPOOL_TAG_LIST_ENTRY)Context->PoolTagDbList->Items[i];

        PhDereferenceObject(entry->DescriptionString);
        PhDereferenceObject(entry->BinaryNameString);
        PhFree(entry);
    }
 
    PhClearHashtable(Context->PoolTagDbHashtable);
    PhClearList(Context->PoolTagDbList);

    PhReleaseQueuedLockExclusive(&Context->PoolTagListLock);
}

QPair<QString, QString> UpdatePoolTagBinaryName(
    _In_ PPOOLTAG_CONTEXT Context,
    _In_ ULONG TagUlong
    )
{
	QPair<QString, QString> Ret;

    PPOOL_TAG_LIST_ENTRY client;
    if (client = FindPoolTagListEntry(Context, TagUlong))
    {
		Ret.first = CastPhString(client->BinaryNameString, false);
		Ret.second = CastPhString(client->DescriptionString, false);

        //if (PhStartsWithString2(PoolEntry->BinaryNameString, L"nt!", FALSE))
        //    PoolEntry->Type = TPOOLTAG_TREE_ITEM_TYPE_OBJECT;

        //if (PhEndsWithString2(PoolEntry->BinaryNameString, L".sys", FALSE))
            //PoolEntry->Type = TPOOLTAG_TREE_ITEM_TYPE_DRIVER;
    }

	return Ret;
}

