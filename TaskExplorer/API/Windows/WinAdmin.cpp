#include "stdafx.h"
#include "WinAdmin.h"

#include <windows.h>
#include <codecvt>
#include <Shlwapi.h>

// Note: we want to restart early before initlaizzing PHlib so we need some standalone functions 

bool IsElevated()
{
    bool fRet = false;
    HANDLE hToken = NULL;
    if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hToken))
	{
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if(GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof( Elevation ), &cbSize)) 
            fRet = Elevation.TokenIsElevated;
    }
    if(hToken) 
        CloseHandle(hToken);
    return fRet;
}

int RunElevated(const std::wstring& Params, bool bGetCode)
{
	wchar_t szPath[MAX_PATH];
	if (!GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)))
		return -3;
	return RunElevated(std::wstring(szPath), Params, bGetCode);
}

int RunElevated(const std::wstring& binaryPath, const std::wstring& Params, bool bGetCode)
{
	// Launch itself as admin
	SHELLEXECUTEINFO sei = { sizeof(sei) };
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.lpVerb = L"runas";
	sei.lpFile = binaryPath.c_str();
	sei.lpParameters = Params.c_str();
	sei.hwnd = NULL;
	sei.nShow = SW_NORMAL;
	if (!ShellExecuteEx(&sei))
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_CANCELLED)
			return -2; // The user refused to allow privileges elevation.
	}
	else
	{
		if (bGetCode)
		{
			WaitForSingleObject(sei.hProcess, 10000);
			DWORD ExitCode = -4;
			BOOL success = GetExitCodeProcess(sei.hProcess, &ExitCode);
			CloseHandle(sei.hProcess);
			return success ? ExitCode : -4;
		}
		return 0;
	}
	return -1;
}

int RestartElevated(int &argc, char **argv)
{
	std::wstring Params;
	for (int i = 1; i < argc; i++)
	{
		if (i > 1)
			Params.append(L" ");
		Params.append(L"\"" + std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(argv[i]) + L"\"");
	}
	return RunElevated(Params);
}

//////////////////////////////////////////////////////////////////////////////////
// AutoRun

#define APP_NAME L"TaskExplorer"

#define AUTO_RUN_KEY_NAME APP_NAME L"_AutoRun"

bool IsAutorunEnabled()
{
	bool result = false;

	HKEY hkey = nullptr;
	if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hkey) == ERROR_SUCCESS)
	{
		WCHAR buffer[MAX_PATH] = {0};
		DWORD size = _countof (buffer);

		if (RegQueryValueEx (hkey, AUTO_RUN_KEY_NAME, nullptr, nullptr, (LPBYTE)buffer, &size) == ERROR_SUCCESS)
		{
			result = true; // todo: check path
		}

		RegCloseKey (hkey);
	}

	return result;
}

bool AutorunEnable (bool is_enable)
{
	bool result = false;

	HKEY hkey = nullptr;
	if (RegOpenKeyEx (HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hkey) == ERROR_SUCCESS)
	{
		if (is_enable)
		{
			wchar_t szPath[MAX_PATH];
			if (GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)))
			{
				std::wstring path = L"\"" + std::wstring(szPath) + L"\" -autorun";

				result = (RegSetValueEx(hkey, AUTO_RUN_KEY_NAME, 0, REG_SZ, (LPBYTE)path.c_str(), DWORD((path.length() + 1) * sizeof(WCHAR))) == ERROR_SUCCESS);
			}
		}
		else
		{
			result = (RegDeleteValue (hkey, AUTO_RUN_KEY_NAME) == ERROR_SUCCESS);
		}

		RegCloseKey (hkey);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////
// Skip UAC from simplewall by Henry++

#define SKIP_UAC_TASK_NAME APP_NAME L"_SkipUac"

#include <comdef.h>
#include <taskschd.h>

struct MBSTR
{
	MBSTR (LPCWSTR asString = nullptr)
	{
		ms_bstr = asString ? SysAllocString (asString) : nullptr;
	}

	~MBSTR ()
	{
		Free ();
	}

	operator BSTR() const
	{
		return ms_bstr;
	}

	MBSTR& operator=(LPCWSTR asString)
	{
		if (asString != ms_bstr)
		{
			Free ();
			ms_bstr = asString ? ::SysAllocString (asString) : NULL;
		}

		return *this;
	}

	void Free ()
	{
		if (ms_bstr)
		{
			SysFreeString (ms_bstr);
			ms_bstr = nullptr;
		}
	}
protected:
	BSTR ms_bstr;
};

bool SkipUacEnable (bool is_enable)
{
	bool result = false;
	bool action_result = false;

	ITaskService* service = nullptr;
	ITaskFolder* folder = nullptr;
	ITaskDefinition* task = nullptr;
	IRegistrationInfo* reginfo = nullptr;
	IPrincipal* principal = nullptr;
	ITaskSettings* settings = nullptr;
	IActionCollection* action_collection = nullptr;
	IAction* action = nullptr;
	IExecAction* exec_action = nullptr;
	IRegisteredTask* registered_task = nullptr;

	wchar_t szPath[MAX_PATH];
	if (!GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)))
		return false;
    std::wstring::size_type pos = std::wstring(szPath).find_last_of( L"\\/" );
    std::wstring dir = std::wstring(szPath).substr(0, pos);

	MBSTR root (L"\\");
	MBSTR name (SKIP_UAC_TASK_NAME);
	MBSTR author (APP_NAME);
	MBSTR path (szPath);
	MBSTR directory (dir.c_str());
	MBSTR args (L"$(Arg0)");
	MBSTR timelimit (L"PT0S");

	VARIANT vtEmpty = {VT_EMPTY};

	if (SUCCEEDED (CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED)))
	{
		//if (SUCCEEDED (CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr)))
		{
			if (SUCCEEDED (CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID*)&service)))
			{
				if (SUCCEEDED (service->Connect (vtEmpty, vtEmpty, vtEmpty, vtEmpty)))
				{
					if (SUCCEEDED (service->GetFolder (root, &folder)))
					{
						// create task
						if (is_enable)
						{
							if (SUCCEEDED (service->NewTask (0, &task)))
							{
								if (SUCCEEDED (task->get_RegistrationInfo (&reginfo)))
								{
									reginfo->put_Author (author);
									reginfo->Release ();
								}

								if (SUCCEEDED (task->get_Principal (&principal)))
								{
									principal->put_RunLevel (TASK_RUNLEVEL_HIGHEST);
									principal->Release ();
								}

								if (SUCCEEDED (task->get_Settings (&settings)))
								{
									settings->put_AllowHardTerminate (VARIANT_BOOL (FALSE));
									settings->put_StartWhenAvailable (VARIANT_BOOL (FALSE));
									settings->put_DisallowStartIfOnBatteries (VARIANT_BOOL (FALSE));
									settings->put_StopIfGoingOnBatteries (VARIANT_BOOL (FALSE));
									settings->put_MultipleInstances (TASK_INSTANCES_PARALLEL);
									settings->put_ExecutionTimeLimit (timelimit);

									settings->Release ();
								}

								if (SUCCEEDED (task->get_Actions (&action_collection)))
								{
									if (SUCCEEDED (action_collection->Create (TASK_ACTION_EXEC, &action)))
									{
										if (SUCCEEDED (action->QueryInterface (IID_IExecAction, (LPVOID*)&exec_action)))
										{
											if (
												SUCCEEDED (exec_action->put_Path (path)) &&
												SUCCEEDED (exec_action->put_WorkingDirectory (directory)) &&
												SUCCEEDED (exec_action->put_Arguments (args))
												)
											{
												action_result = true;
											}

											exec_action->Release ();
										}

										action->Release ();
									}

									action_collection->Release ();
								}

								if (action_result)
								{
									if (SUCCEEDED (folder->RegisterTaskDefinition (
										name,
										task,
										TASK_CREATE_OR_UPDATE,
										vtEmpty,
										vtEmpty,
										TASK_LOGON_INTERACTIVE_TOKEN,
										vtEmpty,
										&registered_task)
										))
									{
										{
											//ConfigSet (L"SkipUacIsEnabled", true);
											result = true;

											registered_task->Release ();
										}
									}

									task->Release ();
								}
							}
						}
						else
						{
							// remove task
							result = SUCCEEDED (folder->DeleteTask (name, 0));

							//ConfigSet (L"SkipUacIsEnabled", false);
						}

						folder->Release ();
					}
				}

				service->Release ();
			}
		}

		CoUninitialize ();
	}

	return result;
}

bool SkipUacRun (bool test_only)
{
	bool result = false;

	ITaskService* service = nullptr;
	ITaskFolder* folder = nullptr;
	IRegisteredTask* registered_task = nullptr;

	ITaskDefinition* task = nullptr;
	IActionCollection* action_collection = nullptr;
	IAction* action = nullptr;
	IExecAction* exec_action = nullptr;

	IRunningTask* running_task = nullptr;

	wchar_t szPath[MAX_PATH];
	if (!GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)))
		return false;

	MBSTR root (L"\\");
	MBSTR name (SKIP_UAC_TASK_NAME);

	VARIANT vtEmpty = {VT_EMPTY};

	if (SUCCEEDED (CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED)))
	{
		//if (SUCCEEDED (CoInitializeSecurity (nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, 0, nullptr)))
		{
			if (SUCCEEDED (CoCreateInstance (CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskService, (LPVOID*)&service)))
			{
				if (SUCCEEDED (service->Connect (vtEmpty, vtEmpty, vtEmpty, vtEmpty)))
				{
					if (SUCCEEDED (service->GetFolder (root, &folder)))
					{
						if (SUCCEEDED (folder->GetTask (name, &registered_task)))
						{
							if (SUCCEEDED (registered_task->get_Definition (&task)))
							{
								if (SUCCEEDED (task->get_Actions (&action_collection)))
								{
									if (SUCCEEDED (action_collection->get_Item (1, &action)))
									{
										if (SUCCEEDED (action->QueryInterface (IID_IExecAction, (LPVOID*)&exec_action)))
										{
											BSTR path = nullptr;

											exec_action->get_Path (&path);

											PathUnquoteSpaces (path);

											// check path is to current module
											if (_wcsicmp (path, szPath) == 0)
											{
												if (test_only)
												{
													result = true;
												}
												else
												{
													std::wstring args;

													// get arguments
													{
														INT numargs = 0;
														LPWSTR* arga = CommandLineToArgvW(GetCommandLine(), &numargs);

														for (INT i = 1; i < numargs; i++) {
															if (i > 1)
																args.append(L" ");
															args.append(arga[i]);
														}

														LocalFree(arga);
													}

													variant_t params = args.c_str();

													if (SUCCEEDED(registered_task->RunEx(params, TASK_RUN_NO_FLAGS, 0, nullptr, &running_task)))
													{
														UINT8 count = 3; // try count

														do
														{
															QThread::msleep(250);

															TASK_STATE state = TASK_STATE_UNKNOWN;

															running_task->Refresh();
															running_task->get_State(&state);

															if (
																state == TASK_STATE_RUNNING ||
																state == TASK_STATE_READY ||
																state == TASK_STATE_DISABLED
																)
															{
																if (
																	state == TASK_STATE_RUNNING ||
																	state == TASK_STATE_READY
																	)
																{
																	result = true;
																}

																break;
															}
														} while (count--);

														running_task->Release();
													}
												}
											}

											exec_action->Release ();
										}

										action->Release ();
									}

									action_collection->Release ();
								}

								task->Release ();
							}

							registered_task->Release ();
						}

						folder->Release ();
					}
				}

				service->Release ();
			}
		}

		CoUninitialize ();
	}

	return result;
}
