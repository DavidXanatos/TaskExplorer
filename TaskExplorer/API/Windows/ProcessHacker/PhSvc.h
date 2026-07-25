#pragma once

#include "../ProcessHacker.h"

// todo: add count
extern PH_KEY_VALUE_PAIR PhpServiceTypePairs[];
extern PH_KEY_VALUE_PAIR PhpServiceStartTypePairs[];
extern PH_KEY_VALUE_PAIR PhpServiceErrorControlPairs[];

LPENUM_SERVICE_STATUS EsEnumDependentServices(
	_In_ SC_HANDLE ServiceHandle,
	_In_opt_ ULONG State,
	_Out_ PULONG Count
);

typedef struct _ES_TRIGGER_INFO
{
	ULONG Type;
	PGUID Subtype;
	ULONG Action;
	PPH_LIST DataList;
	GUID SubtypeBuffer;
} ES_TRIGGER_INFO, *PES_TRIGGER_INFO;

typedef struct _TYPE_ENTRY
{
	ULONG Type;
	PCWSTR Name;
} TYPE_ENTRY, PTYPE_ENTRY;

typedef struct _SUBTYPE_ENTRY
{
	ULONG Type;
	PGUID Guid;
	PCWSTR Name;
} SUBTYPE_ENTRY, PSUBTYPE_ENTRY;

extern GUID NetworkManagerFirstIpAddressArrivalGuid;
extern GUID NetworkManagerLastIpAddressRemovalGuid;
extern GUID DomainJoinGuid;
extern GUID DomainLeaveGuid;
extern GUID FirewallPortOpenGuid;
extern GUID FirewallPortCloseGuid;
extern GUID MachinePolicyPresentGuid;
extern GUID UserPolicyPresentGuid;
extern GUID RpcInterfaceEventGuid;
extern GUID NamedPipeEventGuid;
extern GUID SubTypeUnknownGuid; // dummy

// todo: add count
extern TYPE_ENTRY TypeEntries[];
extern SUBTYPE_ENTRY SubTypeEntries[];

PPH_STRING EspLookupEtwPublisherName(
	_In_ PGUID Guid
);

VOID EspFormatTriggerInfo(
	_In_ PES_TRIGGER_INFO Info,
	_Out_ PWSTR *TriggerString,
	_Out_ PWSTR *ActionString,
	_Out_ PPH_STRING *StringUsed
);

typedef struct _ES_TRIGGER_DATA
{
	ULONG Type;
	union
	{
		PPH_STRING String;
		struct
		{
			PVOID Binary;
			ULONG BinaryLength;
		};
		UCHAR Byte;
		ULONG64 UInt64;
	};
} ES_TRIGGER_DATA, *PES_TRIGGER_DATA;

PES_TRIGGER_DATA EspCreateTriggerData(
	_In_opt_ PSERVICE_TRIGGER_SPECIFIC_DATA_ITEM DataItem
);

PES_TRIGGER_DATA EspCloneTriggerData(
	_In_ PES_TRIGGER_DATA Data
);

VOID EspDestroyTriggerData(
	_In_ PES_TRIGGER_DATA Data
);

PES_TRIGGER_INFO EspCreateTriggerInfo(
	_In_opt_ PSERVICE_TRIGGER Trigger
);

PES_TRIGGER_INFO EspCloneTriggerInfo(
	_In_ PES_TRIGGER_INFO Info
);

VOID EspDestroyTriggerInfo(
	_In_ PES_TRIGGER_INFO Info
);

typedef struct _ETW_PUBLISHER_ENTRY
{
    PPH_STRING PublisherName;
    GUID Guid;
} ETW_PUBLISHER_ENTRY, *PETW_PUBLISHER_ENTRY;

BOOLEAN EspEnumerateEtwPublishers(
	_Out_ PETW_PUBLISHER_ENTRY *Entries,
	_Out_ PULONG NumberOfEntries
);

BOOLEAN EspLookupEtwPublisherGuid(
	_In_ PWSTR PublisherName,
	_Out_ PGUID Guid
);

PPH_STRING EspConvertNewLinesToNulls(
	_In_ PPH_STRING String
);

// todo: add count
extern PH_KEY_VALUE_PAIR EspServiceSidTypePairs[];
extern PH_KEY_VALUE_PAIR EspServiceLaunchProtectedPairs[];

PPH_STRING EspGetServiceSidString(
	_In_ PPH_STRINGREF ServiceName
);

ULONG EspGetServiceSidTypeInteger(
	_In_ PWSTR SidType
);

// todo: add count
extern PH_KEY_VALUE_PAIR ServiceActionPairs[];