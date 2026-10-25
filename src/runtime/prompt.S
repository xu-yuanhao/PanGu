#include "tools_layout.h"
.intel_syntax noprefix
.section .rodata
.Lpath: .asciz "path"
.Lread: .asciz "read"
.text
.globl pg_prompt_from_file
.type pg_prompt_from_file,@function
pg_prompt_from_file:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,56
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov r15,r8
 mov rbp,r9
 mov qword ptr [rsp],0
 mov qword ptr [rsp+48],0
 mov dword ptr [rsp+40],1
 test rbp,rbp
 jz .Lreturn
 mov qword ptr [rbp],0
 test r14,r14
 jz .Lreturn
 test r15,r15
 jz .Lreturn
 mov byte ptr [r14],0
 test rbx,rbx
 jz .Lreturn
 cmp qword ptr [rbx+PG_TL_SVC_AUTHORIZE],0
 je .Lreturn
 cmp qword ptr [rbx+PG_TL_SVC_EXECUTE],0
 je .Lreturn
 test r12,r12
 jz .Lreturn
 test r13,r13
 jz .Lreturn
 cmp r13,4096
 jae .Lreturn
 mov rdi,r12
 xor esi,esi
 mov rdx,r13
 call memchr@PLT
 test rax,rax
 jnz .Lreturn
 call json_object_new_object@PLT
 test rax,rax
 jz .Lreturn
 mov [rsp],rax
 mov rdi,r12
 mov esi,r13d
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Lcleanup
 mov [rsp+8],rax
 mov qword ptr [rsp+48],1
 mov rdi,[rsp]
 lea rsi,[rip+.Lpath]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lcleanup
 mov qword ptr [rsp+48],0
 mov rdi,[rsp]
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Lcleanup
 mov [rsp+16],rax
 mov rdi,rax
 call strlen@PLT
 cmp rax,4096
 jae .Lcleanup
 mov [rsp+24],rax
 mov rdi,[rbx]
 lea rsi,[rip+.Lread]
 mov edx,4
 mov rcx,[rsp+16]
 mov r8,rax
 call [rbx+PG_TL_SVC_AUTHORIZE]
 cmp eax,1
 jne .Lcleanup
 mov qword ptr [rsp+32],0
 mov rdi,[rbx]
 lea rsi,[rip+.Lread]
 mov edx,4
 mov rcx,[rsp+16]
 mov r8,[rsp+24]
 mov r9,r14
 lea rax,[rsp+32]
 push rax
 push r15
 call [rbx+PG_TL_SVC_EXECUTE]
 add rsp,16
 test eax,eax
 jnz .Lcleanup
 mov rax,[rsp+32]
 cmp rax,r15
 jae .Lcleanup
 cmp rax,4096
 jae .Lcleanup
 mov [rbp],rax
 mov byte ptr [r14+rax],0
 mov dword ptr [rsp+40],0
.Lcleanup:
 cmp qword ptr [rsp+48],0
 je .Lroot
 mov rdi,[rsp+8]
 call json_object_put@PLT
.Lroot:
 mov rdi,[rsp]
 test rdi,rdi
 jz .Lreturn
 call json_object_put@PLT
.Lreturn:
 cmp dword ptr [rsp+40],0
 je .Lout
 test rbp,rbp
 jz .Lout
 mov qword ptr [rbp],0
 test r14,r14
 jz .Lout
 test r15,r15
 jz .Lout
 mov byte ptr [r14],0
.Lout:
 mov eax,[rsp+40]
 add rsp,56
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_prompt_from_file,.-pg_prompt_from_file
.section .note.GNU-stack,"",@progbits
