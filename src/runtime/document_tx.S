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
