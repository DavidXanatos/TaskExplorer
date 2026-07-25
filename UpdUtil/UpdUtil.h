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

#pragma once


// Forward declare JSONObject
class JSONValue;
typedef std::map<std::wstring, JSONValue*> JSONObject;
typedef std::vector<JSONValue*> JSONArray;

//
// Data Structures
//

// File state structure
struct SFile
{
	SFile() : State(eNone) {}

	std::wstring	Path;
	std::wstring	Url;
	std::wstring	Hash;
	enum EState
	{
		eNone = 0,
		eChanged,
		ePending	// downloaded
	}				State;
};

// File collection structure
struct SFiles
{
	std::map<std::wstring, std::shared_ptr<SFile>> Map;
	std::wstring Sign;
};

// Release structure
struct SRelease : SFiles
{
	std::wstring Version;
	int iUpdate;
	std::wstring CI;
	std::wstring AgentArch;
	std::wstring Framework;
};

//
// Helper Functions
//

std::wstring Arch2Str(ULONG architecture);
ULONG GetSysArch();
std::wstring ReadRegistryStringValue(std::wstring key, const std::wstring& valueName);
ULONG GetBinaryArch(const std::wstring& file);
std::wstring GetFileVersion(const std::wstring& file);
void CreateDirectoryTree(const std::wstring& root, const std::wstring& path);
std::pair<std::wstring, std::wstring> SplitName(std::wstring path);
bool DeleteDirectoryRecursively(const std::wstring& root, const std::wstring& path, bool bWithFiles);

//
// JSON Helper Functions
//

std::wstring GetJSONStringSafe(const JSONObject& root, const std::wstring& key, const std::wstring& def = L"");
int GetJSONIntSafe(const JSONObject& root, const std::wstring& key, int def = 0);
int GetJSONBoolSafe(const JSONObject& root, const std::wstring& key, bool def = false);
JSONObject GetJSONObjectSafe(const JSONObject& root, const std::wstring& key);
JSONArray GetJSONArraySafe(const JSONObject& root, const std::wstring& key);

//
// Update Data Functions
//

std::shared_ptr<SRelease> ScanDir(std::wstring Path);
std::shared_ptr<SRelease> ReadUpdate(const JSONObject& jsonObject);
std::string WriteUpdate(std::shared_ptr<SRelease> pFiles);
bool VerifyUpdate(std::shared_ptr<SFiles> pFiles);
void ReadFiles(const JSONArray& jsonFiles, std::shared_ptr<SFiles> pFiles);

//
// Shared Helper Functions (for UpdServ and main)
//

// Get default temp directory for updates
std::wstring GetDefaultTempDir();

// Load update JSON from disk (returns nullptr on failure)
std::shared_ptr<SRelease> LoadUpdateFromDisk(const std::wstring& temp_dir);

// Build update URL query parameters
std::wstring BuildUpdateUrlParams(const std::wstring& channel, const std::wstring& version,
                                   const std::wstring& update_num, const std::wstring& arch);

// Download and parse update JSON from server (returns nullptr on failure)
std::shared_ptr<SRelease> DownloadUpdateFromServer(const std::wstring& channel,
                                                     const std::wstring& version,
                                                     const std::wstring& update_num,
                                                     const std::wstring& arch,
                                                     bool verify = true);

// Load update from disk, or download from server if not found
std::shared_ptr<SRelease> LoadOrDownloadUpdate(const std::wstring& temp_dir,
                                                 const std::wstring& channel,
                                                 const std::wstring& version,
                                                 const std::wstring& update_num);

// Resolve package path (starting with backslash) to URL using update JSON
std::wstring ResolvePackagePath(const std::wstring& package_path, const std::wstring& temp_dir);

//
// Update Operations
//
// Note: pFileFilter is a list of filenames to filter by (only update these files when specified)
// When pFileFilter is nullptr, all files are processed

// Find changed files by comparing with local installation
// Returns: number of changed files, or error code (<0)
int FindChanges(
	std::shared_ptr<SRelease> pNewFiles,
	std::wstring base_dir,
	std::wstring temp_dir,
	const std::vector<std::wstring>* pFileFilter);

// Download changed files to temp directory
// Returns: number of downloaded files, or error code (<0)
int DownloadUpdate(
	std::wstring temp_dir,
	std::shared_ptr<SFiles> pNewFiles);

// Apply downloaded updates from temp directory to base directory with fail-safe backup
// backup_dir: directory to move old files to (for rollback on failure)
// Returns: number of installed files, or error code (<0)
int ApplyUpdate(
	std::wstring base_dir,
	std::wstring temp_dir,
	std::wstring backup_dir,
	std::shared_ptr<SFiles> pNewFiles);

//
// Main Update Processing
//
// Main update processing function that orchestrates scan, download, and apply steps
// step: empty for all steps, or "scan"/"prepare"/"apply" for specific step
// Returns: 0 on success or number of changed/downloaded files, error code (<0) on failure
int ProcessUpdate(
	std::shared_ptr<SRelease>& pFiles,
	const std::wstring& step,
	const std::wstring& temp_dir,
	const std::wstring& base_dir,
	const std::vector<std::wstring>* pFileFilter);

//
// Utility Functions
//

void Execute(std::wstring wFile, std::wstring wParams);
int DownloadFile(std::wstring url, std::wstring file_path);
int PrintFile(std::wstring url);
BOOLEAN WebDownload(std::wstring url, PSTR* pData, ULONG* pDataLength);

//
// Pipe Server
//

void RunPipeServer(const std::wstring& pipeName);

extern "C" {
	NTSTATUS CreateKeyPair(_In_ PCWSTR PrivFile, _In_ PCWSTR PubFile);

	NTSTATUS SignHash(_In_ PVOID Hash, _In_ ULONG HashSize, _In_ PVOID PrivKey, _In_ ULONG PrivKeySize, _Out_ PVOID* Signature, _Out_ ULONG* SignatureSize);
	NTSTATUS VerifyHashSignature(PVOID Hash, ULONG HashSize, PVOID Signature, ULONG SignatureSize);
	NTSTATUS VerifyFileSignature(const wchar_t* FilePath);

	NTSTATUS MyHashBuffer(_In_ PVOID pData, _In_ SIZE_T uSize, _Out_ PVOID* Hash, _Out_ PULONG HashSize);
	NTSTATUS MyHashFile(_In_ PCWSTR FileName, _Out_ PVOID* Hash, _Out_ PULONG HashSize);

	NTSTATUS MyReadFile(_In_ PWSTR FileName, _In_ ULONG FileSizeLimit, _Out_ PVOID* Buffer, _Out_ PULONG FileSize);
	NTSTATUS MyWriteFile(_In_ PWSTR FileName, _In_ PVOID Buffer, _In_ ULONG BufferSize);

	size_t b64_encoded_size(size_t inlen);
	wchar_t* b64_encode(const unsigned char* in, size_t inlen);
	size_t b64_decoded_size(const wchar_t* in);
	int b64_decode(const wchar_t* in, unsigned char* out, size_t outlen);
}