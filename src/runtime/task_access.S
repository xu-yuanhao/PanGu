/* Pi Durable scheduler.ts runtime memo/read and harness/types.ts HookApi,
   d5b3de22d0e24a3e9c38a9410d3329b099717283. MIT docs/licenses/pi.md. */
.intel_syntax noprefix
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
.macro AG obj,key
 mov rdi,\obj
 lea rsi,[rip+.La_\key]
 call json_object_object_get@PLT
.endm
.text
.Laccess_valid:
 test rdi,rdi
 jz .Laccess_invalid
 mov rax,[rdi]
 test rax,rax
 jz .Laccess_invalid
 cmp dword ptr [rax+76],1
 jne .Laccess_invalid
 cmp dword ptr [rax+84],0
 jne .Laccess_invalid
 mov rcx,[rdi+8]
 test rcx,rcx
 jz .Laccess_invalid
 cmp [rax+32],rcx
 jne .Laccess_invalid
 cmp dword ptr [rax+92],0
 jne .Laccess_invalid
 mov rax,[rax]
 cmp dword ptr [rax+172],1
 jne .Laccess_invalid
 mov eax,1
 ret
.Laccess_invalid:
 xor eax,eax
 ret
.globl pg_task_access_open
.type pg_task_access_open,@function
pg_task_access_open:
 AF 16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor eax,eax
 mov ecx,5
 rep stosq
 mov [rbx],r12
 mov rax,[r12+32]
 mov [rbx+8],rax
 mov rdi,rbx
 call .Laccess_valid
 test eax,eax
 jz .Laccess_open_bad
 mov [rbx+16],r13
 mov rax,[r12+16]
 mov [rbx+24],rax
 AG "qword ptr [r12+32]",conversationId
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rbx+32],rax
 xor eax,eax
 AE 16
.Laccess_open_bad:
 mov qword ptr [rbx],0
 mov eax,7
 AE 16
/* Every byte contributes two non-NUL key bytes; no object prototype lookup. */
.Lmemo_key:
 AF 16
 mov rbx,rdi
 mov r12,rsi
 cmp r12,2147483647
 ja .Lmemo_key_bad
 test r12,r12
 jz .Lmemo_key_allocate
 test rbx,rbx
 jz .Lmemo_key_bad
.Lmemo_key_allocate:
 lea rdi,[r12*2+1]
 call malloc@PLT
 test rax,rax
 jz .Lmemo_key_end
 xor ecx,ecx
 lea r8,[rip+.La_hex]
.Lmemo_key_byte:
 cmp rcx,r12
 jae .Lmemo_key_done
 movzx edx,byte ptr [rbx+rcx]
 mov esi,edx
 shr esi,4
 movzx esi,byte ptr [r8+rsi]
 mov [rax+rcx*2],sil
 and edx,15
 movzx edx,byte ptr [r8+rdx]
 mov [rax+rcx*2+1],dl
 inc rcx
 jmp .Lmemo_key_byte
.Lmemo_key_done:
 mov byte ptr [rax+r12*2],0
 jmp .Lmemo_key_end
.Lmemo_key_bad:
 xor eax,eax
.Lmemo_key_end:
 AE 16
.globl pg_task_access_memo
.type pg_task_access_memo,@function
pg_task_access_memo:
 AF 224
 mov rbx,rdi
 mov r12,rcx
 mov [rsp+176],r8d
 mov [rsp+184],r9
 mov rax,[rsp+272]
 mov [rsp+192],rax
 mov rdi,rsp
 xor eax,eax
 mov ecx,15
 rep stosq
 mov rax,[rsp+192]
 mov qword ptr [r9],0
 mov dword ptr [rax],0
 mov qword ptr [rsp+120],0
 mov qword ptr [rsp+128],0
 mov qword ptr [rsp+136],0
 mov qword ptr [rsp+144],0
 mov dword ptr [rsp+152],0
 mov dword ptr [rsp+156],0
 mov [rsp+160],rsi
 mov [rsp+168],rdx
 mov rdi,rbx
 call .Laccess_valid
 mov ebp,7
 test eax,eax
 jz .Lmemo_end
 mov r13,[rbx]
 mov rdi,[r13]
 call pg_durable_session_idle@PLT
 mov ebp,1
 test eax,eax
 jz .Lmemo_end
 mov rdi,[rsp+160]
 mov rsi,[rsp+168]
 call .Lmemo_key
 mov [rsp+120],rax
 mov ebp,5
 test rax,rax
 jz .Lmemo_end
 mov r14,[r13+8]
 mov rdi,[r13]
 mov rsi,rsp
 lea rdx,[rsp+48]
 mov rcx,[r14]
 mov r8,[r14+8]
 call pg_durable_session_begin@PLT
 mov ebp,1
 test eax,eax
 jnz .Lmemo_end
 mov dword ptr [rsp+152],1
 mov rdi,rsp
 mov esi,2
 mov rdx,[rbx+24]
 lea rcx,[rsp+128]
 call pg_record_tx_read@PLT
 mov ebp,6
 test eax,eax
 jnz .Lmemo_abort
 AG "qword ptr [rsp+128]",state
 AG rax,status
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lmemo_closed
 mov rdi,rax
 lea rsi,[rip+.La_running]
 call strcmp@PLT
 test eax,eax
 jnz .Lmemo_closed
 cmp qword ptr [r13+48],0
 je .Lmemo_lookup
 AG "qword ptr [rsp+128]",abortRequested
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lmemo_closed
.Lmemo_lookup:
 AG "qword ptr [rsp+128]",memos
 mov [rsp+160],rax
 test rax,rax
 jz .Lmemo_missing
 mov rdi,rax
 mov rsi,[rsp+120]
 lea rdx,[rsp+136]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lmemo_missing
 mov dword ptr [rsp+156],1
 jmp .Lmemo_copy_winner
.Lmemo_missing:
 cmp dword ptr [rsp+176],0
 je .Lmemo_read_absent
 test r12,r12
 jz .Lmemo_candidate_valid
 mov rdi,r12
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lmemo_invalid
.Lmemo_candidate_valid:
 cmp qword ptr [rsp+160],0
 jne .Lmemo_candidate
 call json_object_new_object@PLT
 mov [rsp+160],rax
 test rax,rax
 jz .Lmemo_allocation
 mov rdi,[rsp+128]
 lea rsi,[rip+.La_memos]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lmemo_allocation
.Lmemo_candidate:
 test r12,r12
 jz .Lmemo_candidate_copied
 mov rdi,r12
 lea rsi,[rsp+144]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lmemo_allocation
.Lmemo_candidate_copied:
 mov rdi,[rsp+160]
 mov rsi,[rsp+120]
 mov rdx,[rsp+144]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lmemo_allocation
 mov qword ptr [rsp+144],0
 mov rdi,rsp
 mov rsi,[rsp+128]
 mov rdx,[r14]
 mov rcx,[r14+40]
 call pg_record_tx_set_task@PLT
 test eax,eax
 jnz .Lmemo_storage
 mov rdi,[r13]
 xor esi,esi
 xor edx,edx
 xor ecx,ecx
 call pg_durable_session_settle@PLT
 mov dword ptr [rsp+152],0
 test eax,eax
 jnz .Lmemo_storage
 mov [rsp+136],r12
 mov dword ptr [rsp+156],1
.Lmemo_copy_winner:
 cmp qword ptr [rsp+136],0
 je .Lmemo_read_absent
 mov rdi,[rsp+136]
 lea rsi,[rsp+144]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lmemo_allocation
.Lmemo_read_absent:
 xor ebp,ebp
 jmp .Lmemo_abort
.Lmemo_closed:
 mov ebp,7
 jmp .Lmemo_abort
.Lmemo_invalid:
 mov ebp,4
 jmp .Lmemo_abort
.Lmemo_allocation:
 mov ebp,5
 jmp .Lmemo_abort
.Lmemo_storage:
 mov ebp,6
.Lmemo_abort:
 cmp dword ptr [rsp+152],0
 je .Lmemo_publish
 mov rdi,[r13]
 call pg_durable_session_abort@PLT
 mov dword ptr [rsp+152],0
.Lmemo_publish:
 test ebp,ebp
 jnz .Lmemo_end
 mov rax,[rsp+184]
 mov rdx,[rsp+144]
 mov [rax],rdx
 mov qword ptr [rsp+144],0
 mov rax,[rsp+192]
 mov edx,[rsp+156]
 mov [rax],edx
.Lmemo_end:
 mov rdi,[rsp+120]
 call free@PLT
 mov rdi,[rsp+128]
 call json_object_put@PLT
 mov rdi,[rsp+144]
 call json_object_put@PLT
 mov eax,ebp
 AE 224
.globl pg_task_scheduler_find
.type pg_task_scheduler_find,@function
pg_task_scheduler_find:
 xor ecx,ecx
 mov rdx,[rdi+24]
 mov r8,[rdi+16]
.Lfind_runner:
 cmp rcx,rdx
 jae .Lfind_runner_missing
 mov rax,[r8+rcx*8]
 inc rcx
 test rax,rax
 jz .Lfind_runner
 cmp [rax+16],rsi
 je .Lfind_runner_found
 jmp .Lfind_runner
.Lfind_runner_missing:
 xor eax,eax
.Lfind_runner_found:
 ret
.section .rodata
.La_hex: .ascii "0123456789abcdef"
.La_conversationId: .asciz "conversationId"
.La_state: .asciz "state"
.La_status: .asciz "status"
.La_running: .asciz "running"
.La_abortRequested: .asciz "abortRequested"
.La_memos: .asciz "memos"
.section .note.GNU-stack,"",@progbits
.text
.globl pg_task_access_get_task
.type pg_task_access_get_task,@function
pg_task_access_get_task:
 AF 128
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r13],0
 mov rdi,rsp
 xor eax,eax
 mov ecx,16
 rep stosq
 mov rdi,rbx
 call .Laccess_valid
 mov ebp,7
 test eax,eax
 jz .Lread_end
 mov r14,[rbx]
 mov rdi,rsp
 mov rsi,[r14]
 call pg_durable_read_on_line_begin@PLT
 mov ebp,1
 test eax,eax
 jnz .Lread_end
 mov rax,[r14]
 mov [rsp+16],rax
 mov rax,[r14+8]
 mov rcx,[rax]
 mov [rsp+24],rcx
 mov rcx,[rax+8]
 mov [rsp+32],rcx
 lea rdi,[rsp+16]
 mov esi,2
 mov rdx,r12
 mov rcx,r13
 call pg_durable_storage_read@PLT
 mov ebp,eax
 mov rdi,rsp
 call pg_durable_read_on_line_end@PLT
 test ebp,ebp
 jz .Lread_end
 cmp ebp,1
 je .Lread_end
 mov ebp,6
.Lread_end:
 mov eax,ebp
 AE 128
.globl pg_task_access_snapshot
.type pg_task_access_snapshot,@function
pg_task_access_snapshot:
 AF 32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov qword ptr [rcx],0
 call .Laccess_valid
 test eax,eax
 jz .Lsnapshot_closed
 mov rax,[rbx]
 mov rdi,[rax]
 mov rsi,r12
 mov rdx,r13
 mov rcx,r14
 call pg_durable_snapshot@PLT
 AE 32
.Lsnapshot_closed:
 mov eax,14
 AE 32
.globl pg_task_access_snapshot_as_of
.type pg_task_access_snapshot_as_of,@function
pg_task_access_snapshot_as_of:
 AF 32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov [rsp],r8
 mov qword ptr [r8],0
 call .Laccess_valid
 test eax,eax
 jz .Lsnapshot_as_closed
 cmp qword ptr [rbx+16],0
 je .Lsnapshot_as_invalid
 mov rax,[rbx]
 mov rdi,[rax]
 mov rsi,[rbx+16]
 mov rdx,r12
 mov rcx,r13
 mov r8,r14
 mov r9,[rsp]
 call pg_durable_snapshot_as_of@PLT
 AE 32
.Lsnapshot_as_invalid:
 mov eax,1
 AE 32
.Lsnapshot_as_closed:
 mov eax,14
 AE 32

.text
.globl pg_task_access_create_task
.type pg_task_access_create_task,@function
pg_task_access_create_task:
 AF 112
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov [rsp+80],r8
 mov [rsp+88],r9
 mov qword ptr [r9],0
 call .Laccess_valid
 mov ebp,7
 test eax,eax
 jz .Lchild_end
 mov rdi,rsp
 xor eax,eax
 mov ecx,10
 rep stosq
 mov [rsp],r12
 mov [rsp+8],r13
 mov [rsp+16],r14
 mov rax,[r12+24]
 mov [rsp+24],rax
 mov rax,[r12+32]
 mov [rsp+32],rax
 mov rax,[rsp+80]
 mov [rsp+40],rax
 mov rax,[rbx+32]
 mov [rsp+56],rax
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rip+.Lchild_stage]
 call pg_task_runner_commit_with@PLT
 mov ebp,eax
 test eax,eax
 jnz .Lchild_end
 mov rax,[rsp+88]
 mov rdx,[rsp+72]
 mov [rax],rdx
.Lchild_end:
 mov eax,ebp
 AE 112
.Lchild_stage:
 AF 32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rcx
 mov qword ptr [r13],0
 mov qword ptr [rsp],0
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lchild_bad
 mov r14,[rbx]
 mov rdi,[r14]
 mov esi,[r14+8]
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Lchild_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lchild_name]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lchild_bad
 mov rdi,[r14+16]
 call json_object_new_uint64@PLT
 test rax,rax
 jz .Lchild_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lchild_version]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lchild_bad
 mov rdi,r12
 mov rsi,[rsp]
 mov rdx,[rbx+8]
 mov rcx,[rbx+16]
 lea r8,[rbx+24]
 lea r9,[rbx+72]
 call pg_record_tx_create_task@PLT
 test eax,eax
 jnz .Lchild_bad
 xor ebp,ebp
 jmp .Lchild_stage_end
.Lchild_bad:
 mov ebp,4
.Lchild_stage_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 AE 32
.section .rodata
.Lchild_name: .asciz "name"
.Lchild_version: .asciz "version"

.text
.globl pg_task_access_create_conversation
.type pg_task_access_create_conversation,@function
pg_task_access_create_conversation:
 AF 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov qword ptr [rsp+16],0
 mov ebp,4
 test r14,r14
 jz .Lacc_end
 mov qword ptr [r14],0
 test r12,r12
 jz .Lacc_end
 test r13,r13
 jz .Lacc_end
 call .Laccess_valid
 mov ebp,7
 test eax,eax
 jz .Lacc_end
 mov rax,[rbx]
 mov rax,[rax]
 cmp [r12],rax
 jne .Lacc_end
 mov [rsp],r12
 mov [rsp+8],r13
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rip+.Lacc_stage]
 call pg_task_runner_commit_with@PLT
 mov ebp,eax
 test eax,eax
 jnz .Lacc_end
 mov rax,[rsp+16]
 mov [r14],rax
 mov qword ptr [rsp+16],0
.Lacc_end:
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,ebp
 AE 48
.Lacc_stage:
 mov qword ptr [rcx],0
 mov rax,rdi
 mov rdi,[rax]
 mov rdx,[rax+8]
 lea rcx,[rax+16]
 jmp pg_agent_conversation_stage@PLT
.section .note.GNU-stack,"",@progbits
