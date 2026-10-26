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
 call pg_document_holds_flush_one@PLT
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
 push rbx
 push r12
 sub rsp,8
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
 call pg_document_tx_deferred_merge@PLT
 jmp .Ldurable_settled
.Ldurable_settle_stored:
 mov rsi,rcx
 lea rdx,[rbx+120]
 call pg_document_tx_merge@PLT
.Ldurable_settled:
 mov r12d,eax
 mov qword ptr [rbx+152],0
 mov qword ptr [rbx+160],0
 cmp dword ptr [rbx+172],1
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
 call pg_document_holds_finish@PLT
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
 cmp qword ptr [rbx+16+r12*8],0
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
 call [rbx+16+r12*8]
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
