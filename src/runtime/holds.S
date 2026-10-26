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
 cmp qword ptr [rdi+104],2
 jne .Lrelease_touch_normal
 call pg_document_cache_touch_flushing@PLT
 jmp .Lrelease_touch_done
.Lrelease_touch_normal:
 call pg_document_cache_touch@PLT
.Lrelease_touch_done:
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
 xor r8d,r8d
 xor edx,edx
 jmp .Lflush_common
.globl pg_document_holds_flush_one
.type pg_document_holds_flush_one,@function
pg_document_holds_flush_one:
 xor r8d,r8d
 test rdx,rdx
 jz .Linvalid
 mov rax,9007199254740991
 cmp rdx,rax
 ja .Linvalid
.Lflush_common:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,184
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsp
 xor eax,eax
 mov ecx,23
 rep stosq
 mov [rsp+152],rdx
 mov [rsp+160],r8
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
 cmp qword ptr [rsp+152],0
 je .Lflush_selected_ready
 mov rax,[rbx+8]
 mov rcx,[rbx+16]
.Lflush_select_find:
 test rcx,rcx
 jz .Lflush_empty
 mov rdi,[rax]
 mov rdx,[rsp+152]
 cmp [rdi+72],rdx
 je .Lflush_selected_ready
 add rax,56
 dec rcx
 jmp .Lflush_select_find
.Lflush_selected_ready:
 cmp dword ptr [r12+56],0
 jne .Lflush_readonly
 cmp qword ptr [rbx+16],0x7fffffff
 ja .Lflush_return
 mov rax,[rbx]
 test rax,rax
 jz .Lflush_return
 cmp qword ptr [rax+104],0
 je .Lflush_cache_ready
 cmp qword ptr [rsp+152],0
 je .Lflush_return
 cmp qword ptr [rax+104],2
 jne .Lflush_return
.Lflush_cache_ready:
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
 cmp qword ptr [rsp+160],0
 je .Lflush_notifications_opened
 mov qword ptr [rsp+176],1
 mov rax,[rsp+160]
 mov rdi,[rax]
 mov rsi,rbx
 mov rdx,r12
 lea rcx,[rsp+168]
 call [rax+8]
 test eax,eax
 jnz .Lflush_plan_error
.Lflush_notifications_opened:
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
 mov rax,[rsp+152]
 test rax,rax
 jz .Lflush_selected_plan
 cmp [r15+72],rax
 jne .Lflush_plan_next
.Lflush_selected_plan:
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
 cmp qword ptr [rsp+160],0
 je .Lflush_notice_appended
 mov rax,[rsp+160]
 mov rdi,[rax]
 mov rsi,[rsp+168]
 mov rdx,r15
 mov rcx,[rsp+120]
 call [rax+16]
 test eax,eax
 jnz .Lflush_plan_error
.Lflush_notice_appended:
 mov rdi,[rsp+120]
 call json_object_put@PLT
 mov qword ptr [rsp+120],0
.Lflush_plan_next:
 inc r13
 jmp .Lflush_plan_loop
.Lflush_plan_error:
 cmp eax,3
 je .Lflush_cleanup
.Lflush_invalid_plan:
 mov ebp,1
 jmp .Lflush_cleanup
.Lflush_submit:
 cmp qword ptr [rsp+160],0
 je .Lflush_notice_prepared
 mov rax,[rsp+160]
 mov rdi,[rax]
 mov rsi,[rsp+168]
 mov rdx,[rsp+48]
 call [rax+24]
 test eax,eax
 jnz .Lflush_plan_error
.Lflush_notice_prepared:
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
 cmp qword ptr [rsp+176],0
 je .Lflush_notifications_discarded
 mov rax,[rsp+160]
 mov rdi,[rax]
 mov rsi,[rsp+168]
 call [rax+48]
.Lflush_notifications_discarded:
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
 add rsp,184
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_holds_flush,.-pg_document_holds_flush
.size pg_document_holds_flush_one,.-pg_document_holds_flush_one

/* Storage adoption updates only metadata: held values are already visible. */
.Lflush_adopt:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov [rsp],rsi
 mov [rsp+8],rdx
 mov r12,[rbx]
 cmp qword ptr [rbx+160],0
 je .Lflush_notice_adopted
 mov rax,[rbx+160]
 mov rdi,[rax]
 mov rsi,[rbx+168]
 mov rdx,[rsp+8]
 call [rax+32]
 test eax,eax
 jnz .Lflush_adopt_end
.Lflush_notice_adopted:
 xor ecx,ecx
 mov r8,[r12+8]
 mov r9,[rbx+40]
.Lflush_adopt_loop:
 cmp rcx,[r12+16]
 jae .Lflush_adopt_chain
 mov rax,[r8]
 mov rdx,[rbx+152]
 test rdx,rdx
 jz .Lflush_adopt_selected
 cmp [rax+72],rdx
 jne .Lflush_adopt_next
.Lflush_adopt_selected:
 mov rdx,[r9+rcx*8]
 mov [rax+104],rdx
.Lflush_adopt_next:
 add r8,56
 inc rcx
 jmp .Lflush_adopt_loop
.Lflush_adopt_chain:
 xor eax,eax
 cmp qword ptr [rbx+24],0
 je .Lflush_adopt_notify
 mov rdi,[rbx+16]
 mov rsi,[rsp]
 mov rdx,[rsp+8]
 call qword ptr [rbx+24]
 test eax,eax
 jnz .Lflush_adopt_end
.Lflush_adopt_notify:
 cmp qword ptr [rbx+160],0
 je .Lflush_adopt_ok
 mov rax,[rbx+160]
 mov rdi,[rax]
 mov rsi,[rbx+168]
 mov rdx,[rsp]
 mov rcx,[rsp+8]
 call [rax+40]
.Lflush_adopt_ok:
 xor eax,eax
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
 cmp qword ptr [rbx+152],0
 jne .Lflush_publish_one
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
 jmp .Lflush_publish_chain
.Lflush_publish_one:
 mov r13,[r12+8]
 mov r14,[r12+16]
 mov r15,[rbx+152]
.Lflush_release_find:
 test r14,r14
 jz .Lflush_release_bad
 mov rsi,[r13]
 cmp [rsi+72],r15
 je .Lflush_release_selected
 add r13,56
 dec r14
 jmp .Lflush_release_find
.Lflush_release_selected:
 mov dword ptr [r12+48],0
 mov rdi,r12
 call pg_document_holds_release@PLT
 mov dword ptr [r12+48],1
 test eax,eax
 jz .Lflush_publish_chain
.Lflush_release_bad:
 mov dword ptr [rbx+56],1
 mov rax,[rbx+8]
 mov dword ptr [rax+52],4
.Lflush_publish_chain:
 cmp dword ptr [rbx+56],0
 jne .Lflush_publish_end
.Lflush_notice_published:
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
.text
.globl pg_document_holds_merge_bind
.type pg_document_holds_merge_bind,@function
pg_document_holds_merge_bind:
 test rdi,rdi
 jz .Linvalid
 test rsi,rsi
 jz .Linvalid
 cmp dword ptr [rdi+52],1
 jne .Linvalid
 cmp dword ptr [rdi+48],0
 jne .Linvalid
 mov [rsi],rdi
 lea rax,[rip+.Lmerge_prepare]
 mov [rsi+8],rax
 lea rax,[rip+.Lmerge_adopt]
 mov [rsi+16],rax
 lea rax,[rip+.Lmerge_discard]
 mov [rsi+24],rax
 xor eax,eax
 ret
.size pg_document_holds_merge_bind,.-pg_document_holds_merge_bind

/* Owned StorageWrite from a borrowed content object. */
.Lmerge_write:
 push rbx
 push r12
 push r13
 mov r12,rdi
 mov r13,rsi
 call json_object_new_object@PLT
 mov rbx,rax
 test rax,rax
 jz .Lmerge_write_end
 lea rdi,[rip+.Lflush_change]
 call json_object_new_string@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lflush_type]
 mov rdx,rax
 call .Lflush_field
 test eax,eax
 jnz .Lmerge_write_bad
 mov rdi,r12
 call json_object_new_int64@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lflush_id]
 mov rdx,rax
 call .Lflush_field
 test eax,eax
 jnz .Lmerge_write_bad
 mov rdi,r13
 call json_object_get@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lflush_content]
 mov rdx,rax
 call .Lflush_field
 test eax,eax
 jnz .Lmerge_write_bad
 mov rax,rbx
 jmp .Lmerge_write_end
.Lmerge_write_bad:
 mov rdi,rbx
 call json_object_put@PLT
 xor eax,eax
.Lmerge_write_end:
 pop r13
 pop r12
 pop rbx
 ret

.Lmerge_array_ref:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov rdi,rsi
 call json_object_get@PLT
 mov r12,rax
 mov rdi,rbx
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz .Lmerge_array_end
 mov rdi,r12
 call json_object_put@PLT
 mov eax,-1
.Lmerge_array_end:
 add rsp,8
 pop r12
 pop rbx
 ret

/* Replace the old write by identity, or insert before this ID's retirement.
   A wholly private rebuilt array is swapped into Tx only on success. */
.Lmerge_splice:
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
 mov r14,rcx
 mov qword ptr [rsp+8],-1
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov [rsp],rax
 cmp rax,0x7ffffffe
 ja .Lsplice_no_array
 lea edi,[rax+1]
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lsplice_no_array
 mov r15,rax
 xor ebp,ebp
.Lsplice_loop:
 cmp rbp,[rsp]
 jae .Lsplice_tail
 mov rdi,[rbx+24]
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov [rsp+16],rax
 test r12,r12
 jz .Lsplice_retire
 cmp rax,r12
 jne .Lsplice_original
 mov rdi,r15
 call json_object_array_length@PLT
 mov [rsp+8],rax
 mov rdi,r15
 mov rsi,r13
 call .Lmerge_array_ref
 test eax,eax
 jnz .Lsplice_bad
 jmp .Lsplice_next
.Lsplice_retire:
 cmp qword ptr [rsp+8],-1
 jne .Lsplice_original
 mov rdi,rax
 lea rsi,[rip+.Lflush_type]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lsplice_original
 mov rdi,rax
 lea rsi,[rip+.Lmerge_retire]
 call strcmp@PLT
 test eax,eax
 jnz .Lsplice_original
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lflush_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,r14
 jne .Lsplice_original
 mov rdi,r15
 call json_object_array_length@PLT
 mov [rsp+8],rax
 mov rdi,r15
 mov rsi,r13
 call .Lmerge_array_ref
 test eax,eax
 jnz .Lsplice_bad
.Lsplice_original:
 mov rdi,r15
 mov rsi,[rsp+16]
 call .Lmerge_array_ref
 test eax,eax
 jnz .Lsplice_bad
.Lsplice_next:
 inc rbp
 jmp .Lsplice_loop
.Lsplice_tail:
 cmp qword ptr [rsp+8],-1
 jne .Lsplice_install
 test r12,r12
 jnz .Lsplice_bad
 mov rdi,r15
 call json_object_array_length@PLT
 mov [rsp+8],rax
 mov rdi,r15
 mov rsi,r13
 call .Lmerge_array_ref
 test eax,eax
 jnz .Lsplice_bad
.Lsplice_install:
 mov rdi,[rbx+24]
 mov rsi,r15
 call pg_record_tx_array_transfer@PLT
 mov rdi,[rbx+24]
 mov [rbx+24],r15
 call json_object_put@PLT
 mov rdx,[rsp+8]
 xor eax,eax
 jmp .Lsplice_end
.Lsplice_bad:
 mov rdi,r15
 call json_object_put@PLT
.Lsplice_no_array:
 mov eax,3
.Lsplice_end:
 add rsp,40
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Token owns only the selected incarnation pointers; queues remain pinned
   and unchanged through fallible planning and Storage admission. */
.Lmerge_prepare:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,104
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov [rsp+96],r8
 mov rdi,rsp
 xor eax,eax
 mov ecx,12
 rep stosq
 mov ebp,1
 test r8,r8
 jz .Lmerge_prepare_end
 mov qword ptr [r8],0
 test rbx,rbx
 jz .Lmerge_prepare_end
 cmp dword ptr [rbx+52],1
 jne .Lmerge_prepare_end
 cmp dword ptr [rbx+48],0
 jne .Lmerge_prepare_end
 cmp qword ptr [rbx+16],0
 je .Lmerge_prepare_noop
 mov rdi,[r12+24]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lmerge_prepare_noop
 mov dword ptr [rbx+48],1
 mov ebp,3
 mov edi,1
 mov esi,32
 call calloc@PLT
 test rax,rax
 jz .Lmerge_prepare_unlock
 mov [rsp],rax
 mov [rax],rbx
 mov rcx,[rsp+96]
 mov [rcx],rax
 mov rdi,r14
 test rdi,rdi
 jnz .Lmerge_match_alloc
 inc rdi
.Lmerge_match_alloc:
 mov esi,8
 call calloc@PLT
 test rax,rax
 jz .Lmerge_prepare_clean
 mov rcx,[rsp]
 mov [rcx+8],rax
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lmerge_prepare_clean
 mov [rsp+56],rax
 xor r15d,r15d
.Lmerge_entry_loop:
 cmp r15,r14
 jae .Lmerge_prepare_ok
 imul rax,r15,152
 add rax,r13
 mov [rsp+80],rax
 test dword ptr [rax+148],1
 jnz .Lmerge_entry_next
 mov rdx,[rax]
 mov rdx,[rdx+72]
 mov rcx,[rbx+16]
 mov rdi,[rbx+8]
.Lmerge_find_hold:
 test rcx,rcx
 jz .Lmerge_entry_next
 mov rsi,[rdi]
 cmp [rsi+72],rdx
 je .Lmerge_hold_found
 add rdi,56
 dec rcx
 jmp .Lmerge_find_hold
.Lmerge_hold_found:
 mov [rsp+88],rdi
 test dword ptr [rax+148],4
 jnz .Lmerge_pointer_ready
 cmp [rax],rsi
 jne .Lmerge_prepare_invalid
.Lmerge_pointer_ready:
 lea rsi,[rdi+8]
 lea rdi,[rsp+8]
 mov edx,32
 call memcpy@PLT
 mov rdi,[rsp+8]
 call json_object_get@PLT
 mov rdi,[rsp+16]
 call json_object_get@PLT
 mov rax,[rsp+80]
 mov rsi,[rsp+56]
 mov rcx,[rsp+88]
 mov rcx,[rcx]
 mov rdx,[rcx+8]
 cmp qword ptr [rax+8],0
 je .Lmerge_store_inputs
 mov rdx,[rax+16]
 mov rsi,[rax+24]
.Lmerge_store_inputs:
 mov rax,[rsp+88]
 mov r8d,[rax+48]
 mov rcx,[rcx+96]
 lea rdi,[rsp+8]
 lea r9,[rsp+40]
 call pg_document_store_held@PLT
 test eax,eax
 jnz .Lmerge_prepare_error
 lea rdi,[rsp+8]
 call pg_pending_ops_close@PLT
 mov rdx,[rsp+40]
 mov rax,[rsp+80]
 cmp dword ptr [rax+144],1
 jne .Lmerge_choose_content
 mov rdi,[rax+112]
 lea rsi,[rip+.Lflush_content]
 call json_object_object_get@PLT
 mov rdx,rax
.Lmerge_choose_content:
 mov [rsp+72],rdx
 mov rdi,rdx
 lea rsi,[rip+.Lflush_kind]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov ecx,1
 cmp byte ptr [rax],'b'
 je .Lmerge_kind_ready
 inc ecx
 mov rax,[rsp+88]
 mov rax,[rax]
 mov rax,[rax+104]
 mov rdx,9007199254740991
 cmp rax,rdx
 jae .Lmerge_prepare_invalid
.Lmerge_kind_ready:
 mov rax,[rsp+80]
 mov [rax+144],ecx
 mov rax,[rsp+88]
 mov rax,[rax]
 mov rdi,[rax+72]
 mov rsi,[rsp+72]
 call .Lmerge_write
 test rax,rax
 jz .Lmerge_prepare_clean
 mov [rsp+64],rax
 mov rcx,[rsp+88]
 mov rcx,[rcx]
 mov rcx,[rcx+72]
 mov rdx,rax
 mov rax,[rsp+80]
 mov rsi,[rax+112]
 mov rdi,r12
 call .Lmerge_splice
 test eax,eax
 jnz .Lmerge_prepare_error
 mov rax,[rsp+80]
 mov [rax+120],rdx
 mov rdi,[rax+112]
 call json_object_put@PLT
 mov rax,[rsp+80]
 mov rdx,[rsp+64]
 mov [rax+112],rdx
 mov qword ptr [rsp+64],0
 or dword ptr [rax+148],16
 mov rax,[rsp]
 mov rcx,[rax+16]
 mov rdx,[rax+8]
 mov rsi,[rsp+88]
 mov rsi,[rsi]
 mov [rdx+rcx*8],rsi
 inc qword ptr [rax+16]
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov qword ptr [rsp+40],0
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov qword ptr [rsp+48],0
.Lmerge_entry_next:
 inc r15
 jmp .Lmerge_entry_loop
.Lmerge_prepare_ok:
 xor ebp,ebp
 jmp .Lmerge_prepare_clean
.Lmerge_prepare_error:
 mov ebp,eax
 jmp .Lmerge_prepare_clean
.Lmerge_prepare_invalid:
 mov ebp,4
.Lmerge_prepare_clean:
 lea rdi,[rsp+8]
 call pg_pending_ops_close@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov rdi,[rsp+64]
 call json_object_put@PLT
 jmp .Lmerge_prepare_end
.Lmerge_prepare_unlock:
 mov dword ptr [rbx+48],0
 jmp .Lmerge_prepare_end
.Lmerge_prepare_noop:
 xor ebp,ebp
.Lmerge_prepare_end:
 mov eax,ebp
 add rsp,104
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.Lmerge_adopt:
 test rsi,rsi
 jz .Lmerge_adopt_noop
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 cmp qword ptr [r12+24],0
 jne .Lmerge_adopt_bad
 xor r13d,r13d
.Lmerge_release_loop:
 cmp r13,[r12+16]
 jae .Lmerge_released
 mov rax,[r12+8]
 mov rsi,[rax+r13*8]
 mov dword ptr [rbx+48],0
 mov rdi,rbx
 call pg_document_holds_release@PLT
 mov dword ptr [rbx+48],1
 test eax,eax
 jnz .Lmerge_adopt_end
 inc r13
 jmp .Lmerge_release_loop
.Lmerge_released:
 mov qword ptr [r12+24],1
 xor eax,eax
 jmp .Lmerge_adopt_end
.Lmerge_adopt_bad:
 mov eax,1
.Lmerge_adopt_end:
 pop r13
 pop r12
 pop rbx
 ret
.Lmerge_adopt_noop:
 xor eax,eax
 ret
.Lmerge_discard:
 test rsi,rsi
 jz .Lmerge_discard_end
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,[r12+8]
 call free@PLT
 mov rdi,r12
 call free@PLT
 mov dword ptr [rbx+48],0
 add rsp,8
 pop r12
 pop rbx
.Lmerge_discard_end:
 ret
.section .rodata
.Lmerge_retire: .asciz "document.retire"
.section .note.GNU-stack,"",@progbits

.text
.globl pg_document_holds_finish
.type pg_document_holds_finish,@function
pg_document_holds_finish:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13d,1
 test rbx,rbx
 jz .Lfinish_return
 test r12,r12
 jz .Lfinish_return
 cmp dword ptr [rbx+48],0
 jne .Lfinish_return
 cmp dword ptr [r12+48],0
 jne .Lfinish_return
 cmp dword ptr [rbx+52],0
 je .Lfinish_closed
 cmp dword ptr [rbx+52],1
 jne .Lfinish_return
 mov rax,[rbx]
 test rax,rax
 jz .Lfinish_return
 cmp qword ptr [rax+104],0
 jne .Lfinish_return
 mov r13d,[r12+52]
 test r13d,r13d
 jnz .Lfinish_cleanup
 call pg_document_holds_flush@PLT
 mov r13d,eax
 test eax,eax
 jz .Lfinish_cleanup
 cmp dword ptr [r12+52],0
 jne .Lfinish_cleanup
 mov [r12+52],eax
.Lfinish_cleanup:
 mov rdi,rbx
 call pg_document_holds_discard_all@PLT
 test eax,eax
 jnz .Lfinish_internal
 mov rdi,rbx
 call pg_document_holds_close@PLT
 test eax,eax
 jz .Lfinish_closed
.Lfinish_internal:
 cmp dword ptr [r12+52],0
 jne .Lfinish_closed
 mov dword ptr [r12+52],4
.Lfinish_closed:
 mov r13d,[r12+52]
.Lfinish_return:
 mov eax,r13d
 pop r13
 pop r12
 pop rbx
 ret
.size pg_document_holds_finish,.-pg_document_holds_finish
.section .note.GNU-stack,"",@progbits

.text
.globl pg_document_holds_flush_observed
.type pg_document_holds_flush_observed,@function
pg_document_holds_flush_observed:
 mov eax,1
 test rcx,rcx
 jz .Lobserved_flush_invalid
 cmp qword ptr [rcx+8],0
 je .Lobserved_flush_invalid
 cmp qword ptr [rcx+16],0
 je .Lobserved_flush_invalid
 cmp qword ptr [rcx+24],0
 je .Lobserved_flush_invalid
 cmp qword ptr [rcx+32],0
 je .Lobserved_flush_invalid
 cmp qword ptr [rcx+40],0
 je .Lobserved_flush_invalid
 cmp qword ptr [rcx+48],0
 je .Lobserved_flush_invalid
 mov rax,9007199254740991
 cmp rdx,rax
 ja .Lobserved_flush_invalid
 mov r8,rcx
 jmp .Lflush_common
.Lobserved_flush_invalid:
 mov eax,1
 ret
.size pg_document_holds_flush_observed,.-pg_document_holds_flush_observed
.section .note.GNU-stack,"",@progbits
