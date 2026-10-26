.intel_syntax noprefix
.text
/* Settlement context: entries/count, original context/adopt/publish, error. */
.Ldocs_finalize:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
 mov qword ptr [rsp],0
.Ldocs_prepare_each:
 cmp r13,[rbx+8]
 jae .Ldocs_validate
 imul r14,r13,152
 add r14,[rbx]
 mov rdi,[r14+8]
 lea rsi,[r14+16]
 call pg_document_change_prepare@PLT
 test eax,eax
 jnz .Ldocs_finalize_doc_error
 mov r15,[r14]
 mov rdi,[r15+72]
 mov rsi,[r15+88]
 mov rdx,[r15+96]
 lea rcx,[r14+16]
 xor r8d,r8d
 lea r9,[r14+112]
 call pg_document_plan_change@PLT
 test eax,eax
 jnz .Ldocs_finalize_doc_error
 cmp qword ptr [r14+112],0
 je .Ldocs_prepare_next
 mov dword ptr [r14+144],1
 mov rax,[r15+88]
 cmp rax,[r15+96]
 jb .Ldocs_append_write
 mov dword ptr [r14+144],2
.Ldocs_append_write:
 mov rdi,[r12+24]
 call json_object_array_length@PLT
 mov [r14+120],rax
 mov rdi,[r14+112]
 call json_object_get@PLT
 mov [rsp+8],rax
 mov rdi,[r12+24]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz .Ldocs_prepare_next
 mov rdi,[rsp+8]
 call json_object_put@PLT
 jmp .Ldocs_finalize_allocation
.Ldocs_prepare_next:
 inc r13
 jmp .Ldocs_prepare_each
.Ldocs_validate:
 mov rdi,r12
 call pg_record_tx_validate_batch@PLT
 test eax,eax
 jnz .Ldocs_finalize_invalid
 xor r13d,r13d
.Ldocs_checkpoint_each:
 cmp r13,[rbx+8]
 jae .Ldocs_finalize_ok
 imul r14,r13,152
 add r14,[rbx]
 mov r15,[r14]
 cmp dword ptr [r14+144],2
 jne .Ldocs_prepare_adoption
 cmp qword ptr [r14+136],0
 je .Ldocs_prepare_adoption
 mov rdi,[r14+128]
 mov rsi,[r14+16]
 mov rdx,[r14+24]
 mov rcx,[r15+104]
 call [r14+136]
 cmp eax,1
 ja .Ldocs_finalize_invalid
 test eax,eax
 jz .Ldocs_prepare_adoption
 mov rdi,[r15+72]
 mov rsi,[r15+88]
 mov rdx,[r15+96]
 lea rcx,[r14+16]
 mov r8d,1
 mov r9,rsp
 call pg_document_plan_change@PLT
 test eax,eax
 jnz .Ldocs_finalize_doc_error
 mov rdi,[rsp]
 call json_object_get@PLT
 mov [rsp+8],rax
 mov rdi,[r12+24]
 mov rsi,[r14+120]
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jz .Ldocs_checkpoint_swap
 mov rdi,[rsp+8]
 call json_object_put@PLT
 jmp .Ldocs_finalize_allocation
.Ldocs_checkpoint_swap:
 mov rdi,[r14+112]
 call json_object_put@PLT
 mov rax,[rsp]
 mov [r14+112],rax
 mov qword ptr [rsp],0
 mov dword ptr [r14+144],1
.Ldocs_prepare_adoption:
 lea rdi,[r14+32]
 mov rsi,r15
 lea rdx,[r14+16]
 mov ecx,[r14+144]
 call pg_document_adoption_prepare@PLT
 test eax,eax
 jnz .Ldocs_finalize_doc_error
 inc r13
 jmp .Ldocs_checkpoint_each
.Ldocs_finalize_ok:
 xor ebp,ebp
 jmp .Ldocs_finalize_return
.Ldocs_finalize_doc_error:
 cmp eax,3
 je .Ldocs_finalize_allocation
.Ldocs_finalize_invalid:
 mov ebp,1
 jmp .Ldocs_finalize_return
.Ldocs_finalize_allocation:
 mov ebp,4
.Ldocs_finalize_return:
 mov [rbx+40],ebp
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,24
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.Ldocs_adopt:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
.Ldocs_adopt_each:
 cmp r14,[rbx+8]
 jae .Ldocs_adopt_host
 imul r15,r14,152
 add r15,[rbx]
 lea rdi,[r15+32]
 call pg_document_adoption_apply@PLT
 test eax,eax
 jnz .Ldocs_adopt_return
 inc r14
 jmp .Ldocs_adopt_each
.Ldocs_adopt_host:
 xor eax,eax
 cmp qword ptr [rbx+24],0
 je .Ldocs_adopt_return
 mov rdi,[rbx+16]
 mov rsi,r12
 mov rdx,r13
 call [rbx+24]
.Ldocs_adopt_return:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Ldocs_publish:
 mov rax,[rdi+32]
 test rax,rax
 jz .Ldocs_publish_return
 mov rdi,[rdi+16]
 jmp rax
.Ldocs_publish_return:
 ret

.globl pg_document_tx_commit
.type pg_document_tx_commit,@function
pg_document_tx_commit:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,56
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov ebp,1
 test rbx,rbx
 jz .Ldocs_commit_return
 cmp dword ptr [rbx+32],0
 jne .Ldocs_commit_return
 cmp dword ptr [rbx+44],0
 jne .Ldocs_commit_return
 cmp dword ptr [rbx+36],0
 jne .Ldocs_commit_return
 cmp qword ptr [rbx+24],0
 je .Ldocs_commit_return
 mov rax,0x00ffffffffffffff
 cmp r13,rax
 ja .Ldocs_commit_return
 test r13,r13
 jz .Ldocs_commit_valid
 test r12,r12
 jz .Ldocs_commit_return
 xor r14d,r14d
.Ldocs_commit_check:
 cmp r14,r13
 jae .Ldocs_commit_valid
 imul r15,r14,152
 add r15,r12
 cmp qword ptr [r15],0
 je .Ldocs_commit_return
 cmp qword ptr [r15+8],0
 je .Ldocs_commit_return
 cmp qword ptr [r15+16],0
 jne .Ldocs_commit_return
 cmp qword ptr [r15+24],0
 jne .Ldocs_commit_return
 cmp qword ptr [r15+112],0
 jne .Ldocs_commit_return
 cmp qword ptr [r15+144],0
 jne .Ldocs_commit_return
 cmp qword ptr [r15+40],0
 jne .Ldocs_commit_return
 cmp qword ptr [r15+48],0
 jne .Ldocs_commit_return
 cmp dword ptr [r15+104],0
 jne .Ldocs_commit_return
 xor ecx,ecx
.Ldocs_commit_unique:
 cmp rcx,r14
 jae .Ldocs_commit_check_next
 imul rax,rcx,152
 mov rdx,[r15]
 cmp [r12+rax],rdx
 je .Ldocs_commit_return
 mov rdx,[r15+8]
 cmp [r12+rax+8],rdx
 je .Ldocs_commit_return
 mov rdx,[r15]
 mov rdx,[rdx+72]
 mov rax,[r12+rax]
 cmp [rax+72],rdx
 je .Ldocs_commit_return
 inc rcx
 jmp .Ldocs_commit_unique
.Ldocs_commit_check_next:
 inc r14
 jmp .Ldocs_commit_check
.Ldocs_commit_valid:
 mov r15,[rbx]
 test r15,r15
 jz .Ldocs_commit_return
 cmp dword ptr [r15+48],0
 jne .Ldocs_commit_return
 mov [rsp],r12
 mov [rsp+8],r13
 mov rax,[r15+24]
 mov [rsp+16],rax
 mov rax,[r15+32]
 mov [rsp+24],rax
 mov rax,[r15+40]
 mov [rsp+32],rax
 mov qword ptr [rsp+40],0
 mov [r15+24],rsp
 lea rax,[rip+.Ldocs_adopt]
 mov [r15+32],rax
 lea rax,[rip+.Ldocs_publish]
 mov [r15+40],rax
 mov rdi,rbx
 lea rsi,[rip+.Ldocs_finalize]
 mov rdx,rsp
 call pg_record_tx_commit_finalized@PLT
 mov ebp,eax
 test eax,eax
 jz .Ldocs_commit_restore
 cmp dword ptr [rsp+40],0
 je .Ldocs_commit_restore
 mov ebp,[rsp+40]
.Ldocs_commit_restore:
 mov rax,[rsp+16]
 mov [r15+24],rax
 mov rax,[rsp+24]
 mov [r15+32],rax
 mov rax,[rsp+32]
 mov [r15+40],rax
 xor r14d,r14d
.Ldocs_commit_cleanup:
 cmp r14,r13
 jae .Ldocs_commit_return
 imul r15,r14,152
 add r15,r12
 mov rdi,[r15+8]
 call pg_document_change_discard@PLT
 mov rdi,[r15+16]
 call json_object_put@PLT
 mov rdi,[r15+24]
 call json_object_put@PLT
 mov qword ptr [r15+16],0
 mov qword ptr [r15+24],0
 lea rdi,[r15+32]
 call pg_document_adoption_discard@PLT
 mov rdi,[r15+112]
 call json_object_put@PLT
 mov qword ptr [r15+112],0
 mov qword ptr [r15+144],0
 inc r14
 jmp .Ldocs_commit_cleanup
.Ldocs_commit_return:
 mov eax,ebp
 add rsp,56
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_tx_commit,.-pg_document_tx_commit
.section .note.GNU-stack,"",@progbits
.text
.globl pg_document_tx_open
.type pg_document_tx_open,@function
pg_document_tx_open:
 mov eax,1
 test rdi,rdi
 jz .Ldoc_scope_open_return
 test rsi,rsi
 jz .Ldoc_scope_open_return
 test rdx,rdx
 jz .Ldoc_scope_open_return
 cmp dword ptr [rdi+40],0
 jne .Ldoc_scope_open_return
 cmp qword ptr [rdi+16],0
 jne .Ldoc_scope_open_return
 cmp qword ptr [rdi+24],0
 jne .Ldoc_scope_open_return
 cmp qword ptr [rsi+32],0
 jne .Ldoc_scope_open_return
 cmp qword ptr [rsi+24],0
 je .Ldoc_scope_open_return
 cmp qword ptr [rdx],0
 je .Ldoc_scope_open_return
 cmp qword ptr [rdx+104],0
 jne .Ldoc_scope_open_return
 mov [rdi],rsi
 mov [rdi+8],rdx
 mov qword ptr [rdi+16],0
 mov qword ptr [rdi+24],0
 mov qword ptr [rdi+32],0
 mov qword ptr [rdi+40],0
 xor eax,eax
.Ldoc_scope_open_return:
 ret
.size pg_document_tx_open,.-pg_document_tx_open

.globl pg_document_tx_get
.type pg_document_tx_get,@function
pg_document_tx_get:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,56
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov [rsp+24],rcx
 mov [rsp+32],r8
 mov r14,r9
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 test r14,r14
 jz .Ldoc_scope_get_return
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Ldoc_scope_get_return
 cmp qword ptr [rbx+40],0
 jne .Ldoc_scope_get_return
 mov r15,[rbx]
 test r15,r15
 jz .Ldoc_scope_get_return
 cmp qword ptr [r15+32],0
 jne .Ldoc_scope_get_return
 cmp qword ptr [r15+24],0
 je .Ldoc_scope_get_return
 test r12,r12
 jz .Ldoc_scope_get_return
 mov rdi,r13
 call pg_document_address_valid@PLT
 test eax,eax
 jz .Ldoc_scope_get_return
 mov eax,[r12+24]
 cmp eax,[r13+40]
 jne .Ldoc_scope_get_return
 mov eax,[r12+36]
 cmp eax,[r13+44]
 jne .Ldoc_scope_get_return
 mov rax,[r12+16]
 test rax,rax
 jz .Ldoc_scope_get_return
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Ldoc_scope_get_return
 mov rdx,[r12+8]
 cmp rdx,[r13+8]
 jne .Ldoc_scope_get_return
 test rdx,rdx
 jz .Ldoc_scope_get_identity_ready
 mov rdi,[r12]
 test rdi,rdi
 jz .Ldoc_scope_get_return
 mov rsi,[r13]
 call memcmp@PLT
 test eax,eax
 jnz .Ldoc_scope_get_return
.Ldoc_scope_get_identity_ready:
 mov dword ptr [rbx+40],1
 mov dword ptr [r15+32],1
 cmp dword ptr [r13+40],2
 jne .Ldoc_scope_get_search
 mov rdi,r15
 mov rsi,[r13+32]
 call pg_record_tx_check_document_task@PLT
 test eax,eax
 jnz .Ldoc_scope_get_cleanup
.Ldoc_scope_get_search:
 mov qword ptr [rsp+16],0
.Ldoc_scope_get_existing:
 mov rax,[rsp+16]
 cmp rax,[rbx+24]
 jae .Ldoc_scope_get_load
 imul rax,152
 add rax,[rbx+16]
 mov [rsp+40],rax
 mov rdi,[rax]
 add rdi,24
 mov rsi,r13
 call pg_document_address_equal@PLT
 test eax,eax
 jnz .Ldoc_scope_get_reuse
 inc qword ptr [rsp+16]
 jmp .Ldoc_scope_get_existing
.Ldoc_scope_get_reuse:
 mov rax,[rsp+40]
 mov rax,[rax+8]
 mov [r14],rax
 xor ebp,ebp
 jmp .Ldoc_scope_get_cleanup
.Ldoc_scope_get_load:
 mov rdi,[rbx+8]
 mov rsi,r12
 mov rdx,r13
 mov rcx,rsp
 call pg_document_cache_get@PLT
 mov ebp,eax
 test eax,eax
 jnz .Ldoc_scope_get_cleanup
 mov ebp,3
 mov edi,1
 mov esi,32
 call calloc@PLT
 test rax,rax
 jz .Ldoc_scope_get_cleanup
 mov [rsp+8],rax
 mov rdi,[rsp]
 mov rsi,rax
 call pg_document_loaded_begin@PLT
 mov ebp,eax
 test eax,eax
 jnz .Ldoc_scope_get_cleanup
 mov rax,[rbx+24]
 cmp rax,[rbx+32]
 jb .Ldoc_scope_get_install
 mov rsi,[rbx+32]
 test rsi,rsi
 jnz .Ldoc_scope_get_grow
 mov esi,4
 jmp .Ldoc_scope_get_allocate
.Ldoc_scope_get_grow:
 mov rax,0x007fffffffffffff
 cmp rsi,rax
 jae .Ldoc_scope_get_capacity
 shl rsi,1
.Ldoc_scope_get_allocate:
 mov [rsp+48],rsi
 mov rdi,[rbx+16]
 mov edx,152
 call reallocarray@PLT
 test rax,rax
 jz .Ldoc_scope_get_allocation
 mov [rbx+16],rax
 mov rax,[rsp+48]
 mov [rbx+32],rax
.Ldoc_scope_get_install:
 imul rdi,[rbx+24],152
 add rdi,[rbx+16]
 mov [rsp+40],rdi
 xor eax,eax
 mov ecx,19
 rep stosq
 mov rax,[rsp+40]
 mov rcx,[rsp]
 mov [rax],rcx
 mov rcx,[rsp+8]
 mov [rax+8],rcx
 mov rcx,[rsp+24]
 mov [rax+128],rcx
 mov rcx,[rsp+32]
 mov [rax+136],rcx
 mov rax,[rsp+8]
 mov [r14],rax
 mov qword ptr [rsp+8],0
 inc qword ptr [rbx+24]
 xor ebp,ebp
 jmp .Ldoc_scope_get_cleanup
.Ldoc_scope_get_capacity:
 mov ebp,2
 jmp .Ldoc_scope_get_cleanup
.Ldoc_scope_get_allocation:
 mov ebp,3
.Ldoc_scope_get_cleanup:
 cmp qword ptr [rsp+8],0
 je .Ldoc_scope_get_clear
 mov rdi,[rsp+8]
 call pg_document_change_discard@PLT
 mov rdi,[rsp+8]
 call free@PLT
.Ldoc_scope_get_clear:
 mov dword ptr [r15+32],0
 mov dword ptr [rbx+40],0
.Ldoc_scope_get_return:
 mov eax,ebp
 add rsp,56
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_tx_get,.-pg_document_tx_get

/* Release managed edit allocations after the settlement/discard path. */
.Ldoc_scope_release:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 xor r12d,r12d
.Ldoc_scope_release_each:
 cmp r12,[rbx+24]
 jae .Ldoc_scope_release_array
 imul rax,r12,152
 add rax,[rbx+16]
 mov r13,[rax+8]
 mov rdi,r13
 call pg_document_change_discard@PLT
 mov rdi,r13
 call free@PLT
 inc r12
 jmp .Ldoc_scope_release_each
.Ldoc_scope_release_array:
 mov rdi,[rbx+16]
 call free@PLT
 mov qword ptr [rbx+16],0
 mov qword ptr [rbx+24],0
 mov qword ptr [rbx+32],0
 mov dword ptr [rbx+40],0
 mov dword ptr [rbx+44],1
 pop r13
 pop r12
 pop rbx
 ret

.globl pg_document_tx_finish
.type pg_document_tx_finish,@function
pg_document_tx_finish:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov eax,1
 test rbx,rbx
 jz .Ldoc_scope_finish_return
 cmp qword ptr [rbx+40],0
 jne .Ldoc_scope_finish_return
 mov rdi,[rbx]
 test rdi,rdi
 jz .Ldoc_scope_finish_return
 cmp qword ptr [rdi+32],0
 jne .Ldoc_scope_finish_return
 cmp dword ptr [rdi+44],0
 jne .Ldoc_scope_finish_return
 mov dword ptr [rbx+40],1
 mov rsi,[rbx+16]
 mov rdx,[rbx+24]
 call pg_document_tx_commit@PLT
 mov r12d,eax
 mov rdi,rbx
 call .Ldoc_scope_release
 mov eax,r12d
.Ldoc_scope_finish_return:
 add rsp,8
 pop r12
 pop rbx
 ret
.size pg_document_tx_finish,.-pg_document_tx_finish

.globl pg_document_tx_abort
.type pg_document_tx_abort,@function
pg_document_tx_abort:
 push rbx
 sub rsp,16
 mov rbx,rdi
 mov eax,1
 test rbx,rbx
 jz .Ldoc_scope_abort_return
 cmp dword ptr [rbx+40],0
 jne .Ldoc_scope_abort_return
 cmp dword ptr [rbx+44],1
 jne .Ldoc_scope_abort_active
 xor eax,eax
 jmp .Ldoc_scope_abort_return
.Ldoc_scope_abort_active:
 mov rdi,[rbx]
 test rdi,rdi
 jz .Ldoc_scope_abort_return
 mov dword ptr [rbx+40],1
 call pg_record_tx_discard@PLT
 test eax,eax
 jnz .Ldoc_scope_abort_failed
 mov rdi,rbx
 call .Ldoc_scope_release
 xor eax,eax
 jmp .Ldoc_scope_abort_return
.Ldoc_scope_abort_failed:
 mov dword ptr [rbx+40],0
.Ldoc_scope_abort_return:
 add rsp,16
 pop rbx
 ret
.size pg_document_tx_abort,.-pg_document_tx_abort
