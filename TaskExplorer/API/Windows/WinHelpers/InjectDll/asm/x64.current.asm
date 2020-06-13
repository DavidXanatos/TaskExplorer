BITS 32

original_rip:
	times 8 db 0x0
LoadLibraryW:
	times 8 db 0x0
main:
	pushfq
	push rax
	push rcx
	push rdx
	push rbx
	push rbp
	push rsi
	push rdi
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	sub rsp, 0x28
	lea ecx, wsDllPath
	call [LoadLibraryW]
	add rsp, 40
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdi
	pop rsi
	pop rbp
	pop rbx
	pop rdx
	pop rcx
	pop rax
	popfq
	jmp [original_rip]
	db 0x0
wsDllPath:
	db u('injected_dll'), 0x0, 0x0