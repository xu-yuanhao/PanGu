/* Pi d5b3de22 Session.snapshot/snapshotAsOf; MIT: docs/licenses/pi.md. */
.intel_syntax noprefix
.text
/* Entry guard; pending/closed/failed do not start backend work. */
.Lreader_guard:
 mov eax,1
 test rdi,rdi
 jz .Lreader_guard_end
 cmp dword ptr [rdi+172],1
 jne .Lreader_guard_phase
 mov rdx,[rdi]
 cmp dword ptr [rdx+52],0
 jne .Lreader_guard_failed
 cmp dword ptr [rdi+168],0
 jne .Lreader_guard_pending
 xor eax,eax
 ret
.Lreader_guard_phase:
 cmp dword ptr [rdi+172],2
 jb .Lreader_guard_end
 mov eax,14
 ret
.Lreader_guard_failed:
 mov eax,13
 ret
.Lreader_guard_pending:
 mov eax,11
.Lreader_guard_end:
 ret
.Lreader_storage_failed:
 mov rax,[rdi]
 cmp dword ptr [rax+52],0
 jne .Lreader_storage_latched
 mov dword ptr [rax+52],3
.Lreader_storage_latched:
 mov eax,9
 ret

.Lreader_find:
 mov rax,[rdi+16]
 jmp .Lreader_call
.Lreader_read:
 mov rax,[rdi+24]
.Lreader_call:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rax
 mov rax,[rbx]
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 jne .Lreader_crossed
 mov rdi,[rbx+8]
 call r12
 mov r12d,eax
 mov rax,[rbx]
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 jne .Lreader_crossed
 cmp r12d,68
 je .Lreader_call_result
 cmp r12d,1
 jbe .Lreader_call_result
 mov dword ptr [rax+52],3
.Lreader_call_result:
 mov eax,r12d
 jmp .Lreader_call_end
.Lreader_crossed:
 mov qword ptr [rbx+32],1
 mov eax,7
.Lreader_call_end:
 add rsp,8
 pop r12
 pop rbx
 ret

.globl pg_durable_snapshot
.type pg_durable_snapshot,@function
pg_durable_snapshot:
 xor r8d,r8d
 jmp .Lsnapshot_common
.globl pg_durable_observe
.type pg_durable_observe,@function
pg_durable_observe:
 mov eax,1
 test rsi,rsi
 jz .Lobserve_invalid
 mov r8,rsi
 mov rcx,rdx
 mov rdx,[rsi+8]
 mov rsi,[rsi]
 jmp .Lsnapshot_common
.Lobserve_invalid:
 ret
.Lsnapshot_common:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,88
 mov r15,r8
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov ebp,1
 mov rdi,rsp
 xor eax,eax
 mov ecx,10
 rep stosq
 test r14,r14
 jz .Lsnapshot_end
 test r15,r15
 jz .Lsnapshot_output_clear
 cmp dword ptr [r14+32],0
 jne .Lsnapshot_end
 cmp dword ptr [r14+36],0
 jne .Lsnapshot_end
 cmp dword ptr [r14+96],0
 jne .Lsnapshot_end
 cmp dword ptr [r14+100],0
 jne .Lsnapshot_end
 mov rax,[r15+16]
 test rax,rax
 jz .Lsnapshot_end
 test rbx,rbx
 jz .Lsnapshot_end
 mov rcx,[rbx]
 cmp [rax],rcx
 jne .Lsnapshot_end
 cmp qword ptr [r15+24],0
 je .Lsnapshot_end
 jmp .Lsnapshot_guard
.Lsnapshot_output_clear:
 mov qword ptr [r14],0
.Lsnapshot_guard:
 mov rdi,rbx
 call .Lreader_guard
 mov ebp,eax
 test eax,eax
 jnz .Lsnapshot_end
 mov dword ptr [rbx+168],1
 test r15,r15
 jz .Lsnapshot_cache
 cmp qword ptr [rbx+160],0
 jne .Lsnapshot_pending
 mov rax,[r15+16]
 cmp dword ptr [rax+48],0
 jne .Lsnapshot_pending
 call .Lobserve_cancelled
 test eax,eax
 jnz .Lobserve_cancelled_result
.Lsnapshot_cache:
 mov rdi,[rbx+8]
 mov rsi,r12
 mov rdx,r13
 mov rcx,rsp
 call pg_document_cache_peek@PLT
 cmp eax,8
 jne .Lsnapshot_loaded
 cmp qword ptr [rbx+160],0
 jne .Lsnapshot_pending
 mov rax,[rbx+8]
 mov [rsp+16],rbx
 mov rcx,[rax+40]
 mov [rsp+24],rcx
 mov rcx,[rax+48]
 mov [rsp+32],rcx
 mov rcx,[rax+56]
 mov [rsp+40],rcx
 lea rcx,[rsp+16]
 mov [rax+40],rcx
 lea rcx,[rip+.Lreader_find]
 mov [rax+48],rcx
 lea rcx,[rip+.Lreader_read]
 mov [rax+56],rcx
 mov qword ptr [rsp+8],1
 mov rdi,rax
 mov rsi,r12
 mov rdx,r13
 mov rcx,rsp
 call pg_document_cache_get@PLT
.Lsnapshot_loaded:
 mov ebp,eax
 test r15,r15
 jz .Lsnapshot_loaded_status
 test eax,eax
 jz .Lobserve_check_loaded
 cmp eax,8
 jne .Lsnapshot_loaded_status
.Lobserve_check_loaded:
 call .Lobserve_cancelled
 test eax,eax
 jnz .Lobserve_cancelled_result
 mov eax,ebp
.Lsnapshot_loaded_status:
 cmp eax,9
 je .Lsnapshot_storage
 test eax,eax
 jnz .Lsnapshot_unlock
 mov rax,[rbx]
 cmp dword ptr [rax+52],0
 jne .Lsnapshot_failed
 test r15,r15
 jnz .Lobserve_attach
 mov rax,[rsp]
 mov rdi,[rax+8]
 mov rsi,r14
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jz .Lsnapshot_unlock
 mov rdi,[r14]
 call json_object_put@PLT
 mov qword ptr [r14],0
 mov ebp,3
 jmp .Lsnapshot_unlock
.Lobserve_attach:
 cmp dword ptr [rbx+172],1
 jne .Lobserve_closed
 mov rdi,r14
 mov rsi,[r15+16]
 mov rdx,[rsp]
 mov rcx,[r15+24]
 call pg_document_observer_open@PLT
 mov ebp,eax
 test eax,eax
 jnz .Lsnapshot_unlock
 call .Lobserve_cancelled
 test eax,eax
 jz .Lsnapshot_unlock
 mov rdi,r14
 call pg_document_observer_release@PLT
.Lobserve_cancelled_result:
 mov ebp,15
 jmp .Lsnapshot_unlock
.Lobserve_closed:
 mov ebp,14
 jmp .Lsnapshot_unlock
.Lsnapshot_storage:
 cmp qword ptr [rsp+48],0
 jne .Lsnapshot_failed
 mov ebp,9
 jmp .Lsnapshot_unlock
.Lsnapshot_failed:
 mov ebp,13
 jmp .Lsnapshot_unlock
.Lsnapshot_pending:
 mov ebp,11
.Lsnapshot_unlock:
 cmp qword ptr [rsp+8],0
 je .Lsnapshot_restored
 mov rax,[rbx+8]
 mov rcx,[rsp+24]
 mov [rax+40],rcx
 mov rcx,[rsp+32]
 mov [rax+48],rcx
 mov rcx,[rsp+40]
 mov [rax+56],rcx
.Lsnapshot_restored:
 mov dword ptr [rbx+168],0
.Lsnapshot_end:
 mov eax,ebp
 add rsp,88
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_durable_snapshot,.-pg_durable_snapshot
.size pg_durable_observe,.-pg_durable_observe

.Lobserve_cancelled:
 mov rax,[r15+40]
 test rax,rax
 jz .Lobserve_not_cancelled
 mov rdi,[r15+32]
 jmp rax
.Lobserve_not_cancelled:
 xor eax,eax
 ret

.globl pg_durable_snapshot_as_of
.type pg_durable_snapshot_as_of,@function
pg_durable_snapshot_as_of:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,280
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov r15,r9
 mov rdi,rsp
 xor eax,eax
 mov ecx,35
 rep stosq
 mov [rsp+184],r8
 mov ebp,1
 test r15,r15
 jz .Lasof_end
 mov qword ptr [r15],0
 mov rdi,rbx
 call .Lreader_guard
 mov ebp,eax
 test eax,eax
 jnz .Lasof_end
 mov ebp,1
 test r12,r12
 jz .Lasof_end
 test r13,r13
 jz .Lasof_end
 cmp qword ptr [r12+8],0
 je .Lasof_end
 cmp qword ptr [r12+16],0
 je .Lasof_end
 cmp qword ptr [r12+32],0
 je .Lasof_end
 mov rdi,r14
 call pg_document_address_valid@PLT
 test eax,eax
 jz .Lasof_end
 cmp dword ptr [r14+40],1
 jne .Lasof_end
 cmp dword ptr [r13+24],1
 jne .Lasof_end
 mov rax,[rsp+184]
 test rax,rax
 jz .Lasof_end
 mov rdx,9007199254740991
 cmp rax,rdx
 ja .Lasof_end
 cmp qword ptr [rbx+160],0
 jne .Lasof_pending
 mov dword ptr [rbx+168],1
 mov rdi,[r12+24]
 mov rsi,[r14+32]
 mov rdx,[rsp+184]
 mov rcx,rsp
 lea r8,[rsp+8]
 call [r12+32]
 mov ebp,eax
 mov rax,[rbx]
 cmp dword ptr [rax+52],0
 jne .Lasof_failed
 mov eax,ebp
 test eax,eax
 jz .Lasof_entry
 cmp eax,1
 je .Lasof_invisible
 cmp eax,68
 jne .Lasof_storage
 mov ebp,9
 jmp .Lasof_cleanup
.Lasof_entry:
 mov rdi,[rsp]
 lea rsi,[rip+.Lreader_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number
 test edx,edx
 jz .Lasof_storage
 cmp rax,[rsp+184]
 jne .Lasof_storage
 mov rdi,[rsp]
 lea rsi,[rip+.Lreader_conversation]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number
 test rax,rax
 jz .Lasof_storage
 mov [rsp+192],rax
 mov rax,[rsp+8]
 test rax,rax
 jz .Lasof_storage
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lasof_storage
 lea rdi,[rsp+136]
 mov rsi,r14
 mov edx,48
 call memcpy@PLT
 mov rax,[rsp+192]
 mov [rsp+168],rax
 mov [rsp+200],rbx
 mov rax,[r12]
 mov [rsp+208],rax
 mov rax,[r12+8]
 mov [rsp+216],rax
 mov rax,[r12+16]
 mov [rsp+224],rax
 lea rax,[rsp+200]
 mov [rsp+240],rax
 lea rax,[rip+.Lreader_find]
 mov [rsp+248],rax
 lea rax,[rip+.Lreader_read]
 mov [rsp+256],rax
 lea rdi,[rsp+16]
 lea rsi,[rsp+240]
 mov rdx,r13
 lea rcx,[rsp+136]
 mov r8,[rsp+8]
 call pg_document_loaded_open_at@PLT
 mov ebp,eax
 cmp eax,9
 jne .Lasof_loaded_status
 cmp qword ptr [rsp+232],0
 jne .Lasof_failed
 mov ebp,9
 jmp .Lasof_cleanup
.Lasof_loaded_status:
 test eax,eax
 jnz .Lasof_cleanup
 mov rax,[rbx]
 cmp dword ptr [rax+52],0
 jne .Lasof_failed
 mov rax,[rsp+24]
 mov [r15],rax
 mov qword ptr [rsp+24],0
 jmp .Lasof_cleanup
.Lasof_storage:
 mov rdi,rbx
 call .Lreader_storage_failed
 mov ebp,eax
 jmp .Lasof_cleanup
.Lasof_failed:
 mov ebp,13
 jmp .Lasof_cleanup
.Lasof_invisible:
 mov ebp,12
.Lasof_cleanup:
 lea rdi,[rsp+16]
 call pg_document_loaded_close@PLT
 mov rdi,[rsp]
 call json_object_put@PLT
 mov dword ptr [rbx+168],0
 jmp .Lasof_end
.Lasof_pending:
 mov ebp,11
.Lasof_end:
 mov eax,ebp
 add rsp,280
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_durable_snapshot_as_of,.-pg_durable_snapshot_as_of
.section .rodata
.Lreader_id: .asciz "id"
.Lreader_conversation: .asciz "conversationId"
.section .note.GNU-stack,"",@progbits

.text
.globl pg_durable_watch
.type pg_durable_watch,@function
pg_durable_watch:
 push rbx
 push r12
 push r13
 sub rsp,112
 mov rbx,rdx
 mov r12,rdi
 mov r13,rsi
 mov eax,1
 test rbx,rbx
 jz .Ldurable_watch_end
 test r13,r13
 jz .Ldurable_watch_end
 cmp dword ptr [rbx+236],0
 jne .Ldurable_watch_end
 cmp dword ptr [rbx+256],0
 jne .Ldurable_watch_end
 mov rax,[r13+24]
 test rax,rax
 jz .Ldurable_watch_end
 cmp qword ptr [rax+8],0
 je .Ldurable_watch_end
 cmp qword ptr [rax+16],0
 je .Ldurable_watch_end
 mov rcx,[rax]
 mov [rbx+264],rcx
 mov rcx,[rax+8]
 mov [rbx+272],rcx
 mov rcx,[rax+16]
 mov [rbx+280],rcx
 mov [rsp],rbx
 lea rax,[rip+pg_document_watch_source_advance]
 mov [rsp+8],rax
 lea rax,[rip+pg_document_watch_source_end]
 mov [rsp+16],rax
 lea rax,[rip+pg_document_watch_source_pin]
 mov [rsp+24],rax
 lea rax,[rip+pg_document_watch_source_unpin]
 mov [rsp+32],rax
 mov rax,[r13]
 mov [rsp+40],rax
 mov rax,[r13+8]
 mov [rsp+48],rax
 mov rax,[r13+16]
 mov [rsp+56],rax
 mov rax,rsp
 mov [rsp+64],rax
 mov rax,[r13+32]
 mov [rsp+72],rax
 mov rax,[r13+40]
 mov [rsp+80],rax
 mov rdi,r12
 lea rsi,[rsp+40]
 mov rdx,rbx
 call pg_durable_observe
 test eax,eax
 jnz .Ldurable_watch_clear_pins
 mov [rsp+88],rbx
 lea rax,[rip+pg_document_watch_source_detach]
 mov [rsp+96],rax
 lea rax,[rip+pg_document_watch_source_report]
 mov [rsp+104],rax
 lea rdi,[rbx+104]
 mov rsi,[rbx+8]
 lea rdx,[rsp+88]
 call pg_document_watch_open@PLT
 test eax,eax
 jnz .Ldurable_watch_cleanup
 mov dword ptr [rbx+96],2
 jmp .Ldurable_watch_end
.Ldurable_watch_cleanup:
 mov rdi,rbx
 call pg_document_observer_release@PLT
 mov eax,3
.Ldurable_watch_clear_pins:
 pxor xmm0,xmm0
 movups [rbx+264],xmm0
 mov qword ptr [rbx+280],0
.Ldurable_watch_end:
 add rsp,112
 pop r13
 pop r12
 pop rbx
 ret
.size pg_durable_watch,.-pg_durable_watch
.section .note.GNU-stack,"",@progbits

.text
.Lwatch_acquire_cancelled:
 push rbx
 mov rbx,rdi
 mov rax,[rbx+8]
 test rax,rax
 jz .Lwatch_acquire_token
 mov rdi,[rbx]
 call rax
 test eax,eax
 jnz .Lwatch_acquire_cancelled_done
.Lwatch_acquire_token:
 mov rax,[rbx+16]
 mov rdi,[rax]
 mov rax,[rax+24]
 pop rbx
 jmp rax
.Lwatch_acquire_cancelled_done:
 pop rbx
 ret

.globl pg_durable_watch_cancellable
.type pg_durable_watch_cancellable,@function
pg_durable_watch_cancellable:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,80
 mov r12,rdi
 mov r13,rsi
 mov rbx,rdx
 mov r14,rcx
 mov r15,r8
 mov eax,1
 test r13,r13
 jz .Lwatch_cancellable_end
 test r14,r14
 jz .Lwatch_cancellable_end
 test r15,r15
 jz .Lwatch_cancellable_end
 cmp qword ptr [r14+72],0
 jne .Lwatch_cancellable_end
 cmp qword ptr [r15+8],0
 je .Lwatch_cancellable_end
 cmp qword ptr [r15+16],0
 je .Lwatch_cancellable_end
 cmp qword ptr [r15+24],0
 je .Lwatch_cancellable_end
 mov rax,[r13]
 mov [rsp],rax
 mov rax,[r13+8]
 mov [rsp+8],rax
 mov rax,[r13+16]
 mov [rsp+16],rax
 mov rax,[r13+24]
 mov [rsp+24],rax
 lea rax,[rsp+48]
 mov [rsp+32],rax
 lea rax,[rip+.Lwatch_acquire_cancelled]
 mov [rsp+40],rax
 mov rax,[r13+32]
 mov [rsp+48],rax
 mov rax,[r13+40]
 mov [rsp+56],rax
 mov [rsp+64],r15
 mov rdi,r12
 mov rsi,rsp
 mov rdx,rbx
 call pg_durable_watch
 test eax,eax
 jnz .Lwatch_cancellable_end
 lea rdi,[rbx+104]
 mov rsi,r14
 mov rdx,r15
 call pg_document_watch_observe_cancellation@PLT
 test eax,eax
 jnz .Lwatch_cancellable_error
 cmp dword ptr [rbx+228],2
 jne .Lwatch_cancellable_end
 mov r12d,15
 jmp .Lwatch_cancellable_cleanup
.Lwatch_cancellable_error:
 mov r12d,1
 cmp eax,4
 jne .Lwatch_cancellable_cleanup
 mov r12d,3
.Lwatch_cancellable_cleanup:
 mov rdi,rbx
 call pg_document_watch_source_release@PLT
 mov rdi,r14
 call pg_document_watch_cancellation_release@PLT
 mov eax,r12d
.Lwatch_cancellable_end:
 add rsp,80
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_durable_watch_cancellable,.-pg_durable_watch_cancellable
.section .note.GNU-stack,"",@progbits
