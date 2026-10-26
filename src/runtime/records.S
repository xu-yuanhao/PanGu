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
.text
/* Typed SubmissionChange status selection. Inputs remain borrowed. */
.globl pg_records_submission_change
.type pg_records_submission_change,@function
pg_records_submission_change:
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
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 test r13,r13
 jz .Lsubmission_change_return
 mov qword ptr [r13],0
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lsubmission_change_return
 mov rdi,r12
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lsubmission_change_return
 mov rdi,rbx
 lea rsi,[rip+.Ls_status]
 call json_object_object_get@PLT
 mov rdi,rax
 call .Ls_status_index
 mov r14d,eax
 cmp eax,3
 ja .Lsubmission_change_return
 cmp eax,2
 jae .Lsubmission_change_copy
 mov rdi,r12
 lea rsi,[rip+.Ls_status]
 call json_object_object_get@PLT
 mov rdi,rax
 call .Ls_status_index
 mov r15d,eax
 cmp eax,1
 jb .Lsubmission_change_return
 cmp eax,3
 ja .Lsubmission_change_return
 cmp eax,1
 jne .Lsubmission_change_done
 test r14d,r14d
 jnz .Lsubmission_change_return
 jmp .Lsubmission_change_copy
.Lsubmission_change_done:
 cmp eax,2
 jne .Lsubmission_change_copy
 cmp r14d,1
 jne .Lsubmission_change_return
.Lsubmission_change_copy:
 mov rdi,rbx
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lsubmission_change_allocation
 cmp r14d,2
 jae .Lsubmission_change_success
 cmp r15d,1
 je .Lsubmission_change_place
 mov rdi,r12
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lsubmission_change_allocation
 mov rdi,[rsp+8]
 call json_object_iter_begin@PLT
 mov [rsp+16],rax
 mov rdi,[rsp+8]
 call json_object_iter_end@PLT
 mov [rsp+24],rax
.Lsubmission_change_merge:
 lea rdi,[rsp+16]
 lea rsi,[rsp+24]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lsubmission_change_success
 lea rdi,[rsp+16]
 call json_object_iter_peek_name@PLT
 mov r14,rax
 lea rdi,[rsp+16]
 call json_object_iter_peek_value@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov r15,rax
 mov rdi,[rsp]
 mov rsi,r14
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lsubmission_change_insert_failed
 lea rdi,[rsp+16]
 call json_object_iter_next@PLT
 jmp .Lsubmission_change_merge
.Lsubmission_change_place:
 mov rdi,rbx
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ls_input]
 call .Ls_string
 lea rdi,[rip+.Ls_done]
 test eax,eax
 jz .Lsubmission_change_new_status
 lea rdi,[rip+.Ls_placed]
.Lsubmission_change_new_status:
 call json_object_new_string@PLT
 test rax,rax
 jz .Lsubmission_change_allocation
 mov r15,rax
 mov rdi,[rsp]
 lea rsi,[rip+.Ls_status]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lsubmission_change_insert_failed
 mov rdi,r12
 lea rsi,[rip+.Ls_entry]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rsp+32]
 mov qword ptr [rsp+32],0
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lsubmission_change_entry_failed
 mov r15,[rsp+32]
 mov rdi,[rsp]
 lea rsi,[rip+.Ls_entry]
 mov rdx,r15
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lsubmission_change_insert_failed
.Lsubmission_change_success:
 mov rax,[rsp]
 mov [r13],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
 jmp .Lsubmission_change_cleanup
.Lsubmission_change_entry_failed:
 mov rdi,[rsp+32]
 call json_object_put@PLT
 jmp .Lsubmission_change_allocation
.Lsubmission_change_insert_failed:
 mov rdi,r15
 call json_object_put@PLT
.Lsubmission_change_allocation:
 mov ebp,5
.Lsubmission_change_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
.Lsubmission_change_return:
 mov eax,ebp
 add rsp,56
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_records_submission_change,.-pg_records_submission_change
.Ls_status_index:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 xor r12d,r12d
.Ls_status_loop:
 lea rax,[rip+.Ls_names]
 movsxd rsi,dword ptr [rax+r12*4]
 add rsi,rax
 mov rdi,rbx
 call .Ls_string
 test eax,eax
 jnz .Ls_status_found
 inc r12d
 cmp r12d,4
 jb .Ls_status_loop
.Ls_status_found:
 mov eax,r12d
 add rsp,8
 pop r12
 pop rbx
 ret
.Ls_string:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ls_string_return
 mov rdi,rbx
 call json_object_get_string_len@PLT
 mov [rsp],eax
 mov rdi,r12
 call strlen@PLT
 cmp eax,[rsp]
 jne .Ls_string_no
 mov rdi,rbx
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,r12
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 jmp .Ls_string_return
.Ls_string_no:
 xor eax,eax
.Ls_string_return:
 add rsp,8
 pop r12
 pop rbx
 ret
.section .rodata
.Ls_names: .long .Ls_queued-.Ls_names,.Ls_placed-.Ls_names,.Ls_done-.Ls_names,.Ls_unanswered-.Ls_names
.Ls_queued: .asciz "queued"
.Ls_placed: .asciz "placed"
.Ls_done: .asciz "done"
.Ls_unanswered: .asciz "unanswered"
.Ls_status: .asciz "status"
.Ls_entry: .asciz "entry"
.Ls_type: .asciz "type"
.Ls_input: .asciz "input"
