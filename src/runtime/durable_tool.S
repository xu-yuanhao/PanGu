.intel_syntax noprefix
.macro DGET object,key
 mov rdi,\object
 lea rsi,[rip+.Ldt_\key]
 call json_object_object_get@PLT
.endm
.macro DREF object,key,value
 mov rdi,\value
 call json_object_get@PLT
 mov rdx,rax
 mov rdi,\object
 lea rsi,[rip+.Ldt_\key]
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_end
.endm
.text
.Ldt_set:
 test rdx,rdx
 jz .Ldt_set_missing
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Ldt_set_return
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,5
.Ldt_set_return:
 pop rbx
 ret
.Ldt_set_missing:
 mov eax,5
 ret
.Ldt_initial:
 push rbx
 mov rbx,rdx
 lea rdi,[rip+.Ldt_initial_json]
 call json_tokener_parse@PLT
 mov [rbx],rax
 test rax,rax
 setz al
 movzx eax,al
 pop rbx
 ret
.globl pg_durable_tool_init
.type pg_durable_tool_init,@function
pg_durable_tool_init:
 mov eax,1
 test rdi,rdi
 jz .Ldt_init_end
 test rsi,rsi
 jz .Ldt_init_end
 cmp qword ptr [rsi+8],0
 je .Ldt_init_end
 cmp qword ptr [rsi+16],0
 je .Ldt_init_end
 cmp qword ptr [rsi+24],0
 je .Ldt_init_end
 mov [rdi],rsi
 mov qword ptr [rdi+176],0
 lea rax,[rip+.Ldt_task_name]
 mov [rdi+8],rax
 mov qword ptr [rdi+16],10
 mov qword ptr [rdi+24],2
 mov [rdi+32],rdi
 lea rax,[rip+.Ldt_initial]
 mov [rdi+40],rax
 lea rax,[rdi+96]
 mov [rdi+48],rax
 mov qword ptr [rdi+56],2
 lea rax,[rip+.Ldt_abort]
 mov [rdi+64],rax
 lea rax,[rip+.Ldt_cleanup]
 mov [rdi+72],rax
 mov qword ptr [rdi+80],0
 lea rax,[rip+.Ldt_cutoff]
 mov [rdi+88],rax
 lea rax,[rip+.Ldt_call_phase]
 mov [rdi+96],rax
 mov qword ptr [rdi+104],4
 mov [rdi+112],rdi
 lea rax,[rip+.Ldt_poll]
 mov [rdi+120],rax
 lea rax,[rip+.Ldt_cleanup]
 mov [rdi+128],rax
 lea rax,[rip+.Ldt_execute_phase]
 mov [rdi+136],rax
 mov qword ptr [rdi+144],7
 mov [rdi+152],rdi
 lea rax,[rip+.Ldt_poll]
 mov [rdi+160],rax
 lea rax,[rip+.Ldt_cleanup]
 mov [rdi+168],rax
 xor eax,eax
.Ldt_init_end:
 ret
.size pg_durable_tool_init,.-pg_durable_tool_init

/* Operation owns detached call/arguments and service lease; output is bounded. */
.Ldt_open:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rsi
 mov r12,rdi
 mov r13,rdx
 mov qword ptr [rsp],0
 mov edi,1
 mov esi,416
 call calloc@PLT
 mov r14,rax
 test rax,rax
 jz .Ldt_open_alloc
 mov [rax],r12
 mov [rax+8],rbx
 mov [rbx+56],rax
 mov edi,51201
 call malloc@PLT
 mov [r14+128],rax
 test rax,rax
 jz .Ldt_open_alloc
 mov qword ptr [r14+248],51200
 mov qword ptr [r14+256],51200
 mov qword ptr [r14+264],2000
 mov dword ptr [r14+272],0
 mov qword ptr [r14+320],51200
 mov qword ptr [r14+328],2000
 DGET r13,input
 mov r12,rax
 DGET rax,kind
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Ldt_open_invalid
 mov rbp,rax
 mov rdi,rax
 lea rsi,[rip+.Ldt_nested]
 call strcmp@PLT
 test eax,eax
 jnz .Ldt_open_model
 DGET r12,call
 mov r13,rax
 test rax,rax
 jz .Ldt_open_invalid
 jmp .Ldt_open_found
.Ldt_open_model:
 mov rdi,rbp
 lea rsi,[rip+.Ldt_model]
 call strcmp@PLT
 test eax,eax
 jnz .Ldt_open_invalid
 DGET r12,assistant
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ldt_open_invalid
 mov rdx,rax
 mov rax,[rbx+8]
 mov rdi,[rax]
 mov esi,1
 mov rcx,rsp
 call [rax+8]
 test eax,eax
 jnz .Ldt_open_invalid
 cmp qword ptr [rsp],0
 je .Ldt_open_invalid
 DGET "qword ptr [rsp]",kind
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Ldt_open_invalid
 mov rdi,rax
 lea rsi,[rip+.Ldt_assistant_kind]
 call strcmp@PLT
 test eax,eax
 jnz .Ldt_open_invalid
 DGET "qword ptr [rsp]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rbp,rax
 DGET "qword ptr [rbx+24]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,rbp
 jne .Ldt_open_invalid
 DGET "qword ptr [rsp]",model_key
 mov r13,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldt_open_invalid
 mov rdi,r13
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov r13,rax
 DGET rax,role
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Ldt_open_invalid
 mov rdi,rax
 lea rsi,[rip+.Ldt_assistant_role]
 call strcmp@PLT
 test eax,eax
 jnz .Ldt_open_invalid
 DGET r13,content
 mov [rsp+8],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldt_open_invalid
 xor ebp,ebp
.Ldt_open_call:
 mov rdi,[rsp+8]
 call json_object_array_length@PLT
 cmp rbp,rax
 jae .Ldt_open_invalid
 mov rdi,[rsp+8]
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov r13,rax
 DGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Ldt_open_next
 mov rdi,rax
 lea rsi,[rip+.Ldt_tool_call]
 call strcmp@PLT
 test eax,eax
 jnz .Ldt_open_next
 DGET r12,call_id
 mov [r14+120],rax
 DGET r13,id
 mov rdi,rax
 mov rsi,[r14+120]
 call json_object_equal@PLT
 test eax,eax
 jnz .Ldt_open_found
.Ldt_open_next:
 inc rbp
 jmp .Ldt_open_call
.Ldt_open_found:
 mov rdi,r13
 lea rsi,[r14+16]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ldt_open_alloc
 mov rax,[r14+16]
 DGET rax,name
 mov r13,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldt_open_invalid
 mov rdi,r13
 call json_object_get_string@PLT
 test rax,rax
 jz .Ldt_open_invalid
 mov [r14+88],rax
 mov rdi,rax
 call strlen@PLT
 test rax,rax
 jz .Ldt_open_invalid
 mov [r14+96],rax
 DGET "qword ptr [r14+16]",arguments
 mov r13,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldt_open_invalid
 DGET "qword ptr [rbx+32]",state
 DGET rax,checkpoint
 mov r12,rax
 DGET rax,phase
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Ldt_execute_phase]
 call strcmp@PLT
 test eax,eax
 jnz .Ldt_open_args
 mov dword ptr [r14+156],1
 DGET r12,arguments
 mov r13,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldt_open_invalid
 DGET r12,replay
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Ldt_open_args
 mov rdi,rax
 lea rsi,[rip+.Ldt_safe]
 call strcmp@PLT
 test eax,eax
 setz al
 movzx eax,al
 mov [r14+160],eax
.Ldt_open_args:
 mov rdi,r13
 call json_object_get@PLT
 mov [r14+24],rax
 mov rdi,rax
 mov esi,256
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Ldt_open_alloc
 mov [r14+104],rax
 mov rdi,rax
 call strlen@PLT
 mov [r14+112],rax
 DGET "qword ptr [rbx+32]",input
 DGET rax,offered_tools
 test rax,rax
 jz .Ldt_open_offered
 mov r12,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldt_open_invalid
 DGET "qword ptr [r14+16]",name
 mov r13,rax
 xor ebp,ebp
.Ldt_open_offered_each:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp rbp,rax
 jae .Ldt_open_unoffered
 mov rdi,r12
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov rsi,r13
 call json_object_equal@PLT
 test eax,eax
 jnz .Ldt_open_offered
 inc rbp
 jmp .Ldt_open_offered_each
.Ldt_open_unoffered:
 mov dword ptr [r14+232],1
.Ldt_open_offered:
 xor ebp,ebp
 jmp .Ldt_open_end
.Ldt_open_alloc:
 mov ebp,5
 jmp .Ldt_open_end
.Ldt_open_invalid:
 mov ebp,4
.Ldt_open_end:
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

.Ldt_cleanup:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rsi
 mov r12,[rsi+56]
 test r12,r12
 jz .Ldt_cleanup_end
 cmp dword ptr [r12+368],0
 je .Ldt_cleanup_progress_done
 mov rdi,r12
 call .Ldt_progress_cleanup_flush
 test eax,eax
 jz .Ldt_cleanup_progress_done
 mov rax,[rbx+8]
 cmp qword ptr [rax+48],0
 je .Ldt_cleanup_progress_done
 mov rdi,[rax]
 mov rsi,[rbx+16]
 mov edx,6
 call [rax+48]
.Ldt_cleanup_progress_done:
 mov dword ptr [r12+196],0
 mov rax,[r12]
 mov rax,[rax+176]
 test rax,rax
 jz .Ldt_cleanup_hook_json
 cmp qword ptr [r12+200],0
 je .Ldt_cleanup_hook_json
 mov rdi,[rax]
 mov rsi,[r12+200]
 call [rax+16]
 mov qword ptr [r12+200],0
.Ldt_cleanup_hook_json:
 mov rdi,[r12+208]
 call json_object_put@PLT
 mov rdi,[r12+216]
 call json_object_put@PLT
 cmp dword ptr [r12+148],0
 je .Ldt_cleanup_json
 mov rax,[r12+80]
 /* Unsettled async ownership returns to the host drain on phase cancellation. */
 cmp dword ptr [r12+172],0
 jne .Ldt_cleanup_release
 mov rax,[r12+72]
 test rax,rax
 jz .Ldt_cleanup_release
 mov rdi,[r12+32]
 call rax
.Ldt_cleanup_release:
 mov rax,[r12]
 mov rax,[rax]
 mov rdi,[rax]
 mov rsi,[rbx+16]
 lea rdx,[r12+32]
 call [rax+16]
.Ldt_cleanup_json:
 mov rdi,[r12+184]
 call json_object_put@PLT
 mov rdi,[r12+16]
 call json_object_put@PLT
 mov rdi,[r12+24]
 call json_object_put@PLT
 lea rdi,[r12+288]
 call pg_output_buffer_close@PLT
 mov rdi,[r12+128]
 call free@PLT
 mov rdi,[r12+400]
 call pg_json_schema_close@PLT
 mov rdi,r12
 call free@PLT
 mov qword ptr [rbx+56],0
.Ldt_cleanup_end:
 add rsp,8
 pop r12
 pop rbx
 ret

/* Build and commit final transcript Entry and task receipt together. */
.Ldt_stage:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov r12,rsi
 mov r13,rcx
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 mov [rsp+48],rdx
 DGET "qword ptr [rbx+184]",usage
 mov [rsp+56],rax
 DGET "qword ptr [rbx+16]",name
 mov rdi,rax
 call json_object_get_string@PLT
 mov r8,rax
 mov rdi,[rbx+8]
 mov rsi,[rsp+48]
 mov rax,[rbx]
 mov rax,[rax]
 mov rdx,[rax+24]
 lea rcx,[rip+.Ldt_tools_bucket]
 mov r9,[rsp+56]
 call pg_usage_record@PLT
 test eax,eax
 jnz .Ldt_stage_end
 lea rdi,[rip+.Ldt_entry_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Ldt_stage_alloc
 DGET rax,model_key
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov r14,rax
 DGET "qword ptr [rbx+16]",id
 mov rbp,rax
 DREF r14,tool_call_id,rbp
 DGET "qword ptr [rbx+16]",name
 mov rbp,rax
 DREF r14,tool_name,rbp
 mov rax,[rbx+8]
 mov rax,[rax+8]
 mov rdi,[rax]
 call [rax+40]
 call json_object_new_double@PLT
 mov rdx,rax
 mov rdi,r14
 lea rsi,[rip+.Ldt_timestamp]
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_end
 mov edi,[rbx+164]
 call json_object_new_boolean@PLT
 mov rdx,rax
 mov rdi,r14
 lea rsi,[rip+.Ldt_is_error]
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_end
 DGET r14,content
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rbp,rax
 mov rdi,[rbx+128]
 mov esi,[rbx+136]
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Ldt_stage_alloc
 mov rdi,rbp
 lea rsi,[rip+.Ldt_text]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_end
 cmp qword ptr [rbx+136],0
 jne .Ldt_stage_content_ready
 test dword ptr [rbx+372],2
 jz .Ldt_stage_content_ready
 DGET r14,content
 mov rdi,rax
 xor esi,esi
 mov edx,1
 call json_object_array_del_idx@PLT
 test eax,eax
 jnz .Ldt_stage_alloc
.Ldt_stage_content_ready:
 DGET "qword ptr [rsp]",data
 mov rbp,rax
 DREF rbp,arguments,"qword ptr [rbx+24]"
 lea rdi,[rip+.Ldt_completed]
 cmp dword ptr [rbx+164],0
 je .Ldt_data_status
 lea rdi,[rip+.Ldt_result_error]
 cmp dword ptr [rbx+192],0
 je .Ldt_data_abort
 lea rdi,[rip+.Ldt_interrupted]
 cmp dword ptr [rbx+192],2
 jne .Ldt_data_abort
 lea rdi,[rip+.Ldt_restart_limit]
.Ldt_data_abort:
 cmp dword ptr [rbx+168],2
 jne .Ldt_data_status
 lea rdi,[rip+.Ldt_aborted]
.Ldt_data_status:
 call json_object_new_string@PLT
 mov rdx,rax
 mov rdi,rbp
 lea rsi,[rip+.Ldt_status]
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_end
 mov rdi,[rbx+184]
 test rdi,rdi
 jz .Ldt_metadata_done
 lea rsi,[rip+.Ldt_details]
 lea rdx,[rsp+24]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Ldt_metadata_diagnostics
 /* JSON null is a valid details value. */
 mov rdi,[rsp+24]
 call json_object_get@PLT
 mov rdx,rax
 mov rdi,r14
 lea rsi,[rip+.Ldt_details]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ldt_metadata_add_failed
.Ldt_metadata_diagnostics:
 mov rdi,[rbx+184]
 lea rsi,[rip+.Ldt_structured_output]
 lea rdx,[rsp+24]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Ldt_metadata_diagnostic_array
 mov rdi,[rsp+24]
 call json_object_get@PLT
 mov rdi,rbp
 lea rsi,[rip+.Ldt_structured_output]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ldt_metadata_add_failed
.Ldt_metadata_diagnostic_array:
 DGET "qword ptr [rbx+184]",diagnostics
 test rax,rax
 jz .Ldt_metadata_done
 DREF rbp,diagnostics,rax
.Ldt_metadata_done:
 mov rdi,r14
 mov rsi,[rbx+184]
 lea rdx,[rip+.Ldt_duration_ms]
 call .Ldt_copy_optional
 test eax,eax
 jnz .Ldt_stage_end
 cmp qword ptr [rsp+56],0
 je .Ldt_usage_done
 DREF r14,usage,"qword ptr [rsp+56]"
.Ldt_usage_done:
 DGET rbp,diagnostics
 mov rbp,rax
 mov rdi,rax
 call json_object_array_length@PLT
 test rax,rax
 jz .Ldt_diagnostics_ready
 mov rdi,rbp
 call .Ldt_render_diagnostics
 test rax,rax
 jz .Ldt_stage_alloc
 mov [rsp+32],rax
 lea rdi,[rip+.Ldt_diag_text_json]
 call json_tokener_parse@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Ldt_stage_alloc
 mov rdi,rax
 lea rsi,[rip+.Ldt_text]
 mov rdx,[rsp+32]
 mov qword ptr [rsp+32],0
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_end
 DGET r14,content
 mov rdi,rax
 mov rsi,[rsp+40]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ldt_stage_alloc
 mov qword ptr [rsp+40],0
.Ldt_diagnostics_ready:
 mov rax,[rbx+8]
 DGET "qword ptr [rax+24]",input
 DGET rax,kind
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Ldt_nested]
 call strcmp@PLT
 test eax,eax
 jz .Ldt_stage_nested
 mov rdi,rbx
 mov rsi,[rsp+48]
 call .Ldt_prune_children
 test eax,eax
 jnz .Ldt_stage_end
 mov rax,[rbx+8]
 DGET "qword ptr [rax+24]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rsi,rax
 mov rax,[rbx+8]
 mov rcx,[rax+16]
 mov rdi,r12
 mov rdx,[rsp]
 mov r8,[rbx]
 mov r8,[r8]
 mov r8,[r8+24]
 lea r9,[rsp+8]
 call pg_record_tx_append_entry@PLT
 test eax,eax
 jnz .Ldt_stage_alloc
 .Ldt_stage_outcome:
 lea rdi,[rip+.Ldt_outcome_json]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Ldt_stage_alloc
 DGET rax,outcome
 mov r14,rax
 mov eax,[rbx+168]
 lea rdi,[rip+.Ldt_completed]
 test eax,eax
 jz .Ldt_stage_status
 lea rdi,[rip+.Ldt_failed]
 cmp eax,1
 je .Ldt_stage_status
 lea rdi,[rip+.Ldt_aborted]
.Ldt_stage_status:
 call json_object_new_string@PLT
 mov rdi,r14
 lea rsi,[rip+.Ldt_status]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_end
 cmp dword ptr [rbx+168],1
 jne .Ldt_stage_receipt
 lea rdi,[rip+.Ldt_error_json]
 call json_tokener_parse@PLT
 mov rbp,rax
 test rax,rax
 jz .Ldt_stage_alloc
 mov rdi,[rbx+128]
 mov esi,[rbx+136]
 call json_object_new_string_len@PLT
 mov rdi,rbp
 lea rsi,[rip+.Ldt_message]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_error_alloc
 mov rdi,r14
 lea rsi,[rip+.Ldt_error]
 mov rdx,rbp
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_end
 cmp dword ptr [rbx+192],2
 jne .Ldt_stage_receipt
 lea rdi,[rip+.Ldt_cutoff_detail_json]
 call json_tokener_parse@PLT
 mov rdx,rax
 mov rdi,r14
 lea rsi,[rip+.Ldt_error]
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_end
.Ldt_stage_receipt:
 mov rax,[rbx+8]
 DGET "qword ptr [rax+24]",input
 DGET rax,kind
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Ldt_nested]
 call strcmp@PLT
 test eax,eax
 jnz .Ldt_stage_model_receipt
 mov rax,[rsp+16]
 DGET rax,outcome
 DGET rax,result
 mov rbp,rax
 lea rdi,[rip+.Ldt_nested]
 call json_object_new_string@PLT
 mov rdi,rbp
 lea rsi,[rip+.Ldt_kind]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_end
 mov rdi,rbp
 lea rsi,[rip+.Ldt_entry_id]
 call json_object_object_del@PLT
 jmp .Ldt_stage_transfer
.Ldt_stage_model_receipt:
 DGET r14,result
 mov rbp,rax
 DGET "qword ptr [rsp+8]",id
 mov r14,rax
 DREF rbp,entry_id,r14
 cmp dword ptr [rbx+168],0
 jne .Ldt_stage_transfer
 mov rdi,rbp
 mov rsi,[rbx+184]
 lea rdx,[rip+.Ldt_control]
 call .Ldt_copy_optional
 test eax,eax
 jnz .Ldt_stage_end
.Ldt_stage_transfer:
 mov rax,[rsp+16]
 mov [r13],rax
 mov qword ptr [rsp+16],0
 xor eax,eax
 jmp .Ldt_stage_end
.Ldt_stage_nested:
 mov rax,[rbx+8]
 DGET "qword ptr [rax+32]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+24],rax
 lea rdi,[rip+.Ldt_live_done_json]
 call json_tokener_parse@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Ldt_stage_alloc
 DGET rax,live_summary
 mov rbp,rax
 DGET r14,is_error
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov edi,eax
 call json_object_new_boolean@PLT
 mov rdi,rbp
 lea rsi,[rip+.Ldt_is_error]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_end
 mov rdi,rbp
 mov rsi,r14
 lea rdx,[rip+.Ldt_duration_ms]
 call .Ldt_copy_optional
 test eax,eax
 jnz .Ldt_stage_end
 mov rdi,rbp
 mov rsi,r14
 lea rdx,[rip+.Ldt_usage]
 call .Ldt_copy_optional
 test eax,eax
 jnz .Ldt_stage_end
 mov rdi,rbx
 mov rsi,rbp
 call .Ldt_error_summary
 test eax,eax
 jnz .Ldt_stage_end
 mov rdi,[rsp+48]
 mov rsi,[rsp+24]
 mov rax,[rbx+8]
 mov rdx,[rax+16]
 mov rcx,[rsp+32]
 mov rax,[rbx]
 mov rax,[rax]
 mov r8,[rax+24]
 mov r9d,1
 call pg_tool_live_stage@PLT
 test eax,eax
 jnz .Ldt_stage_end
 DGET "qword ptr [rsp]",data
 mov rdi,r14
 mov rsi,rax
 call .Ldt_nested_metadata
 test eax,eax
 jnz .Ldt_stage_end
 mov rdi,r14
 lea rsi,[rip+.Ldt_structured_output]
 lea rdx,[rsp+40]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Ldt_nested_output_ready
 cmp qword ptr [rbx+400],0
 jne .Ldt_nested_schema_marker
 mov rdi,[rbx+128]
 mov esi,[rbx+136]
 call json_object_new_string_len@PLT
 mov rdi,r14
 lea rsi,[rip+.Ldt_structured_output]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_end
 jmp .Ldt_nested_output_ready
.Ldt_nested_schema_marker:
 mov edi,1
 call json_object_new_boolean@PLT
 mov rdi,r14
 lea rsi,[rip+.Ldt_schema_declared]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_stage_end
.Ldt_nested_output_ready:
 mov qword ptr [rsp+40],0
 mov rax,[rbx+8]
 DGET "qword ptr [rax+24]",input
 mov rbp,rax
 DGET rbp,parent
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+24],rax
 DGET rbp,key
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdx,rax
 mov rdi,[rsp+48]
 mov rsi,[rsp+24]
 mov rax,[rbx+8]
 mov rcx,[rax+16]
 mov r8,r14
 mov rax,[rbx]
 mov rax,[rax]
 mov r9,[rax+24]
 call pg_nested_call_settle@PLT
 test eax,eax
 jnz .Ldt_stage_end
 jmp .Ldt_stage_outcome
.Ldt_metadata_add_failed:
 mov rdi,[rsp+24]
 call json_object_put@PLT
 jmp .Ldt_stage_alloc
.Ldt_stage_error_alloc:
 mov rdi,rbp
 call json_object_put@PLT
.Ldt_stage_alloc:
 mov eax,5
.Ldt_stage_end:
 mov ebp,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Ldt_finish:
 mov rsi,rdi
 mov rdi,[rdi+8]
 lea rdx,[rip+.Ldt_stage]
 jmp pg_task_runner_commit_with@PLT
.Ldt_message_output:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsi
 call strlen@PLT
 mov [rbx+136],rax
 mov rdi,[rbx+128]
 mov rsi,r12
 mov rdx,rax
 call memcpy@PLT
 add rsp,8
 pop r12
 pop rbx
 ret

.Ldt_abort:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rsi
 mov r12,rdi
 call .Ldt_open
 test eax,eax
 jnz .Ldt_abort_error
 mov r12,[rbx+56]
 mov dword ptr [r12+164],1
 mov dword ptr [r12+168],2
 DGET "qword ptr [rbx+32]",abort_reason
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Ldt_abort_ordinary
 mov rdi,rax
 lea rsi,[rip+.Ldt_restart_word]
 call strcmp@PLT
 test eax,eax
 jnz .Ldt_abort_ordinary
 lea rdi,[rip+.Ldt_abandoned_metadata]
 lea rsi,[rip+.Ldt_abandoned_message]
 cmp dword ptr [r12+156],0
 je .Ldt_abort_restart_metadata
 lea rdi,[rip+.Ldt_restart_metadata]
 lea rsi,[rip+.Ldt_restart_message]
.Ldt_abort_restart_metadata:
 mov [rsp],rsi
 call json_tokener_parse@PLT
 mov [r12+184],rax
 mov rdi,r12
 mov rsi,[rsp]
 jmp .Ldt_abort_output
.Ldt_abort_ordinary:
 lea rdi,[rip+.Ldt_aborted_metadata]
 call json_tokener_parse@PLT
 mov [r12+184],rax
 mov rdi,r12
 lea rsi,[rip+.Ldt_aborted_message]
.Ldt_abort_output:
 call .Ldt_message_output
 mov rdi,r12
 xor esi,esi
 call .Ldt_restore_partial
 test eax,eax
 jnz .Ldt_abort_error
 mov rdi,r12
 call .Ldt_finish
.Ldt_abort_error:
 neg eax
 add rsp,8
 pop r12
 pop rbx
 ret

.Ldt_poll:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rsi
 mov r12,rdi
 cmp qword ptr [rbx+56],0
 jne .Ldt_poll_operation
 call .Ldt_open
 test eax,eax
 jnz .Ldt_poll_error
.Ldt_poll_operation:
 mov r14,[rbx+56]
 mov rdi,r14
 xor esi,esi
 call .Ldt_progress_flush
 test eax,eax
 jnz .Ldt_poll_error
 cmp dword ptr [r14+232],0
 je .Ldt_poll_offered
 mov dword ptr [r14+228],1
 mov dword ptr [r14+232],0
 lea rdi,[rip+.Ldt_hook_unavailable_metadata]
 call json_tokener_parse@PLT
 mov [r14+184],rax
 test rax,rax
 jz .Ldt_poll_alloc
 jmp .Ldt_poll_unavailable
.Ldt_poll_offered:
 cmp dword ptr [r14+176],0
 jne .Ldt_poll_final
 cmp dword ptr [r14+148],0
 jne .Ldt_poll_hook_resume
 mov rax,[r12]
 mov rdi,[rax]
 mov rsi,[rbx+16]
 lea rdx,[r14+32]
 call [rax+8]
 cmp eax,1
 je .Ldt_poll_pending
 test eax,eax
 jnz .Ldt_poll_unavailable
 mov dword ptr [r14+148],1
 cmp dword ptr [r14+156],0
 je .Ldt_poll_before_hooks
 mov dword ptr [r14+224],1
 cmp dword ptr [r14+160],1
 jne .Ldt_poll_interrupted
 mov rax,[r14+56]
 test rax,rax
 jz .Ldt_poll_interrupted
 mov rdi,[r14+32]
 mov rsi,[r14+88]
 mov rdx,[r14+96]
 call rax
 cmp eax,1
 jne .Ldt_poll_interrupted
 mov rdi,rbx
 mov rsi,r14
 lea rdx,[rip+.Ldt_intent_stage]
 call pg_task_runner_commit_with@PLT
 test eax,eax
 jnz .Ldt_poll_error
 jmp .Ldt_poll_execute
.Ldt_poll_hook_resume:
 cmp dword ptr [r14+224],0
 jne .Ldt_poll_active
.Ldt_poll_before_hooks:
 mov rdi,r12
 mov rsi,rbx
 mov rdx,r14
 mov ecx,1
 call .Ldt_hooks
 cmp eax,1
 je .Ldt_poll_pending
 cmp eax,2
 je .Ldt_poll_hook_blocked
 test eax,eax
 jnz .Ldt_poll_error
 mov dword ptr [r14+224],1
 jmp .Ldt_poll_intent
.Ldt_poll_hook_blocked:
 mov dword ptr [r14+224],1
 jmp .Ldt_poll_close
.Ldt_poll_intent:
 mov rdi,rbx
 mov rsi,r14
 lea rdx,[rip+.Ldt_intent_stage]
 call pg_task_runner_commit_with@PLT
 test eax,eax
 jnz .Ldt_poll_error
.Ldt_poll_execute:
 mov rdi,r14
 call .Ldt_contract_prepare
 test eax,eax
 jnz .Ldt_poll_error
 call .Ldt_monotonic
 movsd [r14+280],xmm0
 cmp dword ptr [rbx+84],0
 jne .Ldt_poll_closed_owner
 mov rax,[r14+48]
 test rax,rax
 jz .Ldt_poll_unavailable
 mov rdi,[r14+32]
 mov rsi,[r14+88]
 mov rdx,[r14+96]
 mov rcx,[r14+104]
 mov r8,[r14+112]
 mov r9,[r14+128]
 mov rax,[r14+248]
 inc rax
 mov [rsp],rax
 lea rax,[r14+136]
 mov [rsp+8],rax
 mov dword ptr [r14+196],1
 call [r14+48]
 jmp .Ldt_poll_result
.Ldt_poll_active:
 cmp dword ptr [r14+144],2
 jb .Ldt_poll_close
 cmp dword ptr [r14+144],3
 ja .Ldt_poll_close
 mov rax,[r14+64]
 test rax,rax
 jz .Ldt_poll_invalid
 mov rdi,[r14+32]
 mov rsi,[r14+128]
 mov rdx,[r14+248]
 inc rdx
 lea rcx,[r14+136]
 call rax
.Ldt_poll_result:
 cmp eax,4
 ja .Ldt_poll_invalid
 mov [r14+144],eax
 cmp qword ptr [r14+136],-1
 jne .Ldt_poll_explicit_size
 cmp eax,1
 ja .Ldt_poll_invalid
 jmp .Ldt_poll_end_result
.Ldt_poll_explicit_size:
 mov rcx,[r14+248]
 cmp [r14+136],rcx
 ja .Ldt_poll_invalid
 cmp eax,2
 je .Ldt_poll_pending
 cmp eax,3
 jne .Ldt_poll_end_result
 mov rdi,rbx
 mov rsi,[r14+128]
 mov rdx,[r14+136]
 xor ecx,ecx
 call pg_durable_tool_output@PLT
 test eax,eax
 jnz .Ldt_poll_error
 jmp .Ldt_poll_pending
.Ldt_poll_end_result:
 cmp eax,1
 sete al
 movzx eax,al
 mov [r14+164],eax
 call .Ldt_monotonic
 movsd [r14+392],xmm0
 cmp qword ptr [r14+136],-1
 jne .Ldt_poll_close
 mov rdi,r14
 call .Ldt_reported_result
 test eax,eax
 jnz .Ldt_poll_error
.Ldt_poll_close:
 mov rdi,r14
 mov esi,1
 call .Ldt_progress_flush
 test eax,eax
 jnz .Ldt_poll_error
 cmp dword ptr [r14+180],0
 jne .Ldt_snapshot_done
 mov rax,[r12]
 cmp qword ptr [rax+40],0
 je .Ldt_snapshot_mark
 cmp dword ptr [r14+196],0
 je .Ldt_snapshot_mark
 mov rdi,[rax]
 mov rsi,[rbx+16]
 mov rdx,[r14+16]
 lea rcx,[r14+184]
 call [rax+40]
 test eax,eax
 jnz .Ldt_poll_error
 mov rdi,[r14+184]
 test rdi,rdi
 jz .Ldt_snapshot_mark
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldt_snapshot_invalid
 DGET "qword ptr [r14+184]",diagnostics
 test rax,rax
 jz .Ldt_snapshot_mark
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldt_snapshot_invalid
.Ldt_snapshot_mark:
 mov rdi,r14
 mov esi,1
 call .Ldt_restore_partial
 test eax,eax
 jnz .Ldt_poll_error
 mov dword ptr [r14+180],1
.Ldt_snapshot_done:
 cmp dword ptr [r14+228],0
 jne .Ldt_after_hooks_done
 mov rdi,r12
 mov rsi,rbx
 mov rdx,r14
 mov ecx,2
 call .Ldt_hooks
 cmp eax,1
 je .Ldt_poll_pending
 test eax,eax
 jnz .Ldt_poll_error
 mov dword ptr [r14+228],1
.Ldt_after_hooks_done:
 mov rdi,r14
 call .Ldt_reported_warning
 test eax,eax
 jnz .Ldt_poll_error
 mov rdi,r14
 call .Ldt_duration
 test eax,eax
 jnz .Ldt_poll_error
 mov rdi,r14
 call .Ldt_bound_final
 test eax,eax
 jnz .Ldt_poll_error
 mov rdi,r14
 call .Ldt_contract_result
 test eax,eax
 jnz .Ldt_poll_error
 mov dword ptr [r14+196],0
 cmp dword ptr [r14+148],0
 je .Ldt_poll_final
 mov rax,[r14+80]
 test rax,rax
 jz .Ldt_poll_closed
 mov rdi,[r14+32]
 call rax
 cmp eax,1
 je .Ldt_poll_pending
 test eax,eax
 js .Ldt_poll_close_error
.Ldt_poll_closed:
 mov dword ptr [r14+172],1
.Ldt_poll_final:
 mov dword ptr [r14+176],1
 mov rdi,r14
 call .Ldt_finish
 test eax,eax
 jnz .Ldt_poll_error
 xor eax,eax
 jmp .Ldt_poll_return
.Ldt_poll_close_error:
 mov dword ptr [r14+172],1
 mov dword ptr [r14+164],1
 mov rdi,r14
 lea rsi,[rip+.Ldt_close_message]
 call .Ldt_message_output
 jmp .Ldt_poll_final
.Ldt_snapshot_invalid:
 mov eax,4
 jmp .Ldt_poll_error
.Ldt_poll_interrupted:
 mov dword ptr [r14+192],1
 mov dword ptr [r14+164],1
 mov dword ptr [r14+168],1
 mov rdi,r14
 lea rsi,[rip+.Ldt_interrupted_message]
 call .Ldt_message_output
 lea rdi,[rip+.Ldt_restart_metadata]
 call json_tokener_parse@PLT
 mov [r14+184],rax
 mov rdi,r14
 xor esi,esi
 call .Ldt_restore_partial
 test eax,eax
 jnz .Ldt_poll_error
 mov dword ptr [r14+180],1
 jmp .Ldt_poll_close
.Ldt_poll_unavailable:
 cmp dword ptr [r14+156],0
 jne .Ldt_poll_interrupted
 mov dword ptr [r14+164],1
 mov rdi,r14
 lea rsi,[rip+.Ldt_unavailable_message]
 call .Ldt_message_output
 jmp .Ldt_poll_close
.Ldt_poll_pending:
 mov eax,1
 jmp .Ldt_poll_return
.Ldt_poll_closed_owner:
 mov eax,7
 jmp .Ldt_poll_error
.Ldt_poll_alloc:
 mov eax,5
 jmp .Ldt_poll_error
.Ldt_poll_invalid:
 mov eax,4
.Ldt_poll_error:
 neg eax
.Ldt_poll_return:
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size .Ldt_poll,.-.Ldt_poll
.section .rodata
.Ldt_task_name: .asciz "pangu.tool"
.Ldt_assistant_kind: .asciz "pangu.assistant"
.Ldt_model: .asciz "model"
.Ldt_call_phase: .asciz "call"
.Ldt_execute_phase: .asciz "execute"
.Ldt_safe: .asciz "safe"
.Ldt_unsafe: .asciz "unsafe"
.Ldt_tool_call: .asciz "toolCall"
.Ldt_completed: .asciz "completed"
.Ldt_failed: .asciz "failed"
.Ldt_aborted: .asciz "aborted"
.Ldt_role: .asciz "role"
.Ldt_assistant_role: .asciz "assistant"
.Ldt_kind: .asciz "kind"
.Ldt_input: .asciz "input"
.Ldt_assistant: .asciz "assistant"
.Ldt_call_id: .asciz "callId"
.Ldt_id: .asciz "id"
.Ldt_name: .asciz "name"
.Ldt_type: .asciz "type"
.Ldt_model_key: .asciz "model"
.Ldt_content: .asciz "content"
.Ldt_arguments: .asciz "arguments"
.Ldt_state: .asciz "state"
.Ldt_checkpoint: .asciz "checkpoint"
.Ldt_phase: .asciz "phase"
.Ldt_replay: .asciz "replay"
.Ldt_tool_call_id: .asciz "toolCallId"
.Ldt_tool_name: .asciz "toolName"
.Ldt_timestamp: .asciz "timestamp"
.Ldt_is_error: .asciz "isError"
.Ldt_text: .asciz "text"
.Ldt_conversation_id: .asciz "conversationId"
.Ldt_outcome: .asciz "outcome"
.Ldt_status: .asciz "status"
.Ldt_result: .asciz "result"
.Ldt_data: .asciz "data"
.Ldt_details: .asciz "details"
.Ldt_diagnostics: .asciz "diagnostics"
.Ldt_result_error: .asciz "error"
.Ldt_interrupted: .asciz "interrupted"
.Ldt_entry_id: .asciz "entryId"
.Ldt_error: .asciz "error"
.Ldt_message: .asciz "message"
.Ldt_aborted_message: .asciz "Tool call was aborted"
.Ldt_interrupted_message: .asciz "Tool was interrupted and may have partially run"
.Ldt_unavailable_message: .asciz "Tool is not available"
.Ldt_close_message: .asciz "Tool cleanup failed"
.Ldt_initial_json: .asciz "{\"phase\":\"call\"}"
.Ldt_intent_json: .asciz "{\"status\":\"running\",\"checkpoint\":{\"phase\":\"execute\",\"replay\":\"unsafe\"}}"
.Ldt_entry_json: .asciz "{\"kind\":\"pangu.tool-result\",\"model\":[{\"role\":\"toolResult\",\"content\":[{\"type\":\"text\",\"text\":\"\"}]}],\"data\":{\"diagnostics\":[]}}"
.Ldt_outcome_json: .asciz "{\"status\":\"terminal\",\"outcome\":{\"status\":\"completed\",\"result\":{\"kind\":\"model\",\"entryId\":0}}}"
.Ldt_error_json: .asciz "{\"message\":\"\"}"
.section .note.GNU-stack,"",@progbits
.text
/* Serialized committed read: the scan page and referenced entries remain
   owned until the detached observation has acquired its own references. */
.globl pg_durable_tool_observe
.type pg_durable_tool_observe,@function
pg_durable_tool_observe:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,200
 mov [rsp+192],rcx
 mov rbx,rdi
 mov rbp,rsi
 mov [rsp+160],r8
 mov rdi,rsp
 xor eax,eax
 mov ecx,20
 rep stosq
 mov qword ptr [rsp+168],0
 mov qword ptr [rsp+176],0
 mov qword ptr [rsp+184],0
 mov r12d,4
 test r8,r8
 jz .Ldto_end
 mov qword ptr [r8],0
 test rbx,rbx
 jz .Ldto_end
 cmp qword ptr [rbx+8],0
 je .Ldto_end
 cmp qword ptr [rbx+16],0
 je .Ldto_end
 test rbp,rbp
 jz .Ldto_end
 test rdx,rdx
 jz .Ldto_end
 cmp qword ptr [rsp+192],0
 je .Ldto_end
 cmp qword ptr [rsp+192],64
 jae .Ldto_end
 mov rdi,rdx
 mov rsi,[rsp+192]
 call json_object_new_string_len@PLT
 mov [rsp+168],rax
 test rax,rax
 jz .Ldto_alloc
 mov dword ptr [rsp],2
 mov dword ptr [rsp+4],2
 mov qword ptr [rsp+24],32
 mov qword ptr [rsp+32],33
 mov [rsp+40],rbp
 lea rax,[rip+.Ldt_task_name]
 mov [rsp+80],rax
 mov qword ptr [rsp+88],10
.Ldto_page:
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+112]
 call [rbx+16]
 test eax,eax
 jnz .Ldto_status
 mov r14,[rsp+112]
 xor r13d,r13d
.Ldto_task:
 mov rdi,r14
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Ldto_next_page
 mov rdi,r14
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r15,rax
 DGET r15,input
 mov [rsp+184],rax
 DGET rax,call_id
 test rax,rax
 jnz .Ldto_call_identity
 DGET "qword ptr [rsp+184]",call
 DGET rax,id
.Ldto_call_identity:
 mov rdi,rax
 mov rsi,[rsp+168]
 call json_object_equal@PLT
 test eax,eax
 jnz .Ldto_found
 inc r13
 jmp .Ldto_task
.Ldto_next_page:
 cmp dword ptr [rsp+132],0
 je .Ldto_missing
 mov rax,[rsp+120]
 mov [rsp+16],rax
 mov dword ptr [rsp+8],1
 mov dword ptr [rsp+12],2
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 jmp .Ldto_page
.Ldto_found:
 DGET r15,input
 mov [rsp+184],rax
 DGET rax,call
 test rax,rax
 jz .Ldto_model_found
 mov r14,rax
 jmp .Ldto_result
.Ldto_model_found:
 DGET "qword ptr [rsp+184]",assistant
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdx,rax
 mov rdi,[rbx]
 mov esi,1
 lea rcx,[rsp+144]
 call [rbx+8]
 test eax,eax
 jnz .Ldto_status
 DGET "qword ptr [rsp+144]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,rbp
 jne .Ldto_invalid
 DGET "qword ptr [rsp+144]",model_key
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 DGET rax,content
 mov [rsp+184],rax
 xor r13d,r13d
.Ldto_call:
 mov rdi,[rsp+184]
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Ldto_invalid
 mov rdi,[rsp+184]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 DGET r14,id
 mov rdi,rax
 mov rsi,[rsp+168]
 call json_object_equal@PLT
 test eax,eax
 jnz .Ldto_result
 inc r13
 jmp .Ldto_call
.Ldto_result:
 DGET r15,state
 DGET rax,outcome
 DGET rax,result
 DGET rax,entry_id
 test rax,rax
 jz .Ldto_build
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdx,rax
 mov rdi,[rbx]
 mov esi,1
 lea rcx,[rsp+152]
 call [rbx+8]
 test eax,eax
 jnz .Ldto_status
 DGET "qword ptr [rsp+152]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,rbp
 jne .Ldto_invalid
 DGET "qword ptr [rsp+152]",model_key
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 DGET rax,tool_call_id
 mov rdi,rax
 mov rsi,[rsp+168]
 call json_object_equal@PLT
 test eax,eax
 jz .Ldto_invalid
.Ldto_build:
 call json_object_new_object@PLT
 mov [rsp+176],rax
 test rax,rax
 jz .Ldto_alloc
 mov rdi,r15
 call json_object_get@PLT
 mov rdx,rax
 mov rdi,[rsp+176]
 lea rsi,[rip+.Ldto_task_key]
 call .Ldt_set
 test eax,eax
 jnz .Ldto_status
 mov rdi,r14
 call json_object_get@PLT
 mov rdx,rax
 mov rdi,[rsp+176]
 lea rsi,[rip+.Ldto_call_key]
 call .Ldt_set
 test eax,eax
 jnz .Ldto_status
 mov rdi,[rsp+152]
 test rdi,rdi
 jz .Ldto_success
 call json_object_get@PLT
 mov rdx,rax
 mov rdi,[rsp+176]
 lea rsi,[rip+.Ldto_entry_key]
 call .Ldt_set
 test eax,eax
 jnz .Ldto_status
.Ldto_success:
 mov rax,[rsp+160]
 mov rdx,[rsp+176]
 mov [rax],rdx
 mov qword ptr [rsp+176],0
 xor r12d,r12d
 jmp .Ldto_end
.Ldto_missing:
 mov r12d,1
 jmp .Ldto_end
.Ldto_invalid:
 mov r12d,4
 jmp .Ldto_end
.Ldto_alloc:
 mov eax,5
.Ldto_status:
 mov r12d,eax
.Ldto_end:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov rdi,[rsp+144]
 call json_object_put@PLT
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov rdi,[rsp+168]
 call json_object_put@PLT
 mov rdi,[rsp+176]
 call json_object_put@PLT
 mov eax,r12d
 add rsp,200
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_durable_tool_observe,.-pg_durable_tool_observe
.globl pg_durable_tool_observation_release
.type pg_durable_tool_observation_release,@function
pg_durable_tool_observation_release:
 jmp json_object_put@PLT
.size pg_durable_tool_observation_release,.-pg_durable_tool_observation_release
.section .rodata
.Ldto_task_key: .asciz "task"
.Ldto_call_key: .asciz "call"
.Ldto_entry_key: .asciz "entry"

.text
/* Pi renderDiagnostics, retaining explicit JSON string lengths. */
.Ldt_render_diagnostics:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 xor r12d,r12d
 mov r13d,10
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 xor r12d,r12d
 mov qword ptr [rsp+24],22
.Ldr_measure:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ldr_allocate
 mov rdi,rbx
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov r14,rax
 DGET r14,severity
 mov rbp,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldr_end
 mov rdi,rbp
 call json_object_get_string_len@PLT
 add [rsp+24],rax
 jc .Ldr_end
 DGET r14,message
 mov rbp,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldr_end
 mov rdi,rbp
 call json_object_get_string_len@PLT
 add [rsp+24],rax
 jc .Ldr_end
 add qword ptr [rsp+24],4
 jc .Ldr_end
 cmp qword ptr [rsp+24],2147483647
 ja .Ldr_end
 inc r12
 jmp .Ldr_measure
.Ldr_allocate:
 xor r12d,r12d
 mov edi,1
 mov rsi,[rsp+24]
 call calloc@PLT
 mov [rsp],rax
 test rax,rax
 jz .Ldr_end
 mov rdi,rax
 lea rsi,[rip+.Ldt_diag_open]
 mov edx,10
 call memcpy@PLT
.Ldr_loop:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ldr_finish
 mov rdi,rbx
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov r14,rax
 DGET r14,severity
 mov rbp,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldr_end
 mov rdi,rbp
 call json_object_get_string_len@PLT
 mov [rsp+16],rax
 lea rcx,[r13+rax+3]
 cmp rcx,[rsp+24]
 jae .Ldr_end
 test r12,r12
 jz .Ldr_item
 mov rax,[rsp]
 mov byte ptr [rax+r13],10
 inc r13
.Ldr_item:
 mov rax,[rsp]
 mov byte ptr [rax+r13],91
 inc r13
 mov rdi,rbp
 call json_object_get_string@PLT
 mov rsi,rax
 mov rdi,[rsp]
 add rdi,r13
 mov rdx,[rsp+16]
 call memcpy@PLT
 add r13,[rsp+16]
 mov rax,[rsp]
 mov word ptr [rax+r13],0x205d
 add r13,2
 DGET r14,message
 mov rbp,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldr_end
 mov rdi,rbp
 call json_object_get_string_len@PLT
 mov [rsp+16],rax
 lea rcx,[r13+rax+11]
 cmp rcx,[rsp+24]
 jae .Ldr_end
 mov rdi,rbp
 call json_object_get_string@PLT
 mov rsi,rax
 mov rdi,[rsp]
 add rdi,r13
 mov rdx,[rsp+16]
 call memcpy@PLT
 add r13,[rsp+16]
 inc r12
 jmp .Ldr_loop
.Ldr_finish:
 mov rdi,[rsp]
 add rdi,r13
 lea rsi,[rip+.Ldt_diag_close]
 mov edx,11
 call memcpy@PLT
 add r13,11
 mov rdi,[rsp]
 mov esi,r13d
 call json_object_new_string_len@PLT
 mov [rsp+8],rax
.Ldr_end:
 mov rdi,[rsp]
 call free@PLT
 mov rax,[rsp+8]
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Ldt_severity: .asciz "severity"
.Ldt_diag_open: .ascii "<harness>\n"
.Ldt_diag_close: .ascii "\n</harness>"
.Ldt_diag_text_json: .asciz "{\"type\":\"text\"}"

.section .rodata
.Ldt_structured_output: .asciz "structuredOutput"

.text
.Ldt_cutoff:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rsi
 mov r12,rdi
 call .Ldt_open
 test eax,eax
 jnz .Ldt_cutoff_end
 mov r12,[rbx+56]
 mov dword ptr [r12+164],1
 mov dword ptr [r12+168],1
 mov dword ptr [r12+192],2
 lea rdi,[rip+.Ldt_cutoff_metadata]
 call json_tokener_parse@PLT
 mov [r12+184],rax
 test rax,rax
 jz .Ldt_cutoff_alloc
 mov rdi,r12
 lea rsi,[rip+.Ldt_cutoff_message]
 call .Ldt_message_output
 mov rdi,r12
 call .Ldt_finish
.Ldt_cutoff_end:
 neg eax
 add rsp,8
 pop r12
 pop rbx
 ret
.Ldt_cutoff_alloc:
 mov eax,5
 jmp .Ldt_cutoff_end
.section .rodata
.Ldt_cutoff_message: .asciz "Tool reached the consecutive restart limit; it is not run again"
.Ldt_restart_limit: .asciz "restart_limit"

.section .rodata
.Ldt_cutoff_detail_json: .asciz "{\"message\":\"Tool reached the consecutive restart limit\",\"detail\":{\"reason\":\"restart_limit\"}}"

.section .rodata
.Ldt_cutoff_metadata: .asciz "{\"diagnostics\":[{\"severity\":\"error\",\"code\":\"restart_limit\",\"message\":\"Tool reached the consecutive restart limit; it is not run again\"}]}"

.section .rodata
.Ldt_usage: .asciz "usage"
.Ldt_tools_bucket: .asciz "tools"
.text
.globl pg_durable_tool_bind_hooks
.type pg_durable_tool_bind_hooks,@function
pg_durable_tool_bind_hooks:
 mov [rdi+176],rsi
 ret
/* Runs before intent or before result settlement; a pending hook keeps the
   executor and its extension scope leased through the next Scheduler poll. */
.Ldt_hooks:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14d,ecx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov rbp,[rbx+176]
 test rbp,rbp
 jz .Ldt_hooks_ok
 cmp qword ptr [r13+208],0
 jne .Ldt_hooks_poll
 cmp r14d,1
 jne .Ldt_hooks_build
 cmp qword ptr [rbp+24],0
 je .Ldt_hooks_build
 mov rdi,[rbp]
 mov rsi,[r12+16]
 mov rdx,[r13+16]
 call [rbp+24]
 cmp eax,1
 jne .Ldt_hooks_validation_error
.Ldt_hooks_build:
 call json_object_new_object@PLT
 mov [r13+208],rax
 test rax,rax
 jz .Ldt_hooks_bad
 mov rdi,[r13+16]
 call json_object_get@PLT
 mov rdi,[r13+208]
 lea rsi,[rip+.Ldt_hook_call]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_hooks_bad
 cmp r14d,2
 jne .Ldt_hooks_poll
 mov rdi,[r13+184]
 test rdi,rdi
 jz .Ldt_hooks_new_result
 lea rsi,[rsp]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ldt_hooks_bad
 jmp .Ldt_hooks_result
.Ldt_hooks_new_result:
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Ldt_hooks_bad
.Ldt_hooks_result:
 mov rdi,[r13+128]
 mov esi,[r13+136]
 call json_object_new_string_len@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Ldt_hook_output]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_hooks_bad
 mov edi,[r13+164]
 call json_object_new_boolean@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Ldt_is_error]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_hooks_bad
 mov rdi,[r13+208]
 lea rsi,[rip+.Ldt_hook_result]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Ldt_set
 test eax,eax
 jnz .Ldt_hooks_bad
.Ldt_hooks_poll:
 mov rdi,[rbp]
 mov rsi,[r12+16]
 mov edx,r14d
 mov rcx,[r13+208]
 lea r8,[r13+200]
 lea r9,[r13+216]
 call [rbp+8]
 cmp eax,1
 je .Ldt_hooks_pending
 test eax,eax
 jnz .Ldt_hooks_bad
 cmp r14d,2
 je .Ldt_hooks_after
 DGET "qword ptr [r13+216]",hook_block
 test rax,rax
 jnz .Ldt_hooks_block
 DGET "qword ptr [r13+216]",hook_call
 DGET rax,arguments
 mov [rsp+16],rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldt_hooks_bad
 mov rdi,[rsp+16]
 call json_object_get@PLT
 mov [rsp+8],rax
 mov rdi,[r13+24]
 call json_object_put@PLT
 mov rax,[rsp+8]
 mov [r13+24],rax
 mov qword ptr [rsp+8],0
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,[r13+16]
 lea rsi,[rip+.Ldt_arguments]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_hooks_bad
 mov rdi,[r13+24]
 mov esi,256
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Ldt_hooks_bad
 mov [r13+104],rax
 mov rdi,rax
 call strlen@PLT
 mov [r13+112],rax
 cmp qword ptr [rbp+24],0
 je .Ldt_hooks_ok
 mov rdi,[rbp]
 mov rsi,[r12+16]
 mov rdx,[r13+16]
 call [rbp+24]
 cmp eax,1
 jne .Ldt_hooks_validation_error
 jmp .Ldt_hooks_ok
.Ldt_hooks_validation_error:
 lea r14,[rip+.Ldt_hook_invalid_arguments]
 lea r12,[rip+.Ldt_hook_invalid_metadata]
 test eax,eax
 jns .Ldt_hooks_validation_text
 lea r14,[rip+.Ldt_hook_unavailable]
 lea r12,[rip+.Ldt_hook_unavailable_metadata]
.Ldt_hooks_validation_text:
 mov rdi,r14
 call strlen@PLT
 mov [r13+136],rax
 mov rdi,[r13+128]
 mov rsi,r14
 mov rdx,rax
 call memcpy@PLT
 mov dword ptr [r13+144],1
 mov dword ptr [r13+164],1
 mov rdi,r12
 call json_tokener_parse@PLT
 test rax,rax
 jz .Ldt_hooks_bad
 mov [r13+184],rax
 mov ebx,2
 jmp .Ldt_hooks_complete
.Ldt_hooks_block:
 mov [rsp+16],rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 cmp eax,4076
 jbe .Ldt_hooks_block_length
 mov eax,4076
.Ldt_hooks_block_length:
 mov [rsp+24],rax
 mov rdi,[r13+128]
 lea rsi,[rip+.Ldt_hook_prefix]
 mov edx,19
 call memcpy@PLT
 mov rdi,[rsp+16]
 call json_object_get_string@PLT
 mov rsi,rax
 mov rdi,[r13+128]
 add rdi,19
 mov rdx,[rsp+24]
 call memcpy@PLT
 mov rax,[rsp+24]
 add rax,19
 mov [r13+136],rax
 mov dword ptr [r13+144],1
 mov dword ptr [r13+164],1
 lea rdi,[rip+.Ldt_hook_block_metadata]
 call json_tokener_parse@PLT
 test rax,rax
 jz .Ldt_hooks_bad
 mov [r13+184],rax
 mov ebx,2
 jmp .Ldt_hooks_complete
.Ldt_hooks_after:
 DGET "qword ptr [r13+216]",hook_result
 mov [rsp+16],rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldt_hooks_bad
 DGET "qword ptr [rsp+16]",hook_output
 mov [rsp+24],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldt_hooks_bad
 mov rdi,[rsp+24]
 call json_object_get_string_len@PLT
 mov [rsp+32],rax
 cmp dword ptr [r13+372],0
 je .Ldt_hooks_retained_ready
 cmp rax,[r13+136]
 jne .Ldt_hooks_retained_replaced
 mov rdi,[rsp+24]
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,[r13+128]
 mov rdx,[rsp+32]
 call memcmp@PLT
 test eax,eax
 jz .Ldt_hooks_retained_ready
.Ldt_hooks_retained_replaced:
 mov dword ptr [r13+372],0
.Ldt_hooks_retained_ready:
 mov rax,[rsp+32]
 cmp rax,[r13+248]
 jbe .Ldt_hooks_capacity_ready
 lea rdi,[rax+1]
 call malloc@PLT
 test rax,rax
 jz .Ldt_hooks_bad
 mov rdi,[r13+128]
 mov [r13+128],rax
 call free@PLT
 mov rax,[rsp+32]
 mov [r13+248],rax
.Ldt_hooks_capacity_ready:
 DGET "qword ptr [rsp+16]",diagnostics
 test rax,rax
 jz .Ldt_hooks_after_copy
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldt_hooks_bad
.Ldt_hooks_after_copy:
 mov rdi,[rsp+24]
 call json_object_get_string@PLT
 mov rsi,rax
 mov rdi,[r13+128]
 mov rdx,[rsp+32]
 call memcpy@PLT
 mov rax,[rsp+32]
 mov [r13+136],rax
 DGET "qword ptr [rsp+16]",is_error
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov [r13+164],eax
 mov rdi,[rsp+16]
 call json_object_get@PLT
 mov [rsp+8],rax
 mov rdi,[r13+184]
 call json_object_put@PLT
 mov rax,[rsp+8]
 mov [r13+184],rax
 mov qword ptr [rsp+8],0
.Ldt_hooks_ok:
 xor ebx,ebx
 jmp .Ldt_hooks_complete
.Ldt_hooks_bad:
 mov ebx,5
 jmp .Ldt_hooks_end
.Ldt_hooks_pending:
 mov ebx,1
 jmp .Ldt_hooks_end
.Ldt_hooks_complete:
 mov rdi,[r13+208]
 call json_object_put@PLT
 mov qword ptr [r13+208],0
 mov rdi,[r13+216]
 call json_object_put@PLT
 mov qword ptr [r13+216],0
.Ldt_hooks_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebx
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Ldt_hook_call: .asciz "call"
.Ldt_hook_result: .asciz "result"
.Ldt_hook_output: .asciz "output"
.Ldt_hook_block: .asciz "block"
.Ldt_hook_prefix: .asciz "Tool call blocked: "
.Ldt_hook_block_metadata: .asciz "{\"diagnostics\":[{\"severity\":\"error\",\"code\":\"blocked\",\"message\":\"Tool call blocked\"}]}"

.section .rodata
.Ldt_hook_invalid_arguments: .asciz "Tool arguments are invalid"
.Ldt_hook_unavailable: .asciz "Tool is not available"
.Ldt_hook_invalid_metadata: .asciz "{\"diagnostics\":[{\"severity\":\"error\",\"code\":\"invalid_arguments\",\"message\":\"Tool arguments are invalid\"}]}"
.Ldt_hook_unavailable_metadata: .asciz "{\"diagnostics\":[{\"severity\":\"error\",\"code\":\"tool_unavailable\",\"message\":\"Tool is not available\"}]}"

.section .rodata
.Ldt_offered_tools: .asciz "offeredTools"

.section .rodata
.Ldt_nested: .asciz "nested"
.Ldt_call: .asciz "call"
.Ldt_parent: .asciz "parent"
.Ldt_key: .asciz "key"

.text
.globl pg_durable_tool_call
.type pg_durable_tool_call,@function
pg_durable_tool_call:
 xor eax,eax
 test rdi,rdi
 jz .Ldt_call_end
 mov rax,[rdi+56]
 test rax,rax
 jz .Ldt_call_end
 mov rax,[rax+16]
.Ldt_call_end:
 ret
.size pg_durable_tool_call,.-pg_durable_tool_call

.text
.globl pg_durable_tool_executing
.type pg_durable_tool_executing,@function
pg_durable_tool_executing:
 xor eax,eax
 test rdi,rdi
 jz .Ldt_executing_end
 mov rdx,[rdi+56]
 test rdx,rdx
 jz .Ldt_executing_end
 cmp dword ptr [rdi+84],0
 jne .Ldt_executing_end
 cmp dword ptr [rdx+196],0
 je .Ldt_executing_end
 cmp qword ptr [rdx+392],0
 jne .Ldt_executing_end
 cmp dword ptr [rdx+176],0
 jne .Ldt_executing_end
 mov eax,1
.Ldt_executing_end:
 ret
.size pg_durable_tool_executing,.-pg_durable_tool_executing

.text
.Ldt_nested_metadata:
 push rbp
 push rbx
 push r12
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 lea rbp,[rip+.Ldt_structured_output]
.Ldt_nested_metadata_field:
 mov rdi,r12
 mov rsi,rbp
 mov rdx,rsp
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Ldt_nested_metadata_next
 mov rdi,[rsp]
 call json_object_get@PLT
 mov rdi,rbx
 mov rsi,rbp
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ldt_nested_metadata_bad
.Ldt_nested_metadata_next:
 lea rax,[rip+.Ldt_diagnostics]
 cmp rbp,rax
 je .Ldt_nested_metadata_ok
 mov rbp,rax
 jmp .Ldt_nested_metadata_field
.Ldt_nested_metadata_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,5
 jmp .Ldt_nested_metadata_end
.Ldt_nested_metadata_ok:
 xor eax,eax
.Ldt_nested_metadata_end:
 add rsp,16
 pop r12
 pop rbx
 pop rbp
 ret

.text
/* The intent and Live running slot share the ToolTask mutation. */
.Ldt_intent_stage:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rdx
 mov r13,rcx
 mov qword ptr [rcx],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov dword ptr [rsp+16],0
 lea rdi,[rip+.Ldt_live_running_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Ldt_live_stage_bad
 mov rax,[rbx+8]
 DGET "qword ptr [rax+32]",input
 DGET rax,kind
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Ldt_live_intent
 mov rdi,rax
 lea rsi,[rip+.Ldt_nested]
 call strcmp@PLT
 test eax,eax
 jz .Ldt_live_bucket_ready
 mov dword ptr [rsp+16],2
.Ldt_live_bucket_ready:
 mov rdi,[rbx+24]
 call json_object_get@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Ldt_arguments]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_live_stage_end
 mov rdi,[rsp]
 mov rsi,rbx
 call .Ldt_live_identity
 test eax,eax
 jnz .Ldt_live_stage_end
 mov rax,[rbx+8]
 DGET "qword ptr [rax+32]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rsi,rax
 mov rax,[rbx+8]
 mov rdx,[rax+16]
 mov rdi,r12
 mov rcx,[rsp]
 mov rax,[rbx]
 mov rax,[rax]
 mov r8,[rax+24]
 mov r9d,[rsp+16]
 call pg_tool_live_stage@PLT
 test eax,eax
 jnz .Ldt_live_stage_end
.Ldt_live_intent:
 lea rdi,[rip+.Ldt_intent_json]
 call json_tokener_parse@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Ldt_live_stage_bad
 DGET rax,checkpoint
 mov r14,rax
 mov rdi,[rbx+24]
 call json_object_get@PLT
 mov rdi,r14
 lea rsi,[rip+.Ldt_arguments]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_live_stage_end
 xor ebp,ebp
 mov rax,[rbx+56]
 test rax,rax
 jz .Ldt_live_policy
 mov rdi,[rbx+32]
 mov rsi,[rbx+88]
 mov rdx,[rbx+96]
 call rax
 cmp eax,1
 sete bpl
.Ldt_live_policy:
 lea rdi,[rip+.Ldt_unsafe]
 test ebp,ebp
 jz .Ldt_live_replay
 lea rdi,[rip+.Ldt_safe]
.Ldt_live_replay:
 call json_object_new_string@PLT
 mov rdi,r14
 lea rsi,[rip+.Ldt_replay]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_live_stage_end
 mov rax,[rsp+8]
 mov [r13],rax
 mov qword ptr [rsp+8],0
 xor eax,eax
 jmp .Ldt_live_stage_end
.Ldt_live_stage_bad:
 mov eax,5
.Ldt_live_stage_end:
 mov ebp,eax
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
.section .rodata
.Ldt_live_running_json: .asciz "{\"status\":\"running\"}"

.section .rodata
.Ldt_live_done_json: .asciz "{\"status\":\"done\",\"summary\":{}}"
.Ldt_live_summary: .asciz "summary"

.text
/* Persist a bounded UTF-8 tail before publishing its output event. */
.Ldt_progress_stage:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov r12,rdx
 mov qword ptr [rcx],0
 mov rdi,rsp
 xor eax,eax
 mov ecx,8
 rep stosq
 lea rdi,[rbx+288]
 mov rsi,rsp
 call pg_output_buffer_snapshot@PLT
 test eax,eax
 jnz .Ldt_progress_end
 lea rdi,[rip+.Ldt_live_output_json]
 call json_tokener_parse@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Ldt_progress_alloc
 mov rdi,[rsp]
 mov esi,[rsp+8]
 call json_object_new_string_len@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ldt_hook_output]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_progress_end
 mov rdi,[rsp+16]
 call json_object_new_uint64@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ldt_live_dropped_bytes]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_progress_end
 mov rdi,[rsp+24]
 call json_object_new_uint64@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ldt_live_dropped_lines]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_progress_end
 mov rax,[rbx+8]
 DGET "qword ptr [rax+32]",input
 DGET rax,kind
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Ldt_nested]
 call strcmp@PLT
 test eax,eax
 jz .Ldt_progress_bucket
 mov dword ptr [rsp+40],2
.Ldt_progress_bucket:
 mov rax,[rbx+8]
 DGET "qword ptr [rax+32]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rsi,rax
 mov rdi,r12
 mov rax,[rbx+8]
 mov rdx,[rax+16]
 mov rcx,[rsp+32]
 mov rax,[rbx]
 mov rax,[rax]
 mov r8,[rax+24]
 mov r9d,[rsp+40]
 lea rax,[rbx+360]
 sub rsp,16
 mov [rsp],rax
 call pg_tool_live_stage_metered@PLT
 add rsp,16
 jmp .Ldt_progress_end
.Ldt_progress_alloc:
 mov eax,5
.Ldt_progress_end:
 mov [rsp+48],eax
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov eax,[rsp+48]
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Ldt_live_nested_tools: .asciz "nestedTools"
.Ldt_live_task_id: .asciz "taskId"
.Ldt_live_dropped_bytes: .asciz "droppedBytes"
.Ldt_live_output_json: .asciz "{}"

.section .rodata
.Ldt_abort_reason: .asciz "abortReason"
.Ldt_restart_word: .asciz "restart"
.Ldt_abandoned_message: .asciz "Tool was not started: its caller ended with a restart"
.Ldt_restart_message: .asciz "Tool was interrupted by a restart and may have partially run"
.Ldt_abandoned_metadata: .asciz "{\"diagnostics\":[{\"severity\":\"error\",\"code\":\"abandoned\",\"message\":\"Tool was not started: its caller ended with a restart\"}]}"
.Ldt_restart_metadata: .asciz "{\"diagnostics\":[{\"severity\":\"error\",\"code\":\"interrupted\",\"message\":\"Tool was interrupted by a restart and may have partially run\"}]}"

.text
/* Recover durable progress before the terminal transaction clears the slot. */
.Ldt_restore_partial:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov [rsp+40],esi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],0
 mov rax,[rbx+8]
 DGET "qword ptr [rax+32]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rsi,rax
 mov rax,[rbx+8]
 mov rdi,[rax]
 mov rdx,[rax+16]
 mov rcx,rsp
 call pg_tool_live_read@PLT
 cmp eax,8
 je .Ldt_restore_ok
 test eax,eax
 jnz .Ldt_restore_end
 cmp qword ptr [rbx+184],0
 jne .Ldt_restore_metadata
 call json_object_new_object@PLT
 mov [rbx+184],rax
 test rax,rax
 jz .Ldt_restore_alloc
.Ldt_restore_metadata:
 cmp dword ptr [rsp+40],0
 je .Ldt_restore_details
 mov rdi,[rbx+184]
 lea rsi,[rip+.Ldt_details]
 lea rdx,[rsp+8]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Ldt_restore_diagnostics
.Ldt_restore_details:
 DGET "qword ptr [rsp]",details
 mov r12,rax
 mov rdi,[rsp]
 lea rsi,[rip+.Ldt_details]
 lea rdx,[rsp+8]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Ldt_restore_diagnostics
 mov rdi,r12
 call json_object_get@PLT
 mov [rsp+24],rax
 mov rdi,[rbx+184]
 lea rsi,[rip+.Ldt_details]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ldt_restore_alloc
 mov qword ptr [rsp+24],0
.Ldt_restore_diagnostics:
 call json_object_new_array@PLT
 mov r13,rax
 mov [rsp+16],rax
 test rax,rax
 jz .Ldt_restore_alloc
 DGET "qword ptr [rsp]",diagnostics
 mov r12,rax
 xor r14d,r14d
.Ldt_restore_copy_diagnostics:
 test r12,r12
 jz .Ldt_restore_error_diagnostics
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Ldt_restore_error_diagnostics
 mov rdi,r12
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+24],rax
 mov rdi,r13
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ldt_restore_alloc
 mov qword ptr [rsp+24],0
 inc r14
 jmp .Ldt_restore_copy_diagnostics
.Ldt_restore_error_diagnostics:
 cmp dword ptr [rsp+40],0
 jne .Ldt_restore_errors_ready
 DGET "qword ptr [rsp]",live_dropped_bytes
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov r12,rax
 DGET "qword ptr [rsp]",live_dropped_lines
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdx,rax
 mov rdi,r13
 mov rsi,r12
 lea rcx,[rip+.Ldt_tail_word]
 call .Ldt_truncated
 test eax,eax
 jnz .Ldt_restore_end
.Ldt_restore_errors_ready:
 DGET "qword ptr [rbx+184]",diagnostics
 mov r12,rax
 xor r14d,r14d
.Ldt_restore_errors_each:
 test r12,r12
 jz .Ldt_restore_install_diagnostics
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Ldt_restore_install_diagnostics
 mov rdi,r12
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+24],rax
 mov rdi,r13
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ldt_restore_alloc
 mov qword ptr [rsp+24],0
 inc r14
 jmp .Ldt_restore_errors_each
.Ldt_restore_install_diagnostics:
 mov rdi,[rbx+184]
 lea rsi,[rip+.Ldt_diagnostics]
 mov rdx,r13
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ldt_restore_alloc
 mov qword ptr [rsp+16],0
 cmp dword ptr [rsp+40],0
 jne .Ldt_restore_ok
 DGET "qword ptr [rsp]",hook_output
 mov r12,rax
 test rax,rax
 jz .Ldt_restore_ok
 mov rdi,rax
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Ldt_restore_ok
 cmp rax,[rbx+248]
 ja .Ldt_restore_alloc
 mov [rbx+136],rax
 mov rbp,rax
 mov rdi,r12
 call json_object_get_string@PLT
 mov rdi,[rbx+128]
 mov rsi,rax
 mov rdx,rbp
 call memcpy@PLT
.Ldt_restore_ok:
 xor eax,eax
 jmp .Ldt_restore_end
.Ldt_restore_alloc:
 mov eax,5
.Ldt_restore_end:
 mov [rsp+32],eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov eax,[rsp+32]
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.section .rodata
.Ldt_aborted_metadata: .asciz "{\"diagnostics\":[{\"severity\":\"error\",\"code\":\"aborted\",\"message\":\"Tool call was aborted\"}]}"

.text
.Ldt_prune_children:
 push rbp
 push rbx
 push r12
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov rax,[rbx+8]
 DGET "qword ptr [rax+32]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rbp,rax
 mov rsi,rax
 mov rax,[rbx+8]
 mov rdi,[rax]
 mov rdx,rsp
 call pg_live_peek@PLT
 cmp eax,8
 je .Ldt_prune_ok
 test eax,eax
 jnz .Ldt_prune_end
 DGET "qword ptr [rsp]",live_nested_tools
 test rax,rax
 jz .Ldt_prune_ok
 mov rdi,rax
 call json_object_array_length@PLT
 test rax,rax
 jz .Ldt_prune_ok
 mov rdi,r12
 mov rsi,rbp
 mov rax,[rbx+8]
 mov rdx,[rax+16]
 xor ecx,ecx
 mov rax,[rbx]
 mov rax,[rax]
 mov r8,[rax+24]
 mov r9d,1
 call pg_tool_live_stage@PLT
 test eax,eax
 jnz .Ldt_prune_end
.Ldt_prune_ok:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov qword ptr [rsp],0
 mov rax,[rbx+8]
 mov rdi,[rax]
 mov rsi,rbp
 mov rdx,[rax+16]
 mov rcx,rsp
 call pg_tool_live_read@PLT
 cmp eax,8
 je .Ldt_prune_no_slot
 test eax,eax
 jnz .Ldt_prune_end
 mov rdi,[rsp]
 call json_object_put@PLT
 mov qword ptr [rsp],0
 lea rdi,[rip+.Ldt_live_done_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Ldt_prune_alloc
 mov rdi,r12
 mov rsi,rbp
 mov rax,[rbx+8]
 mov rdx,[rax+16]
 mov rcx,[rsp]
 mov rax,[rbx]
 mov rax,[rax]
 mov r8,[rax+24]
 mov r9d,2
 call pg_tool_live_stage@PLT
 jmp .Ldt_prune_end
.Ldt_prune_alloc:
 mov eax,5
 jmp .Ldt_prune_end
.Ldt_prune_no_slot:
 xor eax,eax
.Ldt_prune_end:
 mov ebx,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebx
 add rsp,16
 pop r12
 pop rbx
 pop rbp
 ret

.text
.Ldt_live_identity:
 push rbp
 push rbx
 push r12
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov rax,[rsi+8]
 mov rdi,[rax+16]
 call json_object_new_uint64@PLT
 mov rdi,rbx
 lea rsi,[rip+.Ldt_live_task_id]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_identity_end
 DGET "qword ptr [r12+16]",id
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,rbx
 lea rsi,[rip+.Ldt_call_id]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_identity_end
 DGET "qword ptr [r12+16]",name
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,rbx
 lea rsi,[rip+.Ldt_name]
 mov rdx,rax
 call .Ldt_set
.Ldt_identity_end:
 add rsp,16
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Ldt_live_tools: .asciz "tools"

.section .rodata
.Ldt_live_dropped_lines: .asciz "droppedLines"
.Ldt_head_word: .asciz "beginning"
.Ldt_tail_word: .asciz "end"
.Ldt_truncation_format: .asciz "Output truncated to its %s: %llu lines, %llu bytes dropped"
.Ldt_truncation_json: .asciz "{\"severity\":\"warn\",\"code\":\"truncated\"}"
.text
.Ldt_truncated:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 test r12,r12
 jz .Ldt_truncated_ok
 lea rdi,[rsp]
 lea rsi,[rip+.Ldt_truncation_format]
 mov rdx,r14
 mov rcx,r13
 mov r8,r12
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Ldt_truncated_alloc
 lea rdi,[rip+.Ldt_truncation_json]
 call json_tokener_parse@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Ldt_truncated_alloc
 mov rdi,[rsp]
 call json_object_new_string@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ldt_message]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_truncated_end
 mov rdi,rbx
 mov rsi,[rsp+8]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ldt_truncated_alloc
 mov qword ptr [rsp+8],0
.Ldt_truncated_ok:
 xor eax,eax
 jmp .Ldt_truncated_end
.Ldt_truncated_alloc:
 mov eax,5
.Ldt_truncated_end:
 mov ebp,eax
 mov rdi,[rsp]
 call free@PLT
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
.Ldt_ensure_diagnostics:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 cmp qword ptr [rdi+184],0
 jne .Ldt_diagnostics_get
 call json_object_new_object@PLT
 mov [rbx+184],rax
 test rax,rax
 jz .Ldt_diagnostics_return
.Ldt_diagnostics_get:
 DGET "qword ptr [rbx+184]",diagnostics
 test rax,rax
 jnz .Ldt_diagnostics_return
 call json_object_new_array@PLT
 mov r12,rax
 test rax,rax
 jz .Ldt_diagnostics_return
 mov rdi,[rbx+184]
 lea rsi,[rip+.Ldt_diagnostics]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_diagnostics_failed
 mov rax,r12
 jmp .Ldt_diagnostics_return
.Ldt_diagnostics_failed:
 xor eax,eax
.Ldt_diagnostics_return:
 add rsp,8
 pop r12
 pop rbx
 ret
.Ldt_bound_final:
 push rbx
 sub rsp,32
 mov rbx,rdi
 mov rdi,[rbx+128]
 mov rsi,[rbx+136]
 mov rdx,[rbx+256]
 mov rcx,[rbx+264]
 mov r8d,[rbx+272]
 mov r9,rsp
 call pg_output_bound@PLT
 test eax,eax
 jnz .Ldt_bound_final_end
 cmp qword ptr [rsp+16],0
 je .Ldt_bound_final_end
 cmp qword ptr [rsp],0
 je .Ldt_bound_head_ready
 mov rdi,[rbx+128]
 mov rsi,rdi
 add rsi,[rsp]
 mov rdx,[rsp+8]
 call memmove@PLT
.Ldt_bound_head_ready:
 mov rax,[rsp+8]
 mov [rbx+136],rax
 mov rdi,rbx
 call .Ldt_ensure_diagnostics
 test rax,rax
 jz .Ldt_bound_final_alloc
 mov rdi,rax
 mov rsi,[rsp+16]
 mov rdx,[rsp+24]
 lea rcx,[rip+.Ldt_head_word]
 cmp dword ptr [rbx+272],0
 je .Ldt_final_retention
 lea rcx,[rip+.Ldt_tail_word]
.Ldt_final_retention:
 call .Ldt_truncated
 jmp .Ldt_bound_final_end
.Ldt_bound_final_alloc:
 mov eax,5
.Ldt_bound_final_end:
 add rsp,32
 pop rbx
 ret
.text
.globl pg_durable_tool_output_limits
.type pg_durable_tool_output_limits,@function
pg_durable_tool_output_limits:
 push rbx
 push r12
 sub rsp,8
 mov eax,4
 test rdi,rdi
 jz .Ldt_limits_end
 mov rbx,[rdi+56]
 test rbx,rbx
 jz .Ldt_limits_end
 cmp dword ptr [rbx+148],0
 jne .Ldt_limits_end
 cmp dword ptr [rbx+196],0
 jne .Ldt_limits_end
 mov r12,rsi
 test rsi,rsi
 jz .Ldt_limits_end
 cmp qword ptr [rsi],2147483646
 ja .Ldt_limits_end
 cmp dword ptr [rsi+16],1
 ja .Ldt_limits_end
 cmp dword ptr [rsi+20],0
 jne .Ldt_limits_end
 mov rdi,[rsi]
 mov eax,4096
 cmp rdi,rax
 cmovb rdi,rax
 inc rdi
 call malloc@PLT
 test rax,rax
 jz .Ldt_limits_alloc
 mov rdi,[rbx+128]
 mov [rbx+128],rax
 call free@PLT
 mov rax,[r12]
 mov [rbx+248],rax
 mov [rbx+256],rax
 mov [rbx+320],rax
 mov rax,[r12+8]
 mov [rbx+264],rax
 mov [rbx+328],rax
 mov eax,[r12+16]
 mov [rbx+272],eax
 mov [rbx+336],eax
 xor eax,eax
 jmp .Ldt_limits_end
.Ldt_limits_alloc:
 mov eax,5
.Ldt_limits_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.size pg_durable_tool_output_limits,.-pg_durable_tool_output_limits

.text
.Ldt_monotonic:
 sub rsp,24
 mov edi,1
 mov rsi,rsp
 call clock_gettime@PLT
 pxor xmm0,xmm0
 test eax,eax
 jnz .Ldt_clock_end
 cvtsi2sd xmm0,qword ptr [rsp]
 mulsd xmm0,[rip+.Ldt_thousand]
 pxor xmm1,xmm1
 cvtsi2sd xmm1,qword ptr [rsp+8]
 divsd xmm1,[rip+.Ldt_million]
 addsd xmm0,xmm1
.Ldt_clock_end:
 add rsp,24
 ret
.Ldt_duration:
 push rbx
 sub rsp,16
 mov rbx,rdi
 cmp dword ptr [rbx+196],0
 je .Ldt_duration_ok
 pxor xmm0,xmm0
 ucomisd xmm0,[rbx+280]
 jae .Ldt_duration_ok
 mov rdi,rbx
 call .Ldt_ensure_diagnostics
 test rax,rax
 jz .Ldt_duration_bad
 movsd xmm0,[rbx+392]
 pxor xmm1,xmm1
 ucomisd xmm0,xmm1
 ja .Ldt_duration_measured
 call .Ldt_monotonic
.Ldt_duration_measured:
 subsd xmm0,[rbx+280]
 pxor xmm1,xmm1
 maxsd xmm0,xmm1
 addsd xmm0,[rip+.Ldt_half]
 cvttsd2si rdi,xmm0
 call json_object_new_int64@PLT
 test rax,rax
 jz .Ldt_duration_bad
 mov rdi,[rbx+184]
 lea rsi,[rip+.Ldt_duration_ms]
 mov rdx,rax
 call .Ldt_set
 jmp .Ldt_duration_end
.Ldt_duration_bad:
 mov eax,5
 jmp .Ldt_duration_end
.Ldt_duration_ok:
 xor eax,eax
.Ldt_duration_end:
 add rsp,16
 pop rbx
 ret
.Ldt_copy_optional:
 push rbx
 push r12
 push r13
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 test rsi,rsi
 jz .Ldt_optional_ok
 mov rdi,rsi
 mov rsi,rdx
 mov rdx,rsp
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Ldt_optional_ok
 mov rdi,[rsp]
 call json_object_get@PLT
 mov rdi,rbx
 mov rsi,r13
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Ldt_optional_ok
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,5
 jmp .Ldt_optional_end
.Ldt_optional_ok:
 xor eax,eax
.Ldt_optional_end:
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret
.section .rodata
.Ldt_duration_ms: .asciz "durationMs"
.balign 8
.Ldt_thousand: .double 1000.0
.Ldt_million: .double 1000000.0
.Ldt_half: .double 0.5

.text
/* Foreground polling supplies cooperative timer wakeups; the first dirty
   snapshot flushes immediately, later chunks coalesce until the deadline. */
.Ldt_progress_flush:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov r12d,esi
 cmp dword ptr [rbx+368],0
 je .Ldt_flush_ok
 call .Ldt_monotonic
 movsd [rsp],xmm0
 test r12d,r12d
 jnz .Ldt_flush_due
 ucomisd xmm0,[rbx+352]
 jb .Ldt_flush_ok
.Ldt_flush_due:
 mov rdi,[rbx+8]
 mov rsi,rbx
 lea rdx,[rip+.Ldt_progress_stage]
 call pg_task_runner_commit_with@PLT
 test eax,eax
 jnz .Ldt_flush_end
 pxor xmm0,xmm0
 cvtsi2sd xmm0,qword ptr [rbx+360]
 divsd xmm0,[rip+.Ldt_bytes_per_ms]
 maxsd xmm0,[rip+.Ldt_progress_interval]
 addsd xmm0,[rsp]
 movsd [rbx+352],xmm0
 mov qword ptr [rbx+360],0
 mov dword ptr [rbx+368],0
.Ldt_flush_ok:
 xor eax,eax
.Ldt_flush_end:
 add rsp,24
 pop r12
 pop rbx
 ret
/* Phase cleanup has revoked TaskRunner commits; settle the admitted progress
   through the same Session before freeing the invocation's raw window. */
.Ldt_progress_cleanup_flush:
 push rbp
 push rbx
 push r12
 sub rsp,160
 mov rbx,rdi
 mov r12,[rdi+8]
 mov rdi,rsp
 xor eax,eax
 mov ecx,20
 rep stosq
 mov rdi,[r12]
 mov rax,[rdi]
 cmp dword ptr [rax+52],0
 jne .Ldt_cleanup_flush_closed
 mov rax,[r12+8]
 mov rcx,[rax]
 mov r8,[rax+8]
 mov rsi,rsp
 lea rdx,[rsp+88]
 call pg_durable_session_begin@PLT
 test eax,eax
 jnz .Ldt_cleanup_flush_end
 mov rdi,rbx
 mov rsi,rsp
 lea rdx,[rsp+88]
 lea rcx,[rsp+144]
 call .Ldt_progress_stage
 test eax,eax
 jnz .Ldt_cleanup_flush_abort
 mov rdi,[r12]
 xor esi,esi
 xor edx,edx
 xor ecx,ecx
 call pg_durable_session_settle@PLT
 test eax,eax
 jnz .Ldt_cleanup_flush_abort
 mov dword ptr [rbx+368],0
 jmp .Ldt_cleanup_flush_end
.Ldt_cleanup_flush_abort:
 mov ebp,eax
 mov rdi,[r12]
 call pg_durable_session_abort@PLT
 mov eax,ebp
 jmp .Ldt_cleanup_flush_end
.Ldt_cleanup_flush_closed:
 xor eax,eax
.Ldt_cleanup_flush_end:
 add rsp,160
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.balign 8
.Ldt_bytes_per_ms: .double 102.4
.Ldt_progress_interval: .double 100.0

.text
.globl pg_durable_tool_output
.type pg_durable_tool_output,@function
pg_durable_tool_output:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov qword ptr [rsp],0
 call pg_durable_tool_executing@PLT
 test eax,eax
 jz .Ldt_output_bad
 cmp r13,2147483646
 ja .Ldt_output_bad
 test r13,r13
 jz .Ldt_output_ready
 test r12,r12
 jz .Ldt_output_bad
 mov rdi,r13
 call malloc@PLT
 test rax,rax
 jz .Ldt_output_alloc
 mov [rsp],rax
 mov rdi,rax
 mov rsi,r12
 mov rdx,r13
 call memcpy@PLT
.Ldt_output_ready:
 mov rbx,[rbx+56]
 test r14,r14
 jz .Ldt_output_push
 lea rdi,[rbx+288]
 mov rsi,r14
 call pg_output_buffer_skip@PLT
 test eax,eax
 jnz .Ldt_output_end
.Ldt_output_push:
 lea rdi,[rbx+288]
 mov rsi,r12
 mov rdx,r13
 call pg_output_buffer_push@PLT
 test eax,eax
 jnz .Ldt_output_end
 test r13,r13
 jnz .Ldt_output_dirty
 test r14,r14
 jz .Ldt_output_ok
.Ldt_output_dirty:
 mov dword ptr [rbx+368],1
 mov rdi,rbx
 xor esi,esi
 call .Ldt_progress_flush
 test eax,eax
 jnz .Ldt_output_end
 test r13,r13
 jz .Ldt_output_ok
 mov rdi,[rsp]
 mov rsi,r13
 call pg_output_sanitize@PLT
 test rax,rax
 jz .Ldt_output_ok
 mov rcx,rax
 mov rax,[rbx]
 mov rax,[rax]
 mov r8,[rax+32]
 test r8,r8
 jz .Ldt_output_ok
 mov rdi,[rax]
 mov rax,[rbx+8]
 mov rsi,[rax+16]
 mov rdx,[rsp]
 call r8
.Ldt_output_ok:
 xor eax,eax
 jmp .Ldt_output_end
.Ldt_output_bad:
 mov eax,4
 jmp .Ldt_output_end
.Ldt_output_alloc:
 mov eax,5
.Ldt_output_end:
 mov ebp,eax
 mov rdi,[rsp]
 call free@PLT
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_durable_tool_output,.-pg_durable_tool_output
.globl pg_durable_tool_retained_output
.type pg_durable_tool_retained_output,@function
pg_durable_tool_retained_output:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 test r12,r12
 jz .Ldt_retained_bad
 pxor xmm0,xmm0
 movdqu [r12],xmm0
 movdqu [r12+16],xmm0
 call pg_durable_tool_executing@PLT
 test eax,eax
 jz .Ldt_retained_bad
 mov rdi,[rbx+56]
 add rdi,288
 mov rsi,r12
 call pg_output_buffer_snapshot@PLT
 jmp .Ldt_retained_end
.Ldt_retained_bad:
 mov eax,4
.Ldt_retained_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.size pg_durable_tool_retained_output,.-pg_durable_tool_retained_output
.globl pg_durable_tool_output_window
.type pg_durable_tool_output_window,@function
pg_durable_tool_output_window:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 test r12,r12
 jz .Ldt_window_bad
 pxor xmm0,xmm0
 movdqu [r12],xmm0
 movdqu [r12+16],xmm0
 call pg_durable_tool_executing@PLT
 test eax,eax
 jz .Ldt_window_bad
 mov rax,[rbx+56]
 cmp dword ptr [rax+272],0
 je .Ldt_window_head
 mov rcx,[rax+256]
 mov [r12],rcx
 mov rcx,[rax+264]
 mov [r12+8],rcx
 movsd xmm0,[rip+.Ldt_progress_interval]
 movsd [r12+16],xmm0
 mov qword ptr [r12+24],102400
 xor eax,eax
 jmp .Ldt_window_end
.Ldt_window_head:
 mov eax,1
 jmp .Ldt_window_end
.Ldt_window_bad:
 mov eax,4
.Ldt_window_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.size pg_durable_tool_output_window,.-pg_durable_tool_output_window

.text
/* Resolve an omitted final output before afterTool sees the result. */
.Ldt_reported_result:
 push rbx
 sub rsp,32
 mov rbx,rdi
 lea rdi,[rbx+288]
 mov rsi,rsp
 call pg_output_buffer_snapshot@PLT
 test eax,eax
 jnz .Ldt_reported_end
 mov rax,[rsp+8]
 cmp rax,[rbx+248]
 ja .Ldt_reported_bad
 mov [rbx+136],rax
 mov rdi,[rbx+128]
 mov rsi,[rsp]
 mov rdx,rax
 call memcpy@PLT
 mov rax,[rsp+16]
 mov [rbx+376],rax
 mov rax,[rsp+24]
 mov [rbx+384],rax
 mov dword ptr [rbx+372],3
 xor eax,eax
 jmp .Ldt_reported_release
.Ldt_reported_bad:
 mov eax,4
.Ldt_reported_release:
 mov [rsp+8],eax
 mov rdi,[rsp]
 call free@PLT
 mov eax,[rsp+8]
.Ldt_reported_end:
 add rsp,32
 pop rbx
 ret
/* Harness truncation diagnostics follow afterTool and reported diagnostics. */
.Ldt_reported_warning:
 push rbx
 mov rbx,rdi
 test dword ptr [rbx+372],1
 jz .Ldt_reported_warning_ok
 cmp qword ptr [rbx+376],0
 je .Ldt_reported_warning_done
 call .Ldt_ensure_diagnostics
 test rax,rax
 jz .Ldt_reported_warning_alloc
 mov rdi,rax
 mov rsi,[rbx+376]
 mov rdx,[rbx+384]
 lea rcx,[rip+.Ldt_head_word]
 cmp dword ptr [rbx+272],0
 je .Ldt_reported_warning_call
 lea rcx,[rip+.Ldt_tail_word]
.Ldt_reported_warning_call:
 call .Ldt_truncated
 test eax,eax
 jnz .Ldt_reported_warning_end
.Ldt_reported_warning_done:
 and dword ptr [rbx+372],-2
.Ldt_reported_warning_ok:
 xor eax,eax
 jmp .Ldt_reported_warning_end
.Ldt_reported_warning_alloc:
 mov eax,5
.Ldt_reported_warning_end:
 pop rbx
 ret

.section .rodata
.Ldt_control: .asciz "control"

.text
/* Native strings retain a scalar boundary within the 500 UTF-16-unit bound. */
.Ldt_summary_append:
 mov r8,[rdi+2000]
 mov r9,[rdi+2008]
 xor ecx,ecx
 cmp dword ptr [rdi+2048],0
 jne .Ldt_summary_append_end
.Ldt_summary_append_each:
 cmp rcx,rdx
 jae .Ldt_summary_append_end
 movzx eax,byte ptr [rsi+rcx]
 mov r10d,1
 mov r11d,1
 cmp eax,128
 jb .Ldt_summary_append_width
 inc r10d
 cmp eax,224
 jb .Ldt_summary_append_width
 inc r10d
 cmp eax,240
 jb .Ldt_summary_append_width
 inc r10d
 inc r11d
.Ldt_summary_append_width:
 lea rax,[rcx+r10]
 cmp rax,rdx
 ja .Ldt_summary_append_stop
 lea rax,[r9+r11]
 cmp rax,500
 ja .Ldt_summary_append_stop
 add r9,r11
.Ldt_summary_append_copy:
 mov al,[rsi+rcx]
 mov [rdi+r8],al
 inc rcx
 inc r8
 dec r10d
 jnz .Ldt_summary_append_copy
 jmp .Ldt_summary_append_each
.Ldt_summary_append_stop:
 mov dword ptr [rdi+2048],1
.Ldt_summary_append_end:
 mov [rdi+2000],r8
 mov [rdi+2008],r9
 ret
/* Keep bounded error text in the done slot without retaining a large result. */
.Ldt_error_summary:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,2064
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp+2000],0
 mov qword ptr [rsp+2008],0
 mov dword ptr [rsp+2016],0
 mov dword ptr [rsp+2048],0
 cmp dword ptr [rbx+164],0
 je .Ldt_summary_ok
 DGET "qword ptr [rbx+184]",diagnostics
 mov r13,rax
 xor r14d,r14d
.Ldt_summary_diagnostic_each:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Ldt_summary_fallback
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbp,rax
 DGET rax,severity
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Ldt_summary_next
 mov rdi,rax
 lea rsi,[rip+.Ldt_error]
 call strcmp@PLT
 test eax,eax
 jnz .Ldt_summary_next
 cmp dword ptr [rsp+2016],0
 je .Ldt_summary_message
 mov rdi,rsp
 lea rsi,[rip+.Ldt_summary_newline]
 mov edx,1
 call .Ldt_summary_append
.Ldt_summary_message:
 mov dword ptr [rsp+2016],1
 DGET rbp,message
 mov rbp,rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 mov [rsp+2024],rax
 mov rdi,rbp
 call json_object_get_string@PLT
 mov rdi,rsp
 mov rsi,rax
 mov rdx,[rsp+2024]
 call .Ldt_summary_append
 cmp dword ptr [rsp+2048],0
 jne .Ldt_summary_install
 cmp qword ptr [rsp+2008],500
 jae .Ldt_summary_install
.Ldt_summary_next:
 inc r14
 jmp .Ldt_summary_diagnostic_each
.Ldt_summary_fallback:
 cmp dword ptr [rsp+2016],0
 jne .Ldt_summary_install
 mov rdi,rsp
 mov rsi,[rbx+128]
 mov rdx,[rbx+136]
 call .Ldt_summary_append
.Ldt_summary_install:
 cmp qword ptr [rsp+2000],0
 je .Ldt_summary_ok
 mov rdi,rsp
 mov esi,[rsp+2000]
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Ldt_summary_alloc
 mov rdi,r12
 lea rsi,[rip+.Ldt_error]
 mov rdx,rax
 call .Ldt_set
 jmp .Ldt_summary_end
.Ldt_summary_ok:
 xor eax,eax
 jmp .Ldt_summary_end
.Ldt_summary_alloc:
 mov eax,5
.Ldt_summary_end:
 add rsp,2064
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Ldt_summary_newline: .ascii "\n"

.text
.globl pg_durable_tool_progress_recorded
.type pg_durable_tool_progress_recorded,@function
pg_durable_tool_progress_recorded:
 push rbx
 sub rsp,16
 mov [rsp+8],rsi
 mov rbx,rdi
 call pg_durable_tool_executing@PLT
 test eax,eax
 jz .Ldt_recorded_end
 cmp qword ptr [rsp+8],0
 je .Ldt_recorded_end
 mov rbx,[rbx+56]
 call .Ldt_monotonic
 movsd [rsp],xmm0
 pxor xmm0,xmm0
 cvtsi2sd xmm0,qword ptr [rsp+8]
 divsd xmm0,[rip+.Ldt_bytes_per_ms]
 maxsd xmm0,[rip+.Ldt_progress_interval]
 addsd xmm0,[rsp]
 maxsd xmm0,[rbx+352]
 movsd [rbx+352],xmm0
.Ldt_recorded_end:
 add rsp,16
 pop rbx
 ret
.size pg_durable_tool_progress_recorded,.-pg_durable_tool_progress_recorded

.text
/* code and borrowed length-delimited message become an owned diagnostic. */
.Ldt_contract_diagnostic:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov qword ptr [rsp],0
 call .Ldt_ensure_diagnostics
 mov rbp,rax
 test rax,rax
 jz .Ldt_contract_diag_bad
 lea rdi,[rip+.Ldt_contract_diag_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Ldt_contract_diag_bad
 mov rdi,r12
 call json_object_new_string@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Ldt_contract_code]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_contract_diag_end
 mov rdi,r13
 mov esi,r14d
 call json_object_new_string_len@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Ldt_message]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_contract_diag_end
 mov rdi,rbp
 mov rsi,[rsp]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ldt_contract_diag_bad
 mov qword ptr [rsp],0
 xor eax,eax
 jmp .Ldt_contract_diag_end
.Ldt_contract_diag_bad:
 mov eax,5
.Ldt_contract_diag_end:
 mov [rsp+8],eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,[rsp+8]
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Ldt_contract_result:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov rdi,rsp
 xor eax,eax
 mov ecx,8
 rep stosq
 cmp dword ptr [rbx+408],0
 jne .Ldt_contract_ok
 mov rax,[rbx+8]
 DGET "qword ptr [rax+32]",input
 DGET rax,kind
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Ldt_nested]
 call strcmp@PLT
 test eax,eax
 setz al
 movzx eax,al
 mov [rsp+48],eax
 DGET "qword ptr [rbx+16]",name
 mov rdi,rax
 call json_object_get_string@PLT
 mov r12,rax
.Ldt_contract_value:
 mov rdi,[rbx+184]
 lea rsi,[rip+.Ldt_structured_output]
 lea rdx,[rsp+16]
 call json_object_object_get_ex@PLT
 mov [rsp+52],eax
 cmp qword ptr [rbx+400],0
 jne .Ldt_contract_declared
 test eax,eax
 jz .Ldt_contract_done
 lea rsi,[rip+.Ldt_contract_undeclared_format]
 jmp .Ldt_contract_format_name
.Ldt_contract_declared:
 cmp dword ptr [rsp+52],0
 jne .Ldt_contract_validate
 cmp dword ptr [rbx+164],0
 jne .Ldt_contract_error_text
 lea rsi,[rip+.Ldt_contract_missing_format]
 jmp .Ldt_contract_format_name
.Ldt_contract_validate:
 mov rdi,[rbx+400]
 mov rsi,[rsp+16]
 mov rdx,rsp
 call pg_json_schema_validate@PLT
 test eax,eax
 jz .Ldt_contract_done
 cmp eax,1
 jne .Ldt_contract_end
 mov rcx,[rsp]
 test rcx,rcx
 jnz .Ldt_contract_validation_detail
 lea rcx,[rip+.Ldt_contract_invalid_default]
.Ldt_contract_validation_detail:
 lea rdi,[rsp+8]
 lea rsi,[rip+.Ldt_contract_invalid_format]
 mov rdx,r12
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Ldt_contract_bad
 jmp .Ldt_contract_broken
.Ldt_contract_format_name:
 lea rdi,[rsp+8]
 mov rdx,r12
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Ldt_contract_bad
.Ldt_contract_broken:
 mov rdi,[rbx+184]
 lea rsi,[rip+.Ldt_structured_output]
 call json_object_object_del@PLT
 cmp dword ptr [rsp+48],0
 je .Ldt_contract_report
 mov dword ptr [rbx+164],1
 mov rdi,[rsp+8]
 call strlen@PLT
 mov rcx,rax
 mov rdi,rbx
 lea rsi,[rip+.Ldt_contract_invalid_code]
 mov rdx,[rsp+8]
 call .Ldt_contract_diagnostic
 test eax,eax
 jnz .Ldt_contract_end
 jmp .Ldt_contract_done
.Ldt_contract_report:
 mov rax,[rbx]
 mov rax,[rax]
 cmp qword ptr [rax+56],0
 je .Ldt_contract_done
 mov rdi,[rax]
 mov rcx,[rbx+8]
 mov rsi,[rcx+16]
 mov rdx,[rsp+8]
 call [rax+56]
 jmp .Ldt_contract_done
.Ldt_contract_error_text:
 cmp dword ptr [rsp+48],0
 je .Ldt_contract_done
 DGET "qword ptr [rbx+184]",diagnostics
 mov r13,rax
 xor r14d,r14d
.Ldt_contract_error_each:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Ldt_contract_error_output
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 DGET rax,severity
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Ldt_contract_error_next
 mov rdi,rax
 lea rsi,[rip+.Ldt_error]
 call strcmp@PLT
 test eax,eax
 jz .Ldt_contract_done
.Ldt_contract_error_next:
 inc r14
 jmp .Ldt_contract_error_each
.Ldt_contract_error_output:
 cmp qword ptr [rbx+136],0
 je .Ldt_contract_done
 mov rdi,rbx
 lea rsi,[rip+.Ldt_contract_tool_error_code]
 mov rdx,[rbx+128]
 mov rcx,[rbx+136]
 call .Ldt_contract_diagnostic
 test eax,eax
 jnz .Ldt_contract_end
.Ldt_contract_done:
 mov dword ptr [rbx+408],1
.Ldt_contract_ok:
 xor eax,eax
 jmp .Ldt_contract_end
.Ldt_contract_bad:
 mov eax,5
.Ldt_contract_end:
 mov ebp,eax
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Ldt_schema_declared: .asciz "structuredOutputDeclared"
.Ldt_contract_code: .asciz "code"
.Ldt_contract_diag_json: .asciz "{\"severity\":\"error\",\"code\":\"\",\"message\":\"\"}"
.Ldt_contract_invalid_code: .asciz "invalid_structured_output"
.Ldt_contract_tool_error_code: .asciz "tool_error"
.Ldt_contract_invalid_default: .asciz "root: invalid"
.Ldt_contract_undeclared_format: .asciz "Tool %s returned structuredOutput but declares no structuredOutputSchema"
.Ldt_contract_missing_format: .asciz "Tool %s returned no structuredOutput"
.Ldt_contract_invalid_format: .asciz "Tool %s returned structuredOutput that does not match its schema: %s"

.text
/* Pin the declaration before execute starts effects; settlement only validates. */
.Ldt_contract_prepare:
 push rbp
 push rbx
 sub rsp,24
 mov rbx,rdi
 mov qword ptr [rsp],0
 cmp dword ptr [rbx+412],0
 jne .Ldt_contract_prepare_ok
 mov rax,[rbx]
 mov rax,[rax]
 cmp qword ptr [rax+48],0
 je .Ldt_contract_prepare_done
 mov rdi,[rax]
 mov rcx,[rbx+8]
 mov rsi,[rcx+16]
 mov rdx,[rbx+16]
 call [rax+48]
 test rax,rax
 jz .Ldt_contract_prepare_done
 mov rdi,rax
 lea rsi,[rbx+400]
 mov rdx,rsp
 call pg_json_schema_compile@PLT
 test eax,eax
 jnz .Ldt_contract_prepare_end
.Ldt_contract_prepare_done:
 mov dword ptr [rbx+412],1
.Ldt_contract_prepare_ok:
 xor eax,eax
.Ldt_contract_prepare_end:
 mov ebp,eax
 mov rdi,[rsp]
 call free@PLT
 mov eax,ebp
 add rsp,24
 pop rbx
 pop rbp
 ret
