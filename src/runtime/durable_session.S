/* Pi d5b3de22 Session runCommit/loadDocument/close; MIT: docs/licenses/pi.md. */
.intel_syntax noprefix
.text
.globl pg_durable_session_open
.type pg_durable_session_open,@function
pg_durable_session_open:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov r15,r8
 mov eax,1
 test rbx,rbx
 jz .Ldurable_open_end
 test r12,r12
 jz .Ldurable_open_end
 test r13,r13
 jz .Ldurable_open_end
 test r14,r14
 jz .Ldurable_open_end
 test r15,r15
 jz .Ldurable_open_end
 test r9,r9
 jz .Ldurable_open_end
 cmp qword ptr [r9+8],0
 je .Ldurable_open_end
 cmp qword ptr [r12+8],0
 je .Ldurable_open_end
 cmp dword ptr [r12+48],0
 jne .Ldurable_open_end
 cmp qword ptr [r13],0
 je .Ldurable_open_end
 cmp qword ptr [r13+72],0
 je .Ldurable_open_end
 cmp qword ptr [r13+104],0
 jne .Ldurable_open_end
 cmp qword ptr [r13+88],0
 jne .Ldurable_open_end
 cmp dword ptr [r14+52],1
 jne .Ldurable_open_end
 cmp dword ptr [r14+48],0
 jne .Ldurable_open_end
 cmp [r14],r13
 jne .Ldurable_open_end
 cmp qword ptr [r14+16],0
 jne .Ldurable_open_end
 cmp dword ptr [r15+60],1
 jne .Ldurable_open_end
 cmp dword ptr [r15+56],0
 jne .Ldurable_open_end
 cmp [r15],r14
 jne .Ldurable_open_end
 cmp [r15+8],r12
 jne .Ldurable_open_end
 cmp qword ptr [r15+48],0
 jne .Ldurable_open_end
 mov [rbx],r12
 mov [rbx+8],r13
 mov [rbx+16],r14
 mov [rbx+24],r15
 mov rax,[r9]
 mov [rbx+32],rax
 mov rax,[r9+8]
 mov [rbx+40],rax
 mov rax,[r13+64]
 mov [rbx+48],rax
 mov rax,[r13+72]
 mov [rbx+56],rax
 mov qword ptr [rbx+64],0
 mov rax,[r13+96]
 mov [rbx+72],rax
 mov qword ptr [rbx+152],0
 mov qword ptr [rbx+160],0
 mov dword ptr [rbx+168],0
 mov dword ptr [rbx+172],1
 mov qword ptr [rbx+176],0
 mov rdi,r14
 lea rsi,[rbx+80]
 call pg_document_holds_bind@PLT
 test eax,eax
 jnz .Ldurable_open_end
 mov rdi,r14
 lea rsi,[rbx+120]
 call pg_document_holds_merge_bind@PLT
 test eax,eax
 jnz .Ldurable_open_end
 mov rax,[rbx+136]
 mov [rbx+184],rax
 lea rax,[rip+.Ldurable_release]
 mov [rbx+136],rax
 lea rax,[rip+.Ldurable_memory]
 mov [rbx+112],rax
 mov [r13+64],rbx
 lea rax,[rip+.Ldurable_clock]
 mov [r13+72],rax
 lea rax,[rip+.Ldurable_flush]
 mov [r13+88],rax
 lea rax,[rip+.Ldurable_pin]
 mov [r13+96],rax
 xor eax,eax
.Ldurable_open_end:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_durable_session_open,.-pg_durable_session_open
.Ldurable_memory:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rax,[rbx]
 mov rax,[rax+64]
 cmp dword ptr [rax+172],1
 jne .Ldurable_memory_observe
 mov rdi,[rax+24]
 call pg_hold_timer_sync@PLT
 test eax,eax
 jnz .Ldurable_memory_end
.Ldurable_memory_observe:
 cmp qword ptr [rbx+40],0
 je .Ldurable_memory_end
 mov rdi,[rbx+24]
 mov rsi,r12
 mov rdx,r13
 call [rbx+40]
.Ldurable_memory_end:
 pop r13
 pop r12
 pop rbx
 ret
.Ldurable_release:
 push rbx
 mov rax,[rdi]
 mov rbx,[rax+64]
 call [rbx+184]
 test eax,eax
 jnz .Ldurable_release_end
 cmp dword ptr [rbx+172],1
 jne .Ldurable_release_end
 mov rdi,[rbx+24]
 call pg_hold_timer_sync@PLT
.Ldurable_release_end:
 pop rbx
 ret
.Ldurable_clock:
 mov rax,[rdi+56]
 mov rdi,[rdi+48]
 jmp rax
.Ldurable_pin:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,[rbx+16]
 call pg_document_holds_pinned@PLT
 test eax,eax
 jnz .Ldurable_pin_end
 cmp qword ptr [rbx+72],0
 je .Ldurable_pin_end
 mov rdi,[rbx+48]
 mov rsi,r12
 call [rbx+72]
.Ldurable_pin_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.Ldurable_flush:
 push rbx
 mov rbx,rdi
 mov rdx,[rsi+72]
 mov rdi,[rbx+16]
 mov rsi,[rbx]
 mov rax,[rbx+24]
 cmp dword ptr [rax+68],1
 jne .Ldurable_flush_plain
 mov rcx,[rax+72]
 call pg_document_holds_flush_observed@PLT
 jmp .Ldurable_flushed
.Ldurable_flush_plain:
 call pg_document_holds_flush_one@PLT
.Ldurable_flushed:
 test eax,eax
 jnz .Ldurable_flush_end
 cmp dword ptr [rbx+172],1
 jne .Ldurable_flush_end
 mov rdi,[rbx+24]
 call pg_hold_timer_sync@PLT
.Ldurable_flush_end:
 pop rbx
 ret

.globl pg_durable_session_begin
.type pg_durable_session_begin,@function
pg_durable_session_begin:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov eax,1
 test rbx,rbx
 jz .Ldurable_begin_end
 test r12,r12
 jz .Ldurable_begin_end
 test r13,r13
 jz .Ldurable_begin_end
 cmp dword ptr [rbx+172],1
 jne .Ldurable_begin_end
 cmp dword ptr [rbx+168],0
 jne .Ldurable_begin_end
 cmp qword ptr [rbx+160],0
 jne .Ldurable_begin_end
 mov dword ptr [rbx+168],1
 mov rdi,r12
 mov rsi,[rbx]
 mov rdx,rcx
 mov rcx,r8
 call pg_record_tx_begin@PLT
 test eax,eax
 jnz .Ldurable_begin_unlock
 mov rdi,r13
 mov rsi,r12
 mov rdx,[rbx+8]
 call pg_document_tx_open@PLT
 test eax,eax
 jnz .Ldurable_begin_discard
 mov [rbx+152],r12
 mov [rbx+160],r13
 xor eax,eax
 jmp .Ldurable_begin_unlock
.Ldurable_begin_discard:
 mov rdi,r12
 call pg_record_tx_discard@PLT
 mov eax,1
.Ldurable_begin_unlock:
 mov dword ptr [rbx+168],0
.Ldurable_begin_end:
 pop r13
 pop r12
 pop rbx
 ret
.size pg_durable_session_begin,.-pg_durable_session_begin

.globl pg_durable_session_settle
.type pg_durable_session_settle,@function
pg_durable_session_settle:
 xor r8d,r8d
 jmp .Ldurable_settle_common
.globl pg_durable_session_settle_notified
.type pg_durable_session_settle_notified,@function
pg_durable_session_settle_notified:
 mov eax,1
 test rdx,rdx
 jz .Ldurable_notified_invalid
 cmp qword ptr [rdx+8],0
 je .Ldurable_notified_invalid
 cmp qword ptr [rdx+16],0
 je .Ldurable_notified_invalid
 cmp qword ptr [rdx+24],0
 je .Ldurable_notified_invalid
 cmp qword ptr [rdx+32],0
 je .Ldurable_notified_invalid
 mov r8,rdx
 mov rcx,rsi
 xor esi,esi
 xor edx,edx
 jmp .Ldurable_settle_common
.Ldurable_notified_invalid:
 ret
.size pg_durable_session_settle_notified,.-pg_durable_session_settle_notified
.Ldurable_settle_common:
 push rbx
 push r12
 sub rsp,8
 mov [rsp],r8
 mov rbx,rdi
 mov eax,1
 test rbx,rbx
 jz .Ldurable_settle_end
 cmp dword ptr [rbx+168],0
 jne .Ldurable_settle_end
 mov eax,[rbx+172]
 dec eax
 cmp eax,1
 ja .Ldurable_settle_invalid
 mov rdi,[rbx+160]
 test rdi,rdi
 jz .Ldurable_settle_invalid
 cmp qword ptr [rdi+40],0
 jne .Ldurable_settle_invalid
 mov rax,[rbx+152]
 cmp dword ptr [rax+32],0
 jne .Ldurable_settle_invalid
 cmp esi,1
 ja .Ldurable_settle_invalid
 mov dword ptr [rbx+168],1
 test esi,esi
 jz .Ldurable_settle_stored
 mov rsi,rcx
 mov rcx,rdx
 lea rdx,[rbx+80]
 lea r8,[rbx+120]
 mov r9,[rsp]
 test r9,r9
 jz .Ldurable_defer_unobserved
 call pg_document_tx_deferred_observed@PLT
 jmp .Ldurable_settled
.Ldurable_defer_unobserved:
 call pg_document_tx_deferred_merge@PLT
 jmp .Ldurable_settled
.Ldurable_settle_stored:
 mov rsi,rcx
 lea rdx,[rbx+120]
 cmp qword ptr [rsp],0
 je .Ldurable_settle_unobserved
 mov rcx,[rsp]
 call pg_document_tx_observed@PLT
 jmp .Ldurable_settled
.Ldurable_settle_unobserved:
 call pg_document_tx_merge@PLT
.Ldurable_settled:
 mov r12d,eax
 mov qword ptr [rbx+152],0
 mov qword ptr [rbx+160],0
 cmp dword ptr [rbx+172],1
 jne .Ldurable_settle_unlock
 mov rax,[rbx]
 cmp dword ptr [rax+52],0
 jne .Ldurable_settle_unlock
 mov rdi,[rbx+24]
 call pg_hold_timer_sync@PLT
 test eax,eax
 jz .Ldurable_settle_unlock
 test r12d,r12d
 jnz .Ldurable_settle_unlock
 mov r12d,6
.Ldurable_settle_unlock:
 mov dword ptr [rbx+168],0
 mov eax,r12d
 jmp .Ldurable_settle_end
.Ldurable_settle_invalid:
 mov eax,1
.Ldurable_settle_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.size pg_durable_session_settle,.-pg_durable_session_settle

.globl pg_durable_session_abort
.type pg_durable_session_abort,@function
pg_durable_session_abort:
 push rbx
 mov rbx,rdi
 mov eax,1
 test rbx,rbx
 jz .Ldurable_abort_end
 cmp dword ptr [rbx+168],0
 jne .Ldurable_abort_end
 mov rdi,[rbx+160]
 test rdi,rdi
 jz .Ldurable_abort_empty
 mov dword ptr [rbx+168],1
 call pg_document_tx_abort@PLT
 test eax,eax
 jnz .Ldurable_abort_unlock
 mov qword ptr [rbx+152],0
 mov qword ptr [rbx+160],0
.Ldurable_abort_unlock:
 mov dword ptr [rbx+168],0
 jmp .Ldurable_abort_end
.Ldurable_abort_empty:
 xor eax,eax
.Ldurable_abort_end:
 pop rbx
 ret
.size pg_durable_session_abort,.-pg_durable_session_abort

.globl pg_durable_session_idle
.type pg_durable_session_idle,@function
pg_durable_session_idle:
 xor eax,eax
 test rdi,rdi
 jz .Ldurable_idle_end
 cmp dword ptr [rdi+168],0
 jne .Ldurable_idle_end
 cmp qword ptr [rdi+160],0
 jne .Ldurable_idle_end
 mov ecx,[rdi+172]
 dec ecx
 cmp ecx,1
 ja .Ldurable_idle_end
 mov rcx,[rdi]
 cmp dword ptr [rcx+48],0
 jne .Ldurable_idle_end
 mov rcx,[rdi+8]
 cmp qword ptr [rcx+104],0
 jne .Ldurable_idle_end
 mov rcx,[rdi+16]
 cmp dword ptr [rcx+48],0
 jne .Ldurable_idle_end
 mov rcx,[rdi+24]
 cmp dword ptr [rcx+56],0
 jne .Ldurable_idle_end
 inc eax
.Ldurable_idle_end:
 ret
.size pg_durable_session_idle,.-pg_durable_session_idle

.globl pg_durable_session_close
.type pg_durable_session_close,@function
pg_durable_session_close:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov eax,-2
 test rbx,rbx
 jz .Ldurable_close_end
 cmp dword ptr [rbx+172],1
 jne .Ldurable_close_phase
 mov dword ptr [rbx+172],2
.Ldurable_close_phase:
 cmp dword ptr [rbx+172],4
 je .Ldurable_close_result
 cmp dword ptr [rbx+168],0
 jne .Ldurable_close_pending
 mov eax,[rbx+172]
 sub eax,2
 cmp eax,1
 ja .Ldurable_close_invalid
 mov dword ptr [rbx+168],1
 mov r12,[rbx]
 cmp dword ptr [rbx+172],3
 je .Ldurable_close_backend
 mov rdi,[rbx+24]
 call pg_hold_timer_stop@PLT
 test eax,eax
 jnz .Ldurable_close_wait
 cmp qword ptr [rbx+160],0
 jne .Ldurable_close_wait
 cmp dword ptr [r12+48],0
 jne .Ldurable_close_wait
 mov rax,[rbx+8]
 cmp qword ptr [rax+104],0
 jne .Ldurable_close_wait
 mov rdi,[rbx+16]
 mov rsi,r12
 mov rax,[rbx+24]
 cmp dword ptr [rax+68],1
 jne .Ldurable_finish_plain
 mov rdx,[rax+72]
 call pg_document_holds_finish_observed@PLT
 jmp .Ldurable_finished
.Ldurable_finish_plain:
 call pg_document_holds_finish@PLT
.Ldurable_finished:
 mov rdi,[rbx+8]
 call pg_document_cache_close@PLT
 test eax,eax
 jnz .Ldurable_close_cache_error
 mov rax,[rbx+8]
 mov rcx,[rbx+48]
 mov [rax+64],rcx
 mov rcx,[rbx+56]
 mov [rax+72],rcx
 mov rcx,[rbx+64]
 mov [rax+88],rcx
 mov rcx,[rbx+72]
 mov [rax+96],rcx
 mov dword ptr [rbx+172],3
.Ldurable_close_backend:
 mov rdi,[rbx+32]
 call [rbx+40]
 cmp eax,1
 je .Ldurable_close_wait
 test eax,eax
 jz .Ldurable_close_backend_done
 cmp dword ptr [r12+52],0
 jne .Ldurable_close_backend_done
 mov dword ptr [r12+52],3
.Ldurable_close_backend_done:
 mov eax,[r12+52]
 mov [rbx+176],eax
 mov dword ptr [rbx+172],4
 mov dword ptr [rbx+168],0
 jmp .Ldurable_close_result
.Ldurable_close_cache_error:
 cmp dword ptr [r12+52],0
 jne .Ldurable_close_wait
 mov dword ptr [r12+52],4
.Ldurable_close_wait:
 mov dword ptr [rbx+168],0
.Ldurable_close_pending:
 mov eax,1
 jmp .Ldurable_close_end
.Ldurable_close_invalid:
 mov eax,-2
 jmp .Ldurable_close_end
.Ldurable_close_result:
 xor eax,eax
 cmp dword ptr [rbx+176],0
 je .Ldurable_close_end
 mov eax,-1
.Ldurable_close_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.size pg_durable_session_close,.-pg_durable_session_close
.section .note.GNU-stack,"",@progbits
.text
/* Storage read/mint provider adapter. Descriptor and Session guards prevent
   backend close or nested descriptor use while a synchronous call is underway. */
.Lstorage_enter:
 mov eax,66
 test rdi,rdi
 jz .Lstorage_enter_end
 mov rax,[rdi]
 test rax,rax
 jz .Lstorage_enter_invalid
 mov rdx,[rax]
 test rdx,rdx
 jz .Lstorage_enter_invalid
 cmp dword ptr [rdx+52],0
 jne .Lstorage_enter_failed
 cmp dword ptr [rax+172],1
 je .Lstorage_enter_busy
 cmp dword ptr [rax+172],2
 jne .Lstorage_enter_closed
 cmp qword ptr [rax+152],0
 je .Lstorage_enter_closed
.Lstorage_enter_busy:
 cmp dword ptr [rdi+48],0
 jne .Lstorage_enter_reentry
 mov dword ptr [rdi+48],1
 mov edx,[rax+168]
 mov [rdi+52],edx
 mov dword ptr [rax+168],1
 xor eax,eax
 ret
.Lstorage_enter_invalid:
 mov eax,66
 ret
.Lstorage_enter_failed:
 mov eax,64
 ret
.Lstorage_enter_closed:
 mov eax,65
 ret
.Lstorage_enter_reentry:
 mov eax,67
.Lstorage_enter_end:
 ret
.Lstorage_leave:
 mov rax,[rdi]
 mov edx,[rdi+52]
 mov [rax+168],edx
 mov dword ptr [rdi+48],0
 ret
.Lstorage_health:
 mov rax,[rdi]
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 setne al
 movzx eax,al
 ret
.Lstorage_fail:
 mov rax,[rdi]
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 jne .Lstorage_fail_end
 mov dword ptr [rax+52],3
.Lstorage_fail_end:
 ret
.globl pg_durable_storage_read
.type pg_durable_storage_read,@function
pg_durable_storage_read:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12d,esi
 mov r13,rdx
 mov r14,rcx
 mov ebp,66
 test r14,r14
 jz .Lstorage_read_end
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Lstorage_read_end
 cmp qword ptr [rbx+16],0
 je .Lstorage_read_end
 call .Lstorage_enter
 mov ebp,eax
 test eax,eax
 jnz .Lstorage_read_end
 mov qword ptr [rsp],0
 mov rdi,[rbx+8]
 mov esi,r12d
 mov rdx,r13
 mov rcx,rsp
 call [rbx+16]
 mov esi,eax
 mov rdi,rbx
 call .Lstorage_read_status
 mov ebp,eax
 test eax,eax
 jnz .Lstorage_read_cleanup
 mov rax,[rsp]
 mov [r14],rax
 mov qword ptr [rsp],0
.Lstorage_read_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,rbx
 call .Lstorage_leave
.Lstorage_read_end:
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_durable_storage_read,.-pg_durable_storage_read
.globl pg_durable_storage_mint
.type pg_durable_storage_mint,@function
pg_durable_storage_mint:
 push rbp
 push rbx
 push r12
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov ebp,66
 test r12,r12
 jz .Lstorage_mint_end
 mov qword ptr [r12],0
 test rbx,rbx
 jz .Lstorage_mint_end
 cmp qword ptr [rbx+24],0
 je .Lstorage_mint_end
 call .Lstorage_enter
 mov ebp,eax
 test eax,eax
 jnz .Lstorage_mint_end
 mov qword ptr [rsp],0
 mov rdi,[rbx+8]
 mov rsi,rsp
 call [rbx+24]
 mov ebp,eax
 mov rdi,rbx
 call .Lstorage_health
 test eax,eax
 jnz .Lstorage_mint_crossed
 test ebp,ebp
 jnz .Lstorage_mint_fail
 mov rax,[rsp]
 test rax,rax
 jz .Lstorage_mint_protocol
 mov rdx,9007199254740991
 cmp rax,rdx
 ja .Lstorage_mint_protocol
 mov [r12],rax
 jmp .Lstorage_mint_leave
.Lstorage_mint_protocol:
 mov ebp,66
.Lstorage_mint_fail:
 mov rdi,rbx
 call .Lstorage_fail
 jmp .Lstorage_mint_leave
.Lstorage_mint_crossed:
 mov ebp,64
.Lstorage_mint_leave:
 mov rdi,rbx
 call .Lstorage_leave
.Lstorage_mint_end:
 mov eax,ebp
 add rsp,16
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_durable_storage_mint,.-pg_durable_storage_mint
.section .note.GNU-stack,"",@progbits
.text
/* Normalize synchronous read completion through the shared failure latch. */
.Lstorage_read_status:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12d,esi
 call .Lstorage_health
 test eax,eax
 jnz .Lstorage_status_crossed
 cmp r12d,1
 jbe .Lstorage_status_result
 cmp qword ptr [rbx+32],0
 je .Lstorage_status_cancel
 mov rdi,[rbx+8]
 mov esi,r12d
 call [rbx+32]
 test eax,eax
 jnz .Lstorage_status_result
.Lstorage_status_cancel:
 cmp qword ptr [rbx+40],0
 je .Lstorage_status_fail
 mov rdi,[rbx+8]
 call [rbx+40]
 test eax,eax
 jnz .Lstorage_status_result
.Lstorage_status_fail:
 mov rdi,rbx
 call .Lstorage_fail
 mov eax,r12d
 jmp .Lstorage_status_end
.Lstorage_status_result:
 mov rdi,rbx
 call .Lstorage_health
 test eax,eax
 jnz .Lstorage_status_crossed
 mov eax,r12d
 jmp .Lstorage_status_end
.Lstorage_status_crossed:
 mov eax,64
.Lstorage_status_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.globl pg_durable_storage_scan
.type pg_durable_storage_scan,@function
pg_durable_storage_scan:
 mov r10d,16
 mov r8,rdx
 mov rdx,rsi
 xor esi,esi
 xor ecx,ecx
 xor r9d,r9d
 jmp .Lstorage_query
.size pg_durable_storage_scan,.-pg_durable_storage_scan
.globl pg_durable_storage_head
.type pg_durable_storage_head,@function
pg_durable_storage_head:
 mov r10d,24
 mov r8,rcx
 mov rcx,rdx
 mov rdx,rsi
 mov esi,1
 xor r9d,r9d
 jmp .Lstorage_query
.size pg_durable_storage_head,.-pg_durable_storage_head
.globl pg_durable_storage_request
.type pg_durable_storage_request,@function
pg_durable_storage_request:
 mov r10d,32
 mov r8,rdx
 mov rdx,rsi
 mov esi,2
 xor ecx,ecx
 xor r9d,r9d
 jmp .Lstorage_query
.size pg_durable_storage_request,.-pg_durable_storage_request
.globl pg_durable_storage_entry
.type pg_durable_storage_entry,@function
pg_durable_storage_entry:
 mov r10d,40
 mov r9,r8
 mov r8,rcx
 mov rcx,rdx
 mov rdx,rsi
 mov esi,3
 jmp .Lstorage_query
.size pg_durable_storage_entry,.-pg_durable_storage_entry
.Lstorage_query:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,72
 mov rbx,rdi
 mov r12d,esi
 mov r13,rdx
 mov r14,rcx
 mov r15,r8
 mov [rsp+32],r9
 mov [rsp+40],r10
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],0
 mov ebp,66
 test r15,r15
 jz .Lstorage_query_end
 mov qword ptr [r15],0
 test r12d,r12d
 jnz .Lstorage_query_entry_output
 mov qword ptr [r15+8],0
 mov qword ptr [r15+16],0
.Lstorage_query_entry_output:
 cmp r12d,3
 jne .Lstorage_query_descriptor
 test r9,r9
 jz .Lstorage_query_end
 mov qword ptr [r9],0
.Lstorage_query_descriptor:
 test rbx,rbx
 jz .Lstorage_query_end
 cmp qword ptr [rbx+r10],0
 je .Lstorage_query_end
 mov rdi,[rbx]
 call .Lstorage_enter
 mov ebp,eax
 test eax,eax
 jnz .Lstorage_query_end
 mov rdi,[rbx+8]
 mov rsi,r13
 cmp r12d,0
 je .Lstorage_query_short
 cmp r12d,2
 je .Lstorage_query_short
 mov rdx,r14
 mov rcx,rsp
 lea r8,[rsp+24]
 jmp .Lstorage_query_call
.Lstorage_query_short:
 mov rdx,rsp
.Lstorage_query_call:
 mov rax,[rsp+40]
 call [rbx+rax]
 mov esi,eax
 mov rdi,[rbx]
 call .Lstorage_read_status
 mov ebp,eax
 test eax,eax
 jnz .Lstorage_query_cleanup
 mov rax,[rsp]
 mov [r15],rax
 mov qword ptr [rsp],0
 test r12d,r12d
 jnz .Lstorage_query_sequence
 mov rax,[rsp+8]
 mov [r15+8],rax
 mov rax,[rsp+16]
 mov [r15+16],rax
.Lstorage_query_sequence:
 cmp r12d,3
 jne .Lstorage_query_cleanup
 mov rax,[rsp+24]
 mov rdx,[rsp+32]
 mov [rdx],rax
.Lstorage_query_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rbx]
 call .Lstorage_leave
.Lstorage_query_end:
 mov eax,ebp
 add rsp,72
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .note.GNU-stack,"",@progbits
.text
.globl pg_durable_document_find
.type pg_durable_document_find,@function
pg_durable_document_find:
 mov r10d,16
 jmp .Lstorage_doc_point
.size pg_durable_document_find,.-pg_durable_document_find
.globl pg_durable_document_read
.type pg_durable_document_read,@function
pg_durable_document_read:
 mov r10d,24
.Lstorage_doc_point:
 mov r8,rcx
 mov rcx,rdx
 mov rdx,rsi
 mov esi,1
 xor r9d,r9d
 jmp .Lstorage_doc_query
.size pg_durable_document_read,.-pg_durable_document_read
.globl pg_durable_document_scan
.type pg_durable_document_scan,@function
pg_durable_document_scan:
 mov r10d,32
 mov r8,rdx
 mov rdx,rsi
 xor esi,esi
 xor ecx,ecx
 xor r9d,r9d
 jmp .Lstorage_doc_query
.size pg_durable_document_scan,.-pg_durable_document_scan
.globl pg_durable_document_entry
.type pg_durable_document_entry,@function
pg_durable_document_entry:
 mov r10d,40
 mov r9,r8
 mov r8,rcx
 mov rcx,rsi
 mov esi,3
.Lstorage_doc_query:
 push rbx
 mov rbx,rdi
 call .Lstorage_query
 cmp eax,1
 jbe .Lstorage_doc_end
 test rbx,rbx
 jz .Lstorage_doc_end
 mov rdx,[rbx]
 test rdx,rdx
 jz .Lstorage_doc_end
 mov rdx,[rdx]
 test rdx,rdx
 jz .Lstorage_doc_end
 mov rdx,[rdx]
 test rdx,rdx
 jz .Lstorage_doc_end
 cmp dword ptr [rdx+52],0
 jne .Lstorage_doc_end
 /* The outer Document reader must preserve nonfatal read rejection. */
 mov eax,68
.Lstorage_doc_end:
 pop rbx
 ret
.size pg_durable_document_entry,.-pg_durable_document_entry
.section .note.GNU-stack,"",@progbits
.text
.globl pg_session_listen
.type pg_session_listen,@function
pg_session_listen:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,16
 mov rbx,rdi
 mov r12d,esi
 mov r13,rdx
 mov r14,rcx
 mov eax,1
 test rbx,rbx
 jz .Llisten_end
 test r13,r13
 jz .Llisten_end
 cmp r12d,2
 ja .Llisten_end
 cmp dword ptr [rbx+52],0
 jne .Llisten_end
 mov rax,[rbx]
 test rax,rax
 jz .Llisten_invalid
 cmp dword ptr [rax+52],0
 jne .Llisten_invalid
 xor r15d,r15d
.Llisten_find:
 cmp r15,[rbx+16]
 jae .Llisten_capacity
 imul rax,r15,24
 add rax,[rbx+8]
 cmp [rax],r13
 jne .Llisten_next
 cmp [rax+8],r14
 jne .Llisten_next
 cmp [rax+16],r12d
 je .Llisten_ok
.Llisten_next:
 inc r15
 jmp .Llisten_find
.Llisten_capacity:
 cmp r15,[rbx+24]
 jb .Llisten_append
 mov rsi,[rbx+24]
 add rsi,rsi
 jc .Llisten_invalid
 add rsi,8
 jc .Llisten_invalid
 mov [rsp],rsi
 mov rdi,[rbx+8]
 mov edx,24
 call reallocarray@PLT
 test rax,rax
 jz .Llisten_invalid
 mov [rbx+8],rax
 mov rax,[rsp]
 mov [rbx+24],rax
.Llisten_append:
 imul rax,r15,24
 add rax,[rbx+8]
 mov [rax],r13
 mov [rax+8],r14
 mov [rax+16],r12d
 mov dword ptr [rax+20],0
 inc qword ptr [rbx+16]
.Llisten_ok:
 xor eax,eax
 jmp .Llisten_end
.Llisten_invalid:
 mov eax,1
.Llisten_end:
 add rsp,16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_session_listen,.-pg_session_listen
.globl pg_session_unlisten
.type pg_session_unlisten,@function
pg_session_unlisten:
 test rdi,rdi
 jz .Lunlisten_end
 xor r8d,r8d
.Lunlisten_each:
 cmp r8,[rdi+16]
 jae .Lunlisten_end
 imul rax,r8,24
 add rax,[rdi+8]
 cmp [rax],rdx
 jne .Lunlisten_next
 cmp [rax+8],rcx
 jne .Lunlisten_next
 cmp [rax+16],esi
 jne .Lunlisten_next
 dec qword ptr [rdi+16]
 mov rdx,[rdi+16]
 sub rdx,r8
 imul rdx,rdx,24
 mov rdi,rax
 lea rsi,[rax+24]
 jmp memmove@PLT
.Lunlisten_next:
 inc r8
 jmp .Lunlisten_each
.Lunlisten_end:
 ret
.size pg_session_unlisten,.-pg_session_unlisten
.globl pg_session_deliver
.type pg_session_deliver,@function
pg_session_deliver:
 xor r8d,r8d
 jmp .Ldeliver_common
.globl pg_session_deliver_observed
.type pg_session_deliver_observed,@function
pg_session_deliver_observed:
 mov eax,1
 test rcx,rcx
 jz .Lobserved_deliver_invalid
 mov dword ptr [rcx],0
 mov r8,rcx
 mov rcx,rdx
 mov rdx,rsi
 xor esi,esi
 jmp .Ldeliver_common
.Lobserved_deliver_invalid:
 ret
.size pg_session_deliver_observed,.-pg_session_deliver_observed
.Ldeliver_common:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,24
 mov [rsp+16],r8
 mov rbx,rdi
 mov ebp,esi
 mov r12,rdx
 mov r13,rcx
 mov qword ptr [rsp],0
 mov eax,1
 test rbx,rbx
 jz .Ldeliver_end
 cmp qword ptr [rbx],0
 je .Ldeliver_invalid
 cmp ebp,2
 ja .Ldeliver_end
 cmp dword ptr [rbx+48],0
 jne .Ldeliver_end
 mov r15,[rbx+16]
 test r15,r15
 jz .Ldeliver_ok
 mov rdi,r15
 mov esi,24
 call calloc@PLT
 test rax,rax
 jz .Ldeliver_invalid
 mov [rsp],rax
 mov rdi,rax
 mov rsi,[rbx+8]
 imul rdx,r15,24
 call memcpy@PLT
 mov dword ptr [rbx+48],1
 xor r14d,r14d
.Ldeliver_each:
 cmp r14,r15
 jae .Ldeliver_release
 imul rax,r14,24
 add rax,[rsp]
 cmp [rax+16],ebp
 jne .Ldeliver_next
 mov rdx,[rax]
 test rdx,rdx
 jz .Ldeliver_next
 mov rdi,[rax+8]
 mov [rsp+8],rdx
 mov rsi,r12
 mov rdx,r13
 call [rsp+8]
 test eax,eax
 jz .Ldeliver_next
 test ebp,ebp
 jnz .Ldeliver_report
 mov rdx,[rsp+16]
 test rdx,rdx
 jz .Ldeliver_latch_internal
 cmp dword ptr [rdx],0
 jne .Ldeliver_next
 mov [rdx],eax
 jmp .Ldeliver_next
.Ldeliver_latch_internal:
 mov rdx,[rbx]
 cmp dword ptr [rdx+52],0
 jne .Ldeliver_next
 mov dword ptr [rdx+52],4
 jmp .Ldeliver_next
.Ldeliver_report:
 cmp qword ptr [rbx+40],0
 je .Ldeliver_next
 mov esi,eax
 mov rdi,[rbx+32]
 call [rbx+40]
.Ldeliver_next:
 inc r14
 jmp .Ldeliver_each
.Ldeliver_release:
 mov dword ptr [rbx+48],0
 mov rdi,[rsp]
 call free@PLT
.Ldeliver_ok:
 xor eax,eax
 jmp .Ldeliver_end
.Ldeliver_invalid:
 mov eax,1
.Ldeliver_end:
 add rsp,24
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_session_deliver,.-pg_session_deliver
.globl pg_session_listeners_close
.type pg_session_listeners_close,@function
pg_session_listeners_close:
 push rbx
 mov rbx,rdi
 mov eax,1
 test rbx,rbx
 jz .Llisteners_close_end
 cmp dword ptr [rbx+48],0
 jne .Llisteners_close_end
 cmp dword ptr [rbx+52],2
 je .Llisteners_close_release
 cmp dword ptr [rbx+52],0
 jne .Llisteners_close_ok
 mov dword ptr [rbx+52],1
 mov rcx,rsi
 mov esi,2
 xor edx,edx
 call pg_session_deliver
 test eax,eax
 jz .Llisteners_close_release
 mov dword ptr [rbx+52],0
 jmp .Llisteners_close_end
.Llisteners_close_release:
 mov dword ptr [rbx+52],1
 mov rdi,[rbx+8]
 call free@PLT
 mov qword ptr [rbx+8],0
 mov qword ptr [rbx+16],0
 mov qword ptr [rbx+24],0
.Llisteners_close_ok:
 xor eax,eax
.Llisteners_close_end:
 pop rbx
 ret
.size pg_session_listeners_close,.-pg_session_listeners_close
.section .note.GNU-stack,"",@progbits
.text
/* Append detached JSON array elements to a private output array. */
.Lpublication_append:
 push rbp
 push rbx
 push r12
 push r13
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
 mov qword ptr [rsp],0
 mov rdi,r12
 call json_object_array_length@PLT
 mov rbp,rax
.Lpublication_append_each:
 cmp r13,rbp
 jae .Lpublication_append_ok
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 test rax,rax
 jz .Lpublication_append_attach
 mov rdi,rax
 lea rsi,[rsp]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lpublication_append_bad
.Lpublication_append_attach:
 mov rdi,rbx
 mov rsi,[rsp]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lpublication_append_bad
 mov qword ptr [rsp],0
 inc r13
 jmp .Lpublication_append_each
.Lpublication_append_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,1
 jmp .Lpublication_append_end
.Lpublication_append_ok:
 xor eax,eax
.Lpublication_append_end:
 add rsp,24
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Build one {kind?,seq?,changes} payload; append cloned documents to tables. */
.Lpublication_make:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 call json_object_new_object@PLT
 test rax,rax
 jz .Lpublication_make_bad
 mov [rsp],rax
 mov rdi,r12
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lpublication_make_bad
 mov rdi,[rsp+8]
 mov rsi,r13
 call .Lpublication_append
 test eax,eax
 jnz .Lpublication_make_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lpublication_changes]
 mov rdx,[rsp+8]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lpublication_make_bad
 mov qword ptr [rsp+8],0
 cmp dword ptr [rbx+32],1
 je .Lpublication_make_kind
 mov rdi,[rbx+24]
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lpublication_seq]
 call .Lpublication_attach
 test eax,eax
 jnz .Lpublication_make_bad
.Lpublication_make_kind:
 test r14,r14
 jz .Lpublication_make_ok
 mov rdi,r14
 call json_object_new_string@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lpublication_kind]
 call .Lpublication_attach
 test eax,eax
 jnz .Lpublication_make_bad
.Lpublication_make_ok:
 mov rax,[rsp]
 jmp .Lpublication_make_end
.Lpublication_make_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 xor eax,eax
.Lpublication_make_end:
 add rsp,24
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Lpublication_attach:
 test rax,rax
 jz .Lpublication_attach_bad
 push rbx
 mov rbx,rax
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Lpublication_attach_end
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,1
.Lpublication_attach_end:
 pop rbx
 ret
.Lpublication_attach_bad:
 mov eax,1
 ret
.globl pg_session_publications_prepare
.type pg_session_publications_prepare,@function
pg_session_publications_prepare:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov ebp,1
 test r12,r12
 jz .Lpublication_prepare_end
 mov qword ptr [r12],0
 mov qword ptr [r12+8],0
 test rbx,rbx
 jz .Lpublication_prepare_end
 cmp dword ptr [rbx+32],2
 ja .Lpublication_prepare_end
 xor r13d,r13d
.Lpublication_validate:
 mov rdi,[rbx+r13*8]
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lpublication_prepare_cleanup
 mov rdi,[rbx+r13*8]
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lpublication_prepare_cleanup
 inc r13
 cmp r13,3
 jb .Lpublication_validate
 cmp dword ptr [rbx+32],1
 je .Lpublication_valid_seq
 mov rax,[rbx+24]
 test rax,rax
 jz .Lpublication_prepare_cleanup
 mov rdx,9007199254740991
 cmp rax,rdx
 ja .Lpublication_prepare_cleanup
.Lpublication_valid_seq:
 mov ebp,2
 mov edi,0
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lpublication_prepare_cleanup
 mov [rsp],rax
 cmp dword ptr [rbx+32],0
 jne .Lpublication_build
 xor r13d,r13d
 mov rdi,[rbx]
 call json_object_array_length@PLT
 mov [rsp+24],rax
.Lpublication_tables:
 cmp r13,[rsp+24]
 jae .Lpublication_build
 mov rdi,[rbx]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lpublication_type]
 call json_object_object_get@PLT
 mov [rsp+8],rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 mov [rsp+32],eax
 mov rdi,[rsp+8]
 call json_object_get_string@PLT
 test rax,rax
 jz .Lpublication_next
 mov [rsp+16],rax
 mov rdi,rax
 call strlen@PLT
 cmp eax,[rsp+32]
 jne .Lpublication_next
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lpublication_entry]
 call strcmp@PLT
 test eax,eax
 jz .Lpublication_table
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lpublication_task]
 call strcmp@PLT
 test eax,eax
 jz .Lpublication_table
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lpublication_submission]
 call strcmp@PLT
 test eax,eax
 jz .Lpublication_table
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lpublication_conversation]
 call strcmp@PLT
 test eax,eax
 jnz .Lpublication_next
.Lpublication_table:
 mov qword ptr [rsp+16],0
 mov rdi,r14
 lea rsi,[rsp+16]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lpublication_prepare_cleanup
 mov rdi,[rsp]
 mov rsi,[rsp+16]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lpublication_prepare_cleanup
 mov qword ptr [rsp+16],0
.Lpublication_next:
 mov qword ptr [rsp+16],0
 inc r13
 jmp .Lpublication_tables
.Lpublication_build:
 cmp dword ptr [rbx+32],2
 je .Lpublication_stored
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,[rbx+8]
 lea rcx,[rip+.Lpublication_stored_kind]
 cmp dword ptr [rbx+32],1
 jne .Lpublication_observed_make
 lea rcx,[rip+.Lpublication_memory]
.Lpublication_observed_make:
 call .Lpublication_make
 test rax,rax
 jz .Lpublication_prepare_cleanup
 mov [r12],rax
 cmp dword ptr [rbx+32],1
 je .Lpublication_prepare_ok
 mov rax,[rbx+8]
 cmp rax,[rbx+16]
 jne .Lpublication_stored
 mov rdi,[r12]
 call json_object_get@PLT
 mov [r12+8],rax
 jmp .Lpublication_prepare_ok
.Lpublication_stored:
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,[rbx+16]
 xor ecx,ecx
 call .Lpublication_make
 test rax,rax
 jz .Lpublication_prepare_cleanup
 mov [r12+8],rax
.Lpublication_prepare_ok:
 xor ebp,ebp
.Lpublication_prepare_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 test ebp,ebp
 jz .Lpublication_prepare_end
 mov rdi,r12
 call pg_session_publications_discard
.Lpublication_prepare_end:
 mov eax,ebp
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_session_publications_prepare,.-pg_session_publications_prepare
.globl pg_session_publications_discard
.type pg_session_publications_discard,@function
pg_session_publications_discard:
 test rdi,rdi
 jz .Lpublication_discard_end
 push rbx
 mov rbx,rdi
 mov rdi,[rbx]
 call json_object_put@PLT
 mov rdi,[rbx+8]
 call json_object_put@PLT
 mov qword ptr [rbx],0
 mov qword ptr [rbx+8],0
 pop rbx
.Lpublication_discard_end:
 ret
.size pg_session_publications_discard,.-pg_session_publications_discard
.globl pg_session_publications_deliver
.type pg_session_publications_deliver,@function
pg_session_publications_deliver:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov eax,1
 test r12,r12
 jz .Lpublication_deliver_end
 cmp qword ptr [r12],0
 je .Lpublication_deliver_stored
 xor esi,esi
 mov rdx,[r12]
 mov rcx,r13
 call pg_session_deliver
 test eax,eax
 jnz .Lpublication_deliver_end
.Lpublication_deliver_stored:
 xor eax,eax
 cmp qword ptr [r12+8],0
 je .Lpublication_deliver_end
 mov rdi,rbx
 mov esi,1
 mov rdx,[r12+8]
 mov rcx,r13
 call pg_session_deliver
.Lpublication_deliver_end:
 pop r13
 pop r12
 pop rbx
 ret
.size pg_session_publications_deliver,.-pg_session_publications_deliver
.section .rodata
.Lpublication_changes: .asciz "changes"
.Lpublication_seq: .asciz "seq"
.Lpublication_kind: .asciz "kind"
.Lpublication_type: .asciz "type"
.Lpublication_entry: .asciz "entry"
.Lpublication_task: .asciz "task"
.Lpublication_submission: .asciz "submission"
.Lpublication_conversation: .asciz "conversation"
.Lpublication_stored_kind: .asciz "stored"
.Lpublication_memory: .asciz "memory"
.section .note.GNU-stack,"",@progbits
.text
.Lnotice_copy:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 test rdx,rdx
 jz .Lnotice_copy_attach
 mov rdi,rdx
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lnotice_copy_bad
.Lnotice_copy_attach:
 mov rdi,rbx
 mov rsi,r12
 mov rdx,[rsp]
 call json_object_object_add@PLT
 test eax,eax
 jz .Lnotice_copy_end
.Lnotice_copy_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,1
.Lnotice_copy_end:
 add rsp,24
 pop r12
 pop rbx
 ret
.globl pg_document_publication_prepare
.type pg_document_publication_prepare,@function
pg_document_publication_prepare:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov ebp,1
 test r12,r12
 jz .Lnotice_end
 mov qword ptr [r12],0
 test rbx,rbx
 jz .Lnotice_end
 mov r13d,[rbx+48]
 cmp r13d,15
 ja .Lnotice_end
 test r13d,11
 jnz .Lnotice_validate
 xor ebp,ebp
 jmp .Lnotice_end
.Lnotice_validate:
 mov rdi,[rbx]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lnotice_end
 mov rdi,[rbx]
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lnotice_end
 mov r14,[rbx+32]
 mov rax,9007199254740991
 cmp r14,rax
 ja .Lnotice_end
 test r13d,1
 jnz .Lnotice_build
 test r13d,2
 jnz .Lnotice_copy_validate
 mov rdi,[rbx+16]
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lnotice_end
 mov rax,9007199254740991
 cmp [rbx+40],rax
 ja .Lnotice_end
 test r13d,4
 jnz .Lnotice_build
 mov rdi,[rbx+24]
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lnotice_end
 mov rdi,[rbx+24]
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lnotice_end
 jmp .Lnotice_build
.Lnotice_copy_validate:
 mov rdi,[rbx+8]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lnotice_end
 mov rdi,[rbx+8]
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lnotice_end
.Lnotice_build:
 mov ebp,2
 call json_object_new_object@PLT
 test rax,rax
 jz .Lnotice_cleanup
 mov [rsp],rax
 lea rdi,[rip+.Lnotice_document]
 test r13d,1
 jnz .Lnotice_type
 test r13d,2
 jz .Lnotice_type
 lea rdi,[rip+.Lnotice_document_copy]
.Lnotice_type:
 call json_object_new_string@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lpublication_type]
 call .Lpublication_attach
 test eax,eax
 jnz .Lnotice_cleanup
 mov rdi,[rsp]
 lea rsi,[rip+.Lnotice_record]
 mov rdx,[rbx]
 call .Lnotice_copy
 test eax,eax
 jnz .Lnotice_cleanup
 test r14,r14
 jz .Lnotice_content
 mov rdi,r14
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lnotice_conversation]
 call .Lpublication_attach
 test eax,eax
 jnz .Lnotice_cleanup
.Lnotice_content:
 test r13d,1
 jnz .Lnotice_retire
 test r13d,2
 jz .Lnotice_value
 mov rdi,[rsp]
 lea rsi,[rip+.Lnotice_source]
 mov rdx,[rbx+8]
 call .Lnotice_copy
 test eax,eax
 jnz .Lnotice_cleanup
 jmp .Lnotice_success
.Lnotice_value:
 mov rdi,[rbx+40]
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lnotice_version]
 call .Lpublication_attach
 test eax,eax
 jnz .Lnotice_cleanup
 mov rdx,[rbx+16]
 jmp .Lnotice_set_value
.Lnotice_retire:
 xor edx,edx
.Lnotice_set_value:
 mov rdi,[rsp]
 lea rsi,[rip+.Lnotice_value_key]
 call .Lnotice_copy
 test eax,eax
 jnz .Lnotice_cleanup
 test r13d,5
 jz .Lnotice_loaded_ops
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lnotice_cleanup
 mov rdi,[rsp]
 lea rsi,[rip+.Lnotice_ops]
 call .Lpublication_attach
 test eax,eax
 jnz .Lnotice_cleanup
 jmp .Lnotice_success
.Lnotice_loaded_ops:
 mov rdi,[rsp]
 lea rsi,[rip+.Lnotice_ops]
 mov rdx,[rbx+24]
 call .Lnotice_copy
 test eax,eax
 jnz .Lnotice_cleanup
.Lnotice_success:
 mov rax,[rsp]
 mov [r12],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
.Lnotice_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
.Lnotice_end:
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_publication_prepare,.-pg_document_publication_prepare
.section .rodata
.Lnotice_document: .asciz "document"
.Lnotice_document_copy: .asciz "document.copy"
.Lnotice_record: .asciz "record"
.Lnotice_conversation: .asciz "conversationId"
.Lnotice_source: .asciz "source"
.Lnotice_version: .asciz "version"
.Lnotice_value_key: .asciz "value"
.Lnotice_ops: .asciz "ops"
.section .note.GNU-stack,"",@progbits
.text
.globl pg_session_publications_confirm
.type pg_session_publications_confirm,@function
pg_session_publications_confirm:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,40
 mov rbx,rdi
 mov r12,rsi
 mov eax,1
 test rbx,rbx
 jz .Lconfirm_end
 test r12,r12
 jz .Lconfirm_end
 mov rax,9007199254740991
 cmp r12,rax
 ja .Lconfirm_invalid
 cmp qword ptr [rbx+8],0
 je .Lconfirm_invalid
 mov dword ptr [rsp+32],0
.Lconfirm_pass:
 xor ebp,ebp
.Lconfirm_channel:
 mov r13,[rbx+rbp*8]
 test r13,r13
 jz .Lconfirm_next_channel
 mov rdi,r13
 lea rsi,[rip+.Lpublication_seq]
 call json_object_object_get@PLT
 mov r15,rax
 mov rdi,rax
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lconfirm_invalid
 cmp dword ptr [rsp+32],0
 je .Lconfirm_changes
 mov rdi,r15
 mov rsi,r12
 call json_object_set_uint64@PLT
.Lconfirm_changes:
 mov rdi,r13
 lea rsi,[rip+.Lpublication_changes]
 call json_object_object_get@PLT
 mov [rsp],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lconfirm_invalid
 mov rdi,[rsp]
 call json_object_array_length@PLT
 mov [rsp+8],rax
 xor r14d,r14d
.Lconfirm_change:
 cmp r14,[rsp+8]
 jae .Lconfirm_next_channel
 mov rdi,[rsp]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov [rsp+16],rax
 mov rdi,rax
 lea rsi,[rip+.Lpublication_type]
 call json_object_object_get@PLT
 mov r15,rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 mov [rsp+36],eax
 mov rdi,r15
 call json_object_get_string@PLT
 test rax,rax
 jz .Lconfirm_invalid
 mov r15,rax
 mov rdi,rax
 call strlen@PLT
 cmp eax,[rsp+36]
 jne .Lconfirm_next_change
 mov rdi,r15
 lea rsi,[rip+.Lnotice_document]
 call strcmp@PLT
 test eax,eax
 jz .Lconfirm_record
 mov rdi,r15
 lea rsi,[rip+.Lnotice_document_copy]
 call strcmp@PLT
 test eax,eax
 jnz .Lconfirm_next_change
.Lconfirm_record:
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lnotice_record]
 call json_object_object_get@PLT
 mov [rsp+24],rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lconfirm_invalid
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lconfirm_created]
 call json_object_object_get@PLT
 mov rdi,rax
 mov esi,[rsp+32]
 mov rdx,r12
 call .Lconfirm_stamp
 test eax,eax
 jnz .Lconfirm_invalid
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lconfirm_retired]
 lea rdx,[rsp+16]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lconfirm_next_change
 mov rdi,[rsp+16]
 mov esi,[rsp+32]
 mov rdx,r12
 call .Lconfirm_stamp
 test eax,eax
 jnz .Lconfirm_invalid
.Lconfirm_next_change:
 inc r14
 jmp .Lconfirm_change
.Lconfirm_next_channel:
 inc ebp
 cmp ebp,2
 jb .Lconfirm_channel
 cmp dword ptr [rsp+32],0
 jne .Lconfirm_ok
 mov dword ptr [rsp+32],1
 jmp .Lconfirm_pass
.Lconfirm_ok:
 xor eax,eax
 jmp .Lconfirm_end
.Lconfirm_invalid:
 mov eax,1
.Lconfirm_end:
 add rsp,40
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_session_publications_confirm,.-pg_session_publications_confirm
.Lconfirm_stamp:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12d,esi
 mov r13,rdx
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lconfirm_stamp_bad
 mov rdi,rbx
 call json_object_get_int64@PLT
 test rax,rax
 js .Lconfirm_stamp_bad
 mov rdx,9007199254740991
 cmp rax,rdx
 ja .Lconfirm_stamp_bad
 test rax,rax
 jnz .Lconfirm_stamp_ok
 test r12d,r12d
 jz .Lconfirm_stamp_ok
 mov rdi,rbx
 mov rsi,r13
 call json_object_set_uint64@PLT
.Lconfirm_stamp_ok:
 xor eax,eax
 jmp .Lconfirm_stamp_end
.Lconfirm_stamp_bad:
 mov eax,1
.Lconfirm_stamp_end:
 pop r13
 pop r12
 pop rbx
 ret
.section .rodata
.Lconfirm_created: .asciz "createdAt"
.Lconfirm_retired: .asciz "retiredAt"
.section .note.GNU-stack,"",@progbits
.text
/* Resolve a Document record's owning conversation on the transaction line. */
.Lhost_conversation:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rdx
 mov qword ptr [r12],0
 mov rdi,rsi
 lea rsi,[rip+.Lhost_scope]
 call json_object_object_get@PLT
 mov [rsp],rax
 mov rdi,rax
 lea rsi,[rip+.Lpublication_kind]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lhost_conversation_bad
 mov rdi,rax
 lea rsi,[rip+.Lpublication_conversation]
 call strcmp@PLT
 test eax,eax
 jnz .Lhost_conversation_task
 mov rdi,[rsp]
 lea rsi,[rip+.Lnotice_conversation]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number@PLT
 test rax,rax
 jz .Lhost_conversation_bad
 mov [r12],rax
 xor eax,eax
 jmp .Lhost_conversation_end
.Lhost_conversation_task:
 mov rdi,[rsp]
 lea rsi,[rip+.Lpublication_kind]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lpublication_task]
 call strcmp@PLT
 test eax,eax
 jnz .Lhost_conversation_session
 mov rdi,[rsp]
 lea rsi,[rip+.Lhost_task_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number@PLT
 mov rdi,rbx
 mov rsi,rax
 mov rdx,r12
 call pg_record_tx_task_conversation
 jmp .Lhost_conversation_end
.Lhost_conversation_session:
 mov rdi,[rsp]
 lea rsi,[rip+.Lpublication_kind]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lhost_session]
 call strcmp@PLT
 test eax,eax
 jnz .Lhost_conversation_bad
 xor eax,eax
 jmp .Lhost_conversation_end
.Lhost_conversation_bad:
 mov eax,1
.Lhost_conversation_end:
 add rsp,8
 pop r12
 pop rbx
 ret
/* Clone lifecycle metadata, reserving integer placeholders for confirmation. */
.Lhost_record:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rdx
 mov r13d,esi
 mov qword ptr [r12],0
 mov rdi,rbx
 mov rsi,r12
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lhost_record_bad
 mov rdi,[r12]
 lea rsi,[rip+.Lconfirm_created]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lhost_record_retire
 xor edi,edi
 call json_object_new_int64@PLT
 mov rdi,[r12]
 lea rsi,[rip+.Lconfirm_created]
 call .Lpublication_attach
 test eax,eax
 jnz .Lhost_record_bad
.Lhost_record_retire:
 test r13d,r13d
 jz .Lhost_record_ok
 xor edi,edi
 call json_object_new_int64@PLT
 mov rdi,[r12]
 lea rsi,[rip+.Lconfirm_retired]
 call .Lpublication_attach
 test eax,eax
 jnz .Lhost_record_bad
.Lhost_record_ok:
 xor eax,eax
 jmp .Lhost_record_end
.Lhost_record_bad:
 mov eax,1
.Lhost_record_end:
 pop r13
 pop r12
 pop rbx
 ret
/* Append a retained notice; keep the caller's owner for split-channel use. */
.Lhost_append:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov rdi,rsi
 call json_object_get@PLT
 mov r12,rax
 mov rdi,rbx
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz .Lhost_append_end
 mov rdi,r12
 call json_object_put@PLT
 mov eax,1
.Lhost_append_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.globl pg_session_notifications_bind
.type pg_session_notifications_bind,@function
pg_session_notifications_bind:
 mov eax,1
 test rdi,rdi
 jz .Lhost_bind_end
 test rsi,rsi
 jz .Lhost_bind_end
 cmp qword ptr [rdi],0
 je .Lhost_bind_end
 mov [rsi],rdi
 lea rax,[rip+.Lhost_prepare]
 mov [rsi+8],rax
 lea rax,[rip+.Lhost_adopt]
 mov [rsi+16],rax
 lea rax,[rip+.Lhost_publish]
 mov [rsi+24],rax
 lea rax,[rip+.Lhost_discard]
 mov [rsi+32],rax
 xor eax,eax
.Lhost_bind_end:
 ret
.size pg_session_notifications_bind,.-pg_session_notifications_bind
/* Token: stored pair[16], document arrays[16], split at 32,
   prepared memory pair at 40, memory-route selection at 56. */
.Lhost_prepare:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,200
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov r15,r8
 mov rdi,rsp
 xor eax,eax
 mov ecx,25
 rep stosq
 mov ebp,3
 mov qword ptr [r15],0
 mov rax,[rbx]
 mov rdx,[rax]
 cmp rdx,[r12]
 jne .Lhost_prepare_invalid
 mov rcx,[rax+16]
 mov rax,[rax+8]
.Lhost_interested:
 test rcx,rcx
 jz .Lhost_no_subscribers
 cmp dword ptr [rax+16],1
 jbe .Lhost_allocate
 add rax,24
 dec rcx
 jmp .Lhost_interested
.Lhost_no_subscribers:
 xor ebp,ebp
 jmp .Lhost_prepare_end
.Lhost_allocate:
 mov edi,1
 mov esi,64
 call calloc@PLT
 test rax,rax
 jz .Lhost_prepare_end
 mov [r15],rax
 mov [rsp+192],rax
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lhost_prepare_cleanup
 mov rdx,[rsp+192]
 mov [rdx+16],rax
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lhost_prepare_cleanup
 mov rdx,[rsp+192]
 mov [rdx+24],rax
 mov qword ptr [rsp+184],0
.Lhost_entry:
 mov rax,[rsp+184]
 cmp rax,r14
 jae .Lhost_copies
 imul rax,rax,152
 lea r15,[r13+rax]
 mov rax,[r15]
 mov [rsp+160],rax
 mov edx,[r15+148]
 test edx,2
 jnz .Lhost_entry_ready
 cmp qword ptr [r15+112],0
 je .Lhost_next_entry
.Lhost_entry_ready:
 mov rdi,[rax]
 mov esi,edx
 and esi,2
 lea rdx,[rsp+168]
 call .Lhost_record
 test eax,eax
 jnz .Lhost_prepare_cleanup
 mov rax,[rsp+168]
 mov [rsp],rax
 mov rdi,r12
 mov rsi,rax
 lea rdx,[rsp+32]
 call .Lhost_conversation
 test eax,eax
 jnz .Lhost_prepare_invalid
 mov rax,[rsp+160]
 mov rdx,[rax+96]
 mov [rsp+40],rdx
 mov rdx,[r15+16]
 mov [rsp+16],rdx
 mov rdx,[r15+24]
 mov [rsp+24],rdx
 mov dword ptr [rsp+48],8
 test dword ptr [r15+148],1
 jz .Lhost_entry_retire
 or dword ptr [rsp+48],4
.Lhost_entry_retire:
 test dword ptr [r15+148],2
 jz .Lhost_entry_value
 or dword ptr [rsp+48],1
.Lhost_entry_value:
 cmp qword ptr [rsp+16],0
 jne .Lhost_describe
 mov rdx,[rax+8]
 mov [rsp+16],rdx
.Lhost_describe:
 mov rdi,rsp
 lea rsi,[rsp+176]
 call pg_document_publication_prepare
 test eax,eax
 jnz .Lhost_prepare_cleanup
 test dword ptr [r15+148],16
 jz .Lhost_same_notice
 test dword ptr [r15+148],2
 jnz .Lhost_same_notice
 mov rax,[rsp+192]
 mov dword ptr [rax+32],1
 mov rax,[rsp+160]
 mov rdx,[rax+88]
 cmp rdx,[rax+96]
 jb .Lhost_held_observed
 mov rdi,[r15+24]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lhost_held_stored
.Lhost_held_observed:
 mov rax,[rsp+192]
 mov rdi,[rax+16]
 mov rsi,[rsp+176]
 call .Lhost_append
 test eax,eax
 jnz .Lhost_prepare_cleanup
.Lhost_held_stored:
 mov rdi,[rsp+176]
 lea rsi,[rsp+56]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lhost_prepare_cleanup
 mov rdi,[rsp+176]
 call json_object_put@PLT
 mov rax,[rsp+56]
 mov [rsp+176],rax
 mov qword ptr [rsp+56],0
 mov rdi,[rbx+8]
 test rdi,rdi
 jz .Lhost_prepare_invalid
 mov rcx,[rdi+16]
 mov rdi,[rdi+8]
 mov rax,[rsp+160]
 mov rdx,[rax+72]
.Lhost_find_hold:
 test rcx,rcx
 jz .Lhost_prepare_invalid
 mov rax,[rdi]
 cmp [rax+72],rdx
 je .Lhost_hold_found
 add rdi,56
 dec rcx
 jmp .Lhost_find_hold
.Lhost_hold_found:
 mov [rsp+152],rdi
 movups xmm0,[rdi+8]
 movups [rsp+64],xmm0
 movups xmm0,[rdi+24]
 movups [rsp+80],xmm0
 mov rdi,[rsp+64]
 call json_object_get@PLT
 mov rdi,[rsp+72]
 call json_object_get@PLT
 mov rsi,[r15+24]
 test rsi,rsi
 jnz .Lhost_hold_inputs
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lhost_prepare_cleanup
 mov [rsp+144],rax
 mov rsi,rax
.Lhost_hold_inputs:
 lea rdi,[rsp+64]
 mov rdx,[rsp+16]
 mov rcx,[rsp+40]
 mov rax,[rsp+152]
 mov r8d,[rax+48]
 lea r9,[rsp+96]
 call pg_document_store_held@PLT
 test eax,eax
 jnz .Lhost_prepare_cleanup
 mov rdi,[rsp+176]
 lea rsi,[rip+.Lnotice_ops]
 mov rdx,[rsp+104]
 call .Lnotice_copy
 test eax,eax
 jnz .Lhost_prepare_cleanup
 mov rax,[rsp+192]
 mov rdi,[rax+24]
 mov rsi,[rsp+176]
 call .Lhost_append
 test eax,eax
 jnz .Lhost_prepare_cleanup
 jmp .Lhost_entry_clear
.Lhost_same_notice:
 mov rax,[rsp+192]
 mov rdi,[rax+16]
 mov rsi,[rsp+176]
 call .Lhost_append
 test eax,eax
 jnz .Lhost_prepare_cleanup
 mov rax,[rsp+192]
 mov rdi,[rax+24]
 mov rsi,[rsp+176]
 call .Lhost_append
 test eax,eax
 jnz .Lhost_prepare_cleanup
.Lhost_entry_clear:
 call .Lhost_locals_clear
.Lhost_next_entry:
 inc qword ptr [rsp+184]
 jmp .Lhost_entry
.Lhost_copies:
 mov qword ptr [rsp+184],0
 mov rdi,[r12+24]
 call json_object_array_length@PLT
 mov r14,rax
.Lhost_copy_each:
 mov rsi,[rsp+184]
 cmp rsi,r14
 jae .Lhost_build
 mov rdi,[r12+24]
 call json_object_array_get_idx@PLT
 mov r15,rax
 mov rdi,rax
 lea rsi,[rip+.Lpublication_type]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lhost_copy_next
 mov rdi,rax
 lea rsi,[rip+.Lnotice_document_copy]
 call strcmp@PLT
 test eax,eax
 jnz .Lhost_copy_next
 mov rdi,r15
 lea rsi,[rip+.Lnotice_record]
 call json_object_object_get@PLT
 mov rdi,rax
 xor esi,esi
 lea rdx,[rsp+168]
 call .Lhost_record
 test eax,eax
 jnz .Lhost_prepare_cleanup
 mov rax,[rsp+168]
 mov [rsp],rax
 mov rdi,r12
 mov rsi,rax
 lea rdx,[rsp+32]
 call .Lhost_conversation
 test eax,eax
 jnz .Lhost_prepare_invalid
 mov rdi,r15
 lea rsi,[rip+.Lnotice_source]
 call json_object_object_get@PLT
 mov [rsp+8],rax
 mov dword ptr [rsp+48],2
 mov rdi,rsp
 lea rsi,[rsp+176]
 call pg_document_publication_prepare
 test eax,eax
 jnz .Lhost_prepare_cleanup
 mov rax,[rsp+192]
 mov rdi,[rax+16]
 mov rsi,[rsp+176]
 call .Lhost_append
 test eax,eax
 jnz .Lhost_prepare_cleanup
 mov rax,[rsp+192]
 mov rdi,[rax+24]
 mov rsi,[rsp+176]
 call .Lhost_append
 test eax,eax
 jnz .Lhost_prepare_cleanup
 call .Lhost_locals_clear
.Lhost_copy_next:
 inc qword ptr [rsp+184]
 jmp .Lhost_copy_each
.Lhost_build:
 mov rdi,[r12+24]
 mov [rsp+112],rdi
 mov rax,[rsp+192]
 mov rdx,[rax+16]
 mov [rsp+120],rdx
 cmp dword ptr [rax+32],0
 je .Lhost_build_stored
 mov rdx,[rax+24]
.Lhost_build_stored:
 mov [rsp+128],rdx
 mov qword ptr [rsp+136],1
 mov qword ptr [rsp+144],0
 lea rdi,[rsp+112]
 mov rsi,rax
 call pg_session_publications_prepare
 test eax,eax
 jnz .Lhost_prepare_cleanup
 xor ebp,ebp
 jmp .Lhost_prepare_cleanup
.Lhost_prepare_invalid:
 mov ebp,1
.Lhost_prepare_cleanup:
 call .Lhost_locals_clear
.Lhost_prepare_end:
 mov eax,ebp
 add rsp,200
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Called only with the enclosing prepare frame intact. */
.Lhost_locals_clear:
 sub rsp,8
 mov rdi,[rsp+192]
 call json_object_put@PLT
 mov rdi,[rsp+184]
 call json_object_put@PLT
 mov rdi,[rsp+72]
 call json_object_put@PLT
 lea rdi,[rsp+80]
 call pg_pending_ops_close@PLT
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov rdi,[rsp+120]
 call json_object_put@PLT
 mov rdi,[rsp+160]
 call json_object_put@PLT
 mov qword ptr [rsp+192],0
 mov qword ptr [rsp+184],0
 mov qword ptr [rsp+72],0
 mov qword ptr [rsp+112],0
 mov qword ptr [rsp+120],0
 mov qword ptr [rsp+160],0
 add rsp,8
 ret
.Lhost_adopt:
 test rsi,rsi
 jz .Lhost_adopt_empty
 mov rdi,rsi
 mov rsi,rdx
 jmp pg_session_publications_confirm
.Lhost_adopt_empty:
 xor eax,eax
 ret
.Lhost_publish:
 test rsi,rsi
 jz .Lhost_publish_empty
 push rbx
 mov rbx,rdi
 mov rdi,[rbx]
 mov rdx,[rbx+16]
 call pg_session_publications_deliver
 test eax,eax
 jz .Lhost_publish_end
 mov rax,[rbx]
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 jne .Lhost_publish_end
 mov dword ptr [rax+52],4
.Lhost_publish_end:
 pop rbx
.Lhost_publish_empty:
 ret
.Lhost_discard:
 test rsi,rsi
 jz .Lhost_discard_end
 push rbx
 mov rbx,rsi
 mov rdi,rsi
 call pg_session_publications_discard
 lea rdi,[rbx+40]
 call pg_session_publications_discard
 mov rdi,[rbx+16]
 call json_object_put@PLT
 mov rdi,[rbx+24]
 call json_object_put@PLT
 mov rdi,rbx
 call free@PLT
 pop rbx
.Lhost_discard_end:
 ret
.section .rodata
.Lhost_scope: .asciz "scope"
.Lhost_task_id: .asciz "taskId"
.Lhost_session: .asciz "session"
.section .note.GNU-stack,"",@progbits
.text
.globl pg_session_deferred_notifications_bind
.type pg_session_deferred_notifications_bind,@function
pg_session_deferred_notifications_bind:
 push rbx
 mov rbx,rsi
 call pg_session_notifications_bind
 test eax,eax
 jnz .Ldeferred_bind_end
 lea rdx,[rip+.Lhost_prepare_deferred]
 mov [rbx+8],rdx
 lea rdx,[rip+.Lhost_adopt_deferred]
 mov [rbx+16],rdx
 lea rdx,[rip+.Lhost_publish_deferred]
 mov [rbx+24],rdx
.Ldeferred_bind_end:
 pop rbx
 ret
.size pg_session_deferred_notifications_bind,.-pg_session_deferred_notifications_bind
.Lhost_prepare_deferred:
 push rbx
 push r12
 push r13
 sub rsp,48
 mov rbx,rdi
 mov r12,r8
 mov r13,rsi
 call .Lhost_prepare
 test eax,eax
 jnz .Lprepare_deferred_end
 mov rax,[r12]
 test rax,rax
 jz .Lprepare_deferred_end
 mov rdx,[r13+24]
 mov [rsp],rdx
 mov rdx,[rax+16]
 mov [rsp+8],rdx
 mov rdx,[rax+24]
 mov [rsp+16],rdx
 mov qword ptr [rsp+24],0
 mov qword ptr [rsp+32],1
 mov rdi,rsp
 lea rsi,[rax+40]
 call pg_session_publications_prepare
 test eax,eax
 jz .Lprepare_deferred_end
 mov eax,3
.Lprepare_deferred_end:
 add rsp,48
 pop r13
 pop r12
 pop rbx
 ret
.Lhost_adopt_deferred:
 test rsi,rsi
 jz .Lhost_adopt_empty
 test rdx,rdx
 jnz .Lhost_adopt
 mov dword ptr [rsi+56],1
 xor eax,eax
 ret
.Lhost_publish_deferred:
 test rsi,rsi
 jz .Lhost_publish_empty
 cmp dword ptr [rsi+56],0
 je .Lhost_publish
 add rsi,40
 jmp .Lhost_publish
.globl pg_durable_session_defer_notified
.type pg_durable_session_defer_notified,@function
pg_durable_session_defer_notified:
 mov eax,1
 test rcx,rcx
 jz .Ldurable_notified_invalid
 cmp qword ptr [rcx+8],0
 je .Ldurable_notified_invalid
 cmp qword ptr [rcx+16],0
 je .Ldurable_notified_invalid
 cmp qword ptr [rcx+24],0
 je .Ldurable_notified_invalid
 cmp qword ptr [rcx+32],0
 je .Ldurable_notified_invalid
 mov r8,rcx
 mov rcx,rdx
 mov rdx,rsi
 mov esi,1
 jmp .Ldurable_settle_common
.size pg_durable_session_defer_notified,.-pg_durable_session_defer_notified
.section .note.GNU-stack,"",@progbits
.text
.globl pg_session_hold_notifications_bind
.type pg_session_hold_notifications_bind,@function
pg_session_hold_notifications_bind:
 mov eax,1
 test rdi,rdi
 jz .Lhold_notice_bind_end
 test rsi,rsi
 jz .Lhold_notice_bind_end
 cmp qword ptr [rdi],0
 je .Lhold_notice_bind_end
 cmp qword ptr [rdi+8],0
 je .Lhold_notice_bind_end
 mov [rsi],rdi
 lea rax,[rip+.Lhold_notice_open]
 mov [rsi+8],rax
 lea rax,[rip+.Lhold_notice_append]
 mov [rsi+16],rax
 lea rax,[rip+.Lhold_notice_prepare]
 mov [rsi+24],rax
 lea rax,[rip+.Lhost_adopt]
 mov [rsi+32],rax
 lea rax,[rip+.Lhost_publish]
 mov [rsi+40],rax
 lea rax,[rip+.Lhost_discard]
 mov [rsi+48],rax
 xor eax,eax
.Lhold_notice_bind_end:
 ret
.size pg_session_hold_notifications_bind,.-pg_session_hold_notifications_bind
.Lhold_notice_open:
 push rbp
 push rbx
 push r12
 mov rbx,rdi
 mov r12,rcx
 mov ebp,1
 mov qword ptr [r12],0
 cmp [rbx+8],rsi
 jne .Lhold_notice_open_end
 mov rax,[rbx]
 cmp [rax],rdx
 jne .Lhold_notice_open_end
 mov rcx,[rax+16]
 mov rax,[rax+8]
.Lhold_notice_interest:
 test rcx,rcx
 jz .Lhold_notice_no_interest
 cmp dword ptr [rax+16],1
 je .Lhold_notice_allocate
 add rax,24
 dec rcx
 jmp .Lhold_notice_interest
.Lhold_notice_no_interest:
 xor ebp,ebp
 jmp .Lhold_notice_open_end
.Lhold_notice_allocate:
 mov ebp,3
 mov edi,1
 mov esi,64
 call calloc@PLT
 test rax,rax
 jz .Lhold_notice_open_end
 mov [r12],rax
 mov rbx,rax
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lhold_notice_open_end
 mov [rbx+16],rax
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lhold_notice_open_end
 mov [rbx+24],rax
 xor ebp,ebp
.Lhold_notice_open_end:
 mov eax,ebp
 pop r12
 pop rbx
 pop rbp
 ret
.Lhold_notice_append:
 test rsi,rsi
 jz .Lhold_notice_append_empty
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rsi
 mov r12,rdx
 mov r13,rcx
 mov rdi,rsp
 xor eax,eax
 mov ecx,8
 rep stosq
 mov ebp,1
 cmp dword ptr [r12+64],1
 jne .Lhold_notice_append_end
 mov rax,[r12]
 mov [rsp],rax
 mov rax,[r12+8]
 mov [rsp+16],rax
 mov [rsp+24],r13
 mov rax,[r12+56]
 mov [rsp+32],rax
 mov rax,[r12+96]
 mov [rsp+40],rax
 mov dword ptr [rsp+48],8
 mov rdi,rsp
 lea rsi,[rsp+56]
 call pg_document_publication_prepare
 test eax,eax
 jnz .Lhold_notice_append_bad
 mov rdi,[rbx+24]
 mov rsi,[rsp+56]
 call .Lhost_append
 test eax,eax
 jnz .Lhold_notice_append_bad
 xor ebp,ebp
 jmp .Lhold_notice_append_clear
.Lhold_notice_append_bad:
 mov ebp,3
.Lhold_notice_append_clear:
 mov rdi,[rsp+56]
 call json_object_put@PLT
.Lhold_notice_append_end:
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lhold_notice_append_empty:
 xor eax,eax
 ret
.Lhold_notice_prepare:
 test rsi,rsi
 jz .Lhold_notice_append_empty
 push rbx
 sub rsp,48
 mov rbx,rsi
 mov [rsp],rdx
 mov rax,[rbx+16]
 mov [rsp+8],rax
 mov rax,[rbx+24]
 mov [rsp+16],rax
 mov qword ptr [rsp+24],1
 mov qword ptr [rsp+32],2
 mov rdi,rsp
 mov rsi,rbx
 call pg_session_publications_prepare
 test eax,eax
 jz .Lhold_notice_prepare_end
 mov eax,3
.Lhold_notice_prepare_end:
 add rsp,48
 pop rbx
 ret
.section .note.GNU-stack,"",@progbits
.text
.globl pg_session_dispatch_prepare
.type pg_session_dispatch_prepare,@function
pg_session_dispatch_prepare:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov eax,1
 test r14,r14
 jz .Ldispatch_prepare_end
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Ldispatch_prepare_end
 cmp dword ptr [rbx+36],0
 jne .Ldispatch_prepare_end
 cmp qword ptr [rbx],0
 je .Ldispatch_prepare_end
 cmp qword ptr [rbx+24],-1
 je .Ldispatch_prepare_end
 mov rdi,r12
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldispatch_prepare_bad
 mov rdi,r12
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Ldispatch_prepare_bad
 mov edi,1
 mov esi,40
 call calloc@PLT
 test rax,rax
 jz .Ldispatch_prepare_bad
 mov [r14],rax
 mov [rax+16],r13
 mov [rax+24],rbx
 mov rdi,r12
 call json_object_get@PLT
 mov rdx,[r14]
 mov [rdx+8],rax
 inc qword ptr [rbx+24]
 xor eax,eax
 jmp .Ldispatch_prepare_end
.Ldispatch_prepare_bad:
 mov eax,1
.Ldispatch_prepare_end:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_session_dispatch_prepare,.-pg_session_dispatch_prepare
.globl pg_session_dispatch_enqueue
.type pg_session_dispatch_enqueue,@function
pg_session_dispatch_enqueue:
 mov eax,1
 test rdi,rdi
 jz .Ldispatch_enqueue_end
 test rsi,rsi
 jz .Ldispatch_enqueue_end
 cmp [rsi+24],rdi
 jne .Ldispatch_enqueue_end
 cmp dword ptr [rsi+32],0
 jne .Ldispatch_enqueue_end
 cmp dword ptr [rdi+36],0
 jne .Ldispatch_enqueue_end
 cmp qword ptr [rdi+24],0
 je .Ldispatch_enqueue_end
 mov dword ptr [rsi+32],1
 mov qword ptr [rsi],0
 mov rax,[rdi+16]
 test rax,rax
 jz .Ldispatch_enqueue_first
 mov [rax],rsi
 jmp .Ldispatch_enqueue_tail
.Ldispatch_enqueue_first:
 mov [rdi+8],rsi
.Ldispatch_enqueue_tail:
 mov [rdi+16],rsi
 dec qword ptr [rdi+24]
 xor eax,eax
.Ldispatch_enqueue_end:
 ret
.size pg_session_dispatch_enqueue,.-pg_session_dispatch_enqueue
.globl pg_session_dispatch_discard
.type pg_session_dispatch_discard,@function
pg_session_dispatch_discard:
 mov eax,1
 test rdi,rdi
 jz .Ldispatch_discard_end
 test rsi,rsi
 jz .Ldispatch_discard_end
 cmp [rsi+24],rdi
 jne .Ldispatch_discard_end
 cmp dword ptr [rsi+32],0
 jne .Ldispatch_discard_end
 cmp qword ptr [rdi+24],0
 je .Ldispatch_discard_end
 dec qword ptr [rdi+24]
 push rbx
 mov rbx,rsi
 mov rdi,[rbx+8]
 call json_object_put@PLT
 mov rdi,rbx
 call free@PLT
 pop rbx
 xor eax,eax
.Ldispatch_discard_end:
 ret
.size pg_session_dispatch_discard,.-pg_session_dispatch_discard
.globl pg_session_dispatch_drain
.type pg_session_dispatch_drain,@function
pg_session_dispatch_drain:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov eax,1
 test rbx,rbx
 jz .Ldispatch_drain_end
 cmp dword ptr [rbx+32],0
 jne .Ldispatch_drain_end
 cmp qword ptr [rbx],0
 je .Ldispatch_drain_end
 mov dword ptr [rbx+32],1
.Ldispatch_drain_each:
 mov r12,[rbx+8]
 test r12,r12
 jz .Ldispatch_drain_done
 mov rdi,[rbx]
 mov esi,1
 mov rdx,[r12+8]
 mov rcx,[r12+16]
 call pg_session_deliver
 test eax,eax
 jnz .Ldispatch_drain_pending
 mov rax,[r12]
 mov [rbx+8],rax
 test rax,rax
 jnz .Ldispatch_drain_release
 mov qword ptr [rbx+16],0
.Ldispatch_drain_release:
 mov rdi,[r12+8]
 call json_object_put@PLT
 mov rdi,r12
 call free@PLT
 jmp .Ldispatch_drain_each
.Ldispatch_drain_done:
 xor eax,eax
 jmp .Ldispatch_drain_unlock
.Ldispatch_drain_pending:
 mov eax,1
.Ldispatch_drain_unlock:
 mov dword ptr [rbx+32],0
.Ldispatch_drain_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.size pg_session_dispatch_drain,.-pg_session_dispatch_drain
.globl pg_session_dispatch_close
.type pg_session_dispatch_close,@function
pg_session_dispatch_close:
 mov eax,1
 test rdi,rdi
 jz .Ldispatch_close_end
 cmp dword ptr [rdi+32],0
 jne .Ldispatch_close_end
 cmp qword ptr [rdi+8],0
 jne .Ldispatch_close_end
 cmp qword ptr [rdi+24],0
 jne .Ldispatch_close_end
 mov dword ptr [rdi+36],1
 xor eax,eax
.Ldispatch_close_end:
 ret
.size pg_session_dispatch_close,.-pg_session_dispatch_close
.section .note.GNU-stack,"",@progbits
.text
/* Adoption wrapper: inner token, prepared dispatch token, first observer error. */
.globl pg_session_adoption_notifications_bind
.type pg_session_adoption_notifications_bind,@function
pg_session_adoption_notifications_bind:
 xor edx,edx
 jmp .Ladoption_bind
.size pg_session_adoption_notifications_bind,.-pg_session_adoption_notifications_bind
.globl pg_session_adoption_deferred_bind
.type pg_session_adoption_deferred_bind,@function
pg_session_adoption_deferred_bind:
 mov edx,1
.Ladoption_bind:
 mov eax,1
 test rdi,rdi
 jz .Ladoption_bind_end
 test rsi,rsi
 jz .Ladoption_bind_end
 mov rax,[rdi+24]
 test rax,rax
 jz .Ladoption_bind_bad
 mov rcx,[rdi]
 test rcx,rcx
 jz .Ladoption_bind_bad
 cmp [rax],rcx
 jne .Ladoption_bind_bad
 cmp dword ptr [rax+36],0
 jne .Ladoption_bind_bad
 mov [rsi],rdi
 lea rax,[rip+.Ladoption_prepare_stored]
 test edx,edx
 jz .Ladoption_bind_prepare
 lea rax,[rip+.Ladoption_prepare_deferred]
.Ladoption_bind_prepare:
 mov [rsi+8],rax
 lea rax,[rip+.Ladoption_adopt]
 mov [rsi+16],rax
 lea rax,[rip+.Ladoption_publish]
 mov [rsi+24],rax
 lea rax,[rip+.Ladoption_discard]
 mov [rsi+32],rax
 xor eax,eax
 ret
.Ladoption_bind_bad:
 mov eax,1
.Ladoption_bind_end:
 ret
.size pg_session_adoption_deferred_bind,.-pg_session_adoption_deferred_bind
.Ladoption_prepare_stored:
 xor eax,eax
 jmp .Ladoption_prepare
.Ladoption_prepare_deferred:
 mov eax,1
.Ladoption_prepare:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,24
 mov [rsp],eax
 mov rbx,rdi
 mov r12,r8
 mov r13,rdx
 mov r14,rcx
 mov r15,rsi
 mov qword ptr [r12],0
 mov ebp,3
 mov edi,1
 mov esi,32
 call calloc@PLT
 test rax,rax
 jz .Ladoption_prepare_end
 mov [r12],rax
 mov r8,rax
 mov rdi,rbx
 mov rsi,r15
 mov rdx,r13
 mov rcx,r14
 cmp dword ptr [rsp],0
 jne .Ladoption_prepare_dual
 call .Lhost_prepare
 jmp .Ladoption_prepared_inner
.Ladoption_prepare_dual:
 call .Lhost_prepare_deferred
.Ladoption_prepared_inner:
 mov ebp,eax
 test eax,eax
 jnz .Ladoption_prepare_end
 mov rsi,[r12]
 mov rdi,rbx
 call .Ladoption_reserve
 mov ebp,eax
.Ladoption_prepare_end:
 mov eax,ebp
 add rsp,24
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Ladoption_reserve:
 mov rax,[rsi]
 test rax,rax
 jz .Ladoption_reserve_empty
 lea rcx,[rsi+8]
 mov rsi,[rax+8]
 mov rdx,[rdi+16]
 mov rdi,[rdi+24]
 sub rsp,8
 call pg_session_dispatch_prepare
 add rsp,8
 test eax,eax
 jz .Ladoption_reserve_empty
 mov eax,3
 ret
.Ladoption_reserve_empty:
 xor eax,eax
 ret
.Ladoption_adopt:
 test rsi,rsi
 jz .Lhost_adopt_empty
 mov rsi,[rsi]
 jmp .Lhost_adopt_deferred
.Ladoption_publish:
 test rsi,rsi
 jz .Lhost_publish_empty
 cmp qword ptr [rsi],0
 je .Lhost_publish_empty
 test rcx,rcx
 jnz .Ladoption_publish_stored
 mov rsi,[rsi]
 jmp .Lhost_publish_deferred
.Ladoption_publish_stored:
 push rbx
 push r12
 push r13
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 cmp dword ptr [r12+20],0
 jne .Ladoption_publish_end
 mov dword ptr [r12+20],1
 mov rax,[rbx]
 mov rcx,[rax+16]
 mov rax,[rax+8]
.Ladoption_host_interest:
 test rcx,rcx
 jz .Ladoption_observe
 cmp dword ptr [rax+16],1
 je .Ladoption_enqueue
 add rax,24
 dec rcx
 jmp .Ladoption_host_interest
.Ladoption_enqueue:
 mov rdi,[rbx+24]
 mov rsi,[r12+8]
 call pg_session_dispatch_enqueue
 test eax,eax
 jnz .Ladoption_enqueue_failed
 mov qword ptr [r12+8],0
 jmp .Ladoption_observe
.Ladoption_enqueue_failed:
 mov dword ptr [r12+16],5
.Ladoption_observe:
 mov r13,[rbx+24]
 mov eax,[r13+32]
 mov [rsp],eax
 mov dword ptr [rsp+8],0
 mov dword ptr [r13+32],1
 mov rdi,[rbx]
 mov rax,[r12]
 mov rsi,[rax]
 test rsi,rsi
 jz .Ladoption_observed
 mov rdx,[rbx+16]
 lea rcx,[rsp+8]
 call pg_session_deliver_observed
 test eax,eax
 jz .Ladoption_observed
 cmp dword ptr [r12+16],0
 jne .Ladoption_observed
 mov dword ptr [r12+16],5
.Ladoption_observed:
 cmp dword ptr [r12+16],0
 jne .Ladoption_observer_saved
 mov eax,[rsp+8]
 mov [r12+16],eax
.Ladoption_observer_saved:
 mov eax,[rsp]
 mov [r13+32],eax
.Ladoption_publish_end:
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret
.Ladoption_discard:
 test rsi,rsi
 jz .Lhost_discard_end
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov rdi,[rbx+24]
 call pg_session_dispatch_drain
 mov r13d,eax
 cmp qword ptr [r12+8],0
 je .Ladoption_unused_released
 mov rdi,[rbx+24]
 mov rsi,[r12+8]
 call pg_session_dispatch_discard
.Ladoption_unused_released:
 mov eax,[r12+16]
 test eax,eax
 jnz .Ladoption_handle_observer
 test r13d,r13d
 jz .Ladoption_free
 mov rax,[rbx]
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 jne .Ladoption_free
 mov dword ptr [rax+52],4
 jmp .Ladoption_free
.Ladoption_handle_observer:
 mov rdx,[rbx]
 mov rcx,[rdx]
 cmp dword ptr [rcx+52],0
 jne .Ladoption_report
 mov dword ptr [rcx+52],4
 jmp .Ladoption_free
.Ladoption_report:
 cmp qword ptr [rdx+40],0
 je .Ladoption_free
 mov esi,eax
 mov rdi,[rdx+32]
 call [rdx+40]
.Ladoption_free:
 mov rdi,rbx
 mov rsi,[r12]
 call .Lhost_discard
 mov rdi,r12
 call free@PLT
 pop r13
 pop r12
 pop rbx
 ret
.globl pg_session_adoption_hold_bind
.type pg_session_adoption_hold_bind,@function
pg_session_adoption_hold_bind:
 mov eax,1
 test rdi,rdi
 jz .Ladoption_hold_bind_end
 test rsi,rsi
 jz .Ladoption_hold_bind_end
 mov rax,[rdi+24]
 test rax,rax
 jz .Ladoption_hold_bad
 mov rcx,[rdi]
 test rcx,rcx
 jz .Ladoption_hold_bad
 cmp [rax],rcx
 jne .Ladoption_hold_bad
 cmp dword ptr [rax+36],0
 jne .Ladoption_hold_bad
 cmp qword ptr [rdi+8],0
 je .Ladoption_hold_bad
 mov [rsi],rdi
 lea rax,[rip+.Ladoption_hold_open]
 mov [rsi+8],rax
 lea rax,[rip+.Ladoption_hold_append]
 mov [rsi+16],rax
 lea rax,[rip+.Ladoption_hold_prepare]
 mov [rsi+24],rax
 lea rax,[rip+.Ladoption_adopt]
 mov [rsi+32],rax
 lea rax,[rip+.Ladoption_publish]
 mov [rsi+40],rax
 lea rax,[rip+.Ladoption_discard]
 mov [rsi+48],rax
 xor eax,eax
 ret
.Ladoption_hold_bad:
 mov eax,1
.Ladoption_hold_bind_end:
 ret
.size pg_session_adoption_hold_bind,.-pg_session_adoption_hold_bind
.Ladoption_hold_open:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx,rdi
 mov r12,rcx
 mov r13,rsi
 mov r14,rdx
 mov qword ptr [r12],0
 mov edi,1
 mov esi,32
 call calloc@PLT
 test rax,rax
 jz .Ladoption_hold_open_bad
 mov [r12],rax
 mov rdi,rbx
 mov rsi,r13
 mov rdx,r14
 mov rcx,rax
 call .Lhold_notice_open
 jmp .Ladoption_hold_open_end
.Ladoption_hold_open_bad:
 mov eax,3
.Ladoption_hold_open_end:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Ladoption_hold_append:
 test rsi,rsi
 jz .Lhold_notice_append_empty
 mov rsi,[rsi]
 jmp .Lhold_notice_append
.Ladoption_hold_prepare:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rsi,[rsi]
 call .Lhold_notice_prepare
 test eax,eax
 jnz .Ladoption_hold_prepare_end
 mov rdi,rbx
 mov rsi,r12
 call .Ladoption_reserve
.Ladoption_hold_prepare_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.section .note.GNU-stack,"",@progbits
.text
.globl pg_session_dispatch_storage_commit
.type pg_session_dispatch_storage_commit,@function
pg_session_dispatch_storage_commit:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov eax,1
 test rbx,rbx
 jz .Ldispatch_storage_end
 cmp qword ptr [rbx+8],0
 je .Ldispatch_storage_end
 cmp qword ptr [rbx+16],0
 je .Ldispatch_storage_end
 mov rdi,[rbx]
 call [rbx+8]
 mov r12d,eax
 mov rdi,[rbx+16]
 call pg_session_dispatch_drain
 mov eax,r12d
.Ldispatch_storage_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.size pg_session_dispatch_storage_commit,.-pg_session_dispatch_storage_commit
.section .note.GNU-stack,"",@progbits

.text
.globl pg_session_listeners_begin_close
.type pg_session_listeners_begin_close,@function
pg_session_listeners_begin_close:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 mov eax,1
 test rbx,rbx
 jz .Lbegin_close_end
 cmp dword ptr [rbx+52],0
 jne .Lbegin_close_ok
 mov r13,[rbx+16]
 xor r14d,r14d
 test r13,r13
 jz .Lbegin_close_seal
 mov rdi,r13
 mov esi,24
 call calloc@PLT
 test rax,rax
 jz .Lbegin_close_invalid
 mov r14,rax
 mov rdi,rax
 mov rsi,[rbx+8]
 imul rdx,r13,24
 call memcpy@PLT
.Lbegin_close_seal:
 mov eax,[rbx+48]
 mov [rsp],eax
 mov dword ptr [rbx+52],2
 mov dword ptr [rbx+48],1
 xor ebp,ebp
 xor r15d,r15d
.Lbegin_close_compact:
 cmp rbp,r13
 jae .Lbegin_close_compacted
 imul rax,rbp,24
 add rax,r14
 inc rbp
 cmp dword ptr [rax+16],2
 je .Lbegin_close_compact
 imul rdx,r15,24
 add rdx,[rbx+8]
 mov rcx,[rax]
 mov [rdx],rcx
 mov rcx,[rax+8]
 mov [rdx+8],rcx
 mov rcx,[rax+16]
 mov [rdx+16],rcx
 inc r15
 jmp .Lbegin_close_compact
.Lbegin_close_compacted:
 mov [rbx+16],r15
 xor ebp,ebp
.Lbegin_close_each:
 cmp rbp,r13
 jae .Lbegin_close_release
 imul rax,rbp,24
 add rax,r14
 inc rbp
 cmp dword ptr [rax+16],2
 jne .Lbegin_close_each
 mov rcx,[rax]
 mov rdi,[rax+8]
 xor esi,esi
 mov rdx,r12
 call rcx
 test eax,eax
 jz .Lbegin_close_each
 cmp qword ptr [rbx+40],0
 je .Lbegin_close_each
 mov esi,eax
 mov rdi,[rbx+32]
 call [rbx+40]
 jmp .Lbegin_close_each
.Lbegin_close_release:
 mov eax,[rsp]
 mov [rbx+48],eax
 mov rdi,r14
 call free@PLT
.Lbegin_close_ok:
 xor eax,eax
 jmp .Lbegin_close_end
.Lbegin_close_invalid:
 mov eax,1
.Lbegin_close_end:
 add rsp,24
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_session_listeners_begin_close,.-pg_session_listeners_begin_close

.globl pg_durable_session_close_notified
.type pg_durable_session_close_notified,@function
pg_durable_session_close_notified:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 mov eax,-2
 test rbx,rbx
 jz .Lnotified_close_end
 test r12,r12
 jz .Lnotified_close_end
 mov rcx,[r12]
 test rcx,rcx
 jz .Lnotified_close_end
 mov rax,[rcx]
 cmp rax,[rbx]
 jne .Lnotified_close_invalid
 mov rax,[r12+24]
 test rax,rax
 jz .Lnotified_close_invalid
 cmp [rax],rcx
 jne .Lnotified_close_invalid
 cmp dword ptr [rbx+172],1
 jne .Lnotified_close_sealed
 mov dword ptr [rbx+172],2
.Lnotified_close_sealed:
 mov eax,[rbx+168]
 mov [rsp],eax
 mov dword ptr [rbx+168],1
 mov rdi,[rbx+24]
 call pg_hold_timer_stop@PLT
 mov rdi,[r12]
 mov rsi,[r12+16]
 call pg_session_listeners_begin_close
 mov edx,[rsp]
 mov [rbx+168],edx
 test eax,eax
 jnz .Lnotified_close_pending
 mov rdi,rbx
 call pg_durable_session_close
 mov [rsp+8],eax
 cmp dword ptr [rbx+172],4
 jne .Lnotified_close_end
 mov rdi,[r12+24]
 call pg_session_dispatch_close
 test eax,eax
 jnz .Lnotified_close_pending
 mov rdi,[r12]
 mov rsi,[r12+16]
 call pg_session_listeners_close
 test eax,eax
 jnz .Lnotified_close_pending
 mov eax,[rsp+8]
 jmp .Lnotified_close_end
.Lnotified_close_pending:
 mov eax,1
 jmp .Lnotified_close_end
.Lnotified_close_invalid:
 mov eax,-2
.Lnotified_close_end:
 add rsp,24
 pop r12
 pop rbx
 ret
.size pg_durable_session_close_notified,.-pg_durable_session_close_notified
.section .note.GNU-stack,"",@progbits

.text
.globl pg_document_observation_open
.type pg_document_observation_open,@function
pg_document_observation_open:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov eax,1
 test rbx,rbx
 jz .Lobservation_open_end
 test r12,r12
 jz .Lobservation_open_end
 cmp dword ptr [rbx+32],0
 jne .Lobservation_open_end
 cmp dword ptr [rbx+36],0
 jne .Lobservation_open_end
 cmp dword ptr [r12+116],1
 jne .Lobservation_open_end
 cmp dword ptr [r12+112],0
 jne .Lobservation_open_end
 mov rdi,[r12+16]
 lea rsi,[r12+24]
 mov rdx,[r12+88]
 call pg_document_check@PLT
 test eax,eax
 jnz .Lobservation_open_end
 mov rdi,[r12]
 lea rsi,[rsp]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lobservation_open_bad
 mov rdi,[r12+8]
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lobservation_open_bad
 mov rax,[rsp]
 mov [rbx],rax
 mov rax,[rsp+8]
 mov [rbx+8],rax
 mov rax,[r12+72]
 mov [rbx+16],rax
 mov rax,[r12+96]
 mov [rbx+24],rax
 mov dword ptr [rbx+32],1
 xor eax,eax
 jmp .Lobservation_open_end
.Lobservation_open_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,3
.Lobservation_open_end:
 add rsp,24
 pop r12
 pop rbx
 ret
.size pg_document_observation_open,.-pg_document_observation_open

.globl pg_document_observation_change_release
.type pg_document_observation_change_release,@function
pg_document_observation_change_release:
 push rbx
 mov rbx,rdi
 test rbx,rbx
 jz .Lobservation_change_end
 mov rdi,[rbx]
 call json_object_put@PLT
 mov rdi,[rbx+8]
 call json_object_put@PLT
 mov qword ptr [rbx],0
 mov qword ptr [rbx+8],0
 mov qword ptr [rbx+16],0
.Lobservation_change_end:
 pop rbx
 ret
.size pg_document_observation_change_release,.-pg_document_observation_change_release

.globl pg_document_observation_close
.type pg_document_observation_close,@function
pg_document_observation_close:
 mov eax,1
 test rdi,rdi
 jz .Lobservation_close_end
 cmp dword ptr [rdi+36],0
 jne .Lobservation_close_end
 push rbx
 mov rbx,rdi
 call pg_document_observation_change_release
 mov qword ptr [rbx+24],0
 mov qword ptr [rbx+32],0
 xor eax,eax
 pop rbx
.Lobservation_close_end:
 ret
.size pg_document_observation_close,.-pg_document_observation_close

.globl pg_document_observation_advance
.type pg_document_observation_advance,@function
pg_document_observation_advance:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,104
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov eax,1
 test r13,r13
 jz .Lobservation_advance_end
 mov qword ptr [r13],0
 mov qword ptr [r13+8],0
 mov qword ptr [r13+16],0
 test rbx,rbx
 jz .Lobservation_advance_end
 test r12,r12
 jz .Lobservation_advance_end
 cmp dword ptr [rbx+32],1
 jne .Lobservation_advance_end
 cmp dword ptr [rbx+36],0
 jne .Lobservation_advance_end
 mov r14,[r12]
 test r14,r14
 jz .Lobservation_advance_end
 mov dword ptr [rbx+36],1
 xor ebp,ebp
 mov qword ptr [rsp+64],0
 mov qword ptr [rsp+72],0
 mov qword ptr [rsp+80],0
 mov rax,[r14+48]
 cmp rax,[rbx+16]
 je .Lobservation_address_match
 mov rdi,[rbx]
 mov rsi,rsp
 call pg_document_record_decode@PLT
 test eax,eax
 jnz .Lobservation_invalid
 mov rdi,rsp
 mov rsi,r14
 call pg_document_address_equal@PLT
 test eax,eax
 jz .Lobservation_missing
.Lobservation_address_match:
 cmp qword ptr [r12+8],0
 jne .Lobservation_value
 mov rax,[r14+48]
 cmp rax,[rbx+16]
 jne .Lobservation_missing
 cmp qword ptr [rbx+8],0
 je .Lobservation_missing
 mov ebp,1
 mov rdi,[rbx+8]
 jmp .Lobservation_copy_value
.Lobservation_value:
 mov rdi,[r12+8]
 call json_object_get_type@PLT
 cmp eax,4
 jne .Lobservation_invalid
 mov rax,[r12+24]
 test rax,rax
 jz .Lobservation_invalid
 mov rdx,9007199254740991
 cmp rax,rdx
 ja .Lobservation_invalid
 mov rax,[r14+48]
 cmp rax,[rbx+16]
 jne .Lobservation_replace
 mov rax,[r12+24]
 cmp rax,[rbx+24]
 jne .Lobservation_replace
 mov rdi,[r12+16]
 test rdi,rdi
 jz .Lobservation_invalid
 call json_object_get_type@PLT
 cmp eax,5
 jne .Lobservation_invalid
 mov rdi,[r12+16]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lobservation_missing
 jmp .Lobservation_new_value
.Lobservation_replace:
 mov ebp,2
.Lobservation_new_value:
 mov rdi,[r12+8]
.Lobservation_copy_value:
 cmp ebp,1
 jne .Lobservation_clone_value
 call json_object_get@PLT
 mov [rsp+64],rax
 jmp .Lobservation_value_copied
.Lobservation_clone_value:
 lea rsi,[rsp+64]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lobservation_allocation
.Lobservation_value_copied:
 cmp ebp,2
 je .Lobservation_root
 test ebp,ebp
 jz .Lobservation_copy_ops
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lobservation_allocation
 mov [rsp+72],rax
 jmp .Lobservation_adopt
.Lobservation_copy_ops:
 mov rdi,[r12+16]
 lea rsi,[rsp+72]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lobservation_allocation
 jmp .Lobservation_adopt
.Lobservation_root:
 mov edi,1
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lobservation_allocation
 mov [rsp+72],rax
 mov edi,2
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lobservation_allocation
 mov [rsp+80],rax
 lea rdi,[rip+.Lobservation_root_name]
 call json_object_new_string@PLT
 test rax,rax
 jz .Lobservation_allocation
 mov r15,rax
 mov rdi,[rsp+80]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lobservation_root_item_bad
 mov rdi,[rsp+64]
 call json_object_get@PLT
 mov r15,rax
 mov rdi,[rsp+80]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lobservation_root_item_bad
 mov rdi,[rsp+72]
 mov rsi,[rsp+80]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lobservation_allocation
 mov qword ptr [rsp+80],0
.Lobservation_adopt:
 mov rdi,[rbx+8]
 call json_object_put@PLT
 mov qword ptr [rbx+8],0
 cmp ebp,1
 jne .Lobservation_adopt_value
 mov qword ptr [rbx+16],0
 mov dword ptr [r13+16],1
 jmp .Lobservation_output
.Lobservation_adopt_value:
 mov rdi,[rsp+64]
 call json_object_get@PLT
 mov [rbx+8],rax
 mov rax,[r14+48]
 mov [rbx+16],rax
 mov rax,[r12+24]
 mov [rbx+24],rax
 cmp ebp,2
 sete al
 movzx eax,al
 mov [r13+20],eax
.Lobservation_output:
 mov rax,[rsp+64]
 mov [r13],rax
 mov rax,[rsp+72]
 mov [r13+8],rax
 xor eax,eax
 jmp .Lobservation_unlock
.Lobservation_root_item_bad:
 mov rdi,r15
 call json_object_put@PLT
.Lobservation_allocation:
 mov r15d,3
 jmp .Lobservation_cleanup
.Lobservation_missing:
 mov r15d,8
 jmp .Lobservation_cleanup
.Lobservation_invalid:
 mov r15d,1
.Lobservation_cleanup:
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov rdi,[rsp+72]
 call json_object_put@PLT
 mov rdi,[rsp+80]
 call json_object_put@PLT
 mov eax,r15d
.Lobservation_unlock:
 mov dword ptr [rbx+36],0
.Lobservation_advance_end:
 add rsp,104
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_observation_advance,.-pg_document_observation_advance
.section .rodata
.Lobservation_root_name: .asciz "r"
.section .note.GNU-stack,"",@progbits

.text
/* Caller-owned binding survives snapshots until release sees an idle registry. */
.globl pg_document_observer_detach
.type pg_document_observer_detach,@function
pg_document_observer_detach:
 push rbx
 mov rbx,rdi
 test rbx,rbx
 jz .Lobserver_detach_end
 cmp dword ptr [rbx+96],0
 je .Lobserver_detach_end
 mov dword ptr [rbx+96],0
 mov rdi,[rbx+40]
 xor esi,esi
 lea rdx,[rip+.Lobserver_commit]
 mov rcx,rbx
 call pg_session_unlisten
 mov rdi,[rbx+40]
 mov esi,2
 lea rdx,[rip+.Lobserver_close]
 mov rcx,rbx
 call pg_session_unlisten
 mov rsi,[rbx+88]
 mov qword ptr [rbx+88],0
 test rsi,rsi
 jz .Lobserver_detach_end
 mov rdi,[rbx+48]
 call [rbx+80]
.Lobserver_detach_end:
 pop rbx
 ret
.size pg_document_observer_detach,.-pg_document_observer_detach

.globl pg_document_observer_release
.type pg_document_observer_release,@function
pg_document_observer_release:
 mov eax,1
 test rdi,rdi
 jz .Lobserver_release_end
 cmp dword ptr [rdi+100],0
 jne .Lobserver_release_end
 mov rax,[rdi+40]
 test rax,rax
 jz .Lobserver_release_owned
 cmp dword ptr [rax+48],0
 jne .Lobserver_release_invalid
.Lobserver_release_owned:
 push rbx
 mov rbx,rdi
 call pg_document_observer_detach
 mov rdi,rbx
 call pg_document_observation_close
 pop rbx
 ret
.Lobserver_release_invalid:
 mov eax,1
.Lobserver_release_end:
 ret
.size pg_document_observer_release,.-pg_document_observer_release

.Lobserver_report:
 mov rax,[rdi+40]
 cmp qword ptr [rax+40],0
 je .Lobserver_report_end
 mov rdi,[rax+32]
 jmp [rax+40]
.Lobserver_report_end:
 ret

.Lobserver_end:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12d,esi
 cmp dword ptr [rbx+96],0
 je .Lobserver_end_done
 call pg_document_observer_detach
 mov rdi,[rbx+48]
 mov esi,r12d
 call [rbx+64]
 test eax,eax
 jz .Lobserver_end_done
 mov rdi,rbx
 mov esi,eax
 call .Lobserver_report
.Lobserver_end_done:
 add rsp,8
 pop r12
 pop rbx
 ret

.Lobserver_close:
 push rbx
 mov rbx,rdi
 mov rax,[rbx+40]
 mov rax,[rax]
 mov esi,[rax+52]
 call .Lobserver_end
 xor eax,eax
 pop rbx
 ret

.Lobserver_pin_update:
 push rbx
 mov rbx,rdi
 mov rax,[rbx+16]
 cmp rax,[rbx+88]
 je .Lobserver_pin_ok
 mov rsi,[rbx+88]
 mov qword ptr [rbx+88],0
 test rsi,rsi
 jz .Lobserver_pin_new
 mov rdi,[rbx+48]
 call [rbx+80]
.Lobserver_pin_new:
 mov rsi,[rbx+16]
 test rsi,rsi
 jz .Lobserver_pin_ok
 mov [rbx+88],rsi
 mov rdi,[rbx+48]
 call [rbx+72]
 test eax,eax
 jz .Lobserver_pin_end
 mov qword ptr [rbx+88],0
 jmp .Lobserver_pin_end
.Lobserver_pin_ok:
 xor eax,eax
.Lobserver_pin_end:
 pop rbx
 ret

.globl pg_document_observer_open
.type pg_document_observer_open,@function
pg_document_observer_open:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov eax,1
 test rbx,rbx
 jz .Lobserver_open_end
 test r12,r12
 jz .Lobserver_open_end
 test r14,r14
 jz .Lobserver_open_end
 cmp dword ptr [rbx+96],0
 jne .Lobserver_open_end
 cmp dword ptr [rbx+100],0
 jne .Lobserver_open_end
 cmp dword ptr [r12+52],0
 jne .Lobserver_open_end
 mov rax,[r12]
 test rax,rax
 jz .Lobserver_open_invalid
 cmp dword ptr [rax+52],0
 jne .Lobserver_open_invalid
 cmp qword ptr [r14+8],0
 je .Lobserver_open_invalid
 cmp qword ptr [r14+16],0
 je .Lobserver_open_invalid
 cmp qword ptr [r14+24],0
 je .Lobserver_open_invalid
 cmp qword ptr [r14+32],0
 je .Lobserver_open_invalid
 mov rsi,r13
 call pg_document_observation_open
 test eax,eax
 jnz .Lobserver_open_end
 mov [rbx+40],r12
 mov rax,[r14]
 mov [rbx+48],rax
 mov rax,[r14+8]
 mov [rbx+56],rax
 mov rax,[r14+16]
 mov [rbx+64],rax
 mov rax,[r14+24]
 mov [rbx+72],rax
 mov rax,[r14+32]
 mov [rbx+80],rax
 mov qword ptr [rbx+88],0
 mov dword ptr [rbx+96],1
 mov rdi,r12
 xor esi,esi
 lea rdx,[rip+.Lobserver_commit]
 mov rcx,rbx
 call pg_session_listen
 test eax,eax
 jnz .Lobserver_open_cleanup
 mov rdi,r12
 mov esi,2
 lea rdx,[rip+.Lobserver_close]
 mov rcx,rbx
 call pg_session_listen
 test eax,eax
 jnz .Lobserver_open_cleanup
 mov rdi,rbx
 call .Lobserver_pin_update
 test eax,eax
 jz .Lobserver_open_end
.Lobserver_open_cleanup:
 mov rdi,rbx
 call pg_document_observer_detach
 mov rdi,rbx
 call pg_document_observation_close
 mov eax,3
 jmp .Lobserver_open_end
.Lobserver_open_invalid:
 mov eax,1
.Lobserver_open_end:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_document_observer_open,.-pg_document_observer_open

.Lobserver_commit:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,184
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor ebp,ebp
 cmp dword ptr [rbx+96],0
 je .Lobserver_commit_end
 cmp dword ptr [rbx+100],0
 jne .Lobserver_commit_invalid
 mov dword ptr [rbx+100],1
 mov qword ptr [rsp+96],0
 mov qword ptr [rsp+104],0
 mov qword ptr [rsp+112],0
 mov dword ptr [rsp+144],0
 mov rdi,[rbx]
 mov rsi,rsp
 call pg_document_record_decode@PLT
 test eax,eax
 jnz .Lobserver_commit_error
 mov eax,[rsp+40]
 mov [rsp+152],eax
 mov rax,[rsp+32]
 mov [rsp+160],rax
 mov rdi,r12
 lea rsi,[rip+.Lpublication_changes]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lobserver_commit_bad
 mov [rsp+120],rax
 mov rdi,rax
 call json_object_array_length@PLT
 mov r15,rax
 xor r14d,r14d
.Lobserver_commit_each:
 cmp dword ptr [rbx+96],0
 je .Lobserver_commit_cleanup
 cmp r14,r15
 jae .Lobserver_commit_owner
 mov rdi,[rsp+120]
 mov rsi,r14
 inc r14
 call json_object_array_get_idx@PLT
 mov r12,rax
 mov rdi,rax
 lea rsi,[rip+.Lpublication_type]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lobserver_commit_bad
 mov [rsp+128],rax
 mov rdi,rax
 lea rsi,[rip+.Lpublication_task]
 call strcmp@PLT
 test eax,eax
 jz .Lobserver_commit_task
 mov rdi,[rsp+128]
 lea rsi,[rip+.Lnotice_document]
 call strcmp@PLT
 test eax,eax
 jnz .Lobserver_commit_each
 mov rdi,r12
 lea rsi,[rip+.Lnotice_record]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,rsp
 call pg_document_record_decode@PLT
 test eax,eax
 jnz .Lobserver_commit_error
 mov rax,rsp
 mov [rsp+64],rax
 mov rdi,r12
 lea rsi,[rip+.Lnotice_value_key]
 call json_object_object_get@PLT
 mov [rsp+72],rax
 mov rdi,r12
 lea rsi,[rip+.Lnotice_ops]
 call json_object_object_get@PLT
 mov [rsp+80],rax
 mov rdi,r12
 lea rsi,[rip+.Lnotice_version]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_int64@PLT
 mov [rsp+88],rax
 mov rdi,rbx
 lea rsi,[rsp+64]
 lea rdx,[rsp+96]
 call pg_document_observation_advance
 cmp eax,8
 je .Lobserver_commit_each
 test eax,eax
 jnz .Lobserver_commit_error
 mov rdi,rbx
 call .Lobserver_pin_update
 test eax,eax
 jnz .Lobserver_commit_error
 mov rdi,[rbx+48]
 lea rsi,[rsp+96]
 mov rdx,r13
 call [rbx+56]
 test eax,eax
 jz .Lobserver_commit_delivered
 mov [rsp+168],eax
 mov rdi,rbx
 mov esi,eax
 call .Lobserver_report
 mov rdi,rbx
 mov esi,[rsp+168]
 call .Lobserver_end
.Lobserver_commit_delivered:
 lea rdi,[rsp+96]
 call pg_document_observation_change_release
 jmp .Lobserver_commit_each
.Lobserver_commit_task:
 cmp dword ptr [rsp+152],2
 jne .Lobserver_commit_each
 mov rdi,r12
 lea rsi,[rip+.Lnotice_value_key]
 call json_object_object_get@PLT
 mov r12,rax
 mov rdi,rax
 lea rsi,[rip+.Lobserver_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_int64@PLT
 cmp rax,[rsp+160]
 jne .Lobserver_commit_each
 mov rdi,r12
 lea rsi,[rip+.Lobserver_state]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lobserver_status]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lobserver_commit_bad
 mov rdi,rax
 lea rsi,[rip+.Lobserver_terminal]
 call strcmp@PLT
 test eax,eax
 jnz .Lobserver_commit_each
 mov dword ptr [rsp+144],1
 jmp .Lobserver_commit_each
.Lobserver_commit_owner:
 cmp dword ptr [rsp+144],0
 je .Lobserver_commit_cleanup
 mov rdi,rbx
 xor esi,esi
 call .Lobserver_end
 jmp .Lobserver_commit_cleanup
.Lobserver_commit_bad:
 mov eax,1
.Lobserver_commit_error:
 mov ebp,eax
.Lobserver_commit_cleanup:
 lea rdi,[rsp+96]
 call pg_document_observation_change_release
 mov dword ptr [rbx+100],0
 jmp .Lobserver_commit_end
.Lobserver_commit_invalid:
 mov ebp,1
.Lobserver_commit_end:
 mov eax,ebp
 add rsp,184
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lobserver_id: .asciz "id"
.Lobserver_state: .asciz "state"
.Lobserver_status: .asciz "status"
.Lobserver_terminal: .asciz "terminal"
.section .note.GNU-stack,"",@progbits

.text
.globl pg_session_observers_pin_count
.type pg_session_observers_pin_count,@function
pg_session_observers_pin_count:
 xor eax,eax
 test rdi,rdi
 jz .Lobserver_count_end
 test rsi,rsi
 jz .Lobserver_count_end
 mov r8,[rdi+8]
 mov rcx,[rdi+16]
 lea r9,[rip+.Lobserver_commit]
.Lobserver_count_each:
 test rcx,rcx
 jz .Lobserver_count_end
 cmp dword ptr [r8+16],0
 jne .Lobserver_count_next
 cmp [r8],r9
 jne .Lobserver_count_next
 mov rdx,[r8+8]
 cmp dword ptr [rdx+96],0
 je .Lobserver_count_next
 cmp [rdx+88],rsi
 jne .Lobserver_count_next
 inc rax
.Lobserver_count_next:
 add r8,24
 dec rcx
 jmp .Lobserver_count_each
.Lobserver_count_end:
 ret
.size pg_session_observers_pin_count,.-pg_session_observers_pin_count
.globl pg_session_observers_pinned
.type pg_session_observers_pinned,@function
pg_session_observers_pinned:
 sub rsp,8
 call pg_session_observers_pin_count
 test rax,rax
 setne al
 movzx eax,al
 add rsp,8
 ret
.size pg_session_observers_pinned,.-pg_session_observers_pinned
.section .note.GNU-stack,"",@progbits
