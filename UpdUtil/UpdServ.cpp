/*
* Copyright 2025-2026 David Xanatos, xanasoft.com
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

#include "./Helpers/helpers.h"
#include "./Helpers/WebUtils.h"
#include "./Helpers/json/JSON.h"
#include "UpdDefines.h"
#include "UpdUtil.h"
#include "AppUtil.h"

// Variant support for pipe server mode
#include "../TaskExplorer/Common/Buffer.h"
#include "../TaskExplorer/Common/Variant.h"



//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Pipe Server Mode - Variant-based communication
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Async prepare state
struct SAsyncPrepare
{
	std::atomic<bool> active;
	std::atomic<bool> completed;
	std::atomic<int> result;
	std::wstring current_file;
	std::wstring error_msg;
	std::shared_ptr<SRelease> pFiles;
	std::wstring temp_dir;
	std::wstring base_dir;
	std::vector<std::wstring> file_filter;
	std::mutex mutex;
};

static std::shared_ptr<SAsyncPrepare> g_AsyncPrepare;

// Variant send/receive over pipe (protocol compatible with TaskHelper)
BOOLEAN SendCVariant(HANDLE hPipe, const CVariant& variant)
{
	// Serialize CVariant to CBuffer
	CBuffer buffer;
	variant.ToPacket(&buffer);

	// Send length prefix
	ULONG len = (ULONG)buffer.GetSize();
	DWORD bytesWritten;

	if (!WriteFile(hPipe, &len, sizeof(ULONG), &bytesWritten, NULL) || bytesWritten != sizeof(ULONG))
		return FALSE;

	// Send serialized data
	if (!WriteFile(hPipe, buffer.GetBuffer(), len, &bytesWritten, NULL) || bytesWritten != len)
		return FALSE;

	FlushFileBuffers(hPipe);
	return TRUE;
}

BOOLEAN RecvCVariant(HANDLE hPipe, CVariant& variant)
{
	// Read length prefix
	ULONG len = 0;
	DWORD bytesRead;

	if (!ReadFile(hPipe, &len, sizeof(ULONG), &bytesRead, NULL) || bytesRead != sizeof(ULONG))
		return FALSE;

	if (len == 0 || len > 100 * 1024 * 1024) // Sanity check: max 100MB
		return FALSE;

	// Allocate buffer for serialized data
	PVOID data = malloc(len);
	if (!data)
		return FALSE;

	// Read serialized data
	if (!ReadFile(hPipe, data, len, &bytesRead, NULL) || bytesRead != len)
	{
		free(data);
		return FALSE;
	}

	// Deserialize CBuffer to CVariant
	CBuffer buffer((byte*)data, len, true); // true = take ownership
	variant.FromPacket(&buffer, true);

	free(data);
	return TRUE;
}

// Async prepare worker thread
void AsyncPrepareWorker(std::shared_ptr<SAsyncPrepare> state)
{
	int ret = 0;

	// Download files
	for (auto I = state->pFiles->Map.begin(); I != state->pFiles->Map.end(); ++I)
	{
		if (I->second->State != SFile::eChanged)
			continue;

		{
			std::lock_guard<std::mutex> lock(state->mutex);
			state->current_file = I->second->Path;
		}

		auto path_name = SplitName(I->second->Path);
		if (!path_name.first.empty())
			CreateDirectoryTree(state->temp_dir, path_name.first);

		char* pData = NULL;
		ULONG uDataLen = 0;
		if (WebDownload(I->second->Url, &pData, &uDataLen))
		{
			ULONG hashSize;
			PVOID hash = NULL;
			if (NT_SUCCESS(MyHashBuffer(pData, uDataLen, &hash, &hashSize)))
			{
				std::wstring Hash = hexStr((unsigned char*)hash, hashSize);
				free(hash);

				if (I->second->Hash == Hash)
				{
					MyWriteFile((wchar_t*)(state->temp_dir + L"\\" + I->second->Path).c_str(), pData, uDataLen);
					I->second->State = SFile::ePending;
					ret++;
				}
				else
				{
					std::lock_guard<std::mutex> lock(state->mutex);
					state->error_msg = L"Hash mismatch: " + I->second->Path;
					free(pData);
					state->result = UPD_ERROR_HASH;
					state->completed = true;
					state->active = false;
					return;
				}
			}
			free(pData);
		}
		else
		{
			std::lock_guard<std::mutex> lock(state->mutex);
			state->error_msg = L"Download failed: " + I->second->Path;
			state->result = UPD_ERROR_DOWNLOAD;
			state->completed = true;
			state->active = false;
			return;
		}
	}

	std::lock_guard<std::mutex> lock(state->mutex);
	state->result = ret;
	state->completed = true;
	state->active = false;
}

// Main command processor for pipe server mode
CVariant ProcessUpdaterCommand(const CVariant& Request, const std::wstring& base_dir)
{
	CVariant Response;

	// Extract command and parameters
	if (Request.GetType() != VAR_TYPE_MAP)
	{
		Response.BeginMap();
		Response.Write("status", (sint32)UPD_ERROR_INVALID);
		Response.Write("error", "Invalid command format - expected map");
		Response.Finish();
		return Response;
	}

	CVariant cmdVar = Request.Find("Command");
	CVariant params = Request.Find("Parameters");

	if (!cmdVar.IsValid())
	{
		Response.BeginMap();
		Response.Write("status", (sint32)UPD_ERROR_INVALID);
		Response.Write("error", "Missing Command field");
		Response.Finish();
		return Response;
	}

	std::string command = cmdVar.ToString();

	//
	// GET command - download update information
	//
	if (command == "get")
	{
		std::wstring channel = params.IsValid() ? params.Find("channel").ToWString() : L"";
		std::wstring version = params.IsValid() ? params.Find("version").ToWString() : L"";
		std::wstring update_num = params.IsValid() ? params.Find("update").ToWString() : L"";
		std::wstring temp_dir = params.IsValid() ? params.Find("temp").ToWString() : L"";

		if (temp_dir.empty())
			temp_dir = GetDefaultTempDir();

		if (channel.empty() && version.empty())
		{
			Response.BeginMap();
			Response.Write("status", (sint32)UPD_ERROR_INVALID);
			Response.Write("error", "Either channel or version must be specified");
			Response.Finish();
			return Response;
		}

		// Download update JSON from server
		std::wstring arch = Arch2Str(GetSysArch());
		std::shared_ptr<SRelease> pFiles = DownloadUpdateFromServer(channel, version, update_num, arch, true);

		if (pFiles)
		{
			// Save to disk
			CreateDirectoryW(temp_dir.c_str(), NULL);
			std::wstring file_path = temp_dir + L"\\" _T(UPDATE_FILE);
			std::string json_str = WriteUpdate(pFiles);
			MyWriteFile((wchar_t*)file_path.c_str(), (char*)json_str.c_str(), (ULONG)json_str.length());

			// Return update info
			Response.BeginMap();
			Response.Write("status", (sint32)0);
			Response.Write("version", pFiles->Version);
			Response.Write("update", (sint32)pFiles->iUpdate);
			Response.Write("file_count", (uint32)pFiles->Map.size());
			Response.Write("temp_path", temp_dir);
			Response.Finish();
			return Response;
		}

		Response.BeginMap();
		Response.Write("status", (sint32)UPD_ERROR_GET);
		Response.Write("error", "Failed to download or verify update information");
		Response.Finish();
		return Response;
	}

	//
	// SCAN command - scan for changed files
	//
	else if (command == "scan")
	{
		std::wstring temp_dir = params.IsValid() ? params.Find("temp").ToWString() : L"";
		std::wstring app_path = params.IsValid() ? params.Find("path").ToWString() : base_dir;
		std::wstring channel = params.IsValid() ? params.Find("channel").ToWString() : L"";
		std::wstring version = params.IsValid() ? params.Find("version").ToWString() : L"";

		if (temp_dir.empty())
			temp_dir = GetDefaultTempDir();

		// Load or download update JSON
		std::shared_ptr<SRelease> pFiles = LoadOrDownloadUpdate(temp_dir, channel, version, L"");

		if (!pFiles)
		{
			Response.BeginMap();
			Response.Write("status", (sint32)UPD_ERROR_LOAD);
			Response.Write("error", channel.empty() && version.empty() ?
			               "No update.json found and no channel/version specified" :
			               "Failed to download update information");
			Response.Finish();
			return Response;
		}

		// Scan for changes
		int count = FindChanges(pFiles, app_path, temp_dir, nullptr);
		if (count < 0)
		{
			Response.BeginMap();
			Response.Write("status", (sint32)count);
			Response.Write("error", "Failed to scan for changes");
			Response.Finish();
			return Response;
		}

		// Build list of changed files
		CVariant fileList;
		fileList.BeginList();
		std::wstring files_txt;

		for (auto I = pFiles->Map.begin(); I != pFiles->Map.end(); ++I)
		{
			if (I->second->State == SFile::eChanged)
			{
				fileList.Write(I->second->Path);
				files_txt += I->second->Path + L"\n";
			}
		}
		fileList.Finish();

		// Save files.txt to temp
		std::wstring files_txt_path = temp_dir + L"\\files.txt";
		std::string files_txt_utf8 = g_str_conv.to_bytes(files_txt);
		MyWriteFile((wchar_t*)files_txt_path.c_str(), (char*)files_txt_utf8.c_str(), (ULONG)files_txt_utf8.length());

		// Return response
		Response.BeginMap();
		Response.Write("status", (sint32)0);
		Response.Write("changed_count", (sint32)count);
		Response.WriteVariant("files", fileList);
		Response.Write("files_txt_path", files_txt_path);
		Response.Finish();

		return Response;
	}

	//
	// PREPARE command - download changed files
	//
	else if (command == "prepare")
	{
		bool rescan = params.IsValid() ? params.Find("ReScan").To<bool>() : false;
		bool async = params.IsValid() ? params.Find("Async").To<bool>() : false;
		std::wstring temp_dir = params.IsValid() ? params.Find("temp").ToWString() : L"";
		std::wstring app_path = params.IsValid() ? params.Find("path").ToWString() : base_dir;
		std::wstring channel = params.IsValid() ? params.Find("channel").ToWString() : L"";
		std::wstring version = params.IsValid() ? params.Find("version").ToWString() : L"";

		if (temp_dir.empty())
			temp_dir = GetDefaultTempDir();

		// Load or download update JSON
		std::shared_ptr<SRelease> pFiles;
		if (rescan)
		{
			// Force download from server
			if (!channel.empty() || !version.empty())
			{
				std::wstring arch = Arch2Str(GetSysArch());
				pFiles = DownloadUpdateFromServer(channel, version, L"", arch, true);
			}
		}
		else
		{
			// Load from disk or download
			pFiles = LoadOrDownloadUpdate(temp_dir, channel, version, L"");
		}

		if (!pFiles)
		{
			Response.BeginMap();
			Response.Write("status", (sint32)UPD_ERROR_LOAD);
			Response.Write("error", channel.empty() && version.empty() ?
			               "No update.json found and no channel/version specified" :
			               "Failed to download update information");
			Response.Finish();
			return Response;
		}

		// Perform scan
		int scan_count = FindChanges(pFiles, app_path, temp_dir, nullptr);
		if (scan_count < 0)
		{
			Response.BeginMap();
			Response.Write("status", (sint32)scan_count);
			Response.Write("error", "Scan failed");
			Response.Finish();
			return Response;
		}

		if (scan_count == 0)
		{
			Response.BeginMap();
			Response.Write("status", (sint32)0);
			Response.Write("message", "No files to download");
			Response.Finish();
			return Response;
		}

		// Async mode - start background download
		if (async)
		{
			if (g_AsyncPrepare && g_AsyncPrepare->active)
			{
				Response.BeginMap();
				Response.Write("status", (sint32)UPD_ERROR_INTERNAL);
				Response.Write("error", "Async prepare already in progress");
				Response.Finish();
				return Response;
			}

			g_AsyncPrepare = std::make_shared<SAsyncPrepare>();
			g_AsyncPrepare->active = true;
			g_AsyncPrepare->completed = false;
			g_AsyncPrepare->result = 0;
			g_AsyncPrepare->pFiles = pFiles;
			g_AsyncPrepare->temp_dir = temp_dir;
			g_AsyncPrepare->base_dir = app_path;

			std::thread worker(AsyncPrepareWorker, g_AsyncPrepare);
			worker.detach();

			Response.BeginMap();
			Response.Write("status", (sint32)0);
			Response.Write("async", true);
			Response.Write("changed_count", (sint32)scan_count);
			Response.Write("message", "Download started in background");
			Response.Finish();
			return Response;
		}

		// Synchronous mode - download immediately
		int download_count = DownloadUpdate(temp_dir, pFiles);
		if (download_count < 0)
		{
			Response.BeginMap();
			Response.Write("status", (sint32)download_count);
			Response.Write("error", "Download failed");
			Response.Finish();
			return Response;
		}

		Response.BeginMap();
		Response.Write("status", (sint32)0);
		Response.Write("downloaded_count", (sint32)download_count);
		Response.Finish();
		return Response;
	}

	//
	// GET_STATUS command - check async prepare status
	//
	else if (command == "get_status")
	{
		if (!g_AsyncPrepare)
		{
			Response.BeginMap();
			Response.Write("status", (sint32)UPD_ERROR_INVALID);
			Response.Write("error", "No async operation in progress");
			Response.Finish();
			return Response;
		}

		std::lock_guard<std::mutex> lock(g_AsyncPrepare->mutex);

		Response.BeginMap();
		Response.Write("status", (sint32)0);
		Response.Write("active", g_AsyncPrepare->active.load());
		Response.Write("completed", g_AsyncPrepare->completed.load());

		if (g_AsyncPrepare->completed)
		{
			int result = g_AsyncPrepare->result.load();
			if (result < 0)
			{
				Response.Write("result", (sint32)result);
				Response.Write("error", g_AsyncPrepare->error_msg);
			}
			else
			{
				Response.Write("result", (sint32)result);
				Response.Write("downloaded_count", (sint32)result);
			}
		}
		else if (g_AsyncPrepare->active)
		{
			Response.Write("current_file", g_AsyncPrepare->current_file);
		}

		Response.Finish();
		return Response;
	}

	//
	// DOWNLOAD command - download single file
	//
	else if (command == "download")
	{
		std::wstring url_or_path = params.IsValid() ? params.Find("url").ToWString() : L"";
		std::wstring save_path = params.IsValid() ? params.Find("save_path").ToWString() : L"";
		bool return_content = params.IsValid() ? params.Find("return_content").To<bool>() : false;

		if (url_or_path.empty())
		{
			Response.BeginMap();
			Response.Write("status", (sint32)UPD_ERROR_INVALID);
			Response.Write("error", "Missing url parameter");
			Response.Finish();
			return Response;
		}

		// Determine if it's a URL or package path
		std::wstring download_url;
		if (url_or_path[0] == L'\\')
		{
			// Resolve package path to URL
			std::wstring temp_dir = GetDefaultTempDir();
			download_url = ResolvePackagePath(url_or_path, temp_dir);

			if (download_url.empty())
			{
				Response.BeginMap();
				Response.Write("status", (sint32)UPD_ERROR_INVALID);
				Response.Write("error", "File not found in update package");
				Response.Finish();
				return Response;
			}
		}
		else
		{
			download_url = url_or_path;
		}

		// Download file
		char* pData = NULL;
		ULONG uDataLen = 0;

		if (WebDownload(download_url, &pData, &uDataLen))
		{
			// Save to disk if requested
			if (!save_path.empty())
			{
				MyWriteFile((wchar_t*)save_path.c_str(), pData, uDataLen);
			}

			// Return content if requested
			if (return_content)
			{
				CVariant content((byte*)pData, uDataLen);

				Response.BeginMap();
				Response.Write("status", (sint32)0);
				Response.Write("size", (uint32)uDataLen);
				Response.WriteVariant("content", content);
				if (!save_path.empty())
					Response.Write("save_path", save_path);
				Response.Finish();
			}
			else
			{
				Response.BeginMap();
				Response.Write("status", (sint32)0);
				Response.Write("size", (uint32)uDataLen);
				if (!save_path.empty())
					Response.Write("save_path", save_path);
				Response.Finish();
			}

			free(pData);
			return Response;
		}

		Response.BeginMap();
		Response.Write("status", (sint32)UPD_ERROR_DOWNLOAD);
		Response.Write("error", "Download failed");
		Response.Finish();
		return Response;
	}

	// Unknown command
	Response.BeginMap();
	Response.Write("status", (sint32)UPD_ERROR_INVALID);
	Response.Write("error", "Unknown command");
	Response.Finish();
	return Response;
}

// Client handler thread for pipe server
DWORD WINAPI ClientHandlerThread(LPVOID lpParam)
{
	HANDLE hPipe = (HANDLE)lpParam;

	wchar_t szPath[MAX_PATH];
	GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath));
	*wcsrchr(szPath, L'\\') = L'\0';
	std::wstring base_dir = szPath;

	// Process client requests
	while (TRUE)
	{
		// Read request CVariant
		CVariant request;
		if (!RecvCVariant(hPipe, request))
		{
			break;
		}

		// Process the command
		CVariant response = ProcessUpdaterCommand(request, base_dir);

		// Send response CVariant
		if (!SendCVariant(hPipe, response))
		{
			break;
		}

		// Check for quit command
		if (request.GetType() == VAR_TYPE_MAP)
		{
			CVariant cmdVar = request.Find("Command");
			if (cmdVar.IsValid() && cmdVar.ToString() == "Quit")
			{
				break;
			}
		}
	}

	DisconnectNamedPipe(hPipe);
	CloseHandle(hPipe);

	return 0;
}

// Run pipe server
void RunPipeServer(const std::wstring& pipeName)
{
	std::wcout << L"Starting pipe server: " << pipeName << std::endl;

	while (TRUE)
	{
		std::wstring fullPipeName = L"\\\\.\\pipe\\" + pipeName;

		// Create named pipe
		HANDLE hPipe = CreateNamedPipeW(
			fullPipeName.c_str(),
			PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			1, // Only 1 instance
			4096,
			4096,
			0,
			NULL
		);

		if (hPipe == INVALID_HANDLE_VALUE)
		{
			std::wcout << L"Failed to create pipe" << std::endl;
			Sleep(1000);
			continue;
		}

		// Wait for client connection
		if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED)
		{
			std::wcout << L"Client connected" << std::endl;

			// Handle client in same thread (simple mode)
			ClientHandlerThread(hPipe);

			std::wcout << L"Client disconnected" << std::endl;
		}
		else
		{
			CloseHandle(hPipe);
		}
	}
}