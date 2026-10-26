/* Pi d5b3de22 Session.#hold/#release; MIT: docs/licenses/pi.md. */
.intel_syntax noprefix
.text
.globl pg_document_holds_open
.type pg_document_holds_open,@function
pg_document_holds_open:
 test rdi,rdi
 jz .Linvalid
 test rsi,rsi
 jz .Linvalid
 test rcx,rcx
 jz .Linvalid
 mov [rdi],rsi
 mov qword ptr [rdi+8],0
 mov qword ptr [rdi+16],0
 mov [rdi+24],rdx
 mov [rdi+32],rcx
 mov [rdi+40],r8
 mov dword ptr [rdi+48],0
 mov dword ptr [rdi+52],1
 xor eax,eax
 ret
.size pg_document_holds_open,.-pg_document_holds_open
.Linvalid:
 mov eax,1
 ret

.globl pg_document_holds_bind
.type pg_document_holds_bind,@function
pg_document_holds_bind:
 test rdi,rdi
 jz .Linvalid
 test rsi,rsi
 jz .Linvalid
 cmp dword ptr [rdi+52],1
 jne .Linvalid
 cmp dword ptr [rdi+48],0
 jne .Linvalid
 mov [rsi],rdi
 lea rax,[rip+.Lprepare]
 mov [rsi+8],rax
 lea rax,[rip+.Lapply]
 mov [rsi+16],rax
 lea rax,[rip+.Ldiscard]
 mov [rsi+24],rax
 lea rax,[rip+.Lobserve]
 mov [rsi+32],rax
 xor eax,eax
 ret
.size pg_document_holds_bind,.-pg_document_holds_bind

.globl pg_document_holds_pinned
.type pg_document_holds_pinned,@function
pg_document_holds_pinned:
 xor eax,eax
 test rdi,rdi
 jz .Lpin_end
 cmp dword ptr [rdi+52],1
 jne .Lpin_end
 mov rcx,[rdi+16]
 mov rdx,[rdi+8]
.Lpin_loop:
 test rcx,rcx
 jz .Lpin_end
 mov r8,[rdx]
 cmp [r8+72],rsi
 je .Lpin_yes
 add rdx,56
 dec rcx
 jmp .Lpin_loop
.Lpin_yes:
 mov eax,1
.Lpin_end:
 ret
.size pg_document_holds_pinned,.-pg_document_holds_pinned

.globl pg_document_holds_deadline
.type pg_document_holds_deadline,@function
pg_document_holds_deadline:
 mov rax,-1
 test rdi,rdi
 jz .Ldeadline_end
 cmp dword ptr [rdi+52],1
 jne .Ldeadline_end
 mov rcx,[rdi+16]
 mov rdx,[rdi+8]
.Ldeadline_loop:
 test rcx,rcx
 jz .Ldeadline_end
 cmp [rdx+40],rax
 cmovb rax,[rdx+40]
 add rdx,56
 dec rcx
 jmp .Ldeadline_loop
.Ldeadline_end:
 ret
.size pg_document_holds_deadline,.-pg_document_holds_deadline

/* Drop an owned array of queue snapshots. */
.Lfree_items:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdi
.Lfree_loop:
 test r12,r12
 jz .Lfree_done
 lea rdi,[r13+8]
 call pg_pending_ops_close@PLT
 add r13,56
 dec r12
 jmp .Lfree_loop
.Lfree_done:
 mov rdi,rbx
 call free@PLT
 pop r13
 pop r12
 pop rbx
 ret

/* token = {owner,items,count,applied}; slots = {loaded,pending,deadline,base}. */
.Lprepare:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,40
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov [rsp+24],rcx
 mov r15,r8
 test r15,r15
 jz .Lprepare_invalid
 mov qword ptr [r15],0
 test rbx,rbx
 jz .Lprepare_invalid
 cmp dword ptr [rbx+52],1
 jne .Lprepare_invalid
 cmp dword ptr [rbx+48],0
 jne .Lprepare_invalid
 test r13,r13
 jz .Lprepare_args_ok
 test r12,r12
 jz .Lprepare_invalid
.Lprepare_args_ok:
 mov rax,[rbx+16]
 add rax,r13
 jc .Lprepare_capacity
 mov rcx,0x492492492492492
 cmp rax,rcx
 ja .Lprepare_capacity
 mov [rsp],rax
 mov dword ptr [rbx+48],1
 mov edi,1
 mov esi,32
 call calloc@PLT
 test rax,rax
 jz .Lprepare_nomem_unlock
 mov rbp,rax
 mov [rax],rbx
 mov [r15],rax
 mov rdi,[rsp]
 test rdi,rdi
 jnz .Lprepare_alloc
 inc rdi
.Lprepare_alloc:
 mov esi,56
 call calloc@PLT
 test rax,rax
 jz .Lprepare_nomem
 mov [rbp+8],rax
 mov r14,[rbx+8]
 mov r15,rax
 mov qword ptr [rsp+8],0
.Lclone_loop:
 mov rax,[rsp+8]
 cmp rax,[rbx+16]
 jae .Lclone_done
 mov rdi,r15
 mov rsi,r14
 mov edx,56
 call memcpy@PLT
 mov rdi,[r15+8]
 call json_object_get@PLT
 mov rdi,[r15+16]
 call json_object_get@PLT
 inc qword ptr [rbp+16]
 inc qword ptr [rsp+8]
 add r14,56
 add r15,56
 jmp .Lclone_loop
.Lclone_done:
 /* within was saved before snapshot cloning. */
 mov rdi,[rbx+24]
 call qword ptr [rbx+32]
 mov rcx,[rsp+24]
 add rax,rcx
 jnc .Ldeadline_ready
 mov rax,-1
.Ldeadline_ready:
 mov [rsp+16],rax
 xor r14d,r14d
.Lentry_loop:
 cmp r14,r13
 jae .Lprepare_ok
 cmp qword ptr [r12+112],0
 je .Lentry_next
 mov r15,[rbp+8]
 xor eax,eax
.Lfind_slot:
 cmp rax,[rbp+16]
 jae .Lnew_slot
 mov rcx,[r15]
 cmp rcx,[r12]
 je .Lpush_slot
 mov rcx,[rcx+72]
 mov rdx,[r12]
 cmp rcx,[rdx+72]
 je .Lprepare_semantics
 add r15,56
 inc rax
 jmp .Lfind_slot
.Lnew_slot:
 mov rax,[r12]
 mov [r15],rax
 mov qword ptr [r15+40],-1
 lea rdi,[r15+8]
 call pg_pending_ops_open@PLT
 test eax,eax
 jnz .Lprepare_return
 inc qword ptr [rbp+16]
.Lpush_slot:
 lea rdi,[r15+8]
 mov rsi,[r12+24]
 call pg_pending_ops_push@PLT
 test eax,eax
 jnz .Lprepare_return
 cmp dword ptr [r12+144],1
 jne .Lslot_deadline
 mov dword ptr [r15+48],1
.Lslot_deadline:
 mov rax,[rsp+16]
 cmp rax,[r15+40]
 jae .Lentry_next
 mov [r15+40],rax
.Lentry_next:
 add r12,152
 inc r14
 jmp .Lentry_loop
.Lprepare_ok:
 xor eax,eax
 jmp .Lprepare_return
.Lprepare_nomem_unlock:
 mov dword ptr [rbx+48],0
.Lprepare_nomem:
 mov eax,3
 jmp .Lprepare_return
.Lprepare_semantics:
 mov eax,4
 jmp .Lprepare_return
.Lprepare_capacity:
 mov eax,2
 jmp .Lprepare_return
.Lprepare_invalid:
 mov eax,1
.Lprepare_return:
 add rsp,40
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.Lapply:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 test r12,r12
 jz .Lapply_bad
 cmp [r12],rbx
 jne .Lapply_bad
 cmp dword ptr [rbx+48],1
 jne .Lapply_bad
 cmp qword ptr [r12+24],0
 jne .Lapply_bad
 mov rdi,[rbx+8]
 mov rsi,[rbx+16]
 call .Lfree_items
 mov rax,[r12+8]
 mov [rbx+8],rax
 mov rax,[r12+16]
 mov [rbx+16],rax
 mov qword ptr [r12+8],0
 mov qword ptr [r12+16],0
 mov qword ptr [r12+24],1
 xor r12d,r12d
.Ltouch_loop:
 cmp r12,[rbx+16]
 jae .Lapply_ok
 imul rax,r12,56
 add rax,[rbx+8]
 mov rax,[rax]
 lea rsi,[rax+24]
 mov rdi,[rbx]
 call pg_document_cache_touch@PLT
 test eax,eax
 jnz .Lapply_end
 inc r12
 jmp .Ltouch_loop
.Lapply_ok:
 xor eax,eax
 jmp .Lapply_end
.Lapply_bad:
 mov eax,1
.Lapply_end:
 add rsp,8
 pop r12
 pop rbx
 ret

.Ldiscard:
 test rsi,rsi
 jz .Ldiscard_end
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 cmp [r12],rbx
 jne .Ldiscard_pop
 mov rdi,[r12+8]
 mov rsi,[r12+16]
 call .Lfree_items
 mov rdi,r12
 call free@PLT
 mov dword ptr [rbx+48],0
.Ldiscard_pop:
 add rsp,8
 pop r12
 pop rbx
.Ldiscard_end:
 ret
.Lobserve:
 mov rax,[rdi+40]
 test rax,rax
 jz .Lobserve_end
 mov rdi,[rdi+24]
 jmp rax
.Lobserve_end:
 ret

.globl pg_document_holds_release
.type pg_document_holds_release,@function
pg_document_holds_release:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 test rbx,rbx
 jz .Lrelease_invalid
 test r12,r12
 jz .Lrelease_invalid
 cmp dword ptr [rbx+52],1
 jne .Lrelease_invalid
 cmp dword ptr [rbx+48],0
 jne .Lrelease_invalid
 mov r13,[rbx+8]
 xor eax,eax
.Lrelease_find:
 cmp rax,[rbx+16]
 jae .Lrelease_missing
 cmp [r13],r12
 je .Lrelease_found
 inc rax
 add r13,56
 jmp .Lrelease_find
.Lrelease_found:
 mov dword ptr [rbx+48],1
 push rax
 sub rsp,8
 lea rdi,[r13+8]
 call pg_pending_ops_close@PLT
 add rsp,8
 pop rax
 dec qword ptr [rbx+16]
 mov rdx,[rbx+16]
 sub rdx,rax
 imul rdx,56
 mov rdi,r13
 lea rsi,[r13+56]
 call memmove@PLT
 mov rdi,[rbx]
 lea rsi,[r12+24]
 call pg_document_cache_touch@PLT
 mov dword ptr [rbx+48],0
 jmp .Lrelease_end
.Lrelease_missing:
 mov eax,8
 jmp .Lrelease_end
.Lrelease_invalid:
 mov eax,1
.Lrelease_end:
 pop r13
 pop r12
 pop rbx
 ret
.size pg_document_holds_release,.-pg_document_holds_release

.globl pg_document_holds_discard_all
.type pg_document_holds_discard_all,@function
pg_document_holds_discard_all:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 test rbx,rbx
 jz .Lall_invalid
 cmp dword ptr [rbx+52],1
 jne .Lall_invalid
 cmp dword ptr [rbx+48],0
 jne .Lall_invalid
 mov dword ptr [rbx+48],1
 mov r12,[rbx+8]
 mov r13,[rbx+16]
 mov qword ptr [rbx+16],0
 xor eax,eax
 push rax
.Lall_touch:
 cmp qword ptr [rsp],r13
 jae .Lall_free
 mov rax,[rsp]
 imul rax,56
 mov rax,[r12+rax]
 mov rdi,[rbx]
 lea rsi,[rax+24]
 sub rsp,8
 call pg_document_cache_touch@PLT
 add rsp,8
 inc qword ptr [rsp]
 jmp .Lall_touch
.Lall_free:
 pop rax
 mov rdi,r12
 mov rsi,r13
 call .Lfree_items
 mov qword ptr [rbx+8],0
 mov dword ptr [rbx+48],0
 xor eax,eax
 jmp .Lall_end
.Lall_invalid:
 mov eax,1
.Lall_end:
 pop r13
 pop r12
 pop rbx
 ret
.size pg_document_holds_discard_all,.-pg_document_holds_discard_all

.globl pg_document_holds_close
.type pg_document_holds_close,@function
pg_document_holds_close:
 test rdi,rdi
 jz .Linvalid
 cmp dword ptr [rdi+52],1
 jne .Linvalid
 cmp dword ptr [rdi+48],0
 jne .Linvalid
 cmp qword ptr [rdi+16],0
 jne .Lclose_nonempty
 push rbx
 mov rbx,rdi
 mov rdi,[rdi+8]
 call free@PLT
 mov qword ptr [rbx+8],0
 mov dword ptr [rbx+52],0
 xor eax,eax
 pop rbx
 ret
.Lclose_nonempty:
 mov eax,4
 ret
.size pg_document_holds_close,.-pg_document_holds_close
.section .note.GNU-stack,"",@progbits
.text
/* Add an owned field, releasing it when json-c refuses insertion. */
.Lflush_field:
 push rbx
 mov rbx,rdx
 test rdx,rdx
 jz .Lflush_field_bad
 call json_object_object_add@PLT
 test eax,eax
 jz .Lflush_field_done
 mov rdi,rbx
 call json_object_put@PLT
.Lflush_field_bad:
 mov eax,-1
.Lflush_field_done:
 pop rbx
 ret

.globl pg_document_holds_flush
.type pg_document_holds_flush,@function
pg_document_holds_flush:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,168
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsp
 xor eax,eax
 mov ecx,21
 rep stosq
 mov ebp,1
 test rbx,rbx
 jz .Lflush_return
 test r12,r12
 jz .Lflush_return
 cmp dword ptr [rbx+52],1
 jne .Lflush_return
 cmp dword ptr [rbx+48],0
 jne .Lflush_return
 cmp dword ptr [r12+48],0
 jne .Lflush_return
 mov eax,[r12+52]
 test eax,eax
 jnz .Lflush_existing_failure
 cmp qword ptr [r12+8],0
 je .Lflush_return
 mov rax,9007199254740991
 cmp [r12+16],rax
 ja .Lflush_return
 cmp qword ptr [rbx+16],0
 je .Lflush_empty
 cmp dword ptr [r12+56],0
 jne .Lflush_readonly
 cmp qword ptr [rbx+16],0x7fffffff
 ja .Lflush_return
 mov rax,[rbx]
 test rax,rax
 jz .Lflush_return
 cmp qword ptr [rax+104],0
 jne .Lflush_return
 mov [rsp],rbx
 mov [rsp+8],r12
 mov rax,[r12+24]
 mov [rsp+16],rax
 mov rax,[r12+32]
 mov [rsp+24],rax
 mov rax,[r12+40]
 mov [rsp+32],rax
 mov dword ptr [rbx+48],1
 mov dword ptr [r12+48],1
 mov ebp,5
 mov rdi,[rbx+16]
 mov esi,8
 call calloc@PLT
 test rax,rax
 jz .Lflush_cleanup
 mov [rsp+40],rax
 mov edi,dword ptr [rbx+16]
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lflush_cleanup
 mov [rsp+48],rax
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lflush_cleanup
 mov [rsp+72],rax
 xor r13d,r13d
.Lflush_plan_loop:
 cmp r13,[rbx+16]
 jae .Lflush_submit
 imul r14,r13,56
 add r14,[rbx+8]
 mov r15,[r14]
 lea rdi,[rsp+80]
 lea rsi,[r14+8]
 mov edx,32
 call memcpy@PLT
 mov rdi,[rsp+80]
 call json_object_get@PLT
 mov rdi,[rsp+88]
 call json_object_get@PLT
 lea rdi,[rsp+80]
 mov rsi,[rsp+72]
 mov rdx,[r15+8]
 mov rcx,[r15+96]
 mov r8d,[r14+48]
 lea r9,[rsp+112]
 call pg_document_store_held@PLT
 test eax,eax
 jnz .Lflush_plan_error
 lea rdi,[rsp+80]
 call pg_pending_ops_close@PLT
 mov rdi,[rsp+112]
 lea rsi,[rip+.Lflush_kind]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lflush_invalid_plan
 xor edx,edx
 cmp byte ptr [rax],'b'
 je .Lflush_counter
 mov rdx,[r15+104]
 mov rax,9007199254740991
 cmp rdx,rax
 jae .Lflush_invalid_plan
 inc rdx
.Lflush_counter:
 mov rax,[rsp+40]
 mov [rax+r13*8],rdx
 call json_object_new_object@PLT
 test rax,rax
 jz .Lflush_cleanup
 mov [rsp+128],rax
 lea rdi,[rip+.Lflush_change]
 call json_object_new_string@PLT
 mov rdi,[rsp+128]
 lea rsi,[rip+.Lflush_type]
 mov rdx,rax
 call .Lflush_field
 test eax,eax
 jnz .Lflush_cleanup
 mov rdi,[r15+72]
 call json_object_new_int64@PLT
 mov rdi,[rsp+128]
 lea rsi,[rip+.Lflush_id]
 mov rdx,rax
 call .Lflush_field
 test eax,eax
 jnz .Lflush_cleanup
 mov rdi,[rsp+128]
 lea rsi,[rip+.Lflush_content]
 mov rdx,[rsp+112]
 mov qword ptr [rsp+112],0
 call .Lflush_field
 test eax,eax
 jnz .Lflush_cleanup
 mov rdi,[rsp+48]
 mov rsi,[rsp+128]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lflush_cleanup
 mov qword ptr [rsp+128],0
 mov rdi,[rsp+120]
 call json_object_put@PLT
 mov qword ptr [rsp+120],0
 inc r13
 jmp .Lflush_plan_loop
.Lflush_plan_error:
 cmp eax,3
 je .Lflush_cleanup
.Lflush_invalid_plan:
 mov ebp,1
 jmp .Lflush_cleanup
.Lflush_submit:
 mov [r12+24],rsp
 lea rax,[rip+.Lflush_adopt]
 mov [r12+32],rax
 lea rax,[rip+.Lflush_publish]
 mov [r12+40],rax
 mov qword ptr [rsp+144],1
 mov dword ptr [r12+48],0
 mov rdi,r12
 mov rsi,[rsp+48]
 call pg_records_commit@PLT
 mov ebp,eax
 cmp dword ptr [rsp+56],0
 je .Lflush_cleanup
 mov ebp,4
.Lflush_cleanup:
 cmp qword ptr [rsp+144],0
 je .Lflush_hooks_restored
 mov rax,[rsp+16]
 mov [r12+24],rax
 mov rax,[rsp+24]
 mov [r12+32],rax
 mov rax,[rsp+32]
 mov [r12+40],rax
.Lflush_hooks_restored:
 mov dword ptr [r12+48],1
 lea rdi,[rsp+80]
 call pg_pending_ops_close@PLT
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov rdi,[rsp+120]
 call json_object_put@PLT
 mov rdi,[rsp+128]
 call json_object_put@PLT
 mov rdi,[rsp+72]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call free@PLT
 mov dword ptr [r12+48],0
 mov dword ptr [rbx+48],0
 jmp .Lflush_return
.Lflush_existing_failure:
 mov ebp,eax
 jmp .Lflush_return
.Lflush_readonly:
 mov ebp,2
 jmp .Lflush_return
.Lflush_empty:
 xor ebp,ebp
.Lflush_return:
 mov eax,ebp
 add rsp,168
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_holds_flush,.-pg_document_holds_flush

/* Storage adoption updates only metadata: held values are already visible. */
.Lflush_adopt:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov [rsp],rsi
 mov [rsp+8],rdx
 mov r12,[rbx]
 xor ecx,ecx
 mov r8,[r12+8]
 mov r9,[rbx+40]
.Lflush_adopt_loop:
 cmp rcx,[r12+16]
 jae .Lflush_adopt_chain
 mov rax,[r8]
 mov rdx,[r9+rcx*8]
 mov [rax+104],rdx
 add r8,56
 inc rcx
 jmp .Lflush_adopt_loop
.Lflush_adopt_chain:
 xor eax,eax
 cmp qword ptr [rbx+24],0
 je .Lflush_adopt_end
 mov rdi,[rbx+16]
 mov rsi,[rsp]
 mov rdx,[rsp+8]
 call qword ptr [rbx+24]
.Lflush_adopt_end:
 add rsp,24
 pop r12
 pop rbx
 ret

/* Successful settlement drops pins before notifying durable subscribers. */
.Lflush_publish:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,16
 mov rbx,rdi
 mov [rsp],rsi
 mov [rsp+8],rdx
 mov r12,[rbx]
 mov r13,[r12+8]
 mov r14,[r12+16]
 mov qword ptr [r12+8],0
 mov qword ptr [r12+16],0
 xor r15d,r15d
.Lflush_unpin_loop:
 cmp r15,r14
 jae .Lflush_unpin_done
 imul rax,r15,56
 mov rax,[r13+rax]
 lea rsi,[rax+24]
 mov rdi,[r12]
 call pg_document_cache_touch@PLT
 test eax,eax
 jz .Lflush_unpin_next
 mov dword ptr [rbx+56],1
 mov rax,[rbx+8]
 mov dword ptr [rax+52],4
.Lflush_unpin_next:
 inc r15
 jmp .Lflush_unpin_loop
.Lflush_unpin_done:
 mov rdi,r13
 mov rsi,r14
 call .Lfree_items
 cmp dword ptr [rbx+56],0
 jne .Lflush_publish_end
 cmp qword ptr [rbx+32],0
 je .Lflush_publish_end
 mov rdi,[rbx+16]
 mov rsi,[rsp]
 mov rdx,[rsp+8]
 call qword ptr [rbx+32]
.Lflush_publish_end:
 add rsp,16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.section .rodata
.Lflush_type: .asciz "type"
.Lflush_id: .asciz "id"
.Lflush_kind: .asciz "kind"
.Lflush_content: .asciz "content"
.Lflush_change: .asciz "document.change"
.section .note.GNU-stack,"",@progbits
