#include "injdll.h"

#define make_ptr(typ,ptr,inc) (typ)((DWORD_PTR)(ptr)+(DWORD_PTR)(inc))

#define get_img_dir_from_rva(nthdr,idx) \
	(nthdr->OptionalHeader.DataDirectory[idx].VirtualAddress)

#define get_img_dir_size(nthdr,idx) \
	(nthdr->OptionalHeader.DataDirectory[idx].Size)

static PIMAGE_SECTION_HEADER get_enclosing_header_x86(DWORD rva, PIMAGE_NT_HEADERS32 pNTHeader)
{
	unsigned i;
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(pNTHeader);
	for ( i=0; i < pNTHeader->FileHeader.NumberOfSections; i++, section++ ) {
		DWORD size = section->Misc.VirtualSize;
		if (size == 0)
			size = section->SizeOfRawData;

		if ( (rva >= section->VirtualAddress) && (rva < (section->VirtualAddress + size)) )
			return section;
	}
	return NULL;
}

static void* get_ptr_from_rva_x86(DWORD rva, PIMAGE_NT_HEADERS32 pNTHeader, BYTE* imageBase)
{
	PIMAGE_SECTION_HEADER pSectionHdr;
	int delta;

	pSectionHdr = get_enclosing_header_x86(rva, pNTHeader);
	if ( !pSectionHdr )
		return NULL;

	delta = (int)(pSectionHdr->VirtualAddress-pSectionHdr->PointerToRawData);
	return (void*)(imageBase + rva - delta);
}

void* file_to_buffer(const wchar_t* path, size_t* size)
{
	DWORD r;
	BYTE* buffer = NULL;
	HANDLE hf;
	LARGE_INTEGER fsize;
	*size = 0;

	// open and read file to buffer
	if((hf = CreateFileW(path, GENERIC_READ, (FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE), NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE) {
		return NULL;
	}

	if(!GetFileSizeEx(hf, &fsize)) {
		CloseHandle(hf);
		return NULL;
	}

	if(!(buffer = (BYTE*)calloc(1, fsize.LowPart + 1))) {
		CloseHandle(hf);
		return buffer;
	}

	if (!ReadFile(hf, buffer, fsize.LowPart, &r, NULL)) {
		CloseHandle(hf);
		free(buffer);
		return NULL;
	}

	*size = fsize.LowPart;
	CloseHandle(hf);
	return buffer;
}

DWORD find_export_x86(const wchar_t* sDllName, const char* sTarget)
{
	unsigned j;
	size_t szBufMod = 0;
	wchar_t* sDllPath;
	void* lpImageBase;
	PIMAGE_DOS_HEADER dosHeader;
	PIMAGE_NT_HEADERS32 pNTHeader;
	PIMAGE_EXPORT_DIRECTORY pExportDir;
	PIMAGE_SECTION_HEADER header;
	DWORD dwResult = 0, exportsStartRVA, exportsEndRVA, i;
	DWORD *pdwFunctions = NULL, *pszFuncNames = NULL;
	WORD *pwOrdinals = NULL;

	// Check if the dllname represents a valid filepath. If not,
	// check our system directory.
	if(GetFileAttributesW(sDllName) == INVALID_FILE_ATTRIBUTES) {
		size_t szBuffer = GetSystemWow64DirectoryW(NULL, 0);
		//assert(szBuffer > 0);
		szBuffer += (wcslen(sDllName) + 1)*sizeof(wchar_t);
		sDllPath = malloc((szBuffer+1)*sizeof(wchar_t));
		GetSystemWow64DirectoryW(sDllPath, (UINT)szBuffer);
		wcscat_s(sDllPath, szBuffer, L"\\");
		wcscat_s(sDllPath, szBuffer, sDllName);
	} else {
		DWORD szBuffer = GetLongPathNameW(sDllName, NULL, 0);
		sDllPath = (wchar_t*)malloc((szBuffer+1)*sizeof(wchar_t));
		sDllPath[0] = L'\0';
		GetLongPathNameW(sDllName, sDllPath, szBuffer);
	}
	
	lpImageBase = file_to_buffer(sDllPath, &szBufMod);
	dosHeader = (PIMAGE_DOS_HEADER)lpImageBase;
	pNTHeader = make_ptr( PIMAGE_NT_HEADERS32, dosHeader, dosHeader->e_lfanew );
	exportsStartRVA = get_img_dir_from_rva(pNTHeader,IMAGE_DIRECTORY_ENTRY_EXPORT);
	exportsEndRVA = exportsStartRVA + get_img_dir_size(pNTHeader, IMAGE_DIRECTORY_ENTRY_EXPORT);
	if(!(header = get_enclosing_header_x86(exportsStartRVA, pNTHeader))) {
		if(lpImageBase) free(lpImageBase);
		if(sDllPath) free(sDllPath);
		return 0;
	}

	pExportDir = (PIMAGE_EXPORT_DIRECTORY)get_ptr_from_rva_x86(exportsStartRVA, pNTHeader, (PBYTE)lpImageBase);
	pdwFunctions =	(DWORD*)get_ptr_from_rva_x86(pExportDir->AddressOfFunctions, pNTHeader, (PBYTE)lpImageBase );
	pwOrdinals = (WORD*)get_ptr_from_rva_x86(pExportDir->AddressOfNameOrdinals, pNTHeader, (PBYTE)lpImageBase );
	pszFuncNames =	(DWORD*)get_ptr_from_rva_x86(pExportDir->AddressOfNames, pNTHeader, (PBYTE)lpImageBase );
	if (!pExportDir || !pdwFunctions || !pwOrdinals || !pszFuncNames) {
		if(lpImageBase) free(lpImageBase);
		if(sDllPath) free(sDllPath);
		return 0;
	}

	for (i = 0; i < pExportDir->NumberOfFunctions; i++, pdwFunctions++ ) {
		// see if this function has an associated name exported for it.
		if(!pdwFunctions) continue;
		for (j = 0; j < pExportDir->NumberOfNames; j++ ) {
			// rva to va
			if(pwOrdinals[j] == i) {
				ULONG_PTR va = pszFuncNames[j] - header->VirtualAddress + header->PointerToRawData;
				char* exportName = (char*)(((ULONG_PTR)lpImageBase) + va);
				if(_stricmp(exportName, sTarget) != 0) continue;
				dwResult = *((DWORD*)get_ptr_from_rva_x86(
					pExportDir->AddressOfFunctions + (DWORD)(pwOrdinals[j]*4),
					pNTHeader, (BYTE*)lpImageBase)
				);
				break;
			}
		}
		
		if(dwResult > 0) break;
	}
	if(lpImageBase) free(lpImageBase);
	if(sDllPath) free(sDllPath);
	return dwResult;
}


