/*
 * Process Hacker -
 *   functions from the ExtendedServices pluggin
 *
 * Copyright (C) 2009-2015 wj32
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


#include "stdafx.h"
#include "PhSvc.h"

#define SIP(String, Integer) { (String), (PVOID)(Integer) }

PH_KEY_VALUE_PAIR PhpServiceTypePairs[] =
{
	SIP("Driver", SERVICE_KERNEL_DRIVER),
	SIP("FS driver", SERVICE_FILE_SYSTEM_DRIVER),
	SIP("Own process", SERVICE_WIN32_OWN_PROCESS),
	SIP("Share process", SERVICE_WIN32_SHARE_PROCESS),
	SIP("Own interactive process", SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS),
	SIP("Share interactive process", SERVICE_WIN32_SHARE_PROCESS | SERVICE_INTERACTIVE_PROCESS),
	SIP("User own process", SERVICE_USER_OWN_PROCESS),
	SIP("User own process (instance)", SERVICE_USER_OWN_PROCESS | SERVICE_USERSERVICE_INSTANCE),
	SIP("User share process", SERVICE_USER_SHARE_PROCESS),
	SIP("User share process (instance)", SERVICE_USER_SHARE_PROCESS | SERVICE_USERSERVICE_INSTANCE)
};

PH_KEY_VALUE_PAIR PhpServiceStartTypePairs[] =
{
	SIP("Disabled", SERVICE_DISABLED),
	SIP("Boot start", SERVICE_BOOT_START),
	SIP("System start", SERVICE_SYSTEM_START),
	SIP("Auto start", SERVICE_AUTO_START),
	SIP("Demand start", SERVICE_DEMAND_START)
};

PH_KEY_VALUE_PAIR PhpServiceErrorControlPairs[] =
{
	SIP("Ignore", SERVICE_ERROR_IGNORE),
	SIP("Normal", SERVICE_ERROR_NORMAL),
	SIP("Severe", SERVICE_ERROR_SEVERE),
	SIP("Critical", SERVICE_ERROR_CRITICAL)
};

LPENUM_SERVICE_STATUS EsEnumDependentServices(
	_In_ SC_HANDLE ServiceHandle,
	_In_opt_ ULONG State,
	_Out_ PULONG Count
)
{
	LOGICAL result;
	LPENUM_SERVICE_STATUSW buffer;
	ULONG bufferSize;
	ULONG returnLength;
	ULONG servicesReturned;

	if (!State)
		State = SERVICE_STATE_ALL;

	bufferSize = 0x800;
	buffer = (LPENUM_SERVICE_STATUSW)PhAllocate(bufferSize);

	if (!(result = EnumDependentServices(
		ServiceHandle,
		State,
		buffer,
		bufferSize,
		&returnLength,
		&servicesReturned
	)))
	{
		if (GetLastError() == ERROR_MORE_DATA)
		{
			PhFree(buffer);
			bufferSize = returnLength;
			buffer = (LPENUM_SERVICE_STATUSW)PhAllocate(bufferSize);

			result = EnumDependentServices(
				ServiceHandle,
				State,
				buffer,
				bufferSize,
				&returnLength,
				&servicesReturned
			);
		}

		if (!result)
		{
			PhFree(buffer);
			return NULL;
		}
	}

	*Count = servicesReturned;

	return buffer;
}


GUID NetworkManagerFirstIpAddressArrivalGuid = { 0x4f27f2de, 0x14e2, 0x430b, { 0xa5, 0x49, 0x7c, 0xd4, 0x8c, 0xbc, 0x82, 0x45 } };
GUID NetworkManagerLastIpAddressRemovalGuid = { 0xcc4ba62a, 0x162e, 0x4648, { 0x84, 0x7a, 0xb6, 0xbd, 0xf9, 0x93, 0xe3, 0x35 } };
GUID DomainJoinGuid = { 0x1ce20aba, 0x9851, 0x4421, { 0x94, 0x30, 0x1d, 0xde, 0xb7, 0x66, 0xe8, 0x09 } };
GUID DomainLeaveGuid = { 0xddaf516e, 0x58c2, 0x4866, { 0x95, 0x74, 0xc3, 0xb6, 0x15, 0xd4, 0x2e, 0xa1 } };
GUID FirewallPortOpenGuid = { 0xb7569e07, 0x8421, 0x4ee0, { 0xad, 0x10, 0x86, 0x91, 0x5a, 0xfd, 0xad, 0x09 } };
GUID FirewallPortCloseGuid = { 0xa144ed38, 0x8e12, 0x4de4, { 0x9d, 0x96, 0xe6, 0x47, 0x40, 0xb1, 0xa5, 0x24 } };
GUID MachinePolicyPresentGuid = { 0x659fcae6, 0x5bdb, 0x4da9, { 0xb1, 0xff, 0xca, 0x2a, 0x17, 0x8d, 0x46, 0xe0 } };
GUID UserPolicyPresentGuid = { 0x54fb46c8, 0xf089, 0x464c, { 0xb1, 0xfd, 0x59, 0xd1, 0xb6, 0x2c, 0x3b, 0x50 } };
GUID RpcInterfaceEventGuid = { 0xbc90d167, 0x9470, 0x4139, { 0xa9, 0xba, 0xbe, 0x0b, 0xbb, 0xf5, 0xb7, 0x4d } };
GUID NamedPipeEventGuid = { 0x1f81d131, 0x3fac, 0x4537, { 0x9e, 0x0c, 0x7e, 0x7b, 0x0c, 0x2f, 0x4b, 0x55 } };
GUID SubTypeUnknownGuid; // dummy

TYPE_ENTRY TypeEntries[] =
{
	{ SERVICE_TRIGGER_TYPE_DEVICE_INTERFACE_ARRIVAL, L"Device interface arrival" },
	{ SERVICE_TRIGGER_TYPE_IP_ADDRESS_AVAILABILITY, L"IP address availability" },
	{ SERVICE_TRIGGER_TYPE_DOMAIN_JOIN, L"Domain join" },
	{ SERVICE_TRIGGER_TYPE_FIREWALL_PORT_EVENT, L"Firewall port event" },
	{ SERVICE_TRIGGER_TYPE_GROUP_POLICY, L"Group policy" },
	{ SERVICE_TRIGGER_TYPE_NETWORK_ENDPOINT, L"Network endpoint" },
	{ SERVICE_TRIGGER_TYPE_CUSTOM_SYSTEM_STATE_CHANGE, L"Custom system state change" },
	{ SERVICE_TRIGGER_TYPE_CUSTOM, L"Custom" }
};

SUBTYPE_ENTRY SubTypeEntries[] =
{
	{ SERVICE_TRIGGER_TYPE_IP_ADDRESS_AVAILABILITY, NULL, L"IP address" },
	{ SERVICE_TRIGGER_TYPE_IP_ADDRESS_AVAILABILITY, &NetworkManagerFirstIpAddressArrivalGuid, L"IP address: First arrival" },
	{ SERVICE_TRIGGER_TYPE_IP_ADDRESS_AVAILABILITY, &NetworkManagerLastIpAddressRemovalGuid, L"IP address: Last removal" },
	{ SERVICE_TRIGGER_TYPE_IP_ADDRESS_AVAILABILITY, &SubTypeUnknownGuid, L"IP address: Unknown" },
	{ SERVICE_TRIGGER_TYPE_DOMAIN_JOIN, NULL, L"Domain" },
	{ SERVICE_TRIGGER_TYPE_DOMAIN_JOIN, &DomainJoinGuid, L"Domain: Join" },
	{ SERVICE_TRIGGER_TYPE_DOMAIN_JOIN, &DomainLeaveGuid, L"Domain: Leave" },
	{ SERVICE_TRIGGER_TYPE_DOMAIN_JOIN, &SubTypeUnknownGuid, L"Domain: Unknown" },
	{ SERVICE_TRIGGER_TYPE_FIREWALL_PORT_EVENT, NULL, L"Firewall port" },
	{ SERVICE_TRIGGER_TYPE_FIREWALL_PORT_EVENT, &FirewallPortOpenGuid, L"Firewall port: Open" },
	{ SERVICE_TRIGGER_TYPE_FIREWALL_PORT_EVENT, &FirewallPortCloseGuid, L"Firewall port: Close" },
	{ SERVICE_TRIGGER_TYPE_FIREWALL_PORT_EVENT, &SubTypeUnknownGuid, L"Firewall port: Unknown" },
	{ SERVICE_TRIGGER_TYPE_GROUP_POLICY, NULL, L"Group policy change" },
	{ SERVICE_TRIGGER_TYPE_GROUP_POLICY, &MachinePolicyPresentGuid, L"Group policy change: Machine" },
	{ SERVICE_TRIGGER_TYPE_GROUP_POLICY, &UserPolicyPresentGuid, L"Group policy change: User" },
	{ SERVICE_TRIGGER_TYPE_GROUP_POLICY, &SubTypeUnknownGuid, L"Group policy change: Unknown" },
	{ SERVICE_TRIGGER_TYPE_NETWORK_ENDPOINT, NULL, L"Network endpoint" },
	{ SERVICE_TRIGGER_TYPE_NETWORK_ENDPOINT, &RpcInterfaceEventGuid, L"Network endpoint: RPC interface" },
	{ SERVICE_TRIGGER_TYPE_NETWORK_ENDPOINT, &NamedPipeEventGuid, L"Network endpoint: Named pipe" },
	{ SERVICE_TRIGGER_TYPE_NETWORK_ENDPOINT, &SubTypeUnknownGuid, L"Network endpoint: Unknown" }
};

static PH_STRINGREF PublishersKeyName = PH_STRINGREF_INIT(L"Software\\Microsoft\\Windows\\CurrentVersion\\WINEVT\\Publishers\\");

PPH_STRING EspLookupEtwPublisherName(
	_In_ PGUID Guid
)
{
	PPH_STRING guidString;
	PPH_STRING keyName;
	HANDLE keyHandle;
	PPH_STRING publisherName = NULL;

	// Copied from ProcessHacker\hndlinfo.c.

	guidString = PhFormatGuid(Guid);

	keyName = PhConcatStringRef2(&PublishersKeyName, &guidString->sr);

	if (NT_SUCCESS(PhOpenKey(
		&keyHandle,
		KEY_READ,
		PH_KEY_LOCAL_MACHINE,
		&keyName->sr,
		0
	)))
	{
		publisherName = PhQueryRegistryString(keyHandle, NULL);

		if (publisherName && publisherName->Length == 0)
		{
			PhDereferenceObject(publisherName);
			publisherName = NULL;
		}

		NtClose(keyHandle);
	}

	PhDereferenceObject(keyName);

	if (publisherName)
	{
		PhDereferenceObject(guidString);
		return publisherName;
	}
	else
	{
		return guidString;
	}
}

VOID EspFormatTriggerInfo(
	_In_ PES_TRIGGER_INFO Info,
	_Out_ PWSTR *TriggerString,
	_Out_ PWSTR *ActionString,
	_Out_ PPH_STRING *StringUsed
)
{
	PPH_STRING stringUsed = NULL;
	PWSTR triggerString = NULL;
	PWSTR actionString;
	ULONG i;
	BOOLEAN typeFound;
	BOOLEAN subTypeFound;

	switch (Info->Type)
	{
	case SERVICE_TRIGGER_TYPE_DEVICE_INTERFACE_ARRIVAL:
	{
		PPH_STRING guidString;

		if (!Info->Subtype)
		{
			triggerString = L"Device interface arrival";
		}
		else
		{
			guidString = PhFormatGuid(Info->Subtype);
			stringUsed = PhConcatStrings2(L"Device interface arrival: ", guidString->Buffer);
			triggerString = stringUsed->Buffer;
		}
	}
	break;
	case SERVICE_TRIGGER_TYPE_CUSTOM_SYSTEM_STATE_CHANGE:
	{
		PPH_STRING guidString;

		if (!Info->Subtype)
		{
			triggerString = L"Custom system state change";
		}
		else
		{
			guidString = PhFormatGuid(Info->Subtype);
			stringUsed = PhConcatStrings2(L"Custom system state change: ", guidString->Buffer);
			triggerString = stringUsed->Buffer;
		}
	}
	break;
	case SERVICE_TRIGGER_TYPE_CUSTOM:
	{
		if (Info->Subtype)
		{
			PPH_STRING publisherName;

			// Try to lookup the publisher name from the GUID.
			publisherName = EspLookupEtwPublisherName(Info->Subtype);
			stringUsed = PhConcatStrings2(L"Custom: ", publisherName->Buffer);
			PhDereferenceObject(publisherName);
			triggerString = stringUsed->Buffer;
		}
		else
		{
			triggerString = L"Custom";
		}
	}
	break;
	default:
	{
		typeFound = FALSE;
		subTypeFound = FALSE;

		for (i = 0; i < sizeof(SubTypeEntries) / sizeof(SUBTYPE_ENTRY); i++)
		{
			if (SubTypeEntries[i].Type == Info->Type)
			{
				typeFound = TRUE;

				if (!Info->Subtype && !SubTypeEntries[i].Guid) 
				{
					subTypeFound = TRUE;
					triggerString = SubTypeEntries[i].Name;
					break;
				}
				else if (Info->Subtype && SubTypeEntries[i].Guid && IsEqualGUID(*(_GUID*)&Info->Subtype, *(_GUID*)&SubTypeEntries[i].Guid))
				{
					subTypeFound = TRUE;
					triggerString = SubTypeEntries[i].Name;
					break;
				}
				else if (!subTypeFound && SubTypeEntries[i].Guid == &SubTypeUnknownGuid)
				{
					triggerString = SubTypeEntries[i].Name;
					break;
				}
			}
		}

		if (!typeFound)
		{
			triggerString = L"Unknown";
		}
	}
	break;
	}

	switch (Info->Action)
	{
	case SERVICE_TRIGGER_ACTION_SERVICE_START:
		actionString = L"Start";
		break;
	case SERVICE_TRIGGER_ACTION_SERVICE_STOP:
		actionString = L"Stop";
		break;
	default:
		actionString = L"Unknown";
		break;
	}

	*TriggerString = triggerString;
	*ActionString = actionString;
	*StringUsed = stringUsed;
}

PES_TRIGGER_DATA EspCreateTriggerData(
	_In_opt_ PSERVICE_TRIGGER_SPECIFIC_DATA_ITEM DataItem
)
{
	PES_TRIGGER_DATA data;

	data = (PES_TRIGGER_DATA)PhAllocate(sizeof(ES_TRIGGER_DATA));
	memset(data, 0, sizeof(ES_TRIGGER_DATA));

	if (DataItem)
	{
		data->Type = DataItem->dwDataType;

		if (data->Type == SERVICE_TRIGGER_DATA_TYPE_STRING)
		{
			if (DataItem->pData && DataItem->cbData >= 2)
				data->String = PhCreateStringEx((PWSTR)DataItem->pData, DataItem->cbData - 2); // exclude final null terminator
			else
				data->String = PhReferenceEmptyString();
		}
		else if (data->Type == SERVICE_TRIGGER_DATA_TYPE_BINARY)
		{
			data->BinaryLength = DataItem->cbData;
			data->Binary = PhAllocateCopy(DataItem->pData, DataItem->cbData);
		}
		else if (data->Type == SERVICE_TRIGGER_DATA_TYPE_LEVEL)
		{
			if (DataItem->cbData == sizeof(UCHAR))
				data->Byte = *(PUCHAR)DataItem->pData;
		}
		else if (data->Type == SERVICE_TRIGGER_DATA_TYPE_KEYWORD_ANY || data->Type == SERVICE_TRIGGER_DATA_TYPE_KEYWORD_ALL)
		{
			if (DataItem->cbData == sizeof(ULONG64))
				data->UInt64 = *(PULONG64)DataItem->pData;
		}
	}

	return data;
}

PES_TRIGGER_DATA EspCloneTriggerData(
	_In_ PES_TRIGGER_DATA Data
)
{
	PES_TRIGGER_DATA newData;

	newData = (PES_TRIGGER_DATA)PhAllocateCopy(Data, sizeof(ES_TRIGGER_DATA));

	if (newData->Type == SERVICE_TRIGGER_DATA_TYPE_STRING)
	{
		if (newData->String)
			newData->String = PhDuplicateString(newData->String);
	}
	else if (newData->Type == SERVICE_TRIGGER_DATA_TYPE_BINARY)
	{
		if (newData->Binary)
			newData->Binary = PhAllocateCopy(newData->Binary, newData->BinaryLength);
	}

	return newData;
}

VOID EspDestroyTriggerData(
	_In_ PES_TRIGGER_DATA Data
)
{
	if (Data->Type == SERVICE_TRIGGER_DATA_TYPE_STRING)
	{
		if (Data->String)
			PhDereferenceObject(Data->String);
	}
	else if (Data->Type == SERVICE_TRIGGER_DATA_TYPE_BINARY)
	{
		if (Data->Binary)
			PhFree(Data->Binary);
	}

	PhFree(Data);
}


PES_TRIGGER_INFO EspCreateTriggerInfo(
	_In_opt_ PSERVICE_TRIGGER Trigger
)
{
	PES_TRIGGER_INFO info;

	info = (PES_TRIGGER_INFO)PhAllocate(sizeof(ES_TRIGGER_INFO));
	memset(info, 0, sizeof(ES_TRIGGER_INFO));

	if (Trigger)
	{
		info->Type = Trigger->dwTriggerType;

		if (Trigger->pTriggerSubtype)
		{
			info->SubtypeBuffer = *Trigger->pTriggerSubtype;
			info->Subtype = &info->SubtypeBuffer;
		}

		info->Action = Trigger->dwAction;

		if (
			info->Type == SERVICE_TRIGGER_TYPE_CUSTOM ||
			info->Type == SERVICE_TRIGGER_TYPE_DEVICE_INTERFACE_ARRIVAL ||
			info->Type == SERVICE_TRIGGER_TYPE_FIREWALL_PORT_EVENT ||
			info->Type == SERVICE_TRIGGER_TYPE_NETWORK_ENDPOINT
			)
		{
			ULONG i;

			info->DataList = PhCreateList(Trigger->cDataItems);

			for (i = 0; i < Trigger->cDataItems; i++)
			{
				PhAddItemList(info->DataList, EspCreateTriggerData(&Trigger->pDataItems[i]));
			}
		}
	}

	return info;
}

PES_TRIGGER_INFO EspCloneTriggerInfo(
	_In_ PES_TRIGGER_INFO Info
)
{
	PES_TRIGGER_INFO newInfo;

	newInfo = (PES_TRIGGER_INFO)PhAllocateCopy(Info, sizeof(ES_TRIGGER_INFO));

	if (newInfo->Subtype == &Info->SubtypeBuffer)
		newInfo->Subtype = &newInfo->SubtypeBuffer;

	if (newInfo->DataList)
	{
		ULONG i;

		newInfo->DataList = PhCreateList(Info->DataList->AllocatedCount);
		newInfo->DataList->Count = Info->DataList->Count;

		for (i = 0; i < Info->DataList->Count; i++)
			newInfo->DataList->Items[i] = EspCloneTriggerData((PES_TRIGGER_DATA)Info->DataList->Items[i]);
	}

	return newInfo;
}

VOID EspDestroyTriggerInfo(
	_In_ PES_TRIGGER_INFO Info
)
{
	if (Info->DataList)
	{
		ULONG i;

		for (i = 0; i < Info->DataList->Count; i++)
		{
			EspDestroyTriggerData((PES_TRIGGER_DATA)Info->DataList->Items[i]);
		}

		PhDereferenceObject(Info->DataList);
	}

	PhFree(Info);
}


BOOLEAN EspEnumerateEtwPublishers(
    _Out_ PETW_PUBLISHER_ENTRY *Entries,
    _Out_ PULONG NumberOfEntries
    )
{
    NTSTATUS status;
    HANDLE publishersKeyHandle;
    ULONG index;
    PKEY_BASIC_INFORMATION buffer;
    ULONG bufferSize;
    PETW_PUBLISHER_ENTRY entries;
    ULONG numberOfEntries;
    ULONG allocatedEntries;

    if (!NT_SUCCESS(PhOpenKey(
        &publishersKeyHandle,
        KEY_READ,
        PH_KEY_LOCAL_MACHINE,
        &PublishersKeyName,
        0
        )))
    {
        return FALSE;
    }

    numberOfEntries = 0;
    allocatedEntries = 256;
    entries = (PETW_PUBLISHER_ENTRY)PhAllocate(allocatedEntries * sizeof(ETW_PUBLISHER_ENTRY));

    index = 0;
    bufferSize = 0x100;
    buffer = (PKEY_BASIC_INFORMATION)PhAllocate(0x100);

    while (TRUE)
    {
        status = NtEnumerateKey(
            publishersKeyHandle,
            index,
            KeyBasicInformation,
            buffer,
            bufferSize,
            &bufferSize
            );

        if (NT_SUCCESS(status))
        {
            UNICODE_STRING nameUs;
            PH_STRINGREF name;
            HANDLE keyHandle;
            GUID guid;
            PPH_STRING publisherName;

            nameUs.Buffer = buffer->Name;
            nameUs.Length = (USHORT)buffer->NameLength;
            name.Buffer = buffer->Name;
            name.Length = buffer->NameLength;

            // Make sure this is a valid publisher key.
            if (NT_SUCCESS(RtlGUIDFromString(&nameUs, &guid)))
            {
                if (NT_SUCCESS(PhOpenKey(
                    &keyHandle,
                    KEY_READ,
                    publishersKeyHandle,
                    &name,
                    0
                    )))
                {
                    publisherName = PhQueryRegistryString(keyHandle, NULL);

                    if (publisherName)
                    {
                        if (publisherName->Length != 0)
                        {
                            PETW_PUBLISHER_ENTRY entry;

                            if (numberOfEntries == allocatedEntries)
                            {
                                allocatedEntries *= 2;
                                entries = (PETW_PUBLISHER_ENTRY)PhReAllocate(entries, allocatedEntries * sizeof(ETW_PUBLISHER_ENTRY));
                            }

                            entry = &entries[numberOfEntries++];
                            entry->PublisherName = publisherName;
                            entry->Guid = guid;
                        }
                        else
                        {
                            PhDereferenceObject(publisherName);
                        }
                    }

                    NtClose(keyHandle);
                }
            }

            index++;
        }
        else if (status == STATUS_NO_MORE_ENTRIES)
        {
            break;
        }
        else if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL)
        {
            PhFree(buffer);
            buffer = (PKEY_BASIC_INFORMATION)PhAllocate(bufferSize);
        }
        else
        {
            break;
        }
    }

    PhFree(buffer);
    NtClose(publishersKeyHandle);

    *Entries = entries;
    *NumberOfEntries = numberOfEntries;

    return TRUE;
}

PPH_STRING EspConvertNullsToSpaces(
    _In_ PPH_STRING String
    )
{
    PPH_STRING text;
    SIZE_T j;

    text = PhDuplicateString(String);

    for (j = 0; j < text->Length / 2; j++)
    {
        if (text->Buffer[j] == 0)
            text->Buffer[j] = ' ';
    }

    return text;
}

BOOLEAN EspLookupEtwPublisherGuid(
    _In_ PWSTR PublisherName,
    _Out_ PGUID Guid
    )
{
    BOOLEAN result;
    PETW_PUBLISHER_ENTRY entries;
    ULONG numberOfEntries;
    ULONG i;

    if (!EspEnumerateEtwPublishers(&entries, &numberOfEntries))
        return FALSE;

    result = FALSE;

    for (i = 0; i < numberOfEntries; i++)
    {
        if (!result && PhEqualStringZ(entries[i].PublisherName->sr.Buffer, PublisherName, TRUE))
        {
            *Guid = entries[i].Guid;
            result = TRUE;
        }

        PhDereferenceObject(entries[i].PublisherName);
    }

    PhFree(entries);

    return result;
}

PPH_STRING EspConvertNewLinesToNulls(
    _In_ PPH_STRING String
    )
{
    PPH_STRING text;
    SIZE_T count;
    SIZE_T i;

    text = PhCreateStringEx(NULL, String->Length + 2); // plus one character for an extra null terminator (see below)
    text->Length = 0;
    count = 0;

    for (i = 0; i < String->Length / 2; i++)
    {
        // Lines are terminated by "\r\n".
        if (String->Buffer[i] == '\r')
        {
            continue;
        }

        if (String->Buffer[i] == '\n')
        {
            text->Buffer[count++] = 0;
            continue;
        }

        text->Buffer[count++] = String->Buffer[i];
    }

    if (count != 0)
    {
        // Make sure we have an extra null terminator at the end, as required of multistrings.
        if (text->Buffer[count - 1] != 0)
            text->Buffer[count++] = 0;
    }

    text->Length = count * 2;

    return text;
}

PH_KEY_VALUE_PAIR EspServiceSidTypePairs[] =
{
    SIP("None", SERVICE_SID_TYPE_NONE),
    SIP("Restricted", SERVICE_SID_TYPE_RESTRICTED),
    SIP("Unrestricted", SERVICE_SID_TYPE_UNRESTRICTED)
};

PH_KEY_VALUE_PAIR EspServiceLaunchProtectedPairs[] =
{
    SIP("None", SERVICE_LAUNCH_PROTECTED_NONE),
    SIP("Full (Windows)", SERVICE_LAUNCH_PROTECTED_WINDOWS),
    SIP("Light (Windows)", SERVICE_LAUNCH_PROTECTED_WINDOWS_LIGHT),
    SIP("Light (Antimalware)", SERVICE_LAUNCH_PROTECTED_ANTIMALWARE_LIGHT)
};

typedef NTSTATUS (NTAPI *_RtlCreateServiceSid)(
    _In_ PUNICODE_STRING ServiceName,
    _Out_writes_bytes_opt_(*ServiceSidLength) PSID ServiceSid,
    _Inout_ PULONG ServiceSidLength
    );

static _RtlCreateServiceSid RtlCreateServiceSid_I = NULL;

PPH_STRING EspGetServiceSidString(
    _In_ PPH_STRINGREF ServiceName
    )
{
    PSID serviceSid = NULL;
    UNICODE_STRING serviceNameUs;
    ULONG serviceSidLength = 0;
    PPH_STRING sidString = NULL;

    if (!RtlCreateServiceSid_I)
    {
        if (!(RtlCreateServiceSid_I = (_RtlCreateServiceSid)PhGetModuleProcAddress(L"ntdll.dll", "RtlCreateServiceSid")))
            return NULL;
    }

    PhStringRefToUnicodeString(ServiceName, &serviceNameUs);

    if (RtlCreateServiceSid_I(&serviceNameUs, serviceSid, &serviceSidLength) == STATUS_BUFFER_TOO_SMALL)
    {
        serviceSid = PhAllocate(serviceSidLength);

        if (NT_SUCCESS(RtlCreateServiceSid_I(&serviceNameUs, serviceSid, &serviceSidLength)))
            sidString = PhSidToStringSid(serviceSid);

        PhFree(serviceSid);
    }

    return sidString;
}

ULONG EspGetServiceSidTypeInteger(
    _In_ PWSTR SidType
    )
{
    ULONG integer;

    if (PhFindIntegerSiKeyValuePairs(
        EspServiceSidTypePairs,
        sizeof(EspServiceSidTypePairs),
        SidType,
        &integer
        ))
        return integer;
    else
        return ULONG_MAX;
}

PH_KEY_VALUE_PAIR ServiceActionPairs[] =
{
    SIP("Take no action", SC_ACTION_NONE),
    SIP("Restart the service", SC_ACTION_RESTART),
    SIP("Run a program", SC_ACTION_RUN_COMMAND),
    SIP("Restart the computer", SC_ACTION_REBOOT)
};

/*
* Process Hacker Extra Plugins -
*   Service Backup and Restore Plugin
*
*/

/*
#include "../../Common/FlexError.h"

static PH_STRINGREF servicesKeyName = PH_STRINGREF_INIT(L"System\\CurrentControlSet\\Services\\");
static PH_STRINGREF fileExtName = PH_STRINGREF_INIT(L".phservicebackup");

STATUS PhBackupService(const wstring& svcName, const wstring& backFile)
{
    NTSTATUS status = STATUS_SUCCESS;
    HANDLE keyHandle = NULL;
    HANDLE fileHandle = NULL;
    PPH_STRING serviceKeyName = NULL;

	PH_STRINGREF ServiceItemName = PH_STRINGREF_INIT((wchar_t*)svcName.c_str());

    serviceKeyName = PhConcatStringRef2(&servicesKeyName, &ServiceItemName);

    __try
    {
        if (!NT_SUCCESS(status = PhOpenKey(
            &keyHandle,
            KEY_ALL_ACCESS, // KEY_READ,
            PH_KEY_LOCAL_MACHINE,
            &serviceKeyName->sr,
            0
            )))
        {
            __leave;
        }

        if (!NT_SUCCESS(status = PhCreateFileWin32(
            &fileHandle,
            (wchar_t*)backFile.c_str(),
            FILE_GENERIC_WRITE | DELETE,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ,
            FILE_OVERWRITE_IF,
            FILE_NON_DIRECTORY_FILE | FILE_OPEN_FOR_BACKUP_INTENT | FILE_SYNCHRONOUS_IO_NONALERT
            )))
        {
            __leave;
        }

        if (!NT_SUCCESS(status = NtSaveKeyEx(keyHandle,  fileHandle,  2)))
        {
            __leave;
        }
    }
    __finally
    {
        if (fileHandle)
        {
            NtClose(fileHandle);
        }

        if (keyHandle)
        {
            NtClose(keyHandle);
        }

        PhDereferenceObject(serviceKeyName);
    }

    if (!NT_SUCCESS(status))
		return ERR(QObject::tr("Unable to backup the service"), status);
    
	return OK;
}

STATUS PhRestoreService(const wstring& backFile, const wstring& svcName)
{
    NTSTATUS status = STATUS_SUCCESS;
    HANDLE keyHandle = NULL;
    HANDLE fileHandle = NULL;
    PPH_STRING serviceKeyName = NULL;

	PH_STRINGREF ServiceItemName = PH_STRINGREF_INIT((wchar_t*)svcName.c_str());

    serviceKeyName = PhConcatStringRef2(&servicesKeyName, &ServiceItemName);

	wstring err;

    __try
    {
        if (!NT_SUCCESS(status = PhOpenKey(
            &keyHandle,
            KEY_ALL_ACCESS,// KEY_WRITE
            PH_KEY_LOCAL_MACHINE,
            &serviceKeyName->sr,
            0
            )))
        {
            __leave;
        }

        HKEY appKeyHandle;

        if (RegLoadAppKey(
            (wchar_t*)backFile.c_str(),
            &appKeyHandle,
            KEY_ALL_ACCESS,
            REG_PROCESS_APPKEY, // REG_APP_HIVE
            0
            ) == ERROR_SUCCESS)
        {
            PPH_STRING displayName = PhQueryRegistryString(appKeyHandle, L"DisplayName");
            PPH_STRING currentDisplayName = PhQueryRegistryString(keyHandle, L"DisplayName");

            if (!PhEqualString(displayName, currentDisplayName, TRUE))
            {
                PhDereferenceObject(currentDisplayName);
                PhDereferenceObject(displayName);
                NtClose(appKeyHandle);
                err = L"The display name does not match the backup of this service.";
                __leave;
            }

            PhDereferenceObject(currentDisplayName);
            PhDereferenceObject(displayName);
            NtClose(appKeyHandle);
        }

        if (PhFindStringInString(ofdFileName, 0, ServiceItem->Name->Buffer) == -1)
        {
           PhShowError(OwnerWindow, L"The file name does not match the name of this service.");
            __leave;
        }

        if (!NT_SUCCESS(status = PhCreateFileWin32(
            &fileHandle,
            (wchar_t*)backFile.c_str(),
            FILE_GENERIC_READ,
            FILE_ATTRIBUTE_NORMAL,
            FILE_SHARE_READ,
            FILE_OPEN,
            FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
            )))
        {
            __leave;
        }

        if (!NT_SUCCESS(status = NtRestoreKey(
            keyHandle,
            fileHandle,
            0 // REG_FORCE_RESTORE
            )))
        {
            __leave;
        }
    }
    __finally
    {
        if (fileHandle)
        {
            NtClose(fileHandle);
        }

        if (keyHandle)
        {
            NtClose(keyHandle);
        }

        PhDereferenceObject(serviceKeyName);
    }

	if(!err.empty())
		return ERR(QString::fromStdWString(err), -1);

    if (!NT_SUCCESS(status))
		return ERR(QObject::tr("Unable to backup the service"), status);
    
	return OK;
}
*/