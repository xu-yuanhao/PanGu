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

.text
.globl pg_task_extensions_validate
.type pg_task_extensions_validate,@function
pg_task_extensions_validate:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov qword ptr [rsp],0
 xor r12d,r12d
 xor r13d,r13d
.Lte_count:
 cmp r12,[rbx+16]
 jae .Lte_allocate
 mov rax,[rbx+8]
 mov rax,[rax+r12*8]
 mov rcx,[rax+80]
 test rcx,rcx
 jz .Lte_count_next
 cmp qword ptr [rax+72],0
 je .Lte_bad
 add r13,rcx
 jc .Lte_bad
 cmp r13,2147483647
 ja .Lte_bad
.Lte_count_next:
 inc r12
 jmp .Lte_count
.Lte_allocate:
 test r13,r13
 jz .Lte_ok
 mov rdi,r13
 mov esi,8
 call calloc@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lte_bad
 xor r12d,r12d
 xor r14d,r14d
.Lte_copy_extension:
 cmp r12,[rbx+16]
 jae .Lte_validate
 mov rax,[rbx+8]
 mov rax,[rax+r12*8]
 xor ebp,ebp
.Lte_copy_task:
 cmp rbp,[rax+80]
 jae .Lte_copy_next
 mov rdx,[rax+72]
 mov rdx,[rdx+rbp*8]
 mov rcx,[rsp]
 mov [rcx+r14*8],rdx
 inc r14
 inc rbp
 jmp .Lte_copy_task
.Lte_copy_next:
 inc r12
 jmp .Lte_copy_extension
.Lte_validate:
 mov dword ptr [rsp+8],1
 mov dword ptr [rsp+12],24
 mov rax,[rsp]
 mov [rsp+16],rax
 mov [rsp+24],r13
 lea rdi,[rsp+8]
 lea rsi,[rip+.Lte_builtins]
 mov edx,3
 call pg_task_provider_validate
 test eax,eax
 jnz .Lte_bad
.Lte_ok:
 xor ebp,ebp
 jmp .Lte_end
.Lte_bad:
 mov ebp,5
.Lte_end:
 mov rdi,[rsp]
 call free@PLT
 mov eax,ebp
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lte_generation: .asciz "pangu.generation"
.Lte_tool: .asciz "pangu.tool"
.Lte_compaction: .asciz "pangu.compaction"
.section .data.rel.ro
.balign 8
.Lte_generation_def: .quad .Lte_generation,16
.Lte_tool_def: .quad .Lte_tool,10
.Lte_compaction_def: .quad .Lte_compaction,16
.Lte_builtins: .quad .Lte_generation_def,.Lte_tool_def,.Lte_compaction_def
