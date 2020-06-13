#include "injdll.h"

#ifdef BUILD_ARCH_X64
#	define GetThreadContext Wow64GetThreadContext
#	define SetThreadContext Wow64SetThreadContext
#	define X_CONTEXT WOW64_CONTEXT
#else
#	define X_CONTEXT CONTEXT
#endif

#define ptr_to_dw(ptr) ((DWORD)(DWORD_PTR)(ptr))

//#define CODESIZE 68
#define CODESIZE 58
BOOL inject_x86(HANDLE hProcess, HANDLE hThread, const wchar_t* sDllPath)
{
	LPVOID mem;
	X_CONTEXT context;
	size_t dwCodeSize;
	DWORD offLoadLibraryW, /*offInitFunc,*/ mem32;
	BYTE code[CODESIZE + (MAX_PATH+1)*sizeof(wchar_t)] = {0};
	union {
		PBYTE  pB;
		PWORD  pW;
		PDWORD pL;
	} ip;

	dwCodeSize = CODESIZE + (wcslen(sDllPath) + 1)*sizeof(wchar_t);
	offLoadLibraryW = find_export_x86(L"kernel32.dll", "LoadLibraryW");
	//offInitFunc = find_export_x86(sDllPath, "Init");
	if (!offLoadLibraryW /*|| !offInitFunc*/)
		return FALSE;

	context.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hThread, &context);
	
	mem = VirtualAllocEx( hProcess, NULL, dwCodeSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	mem32 = ptr_to_dw(mem);

	ip.pB = code;

	/** <label>entry</label> */
	*ip.pB++ = 0x8b;			// mov eax, [esp]
	*ip.pW++ = 0x2404;
	*ip.pB++ = 0xc3;			// ret

	/** <label>main</label> */
	*ip.pB++ = 0x68;			// push eip
	*ip.pL++ = context.Eip;		//   store the original context
	*ip.pB++ = 0x9c;			// pushf
	*ip.pB++ = 0x60;			// pusha
	*ip.pB++ = 0xe8;			// call entry
	*ip.pL++ = 0xfffffff0L;
	*ip.pB++ = 0x83;			// add eax, wcslen(sDllPath) - $
	*ip.pB++ = 0xc0;
	*ip.pB++ = (CODESIZE - 16);
	*ip.pB++ = 0x50;			// push eax
	//   stack = [ dllpath, original context ]
	*ip.pB++ = 0x31;			// xor ecx,ecx (ECX = 0)
	*ip.pB++ = 0xc9;			//   ecx = 0
	*ip.pB++ = 0x64;			// mov esi, [ fs:ecx + 0x30 ]
	*ip.pB++ = 0x8b;			//   ecx = &PEB ([fs:0x30])
	*ip.pB++ = 0x71;
	*ip.pB++ = 0x30;
	*ip.pB++ = 0x8b;			// mov esi, [ esi + 0x0C ]
	*ip.pW++ = 0xc76;			//   esi = PEB->Ldr
	*ip.pB++ = 0x8b;			// mov esi, [ esi + 0x1C ]
	*ip.pW++ = 0x1c76;			//   esi = PEB->Ldr.InInitOrder (first module)

	/** <label>nextmod</label> */
	*ip.pB++ = 0x8b;			// mov ebp, [ esi + 0x08 ]
	*ip.pW++ = 0x86e;			//   ebp = InInitOrder[X].base_address
	*ip.pB++ = 0x8b;			// mov edi, [ esi + 0x20 ]
	*ip.pW++ = 0x207e;			//   edi = InInitOrder[X].module_name (unicode string)
	*ip.pB++ = 0x8b;			// mov esi, [ esi]
	*ip.pB++ = 0x36;			//   esi = InInitOrder[X].flink (next module)
	*ip.pB++ = 0x38;			// cmp [ edi + 12*SZWCHAR ], cl
	*ip.pW++ = 0x184f;			//   modulename[12] == 0 ? strlen("kernel32.dll") == 12
	*ip.pB++ = 0x75;			// jne nextmod
	*ip.pB++ = 0xf3;			//   continue until we find kernel32

	*ip.pB++ = 0x89;			// mov edi, ebp
	*ip.pB++ = 0xef;			//   edi = kernel32.base_address
	*ip.pB++ = 0x81;			// add edi, LoadLibraryW offset
	*ip.pB++ = 0xc7;			//   edi = kernel32.LoadLibraryW
	*ip.pL++ = offLoadLibraryW;
	*ip.pB++ = 0xff;			// call edi
	*ip.pB++ = 0xd7;			//   stack = [ dllpath, ebp, caller ]
	//*ip.pB++ = 0x89;			// mov edi, eax
	//*ip.pB++ = 0xc7;			//   edi = dll.base
	//*ip.pB++ = 0x81;			// add edi, dll.init_func offset
	//*ip.pB++ = 0xc7;			//   edi = dll.init_func
	//*ip.pL++ = offInitFunc;
	//*ip.pB++ = 0xff;			// call DWORD edi
	//*ip.pB++ = 0xd7;			//   call dll.init_func()
	*ip.pB++ = 0x61;			// popa
	*ip.pB++ = 0x9d;			// popf
	*ip.pB++ = 0xc3;			// ret
	
	CopyMemory((void*)ip.pB, sDllPath, (wcslen(sDllPath)+1)*sizeof(wchar_t));
	WriteProcessMemory(hProcess, mem, code, dwCodeSize, NULL);
	FlushInstructionCache(hProcess, mem, dwCodeSize);
	context.Eip = mem32 + 4;
	SetThreadContext( hThread, &context );

	return TRUE;
}
