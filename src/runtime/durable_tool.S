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
 lea rax,[rip+.Ldt_task_name]
 mov [rdi+8],rax
 mov qword ptr [rdi+16],10
 mov qword ptr [rdi+24],2
 mov [rdi+32],rdi
 lea rax,[rip+.Ldt_initial]
 mov [rdi+40],rax
 lea rax,[rdi+88]
 mov [rdi+48],rax
 mov qword ptr [rdi+56],2
 lea rax,[rip+.Ldt_abort]
 mov [rdi+64],rax
 lea rax,[rip+.Ldt_cleanup]
 mov [rdi+72],rax
 mov qword ptr [rdi+80],0
 lea rax,[rip+.Ldt_call_phase]
 mov [rdi+88],rax
 mov qword ptr [rdi+96],4
 mov [rdi+104],rdi
 lea rax,[rip+.Ldt_poll]
 mov [rdi+112],rax
 lea rax,[rip+.Ldt_cleanup]
 mov [rdi+120],rax
 lea rax,[rip+.Ldt_execute_phase]
 mov [rdi+128],rax
 mov qword ptr [rdi+136],7
 mov [rdi+144],rdi
 lea rax,[rip+.Ldt_poll]
 mov [rdi+152],rax
 lea rax,[rip+.Ldt_cleanup]
 mov [rdi+160],rax
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
 mov esi,4272
 call calloc@PLT
 mov r14,rax
 test rax,rax
 jz .Ldt_open_alloc
 mov [rax],r12
 mov [rax+8],rbx
 mov [rbx+56],rax
 DGET r13,input
 mov r12,rax
 DGET rax,kind
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Ldt_open_invalid
 mov rdi,rax
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
 call json_object_get@PLT
 mov [r14+16],rax
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
 mov dword ptr [r14+4244],1
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
 mov [r14+4248],eax
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
 cmp dword ptr [r12+4236],0
 je .Ldt_cleanup_json
 mov rax,[r12+80]
 /* Unsettled async ownership returns to the host drain on phase cancellation. */
 cmp dword ptr [r12+4260],0
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
 mov rdi,[r12+16]
 call json_object_put@PLT
 mov rdi,[r12+24]
 call json_object_put@PLT
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
 sub rsp,48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rcx
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
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
 mov edi,[rbx+4252]
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
 lea rdi,[rbx+128]
 mov esi,[rbx+4224]
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Ldt_stage_alloc
 mov rdi,rbp
 lea rsi,[rip+.Ldt_text]
 mov rdx,rax
 call .Ldt_set
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
 lea rdi,[rip+.Ldt_outcome_json]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Ldt_stage_alloc
 DGET rax,outcome
 mov r14,rax
 mov eax,[rbx+4256]
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
 cmp dword ptr [rbx+4256],1
 jne .Ldt_stage_receipt
 lea rdi,[rip+.Ldt_error_json]
 call json_tokener_parse@PLT
 mov rbp,rax
 test rax,rax
 jz .Ldt_stage_alloc
 lea rdi,[rbx+128]
 mov esi,[rbx+4224]
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
.Ldt_stage_receipt:
 DGET r14,result
 mov rbp,rax
 DGET "qword ptr [rsp+8]",id
 mov r14,rax
 DREF rbp,entry_id,r14
 mov rax,[rsp+16]
 mov [r13],rax
 mov qword ptr [rsp+16],0
 xor eax,eax
 jmp .Ldt_stage_end
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
 mov eax,ebp
 add rsp,48
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
 mov [rbx+4224],rax
 lea rdi,[rbx+128]
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
 mov dword ptr [r12+4252],1
 mov dword ptr [r12+4256],2
 mov rdi,r12
 lea rsi,[rip+.Ldt_aborted_message]
 call .Ldt_message_output
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
 cmp dword ptr [r14+4264],0
 jne .Ldt_poll_final
 cmp dword ptr [r14+4236],0
 jne .Ldt_poll_active
 mov rax,[r12]
 mov rdi,[rax]
 mov rsi,[rbx+16]
 lea rdx,[r14+32]
 call [rax+8]
 cmp eax,1
 je .Ldt_poll_pending
 test eax,eax
 jnz .Ldt_poll_unavailable
 mov dword ptr [r14+4236],1
 cmp dword ptr [r14+4244],0
 je .Ldt_poll_intent
 cmp dword ptr [r14+4248],1
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
 jmp .Ldt_poll_execute
.Ldt_poll_intent:
 lea rdi,[rip+.Ldt_intent_json]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Ldt_poll_alloc
 DGET rax,checkpoint
 mov r13,rax
 mov rdi,[r14+24]
 call json_object_get@PLT
 mov rdi,r13
 lea rsi,[rip+.Ldt_arguments]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_poll_intent_error
 xor ebp,ebp
 mov rax,[r14+56]
 test rax,rax
 jz .Ldt_poll_replay
 mov rdi,[r14+32]
 mov rsi,[r14+88]
 mov rdx,[r14+96]
 call rax
 cmp eax,1
 sete bpl
.Ldt_poll_replay:
 lea rdi,[rip+.Ldt_unsafe]
 test ebp,ebp
 jz .Ldt_poll_replay_string
 lea rdi,[rip+.Ldt_safe]
.Ldt_poll_replay_string:
 call json_object_new_string@PLT
 mov rdi,r13
 lea rsi,[rip+.Ldt_replay]
 mov rdx,rax
 call .Ldt_set
 test eax,eax
 jnz .Ldt_poll_intent_error
 mov rdi,rbx
 mov rsi,[rsp+16]
 call pg_task_runner_commit@PLT
.Ldt_poll_intent_error:
 mov ebp,eax
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,ebp
 test eax,eax
 jnz .Ldt_poll_error
.Ldt_poll_execute:
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
 lea r9,[r14+128]
 mov qword ptr [rsp],4096
 lea rax,[r14+4224]
 mov [rsp+8],rax
 call [r14+48]
 jmp .Ldt_poll_result
.Ldt_poll_active:
 cmp dword ptr [r14+4232],2
 jb .Ldt_poll_close
 cmp dword ptr [r14+4232],3
 ja .Ldt_poll_close
 mov rax,[r14+64]
 test rax,rax
 jz .Ldt_poll_invalid
 mov rdi,[r14+32]
 lea rsi,[r14+128]
 mov edx,4096
 lea rcx,[r14+4224]
 call rax
.Ldt_poll_result:
 cmp eax,4
 ja .Ldt_poll_invalid
 mov [r14+4232],eax
 cmp qword ptr [r14+4224],4096
 ja .Ldt_poll_invalid
 cmp eax,2
 je .Ldt_poll_pending
 cmp eax,3
 jne .Ldt_poll_end_result
 mov rax,[r12]
 mov rcx,[rax+32]
 test rcx,rcx
 jz .Ldt_poll_pending
 mov rdi,[rax]
 mov rsi,[rbx+16]
 lea rdx,[r14+128]
 mov r8,rcx
 mov rcx,[r14+4224]
 call r8
 jmp .Ldt_poll_pending
.Ldt_poll_end_result:
 cmp eax,1
 sete al
 movzx eax,al
 mov [r14+4252],eax
.Ldt_poll_close:
 cmp dword ptr [r14+4236],0
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
 mov dword ptr [r14+4260],1
.Ldt_poll_final:
 mov dword ptr [r14+4264],1
 mov rdi,r14
 call .Ldt_finish
 test eax,eax
 jnz .Ldt_poll_error
 xor eax,eax
 jmp .Ldt_poll_return
.Ldt_poll_close_error:
 mov dword ptr [r14+4260],1
 mov dword ptr [r14+4252],1
 mov rdi,r14
 lea rsi,[rip+.Ldt_close_message]
 call .Ldt_message_output
 jmp .Ldt_poll_final
.Ldt_poll_interrupted:
 mov dword ptr [r14+4252],1
 mov dword ptr [r14+4256],1
 mov rdi,r14
 lea rsi,[rip+.Ldt_interrupted_message]
 call .Ldt_message_output
 jmp .Ldt_poll_close
.Ldt_poll_unavailable:
 cmp dword ptr [r14+4244],0
 jne .Ldt_poll_interrupted
 mov dword ptr [r14+4252],1
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
