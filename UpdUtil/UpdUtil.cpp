/*
 * Copyright 2022-2026 David Xanatos, xanasoft.com
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
#include <shellapi.h>
#include <io.h>
#include <fcntl.h>
#include <aclapi.h>

#include "./Helpers/helpers.h"
#include "./Helpers/WebUtils.h"
#include "./Helpers/json/JSON.h"
#include "UpdDefines.h"
#include "UpdUtil.h"
#include "AppUtil.h"

//
// Architecture conversion
//

std::wstring Arch2Str(ULONG architecture)
{
	switch (architecture)
	{
	case IMAGE_FILE_MACHINE_ARM64:	return L"a64";
	case IMAGE_FILE_MACHINE_AMD64:	return L"x64";
	case IMAGE_FILE_MACHINE_I386:	return L"x86";
	default: return L"";
	}
}

extern "C"
{
NTSYSCALLAPI NTSTATUS NTAPI NtQuerySystemInformationEx(
    _In_ SYSTEM_INFORMATION_CLASS SystemInformationClass,
    _In_reads_bytes_(InputBufferLength) PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_writes_bytes_opt_(SystemInformationLength) PVOID SystemInformation,
    _In_ ULONG SystemInformationLength,
    _Out_opt_ PULONG ReturnLength
    );
}

ULONG GetSysArch()
{
    USHORT architecture = 0;
    NTSTATUS status;
	HANDLE ProcessHandle = GetCurrentProcess();
	ULONG bufferLength;
	SYSTEM_SUPPORTED_PROCESSOR_ARCHITECTURES_INFORMATION* buffer;
    ULONG returnLength;

	bufferLength = sizeof(SYSTEM_SUPPORTED_PROCESSOR_ARCHITECTURES_INFORMATION[5]);
	buffer = (SYSTEM_SUPPORTED_PROCESSOR_ARCHITECTURES_INFORMATION*)malloc(bufferLength);

	const ULONG SystemSupportedProcessorArchitectures = 181;
    status = NtQuerySystemInformationEx((SYSTEM_INFORMATION_CLASS)SystemSupportedProcessorArchitectures, &ProcessHandle, sizeof(ProcessHandle), buffer, bufferLength, &returnLength);

    if (NT_SUCCESS(status))
    {
        for (ULONG i = 0; i < returnLength / sizeof(SYSTEM_SUPPORTED_PROCESSOR_ARCHITECTURES_INFORMATION); i++)
        {
            if (buffer[i].Native)
            {
                architecture = (USHORT)buffer[i].Machine;
                break;
            }
        }
    }
    else // windows 7 fallback
    {
        SYSTEM_INFO SystemInfo = {0};
        GetNativeSystemInfo(&SystemInfo);
        switch (SystemInfo.wProcessorArchitecture)
        {
        case PROCESSOR_ARCHITECTURE_AMD64:  architecture = IMAGE_FILE_MACHINE_AMD64; break;
        case PROCESSOR_ARCHITECTURE_ARM64:  architecture = IMAGE_FILE_MACHINE_ARM64; break;
        case PROCESSOR_ARCHITECTURE_INTEL:  architecture = IMAGE_FILE_MACHINE_I386; break;
        }
    }

    free(buffer);

	return architecture;
}

//
// Registry helpers
//

std::wstring ReadRegistryStringValue(std::wstring key, const std::wstring& valueName)
{
	auto RootPath = Split2(key, L"\\");

	HKEY hKey;
    if (_wcsicmp(RootPath.first.c_str(), L"HKEY_LOCAL_MACHINE") == 0)
        hKey = HKEY_LOCAL_MACHINE;
    else if (_wcsicmp(RootPath.first.c_str(), L"HKEY_CURRENT_USER") == 0)
        hKey = HKEY_CURRENT_USER;
    else
        return L"";

    if (RegOpenKeyEx(hKey, RootPath.second.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
        wchar_t valueData[0x1000] = L"";
        DWORD dataSize = sizeof(valueData);
        DWORD valueType;

		RegQueryValueExW(hKey, valueName.c_str(), NULL, &valueType, (LPBYTE)valueData, &dataSize);

        RegCloseKey(hKey);

		return valueData;
    }
	return L"";
}

//
// Binary/file helpers
//

ULONG GetBinaryArch(const std::wstring& file)
{
	ULONG arch = 0;

    HANDLE hFile = CreateFile(file.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
		goto finish;

    IMAGE_DOS_HEADER dosHeader;
    DWORD bytesRead;
    if (!ReadFile(hFile, &dosHeader, sizeof(IMAGE_DOS_HEADER), &bytesRead, NULL) || bytesRead != sizeof(IMAGE_DOS_HEADER))
        goto finish;

    if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
        goto finish;

    SetFilePointer(hFile, dosHeader.e_lfanew, NULL, FILE_BEGIN);
    IMAGE_NT_HEADERS ntHeader;
    if (!ReadFile(hFile, &ntHeader, sizeof(IMAGE_NT_HEADERS), &bytesRead, NULL) || bytesRead != sizeof(IMAGE_NT_HEADERS))
        goto finish;

    if (ntHeader.Signature != IMAGE_NT_SIGNATURE)
        goto finish;

	arch = ntHeader.FileHeader.Machine;

finish:
    if(hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);

	return arch;
}

std::wstring GetFileVersion(const std::wstring& file)
{
	LPVOID versionInfo = NULL;
	VS_FIXEDFILEINFO *fixedFileInfo = NULL;
    UINT fixedFileInfoSize;

    DWORD versionHandle;
    DWORD versionSize = GetFileVersionInfoSizeW(file.c_str(), &versionHandle);
	if (versionSize == 0)
		goto finish;

	versionInfo = malloc(versionSize);

    if (!GetFileVersionInfo(file.c_str(), versionHandle, versionSize, versionInfo))
		goto finish;

    if (!VerQueryValue(versionInfo, L"\\", (LPVOID *)&fixedFileInfo, &fixedFileInfoSize))
		goto finish;

finish:
	std::wstring version;

	if (fixedFileInfo)
	{
		DWORD fileVersionMS = fixedFileInfo->dwFileVersionMS;
		DWORD fileVersionLS = fixedFileInfo->dwFileVersionLS;

		WORD majorVersion = HIWORD(fileVersionMS);
		WORD minorVersion = LOWORD(fileVersionMS);
		WORD buildNumber = HIWORD(fileVersionLS);
		WORD revisionNumber = LOWORD(fileVersionLS);

		version = std::to_wstring(majorVersion) + L"." + std::to_wstring(minorVersion) + L"." + std::to_wstring(buildNumber);
		if(revisionNumber) version += L"." + std::to_wstring(revisionNumber);
	}

	if (versionInfo)free(versionInfo);

	return version;
}

//
// Directory helpers
//

void CreateDirectoryTree(const std::wstring& root, const std::wstring& path)
{
    wchar_t* pathCopy = _wcsdup(path.c_str());
    wchar_t* token = _wcstok(pathCopy, L"\\");

    wchar_t currentPath[MAX_PATH] = L"";
	wcscpy(currentPath, root.c_str());

	for (; token != NULL;) {

		wcscat(currentPath, L"\\");
		wcscat(currentPath, token);

		if (CreateDirectoryW(currentPath, NULL) == 0) {
			if (GetLastError() != ERROR_ALREADY_EXISTS) {
				printf("Error creating directory %S\n", currentPath);
			}
		}

		token = _wcstok(NULL, L"\\");
	}

    free(pathCopy);
}

std::pair<std::wstring, std::wstring> SplitName(std::wstring path)
{
	std::wstring name;
	size_t pos = path.find_last_of(L"\\");
	if (pos == -1) {
		name = path;
		path.clear();
	}
	else {
		name = path.substr(pos + 1);
		path.erase(pos);
		path = L"\\" + path;
	}
	return std::make_pair(path, name);
}

bool DeleteDirectoryRecursively(const std::wstring& root, const std::wstring& path, bool bWithFiles)
{
	std::vector<std::wstring> Entries;
	ListDir(root + path, Entries);

	for (int i = 0; i < Entries.size(); i++)
	{
		auto path_name = SplitName(Entries[i]);
		if (path_name.second.empty()) {
			if (!DeleteDirectoryRecursively(Entries[i], L"", bWithFiles))
				return false;
		}
		else if (bWithFiles) {
			if (!DeleteFileW(Entries[i].c_str()))
				return false;
		}
		else
			return false;
	}

	return !!RemoveDirectoryW((root + path).c_str());
}

//
// Directory scanning
//

std::shared_ptr<SRelease> ScanDir(std::wstring Path)
{
	if (Path.back() != L'\\')
		Path.push_back(L'\\');

	std::shared_ptr<SRelease> pFiles = std::make_shared<SRelease>();

	std::vector<std::wstring> Entries;
	Entries.push_back(Path);

	for (int i = 0; i < Entries.size(); i++)
	{
		if (Entries[i].back() == '\\') {
			ListDir(Entries[i], Entries);
			continue;
		}

		ULONG hashSize;
		PVOID hash = NULL;
		if (NT_SUCCESS(MyHashFile(Entries[i].c_str(), &hash, &hashSize)))
		{
			std::shared_ptr<SFile> pFile = std::make_shared<SFile>();
			pFile->Path = Entries[i].substr(Path.length());
			pFile->Hash = hexStr((unsigned char*)hash, hashSize);
			pFiles->Map[pFile->Path] = pFile;

			free(hash);
		}
	}

	return pFiles;
}

//
// JSON helper functions
//

std::wstring GetJSONStringSafe(const JSONObject& root, const std::wstring& key, const std::wstring& def)
{
	auto I = root.find(key);
	if (I == root.end() || !I->second->IsString())
		return def;
	return I->second->AsString();
}

int GetJSONIntSafe(const JSONObject& root, const std::wstring& key, int def)
{
	auto I = root.find(key);
	if (I == root.end() || !I->second->IsNumber())
		return def;
	return (int)I->second->AsNumber();
}

int GetJSONBoolSafe(const JSONObject& root, const std::wstring& key, bool def)
{
	auto I = root.find(key);
	if (I == root.end() || !I->second->IsBool())
		return def;
	return I->second->AsBool();
}

JSONObject GetJSONObjectSafe(const JSONObject& root, const std::wstring& key)
{
	auto I = root.find(key);
	if (I == root.end() || !I->second->IsObject())
		return JSONObject();
	return I->second->AsObject();
}

JSONArray GetJSONArraySafe(const JSONObject& root, const std::wstring& key)
{
	auto I = root.find(key);
	if (I == root.end() || !I->second->IsArray())
		return JSONArray();
	return I->second->AsArray();
}

//
// Update JSON serialization
//

std::string WriteUpdate(std::shared_ptr<SRelease> pFiles)
{
	JSONObject root;

	JSONArray files;
	for (auto I = pFiles->Map.begin(); I != pFiles->Map.end(); ++I)
	{
		JSONObject file;
		file[L"path"] = new JSONValue(I->second->Path);
		file[L"hash"] = new JSONValue(I->second->Hash);
		file[L"url"] = new JSONValue(I->second->Url);
		files.push_back(new JSONValue(file));
	}
	root[L"files"] = new JSONValue(files);

	root[L"signature"] = new JSONValue(pFiles->Sign);

	root[L"version"] = new JSONValue(pFiles->Version);
	root[L"update"] = new JSONValue(pFiles->iUpdate);

	root[L"ci"] = new JSONValue(pFiles->CI);

	root[L"framework"] = new JSONValue(pFiles->Framework);
	root[L"agent_arch"] = new JSONValue(pFiles->AgentArch);

	JSONValue *value = new JSONValue(root);
	auto wJson = value->Stringify();
	delete value;

	return g_str_conv.to_bytes(wJson);
}

void ReadFiles(const JSONArray& jsonFiles, std::shared_ptr<SFiles> pFiles)
{
	for (auto I = jsonFiles.begin(); I != jsonFiles.end(); ++I) {
		if ((*I)->IsObject()) {
			JSONObject jsonFile = (*I)->AsObject();

			std::shared_ptr<SFile> pFile = std::make_shared<SFile>();
			pFile->Path = GetJSONStringSafe(jsonFile, L"path");
			pFile->Hash = GetJSONStringSafe(jsonFile, L"hash");
			pFile->Url = GetJSONStringSafe(jsonFile, L"url");
			pFiles->Map[pFile->Path] = pFile;
		}
	}
}

std::shared_ptr<SRelease> ReadUpdate(const JSONObject& jsonObject)
{
	if (jsonObject.find(L"files") == jsonObject.end())
		return std::shared_ptr<SRelease>();

	std::shared_ptr<SRelease> pFiles = std::make_shared<SRelease>();

	JSONArray jsonFiles = GetJSONArraySafe(jsonObject, L"files");
	ReadFiles(jsonFiles, pFiles);

	pFiles->Sign = GetJSONStringSafe(jsonObject, L"signature");

	pFiles->Version = GetJSONStringSafe(jsonObject, L"version");
	pFiles->iUpdate = GetJSONIntSafe(jsonObject, L"update", -1);

	pFiles->CI = GetJSONStringSafe(jsonObject, L"ci");

	pFiles->Framework = GetJSONStringSafe(jsonObject, L"framework");
	pFiles->AgentArch = GetJSONStringSafe(jsonObject, L"agent_arch");

	return pFiles;
}

//
// Update verification
//

bool VerifyUpdate(std::shared_ptr<SFiles> pFiles)
{
	bool pass = false;

	std::set<std::wstring> hash_set;
	for (auto I = pFiles->Map.begin(); I != pFiles->Map.end(); ++I)
		hash_set.insert(I->second->Path + L":" + I->second->Hash);

	std::string hashes;
	for (auto I = hash_set.begin(); I != hash_set.end(); ++I)
		hashes += g_str_conv.to_bytes(*I) + "\n";

	ULONG hashSize;
	PVOID hash = NULL;
	if (NT_SUCCESS(MyHashBuffer((void*)hashes.c_str(), hashes.length(), &hash, &hashSize)))
	{
		ULONG signatureSize = (ULONG)b64_decoded_size(pFiles->Sign.c_str());
		if (signatureSize)
		{
			PUCHAR signature = (PUCHAR)malloc(signatureSize);
			b64_decode(pFiles->Sign.c_str(), signature, signatureSize);

			if (NT_SUCCESS(VerifyHashSignature((PUCHAR)hash, hashSize, (PUCHAR)signature, signatureSize)))
			{
				pass = true;
			}

			free(signature);
		}

		free(hash);
	}

	return pass;
}

//
// File filter helper
//

bool InFileFilter(const std::vector<std::wstring>* pFileFilter, const std::wstring& name)
{
	if (!pFileFilter) return true;

	// Check if name matches any of the filter entries
	for (auto I = pFileFilter->begin(); I != pFileFilter->end(); ++I) {
		if (_wcsicmp(I->c_str(), name.c_str()) == 0)
			return true;
	}

	return false;
}

//
// Shared helper functions
//

std::wstring GetDefaultTempDir()
{
	wchar_t szTemp[MAX_PATH];
	GetTempPath(MAX_PATH, szTemp);
	return std::wstring(szTemp) + GetSoftware() + L"-Updater";
}

std::shared_ptr<SRelease> LoadUpdateFromDisk(const std::wstring& temp_dir)
{
	std::shared_ptr<SRelease> pFiles;
	char* aJson = NULL;
	std::wstring file_path = temp_dir + L"\\" _T(UPDATE_FILE);

	if (NT_SUCCESS(MyReadFile((wchar_t*)file_path.c_str(), 1024 * 1024, (PVOID*)&aJson, NULL)) && aJson != NULL)
	{
		JSONValue* jsonObject = JSON::Parse(aJson);
		if (jsonObject && jsonObject->IsObject())
		{
			pFiles = ReadUpdate(jsonObject->AsObject());
			delete jsonObject;
		}
		free(aJson);
	}

	return pFiles;
}

std::wstring BuildUpdateUrlParams(const std::wstring& channel, const std::wstring& version,
                                   const std::wstring& update_num, const std::wstring& arch)
{
	std::wstringstream params_str;
	params_str << L"&action=update";

	if (!channel.empty())
		params_str << L"&channel=" << channel;
	if (!version.empty())
	{
		params_str << L"&version=" << version;
		if (!update_num.empty())
			params_str << L"&update=" << update_num;
	}

	if (!arch.empty())
		params_str << L"&system=windows-" << arch;

	return params_str.str();
}

std::shared_ptr<SRelease> DownloadUpdateFromServer(const std::wstring& channel,
                                                     const std::wstring& version,
                                                     const std::wstring& update_num,
                                                     const std::wstring& arch,
                                                     bool verify)
{
	std::shared_ptr<SRelease> pFiles;

	std::wstring params = BuildUpdateUrlParams(channel, version, update_num, arch);
	std::wstring url_path = L"/update.php?software=" + GetSoftware() + params;

	char* aJson = NULL;
	if (WebDownload(_T(UPDATE_DOMAIN), url_path.c_str(), &aJson, NULL) && aJson != NULL)
	{
		JSONValue* jsonObject = JSON::Parse(aJson);
		if (jsonObject && jsonObject->IsObject())
		{
			JSONObject root = jsonObject->AsObject();
			JSONObject update_obj = GetJSONObjectSafe(root, version.empty() ? L"release" : L"update");
			pFiles = ReadUpdate(update_obj);

			// Verify signature if requested
			if (pFiles && verify && !VerifyUpdate(pFiles))
			{
				pFiles.reset();
			}

			delete jsonObject;
		}
		free(aJson);
	}

	return pFiles;
}

std::shared_ptr<SRelease> LoadOrDownloadUpdate(const std::wstring& temp_dir,
                                                 const std::wstring& channel,
                                                 const std::wstring& version,
                                                 const std::wstring& update_num)
{
	// Try to load from disk first
	std::shared_ptr<SRelease> pFiles = LoadUpdateFromDisk(temp_dir);

	// If not found and channel/version provided, download from server
	if (!pFiles && (!channel.empty() || !version.empty()))
	{
		std::wstring arch = Arch2Str(GetSysArch());
		pFiles = DownloadUpdateFromServer(channel, version, update_num, arch, true);
	}

	return pFiles;
}

std::wstring ResolvePackagePath(const std::wstring& package_path, const std::wstring& temp_dir)
{
	std::wstring download_url;

	if (package_path.empty() || package_path[0] != L'\\')
		return download_url;

	std::shared_ptr<SRelease> pFiles = LoadUpdateFromDisk(temp_dir);
	if (pFiles)
	{
		// Find file in package (remove leading backslash)
		std::wstring search_path = package_path.substr(1);
		auto I = pFiles->Map.find(search_path);
		if (I != pFiles->Map.end())
		{
			download_url = I->second->Url;
		}
	}

	return download_url;
}

//
// Update operations
//

int FindChanges(std::shared_ptr<SRelease> pNewFiles, std::wstring base_dir, std::wstring temp_dir, const std::vector<std::wstring>* pFileFilter)
{
	std::shared_ptr<SRelease> pOldFiles = ScanDir(base_dir);
	if (!pOldFiles)
		return UPD_ERROR_SCAN;

	int Count = 0;
	for (auto I = pNewFiles->Map.begin(); I != pNewFiles->Map.end(); ++I)
	{
		I->second->State = SFile::eNone;
		if (!InFileFilter(pFileFilter, I->second->Path))
			continue;

		auto J = pOldFiles->Map.find(I->first);
		if (J == pOldFiles->Map.end() || J->second->Hash != I->second->Hash) {
			I->second->State = SFile::eChanged;
			Count++;
		}
	}

	return Count;
}

BOOLEAN WebDownload(std::wstring url, PSTR* pData, ULONG* pDataLength)
{
	size_t pos = url.find_first_of(L'/', 8);
	if (pos == std::wstring::npos)
		return FALSE;
	std::wstring path = url.substr(pos);
	std::wstring domain = url.substr(8, pos-8);

	return WebDownload(domain.c_str(), path.c_str(), pData, pDataLength);
}

int DownloadUpdate(std::wstring temp_dir, std::shared_ptr<SFiles> pNewFiles)
{
	std::wcout << L"Downloading" << std::endl;

	int Count = 0;
	for (auto I = pNewFiles->Map.begin(); I != pNewFiles->Map.end(); ++I)
	{
		if (I->second->State != SFile::eChanged)
			continue;
		Count++;

		auto path_name = SplitName(I->second->Path);

		if (!path_name.first.empty())
			CreateDirectoryTree(temp_dir, path_name.first);

		ULONG hashSize;
		PVOID hash = NULL;
		if (NT_SUCCESS(MyHashFile((wchar_t*)(temp_dir + L"\\" + I->second->Path).c_str(), &hash, &hashSize)))
		{
			std::wstring Hash = hexStr((unsigned char*)hash, hashSize);
			free(hash);

			if (I->second->Hash == Hash) {
				I->second->State = SFile::ePending;
				continue; // already downloaded and up to date
			}
		}

		std::wcout << L"\tDownloading: " << I->second->Path << L" ...";

		char* pData = NULL;
		ULONG uDataLen = 0;
		if (WebDownload(I->second->Url, &pData, &uDataLen)) {

			ULONG hashSize;
			PVOID hash = NULL;
			if(NT_SUCCESS(MyHashBuffer(pData, uDataLen, &hash, &hashSize)))
			{
				std::wstring Hash = hexStr((unsigned char*)hash, hashSize);
				free(hash);

				if (I->second->Hash != Hash)
				{
					free(pData);
					std::wcout << L" BAD!!!" << std::endl;
					return UPD_ERROR_HASH;
				}

				MyWriteFile((wchar_t*)(temp_dir + L"\\" + I->second->Path).c_str(), pData, uDataLen);
				I->second->State = SFile::ePending;
			}

			free(pData);
			std::wcout << L" done" << std::endl;
		}
		else {

			std::wcout << L" FAILED" << std::endl;

			return UPD_ERROR_DOWNLOAD;
		}
	}

	return Count;
}

bool BackupCurrent(std::wstring base_dir, std::wstring backup_dir, std::shared_ptr<SFiles> pNewFiles)
{
	std::wcout << L"Backing up existing files..." << std::endl;
	for (auto I = pNewFiles->Map.begin(); I != pNewFiles->Map.end(); ++I)
	{
		if (I->second->State != SFile::ePending)
			continue;
		if(_wcsicmp(I->second->Path.c_str(), L"UpdUtil.exe") == 0)
			continue; // don't try overwriting ourselves

		std::wstring src = base_dir + L"\\" + I->second->Path;
		std::wstring backup = backup_dir + L"\\" + I->second->Path;

		// Check if source file exists
		if (GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES)
			continue; // file doesn't exist yet, skip backup

		auto path_name = SplitName(I->second->Path);
		if (!path_name.first.empty())
			CreateDirectoryTree(backup_dir, path_name.first);

		// Move file to backup
		if (!MoveFileExW(src.c_str(), backup.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
			std::wcout << L"Failed to backup: " << I->second->Path << std::endl;

			// On failure, restore the move
			std::wcout << L"Reverting failed backup atempt..." << std::endl;
			for (auto J = pNewFiles->Map.begin(); J != I; ++J) {
				if (J->second->State != SFile::ePending)
					continue;
				std::wstring orig = base_dir + L"\\" + J->second->Path;
				std::wstring bak = backup_dir + L"\\" + J->second->Path;
				MoveFileExW(bak.c_str(), orig.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
			}
			return false;
		}
	}
	return true;
}

bool RestoreBackup(std::wstring base_dir, std::wstring backup_dir, std::shared_ptr<SFiles> pNewFiles)
{
	std::wcout << L"Restoring from backup..." << std::endl;
	for (auto J = pNewFiles->Map.begin(); J != pNewFiles->Map.end(); ++J) {
		if (J->second->State != SFile::ePending)
			continue;
		std::wstring orig = base_dir + L"\\" + J->second->Path;
		std::wstring bak = backup_dir + L"\\" + J->second->Path;
		if (GetFileAttributesW(bak.c_str()) != INVALID_FILE_ATTRIBUTES) {
			DeleteFileW(orig.c_str());
			MoveFileExW(bak.c_str(), orig.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
		}
	}
	return true;
}

int ApplyUpdate(std::wstring base_dir, std::wstring temp_dir, std::wstring backup_dir, std::shared_ptr<SFiles> pNewFiles)
{
	std::wcout << L"Applying updates" << std::endl;

	// Prepare: Backup all files to be replaced
	if (!BackupCurrent(base_dir, backup_dir, pNewFiles))
		return UPD_ERROR_INTERNAL;

	// Apply: Copy new files
	std::wcout << L"Installing new files..." << std::endl;
	int Count = 0;
	for (auto I = pNewFiles->Map.begin(); I != pNewFiles->Map.end(); ++I)
	{
		if (I->second->State != SFile::ePending)
			continue;
		if(_wcsicmp(I->second->Path.c_str(), L"UpdUtil.exe") == 0)
			continue; // don't try overwriting ourselves
		Count++;

		auto path_name = SplitName(I->second->Path);

		std::wcout << L"\tInstalling: " << I->second->Path << L" ...";

		std::wstring src = temp_dir + L"\\" + I->second->Path;
		std::wstring dest = base_dir + L"\\" + I->second->Path;

		if (!path_name.first.empty())
			CreateDirectoryTree(base_dir, path_name.first);

		if (CopyFileW(src.c_str(), dest.c_str(), FALSE)) {

			// inherit parent folder permissions
			ACL g_null_acl = { 0 };
			InitializeAcl(&g_null_acl, sizeof(g_null_acl), ACL_REVISION);
			DWORD error = SetNamedSecurityInfoW((wchar_t*)dest.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | UNPROTECTED_DACL_SECURITY_INFORMATION, NULL, NULL, (PACL)&g_null_acl, NULL);

			std::wcout << L" done" << std::endl;
		} else {
			std::wcout << L" FAILED" << std::endl;

			// Restore from backup
			RestoreBackup(base_dir, backup_dir, pNewFiles);
			return UPD_ERROR_INTERNAL;
		}
	}

	// Cleanup: delete backup
	std::wcout << L"Cleaning up backup..." << std::endl;
	DeleteDirectoryRecursively(backup_dir, L"", true);

	return Count;
}

void Execute(std::wstring wFile, std::wstring wParams)
{
	SHELLEXECUTEINFO si = { sizeof(SHELLEXECUTEINFO) };
	si.fMask = SEE_MASK_NOCLOSEPROCESS;
	si.lpVerb = L"runas";
	si.lpFile = wFile.c_str();
	si.lpParameters = wParams.c_str();
	si.nShow = SW_HIDE;

	std::wcout << wFile.c_str() << L" " << si.lpParameters << std::endl;
	if (ShellExecuteEx(&si)) {
		WaitForSingleObject(si.hProcess, INFINITE);
		CloseHandle(si.hProcess);
	}
}

//
// Utility functions
//

int DownloadFile(std::wstring url, std::wstring file_path)
{
	char* pData = NULL;
	ULONG uDataLen = 0;

	if (WebDownload(url, &pData, &uDataLen)) {

		MyWriteFile((wchar_t*)file_path.c_str(), pData, uDataLen);

		free(pData);
		std::wcout << L" done" << std::endl;
		return 0;
	}
	std::wcout << L" FAILED" << std::endl;
	return UPD_ERROR_DOWNLOAD;
}

int PrintFile(std::wstring url)
{
	char* pData = NULL;
	ULONG uDataLen = 0;

	if (WebDownload(url, &pData, &uDataLen)) {

		std::wcout << pData;
		free(pData);
		return 0;
	}
	return UPD_ERROR_DOWNLOAD;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEV Functionality - Commit, Sign, Upload
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UploadFileToServer(std::wstring path, std::wstring name, const VOID* pData, ULONG DataLength, std::string apiKey)
{
	std::map<std::string, std::string> params;
	params["dest"] = g_str_conv.to_bytes(path);
	params["apikey"] = apiKey;

	return !!WebUpload(_T(UPDATE_DOMAIN), L"/update/updater.php", name.c_str(), (PSTR*)pData, DataLength, NULL, NULL, params);
}

int UploadUpdate(std::wstring software, std::wstring arch, std::wstring version, std::wstring sign, std::wstring base_dir, const std::vector<std::wstring>* pFileFilter, PVOID key, ULONG keySize, std::string apiKey)
{
	std::shared_ptr<SRelease> pOldFiles;

	char* aJson = NULL;
	std::wstring path = L"/update.php?software=" + software + L"&action=commit&system=windows-" + arch + L"&version=" + version;
	if (WebDownload(_T(UPDATE_DOMAIN), path.c_str(), &aJson, NULL) && aJson != NULL)
	{
		JSONValue* jsonObject = JSON::Parse(aJson);
		if (jsonObject) {
			if (jsonObject->IsObject()) {
				JSONObject update = GetJSONObjectSafe(jsonObject->AsObject(), L"update");
				pOldFiles = ReadUpdate(update);
			}
			delete jsonObject;
		}
		free(aJson);
	}

	int iUpdate = pOldFiles ? pOldFiles->iUpdate + 1 : 0;

	std::wcout << L"Uploading " << software << L" v" << version << L"-" << iUpdate << std::endl;

	std::shared_ptr<SRelease> pNewFiles = ScanDir(base_dir);

	std::wstring upload_path = software + L"/" + version + L"/" + arch + L"/" + std::to_wstring(iUpdate);

	std::set<std::wstring> hash_set;

	JSONObject root;
	JSONArray files;
	for (auto I = pNewFiles->Map.begin(); I != pNewFiles->Map.end(); ++I)
	{
		// Use file filter if specified
		if (!InFileFilter(pFileFilter, I->second->Path))
			continue;

		hash_set.insert(I->second->Path + L":" + I->second->Hash);

		auto path_name = SplitName(I->second->Path);
		std::replace(path_name.first.begin(), path_name.first.end(), '\\', '/');

		std::wstring url;

		if (pOldFiles) {
			auto J = pOldFiles->Map.find(I->first);
			if (J != pOldFiles->Map.end() && J->second->Hash == I->second->Hash) {
				std::wcout << L"\tSkipping: " << I->second->Path << std::endl;
				url = J->second->Url;
				goto skip;
			}
		}

		std::wcout << L"\tUploading: " << I->second->Path << L" ...";
		url = upload_path + path_name.first + L"/" + path_name.second;

		ULONG FileSize;
		char* pFileBuffer;
		MyReadFile((wchar_t*)(base_dir + L"\\" + I->second->Path).c_str(), -1, (PVOID*)&pFileBuffer, &FileSize);

		if(UploadFileToServer(upload_path + path_name.first, path_name.second.c_str(), pFileBuffer, FileSize, apiKey))
			std::wcout << L" done" << std::endl;
		else
			std::wcout << L" FAILED" << std::endl;

		free(pFileBuffer);

	skip:
		JSONObject file;
		file[L"path"] = new JSONValue(I->second->Path);
		file[L"hash"] = new JSONValue(I->second->Hash);
		file[L"url"] = new JSONValue(url);
		files.push_back(new JSONValue(file));
	}
	root[L"files"] = new JSONValue(files);

	// Create signature
	std::string hashes;
	for (auto I = hash_set.begin(); I != hash_set.end(); ++I)
		hashes += g_str_conv.to_bytes(*I) + "\n";

	ULONG hashSize;
	PVOID hash = NULL;
	if (NT_SUCCESS(MyHashBuffer((void*)hashes.c_str(), hashes.length(), &hash, &hashSize)))
	{
		ULONG signatureSize;
		PUCHAR signature = NULL;
		if (NT_SUCCESS(SignHash(hash, hashSize, key, keySize, (PVOID*)&signature, &signatureSize)))
		{
			wchar_t* enc = b64_encode(signature, signatureSize);
			root[L"signature"] = new JSONValue(enc);
			free(enc);

			free(signature);
		}

		free(hash);
	}

	if (!sign.empty()) { // driver|full
		root[L"ci"] = new JSONValue(sign);
	}

	root[L"version"] = new JSONValue(version);
	root[L"update"] = new JSONValue(iUpdate);

	JSONValue *value = new JSONValue(root);
	auto wJson = value->Stringify();
	delete value;

	std::string Json = g_str_conv.to_bytes(wJson);

	UploadFileToServer(upload_path, _T(UPDATE_FILE), Json.c_str(), (ULONG)Json.size(), apiKey);

	std::wcout << L"done"<< std::endl;

	return iUpdate;
}

void SignInstaller(std::wstring software, std::wstring version, std::wstring installer, PVOID key, ULONG keySize, std::string apiKey)
{
	std::wcout << L"Signing installer: " << installer << std::endl;

	ULONG hashSize;
	PVOID hash = NULL;
	if (NT_SUCCESS(MyHashFile((installer).c_str(), &hash, &hashSize)))
	{
		ULONG signatureSize;
		PUCHAR signature = NULL;
		if (NT_SUCCESS(SignHash(hash, hashSize, key, keySize, (PVOID*)&signature, &signatureSize)))
		{
			UploadFileToServer(software + L"/" + version, installer + L".sig", signature, signatureSize, apiKey);

			std::wcout << L"Installer signed and uploaded" << std::endl;

			free(signature);
		}

		free(hash);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Command-line argument parsing
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> g_arguments;

bool HasFlag(const std::wstring& name)
{
	return std::find(g_arguments.begin(), g_arguments.end(), L"/" + name) != g_arguments.end();
}

std::wstring GetArgument(const std::wstring& name, const std::wstring& mod = L"/")
{
	std::wstring prefix = mod + name + L":";
	for (size_t i = 0; i < g_arguments.size(); i++) {
		if (_wcsicmp(g_arguments[i].substr(0, prefix.length()).c_str(), prefix.c_str()) == 0) {
			return g_arguments[i].substr(prefix.length());
		}
	}
	return L"";
}

std::vector<std::wstring> ParseFileList(const std::wstring& files_str)
{
	std::vector<std::wstring> result;
	if (files_str.empty())
		return result;

	size_t start = 0;
	size_t end = files_str.find(L';');

	while (end != std::wstring::npos)
	{
		result.push_back(files_str.substr(start, end - start));
		start = end + 1;
		end = files_str.find(L';', start);
	}

	result.push_back(files_str.substr(start));
	return result;
}

void PrintUsage()
{
	std::wcout << SOFTWARE_NAME << L" Update Utility - Usage" << std::endl;
	std::wcout << L"================================" << std::endl;
	std::wcout << std::endl;
	std::wcout << L"Update mode:" << std::endl;
	std::wcout << L"\tUpdUtil.exe update (/channel:[stable|preview|live]) (/version:[a.bb.c]) {Options}" << std::endl;
	std::wcout << std::endl;
	std::wcout << L"Options:" << std::endl;
	std::wcout << L"\t/files:file1.txt;file2.txt;file3.txt - Update only specified files" << std::endl;
	std::wcout << L"\t/step:[get|scan|prepare|apply] - Execute specific update step" << std::endl;
	std::wcout << L"\t\tget - download update information to " _T(UPDATE_FILE) << std::endl;
	std::wcout << L"\t\tscan - check for updates" << std::endl;
	std::wcout << L"\t\tprepare - download updates, but don't install" << std::endl;
	std::wcout << L"\t\tapply - install updates" << std::endl;
	std::wcout << L"\t/restart - Restart application after update" << std::endl;
	std::wcout << L"\t/temp:[path] - Specify temporary directory" << std::endl;
	std::wcout << L"\t/path:[path] - Specify application directory" << std::endl;
	std::wcout << std::endl;
	std::wcout << L"Pipe server mode:" << std::endl;
	std::wcout << L"\tUpdUtil.exe /server:name" << std::endl;
	std::wcout << L"\t\tCommands: get, scan, prepare, get_status, download" << std::endl;
	std::wcout << std::endl;
	std::wcout << L"Utility Commands:" << std::endl;
	std::wcout << L"\tUpdUtil.exe download [url] (/name:[filename])" << std::endl;
	std::wcout << L"\t\tDownload file from URL to temp directory" << std::endl;
	std::wcout << std::endl;
	std::wcout << L"\tUpdUtil.exe print [url]" << std::endl;
	std::wcout << L"\t\tDownload file and print to stdout" << std::endl;
	std::wcout << std::endl;
	std::wcout << L"\tUpdUtil.exe run_setup [file.exe] (/embedded)" << std::endl;
	std::wcout << L"\t\tVerify signature and run setup file" << std::endl;
	std::wcout << L"\t\t/embedded - Run in silent mode" << std::endl;
	std::wcout << std::endl;
	std::wcout << L"\tUpdUtil.exe get_cert [serial_number] (/hwid:[id]) (/update_key:[key])" << std::endl;
	std::wcout << L"\tUpdUtil.exe get_cert_lr (/serial:[number]) (/hwid:[id]) (/update_key:[key])" << std::endl;
	std::wcout << L"\t\tDownload certificate from server" << std::endl;
	std::wcout << L"\t\tget_cert_lr - Uses legacy registration system (LR=1)" << std::endl;
	std::wcout << std::endl;
	std::wcout << L"DEV Commands:" << std::endl;
	std::wcout << L"\tUpdUtil.exe make_keys" << std::endl;
	std::wcout << L"\t\tGenerate private.key and public.key for signing" << std::endl;
	std::wcout << std::endl;
	std::wcout << L"\tUpdUtil.exe commit [software] /version:[a.b.c] {Options}" << std::endl;
	std::wcout << L"\t\tUpload new release version to update server" << std::endl;
	std::wcout << L"\t\t/files:file1;file2 - Upload only specified files" << std::endl;
	std::wcout << L"\t\t/signed:[driver|full] - Mark as signed build" << std::endl;
	std::wcout << L"\t\t/api_key:[key] - API key (auto-generated from private.key if not specified)" << std::endl;
	std::wcout << std::endl;
	std::wcout << L"\tUpdUtil.exe sign [software] [installer.exe] /version:[a.b.c]" << std::endl;
	std::wcout << L"\t\tSign installer and upload signature" << std::endl;
	std::wcout << std::endl;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main function
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow)
{
	InitOsVersionInfo();

	wchar_t szPath[MAX_PATH];
	GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath));
	*wcsrchr(szPath, L'\\') = L'\0';
	std::wstring wPath = szPath;

	// Parse command line
	int nArgs = 0;
	LPWSTR* szArglist = CommandLineToArgvW(lpCmdLine, &nArgs);
	for (int i = 0; i < nArgs; i++)
		g_arguments.push_back(szArglist[i]);
	LocalFree(szArglist);

	// Console setup
	if (!HasFlag(L"embedded")) {
		if (AttachConsole(ATTACH_PARENT_PROCESS) == FALSE)
			AllocConsole();
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
		
		if (HasFlag(L"pause")) {
			std::cout << "Update Utility" << std::endl;
			std::wcout << lpCmdLine << std::endl;
			std::cout << std::endl << "Press enter to continue..." << std::endl;
			std::cin.get();
		}
	}

	std::wstring temp_dir = GetArgument(L"temp");
	if (temp_dir.empty()) {
		wchar_t szTemp[MAX_PATH];
		GetTempPath(MAX_PATH, szTemp);
		temp_dir = std::wstring(szTemp) + GetSoftware() + L"-Updater";
	}

	std::wstring base_dir = GetArgument(L"path");
	if (base_dir.empty())
		base_dir = wPath;

	std::wstring backup_dir = base_dir + L"\\backup";

	std::wstring arch = GetArgument(L"arch");
	if (!arch.empty()) {
		// normalize architecture
		if (arch == L"x86_64")
			arch = L"x64";
		else if (arch == L"i386")
			arch = L"x86";
		else if (arch == L"ARM64" || arch == L"A64" || arch == L"arm64")
			arch = L"a64";
	} else
		arch = Arch2Str(GetSysArch());


	//
	// Pipe server mode
	//
	std::wstring server_name = GetArgument(L"server");
	if (!server_name.empty())
	{
		RunPipeServer(server_name);
		return 0;
	}


	//
	// Download - download file to disk
	//
	if (g_arguments.size() >= 2 && g_arguments[0] == L"download")
	{
		std::wstring url = g_arguments[1];
		std::wstring name = GetArgument(L"name");
		if (name.empty()) {
			size_t end = url.find_last_of(L'?');
			size_t pos = url.find_last_of(L'/', end);
			if (pos == std::wstring::npos) {
				std::wcout << L"Error: Invalid URL" << std::endl;
				return UPD_ERROR_INVALID;
			}
			name = url.substr(++pos, end - pos);
		}

		std::wcout << L"Downloading: " << url << std::endl;
		std::wcout << L"To: " << temp_dir << L"\\" << name << std::endl;

		return DownloadFile(url, temp_dir + L"\\" + name);
	}

	//
	// Print - download file and print to stdout
	//
	else if (g_arguments.size() >= 2 && g_arguments[0] == L"print")
	{
		std::wstring url = g_arguments[1];
		return PrintFile(url);
	}

	//
	// Run Setup - run a signed setup file
	//
	else if (g_arguments.size() >= 2 && g_arguments[0] == L"run_setup")
	{
		std::wstring wFile = g_arguments[1];

		std::wcout << L"Verifying setup file signature: " << wFile << std::endl;

		if(!NT_SUCCESS(VerifyFileSignature(wFile.c_str()))) {
			std::wcout << L"ERROR: Invalid signature!" << std::endl;
			return UPD_ERROR_SIGN;
		}

		std::wcout << L"Signature valid. Running setup..." << std::endl;

		std::wstring wParams;
		if (HasFlag(L"embedded"))
			wParams = L"/SILENT";

		SHELLEXECUTEINFO si = { sizeof(SHELLEXECUTEINFO) };
		si.lpFile = wFile.c_str();
		si.lpParameters = wParams.c_str();
		si.nShow = SW_SHOW;

		if (!ShellExecuteEx(&si)) {
			std::wcout << L"ERROR: Failed to execute setup" << std::endl;
			return UPD_ERROR_EXEC;
		}

		return 0;
	}

	//
	// Update - perform update steps
	//
	if (g_arguments.size() >= 1 && g_arguments[0] == L"update")
	{
		std::wstring channel = GetArgument(L"channel");
		std::wstring version = GetArgument(L"version");
		bool bCanGet = !channel.empty() || !version.empty();
		std::wstring update_num = GetArgument(L"update");
		std::wstring step = GetArgument(L"step");
		std::wstring files_str = GetArgument(L"files");
		bool restart = HasFlag(L"restart");

		// Parse file filter
		std::vector<std::wstring> file_filter = ParseFileList(files_str);
		const std::vector<std::wstring>* pFileFilter = file_filter.empty() ? nullptr : &file_filter;

		int ret = 0;
		std::shared_ptr<SRelease> pFiles;

		//
		// Step: GET - Download update information
		//
		if (bCanGet && step.empty() || step == L"get")
		{
			CreateDirectoryW(temp_dir.c_str(), NULL);

			pFiles = DownloadUpdateFromServer(channel, version, update_num, arch, true);
			if (!pFiles)
			{
				std::wcout << L"Failed to download or verify update information" << std::endl;
				return UPD_ERROR_GET;
			}

			std::wcout << L"Downloaded update info: v" << pFiles->Version << L"-" << pFiles->iUpdate << std::endl;

			// Save to disk
			std::wstring file_path = temp_dir + L"\\" _T(UPDATE_FILE);
			std::string json_str = WriteUpdate(pFiles);
			MyWriteFile((wchar_t*)file_path.c_str(), (char*)json_str.c_str(), (ULONG)json_str.length());

			if (step == L"get")
				return 0;
		}

		// Load update JSON if not already loaded
		if (!pFiles)
		{
			pFiles = LoadUpdateFromDisk(temp_dir);
			if (!pFiles)
			{
				if (!bCanGet)
				{
					std::wcout << L"Error: Either channel or version must be specified" << std::endl;
					PrintUsage();
					return UPD_ERROR_INVALID;
				}

				std::wcout << L"No update information found. Run with /step:get first." << std::endl;
				return UPD_ERROR_LOAD;
			}
		}

		//
		// Step: SCAN - Find changed files
		//
		if (step.empty() || step == L"scan" || step == L"prepare" || step == L"apply")
		{
			ret = FindChanges(pFiles, base_dir, temp_dir, pFileFilter);
			if (ret < 0)
			{
				std::wcout << L"Scan failed" << std::endl;
				return ret;
			}

			std::wcout << L"Found " << ret << L" changed file(s)" << std::endl;

			if (ret == 0)
			{
				std::wcout << L"No updates needed" << std::endl;
				return 0;
			}

			if (step == L"scan")
				return ret;
		}

		//
		// Step: PREPARE - Download changed files
		//
		if (step.empty() || step == L"prepare" || step == L"apply")
		{
			ret = DownloadUpdate(temp_dir, pFiles);
			if (ret < 0)
			{
				std::wcout << L"Download failed" << std::endl;
				return ret;
			}

			std::wcout << L"Downloaded " << ret << L" file(s)" << std::endl;

			if (step == L"prepare")
				return ret;
		}

		//
		// Step: APPLY - Install updated files
		//
		if (step.empty() || step == L"apply")
		{
			bool should_restart = restart;

			// Shutdown app if needed
			if (should_restart && AppIsRunning(base_dir))
			{
				if (!AppShutdown(base_dir))
				{
					std::wcout << L"Failed to shutdown application" << std::endl;
					return UPD_ERROR_INTERNAL;
				}
			}

			// Create backup directory
			CreateDirectoryW(backup_dir.c_str(), NULL);

			// Apply updates
			ret = ApplyUpdate(base_dir, temp_dir, backup_dir, pFiles);
			if (ret < 0)
			{
				std::wcout << L"Apply failed" << std::endl;
				return ret;
			}

			std::wcout << L"Installed " << ret << L" file(s)" << std::endl;

			// Restart app if needed
			if (should_restart)
			{
				Sleep(1000);
				if (!AppStart(base_dir))
				{
					std::wcout << L"Warning: Failed to restart application" << std::endl;
				}
			}
		}

		return 0;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Get Certificate - download certificate from server
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	else if (g_arguments.size() >= 2 && (g_arguments[0] == L"get_cert" || g_arguments[0] == L"get_cert_lr"))
	{
		int ret = 0;

		std::wstring file_path = base_dir + L"\\Certificate.dat";
		std::wstring path;
		std::wstring serial;
		std::wstring hwid = GetArgument(L"hwid");

		if (g_arguments[0] == L"get_cert_lr")
		{
			path = L"/get_cert.php?LR=1";
			serial = GetArgument(L"serial");
			if(!serial.empty()) // serial is optional, update key is used instead
				path += L"&SN=" + serial;
		}
		else
		{
			serial = g_arguments[1];
			path = L"/get_cert.php?SN=" + serial;
		}

		// When hwid is specified manually, don't save to disk (assume it's not for this system)
		if (!hwid.empty())
			file_path.clear();
		if (!hwid.empty())
			path += L"&HwId=" + hwid;

		// Try to read existing update key from certificate
		std::wstring update_key = GetArgument(L"update_key");
		if (!file_path.empty() && update_key.empty()) {
			char* aCert = NULL;
			if (NT_SUCCESS(MyReadFile((wchar_t*)file_path.c_str(), 1024 * 1024, (PVOID*)&aCert, NULL)) && aCert != NULL) {
				std::string sCert = aCert;
				free(aCert);
				auto Cert = GetArguments(std::wstring(sCert.begin(), sCert.end()), L'\n', L':');
				auto F = Cert.find(L"UPDATEKEY");
				if (F != Cert.end())
					update_key = F->second;
			}
		}
		if(!update_key.empty())
			path += L"&UpdateKey=" + update_key;

		std::wcout << L"Downloading certificate from server..." << std::endl;

		char* aCert = NULL;
		ULONG lCert = 0;
		if (WebDownload(_T(UPDATE_DOMAIN), path.c_str(), &aCert, &lCert) && aCert != NULL && *aCert)
		{
			// Check for error response (JSON)
			if (aCert[0] == L'{') {
				JSONValue* jsonObject = JSON::Parse(aCert);
				if (jsonObject) {
					if (jsonObject->IsObject() && GetJSONBoolSafe(jsonObject->AsObject(), L"error"))
					{
						std::wcout << L"Server error: " << GetJSONStringSafe(jsonObject->AsObject(), L"errorMsg") << std::endl;
						ret = UPD_ERROR_GET;
					}
					delete jsonObject;
				}
				free(aCert);
				return ret;
			}
		}
		else
		{
			std::wcout << L"FAILED to call get_cert.php" << std::endl;
			return UPD_ERROR_GET;
		}

		// Save or print certificate
		if (ret == 0)
		{
			if (file_path.empty()) {
				printf("== CERTIFICATE ==\r\n%s\r\n== END ==", aCert);
			}
			else {
				if(!NT_SUCCESS(MyWriteFile((wchar_t*)file_path.c_str(), aCert, lCert))) {
					std::wcout << L"Failed to write certificate file" << std::endl;
					ret = UPD_ERROR_INTERNAL;
				}
				else {
					std::wcout << L"Certificate saved to: " << file_path << std::endl;
				}
			}
			free(aCert);
		}

		return ret;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// DEV Commands - commit, sign, make_keys
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////
	else if (g_arguments[0] == L"commit" || (g_arguments.size() >= 2 && g_arguments[0] == L"sign"))
	{
		std::wstring version = GetArgument(L"version");
		if (version.empty()) {
			std::wcout << L"Version not specified" << std::endl;
			PrintUsage();
			return UPD_ERROR_INVALID;
		}

		// Read private key
		PVOID key;
		ULONG keySize;
		if (!NT_SUCCESS(MyReadFile((wchar_t*)(wPath + L"\\private.key").c_str(), 1024 * 1024, &key, &keySize))) {
			std::wcout << L"Failed to read private.key" << std::endl;
			return UPD_ERROR_INTERNAL;
		}

		// Generate API key from private key hash if not provided
		std::string apiKey = g_str_conv.to_bytes(GetArgument(L"api_key"));
		if (apiKey.empty()) 
		{
			ULONG hashSize;
			char* hash = NULL;
			if (NT_SUCCESS(MyHashBuffer(key, keySize, (PVOID*)&hash, &hashSize)))
			{
				hashSize /= 2;
				for (ULONG i = 0; i < hashSize; i++)
					hash[i] ^= hash[i + hashSize];

				std::wstring wKey = hexStr((unsigned char*)hash, hashSize);
				apiKey = g_str_conv.to_bytes(wKey);

				free(hash);
			}
		}

		if (g_arguments[0] == L"commit") 
		{
			std::wstring sign_ci = GetArgument(L"signed");
			std::wstring files_str = GetArgument(L"files");

			// Parse file filter if specified
			std::vector<std::wstring> file_filter = ParseFileList(files_str);
			const std::vector<std::wstring>* pFileFilter = file_filter.empty() ? nullptr : &file_filter;

			int ret = UploadUpdate(GetSoftware(), arch, version, sign_ci, base_dir, pFileFilter, key, keySize, apiKey);
			free(key);
			return ret;
		}
		else if (g_arguments[0] == L"sign")
		{
			std::wstring installer = g_arguments[1];
			SignInstaller(GetSoftware(), version, installer, key, keySize, apiKey);
			free(key);
			return 0;
		}

		free(key);
		return 0;
	}
	else if (g_arguments.size() >= 1 && (g_arguments[0] == L"make_keys"))
	{
		std::wcout << L"Generating key pair..." << std::endl;
		CreateKeyPair((wPath + L"\\private.key").c_str(), (wPath + L"\\public.key").c_str());
		std::wcout << L"Keys created: private.key and public.key" << std::endl;
		return 0;
	}

	// No valid command
	PrintUsage();
	return UPD_ERROR_INVALID;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Test command lines:
// 
// sign C:\Projects\TaskExplorer\Installer\Output\TaskExplorer-v1.7.0.exe /version:1.7.0
// commit /path:C:\Projects\TaskExplorer\Installer\Build /version:1.7.0
// update /path:C:\Projects\TaskExplorer\Installer\Test /channel:stable /temp:C:\Projects\TaskExplorer\Installer\Temp
//
//
//
