/* Pi Durable harness/tool.ts NestedCallDoc/startNestedCall/settle, d5b3de22.
   MIT docs/licenses/pi.md. */
.intel_syntax noprefix
.macro NF n
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\n
.endm
.macro NE n
 add rsp,\n
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.macro NG obj,key
 mov rdi,\obj
 lea rsi,[rip+.Ln_\key]
 call json_object_object_get@PLT
.endm
.text
.Ln_initial:
 push rbx
 mov rbx,rdx
 call json_object_new_object@PLT
 mov [rbx],rax
 test rax,rax
 setz al
 movzx eax,al
 pop rbx
 ret
.globl pg_nested_call_document
.type pg_nested_call_document,@function
pg_nested_call_document:
 NF 128
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rbp,r8
 mov rdi,rsp
 xor eax,eax
 mov ecx,16
 rep stosq
 lea rax,[rip+.Ln_kind]
 mov [rsp],rax
 mov qword ptr [rsp+8],22
 mov [rsp+16],r13
 mov rdi,r13
 call strlen@PLT
 mov [rsp+24],rax
 mov [rsp+32],r12
 mov dword ptr [rsp+40],2
 mov dword ptr [rsp+44],1
 lea rax,[rip+.Ln_definition]
 mov [rsp+48],rax
 lea rax,[rsp]
 mov [rsp+56],rax
 lea rdi,[rip+.Ln_empty]
 call json_tokener_parse@PLT
 mov [rsp+64],rax
 lea rax,[rip+.Ln_initializer]
 mov [rsp+72],rax
 mov [rsp+80],r14
 mov rdi,rbx
 lea rsi,[rsp+48]
 mov rdx,rbp
 call pg_document_tx_doc@PLT
 mov ebx,eax
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov eax,ebx
 NE 128
/* Append an owned scalar/object to a delta set operation. */
.Ln_change:
 NF 32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rsp],0
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Ln_change_bad
 call json_object_new_array@PLT
 mov r14,rax
 test rax,rax
 jz .Ln_change_bad
 mov rdi,[rsp]
 mov rsi,r14
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ln_change_bad
 lea rdi,[rip+.Ln_set]
 call json_object_new_string@PLT
 mov rdi,r14
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ln_change_bad
 call json_object_new_array@PLT
 mov rbp,rax
 test rax,rax
 jz .Ln_change_bad
 mov rdi,r14
 mov rsi,rbp
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ln_change_bad
 mov rdi,r12
 call json_object_new_string@PLT
 mov rdi,rbp
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ln_change_bad
 mov rdi,r13
 call json_object_get@PLT
 mov rdi,r14
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ln_change_bad
 mov rdi,rbx
 mov rsi,[rsp]
 call pg_document_change_append@PLT
 mov ebp,eax
 jmp .Ln_change_end
.Ln_change_bad:
 mov ebp,5
.Ln_change_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 NE 32
.globl pg_nested_call_settle
.type pg_nested_call_settle,@function
pg_nested_call_settle:
 NF 32
 mov rbx,rcx
 mov r12,r8
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov rcx,r9
 mov r8,rsp
 call pg_nested_call_document
 test eax,eax
 jnz .Ln_settle_bad
 mov rdi,[rsp]
 lea rsi,[rsp+8]
 call pg_document_change_snapshot@PLT
 test eax,eax
 jnz .Ln_settle_bad
 NG "qword ptr [rsp+8]",taskId
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,rbx
 jne .Ln_settle_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Ln_result]
 mov rdx,r12
 call .Ln_change
 mov ebx,eax
 jmp .Ln_settle_end
.Ln_settle_bad:
 mov ebx,4
.Ln_settle_end:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebx
 NE 32
.globl pg_nested_call_start
.type pg_nested_call_start,@function
pg_nested_call_start:
 NF 96
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rbp,r8
 mov [rsp+80],r9
 mov qword ptr [r9],0
 mov rdi,rsp
 xor eax,eax
 mov ecx,10
 rep stosq
 mov [rsp],rbx
 mov [rsp+8],r12
 mov [rsp+16],r13
 mov [rsp+24],r14
 mov [rsp+32],rbp
 test r13,r13
 jz .Ln_start_invalid
 cmp byte ptr [r13],0
 je .Ln_start_invalid
 test r12,r12
 jz .Ln_start_invalid
 test rbp,rbp
 jz .Ln_start_invalid
 mov rdi,r14
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ln_start_invalid
 mov rdi,r14
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Ln_start_invalid
 mov rdi,rbx
 mov rsi,rsp
 lea rdx,[rip+.Ln_start_stage]
 call pg_task_runner_commit_with@PLT
 test eax,eax
 jnz .Ln_start_end
 mov rcx,[rsp+80]
 mov rdx,[rsp+40]
 mov [rcx],rdx
.Ln_start_end:
 NE 96
.Ln_start_invalid:
 mov eax,4
 NE 96
.Ln_start_stage:
 NF 144
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rcx],0
 mov rdi,rsp
 xor eax,eax
 mov ecx,18
 rep stosq
 mov rax,[rbx]
 mov rsi,[rax+16]
 mov rdi,r13
 mov rdx,[rbx+16]
 mov rcx,[rbx+32]
 mov r8,rsp
 call pg_nested_call_document
 test eax,eax
 jnz .Ln_stage_bad
 mov rdi,[rsp]
 lea rsi,[rsp+8]
 call pg_document_change_snapshot@PLT
 test eax,eax
 jnz .Ln_stage_bad
 NG "qword ptr [rsp+8]",taskId
 mov rdi,rax
 call json_object_get_uint64@PLT
 test rax,rax
 jz .Ln_stage_create
 mov [rbx+40],rax
 mov rdi,r12
 mov esi,2
 mov rdx,rax
 lea rcx,[rsp+16]
 call pg_record_tx_read@PLT
 test eax,eax
 jnz .Ln_stage_bad
 NG "qword ptr [rsp+16]",input
 NG rax,call
 mov r14,rax
 NG r14,name
 mov [rsp+48],rax
 NG "qword ptr [rbx+24]",name
 mov rdi,[rsp+48]
 mov rsi,rax
 call json_object_equal@PLT
 test eax,eax
 jz .Ln_stage_bad
 NG r14,arguments
 mov [rsp+48],rax
 NG "qword ptr [rbx+24]",arguments
 mov rdi,[rsp+48]
 mov rsi,rax
 call json_object_equal@PLT
 test eax,eax
 jz .Ln_stage_bad
 xor ebp,ebp
 jmp .Ln_stage_end
.Ln_stage_create:
 lea rdi,[rip+.Ln_input_json]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Ln_stage_bad
 mov rdi,[rbx+24]
 call json_object_get@PLT
 mov rdi,[rsp+16]
 lea rsi,[rip+.Ln_call]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ln_stage_bad
 mov rax,[rbx]
 mov rdi,[rax+16]
 call json_object_new_uint64@PLT
 mov rdi,[rsp+16]
 lea rsi,[rip+.Ln_parent]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ln_stage_bad
 mov rdi,[rbx+16]
 call json_object_new_string@PLT
 mov rdi,[rsp+16]
 lea rsi,[rip+.Ln_key]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ln_stage_bad
 lea rdi,[rip+.Ln_options_json]
 call json_tokener_parse@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Ln_stage_bad
 NG rax,ownership
 mov r14,rax
 mov rax,[rbx]
 mov rdi,[rax+16]
 call json_object_new_uint64@PLT
 mov rdi,r14
 lea rsi,[rip+.Ln_taskId]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ln_stage_bad
 mov r14,[rbx+8]
 mov rax,[r14+24]
 mov [rsp+64],rax
 mov rax,[r14+32]
 mov [rsp+72],rax
 mov rax,[rbx+32]
 mov [rsp+80],rax
 mov rax,[rbx]
 NG "qword ptr [rax+32]",conversationId
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+96],rax
 lea rdi,[rip+.Ln_tool_definition]
 call json_tokener_parse@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Ln_stage_bad
 mov rdi,r12
 mov rsi,[rsp+32]
 mov rdx,[rsp+16]
 mov rcx,[rsp+24]
 lea r8,[rsp+64]
 lea r9,[rbx+40]
 call pg_record_tx_create_task@PLT
 test eax,eax
 jnz .Ln_stage_bad
 mov rdi,[rbx+40]
 call json_object_new_uint64@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Ln_stage_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Ln_taskId]
 mov rdx,rax
 call .Ln_change
 test eax,eax
 jnz .Ln_stage_bad
 xor ebp,ebp
 jmp .Ln_stage_end
.Ln_stage_bad:
 mov ebp,4
.Ln_stage_end:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov eax,ebp
 NE 144
.section .rodata
.Ln_kind: .asciz "pangu.tool.nested-call"
.Ln_set: .asciz "s"
.Ln_taskId: .asciz "taskId"
.Ln_result: .asciz "result"
.Ln_input: .asciz "input"
.Ln_call: .asciz "call"
.Ln_parent: .asciz "parent"
.Ln_key: .asciz "key"
.Ln_ownership: .asciz "ownership"
.Ln_conversationId: .asciz "conversationId"
.Ln_input_json: .asciz "{\"kind\":\"nested\"}"
.Ln_options_json: .asciz "{\"ownership\":{\"kind\":\"task\",\"taskId\":0}}"
.Ln_tool_definition: .asciz "{\"name\":\"pangu.tool\",\"version\":2}"
.section .data.rel.ro
.balign 8
.Ln_definition:
 .quad .Ln_kind,22,1
 .long 2,0,0,1
 .quad 0,0
.Ln_initializer: .quad 0,.Ln_initial
.section .note.GNU-stack,"",@progbits

.section .rodata
.Ln_empty: .asciz "{}"

.text
.globl pg_nested_call_read
.type pg_nested_call_read,@function
pg_nested_call_read:
 NF 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rdi,rsp
 xor eax,eax
 mov ecx,6
 rep stosq
 lea rax,[rip+.Ln_kind]
 mov [rsp],rax
 mov qword ptr [rsp+8],22
 mov [rsp+16],r13
 mov rdi,r13
 call strlen@PLT
 mov [rsp+24],rax
 mov [rsp+32],r12
 mov dword ptr [rsp+40],2
 mov dword ptr [rsp+44],1
 mov rdi,rbx
 lea rsi,[rip+.Ln_definition]
 mov rdx,rsp
 mov rcx,r14
 call pg_durable_snapshot@PLT
 NE 48

.section .rodata
.Ln_name: .asciz "name"
.Ln_arguments: .asciz "arguments"
