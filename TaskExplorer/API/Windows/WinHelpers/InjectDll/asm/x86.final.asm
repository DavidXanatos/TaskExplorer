BITS 32

%define MAX_PATH		0x1
%define SZWCHAR			0x2 ; sizeof(wchar_t)
%define u(x)			__utf16__(x) 

; This technique for finding the address to wsDllPath was put in place pretty early
; on. This could've easily been replaced by populating the address at runtime on the
; C side, but by the time that option became available, I didn't really feel like
; changing any of the assembly.
entry_helper:
	mov eax, [esp]
	ret

main:
	push dword 0xDEADBEEF 			; Original EIP is populated at runtime in C
	pushf							; Store original context
	pusha
	call entry_helper
	add eax, wsDllPath - $			; EAX =   L"project-52a-32.dll\0"
	push dword eax					; Stack = L"project52a-32.dll\0", original context

; The following few lines are based on shellcode found at:
;    https://code.google.com/p/w32-bind-ngs-shellcode/source/browse/trunk/w32-bind-ngs-shellcode.asm
;
; It's been modified a bit since I originally started playing with it,
; but the underlying mechanics and comments remain the same, so I've
; added the appropriate COPYRIGHT.txt file to this directory.

; Find base address of kernel32.dll. This code should work on Windows 5.0-7.0
	xor ecx, ecx					; ECX = 0
	mov esi, [ fs:ecx + 0x30 ]		; ESI = &(PEB) ([FS:0x30])
	mov esi, [ esi + 0x0C ]			; ESI = PEB->Ldr
	mov esi, [ esi + 0x1C ]			; ESI = PEB->Ldr.InInitOrder (first module)
next_module:
	mov ebp, [ esi + 0x08 ]			; EBP = InInitOrder[X].base_address
	mov edi, [ esi + 0x20 ]			; EDI = InInitOrder[X].module_name (unicode string)
	mov esi, [ esi]					; ESI = InInitOrder[X].flink (next module)
	cmp [ edi + 12*SZWCHAR ], cl	; modulename[12] == 0 ? strlen("kernel32.dll") == 12
	jne next_module					;	No, try next module.

begin_calls:
	mov edi, dword ebp				; EDI = kernel32.base_address
	add edi, dword 0xDEADBEEF		; EDI = kernel32.LoadLibraryW (LoadLibraryW offset found at runtime)
	call dword edi					; Stack = L"project52a-32.dll\0", EBP, caller
	mov edi, eax					; EDI = project52a.base
	add edi, dword 0xDEADBEEF		; EDI = project-52a.Initialize (Initialize offset found at runtime)
	call dword edi					; Call project52a.Initialize()
	popa							; Restore original context.
	popf
	ret								; Return to the original EIP
wsDllPath:							; project
	db u('/path/to/project52a-32.dll'), 0x0, 0x0
