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
 AF 176
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rdi,rsp
 xor eax,eax
 mov ecx,22
 rep stosq
 mov rdi,r13
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .La_stage_bad
 mov rdi,r13
 call .La_validate_text_fields
 test eax,eax
 jnz .La_stage_bad
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
 cmp r14,[rbx+1152]
 jae .La_find_missing
 imul rax,r14,72
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
 AF 1200
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
 mov qword ptr [rsp+1152],0
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
 cmp r14,[rbx+1152]
 jae .La_filter_init
 imul rbp,r14,72
 add rbp,rbx
 test r12,r12
 jz .La_filter_copy
 mov qword ptr [rsp+1168],0
.La_remove_each:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp [rsp+1168],rax
 jae .La_filter_copy
 mov rdi,r12
 mov rsi,[rsp+1168]
 call json_object_array_get_idx@PLT
 mov [rsp+1184],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .La_filter_bad
 mov rdi,[rbp]
 call strlen@PLT
 mov [rsp+1192],rax
 mov rdi,[rsp+1184]
 call json_object_get_string_len@PLT
 cmp rax,[rsp+1192]
 jne .La_remove_next
 mov rdi,[rsp+1184]
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,[rbp]
 call strcmp@PLT
 test eax,eax
 jz .La_filter_all_next
.La_remove_next:
 inc qword ptr [rsp+1168]
 jmp .La_remove_each
.La_filter_copy:
 mov rax,[rsp+1152]
 imul rax,rax,72
 lea rdi,[rsp+rax]
 mov rsi,rbp
 mov ecx,9
 rep movsq
 inc qword ptr [rsp+1152]
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
 mov [rsp+1160],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .La_filter_bad
 mov rdi,[rsp+1160]
 call json_object_get_string@PLT
 mov [rsp+1176],rax
 mov rdi,[rsp+1160]
 call json_object_get_string_len@PLT
 mov rdi,rbx
 mov rsi,[rsp+1176]
 mov edx,eax
 call .La_find
 test rax,rax
 jz .La_filter_array_next
 mov rbp,rax
 xor ecx,ecx
.La_filter_duplicate:
 cmp rcx,[rsp+1152]
 jae .La_filter_copy_array
 imul rax,rcx,72
 mov rax,[rsp+rax]
 cmp rax,[rbp]
 je .La_filter_array_next
 inc rcx
 jmp .La_filter_duplicate
.La_filter_copy_array:
 mov rax,[rsp+1152]
 imul rax,rax,72
 lea rdi,[rsp+rax]
 mov rsi,rbp
 mov ecx,9
 rep movsq
 inc qword ptr [rsp+1152]
.La_filter_array_next:
 inc r14
 jmp .La_filter_array
.La_filter_init:
 mov rdi,r13
 mov rsi,rsp
 mov rdx,[rsp+1152]
 call pg_tool_registry_init@PLT
 jmp .La_filter_end
.La_filter_bad:
 mov eax,5
.La_filter_end:
 AE 1200
.globl pg_agent_tools_select
.type pg_agent_tools_select,@function
pg_agent_tools_select:
 AF 1200
 mov rbx,rdi
 mov r12,rsi
 mov r13,rcx
 mov r14d,edx
 cmp qword ptr [rbx+1152],16
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
 mov ecx,148
 rep movsq
 xor eax,eax
 jmp .La_select_end
.La_select_bad:
 mov eax,5
.La_select_end:
 AE 1200
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
.La_request_fields: .quad .La_instructions,.La_tools,.La_model_tools,.La_extensions,0
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
