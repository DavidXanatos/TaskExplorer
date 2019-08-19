/*
 * Process Hacker -
 *   qt wrapper and support functions based on tokprop.c
 *
 * Copyright (C) 2010-2012 wj32
 * Copyright (C) 2017-2019 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains Process Hacker code.
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

#include "stdafx.h"
#include "WinToken.h"
#include "../../GUI/TaskExplorer.h"
#include "ProcessHacker.h"
#include "WindowsAPI.h"

struct SWinToken
{
	SWinToken()
	{
		QueryHandle = NULL;
		Handle = NULL;
		Type = CWinToken::eProcess;
	}

	HANDLE QueryHandle;
	HANDLE Handle;
	CWinToken::EQueryType Type;
};

CWinToken::CWinToken(QObject *parent)
	:CAbstractInfo(parent)
{
	m_IsAppContainer = false;
	m_SessionId = 0;
	m_Elevated = false;
	m_ElevationType = 0;
	m_IntegrityLevel = -1;
	m_Virtualization = 0;

	m = new SWinToken();
}

CWinToken::~CWinToken()
{
	if (m->Type != eProcess)
		NtClose(m->QueryHandle);
	delete m;
}

NTSTATUS NTAPI PhpOpenProcessTokenForPage(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
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
	else
	{
		HANDLE processHandle;

		// Note: we are using the query handle of the process instance instead of pid and re opening it
		processHandle = m->QueryHandle;
		//if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION, (HANDLE)Context)))
		//    return status;

		// HACK: Add extra access_masks for querying default token. (dmex)
		if (!NT_SUCCESS(status = PhOpenProcessToken(processHandle, DesiredAccess | TOKEN_READ | TOKEN_ADJUST_DEFAULT | READ_CONTROL, Handle)))
		{
			status = PhOpenProcessToken(processHandle, DesiredAccess, Handle);
		}

		//NtClose(processHandle);
	}
	return status;
}


QReadWriteLock g_Sid2NameMutex;
QMap<QByteArray, QString> g_Sid2NameCache;

QPair<QByteArray, QString> GetSidFullNameAndCache(const QByteArray& Sid)
{
	QWriteLocker WriteLocker(&g_Sid2NameMutex);
	PPH_STRING fullName = PhGetSidFullName((PSID)Sid.data(), TRUE, NULL);
	QString FullName = CastPhString(fullName);
	g_Sid2NameCache.insert(Sid, FullName);
	return qMakePair(Sid, FullName);
}

QString GetSidFullNameCached(const QByteArray& Sid, QObject* pTarget)
{
	QReadLocker ReadLocker(&g_Sid2NameMutex);
	QMap<QByteArray, QString>::iterator I = g_Sid2NameCache.find(Sid);
	if (I != g_Sid2NameCache.end())
		return I.value();
	ReadLocker.unlock();

	// From PhpProcessQueryStage1 
	// Note: We delay resolving the SID name because the local LSA cache might still be
	// initializing for users on domain networks with slow links (e.g. VPNs). This can block
	// for a very long time depending on server/network conditions. (dmex)

	if (pTarget) // so if we want a quick result we skip this step
	{
		// ToDo: we should use a dedicated worker thread as Qt's Future system will start many paralell requests
		// so we may end up with multiple requests for teh same SID, and as we have a limited worker pool 
		// we may block other things using QFutureWatcher like resolving file infos for CWinModule

		QFutureWatcher<QPair<QByteArray, QString> >* pWatcher = new QFutureWatcher<QPair<QByteArray, QString> >(pTarget); // Note: the job will be canceled if the file will be deleted :D
		QObject::connect(pWatcher, SIGNAL(resultReadyAt(int)), pTarget, SLOT(OnSidResolved(int)));
		QObject::connect(pWatcher, SIGNAL(finished()), pWatcher, SLOT(deleteLater()));
		pWatcher->setFuture(QtConcurrent::run(GetSidFullNameAndCache, Sid));

		return CWinToken::tr("Resolving...");
	}

	return CWinToken::tr("Not resolved...");
}

void CWinToken::OnSidResolved(int Index)
{
	QFutureWatcher<QPair<QByteArray, QString> >* pWatcher = (QFutureWatcher<QPair<QByteArray, QString> >*)sender();

	if (!pWatcher)
		return;

	QPair<QByteArray, QString> Result = pWatcher->resultAt(Index);

	QWriteLocker Locker(&m_Mutex);
	if(Result.first == m_UserSid)
		m_UserName = Result.second;
	else if(Result.first == m_OwnerSid)
		m_OwnerName = Result.second;
	else if(Result.first == m_GroupSid)
		m_GroupName = Result.second;
	else
	{
		QMap<QByteArray, SGroup>::iterator I = m_Groups.find(Result.first);
		if (I != m_Groups.end())
			I.value().Name = Result.second;
	}
}

CWinToken* CWinToken::NewSystemToken()
{
	CWinToken* pToken = new CWinToken();
	pToken->m_UserSid = QByteArray((char*)&PhSeLocalSystemSid, RtlLengthSid(&PhSeLocalSystemSid));
	pToken->m_UserName = GetSidFullNameCached(pToken->m_UserSid, pToken);
	return pToken;
}

CWinToken* CWinToken::TokenFromHandle(quint64 ProcessId, quint64 HandleId)
{
	HANDLE processHandle;
    if (!NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_DUP_HANDLE, (HANDLE)ProcessId)))
        return NULL;

	CWinToken* pToken = new CWinToken();
	pToken->m->Handle = (HANDLE)HandleId;
	pToken->InitStaticData(processHandle, eHandle);
	return pToken;
}

bool CWinToken::InitStaticData(void* QueryHandle, EQueryType Type)
{
	QWriteLocker Locker(&m_Mutex);

	m->Type = Type;
	m->QueryHandle = QueryHandle;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_QUERY, m)))
		return false;

	BOOLEAN tokenIsAppContainer = FALSE;
    PhGetTokenIsAppContainer(tokenHandle, &tokenIsAppContainer);
	m_IsAppContainer = tokenIsAppContainer != FALSE;

	PTOKEN_USER tokenUser;
    if (NT_SUCCESS(PhGetTokenUser(tokenHandle, &tokenUser)))
    {
		m_UserSid = QByteArray((char*)tokenUser->User.Sid, RtlLengthSid(tokenUser->User.Sid));

        if (!tokenIsAppContainer) // HACK (dmex)
        {
			m_UserName = GetSidFullNameCached(m_UserSid, this);
        }

		PPH_STRING stringUserSid;
        if (stringUserSid = PhSidToStringSid(tokenUser->User.Sid))
			m_SidString = CastPhString(stringUserSid);

        PhFree(tokenUser);
    }

	PTOKEN_OWNER tokenOwner;
    if (NT_SUCCESS(PhGetTokenOwner(tokenHandle, &tokenOwner)))
    {
		m_OwnerSid = QByteArray((char*)tokenOwner->Owner, RtlLengthSid(tokenOwner->Owner));

		m_OwnerName = GetSidFullNameCached(m_OwnerSid, this);

        PhFree(tokenOwner);
    }

	PTOKEN_PRIMARY_GROUP tokenPrimaryGroup;
    if (NT_SUCCESS(PhGetTokenPrimaryGroup(tokenHandle, &tokenPrimaryGroup)))
    {
		m_GroupSid = QByteArray((char*)tokenPrimaryGroup->PrimaryGroup, RtlLengthSid(tokenPrimaryGroup->PrimaryGroup));

		m_GroupName = GetSidFullNameCached(m_GroupSid, this);

        PhFree(tokenPrimaryGroup);
    }

    if (WINDOWS_HAS_IMMERSIVE)
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
					m_UserName = CastPhString(appContainerName) + tr(" (APP_CONTAINER)");
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

bool CWinToken::UpdateDynamicData()
{
	QWriteLocker Locker(&m_Mutex); 

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_QUERY, m)))
		return false;

	ULONG sessionId;
	if (NT_SUCCESS(PhGetTokenSessionId(tokenHandle, &sessionId)))
		m_SessionId = sessionId;

	TOKEN_ELEVATION_TYPE elevationType;
	if (NT_SUCCESS(PhGetTokenElevationType(tokenHandle, &elevationType)))
		m_ElevationType = elevationType;

	BOOLEAN elevated = TRUE;
	if (NT_SUCCESS(PhGetTokenIsElevated(tokenHandle, &elevated)))
		m_Elevated = elevated;


	// Integrity
	MANDATORY_LEVEL_RID integrityLevel;
	PWSTR integrityString;  // this will point to static stings so dont free it
	if (NT_SUCCESS(PhGetTokenIntegrityLevelRID(tokenHandle, &integrityLevel, &integrityString)))
	{
		if (m_IntegrityLevel != integrityLevel)
		{
			m_IntegrityLevel = integrityLevel;
			m_IntegrityString = QString::fromWCharArray(integrityString);
		}
	}

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

	NtClose(tokenHandle);

	return true;
}

bool CWinToken::UpdateExtendedData()
{
	QWriteLocker Locker(&m_Mutex); 

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_QUERY, m)))
		return false;

    //PhpUpdateTokenDangerousFlags
	// ToDo: xxx
    /*TOKEN_MANDATORY_POLICY mandatoryPolicy;
    if (NT_SUCCESS(PhGetTokenMandatoryPolicy(tokenHandle, &mandatoryPolicy)))
    {
        // The disabled no-write-up policy is considered to be dangerous (diversenok)
        if ((mandatoryPolicy.Policy & TOKEN_MANDATORY_POLICY_NO_WRITE_UP) == 0)
        {
            // PH_PROCESS_TOKEN_FLAG_NO_WRITE_UP, "No-Write-Up Policy", "Prevents the process from modifying objects with a higher integrity"
        }
    }

    BOOLEAN isSandboxInert;
    if (NT_SUCCESS(PhGetTokenIsSandBoxInert(tokenHandle, &isSandboxInert)))
    {
        // The presence of SandboxInert flag is considered dangerous (diversenok)
        if (isSandboxInert)
        {
            // PH_PROCESS_TOKEN_FLAG_SANDBOX_INERT, "Sandbox Inert", "Ignore AppLocker rules and Software Restriction Policies"
        }
    }

	BOOLEAN isUIAccess;
    if (NT_SUCCESS(PhGetTokenIsUIAccessEnabled(tokenHandle, &isUIAccess)))
    {
        // The presence of UIAccess flag is considered dangerous (diversenok)
        if (isUIAccess)
        {
            // PH_PROCESS_TOKEN_FLAG_UIACCESS, "UIAccess", "Ignore User Interface Privilege Isolation"
        }
    }*/
	//

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
			//Group.SidString = CastPhString(PhSidToStringSid(Groups->Groups[i].Sid));
			Group.Attributes = Groups->Groups[i].Attributes;
			Group.Name = GetSidFullNameCached(Sid, this);
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
			//Group.SidString = CastPhString(PhSidToStringSid(RestrictedSIDs->Groups[i].Sid));
			Group.Attributes = RestrictedSIDs->Groups[i].Attributes;
			Group.Name = GetSidFullNameCached(Sid, this);
		}

		PhFree(RestrictedSIDs);
    }
	//

    //PhpUpdateTokenPrivileges
	PTOKEN_PRIVILEGES privileges = NULL;
	if (NT_SUCCESS(PhGetTokenPrivileges(tokenHandle, &privileges)))
	{
		for (ULONG i = 0; i < privileges->PrivilegeCount; i++)
		{
			PPH_STRING privilegeName;
			if (PhLookupPrivilegeName(&privileges->Privileges[i].Luid, &privilegeName))
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


QString CWinToken::GetElevationString() const
{
	QReadLocker Locker(&m_Mutex); 
	switch (m_ElevationType)
    {
    case TokenElevationTypeFull:	return tr("Yes");
    case TokenElevationTypeLimited:	return tr("No");
    default:						return tr("N/A");
    }
}

QString CWinToken::GetVirtualizationString() const 
{
	QReadLocker Locker(&m_Mutex);
	if ((m_Virtualization & VIRTUALIZATION_ENABLED) != 0)
		return tr("Virtualized");
	if ((m_Virtualization & VIRTUALIZATION_ALLOWED) != 0)
		return tr("Allowed");
	return tr("Not allowed");
}

QString CWinToken::GetGroupStatusString(quint32 Attributes, bool Restricted)
{
	QString Str = "";
    if ((Attributes & (SE_GROUP_INTEGRITY | SE_GROUP_INTEGRITY_ENABLED)) != 0)
    {
        if ((Attributes & SE_GROUP_ENABLED) != 0)
            Str = tr("Enabled (as a group)");
    }
    else
    {
        if ((Attributes & SE_GROUP_ENABLED) != 0)
        {
            if ((Attributes & SE_GROUP_ENABLED_BY_DEFAULT) != 0)
                Str = tr("Enabled");
            else
                Str = tr("Enabled (modified)");
        }
        else
        {
            if ((Attributes & SE_GROUP_ENABLED_BY_DEFAULT) != 0)
                Str = tr("Disabled (modified)");
            else
                Str = tr("Disabled");
        }
    }

    if (Restricted && !Str.isEmpty())
		Str += tr(" (restricted)");

    return Str;
}

PH_ACCESS_ENTRY GroupDescriptionEntries[6] =
{
    { NULL, SE_GROUP_INTEGRITY | SE_GROUP_INTEGRITY_ENABLED, FALSE, FALSE, L"Integrity" },
    { NULL, SE_GROUP_LOGON_ID, FALSE, FALSE, L"Logon Id" },
    { NULL, SE_GROUP_OWNER, FALSE, FALSE, L"Owner" },
    { NULL, SE_GROUP_MANDATORY, FALSE, FALSE, L"Mandatory" },
    { NULL, SE_GROUP_USE_FOR_DENY_ONLY, FALSE, FALSE, L"Use for deny only" },
    { NULL, SE_GROUP_RESOURCE, FALSE, FALSE, L"Resource" }
};

QString CWinToken::GetGroupDescription(quint32 Attributes)
{
	return CastPhString(PhGetAccessString(Attributes, GroupDescriptionEntries, RTL_NUMBER_OF(GroupDescriptionEntries)));
}

bool CWinToken::IsGroupEnabled(quint32 Attributes)
{
	return ((Attributes & SE_GROUP_ENABLED) != 0);
}
	
bool CWinToken::IsGroupModified(quint32 Attributes)
{
	if ((Attributes & (SE_GROUP_INTEGRITY | SE_GROUP_INTEGRITY_ENABLED)) != 0 && (Attributes & SE_GROUP_ENABLED) != 0)
		return false;
	return ((Attributes & SE_GROUP_ENABLED) != 0) != ((Attributes & SE_GROUP_ENABLED_BY_DEFAULT) != 0);
}

QString CWinToken::GetPrivilegeAttributesString(quint32 Attributes)
{
    if ((Attributes & SE_PRIVILEGE_ENABLED) != 0)
    {
        if ((Attributes & SE_PRIVILEGE_ENABLED_BY_DEFAULT) != 0)
            return tr("Enabled");
        else
            return tr("Enabled (modified)");
    }
    else
    {
        if ((Attributes & SE_PRIVILEGE_ENABLED_BY_DEFAULT) != 0)
            return tr("Disabled (modified)");
        else
            return tr("Disabled");
    }
}

bool CWinToken::IsPrivilegeEnabled(quint32 Attributes)
{
	return ((Attributes & SE_PRIVILEGE_ENABLED) != 0);
}

bool CWinToken::IsPrivilegeModified(quint32 Attributes)
{
	return ((Attributes & SE_PRIVILEGE_ENABLED) != 0) != ((Attributes & SE_PRIVILEGE_ENABLED_BY_DEFAULT) != 0);
}

/*{
                    NTSTATUS status;
                    PPHP_TOKEN_PAGE_LISTVIEW_ITEM *listViewItems;
                    ULONG numberOfItems;
                    HANDLE tokenHandle;

                    PhGetSelectedListViewItemParams(
                        tokenPageContext->ListViewHandle,
                        &listViewItems,
                        &numberOfItems
                        );

                    if (numberOfItems != 1)
                    {
                        PhFree(listViewItems);
                        break;
                    }

                    if ((listViewItems[0]->ItemCategory != PH_PROCESS_TOKEN_CATEGORY_DANGEROUS_FLAGS) ||
                        (listViewItems[0]->ItemFlag != PH_PROCESS_TOKEN_FLAG_UIACCESS))
                    {
                        PhFree(listViewItems);
                        break;
                    }

                    if (!PhShowConfirmMessage(
                        hwndDlg,
                        L"remove",
                        L"the UIAccess flag",
                        L"Removing this flag may reduce the functionality of the process "
                        L"provided it is an accessibility application.",
                        FALSE
                        ))
                    {
                        PhFree(listViewItems);
                        break;
                    }

                    status = tokenPageContext->OpenObject(
                        &tokenHandle,
                        TOKEN_ADJUST_DEFAULT,
                        tokenPageContext->Context // ProcessId
                        );

                    if (NT_SUCCESS(status))
                    {
                        ExtendedListView_SetRedraw(tokenPageContext->ListViewHandle, FALSE);

                        status = PhSetTokenUIAccessEnabled(tokenHandle, FALSE);

                        if (NT_SUCCESS(status))
                        {
                            INT lvItemIndex = PhFindListViewItemByParam(
                                tokenPageContext->ListViewHandle,
                                -1,
                                listViewItems[0]
                                );

                            if (lvItemIndex != -1)
                            {
                                ListView_DeleteItem(tokenPageContext->ListViewHandle, lvItemIndex);
                            }
                        }
                        else
                        {
                            PhShowStatus(hwndDlg, L"Unable to disable UIAccess flag.", status, 0);
                        }

                        ExtendedListView_SortItems(tokenPageContext->ListViewHandle);
                        ExtendedListView_SetRedraw(tokenPageContext->ListViewHandle, TRUE);

                        NtClose(tokenHandle);
                    }
                    else
                    {
                        PhShowStatus(hwndDlg, L"Unable to open the token.", status, 0);
                    }

                    PhFree(listViewItems);
                }*/

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
		return ERR(tr("Failed to set process virtualization"), status);
    }

    return OK;
}

STATUS CWinToken::SetIntegrityLevel(quint32 IntegrityLevel) const
{
	QWriteLocker Locker(&m_Mutex);

	NTSTATUS status;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(status = PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_QUERY | TOKEN_ADJUST_DEFAULT, m)))
		return ERR(tr("Could not open token."), status);

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
		return ERR(tr("failed to Set Token Information"), status);
	}

	return OK;
}

STATUS CWinToken::PrivilegeAction(const SPrivilege& Privilege, EAction Action, bool bForce)
{
	if(!bForce && Action == eRemove)
		return ERR(tr("Removing privileges may reduce the functionality of the process, and is permanent for the lifetime of the process."), ERROR_CONFIRM);

	QWriteLocker Locker(&m_Mutex);

	NTSTATUS status;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(status = PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_ADJUST_PRIVILEGES, m)))
		return ERR(tr("Could not open token."), status);

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

	BOOLEAN ok = PhSetTokenPrivilege(tokenHandle, NULL, &luid, newAttributes);

    NtClose(tokenHandle);

	if (!ok)
		return ERR(tr("Unable to Set Token Privilege"), -1);

	return OK;
}

STATUS CWinToken::GroupAction(const SGroup& Group, EAction Action)
{
	QWriteLocker Locker(&m_Mutex);

	NTSTATUS status;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(status = PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_ADJUST_GROUPS, m)))
		return ERR(tr("Could not open token."), status);

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
		return ERR(tr("Unable to Set Token Groups"), status);

	return OK;
}

void CWinToken::OpenPermissions(bool bDefaultToken)
{
	QWriteLocker Locker(&m_Mutex); 

    PhEditSecurity(NULL, bDefaultToken ? L"Default Token" : L"Token", bDefaultToken ? L"TokenDefault" : L"Token", PhpOpenProcessTokenForPage, NULL, m);  // todo: fixme m may get deleted!!!
}

QSharedPointer<CWinToken> CWinToken::GetLinkedToken()
{
	QReadLocker Locker(&m_Mutex); 

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_QUERY, m)))
		return QSharedPointer<CWinToken>();

	QSharedPointer<CWinToken> pLinkedToken = QSharedPointer<CWinToken>(new CWinToken());

	pLinkedToken->InitStaticData(tokenHandle, eLinked);

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
    PTOKEN_USER tokenUser;

    if (NT_SUCCESS(PhGetTokenUser(TokenHandle, &tokenUser)))
    {
        if (tokenUserSid = PhSidToStringSid(tokenUser->User.Sid))
        {
            profileKeyPath = PhConcatStringRef2(&servicesKeyName, &tokenUserSid->sr);
            PhDereferenceObject(tokenUserSid);
        }

        PhFree(tokenUser);
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

            if (profileFolderPath = PhQueryRegistryString(keyHandle, L"ProfileImagePath"))
            {
                if (profileImagePath = PhExpandEnvironmentStrings(&profileFolderPath->sr))
                {
                    PhMoveReference((PVOID*)&profileFolderPath, profileImagePath);
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
    PTOKEN_USER tokenUser;

    if (NT_SUCCESS(PhGetTokenUser(TokenHandle, &tokenUser)))
    {
        tokenUserSid = PhSidToStringSid(tokenUser->User.Sid);
        PhFree(tokenUser);
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
    if (NT_SUCCESS(PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_QUERY | TOKEN_QUERY_SOURCE, m)))
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

	if (tokenHandle == NULL && !NT_SUCCESS(PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_QUERY, m)))
		return AdvancedInfo;

	TOKEN_STATISTICS statistics;
    if (NT_SUCCESS(PhGetTokenStatistics(tokenHandle, &statistics)))
    {
        switch (statistics.TokenType)
        {
        case TokenPrimary:
            AdvancedInfo.tokenType = tr("Primary");
            break;
        case TokenImpersonation:
            AdvancedInfo.tokenType = tr("Impersonation");
            break;
        }

        if (statistics.TokenType == TokenImpersonation)
        {
            switch (statistics.ImpersonationLevel)
            {
            case SecurityAnonymous:
                AdvancedInfo.tokenImpersonationLevel = tr("Anonymous");
                break;
            case SecurityIdentification:
                AdvancedInfo.tokenImpersonationLevel = tr("Identification");
                break;
            case SecurityImpersonation:
                AdvancedInfo.tokenImpersonationLevel = tr("Impersonation");
                break;
            case SecurityDelegation:
                AdvancedInfo.tokenImpersonationLevel = tr("Delegation");
                break;
            }
        }
        else
        {
            AdvancedInfo.tokenImpersonationLevel = tr("N/A");
        }

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
    if(NT_SUCCESS(PhGetTokenSecurityDescriptorAsString(tokenHandle, &tokenSecurityDescriptorString)))
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
	if (!NT_SUCCESS(PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_QUERY, m)))
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

    switch (appContainerSidType)
    {
    case ChildAppContainerSidType:	ContainerInfo.appContainerSidType = tr("Child"); break;
    case ParentAppContainerSidType:	ContainerInfo.appContainerSidType = tr("Parent"); break;
    default:						ContainerInfo.appContainerSidType = tr("Unknown");
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
                PTOKEN_SECURITY_ATTRIBUTE_V1 attribute = &info->Attribute.pAttributeV1[i];

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



	if (!NT_SUCCESS(PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_QUERY | TOKEN_IMPERSONATE | TOKEN_DUPLICATE, m)))
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
	if (!NT_SUCCESS(PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_QUERY, m)))
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
		return CWinToken::tr("Version %1: %2").arg(Attribute->Values.pFqbn[ValueIndex].Version).arg(QString::fromWCharArray(Attribute->Values.pFqbn[ValueIndex].Name));
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
        return CWinToken::tr("(Invalid SID)");
    case CLAIM_SECURITY_ATTRIBUTE_TYPE_BOOLEAN:
        return Attribute->Values.pInt64[ValueIndex] != 0;
    case CLAIM_SECURITY_ATTRIBUTE_TYPE_OCTET_STRING:
        return QByteArray((char*)Attribute->Values.pOctetString->pValue, Attribute->Values.pOctetString->ValueLength).toHex();
    default:
        return CWinToken::tr("(Unknown)");
    }
}

QMap<QString, CWinToken::SAttribute> CWinToken::GetClaims(bool DeviceClaims)
{
	QReadLocker Locker(&m_Mutex); 

	QMap<QString, SAttribute> Claims;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_QUERY, m)))
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

QString CWinToken::GetSecurityAttributeTypeString(quint16 Type)
{
    // These types are shared between CLAIM_* and TOKEN_* security attributes.
    switch (Type)
    {
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_INVALID:
        return tr("Invalid");
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_INT64:
        return tr("Int64");
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_UINT64:
        return tr("UInt64");
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_STRING:
        return tr("String");
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_FQBN:
        return tr("FQBN");
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_SID:
        return tr("SID");
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_BOOLEAN:
        return tr("Boolean");
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_OCTET_STRING:
        return tr("Octet string");
    default:
        return tr("(Unknown)");
    }
}

QString CWinToken::GetSecurityAttributeFlagsString(quint32 Flags)
{
	QString Str = "";

    // These flags are shared between CLAIM_* and TOKEN_* security attributes.

    if (Flags & TOKEN_SECURITY_ATTRIBUTE_MANDATORY)
        Str = tr("Mandatory, ");
    if (Flags & TOKEN_SECURITY_ATTRIBUTE_DISABLED)
        Str = tr("Disabled, ");
    if (Flags & TOKEN_SECURITY_ATTRIBUTE_DISABLED_BY_DEFAULT)
        Str = tr("Default disabled, ");
    if (Flags & TOKEN_SECURITY_ATTRIBUTE_USE_FOR_DENY_ONLY)
        Str = tr("Use for deny only, ");
    if (Flags & TOKEN_SECURITY_ATTRIBUTE_VALUE_CASE_SENSITIVE)
        Str = tr("Case-sensitive, ");
    if (Flags & TOKEN_SECURITY_ATTRIBUTE_NON_INHERITABLE)
        Str = tr("Non-inheritable, ");
    if (Flags & TOKEN_SECURITY_ATTRIBUTE_COMPARE_IGNORE)
        Str = tr("Compare-ignore, ");

	if (Str.length() != 0)
		Str.remove(Str.length() - 2, 2);
    else
        Str += tr("(None)");

	return Str;
}

QVariant TokenSecurityAttribute2Variant(PTOKEN_SECURITY_ATTRIBUTE_V1 Attribute, ULONG ValueIndex)
{
    switch (Attribute->ValueType)
    {
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_INT64:
		return Attribute->Values.pInt64[ValueIndex];
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_UINT64:
		return Attribute->Values.pUint64[ValueIndex];
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_STRING:
		return QString::fromWCharArray(Attribute->Values.pString[ValueIndex].Buffer, Attribute->Values.pString[ValueIndex].Length / sizeof(wchar_t));
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_FQBN:
		return CWinToken::tr("Version %1: %2").arg(Attribute->Values.pFqbn[ValueIndex].Version).arg(QString::fromWCharArray(Attribute->Values.pFqbn[ValueIndex].Name.Buffer, Attribute->Values.pFqbn[ValueIndex].Name.Length / sizeof(WCHAR)));
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_SID:
        {
            if (RtlValidSid(Attribute->Values.pOctetString[ValueIndex].pValue))
            {
                PPH_STRING name = PhGetSidFullName(Attribute->Values.pOctetString[ValueIndex].pValue, TRUE, NULL); // note this may be slow
                if (name)
                    return CastPhString(name);

                name = PhSidToStringSid(Attribute->Values.pOctetString[ValueIndex].pValue);
                if (name)
                    return CastPhString(name);
            }
        }
        return CWinToken::tr("(Invalid SID)");
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_BOOLEAN:
		return  Attribute->Values.pInt64[ValueIndex] != 0;
    case TOKEN_SECURITY_ATTRIBUTE_TYPE_OCTET_STRING:
        return QByteArray((char*)Attribute->Values.pOctetString->pValue, Attribute->Values.pOctetString->ValueLength).toHex();
    default:
        return CWinToken::tr("(Unknown)");
    }
}

QMap<QString, CWinToken::SAttribute> CWinToken::GetAttributes()
{
	QReadLocker Locker(&m_Mutex); 

	QMap<QString, SAttribute> Attributes;

	HANDLE tokenHandle = NULL;
	if (!NT_SUCCESS(PhpOpenProcessTokenForPage(&tokenHandle, TOKEN_QUERY, m)))
		return Attributes;

	PTOKEN_SECURITY_ATTRIBUTES_INFORMATION info;
    if (NT_SUCCESS(PhQueryTokenVariableSize(tokenHandle, TokenSecurityAttributes, (PVOID*)&info)))
    {
        for (int i = 0; i < info->AttributeCount; i++)
        {
            PTOKEN_SECURITY_ATTRIBUTE_V1 attribute = &info->Attribute.pAttributeV1[i];
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