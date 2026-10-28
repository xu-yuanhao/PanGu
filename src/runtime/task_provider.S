/* Pi Durable harness/registry.ts Snapshot tasks; Host lifetime follows
   Harness/Cordis service scope. Pi d5b3de22, MIT docs/licenses/pi.md. */
.intel_syntax noprefix
.text
.globl pg_task_provider_find
.type pg_task_provider_find,@function
pg_task_provider_find:
 push rbx
 push r12
 push r13
 push r14
 push rbp
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov qword ptr [rcx],0
 test rbx,rbx
 jz .Ltp_find_missing
 xor ebp,ebp
.Ltp_find_each:
 cmp rbp,[rbx+16]
 jae .Ltp_find_missing
 mov rax,[rbx+8]
 mov rax,[rax+rbp*8]
 cmp r13,[rax+8]
 jne .Ltp_find_next
 mov rdi,[rax]
 mov rsi,r12
 mov rdx,r13
 call memcmp@PLT
 test eax,eax
 jnz .Ltp_find_next
 mov rax,[rbx+8]
 mov rax,[rax+rbp*8]
 mov [r14],rax
 xor eax,eax
 jmp .Ltp_find_end
.Ltp_find_next:
 inc rbp
 jmp .Ltp_find_each
.Ltp_find_missing:
 mov eax,1
.Ltp_find_end:
 pop rbp
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.globl pg_task_provider_validate
.type pg_task_provider_validate,@function
pg_task_provider_validate:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov [rsp],rdx
 test rbx,rbx
 jz .Ltp_valid
 cmp dword ptr [rbx],1
 jne .Ltp_invalid
 cmp dword ptr [rbx+4],24
 jne .Ltp_invalid
 cmp qword ptr [rbx+16],0
 je .Ltp_valid
 cmp qword ptr [rbx+8],0
 je .Ltp_invalid
 xor r13d,r13d
.Ltp_validate_each:
 cmp r13,[rbx+16]
 jae .Ltp_valid
 mov rax,[rbx+8]
 mov r14,[rax+r13*8]
 test r14,r14
 jz .Ltp_invalid
 cmp qword ptr [r14],0
 je .Ltp_invalid
 cmp qword ptr [r14+8],0
 je .Ltp_invalid
 cmp qword ptr [r14+8],2147483647
 ja .Ltp_invalid
 cmp qword ptr [r14+16],0
 je .Ltp_invalid
 mov rax,9007199254740991
 cmp [r14+16],rax
 ja .Ltp_invalid
 cmp qword ptr [r14+32],0
 je .Ltp_invalid
 cmp qword ptr [r14+40],0
 je .Ltp_invalid
 cmp qword ptr [r14+48],0
 je .Ltp_invalid
 xor ebp,ebp
.Ltp_validate_builtin:
 cmp rbp,[rsp]
 jae .Ltp_validate_previous_start
 mov rax,[r12+rbp*8]
 mov rdi,r14
 mov rsi,rax
 call .Ltp_same_name
 test eax,eax
 jnz .Ltp_invalid
 inc rbp
 jmp .Ltp_validate_builtin
.Ltp_validate_previous_start:
 xor ebp,ebp
.Ltp_validate_previous:
 cmp rbp,r13
 jae .Ltp_validate_next
 mov rax,[rbx+8]
 mov rsi,[rax+rbp*8]
 mov rdi,r14
 call .Ltp_same_name
 test eax,eax
 jnz .Ltp_invalid
 inc rbp
 jmp .Ltp_validate_previous
.Ltp_validate_next:
 inc r13
 jmp .Ltp_validate_each
.Ltp_valid:
 xor eax,eax
 jmp .Ltp_validate_end
.Ltp_invalid:
 mov eax,1
.Ltp_validate_end:
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Ltp_same_name:
 mov rdx,[rdi+8]
 cmp rdx,[rsi+8]
 jne .Ltp_different
 sub rsp,8
 mov rdi,[rdi]
 mov rsi,[rsi]
 call memcmp@PLT
 test eax,eax
 setz al
 movzx eax,al
 add rsp,8
 ret
.Ltp_different:
 xor eax,eax
 ret
.section .note.GNU-stack,"",@progbits
