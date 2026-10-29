/* Pi Durable compaction.ts beforeCompact/summarizedMessages and hook ordering.
   Fixed source d5b3de22d0e24a3e9c38a9410d3329b099717283; MIT, docs/licenses/pi.md. */
.intel_syntax noprefix
.macro HF bytes
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\bytes
.endm
.macro HE bytes
 add rsp,\bytes
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.macro HG object,key
 mov rdi,\object
 lea rsi,[rip+.Lh_\key]
 call json_object_object_get@PLT
.endm
.text
.Lh_copy:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov rdi,rdx
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lh_copy_bad
 mov rdi,rbx
 mov rsi,r12
 mov rdx,[rsp]
 call json_object_object_add@PLT
 test eax,eax
 jz .Lh_copy_end
.Lh_copy_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,5
.Lh_copy_end:
 add rsp,24
 pop r12
 pop rbx
 ret
.globl pg_compaction_hook_open
.type pg_compaction_hook_open,@function
pg_compaction_hook_open:
 HF 64
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov [rsp+40],r8
 mov qword ptr [r8],0
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 test rbx,rbx
 jz .Lh_open_alloc
 cmp dword ptr [rbx],1
 jne .Lh_open_bad
 cmp dword ptr [rbx+4],40
 jne .Lh_open_bad
 cmp qword ptr [rbx+32],0
 je .Lh_open_bad
 cmp qword ptr [rbx+16],0
 je .Lh_open_alloc
 cmp qword ptr [rbx+8],0
 je .Lh_open_bad
.Lh_open_alloc:
 mov edi,1
 mov esi,72
 call calloc@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lh_open_bad
 mov [rax],rbx
 mov rdi,r12
 lea rsi,[rax+16]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lh_open_bad
 call json_object_new_object@PLT
 mov rbx,[rsp]
 mov [rbx+8],rax
 test rax,rax
 jz .Lh_open_bad
 mov rdi,r13
 call json_object_new_uint64@PLT
 mov rdi,[rbx+8]
 lea rsi,[rip+.Lh_firstKept]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lh_open_bad
 HG r14,reason
 mov rdi,[rbx+8]
 lea rsi,[rip+.Lh_reason]
 mov rdx,rax
 call .Lh_copy
 test eax,eax
 jnz .Lh_open_bad
 HG r14,instructions
 test rax,rax
 jz .Lh_open_arrays
 mov rdi,[rbx+8]
 lea rsi,[rip+.Lh_instructions]
 mov rdx,rax
 call .Lh_copy
 test eax,eax
 jnz .Lh_open_bad
.Lh_open_arrays:
 call json_object_new_array@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lh_open_bad
 call json_object_new_array@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lh_open_bad
 HG "qword ptr [rbx+16]",entries
 mov r12,rax
 HG "qword ptr [rbx+16]",contributions
 mov r14,rax
 mov qword ptr [rsp+32],0
.Lh_open_entry:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp [rsp+32],rax
 jae .Lh_open_order
 mov rdi,r12
 mov rsi,[rsp+32]
 call json_object_array_get_idx@PLT
 mov [rsp+48],rax
 HG rax,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,r13
 je .Lh_open_order
 mov rdi,[rsp+48]
 call json_object_get@PLT
 mov rdi,[rsp+8]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lh_open_bad
 mov rdi,r14
 mov rsi,[rsp+32]
 call json_object_array_get_idx@PLT
 mov [rsp+56],rax
 xor ebp,ebp
.Lh_open_message:
 mov rdi,[rsp+56]
 call json_object_array_length@PLT
 cmp rbp,rax
 jae .Lh_open_next
 mov rdi,[rsp+56]
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,[rsp+16]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lh_open_bad
 inc rbp
 jmp .Lh_open_message
.Lh_open_next:
 inc qword ptr [rsp+32]
 jmp .Lh_open_entry
.Lh_open_order:
 mov rdi,[rsp+16]
 lea rsi,[rsp+24]
 call pg_durable_context_order@PLT
 test eax,eax
 jnz .Lh_open_bad
 mov rdi,[rbx+8]
 lea rsi,[rip+.Lh_entries]
 mov rdx,[rsp+8]
 call .Lh_copy
 test eax,eax
 jnz .Lh_open_bad
 mov rdi,[rbx+8]
 lea rsi,[rip+.Lh_messages]
 mov rdx,[rsp+24]
 call .Lh_copy
 test eax,eax
 jnz .Lh_open_bad
 mov rax,[rsp+40]
 mov [rax],rbx
 mov qword ptr [rsp],0
 xor ebp,ebp
 jmp .Lh_open_end
.Lh_open_bad:
 mov ebp,5
 mov rdi,[rsp]
 call pg_compaction_hook_close
.Lh_open_end:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov eax,ebp
 HE 64
.globl pg_compaction_hook_poll
.type pg_compaction_hook_poll,@function
pg_compaction_hook_poll:
 HF 32
 mov rbx,rdi
 mov r12,[rdi]
 cmp dword ptr [rbx+44],0
 jne .Lh_poll_next
 mov rax,[rbx+56]
 test rax,rax
 jz .Lh_poll_next
 mov qword ptr [rsp],0
 mov rdi,[rax]
 mov rsi,[rbx+64]
 mov edx,7
 mov rcx,[rbx+8]
 lea r8,[rbx+48]
 mov r9,rsp
 call [rax+8]
 test eax,eax
 jnz .Lh_poll_scoped_return
 mov dword ptr [rbx+44],1
 HG "qword ptr [rsp]",decision
 mov rdi,rax
 call json_object_get@PLT
 mov [rbx+24],rax
 mov rdi,[rsp]
 call json_object_put@PLT
.Lh_poll_next:
 test r12,r12
 jz .Lh_poll_done
 cmp qword ptr [rbx+24],0
 jne .Lh_poll_done
 mov rax,[rbx+32]
 cmp rax,[r12+16]
 jae .Lh_poll_done
 imul rax,24
 add rax,[r12+8]
 mov r13,rax
 cmp qword ptr [rax+8],0
 je .Lh_poll_skip
 mov qword ptr [rsp],0
 mov rdi,[rax]
 mov rsi,[rbx+8]
 mov rdx,rsp
 call [rax+8]
 cmp eax,1
 je .Lh_poll_pending
 mov dword ptr [rbx+40],0
 test eax,eax
 jnz .Lh_poll_error
 cmp qword ptr [rsp],0
 je .Lh_poll_skip
 mov rdi,[rsp]
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lh_poll_invalid
 HG "qword ptr [rsp]",decline
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lh_poll_accept
 HG "qword ptr [rsp]",summary
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lh_poll_invalid
.Lh_poll_accept:
 mov rax,[rsp]
 mov [rbx+24],rax
.Lh_poll_done:
 xor eax,eax
 HE 32
.Lh_poll_scoped_return:
 mov [rsp+8],eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,[rsp+8]
 HE 32
.Lh_poll_pending:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov dword ptr [rbx+40],1
 mov eax,1
 HE 32
.Lh_poll_invalid:
 mov eax,-4
.Lh_poll_error:
 mov [rsp+8],eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[r12+24]
 mov rsi,[rbx+32]
 mov edx,[rsp+8]
 call [r12+32]
.Lh_poll_skip:
 inc qword ptr [rbx+32]
 jmp .Lh_poll_next
.globl pg_compaction_hook_close
.type pg_compaction_hook_close,@function
pg_compaction_hook_close:
 test rdi,rdi
 jz .Lh_close_return
 push rbx
 mov rbx,rdi
 cmp dword ptr [rdi+40],0
 je .Lh_close_values
 mov rax,[rbx]
 mov rcx,[rbx+32]
 imul rcx,24
 add rcx,[rax+8]
 cmp qword ptr [rcx+16],0
 je .Lh_close_values
 mov rdi,[rcx]
 mov rsi,[rbx+8]
 call [rcx+16]
.Lh_close_values:
 mov rax,[rbx+56]
 test rax,rax
 jz .Lh_close_request
 cmp qword ptr [rbx+48],0
 je .Lh_close_request
 mov rdi,[rax]
 mov rsi,[rbx+48]
 call [rax+16]
.Lh_close_request:
 mov rdi,[rbx+8]
 call json_object_put@PLT
 mov rdi,[rbx+16]
 call json_object_put@PLT
 mov rdi,[rbx+24]
 call json_object_put@PLT
 mov rdi,rbx
 call free@PLT
 pop rbx
.Lh_close_return:
 ret
.globl pg_compaction_hook_bind_scope
.type pg_compaction_hook_bind_scope,@function
pg_compaction_hook_bind_scope:
 mov [rdi+56],rsi
 mov [rdi+64],rdx
 ret
.section .rodata
.Lh_decision: .asciz "decision"
.Lh_firstKept: .asciz "firstKept"
.Lh_reason: .asciz "reason"
.Lh_instructions: .asciz "instructions"
.Lh_entries: .asciz "entries"
.Lh_contributions: .asciz "contributions"
.Lh_messages: .asciz "messages"
.Lh_id: .asciz "id"
.Lh_decline: .asciz "decline"
.Lh_summary: .asciz "summary"
.section .note.GNU-stack,"",@progbits
