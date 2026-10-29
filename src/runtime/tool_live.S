/* Pi Durable harness/tool.ts startNestedCall/toolSlot/settle, d5b3de22.
   MIT docs/licenses/pi.md. */
.intel_syntax noprefix
.macro LF n
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\n
.endm
.macro LE n
 add rsp,\n
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.macro LG obj,key
 mov rdi,\obj
 lea rsi,[rip+.Llv_\key]
 call json_object_object_get@PLT
.endm
.text
.Llv_initial:
 push rbx
 mov rbx,rdx
 call json_object_new_object@PLT
 mov [rbx],rax
 test rax,rax
 setz al
 movzx eax,al
 pop rbx
 ret
/* Adopt a reference into an array on either return. */
.Llv_array:
 push rbx
 mov rbx,rsi
 call json_object_array_add@PLT
 test eax,eax
 jz .Llv_array_end
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,5
.Llv_array_end:
 pop rbx
 ret
/* ops, operation JSON, index, value, field name (nullable). */
.Llv_op:
 LF 32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rbp,r8
 mov qword ptr [rsp],0
 mov rdi,r12
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Llv_op_bad
 mov rdi,rax
 mov esi,1
 call json_object_array_get_idx@PLT
 mov [rsp+8],rax
 test rbp,rbp
 jz .Llv_op_index
 mov rdi,rbp
 call json_object_new_string@PLT
 test rax,rax
 jz .Llv_op_bad
 mov rdi,[rsp+8]
 mov esi,2
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Llv_op_bad
.Llv_op_index:
 mov rdi,r13
 call json_object_new_uint64@PLT
 test rax,rax
 jz .Llv_op_bad
 mov rdi,[rsp+8]
 mov esi,1
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Llv_op_bad
 cmp byte ptr [r12+2],'d'
 je .Llv_op_append
 mov rdi,r14
 call json_object_get@PLT
 mov rdi,[rsp]
 mov esi,2
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Llv_op_bad
.Llv_op_append:
 mov rdi,rbx
 mov rsi,[rsp]
 mov qword ptr [rsp],0
 call .Llv_array
 mov ebp,eax
 jmp .Llv_op_end
.Llv_op_bad:
 mov ebp,5
.Llv_op_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 LE 32

.globl pg_tool_live_stage
.type pg_tool_live_stage,@function
pg_tool_live_stage:
 LF 224
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rbp,r8
 mov rdi,rsp
 xor eax,eax
 mov ecx,28
 rep stosq
 mov [rsp+208],r9d
 lea rax,[rip+.Llv_kind]
 mov [rsp],rax
 mov qword ptr [rsp+8],10
 mov [rsp+32],r12
 mov dword ptr [rsp+40],1
 lea rax,[rip+.Llv_definition]
 mov [rsp+48],rax
 lea rax,[rsp]
 mov [rsp+56],rax
 lea rax,[rip+.Llv_initializer]
 mov [rsp+72],rax
 mov [rsp+80],rbp
 mov rdi,rbx
 lea rsi,[rsp+48]
 lea rdx,[rsp+112]
 call pg_document_tx_doc@PLT
 test eax,eax
 jnz .Llv_finish
 mov rdi,[rsp+112]
 lea rsi,[rsp+120]
 call pg_document_change_snapshot@PLT
 test eax,eax
 jnz .Llv_finish
 LG "qword ptr [rsp+120]",nestedTools
 mov [rsp+128],rax
 test rax,rax
 jz .Llv_empty_length
 mov rdi,rax
 call json_object_array_length@PLT
.Llv_empty_length:
 mov [rsp+136],rax
 mov [rsp+144],rax
 call json_object_new_array@PLT
 mov [rsp+152],rax
 test rax,rax
 jz .Llv_bad
.Llv_each:
 cmp qword ptr [rsp+136],0
 je .Llv_created
 dec qword ptr [rsp+136]
 mov rdi,[rsp+128]
 mov rsi,[rsp+136]
 call json_object_array_get_idx@PLT
 mov [rsp+176],rax
 cmp dword ptr [rsp+208],0
 je .Llv_match
 LG rax,parentTaskId
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,r13
 jne .Llv_match
 mov rdi,[rsp+152]
 lea rsi,[rip+.Llv_delete_json]
 mov rdx,[rsp+136]
 xor ecx,ecx
 xor r8d,r8d
 call .Llv_op
 test eax,eax
 jnz .Llv_finish
 dec qword ptr [rsp+144]
 jmp .Llv_each
.Llv_match:
 test r14,r14
 jz .Llv_each
 LG "qword ptr [rsp+176]",taskId
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,r13
 jne .Llv_each
 mov dword ptr [rsp+200],1
 LG r14,status
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Llv_fields_begin
 mov rbp,rax
 mov rdi,rax
 lea rsi,[rip+.Llv_done]
 call strcmp@PLT
 test eax,eax
 jz .Llv_clear_begin
 mov rdi,rbp
 lea rsi,[rip+.Llv_running]
 call strcmp@PLT
 test eax,eax
 jnz .Llv_fields_begin
.Llv_clear_begin:
 mov qword ptr [rsp+192],0
.Llv_clear_field:
 mov rax,[rsp+192]
 lea rdx,[rip+.Llv_progress_fields]
 mov rbp,[rdx+rax*8]
 test rbp,rbp
 jz .Llv_fields_begin
 mov rdi,[rsp+176]
 mov rsi,rbp
 lea rdx,[rsp+160]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Llv_clear_next
 mov rdi,[rsp+152]
 lea rsi,[rip+.Llv_delete_field_json]
 mov rdx,[rsp+136]
 xor ecx,ecx
 mov r8,rbp
 call .Llv_op
 test eax,eax
 jnz .Llv_finish
.Llv_clear_next:
 inc qword ptr [rsp+192]
 jmp .Llv_clear_field
.Llv_fields_begin:
 mov qword ptr [rsp+192],0
.Llv_field:
 mov rax,[rsp+192]
 lea rdx,[rip+.Llv_fields]
 mov rbp,[rdx+rax*8]
 test rbp,rbp
 jz .Llv_each
 mov rdi,r14
 mov rsi,rbp
 lea rdx,[rsp+184]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Llv_field_next
 /* Omit unchanged leaves, including explicit JSON null. */
 mov rdi,[rsp+176]
 mov rsi,rbp
 lea rdx,[rsp+160]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Llv_field_write
 mov rdi,[rsp+160]
 mov rsi,[rsp+184]
 call json_object_equal@PLT
 test eax,eax
 jnz .Llv_field_next
.Llv_field_write:
 mov rdi,[rsp+152]
 lea rsi,[rip+.Llv_field_json]
 mov rdx,[rsp+136]
 mov rcx,[rsp+184]
 mov r8,rbp
 call .Llv_op
 test eax,eax
 jnz .Llv_finish
.Llv_field_next:
 inc qword ptr [rsp+192]
 jmp .Llv_field
.Llv_created:
 test r14,r14
 jz .Llv_commit
 cmp dword ptr [rsp+200],0
 jne .Llv_commit
 cmp qword ptr [rsp+128],0
 jne .Llv_append
 lea rdi,[rip+.Llv_array_json]
 call json_tokener_parse@PLT
 test rax,rax
 jz .Llv_bad
 mov rdi,[rsp+152]
 mov rsi,rax
 call .Llv_array
 test eax,eax
 jnz .Llv_finish
.Llv_append:
 /* Dense array set at length appends, matching decoded Chord set. */
 mov rdi,[rsp+152]
 lea rsi,[rip+.Llv_slot_json]
 mov rdx,[rsp+144]
 mov rcx,r14
 xor r8d,r8d
 call .Llv_op
 test eax,eax
 jnz .Llv_finish
.Llv_commit:
 mov rdi,[rsp+152]
 call json_object_array_length@PLT
 test rax,rax
 jz .Llv_ok
 mov rdi,[rsp+112]
 mov rsi,[rsp+152]
 call pg_document_change_append@PLT
 jmp .Llv_finish
.Llv_ok:
 xor eax,eax
 jmp .Llv_finish
.Llv_bad:
 mov eax,5
.Llv_finish:
 mov [rsp+216],eax
 mov rdi,[rsp+120]
 call json_object_put@PLT
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov eax,[rsp+216]
 LE 224
.size pg_tool_live_stage,.-pg_tool_live_stage
.section .rodata
.Llv_kind: .asciz "pangu.live"
.Llv_nestedTools: .asciz "nestedTools"
.Llv_taskId: .asciz "taskId"
.Llv_parentTaskId: .asciz "parentTaskId"
.Llv_status: .asciz "status"
.Llv_arguments: .asciz "arguments"
.Llv_output: .asciz "output"
.Llv_details: .asciz "details"
.Llv_diagnostics: .asciz "diagnostics"
.Llv_summary: .asciz "summary"
.Llv_parentCallId: .asciz "parentCallId"
.Llv_callId: .asciz "callId"
.Llv_name: .asciz "name"
.Llv_droppedBytes: .asciz "droppedBytes"
.Llv_droppedLines: .asciz "droppedLines"
.Llv_delete_json: .asciz "[\"d\",[\"nestedTools\",0]]"
.Llv_field_json: .asciz "[\"s\",[\"nestedTools\",0,\"\"],null]"
.Llv_slot_json: .asciz "[\"s\",[\"nestedTools\",0],null]"
.Llv_array_json: .asciz "[\"s\",[\"nestedTools\"],[]]"
.section .data.rel.ro
.balign 8
.Llv_fields:
 .quad .Llv_status,.Llv_arguments,.Llv_output,.Llv_details,.Llv_diagnostics,.Llv_summary
 .quad .Llv_parentCallId,.Llv_callId,.Llv_name,.Llv_taskId,.Llv_parentTaskId
 .quad .Llv_droppedBytes,.Llv_droppedLines,0
.Llv_definition:
 .quad .Llv_kind,10,1
 .long 1,1,2,0
 .quad 0,0
.Llv_initializer: .quad 0,.Llv_initial
.section .note.GNU-stack,"",@progbits

.text
.globl pg_tool_live_created
.type pg_tool_live_created,@function
pg_tool_live_created:
 LF 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rbp,r8
 mov [rsp+8],r9
 mov qword ptr [rsp],0
 lea rdi,[rip+.Llv_pending_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Llv_created_bad
 mov rdi,r13
 call json_object_new_uint64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Llv_taskId]
 mov rdx,rax
 call .Llv_set
 test eax,eax
 jnz .Llv_created_end
 mov rdi,r14
 call json_object_new_uint64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Llv_parentTaskId]
 mov rdx,rax
 call .Llv_set
 test eax,eax
 jnz .Llv_created_end
 mov qword ptr [rsp+16],0
.Llv_created_fields:
 mov rax,[rsp+16]
 lea rdx,[rip+.Llv_call_fields]
 mov r14,[rdx+rax*8]
 test r14,r14
 jz .Llv_created_stage
 mov rdi,rbp
 mov rsi,r14
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,[rsp]
 mov rsi,r14
 cmp qword ptr [rsp+16],0
 jne .Llv_created_add
 lea rsi,[rip+.Llv_callId]
.Llv_created_add:
 mov rdx,rax
 call .Llv_set
 test eax,eax
 jnz .Llv_created_end
 inc qword ptr [rsp+16]
 jmp .Llv_created_fields
.Llv_created_stage:
 mov rdi,rbp
 lea rsi,[rip+.Llv_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Llv_created_bad
 mov [rsp+32],rax
 mov rdi,rax
 mov esi,'/'
 call strrchr@PLT
 test rax,rax
 jz .Llv_created_no_parent
 sub rax,[rsp+32]
 mov esi,eax
 mov rdi,[rsp+32]
 call json_object_new_string_len@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Llv_parentCallId]
 mov rdx,rax
 call .Llv_set
 test eax,eax
 jnz .Llv_created_end
.Llv_created_no_parent:
 mov rdi,rbx
 mov rsi,r12
 mov rdx,r13
 mov rcx,[rsp]
 mov r8,[rsp+8]
 xor r9d,r9d
 call pg_tool_live_stage
 jmp .Llv_created_end
.Llv_created_bad:
 mov eax,5
.Llv_created_end:
 mov [rsp+24],eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,[rsp+24]
 LE 48
.size pg_tool_live_created,.-pg_tool_live_created
.Llv_set:
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Llv_set_end
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,5
.Llv_set_end:
 pop rbx
 ret
.section .rodata
.Llv_pending_json: .asciz "{\"status\":\"pending\"}"
.Llv_id: .asciz "id"
.section .data.rel.ro
.Llv_call_fields: .quad .Llv_id,.Llv_name,.Llv_arguments,0

.section .rodata
.Llv_done: .asciz "done"
.Llv_running: .asciz "running"
.Llv_delete_field_json: .asciz "[\"d\",[\"nestedTools\",0,\"\"]]"
.section .data.rel.ro
.Llv_progress_fields:
 .quad .Llv_output,.Llv_details,.Llv_diagnostics,.Llv_droppedBytes,.Llv_droppedLines,0
