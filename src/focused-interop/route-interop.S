#include "src/t14-abi/interop-contract-v1.h"
.intel_syntax noprefix
.text
.global pangu_route_asm_invoke
.type pangu_route_asm_invoke,@function
pangu_route_asm_invoke:
 test rdi,rdi
 jz .Lbad_pointer
 test r8,r8
 jz .Lbad_pointer
 test rdi,7
 jnz .Lbad_pointer
 test edx,edx
 jz .Lbad_header
 cmp DWORD PTR [rdi],edx
 jne .Lbad_header
 cmp DWORD PTR [rdi+4],PANGU_INTEROP_SERVICE_SIZE
 jb .Lbad_header
 cmp DWORD PTR [rdi+8],1
 jne .Lbad_header
 cmp DWORD PTR [rdi+12],0
 jne .Lbad_header
 mov rax,QWORD PTR [rdi+PANGU_INTEROP_METHOD_OFFSET]
 test rax,rax
 jz .Lbad_header
 mov rdi,rsi
 mov esi,ecx
 mov rdx,r8
 jmp rax
.Lbad_pointer:
 mov eax,1
 ret
.Lbad_header:
 mov eax,9
 ret
.size pangu_route_asm_invoke,.-pangu_route_asm_invoke
/* Seed all six callee-saved registers. Check on success and error exits. */
.global pangu_route_asm_preserve
.type pangu_route_asm_preserve,@function
pangu_route_asm_preserve:
 push rbx
 push rbp
 push r12
 push r13
 push r14
 push r15
 sub rsp,8
 mov ebx,0x12345678
 mov ebp,0x23456789
 mov r12d,0x3456789a
 mov r13d,0x456789ab
 mov r14d,0x56789abc
 mov r15d,0x6789abcd
 call pangu_route_asm_invoke
 mov DWORD PTR [rsp],eax
 cmp rbx,0x12345678
 jne .Lbad_register
 cmp rbp,0x23456789
 jne .Lbad_register
 cmp r12,0x3456789a
 jne .Lbad_register
 cmp r13,0x456789ab
 jne .Lbad_register
 cmp r14,0x56789abc
 jne .Lbad_register
 cmp r15,0x6789abcd
 jne .Lbad_register
 mov eax,DWORD PTR [rsp]
 jmp .Lrestore
.Lbad_register:
 mov eax,-90
.Lrestore:
 add rsp,8
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbp
 pop rbx
 ret
.size pangu_route_asm_preserve,.-pangu_route_asm_preserve
.section .note.GNU-stack,"",@progbits
