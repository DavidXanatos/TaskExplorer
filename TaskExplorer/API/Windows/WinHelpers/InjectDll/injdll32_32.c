#include "injdll.h"

#define ptr_to_dw(ptr) ((DWORD)(DWORD_PTR)(ptr))

DWORD find_export_x86(const wchar_t* sDllName, const char* sTarget)
{
	HMODULE hMod;
	DWORD dwResult = 0;
	FARPROC pExport = NULL;
	if(!(hMod = LoadLibraryW(sDllName))) {
		//error_function(L"LoadLibraryW");
		return dwResult;
	}

	if(!(pExport = GetProcAddress(hMod, sTarget))) {
		//error_function(L"GetProcAddress");
		FreeLibrary(hMod);
		return dwResult;
	}

	dwResult = (DWORD)(ptr_to_dw(pExport) - ptr_to_dw(hMod));
	FreeLibrary(hMod);
	return dwResult;
}
