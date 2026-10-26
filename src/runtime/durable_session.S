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
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,24
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
