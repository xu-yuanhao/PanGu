/* Pi Durable harness/registry.ts immutable publication and name replacement;
   d5b3de22d0e24a3e9c38a9410d3329b099717283. MIT docs/licenses/pi.md.
   Native owners retain descriptor/code leases across retired snapshots. */
.intel_syntax noprefix
.macro RF n
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\n
.endm
.macro RE n
 add rsp,\n
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.text
/* Node: extension, owner context, retain, release, epoch references. */
.Lr_node_new:
 RF 16
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
 test r12,r12
 jz .Lr_node_allocate
 cmp qword ptr [r12+8],0
 je .Lr_node_no_retain
 cmp qword ptr [r12+16],0
 je .Lr_node_end
 jmp .Lr_node_allocate
.Lr_node_no_retain:
 cmp qword ptr [r12+16],0
 jne .Lr_node_end
.Lr_node_allocate:
 mov edi,1
 mov esi,40
 call calloc@PLT
 mov r13,rax
 test rax,rax
 jz .Lr_node_end
 mov [rax],rbx
 mov qword ptr [rax+32],1
 test r12,r12
 jz .Lr_node_end
 mov rax,[r12]
 mov [r13+8],rax
 mov rax,[r12+8]
 mov [r13+16],rax
 mov rax,[r12+16]
 mov [r13+24],rax
 cmp qword ptr [r12+8],0
 je .Lr_node_end
 mov rdi,[r12]
 call [r12+8]
 test eax,eax
 jz .Lr_node_end
 mov rdi,r13
 call free@PLT
 xor r13d,r13d
.Lr_node_end:
 mov rax,r13
 RE 16
.Lr_node_release:
 push rbx
 mov rbx,rdi
 dec qword ptr [rbx+32]
 jnz .Lr_node_release_end
 cmp qword ptr [rbx+24],0
 je .Lr_node_free
 mov rdi,[rbx+8]
 call [rbx+24]
.Lr_node_free:
 mov rdi,rbx
 call free@PLT
.Lr_node_release_end:
 pop rbx
 ret
/* Epoch: immutable provider[72], refs, registry, node pointers; then both arrays. */
.Lr_epoch_new:
 RF 16
 mov rbx,rdi
 mov r12,rsi
 shl rsi,4
 jc .Lr_epoch_new_bad
 add rsi,96
 jc .Lr_epoch_new_bad
 mov edi,1
 call calloc@PLT
 mov r13,rax
 test rax,rax
 jz .Lr_epoch_new_end
 mov rdi,rax
 mov rsi,rbx
 mov ecx,6
 rep movsq
 mov [r13+16],r12
 lea rax,[r13+96]
 mov [r13+8],rax
 lea rax,[rax+r12*8]
 mov [r13+88],rax
 mov qword ptr [r13+72],1
 mov [r13+80],rbx
 mov rdi,[rbx+24]
 call json_object_get@PLT
 mov [r13+24],rax
 inc qword ptr [rbx+88]
 jmp .Lr_epoch_new_end
.Lr_epoch_new_bad:
 xor r13d,r13d
.Lr_epoch_new_end:
 mov rax,r13
 RE 16
.globl pg_extension_registry_release
.type pg_extension_registry_release,@function
pg_extension_registry_release:
 RF 16
 mov rbx,rdi
 test rbx,rbx
 jz .Lr_release_end
 dec qword ptr [rbx+72]
 jnz .Lr_release_end
 mov r12,[rbx+80]
 mov eax,[r12+100]
 mov [rsp],eax
 mov dword ptr [r12+100],1
 xor r13d,r13d
.Lr_release_nodes:
 cmp r13,[rbx+16]
 jae .Lr_release_epoch
 mov rax,[rbx+88]
 mov rdi,[rax+r13*8]
 test rdi,rdi
 jz .Lr_release_next
 call .Lr_node_release
.Lr_release_next:
 inc r13
 jmp .Lr_release_nodes
.Lr_release_epoch:
 mov rdi,[rbx+24]
 call json_object_put@PLT
 mov rdi,rbx
 call free@PLT
 dec qword ptr [r12+88]
 mov eax,[rsp]
 mov [r12+100],eax
.Lr_release_end:
 RE 16
.Lr_acquire:
 mov qword ptr [rsi],0
 mov qword ptr [rdx],0
 cmp dword ptr [rdi+96],0
 jne .Lr_acquire_closed
 cmp dword ptr [rdi+100],0
 jne .Lr_acquire_busy
 mov rax,[rdi+72]
 test rax,rax
 jz .Lr_acquire_closed
 add qword ptr [rax+72],1
 jc .Lr_acquire_overflow
 mov [rsi],rax
 mov [rdx],rax
 xor eax,eax
 ret
.Lr_acquire_overflow:
 dec qword ptr [rax+72]
 mov eax,5
 ret
.Lr_acquire_closed:
 mov eax,7
 ret
.Lr_acquire_busy:
 mov eax,3
 ret
.globl pg_extension_registry_open
.type pg_extension_registry_open,@function
pg_extension_registry_open:
 RF 32
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 cmp qword ptr [rbx+72],0
 jne .Lr_open_bad
 cmp qword ptr [rbx+88],0
 jne .Lr_open_bad
 mov rdi,r12
 call pg_extensions_validate@PLT
 test eax,eax
 jnz .Lr_open_bad
 test r12,r12
 jz .Lr_open_clear
 cmp qword ptr [r12+56],0
 jne .Lr_open_bad
.Lr_open_clear:
 mov rdi,rbx
 xor eax,eax
 mov ecx,13
 rep stosq
 mov dword ptr [rbx],11
 mov dword ptr [rbx+4],72
 test r12,r12
 jz .Lr_open_facade
 mov rdi,rbx
 mov rsi,r12
 mov ecx,6
 rep movsq
 mov qword ptr [rbx+8],0
 mov qword ptr [rbx+16],0
 mov qword ptr [rbx+24],0
 cmp qword ptr [r12+24],0
 je .Lr_open_facade
 mov rdi,[r12+24]
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lr_open_release
 mov rdi,[r12+24]
 lea rsi,[rbx+24]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lr_open_release
.Lr_open_facade:
 mov [rbx+48],rbx
 lea rax,[rip+.Lr_acquire]
 mov [rbx+56],rax
 lea rax,[rip+pg_extension_registry_release]
 mov [rbx+64],rax
 xor esi,esi
 test r12,r12
 jz .Lr_open_epoch
 mov rsi,[r12+16]
.Lr_open_epoch:
 mov rdi,rbx
 call .Lr_epoch_new
 mov [rsp],rax
 test rax,rax
 jz .Lr_open_release
 mov r13,rax
 xor r14d,r14d
.Lr_open_nodes:
 cmp r14,[r13+16]
 jae .Lr_open_publish
 mov rax,[r12+8]
 mov rdi,[rax+r14*8]
 xor esi,esi
 call .Lr_node_new
 test rax,rax
 jz .Lr_open_release
 mov rdx,[r13+88]
 mov [rdx+r14*8],rax
 mov rax,[rax]
 mov rdx,[r13+8]
 mov [rdx+r14*8],rax
 inc r14
 jmp .Lr_open_nodes
.Lr_open_publish:
 mov [rbx+72],r13
 xor eax,eax
 jmp .Lr_open_end
.Lr_open_release:
 mov rax,[rsp]
 mov [rbx+72],rax
 mov rdi,rbx
 call pg_extension_registry_close
.Lr_open_bad:
 mov eax,5
.Lr_open_end:
 RE 32
/* Find by stable name; absent returns count. */
.Lr_find:
 RF 16
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
.Lr_find_each:
 cmp r13,[rbx+16]
 jae .Lr_find_end
 mov rax,[rbx+8]
 mov rax,[rax+r13*8]
 mov rdi,[rax]
 mov rsi,r12
 call strcmp@PLT
 test eax,eax
 jz .Lr_find_end
 inc r13
 jmp .Lr_find_each
.Lr_find_end:
 mov rax,r13
 RE 16
.globl pg_extension_registry_install
.type pg_extension_registry_install,@function
pg_extension_registry_install:
 RF 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 cmp dword ptr [rbx+96],0
 jne .Lr_install_closed
 cmp dword ptr [rbx+100],0
 jne .Lr_install_busy
 test r12,r12
 jz .Lr_install_bad
 cmp qword ptr [r12],0
 je .Lr_install_bad
 mov dword ptr [rbx+100],1
 mov r14,[rbx+72]
 mov rdi,r14
 mov rsi,[r12]
 call .Lr_find
 mov [rsp+16],rax
 mov rsi,[r14+16]
 cmp rax,rsi
 jb .Lr_install_count
 inc rsi
.Lr_install_count:
 mov rdi,rbx
 call .Lr_epoch_new
 mov [rsp],rax
 test rax,rax
 jz .Lr_install_release
 mov rdi,r12
 mov rsi,r13
 call .Lr_node_new
 mov [rsp+8],rax
 test rax,rax
 jz .Lr_install_release
 xor ebp,ebp
.Lr_install_copy:
 mov rax,[rsp]
 cmp rbp,[rax+16]
 jae .Lr_install_validate
 mov rdx,[rsp+8]
 cmp rbp,[rsp+16]
 je .Lr_install_node
 mov rdx,[r14+88]
 mov rdx,[rdx+rbp*8]
.Lr_install_node:
 inc qword ptr [rdx+32]
 mov rcx,[rax+88]
 mov [rcx+rbp*8],rdx
 mov rdx,[rdx]
 mov rcx,[rax+8]
 mov [rcx+rbp*8],rdx
 inc rbp
 jmp .Lr_install_copy
.Lr_install_validate:
 mov rdi,[rsp]
 call pg_extensions_validate@PLT
 test eax,eax
 jnz .Lr_install_release
 mov rax,[rsp]
 mov [rbx+72],rax
 mov qword ptr [rsp],0
 inc qword ptr [rbx+80]
 mov rdi,r14
 call pg_extension_registry_release
 mov ebp,0
 jmp .Lr_install_finish
.Lr_install_release:
 mov ebp,5
.Lr_install_finish:
 mov rdi,[rsp]
 call pg_extension_registry_release
 mov rdi,[rsp+8]
 test rdi,rdi
 jz .Lr_install_clear
 call .Lr_node_release
.Lr_install_clear:
 mov dword ptr [rbx+100],0
 mov eax,ebp
 jmp .Lr_install_end
.Lr_install_bad:
 mov eax,5
 jmp .Lr_install_end
.Lr_install_closed:
 mov eax,7
 jmp .Lr_install_end
.Lr_install_busy:
 mov eax,3
.Lr_install_end:
 RE 48
.globl pg_extension_registry_remove
.type pg_extension_registry_remove,@function
pg_extension_registry_remove:
 RF 32
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 cmp dword ptr [rbx+96],0
 jne .Lr_remove_closed
 cmp dword ptr [rbx+100],0
 jne .Lr_remove_busy
 test r12,r12
 jz .Lr_remove_bad
 mov dword ptr [rbx+100],1
 mov r14,[rbx+72]
 mov rdi,r14
 mov rsi,r12
 call .Lr_find
 mov r13,rax
 cmp rax,[r14+16]
 je .Lr_remove_noop
 mov rsi,[r14+16]
 dec rsi
 mov rdi,rbx
 call .Lr_epoch_new
 mov [rsp],rax
 test rax,rax
 jz .Lr_remove_fail
 xor ebp,ebp
 xor r12d,r12d
.Lr_remove_copy:
 cmp rbp,[r14+16]
 jae .Lr_remove_publish
 cmp rbp,r13
 je .Lr_remove_next
 mov rax,[r14+88]
 mov rdx,[rax+rbp*8]
 inc qword ptr [rdx+32]
 mov rax,[rsp]
 mov rcx,[rax+88]
 mov [rcx+r12*8],rdx
 mov rdx,[rdx]
 mov rcx,[rax+8]
 mov [rcx+r12*8],rdx
 inc r12
.Lr_remove_next:
 inc rbp
 jmp .Lr_remove_copy
.Lr_remove_publish:
 mov rax,[rsp]
 mov [rbx+72],rax
 inc qword ptr [rbx+80]
 mov rdi,r14
 call pg_extension_registry_release
.Lr_remove_noop:
 mov dword ptr [rbx+100],0
 xor eax,eax
 jmp .Lr_remove_end
.Lr_remove_fail:
 mov dword ptr [rbx+100],0
.Lr_remove_bad:
 mov eax,5
 jmp .Lr_remove_end
.Lr_remove_closed:
 mov eax,7
 jmp .Lr_remove_end
.Lr_remove_busy:
 mov eax,3
.Lr_remove_end:
 RE 32
.globl pg_extension_registry_close
.type pg_extension_registry_close,@function
pg_extension_registry_close:
 push rbx
 mov rbx,rdi
 cmp dword ptr [rbx+96],2
 je .Lr_close_done
 cmp dword ptr [rbx+100],0
 jne .Lr_close_pending
 mov dword ptr [rbx+100],1
 cmp dword ptr [rbx+96],0
 jne .Lr_close_wait
 mov dword ptr [rbx+96],1
 mov rdi,[rbx+72]
 mov qword ptr [rbx+72],0
 call pg_extension_registry_release
.Lr_close_wait:
 cmp qword ptr [rbx+88],0
 jne .Lr_close_wait_pending
 mov rdi,[rbx+24]
 call json_object_put@PLT
 mov qword ptr [rbx+24],0
 mov dword ptr [rbx+96],2
 mov dword ptr [rbx+100],0
.Lr_close_done:
 xor eax,eax
 pop rbx
 ret
.Lr_close_wait_pending:
 mov dword ptr [rbx+100],0
.Lr_close_pending:
 mov eax,1
 pop rbx
 ret
.globl pg_extension_registry_service
.type pg_extension_registry_service,@function
pg_extension_registry_service:
 mov [rsi],rdi
 lea rax,[rip+pg_extension_registry_install]
 mov [rsi+8],rax
 lea rax,[rip+pg_extension_registry_remove]
 mov [rsi+16],rax
 lea rax,[rip+pg_extension_registry_describe]
 mov [rsi+24],rax
 ret
.section .note.GNU-stack,"",@progbits
.text
.globl pg_extension_registry_describe
.type pg_extension_registry_describe,@function
pg_extension_registry_describe:
 RF 32
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [r12],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 cmp dword ptr [rbx+96],0
 jne .Lr_describe_closed
 cmp dword ptr [rbx+100],0
 jne .Lr_describe_busy
 mov r13,[rbx+72]
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lr_describe_bad
 mov rdi,[rbx+80]
 call json_object_new_uint64@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lr_describe_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lr_revision]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lr_describe_bad
 mov qword ptr [rsp+16],0
 call json_object_new_array@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lr_describe_bad
 xor r14d,r14d
.Lr_describe_each:
 cmp r14,[r13+16]
 jae .Lr_describe_names
 mov rax,[r13+8]
 mov rax,[rax+r14*8]
 mov rdi,[rax]
 call json_object_new_string@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lr_describe_bad
 mov rdi,[rsp+8]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lr_describe_bad
 mov qword ptr [rsp+16],0
 inc r14
 jmp .Lr_describe_each
.Lr_describe_names:
 mov rdi,[rsp]
 lea rsi,[rip+.Lr_extensions]
 mov rdx,[rsp+8]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lr_describe_bad
 mov qword ptr [rsp+8],0
 mov rax,[rsp]
 mov [r12],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
 jmp .Lr_describe_end
.Lr_describe_bad:
 mov ebp,5
 jmp .Lr_describe_end
.Lr_describe_closed:
 mov ebp,7
 jmp .Lr_describe_end
.Lr_describe_busy:
 mov ebp,3
.Lr_describe_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,ebp
 RE 32
.section .rodata
.Lr_revision: .asciz "revision"
.Lr_extensions: .asciz "extensions"

.text
.globl pg_extension_registry_task
.type pg_extension_registry_task,@function
pg_extension_registry_task:
 RF 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov [rsp+24],rcx
 mov [rsp+32],r8
 mov qword ptr [rcx],0
 mov qword ptr [r8],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 lea rsi,[rsp]
 lea rdx,[rsp+8]
 call .Lr_acquire
 mov ebp,eax
 test eax,eax
 jnz .Lrt_end
 xor r14d,r14d
.Lrt_extension:
 mov rax,[rsp]
 cmp r14,[rax+16]
 jae .Lrt_missing
 mov rax,[rax+8]
 mov rax,[rax+r14*8]
 mov rcx,[rax+72]
 /* A borrowed provider is assembled in a second frame for the lookup. */
 sub rsp,32
 mov dword ptr [rsp],1
 mov dword ptr [rsp+4],24
 mov [rsp+8],rcx
 mov rax,[rax+80]
 mov [rsp+16],rax
 mov rdi,rsp
 mov rsi,r12
 mov rdx,r13
 mov rcx,[rsp+56]
 call pg_task_provider_find@PLT
 add rsp,32
 test eax,eax
 jz .Lrt_found
 inc r14
 jmp .Lrt_extension
.Lrt_found:
 mov rax,[rsp+8]
 mov rdx,[rsp+32]
 mov [rdx],rax
 mov qword ptr [rsp+8],0
 xor ebp,ebp
 jmp .Lrt_release
.Lrt_missing:
 mov ebp,1
.Lrt_release:
 mov rdi,[rsp+8]
 call pg_extension_registry_release
.Lrt_end:
 mov eax,ebp
 RE 48
