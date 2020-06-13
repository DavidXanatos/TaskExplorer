#include "injdll.h"

BOOL inject_x64(HANDLE hProcess, HANDLE hThread, const wchar_t* dllpath)
{
	CONTEXT context;
	DWORD   len;
	LPVOID  mem;
	DWORD64 LLW;
	union
	{
		PBYTE pB;
		PDWORD64 pL;
	} ip;
#define CODESIZE 92
	static BYTE code[CODESIZE + MAX_PATH*sizeof(wchar_t)] = {
		0,0,0,0,0,0,0,0,	// original rip
		0,0,0,0,0,0,0,0,	// LoadLibraryW
		0x9C,				// pushfq
		0x50,				// push  rax
		0x51,				// push  rcx
		0x52,				// push  rdx
		0x53,				// push  rbx
		0x55,				// push  rbp
		0x56,				// push  rsi
		0x57,				// push  rdi
		0x41,0x50,			// push  r8
		0x41,0x51,			// push  r9
		0x41,0x52,			// push  r10
		0x41,0x53,			// push  r11
		0x41,0x54,			// push  r12
		0x41,0x55,			// push  r13
		0x41,0x56,			// push  r14
		0x41,0x57,			// push  r15
		0x48,0x83,0xEC,0x28,		// sub   rsp, 40
		0x48,0x8D,0x0D,41,0,0,0,	// lea   ecx, L"path\to\*.dll"
		0xFF,0x15,-49,-1,-1,-1,		// call  LoadLibraryW
		0x48,0x83,0xC4,0x28,		// add   rsp, 40
		0x41,0x5F,			// pop r15
		0x41,0x5E,			// pop r14
		0x41,0x5D,			// pop r13
		0x41,0x5C,			// pop r12
		0x41,0x5B,			// pop r11
		0x41,0x5A,			// pop r10
		0x41,0x59,			// pop r9
		0x41,0x58,			// pop r8
		0x5F,				// pop rdi
		0x5E,				// pop rsi
		0x5D,				// pop rbp
		0x5B,				// pop rbx
		0x5A,				// pop rdx
		0x59,				// pop rcx
		0x58,				// pop rax
		0x9D,				// popfq
		0xFF,0x25,-91,-1,-1,-1,    // jmp   original Rip
		0,					// DWORD alignment for LLW, fwiw
	};

	len = (DWORD)((wcslen(dllpath) + 1)*sizeof(wchar_t));
	if (len > MAX_PATH*sizeof(wchar_t))
		return FALSE;
	CopyMemory( code + CODESIZE, dllpath, len );
	len += CODESIZE;

	context.ContextFlags = CONTEXT_CONTROL;
	GetThreadContext( hThread, &context );
	mem = VirtualAllocEx( hProcess, NULL, len, MEM_COMMIT, PAGE_EXECUTE_READWRITE );
	LLW = (DWORD64)LoadLibraryW;

	ip.pB = code;

	*ip.pL++ = context.Rip;
	*ip.pL++ = LLW;

	WriteProcessMemory( hProcess, mem, code, len, NULL );
	FlushInstructionCache( hProcess, mem, len );
	context.Rip = (DWORD64)mem + 16;
	SetThreadContext( hThread, &context );
	return TRUE;
}
