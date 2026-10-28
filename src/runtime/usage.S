/* Pi Durable harness/usage.ts, d5b3de22d0e24a3e9c38a9410d3329b099717283.
   MIT; docs/licenses/pi.md. */
.intel_syntax noprefix
.macro UF n
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\n
.endm
.macro UE n
 add rsp,\n
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.text
.Lu_initial:
 push rbx
 mov rbx,rdx
 lea rdi,[rip+.Lu_empty]
 call json_tokener_parse@PLT
 mov [rbx],rax
 test rax,rax
 setz al
 movzx eax,al
 pop rbx
 ret
/* Mutable owned total, borrowed increment, zero-terminated key table. */
.Lu_add:
 UF 32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
.Lu_add_next:
 mov rsi,[r13+r14*8]
 test rsi,rsi
 jz .Lu_add_ok
 mov [rsp+8],rsi
 mov rdi,r12
 call json_object_object_get@PLT
 mov [rsp+16],rax
 mov rdi,rbx
 mov rsi,[rsp+8]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lu_add_total
 cmp qword ptr [rsp+16],0
 je .Lu_add_skip
.Lu_add_total:
 mov rdi,rax
 call json_object_get_double@PLT
 movsd [rsp],xmm0
 mov rdi,[rsp+16]
 call json_object_get_double@PLT
 addsd xmm0,[rsp]
 call json_object_new_double@PLT
 test rax,rax
 jz .Lu_add_bad
 mov [rsp+24],rax
 mov rdi,rbx
 mov rsi,[rsp+8]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Lu_add_skip
 mov rdi,[rsp+24]
 call json_object_put@PLT
 jmp .Lu_add_bad
.Lu_add_skip:
 inc r14
 jmp .Lu_add_next
.Lu_add_ok:
 xor eax,eax
 jmp .Lu_add_end
.Lu_add_bad:
 mov eax,5
.Lu_add_end:
 UE 32
.globl pg_usage_record
.type pg_usage_record,@function
pg_usage_record:
 UF 192
 mov rbx,rdi
 mov r12,rsi
 mov r13,rcx
 mov r14,r8
 mov [rsp+176],r9
 mov [rsp+184],rdx
 mov rdi,rsp
 xor eax,eax
 mov ecx,22
 rep stosq
 cmp qword ptr [rsp+176],0
 je .Lu_record_ok
 lea rax,[rip+.Lu_kind]
 mov [rsp],rax
 mov qword ptr [rsp+8],11
 mov rdi,[rbx+32]
 lea rsi,[rip+.Lu_conversation]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+32],rax
 mov dword ptr [rsp+40],1
 lea rax,[rip+.Lu_definition]
 mov [rsp+48],rax
 lea rax,[rsp]
 mov [rsp+56],rax
 lea rax,[rip+.Lu_initializer]
 mov [rsp+72],rax
 mov rax,[rsp+184]
 mov [rsp+80],rax
 mov rdi,r12
 lea rsi,[rsp+48]
 lea rdx,[rsp+112]
 call pg_document_tx_doc@PLT
 test eax,eax
 jnz .Lu_record_bad
 mov rdi,[rsp+112]
 lea rsi,[rsp+120]
 call pg_document_change_snapshot@PLT
 test eax,eax
 jnz .Lu_record_bad
 mov rdi,[rsp+120]
 mov rsi,r13
 call json_object_object_get@PLT
 mov [rsp+128],rax
 test rax,rax
 jz .Lu_record_bad
 mov rdi,rax
 mov rsi,r14
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lu_record_existing
 mov rdi,[rsp+176]
 lea rsi,[rsp+136]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lu_record_bad
 jmp .Lu_record_install
.Lu_record_existing:
 mov [rsp+136],rax
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,[rsp+136]
 mov rsi,[rsp+176]
 lea rdx,[rip+.Lu_counters]
 call .Lu_add
 test eax,eax
 jnz .Lu_record_bad
 mov rdi,[rsp+176]
 lea rsi,[rip+.Lu_cost]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lu_record_install
 mov [rsp+144],rax
 mov rdi,[rsp+136]
 lea rsi,[rip+.Lu_cost]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lu_record_cost_add
 mov rdi,[rsp+144]
 lea rsi,[rsp+152]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lu_record_bad
 mov rdi,[rsp+136]
 lea rsi,[rip+.Lu_cost]
 mov rdx,[rsp+152]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lu_record_bad
 mov qword ptr [rsp+152],0
 jmp .Lu_record_install
.Lu_record_cost_add:
 mov rdi,rax
 mov rsi,[rsp+144]
 lea rdx,[rip+.Lu_costs]
 call .Lu_add
 test eax,eax
 jnz .Lu_record_bad
.Lu_record_install:
 mov rdi,[rsp+128]
 mov rsi,r14
 mov rdx,[rsp+136]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lu_record_bad
 mov qword ptr [rsp+136],0
 lea rdi,[rip+.Lu_patch]
 call json_tokener_parse@PLT
 mov [rsp+160],rax
 test rax,rax
 jz .Lu_record_bad
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov [rsp+168],rax
 mov rdi,rax
 mov esi,1
 call json_object_array_get_idx@PLT
 mov [rsp+144],rax
 mov rdi,r13
 call json_object_new_string@PLT
 mov rdi,[rsp+144]
 xor esi,esi
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Lu_record_bad
 mov rdi,[rsp+128]
 call json_object_get@PLT
 mov rdi,[rsp+168]
 mov esi,2
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Lu_record_bad
 mov rdi,[rsp+112]
 mov rsi,[rsp+160]
 call pg_document_change_append@PLT
 test eax,eax
 jnz .Lu_record_bad
.Lu_record_ok:
 xor ebp,ebp
 jmp .Lu_record_end
.Lu_record_bad:
 mov ebp,5
.Lu_record_end:
 mov rdi,[rsp+120]
 call json_object_put@PLT
 mov rdi,[rsp+136]
 call json_object_put@PLT
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov rdi,[rsp+160]
 call json_object_put@PLT
 mov eax,ebp
 UE 192
.globl pg_usage_model
.type pg_usage_model,@function
pg_usage_model:
 UF 64
 mov [rsp],rdi
 mov [rsp+8],rsi
 mov [rsp+16],rdx
 mov rbx,rcx
 mov r12,r8
 mov qword ptr [rsp+24],0
 mov rdi,r12
 lea rsi,[rip+.Lu_usage]
 call json_object_object_get@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lu_model_ok
 mov rdi,r12
 lea rsi,[rip+.Lu_provider]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lu_model_provider_value
 mov rdi,rbx
 lea rsi,[rip+.Lu_provider]
 call json_object_object_get@PLT
.Lu_model_provider_value:
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jnz .Lu_model_provider
 lea rax,[rip+.Lu_unknown]
.Lu_model_provider:
 mov r13,rax
 mov rdi,r12
 lea rsi,[rip+.Lu_model]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lu_model_model_value
 mov rdi,rbx
 lea rsi,[rip+.Lu_model]
 call json_object_object_get@PLT
.Lu_model_model_value:
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lu_model_bad
 mov r14,rax
 mov rdi,r13
 call strlen@PLT
 mov rbx,rax
 mov rdi,r14
 call strlen@PLT
 lea rdi,[rbx+rax+2]
 call malloc@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lu_model_bad
 mov rdi,rax
 mov rsi,r13
 call strcpy@PLT
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lu_slash]
 call strcat@PLT
 mov rdi,[rsp+24]
 mov rsi,r14
 call strcat@PLT
 mov rdi,[rsp]
 mov rsi,[rsp+8]
 mov rdx,[rsp+16]
 lea rcx,[rip+.Lu_models]
 mov r8,[rsp+24]
 mov r9,[rsp+32]
 call pg_usage_record
 mov ebp,eax
 jmp .Lu_model_end
.Lu_model_ok:
 xor ebp,ebp
 jmp .Lu_model_end
.Lu_model_bad:
 mov ebp,5
.Lu_model_end:
 mov rdi,[rsp+24]
 call free@PLT
 mov eax,ebp
 UE 64
.globl pg_usage_read
.type pg_usage_read,@function
pg_usage_read:
 UF 48
 mov rbx,rdi
 mov r12,rdx
 mov rdi,rsp
 mov r8,rsi
 xor eax,eax
 mov ecx,6
 rep stosq
 lea rax,[rip+.Lu_kind]
 mov [rsp],rax
 mov qword ptr [rsp+8],11
 mov [rsp+32],r8
 mov dword ptr [rsp+40],1
 mov rdi,rbx
 lea rsi,[rip+.Lu_definition]
 mov rdx,rsp
 mov rcx,r12
 call pg_durable_snapshot@PLT
 cmp eax,8
 jne .Lu_read_end
 lea rdi,[rip+.Lu_empty]
 call json_tokener_parse@PLT
 mov [r12],rax
 test rax,rax
 setz al
 movzx eax,al
.Lu_read_end:
 UE 48
.globl pg_usage_service
.type pg_usage_service,@function
pg_usage_service:
 mov [rsi],rdi
 lea rax,[rip+pg_usage_read]
 mov [rsi+8],rax
 ret
.globl pg_agent_snapshot
.type pg_agent_snapshot,@function
pg_agent_snapshot:
 UF 80
 mov rbx,rdi
 mov r13,rsi
 mov r12,rdx
 mov qword ptr [r12],0
 mov rdi,rsp
 xor eax,eax
 mov ecx,10
 rep stosq
 lea rax,[rip+.Lu_live_kind]
 mov [rsp],rax
 mov qword ptr [rsp+8],10
 mov [rsp+32],r13
 mov dword ptr [rsp+40],1
 mov rdi,rbx
 lea rsi,[rip+.Lu_live_definition]
 mov rdx,rsp
 lea rcx,[rsp+48]
 call pg_durable_snapshot@PLT
 cmp eax,8
 jne .Lu_snapshot_live
 call json_object_new_object@PLT
 mov [rsp+48],rax
 test rax,rax
 jz .Lu_snapshot_bad
 xor eax,eax
.Lu_snapshot_live:
 test eax,eax
 jnz .Lu_snapshot_result
 mov rdi,rbx
 mov rsi,r13
 lea rdx,[rsp+56]
 call pg_usage_read
 test eax,eax
 jnz .Lu_snapshot_result
 call json_object_new_object@PLT
 mov [rsp+64],rax
 test rax,rax
 jz .Lu_snapshot_bad
 mov rdi,rax
 lea rsi,[rip+.Lu_live]
 mov rdx,[rsp+48]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lu_snapshot_bad
 mov qword ptr [rsp+48],0
 mov rdi,[rsp+64]
 lea rsi,[rip+.Lu_usage]
 mov rdx,[rsp+56]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lu_snapshot_bad
 mov qword ptr [rsp+56],0
 mov rax,[rsp+64]
 mov [r12],rax
 mov qword ptr [rsp+64],0
 xor eax,eax
 jmp .Lu_snapshot_result
.Lu_snapshot_bad:
 mov eax,5
.Lu_snapshot_result:
 mov ebp,eax
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov eax,ebp
 UE 80
.globl pg_observation_service
.type pg_observation_service,@function
pg_observation_service:
 mov [rsi],rdi
 lea rax,[rip+pg_agent_snapshot]
 mov [rsi+8],rax
 mov rax,qword ptr [rip+json_object_put@GOTPCREL]
 mov [rsi+16],rax
 lea rax,[rip+pg_agent_watch_open]
 mov [rsi+24],rax
 lea rax,[rip+pg_agent_watch_next]
 mov [rsi+32],rax
 lea rax,[rip+pg_agent_watch_close]
 mov [rsi+40],rax
 ret
.section .rodata
.Lu_kind: .asciz "pangu.usage"
.Lu_empty: .asciz "{\"models\":{},\"tools\":{}}"
.Lu_patch: .asciz "[[\"s\",[\"\"],null]]"
.Lu_conversation: .asciz "conversationId"
.Lu_usage: .asciz "usage"
.Lu_provider: .asciz "provider"
.Lu_model: .asciz "model"
.Lu_models: .asciz "models"
.Lu_unknown: .asciz "openai-compatible"
.Lu_slash: .asciz "/"
.Lu_input: .asciz "input"
.Lu_output: .asciz "output"
.Lu_cacheRead: .asciz "cacheRead"
.Lu_cacheWrite: .asciz "cacheWrite"
.Lu_totalTokens: .asciz "totalTokens"
.Lu_cacheWrite1h: .asciz "cacheWrite1h"
.Lu_reasoning: .asciz "reasoning"
.Lu_cost: .asciz "cost"
.Lu_total: .asciz "total"
.section .data.rel.ro
.balign 8
.Lu_counters: .quad .Lu_input,.Lu_output,.Lu_cacheRead,.Lu_cacheWrite,.Lu_totalTokens,.Lu_cacheWrite1h,.Lu_reasoning,0
.Lu_costs: .quad .Lu_input,.Lu_output,.Lu_cacheRead,.Lu_cacheWrite,.Lu_total,0
.Lu_definition:
 .quad .Lu_kind,11,1
 .long 1,1,2,0
 .quad 0,0
.Lu_initializer: .quad 0,.Lu_initial
.section .note.GNU-stack,"",@progbits

.section .rodata
.Lu_live_kind: .asciz "pangu.live"
.Lu_live: .asciz "live"
.section .data.rel.ro
.balign 8
.Lu_live_definition:
 .quad .Lu_live_kind,10,1
 .long 1,1,2,0
 .quad 0,0

.text
.globl pg_agent_watch_open
.type pg_agent_watch_open,@function
pg_agent_watch_open:
 test rdi,rdi
 jz .Lu_watch_invalid
 test rsi,rsi
 jz .Lu_watch_invalid
 test rdx,rdx
 jz .Lu_watch_invalid
 cmp dword ptr [rdx+24],0
 jne .Lu_watch_invalid
 mov [rdx],rdi
 mov [rdx+8],rsi
 mov qword ptr [rdx+16],0
 mov dword ptr [rdx+24],1
 xor eax,eax
 ret
.Lu_watch_invalid:
 mov eax,1
 ret
.globl pg_agent_watch_next
.type pg_agent_watch_next,@function
pg_agent_watch_next:
 UF 16
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [r12],0
 mov qword ptr [rsp],0
 cmp dword ptr [rbx+24],1
 jne .Lu_watch_next_closed
 mov rdi,[rbx]
 mov rsi,[rbx+8]
 mov rdx,rsp
 call pg_agent_snapshot
 test eax,eax
 jnz .Lu_watch_next_error
 mov rdi,[rbx+16]
 test rdi,rdi
 jz .Lu_watch_changed
 mov rsi,[rsp]
 call json_object_equal@PLT
 test eax,eax
 jnz .Lu_watch_same
.Lu_watch_changed:
 mov qword ptr [rsp+8],0
 mov rdi,[rsp]
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lu_watch_copy_failed
 mov rdi,[rbx+16]
 call json_object_put@PLT
 mov rax,[rsp+8]
 mov [rbx+16],rax
 mov rax,[rsp]
 mov [r12],rax
 mov eax,1
 jmp .Lu_watch_next_end
.Lu_watch_copy_failed:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,-5
 jmp .Lu_watch_next_end
.Lu_watch_same:
 mov rdi,[rsp]
 call json_object_put@PLT
 xor eax,eax
 jmp .Lu_watch_next_end
.Lu_watch_next_error:
 neg eax
 jmp .Lu_watch_next_end
.Lu_watch_next_closed:
 mov eax,-1
.Lu_watch_next_end:
 UE 16
.globl pg_agent_watch_close
.type pg_agent_watch_close,@function
pg_agent_watch_close:
 push rbx
 mov rbx,rdi
 test rdi,rdi
 jz .Lu_watch_close_end
 mov rdi,[rbx+16]
 call json_object_put@PLT
 pxor xmm0,xmm0
 movdqu [rbx],xmm0
 movdqu [rbx+16],xmm0
.Lu_watch_close_end:
 pop rbx
 ret
