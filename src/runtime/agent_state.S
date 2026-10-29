/* Pi Durable harness/agent.ts configure/filterTools; d5b3de22d0e24a3e9c38a9410d3329b099717283.
   MIT, docs/licenses/pi.md. */
.intel_syntax noprefix
.section .rodata
.La_disabled: .asciz "Tool is not enabled for this agent"
.set .La_disabled_size,.-.La_disabled-1


.macro AF n
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\n
.endm
.macro AE n
 add rsp,\n
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.text
.La_initial:
 push rbx
 mov rbx,rdx
 call json_object_new_object@PLT
 mov [rbx],rax
 test rax,rax
 setz al
 movzx eax,al
 pop rbx
 ret
.La_checkpoint:
 mov eax,1
 ret
.globl pg_agent_state_read
.type pg_agent_state_read,@function
pg_agent_state_read:
 AF 48
 mov rbx,rdi
 mov r12,rdx
 mov r8,rsi
 mov rdi,rsp
 xor eax,eax
 mov ecx,6
 rep stosq
 lea rax,[rip+.La_kind]
 mov [rsp],rax
 mov qword ptr [rsp+8],11
 mov [rsp+32],r8
 mov dword ptr [rsp+40],1
 mov rdi,rbx
 lea rsi,[rip+.La_definition]
 mov rdx,rsp
 mov rcx,r12
 call pg_durable_snapshot@PLT
 cmp eax,8
 jne .La_read_end
 call json_object_new_object@PLT
 mov [r12],rax
 test rax,rax
 setz al
 movzx eax,al
.La_read_end:
 AE 48
.globl pg_agent_state_stage
.type pg_agent_state_stage,@function
pg_agent_state_stage:
 xor r8d,r8d
 jmp .La_stage_entry
.globl pg_agent_state_add_tools
.type pg_agent_state_add_tools,@function
pg_agent_state_add_tools:
 mov r8d,1
.La_stage_entry:
 AF 176
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rdi,rsp
 xor eax,eax
 mov ecx,22
 rep stosq
 mov [rsp+168],r8d
 mov rdi,r13
 mov esi,4
 add esi,r8d
 call json_object_is_type@PLT
 test eax,eax
 jz .La_stage_bad
 cmp dword ptr [rsp+168],0
 jne .La_stage_validated
 mov rdi,r13
 call .La_validate_text_fields
 test eax,eax
 jnz .La_stage_bad
.La_stage_validated:
 mov rdi,r13
 call pg_document_value_valid@PLT
 test eax,eax
 jz .La_stage_bad
 lea rax,[rip+.La_kind]
 mov [rsp],rax
 mov qword ptr [rsp+8],11
 mov [rsp+32],r12
 mov dword ptr [rsp+40],1
 lea rax,[rip+.La_definition]
 mov [rsp+48],rax
 mov rax,rsp
 mov [rsp+56],rax
 lea rax,[rip+.La_initializer]
 mov [rsp+72],rax
 mov [rsp+80],r14
 lea rax,[rip+.La_checkpoint]
 mov [rsp+96],rax
 mov rdi,rbx
 lea rsi,[rsp+48]
 lea rdx,[rsp+112]
 call pg_document_tx_doc@PLT
 test eax,eax
 jnz .La_stage_bad
 mov rdi,[rsp+112]
 lea rsi,[rsp+120]
 call pg_document_change_snapshot@PLT
 test eax,eax
 jnz .La_stage_bad
 cmp dword ptr [rsp+168],0
 je .La_stage_replace_fields
 mov rdi,[rsp+120]
 mov rsi,r13
 call .La_control_add
 test eax,eax
 jnz .La_stage_bad
 jmp .La_stage_patch
.La_stage_replace_fields:
 xor r12d,r12d
.La_stage_field:
 lea rax,[rip+.La_fields]
 mov r14,[rax+r12*8]
 test r14,r14
 jz .La_stage_patch
 mov rdi,r13
 mov rsi,r14
 lea rdx,[rsp+144]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .La_stage_next
 mov rdi,[rsp+144]
 test rdi,rdi
 jz .La_stage_delete
 call json_object_get@PLT
 mov [rsp+152],rax
 mov rdi,[rsp+120]
 mov rsi,r14
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .La_stage_bad
 mov qword ptr [rsp+152],0
 jmp .La_stage_next
.La_stage_delete:
 mov rdi,[rsp+120]
 mov rsi,r14
 call json_object_object_del@PLT
.La_stage_next:
 inc r12
 jmp .La_stage_field
.La_stage_patch:
 mov rdi,[rsp+112]
 lea rsi,[rsp+160]
 call pg_document_change_snapshot@PLT
 test eax,eax
 jnz .La_stage_bad
 mov rdi,[rsp+120]
 mov rsi,[rsp+160]
 call json_object_equal@PLT
 test eax,eax
 jnz .La_stage_unchanged
 lea rdi,[rip+.La_patch]
 call json_tokener_parse@PLT
 mov [rsp+128],rax
 test rax,rax
 jz .La_stage_bad
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov [rsp+136],rax
 mov rdi,[rsp+120]
 call json_object_get@PLT
 mov rdi,[rsp+136]
 mov esi,1
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .La_stage_bad
 mov rdi,[rsp+112]
 mov rsi,[rsp+128]
 call pg_document_change_append@PLT
 test eax,eax
 jnz .La_stage_bad
.La_stage_unchanged:
 xor ebp,ebp
 jmp .La_stage_end
.La_stage_bad:
 mov ebp,5
.La_stage_end:
 mov rdi,[rsp+120]
 call json_object_put@PLT
 mov rdi,[rsp+128]
 call json_object_put@PLT
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov rdi,[rsp+160]
 call json_object_put@PLT
 mov eax,ebp
 AE 176
/* Apply native request fields, keeping the saved configuration detached. */
.globl pg_agent_state_prepare
.type pg_agent_state_prepare,@function
pg_agent_state_prepare:
 AF 32
 mov qword ptr [rsp],0
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r13],0
 mov rdi,rbx
 mov rsi,r13
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .La_prepare_bad
 xor r14d,r14d
.La_prepare_field:
 lea rax,[rip+.La_request_fields]
 mov rsi,[rax+r14*8]
 test rsi,rsi
 jz .La_prepare_ok
 mov [rsp+8],rsi
 mov rdi,r12
 call json_object_object_get@PLT
 test rax,rax
 jz .La_prepare_next
 mov rdi,rax
 mov qword ptr [rsp],0
 lea rsi,[rsp]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .La_prepare_bad
 mov rdi,[r13]
 mov rsi,[rsp+8]
 test r14,r14
 jnz .La_prepare_install
 lea rsi,[rip+.La_system]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+16],rax
 lea rdi,[rsp+16]
 mov rsi,[rsp]
 mov edx,2
 call pg_context_append@PLT
 test eax,eax
 jnz .La_prepare_append_bad
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rax,[rsp+16]
 mov [rsp],rax
 mov qword ptr [rsp+16],0
 mov rdi,[r13]
 lea rsi,[rip+.La_system]
.La_prepare_install:
 mov rdx,[rsp]
 call json_object_object_add@PLT
 test eax,eax
 jnz .La_prepare_bad
 mov qword ptr [rsp],0
.La_prepare_next:
 inc r14
 jmp .La_prepare_field
.La_prepare_append_bad:
 mov rdi,[rsp+16]
 call json_object_put@PLT
.La_prepare_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[r13]
 call json_object_put@PLT
 mov qword ptr [r13],0
 mov eax,5
 jmp .La_prepare_end
.La_prepare_ok:
 xor eax,eax
.La_prepare_end:
 AE 32
/* Return the matching borrowed definition; explicit name bytes. */
.La_find:
 AF 16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
.La_find_each:
 cmp r14,[rbx+1280]
 jae .La_find_missing
 imul rax,r14,80
 lea rbp,[rbx+rax]
 mov rdi,[rbp]
 call strlen@PLT
 cmp rax,r13
 jne .La_find_next
 mov rdi,[rbp]
 mov rsi,r12
 mov rdx,r13
 call memcmp@PLT
 test eax,eax
 jz .La_find_found
.La_find_next:
 inc r14
 jmp .La_find_each
.La_find_missing:
 xor ebp,ebp
.La_find_found:
 mov rax,rbp
 AE 16
.globl pg_agent_tool_allowed
.type pg_agent_tool_allowed,@function
pg_agent_tool_allowed:
 sub rsp,8
 call .La_find
 test rax,rax
 setnz al
 movzx eax,al
 add rsp,8
 ret
/* Filter an existing registry into another fresh registry. */
.La_filter:
 AF 1328
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
 mov qword ptr [rsp+1280],0
 test r12,r12
 jz .La_filter_all
 mov rdi,r12
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jnz .La_filter_array
 mov rdi,r12
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .La_filter_bad
 mov rdi,r12
 lea rsi,[rip+.La_remove]
 call json_object_object_get@PLT
 mov r12,rax
 test rax,rax
 jz .La_filter_all
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .La_filter_bad
.La_filter_all:
 cmp r14,[rbx+1280]
 jae .La_filter_init
 imul rbp,r14,80
 add rbp,rbx
 test r12,r12
 jz .La_filter_copy
 mov qword ptr [rsp+1296],0
.La_remove_each:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp [rsp+1296],rax
 jae .La_filter_copy
 mov rdi,r12
 mov rsi,[rsp+1296]
 call json_object_array_get_idx@PLT
 mov [rsp+1312],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .La_filter_bad
 mov rdi,[rbp]
 call strlen@PLT
 mov [rsp+1320],rax
 mov rdi,[rsp+1312]
 call json_object_get_string_len@PLT
 cmp rax,[rsp+1320]
 jne .La_remove_next
 mov rdi,[rsp+1312]
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,[rbp]
 call strcmp@PLT
 test eax,eax
 jz .La_filter_all_next
.La_remove_next:
 inc qword ptr [rsp+1296]
 jmp .La_remove_each
.La_filter_copy:
 mov rax,[rsp+1280]
 imul rax,rax,80
 lea rdi,[rsp+rax]
 mov rsi,rbp
 mov ecx,10
 rep movsq
 inc qword ptr [rsp+1280]
.La_filter_all_next:
 inc r14
 jmp .La_filter_all
.La_filter_array:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r14,rax
 jae .La_filter_init
 mov rdi,r12
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov [rsp+1288],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .La_filter_bad
 mov rdi,[rsp+1288]
 call json_object_get_string@PLT
 mov [rsp+1304],rax
 mov rdi,[rsp+1288]
 call json_object_get_string_len@PLT
 mov rdi,rbx
 mov rsi,[rsp+1304]
 mov edx,eax
 call .La_find
 test rax,rax
 jz .La_filter_array_next
 mov rbp,rax
 xor ecx,ecx
.La_filter_duplicate:
 cmp rcx,[rsp+1280]
 jae .La_filter_copy_array
 imul rax,rcx,80
 mov rax,[rsp+rax]
 cmp rax,[rbp]
 je .La_filter_array_next
 inc rcx
 jmp .La_filter_duplicate
.La_filter_copy_array:
 mov rax,[rsp+1280]
 imul rax,rax,80
 lea rdi,[rsp+rax]
 mov rsi,rbp
 mov ecx,10
 rep movsq
 inc qword ptr [rsp+1280]
.La_filter_array_next:
 inc r14
 jmp .La_filter_array
.La_filter_init:
 mov rdi,r13
 mov rsi,rsp
 mov rdx,[rsp+1280]
 call pg_tool_registry_init@PLT
 jmp .La_filter_end
.La_filter_bad:
 mov eax,5
.La_filter_end:
 AE 1328
.globl pg_agent_tools_select
.type pg_agent_tools_select,@function
pg_agent_tools_select:
 AF 1328
 mov rbx,rdi
 mov r12,rsi
 mov r13,rcx
 mov r14d,edx
 cmp qword ptr [rbx+1280],16
 ja .La_select_bad
 mov rdi,r12
 lea rsi,[rip+.La_tools]
 call json_object_object_get@PLT
 mov rsi,rax
 mov rdi,rbx
 mov rdx,rsp
 call .La_filter
 test eax,eax
 jnz .La_select_end
 test r14d,r14d
 jz .La_select_copy
 mov rdi,r12
 lea rsi,[rip+.La_model_tools]
 call json_object_object_get@PLT
 mov rdi,rsp
 mov rsi,rax
 mov rdx,r13
 call .La_filter
 jmp .La_select_end
.La_select_copy:
 mov rdi,r13
 mov rsi,rsp
 mov ecx,164
 rep movsq
 xor eax,eax
 jmp .La_select_end
.La_select_bad:
 mov eax,5
.La_select_end:
 AE 1328
.section .rodata
.La_kind: .asciz "pangu.agent"
.La_patch: .asciz "[[\"r\",null]]"
.La_model: .asciz "model"
.La_thinking: .asciz "thinkingLevel"
.La_extensions: .asciz "extensions"
.La_tools: .asciz "tools"
.La_model_tools: .asciz "modelTools"
.La_instructions: .asciz "instructions"
.La_cwd: .asciz "cwd"
.La_system: .asciz "systemPrompt"
.La_remove: .asciz "remove"
.section .data.rel.ro
.balign 8
.La_fields: .quad .La_model,.La_thinking,.La_extensions,.La_tools,.La_model_tools,.La_instructions,.La_cwd,0
.La_request_fields: .quad .La_instructions,.La_tools,.La_model_tools,0
.La_definition:
 .quad .La_kind,11,1
 .long 1,2,3,0
 .quad 0,0
.La_initializer: .quad 0,.La_initial
.section .note.GNU-stack,"",@progbits

.text
/* Preserve the native executor lease; the enabled catalog is immutable. */
.globl pg_agent_tools_service
.type pg_agent_tools_service,@function
pg_agent_tools_service:
 mov [rdi],rsi
 lea rax,[rip+.La_gate_authorize]
 mov [rdi+8],rax
 lea rax,[rip+.La_gate_execute]
 mov [rdi+16],rax
 lea rax,[rip+.La_gate_replay]
 mov [rdi+24],rax
 lea rax,[rip+.La_gate_next]
 mov [rdi+32],rax
 lea rax,[rip+.La_gate_cancel]
 mov [rdi+40],rax
 lea rax,[rip+.La_gate_close]
 mov [rdi+48],rax
 mov rax,rdi
 ret
/* All argument registers restored before tail-calling the original method. */
.La_gate_check:
 sub rsp,72
 mov [rsp],rdi
 mov [rsp+8],rsi
 mov [rsp+16],rdx
 mov [rsp+24],rcx
 mov [rsp+32],r8
 mov [rsp+40],r9
 mov rdi,[rdi+56]
 call pg_agent_tool_allowed
 mov rdi,[rsp]
 mov rsi,[rsp+8]
 mov rdx,[rsp+16]
 mov rcx,[rsp+24]
 mov r8,[rsp+32]
 mov r9,[rsp+40]
 add rsp,72
 ret
.La_gate_authorize:
 sub rsp,8
 call .La_gate_check
 add rsp,8
 test eax,eax
 jz .La_gate_return
 mov rax,[rdi+8]
 mov rdi,[rdi]
 jmp rax
.La_gate_execute:
 sub rsp,8
 call .La_gate_check
 add rsp,8
 test eax,eax
 jz .La_gate_denied
 mov rax,[rdi+16]
 mov rdi,[rdi]
 jmp rax
.La_gate_denied:
 mov r10,[rsp+16]
 mov qword ptr [r10],0
 cmp qword ptr [rsp+8],OFFSET .La_disabled_size
 jbe .La_gate_denied_return
 mov rdi,r9
 lea rsi,[rip+.La_disabled]
 mov ecx,OFFSET .La_disabled_size+1
 rep movsb
 mov qword ptr [r10],OFFSET .La_disabled_size
.La_gate_denied_return:
 mov eax,1
 ret
.La_gate_replay:
 sub rsp,8
 call .La_gate_check
 add rsp,8
 test eax,eax
 jz .La_gate_return
 mov rax,[rdi+24]
 test rax,rax
 jz .La_gate_return
 mov rdi,[rdi]
 jmp rax
.La_gate_next:
 mov rax,[rdi+32]
 test rax,rax
 jz .La_gate_error
 mov rdi,[rdi]
 jmp rax
.La_gate_cancel:
 mov rax,[rdi+40]
 test rax,rax
 jz .La_gate_return
 mov rdi,[rdi]
 jmp rax
.La_gate_close:
 mov rax,[rdi+48]
 test rax,rax
 jz .La_gate_return
 mov rdi,[rdi]
 jmp rax
.La_gate_error:
 mov eax,1
.La_gate_return:
 ret

.text
.globl pg_agent_state_peek
.type pg_agent_state_peek,@function
pg_agent_state_peek:
 AF 64
 mov rbx,rdi
 mov r12,rdx
 mov qword ptr [r12],0
 mov r8,rsi
 mov rdi,rsp
 xor eax,eax
 mov ecx,8
 rep stosq
 lea rax,[rip+.La_kind]
 mov [rsp],rax
 mov qword ptr [rsp+8],11
 mov [rsp+32],r8
 mov dword ptr [rsp+40],1
 mov rdi,[rbx+8]
 lea rsi,[rip+.La_definition]
 mov rdx,rsp
 lea rcx,[rsp+48]
 call pg_document_cache_peek@PLT
 cmp eax,8
 je .La_peek_empty
 test eax,eax
 jnz .La_peek_end
 mov rax,[rsp+48]
 mov rdi,[rax+8]
 mov rsi,r12
 xor edx,edx
 call json_object_deep_copy@PLT
 jmp .La_peek_end
.La_peek_empty:
 call json_object_new_object@PLT
 mov [r12],rax
 test rax,rax
 setz al
 movzx eax,al
.La_peek_end:
 AE 64

.text
.La_validate_text_fields:
 AF 16
 mov rbx,rdi
 xor r12d,r12d
.La_validate_text_each:
 lea rax,[rip+.La_text_fields]
 mov rsi,[rax+r12*8]
 test rsi,rsi
 jz .La_validate_text_ok
 mov rdi,rbx
 call json_object_object_get@PLT
 test rax,rax
 jz .La_validate_text_next
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .La_validate_text_bad
.La_validate_text_next:
 inc r12
 jmp .La_validate_text_each
.La_validate_text_bad:
 mov eax,5
 jmp .La_validate_text_end
.La_validate_text_ok:
 xor eax,eax
.La_validate_text_end:
 AE 16
.section .data.rel.ro
.La_text_fields: .quad .La_instructions,.La_cwd,.La_thinking,0
.text
.globl pg_agent_tools_names
.type pg_agent_tools_names,@function
pg_agent_tools_names:
 AF 16
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [r12],0
 mov qword ptr [rsp],0
 call json_object_new_array@PLT
 mov r13,rax
 test rax,rax
 jz .La_names_bad
 xor r14d,r14d
.La_names_each:
 cmp r14,[rbx+1280]
 jae .La_names_done
 imul rax,r14,80
 mov rdi,[rbx+rax]
 call json_object_new_string@PLT
 mov [rsp],rax
 test rax,rax
 jz .La_names_bad
 mov rdi,r13
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .La_names_bad
 mov qword ptr [rsp],0
 inc r14
 jmp .La_names_each
.La_names_done:
 mov [r12],r13
 xor eax,eax
 jmp .La_names_end
.La_names_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,r13
 call json_object_put@PLT
 mov eax,5
.La_names_end:
 AE 16

.text
/* Strict value equality preserves first occurrence in tool-name arrays. */
.La_control_contains:
 AF 0
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
.La_control_contains_each:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r13,rax
 jae .La_control_contains_no
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov rsi,r12
 call json_object_equal@PLT
 test eax,eax
 jnz .La_control_contains_end
 inc r13
 jmp .La_control_contains_each
.La_control_contains_no:
 xor eax,eax
.La_control_contains_end:
 AE 0
/* mode 0 appends unique names, mode 1 removes named exclusions. */
.La_control_names:
 AF 16
 mov rbx,rdi
 mov r12,rsi
 mov r14d,edx
 xor r13d,r13d
.La_control_names_each:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .La_control_names_ok
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rbp,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .La_control_names_bad
 test r14d,r14d
 jnz .La_control_names_remove
 mov rdi,rbx
 mov rsi,rbp
 call .La_control_contains
 test eax,eax
 jnz .La_control_names_next
 mov rdi,rbp
 call json_object_get@PLT
 mov [rsp],rax
 mov rdi,rbx
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz .La_control_names_next
 mov rdi,[rsp]
 call json_object_put@PLT
 jmp .La_control_names_bad
.La_control_names_remove:
 mov rdi,rbx
 call json_object_array_length@PLT
 mov [rsp+8],rax
.La_control_remove_each:
 cmp qword ptr [rsp+8],0
 je .La_control_names_next
 dec qword ptr [rsp+8]
 mov rdi,rbx
 mov rsi,[rsp+8]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov rsi,rbp
 call json_object_equal@PLT
 test eax,eax
 jz .La_control_remove_each
 mov rdi,rbx
 mov rsi,[rsp+8]
 mov edx,1
 call json_object_array_del_idx@PLT
 test eax,eax
 jnz .La_control_names_bad
 jmp .La_control_remove_each
.La_control_names_next:
 inc r13
 jmp .La_control_names_each
.La_control_names_ok:
 xor eax,eax
 jmp .La_control_names_end
.La_control_names_bad:
 mov eax,5
.La_control_names_end:
 AE 16
.La_control_install:
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .La_control_install_end
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,5
.La_control_install_end:
 pop rbx
 ret
/* tools and modelTools retain their array or add/remove representation. */
.La_control_add:
 AF 32
 mov rbx,rdi
 mov r12,rsi
 xor r14d,r14d
.La_control_field:
 lea r13,[rip+.La_tools]
 test r14d,r14d
 jz .La_control_field_get
 lea r13,[rip+.La_model_tools]
.La_control_field_get:
 mov rdi,rbx
 mov rsi,r13
 call json_object_object_get@PLT
 mov rbp,rax
 test rax,rax
 jnz .La_control_field_type
 test r14d,r14d
 jz .La_control_field_next
 call json_object_new_object@PLT
 mov rbp,rax
 test rax,rax
 jz .La_control_add_bad
 mov rdi,rbx
 mov rsi,r13
 mov rdx,rax
 call .La_control_install
 test eax,eax
 jnz .La_control_add_end
.La_control_field_type:
 mov rdi,rbp
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .La_control_object
 mov rdi,rbp
 mov rsi,r12
 xor edx,edx
 call .La_control_names
 test eax,eax
 jnz .La_control_add_end
 jmp .La_control_field_next
.La_control_object:
 mov rdi,rbp
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .La_control_add_bad
 mov rdi,rbp
 lea rsi,[rip+.La_remove]
 call json_object_object_get@PLT
 mov [rsp],rax
 test rax,rax
 jz .La_control_model_add
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .La_control_add_bad
 mov rdi,[rsp]
 mov rsi,r12
 mov edx,1
 call .La_control_names
 test eax,eax
 jnz .La_control_add_end
 test r14d,r14d
 jz .La_control_model_add
 mov rdi,[rsp]
 call json_object_array_length@PLT
 test rax,rax
 jnz .La_control_model_add
 mov rdi,rbp
 lea rsi,[rip+.La_remove]
 call json_object_object_del@PLT
.La_control_model_add:
 test r14d,r14d
 jz .La_control_field_next
 mov rdi,rbp
 lea rsi,[rip+.La_add]
 call json_object_object_get@PLT
 mov r13,rax
 test rax,rax
 jnz .La_control_add_array
 call json_object_new_array@PLT
 mov r13,rax
 test rax,rax
 jz .La_control_add_bad
 mov rdi,rbp
 lea rsi,[rip+.La_add]
 mov rdx,rax
 call .La_control_install
 test eax,eax
 jnz .La_control_add_end
.La_control_add_array:
 mov rdi,r13
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .La_control_add_bad
 mov rdi,r13
 mov rsi,r12
 xor edx,edx
 call .La_control_names
 test eax,eax
 jnz .La_control_add_end
.La_control_field_next:
 inc r14d
 cmp r14d,2
 jb .La_control_field
 xor eax,eax
 jmp .La_control_add_end
.La_control_add_bad:
 mov eax,5
.La_control_add_end:
 AE 32
.section .rodata
.La_add: .asciz "add"

.text
/* Document request assembled against the caller's already admitted mutation. */
.Lac_doc:
 AF 112
 mov rbx,rdi
 mov r12,r9
 mov r9,rcx
 mov r10,rdx
 mov r11,rsi
 mov rdi,rsp
 xor eax,eax
 mov ecx,14
 rep stosq
 mov rax,[r10]
 mov [rsp],rax
 mov rax,[r10+8]
 mov [rsp+8],rax
 mov [rsp+32],r11
 mov dword ptr [rsp+40],1
 mov [rsp+48],r10
 mov rax,rsp
 mov [rsp+56],rax
 mov [rsp+72],r9
 mov [rsp+80],r8
 lea rax,[rip+.La_checkpoint]
 mov [rsp+96],rax
 mov rdi,rbx
 lea rsi,[rsp+48]
 mov rdx,r12
 call pg_document_tx_doc@PLT
 AE 112
.Lac_json_initial:
 push rbx
 mov rbx,rdx
 call json_tokener_parse@PLT
 mov [rbx],rax
 test rax,rax
 setz al
 movzx eax,al
 pop rbx
 ret
/* Pi uuidv7: wall time never decreases; random-seeded sequence orders this host. */
.Lac_provider_initial:
 AF 96
 mov rbx,rdx
 mov qword ptr [rbx],0
 xor r12d,r12d
.Lac_random:
 lea rdi,[rsp+r12]
 mov esi,16
 sub rsi,r12
 xor edx,edx
 call getrandom@PLT
 test rax,rax
 jg .Lac_random_bytes
 call __errno_location@PLT
 cmp dword ptr [rax],4
 je .Lac_random
 jmp .Lac_uuid_bad
.Lac_random_bytes:
 add r12,rax
 cmp r12,16
 jb .Lac_random
 xor edi,edi
 lea rsi,[rsp+16]
 call clock_gettime@PLT
 test eax,eax
 jnz .Lac_uuid_bad
 mov rax,[rsp+24]
 xor edx,edx
 mov ecx,1000000
 div rcx
 imul rdx,[rsp+16],1000
 add rax,rdx
 mov rcx,0xffffffffffff
 cmp rax,rcx
 ja .Lac_uuid_bad
 cmp rax,[rip+.Lac_timestamp]
 jae .Lac_uuid_time
 mov rax,[rip+.Lac_timestamp]
.Lac_uuid_time:
 mov [rip+.Lac_timestamp],rax
 mov ecx,5
.Lac_uuid_time_byte:
 mov [rsp+rcx],al
 shr rax,8
 dec ecx
 jns .Lac_uuid_time_byte
 cmp byte ptr [rip+.Lac_seeded],0
 jne .Lac_uuid_sequence_next
 /* Random bytes 1..5 were replaced by timestamp: retain an independent
    random 40-bit seed from bytes 11..15. */
 xor r13d,r13d
 mov ecx,11
.Lac_uuid_sequence_seed:
 shl r13,8
 movzx eax,byte ptr [rsp+rcx]
 or r13,rax
 inc ecx
 cmp ecx,16
 jb .Lac_uuid_sequence_seed
 mov byte ptr [rip+.Lac_seeded],1
 jmp .Lac_uuid_sequence_ready
.Lac_uuid_sequence_next:
 mov r13,[rip+.Lac_sequence]
 inc r13
 mov rax,0x1ffffffffff
 cmp r13,rax
 ja .Lac_uuid_bad
.Lac_uuid_sequence_ready:
 mov [rip+.Lac_sequence],r13
 mov rax,r13
 shr rax,37
 or al,0x70
 mov [rsp+6],al
 mov rax,r13
 shr rax,29
 mov [rsp+7],al
 mov rax,r13
 shr rax,23
 and al,0x3f
 or al,0x80
 mov [rsp+8],al
 mov rax,r13
 shr rax,15
 mov [rsp+9],al
 mov rax,r13
 shr rax,7
 mov [rsp+10],al
 mov al,[rsp+11]
 and al,1
 mov rdx,r13
 shl dl,1
 or al,dl
 mov [rsp+11],al
 xor ecx,ecx
 xor edx,edx
 lea r8,[rip+.Lac_hex]
.Lac_uuid_hex:
 cmp ecx,4
 je .Lac_uuid_dash
 cmp ecx,6
 je .Lac_uuid_dash
 cmp ecx,8
 je .Lac_uuid_dash
 cmp ecx,10
 jne .Lac_uuid_hex_byte
.Lac_uuid_dash:
 mov byte ptr [rsp+rdx+32],45
 inc edx
.Lac_uuid_hex_byte:
 movzx eax,byte ptr [rsp+rcx]
 mov esi,eax
 shr eax,4
 and esi,15
 mov al,[r8+rax]
 mov [rsp+rdx+32],al
 inc edx
 mov al,[r8+rsi]
 mov [rsp+rdx+32],al
 inc edx
 inc ecx
 cmp ecx,16
 jb .Lac_uuid_hex
 lea rdi,[rsp+32]
 mov esi,36
 call json_object_new_string_len@PLT
 mov r12,rax
 test rax,rax
 jz .Lac_uuid_bad
 call json_object_new_object@PLT
 mov r13,rax
 test rax,rax
 jz .Lac_uuid_string_bad
 mov rdi,rax
 lea rsi,[rip+.Lac_session_id]
 mov rdx,r12
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lac_uuid_object_bad
 mov [rbx],r13
 xor eax,eax
 AE 96
.Lac_uuid_object_bad:
 mov rdi,r13
 call json_object_put@PLT
.Lac_uuid_string_bad:
 mov rdi,r12
 call json_object_put@PLT
.Lac_uuid_bad:
 mov eax,5
 AE 96

.Lac_created:
 AF 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rsp+24],0
 mov rax,[rbx]
 mov rax,[rax]
 mov rbp,[rax+160]
 mov rdi,r13
 lea rsi,[rip+.Lac_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov r14,rax
 mov qword ptr [rsp],0
.Lac_created_docs:
 mov rax,[rsp]
 lea rdx,[rip+.Lac_documents]
 shl rax,4
 mov rcx,[rdx+rax+8]
 mov rdx,[rdx+rax]
 test rdx,rdx
 jz .Lac_created_owner
 mov rax,[rbx]
 mov r8,[rax+24]
 mov rdi,rbp
 mov rsi,r14
 lea r9,[rsp+16]
 call .Lac_doc
 test eax,eax
 jnz .Lac_created_bad
 inc qword ptr [rsp]
 jmp .Lac_created_docs
.Lac_created_owner:
 mov rdi,r13
 lea rsi,[rip+.Lac_parent]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lac_created_configure
 mov rdi,r13
 lea rsi,[rip+.Lac_owner]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lac_created_configure
 mov rdi,rax
 lea rsi,[rip+.Lac_conversation_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rsi,rax
 mov rdi,rbp
 lea rdx,[rip+.La_definition]
 lea rcx,[rip+.La_initializer]
 mov rax,[rbx]
 mov r8,[rax+24]
 lea r9,[rsp+16]
 call .Lac_doc
 test eax,eax
 jnz .Lac_created_bad
 mov rdi,[rsp+16]
 lea rsi,[rsp+24]
 call pg_document_change_snapshot@PLT
 test eax,eax
 jnz .Lac_created_bad
 mov rax,[rbx]
 mov rcx,[rax+24]
 mov rdi,rbp
 mov rsi,r14
 mov rdx,[rsp+24]
 call pg_agent_state_stage@PLT
 test eax,eax
 jnz .Lac_created_bad
.Lac_created_configure:
 mov rax,[rbx+8]
 mov rdx,[rax+32]
 test rdx,rdx
 jz .Lac_created_init
 mov rax,[rbx]
 mov rcx,[rax+24]
 mov rdi,rbp
 mov rsi,r14
 call pg_agent_state_stage@PLT
 test eax,eax
 jnz .Lac_created_bad
.Lac_created_init:
 mov rax,[rbx+8]
 cmp qword ptr [rax+48],0
 je .Lac_created_ok
 mov rdi,[rax+40]
 mov rsi,r12
 mov rdx,r14
 call [rax+48]
 test eax,eax
 jnz .Lac_created_bad
.Lac_created_ok:
 xor r12d,r12d
 jmp .Lac_created_end
.Lac_created_bad:
 mov r12d,5
.Lac_created_end:
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov eax,r12d
 AE 48
.globl pg_agent_conversation_create
.type pg_agent_conversation_create,@function
pg_agent_conversation_create:
 AF 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 test r13,r13
 jz .Lac_create_invalid
 mov qword ptr [r13],0
 test rbx,rbx
 jz .Lac_create_invalid
 test r12,r12
 jz .Lac_create_invalid
 mov [rsp],rbx
 mov [rsp+8],r12
 mov rax,rsp
 mov [rsp+16],rax
 mov rax,[rbx+24]
 mov [rsp+24],rax
 mov rax,[rbx+32]
 mov [rsp+32],rax
 lea rax,[rip+.Lac_created]
 mov [rsp+40],rax
 mov rdi,[rbx]
 mov rsi,r12
 lea rdx,[rsp+16]
 mov rcx,[rbx+8]
 mov r8,[rbx+16]
 mov r9,r13
 call pg_durable_session_create_conversation@PLT
 AE 48
.Lac_create_invalid:
 mov eax,1
 AE 48
.globl pg_provider_state_read
.type pg_provider_state_read,@function
pg_provider_state_read:
 AF 48
 mov rbx,rdi
 mov r12,rdx
 mov r8,rsi
 mov rdi,rsp
 xor eax,eax
 mov ecx,6
 rep stosq
 lea rax,[rip+.Lac_provider_kind]
 mov [rsp],rax
 mov qword ptr [rsp+8],14
 mov [rsp+32],r8
 mov dword ptr [rsp+40],1
 mov rdi,rbx
 lea rsi,[rip+.Lac_provider_definition]
 mov rdx,rsp
 mov rcx,r12
 call pg_durable_snapshot@PLT
 AE 48
.section .rodata
.Lac_id: .asciz "id"
.Lac_parent: .asciz "parent"
.Lac_owner: .asciz "owner"
.Lac_conversation_id: .asciz "conversationId"
.Lac_session_id: .asciz "sessionId"
.Lac_hex: .ascii "0123456789abcdef"
.Lac_empty: .asciz "{}"
.Lac_inbox_empty: .asciz "{\"items\":[]}"
.Lac_usage_empty: .asciz "{\"models\":{},\"tools\":{}}"
.Lac_live_kind: .asciz "pangu.live"
.Lac_inbox_kind: .asciz "pangu.inbox"
.Lac_usage_kind: .asciz "pangu.usage"
.Lac_provider_kind: .asciz "pangu.provider"
.section .data.rel.ro
.balign 8
.Lac_live_definition:
 .quad .Lac_live_kind,10,1
 .long 1,1,2,0
 .quad 0,0
.Lac_inbox_definition:
 .quad .Lac_inbox_kind,11,1
 .long 1,1,2,0
 .quad 0,0
.Lac_usage_definition:
 .quad .Lac_usage_kind,11,1
 .long 1,1,2,0
 .quad 0,0
.Lac_provider_definition:
 .quad .Lac_provider_kind,14,1
 .long 1,1,2,0
 .quad 0,0
.Lac_empty_initializer: .quad .Lac_empty,.Lac_json_initial
.Lac_inbox_initializer: .quad .Lac_inbox_empty,.Lac_json_initial
.Lac_usage_initializer: .quad .Lac_usage_empty,.Lac_json_initial
.Lac_provider_initializer: .quad 0,.Lac_provider_initial
.Lac_documents:
 .quad .Lac_live_definition,.Lac_empty_initializer
 .quad .Lac_inbox_definition,.Lac_inbox_initializer
 .quad .Lac_usage_definition,.Lac_usage_initializer
 .quad .Lac_provider_definition,.Lac_provider_initializer
 .quad .La_definition,.La_initializer
 .quad 0,0
.section .bss
.balign 8
.Lac_timestamp: .zero 8
.Lac_sequence: .zero 8
.Lac_seeded: .zero 1
.section .note.GNU-stack,"",@progbits

.text
.globl pg_provider_state_ensure
.type pg_provider_state_ensure,@function
pg_provider_state_ensure:
 AF 112
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 test r13,r13
 jz .Lac_ensure_invalid
 mov qword ptr [r13],0
 test rbx,rbx
 jz .Lac_ensure_invalid
 mov rdi,rsp
 xor eax,eax
 mov ecx,14
 rep stosq
 mov rdi,[rbx]
 mov rsi,r12
 mov rdx,r13
 call pg_provider_state_read
 cmp eax,8
 jne .Lac_ensure_end
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+48]
 mov rcx,[rbx+8]
 mov r8,[rbx+16]
 call pg_durable_session_begin@PLT
 test eax,eax
 jnz .Lac_ensure_end
 lea rdi,[rsp+48]
 mov rsi,r12
 lea rdx,[rip+.Lac_provider_definition]
 lea rcx,[rip+.Lac_provider_initializer]
 mov r8,[rbx+24]
 lea r9,[rsp+104]
 call .Lac_doc
 test eax,eax
 jnz .Lac_ensure_abort
 mov rdi,[rsp+104]
 lea rsi,[rsp+96]
 call pg_document_change_snapshot@PLT
 test eax,eax
 jnz .Lac_ensure_abort
 mov rdi,[rbx]
 xor esi,esi
 xor edx,edx
 xor ecx,ecx
 call pg_durable_session_settle@PLT
 test eax,eax
 jnz .Lac_ensure_release
 mov rax,[rsp+96]
 mov [r13],rax
 xor eax,eax
 jmp .Lac_ensure_end
.Lac_ensure_abort:
 mov rdi,[rbx]
 call pg_durable_session_abort@PLT
.Lac_ensure_release:
 mov rdi,[rsp+96]
 call json_object_put@PLT
 mov eax,5
 jmp .Lac_ensure_end
.Lac_ensure_invalid:
 mov eax,4
.Lac_ensure_end:
 AE 112
.size pg_provider_state_ensure,.-pg_provider_state_ensure
.section .note.GNU-stack,"",@progbits
