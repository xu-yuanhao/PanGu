.intel_syntax noprefix
.macro TGET object,key
 mov rdi,\object
 lea rsi,[rip+.Ltr_\key]
 call json_object_object_get@PLT
.endm
.text
.Ltr_equal:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ltr_equal_return
 mov rdi,r12
 call strlen@PLT
 mov [rsp],rax
 mov rdi,rbx
 call json_object_get_string_len@PLT
 cmp rax,[rsp]
 jne .Ltr_not_equal
 mov rdi,rbx
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,r12
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 jmp .Ltr_equal_return
.Ltr_not_equal:
 xor eax,eax
.Ltr_equal_return:
 add rsp,8
 pop r12
 pop rbx
 ret
/* Pi jsonEqual treats integral/floating JSON numbers as one numeric type. */
.Ltr_json_equal:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov r12,rsi
 cmp rdi,rsi
 je .Ltr_json_yes
 call json_object_get_type@PLT
 mov r13d,eax
 mov rdi,r12
 call json_object_get_type@PLT
 mov r14d,eax
 cmp r13d,2
 je .Ltr_json_left_number
 cmp r13d,3
 jne .Ltr_json_types
.Ltr_json_left_number:
 cmp r14d,2
 je .Ltr_json_numbers
 cmp r14d,3
 je .Ltr_json_numbers
.Ltr_json_types:
 cmp r13d,r14d
 jne .Ltr_json_no
 cmp r13d,5
 je .Ltr_json_array
 cmp r13d,4
 je .Ltr_json_object
 mov rdi,rbx
 mov rsi,r12
 call json_object_equal@PLT
 jmp .Ltr_json_return
.Ltr_json_numbers:
 mov rdi,rbx
 call json_object_get_double@PLT
 movsd [rsp],xmm0
 mov rdi,r12
 call json_object_get_double@PLT
 ucomisd xmm0,qword ptr [rsp]
 jp .Ltr_json_no
 jne .Ltr_json_no
 jmp .Ltr_json_yes
.Ltr_json_array:
 mov rdi,rbx
 call json_object_array_length@PLT
 mov r13,rax
 mov rdi,r12
 call json_object_array_length@PLT
 cmp rax,r13
 jne .Ltr_json_no
 xor r14d,r14d
.Ltr_json_array_each:
 cmp r14,r13
 jae .Ltr_json_yes
 mov rdi,rbx
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbp,rax
 mov rdi,r12
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rbp
 mov rsi,rax
 call .Ltr_json_equal
 test eax,eax
 jz .Ltr_json_no
 inc r14
 jmp .Ltr_json_array_each
.Ltr_json_object:
 mov rdi,rbx
 call json_object_object_length@PLT
 mov r13d,eax
 mov rdi,r12
 call json_object_object_length@PLT
 cmp eax,r13d
 jne .Ltr_json_no
 mov rdi,rbx
 call json_object_iter_begin@PLT
 mov [rsp+8],rax
 mov rdi,rbx
 call json_object_iter_end@PLT
 mov [rsp+16],rax
.Ltr_json_object_each:
 lea rdi,[rsp+8]
 lea rsi,[rsp+16]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Ltr_json_yes
 lea rdi,[rsp+8]
 call json_object_iter_peek_name@PLT
 mov rdi,r12
 mov rsi,rax
 lea rdx,[rsp+24]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Ltr_json_no
 lea rdi,[rsp+8]
 call json_object_iter_peek_value@PLT
 mov rdi,rax
 mov rsi,[rsp+24]
 call .Ltr_json_equal
 test eax,eax
 jz .Ltr_json_no
 lea rdi,[rsp+8]
 call json_object_iter_next@PLT
 jmp .Ltr_json_object_each
.Ltr_json_yes:
 mov eax,1
 jmp .Ltr_json_return
.Ltr_json_no:
 xor eax,eax
.Ltr_json_return:
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Take an owned JSON field; preserve ownership on allocation failures. */
.Ltr_set:
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Ltr_set_done
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,1
.Ltr_set_done:
 pop rbx
 ret
/* State -> 0 pending,1 running,2 waiting,3 sleeping,4 completing,5 terminal. */
.Ltr_state:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ltr_state_bad
 TGET rbx,status
 mov rbx,rax
 xor r12d,r12d
.Ltr_state_each:
 lea rax,[rip+.Ltr_status_names]
 movsxd rsi,dword ptr [rax+r12*4]
 add rsi,rax
 mov rdi,rbx
 call .Ltr_equal
 test eax,eax
 jnz .Ltr_state_found
 inc r12
 cmp r12,6
 jb .Ltr_state_each
.Ltr_state_bad:
 mov eax,-1
 jmp .Ltr_state_return
.Ltr_state_found:
 mov eax,r12d
.Ltr_state_return:
 add rsp,8
 pop r12
 pop rbx
 ret
.Ltr_record_state:
 sub rsp,8
 TGET rdi,state_key
 mov rdi,rax
 call .Ltr_state
 add rsp,8
 ret
/* One task-owned or conversation-owned scan, return 1 live,0 none,-1 error. */
.Ltr_scan_live:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,144
 mov rbx,rdi
 mov r13,rsi
 mov r14,rdx
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 movdqu [rsp+64],xmm0
 movdqu [rsp+80],xmm0
 movdqu [rsp+96],xmm0
 movdqu [rsp+112],xmm0
 mov qword ptr [rsp+128],0
 mov dword ptr [rsp],2
 mov dword ptr [rsp+4],1
 mov qword ptr [rsp+24],256
 mov [rsp+32],rsi
 cmp esi,4
 jne .Ltr_live_conversation
 mov qword ptr [rsp+32],0
 jmp .Ltr_live_page
.Ltr_live_conversation:
 mov [rsp+40],rdx
.Ltr_live_page:
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+112]
 call [rbx+16]
 test eax,eax
 jnz .Ltr_live_error
 xor r12d,r12d
.Ltr_live_row:
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ltr_live_next_page
 mov rdi,[rsp+112]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 cmp r13d,4
 jne .Ltr_live_check_state
 mov [rsp+136],rax
 TGET rax,owner
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ltr_live_skip
 cmp rax,r14
 jne .Ltr_live_skip
 mov rax,[rsp+136]
.Ltr_live_check_state:
 mov rdi,rax
 call .Ltr_record_state
 cmp eax,5
 jne .Ltr_live_found
.Ltr_live_skip:
 inc r12
 jmp .Ltr_live_row
.Ltr_live_next_page:
 cmp dword ptr [rsp+132],0
 je .Ltr_live_none
 mov rax,[rsp+120]
 cmp dword ptr [rsp+8],0
 je .Ltr_live_cursor
 cmp rax,[rsp+16]
 jbe .Ltr_live_error
.Ltr_live_cursor:
 mov [rsp+16],rax
 mov dword ptr [rsp+8],1
 mov dword ptr [rsp+12],1
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 jmp .Ltr_live_page
.Ltr_live_found:
 mov ebp,1
 jmp .Ltr_live_cleanup
.Ltr_live_none:
 xor ebp,ebp
 jmp .Ltr_live_cleanup
.Ltr_live_error:
 mov ebp,-1
.Ltr_live_cleanup:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,144
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Ordinary owned tasks plus top-level work in conversations owned by a task. */
.Ltr_owned:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,144
 mov rbx,rdi
 mov r12,rsi
 mov esi,4
 mov rdx,r12
 call .Ltr_scan_live
 test eax,eax
 jnz .Ltr_owned_return
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 movdqu [rsp+64],xmm0
 movdqu [rsp+80],xmm0
 movdqu [rsp+96],xmm0
 movdqu [rsp+112],xmm0
 mov qword ptr [rsp+128],0
 mov dword ptr [rsp+4],1
 mov qword ptr [rsp+24],256
 mov qword ptr [rsp+32],4
 mov [rsp+56],r12
.Ltr_owned_page:
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+112]
 call [rbx+16]
 test eax,eax
 jnz .Ltr_owned_error
 xor r13d,r13d
.Ltr_owned_row:
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Ltr_owned_page_done
 mov rdi,[rsp+112]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 TGET rax,id
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ltr_owned_error
 mov rdi,rbx
 mov esi,257
 mov rdx,rax
 call .Ltr_scan_live
 test eax,eax
 jnz .Ltr_owned_clean_result
 inc r13
 jmp .Ltr_owned_row
.Ltr_owned_page_done:
 cmp dword ptr [rsp+132],0
 je .Ltr_owned_none
 mov rax,[rsp+120]
 cmp dword ptr [rsp+8],0
 je .Ltr_owned_cursor
 cmp rax,[rsp+16]
 jbe .Ltr_owned_error
.Ltr_owned_cursor:
 mov [rsp+16],rax
 mov dword ptr [rsp+8],1
 mov dword ptr [rsp+12],1
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 jmp .Ltr_owned_page
.Ltr_owned_error:
 mov eax,-1
 jmp .Ltr_owned_clean_result
.Ltr_owned_none:
 xor eax,eax
.Ltr_owned_clean_result:
 mov ebp,eax
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov eax,ebp
.Ltr_owned_return:
 add rsp,144
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Persist a candidate and retain its timestamped Tx snapshot before admission. */
.Ltr_store:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,144
 mov rbx,rdi
 mov r12,rsi
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 movdqu [rsp+64],xmm0
 movdqu [rsp+80],xmm0
 movdqu [rsp+96],xmm0
 mov r13,[rbx+8]
 mov rax,[r13]
 mov [rsp+112],rax
 mov rax,[r13+24]
 mov [rsp+120],rax
 cmp qword ptr [rbx+112],0
 jne .Ltr_store_admitted
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+48]
 mov rcx,[r13]
 mov r8,[r13+8]
 call pg_durable_session_begin@PLT
 mov ebp,1
 test eax,eax
 jnz .Ltr_store_cleanup
.Ltr_store_admitted:
 mov rdi,[rbx+112]
 test rdi,rdi
 jnz .Ltr_store_set
 mov rdi,rsp
.Ltr_store_set:
 mov rsi,r12
 mov rdx,[r13]
 mov rcx,[r13+40]
 call pg_record_tx_set_task@PLT
 test eax,eax
 jnz .Ltr_store_reject
 mov rdi,[rbx+112]
 test rdi,rdi
 jnz .Ltr_store_candidates
 mov rdi,rsp
.Ltr_store_candidates:
 lea rsi,[rsp+96]
 call pg_record_tx_staged_tasks@PLT
 test eax,eax
 jnz .Ltr_store_reject
 xor r14d,r14d
.Ltr_store_find_snapshot:
 mov rdi,[rsp+96]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Ltr_store_alloc
 mov rdi,[rsp+96]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov [rsp+128],rax
 TGET rax,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rbx+16]
 je .Ltr_store_snapshot
 inc r14
 jmp .Ltr_store_find_snapshot
.Ltr_store_snapshot:
 mov rdi,[rsp+128]
 call json_object_get@PLT
 mov [rsp+104],rax
 test rax,rax
 jz .Ltr_store_alloc
 mov rdi,[rbx]
 xor esi,esi
 xor edx,edx
 lea rcx,[rsp+112]
 call pg_durable_session_settle@PLT
 test eax,eax
 jnz .Ltr_store_reject
 mov rdi,[rbx+24]
 call json_object_put@PLT
 mov rax,[rsp+104]
 mov [rbx+24],rax
 mov qword ptr [rsp+104],0
 xor ebp,ebp
 jmp .Ltr_store_cleanup
.Ltr_store_alloc:
 mov eax,4
.Ltr_store_reject:
 mov ebp,4
 cmp eax,4
 jne .Ltr_store_status
 mov ebp,5
.Ltr_store_status:
 mov rax,[rbx]
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 je .Ltr_store_abort
 mov ebp,6
.Ltr_store_abort:
 mov rdi,[rbx]
 mov rax,[rdi+152]
 lea rdx,[rsp]
 cmp rax,rdx
 jne .Ltr_store_cleanup
 call pg_durable_session_abort@PLT
.Ltr_store_cleanup:
 mov rdi,[rsp+96]
 call json_object_put@PLT
 mov rdi,[rsp+104]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,144
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Clone the record, replace state, reset progress counters and persist. */
.Ltr_replace:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13d,edx
 mov qword ptr [rsp],0
 mov rdi,[rbx+24]
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 mov ebp,5
 test eax,eax
 jnz .Ltr_replace_cleanup
 mov rdi,r12
 call json_object_get@PLT
 mov rdx,rax
 mov rdi,[rsp]
 lea rsi,[rip+.Ltr_state_key]
 call .Ltr_set
 test eax,eax
 jnz .Ltr_replace_cleanup
 mov rdi,r12
 call .Ltr_state
 cmp eax,4
 jb .Ltr_replace_progress
 mov rdi,[rsp]
 lea rsi,[rip+.Ltr_memos]
 call json_object_object_del@PLT
 jmp .Ltr_replace_reset
.Ltr_replace_progress:
 test r13d,r13d
 jz .Ltr_replace_store
 TGET "qword ptr [rbx+24]",state_key
 mov rdi,rax
 mov rsi,r12
 call .Ltr_json_equal
 test eax,eax
 jnz .Ltr_replace_store
.Ltr_replace_reset:
 mov rdi,[rsp]
 lea rsi,[rip+.Ltr_restarts]
 call json_object_object_del@PLT
.Ltr_replace_store:
 mov rdi,rbx
 mov rsi,[rsp]
 call .Ltr_store
 mov ebp,eax
.Ltr_replace_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.globl pg_task_runner_open
.type pg_task_runner_open,@function
pg_task_runner_open:
 mov eax,4
 test rdi,rdi
 jz .Ltr_open_return
 test rsi,rsi
 jz .Ltr_open_return
 test rcx,rcx
 jz .Ltr_open_return
 test rdx,rdx
 jz .Ltr_open_return
 mov r9,9007199254740991
 cmp rdx,r9
 ja .Ltr_open_return
 test r8,r8
 jz .Ltr_open_return
 cmp dword ptr [rdi+76],0
 jne .Ltr_open_return
 cmp dword ptr [rdi+72],0
 jne .Ltr_open_return
 cmp dword ptr [rsi+172],1
 jne .Ltr_open_return
 mov r9,[rsi]
 cmp dword ptr [r9+52],0
 jne .Ltr_open_return
 cmp dword ptr [r9+56],0
 jne .Ltr_open_return
 cmp qword ptr [rcx+8],0
 je .Ltr_open_return
 cmp qword ptr [rcx+16],0
 je .Ltr_open_return
 cmp qword ptr [rcx+24],0
 je .Ltr_open_return
 cmp qword ptr [rcx+32],0
 je .Ltr_open_return
 cmp qword ptr [rcx+40],0
 je .Ltr_open_return
 mov [rdi],rsi
 mov [rdi+8],rcx
 mov [rdi+16],rdx
 mov qword ptr [rdi+24],0
 mov qword ptr [rdi+32],0
 mov qword ptr [rdi+40],0
 mov qword ptr [rdi+48],0
 mov qword ptr [rdi+56],0
 mov [rdi+64],r8
 mov dword ptr [rdi+76],1
 mov qword ptr [rdi+80],0
 mov qword ptr [rdi+88],0
 mov qword ptr [rdi+96],0
 mov qword ptr [rdi+104],0
 mov qword ptr [rdi+112],0
 mov qword ptr [rdi+120],0
 mov qword ptr [rdi+128],0
 mov qword ptr [rdi+136],0
 mov qword ptr [rdi+144],0
 mov qword ptr [rdi+152],0
 xor eax,eax
.Ltr_open_return:
 ret
.size pg_task_runner_open,.-pg_task_runner_open

/* Read failures are Storage failures; a missing task is a blocked execution. */
.Ltr_fetch:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,[rbx+8]
 mov qword ptr [rsp],0
 mov rdi,[r12]
 mov esi,2
 mov rdx,[rbx+16]
 mov rcx,rsp
 call [r12+8]
 test eax,eax
 jz .Ltr_fetch_ok
 cmp eax,1
 je .Ltr_fetch_missing
 .Ltr_fetch_provider_failed:
 mov rax,[rbx]
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 jne .Ltr_fetch_failed
 mov dword ptr [rax+52],3
.Ltr_fetch_failed:
 mov r12d,6
 jmp .Ltr_fetch_cleanup
.Ltr_fetch_missing:
 mov r12d,3
 jmp .Ltr_fetch_cleanup
.Ltr_fetch_ok:
 cmp qword ptr [rsp],0
 je .Ltr_fetch_provider_failed
 mov rdi,[rbx+24]
 call json_object_put@PLT
 mov rax,[rsp]
 mov [rbx+24],rax
 mov qword ptr [rsp],0
 xor r12d,r12d
.Ltr_fetch_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,r12d
 add rsp,8
 pop r12
 pop rbx
 ret
/* New {status,checkpoint/outcome}, retaining the supplied value. */
.Ltr_make_state:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,8
 mov r12,rdi
 mov r13,rsi
 mov r14,rdx
 call json_object_new_object@PLT
 test rax,rax
 jz .Ltr_make_return
 mov rbx,rax
 mov rdi,r12
 call json_object_new_string@PLT
 test rax,rax
 jz .Ltr_make_bad
 mov rdx,rax
 mov rdi,rbx
 lea rsi,[rip+.Ltr_status]
 call .Ltr_set
 test eax,eax
 jnz .Ltr_make_bad
 mov rdi,r14
 call json_object_get@PLT
 mov rdx,rax
 mov rdi,rbx
 mov rsi,r13
 call .Ltr_set
 test eax,eax
 jnz .Ltr_make_bad
 mov rax,rbx
 jmp .Ltr_make_return
.Ltr_make_bad:
 mov rdi,rbx
 call json_object_put@PLT
 xor eax,eax
.Ltr_make_return:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret

/* Park running work; crash recovery alone increments its restart count. */
.Ltr_park:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12d,esi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 TGET "qword ptr [rbx+24]",state_key
 TGET rax,checkpoint
 mov rdx,rax
 lea rdi,[rip+.Ltr_pending_name]
 lea rsi,[rip+.Ltr_checkpoint]
 call .Ltr_make_state
 mov [rsp+8],rax
 mov ebp,5
 test rax,rax
 jz .Ltr_park_cleanup
 mov rdi,[rbx+24]
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ltr_park_cleanup
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 mov rdi,[rsp]
 lea rsi,[rip+.Ltr_state_key]
 call .Ltr_set
 test eax,eax
 jnz .Ltr_park_cleanup
 test r12d,r12d
 jz .Ltr_park_store
 TGET "qword ptr [rbx+24]",restarts
 mov rdi,rax
 call json_object_get_int64@PLT
 lea rdi,[rax+1]
 call json_object_new_int64@PLT
 test rax,rax
 jz .Ltr_park_cleanup
 mov rdx,rax
 mov rdi,[rsp]
 lea rsi,[rip+.Ltr_restarts]
 call .Ltr_set
 test eax,eax
 jnz .Ltr_park_cleanup
.Ltr_park_store:
 mov rdi,rbx
 mov rsi,[rsp]
 call .Ltr_store
 mov ebp,eax
.Ltr_park_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.globl pg_task_runner_commit
.type pg_task_runner_commit,@function
pg_task_runner_commit:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov ebp,4
 test rbx,rbx
 jz .Ltr_commit_return
 cmp dword ptr [rbx+76],1
 jne .Ltr_commit_return
 cmp dword ptr [rbx+84],0
 jne .Ltr_commit_closed
 cmp dword ptr [rbx+92],0
 jne .Ltr_commit_return
 cmp qword ptr [rbx+32],0
 je .Ltr_commit_return
 mov rdi,r12
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Ltr_commit_return
 mov rdi,r12
 call .Ltr_state
 mov r13d,eax
 cmp eax,1
 je .Ltr_commit_checkpoint
 cmp eax,2
 je .Ltr_commit_checkpoint
 cmp eax,3
 je .Ltr_commit_checkpoint
 cmp eax,5
 jne .Ltr_commit_return
 TGET r12,outcome
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ltr_commit_return
 jmp .Ltr_commit_admit
.Ltr_commit_checkpoint:
 TGET r12,checkpoint
 mov r14,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ltr_commit_return
 TGET r14,phase
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ltr_commit_return
 cmp r13d,3
 jne .Ltr_commit_admit
 TGET r12,until
 mov r14,rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,2
 je .Ltr_commit_sleep_number
 cmp eax,3
 jne .Ltr_commit_return
.Ltr_commit_sleep_number:
 mov rdi,r14
 call json_object_get_double@PLT
 movq rax,xmm0
 shr rax,52
 and eax,0x7ff
 cmp eax,0x7ff
 je .Ltr_commit_return
 xorpd xmm1,xmm1
 ucomisd xmm0,xmm1
 jb .Ltr_commit_return
.Ltr_commit_admit:
 mov rax,[rbx]
 cmp dword ptr [rax+172],1
 jne .Ltr_commit_closed
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 jne .Ltr_commit_storage
 mov dword ptr [rbx+92],1
 mov rdi,rbx
 call .Ltr_fetch
 mov ebp,eax
 test eax,eax
 jnz .Ltr_commit_cleanup
 mov rdi,[rbx+24]
 call .Ltr_record_state
 mov ebp,4
 cmp eax,1
 jne .Ltr_commit_cleanup
 TGET "qword ptr [rbx+24]",abort_requested
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Ltr_commit_owner_check
 cmp qword ptr [rbx+48],0
 jne .Ltr_commit_active_abort
 cmp r13d,5
 jne .Ltr_commit_cleanup
 jmp .Ltr_commit_owner_check
.Ltr_commit_active_abort:
 mov ebp,7
 jmp .Ltr_commit_cleanup
.Ltr_commit_owner_check:
 cmp r13d,2
 jne .Ltr_commit_terminal_check
 mov rdi,rbx
 mov rsi,r12
 call .Ltw_validate
 mov ebp,eax
 cmp eax,6
 je .Ltr_commit_storage_active
 test eax,eax
 jnz .Ltr_commit_cleanup
.Ltr_commit_terminal_check:
 cmp r13d,5
 jne .Ltr_commit_replace
 cmp qword ptr [rbx+112],0
 je .Ltr_commit_owned_stored
 mov rdi,rbx
 call .Ltw_has_owned
 cmp eax,-4
 jne .Ltr_commit_owned_allocation
 mov ebp,4
 jmp .Ltr_commit_cleanup
.Ltr_commit_owned_allocation:
 cmp eax,-5
 jne .Ltr_commit_owned_status
 mov ebp,5
 jmp .Ltr_commit_cleanup
.Ltr_commit_owned_stored:
 mov rdi,[rbx+8]
 mov rsi,[rbx+16]
 call .Ltr_owned
.Ltr_commit_owned_status:
 test eax,eax
 js .Ltr_commit_storage_active
 jz .Ltr_commit_replace
 mov rdi,r12
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 mov ebp,5
 test eax,eax
 jnz .Ltr_commit_cleanup
 lea rdi,[rip+.Ltr_completing_name]
 call json_object_new_string@PLT
 test rax,rax
 jz .Ltr_commit_cleanup
 mov rdi,[rsp]
 lea rsi,[rip+.Ltr_status]
 mov rdx,rax
 call .Ltr_set
 test eax,eax
 jnz .Ltr_commit_cleanup
 mov r12,[rsp]
.Ltr_commit_replace:
 mov rdi,rbx
 mov rsi,r12
 mov edx,1
 call .Ltr_replace
 mov ebp,eax
 jmp .Ltr_commit_cleanup
.Ltr_commit_storage_active:
 mov rax,[rbx]
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 jne .Ltr_commit_storage
 mov dword ptr [rax+52],3
.Ltr_commit_storage:
 mov ebp,6
 jmp .Ltr_commit_cleanup
.Ltr_commit_closed:
 mov ebp,7
.Ltr_commit_cleanup:
 mov dword ptr [rbx+92],0
 mov rdi,[rsp]
 call json_object_put@PLT
.Ltr_commit_return:
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_task_runner_commit,.-pg_task_runner_commit

/* Release the phase lease outside the Storage adoption call stack. */
.Ltr_end_phase:
 push rbx
 mov rbx,rdi
 cmp qword ptr [rbx+32],0
 je .Ltr_end_phase_done
 mov rax,[rbx+48]
 test rax,rax
 jz .Ltr_end_abort
 mov rdx,[rax+32]
 mov rdi,[rax+16]
 jmp .Ltr_end_callback
.Ltr_end_abort:
 mov rax,[rbx+40]
 mov rdx,[rax+64]
 mov rdi,[rax+24]
.Ltr_end_callback:
 test rdx,rdx
 jz .Ltr_end_reference
 mov rsi,rbx
 call rdx
.Ltr_end_reference:
 mov rdi,[rbx+32]
 call json_object_put@PLT
 mov qword ptr [rbx+32],0
 mov qword ptr [rbx+40],0
 mov qword ptr [rbx+48],0
 mov qword ptr [rbx+56],0
 mov dword ptr [rbx+80],0
.Ltr_end_phase_done:
 pop rbx
 ret

/* Faults become durable outcomes; an already committed non-running state wins. */
.Ltr_fault:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12d,esi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov ebp,5
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Ltr_fault_cleanup
 mov rdi,[rbx+96]
 mov rsi,[rbx+104]
 cmp rsi,2147483647
 ja .Ltr_fault_cleanup
 test rsi,rsi
 jnz .Ltr_fault_message
 lea rdi,[rip+.Ltr_phase_failed]
 lea rsi,[rip+.Ltr_phase_failed_end]
 sub rsi,rdi
 dec rsi
.Ltr_fault_message:
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Ltr_fault_cleanup
 mov rdx,rax
 mov rdi,[rsp]
 lea rsi,[rip+.Ltr_message]
 call .Ltr_set
 test eax,eax
 jnz .Ltr_fault_cleanup
 call json_object_new_object@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Ltr_fault_cleanup
 lea rdi,[rip+.Ltr_faulted]
 call json_object_new_string@PLT
 test rax,rax
 jz .Ltr_fault_cleanup
 mov rdx,rax
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ltr_status]
 call .Ltr_set
 test eax,eax
 jnz .Ltr_fault_cleanup
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ltr_error]
 call .Ltr_set
 test eax,eax
 jnz .Ltr_fault_cleanup
 lea rdi,[rip+.Ltr_terminal_name]
 lea rsi,[rip+.Ltr_outcome]
 mov rdx,[rsp+8]
 call .Ltr_make_state
 mov [rsp+16],rax
 test rax,rax
 jz .Ltr_fault_cleanup
 mov rdi,rbx
 mov rsi,rax
 call pg_task_runner_commit@PLT
 mov ebp,eax
 test eax,eax
 jnz .Ltr_fault_cleanup
 mov rax,[rbx+8]
 cmp qword ptr [rax+48],0
 je .Ltr_fault_cleanup
 mov rdi,[rax]
 mov rsi,[rbx+16]
 mov edx,r12d
 call [rax+48]
.Ltr_fault_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Resolve one borrowed definition, locally materializing migrations before reservation. */
.Ltr_resolve:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov r12,[rbx+8]
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+48],0
 mov qword ptr [rsp+56],0
 TGET "qword ptr [rbx+24]",kind
 mov r14,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 mov ebp,3
 test eax,eax
 jz .Ltr_resolve_cleanup
 mov rdi,r14
 call json_object_get_string_len@PLT
 mov r13,rax
 mov rdi,r14
 call json_object_get_string@PLT
 mov rsi,rax
 mov rdx,r13
 mov rcx,rsp
 mov rax,[rbx+128]
 test rax,rax
 jz .Ltr_resolve_borrowed
 mov rdi,[rax]
 lea r8,[rsp+48]
 call [rax+8]
 jmp .Ltr_resolve_acquired
.Ltr_resolve_borrowed:
 mov rdi,[r12]
 call [r12+32]
.Ltr_resolve_acquired:
 test eax,eax
 jnz .Ltr_resolve_cleanup
 mov r12,[rsp]
 test r12,r12
 jz .Ltr_resolve_cleanup
 cmp qword ptr [r12+16],0
 je .Ltr_resolve_cleanup
 cmp [r12+8],r13
 jne .Ltr_resolve_cleanup
 mov rdi,r14
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,[r12]
 mov rdx,r13
 call memcmp@PLT
 test eax,eax
 jnz .Ltr_resolve_cleanup
 TGET "qword ptr [rbx+24]",version
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ltr_resolve_cleanup
 test rax,rax
 jz .Ltr_resolve_cleanup
 cmp rax,[r12+16]
 ja .Ltr_resolve_cleanup
 je .Ltr_resolve_ready
 cmp [rbx+120],r12
 jne .Ltr_resolve_migrate
 mov qword ptr [rsp+56],1
 jmp .Ltr_resolve_cleanup
.Ltr_resolve_migrate:
 cmp qword ptr [r12+72],0
 je .Ltr_resolve_cleanup
 mov [rsp+32],rax
 TGET "qword ptr [rbx+24]",input
 mov [rsp+24],rax
 TGET "qword ptr [rbx+24]",state_key
 TGET rax,checkpoint
 mov rdx,rax
 mov rdi,[r12+24]
 mov rsi,[rsp+24]
 mov rcx,[rsp+32]
 lea r8,[rsp+8]
 call [r12+72]
 test eax,eax
 jnz .Ltr_migration_failed
 mov rdi,[rsp+8]
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Ltr_migration_failed
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ltr_input]
 lea rdx,[rsp+24]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Ltr_migration_failed
 TGET "qword ptr [rsp+8]",checkpoint
 mov r14,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ltr_migration_failed
 mov rdi,[rbx+24]
 lea rsi,[rsp+16]
 xor edx,edx
 call json_object_deep_copy@PLT
 mov ebp,5
 test eax,eax
 jnz .Ltr_resolve_cleanup
 mov rdi,[rsp+24]
 call json_object_get@PLT
 mov rdx,rax
 mov rdi,[rsp+16]
 lea rsi,[rip+.Ltr_input]
 call .Ltr_set
 test eax,eax
 jnz .Ltr_resolve_cleanup
 TGET "qword ptr [rsp+16]",state_key
 mov r13,rax
 mov rdi,r14
 call json_object_get@PLT
 mov rdi,r13
 lea rsi,[rip+.Ltr_checkpoint]
 mov rdx,rax
 call .Ltr_set
 test eax,eax
 jnz .Ltr_resolve_cleanup
 mov rdi,[r12+16]
 call json_object_new_int64@PLT
 test rax,rax
 jz .Ltr_resolve_cleanup
 mov rdx,rax
 mov rdi,[rsp+16]
 lea rsi,[rip+.Ltr_version]
 call .Ltr_set
 test eax,eax
 jnz .Ltr_resolve_cleanup
 mov rdi,[rbx+24]
 call json_object_put@PLT
 mov rax,[rsp+16]
 mov [rbx+24],rax
 mov qword ptr [rsp+16],0
 or dword ptr [rbx+88],2
 jmp .Ltr_resolve_ready
.Ltr_migration_failed:
 mov rdi,rbx
 mov esi,144
 call .Ltr_drop_lease
 mov rax,[rsp+48]
 mov [rbx+144],rax
 mov qword ptr [rsp+48],0
 mov rdi,rbx
 mov esi,136
 call .Ltr_drop_lease
 mov qword ptr [rbx+152],0
 mov qword ptr [rbx+40],0
 mov qword ptr [rsp+56],1
 mov [rbx+120],r12
 mov ebp,3
 jmp .Ltr_resolve_cleanup
.Ltr_resolve_ready:
 mov rdi,rbx
 mov esi,136
 call .Ltr_drop_lease
 mov rdi,rbx
 mov esi,144
 call .Ltr_drop_lease
 mov rax,[rsp+48]
 mov [rbx+136],rax
 mov qword ptr [rsp+48],0
 mov [rbx+152],r12
 mov qword ptr [rbx+120],0
 mov [rbx+40],r12
 xor ebp,ebp
.Ltr_resolve_cleanup:
 cmp ebp,3
 jne .Ltr_resolve_release_candidate
 cmp qword ptr [rsp+56],0
 jne .Ltr_resolve_release_candidate
 mov rax,[rbx+152]
 test rax,rax
 jz .Ltr_resolve_release_candidate
 mov [rbx+40],rax
 xor ebp,ebp
.Ltr_resolve_release_candidate:
 mov rdi,[rsp+48]
 test rdi,rdi
 jz .Ltr_resolve_values
 mov rax,[rbx+128]
 call [rax+16]
.Ltr_resolve_values:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.Ltr_find_phase:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,8
 mov rbx,rdi
 TGET "qword ptr [rbx+24]",state_key
 TGET rax,checkpoint
 TGET rax,phase
 mov r12,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ltr_phase_absent
 mov rdi,r12
 call json_object_get_string_len@PLT
 mov r13,rax
 mov r14,[rbx+40]
 xor ebx,ebx
.Ltr_phase_each:
 cmp rbx,[r14+48]
 jae .Ltr_phase_absent
 imul rax,rbx,40
 add rax,[r14+40]
 mov [rsp],rax
 cmp [rax+8],r13
 jne .Ltr_phase_next
 mov rdi,r12
 call json_object_get_string@PLT
 mov rdi,rax
 mov rax,[rsp]
 mov rsi,[rax]
 mov rdx,r13
 call memcmp@PLT
 test eax,eax
 jz .Ltr_phase_found
.Ltr_phase_next:
 inc rbx
 jmp .Ltr_phase_each
.Ltr_phase_found:
 mov rax,[rsp]
 jmp .Ltr_find_phase_return
.Ltr_phase_absent:
 lea rax,[rip+.Ltr_missing_phase]
.Ltr_find_phase_return:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret

.globl pg_task_runner_poll
.type pg_task_runner_poll,@function
pg_task_runner_poll:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+16],0
 mov ebp,4
 test rbx,rbx
 jz .Ltr_poll_return
 cmp dword ptr [rbx+72],0
 jne .Ltr_poll_return
 cmp dword ptr [rbx+92],0
 jne .Ltr_poll_return
 cmp dword ptr [rbx+76],1
 jne .Ltr_poll_closed
 cmp dword ptr [rbx+84],0
 jne .Ltr_poll_closed
 mov rax,[rbx]
 cmp dword ptr [rax+172],1
 jne .Ltr_poll_closed
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 jne .Ltr_poll_storage
 mov rdi,[rbx]
 call pg_durable_session_idle@PLT
 mov ebp,1
 test eax,eax
 jz .Ltr_poll_return
 mov dword ptr [rbx+72],1
 mov rdi,rbx
 call .Ltr_fetch
 mov ebp,eax
 test eax,eax
 jnz .Ltr_poll_unlock
 cmp qword ptr [rbx+32],0
 je .Ltr_poll_no_invocation
 TGET "qword ptr [rbx+24]",abort_requested
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Ltr_poll_invoke
 cmp qword ptr [rbx+48],0
 je .Ltr_poll_invoke
 mov rdi,rbx
 call .Ltr_end_phase
.Ltr_poll_no_invocation:
 mov rdi,[rbx+24]
 call .Ltr_record_state
 mov r12d,eax
 cmp eax,5
 je .Ltr_poll_done
 cmp eax,4
 je .Ltr_poll_completing
 test eax,eax
 js .Ltr_poll_invalid
 cmp dword ptr [rbx+88],0
 jne .Ltr_poll_recovered
 cmp eax,1
 jne .Ltr_poll_set_recovered
 mov rdi,rbx
 mov esi,1
 call .Ltr_park
 mov ebp,eax
 test eax,eax
 jnz .Ltr_poll_unlock
 xor r12d,r12d
.Ltr_poll_set_recovered:
 mov dword ptr [rbx+88],1
.Ltr_poll_recovered:
 TGET "qword ptr [rbx+24]",abort_requested
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov r13d,eax
 test eax,eax
 jnz .Ltr_poll_abort_ready
 TGET "qword ptr [rbx+24]",restarts
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rbx+64]
 jb .Ltr_restart_allowed
 mov r13d,2
 jmp .Ltr_poll_resolve
.Ltr_restart_allowed:
 cmp r12d,2
 jne .Ltr_poll_sleep_check
 mov rdi,rbx
 call .Ltw_waiting
 test eax,eax
 js .Ltr_poll_storage_active
 jnz .Ltr_poll_pending
 jmp .Ltr_poll_resolve
.Ltr_poll_sleep_check:
 cmp r12d,3
 jne .Ltr_poll_resolve
 TGET "qword ptr [rbx+24]",state_key
 TGET rax,until
 mov rdi,rax
 call json_object_get_double@PLT
 movsd [rsp+8],xmm0
 mov rax,[rbx+8]
 mov rdi,[rax]
 call [rax+40]
 ucomisd xmm0,qword ptr [rsp+8]
 jb .Ltr_poll_pending
 jp .Ltr_poll_invalid
 jmp .Ltr_poll_resolve
.Ltr_poll_abort_ready:
 mov rdi,[rbx+8]
 mov rsi,[rbx+16]
 call .Ltr_owned
 test eax,eax
 js .Ltr_poll_storage_active
 jnz .Ltr_poll_pending
.Ltr_poll_resolve:
 mov rdi,rbx
 call .Ltr_resolve
 mov ebp,eax
 test eax,eax
 jnz .Ltr_poll_unlock
 cmp r13d,2
 jne .Ltr_cutoff_available
 mov rax,[rbx+40]
 cmp qword ptr [rax+80],0
 je .Ltr_poll_blocked
.Ltr_cutoff_available:
 cmp r12d,1
 jne .Ltr_poll_reserve_state
 test dword ptr [rbx+88],2
 jz .Ltr_poll_reserved
.Ltr_poll_reserve_state:
 TGET "qword ptr [rbx+24]",state_key
 TGET rax,checkpoint
 mov rdx,rax
 lea rdi,[rip+.Ltr_running_name]
 lea rsi,[rip+.Ltr_checkpoint]
 call .Ltr_make_state
 mov [rsp],rax
 test rax,rax
 jz .Ltr_poll_allocation
 mov rdi,rbx
 mov rsi,rax
 xor edx,edx
 call .Ltr_replace
 mov ebp,eax
 test eax,eax
 jnz .Ltr_poll_unlock
 and dword ptr [rbx+88],1
.Ltr_poll_reserved:
 test r13d,r13d
 jnz .Ltr_poll_abort_phase
 mov rdi,rbx
 call .Ltr_find_phase
 mov [rbx+48],rax
 jmp .Ltr_poll_phase_record
.Ltr_poll_abort_phase:
 mov qword ptr [rbx+48],0
.Ltr_poll_phase_record:
 mov rdi,[rbx+24]
 call json_object_get@PLT
 mov [rbx+32],rax
 mov dword ptr [rbx+80],1
.Ltr_poll_invoke:
 mov qword ptr [rbx+96],0
 mov qword ptr [rbx+104],0
 mov rax,[rbx+48]
 test rax,rax
 jz .Ltr_poll_invoke_abort
 mov r12,[rax+24]
 mov rdi,[rax+16]
 jmp .Ltr_poll_handler
.Ltr_poll_invoke_abort:
 mov rax,[rbx+40]
 mov r12,[rax+56]
 cmp r13d,2
 jne .Ltr_handler_selected
 mov r12,[rax+80]
.Ltr_handler_selected:
 mov rdi,[rax+24]
.Ltr_poll_handler:
 test r12,r12
 jz .Ltr_poll_missing
 mov rsi,rbx
 mov rdx,[rbx+32]
 call r12
 mov r12d,eax
 cmp dword ptr [rbx+84],0
 jne .Ltr_poll_requested_close
 cmp eax,1
 je .Ltr_poll_pending
 mov rdi,rbx
 call .Ltr_fetch
 mov ebp,eax
 test eax,eax
 jnz .Ltr_poll_end_error
 mov rdi,[rbx+24]
 call .Ltr_record_state
 cmp eax,1
 jne .Ltr_poll_phase_ended
 test r12d,r12d
 js .Ltr_poll_fault
 /* Abort handlers must decide an outcome; a running checkpoint is not enough. */
 cmp qword ptr [rbx+48],0
 je .Ltr_poll_no_progress
 TGET "qword ptr [rbx+32]",state_key
 TGET rax,checkpoint
 mov r13,rax
 TGET "qword ptr [rbx+24]",state_key
 TGET rax,checkpoint
 mov rdi,r13
 mov rsi,rax
 call .Ltr_json_equal
 test eax,eax
 jnz .Ltr_poll_no_progress
 mov rdi,rbx
 call .Ltr_end_phase
 xor ebp,ebp
 jmp .Ltr_poll_unlock
.Ltr_poll_no_progress:
 cmp qword ptr [rbx+48],0
 je .Ltr_poll_abort_no_progress
 TGET "qword ptr [rbx+32]",kind
 mov rdi,rax
 call json_object_get_string@PLT
 mov r13,rax
 TGET "qword ptr [rbx+32]",state_key
 TGET rax,checkpoint
 TGET rax,phase
 mov rdi,rax
 call json_object_get_string@PLT
 mov rcx,rax
 mov rdx,r13
 lea rdi,[rsp+16]
 lea rsi,[rip+.Ltr_no_progress_format]
 xor eax,eax
 call asprintf@PLT
 jmp .Ltr_poll_no_progress_ready
.Ltr_poll_abort_no_progress:
 lea rdi,[rsp+16]
 lea rsi,[rip+.Ltr_abort_no_progress_format]
 mov rdx,[rbx+16]
 xor eax,eax
 call asprintf@PLT
.Ltr_poll_no_progress_ready:
 test eax,eax
 js .Ltr_poll_allocation
 mov rcx,[rsp+16]
 mov [rbx+96],rcx
 mov [rbx+104],rax
 mov r12d,-1
 jmp .Ltr_poll_fault
.Ltr_poll_missing:
 mov r12d,-2
.Ltr_poll_fault:
 mov rdi,rbx
 mov esi,r12d
 call .Ltr_fault
 mov ebp,eax
 test eax,eax
 jnz .Ltr_poll_end_error
 mov rdi,[rbx+24]
 call .Ltr_record_state
.Ltr_poll_phase_ended:
 mov r12d,eax
 mov rdi,rbx
 call .Ltr_end_phase
 cmp r12d,5
 je .Ltr_poll_done
 cmp r12d,1
 je .Ltr_poll_progress
 jmp .Ltr_poll_pending
.Ltr_poll_end_error:
 mov rdi,rbx
 call .Ltr_end_phase
 jmp .Ltr_poll_unlock
.Ltr_poll_completing:
 mov rdi,[rbx+8]
 mov rsi,[rbx+16]
 call .Ltr_owned
 test eax,eax
 js .Ltr_poll_storage_active
 jnz .Ltr_poll_pending
 TGET "qword ptr [rbx+24]",state_key
 TGET rax,outcome
 mov rdx,rax
 lea rdi,[rip+.Ltr_terminal_name]
 lea rsi,[rip+.Ltr_outcome]
 call .Ltr_make_state
 mov [rsp],rax
 test rax,rax
 jz .Ltr_poll_allocation
 mov rdi,rbx
 mov rsi,rax
 mov edx,1
 call .Ltr_replace
 mov ebp,eax
 test eax,eax
 jnz .Ltr_poll_unlock
.Ltr_poll_done:
 mov ebp,2
 jmp .Ltr_poll_unlock
.Ltr_poll_progress:
 xor ebp,ebp
 jmp .Ltr_poll_unlock
.Ltr_poll_pending:
 mov ebp,1
 jmp .Ltr_poll_unlock
.Ltr_poll_blocked:
 mov ebp,3
 jmp .Ltr_poll_unlock
.Ltr_poll_invalid:
 mov ebp,4
 jmp .Ltr_poll_unlock
.Ltr_poll_allocation:
 mov ebp,5
 jmp .Ltr_poll_unlock
.Ltr_poll_storage_active:
 mov rax,[rbx]
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 jne .Ltr_poll_storage
 mov dword ptr [rax+52],3
.Ltr_poll_storage:
 mov ebp,6
 jmp .Ltr_poll_unlock
.Ltr_poll_requested_close:
 mov rdi,rbx
 call .Ltr_close_locked
 mov ebp,eax
 jmp .Ltr_poll_unlock
.Ltr_poll_closed:
 mov ebp,7
 jmp .Ltr_poll_return
.Ltr_poll_unlock:
 mov dword ptr [rbx+72],0
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call free@PLT
 mov qword ptr [rbx+96],0
 mov qword ptr [rbx+104],0
.Ltr_poll_return:
 mov eax,ebp
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_task_runner_poll,.-pg_task_runner_poll

.Ltr_close_locked:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov dword ptr [rbx+84],1
 call .Ltr_end_phase
 xor r12d,r12d
 cmp qword ptr [rbx+24],0
 je .Ltr_close_release
 mov rax,[rbx]
 cmp dword ptr [rax+172],1
 jne .Ltr_close_release
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 jne .Ltr_close_release
 mov rdi,rbx
 call .Ltr_fetch
 mov r12d,eax
 test eax,eax
 jnz .Ltr_close_release
 mov rdi,[rbx+24]
 call .Ltr_record_state
 cmp eax,1
 jne .Ltr_close_release
 mov rdi,rbx
 xor esi,esi
 call .Ltr_park
 mov r12d,eax
.Ltr_close_release:
 mov rdi,rbx
 mov esi,136
 call .Ltr_drop_lease
 mov rdi,rbx
 mov esi,144
 call .Ltr_drop_lease
 mov qword ptr [rbx+152],0
 mov qword ptr [rbx+120],0
 mov rdi,[rbx+24]
 call json_object_put@PLT
 mov qword ptr [rbx+24],0
 mov dword ptr [rbx+76],0
 mov qword ptr [rbx+96],0
 mov qword ptr [rbx+104],0
 mov eax,r12d
 add rsp,8
 pop r12
 pop rbx
 ret
.globl pg_task_runner_close
.type pg_task_runner_close,@function
pg_task_runner_close:
 mov eax,4
 test rdi,rdi
 jz .Ltr_close_return
 cmp dword ptr [rdi+92],0
 jne .Ltr_close_busy
 cmp dword ptr [rdi+72],0
 jne .Ltr_close_busy
 xor eax,eax
 cmp dword ptr [rdi+76],0
 je .Ltr_close_return
 push rbx
 mov rbx,rdi
 mov dword ptr [rbx+72],1
 call .Ltr_close_locked
 mov dword ptr [rbx+72],0
 pop rbx
 ret
.Ltr_close_busy:
 mov dword ptr [rdi+84],1
 mov eax,1
.Ltr_close_return:
 ret
.size pg_task_runner_close,.-pg_task_runner_close

.section .rodata
.Ltr_owner: .asciz "owner"
.Ltr_id: .asciz "id"
.Ltr_status: .asciz "status"
.Ltr_state_key: .asciz "state"
.Ltr_checkpoint: .asciz "checkpoint"
.Ltr_phase: .asciz "phase"
.Ltr_kind: .asciz "kind"
.Ltr_version: .asciz "version"
.Ltr_input: .asciz "input"
.Ltr_outcome: .asciz "outcome"
.Ltr_message: .asciz "message"
.Ltr_error: .asciz "error"
.Ltr_memos: .asciz "memos"
.Ltr_restarts: .asciz "restarts"
.Ltr_abort_requested: .asciz "abortRequested"
.Ltr_until: .asciz "until"
.Ltr_pending_name: .asciz "pending"
.Ltr_running_name: .asciz "running"
.Ltr_waiting_name: .asciz "waiting"
.Ltr_sleeping_name: .asciz "sleeping"
.Ltr_completing_name: .asciz "completing"
.Ltr_terminal_name: .asciz "terminal"
.Ltr_faulted: .asciz "faulted"
.Ltr_phase_failed: .asciz "Task phase failed"
.Ltr_phase_failed_end:
.Ltr_no_progress_format: .asciz "Task %s phase %s returned without durable progress"
.Ltr_abort_no_progress_format: .asciz "Abort handler of task %llu returned without a terminal outcome"
.Ltr_status_names:
 .long .Ltr_pending_name-.Ltr_status_names,.Ltr_running_name-.Ltr_status_names,.Ltr_waiting_name-.Ltr_status_names
 .long .Ltr_sleeping_name-.Ltr_status_names,.Ltr_completing_name-.Ltr_status_names,.Ltr_terminal_name-.Ltr_status_names
.balign 8
.Ltr_missing_phase: .zero 40
.text
/* Live-status scans admit stable runner allocations before any phase runs. */
.Lts_discover:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,144
 mov rbx,rdi
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 movdqu [rsp+64],xmm0
 movdqu [rsp+80],xmm0
 movdqu [rsp+96],xmm0
 movdqu [rsp+112],xmm0
 mov dword ptr [rsp],2
 mov dword ptr [rsp+4],1
 mov qword ptr [rsp+24],256
 mov qword ptr [rsp+32],64
 mov dword ptr [rsp+128],0
 mov rdi,[rbx]
 mov ebp,7
 cmp dword ptr [rdi+172],1
 jne .Lts_discover_end
 mov rax,[rdi]
 mov ebp,6
 cmp dword ptr [rax+52],0
 jne .Lts_discover_end
 call pg_durable_session_idle@PLT
 mov ebp,1
 test eax,eax
 jz .Lts_discover_end
.Lts_scan_page:
 mov rax,[rbx+8]
 mov rdi,[rax]
 mov rsi,rsp
 lea rdx,[rsp+112]
 call [rax+16]
 mov ebp,6
 test eax,eax
 jnz .Lts_discover_end
 xor r12d,r12d
.Lts_scan_row:
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lts_scan_next
 mov rdi,[rsp+112]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 TGET rax,id
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Lts_discover_end
 mov r13,rax
 xor r14d,r14d
 mov r8,[rbx+24]
 mov r10,[rbx+16]
.Lts_find_runner:
 cmp r14,r8
 jae .Lts_new_runner
 lea r9,[r14+r8]
 shr r9,1
 mov rax,[r10+r9*8]
 cmp [rax+16],r13
 je .Lts_scan_skip
 jb .Lts_find_after
 mov r8,r9
 jmp .Lts_find_runner
.Lts_find_after:
 lea r14,[r9+1]
 jmp .Lts_find_runner
.Lts_new_runner:
 mov [rsp+136],r14
 mov rax,[rbx+24]
 cmp rax,[rbx+32]
 jb .Lts_allocate_runner
 mov r14,[rbx+32]
 test r14,r14
 jnz .Lts_grow_runner
 mov r14,8
 jmp .Lts_reallocate
.Lts_grow_runner:
 shl r14,1
 jc .Lts_discover_allocation
.Lts_reallocate:
 mov rdi,[rbx+16]
 mov rsi,r14
 mov edx,8
 call reallocarray@PLT
 test rax,rax
 jz .Lts_discover_allocation
 mov [rbx+16],rax
 mov [rbx+32],r14
.Lts_allocate_runner:
 mov edi,1
 mov esi,160
 call calloc@PLT
 test rax,rax
 jz .Lts_discover_allocation
 mov r14,rax
 mov rdi,rax
 mov rsi,[rbx]
 mov rdx,r13
 mov rcx,[rbx+8]
 mov r8,[rbx+40]
 call pg_task_runner_open@PLT
 mov ebp,eax
 test eax,eax
 jnz .Lts_free_failed
 mov rax,[rbx+72]
 mov [r14+128],rax
 mov rax,[rbx+16]
 mov rcx,[rbx+24]
 mov rdx,[rsp+136]
.Lts_insert_runner:
 cmp rcx,rdx
 jbe .Lts_insert_ready
 mov rsi,[rax+rcx*8-8]
 mov [rax+rcx*8],rsi
 dec rcx
 jmp .Lts_insert_runner
.Lts_insert_ready:
 mov [rax+rdx*8],r14
 inc qword ptr [rbx+24]
.Lts_scan_skip:
 inc r12
 jmp .Lts_scan_row
.Lts_scan_next:
 cmp dword ptr [rsp+132],0
 je .Lts_next_status
 mov rax,[rsp+120]
 cmp dword ptr [rsp+8],0
 je .Lts_scan_cursor
 cmp rax,[rsp+16]
 jbe .Lts_discover_end
.Lts_scan_cursor:
 mov [rsp+16],rax
 mov dword ptr [rsp+8],1
 mov dword ptr [rsp+12],1
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 jmp .Lts_scan_page
.Lts_next_status:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 mov dword ptr [rsp+8],0
 mov dword ptr [rsp+12],0
 mov qword ptr [rsp+16],0
 inc dword ptr [rsp+96]
 cmp dword ptr [rsp+96],5
 jb .Lts_scan_page
 /* Recover every newly discovered running record before invoking any handler. */
 xor r12d,r12d
.Lts_recover:
 cmp r12,[rbx+24]
 jae .Lts_discover_ok
 mov rax,[rbx+16]
 mov r14,[rax+r12*8]
 mov rdi,r14
 call .Ltr_fetch
 mov ebp,eax
 test eax,eax
 jnz .Lts_discover_end
 cmp dword ptr [r14+88],0
 jne .Lts_recover_next
 mov rdi,[r14+24]
 call .Ltr_record_state
 cmp eax,1
 jne .Lts_recovered
 mov rdi,r14
 mov esi,1
 call .Ltr_park
 mov ebp,eax
 test eax,eax
 jnz .Lts_discover_end
.Lts_recovered:
 mov dword ptr [r14+88],1
.Lts_recover_next:
 inc r12
 jmp .Lts_recover
.Lts_free_failed:
 mov rdi,r14
 call free@PLT
 jmp .Lts_discover_end
.Lts_discover_allocation:
 mov ebp,5
 jmp .Lts_discover_end
.Lts_discover_ok:
 xor ebp,ebp
.Lts_discover_end:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,144
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.globl pg_task_scheduler_open
.type pg_task_scheduler_open,@function
pg_task_scheduler_open:
 push rbx
 mov rbx,rdi
 mov eax,4
 test rdi,rdi
 jz .Lts_open_return
 cmp dword ptr [rdi+60],0
 jne .Lts_open_return
 test rsi,rsi
 jz .Lts_open_return
 test rdx,rdx
 jz .Lts_open_return
 test rcx,rcx
 jz .Lts_open_return
 cmp dword ptr [rsi+172],1
 jne .Lts_open_return
 mov r8,[rsi]
 cmp dword ptr [r8+52],0
 jne .Lts_open_return
 cmp dword ptr [r8+56],0
 jne .Lts_open_return
 mov r8d,8
.Lts_open_host:
 cmp qword ptr [rdx+r8],0
 je .Lts_open_return
 add r8,8
 cmp r8,48
 jb .Lts_open_host
 mov [rdi],rsi
 mov [rdi+8],rdx
 mov [rdi+40],rcx
 mov qword ptr [rdi+16],0
 mov qword ptr [rdi+24],0
 mov qword ptr [rdi+32],0
 movabs rax,0x7ff0000000000000
 mov [rdi+48],rax
 mov qword ptr [rdi+64],0
 mov dword ptr [rdi+56],1
 mov dword ptr [rdi+60],1
 call .Lts_discover
 mov dword ptr [rbx+56],0
.Lts_open_return:
 pop rbx
 ret
.size pg_task_scheduler_open,.-pg_task_scheduler_open

.globl pg_task_scheduler_poll
.type pg_task_scheduler_poll,@function
pg_task_scheduler_poll:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rdi
 mov ebp,4
 test rdi,rdi
 jz .Lts_poll_return
 cmp dword ptr [rbx+56],0
 jne .Lts_poll_return
 mov ebp,7
 cmp dword ptr [rbx+60],1
 jne .Lts_poll_return
 cmp dword ptr [rbx+64],0
 jne .Lts_poll_return
 mov rdi,[rbx]
 call pg_durable_session_idle@PLT
 mov ebp,1
 test eax,eax
 jz .Lts_poll_return
 mov dword ptr [rbx+56],1
 mov rdi,rbx
 call .Lts_discover
 mov ebp,eax
 test eax,eax
 jnz .Lts_poll_unlock
 mov rdi,rbx
 call .Ltw_reconcile
 mov ebp,eax
 test eax,eax
 jnz .Lts_poll_unlock
 mov ebp,3
 xor r12d,r12d
 movabs rax,0x7ff0000000000000
 mov [rbx+48],rax
.Lts_poll_each:
 cmp r12,[rbx+24]
 jae .Lts_poll_finished
 cmp dword ptr [rbx+64],0
 jne .Lts_poll_closed
 mov rax,[rbx+16]
 mov r14,[rax+r12*8]
 mov rdi,r14
 call pg_task_runner_poll@PLT
 cmp eax,4
 jae .Lts_poll_error
 cmp eax,2
 je .Lts_poll_remove
 test eax,eax
 jz .Lts_poll_progress
 cmp eax,1
 jne .Lts_poll_deadline
 test ebp,ebp
 jz .Lts_poll_deadline
 mov ebp,1
 jmp .Lts_poll_deadline
.Lts_poll_progress:
 xor ebp,ebp
.Lts_poll_deadline:
 mov rdi,[r14+24]
 call .Ltr_record_state
 cmp eax,3
 jne .Lts_poll_next
 TGET "qword ptr [r14+24]",abort_requested
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lts_poll_next
 TGET "qword ptr [r14+24]",state_key
 TGET rax,until
 mov rdi,rax
 call json_object_get_double@PLT
 minsd xmm0,qword ptr [rbx+48]
 movsd [rbx+48],xmm0
.Lts_poll_next:
 inc r12
 jmp .Lts_poll_each
.Lts_poll_remove:
 mov rdi,r14
 call pg_task_runner_close@PLT
 test eax,eax
 jnz .Lts_poll_error
 mov rdi,r14
 call free@PLT
 mov rax,[rbx+16]
 mov qword ptr [rax+r12*8],0
 xor ebp,ebp
 inc r12
 jmp .Lts_poll_each
.Lts_poll_finished:
 /* Compact once per pass; settled runner removal is linear in live count. */
 mov rax,[rbx+16]
 xor ecx,ecx
 xor edx,edx
.Lts_poll_compact:
 cmp rcx,[rbx+24]
 jae .Lts_poll_compacted
 mov rsi,[rax+rcx*8]
 inc rcx
 test rsi,rsi
 jz .Lts_poll_compact
 mov [rax+rdx*8],rsi
 inc rdx
 jmp .Lts_poll_compact
.Lts_poll_compacted:
 mov [rbx+24],rdx
 test rdx,rdx
 jnz .Lts_poll_unlock
 cmp ebp,4
 jae .Lts_poll_unlock
 /* A phase can hand over to a newly committed conversation-owned task. */
 mov rdi,rbx
 call .Lts_discover
 mov ebp,eax
 test eax,eax
 jnz .Lts_poll_unlock
 cmp qword ptr [rbx+24],0
 jne .Lts_poll_unlock
 mov ebp,2
 jmp .Lts_poll_unlock
.Lts_poll_closed:
 mov ebp,7
 jmp .Lts_poll_finished
.Lts_poll_error:
 mov ebp,eax
 jmp .Lts_poll_finished
.Lts_poll_unlock:
 mov dword ptr [rbx+56],0
.Lts_poll_return:
 mov eax,ebp
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_task_scheduler_poll,.-pg_task_scheduler_poll

.globl pg_task_scheduler_close
.type pg_task_scheduler_close,@function
pg_task_scheduler_close:
 push rbp
 push rbx
 push r12
 mov rbx,rdi
 mov ebp,4
 test rdi,rdi
 jz .Lts_close_return
 xor ebp,ebp
 cmp dword ptr [rbx+60],0
 je .Lts_close_return
 mov dword ptr [rbx+64],1
 mov ebp,1
 cmp dword ptr [rbx+56],0
 jne .Lts_close_return
 mov dword ptr [rbx+56],1
 xor ebp,ebp
.Lts_close_each:
 cmp qword ptr [rbx+24],0
 je .Lts_close_free
 dec qword ptr [rbx+24]
 mov rax,[rbx+16]
 mov rcx,[rbx+24]
 mov r12,[rax+rcx*8]
 mov rdi,r12
 call pg_task_runner_close@PLT
 test ebp,ebp
 cmovz ebp,eax
 mov rdi,r12
 call free@PLT
 jmp .Lts_close_each
.Lts_close_free:
 mov rdi,[rbx+16]
 call free@PLT
 mov qword ptr [rbx+16],0
 mov qword ptr [rbx+32],0
 mov qword ptr [rbx+56],0
.Lts_close_return:
 mov eax,ebp
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_task_scheduler_close,.-pg_task_scheduler_close

.text
/* Temporary wait graph: sorted nodes (id,parent,record,first,next,status,table),
   stable native pointers, owned record snapshots; queue and visited are bounded
   by node count. Background tasks break ordinary ownership edges. */
.Ltw_compare:
 mov rax,[rdi]
 cmp rax,[rsi]
 setg al
 setl dl
 movzx eax,al
 movzx edx,dl
 sub eax,edx
 ret
.Ltw_find:
 mov r8,[rdi]
 xor ecx,ecx
 mov r9,[rdi+8]
.Ltw_find_each:
 cmp rcx,r9
 jae .Ltw_find_missing
 lea r10,[rcx+r9]
 shr r10,1
 imul rax,r10,48
 add rax,r8
 cmp [rax],rsi
 je .Ltw_find_return
 jb .Ltw_find_after
 mov r9,r10
 jmp .Ltw_find_each
.Ltw_find_after:
 lea rcx,[r10+1]
 jmp .Ltw_find_each
.Ltw_find_missing:
 xor eax,eax
.Ltw_find_return:
 ret
.Ltw_push:
 test rsi,rsi
 jz .Ltw_push_return
 mov rax,rsi
 sub rax,[rdi]
 xor edx,edx
 mov ecx,48
 div rcx
 mov rcx,[rdi+32]
 cmp byte ptr [rcx+rax],0
 jne .Ltw_push_return
 mov byte ptr [rcx+rax],1
 mov rcx,[rdi+24]
 mov rax,[rdi+40]
 mov [rcx+rax*8],rsi
 inc qword ptr [rdi+40]
.Ltw_push_return:
 ret
.Ltw_release:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 xor r12d,r12d
.Ltw_release_each:
 cmp r12,[rbx+8]
 jae .Ltw_release_arrays
 imul rax,r12,48
 add rax,[rbx]
 mov rdi,[rax+16]
 call json_object_put@PLT
 inc r12
 jmp .Ltw_release_each
.Ltw_release_arrays:
 mov rdi,[rbx]
 call free@PLT
 mov rdi,[rbx+24]
 call free@PLT
 mov rdi,[rbx+32]
 call free@PLT
 add rsp,8
 pop r12
 pop rbx
 ret
.Ltw_snapshot:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,144
 mov rbx,rdi
 mov r14,rsi
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 movdqu [rsp+64],xmm0
 movdqu [rsp+80],xmm0
 movdqu [rsp+96],xmm0
 movdqu [rsp+112],xmm0
 mov qword ptr [rsp+128],0
 mov dword ptr [rsp],2
 mov dword ptr [rsp+4],1
 mov qword ptr [rsp+24],256
 mov qword ptr [rsp+32],64
.Ltw_snapshot_page:
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+112]
 call [rbx+16]
 mov ebp,6
 test eax,eax
 jnz .Ltw_snapshot_end
 xor r12d,r12d
.Ltw_snapshot_row:
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ltw_snapshot_next
 mov rdi,[rsp+112]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov [rsp+136],rax
 mov rax,[r14+8]
 cmp rax,[r14+16]
 jb .Ltw_snapshot_append
 mov r13,[r14+16]
 test r13,r13
 jnz .Ltw_snapshot_grow
 mov r13,16
 jmp .Ltw_snapshot_resize
.Ltw_snapshot_grow:
 shl r13,1
 jc .Ltw_snapshot_alloc
.Ltw_snapshot_resize:
 mov rdi,[r14]
 mov rsi,r13
 mov edx,48
 call reallocarray@PLT
 test rax,rax
 jz .Ltw_snapshot_alloc
 mov [r14],rax
 mov [r14+16],r13
.Ltw_snapshot_append:
 imul r13,qword ptr [r14+8],48
 add r13,[r14]
 inc qword ptr [r14+8]
 mov rdi,r13
 mov rsi,[rsp+136]
 mov edx,[rsp]
 call .Ltw_node
 test eax,eax
 jnz .Ltw_snapshot_bad
.Ltw_snapshot_skip:
 inc r12
 jmp .Ltw_snapshot_row
.Ltw_snapshot_next:
 cmp dword ptr [rsp+132],0
 je .Ltw_snapshot_table
 mov rax,[rsp+120]
 cmp dword ptr [rsp+8],0
 je .Ltw_snapshot_cursor
 cmp rax,[rsp+16]
 jbe .Ltw_snapshot_bad
.Ltw_snapshot_cursor:
 mov [rsp+16],rax
 mov dword ptr [rsp+8],1
 mov dword ptr [rsp+12],1
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 jmp .Ltw_snapshot_page
.Ltw_snapshot_table:
 cmp dword ptr [rsp],0
 je .Ltw_snapshot_sort
 inc dword ptr [rsp+96]
 cmp dword ptr [rsp+96],5
 jb .Ltw_snapshot_reset
 mov dword ptr [rsp],0
 mov qword ptr [rsp+32],0
 mov dword ptr [rsp+96],0
.Ltw_snapshot_reset:
 mov dword ptr [rsp+8],0
 mov dword ptr [rsp+12],0
 mov qword ptr [rsp+16],0
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 jmp .Ltw_snapshot_page
.Ltw_snapshot_sort:
 mov rdi,r14
 call .Ltw_index
 mov ebp,eax
 jmp .Ltw_snapshot_end
.Ltw_snapshot_bad:
 mov ebp,6
 jmp .Ltw_snapshot_end
.Ltw_snapshot_alloc:
 mov ebp,5
.Ltw_snapshot_end:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,144
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Admission checks the wait-for graph with ordinary ownership edges and
   committed waits. The Session line is synchronous through this decision. */
.Ltw_validate:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,96
 mov rbx,rdi
 mov r12,rsi
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 mov qword ptr [rsp+64],0
 mov ebp,4
 cmp qword ptr [rbx+48],0
 je .Ltw_validate_end
 TGET r12,on
 mov [rsp+72],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ltw_validate_end
 TGET r12,policy
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Ltw_fail_fast]
 call .Ltr_equal
 mov [rsp+80],eax
 test eax,eax
 jnz .Ltw_validate_snapshot
 mov rdi,r13
 lea rsi,[rip+.Ltw_all_settled]
 call .Ltr_equal
 test eax,eax
 jz .Ltw_validate_end
.Ltw_validate_snapshot:
 mov rdi,[rbx+8]
 mov rsi,rsp
 call .Ltw_snapshot
 mov ebp,eax
 test eax,eax
 jnz .Ltw_validate_end
 cmp qword ptr [rbx+112],0
 je .Ltw_validate_targets_start
 mov rdi,rsp
 mov rsi,[rbx+112]
 call .Ltw_overlay
 mov ebp,eax
 test eax,eax
 jnz .Ltw_validate_end
.Ltw_validate_targets_start:
 xor r12d,r12d
.Ltw_validate_target:
 mov rdi,[rsp+72]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ltw_validate_walk
 mov rdi,[rsp+72]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ltw_validate_invalid
 test rax,rax
 jz .Ltw_validate_invalid
 mov rsi,rax
 mov rdi,rsp
 call .Ltw_find
 test rax,rax
 jz .Ltw_validate_read_target
 mov r13,rax
 cmp dword ptr [r13+44],2
 jne .Ltw_validate_missing
 mov rax,[r13+16]
 mov [rsp+88],rax
 jmp .Ltw_validate_owner
.Ltw_validate_read_target:
 mov rdi,[rsp+72]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdx,rax
 mov rax,[rbx+8]
 mov rdi,[rax]
 mov esi,2
 lea rcx,[rsp+64]
 call [rax+8]
 cmp eax,1
 je .Ltw_validate_missing
 test eax,eax
 jnz .Ltw_validate_storage
 cmp qword ptr [rsp+64],0
 je .Ltw_validate_storage
 mov rdi,[rsp+64]
 call .Ltr_record_state
 cmp eax,5
 jne .Ltw_validate_storage
 xor r13d,r13d
 mov rax,[rsp+64]
 mov [rsp+88],rax
.Ltw_validate_owner:
 cmp dword ptr [rsp+80],0
 je .Ltw_validate_live
 TGET "qword ptr [rsp+88]",owner
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ltw_validate_ownership
 cmp rax,[rbx+16]
 jne .Ltw_validate_ownership
.Ltw_validate_live:
 test r13,r13
 jz .Ltw_validate_next
 cmp dword ptr [r13+40],5
 je .Ltw_validate_next
 mov rdi,rsp
 mov rsi,r13
 call .Ltw_push
.Ltw_validate_next:
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov qword ptr [rsp+64],0
 inc r12
 jmp .Ltw_validate_target
.Ltw_validate_walk:
 mov rax,[rsp+48]
 cmp rax,[rsp+40]
 jae .Ltw_validate_ok
 mov rcx,[rsp+24]
 mov r13,[rcx+rax*8]
 inc qword ptr [rsp+48]
 mov rax,[rbx+16]
 cmp [r13],rax
 je .Ltw_validate_cycle
 cmp dword ptr [r13+44],0
 je .Ltw_validate_children
 cmp dword ptr [r13+40],5
 je .Ltw_validate_walk
 cmp dword ptr [r13+40],2
 jne .Ltw_validate_children
 TGET "qword ptr [r13+16]",abort_requested
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Ltw_validate_children
 TGET "qword ptr [r13+16]",state_key
 TGET rax,on
 mov r14,rax
 xor r12d,r12d
.Ltw_validate_wait_edge:
 mov rdi,r14
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ltw_validate_children
 mov rdi,r14
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdi,rsp
 mov rsi,rax
 call .Ltw_find
 mov rdi,rsp
 mov rsi,rax
 call .Ltw_push
 inc r12
 jmp .Ltw_validate_wait_edge
.Ltw_validate_children:
 mov r12,[r13+24]
.Ltw_validate_child:
 test r12,r12
 jz .Ltw_validate_walk
 lea rax,[r12-1]
 imul rax,48
 add rax,[rsp]
 mov r12,[rax+32]
 mov rdi,rsp
 mov rsi,rax
 call .Ltw_push
 jmp .Ltw_validate_child
.Ltw_validate_ok:
 xor ebp,ebp
 jmp .Ltw_validate_end
.Ltw_validate_missing:
 lea rax,[rip+.Ltw_missing]
 jmp .Ltw_validate_error
.Ltw_validate_ownership:
 lea rax,[rip+.Ltw_ownership]
 jmp .Ltw_validate_error
.Ltw_validate_cycle:
 lea rax,[rip+.Ltw_cycle]
.Ltw_validate_error:
 mov [rbx+96],rax
 mov rdi,rax
 call strlen@PLT
 mov [rbx+104],rax
.Ltw_validate_invalid:
 mov ebp,4
 jmp .Ltw_validate_end
.Ltw_validate_storage:
 mov ebp,6
.Ltw_validate_end:
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov rdi,rsp
 call .Ltw_release
 mov eax,ebp
 add rsp,96
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Ltr_on: .asciz "on"
.Ltr_policy: .asciz "policy"
.Ltr_background: .asciz "background"
.Ltr_conversation_id: .asciz "conversationId"
.Ltr_task_id: .asciz "taskId"
.Ltw_fail_fast: .asciz "failFast"
.Ltw_all_settled: .asciz "allSettled"
.Ltw_missing: .asciz "Task wait names a missing task"
.Ltw_ownership: .asciz "Task can wait failFast only on tasks it owns"
.Ltw_cycle: .asciz "Task wait would create a wait cycle"

.text
/* 0 ready, 1 live dependency, -1 provider failure. */
.Ltw_waiting:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov qword ptr [rsp],0
 TGET "qword ptr [rbx+24]",state_key
 TGET rax,on
 mov r13,rax
 xor r12d,r12d
.Ltw_waiting_each:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ltw_waiting_ready
 mov rdi,r13
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdx,rax
 mov r14,[rbx+8]
 mov rdi,[r14]
 mov esi,2
 mov rcx,rsp
 call [r14+8]
 cmp eax,1
 je .Ltw_waiting_missing
 test eax,eax
 jnz .Ltw_waiting_error
 cmp qword ptr [rsp],0
 je .Ltw_waiting_error
 mov rdi,[rsp]
 call .Ltr_record_state
 cmp eax,5
 jne .Ltw_waiting_pending
.Ltw_waiting_missing:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov qword ptr [rsp],0
 inc r12
 jmp .Ltw_waiting_each
.Ltw_waiting_ready:
 xor ebp,ebp
 jmp .Ltw_waiting_end
.Ltw_waiting_pending:
 mov ebp,1
 jmp .Ltw_waiting_end
.Ltw_waiting_error:
 mov ebp,-1
.Ltw_waiting_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Ltw_children:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,[rsi+24]
.Ltw_children_each:
 test r12,r12
 jz .Ltw_children_end
 lea rax,[r12-1]
 imul rax,48
 add rax,[rbx]
 mov r12,[rax+32]
 mov rsi,rax
 mov rdi,rbx
 call .Ltw_push
 jmp .Ltw_children_each
.Ltw_children_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.Ltw_failed:
 xor eax,eax
 cmp dword ptr [rdi+44],2
 jne .Ltw_failed_return
 cmp dword ptr [rdi+40],4
 jb .Ltw_failed_return
 sub rsp,8
 TGET "qword ptr [rdi+16]",state_key
 TGET rax,outcome
 TGET rax,status
 mov rdi,rax
 lea rsi,[rip+.Ltw_completed]
 call .Ltr_equal
 xor eax,1
 add rsp,8
.Ltw_failed_return:
 ret
/* Derive cancellation from persisted intent and failed join members, before
   reserving handlers. All new marks share a single real Session transaction. */
.Ltw_reconcile:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,208
 mov rbx,rdi
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 movdqu [rsp+64],xmm0
 movdqu [rsp+80],xmm0
 movdqu [rsp+96],xmm0
 movdqu [rsp+112],xmm0
 movdqu [rsp+128],xmm0
 movdqu [rsp+144],xmm0
 movdqu [rsp+160],xmm0
 movdqu [rsp+176],xmm0
 movdqu [rsp+192],xmm0
 xor r12d,r12d
 xor ebp,ebp
.Ltw_reconcile_needed:
 cmp r12,[rbx+24]
 jae .Ltw_reconcile_end
 mov rax,[rbx+16]
 mov r13,[rax+r12*8]
 TGET "qword ptr [r13+24]",abort_requested
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Ltw_reconcile_snapshot
 mov rdi,[r13+24]
 call .Ltr_record_state
 cmp eax,2
 je .Ltw_reconcile_snapshot
 cmp eax,4
 je .Ltw_reconcile_snapshot
 inc r12
 jmp .Ltw_reconcile_needed
.Ltw_reconcile_snapshot:
 mov rdi,[rbx+8]
 mov rsi,rsp
 call .Ltw_snapshot
 mov ebp,eax
 test eax,eax
 jnz .Ltw_reconcile_end
 call json_object_new_array@PLT
 mov [rsp+64],rax
 test rax,rax
 jz .Ltw_reconcile_alloc
 xor r12d,r12d
.Ltw_reconcile_owner:
 cmp r12,[rsp+8]
 jae .Ltw_reconcile_walk
 imul r13,r12,48
 add r13,[rsp]
 cmp dword ptr [r13+44],2
 jne .Ltw_reconcile_owner_next
 cmp dword ptr [r13+40],5
 je .Ltw_reconcile_owner_next
 TGET "qword ptr [r13+16]",abort_requested
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Ltw_reconcile_intent
 mov rdi,r13
 call .Ltw_failed
 test eax,eax
 jz .Ltw_reconcile_join
.Ltw_reconcile_intent:
 mov rdi,rsp
 mov rsi,r13
 call .Ltw_children
.Ltw_reconcile_join:
 cmp dword ptr [r13+40],2
 jne .Ltw_reconcile_owner_next
 TGET "qword ptr [r13+16]",state_key
 TGET rax,policy
 mov rdi,rax
 lea rsi,[rip+.Ltw_fail_fast]
 call .Ltr_equal
 test eax,eax
 jz .Ltw_reconcile_owner_next
 TGET "qword ptr [r13+16]",state_key
 TGET rax,on
 mov [rsp+192],rax
 xor r14d,r14d
.Ltw_reconcile_failed:
 mov rdi,[rsp+192]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Ltw_reconcile_owner_next
 mov rdi,[rsp+192]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdi,rsp
 mov rsi,[rbx+8]
 mov rdx,rax
 call .Ltw_member_failed
 test eax,eax
 js .Ltw_reconcile_storage
 jnz .Ltw_reconcile_members
.Ltw_reconcile_failed_next:
 inc r14
 jmp .Ltw_reconcile_failed
.Ltw_reconcile_members:
 xor r14d,r14d
.Ltw_reconcile_member:
 mov rdi,[rsp+192]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Ltw_reconcile_owner_next
 mov rdi,[rsp+192]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdi,rsp
 mov rsi,rax
 call .Ltw_find
 mov [rsp+200],rax
 test rax,rax
 jz .Ltw_reconcile_member_next
 mov rdi,rax
 call .Ltw_failed
 test eax,eax
 jnz .Ltw_reconcile_member_next
 mov rdi,rsp
 mov rsi,[rsp+200]
 call .Ltw_push
.Ltw_reconcile_member_next:
 inc r14
 jmp .Ltw_reconcile_member
.Ltw_reconcile_owner_next:
 inc r12
 jmp .Ltw_reconcile_owner
.Ltw_reconcile_walk:
 mov rax,[rsp+48]
 cmp rax,[rsp+40]
 jae .Ltw_reconcile_commit
 mov rcx,[rsp+24]
 mov r13,[rcx+rax*8]
 inc qword ptr [rsp+48]
 cmp dword ptr [r13+44],0
 je .Ltw_reconcile_descend
 cmp dword ptr [r13+40],5
 je .Ltw_reconcile_walk
 mov rdi,r13
 call .Ltw_failed
 test eax,eax
 jnz .Ltw_reconcile_descend
 TGET "qword ptr [r13+16]",abort_requested
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Ltw_reconcile_mark
 TGET "qword ptr [r13+16]",abort_reason
 test rax,rax
 jz .Ltw_reconcile_descend
.Ltw_reconcile_mark:
 mov rdi,[r13+16]
 lea rsi,[rsp+184]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ltw_reconcile_alloc
 mov edi,1
 call json_object_new_boolean@PLT
 test rax,rax
 jz .Ltw_reconcile_alloc
 mov rdx,rax
 mov rdi,[rsp+184]
 lea rsi,[rip+.Ltr_abort_requested]
 call .Ltr_set
 test eax,eax
 jnz .Ltw_reconcile_alloc
 mov rdi,[rsp+184]
 lea rsi,[rip+.Ltr_abort_reason]
 call json_object_object_del@PLT
 mov rdi,[rsp+64]
 mov rsi,[rsp+184]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ltw_reconcile_alloc
 mov qword ptr [rsp+184],0
.Ltw_reconcile_descend:
 mov rdi,rsp
 mov rsi,r13
 call .Ltw_children
 jmp .Ltw_reconcile_walk
.Ltw_reconcile_commit:
 mov rdi,[rsp+64]
 call json_object_array_length@PLT
 test rax,rax
 jz .Ltw_reconcile_ok
 mov r14,[rbx+8]
 mov rdi,[rbx]
 lea rsi,[rsp+72]
 lea rdx,[rsp+120]
 mov rcx,[r14]
 mov r8,[r14+8]
 call pg_durable_session_begin@PLT
 mov ebp,1
 test eax,eax
 jnz .Ltw_reconcile_end
 xor r12d,r12d
.Ltw_reconcile_stage:
 mov rdi,[rsp+64]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ltw_reconcile_settle
 mov rdi,[rsp+64]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov rsi,rax
 lea rdi,[rsp+72]
 mov rdx,[r14]
 mov rcx,[r14+40]
 call pg_record_tx_set_task@PLT
 test eax,eax
 jnz .Ltw_reconcile_reject
 inc r12
 jmp .Ltw_reconcile_stage
.Ltw_reconcile_settle:
 mov rdi,[rbx]
 xor esi,esi
 xor edx,edx
 xor ecx,ecx
 call pg_durable_session_settle@PLT
 test eax,eax
 jnz .Ltw_reconcile_reject
.Ltw_reconcile_ok:
 xor ebp,ebp
 jmp .Ltw_reconcile_end
.Ltw_reconcile_reject:
 mov rdi,[rbx]
 call pg_durable_session_abort@PLT
 mov ebp,6
 jmp .Ltw_reconcile_end
.Ltw_reconcile_alloc:
 mov ebp,5
 jmp .Ltw_reconcile_end
.Ltw_reconcile_storage:
 mov ebp,6
.Ltw_reconcile_end:
 cmp ebp,6
 jne .Ltw_reconcile_release
 mov rax,[rbx]
 mov rax,[rax]
 cmp dword ptr [rax+52],0
 jne .Ltw_reconcile_release
 mov dword ptr [rax+52],3
.Ltw_reconcile_release:
 mov rdi,[rsp+184]
 call json_object_put@PLT
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov rdi,rsp
 call .Ltw_release
 mov eax,ebp
 add rsp,208
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Ltr_abort_reason: .asciz "abortReason"
.Ltw_completed: .asciz "completed"

.text
/* Live graph omits settled history; direct lookup tests settled join receipts. */
.Ltw_member_failed:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rsi
 mov r12,rdx
 mov qword ptr [rsp],0
 mov rsi,rdx
 call .Ltw_find
 test rax,rax
 jz .Ltw_member_read
 mov rdi,rax
 call .Ltw_failed
 jmp .Ltw_member_end
.Ltw_member_read:
 mov rdi,[rbx]
 mov esi,2
 mov rdx,r12
 mov rcx,rsp
 call [rbx+8]
 cmp eax,1
 je .Ltw_member_none
 test eax,eax
 jnz .Ltw_member_error
 cmp qword ptr [rsp],0
 je .Ltw_member_error
 mov rdi,[rsp]
 call .Ltr_record_state
 cmp eax,4
 jb .Ltw_member_none
 TGET "qword ptr [rsp]",state_key
 TGET rax,outcome
 TGET rax,status
 mov rdi,rax
 lea rsi,[rip+.Ltw_completed]
 call .Ltr_equal
 xor eax,1
 jmp .Ltw_member_end
.Ltw_member_none:
 xor eax,eax
 jmp .Ltw_member_end
.Ltw_member_error:
 mov eax,-1
.Ltw_member_end:
 mov r12d,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,r12d
 add rsp,24
 pop r12
 pop rbx
 ret

.text
.globl pg_task_runner_commit_with
.type pg_task_runner_commit_with,@function
pg_task_runner_commit_with:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,128
 mov rbx,rdi
 mov r13,rsi
 mov r12,rdx
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 movdqu [rsp+64],xmm0
 movdqu [rsp+80],xmm0
 mov qword ptr [rsp+96],0
 mov ebp,4
 test rbx,rbx
 jz .Ltc_return
 test r12,r12
 jz .Ltc_return
 cmp dword ptr [rbx+76],1
 jne .Ltc_return
 cmp qword ptr [rbx+32],0
 je .Ltc_return
 cmp qword ptr [rbx+112],0
 jne .Ltc_return
 cmp dword ptr [rbx+92],0
 jne .Ltc_return
 mov ebp,7
 cmp dword ptr [rbx+84],0
 jne .Ltc_return
 mov rdi,[rbx]
 call pg_durable_session_idle@PLT
 mov ebp,1
 test eax,eax
 jz .Ltc_return
 mov r14,[rbx+8]
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+48]
 mov rcx,[r14]
 mov r8,[r14+8]
 call pg_durable_session_begin@PLT
 test eax,eax
 jnz .Ltc_return
 mov [rbx+112],rsp
 mov dword ptr [rbx+92],1
 mov rdi,r13
 mov rsi,rsp
 lea rdx,[rsp+48]
 lea rcx,[rsp+96]
 call r12
 mov ebp,eax
 test eax,eax
 jnz .Ltc_callback_error
 mov dword ptr [rbx+92],0
 cmp qword ptr [rsp+96],0
 jne .Ltc_decide
 mov rdi,rbx
 call .Ltr_fetch
 mov ebp,eax
 test eax,eax
 jnz .Ltc_cleanup
 TGET "qword ptr [rbx+24]",state_key
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+96],rax
.Ltc_decide:
 mov rdi,rbx
 mov rsi,[rsp+96]
 call pg_task_runner_commit@PLT
 mov ebp,eax
 jmp .Ltc_cleanup
.Ltc_callback_error:
 cmp ebp,4
 jae .Ltc_cleanup
 mov ebp,4
.Ltc_cleanup:
 mov dword ptr [rbx+92],0
 mov rdi,[rbx]
 cmp [rdi+152],rsp
 jne .Ltc_release
 call pg_durable_session_abort@PLT
.Ltc_release:
 mov qword ptr [rbx+112],0
 mov rdi,[rsp+96]
 call json_object_put@PLT
.Ltc_return:
 mov eax,ebp
 add rsp,128
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_task_runner_commit_with,.-pg_task_runner_commit_with

/* Replace/admit a detached staged record in a temporary graph node. */
.Ltw_node:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12d,edx
 pxor xmm0,xmm0
 movdqu [rbx],xmm0
 movdqu [rbx+16],xmm0
 movdqu [rbx+32],xmm0
 mov rdi,rsi
 call json_object_get@PLT
 mov [rbx+16],rax
 mov [rbx+44],r12d
 TGET rax,id
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ltw_node_bad
 test rax,rax
 jz .Ltw_node_bad
 mov [rbx],rax
 test r12d,r12d
 jz .Ltw_node_conversation
 mov rdi,[rbx+16]
 call .Ltr_record_state
 test eax,eax
 js .Ltw_node_bad
 mov [rbx+40],eax
 TGET "qword ptr [rbx+16]",background
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Ltw_node_good
 TGET "qword ptr [rbx+16]",owner
 test rax,rax
 jnz .Ltw_node_parent
 TGET "qword ptr [rbx+16]",conversation_id
 jmp .Ltw_node_parent
.Ltw_node_conversation:
 TGET "qword ptr [rbx+16]",owner
 TGET rax,task_id
 test rax,rax
 jz .Ltw_node_good
.Ltw_node_parent:
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ltw_node_bad
 mov [rbx+8],rax
.Ltw_node_good:
 xor eax,eax
 jmp .Ltw_node_end
.Ltw_node_bad:
 mov eax,4
.Ltw_node_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.Ltw_index:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov rdi,[rbx]
 mov rsi,[rbx+8]
 mov edx,48
 lea rcx,[rip+.Ltw_compare]
 call qsort@PLT
 xor r12d,r12d
.Ltw_index_clear:
 cmp r12,[rbx+8]
 jae .Ltw_index_link_start
 imul rax,r12,48
 add rax,[rbx]
 mov qword ptr [rax+24],0
 mov qword ptr [rax+32],0
 inc r12
 jmp .Ltw_index_clear
.Ltw_index_link_start:
 xor r12d,r12d
.Ltw_index_link:
 cmp r12,[rbx+8]
 jae .Ltw_index_buffers
 imul r13,r12,48
 add r13,[rbx]
 mov rsi,[r13+8]
 test rsi,rsi
 jz .Ltw_index_next
 mov rdi,rbx
 call .Ltw_find
 test rax,rax
 jz .Ltw_index_next
 mov rcx,[rax+24]
 mov [r13+32],rcx
 lea rcx,[r12+1]
 mov [rax+24],rcx
.Ltw_index_next:
 inc r12
 jmp .Ltw_index_link
.Ltw_index_buffers:
 mov qword ptr [rbx+40],0
 mov qword ptr [rbx+48],0
 mov rdi,[rbx+24]
 call free@PLT
 mov qword ptr [rbx+24],0
 mov rdi,[rbx+32]
 call free@PLT
 mov qword ptr [rbx+32],0
 mov rdi,[rbx+8]
 mov esi,8
 call calloc@PLT
 mov [rbx+24],rax
 test rax,rax
 jz .Ltw_index_alloc
 mov rdi,[rbx+8]
 mov esi,1
 call calloc@PLT
 mov [rbx+32],rax
 test rax,rax
 jz .Ltw_index_alloc
 xor eax,eax
 jmp .Ltw_index_end
.Ltw_index_alloc:
 mov eax,5
.Ltw_index_end:
 pop r13
 pop r12
 pop rbx
 ret
/* Merge the Tx's latest detached candidates before graph decisions. */
.Ltw_overlay:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov rdi,r12
 mov rsi,rsp
 call pg_record_tx_staged_tasks@PLT
 mov ebp,5
 test eax,eax
 jnz .Ltw_overlay_end
 mov rdi,r12
 lea rsi,[rsp+8]
 call pg_record_tx_staged_conversations@PLT
 test eax,eax
 jnz .Ltw_overlay_end
 mov rdi,[rsp]
 call json_object_array_length@PLT
 mov r12,rax
 mov rdi,[rsp+8]
 call json_object_array_length@PLT
 add r12,rax
 jc .Ltw_overlay_end
 add r12,[rbx+8]
 jc .Ltw_overlay_end
 cmp r12,[rbx+16]
 jbe .Ltw_overlay_start
 mov rdi,[rbx]
 mov rsi,r12
 mov edx,48
 call reallocarray@PLT
 test rax,rax
 jz .Ltw_overlay_end
 mov [rbx],rax
 mov [rbx+16],r12
.Ltw_overlay_start:
 xor r12d,r12d
 mov dword ptr [rsp+24],2
.Ltw_overlay_each:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ltw_overlay_next_table
 mov rdi,[rsp]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov [rsp+16],rax
 TGET rax,id
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ltw_overlay_invalid
 mov r14,rax
 xor r13d,r13d
.Ltw_overlay_find:
 cmp r13,[rbx+8]
 jae .Ltw_overlay_append
 imul rax,r13,48
 add rax,[rbx]
 cmp [rax],r14
 je .Ltw_overlay_replace
 inc r13
 jmp .Ltw_overlay_find
.Ltw_overlay_replace:
 mov rdi,[rax+16]
 call json_object_put@PLT
 jmp .Ltw_overlay_node
.Ltw_overlay_append:
 inc qword ptr [rbx+8]
.Ltw_overlay_node:
 imul rdi,r13,48
 add rdi,[rbx]
 mov rsi,[rsp+16]
 mov edx,[rsp+24]
 call .Ltw_node
 test eax,eax
 jnz .Ltw_overlay_invalid
 inc r12
 jmp .Ltw_overlay_each
.Ltw_overlay_next_table:
 cmp dword ptr [rsp+24],0
 je .Ltw_overlay_index
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rax,[rsp+8]
 mov [rsp],rax
 mov qword ptr [rsp+8],0
 mov dword ptr [rsp+24],0
 xor r12d,r12d
 jmp .Ltw_overlay_each
.Ltw_overlay_index:
 mov rdi,rbx
 call .Ltw_index
 mov ebp,eax
 jmp .Ltw_overlay_end
.Ltw_overlay_invalid:
 mov ebp,4
.Ltw_overlay_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Ltw_has_owned:
 push rbp
 push rbx
 push r12
 sub rsp,64
 mov rbx,rdi
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 mov rdi,[rbx+8]
 mov rsi,rsp
 call .Ltw_snapshot
 mov ebp,eax
 test eax,eax
 jnz .Ltw_owned_error
 mov rdi,rsp
 mov rsi,[rbx+112]
 call .Ltw_overlay
 mov ebp,eax
 test eax,eax
 jnz .Ltw_owned_error
 mov rdi,rsp
 mov rsi,[rbx+16]
 call .Ltw_find
 test rax,rax
 jz .Ltw_owned_storage
 mov rdi,rsp
 mov rsi,rax
 call .Ltw_children
.Ltw_owned_walk:
 mov rax,[rsp+48]
 cmp rax,[rsp+40]
 jae .Ltw_owned_none
 mov rcx,[rsp+24]
 mov r12,[rcx+rax*8]
 inc qword ptr [rsp+48]
 cmp dword ptr [r12+44],0
 je .Ltw_owned_conversation
 cmp dword ptr [r12+40],5
 je .Ltw_owned_walk
 mov ebp,1
 jmp .Ltw_owned_end
.Ltw_owned_conversation:
 mov rdi,rsp
 mov rsi,r12
 call .Ltw_children
 jmp .Ltw_owned_walk
.Ltw_owned_none:
 xor ebp,ebp
 jmp .Ltw_owned_end
.Ltw_owned_storage:
 mov ebp,6
.Ltw_owned_error:
 neg ebp
.Ltw_owned_end:
 mov rdi,rsp
 call .Ltw_release
 mov eax,ebp
 add rsp,64
 pop r12
 pop rbx
 pop rbp
 ret

.section .note.GNU-stack,"",@progbits

.text
.Ltr_drop_lease:
 mov rax,[rdi+rsi]
 test rax,rax
 jz .Ltr_drop_lease_return
 mov qword ptr [rdi+rsi],0
 mov rcx,[rdi+128]
 mov rdi,rax
 jmp [rcx+16]
.Ltr_drop_lease_return:
 ret
.globl pg_task_scheduler_bind_definitions
.type pg_task_scheduler_bind_definitions,@function
pg_task_scheduler_bind_definitions:
 mov eax,4
 cmp dword ptr [rdi+60],0
 jne .Lts_bind_end
 cmp dword ptr [rdi+56],0
 jne .Lts_bind_end
 test rsi,rsi
 jz .Lts_bind_store
 cmp qword ptr [rsi+8],0
 je .Lts_bind_end
 cmp qword ptr [rsi+16],0
 je .Lts_bind_end
.Lts_bind_store:
 mov [rdi+72],rsi
 xor eax,eax
.Lts_bind_end:
 ret
