#include "stdafx.h"
#include "SecurityExplorer.h"
#include "../API/Windows/ProcessHacker.h"
#include "../API/Windows/WindowsAPI.h"
#include "../../MiscHelpers/Common/Settings.h"
#include <wincred.h>
#include <wincrypt.h>
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Samlib.lib")
#pragma comment(lib, "Secur32.lib")

CSecurityExplorer::CSecurityExplorer(QWidget *parent)
	: QMainWindow(parent)
{
	this->setWindowTitle(tr("Security Explorer"));

	QWidget* centralWidget = new QWidget();
	ui.setupUi(centralWidget);
	this->setCentralWidget(centralWidget);

	connect(ui.btnEditPolSec, SIGNAL(pressed()), this, SLOT(OnSecPolEdit()));
	connect(ui.accounts, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnAccount(QTreeWidgetItem*)));
	connect(ui.users, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnUser(QTreeWidgetItem*)));
	connect(ui.groups, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnGroupe(QTreeWidgetItem*)));

	
	//connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	restoreGeometry(theConf->GetBlob("SecExplorerWindow/Window_Geometry"));

	LoadAccounts();
	LoadPriviledges();
	LoadSessions();
	LoadUsers();
	LoadGroups();
	LoadCredentials();
}

CSecurityExplorer::~CSecurityExplorer()
{
	theConf->SetBlob("SecExplorerWindow/Window_Geometry",saveGeometry());
}

void CSecurityExplorer::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

/*void CSecurityExplorer::accept()
{
	this->close();
}*/

void CSecurityExplorer::reject()
{
	this->close();
}

void CSecurityExplorer::LoadAccounts()
{
	ui.accounts->clear();

	LSA_HANDLE policyHandle;
	if (NT_SUCCESS(PhOpenLsaPolicy(&policyHandle, POLICY_VIEW_LOCAL_INFORMATION, NULL)))
	{
		LSA_ENUMERATION_HANDLE enumerationHandle = 0;
		PLSA_ENUMERATION_INFORMATION accounts;
		ULONG numberOfAccounts;
		while (NT_SUCCESS(LsaEnumerateAccounts(policyHandle, &enumerationHandle, (PVOID*)&accounts, 0x100, &numberOfAccounts)))
		{
			for (ULONG i = 0; i < numberOfAccounts; i++)
			{
				QByteArray Sid = QByteArray((char*)accounts[i].Sid, RtlLengthSid(accounts[i].Sid));

				QTreeWidgetItem* pItem = new QTreeWidgetItem();
				
				PPH_STRING name = PhGetSidFullName(accounts[i].Sid, TRUE, NULL);
				pItem->setText(0, name ? CastPhString(name) : tr("unknown"));

				PPH_STRING sid = PhSidToStringSid(accounts[i].Sid);
				pItem->setText(1, sid ? CastPhString(sid) : tr("unknown"));
				pItem->setData(1, Qt::UserRole, Sid);

				ui.accounts->addTopLevelItem(pItem);
			}

			LsaFreeMemory(accounts);
		}

        LsaClose(policyHandle);
    }

}

void CSecurityExplorer::LoadPriviledges()
{
	/*LSA_HANDLE policyHandle;
	if (NT_SUCCESS(PhOpenLsaPolicy(&policyHandle, POLICY_VIEW_LOCAL_INFORMATION, NULL)))
	{
		LSA_ENUMERATION_HANDLE enumerationHandle = 0;
		PPOLICY_PRIVILEGE_DEFINITION privileges;
		ULONG numberOfPrivileges;
        while (NT_SUCCESS(LsaEnumeratePrivileges(policyHandle, &enumerationHandle, (PVOID*)&privileges, 0x100, &numberOfPrivileges)))
        {
            for (ULONG i = 0; i < numberOfPrivileges; i++)
            {
                PPH_STRING name = PhCreateStringEx(privileges[i].Name.Buffer, privileges[i].Name.Length);
				PPH_STRING displayName;
                if (PhLookupPrivilegeDisplayName(&name->sr, &displayName))
                {
                    // name
					// displayName
                }
            }

            LsaFreeMemory(privileges);
        }

        LsaClose(policyHandle);
    }*/
}

void CSecurityExplorer::LoadSessions()
{
	ui.sessions->clear();

	ULONG logonSessionCount = 0;
    PLUID logonSessionList = NULL;
	if (NT_SUCCESS(LsaEnumerateLogonSessions(&logonSessionCount, &logonSessionList)))
    {
        for (ULONG i = 0; i < logonSessionCount; i++)
        {
            PSECURITY_LOGON_SESSION_DATA logonSessionData;

            if (NT_SUCCESS(LsaGetLogonSessionData(&logonSessionList[i], &logonSessionData)))
            {
                QTreeWidgetItem* pItem = new QTreeWidgetItem();
				
                if (RtlValidSid(logonSessionData->Sid))
                {
					QByteArray Sid = QByteArray((char*)logonSessionData->Sid, RtlLengthSid(logonSessionData->Sid));

					PPH_STRING name = PhGetSidFullName(logonSessionData->Sid, TRUE, NULL);
					pItem->setText(0, name ? CastPhString(name) : tr("unknown"));

					PPH_STRING sid = PhSidToStringSid(logonSessionData->Sid);
					pItem->setText(1, sid ? CastPhString(sid) : tr("unknown"));
					pItem->setData(1, Qt::UserRole, Sid);

					ui.sessions->addTopLevelItem(pItem);
                }
                
				pItem->setText(2, tr("0x%1").arg(logonSessionData->LogonId.LowPart, 0, 16));

                LsaFreeReturnBuffer(logonSessionData);
            }
        }

        LsaFreeReturnBuffer(logonSessionList);
    }
}

void CSecurityExplorer::LoadUsers()
{
	ui.users->clear();

	NTSTATUS status;
    LSA_HANDLE policyHandle = NULL;
    SAM_HANDLE serverHandle = NULL;
    SAM_HANDLE domainHandle = NULL;
    SAM_HANDLE userHandle = NULL;
    SAM_ENUMERATE_HANDLE enumContext = 0;
    ULONG enumBufferLength = 0;
    PSAM_RID_ENUMERATION enumBuffer = NULL;
    PPOLICY_ACCOUNT_DOMAIN_INFO policyDomainInfo = NULL;

    if (!NT_SUCCESS(status = PhOpenLsaPolicy(&policyHandle, POLICY_VIEW_LOCAL_INFORMATION, NULL)))
        goto CleanupExit;

    if (!NT_SUCCESS(status = LsaQueryInformationPolicy(policyHandle, PolicyAccountDomainInformation, (PVOID*)&policyDomainInfo)))
        goto CleanupExit;

    if (!NT_SUCCESS(status = SamConnect(NULL, &serverHandle, SAM_SERVER_CONNECT | SAM_SERVER_LOOKUP_DOMAIN, NULL)))
        goto CleanupExit;

    if (!NT_SUCCESS(status = SamOpenDomain(serverHandle, DOMAIN_LIST_ACCOUNTS | DOMAIN_LOOKUP, policyDomainInfo->DomainSid, &domainHandle)))
        goto CleanupExit;

    if (!NT_SUCCESS(status = SamEnumerateUsersInDomain(domainHandle, &enumContext, 0 /*USER_ACCOUNT_TYPE_MASK*/, (PVOID*)&enumBuffer, -1, &enumBufferLength)))
        goto CleanupExit;

    for (ULONG i = 0; i < enumBufferLength; i++)
    {
        PSID userSid = NULL;
        PUSER_ALL_INFORMATION userInfo = NULL;

        if (!NT_SUCCESS(status = SamOpenUser(domainHandle, USER_ALL_ACCESS, enumBuffer[i].RelativeId, &userHandle)))
            continue;

		if (NT_SUCCESS(status = SamQueryInformationUser(userHandle, UserAllInformation, (PVOID*)&userInfo)))
		{
			if (NT_SUCCESS(status = SamRidToSid(userHandle, enumBuffer[i].RelativeId, &userSid)))
			{
				QByteArray Sid = QByteArray((char*)userSid, RtlLengthSid(userSid));

				QTreeWidgetItem* pItem = new QTreeWidgetItem();
				pItem->setData(0, Qt::UserRole, (quint64)enumBuffer[i].RelativeId);

				PPH_STRING name = PhGetSidFullName(userSid, TRUE, NULL);
				pItem->setText(0, name ? CastPhString(name) : tr("unknown"));

				PPH_STRING sid = PhSidToStringSid(userSid);
				pItem->setText(1, sid ? CastPhString(sid) : tr("unknown"));
				pItem->setData(1, Qt::UserRole, Sid);

				ui.users->addTopLevelItem(pItem);
			}

			SamCloseHandle(userHandle);
		}

        SamFreeMemory(userInfo);
    }

CleanupExit:

    if (enumBuffer)
        SamFreeMemory(enumBuffer);

    if (domainHandle)
        SamCloseHandle(domainHandle);

    if (serverHandle)
        SamCloseHandle(serverHandle);

    if (policyDomainInfo)
        LsaFreeMemory(policyDomainInfo);

    if (policyHandle)
        LsaClose(policyHandle);
}

void CSecurityExplorer::LoadGroups()
{
	ui.groups->clear();

	NTSTATUS status;
    LSA_HANDLE policyHandle = NULL;
    SAM_HANDLE serverHandle = NULL;
    SAM_HANDLE domainHandle = NULL;
    SAM_HANDLE groupHandle = NULL;
    SAM_ENUMERATE_HANDLE enumContext = 0;
    ULONG enumBufferLength = 0;
    PSAM_RID_ENUMERATION enumBuffer = NULL;
    PPOLICY_ACCOUNT_DOMAIN_INFO policyDomainInfo = NULL;

    if (!NT_SUCCESS(status = PhOpenLsaPolicy(&policyHandle, POLICY_VIEW_LOCAL_INFORMATION, NULL)))
        goto CleanupExit;

    if (!NT_SUCCESS(status = LsaQueryInformationPolicy(policyHandle, PolicyAccountDomainInformation, (PVOID*)&policyDomainInfo)))
        goto CleanupExit;

    if (!NT_SUCCESS(status = SamConnect(NULL, &serverHandle, SAM_SERVER_CONNECT | SAM_SERVER_LOOKUP_DOMAIN, NULL)))
        goto CleanupExit;

    if (!NT_SUCCESS(status = SamOpenDomain(serverHandle, DOMAIN_LIST_ACCOUNTS | DOMAIN_LOOKUP, policyDomainInfo->DomainSid, &domainHandle)))
        goto CleanupExit;

	if (!NT_SUCCESS(status = SamEnumerateGroupsInDomain(domainHandle, &enumContext, (PVOID*)&enumBuffer, -1, &enumBufferLength)))
        goto CleanupExit;

    for (ULONG i = 0; i < enumBufferLength; i++)
    {
		PSID groupSid = NULL;
        PGROUP_GENERAL_INFORMATION groupInfo = NULL;

        if (!NT_SUCCESS(status = SamOpenGroup(domainHandle, GROUP_ALL_ACCESS, enumBuffer[i].RelativeId, (PVOID*)&groupHandle)))
            continue;

        if (NT_SUCCESS(status = SamQueryInformationGroup(groupHandle, GroupGeneralInformation, (PVOID*)&groupInfo)))
        {
			QTreeWidgetItem* pItem = new QTreeWidgetItem();
			pItem->setData(0, Qt::UserRole, (quint64)enumBuffer[i].RelativeId);

			if (NT_SUCCESS(status = SamRidToSid(groupHandle, enumBuffer[i].RelativeId, &groupSid)))
			{
				QByteArray Sid = QByteArray((char*)groupSid, RtlLengthSid(groupSid));

				//PPH_STRING name = PhGetSidFullName(groupSid, TRUE, NULL);
				//pItem->setText(0, name ? CastPhString(name) : tr("unknown"));

				PPH_STRING sid = PhSidToStringSid(groupSid);
				pItem->setText(1, sid ? CastPhString(sid) : tr("unknown"));
				pItem->setData(1, Qt::UserRole, Sid);
			}

			pItem->setText(0, QString::fromWCharArray(groupInfo->Name.Buffer, groupInfo->Name.Length / sizeof(wchar_t)));

			pItem->setText(2, QString::fromWCharArray(groupInfo->AdminComment.Buffer, groupInfo->AdminComment.Length / sizeof(wchar_t)));

			ui.groups->addTopLevelItem(pItem);

            SamFreeMemory(groupInfo);
        }

        SamCloseHandle(groupHandle);
    }

CleanupExit:

    if (enumBuffer)
        SamFreeMemory(enumBuffer);

    if (domainHandle)
        SamCloseHandle(domainHandle);

    if (serverHandle)
        SamCloseHandle(serverHandle);

    if (policyDomainInfo)
        LsaFreeMemory(policyDomainInfo);

    if (policyHandle)
        LsaClose(policyHandle);
}

void CSecurityExplorer::LoadCredentials()
{
	ui.credential->clear();

    ULONG count = 0;
    PCREDENTIAL* credential = NULL;

    CredEnumerate(NULL, CRED_ENUMERATE_ALL_CREDENTIALS, &count, &credential);

    for (ULONG i = 0; i < count; i++)
    {
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
	
		pItem->setText(0, QString::fromWCharArray(credential[i]->TargetName));
		pItem->setText(1, QString::fromWCharArray(credential[i]->UserName));
		pItem->setText(2, QString::fromWCharArray(credential[i]->Comment));
		LARGE_INTEGER FileTime;
		FileTime.HighPart = credential[i]->LastWritten.dwHighDateTime;
		FileTime.LowPart = credential[i]->LastWritten.dwLowDateTime;
		pItem->setText(3, QDateTime::fromTime_t(FILETIME2time(FileTime.QuadPart)).toString("dd.MM.yyyy hh:mm:ss"));

		ui.credential->addTopLevelItem(pItem);
    }
}

NTSTATUS NTAPI SxpOpenLsaPolicy(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
{
    return PhOpenLsaPolicy(Handle, DesiredAccess, NULL);
}

void CSecurityExplorer::OnSecPolEdit()
{
	PhEditSecurity(NULL, L"Local LSA Policy", L"LsaPolicy", SxpOpenLsaPolicy, NULL, NULL);
}

NTSTATUS NTAPI SxpOpenSelectedLsaAccount(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
{
    NTSTATUS status;
    LSA_HANDLE policyHandle;

    if (NT_SUCCESS(status = PhOpenLsaPolicy(&policyHandle, POLICY_LOOKUP_NAMES, NULL)))
    {
        status = LsaOpenAccount(policyHandle, Context, DesiredAccess, Handle);
        LsaClose(policyHandle);
    }

    return status;
}

void CSecurityExplorer::OnAccount(QTreeWidgetItem* pItem)
{
	QByteArray Sid = pItem->data(1, Qt::UserRole).toByteArray();
	static char sid[256];
	memcpy(&sid, Sid.data(), qMin(Sid.size(), 256));

	PhEditSecurity(NULL, (wchar_t*)pItem->text(0).toStdWString().c_str(), L"LsaAccount", SxpOpenSelectedLsaAccount, NULL, &sid);
}

NTSTATUS NTAPI SxpOpenSelectedSamAccount(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
{
	quint64 RelativeId = *((quint64*)Context);

    NTSTATUS status;
    LSA_HANDLE policyHandle = NULL;
    SAM_HANDLE serverHandle = NULL;
    SAM_HANDLE domainHandle = NULL;
    PPOLICY_ACCOUNT_DOMAIN_INFO policyDomainInfo = NULL;
    
    __try
    {
        if (!NT_SUCCESS(status = PhOpenLsaPolicy(&policyHandle, POLICY_VIEW_LOCAL_INFORMATION, NULL)))
            __leave;

        if (!NT_SUCCESS(status = LsaQueryInformationPolicy(policyHandle, PolicyAccountDomainInformation, (PVOID*)&policyDomainInfo)))
            __leave;

        if (!NT_SUCCESS(status = SamConnect(NULL, &serverHandle, SAM_SERVER_CONNECT | SAM_SERVER_LOOKUP_DOMAIN, NULL)))
            __leave;

        if (!NT_SUCCESS(status = SamOpenDomain(serverHandle, DOMAIN_LIST_ACCOUNTS | DOMAIN_LOOKUP, policyDomainInfo->DomainSid, &domainHandle)))
            __leave;

		switch (RelativeId >> 32)
		{
		case 0:
			if (!NT_SUCCESS(status = SamOpenUser(domainHandle, DesiredAccess, RelativeId & 0xFFFFFFFF, Handle)))
				__leave;
			break;
		case 1:
			if (!NT_SUCCESS(status = SamOpenGroup(domainHandle, DesiredAccess, RelativeId & 0xFFFFFFFF, Handle)))
				__leave;
			break;
		}   
    }
    __finally
    {
        if (domainHandle)
            SamFreeMemory(domainHandle);

        if (serverHandle)
            SamFreeMemory(serverHandle);

        if (policyDomainInfo)
            LsaFreeMemory(policyDomainInfo);

        if (policyHandle)
            LsaClose(policyHandle);
    }

    return status;
}

void CSecurityExplorer::OnUser(QTreeWidgetItem* pItem)
{
	static quint64 RelativeId = 0;
	RelativeId = pItem->data(0, Qt::UserRole).toUInt();
	RelativeId |= (quint64)0 << 32;

	PhEditSecurity(NULL, (wchar_t*)pItem->text(0).toStdWString().c_str(), L"SamUser", SxpOpenSelectedSamAccount, NULL, &RelativeId);
}

void CSecurityExplorer::OnGroupe(QTreeWidgetItem* pItem)
{
	static quint64 RelativeId = 0;
	RelativeId = pItem->data(0, Qt::UserRole).toUInt();
	RelativeId |= (quint64)1 << 32;

	PhEditSecurity(NULL, (wchar_t*)pItem->text(0).toStdWString().c_str(), L"SamGroup", SxpOpenSelectedSamAccount, NULL, &RelativeId);
}

/*
void CSecurityExplorer::OnDeleteAccount(QTreeWidgetItem* pItem)
{
	if (QMessageBox("TaskExplorer", tr("Do you want to delete the sellected account?"), QMessageBox::Question, QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

	QByteArray Sid = pItem->data(1, Qt::UserRole).toByteArray();
	static char sid[256];
	memcpy(&sid, Sid.data(), qMin(Sid.size(), 256));

	NTSTATUS status;
	LSA_HANDLE policyHandle;
	LSA_HANDLE accountHandle;

	if (NT_SUCCESS(status = PhOpenLsaPolicy(&policyHandle, POLICY_LOOKUP_NAMES, NULL)))
	{
		if (NT_SUCCESS(status = LsaOpenAccount(policyHandle, sid, ACCOUNT_VIEW | DELETE, &accountHandle))) // ACCOUNT_VIEW is needed as well for some reason
		{
			status = LsaDelete(accountHandle);
			LsaClose(accountHandle);
		}

		LsaClose(policyHandle);
	}

	if (NT_SUCCESS(status))
		LoadAccounts();
	else
		QMessageBox::warning(this, "TaskExplorer", tr("Unable to delete the account"));
}
*/