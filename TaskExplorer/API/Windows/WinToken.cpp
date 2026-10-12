/*
 * Task Explorer -
 *   qt wrapper and support functions based on tokprop.c
 *
 * Copyright (C) 2010-2012 wj32
 * Copyright (C) 2017-2019 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 *
 */

#include "stdafx.h"
#include "WinToken.h"
#include "ProcessHacker.h"
#include "WinSecurityEditor.h"
#include "WindowsAPI.h"

struct SWinToken
{
	SWinToken()
	{
		pSystem = NULL;
		QueryHandle = NULL;
		QueryAux = NULL;
		Handle = NULL;
		Type = CWinToken::eProcess;
		ExtAccess = false;

		tokenLuid = authenticationLuid = tokenModifiedLuid = 0;
	}

	CSystemAPI* pSystem; // CWinToken__OpenProcessToken is a C callback and only gets this struct
	HANDLE QueryHandle;
	HANDLE QueryAux;
	HANDLE Handle;
	CWinToken::EQueryType Type;
	bool ExtAccess;

	quint32 tokenLuid;
	quint32 authenticationLuid;
	quint32 tokenModifiedLuid;
};

CWinToken::CWinToken(QObject *parent)
	:CTokenInfo(parent)
{
	m_IsAppContainer = 0;
	m_SessionId = 0;
	m_Elevated = false;
	m_ElevationType = 0;
	m_IntegrityLevel = -1;
	m_Virtualization = 0;

	m_TokenState = eNotInitialized;

	m = new SWinToken();
}

CWinToken::~CWinToken()
{
	if (m->Type != eProcess && m->Type != eOriginalPrimary && m->Type != eOriginalThread)
		NtClose(m->QueryHandle);
	delete m;
}

NTSTATUS NTAPI CWinToken__OpenProcessToken(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
{
	NTSTATUS status = STATUS_INVALID_PARAMETER;
	SWinToken* m = (SWinToken*)Context;
	if (m->Type == CWinToken::eLinked)
	{
		status = PhGetTokenLinkedToken(m->QueryHandle, Handle);
	}
	else if (m->Type == CWinToken::eHandle)
	{
		status = NtDuplicateObject(m->QueryHandle, m->Handle, NtCurrentProcess(), Handle, DesiredAccess, 0, 0 );
	}
	else if (m->Type == CWinToken::eThread)
	{
		status = NtOpenThreadToken(m->QueryHandle, DesiredAccess, TRUE, Handle);
	}
	else if (m->Type == CWinToken::eOriginalPrimary)
	{
		CSandboxieAPI* pSandboxieAPI = ((CWindowsAPI*)m->pSystem)->GetSandboxieAPI();
		*Handle = pSandboxieAPI ? (HANDLE)pSandboxieAPI->OpenOriginalToken((quint64)m->QueryHandle) : NULL;
		if (*Handle != NULL)
			status = STATUS_SUCCESS;
	}
	else if (m->Type == CWinToken::eOriginalThread)
	{
		CSandboxieAPI* pSandboxieAPI = ((CWindowsAPI*)m->pSystem)->GetSandboxieAPI();
		*Handle = pSandboxieAPI ? (HANDLE)pSandboxieAPI->OpenOriginalToken((quint64)m->QueryHandle, (quint64)m->QueryAux) : NULL;
		if (*Handle != NULL)
			status = STATUS_SUCCESS;
	}
	else
	{
		HANDLE processHandle;

		// Note: we are using the query handle of the process instance instead of pid and re opening it
		processHandle = m->QueryHandle;
		//if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION, (HANDLE)Context)))
		//    return status;

		// HACK: Add extra access_masks for querying default token. (dmex)
		if (!m->ExtAccess || !NT_SUCCESS(status = PhOpenProcessToken(processHandle, DesiredAccess | TOKEN_READ | TOKEN_ADJUST_DEFAULT | READ_CONTROL, Handle)))
		{
			status = PhOpenProcessToken(processHandle, DesiredAccess, Handle);
		}

		//NtClose(processHandle);
	}
	return status;
}

//
// The masks CTokenInfo names are the Win32 ones; a typo here has to break the
// build rather than quietly read a token wrong.
//
static_assert(CTokenInfo::ePrivilegeEnabledByDefault == SE_PRIVILEGE_ENABLED_BY_DEFAULT, "privilege enabled by default");
static_assert(CTokenInfo::ePrivilegeEnabled         == SE_PRIVILEGE_ENABLED,             "privilege enabled");
static_assert(CTokenInfo::ePrivilegeRemoved         == SE_PRIVILEGE_REMOVED,             "privilege removed");
static_assert(CTokenInfo::ePrivilegeUsedForAccess   == SE_PRIVILEGE_USED_FOR_ACCESS,     "privilege used for access");

static_assert(CTokenInfo::eGroupMandatory        == SE_GROUP_MANDATORY,          "group mandatory");
static_assert(CTokenInfo::eGroupEnabledByDefault == SE_GROUP_ENABLED_BY_DEFAULT, "group enabled by default");
static_assert(CTokenInfo::eGroupEnabled          == SE_GROUP_ENABLED,            "group enabled");
static_assert(CTokenInfo::eGroupOwner            == SE_GROUP_OWNER,              "group owner");
static_assert(CTokenInfo::eGroupUseForDenyOnly   == SE_GROUP_USE_FOR_DENY_ONLY,  "group use for deny only");
static_assert(CTokenInfo::eGroupIntegrity        == SE_GROUP_INTEGRITY,          "group integrity");
static_assert(CTokenInfo::eGroupIntegrityEnabled == SE_GROUP_INTEGRITY_ENABLED,  "group integrity enabled");
static_assert(CTokenInfo::eGroupResource         == SE_GROUP_RESOURCE,           "group resource");
static_assert(CTokenInfo::eGroupLogonId          == SE_GROUP_LOGON_ID,           "group logon id");

static_assert(CTokenInfo::eSecAttrInvalid     == TOKEN_SECURITY_ATTRIBUTE_TYPE_INVALID,      "sec attr invalid");
static_assert(CTokenInfo::eSecAttrInt64       == TOKEN_SECURITY_ATTRIBUTE_TYPE_INT64,        "sec attr int64");
static_assert(CTokenInfo::eSecAttrUInt64      == TOKEN_SECURITY_ATTRIBUTE_TYPE_UINT64,       "sec attr uint64");
static_assert(CTokenInfo::eSecAttrString      == TOKEN_SECURITY_ATTRIBUTE_TYPE_STRING,       "sec attr string");
static_assert(CTokenInfo::eSecAttrFqbn        == TOKEN_SECURITY_ATTRIBUTE_TYPE_FQBN,         "sec attr fqbn");
static_assert(CTokenInfo::eSecAttrSid         == TOKEN_SECURITY_ATTRIBUTE_TYPE_SID,          "sec attr sid");
static_assert(CTokenInfo::eSecAttrBoolean     == TOKEN_SECURITY_ATTRIBUTE_TYPE_BOOLEAN,      "sec attr boolean");
static_assert(CTokenInfo::eSecAttrOctetString == TOKEN_SECURITY_ATTRIBUTE_TYPE_OCTET_STRING, "sec attr octet string");

static_assert(CTokenInfo::eSecAttrNonInheritable     == TOKEN_SECURITY_ATTRIBUTE_NON_INHERITABLE,      "sec attr non inheritable");
static_assert(CTokenInfo::eSecAttrValueCaseSensitive == TOKEN_SECURITY_ATTRIBUTE_VALUE_CASE_SENSITIVE, "sec attr case sensitive");
static_assert(CTokenInfo::eSecAttrUseForDenyOnly     == TOKEN_SECURITY_ATTRIBUTE_USE_FOR_DENY_ONLY,    "sec attr use for deny only");
static_assert(CTokenInfo::eSecAttrDisabledByDefault  == TOKEN_SECURITY_ATTRIBUTE_DISABLED_BY_DEFAULT,  "sec attr disabled by default");
static_assert(CTokenInfo::eSecAttrDisabled           == TOKEN_SECURITY_ATTRIBUTE_DISABLED,             "sec attr disabled");
static_assert(CTokenInfo::eSecAttrMandatory          == TOKEN_SECURITY_ATTRIBUTE_MANDATORY,            "sec attr mandatory");
static_assert(CTokenInfo::eSecAttrCompareIgnore      == TOKEN_SECURITY_ATTRIBUTE_COMPARE_IGNORE,       "sec attr compare ignore");

static_assert(CTokenInfo::eElevationDefault == TokenElevationTypeDefault, "elevation default");
static_assert(CTokenInfo::eElevationFull    == TokenElevationTypeFull,    "elevation full");
static_assert(CTokenInfo::eElevationLimited == TokenElevationTypeLimited, "elevation limited");

void CWinToken::OnSidResolved(const QByteArray& SID, const QString& Name)
{
	QWriteLocker Locker(&m_Mutex);

	if(SID == m_UserSid || (m_IsAppContainer == 2 && SID == m_OwnerSid))
		m_UserName = Name;

	if(SID == m_OwnerSid)
		m_OwnerName = Name;

	if(SID == m_GroupSid)
		m_GroupName = Name;
	
	QMap<QByteArray, SGroup>::iterator I = m_Groups.find(SID);
	if (I != m_Groups.end())
		I.value().Name = Name;
}

CWinToken* CWinToken::NewSystemToken(const CSystemPtr& pSystem)
{
	CWinToken* pToken = new CWinToken();
	pToken->SetSystem(pSystem);
	pToken->m->pSystem = pSystem.data();
	pToken->m_UserSid = QByteArray((char*)&PhSeLocalSystemSid, RtlLengthSid((PSID)&PhSeLocalSystemSid));
	pToken->m_UserName = ((CWindowsAPI*)pSystem.data())->GetSidResolver()->GetSidFullName(pToken->m_UserSid, pToken, SLOT(OnSidResolved(const QByteArray&, const QString&)));
	return pToken;
}

CWinToken* CWinToken::TokenFromHandle(const CSystemPtr& pSystem, quint64 ProcessId, quint64 HandleId)
{
	HANDLE processHandle;
    if (!NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_DUP_HANDLE, (HANDLE)ProcessId)))
        return NULL;

	CWinToken* pToken = new CWinToken();
	pToken->SetSystem(pSystem);
	pToken->m->pSystem = pSystem.data();
	pToken->m->Type = eHandle;
	pToken->m->QueryHandle = processHandle;
	pToken->m->Handle = (HANDLE)HandleId;
	pToken->InitStaticData();
	return pToken;
}

CWinToken* CWinToken::TokenFromThread(const CSystemPtr& pSystem, quint64 ThreadId)
{
	HANDLE threadHandle;
    if (!NT_SUCCESS(PhOpenThread(&threadHandle, THREAD_QUERY_LIMITED_INFORMATION, (HANDLE)ThreadId)))
        return NULL;

	CWinToken* pToken = new CWinToken();
	pToken->SetSystem(pSystem);
	pToken->m->pSystem = pSystem.data();
	pToken->m->Type = eThread;
	pToken->m->QueryHandle = threadHandle;
	pToken->InitStaticData();
	return pToken;
}

CWinToken* CWinToken::TokenFromProcess(const CSystemPtr& pSystem, void* QueryHandle)
{
	CWinToken* pToken = new CWinToken();
	pToken->SetSystem(pSystem);
	pToken->m->pSystem = pSystem.data();
	pToken->m->Type = eProcess;
	pToken->m->QueryHandle = QueryHandle;
	pToken->InitStaticData();
	return pToken;
}

CWinToken* CWinToken::OriginalToken(const CSystemPtr& pSystem, quint64 ProcessId)
{
	CWinToken* pOriginalToken = new CWinToken();
	pOriginalToken->SetSystem(pSystem);
	pOriginalToken->m->pSystem = pSystem.data();
	pOriginalToken->m->Type = eOriginalPrimary;
	pOriginalToken->m->QueryHandle = (HANDLE)ProcessId;
	pOriginalToken->InitStaticData();
	return pOriginalToken;
}

CWinToken* CWinToken::OriginalToken(const CSystemPtr& pSystem, quint64 ProcessId, quint64 ThreadId)
{
	CWinToken* pOriginalToken = new CWinToken();
	pOriginalToken->SetSystem(pSystem);
	pOriginalToken->m->pSystem = pSystem.data();
	pOriginalToken->m->Type = eOriginalThread;
	pOriginalToken->m->QueryHandle = (HANDLE)ProcessId;
	pOriginalToken->m->QueryAux = (HANDLE)ThreadId;
	pOriginalToken->InitStaticData();
	return pOriginalToken;
}

bool CWinToken::InitStaticData()
{
	// Note: Once a process has started running the process token is locked and can no longer be modified. 
	//		However using CREATE_SUSPENDED and calling the undocumented NtSetInformationProcess function, 
	//		with the ProcessAccessToken parameter, the token can be changed before calling ResumeThread().
	//		
	//		Generall it is possible to alter a primary token on early stages of process start.
	//
	//		Hence we have to be able to handle teh case when the entire Token gets replaced.
	//
	return true;
}

bool CWinToken::UpdateDynamicData(bool MonitorChange, bool IsOrWasRunning)
{
	QWriteLocker Locker(&m_Mutex);

	// When token data has been initialized and we are not monitoring for Token changes we can return here.
	if (m_TokenState == eInitialized && !MonitorChange)
		return false;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(CWinToken__OpenProcessToken(&tokenHandle, TOKEN_QUERY, m)))
		return false;


	// if we are monitoring Token change we always update some values

	// Integrity
	PH_INTEGRITY_LEVEL integrityLevel;
	PCPH_STRINGREF integrityString;
	if (NT_SUCCESS(PhGetTokenIntegrityLevelEx(tokenHandle, &integrityLevel, &integrityString)))
	{
		if (m_IntegrityLevel != integrityLevel.Level)
		{
			m_IntegrityLevel = integrityLevel.Level;
		}
	}

	// A feature of User Account Control (UAC) that allows per-machine file and registry operations to target virtual, 
	//		per-user file and registry locations rather than the actual per-machine locations.
	BOOLEAN isVirtualizationAllowed;
    if (NT_SUCCESS(PhGetTokenIsVirtualizationAllowed(tokenHandle, &isVirtualizationAllowed)))
    {
        if (isVirtualizationAllowed)
        {
			m_Virtualization = VIRTUALIZATION_ALLOWED;

			BOOLEAN isVirtualizationEnabled;
            if (NT_SUCCESS(PhGetTokenIsVirtualizationEnabled(tokenHandle, &isVirtualizationEnabled)))
            {
				if (isVirtualizationEnabled)
					m_Virtualization |= VIRTUALIZATION_ENABLED;
            }
        }
        else
			m_Virtualization = VIRTUALIZATION_NOT_ALLOWED;
    }

	TOKEN_ELEVATION_TYPE elevationType;
	if (NT_SUCCESS(PhGetTokenElevationType(tokenHandle, &elevationType)))
		m_ElevationType = elevationType;

	//BOOLEAN elevated = TRUE;
	//if (NT_SUCCESS(PhGetTokenIsElevated(tokenHandle, &elevated)))
	//	m_Elevated = elevated;
	m_Elevated = (m_ElevationType == TokenElevationTypeFull);


	// if the token is initialized we only need to check if it changed
	TOKEN_STATISTICS statistics;
	if (NT_SUCCESS(PhGetTokenStatistics(tokenHandle, &statistics)))
	{
		if (m_TokenState == eInitialized)
		{
			if (m->tokenLuid != statistics.TokenId.LowPart
			 || m->authenticationLuid != statistics.AuthenticationId.LowPart
			 || m->tokenModifiedLuid != statistics.ModifiedId.LowPart)
			{
				m_TokenState = eHasChanged;
			}
			else
			{
				NtClose(tokenHandle);

				return false;
			}
		}
		else
		{
			m->tokenLuid = statistics.TokenId.LowPart;
			m->authenticationLuid = statistics.AuthenticationId.LowPart;
			m->tokenModifiedLuid = statistics.ModifiedId.LowPart;
		}
	}


	// full update

	if (m_TokenState < eInitialized)
	{
		if (IsOrWasRunning)
			m_TokenState = eInitialized;
		else if(m_TokenState == eNotInitialized)
			m_TokenState = eNotYetLocked;
	}

	BOOLEAN tokenIsAppContainer = FALSE;
    PhGetTokenIsAppContainer(tokenHandle, &tokenIsAppContainer);
	m_IsAppContainer = tokenIsAppContainer != FALSE ? 1 : 0;

	PH_TOKEN_USER tokenUser;
    if (NT_SUCCESS(PhGetTokenUser(tokenHandle, &tokenUser)))
    {
		m_UserSid = QByteArray((char*)tokenUser.User.Sid, RtlLengthSid(tokenUser.User.Sid));

        if (!tokenIsAppContainer) // HACK (dmex)
        {
			m_UserName = ((CWindowsAPI*)GetSystem().data())->GetSidResolver()->GetSidFullName(m_UserSid, this, SLOT(OnSidResolved(const QByteArray&, const QString&)));
        }

		PPH_STRING stringUserSid;
        if (stringUserSid = PhSidToStringSid(tokenUser.User.Sid))
			m_SidString = CastPhString(stringUserSid);
    }

	PH_TOKEN_OWNER tokenOwner;
    if (NT_SUCCESS(PhGetTokenOwner(tokenHandle, &tokenOwner)))
    {
		m_OwnerSid = QByteArray((char*)tokenOwner.Owner.Sid, RtlLengthSid(tokenOwner.Owner.Sid));

		m_OwnerName = ((CWindowsAPI*)GetSystem().data())->GetSidResolver()->GetSidFullName(m_OwnerSid, this, SLOT(OnSidResolved(const QByteArray&, const QString&)));
    }

	PTOKEN_PRIMARY_GROUP tokenPrimaryGroup;
    if (NT_SUCCESS(PhGetTokenPrimaryGroup(tokenHandle, &tokenPrimaryGroup)))
    {
		m_GroupSid = QByteArray((char*)tokenPrimaryGroup->PrimaryGroup, RtlLengthSid(tokenPrimaryGroup->PrimaryGroup));

		m_GroupName = ((CWindowsAPI*)GetSystem().data())->GetSidResolver()->GetSidFullName(m_GroupSid, this, SLOT(OnSidResolved(const QByteArray&, const QString&)));

        PhFree(tokenPrimaryGroup);
    }

	ULONG sessionId;
	if (NT_SUCCESS(PhGetTokenSessionId(tokenHandle, &sessionId)))
		m_SessionId = sessionId;


    if (WindowsVersion >= WINDOWS_8)
    {
		PTOKEN_APPCONTAINER_INFORMATION appContainerInfo;
		PPH_STRING appContainerName = NULL;
	    PPH_STRING appContainerSid = NULL;

        if (NT_SUCCESS(PhQueryTokenVariableSize(tokenHandle, TokenAppContainerSid, (PVOID*)&appContainerInfo)))
        {
            if (appContainerInfo->TokenAppContainer)
            {
                appContainerName = PhGetAppContainerName(appContainerInfo->TokenAppContainer);
                appContainerSid = PhSidToStringSid(appContainerInfo->TokenAppContainer);

				if (appContainerName)
				{
					m_ContainerName = CastPhString(appContainerName);

					m_IsAppContainer = 2;

					m_UserName = ((CWindowsAPI*)GetSystem().data())->GetSidResolver()->GetSidFullName(m_OwnerSid, this, SLOT(OnSidResolved(const QByteArray&, const QString&)));
				}

				if (appContainerSid)
				{
					m_UserSid = QByteArray((char*)appContainerInfo->TokenAppContainer, RtlLengthSid(appContainerInfo->TokenAppContainer));

					m_SidString = CastPhString(appContainerSid);
				}
            }

            PhFree(appContainerInfo);
        }
    }

	NtClose(tokenHandle);

	return true;
}

//
// Fills in the text forms of a group's SID. Done here, where the SID is still
// a live structure and the local authority database is the right one to ask.
//
static void ResolveGroupSid(CTokenInfo::SGroup& Group, PSID pSid)
{
	PPH_STRING stringSid;
	if (stringSid = PhSidToStringSid(pSid))
		Group.SidString = CastPhString(stringSid);

	Group.AccountType = QString::fromWCharArray(PhGetSidAccountTypeString(pSid));

	static_assert(CTokenInfo::eSidUser           == SidTypeUser,           "sid type user");
	static_assert(CTokenInfo::eSidGroup          == SidTypeGroup,          "sid type group");
	static_assert(CTokenInfo::eSidDomain         == SidTypeDomain,         "sid type domain");
	static_assert(CTokenInfo::eSidAlias          == SidTypeAlias,          "sid type alias");
	static_assert(CTokenInfo::eSidWellKnownGroup == SidTypeWellKnownGroup, "sid type well known group");
	static_assert(CTokenInfo::eSidDeletedAccount == SidTypeDeletedAccount, "sid type deleted account");
	static_assert(CTokenInfo::eSidInvalid        == SidTypeInvalid,        "sid type invalid");
	static_assert(CTokenInfo::eSidUnknown        == SidTypeUnknown,        "sid type unknown");
	static_assert(CTokenInfo::eSidComputer       == SidTypeComputer,       "sid type computer");
	static_assert(CTokenInfo::eSidLabel          == SidTypeLabel,          "sid type label");
	static_assert(CTokenInfo::eSidLogonSession   == SidTypeLogonSession,   "sid type logon session");

	SID_NAME_USE sidUse;
	if (NT_SUCCESS(PhLookupSid(pSid, NULL, NULL, &sidUse)))
		Group.Use = (quint8)sidUse;
}

bool CWinToken::UpdateExtendedData()
{
	QWriteLocker Locker(&m_Mutex); 

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(CWinToken__OpenProcessToken(&tokenHandle, TOKEN_QUERY, m)))
		return false;

    //PhpUpdateTokenDangerousFlags
	TOKEN_MANDATORY_POLICY mandatoryPolicy;
    if (NT_SUCCESS(PhGetTokenMandatoryPolicy(tokenHandle, &mandatoryPolicy)))
    {
        // The disabled no-write-up policy is considered to be dangerous (diversenok)
		SetDangerousFlag(eNoWriteUpDisabled, (mandatoryPolicy.Policy & TOKEN_MANDATORY_POLICY_NO_WRITE_UP) == 0);
    }

    BOOLEAN isSandboxInert;
    if (NT_SUCCESS(PhGetTokenIsSandBoxInert(tokenHandle, &isSandboxInert)))
    {
		// The presence of SandboxInert flag is considered dangerous (diversenok)
		SetDangerousFlag(eSandboxInertEnabled, isSandboxInert);
    }

	BOOLEAN isUIAccess;
	if (NT_SUCCESS(PhGetTokenUIAccess(tokenHandle, &isUIAccess)))
	{
		// The presence of UIAccess flag is considered dangerous (diversenok)
		SetDangerousFlag(eUIAccessEnabled, isUIAccess);
	}

    //PhpUpdateTokenGroups
	PTOKEN_GROUPS Groups = NULL;
    if (NT_SUCCESS(PhGetTokenGroups(tokenHandle, &Groups)))
    {
		for (ULONG i = 0; i < Groups->GroupCount; i++)
		{
			QByteArray Sid = QByteArray((char*)Groups->Groups[i].Sid, RtlLengthSid(Groups->Groups[i].Sid));

			SGroup &Group = m_Groups[Sid];
			Group.Sid = Sid;
			Group.Restricted = false;
			Group.Attributes = Groups->Groups[i].Attributes;
			ResolveGroupSid(Group, Groups->Groups[i].Sid);
			Group.Name = ((CWindowsAPI*)GetSystem().data())->GetSidResolver()->GetSidFullName(Sid, this, SLOT(OnSidResolved(const QByteArray&, const QString&)));
		}
		
		PhFree(Groups);
    }

	PTOKEN_GROUPS RestrictedSIDs = NULL;
	if (NT_SUCCESS(PhGetTokenRestrictedSids(tokenHandle, &RestrictedSIDs)))
    {
		for (ULONG i = 0; i < RestrictedSIDs->GroupCount; i++)
		{
			QByteArray Sid = QByteArray((char*)RestrictedSIDs->Groups[i].Sid, RtlLengthSid(RestrictedSIDs->Groups[i].Sid));

			SGroup &Group = m_Groups[Sid];
			Group.Sid = Sid;
			Group.Restricted = true;
			Group.Attributes = RestrictedSIDs->Groups[i].Attributes;
			ResolveGroupSid(Group, RestrictedSIDs->Groups[i].Sid);
			Group.Name = ((CWindowsAPI*)GetSystem().data())->GetSidResolver()->GetSidFullName(Sid, this, SLOT(OnSidResolved(const QByteArray&, const QString&)));
		}

		PhFree(RestrictedSIDs);
    }
	//

    //PhpUpdateTokenPrivileges
	m_Privileges.clear();
	PTOKEN_PRIVILEGES privileges = NULL;
	if (NT_SUCCESS(PhGetTokenPrivileges(tokenHandle, &privileges)))
	{
		for (ULONG i = 0; i < privileges->PrivilegeCount; i++)
		{
			PPH_STRING privilegeName;
			if (NT_SUCCESS(PhLookupPrivilegeName(&privileges->Privileges[i].Luid, &privilegeName)))
			{
				PPH_STRING privilegeDisplayName = NULL;
				PhLookupPrivilegeDisplayName(&privilegeName->sr, &privilegeDisplayName);

				QString Name = CastPhString(privilegeName);
				SPrivilege &Privilege = m_Privileges[Name];
				Privilege.Name = Name;
				Privilege.lLuid = privileges->Privileges[i].Luid.LowPart;
				Privilege.hLuid = privileges->Privileges[i].Luid.HighPart;
				Privilege.Description = CastPhString(privilegeDisplayName);
				Privilege.Attributes = privileges->Privileges[i].Attributes;
			}
		}

		PhFree(privileges);
	}
	//

	NtClose(tokenHandle);

	return true;
}

void CWinToken::SetDangerousFlag(EDangerousFlags Flag, bool Set)
{
	if (Set)
		m_DangerousFlags.insert(Flag);
	else
		m_DangerousFlags.remove(Flag);
}

PH_ACCESS_ENTRY GroupDescriptionEntries[6] =
{
    { NULL, SE_GROUP_INTEGRITY | SE_GROUP_INTEGRITY_ENABLED, FALSE, FALSE, (PWSTR)L"Integrity" },
    { NULL, SE_GROUP_LOGON_ID, FALSE, FALSE, (PWSTR)L"Logon Id" },
    { NULL, SE_GROUP_OWNER, FALSE, FALSE, (PWSTR)L"Owner" },
    { NULL, SE_GROUP_MANDATORY, FALSE, FALSE, (PWSTR)L"Mandatory" },
    { NULL, SE_GROUP_USE_FOR_DENY_ONLY, FALSE, FALSE, (PWSTR)L"Use for deny only" },
    { NULL, SE_GROUP_RESOURCE, FALSE, FALSE, (PWSTR)L"Resource" }
};




	








STATUS CWinToken::SetVirtualizationEnabled(bool bSet)
{
	QWriteLocker Locker(&m_Mutex);

    NTSTATUS status;

    HANDLE tokenHandle;
    if (NT_SUCCESS(status = PhOpenProcessToken(m->QueryHandle, TOKEN_WRITE, &tokenHandle)))
    {
        status = PhSetTokenIsVirtualizationEnabled(tokenHandle, bSet);
        NtClose(tokenHandle);
    }

    if (!NT_SUCCESS(status))
    {
		return ERR(TE_SetProcVirtualization, status);
    }

    return OK;
}

STATUS CWinToken::SetIntegrityLevel(quint32 IntegrityLevel) const
{
	QWriteLocker Locker(&m_Mutex);

	NTSTATUS status;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(status = CWinToken__OpenProcessToken(&tokenHandle, TOKEN_QUERY | TOKEN_ADJUST_DEFAULT, m)))
		return ERR(TE_OpenToken, status);

    static SID_IDENTIFIER_AUTHORITY mandatoryLabelAuthority = SECURITY_MANDATORY_LABEL_AUTHORITY;

    UCHAR newSidBuffer[FIELD_OFFSET(SID, SubAuthority) + sizeof(ULONG)];
    PSID newSid;
    newSid = (PSID)newSidBuffer;
    RtlInitializeSid(newSid, &mandatoryLabelAuthority, 1);
    *RtlSubAuthoritySid(newSid, 0) = IntegrityLevel;

	TOKEN_MANDATORY_LABEL mandatoryLabel;
    mandatoryLabel.Label.Sid = newSid;
    mandatoryLabel.Label.Attributes = SE_GROUP_INTEGRITY;

    status = NtSetInformationToken(tokenHandle, TokenIntegrityLevel, &mandatoryLabel, sizeof(TOKEN_MANDATORY_LABEL));

    NtClose(tokenHandle);

	if (!NT_SUCCESS(status))
	{
		return ERR(TE_SetTokenInfo, status);
	}

	return OK;
}

STATUS CWinToken::PrivilegeAction(const SPrivilege& Privilege, EAction Action, bool bForce)
{
	if(!bForce && Action == eRemove)
		return ERR(TE_ConfirmRemovePrivileges, ERROR_CONFIRM);

	QWriteLocker Locker(&m_Mutex);

	NTSTATUS status;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(status = CWinToken__OpenProcessToken(&tokenHandle, TOKEN_ADJUST_PRIVILEGES, m)))
		return ERR(TE_OpenToken, status);

    ULONG newAttributes = Privilege.Attributes;

    switch (Action)
    {
    case eEnable:
        newAttributes |= SE_PRIVILEGE_ENABLED;
        break;
    case eDisable:
        newAttributes &= ~SE_PRIVILEGE_ENABLED;
        break;
    case eReset:
        {
            if (newAttributes & SE_PRIVILEGE_ENABLED_BY_DEFAULT)
                newAttributes |= SE_PRIVILEGE_ENABLED;
            else
                newAttributes &= ~SE_PRIVILEGE_ENABLED;
        }
        break;
    case eRemove:
        newAttributes = SE_PRIVILEGE_REMOVED;
        break;
    }

	
	LUID luid;
	luid.LowPart = Privilege.lLuid;
	luid.HighPart = Privilege.hLuid;

	// PhSetTokenPrivilege returns an NTSTATUS (STATUS_SUCCESS == 0), not a
	// BOOLEAN. Capturing it in a BOOLEAN inverted the test: success (0) looked
	// like failure - hence the bogus 0xffffffff while the change still took
	// effect - and a real failure like STATUS_UNSUCCESSFUL (low byte 0x01) was
	// read as success. Test the status properly and surface the real code.
	status = PhSetTokenPrivilege(tokenHandle, NULL, &luid, newAttributes);

    NtClose(tokenHandle);

	if (!NT_SUCCESS(status))
		return ERR(TE_SetTokenPriv, status);

	return OK;
}

STATUS CWinToken::GroupAction(const SGroup& Group, EAction Action)
{
	QWriteLocker Locker(&m_Mutex);

	NTSTATUS status;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(status = CWinToken__OpenProcessToken(&tokenHandle, TOKEN_ADJUST_GROUPS, m)))
		return ERR(TE_OpenToken, status);

	ULONG newAttributes = Group.Attributes;

	switch (Action)
	{
	case eEnable:
		newAttributes |= SE_GROUP_ENABLED;
		break;
	case eDisable:
		newAttributes &= ~SE_GROUP_ENABLED;
		break;
	case eReset:
		{
			if (newAttributes & SE_GROUP_ENABLED_BY_DEFAULT)
				newAttributes |= SE_GROUP_ENABLED;
			else
				newAttributes &= ~SE_GROUP_ENABLED;
		}
		break;
	case eRemove:
		ASSERT(0);
		break;
	}

	SID sid;
	memcpy(&sid, Group.Sid.data(), sizeof(SID));

	status = PhSetTokenGroups(tokenHandle, NULL, &sid, newAttributes);

	NtClose(tokenHandle);

	if (!NT_SUCCESS(status))
		return ERR(TE_SetTokenGroups, status);

	return OK;
}

NTSTATUS NTAPI CWinToken__cbPermissionsClosed(_In_ HANDLE Handle, _In_ BOOLEAN Release, _In_opt_ PVOID Context)
{
	if (Release) {
		SWinToken* context = (SWinToken*)Context;
		delete context;
	}

	return STATUS_SUCCESS;
}

CSecurityEditablePtr CWinToken::GetSecurityObject(bool bDefaultToken) const
{
	QReadLocker Locker(&m_Mutex);

	//
	// The opener needs the handles and the kind of token this is. A copy of the
	// struct travels with the object, so nothing has to be freed afterwards.
	//
	SWinToken Context;
	Context.QueryHandle = m->QueryHandle;
	Context.Handle = m->Handle;
	Context.Type = m->Type;
	if (bDefaultToken)
		Context.ExtAccess = true;
	Locker.unlock();

	return CSecurityEditablePtr(new CWinSecurityObject(
		QString(),
		bDefaultToken ? "TokenDefault" : "Token",
		(CWinSecurityObject::POpenObject)CWinToken__OpenProcessToken,
		QByteArray((const char*)&Context, sizeof(Context))));
}

QSharedPointer<CTokenInfo> CWinToken::GetLinkedToken()
{
	QReadLocker Locker(&m_Mutex); 

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(CWinToken__OpenProcessToken(&tokenHandle, TOKEN_QUERY, m)))
		return QSharedPointer<CWinToken>();

	QSharedPointer<CWinToken> pLinkedToken = QSharedPointer<CWinToken>(new CWinToken());
	pLinkedToken->SetSystem(GetSystem());
	pLinkedToken->m->pSystem = GetSystem().data();
	pLinkedToken->m->Type = eLinked;
	pLinkedToken->m->QueryHandle = tokenHandle;
	pLinkedToken->InitStaticData();
	return pLinkedToken;
}

// rev from GetUserProfileDirectory (dmex)
PPH_STRING PhpGetTokenFolderPath(
    _In_ HANDLE TokenHandle
    )
{
    static PH_STRINGREF servicesKeyName = PH_STRINGREF_INIT(L"Software\\Microsoft\\Windows NT\\CurrentVersion\\ProfileList\\");
    PPH_STRING profileFolderPath = NULL;
    PPH_STRING profileKeyPath = NULL;
    PPH_STRING tokenUserSid;
    PH_TOKEN_USER tokenUser;

    if (NT_SUCCESS(PhGetTokenUser(TokenHandle, &tokenUser)))
    {
        if (tokenUserSid = PhSidToStringSid(tokenUser.User.Sid))
        {
            profileKeyPath = PhConcatStringRef2(&servicesKeyName, &tokenUserSid->sr);
            PhDereferenceObject(tokenUserSid);
        }
    }

    if (profileKeyPath)
    {
        HANDLE keyHandle;

        if (NT_SUCCESS(PhOpenKey(
            &keyHandle,
            KEY_READ,
            PH_KEY_LOCAL_MACHINE,
            &profileKeyPath->sr,
            0
            )))
        {
            PPH_STRING profileImagePath;

            if (profileFolderPath = PhQueryRegistryStringZ(keyHandle, L"ProfileImagePath"))
            {
                if (profileImagePath = PhExpandEnvironmentStrings(&profileFolderPath->sr))
                {
                    PhMoveReference(&profileFolderPath, profileImagePath);
                }
            }

            NtClose(keyHandle);
        }

        PhDereferenceObject(profileKeyPath);
    }

    //ULONG profileFolderLength;
    //if (GetUserProfileDirectory)
    //{
    //    GetUserProfileDirectory(TokenHandle, NULL, &profileFolderLength);
    //    profileFolderPath = PhCreateStringEx(NULL, profileFolderLength * sizeof(WCHAR));
    //    GetUserProfileDirectory(TokenHandle, profileFolderPath->Buffer, &profileFolderLength);
    //}

    return profileFolderPath;
}

PPH_STRING PhpGetTokenRegistryPath(
    _In_ HANDLE TokenHandle
    )
{
    PPH_STRING profileRegistryPath = NULL;
    PPH_STRING tokenUserSid = NULL;
    PH_TOKEN_USER tokenUser;

    if (NT_SUCCESS(PhGetTokenUser(TokenHandle, &tokenUser)))
    {
        tokenUserSid = PhSidToStringSid(tokenUser.User.Sid);
    }

    if (tokenUserSid)
    {
        NTSTATUS status;
        HANDLE keyHandle = NULL;

        status = PhOpenKey(
            &keyHandle,
            KEY_READ,
            PH_KEY_USERS,
            &tokenUserSid->sr,
            0
            );

        if (NT_SUCCESS(status) || status == STATUS_ACCESS_DENIED)
        {
            profileRegistryPath = PhConcatStrings2(L"HKU\\", tokenUserSid->Buffer);
        }

        if (keyHandle)
            NtClose(keyHandle);

        PhDereferenceObject(tokenUserSid);
    }

    return profileRegistryPath;
}

extern "C" {
#define IS_TE
#include <apiimport.h>
}

PPH_STRING PhpGetTokenAppContainerFolderPath(
    _In_ PSID TokenAppContainerSid
    )
{
    PPH_STRING appContainerFolderPath = NULL;
    PPH_STRING appContainerSid;
    PWSTR folderPath;

    appContainerSid = PhSidToStringSid(TokenAppContainerSid);

    if (GetAppContainerFolderPath_Import())
    {
        if (SUCCEEDED(GetAppContainerFolderPath_Import()(appContainerSid->Buffer, &folderPath)))
        {
            appContainerFolderPath = PhCreateString(folderPath);
            CoTaskMemFree(folderPath);
        }
    }

    PhDereferenceObject(appContainerSid);

    return appContainerFolderPath;
}

PPH_STRING PhpGetTokenAppContainerRegistryPath(
    _In_ HANDLE TokenHandle
    )
{
    PPH_STRING appContainerRegistryPath = NULL;
    HKEY registryHandle = NULL;

    if (NT_SUCCESS(PhImpersonateToken(NtCurrentThread(), TokenHandle)))
    {
        if (GetAppContainerRegistryLocation_Import())
            GetAppContainerRegistryLocation_Import()(KEY_READ, &registryHandle);

        PhRevertImpersonationToken(NtCurrentThread());
    }

    if (registryHandle)
    {
        PhGetHandleInformation(
            NtCurrentProcess(),
            registryHandle,
            ULONG_MAX,
            NULL,
            NULL,
            NULL,
            &appContainerRegistryPath
            );

        NtClose(registryHandle);
    }

    return appContainerRegistryPath;
}

CWinToken::SAdvancedInfo CWinToken::GetAdvancedInfo()
{
	QReadLocker Locker(&m_Mutex); 

	SAdvancedInfo AdvancedInfo;

	HANDLE tokenHandle = NULL;
    if (NT_SUCCESS(CWinToken__OpenProcessToken(&tokenHandle, TOKEN_QUERY | TOKEN_QUERY_SOURCE, m)))
    {
		WCHAR tokenSourceName[TOKEN_SOURCE_LENGTH + 1] = L"Unknown";
        WCHAR tokenSourceLuid[PH_PTR_STR_LEN_1] = L"Unknown";

        TOKEN_SOURCE tokenSource;
        if (NT_SUCCESS(PhGetTokenSource(tokenHandle, &tokenSource)))
        {
            PhCopyStringZFromBytes(tokenSource.SourceName, TOKEN_SOURCE_LENGTH, tokenSourceName, RTL_NUMBER_OF(tokenSourceName), NULL);

            PhPrintPointer(tokenSourceLuid, UlongToPtr(tokenSource.SourceIdentifier.LowPart));

			AdvancedInfo.sourceName = QString::fromWCharArray(tokenSourceName);
			AdvancedInfo.sourceLuid = QString::fromWCharArray(tokenSourceLuid);
        }
    }

	if (tokenHandle == NULL && !NT_SUCCESS(CWinToken__OpenProcessToken(&tokenHandle, TOKEN_QUERY, m)))
		return AdvancedInfo;

	TOKEN_STATISTICS statistics;
    if (NT_SUCCESS(PhGetTokenStatistics(tokenHandle, &statistics)))
    {
        static_assert(CTokenInfo::eTokenTypePrimary       == TokenPrimary,           "token type primary");
        static_assert(CTokenInfo::eTokenTypeImpersonation == TokenImpersonation,     "token type impersonation");
        static_assert(CTokenInfo::eImpersonationAnonymous == SecurityAnonymous,      "impersonation anonymous");
        static_assert(CTokenInfo::eImpersonationDelegation== SecurityDelegation,     "impersonation delegation");

        AdvancedInfo.tokenType = (quint8)statistics.TokenType;

        //
        // A primary token has no impersonation level to speak of, and saying so
        // is the viewer's job - here it is simply absent.
        //
        AdvancedInfo.tokenImpersonationLevel = statistics.TokenType == TokenImpersonation
            ? (qint8)statistics.ImpersonationLevel : (qint8)CTokenInfo::eImpersonationNone;

        AdvancedInfo.tokenLuid = statistics.TokenId.LowPart;
        AdvancedInfo.authenticationLuid = statistics.AuthenticationId.LowPart;
        AdvancedInfo.tokenModifiedLuid = statistics.ModifiedId.LowPart;

        AdvancedInfo.memoryUsed = statistics.DynamicCharged - statistics.DynamicAvailable; // DynamicAvailable contains the number of bytes free.
        AdvancedInfo.memoryAvailable = statistics.DynamicCharged; // DynamicCharged contains the number of bytes allocated.
    }

	TOKEN_ORIGIN origin;
    if (NT_SUCCESS(PhGetTokenOrigin(tokenHandle, &origin)))
    {
		AdvancedInfo.tokenOriginLogonSession = origin.OriginatingLogonSession.LowPart;
    }

	PPH_STRING tokenNamedObjectPathString = NULL;
	if (NT_SUCCESS(PhGetTokenNamedObjectPath(tokenHandle, NULL, &tokenNamedObjectPathString)))
		AdvancedInfo.tokenNamedObjectPath = CastPhString(tokenNamedObjectPathString);

	PPH_STRING tokenSecurityDescriptorString = NULL;
    if(NT_SUCCESS(PhGetObjectSecurityDescriptorAsString(tokenHandle, &tokenSecurityDescriptorString)))
		AdvancedInfo.tokenSecurityDescriptor = CastPhString(tokenSecurityDescriptorString);

	PPH_STRING tokenTrustLevelSidString = NULL;
    PPH_STRING tokenTrustLevelNameString = NULL;
    if (NT_SUCCESS(PhGetTokenProcessTrustLevelRID(tokenHandle, NULL, NULL, &tokenTrustLevelNameString, &tokenTrustLevelSidString)))
    {
		AdvancedInfo.tokenTrustLevelSid = CastPhString(tokenTrustLevelSidString);
		AdvancedInfo.tokenTrustLevelName = CastPhString(tokenTrustLevelNameString);
    }

    PTOKEN_GROUPS tokenLogonGroups;
    if (NT_SUCCESS(PhQueryTokenVariableSize(tokenHandle, TokenLogonSid, (PVOID*)&tokenLogonGroups)))
    {
		AdvancedInfo.tokenLogonName = CastPhString(PhGetSidFullName(tokenLogonGroups->Groups[0].Sid, TRUE, NULL));
		AdvancedInfo.tokenLogonSid = CastPhString(PhSidToStringSid(tokenLogonGroups->Groups[0].Sid));

        PhFree(tokenLogonGroups);
    }

	AdvancedInfo.tokenProfilePath = CastPhString(PhpGetTokenFolderPath(tokenHandle));
	AdvancedInfo.tokenProfileRegistry = CastPhString(PhpGetTokenRegistryPath(tokenHandle));

	NtClose(tokenHandle);

	return AdvancedInfo;
}

CWinToken::SContainerInfo CWinToken::GetContainerInfo()
{
	QReadLocker Locker(&m_Mutex); 

	SContainerInfo ContainerInfo;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(CWinToken__OpenProcessToken(&tokenHandle, TOKEN_QUERY, m)))
		return ContainerInfo;

	APPCONTAINER_SID_TYPE appContainerSidType = InvalidAppContainerSidType;
	PTOKEN_APPCONTAINER_INFORMATION appContainerInfo;    
    if (NT_SUCCESS(PhQueryTokenVariableSize(tokenHandle, TokenAppContainerSid, (PVOID*)&appContainerInfo)))
    {
        if (appContainerInfo->TokenAppContainer)
        {
            if (RtlGetAppContainerSidType_Import())
                RtlGetAppContainerSidType_Import()(appContainerInfo->TokenAppContainer, &appContainerSidType);
			
            ContainerInfo.appContainerName = CastPhString(PhGetAppContainerName(appContainerInfo->TokenAppContainer));
            ContainerInfo.appContainerSid = CastPhString(PhSidToStringSid(appContainerInfo->TokenAppContainer));

			PSID appContainerSidParent = NULL;
            if (RtlGetAppContainerParent_Import())
                RtlGetAppContainerParent_Import()(appContainerInfo->TokenAppContainer, &appContainerSidParent);
			if (appContainerSidParent)
			{
				ContainerInfo.parentContainerName = CastPhString(PhGetAppContainerName(appContainerSidParent));
				ContainerInfo.parentContainerSid = CastPhString(PhSidToStringSid(appContainerSidParent));
				RtlFreeSid(appContainerSidParent);
			}
        }

        PhFree(appContainerInfo);
    }

    //
    // Windows numbers the parent kind 0, which is also "nothing reported", so
    // the value is shifted by one to keep the two apart.
    //
    switch (appContainerSidType)
    {
    case ParentAppContainerSidType:	ContainerInfo.appContainerSidType = CTokenInfo::eAppContainerSidParent; break;
    case ChildAppContainerSidType:	ContainerInfo.appContainerSidType = CTokenInfo::eAppContainerSidChild; break;
    default:						ContainerInfo.appContainerSidType = CTokenInfo::eAppContainerSidUnknown; break;
    }

	ULONG appContainerNumber;
    if (NT_SUCCESS(PhGetTokenAppContainerNumber(tokenHandle, &appContainerNumber)))
		ContainerInfo.appContainerNumber = appContainerNumber;

    // TO-DO: TokenIsLessPrivilegedAppContainer
    {
        static UNICODE_STRING attributeNameUs = RTL_CONSTANT_STRING(L"WIN://NOALLAPPPKG");
        PTOKEN_SECURITY_ATTRIBUTES_INFORMATION info;
        if (NT_SUCCESS(PhQueryTokenVariableSize(tokenHandle, TokenSecurityAttributes, (PVOID*)&info)))
        {
            for (ULONG i = 0; i < info->AttributeCount; i++)
            {
                PTOKEN_SECURITY_ATTRIBUTE_V1 attribute = &info->AttributeV1[i];

                if (RtlEqualUnicodeString(&attribute->Name, &attributeNameUs, FALSE))
                {
                    if (attribute->ValueType == TOKEN_SECURITY_ATTRIBUTE_TYPE_UINT64)
                    {
                        ContainerInfo.isLessPrivilegedAppContainer = true; // (*attribute->Values.pUint64 == 1);
                        break;
                    }
                }
            }

            PhFree(info);
        }
    }
      
	PPH_STRING tokenNamedObjectPathString = NULL;
    if (NT_SUCCESS(PhGetAppContainerNamedObjectPath(tokenHandle, NULL, FALSE, &tokenNamedObjectPathString)))
		ContainerInfo.tokenNamedObjectPath = CastPhString(tokenNamedObjectPathString);

    NtClose(tokenHandle);



    PPH_STRING packageFullName;
    if (packageFullName = PhGetProcessPackageFullName(m->QueryHandle))
    {
		PPH_STRING packagePath;
        if (packagePath = PhGetPackagePath(packageFullName))
			ContainerInfo.packagePath = CastPhString(packagePath);
        ContainerInfo.packageFullName = CastPhString(packageFullName);
    }



	if (!NT_SUCCESS(CWinToken__OpenProcessToken(&tokenHandle, TOKEN_QUERY | TOKEN_IMPERSONATE | TOKEN_DUPLICATE, m)))
		return ContainerInfo;

	//PTOKEN_APPCONTAINER_INFORMATION appContainerInfo;
	if (NT_SUCCESS(PhQueryTokenVariableSize(tokenHandle, TokenAppContainerSid, (PVOID*)&appContainerInfo)))
	{
		if (appContainerInfo->TokenAppContainer)
			ContainerInfo.appContainerFolderPath = CastPhString(PhpGetTokenAppContainerFolderPath(appContainerInfo->TokenAppContainer));
		PhFree(appContainerInfo);
	}

	ContainerInfo.appContainerRegistryPath = CastPhString(PhpGetTokenAppContainerRegistryPath(tokenHandle));

	NtClose(tokenHandle);

	return ContainerInfo;
}

QMap<QByteArray, CWinToken::SCapability> CWinToken::GetCapabilities()
{
	QReadLocker Locker(&m_Mutex);

	QMap<QByteArray, SCapability> Capabilities;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(CWinToken__OpenProcessToken(&tokenHandle, TOKEN_QUERY, m)))
		return Capabilities;

	PTOKEN_GROUPS capabilities;
	if (NT_SUCCESS(PhQueryTokenVariableSize(tokenHandle, TokenCapabilities, (PVOID*)&capabilities)))
	{
		for (int i = 0; i < capabilities->GroupCount; i++)
		{
			QByteArray sid =  QByteArray((char*)capabilities->Groups[i].Sid, RtlLengthSid(capabilities->Groups[i].Sid));

			SCapability &Capability = Capabilities[sid];
			Capability.SidString = CastPhString(PhSidToStringSid(capabilities->Groups[i].Sid));
			
			Capability.FullName = CastPhString(PhGetSidFullName(capabilities->Groups[i].Sid, TRUE, NULL)); // note this may be slow

			Capability.Capability = CastPhString(PhGetCapabilitySidName(capabilities->Groups[i].Sid));
			
			ulong subAuthoritiesCount = *RtlSubAuthorityCountSid(capabilities->Groups[i].Sid);
			ulong subAuthority = *RtlSubAuthoritySid(capabilities->Groups[i].Sid, 0);

			// RtlIdentifierAuthoritySid(capabilities->Groups[i].Sid) == (BYTE[])SECURITY_APP_PACKAGE_AUTHORITY
			if (subAuthority == SECURITY_CAPABILITY_BASE_RID)
			{
				if (subAuthoritiesCount == SECURITY_APP_PACKAGE_RID_COUNT)
				{
					PTOKEN_APPCONTAINER_INFORMATION appContainerInfo;

					//if (*RtlSubAuthoritySid(capabilities->Groups[i].Sid, 1) == SECURITY_CAPABILITY_APP_RID)
					//    continue;

					if (NT_SUCCESS(PhQueryTokenVariableSize(tokenHandle, TokenAppContainerSid, (PVOID*)&appContainerInfo)))
					{
						if (appContainerInfo->TokenAppContainer)
						{
							if (PhIsPackageCapabilitySid(appContainerInfo->TokenAppContainer, capabilities->Groups[i].Sid))
							{
								if(m->QueryHandle)
									Capability.Package = CastPhString(PhGetProcessPackageFullName(m->QueryHandle));
							}
						}

						PhFree(appContainerInfo);
					}
				}
				else if (subAuthoritiesCount == SECURITY_CAPABILITY_RID_COUNT)
				{
					union
					{
						GUID Guid;
						struct
						{
							ULONG Data1;
							ULONG Data2;
							ULONG Data3;
							ULONG Data4;
						};
					} capabilityGuid;

					capabilityGuid.Data1 = *RtlSubAuthoritySid(capabilities->Groups[i].Sid, 1);
					capabilityGuid.Data2 = *RtlSubAuthoritySid(capabilities->Groups[i].Sid, 2);
					capabilityGuid.Data3 = *RtlSubAuthoritySid(capabilities->Groups[i].Sid, 3);
					capabilityGuid.Data4 = *RtlSubAuthoritySid(capabilities->Groups[i].Sid, 4);

					PPH_STRING name = PhFormatGuid(&capabilityGuid.Guid);
					if (name)
					{
						Capability.Capability = CastPhString(PhGetCapabilityGuidName(name));
						Capability.Guid = CastPhString(name);
					}
				}
			}
		}

		PhFree(capabilities);
	}

	NtClose(tokenHandle);

	return Capabilities;
}

QVariant ClaimSecurityAttribute2Variant(PCLAIM_SECURITY_ATTRIBUTE_V1 Attribute, ULONG ValueIndex)
{
    PH_FORMAT format;

    switch (Attribute->ValueType)
    {
    case CLAIM_SECURITY_ATTRIBUTE_TYPE_INT64:
		return Attribute->Values.pInt64[ValueIndex];
    case CLAIM_SECURITY_ATTRIBUTE_TYPE_UINT64:
		return Attribute->Values.pUint64[ValueIndex];
    case CLAIM_SECURITY_ATTRIBUTE_TYPE_STRING:
		return QString::fromWCharArray(Attribute->Values.ppString[ValueIndex]);
    case CLAIM_SECURITY_ATTRIBUTE_TYPE_FQBN:
		//
		// A fully-qualified binary name is a version and a name; they are handed
		// over as the two values they are and put together by the viewer.
		//
		return QVariantList() << (quint64)Attribute->Values.pFqbn[ValueIndex].Version
							  << QString::fromWCharArray(Attribute->Values.pFqbn[ValueIndex].Name);
    case CLAIM_SECURITY_ATTRIBUTE_TYPE_SID:
        {
            if (RtlValidSid(Attribute->Values.pOctetString[ValueIndex].pValue))
            {
                PPH_STRING name = PhGetSidFullName(Attribute->Values.pOctetString[ValueIndex].pValue, TRUE, NULL);
                if (name)
                    return CastPhString(name);

                name = PhSidToStringSid(Attribute->Values.pOctetString[ValueIndex].pValue);
                if (name)
                    return CastPhString(name);
            }
        }
        return QVariant();
    case CLAIM_SECURITY_ATTRIBUTE_TYPE_BOOLEAN:
        return Attribute->Values.pInt64[ValueIndex] != 0;
    case CLAIM_SECURITY_ATTRIBUTE_TYPE_OCTET_STRING:
        return QByteArray((char*)Attribute->Values.pOctetString->pValue, Attribute->Values.pOctetString->ValueLength).toHex();
    default:
        return QVariant();
    }
}

QMap<QString, CWinToken::SAttribute> CWinToken::GetClaims(bool DeviceClaims)
{
	QReadLocker Locker(&m_Mutex); 

	QMap<QString, SAttribute> Claims;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(CWinToken__OpenProcessToken(&tokenHandle, TOKEN_QUERY, m)))
		return Claims;


	PCLAIM_SECURITY_ATTRIBUTES_INFORMATION info;
	if (NT_SUCCESS(PhQueryTokenVariableSize(tokenHandle, DeviceClaims ? TokenDeviceClaimAttributes : TokenUserClaimAttributes, (PVOID*)&info)))
	{
		for (int i = 0; i < info->AttributeCount; i++)
		{
			PCLAIM_SECURITY_ATTRIBUTE_V1 attribute = &info->Attribute.pAttributeV1[i];
			SAttribute &Attribute = Claims[QString::fromWCharArray(attribute->Name)];
			Attribute.Type = attribute->ValueType;
			Attribute.Flags = attribute->Flags;
            for (int j = 0; j < attribute->ValueCount; j++)
				Attribute.Values.append(ClaimSecurityAttribute2Variant(attribute, j));
		}

		PhFree(info);
	}

	NtClose(tokenHandle);

	return Claims;
}





QVariant TokenSecurityAttribute2Variant(PTOKEN_SECURITY_ATTRIBUTE_V1 Attribute, ULONG ValueIndex)
{
    switch (Attribute->ValueType)
    {
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_INT64:
		return Attribute->Values.Int64[ValueIndex];
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_UINT64:
		return Attribute->Values.Uint64[ValueIndex];
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_STRING:
		return QString::fromWCharArray(Attribute->Values.String[ValueIndex].Buffer, Attribute->Values.String[ValueIndex].Length / sizeof(wchar_t));
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_FQBN:
		return QVariantList() << (quint64)Attribute->Values.Fqbn[ValueIndex].Version
							  << QString::fromWCharArray(Attribute->Values.Fqbn[ValueIndex].Name.Buffer, Attribute->Values.Fqbn[ValueIndex].Name.Length / sizeof(WCHAR));
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_SID:
        {
            if (RtlValidSid(Attribute->Values.OctetString[ValueIndex].Value))
            {
                PPH_STRING name = PhGetSidFullName(Attribute->Values.OctetString[ValueIndex].Value, TRUE, NULL); // note this may be slow
                if (name)
                    return CastPhString(name);

                name = PhSidToStringSid(Attribute->Values.OctetString[ValueIndex].Value);
                if (name)
                    return CastPhString(name);
            }
        }
        return QVariant();
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_BOOLEAN:
		return  Attribute->Values.Int64[ValueIndex] != 0;
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_OCTET_STRING:
        return QByteArray((char*)Attribute->Values.OctetString->Value, Attribute->Values.OctetString->ValueLength).toHex();
    default:
        return QVariant();
    }
}

QMap<QString, CWinToken::SAttribute> CWinToken::GetAttributes()
{
	QReadLocker Locker(&m_Mutex); 

	QMap<QString, SAttribute> Attributes;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(CWinToken__OpenProcessToken(&tokenHandle, TOKEN_QUERY, m)))
		return Attributes;

	PTOKEN_SECURITY_ATTRIBUTES_INFORMATION info;
    if (NT_SUCCESS(PhQueryTokenVariableSize(tokenHandle, TokenSecurityAttributes, (PVOID*)&info)))
    {
        for (int i = 0; i < info->AttributeCount; i++)
        {
            PTOKEN_SECURITY_ATTRIBUTE_V1 attribute = &info->AttributeV1[i];
			SAttribute &Attribute = Attributes[QString::fromWCharArray(attribute->Name.Buffer, attribute->Name.Length / sizeof(wchar_t))];
			Attribute.Type = attribute->ValueType;
			Attribute.Flags = attribute->Flags;
            for (int j = 0; j < attribute->ValueCount; j++)
				Attribute.Values.append(TokenSecurityAttribute2Variant(attribute, j));
        }

        PhFree(info);
    }

	NtClose(tokenHandle);

	return Attributes;
}