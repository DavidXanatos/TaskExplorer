#include "stdafx.h"
#include "RunDialog.h"
#include "../../MiscHelpers/Common/Settings.h"
#include "../API/SystemAPI.h"
#ifdef WIN32
#include "../API/Windows/ProcessHacker/RunAs.h"
#include "../API/Windows/WinHelpers/InjectDll/injdll.h"
#endif

CRunDialog::CRunDialog(QWidget *parent)
	: QMainWindow(parent)
{
	QWidget* centralWidget = new QWidget();
	ui.setupUi(centralWidget);
	this->setCentralWidget(centralWidget);

#ifdef WIN32
	AddProgramsToComboBox(ui.binaryPath);
#endif

	bool IsAdmin = theAPI->RootAvaiable();
	ui.elevated->setEnabled(IsAdmin);
	ui.lblShield->setVisible(IsAdmin);

	ui.dllPath->addItems(theConf->GetStringList("General/InjectionDlls"));
	//if(ui.dllPath->count() == 0)
	//	ui.dllPath->addItem("");
	ui.dllPath->addItem(tr("[Browse for Dll]"));
	ui.dllPath->setCurrentIndex(-1);
	ui.dllPath->setEnabled(false);


	connect(ui.injectDll, SIGNAL(stateChanged(int)), this, SLOT(OnInjectDll()));
	connect(ui.dllPath, SIGNAL(currentIndexChanged(int)), this, SLOT(OnDllPath()));
	connect(ui.browseBtn, SIGNAL(pressed()), this, SLOT(OnBrowse()));
	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	restoreGeometry(theConf->GetBlob("RunWindow/Window_Geometry"));
}

CRunDialog::~CRunDialog()
{
	theConf->SetBlob("RunWindow/Window_Geometry",saveGeometry());
}

void CRunDialog::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

bool CRunDialog::event(QEvent* event)
{
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent* key = static_cast<QKeyEvent*>(event);
		if ((key->key() == Qt::Key_Enter) || (key->key() == Qt::Key_Return)) {
			accept();
			return true;
		}
	}
	return QMainWindow::event(event);
}

NTSTATUS CWinProcess__LoadModule(HANDLE ProcessHandle, const QString& Path);

void CRunDialog::accept()
{
#ifdef WIN32
	wstring filePath = ui.binaryPath->currentText().toStdWString();
	QString DllPath = ui.dllPath->currentText();
	wstring dllPath = DllPath.toStdWString();

	NTSTATUS status;

	if (!ui.elevated->isChecked() && theAPI->RootAvaiable() && !ui.injectDll->isChecked() && !ui.suspended->isChecked())
	{
		if (NT_SUCCESS(status = RunAsLimitedUser((wchar_t*)filePath.c_str())))
			goto done;
	}

    HANDLE tokenHandle = NULL;
	HANDLE newTokenHandle = NULL;
	HANDLE threadHandle = NULL;
	HANDLE processHandle = NULL;
#ifdef _WIN64
	BOOLEAN isWow64 = FALSE;
#endif

	if (!NT_SUCCESS(status = PhOpenProcessToken(
		NtCurrentProcess(),
		TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_GROUPS | TOKEN_ADJUST_DEFAULT | READ_CONTROL | WRITE_DAC,
		&tokenHandle
	)))
		goto cleanup;

	if (!ui.elevated->isChecked() && theAPI->RootAvaiable())
	{
		// Note: the process will still look admin but wont have the privilegs
		if (!NT_SUCCESS(status = PhFilterTokenForLimitedUser(tokenHandle, &newTokenHandle)))
			goto cleanup;
	}

	/*
	 * PH_CREATE_PROCESS_INHERIT_HANDLES Inheritable handles will be duplicated to the process from the parent process.
	 * PH_CREATE_PROCESS_SUSPENDED The initial thread will be created suspended.
	 * PH_CREATE_PROCESS_BREAKAWAY_FROM_JOB The process will not be assigned to the job object associated with the parent process.
	 * PH_CREATE_PROCESS_NEW_CONSOLE The process will have its own console, instead of inheriting the console of the parent process.
	 */
    if (!NT_SUCCESS(status = PhCreateProcessWin32(
        NULL,
        (wchar_t*)filePath.c_str(),
        NULL,
        NULL,
        (ui.injectDll->isChecked() || ui.suspended->isChecked()) ? PH_CREATE_PROCESS_SUSPENDED : 0,
        newTokenHandle ? newTokenHandle : tokenHandle,
        &processHandle,
        &threadHandle
        )))
			goto cleanup;


	if (ui.injectDll->isChecked())
	{
		BYTE* pebAddress;

#ifdef _WIN64
		PVOID peb32;
		if (!NT_SUCCESS(status = PhGetProcessPeb32(processHandle, &peb32)))
			goto cleanup;
		isWow64 = !!peb32; // PhGetProcessIsWow64

		if (isWow64)
			pebAddress = (BYTE*)peb32;
		else
#endif
		{
			PROCESS_BASIC_INFORMATION basicInfo;
			if (!NT_SUCCESS(status = PhGetProcessBasicInformation(processHandle, &basicInfo)))
				goto cleanup;

			pebAddress = (BYTE*)basicInfo.PebBaseAddress;
		}

		unsigned long long LoaderThreadsOffset = 0;
#ifdef _WIN64
		if (!isWow64)
		{
			const int ProcessParameters_64 = 32; // FIELD_OFFSET(PEB, ProcessParameters); // 64 bit
			const int LoaderThreads_64 = 1036; // FIELD_OFFSET(RTL_USER_PROCESS_PARAMETERS, LoaderThreads); // 64 bit

			unsigned long long ProcessParameters;
			if (!NT_SUCCESS(status = NtReadVirtualMemory(processHandle, pebAddress + ProcessParameters_64, &ProcessParameters, sizeof(ProcessParameters), NULL)))
				goto cleanup;

			LoaderThreadsOffset = ProcessParameters + LoaderThreads_64;
		}
		else
#endif
		{
			const int ProcessParameters_32 = 16; // FIELD_OFFSET(PEB, ProcessParameters); // 32 bit
			const int LoaderThreads_32 = 672; // FIELD_OFFSET(RTL_USER_PROCESS_PARAMETERS, LoaderThreads); // 32 bit

			unsigned long ProcessParameters;
			if (!NT_SUCCESS(status = NtReadVirtualMemory(processHandle, pebAddress + ProcessParameters_32, &ProcessParameters, sizeof(ProcessParameters), NULL)))
				goto cleanup;

			LoaderThreadsOffset = ProcessParameters + LoaderThreads_32;
		}

		ULONG LoaderThreads;
		if (!NT_SUCCESS(status = NtReadVirtualMemory(processHandle, (PVOID)LoaderThreadsOffset, &LoaderThreads, sizeof(LoaderThreads), NULL)))
			goto cleanup;
		LoaderThreads = 1; 
		if (!NT_SUCCESS(status = NtWriteVirtualMemory(processHandle, (PVOID)LoaderThreadsOffset, &LoaderThreads, sizeof(LoaderThreads), NULL)))
			goto cleanup;

#ifdef _WIN64
		if (!isWow64)
			status = inject_x64(processHandle, threadHandle, dllPath.c_str()) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
		else
#endif
			status = inject_x86(processHandle, threadHandle, dllPath.c_str()) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;

		if (!NT_SUCCESS(status))
			status = STATUS_DLL_NOT_FOUND;
		else if (!ui.suspended->isChecked())
			status = NtResumeThread(threadHandle, NULL);
	}

cleanup:
	if (!NT_SUCCESS(status) && processHandle && processHandle != INVALID_HANDLE_VALUE)
		NtTerminateProcess(processHandle, -1);

	if(newTokenHandle)
		NtClose(newTokenHandle);
	if(tokenHandle)
		NtClose(tokenHandle);
	if(processHandle)
		NtClose(processHandle);
	if(threadHandle)
		NtClose(threadHandle);

done:
	if (!NT_SUCCESS(status))
	{
		PhShowStatus(PhMainWndHandle, L"Unable to execute the program.", status, 0);
	}
	else
	{
		PhpAddRunMRUListEntry(PH_STRINGREF { filePath.length() * sizeof(wchar_t), (wchar_t*)filePath.c_str() });

		if (ui.injectDll->isChecked())
		{
			QStringList DLLs = theConf->GetStringList("General/InjectionDlls");
			DLLs.removeAll(DllPath);
			DLLs.prepend(DllPath);
			theConf->SetValue("General/InjectionDlls", DLLs);
		}

		this->close();
	}
#else
	// linux-todo:
#endif
}

void CRunDialog::reject()
{
	this->close();
}

void CRunDialog::OnBrowse()
{
	QString FilePath = QFileDialog::getOpenFileName(0, tr("Select binary"), "", tr("All files (*.*)"));
	if (!FilePath.isEmpty())
		ui.binaryPath->setEditText(FilePath);
}

void CRunDialog::OnInjectDll()
{
	ui.dllPath->setEnabled(ui.injectDll->isChecked());
}

void CRunDialog::OnDllPath()
{
	if (ui.dllPath->currentIndex() != ui.dllPath->count() - 1)
		return;
	ui.dllPath->setCurrentIndex(-1);
	QString FilePath = QFileDialog::getOpenFileName(0, tr("Select injection DLL"), "", tr("Dll files (*.dll)"));
	if (!FilePath.isEmpty())
		ui.dllPath->setEditText(FilePath);
}