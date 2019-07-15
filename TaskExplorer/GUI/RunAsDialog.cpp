#include "stdafx.h"
#include "RunAsDialog.h"
#ifdef WIN32
#include "../API/Windows/ProcessHacker/RunAs.h"
#endif

CRunAsDialog::CRunAsDialog(quint64 PID, QWidget *parent)
	: QMainWindow(parent)
{
	QWidget* centralWidget = new QWidget();
	this->setCentralWidget(centralWidget);
	ui.setupUi(centralWidget);

	m_PID = PID;

	ui.loginType->addItem("Batch", LOGON32_LOGON_BATCH);
	ui.loginType->addItem("Interactive", LOGON32_LOGON_INTERACTIVE);
	ui.loginType->addItem("Network", LOGON32_LOGON_NETWORK);
	ui.loginType->addItem("New credentials", LOGON32_LOGON_NEW_CREDENTIALS);
	ui.loginType->addItem("Service", LOGON32_LOGON_SERVICE);
	ui.loginType->setCurrentIndex(1); // Interactive
	
	AddProgramsToComboBox(ui.binaryPath);
	AddAccountsToComboBox(ui.userName);
	AddSessionsToComboBox(ui.session);
    AddDesktopsToComboBox(ui.desktop);

    SetDefaultSessionEntry(ui.session);
	SetDefaultDesktopEntry(ui.desktop);

	if (m_PID != 0)
	{
		ui.userName->setEnabled(false);
		ui.loginType->setEnabled(false);
		ui.password->setEnabled(false);
	}
	OnUserName(ui.userName->currentText());

	connect(ui.userName, SIGNAL(currentTextChanged(const QString&)), this, SLOT(OnUserName(const QString&)));

	connect(ui.browseBtn, SIGNAL(pressed()), this, SLOT(OnBrowse()));
	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

void CRunAsDialog::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

void CRunAsDialog::accept()
{
#ifdef WIN32
	NTSTATUS status;
	BOOLEAN useLinkedToken = FALSE;
	BOOLEAN createSuspended = FALSE;
	ULONG logonType = ULONG_MAX;
	ULONG sessionId = ULONG_MAX;
	PPH_STRING program = NULL;
	PPH_STRING username = NULL;
	PPH_STRING password = NULL;
	PPH_STRING desktopName = NULL;
	HANDLE ProcessId = (HANDLE)m_PID;

	program = CastQString(ui.binaryPath->currentText());
	username = CastQString(ui.userName->currentText());
	useLinkedToken = ui.useToken->isChecked();
	createSuspended = ui.suspended->isChecked();

	if (PhIsNullOrEmptyString(program))
		goto cleanup;

	logonType = ui.loginType->currentData().toUInt();
	sessionId = ui.session->currentData().toUInt();
	desktopName = CastQString(ui.desktop->currentText());

	if (sessionId == ULONG_MAX)
		goto cleanup;

	// Fix up the user name if it doesn't have a domain.
	if (PhFindCharInString(username, 0, '\\') == -1)
	{
		PSID sid;
		PPH_STRING newUserName;

		if (NT_SUCCESS(PhLookupName(&username->sr, &sid, NULL, NULL)))
		{
			if (newUserName = PhGetSidFullName(sid, TRUE, NULL))
				PhSwapReference((PVOID*)&username, newUserName);

			PhFree(sid);
		}
	}

	if (!IsServiceAccount(username))
	{
		password = CastQString(ui.password->text());
		ui.password->clear();
	}

	//if (IsCurrentUserAccount(username))
	//{
	//    status = PhCreateProcessWin32(
	//        NULL,
	//        program->Buffer,
	//        NULL,
	//        NULL,
	//        0,
	//        NULL,
	//        NULL,
	//        NULL
	//        );
	//}

	ULONG currentSessionId = ULONG_MAX;

	PhGetProcessSessionId(NtCurrentProcess(), &currentSessionId);

	if (logonType == LOGON32_LOGON_INTERACTIVE && !ProcessId && sessionId == currentSessionId && !useLinkedToken)
	{
		// We are eligible to load the user profile.
		// This must be done here, not in the service, because
		// we need to be in the target session.

		PH_CREATE_PROCESS_AS_USER_INFO createInfo;
		PPH_STRING domainPart = NULL;
		PPH_STRING userPart = NULL;

		PhpSplitUserName(username->Buffer, &domainPart, &userPart);

		memset(&createInfo, 0, sizeof(PH_CREATE_PROCESS_AS_USER_INFO));
		createInfo.CommandLine = PhGetString(program);
		createInfo.UserName = PhGetString(userPart);
		createInfo.DomainName = PhGetString(domainPart);
		createInfo.Password = PhGetStringOrEmpty(password);

		// Whenever we can, try not to set the desktop name; it breaks a lot of things.
		if (!PhIsNullOrEmptyString(desktopName) && !PhEqualString2(desktopName, L"WinSta0\\Default", TRUE))
			createInfo.DesktopName = PhGetString(desktopName);

		PhSetDesktopWinStaAccess();

		status = PhCreateProcessAsUser(
			&createInfo,
			PH_CREATE_PROCESS_WITH_PROFILE | (createSuspended ? PH_CREATE_PROCESS_SUSPENDED : 0),
			NULL,
			NULL,
			NULL
			);

		if (domainPart) PhDereferenceObject(domainPart);
		if (userPart) PhDereferenceObject(userPart);
	}
	else
	{
		status = PhExecuteRunAsCommand3(
			PhMainWndHandle,
			PhGetString(program),
			PhGetString(username),
			PhGetStringOrEmpty(password),
			logonType,
			ProcessId,
			sessionId,
			PhGetString(desktopName),
			useLinkedToken,
			createSuspended
			);
	}

	if (!NT_SUCCESS(status))
	{
		if (status != STATUS_CANCELLED)
			QMessageBox::warning(NULL, "TaskExplorer", tr("Unable to start the program, Error: %1").arg(status));
	}
	else if (status != STATUS_TIMEOUT)
	{
		PhpAddRunMRUListEntry(program);
		this->close();
	}

cleanup:
	if (program)
		PhDereferenceObject(program);
	if (username)
		PhDereferenceObject(username);
	if (password)
	{
		RtlSecureZeroMemory(password->Buffer, password->Length);
		PhDereferenceObject(password);
	}
	if (desktopName)
		PhDereferenceObject(desktopName);
#endif
}

void CRunAsDialog::reject()
{
	this->close();
}

void CRunAsDialog::OnBrowse()
{
	QStringList FilePaths = QFileDialog::getOpenFileNames(0, tr("Select binary"), "", tr("All files (*.*)"));
	if (FilePaths.isEmpty())
		return;

	ui.binaryPath->setEditText(FilePaths.first());
}

void CRunAsDialog::OnUserName(const QString& userName)
{
#ifdef WIN32
	PPH_STRING username = CastQString(userName);
	if (IsServiceAccount(username))
    {
		ui.password->setEnabled(false);
        ui.loginType->setCurrentIndex(4); // Service
    }
    else
    {
        ui.password->setEnabled(true);
        ui.loginType->setCurrentIndex(1); // Interactive
    }
	if (username)
		PhDereferenceObject(username);
#endif
}