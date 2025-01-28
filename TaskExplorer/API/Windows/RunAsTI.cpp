#include <string>
#include <iostream>
#include <codecvt>
#include <Windows.h>
#include <TlHelp32.h>

using namespace std;

void enable_privilege(std::wstring privilege_name)
{
	HANDLE token_handle;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &token_handle))
		throw runtime_error("OpenProcessToken failed: " + to_string(GetLastError()));

	LUID luid;
	if (!LookupPrivilegeValue(nullptr, privilege_name.c_str(), &luid))
	{
		CloseHandle(token_handle);
		throw runtime_error("LookupPrivilegeValue failed: " + to_string(GetLastError()));
	}

	TOKEN_PRIVILEGES tp;
	tp.PrivilegeCount = 1;
	tp.Privileges[0].Luid = luid;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	if (!AdjustTokenPrivileges(token_handle, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr))
	{
		CloseHandle(token_handle);
		throw runtime_error("AdjustTokenPrivilege failed: " + to_string(GetLastError()));
	}

	CloseHandle(token_handle);
}

DWORD get_process_id_by_name(const std::wstring process_name)
{
	HANDLE snapshot_handle;
	if ((snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) == INVALID_HANDLE_VALUE)
	{
		throw runtime_error("CreateToolhelp32Snapshot failed: " + to_string(GetLastError()));
	}

	DWORD pid = -1;
	PROCESSENTRY32 pe;
	ZeroMemory(&pe, sizeof(PROCESSENTRY32W));
	pe.dwSize = sizeof(PROCESSENTRY32W);
	if (Process32First(snapshot_handle, &pe))
	{
		while (Process32Next(snapshot_handle, &pe))
		{
			if (pe.szExeFile == process_name)
			{
				pid = pe.th32ProcessID;
				break;
			}
		}
	}
	else
	{
		CloseHandle(snapshot_handle);
		throw runtime_error("Process32First failed: " + to_string(GetLastError()));
	}

	if (pid == -1)
	{
		CloseHandle(snapshot_handle);
		throw runtime_error("process not found");
	}

	CloseHandle(snapshot_handle);
	return pid;
}

void impersonate_system()
{
	const auto system_pid = get_process_id_by_name(L"winlogon.exe");
	HANDLE process_handle;
	if ((process_handle = OpenProcess(
		PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION,
		FALSE,
		system_pid)) == nullptr)
	{
		throw runtime_error("OpenProcess failed (winlogon.exe): " + to_string(GetLastError()));
	}

	HANDLE token_handle;
	if (!OpenProcessToken(
		process_handle,
		MAXIMUM_ALLOWED,
		&token_handle))
	{
		CloseHandle(process_handle);
		throw runtime_error("OpenProcessToken failed (winlogon.exe): " + to_string(GetLastError()));
	}

	HANDLE dup_token_handle;
	SECURITY_ATTRIBUTES token_attributes;
	token_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	token_attributes.lpSecurityDescriptor = nullptr;
	token_attributes.bInheritHandle = FALSE;
	if (!DuplicateTokenEx(
		token_handle,
		MAXIMUM_ALLOWED,
		&token_attributes,
		SecurityImpersonation,
		TokenImpersonation,
		&dup_token_handle))
	{
		CloseHandle(token_handle);
		throw runtime_error("DuplicateTokenEx failed (winlogon.exe): " + to_string(GetLastError()));
	}

	if (!ImpersonateLoggedOnUser(dup_token_handle))
	{
		CloseHandle(dup_token_handle);
		CloseHandle(token_handle);
		throw runtime_error("ImpersonateLoggedOnUser failed: " + to_string(GetLastError()));
	}

	CloseHandle(dup_token_handle);
	CloseHandle(token_handle);
}

int start_trusted_installer_service()
{
	SC_HANDLE sc_manager_handle;
	if ((sc_manager_handle = OpenSCManager(
		nullptr,
		SERVICES_ACTIVE_DATABASE,
		GENERIC_EXECUTE)) == nullptr)
	{
		throw runtime_error("OpenSCManager failed: " + to_string(GetLastError()));
	}

	SC_HANDLE service_handle;
	if ((service_handle = OpenServiceW(
		sc_manager_handle,
		L"TrustedInstaller",
		GENERIC_READ | GENERIC_EXECUTE)) == nullptr)
	{
		CloseServiceHandle(sc_manager_handle);
		throw runtime_error("OpenService failed: " + to_string(GetLastError()));
	}

	SERVICE_STATUS_PROCESS status_buffer;
	DWORD bytes_needed;
	while (QueryServiceStatusEx(
		service_handle,
		SC_STATUS_PROCESS_INFO,
		reinterpret_cast<LPBYTE>(&status_buffer),
		sizeof(SERVICE_STATUS_PROCESS),
		&bytes_needed))
	{
		if (status_buffer.dwCurrentState == SERVICE_STOPPED)
		{
			if (!StartServiceW(service_handle, 0, nullptr))
			{
				CloseServiceHandle(service_handle);
				CloseServiceHandle(sc_manager_handle);
				throw runtime_error("StartService failed: " + to_string(GetLastError()));
			}
		}
		if (status_buffer.dwCurrentState == SERVICE_START_PENDING ||
			status_buffer.dwCurrentState == SERVICE_STOP_PENDING)
		{
			Sleep(status_buffer.dwWaitHint);
			continue;
		}
		if (status_buffer.dwCurrentState == SERVICE_RUNNING)
		{
			CloseServiceHandle(service_handle);
			CloseServiceHandle(sc_manager_handle);
			return status_buffer.dwProcessId;
		}
	}
	CloseServiceHandle(service_handle);
	CloseServiceHandle(sc_manager_handle);
	throw runtime_error("QueryServiceStatusEx failed: " + to_string(GetLastError()));
}

void create_process_as_trusted_installer(const DWORD pid, std::wstring command_line)
{
	enable_privilege(SE_DEBUG_NAME);
	enable_privilege(SE_IMPERSONATE_NAME);
	impersonate_system();

	HANDLE process_handle;
	if ((process_handle = OpenProcess(
		PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION,
		FALSE,
		pid)) == nullptr)
	{
		throw runtime_error("OpenProcess failed (TrustedInstaller.exe): " + to_string(GetLastError()));
	}

	HANDLE token_handle;
	if (!OpenProcessToken(
		process_handle,
		MAXIMUM_ALLOWED,
		&token_handle))
	{
		CloseHandle(process_handle);
		throw runtime_error("OpenProcessToken failed (TrustedInstaller.exe): " + to_string(GetLastError()));
	}

	HANDLE dup_token_handle;
	SECURITY_ATTRIBUTES token_attributes;
	token_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	token_attributes.lpSecurityDescriptor = nullptr;
	token_attributes.bInheritHandle = FALSE;
	if (!DuplicateTokenEx(
		token_handle,
		MAXIMUM_ALLOWED,
		&token_attributes,
		SecurityImpersonation,
		TokenImpersonation,
		&dup_token_handle))
	{
		CloseHandle(token_handle);
		throw runtime_error("DuplicateTokenEx failed (TrustedInstaller.exe): " + to_string(GetLastError()));
	}

	STARTUPINFOW startup_info;
	ZeroMemory(&startup_info, sizeof(STARTUPINFOW));
	startup_info.lpDesktop = (PWSTR)L"Winsta0\\Default";
	PROCESS_INFORMATION process_info;
	ZeroMemory(&process_info, sizeof(PROCESS_INFORMATION));
	if (!CreateProcessWithTokenW(
		dup_token_handle,
		LOGON_WITH_PROFILE,
		nullptr,
		const_cast<LPWSTR>(command_line.c_str()),
		CREATE_UNICODE_ENVIRONMENT,
		nullptr,
		nullptr,
		&startup_info,
		&process_info))
	{
		throw runtime_error("CreateProcessWithTokenW failed: " + to_string(GetLastError()));
	}
}

void create_process_as_trusted_installer(std::wstring command_line)
{
	try
	{
		const auto pid = start_trusted_installer_service();
		create_process_as_trusted_installer(pid, L"\"" + command_line + L"\"");
	}
	catch (exception e)
	{
		wcout << e.what() << endl;
	}
}