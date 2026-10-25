.intel_syntax noprefix
.text
.globl pg_records_commit
.type pg_records_commit,@function
pg_records_commit:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r14d,1
 xor r13d,r13d
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 test rbx,rbx
 jz .Lrecords_return
 cmp dword ptr [rbx+48],0
 jne .Lrecords_return
 mov eax,[rbx+52]
 test eax,eax
 jz .Lrecords_healthy
 mov r14d,eax
 jmp .Lrecords_return
.Lrecords_healthy:
 cmp qword ptr [rbx+8],0
 je .Lrecords_return
 mov rax,9007199254740991
 cmp [rbx+16],rax
 ja .Lrecords_return
 mov rdi,r12
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lrecords_return
 mov dword ptr [rbx+48],1
 mov rdi,r12
 call pg_document_value_valid
 test eax,eax
 jz .Lrecords_finish
 mov rdi,r12
 call json_object_array_length@PLT
 test rax,rax
 jz .Lrecords_empty
 mov r14d,2
 cmp dword ptr [rbx+56],0
 jne .Lrecords_finish
 mov r14d,5
 mov rdi,r12
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 mov r13,[rsp+8]
 test eax,eax
 jnz .Lrecords_finish
 mov [rbx+80],r13
 mov qword ptr [rbx+72],0
 mov dword ptr [rbx+60],0
 mov dword ptr [rbx+64],0
 mov rdi,[rbx]
 mov rsi,r13
 lea rdx,[rip+.Lrecords_adopt]
 mov rcx,rbx
 mov r8,rsp
 call [rbx+8]
 test eax,eax
 jnz .Lrecords_storage_failed
 cmp dword ptr [rbx+64],0
 jne .Lrecords_adoption_failed
 cmp dword ptr [rbx+60],1
 jne .Lrecords_adoption_failed
 mov rax,[rsp]
 cmp rax,[rbx+72]
 jne .Lrecords_adoption_failed
 cmp dword ptr [rbx+52],0
 jne .Lrecords_adoption_failed
 cmp qword ptr [rbx+40],0
 je .Lrecords_empty
 mov rdi,[rbx+24]
 mov rsi,r13
 mov rdx,[rbx+16]
 call [rbx+40]
.Lrecords_empty:
 xor r14d,r14d
 jmp .Lrecords_finish
.Lrecords_storage_failed:
 mov r14d,3
 jmp .Lrecords_fail
.Lrecords_adoption_failed:
 mov r14d,4
.Lrecords_fail:
 mov [rbx+52],r14d
.Lrecords_finish:
 mov qword ptr [rbx+80],0
 mov qword ptr [rbx+72],0
 mov dword ptr [rbx+60],0
 mov dword ptr [rbx+64],0
 mov rdi,r13
 call json_object_put@PLT
 mov dword ptr [rbx+48],0
.Lrecords_return:
 mov eax,r14d
 add rsp,16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_records_commit,.-pg_records_commit

/* Nonthrowing Storage adoption boundary. */
.Lrecords_adopt:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 cmp dword ptr [rbx+60],0
 jne .Lrecords_adopt_bad
 mov dword ptr [rbx+60],1
 cmp r12,[rbx+16]
 jbe .Lrecords_adopt_bad
 mov rax,9007199254740991
 cmp r12,rax
 ja .Lrecords_adopt_bad
 mov [rbx+72],r12
 cmp qword ptr [rbx+32],0
 je .Lrecords_adopted
 mov rdi,[rbx+24]
 mov rsi,[rbx+80]
 mov rdx,r12
 call [rbx+32]
 test eax,eax
 jnz .Lrecords_adopt_bad
.Lrecords_adopted:
 mov [rbx+16],r12
 jmp .Lrecords_adopt_return
.Lrecords_adopt_bad:
 mov dword ptr [rbx+64],1
 mov dword ptr [rbx+52],4
.Lrecords_adopt_return:
 add rsp,8
 pop r12
 pop rbx
 ret
.section .note.GNU-stack,"",@progbits
