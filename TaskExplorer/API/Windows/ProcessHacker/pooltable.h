#pragma once

// pool.c

typedef struct _POOL_TAG_LIST_ENTRY
{
    LIST_ENTRY ListEntry;

    union
    {
        UCHAR Tag[4];
        ULONG TagUlong;
    };

    PPH_STRING BinaryNameString;
    PPH_STRING DescriptionString;
} POOL_TAG_LIST_ENTRY, *PPOOL_TAG_LIST_ENTRY;

NTSTATUS EnumPoolTagTable(
    _Out_ PVOID* Buffer
    );

NTSTATUS EnumBigPoolTable(
    _Out_ PVOID* Buffer
    );


// db.c

typedef struct _POOLTAG_CONTEXT
{
    PH_QUEUED_LOCK PoolTagListLock;
    PPH_LIST PoolTagDbList;
    PPH_HASHTABLE PoolTagDbHashtable;  

} POOLTAG_CONTEXT, *PPOOLTAG_CONTEXT;


VOID LoadPoolTagDatabase(
    _In_ PPOOLTAG_CONTEXT Context
    );

VOID FreePoolTagDatabase(
    _In_ PPOOLTAG_CONTEXT Context
    );

QPair<QString, QString> UpdatePoolTagBinaryName(
    _In_ PPOOLTAG_CONTEXT Context,
    _In_ ULONG TagUlong
    );