/*
 * Copyright 2022-2025 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>
#include "UpdDefines.h"
#include "AppUtil.h"

//
// TaskExplorer-specific implementation
//

std::wstring GetSoftware()
{
	return SOFTWARE_NAME;
}

// Find process by name
DWORD FindProcessByName(const std::wstring& processName)
{
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return 0;

	PROCESSENTRY32W pe32;
	pe32.dwSize = sizeof(PROCESSENTRY32W);

	if (Process32FirstW(hSnapshot, &pe32))
	{
		do
		{
			if (_wcsicmp(pe32.szExeFile, processName.c_str()) == 0)
			{
				CloseHandle(hSnapshot);
				return pe32.th32ProcessID;
			}
		} while (Process32NextW(hSnapshot, &pe32));
	}

	CloseHandle(hSnapshot);
	return 0;
}

// Terminate process by PID
bool TerminateProcessByPid(DWORD pid, DWORD timeout = 10000)
{
	HANDLE hProcess = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
	if (!hProcess)
		return false;

	if (!TerminateProcess(hProcess, 0))
	{
		CloseHandle(hProcess);
		return false;
	}

	// Wait for process to exit
	DWORD result = WaitForSingleObject(hProcess, timeout);
	CloseHandle(hProcess);

	return (result == WAIT_OBJECT_0);
}

//
// App-specific function implementations
//

bool AppShutdown(const std::wstring& base_dir)
{
	std::wcout << L"Shutting down TaskExplorer..." << std::endl;

	// Find TaskExplorer.exe process
	DWORD pid = FindProcessByName(L"TaskExplorer.exe");
	if (pid == 0)
	{
		std::wcout << L"TaskExplorer is not running" << std::endl;
		return true; // Already not running
	}

	// Try to terminate gracefully by sending WM_CLOSE to main window
	HWND hwnd = FindWindowW(NULL, L"Task Explorer"); // Adjust window title as needed
	if (hwnd)
	{
		std::wcout << L"Sending close message to TaskExplorer..." << std::endl;
		SendMessageW(hwnd, WM_CLOSE, 0, 0);

		// Wait a bit for graceful shutdown
		for (int i = 0; i < 50; i++) // 5 seconds
		{
			Sleep(100);
			if (FindProcessByName(L"TaskExplorer.exe") == 0)
			{
				std::wcout << L"TaskExplorer closed gracefully" << std::endl;
				return true;
			}
		}
	}

	// Force terminate if still running
	std::wcout << L"Force terminating TaskExplorer..." << std::endl;
	if (TerminateProcessByPid(pid))
	{
		std::wcout << L"TaskExplorer terminated" << std::endl;
		return true;
	}

	std::wcout << L"Failed to terminate TaskExplorer" << std::endl;
	return false;
}

bool AppStart(const std::wstring& base_dir)
{
	std::wcout << L"Starting TaskExplorer..." << std::endl;

	std::wstring exePath = base_dir + L"\\TaskExplorer.exe";

	STARTUPINFOW si = { sizeof(si) };
	PROCESS_INFORMATION pi;

	if (CreateProcessW(
		exePath.c_str(),
		NULL,
		NULL,
		NULL,
		FALSE,
		0,
		NULL,
		base_dir.c_str(),
		&si,
		&pi))
	{
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		std::wcout << L"TaskExplorer started successfully" << std::endl;
		return true;
	}

	std::wcout << L"Failed to start TaskExplorer" << std::endl;
	return false;
}

bool AppIsRunning(const std::wstring& base_dir)
{
	return (FindProcessByName(L"TaskExplorer.exe") != 0);
}

