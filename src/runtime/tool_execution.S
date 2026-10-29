/* Pi Durable harness/tool.ts ToolExecutionApi/executeTool, d5b3de22.
   MIT docs/licenses/pi.md. */
.intel_syntax noprefix
.macro EF n
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\n
.endm
.macro EE n
 add rsp,\n
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.macro EG obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lte_\key]
 call json_object_object_get@PLT
.endm
.text
/* add consumes value on either outcome. */
.Lte_add:
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Lte_add_end
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,5
.Lte_add_end:
 pop rbx
 ret
/* Copy a present field, including JSON null. */
.Lte_copy:
 EF 16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,r12
 mov rsi,r13
 mov rdx,rsp
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lte_copy_end
 mov rdi,[rsp]
 call json_object_get@PLT
 mov rdi,rbx
 mov rsi,r13
 mov rdx,rax
 call .Lte_add
.Lte_copy_end:
 EE 16

.globl pg_tool_execution_start
.type pg_tool_execution_start,@function
pg_tool_execution_start:
 EF 128
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rbp,r8
 xor eax,eax
 mov rdi,rsp
 mov ecx,16
 rep stosq
 test rbp,rbp
 jz .Lte_start_invalid
 mov qword ptr [rbp],0
 test rbx,rbx
 jz .Lte_start_invalid
 test r12,r12
 jz .Lte_start_invalid
 cmp byte ptr [r12],0
 je .Lte_start_invalid
 mov rdi,[rbx]
 call pg_durable_tool_executing@PLT
 test eax,eax
 jz .Lte_start_invalid
 mov rdi,r13
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lte_start_invalid
 mov rdi,r13
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lte_start_invalid
 test r14,r14
 jz .Lte_sequence
 cmp byte ptr [r14],0
 je .Lte_start_invalid
 mov rdi,r14
 mov esi,'/'
 call strchr@PLT
 test rax,rax
 jnz .Lte_start_invalid
 mov rdi,r14
 lea rsi,[rip+.Lte_proto]
 call strcmp@PLT
 test eax,eax
 jz .Lte_start_invalid
 movzx eax,byte ptr [r14]
 cmp al,'1'
 jb .Lte_key_ready
 cmp al,'9'
 ja .Lte_key_ready
 mov rdx,r14
.Lte_numeric_key:
 inc rdx
 movzx eax,byte ptr [rdx]
 test al,al
 jz .Lte_start_invalid
 cmp al,'0'
 jb .Lte_key_ready
 cmp al,'9'
 jbe .Lte_numeric_key
 jmp .Lte_key_ready
.Lte_sequence:
 mov rcx,[rbx+64]
 add rcx,1
 jc .Lte_start_invalid
 mov [rbx+64],rcx
 lea rdi,[rsp+80]
 mov esi,32
 lea rdx,[rip+.Lte_sequence_format]
 xor eax,eax
 call snprintf@PLT
 lea r14,[rsp+80]
.Lte_key_ready:
 cmp dword ptr [rbx+72],0
 jne .Lte_memo_ready
 mov edi,1
 call json_object_new_boolean@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lte_start_alloc
 lea rax,[rsp+16]
 mov [rsp],rax
 mov rdi,rbx
 lea rsi,[rip+.Lte_memo]
 mov edx,26
 mov rcx,[rsp+24]
 mov r8d,1
 lea r9,[rsp+8]
 call pg_task_access_memo@PLT
 test eax,eax
 jnz .Lte_start_finish
 mov dword ptr [rbx+72],1
.Lte_memo_ready:
 mov rdi,[rbx+56]
 test rdi,rdi
 jz .Lte_start_invalid
 call strlen@PLT
 mov [rsp+72],rax
 mov rdi,r14
 call strlen@PLT
 add rax,[rsp+72]
 jc .Lte_start_invalid
 add rax,2
 jc .Lte_start_invalid
 mov [rsp+72],rax
 mov rdi,rax
 call malloc@PLT
 mov [rsp+56],rax
 test rax,rax
 jz .Lte_start_alloc
 mov rdi,rax
 mov rsi,[rsp+72]
 lea rdx,[rip+.Lte_id_format]
 mov rcx,[rbx+56]
 mov r8,r14
 xor eax,eax
 call snprintf@PLT
 test eax,eax
 js .Lte_start_invalid
 lea rdi,[rip+.Lte_call_json]
 call json_tokener_parse@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lte_start_alloc
 mov rdi,[rsp+56]
 call json_object_new_string@PLT
 test rax,rax
 jz .Lte_start_alloc
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lte_id]
 mov rdx,rax
 call .Lte_add
 test eax,eax
 jnz .Lte_start_finish
 mov rdi,r12
 call json_object_new_string@PLT
 test rax,rax
 jz .Lte_start_alloc
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lte_name]
 mov rdx,rax
 call .Lte_add
 test eax,eax
 jnz .Lte_start_finish
 mov rdi,r13
 lea rsi,[rsp+48]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lte_start_alloc
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lte_arguments]
 mov rdx,[rsp+48]
 mov qword ptr [rsp+48],0
 call .Lte_add
 test eax,eax
 jnz .Lte_start_finish
 mov eax,[rbx+76]
 xor eax,1
 mov [rsp],rax
 mov rdi,[rbx]
 mov rsi,[rbx+40]
 mov rdx,r14
 mov rcx,[rsp+32]
 mov r8,[rbx+48]
 mov r9,rbp
 call pg_nested_call_start@PLT
 jmp .Lte_start_finish
.Lte_start_invalid:
 mov eax,4
 jmp .Lte_start_finish
.Lte_start_alloc:
 mov eax,5
.Lte_start_finish:
 mov [rsp+64],eax
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call free@PLT
 mov eax,[rsp+64]
 EE 128
.size pg_tool_execution_start,.-pg_tool_execution_start

.globl pg_tool_execution_poll
.type pg_tool_execution_poll,@function
pg_tool_execution_poll:
 EF 64
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rsp
 xor eax,eax
 mov ecx,8
 rep stosq
 test r13,r13
 jz .Lte_poll_invalid
 mov qword ptr [r13],0
 test rbx,rbx
 jz .Lte_poll_invalid
 test r12,r12
 jz .Lte_poll_invalid
 mov rdi,[rbx]
 call pg_durable_tool_executing@PLT
 test eax,eax
 jz .Lte_poll_invalid
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rsp
 call pg_task_access_get_task@PLT
 cmp eax,1
 je .Lte_poll_invalid
 test eax,eax
 jnz .Lte_poll_finish
 EG "qword ptr [rsp]",input
 mov rbp,rax
 EG rbp,kind
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lte_poll_invalid
 mov rdi,rax
 lea rsi,[rip+.Lte_nested]
 call strcmp@PLT
 test eax,eax
 jnz .Lte_poll_invalid
 EG rbp,parent
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rbx+24]
 jne .Lte_poll_invalid
 EG "qword ptr [rsp]",state
 EG rax,status
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lte_poll_invalid
 mov rdi,rax
 lea rsi,[rip+.Lte_terminal]
 call strcmp@PLT
 test eax,eax
 jnz .Lte_poll_pending
 EG rbp,key
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdx,rax
 mov rax,[rbx]
 mov rdi,[rax]
 mov rsi,[rbx+24]
 lea rcx,[rsp+8]
 call pg_nested_call_read@PLT
 test eax,eax
 jnz .Lte_poll_finish
 EG "qword ptr [rsp+8]",result
 mov r14,rax
 lea rdi,[rip+.Lte_result_json]
 test r14,r14
 jnz .Lte_poll_result
 lea rdi,[rip+.Lte_fallback_json]
.Lte_poll_result:
 call json_tokener_parse@PLT
 mov rbp,rax
 mov [rsp+16],rax
 test rax,rax
 jz .Lte_poll_alloc
 mov rdi,r12
 call json_object_new_uint64@PLT
 test rax,rax
 jz .Lte_poll_alloc
 mov rdi,rbp
 lea rsi,[rip+.Lte_taskId]
 mov rdx,rax
 call .Lte_add
 test eax,eax
 jnz .Lte_poll_finish
 test r14,r14
 jz .Lte_poll_fallback
 mov rdi,rbp
 mov rsi,r14
 lea rdx,[rip+.Lte_isError]
 call .Lte_copy
 test eax,eax
 jnz .Lte_poll_finish
 mov rdi,rbp
 mov rsi,r14
 lea rdx,[rip+.Lte_details]
 call .Lte_copy
 test eax,eax
 jnz .Lte_poll_finish
 mov rdi,rbp
 mov rsi,r14
 lea rdx,[rip+.Lte_diagnostics]
 call .Lte_copy
 test eax,eax
 jnz .Lte_poll_finish
 mov rdi,rbp
 mov rsi,r14
 lea rdx,[rip+.Lte_usage]
 call .Lte_copy
 test eax,eax
 jnz .Lte_poll_finish
 mov rdi,rbp
 mov rsi,r14
 lea rdx,[rip+.Lte_durationMs]
 call .Lte_copy
 test eax,eax
 jnz .Lte_poll_finish
 mov rdi,r14
 lea rsi,[rip+.Lte_structuredOutput]
 lea rdx,[rsp+24]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Lte_poll_structured
 EG r14,structuredOutputDeclared
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lte_poll_transfer
 EG r14,content
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 EG rax,text
 mov [rsp+24],rax
.Lte_poll_structured:
 mov rdi,[rsp+24]
 call json_object_get@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lte_structuredOutput]
 mov rdx,rax
 call .Lte_add
 test eax,eax
 jnz .Lte_poll_finish
 jmp .Lte_poll_transfer
.Lte_poll_fallback:
 EG "qword ptr [rsp]",state
 EG rax,outcome
 mov [rsp+40],rax
 EG rax,status
 mov r12,rax
 mov rdi,rbp
 lea rsi,[rip+.Lte_diagnostics]
 call json_object_object_get@PLT
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,r12
 call json_object_get@PLT
 mov rdi,r14
 lea rsi,[rip+.Lte_code]
 mov rdx,rax
 call .Lte_add
 test eax,eax
 jnz .Lte_poll_finish
 EG "qword ptr [rsp+40]",error
 EG rax,message
 test rax,rax
 jnz .Lte_fallback_reason
 EG "qword ptr [rsp+40]",reason
.Lte_fallback_reason:
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+56],rax
 mov rdi,r12
 call json_object_get_string@PLT
 mov r12,rax
 lea rax,[rip+.Lte_ended_format]
 mov [rsp+40],rax
 mov rdi,r12
 lea rsi,[rip+.Lte_faulted]
 call strcmp@PLT
 test eax,eax
 jnz .Lte_fallback_orphan
 lea rax,[rip+.Lte_failed_format]
 mov [rsp+40],rax
 jmp .Lte_fallback_format
.Lte_fallback_orphan:
 mov rdi,r12
 lea rsi,[rip+.Lte_orphaned]
 call strcmp@PLT
 test eax,eax
 jnz .Lte_fallback_format
 lea rax,[rip+.Lte_orphan_format]
 mov [rsp+40],rax
.Lte_fallback_format:
 EG "qword ptr [rsp]",input
 EG rax,call
 EG rax,name
 mov rdi,rax
 call json_object_get_string@PLT
 mov r12,rax
 mov qword ptr [rsp+48],0
 lea rdi,[rsp+48]
 mov rsi,[rsp+40]
 mov rdx,r12
 mov rcx,[rsp+56]
 test rcx,rcx
 jnz .Lte_fallback_printf
 lea rcx,[rip+.Lte_unknown_reason]
.Lte_fallback_printf:
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lte_poll_alloc
 mov rdi,[rsp+48]
 call json_object_new_string@PLT
 mov r12,rax
 mov rdi,[rsp+48]
 call free@PLT
 test r12,r12
 jz .Lte_poll_alloc
 mov rdi,r14
 lea rsi,[rip+.Lte_message]
 mov rdx,r12
 call .Lte_add
 test eax,eax
 jnz .Lte_poll_finish
.Lte_poll_transfer:
 mov [r13],rbp
 mov qword ptr [rsp+16],0
 xor eax,eax
 jmp .Lte_poll_finish
.Lte_poll_pending:
 mov eax,1
 jmp .Lte_poll_finish
.Lte_poll_invalid:
 mov eax,4
 jmp .Lte_poll_finish
.Lte_poll_alloc:
 mov eax,5
.Lte_poll_finish:
 mov [rsp+32],eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,[rsp+32]
 EE 64
.size pg_tool_execution_poll,.-pg_tool_execution_poll
.section .rodata
.Lte_proto: .asciz "__proto__"
.Lte_memo: .asciz "pangu.tool.madeNestedCalls"
.Lte_memo_end:
.Lte_sequence_format: .asciz "%llu"
.Lte_id_format: .asciz "%s/%s"
.Lte_call_json: .asciz "{\"type\":\"toolCall\"}"
.Lte_result_json: .asciz "{\"isError\":false,\"diagnostics\":[]}"
.Lte_fallback_json: .asciz "{\"isError\":true,\"diagnostics\":[{\"severity\":\"error\",\"code\":\"nested_task_failed\",\"message\":\"Nested tool ended without a stored result\"}]}"
.Lte_id: .asciz "id"
.Lte_taskId: .asciz "taskId"
.Lte_name: .asciz "name"
.Lte_arguments: .asciz "arguments"
.Lte_input: .asciz "input"
.Lte_kind: .asciz "kind"
.Lte_nested: .asciz "nested"
.Lte_parent: .asciz "parent"
.Lte_state: .asciz "state"
.Lte_status: .asciz "status"
.Lte_terminal: .asciz "terminal"
.Lte_key: .asciz "key"
.Lte_result: .asciz "result"
.Lte_isError: .asciz "isError"
.Lte_details: .asciz "details"
.Lte_diagnostics: .asciz "diagnostics"
.Lte_usage: .asciz "usage"
.Lte_structuredOutput: .asciz "structuredOutput"
.Lte_content: .asciz "content"
.Lte_text: .asciz "text"
.section .note.GNU-stack,"",@progbits

.section .rodata
.Lte_outcome: .asciz "outcome"
.Lte_error: .asciz "error"
.Lte_message: .asciz "message"
.Lte_reason: .asciz "reason"
.Lte_code: .asciz "code"

.section .rodata
.Lte_faulted: .asciz "faulted"
.Lte_orphaned: .asciz "orphaned"
.Lte_failed_format: .asciz "Tool %s failed: %s"
.Lte_orphan_format: .asciz "Tool %s could not resume: %s"
.Lte_ended_format: .asciz "Tool %s ended without a result"
.Lte_unknown_reason: .asciz "Unknown task outcome"
.Lte_call: .asciz "call"
.text
.globl pg_tool_execution_details
.type pg_tool_execution_details,@function
pg_tool_execution_details:
 mov rdx,rsi
 xor esi,esi
 jmp .Lte_report
.size pg_tool_execution_details,.-pg_tool_execution_details
.globl pg_tool_execution_diagnostic
.type pg_tool_execution_diagnostic,@function
pg_tool_execution_diagnostic:
 mov rdx,rsi
 mov esi,1
 jmp .Lte_report
.size pg_tool_execution_diagnostic,.-pg_tool_execution_diagnostic
.Lte_report:
 EF 96
 mov rbx,rdi
 mov r12,rdx
 mov r13d,esi
 mov rdi,rsp
 xor eax,eax
 mov ecx,12
 rep stosq
 test rbx,rbx
 jz .Lte_report_invalid
 mov rdi,[rbx]
 call pg_durable_tool_executing@PLT
 test eax,eax
 jz .Lte_report_invalid
 test r12,r12
 jnz .Lte_report_value
 test r13d,r13d
 jnz .Lte_report_invalid
 jmp .Lte_report_valid
.Lte_report_value:
 mov rdi,r12
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lte_report_invalid
 test r13d,r13d
 jz .Lte_report_valid
 mov rdi,r12
 call .Lte_diagnostic_valid
 test eax,eax
 jz .Lte_report_invalid
.Lte_report_valid:
 mov rax,[rbx]
 mov [rsp],rax
 mov rax,[rbx+48]
 mov [rsp+8],rax
 mov rax,[rbx+32]
 mov [rsp+24],rax
 mov rax,[rbx]
 EG "qword ptr [rax+32]",input
 EG rax,kind
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lte_nested]
 call strcmp@PLT
 test eax,eax
 jz .Lte_report_bucket
 mov dword ptr [rsp+32],2
.Lte_report_bucket:
 call json_object_new_object@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lte_report_alloc
 test r12,r12
 jz .Lte_report_copy_ready
 mov rdi,r12
 lea rsi,[rsp+56]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lte_report_alloc
.Lte_report_copy_ready:
 test r13d,r13d
 jnz .Lte_report_diagnostic
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lte_details]
 mov rdx,[rsp+56]
 mov qword ptr [rsp+56],0
 call .Lte_add
 test eax,eax
 jnz .Lte_report_finish
 jmp .Lte_report_commit
.Lte_report_diagnostic:
 mov rax,[rbx]
 mov rdi,[rax]
 mov rsi,[rbx+32]
 mov rdx,[rbx+24]
 lea rcx,[rsp+48]
 call pg_tool_live_read@PLT
 test eax,eax
 jnz .Lte_report_finish
 EG "qword ptr [rsp+48]",diagnostics
 test rax,rax
 jz .Lte_report_new_array
 mov rdi,rax
 lea rsi,[rsp+72]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lte_report_alloc
 jmp .Lte_report_append
.Lte_report_new_array:
 call json_object_new_array@PLT
 mov [rsp+72],rax
 test rax,rax
 jz .Lte_report_alloc
.Lte_report_append:
 mov rdi,[rsp+72]
 mov rsi,[rsp+56]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lte_report_alloc
 mov qword ptr [rsp+56],0
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lte_diagnostics]
 mov rdx,[rsp+72]
 mov qword ptr [rsp+72],0
 call .Lte_add
 test eax,eax
 jnz .Lte_report_finish
.Lte_report_commit:
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rip+.Lte_report_stage]
 call pg_task_runner_commit_with@PLT
 test eax,eax
 jnz .Lte_report_finish
 mov rdi,[rbx]
 mov rsi,[rsp+88]
 call pg_durable_tool_progress_recorded@PLT
 xor eax,eax
 jmp .Lte_report_finish
.Lte_report_invalid:
 mov eax,4
 jmp .Lte_report_finish
.Lte_report_alloc:
 mov eax,5
.Lte_report_finish:
 mov [rsp+80],eax
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov rdi,[rsp+72]
 call json_object_put@PLT
 mov eax,[rsp+80]
 EE 96
.Lte_report_stage:
 push rbx
 sub rsp,16
 mov rbx,rdi
 mov qword ptr [rcx],0
 lea rax,[rbx+88]
 mov [rsp],rax
 mov rdi,rdx
 mov rsi,[rbx+24]
 mov rcx,[rbx+16]
 mov r8,[rbx+8]
 mov r9d,[rbx+32]
 mov rax,[rbx]
 mov rdx,[rax+16]
 call pg_tool_live_stage_metered@PLT
 add rsp,16
 pop rbx
 ret
.Lte_diagnostic_valid:
 EF 16
 mov rbx,rdi
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lte_diagnostic_bad
 EG rbx,message
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lte_diagnostic_bad
 EG rbx,severity
 mov r14,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lte_diagnostic_bad
 mov rdi,r14
 call json_object_get_string@PLT
 mov r12,rax
 test rax,rax
 jz .Lte_diagnostic_bad
 xor r13d,r13d
.Lte_severity_each:
 lea rax,[rip+.Lte_severities]
 mov rsi,[rax+r13*8]
 test rsi,rsi
 jz .Lte_diagnostic_bad
 mov rdi,r12
 call strcmp@PLT
 test eax,eax
 jz .Lte_diagnostic_code
 inc r13
 jmp .Lte_severity_each
.Lte_diagnostic_code:
 mov rdi,r14
 call json_object_get_string_len@PLT
 mov edx,4
 cmp r13d,2
 jne .Lte_severity_length
 inc edx
.Lte_severity_length:
 cmp eax,edx
 jne .Lte_diagnostic_bad
 mov rdi,rbx
 lea rsi,[rip+.Lte_code]
 mov rdx,rsp
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lte_diagnostic_ok
 mov rdi,[rsp]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lte_diagnostic_bad
.Lte_diagnostic_ok:
 mov eax,1
 jmp .Lte_diagnostic_end
.Lte_diagnostic_bad:
 xor eax,eax
.Lte_diagnostic_end:
 EE 16
.section .rodata
.Lte_severity: .asciz "severity"
.Lte_info: .asciz "info"
.Lte_warn: .asciz "warn"
.Lte_error_severity: .asciz "error"
.section .data.rel.ro
.Lte_severities: .quad .Lte_info,.Lte_warn,.Lte_error_severity,0

.section .rodata
.Lte_durationMs: .asciz "durationMs"

.text
.globl pg_tool_execution_output
.type pg_tool_execution_output,@function
pg_tool_execution_output:
 test rdi,rdi
 jz .Lte_output_invalid
 mov rdi,[rdi]
 jmp pg_durable_tool_output@PLT
.Lte_output_invalid:
 mov eax,4
 ret
.size pg_tool_execution_output,.-pg_tool_execution_output

.text
.globl pg_tool_execution_retained_output
.type pg_tool_execution_retained_output,@function
pg_tool_execution_retained_output:
 test rdi,rdi
 jz .Lte_retained_output_invalid
 mov rdi,[rdi]
 jmp pg_durable_tool_retained_output@PLT
.Lte_retained_output_invalid:
 mov eax,4
 ret
.size pg_tool_execution_retained_output,.-pg_tool_execution_retained_output

.text
.globl pg_tool_execution_output_window
.type pg_tool_execution_output_window,@function
pg_tool_execution_output_window:
 test rdi,rdi
 jz .Lte_output_window_invalid
 mov rdi,[rdi]
 jmp pg_durable_tool_output_window@PLT
.Lte_output_window_invalid:
 mov eax,4
 ret
.size pg_tool_execution_output_window,.-pg_tool_execution_output_window

.section .rodata
.Lte_structuredOutputDeclared: .asciz "structuredOutputDeclared"
