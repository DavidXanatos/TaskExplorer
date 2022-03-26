/*
 * Process Hacker -
 *   settings
 *
 * Copyright (C) 2010-2016 wj32
 * Copyright (C) 2017-2022 dmex
 *
 * This file is part of Process Hacker.
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This file contains a program-specific settings system. All possible
 * settings are defined at program startup and added to a hashtable.
 * The values of these settings can then be read in from a XML file or
 * saved to a XML file at any time. Settings which are not recognized
 * are added to a list of "ignored settings"; this is necessary to
 * support plugin settings, as we don't want their settings to get
 * deleted whenever the plugins are disabled.
 *
 * The get/set functions are very strict. If the wrong function is used
 * (the get-integer-setting function is used on a string setting) or
 * the setting does not exist, an exception will be raised.
 */

#include <ph.h>
#include <guisup.h>
#include <settings.h>
#include <json.h>

BOOLEAN NTAPI PhpSettingsHashtableEqualFunction(
    _In_ PVOID Entry1,
    _In_ PVOID Entry2
    );

ULONG NTAPI PhpSettingsHashtableHashFunction(
    _In_ PVOID Entry
    );

VOID PhpFreeSettingValue(
    _In_ PH_SETTING_TYPE Type,
    _In_ PPH_SETTING Setting
    );

PVOID PhpLookupSetting(
    _In_ PPH_STRINGREF Name
    );

PPH_HASHTABLE PhSettingsHashtable;
PH_QUEUED_LOCK PhSettingsLock = PH_QUEUED_LOCK_INIT;
PPH_LIST PhIgnoredSettings;

VOID PhSettingsInitialization(
    VOID
    )
{
    PhSettingsHashtable = PhCreateHashtable(
        sizeof(PH_SETTING),
        PhpSettingsHashtableEqualFunction,
        PhpSettingsHashtableHashFunction,
        512
        );
    PhIgnoredSettings = PhCreateList(4);

    PhAddDefaultSettings();
    PhUpdateCachedSettings();
}

BOOLEAN NTAPI PhpSettingsHashtableEqualFunction(
    _In_ PVOID Entry1,
    _In_ PVOID Entry2
    )
{
    PPH_SETTING setting1 = (PPH_SETTING)Entry1;
    PPH_SETTING setting2 = (PPH_SETTING)Entry2;

    return PhEqualStringRef(&setting1->Name, &setting2->Name, FALSE);
}

ULONG NTAPI PhpSettingsHashtableHashFunction(
    _In_ PVOID Entry
    )
{
    PPH_SETTING setting = (PPH_SETTING)Entry;

    return PhHashStringRefEx(&setting->Name, FALSE, PH_STRING_HASH_X65599);
}

static ULONG PhpGetCurrentScale(
    VOID
    )
{
    return PhGlobalDpi;
}

PPH_STRING PhSettingToString(
    _In_ PH_SETTING_TYPE Type,
    _In_ PPH_SETTING Setting
    )
{
    switch (Type)
    {
    case StringSettingType:
        {
            if (!Setting->u.Pointer)
                return PhReferenceEmptyString();

            PhReferenceObject(Setting->u.Pointer);

            return (PPH_STRING)Setting->u.Pointer;
        }
    case IntegerSettingType:
        {
            PH_FORMAT format[1];

            // %x
            PhInitFormatX(&format[0], Setting->u.Integer);

            return PhFormat(format, RTL_NUMBER_OF(format), 0);  
        }
    case IntegerPairSettingType:
        {
            PPH_INTEGER_PAIR integerPair = &Setting->u.IntegerPair;
            PH_FORMAT format[3];

            // %ld,%ld
            PhInitFormatD(&format[0], integerPair->X);
            PhInitFormatC(&format[1], L',');
            PhInitFormatD(&format[2], integerPair->Y);

            return PhFormat(format, RTL_NUMBER_OF(format), 0);
        }
    case ScalableIntegerPairSettingType:
        {
            PPH_SCALABLE_INTEGER_PAIR scalableIntegerPair = Setting->u.Pointer;
            PH_FORMAT format[6];

            if (!scalableIntegerPair)
                return PhReferenceEmptyString();

            // @%lu|%ld,%ld
            PhInitFormatC(&format[0], L'@');
            PhInitFormatU(&format[1], scalableIntegerPair->Scale);
            PhInitFormatC(&format[2], L'|');
            PhInitFormatD(&format[3], scalableIntegerPair->X);
            PhInitFormatC(&format[4], L',');
            PhInitFormatD(&format[5], scalableIntegerPair->Y);

            return PhFormat(format, RTL_NUMBER_OF(format), 0);
        }
    }

    return PhReferenceEmptyString();
}

BOOLEAN PhSettingFromString(
    _In_ PH_SETTING_TYPE Type,
    _In_ PPH_STRINGREF StringRef,
    _In_opt_ PPH_STRING String,
    _Inout_ PPH_SETTING Setting
    )
{
    switch (Type)
    {
    case StringSettingType:
        {
            if (String)
            {
                PhSetReference(&Setting->u.Pointer, String);
            }
            else
            {
                Setting->u.Pointer = PhCreateString2(StringRef);
            }

            return TRUE;
        }
    case IntegerSettingType:
        {
            ULONG64 integer;

            if (PhStringToInteger64(StringRef, 16, &integer))
            {
                Setting->u.Integer = (ULONG)integer;
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
    case IntegerPairSettingType:
        {
            LONG64 x;
            LONG64 y;
            PH_STRINGREF xString;
            PH_STRINGREF yString;

            if (!PhSplitStringRefAtChar(StringRef, L',', &xString, &yString))
                return FALSE;

            if (PhStringToInteger64(&xString, 10, &x) && PhStringToInteger64(&yString, 10, &y))
            {
                Setting->u.IntegerPair.X = (LONG)x;
                Setting->u.IntegerPair.Y = (LONG)y;
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
    case ScalableIntegerPairSettingType:
        {
            ULONG64 scale;
            LONG64 x;
            LONG64 y;
            PH_STRINGREF stringRef;
            PH_STRINGREF firstPart;
            PH_STRINGREF secondPart;
            PPH_SCALABLE_INTEGER_PAIR scalableIntegerPair;

            stringRef = *StringRef;

            if (stringRef.Length != 0 && stringRef.Buffer[0] == L'@')
            {
                PhSkipStringRef(&stringRef, sizeof(WCHAR));

                if (!PhSplitStringRefAtChar(&stringRef, L'|', &firstPart, &stringRef))
                    return FALSE;
                if (!PhStringToInteger64(&firstPart, 10, &scale))
                    return FALSE;
            }
            else
            {
                scale = PhpGetCurrentScale();
            }

            if (!PhSplitStringRefAtChar(&stringRef, L',', &firstPart, &secondPart))
                return FALSE;

            if (PhStringToInteger64(&firstPart, 10, &x) && PhStringToInteger64(&secondPart, 10, &y))
            {
                scalableIntegerPair = PhAllocate(sizeof(PH_SCALABLE_INTEGER_PAIR));
                scalableIntegerPair->X = (LONG)x;
                scalableIntegerPair->Y = (LONG)y;
                scalableIntegerPair->Scale = (ULONG)scale;
                Setting->u.Pointer = scalableIntegerPair;
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
    }

    return FALSE;
}

static VOID PhpFreeSettingValue(
    _In_ PH_SETTING_TYPE Type,
    _In_ PPH_SETTING Setting
    )
{
    switch (Type)
    {
    case StringSettingType:
        PhClearReference(&Setting->u.Pointer);
        break;
    case ScalableIntegerPairSettingType:
        PhFree(Setting->u.Pointer);
        Setting->u.Pointer = NULL;
        break;
    }
}

static PVOID PhpLookupSetting(
    _In_ PPH_STRINGREF Name
    )
{
    PH_SETTING lookupSetting;
    PPH_SETTING setting;

    lookupSetting.Name = *Name;
    setting = (PPH_SETTING)PhFindEntryHashtable(
        PhSettingsHashtable,
        &lookupSetting
        );

    return setting;
}

VOID PhEnumSettings(
    _In_ PPH_SETTINGS_ENUM_CALLBACK Callback,
    _In_ PVOID Context
    )
{
    PH_HASHTABLE_ENUM_CONTEXT enumContext;
    PPH_SETTING setting;

    PhAcquireQueuedLockExclusive(&PhSettingsLock);

    PhBeginEnumHashtable(PhSettingsHashtable, &enumContext);

    while (setting = PhNextEnumHashtable(&enumContext))
    {
        if (!Callback(setting, Context))
            break;
    }

    PhReleaseQueuedLockExclusive(&PhSettingsLock);
}

_May_raise_ ULONG PhGetIntegerSetting(
    _In_ PWSTR Name
    )
{
    PPH_SETTING setting;
    PH_STRINGREF name;
    ULONG value;

    PhInitializeStringRefLongHint(&name, Name);

    PhAcquireQueuedLockShared(&PhSettingsLock);

    setting = PhpLookupSetting(&name);

    if (setting && setting->Type == IntegerSettingType)
    {
        value = setting->u.Integer;
    }
    else
    {
        setting = NULL;
    }

    PhReleaseQueuedLockShared(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);

    return value;
}

_May_raise_ PH_INTEGER_PAIR PhGetIntegerPairSetting(
    _In_ PWSTR Name
    )
{
    PPH_SETTING setting;
    PH_STRINGREF name;
    PH_INTEGER_PAIR value;

    PhInitializeStringRefLongHint(&name, Name);

    PhAcquireQueuedLockShared(&PhSettingsLock);

    setting = PhpLookupSetting(&name);

    if (setting && setting->Type == IntegerPairSettingType)
    {
        value = setting->u.IntegerPair;
    }
    else
    {
        setting = NULL;
    }

    PhReleaseQueuedLockShared(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);

    return value;
}

_May_raise_ PH_SCALABLE_INTEGER_PAIR PhGetScalableIntegerPairSetting(
    _In_ PWSTR Name,
    _In_ BOOLEAN ScaleToCurrent
    )
{
    PPH_SETTING setting;
    PH_STRINGREF name;
    PH_SCALABLE_INTEGER_PAIR value;

    PhInitializeStringRefLongHint(&name, Name);

    PhAcquireQueuedLockShared(&PhSettingsLock);

    setting = PhpLookupSetting(&name);

    if (setting && setting->Type == ScalableIntegerPairSettingType)
    {
        value = *(PPH_SCALABLE_INTEGER_PAIR)setting->u.Pointer;
    }
    else
    {
        setting = NULL;
    }

    PhReleaseQueuedLockShared(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);

    if (ScaleToCurrent)
    {
        ULONG currentScale;

        currentScale = PhpGetCurrentScale();

        if (value.Scale != currentScale && value.Scale != 0)
        {
            value.X = PhMultiplyDivideSigned(value.X, currentScale, value.Scale);
            value.Y = PhMultiplyDivideSigned(value.Y, currentScale, value.Scale);
            value.Scale = currentScale;
        }
    }

    return value;
}

_May_raise_ PPH_STRING PhGetStringSetting(
    _In_ PWSTR Name
    )
{
    PPH_SETTING setting;
    PH_STRINGREF name;
    PPH_STRING value;

    PhInitializeStringRefLongHint(&name, Name);

    PhAcquireQueuedLockShared(&PhSettingsLock);

    setting = PhpLookupSetting(&name);

    if (setting && setting->Type == StringSettingType)
    {
        if (setting->u.Pointer)
        {
            PhSetReference(&value, setting->u.Pointer);
        }
        else
        {
            // Set to NULL, create an empty string
            // outside of the lock.
            value = NULL;
        }
    }
    else
    {
        setting = NULL;
    }

    PhReleaseQueuedLockShared(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);

    if (!value)
        value = PhReferenceEmptyString();

    return value;
}

_May_raise_ BOOLEAN PhGetBinarySetting(
    _In_ PWSTR Name,
    _Out_ PVOID Buffer
    )
{
    PPH_STRING setting;
    BOOLEAN result;

    setting = PhGetStringSetting(Name);
    result = PhHexStringToBuffer(&setting->sr, (PUCHAR)Buffer);
    PhDereferenceObject(setting);

    return result;
}

_May_raise_ VOID PhSetIntegerSetting(
    _In_ PWSTR Name,
    _In_ ULONG Value
    )
{
    PPH_SETTING setting;
    PH_STRINGREF name;

    PhInitializeStringRefLongHint(&name, Name);

    PhAcquireQueuedLockExclusive(&PhSettingsLock);

    setting = PhpLookupSetting(&name);

    if (setting && setting->Type == IntegerSettingType)
    {
        setting->u.Integer = Value;
    }

    PhReleaseQueuedLockExclusive(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);
}

_May_raise_ VOID PhSetIntegerPairSetting(
    _In_ PWSTR Name,
    _In_ PH_INTEGER_PAIR Value
    )
{
    PPH_SETTING setting;
    PH_STRINGREF name;

    PhInitializeStringRefLongHint(&name, Name);

    PhAcquireQueuedLockExclusive(&PhSettingsLock);

    setting = PhpLookupSetting(&name);

    if (setting && setting->Type == IntegerPairSettingType)
    {
        setting->u.IntegerPair = Value;
    }

    PhReleaseQueuedLockExclusive(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);
}

_May_raise_ VOID PhSetScalableIntegerPairSetting(
    _In_ PWSTR Name,
    _In_ PH_SCALABLE_INTEGER_PAIR Value
    )
{
    PPH_SETTING setting;
    PH_STRINGREF name;

    PhInitializeStringRefLongHint(&name, Name);

    PhAcquireQueuedLockExclusive(&PhSettingsLock);

    setting = PhpLookupSetting(&name);

    if (setting && setting->Type == ScalableIntegerPairSettingType)
    {
        PhpFreeSettingValue(ScalableIntegerPairSettingType, setting);
        setting->u.Pointer = PhAllocateCopy(&Value, sizeof(PH_SCALABLE_INTEGER_PAIR));
    }

    PhReleaseQueuedLockExclusive(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);
}

_May_raise_ VOID PhSetScalableIntegerPairSetting2(
    _In_ PWSTR Name,
    _In_ PH_INTEGER_PAIR Value
    )
{
    PH_SCALABLE_INTEGER_PAIR scalableIntegerPair;

    scalableIntegerPair.Pair = Value;
    scalableIntegerPair.Scale = PhpGetCurrentScale();
    PhSetScalableIntegerPairSetting(Name, scalableIntegerPair);
}

_May_raise_ VOID PhSetStringSetting(
    _In_ PWSTR Name,
    _In_ PWSTR Value
    )
{
    PPH_SETTING setting;
    PH_STRINGREF name;

    PhInitializeStringRefLongHint(&name, Name);

    PhAcquireQueuedLockExclusive(&PhSettingsLock);

    setting = PhpLookupSetting(&name);

    if (setting && setting->Type == StringSettingType)
    {
        PhpFreeSettingValue(StringSettingType, setting);
        setting->u.Pointer = PhCreateString(Value);
    }

    PhReleaseQueuedLockExclusive(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);
}

_May_raise_ VOID PhSetStringSetting2(
    _In_ PWSTR Name,
    _In_ PPH_STRINGREF Value
    )
{
    PPH_SETTING setting;
    PH_STRINGREF name;

    PhInitializeStringRefLongHint(&name, Name);

    PhAcquireQueuedLockExclusive(&PhSettingsLock);

    setting = PhpLookupSetting(&name);

    if (setting && setting->Type == StringSettingType)
    {
        PhpFreeSettingValue(StringSettingType, setting);
        setting->u.Pointer = PhCreateString2(Value);
    }

    PhReleaseQueuedLockExclusive(&PhSettingsLock);

    if (!setting)
        PhRaiseStatus(STATUS_NOT_FOUND);
}

_May_raise_ VOID PhSetBinarySetting(
    _In_ PWSTR Name,
    _In_ PVOID Buffer,
    _In_ ULONG Length
    )
{
    PPH_STRING binaryString;
    
    binaryString = PhBufferToHexString((PUCHAR)Buffer, Length);
    PhSetStringSetting(Name, binaryString->Buffer);
    PhDereferenceObject(binaryString);
}

VOID PhpFreeIgnoredSetting(
    _In_ PPH_SETTING Setting
    )
{
    PhFree(Setting->Name.Buffer);
    PhDereferenceObject(Setting->u.Pointer);

    PhFree(Setting);
}

VOID PhpClearIgnoredSettings(
    VOID
    )
{
    ULONG i;

    PhAcquireQueuedLockExclusive(&PhSettingsLock);

    for (i = 0; i < PhIgnoredSettings->Count; i++)
    {
        PhpFreeIgnoredSetting(PhIgnoredSettings->Items[i]);
    }

    PhClearList(PhIgnoredSettings);

    PhReleaseQueuedLockExclusive(&PhSettingsLock);
}

VOID PhClearIgnoredSettings(
    VOID
    )
{
    PhpClearIgnoredSettings();
}

VOID PhConvertIgnoredSettings(
    VOID
    )
{
    ULONG i;

    PhAcquireQueuedLockExclusive(&PhSettingsLock);

    for (i = 0; i < PhIgnoredSettings->Count; i++)
    {
        PPH_SETTING ignoredSetting = PhIgnoredSettings->Items[i];
        PPH_SETTING setting;

        setting = PhpLookupSetting(&ignoredSetting->Name);

        if (setting)
        {
            PhpFreeSettingValue(setting->Type, setting);

            if (!PhSettingFromString(
                setting->Type,
                &((PPH_STRING)ignoredSetting->u.Pointer)->sr,
                ignoredSetting->u.Pointer,
                setting
                ))
            {
                PhSettingFromString(
                    setting->Type,
                    &setting->DefaultValue,
                    NULL,
                    setting
                    );
            }

            PhpFreeIgnoredSetting(ignoredSetting);

            PhRemoveItemList(PhIgnoredSettings, i);
            i--;
        }
    }

    PhReleaseQueuedLockExclusive(&PhSettingsLock);
}

NTSTATUS PhLoadSettings(
    _In_ PWSTR FileName
    )
{
    NTSTATUS status;
    PVOID topNode;
    PVOID currentNode;

    PhpClearIgnoredSettings();

    if (!NT_SUCCESS(status = PhLoadXmlObjectFromFile(FileName, &topNode)))
        return status;
    if (!topNode) // Return corrupt status and reset the settings.
        return STATUS_FILE_CORRUPT_ERROR;

    currentNode = PhGetXmlNodeFirstChild(topNode);

    while (currentNode)
    {
        PPH_STRING settingName;

        if (settingName = PhGetXmlNodeAttributeText(currentNode, "name"))
        {
            PPH_STRING settingValue = PhGetXmlNodeOpaqueText(currentNode);

            PhAcquireQueuedLockExclusive(&PhSettingsLock);

            {
                PPH_SETTING setting;

                setting = PhpLookupSetting(&settingName->sr);

                if (setting)
                {
                    PhpFreeSettingValue(setting->Type, setting);

                    if (!PhSettingFromString(
                        setting->Type,
                        &settingValue->sr,
                        settingValue,
                        setting
                        ))
                    {
                        PhSettingFromString(
                            setting->Type,
                            &setting->DefaultValue,
                            NULL,
                            setting
                            );
                    }
                }
                else
                {
                    setting = PhAllocate(sizeof(PH_SETTING));
                    setting->Name.Buffer = PhAllocateCopy(settingName->Buffer, settingName->Length + sizeof(WCHAR));
                    setting->Name.Length = settingName->Length;
                    PhReferenceObject(settingValue);
                    setting->u.Pointer = settingValue;

                    PhAddItemList(PhIgnoredSettings, setting);
                }
            }

            PhReleaseQueuedLockExclusive(&PhSettingsLock);

            PhDereferenceObject(settingValue);
            PhDereferenceObject(settingName);
        }

        currentNode = PhGetXmlNodeNextChild(currentNode);
    }

    PhFreeXmlObject(topNode);

    PhUpdateCachedSettings();

    return STATUS_SUCCESS;
}

PSTR PhpSettingsSaveCallback(
    _In_ PVOID node,
    _In_ INT position
    )
{
#define MXML_WS_AFTER_OPEN 1
#define MXML_WS_AFTER_CLOSE 3

    PSTR elementName;

    if (!(elementName = PhGetXmlNodeElementText(node)))
        return NULL;

    if (PhEqualBytesZ(elementName, "setting", TRUE))
    {
        if (position == MXML_WS_AFTER_CLOSE)
            return "\r\n";
    }
    else if (PhEqualBytesZ(elementName, "settings", TRUE))
    {
        if (position == MXML_WS_AFTER_OPEN)
            return "\r\n";
    }

    return NULL;
}

PVOID PhpCreateSettingElement(
    _Inout_ PVOID ParentNode,
    _In_ PPH_STRINGREF SettingName,
    _In_ PPH_STRINGREF SettingValue
    )
{
    PVOID settingNode;
    PPH_BYTES settingNameUtf8;
    PPH_BYTES settingValueUtf8;

    // Create the setting element.

    settingNode = PhCreateXmlNode(ParentNode, "setting");

    settingNameUtf8 = PhConvertUtf16ToUtf8Ex(SettingName->Buffer, SettingName->Length);
    PhSetXmlNodeAttributeText(settingNode, "name", settingNameUtf8->Buffer);
    PhDereferenceObject(settingNameUtf8);

    // Set the value.

    settingValueUtf8 = PhConvertUtf16ToUtf8Ex(SettingValue->Buffer, SettingValue->Length);
    PhCreateXmlOpaqueNode(settingNode, settingValueUtf8->Buffer);
    PhDereferenceObject(settingValueUtf8);

    return settingNode;
}

NTSTATUS PhSaveSettings(
    _In_ PWSTR FileName
    )
{
    NTSTATUS status;
    PVOID topNode;
    PH_HASHTABLE_ENUM_CONTEXT enumContext;
    PPH_SETTING setting;

    topNode = PhCreateXmlNode(NULL, "settings");

    PhAcquireQueuedLockShared(&PhSettingsLock);

    PhBeginEnumHashtable(PhSettingsHashtable, &enumContext);

    while (setting = PhNextEnumHashtable(&enumContext))
    {
        PPH_STRING settingValue;

        settingValue = PhSettingToString(setting->Type, setting);
        PhpCreateSettingElement(topNode, &setting->Name, &settingValue->sr);
        PhDereferenceObject(settingValue);
    }

    // Write the ignored settings.

    for (ULONG i = 0; i < PhIgnoredSettings->Count; i++)
    {
        PPH_STRING settingValue;

        setting = PhIgnoredSettings->Items[i];
        settingValue = setting->u.Pointer;
        PhpCreateSettingElement(topNode, &setting->Name, &settingValue->sr);
    }

    PhReleaseQueuedLockShared(&PhSettingsLock);

    status = PhSaveXmlObjectToFile(
        FileName,
        topNode,
        PhpSettingsSaveCallback
        );
    PhFreeXmlObject(topNode);

    return status;
}

VOID PhResetSettings(
    VOID
    )
{
    PH_HASHTABLE_ENUM_CONTEXT enumContext;
    PPH_SETTING setting;

    PhAcquireQueuedLockExclusive(&PhSettingsLock);

    PhBeginEnumHashtable(PhSettingsHashtable, &enumContext);

    while (setting = PhNextEnumHashtable(&enumContext))
    {
        PhpFreeSettingValue(setting->Type, setting);
        PhSettingFromString(setting->Type, &setting->DefaultValue, NULL, setting);
    }

    PhReleaseQueuedLockExclusive(&PhSettingsLock);
}

VOID PhAddSetting(
    _In_ PH_SETTING_TYPE Type,
    _In_ PPH_STRINGREF Name,
    _In_ PPH_STRINGREF DefaultValue
    )
{
    PH_SETTING setting;

    setting.Type = Type;
    setting.Name = *Name;
    setting.DefaultValue = *DefaultValue;
    memset(&setting.u, 0, sizeof(setting.u));

    PhSettingFromString(Type, &setting.DefaultValue, NULL, &setting);

    PhAddEntryHashtable(PhSettingsHashtable, &setting);
}

VOID PhAddSettings(
    _In_ PPH_SETTING_CREATE Settings,
    _In_ ULONG NumberOfSettings
    )
{
    ULONG i;

    PhAcquireQueuedLockExclusive(&PhSettingsLock);

    for (i = 0; i < NumberOfSettings; i++)
    {
        PH_STRINGREF name;
        PH_STRINGREF defaultValue;

        PhInitializeStringRefLongHint(&name, Settings[i].Name);
        PhInitializeStringRefLongHint(&defaultValue, Settings[i].DefaultValue);
        PhAddSetting(Settings[i].Type, &name, &defaultValue);
    }

    PhReleaseQueuedLockExclusive(&PhSettingsLock);
}

VOID PhLoadWindowPlacementFromSetting(
    _In_opt_ PWSTR PositionSettingName,
    _In_opt_ PWSTR SizeSettingName,
    _In_ HWND WindowHandle
    )
{
    PH_RECTANGLE windowRectangle;

    if (PositionSettingName && SizeSettingName)
    {
        RECT rectForAdjust;

        windowRectangle.Position = PhGetIntegerPairSetting(PositionSettingName);
        windowRectangle.Size = PhGetScalableIntegerPairSetting(SizeSettingName, TRUE).Pair;
        PhAdjustRectangleToWorkingArea(NULL, &windowRectangle);

        // Let the window adjust for the minimum size if needed.
        rectForAdjust = PhRectangleToRect(windowRectangle);
        SendMessage(WindowHandle, WM_SIZING, WMSZ_BOTTOMRIGHT, (LPARAM)&rectForAdjust);
        windowRectangle = PhRectToRectangle(rectForAdjust);

        MoveWindow(WindowHandle, windowRectangle.Left, windowRectangle.Top,
            windowRectangle.Width, windowRectangle.Height, FALSE);
    }
    else
    {
        PH_INTEGER_PAIR position;
        PH_INTEGER_PAIR size;
        ULONG flags;

        flags = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOSIZE | SWP_NOZORDER;

        if (PositionSettingName)
        {
            position = PhGetIntegerPairSetting(PositionSettingName);
            flags &= ~SWP_NOMOVE;
        }
        else
        {
            position.X = 0;
            position.Y = 0;
        }

        if (SizeSettingName)
        {
            size = PhGetScalableIntegerPairSetting(SizeSettingName, TRUE).Pair;
            flags &= ~SWP_NOSIZE;
        }
        else
        {
            RECT windowRect;

            // Make sure the window doesn't get positioned on disconnected monitors. (dmex)
            //size.X = 16;
            //size.Y = 16;
            GetWindowRect(WindowHandle, &windowRect);
            size.X = windowRect.right - windowRect.left;
            size.Y = windowRect.bottom - windowRect.top;
        }

        // Make sure the window doesn't get positioned on disconnected monitors. (dmex) 
        windowRectangle.Position = position;
        windowRectangle.Size = size;
        PhAdjustRectangleToWorkingArea(NULL, &windowRectangle);

        SetWindowPos(WindowHandle, NULL, windowRectangle.Left, windowRectangle.Top, size.X, size.Y, flags);
    }
}

VOID PhSaveWindowPlacementToSetting(
    _In_opt_ PWSTR PositionSettingName,
    _In_opt_ PWSTR SizeSettingName,
    _In_ HWND WindowHandle
    )
{
    WINDOWPLACEMENT placement = { sizeof(placement) };
    PH_RECTANGLE windowRectangle;
    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };

    GetWindowPlacement(WindowHandle, &placement);
    windowRectangle = PhRectToRectangle(placement.rcNormalPosition);

    // The rectangle is in workspace coordinates. Convert the values back to screen coordinates.
    if (GetMonitorInfo(MonitorFromRect(&placement.rcNormalPosition, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
    {
        windowRectangle.Left += monitorInfo.rcWork.left - monitorInfo.rcMonitor.left;
        windowRectangle.Top += monitorInfo.rcWork.top - monitorInfo.rcMonitor.top;
    }

    if (PositionSettingName)
        PhSetIntegerPairSetting(PositionSettingName, windowRectangle.Position);
    if (SizeSettingName)
        PhSetScalableIntegerPairSetting2(SizeSettingName, windowRectangle.Size);
}

BOOLEAN PhLoadListViewColumnSettings(
    _In_ HWND ListViewHandle,
    _In_ PPH_STRING Settings
    )
{
#define ORDER_LIMIT 50
    PH_STRINGREF remainingPart;
    ULONG columnIndex;
    ULONG orderArray[ORDER_LIMIT]; // HACK, but reasonable limit
    ULONG maxOrder;
    ULONG scale;

    if (PhIsNullOrEmptyString(Settings))
        return FALSE;

    remainingPart = Settings->sr;
    columnIndex = 0;
    memset(orderArray, 0, sizeof(orderArray));
    maxOrder = 0;

    if (remainingPart.Length != 0 && remainingPart.Buffer[0] == L'@')
    {
        PH_STRINGREF scalePart;
        ULONG64 integer;

        PhSkipStringRef(&remainingPart, sizeof(WCHAR));
        PhSplitStringRefAtChar(&remainingPart, L'|', &scalePart, &remainingPart);

        if (scalePart.Length == 0 || !PhStringToInteger64(&scalePart, 10, &integer))
            return FALSE;

        scale = (ULONG)integer;
    }
    else
    {
        scale = PhGlobalDpi;
    }

    while (remainingPart.Length != 0)
    {
        PH_STRINGREF columnPart;
        PH_STRINGREF orderPart;
        PH_STRINGREF widthPart;
        ULONG64 integer;
        ULONG order;
        ULONG width;
        LVCOLUMN lvColumn;

        PhSplitStringRefAtChar(&remainingPart, L'|', &columnPart, &remainingPart);

        if (columnPart.Length == 0)
            return FALSE;

        PhSplitStringRefAtChar(&columnPart, L',', &orderPart, &widthPart);

        if (orderPart.Length == 0 || widthPart.Length == 0)
            return FALSE;

        // Order

        if (!PhStringToInteger64(&orderPart, 10, &integer))
            return FALSE;

        order = (ULONG)integer;

        if (order < ORDER_LIMIT)
        {
            orderArray[order] = columnIndex;

            if (maxOrder < order + 1)
                maxOrder = order + 1;
        }

        // Width

        if (!PhStringToInteger64(&widthPart, 10, &integer))
            return FALSE;

        width = (ULONG)integer;

        if (scale != PhGlobalDpi && scale != 0)
            width = PhMultiplyDivide(width, PhGlobalDpi, scale);

        lvColumn.mask = LVCF_WIDTH;
        lvColumn.cx = width;
        ListView_SetColumn(ListViewHandle, columnIndex, &lvColumn);

        columnIndex++;
    }

    ListView_SetColumnOrderArray(ListViewHandle, maxOrder, orderArray);

    return TRUE;
}

PPH_STRING PhSaveListViewColumnSettings(
    _In_ HWND ListViewHandle
    )
{
    PH_STRING_BUILDER stringBuilder;
    ULONG i = 0;
    LVCOLUMN lvColumn;

    PhInitializeStringBuilder(&stringBuilder, 20);

    PhAppendFormatStringBuilder(&stringBuilder, L"@%lu|", PhGlobalDpi);

    lvColumn.mask = LVCF_WIDTH | LVCF_ORDER;

    while (ListView_GetColumn(ListViewHandle, i, &lvColumn))
    {
        PhAppendFormatStringBuilder(
            &stringBuilder,
            L"%u,%u|",
            lvColumn.iOrder,
            lvColumn.cx
            );
        i++;
    }

    if (stringBuilder.String->Length != 0)
        PhRemoveEndStringBuilder(&stringBuilder, 1);

    return PhFinalStringBuilderString(&stringBuilder);
}

VOID PhLoadListViewColumnsFromSetting(
    _In_ PWSTR Name,
    _In_ HWND ListViewHandle
    )
{
    PPH_STRING string;

    string = PhGetStringSetting(Name);
    PhLoadListViewColumnSettings(ListViewHandle, string);
    PhDereferenceObject(string);
}

VOID PhSaveListViewColumnsToSetting(
    _In_ PWSTR Name,
    _In_ HWND ListViewHandle
    )
{
    PPH_STRING string;

    string = PhSaveListViewColumnSettings(ListViewHandle);
    PhSetStringSetting2(Name, &string->sr);
    PhDereferenceObject(string);
}

VOID PhLoadListViewSortColumnsFromSetting(
    _In_ PWSTR Name,
    _In_ HWND ListViewHandle
    )
{
    PPH_STRING string;
    ULONG sortColumn = 0;
    PH_SORT_ORDER sortOrder = AscendingSortOrder;
    PH_STRINGREF remainingPart;

    string = PhGetStringSetting(Name);

    if (PhIsNullOrEmptyString(string))
        return;

    remainingPart = string->sr;

    if (remainingPart.Length != 0)
    {
        PH_STRINGREF columnPart;
        PH_STRINGREF orderPart;
        ULONG64 integer;

        if (!PhSplitStringRefAtChar(&remainingPart, L',', &columnPart, &orderPart))
            return;

        if (!PhStringToInteger64(&columnPart, 10, &integer))
            return;

        sortColumn = (ULONG)integer;

        if (!PhStringToInteger64(&orderPart, 10, &integer))
            return;

        sortOrder = (ULONG)integer;
    }

    ExtendedListView_SetSort(ListViewHandle, sortColumn, sortOrder);

    PhDereferenceObject(string);
}

VOID PhSaveListViewSortColumnsToSetting(
    _In_ PWSTR Name,
    _In_ HWND ListViewHandle
    )
{
    PPH_STRING string;
    ULONG sortColumn = 0;
    PH_SORT_ORDER sortOrder = AscendingSortOrder;

    if (ExtendedListView_GetSort(ListViewHandle, &sortColumn, &sortOrder))
    {
        PH_FORMAT format[3];

        // %lu,%lu
        PhInitFormatU(&format[0], sortColumn);
        PhInitFormatC(&format[1], L',');
        PhInitFormatU(&format[2], sortOrder);

        string = PhFormat(format, RTL_NUMBER_OF(format), 16);
    }
    else
    {
        string = PhCreateString(L"0,0");
    }

    PhSetStringSetting2(Name, &string->sr);
    PhDereferenceObject(string);
}

VOID PhLoadListViewGroupStatesFromSetting(
    _In_ PWSTR Name,
    _In_ HWND ListViewHandle
    )
{
    ULONG64 countInteger;
    PPH_STRING settingsString;
    PH_STRINGREF remaining;
    PH_STRINGREF part;

    settingsString = PhaGetStringSetting(Name);
    remaining = settingsString->sr;

    if (remaining.Length == 0)
        return;

    if (!PhSplitStringRefAtChar(&remaining, L'|', &part, &remaining))
        return;

    if (!PhStringToInteger64(&part, 10, &countInteger))
        return;

    for (INT index = 0; index < (INT)countInteger; index++)
    {
        ULONG64 groupId;
        ULONG64 stateMask;
        PH_STRINGREF groupIdPart;
        PH_STRINGREF stateMaskPart;

        if (remaining.Length == 0)
            break;

        PhSplitStringRefAtChar(&remaining, L'|', &groupIdPart, &remaining);

        if (groupIdPart.Length == 0)
            break;

        PhSplitStringRefAtChar(&remaining, L'|', &stateMaskPart, &remaining);

        if (stateMaskPart.Length == 0)
            break;

        if (!PhStringToInteger64(&groupIdPart, 10, &groupId))
            break;
        if (!PhStringToInteger64(&stateMaskPart, 10, &stateMask))
            break;

        ListView_SetGroupState(
            ListViewHandle,
            (INT)groupId,
            LVGS_NORMAL | LVGS_COLLAPSED,
            (UINT)stateMask
            );
    }
}

VOID PhSaveListViewGroupStatesToSetting(
    _In_ PWSTR Name,
    _In_ HWND ListViewHandle
    )
{
    INT index;
    INT count;
    PPH_STRING settingsString;
    PH_STRING_BUILDER stringBuilder;

    PhInitializeStringBuilder(&stringBuilder, 100);

    count = (INT)ListView_GetGroupCount(ListViewHandle);

    PhAppendFormatStringBuilder(
        &stringBuilder,
        L"%d|",
        count
        );

    for (index = 0; index < count; index++)
    {
        LVGROUP group;

        memset(&group, 0, sizeof(LVGROUP));
        group.cbSize = sizeof(LVGROUP);
        group.mask = LVGF_GROUPID | LVGF_STATE;
        group.stateMask = LVGS_NORMAL | LVGS_COLLAPSED;

        if (ListView_GetGroupInfoByIndex(ListViewHandle, index, &group) == -1)
            continue;

        PhAppendFormatStringBuilder(
            &stringBuilder,
            L"%d|%u|",
            group.iGroupId,
            group.state
            );
    }

    if (stringBuilder.String->Length != 0)
        PhRemoveEndStringBuilder(&stringBuilder, 1);

    settingsString = PH_AUTO(PhFinalStringBuilderString(&stringBuilder));
    PhSetStringSetting2(Name, &settingsString->sr);
}
