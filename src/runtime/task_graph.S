/* Pi Durable harness/task-graph.ts nodeOf/stateOf, fixed
   d5b3de22d0e24a3e9c38a9410d3329b099717283; MIT, docs/licenses/pi.md. */
.intel_syntax noprefix
.macro QF n
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\n
.endm
.macro QE n
 add rsp,\n
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.macro QG obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lq_\key]
 call json_object_object_get@PLT
.endm
.text
/* Owned target, field name, borrowed field. */
.Lq_field:
 push rbx
 push r12
 sub rsp,8
 mov [rsp],rdx
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 call json_object_get@PLT
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Lq_field_end
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,5
.Lq_field_end:
 add rsp,8
 pop r12
 pop rbx
 ret
/* Read owned conversations in ascending ID order, using existing scan cursor. */
.Lq_owned_conversations:
 QF 160
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r13],0
 mov rdi,rsp
 xor eax,eax
 mov ecx,20
 rep stosq
 mov dword ptr [rsp+4],1
 mov qword ptr [rsp+24],256
 mov qword ptr [rsp+32],4
 mov [rsp+56],r12
 call json_object_new_array@PLT
 mov [rsp+144],rax
 test rax,rax
 jz .Lq_conv_bad
.Lq_conv_page:
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+112]
 call [rbx+16]
 test eax,eax
 jnz .Lq_conv_bad
 xor r14d,r14d
.Lq_conv_each:
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lq_conv_next
 mov rdi,[rsp+112]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 QG rax,id
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,[rsp+144]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lq_conv_bad
 inc r14
 jmp .Lq_conv_each
.Lq_conv_next:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 cmp dword ptr [rsp+132],0
 je .Lq_conv_ok
 mov rax,[rsp+120]
 cmp rax,[rsp+16]
 jle .Lq_conv_bad
 mov [rsp+16],rax
 mov dword ptr [rsp+8],1
 mov eax,[rsp+128]
 mov [rsp+12],eax
 jmp .Lq_conv_page
.Lq_conv_ok:
 mov rax,[rsp+144]
 mov [r13],rax
 mov qword ptr [rsp+144],0
 xor ebp,ebp
 jmp .Lq_conv_end
.Lq_conv_bad:
 mov ebp,5
.Lq_conv_end:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov rdi,[rsp+144]
 call json_object_put@PLT
 mov eax,ebp
 QE 160
/* scheduler, borrowed task, output node. */
.Lq_node:
 QF 96
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r13],0
 mov rdi,rsp
 xor eax,eax
 mov ecx,12
 rep stosq
 QG r12,state
 mov [rsp+16],rax
 QG rax,status
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lq_terminal]
 call strcmp@PLT
 test eax,eax
 jz .Lq_node_skip
 lea rdi,[rip+.Lq_node_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lq_node_bad
 xor ebp,ebp
.Lq_node_fields:
 lea rax,[rip+.Lq_fields]
 mov rsi,[rax+rbp*8]
 test rsi,rsi
 jz .Lq_node_state
 mov [rsp+24],rsi
 mov rdi,r12
 call json_object_object_get@PLT
 test rax,rax
 jz .Lq_node_field_next
 mov rdi,[rsp]
 mov rsi,[rsp+24]
 mov rdx,rax
 call .Lq_field
 test eax,eax
 jnz .Lq_node_bad
.Lq_node_field_next:
 inc rbp
 jmp .Lq_node_fields
.Lq_node_state:
 QG "qword ptr [rsp]",state
 mov [rsp+8],rax
 QG "qword ptr [rsp+16]",status
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lq_status]
 mov rdx,rax
 call .Lq_field
 test eax,eax
 jnz .Lq_node_bad
 mov rdi,r14
 lea rsi,[rip+.Lq_completing]
 call strcmp@PLT
 test eax,eax
 jnz .Lq_node_phase
 QG "qword ptr [rsp+16]",outcome
 QG rax,status
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lq_outcome]
 mov rdx,rax
 call .Lq_field
 test eax,eax
 jnz .Lq_node_bad
 jmp .Lq_node_owned
.Lq_node_phase:
 QG "qword ptr [rsp+16]",checkpoint
 QG rax,phase
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lq_phase]
 mov rdx,rax
 call .Lq_field
 test eax,eax
 jnz .Lq_node_bad
 xor ebp,ebp
.Lq_node_wait_fields:
 lea rax,[rip+.Lq_state_fields]
 mov rsi,[rax+rbp*8]
 test rsi,rsi
 jz .Lq_node_owned
 mov [rsp+24],rsi
 mov rdi,[rsp+16]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lq_node_wait_next
 mov rdi,[rsp+8]
 mov rsi,[rsp+24]
 mov rdx,rax
 call .Lq_field
 test eax,eax
 jnz .Lq_node_bad
.Lq_node_wait_next:
 inc rbp
 jmp .Lq_node_wait_fields
.Lq_node_owned:
 mov rdi,rbx
 mov rsi,r12
 call .Lq_blocked
 test rax,rax
 jz .Lq_node_no_block
 mov rdi,rax
 call json_object_new_string@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lq_node_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lq_blocked_key]
 mov rdx,rax
 call .Lq_field
 test eax,eax
 jnz .Lq_node_bad
.Lq_node_no_block:
 QG r12,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rsi,rax
 mov rdi,[rbx+8]
 lea rdx,[rsp+32]
 call .Lq_owned_conversations
 test eax,eax
 jnz .Lq_node_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lq_conversations]
 mov rdx,[rsp+32]
 call .Lq_field
 test eax,eax
 jnz .Lq_node_bad
 mov rax,[rsp]
 mov [r13],rax
 mov qword ptr [rsp],0
.Lq_node_skip:
 xor ebp,ebp
 jmp .Lq_node_end
.Lq_node_bad:
 mov ebp,5
.Lq_node_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov eax,ebp
 QE 96
.globl pg_task_graph_read
.type pg_task_graph_read,@function
pg_task_graph_read:
 QF 256
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [r12],0
 mov rdi,rsp
 xor eax,eax
 mov ecx,32
 rep stosq
 cmp dword ptr [rbx+60],1
 jne .Lq_graph_bad
 cmp dword ptr [rbx+56],0
 jne .Lq_graph_busy
 lea rdi,[rsp+240]
 mov rsi,[rbx]
 call pg_durable_read_on_line_begin@PLT
 test eax,eax
 jnz .Lq_graph_busy
 mov dword ptr [rsp],2
 mov dword ptr [rsp+4],1
 mov qword ptr [rsp+24],256
 mov qword ptr [rsp+32],64
 lea rdi,[rip+.Lq_graph_json]
 call json_tokener_parse@PLT
 mov [rsp+144],rax
 test rax,rax
 jz .Lq_graph_bad
 QG rax,tasks
 mov r13,rax
.Lq_graph_page:
 mov rax,[rbx+8]
 mov rdi,[rax]
 mov rsi,rsp
 lea rdx,[rsp+112]
 call [rax+16]
 test eax,eax
 jnz .Lq_graph_bad
 xor r14d,r14d
.Lq_graph_each:
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lq_graph_next
 mov rdi,[rsp+112]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rbx
 mov rsi,rax
 lea rdx,[rsp+152]
 call .Lq_node
 test eax,eax
 jnz .Lq_graph_bad
 cmp qword ptr [rsp+152],0
 je .Lq_graph_skip
 QG "qword ptr [rsp+152]",id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rcx,rax
 lea rdi,[rsp+176]
 mov esi,64
 lea rdx,[rip+.Lq_decimal]
 xor eax,eax
 call snprintf@PLT
 mov rdi,r13
 lea rsi,[rsp+176]
 mov rdx,[rsp+152]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lq_graph_bad
 mov qword ptr [rsp+152],0
.Lq_graph_skip:
 inc r14
 jmp .Lq_graph_each
.Lq_graph_next:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 cmp dword ptr [rsp+132],0
 je .Lq_graph_status_next
 mov rax,[rsp+120]
 cmp rax,[rsp+16]
 jle .Lq_graph_bad
 mov [rsp+16],rax
 mov dword ptr [rsp+8],1
 mov eax,[rsp+128]
 mov [rsp+12],eax
 jmp .Lq_graph_page
.Lq_graph_status_next:
 inc dword ptr [rsp+96]
 cmp dword ptr [rsp+96],5
 je .Lq_graph_ok
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 jmp .Lq_graph_page
.Lq_graph_ok:
 mov rax,[rsp+144]
 mov [r12],rax
 mov qword ptr [rsp+144],0
 xor ebp,ebp
 jmp .Lq_graph_end
.Lq_graph_busy:
 mov ebp,3
 jmp .Lq_graph_end
.Lq_graph_bad:
 mov ebp,5
.Lq_graph_end:
 cmp dword ptr [rsp+248],0
 je .Lq_graph_line_released
 lea rdi,[rsp+240]
 call pg_durable_read_on_line_end@PLT
.Lq_graph_line_released:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov rdi,[rsp+144]
 call json_object_put@PLT
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov eax,ebp
 QE 256
.globl pg_task_graph_service
.type pg_task_graph_service,@function
pg_task_graph_service:
 mov [rdi],rsi
 mov qword ptr [rdi+8],0
 mov qword ptr [rdi+16],0
 mov qword ptr [rdi+24],0
 lea rax,[rip+pg_task_graph_read]
 mov [rdi+32],rax
 mov [rdx],rdi
 lea rax,[rip+.Lq_binding_read]
 mov [rdx+8],rax
 lea rax,[rip+pg_view_watch_open]
 mov [rdx+16],rax
 lea rax,[rip+pg_view_watch_next]
 mov [rdx+24],rax
 lea rax,[rip+pg_view_watch_close]
 mov [rdx+32],rax
 ret
.Lq_binding_read:
 cmp dword ptr [rdi+24],0
 jne .Lq_binding_closed
 mov rdi,[rdi]
 jmp pg_task_graph_read
.Lq_binding_closed:
 mov qword ptr [rsi],0
 mov eax,7
 ret
.section .rodata
.Lq_id: .asciz "id"
.Lq_kind: .asciz "kind"
.Lq_conversationId: .asciz "conversationId"
.Lq_owner: .asciz "owner"
.Lq_background: .asciz "background"
.Lq_abortRequested: .asciz "abortRequested"
.Lq_state: .asciz "state"
.Lq_status: .asciz "status"
.Lq_phase: .asciz "phase"
.Lq_checkpoint: .asciz "checkpoint"
.Lq_outcome: .asciz "outcome"
.Lq_on: .asciz "on"
.Lq_policy: .asciz "policy"
.Lq_until: .asciz "until"
.Lq_tasks: .asciz "tasks"
.Lq_conversations: .asciz "conversations"
.Lq_terminal: .asciz "terminal"
.Lq_completing: .asciz "completing"
.Lq_decimal: .asciz "%llu"
.Lq_node_json: .asciz "{\"background\":false,\"abortRequested\":false,\"state\":{}}"
.Lq_graph_json: .asciz "{\"tasks\":{}}"
.section .data.rel.ro
.balign 8
.Lq_fields: .quad .Lq_id,.Lq_kind,.Lq_conversationId,.Lq_owner,.Lq_background,.Lq_abortRequested,0
.Lq_state_fields: .quad .Lq_on,.Lq_policy,.Lq_until,0
.section .note.GNU-stack,"",@progbits

.text
/* Inspect registry metadata without invoking task phases or migration. */
.Lq_blocked:
 QF 32
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 QG r12,state
 QG rax,status
 mov rdi,rax
 call json_object_get_string@PLT
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Lq_running]
 call strcmp@PLT
 test eax,eax
 jz .Lq_block_ready
 mov rdi,r13
 lea rsi,[rip+.Lq_completing]
 call strcmp@PLT
 test eax,eax
 jz .Lq_block_ready
 QG r12,abortRequested
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lq_block_ready
 QG r12,kind
 mov r13,rax
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 mov rdi,r13
 call json_object_get_string_len@PLT
 mov edx,eax
 mov rax,[rbx+8]
 mov rdi,[rax]
 mov rsi,r14
 mov rcx,rsp
 call [rax+32]
 cmp eax,1
 je .Lq_block_missing
 test eax,eax
 jnz .Lq_block_ready
 cmp qword ptr [rsp],0
 je .Lq_block_missing
 QG r12,restarts
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rbx+40]
 jb .Lq_block_version
 mov rax,[rsp]
 cmp qword ptr [rax+80],0
 je .Lq_block_restarts
.Lq_block_version:
 QG r12,version
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rcx,[rsp]
 cmp rax,[rcx+16]
 ja .Lq_block_old
 je .Lq_block_ready
 mov [rsp+8],rcx
 QG r12,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov r13,rax
 xor r14d,r14d
.Lq_block_migration_runner:
 cmp r14,[rbx+24]
 jae .Lq_block_migration_available
 mov rax,[rbx+16]
 mov rax,[rax+r14*8]
 cmp [rax+16],r13
 jne .Lq_block_migration_next
 mov rcx,[rsp+8]
 cmp [rax+120],rcx
 je .Lq_block_migration
.Lq_block_migration_next:
 inc r14
 jmp .Lq_block_migration_runner
.Lq_block_migration_available:
 mov rcx,[rsp+8]
 cmp qword ptr [rcx+72],0
 je .Lq_block_migration
.Lq_block_ready:
 xor eax,eax
 jmp .Lq_block_end
.Lq_block_missing:
 lea rax,[rip+.Lq_missing]
 jmp .Lq_block_end
.Lq_block_old:
 lea rax,[rip+.Lq_old]
 jmp .Lq_block_end
.Lq_block_migration:
 lea rax,[rip+.Lq_migration]
 jmp .Lq_block_end
.Lq_block_restarts:
 lea rax,[rip+.Lq_restart_limit]
.Lq_block_end:
 QE 32
.section .rodata
.Lq_blocked_key: .asciz "blocked"
.Lq_running: .asciz "running"
.Lq_restarts: .asciz "restarts"
.Lq_version: .asciz "version"
.Lq_missing: .asciz "missing_task"
.Lq_old: .asciz "task_too_old"
.Lq_migration: .asciz "migration_failed"
.Lq_restart_limit: .asciz "restart_limit"

.text
