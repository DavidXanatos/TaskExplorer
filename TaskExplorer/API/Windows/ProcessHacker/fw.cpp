
#include "stdafx.h"
#include "../ProcessHacker.h"

#include "wf.h"

FW_POLICY_STORE_HANDLE FwApiDefaultHandle = NULL;
HMODULE FwApiLibraryHandle = NULL;
_FWStatusMessageFromStatusCode FWStatusMessageFromStatusCode_I = NULL;
_FWOpenPolicyStore FWOpenPolicyStore_I = NULL;
_FWClosePolicyStore FWClosePolicyStore_I = NULL;
_FWEnumFirewallRules FWEnumFirewallRules_I = NULL;
_FWFreeFirewallRules FWFreeFirewallRules_I = NULL;

BOOLEAN InitializeFirewallApi(
    VOID
    )
{
    if (FwApiLibraryHandle = LoadLibrary(L"FirewallAPI.dll"))
    {
        USHORT fwApiVersion = 0;

        FWOpenPolicyStore_I = (_FWOpenPolicyStore)PhGetProcedureAddress(FwApiLibraryHandle, "FWOpenPolicyStore", 0);
        FWClosePolicyStore_I = (_FWClosePolicyStore)PhGetProcedureAddress(FwApiLibraryHandle, "FWClosePolicyStore", 0);
        FWEnumFirewallRules_I = (_FWEnumFirewallRules)PhGetProcedureAddress(FwApiLibraryHandle, "FWEnumFirewallRules", 0);
        FWFreeFirewallRules_I = (_FWFreeFirewallRules)PhGetProcedureAddress(FwApiLibraryHandle, "FWFreeFirewallRules", 0);
        FWStatusMessageFromStatusCode_I = (_FWStatusMessageFromStatusCode)PhGetProcedureAddress(FwApiLibraryHandle, "FWStatusMessageFromStatusCode", 0);

        if (WindowsVersion >= WINDOWS_10_RS2)
        {
            fwApiVersion = FW_REDSTONE2_BINARY_VERSION;
        }
        else if (WindowsVersion >= WINDOWS_10_RS1)
        {
            fwApiVersion = FW_REDSTONE1_BINARY_VERSION;
        }
        else if (WindowsVersion >= WINDOWS_10_TH2)
        {
            fwApiVersion = FW_THRESHOLD2_BINARY_VERSION;
        }
        else if (WindowsVersion >= WINDOWS_10)
        {
            // TODO: FW_THRESHOLD_BINARY_VERSION
            fwApiVersion = FW_WIN10_BINARY_VERSION;
        }
        else if (WindowsVersion >= WINDOWS_8_1)
        {
            fwApiVersion = FW_WIN8_1_BINARY_VERSION;
        }
        else if (WindowsVersion >= WINDOWS_8)
        {
            // TODO: Win8 binary version
        }
        else if (WindowsVersion >= WINDOWS_7)
        {
            fwApiVersion = FW_SEVEN_BINARY_VERSION;
        }

        if (FWOpenPolicyStore_I(
            fwApiVersion,
            NULL,
            FW_STORE_TYPE_DEFAULTS,
            FW_POLICY_ACCESS_RIGHT_READ,
            FW_POLICY_STORE_FLAGS_NONE,
            &FwApiDefaultHandle
            ) == ERROR_SUCCESS)
        {
            return TRUE;
        }
    }

    return FALSE;
}

VOID FreeFirewallApi(
    VOID
    )
{
    if (FWClosePolicyStore_I && FwApiDefaultHandle)
        FWClosePolicyStore_I(&FwApiDefaultHandle);
}

VOID FwStatusMessageFromStatusCode(
    _In_ FW_RULE_STATUS code
    )
{
    ULONG statusLength = 0;

    FWStatusMessageFromStatusCode_I(code, NULL, &statusLength);
}

VOID EnumerateFirewallRules(
    _In_ FW_PROFILE_TYPE Type,
    _In_ FW_DIRECTION Direction,
    _In_ PFW_WALK_RULES Callback,
    _In_ PVOID Context
    )
{
    ULONG uRuleCount = 0;
    ULONG result = 0;
    PFW_RULE pRules = NULL;

    result = FWEnumFirewallRules_I(
        FwApiDefaultHandle,
        FW_RULE_STATUS_CLASS_ALL,
        Type, 
        (FW_ENUM_RULES_FLAGS)(FW_ENUM_RULES_FLAG_RESOLVE_NAME | FW_ENUM_RULES_FLAG_RESOLVE_DESCRIPTION | FW_ENUM_RULES_FLAG_RESOLVE_APPLICATION), 
        &uRuleCount, 
        &pRules
        );

    if (!result && pRules && uRuleCount)
    {
        for (PFW_RULE i = pRules; i; i = i->pNext)
        {
            if (Direction == FW_DIR_BOTH)
            {
                if (!Callback || !Callback(i, Context))
                    break;
            }
            else
            {
                if (i->Direction == Direction)
                {
                    if (!Callback || !Callback(i, Context))
                        break;
                }
            }
        }
    }

    if (FWFreeFirewallRules_I && pRules)
    {
        FWFreeFirewallRules_I(pRules);
    }
}