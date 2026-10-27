.intel_syntax noprefix
.macro IGET object,key
 mov rdi,\object
 lea rsi,[rip+.Li_\key]
 call json_object_object_get@PLT
.endm
.macro IEQ object,key
 mov rdi,\object
 lea rsi,[rip+.Li_\key]
 call .Li_equal
.endm
.macro BEGIN bytes
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\bytes
.endm
.macro END bytes
 add rsp,\bytes
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.text
.Li_set:
 test rdx,rdx
 jz .Li_set_bad
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Li_set_end
 mov rdi,rbx
 call json_object_put@PLT
.Li_set_bad_pop:
 mov eax,5
.Li_set_end:
 pop rbx
 ret
.Li_set_bad:
 mov eax,5
 ret
.Li_copy:
 test rdx,rdx
 jnz .Li_copy_value
 push rbx
 call json_object_object_add@PLT
 test eax,eax
 jz .Li_copy_null_end
 mov eax,5
.Li_copy_null_end:
 pop rbx
 ret
.Li_copy_value:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 call json_object_get@PLT
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call .Li_set
 add rsp,8
 pop r12
 pop rbx
 ret
.Li_equal:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Li_equal_end
 mov rdi,r12
 call strlen@PLT
 mov [rsp],rax
 mov rdi,rbx
 call json_object_get_string_len@PLT
 cmp rax,[rsp]
 jne .Li_equal_no
 mov rdi,rbx
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,r12
 mov rdx,[rsp]
 call memcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 jmp .Li_equal_end
.Li_equal_no:
 xor eax,eax
.Li_equal_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.Li_push:
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
 jz .Li_push_end
 mov rdi,r12
 call json_object_put@PLT
 mov eax,5
.Li_push_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.globl pg_durable_inputs_open
.type pg_durable_inputs_open,@function
pg_durable_inputs_open:
 mov eax,4
 test rdi,rdi
 jz .Li_open_end
 test rsi,rsi
 jz .Li_open_end
 test rdx,rdx
 jz .Li_open_end
 test rcx,rcx
 jz .Li_open_end
 test r8,r8
 jz .Li_open_end
 test r9,r9
 jz .Li_open_end
 mov [rdi],rsi
 mov [rdi+8],rdx
 mov [rdi+16],rcx
 mov [rdi+24],r8
 mov [rdi+32],r9
 mov dword ptr [rdi+40],0
 mov dword ptr [rdi+44],1
 xor eax,eax
.Li_open_end:
 ret
.size pg_durable_inputs_open,.-pg_durable_inputs_open
/* Snapshot queued/placed submissions and the live foreground generation before
   the transaction's first table write. Pages retain ID order. */
.globl pg_durable_inputs_prepare
.type pg_durable_inputs_prepare,@function
pg_durable_inputs_prepare:
 BEGIN 192
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov qword ptr [rcx],0
 pxor xmm0,xmm0
 movdqu [rsp+112],xmm0
 movdqu [rsp+128],xmm0
 mov qword ptr [rsp+160],0
 mov qword ptr [rsp+176],0
 mov dword ptr [rsp+156],4
 lea rdi,[rip+.Li_plan_json]
 call json_tokener_parse@PLT
 mov rbp,rax
 test rax,rax
 jz .Li_prepare_alloc
 mov rdi,r13
 call json_object_new_int64@PLT
 mov rdi,rbp
 lea rsi,[rip+.Li_conversation]
 mov rdx,rax
 call .Li_set
 test eax,eax
 jnz .Li_prepare_error
 mov dword ptr [rsp+152],0
.Li_prepare_query:
 mov rdi,rsp
 xor eax,eax
 mov ecx,14
 cld
 rep stosq
 mov dword ptr [rsp],3
 mov dword ptr [rsp+4],1
 mov qword ptr [rsp+24],256
 mov qword ptr [rsp+32],65
 mov [rsp+40],r13
 mov eax,[rsp+152]
 mov [rsp+96],eax
 cmp eax,2
 jne .Li_prepare_page
 mov dword ptr [rsp],2
 mov qword ptr [rsp+32],33
 lea rax,[rip+.Li_generation]
 mov [rsp+80],rax
 mov qword ptr [rsp+88],16
.Li_prepare_page:
 mov rdi,r12
 mov rsi,[rbx+24]
 mov rdx,rsp
 lea rcx,[rsp+112]
 call pg_record_tx_scan@PLT
 test eax,eax
 jnz .Li_prepare_error
 mov qword ptr [rsp+144],0
.Li_prepare_collect:
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp [rsp+144],rax
 jae .Li_prepare_next_page
 mov rdi,[rsp+112]
 mov rsi,[rsp+144]
 call json_object_array_get_idx@PLT
 mov [rsp+136],rax
 cmp dword ptr [rsp+152],2
 je .Li_prepare_task
 mov edx,[rsp+152]
 lea rsi,[rip+.Li_queued]
 test edx,edx
 jz .Li_prepare_bucket
 lea rsi,[rip+.Li_placed]
.Li_prepare_bucket:
 mov rdi,rbp
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,[rsp+136]
 call .Li_push
 test eax,eax
 jnz .Li_prepare_error
 jmp .Li_prepare_item_next
.Li_prepare_task:
 IGET "qword ptr [rsp+136]",background
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Li_prepare_item_next
 IGET "qword ptr [rsp+136]",state
 IGET rax,status
 IEQ rax,terminal
 test eax,eax
 jnz .Li_prepare_item_next
 mov edi,1
 call json_object_new_boolean@PLT
 mov rdi,rbp
 lea rsi,[rip+.Li_busy]
 mov rdx,rax
 call .Li_set
 test eax,eax
 jnz .Li_prepare_error
.Li_prepare_item_next:
 inc qword ptr [rsp+144]
 jmp .Li_prepare_collect
.Li_prepare_next_page:
 mov eax,[rsp+132]
 mov [rsp+168],eax
 mov rax,[rsp+120]
 mov [rsp+16],rax
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 cmp dword ptr [rsp+168],0
 je .Li_prepare_next_query
 mov dword ptr [rsp+8],1
 mov dword ptr [rsp+12],1
 jmp .Li_prepare_page
.Li_prepare_next_query:
 inc dword ptr [rsp+152]
 cmp dword ptr [rsp+152],3
 jb .Li_prepare_query
 mov rdi,r12
 mov rsi,[rbx+24]
 mov rdx,r13
 lea rcx,[rsp+160]
 call pg_record_tx_latest_head@PLT
 cmp eax,7
 je .Li_prepare_settings
 test eax,eax
 jnz .Li_prepare_error
 IGET "qword ptr [rsp+160]",head
 mov rdi,rbp
 lea rsi,[rip+.Li_head]
 mov rdx,rax
 call .Li_copy
 test eax,eax
 jnz .Li_prepare_error
.Li_prepare_settings:
 mov rax,[rbx+16]
 mov rax,[rax]
 mov rdi,[rax]
 mov rsi,r13
 lea rdx,[rsp+176]
 call [rax+8]
 test eax,eax
 jnz .Li_prepare_error
 mov rdi,rbp
 lea rsi,[rip+.Li_settings]
 mov rdx,[rsp+176]
 mov qword ptr [rsp+176],0
 call .Li_set
 test eax,eax
 jnz .Li_prepare_error
 mov [r14],rbp
 xor ebp,ebp
 xor eax,eax
 jmp .Li_prepare_error
.Li_prepare_alloc:
 mov eax,5
.Li_prepare_error:
 mov [rsp+156],eax
 mov rdi,rbp
 call json_object_put@PLT
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov rdi,[rsp+160]
 call json_object_put@PLT
 mov rdi,[rsp+176]
 call json_object_put@PLT
 mov eax,[rsp+156]
 END 192
.size pg_durable_inputs_prepare,.-pg_durable_inputs_prepare
/* Apply a typed transition and remove native queue payload after placement. */
.Li_change:
 BEGIN 16
 mov rbx,rdi
 mov rdi,rsi
 mov rsi,rdx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov rdx,rsp
 call pg_records_submission_change@PLT
 test eax,eax
 jnz .Li_change_end
 mov rdi,[rsp]
 lea rsi,[rip+.Li_draft]
 call json_object_object_del@PLT
 lea rdi,[rip+.Li_submission_write_json]
 call json_tokener_parse@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Li_change_alloc
 mov rdi,rax
 lea rsi,[rip+.Li_value]
 mov rdx,[rsp]
 call .Li_copy
 test eax,eax
 jnz .Li_change_end
 mov rdi,rbx
 mov esi,3
 mov rdx,[rsp+8]
 call pg_record_tx_stage@PLT
 jmp .Li_change_end
.Li_change_alloc:
 mov eax,5
.Li_change_end:
 mov ebp,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebp
 END 16
/* Append input with PanGu identity, or a host's typed Entry write. */
.Li_append:
 BEGIN 32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rbp,r8
 mov qword ptr [rsp],0
 IGET r14,type
 IEQ rax,write
 test eax,eax
 jz .Li_append_input
 IGET r14,entry
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp],rax
 jmp .Li_append_stage
.Li_append_input:
 lea rdi,[rip+.Li_user_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Li_append_alloc
 IGET r14,entry
 mov [rsp+8],rax
 test rax,rax
 jz .Li_append_model
 IGET rax,kind
 mov rdi,[rsp]
 lea rsi,[rip+.Li_kind]
 mov rdx,rax
 call .Li_copy
 test eax,eax
 jnz .Li_append_end
 mov rdi,[rsp+8]
 lea rsi,[rip+.Li_data]
 lea rdx,[rsp+24]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Li_append_model
 mov rdi,[rsp]
 lea rsi,[rip+.Li_data]
 mov rdx,[rsp+24]
 call .Li_copy
 test eax,eax
 jnz .Li_append_end
.Li_append_model:
 IGET "qword ptr [rsp]",model
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov [rsp+16],rax
 IGET r14,content
 mov rdi,[rsp+16]
 lea rsi,[rip+.Li_content]
 mov rdx,rax
 call .Li_copy
 test eax,eax
 jnz .Li_append_end
 mov rax,[rbx+8]
 mov rdi,[rax]
 call [rax+40]
 call json_object_new_double@PLT
 mov rdi,[rsp+16]
 lea rsi,[rip+.Li_timestamp]
 mov rdx,rax
 call .Li_set
 test eax,eax
 jnz .Li_append_end
.Li_append_stage:
 mov rdi,r12
 mov rsi,r13
 mov rdx,[rsp]
 xor ecx,ecx
 mov rax,[rbx+16]
 mov rax,[rax]
 mov r8,[rax+32]
 mov r9,rbp
 call pg_record_tx_append_entry@PLT
 jmp .Li_append_end
.Li_append_alloc:
 mov eax,5
.Li_append_end:
 mov ebp,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 END 32
.Li_initial:
 mov rax,[rdi+16]
 mov rdi,[rax+32]
 jmp [rax+40]
.Li_mint:
 mov rax,[rdi+16]
 mov rax,[rax]
 mov rax,[rax+32]
 mov rdi,[rdi+8]
 mov rdi,[rdi]
 jmp rax
.globl pg_durable_inputs_start
.type pg_durable_inputs_start,@function
pg_durable_inputs_start:
 BEGIN 80
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rbp,r8
 pxor xmm0,xmm0
 movdqu [rsp+48],xmm0
 mov qword ptr [rsp+64],0
 lea rdi,[rip+.Li_definition_json]
 call json_tokener_parse@PLT
 mov [rsp+48],rax
 lea rdi,[rip+.Li_options_json]
 call json_tokener_parse@PLT
 mov [rsp+56],rax
 call json_object_new_object@PLT
 mov [rsp+64],rax
 test rax,rax
 jz .Li_start_alloc
 cmp qword ptr [rsp+48],0
 je .Li_start_alloc
 cmp qword ptr [rsp+56],0
 je .Li_start_alloc
 test r14,r14
 jz .Li_start_creation
 mov rdi,[rsp+56]
 lea rsi,[rip+.Li_submission_ids]
 mov rdx,r14
 call .Li_copy
 test eax,eax
 jnz .Li_start_end
.Li_start_creation:
 mov [rsp],rbx
 lea rax,[rip+.Li_initial]
 mov [rsp+8],rax
 lea rax,[rip+.Li_mint]
 mov [rsp+16],rax
 mov qword ptr [rsp+24],0
 mov [rsp+32],r13
 mov rdi,r12
 mov rsi,[rsp+48]
 mov rdx,[rsp+64]
 mov rcx,[rsp+56]
 mov r8,rsp
 mov r9,rbp
 call pg_record_tx_create_task@PLT
 jmp .Li_start_end
.Li_start_alloc:
 mov eax,5
.Li_start_end:
 mov ebp,eax
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov eax,ebp
 END 80
.size pg_durable_inputs_start,.-pg_durable_inputs_start
.globl pg_durable_inputs_apply
.type pg_durable_inputs_apply,@function
pg_durable_inputs_apply:
 BEGIN 112
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,r9
 mov dword ptr [rsp+104],0
 cmp ecx,5
 jne .Li_apply_kind_ready
 mov dword ptr [rsp+104],1
 mov ecx,2
.Li_apply_kind_ready:
 mov [rsp+76],ecx
 mov [rsp+80],r8
 pxor xmm0,xmm0
 movdqu [r14],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 mov qword ptr [rsp+64],0
 mov dword ptr [rsp+72],0
 call json_object_new_array@PLT
 mov [r14],rax
 test rax,rax
 jz .Li_apply_alloc
 IGET r13,head
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+48],rax
 IGET r13,queued
 mov [rsp],rax
 mov qword ptr [rsp+56],0
.Li_apply_reset_scan:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 cmp [rsp+56],rax
 jae .Li_apply_settlement
 mov rdi,[rsp]
 mov rsi,[rsp+56]
 call json_object_array_get_idx@PLT
 mov [rsp+8],rax
 IGET rax,type
 IEQ rax,write
 test eax,eax
 jz .Li_apply_reset_next
 IGET "qword ptr [rsp+8]",draft
 IGET rax,entry
 IGET rax,head
 IEQ rax,self
 test eax,eax
 jz .Li_apply_reset_next
 mov dword ptr [rsp+72],1
.Li_apply_reset_next:
 inc qword ptr [rsp+56]
 jmp .Li_apply_reset_scan
.Li_apply_settlement:
 cmp dword ptr [rsp+76],4
 je .Li_apply_policy
 cmp dword ptr [rsp+76],0
 jne .Li_apply_settle_rows
 cmp dword ptr [rsp+72],0
 je .Li_apply_policy
.Li_apply_settle_rows:
 IGET r13,placed
 mov [rsp+88],rax
 mov qword ptr [rsp+56],0
.Li_apply_settle_loop:
 mov rdi,[rsp+88]
 call json_object_array_length@PLT
 cmp [rsp+56],rax
 jae .Li_apply_policy
 mov rdi,[rsp+88]
 mov rsi,[rsp+56]
 call json_object_array_get_idx@PLT
 mov [rsp+8],rax
 lea rdi,[rip+.Li_done_json]
 cmp dword ptr [rsp+76],1
 je .Li_apply_settle_parse
 lea rdi,[rip+.Li_unanswered_json]
.Li_apply_settle_parse:
 call json_tokener_parse@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Li_apply_alloc
 cmp dword ptr [rsp+76],1
 jne .Li_apply_reason
 cmp qword ptr [rsp+80],0
 je .Li_apply_settle_change
 mov rdi,[rsp+80]
 call json_object_new_int64@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Li_answer]
 mov rdx,rax
 call .Li_set
 test eax,eax
 jnz .Li_apply_end
 jmp .Li_apply_settle_change
.Li_apply_reason:
 lea rdi,[rip+.Li_aborted]
 cmp dword ptr [rsp+76],2
 jne .Li_apply_reason_reset
 lea rdi,[rip+.Li_model_error]
 cmp dword ptr [rsp+104],0
 je .Li_apply_reason_string
 lea rdi,[rip+.Li_restart_limit]
 jmp .Li_apply_reason_string
.Li_apply_reason_reset:
 cmp dword ptr [rsp+76],0
 jne .Li_apply_reason_string
 lea rdi,[rip+.Li_reset]
.Li_apply_reason_string:
 call json_object_new_string@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Li_reason]
 mov rdx,rax
 call .Li_set
 test eax,eax
 jnz .Li_apply_end
.Li_apply_settle_change:
 mov rdi,r12
 mov rsi,[rsp+8]
 mov rdx,[rsp+32]
 call .Li_change
 mov ebp,eax
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov qword ptr [rsp+32],0
 test ebp,ebp
 jnz .Li_apply_saved_error
 inc qword ptr [rsp+56]
 jmp .Li_apply_settle_loop
.Li_apply_policy:
 lea rax,[rip+.Li_aborted]
 mov [rsp+96],rax
 cmp dword ptr [rsp+76],2
 jne .Li_apply_dispatch
 IGET r13,settings
 IGET rax,after_failed
 test rax,rax
 jz .Li_apply_ok
 mov rbp,rax
 IEQ rax,keep
 test eax,eax
 jnz .Li_apply_ok
 IEQ rbp,withdraw
 test eax,eax
 jz .Li_apply_continue
 mov dword ptr [rsp+76],4
 lea rax,[rip+.Li_run_failed]
 mov [rsp+96],rax
 jmp .Li_apply_dispatch
.Li_apply_continue:
 IEQ rbp,continue
 test eax,eax
 jz .Li_apply_invalid
 mov dword ptr [rsp+76],1
.Li_apply_dispatch:
 cmp dword ptr [rsp+76],3
 jae .Li_apply_users_begin
 mov qword ptr [rsp+56],0
.Li_apply_write_loop:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 cmp [rsp+56],rax
 jae .Li_apply_users_begin
 mov rdi,[rsp]
 mov rsi,[rsp+56]
 call json_object_array_get_idx@PLT
 mov [rsp+8],rax
 IGET rax,type
 IEQ rax,write
 test eax,eax
 jz .Li_apply_write_next
 IGET "qword ptr [rsp+8]",draft
 mov [rsp+16],rax
 IGET rax,entry
 IGET rax,head
 mov rbp,rax
 mov rdi,rax
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jnz .Li_apply_stale_check
 mov rdi,rbp
 mov esi,2
 call json_object_is_type@PLT
 test eax,eax
 jz .Li_apply_write_append
.Li_apply_stale_check:
 mov rdi,rbp
 call json_object_get_uint64@PLT
 cmp rax,[rsp+48]
 jae .Li_apply_write_append
 lea rdi,[rip+.Li_stale_json]
 call json_tokener_parse@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Li_apply_alloc
 mov rdi,r12
 mov rsi,[rsp+8]
 mov rdx,rax
 call .Li_change
 jmp .Li_apply_write_changed
.Li_apply_write_append:
 IGET r13,conversation
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdx,rax
 mov rdi,rbx
 mov rsi,r12
 mov rcx,[rsp+16]
 lea r8,[rsp+24]
 call .Li_append
 test eax,eax
 jnz .Li_apply_end
 IGET "qword ptr [rsp+24]",head
 test rax,rax
 jz .Li_apply_write_placed
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+48],rax
 IGET "qword ptr [rsp+16]",entry
 IGET rax,head
 IEQ rax,self
 test eax,eax
 jz .Li_apply_write_placed
 mov dword ptr [r14+8],1
.Li_apply_write_placed:
 call .Li_apply_place
.Li_apply_write_changed:
 test eax,eax
 jnz .Li_apply_end
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov qword ptr [rsp+24],0
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov qword ptr [rsp+32],0
.Li_apply_write_next:
 inc qword ptr [rsp+56]
 jmp .Li_apply_write_loop
.Li_apply_users_begin:
 mov qword ptr [rsp+56],0
.Li_apply_user_loop:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 cmp [rsp+56],rax
 jae .Li_apply_ok
 mov rdi,[rsp]
 mov rsi,[rsp+56]
 call json_object_array_get_idx@PLT
 mov [rsp+8],rax
 IGET rax,type
 IEQ rax,input
 test eax,eax
 jz .Li_apply_user_next
 cmp dword ptr [rsp+76],3
 jae .Li_apply_withdraw
 IGET "qword ptr [rsp+8]",draft
 mov [rsp+16],rax
 IGET rax,when_busy
 IEQ rax,steer
 test eax,eax
 jz .Li_apply_follow
 mov ebp,64
 IGET r13,settings
 IGET rax,steering_mode
 jmp .Li_apply_count
.Li_apply_follow:
 cmp dword ptr [rsp+76],0
 jne .Li_apply_follow_allowed
 cmp dword ptr [rsp+72],0
 je .Li_apply_user_next
.Li_apply_follow_allowed:
 mov ebp,68
 IGET r13,settings
 IGET rax,follow_mode
.Li_apply_count:
 IEQ rax,all
 test eax,eax
 jnz .Li_apply_user_append
 cmp dword ptr [rsp+rbp],0
 jne .Li_apply_user_next
.Li_apply_user_append:
 inc dword ptr [rsp+rbp]
 IGET r13,conversation
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdx,rax
 mov rdi,rbx
 mov rsi,r12
 mov rcx,[rsp+16]
 lea r8,[rsp+24]
 call .Li_append
 test eax,eax
 jnz .Li_apply_end
 call .Li_apply_place
 test eax,eax
 jnz .Li_apply_end
 IGET "qword ptr [rsp+8]",id
 mov rdi,[r14]
 mov rsi,rax
 call .Li_push
 jmp .Li_apply_user_changed
.Li_apply_withdraw:
 lea rdi,[rip+.Li_unanswered_json]
 call json_tokener_parse@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Li_apply_alloc
 mov rdi,[rsp+96]
 call json_object_new_string@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Li_reason]
 mov rdx,rax
 call .Li_set
 test eax,eax
 jnz .Li_apply_end
 mov rdi,r12
 mov rsi,[rsp+8]
 mov rdx,[rsp+32]
 call .Li_change
.Li_apply_user_changed:
 test eax,eax
 jnz .Li_apply_end
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov qword ptr [rsp+24],0
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov qword ptr [rsp+32],0
.Li_apply_user_next:
 inc qword ptr [rsp+56]
 jmp .Li_apply_user_loop
.Li_apply_ok:
 xor eax,eax
 jmp .Li_apply_end
.Li_apply_alloc:
 mov eax,5
 jmp .Li_apply_end
.Li_apply_invalid:
 mov eax,4
 jmp .Li_apply_end
.Li_apply_saved_error:
 mov eax,ebp
.Li_apply_end:
 mov ebp,eax
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov eax,ebp
 END 112
.size pg_durable_inputs_apply,.-pg_durable_inputs_apply
/* Local subroutine borrows the apply frame below its return address. */
.Li_apply_place:
 sub rsp,8
 lea rdi,[rip+.Li_place_json]
 call json_tokener_parse@PLT
 mov [rsp+48],rax
 test rax,rax
 jz .Li_apply_place_alloc
 IGET "qword ptr [rsp+40]",id
 mov rdi,[rsp+48]
 lea rsi,[rip+.Li_entry]
 mov rdx,rax
 call .Li_copy
 test eax,eax
 jnz .Li_apply_place_end
 mov rdi,r12
 mov rsi,[rsp+24]
 mov rdx,[rsp+48]
 call .Li_change
 jmp .Li_apply_place_end
.Li_apply_place_alloc:
 mov eax,5
.Li_apply_place_end:
 add rsp,8
 ret
.Li_text_valid:
 BEGIN 16
 mov rbx,rdi
 call json_object_get_string_len@PLT
 cmp eax,4096
 jae .Li_text_no
 mov r12,rax
 mov rdi,rbx
 call json_object_get_string@PLT
 mov r13,rax
 xor r14d,r14d
 test r12,r12
 jz .Li_text_yes
.Li_text_loop:
 lea rdi,[r13+r14]
 mov rsi,r12
 sub rsi,r14
 mov rdx,rsp
 call utf8proc_iterate@PLT
 test rax,rax
 jle .Li_text_no
 cmp dword ptr [rsp],0
 je .Li_text_no
 add r14,rax
 cmp r14,r12
 jb .Li_text_loop
.Li_text_yes:
 mov eax,1
 jmp .Li_text_end
.Li_text_no:
 xor eax,eax
.Li_text_end:
 END 16
.Li_has_reset:
 BEGIN 16
 IGET rdi,queued
 mov rbx,rax
 xor r12d,r12d
.Li_has_reset_loop:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Li_has_reset_no
 mov rdi,rbx
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov r13,rax
 IGET rax,type
 IEQ rax,write
 test eax,eax
 jz .Li_has_reset_next
 IGET r13,draft
 IGET rax,entry
 IGET rax,head
 IEQ rax,self
 test eax,eax
 jnz .Li_has_reset_end
.Li_has_reset_next:
 inc r12
 jmp .Li_has_reset_loop
.Li_has_reset_no:
 xor eax,eax
.Li_has_reset_end:
 END 16
.Li_display_only:
 BEGIN 16
 mov rbx,rdi
 xor r12d,r12d
.Li_display_loop:
 lea rax,[rip+.Li_display_fields]
 movsxd rsi,dword ptr [rax+r12*4]
 add rsi,rax
 mov rdi,rbx
 mov rdx,rsp
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Li_display_no
 inc r12
 cmp r12,3
 jb .Li_display_loop
 mov eax,1
 jmp .Li_display_end
.Li_display_no:
 xor eax,eax
.Li_display_end:
 END 16
.globl pg_durable_inputs_submit
.type pg_durable_inputs_submit,@function
pg_durable_inputs_submit:
 BEGIN 224
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rdi,rsp
 xor eax,eax
 mov ecx,28
 cld
 rep stosq
 pxor xmm0,xmm0
 movdqu [r14],xmm0
 mov qword ptr [r14+16],0
 mov ebp,4
 cmp dword ptr [rbx+44],1
 jne .Li_submit_closed
 cmp dword ptr [rbx+40],0
 jne .Li_submit_busy
 mov rax,[rbx]
 cmp dword ptr [rax+172],1
 jne .Li_submit_closed
 mov rdi,r13
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Li_submit_end
 IGET r13,type
 mov [rsp+192],rax
 IEQ rax,input
 test eax,eax
 jnz .Li_submit_validate_input
 IEQ "qword ptr [rsp+192]",write
 test eax,eax
 jz .Li_submit_end
 IGET r13,entry
 test rax,rax
 jz .Li_submit_end
 jmp .Li_submit_begin
.Li_submit_validate_input:
.Li_submit_custom:
 IGET r13,entry
 test rax,rax
 jz .Li_submit_begin
 IGET rax,kind
 mov [rsp+184],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Li_submit_end
 mov rdi,[rsp+184]
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Li_submit_end
 mov rdi,[rsp+184]
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Li_reserved]
 mov edx,6
 call strncmp@PLT
 test eax,eax
 jz .Li_submit_end
.Li_submit_begin:
 mov dword ptr [rbx+40],1
 mov rax,[rbx+8]
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+48]
 mov rcx,[rax]
 mov r8,[rax+8]
 call pg_durable_session_begin@PLT
 test eax,eax
 jnz .Li_submit_busy_unlock
 mov dword ptr [rsp+212],1
 IGET r13,request_id
 test rax,rax
 jz .Li_submit_prepare
 mov [rsp+184],rax
 mov [rsp+128],r12
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+136],rax
 mov rdi,[rsp+184]
 call json_object_get_string_len@PLT
 mov [rsp+144],rax
 mov rdi,rsp
 mov rsi,[rbx+32]
 lea rdx,[rsp+128]
 lea rcx,[rsp+112]
 call pg_record_tx_submission_by_request@PLT
 cmp eax,7
 je .Li_submit_prepare
 test eax,eax
 jnz .Li_submit_error
 IGET "qword ptr [rsp+112]",type
 mov rdi,rax
 mov rsi,[rsp+192]
 call json_object_equal@PLT
 test eax,eax
 jz .Li_submit_unlock
 IGET "qword ptr [rsp+112]",id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [r14],rax
 IGET "qword ptr [rsp+112]",status
 mov [rsp+184],rax
 mov dword ptr [r14+16],2
 IEQ rax,queued
 test eax,eax
 jz .Li_submit_known_placed
 mov dword ptr [r14+16],0
 jmp .Li_submit_known
.Li_submit_known_placed:
 IEQ "qword ptr [rsp+184]",placed
 test eax,eax
 jz .Li_submit_known
 mov dword ptr [r14+16],1
.Li_submit_known:
 xor ebp,ebp
 jmp .Li_submit_unlock
.Li_submit_prepare:
 IEQ "qword ptr [rsp+192]",input
 test eax,eax
 jz .Li_submit_prepare_snapshot
 IGET r13,content
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Li_submit_unlock
 IGET r13,content
 mov rdi,rax
 call .Li_text_valid
 test eax,eax
 jz .Li_submit_unlock
 IGET r13,when_busy
 test rax,rax
 jz .Li_submit_prepare_snapshot
 mov [rsp+184],rax
 IEQ rax,steer
 test eax,eax
 jnz .Li_submit_prepare_snapshot
 IEQ "qword ptr [rsp+184]",follow_up
 test eax,eax
 jnz .Li_submit_prepare_snapshot
 IEQ "qword ptr [rsp+184]",reject
 test eax,eax
 jz .Li_submit_unlock
.Li_submit_prepare_snapshot:
 mov rdi,rbx
 mov rsi,rsp
 mov rdx,r12
 lea rcx,[rsp+96]
 call pg_durable_inputs_prepare@PLT
 test eax,eax
 jnz .Li_submit_error
 IGET "qword ptr [rsp+96]",busy
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov [rsp+208],eax
 test eax,eax
 jz .Li_submit_create
 IEQ "qword ptr [rsp+192]",input
 test eax,eax
 jz .Li_submit_create
 IGET r13,when_busy
 IEQ rax,reject
 test eax,eax
 jnz .Li_submit_busy_unlock
.Li_submit_create:
 IEQ "qword ptr [rsp+192]",write
 test eax,eax
 jz .Li_submit_create_record
 IGET r13,entry
 mov rdi,rax
 call .Li_display_only
 test eax,eax
 jz .Li_submit_create_record
 mov rdi,[rsp+96]
 call .Li_has_reset
 test eax,eax
 jnz .Li_submit_create_record
 mov dword ptr [rsp+216],1
.Li_submit_create_record:
 lea rdi,[rip+.Li_submission_json]
 call json_tokener_parse@PLT
 mov [rsp+152],rax
 test rax,rax
 jz .Li_submit_alloc
 mov rdi,r12
 call json_object_new_int64@PLT
 mov rdi,[rsp+152]
 lea rsi,[rip+.Li_conversation]
 mov rdx,rax
 call .Li_set
 test eax,eax
 jnz .Li_submit_error
 mov rdi,[rsp+152]
 lea rsi,[rip+.Li_type]
 mov rdx,[rsp+192]
 call .Li_copy
 test eax,eax
 jnz .Li_submit_error
 mov rdi,[rsp+152]
 lea rsi,[rip+.Li_draft]
 mov rdx,r13
 call .Li_copy
 test eax,eax
 jnz .Li_submit_error
 IGET r13,request_id
 test rax,rax
 jz .Li_submit_mint
 mov rdi,[rsp+152]
 lea rsi,[rip+.Li_request_id]
 mov rdx,rax
 call .Li_copy
 test eax,eax
 jnz .Li_submit_error
.Li_submit_mint:
 mov rax,[rbx+16]
 mov rax,[rax]
 mov rdi,rsp
 mov rsi,[rsp+152]
 mov rdx,[rax+32]
 lea rcx,[rsp+104]
 call pg_record_tx_create_submission@PLT
 test eax,eax
 jnz .Li_submit_error
 IGET "qword ptr [rsp+104]",id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [r14],rax
 cmp dword ptr [rsp+216],0
 je .Li_submit_enqueue
 mov rdi,rbx
 mov rsi,rsp
 mov rdx,r12
 mov rcx,r13
 lea r8,[rsp+120]
 call .Li_append
 test eax,eax
 jnz .Li_submit_error
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov qword ptr [rsp+152],0
 lea rdi,[rip+.Li_place_json]
 call json_tokener_parse@PLT
 mov [rsp+152],rax
 test rax,rax
 jz .Li_submit_alloc
 IGET "qword ptr [rsp+120]",id
 mov rdi,[rsp+152]
 lea rsi,[rip+.Li_entry]
 mov rdx,rax
 call .Li_copy
 test eax,eax
 jnz .Li_submit_error
 mov rdi,rsp
 mov rsi,[rsp+104]
 mov rdx,[rsp+152]
 call .Li_change
 test eax,eax
 jnz .Li_submit_error
 mov dword ptr [r14+16],2
 jmp .Li_submit_commit
.Li_submit_enqueue:
 IGET "qword ptr [rsp+96]",queued
 mov rdi,rax
 mov rsi,[rsp+104]
 call .Li_push
 test eax,eax
 jnz .Li_submit_error
 cmp dword ptr [rsp+208],0
 jne .Li_submit_commit
 mov rdi,rbx
 mov rsi,rsp
 mov rdx,[rsp+96]
 mov ecx,1
 xor r8d,r8d
 lea r9,[rsp+160]
 call pg_durable_inputs_apply@PLT
 test eax,eax
 jnz .Li_submit_error
 IEQ "qword ptr [rsp+192]",write
 test eax,eax
 jz .Li_submit_selected
 mov dword ptr [r14+16],2
.Li_submit_selected:
 mov qword ptr [rsp+184],0
.Li_submit_selected_loop:
 mov rdi,[rsp+160]
 call json_object_array_length@PLT
 cmp [rsp+184],rax
 jae .Li_submit_start
 mov rdi,[rsp+160]
 mov rsi,[rsp+184]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[r14]
 jne .Li_submit_selected_next
 mov dword ptr [r14+16],1
.Li_submit_selected_next:
 inc qword ptr [rsp+184]
 jmp .Li_submit_selected_loop
.Li_submit_start:
 mov rdi,[rsp+160]
 call json_object_array_length@PLT
 test rax,rax
 jz .Li_submit_commit
 mov rdi,rbx
 mov rsi,rsp
 mov rdx,r12
 mov rcx,[rsp+160]
 lea r8,[r14+8]
 call pg_durable_inputs_start@PLT
 test eax,eax
 jnz .Li_submit_error
.Li_submit_commit:
 mov rdi,[rbx]
 xor esi,esi
 xor edx,edx
 xor ecx,ecx
 call pg_durable_session_settle@PLT
 test eax,eax
 jnz .Li_submit_storage
 xor ebp,ebp
 mov dword ptr [rsp+212],0
 jmp .Li_submit_unlock
.Li_submit_closed:
 mov ebp,7
 jmp .Li_submit_end
.Li_submit_storage:
 mov ebp,6
 jmp .Li_submit_unlock
.Li_submit_alloc:
 mov ebp,5
 jmp .Li_submit_unlock
.Li_submit_error:
 mov ebp,eax
 jmp .Li_submit_unlock
.Li_submit_busy:
 mov ebp,3
 jmp .Li_submit_end
.Li_submit_busy_unlock:
 mov ebp,3
.Li_submit_unlock:
 cmp dword ptr [rsp+212],0
 je .Li_submit_unlock_flag
 mov rdi,[rbx]
 call pg_durable_session_abort@PLT
.Li_submit_unlock_flag:
 mov dword ptr [rbx+40],0
.Li_submit_end:
 test ebp,ebp
 jz .Li_submit_release
 pxor xmm0,xmm0
 movdqu [r14],xmm0
 mov qword ptr [r14+16],0
.Li_submit_release:
 mov rdi,[rsp+96]
 call json_object_put@PLT
 mov rdi,[rsp+104]
 call json_object_put@PLT
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov rdi,[rsp+120]
 call json_object_put@PLT
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov rdi,[rsp+160]
 call json_object_put@PLT
 mov eax,ebp
 END 224
.size pg_durable_inputs_submit,.-pg_durable_inputs_submit
.globl pg_durable_inputs_status
.type pg_durable_inputs_status,@function
pg_durable_inputs_status:
 BEGIN 16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rdx],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov rdi,rsp
 mov rsi,[rbx]
 call pg_durable_read_on_line_begin@PLT
 test eax,eax
 jnz .Li_status_closed
 mov rax,[rbx+8]
 mov rdi,[rax]
 mov esi,3
 mov rdx,r12
 mov rcx,r13
 call [rax+8]
 mov ebp,eax
 mov rdi,rsp
 call pg_durable_read_on_line_end@PLT
 test ebp,ebp
 jz .Li_status_end
 cmp ebp,1
 mov ebp,6
 jne .Li_status_clear
 mov ebp,2
.Li_status_clear:
 mov rdi,[r13]
 call json_object_put@PLT
 mov qword ptr [r13],0
 jmp .Li_status_end
.Li_status_closed:
 mov ebp,7
 mov rax,[rbx]
 cmp dword ptr [rax+172],1
 jne .Li_status_end
 mov ebp,3
.Li_status_end:
 mov eax,ebp
 END 16
.size pg_durable_inputs_status,.-pg_durable_inputs_status
.globl pg_durable_inputs_poll
.type pg_durable_inputs_poll,@function
pg_durable_inputs_poll:
 push rbx
 push r12
 sub rsp,8
 mov r12,rdi
 mov rbx,rdx
 call pg_durable_inputs_status@PLT
 test eax,eax
 jnz .Li_poll_end
 IGET "qword ptr [rbx]",status
 mov [rsp],rax
 IEQ rax,queued
 test eax,eax
 jnz .Li_poll_pending
 IEQ "qword ptr [rsp]",placed
 test eax,eax
 jnz .Li_poll_pending
 xor eax,eax
 jmp .Li_poll_end
.Li_poll_pending:
 mov rax,[r12]
 cmp dword ptr [rax+172],1
 je .Li_poll_wait
 mov rdi,[rbx]
 call json_object_put@PLT
 mov qword ptr [rbx],0
 mov eax,7
 jmp .Li_poll_end
.Li_poll_wait:
 mov eax,1
.Li_poll_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.size pg_durable_inputs_poll,.-pg_durable_inputs_poll
.globl pg_durable_inputs_withdraw
.type pg_durable_inputs_withdraw,@function
pg_durable_inputs_withdraw:
 BEGIN 128
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov dword ptr [rcx],3
 mov rdi,rsp
 xor eax,eax
 mov ecx,16
 cld
 rep stosq
 mov rax,[rbx+8]
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+48]
 mov rcx,[rax]
 mov r8,[rax+8]
 call pg_durable_session_begin@PLT
 test eax,eax
 jnz .Li_withdraw_error
 mov dword ptr [rsp+112],1
 mov rdi,rsp
 mov esi,3
 mov rdx,r13
 lea rcx,[rsp+96]
 call pg_record_tx_read@PLT
 cmp eax,7
 je .Li_withdraw_ok
 test eax,eax
 jnz .Li_withdraw_error
 test r12,r12
 jz .Li_withdraw_status
 IGET "qword ptr [rsp+96]",conversation
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,r12
 jne .Li_withdraw_ok
.Li_withdraw_status:
 IGET "qword ptr [rsp+96]",status
 mov r13,rax
 IEQ rax,queued
 test eax,eax
 jnz .Li_withdraw_queued
 mov dword ptr [r14],2
 IEQ r13,placed
 test eax,eax
 jz .Li_withdraw_ok
 mov dword ptr [r14],1
 jmp .Li_withdraw_ok
.Li_withdraw_queued:
 lea rdi,[rip+.Li_unanswered_json]
 call json_tokener_parse@PLT
 mov [rsp+104],rax
 test rax,rax
 jz .Li_withdraw_alloc
 mov rdi,rsp
 mov rsi,[rsp+96]
 mov rdx,rax
 call .Li_change
 test eax,eax
 jnz .Li_withdraw_error
 mov rdi,[rbx]
 xor esi,esi
 xor edx,edx
 xor ecx,ecx
 call pg_durable_session_settle@PLT
 test eax,eax
 jnz .Li_withdraw_error
 mov dword ptr [r14],0
.Li_withdraw_ok:
 xor eax,eax
 jmp .Li_withdraw_error
.Li_withdraw_alloc:
 mov eax,5
.Li_withdraw_error:
 mov ebp,eax
 cmp dword ptr [rsp+112],0
 je .Li_withdraw_release
 mov rdi,[rbx]
 call pg_durable_session_abort@PLT
.Li_withdraw_release:
 mov rdi,[rsp+96]
 call json_object_put@PLT
 mov rdi,[rsp+104]
 call json_object_put@PLT
 mov eax,ebp
 END 128
.size pg_durable_inputs_withdraw,.-pg_durable_inputs_withdraw
.section .rodata
.Li_display_fields: .long .Li_model-.Li_display_fields,.Li_head-.Li_display_fields,.Li_edits-.Li_display_fields
.Li_plan_json: .asciz "{\"conversationId\":0,\"queued\":[],\"placed\":[],\"busy\":false,\"head\":0,\"settings\":{}}"
.Li_user_json: .asciz "{\"kind\":\"pangu.user\",\"model\":[{\"role\":\"user\",\"content\":\"\"}]}"
.Li_definition_json: .asciz "{\"name\":\"pangu.generation\",\"version\":1}"
.Li_options_json: .asciz "{\"ownership\":{\"kind\":\"conversation\"}}"
.Li_value: .asciz "value"
.Li_submission_write_json: .asciz "{\"type\":\"submission\",\"value\":{}}"
.Li_submission_json: .asciz "{\"conversationId\":0,\"type\":\"input\",\"status\":\"queued\",\"draft\":{}}"
.Li_done_json: .asciz "{\"status\":\"done\"}"
.Li_place_json: .asciz "{\"status\":\"placed\",\"entry\":0}"
.Li_unanswered_json: .asciz "{\"status\":\"unanswered\",\"reason\":\"aborted\"}"
.Li_stale_json: .asciz "{\"status\":\"unanswered\",\"reason\":\"stale\"}"
.Li_conversation: .asciz "conversationId"
.Li_submission_ids: .asciz "submissionIds"
.Li_queued: .asciz "queued"
.Li_placed: .asciz "placed"
.Li_done: .asciz "done"
.Li_unanswered: .asciz "unanswered"
.Li_busy: .asciz "busy"
.Li_generation: .asciz "pangu.generation"
.Li_state: .asciz "state"
.Li_status: .asciz "status"
.Li_terminal: .asciz "terminal"
.Li_background: .asciz "background"
.Li_head: .asciz "head"
.Li_settings: .asciz "settings"
.Li_draft: .asciz "draft"
.Li_type: .asciz "type"
.Li_input: .asciz "input"
.Li_write: .asciz "write"
.Li_kind: .asciz "kind"
.Li_reserved: .asciz "pangu."
.Li_model: .asciz "model"
.Li_content: .asciz "content"
.Li_entry: .asciz "entry"
.Li_data: .asciz "data"
.Li_edits: .asciz "edits"
.Li_timestamp: .asciz "timestamp"
.Li_id: .asciz "id"
.Li_self: .asciz "self"
.Li_answer: .asciz "answer"
.Li_reason: .asciz "reason"
.Li_aborted: .asciz "aborted"
.Li_reset: .asciz "reset"
.Li_model_error: .asciz "model_error"
.Li_run_failed: .asciz "run_failed"
.Li_after_failed: .asciz "afterFailedRun"
.Li_keep: .asciz "keep"
.Li_withdraw: .asciz "withdraw"
.Li_continue: .asciz "continue"
.Li_when_busy: .asciz "whenBusy"
.Li_steer: .asciz "steer"
.Li_follow_up: .asciz "followUp"
.Li_reject: .asciz "reject"
.Li_steering_mode: .asciz "steeringMode"
.Li_follow_mode: .asciz "followUpMode"
.Li_all: .asciz "all"
.Li_request_id: .asciz "requestId"
.section .note.GNU-stack,"",@progbits

.section .rodata
.Li_restart_limit: .asciz "restart_limit"
