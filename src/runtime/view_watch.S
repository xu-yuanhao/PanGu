/* Pi Durable task-graph.ts/view.ts Watch attach/coalescing/close.
   d5b3de22d0e24a3e9c38a9410d3329b099717283; MIT, docs/licenses/pi.md. */
.intel_syntax noprefix
.macro QF n
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\n
.endm
.macro QE n
 add rsp,\n
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.text
/* Refresh the shared view without changing observers' delivery baselines. */
.Lq_refresh:
 push rbx
 sub rsp,16
 mov rbx,rdi
 mov qword ptr [rsp],0
 cmp dword ptr [rbx+24],0
 jne .Lq_refresh_closed
 mov rdi,[rbx]
 mov rsi,rsp
 call [rbx+32]
 test eax,eax
 jnz .Lq_refresh_end
 cmp dword ptr [rbx+24],0
 jne .Lq_refresh_late_close
 mov rdi,[rbx+16]
 call json_object_put@PLT
 mov rax,[rsp]
 mov [rbx+16],rax
 xor eax,eax
 jmp .Lq_refresh_end
.Lq_refresh_late_close:
 mov rdi,[rsp]
 call json_object_put@PLT
.Lq_refresh_closed:
 mov eax,7
.Lq_refresh_end:
 add rsp,16
 pop rbx
 ret
.globl pg_view_watch_open
.type pg_view_watch_open,@function
pg_view_watch_open:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 cmp dword ptr [r12+24],0
 jne .Lq_watch_invalid
 call .Lq_refresh
 test eax,eax
 jnz .Lq_watch_open_end
 mov rdi,[rbx+16]
 call json_object_get@PLT
 mov [r12+16],rax
 mov [r12],rbx
 mov rax,[rbx+8]
 mov [r12+8],rax
 mov [rbx+8],r12
 mov dword ptr [r12+24],1
 mov dword ptr [r12+28],1
 xor eax,eax
 jmp .Lq_watch_open_end
.Lq_watch_invalid:
 mov eax,1
.Lq_watch_open_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.globl pg_view_watch_next
.type pg_view_watch_next,@function
pg_view_watch_next:
 QF 32
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [r12],0
 mov qword ptr [rsp],0
 cmp dword ptr [rbx+24],1
 jne .Lq_watch_next_closed
 mov r13,[rbx]
 cmp dword ptr [r13+24],0
 jne .Lq_watch_next_closed
 cmp dword ptr [rbx+28],0
 jne .Lq_watch_initial
 mov rdi,r13
 call .Lq_refresh
 test eax,eax
 jnz .Lq_watch_next_error
 cmp dword ptr [rbx+24],1
 jne .Lq_watch_next_closed
 mov rdi,[rbx+16]
 mov rsi,[r13+16]
 call json_object_equal@PLT
 test eax,eax
 jnz .Lq_watch_unchanged
 mov r14,[r13+16]
 jmp .Lq_watch_deliver
.Lq_watch_initial:
 mov r14,[rbx+16]
.Lq_watch_deliver:
 mov rdi,r14
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lq_watch_copy_failed
 mov rdi,r14
 call json_object_get@PLT
 mov [rsp+8],rax
 mov rdi,[rbx+16]
 call json_object_put@PLT
 mov rax,[rsp+8]
 mov [rbx+16],rax
 mov dword ptr [rbx+28],0
 mov rax,[rsp]
 mov [r12],rax
 mov eax,1
 jmp .Lq_watch_next_end
.Lq_watch_unchanged:
 xor eax,eax
 jmp .Lq_watch_next_end
.Lq_watch_copy_failed:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,-5
 jmp .Lq_watch_next_end
.Lq_watch_next_error:
 neg eax
 jmp .Lq_watch_next_end
.Lq_watch_next_closed:
 mov eax,-7
.Lq_watch_next_end:
 QE 32
.globl pg_view_watch_close
.type pg_view_watch_close,@function
pg_view_watch_close:
 push rbx
 mov rbx,rdi
 test rdi,rdi
 jz .Lq_watch_close_end
 cmp dword ptr [rbx+24],0
 je .Lq_watch_close_end
 mov rax,[rbx]
 lea rcx,[rax+8]
.Lq_watch_unlink:
 mov rax,[rcx]
 test rax,rax
 jz .Lq_watch_unlinked
 cmp rax,rbx
 je .Lq_watch_remove
 lea rcx,[rax+8]
 jmp .Lq_watch_unlink
.Lq_watch_remove:
 mov rax,[rbx+8]
 mov [rcx],rax
.Lq_watch_unlinked:
 mov rdi,[rbx+16]
 call json_object_put@PLT
 pxor xmm0,xmm0
 movdqu [rbx],xmm0
 movdqu [rbx+16],xmm0
.Lq_watch_close_end:
 pop rbx
 ret
.globl pg_view_binding_close
.type pg_view_binding_close,@function
pg_view_binding_close:
 push rbx
 mov rbx,rdi
 mov dword ptr [rbx+24],1
.Lq_binding_close_each:
 mov rdi,[rbx+8]
 test rdi,rdi
 jz .Lq_binding_close_value
 call pg_view_watch_close
 jmp .Lq_binding_close_each
.Lq_binding_close_value:
 mov rdi,[rbx+16]
 call json_object_put@PLT
 mov qword ptr [rbx+16],0
 pop rbx
 ret

.text
.globl pg_view_binding_init
.type pg_view_binding_init,@function
pg_view_binding_init:
 mov [rdi],rsi
 mov qword ptr [rdi+8],0
 mov qword ptr [rdi+16],0
 mov qword ptr [rdi+24],0
 mov [rdi+32],rdx
 ret
.section .note.GNU-stack,"",@progbits
