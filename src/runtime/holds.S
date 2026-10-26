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
