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
.text
.globl pg_record_tx_begin
.type pg_record_tx_begin,@function
pg_record_tx_begin:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov eax,1
 test rbx,rbx
 jz .Ltx_begin_return
 test r12,r12
 jz .Ltx_begin_return
 cmp dword ptr [rbx+32],0
 jne .Ltx_begin_return
 cmp qword ptr [rbx+24],0
 jne .Ltx_begin_return
 cmp dword ptr [r12+48],0
 jne .Ltx_begin_return
 cmp dword ptr [r12+52],0
 jne .Ltx_begin_return
 mov dword ptr [rbx+32],1
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Ltx_begin_allocation
 mov [rbx],r12
 mov [rbx+8],r13
 mov [rbx+16],r14
 mov [rbx+24],rax
 mov qword ptr [rbx+32],0
 mov qword ptr [rbx+40],0
 xor eax,eax
 jmp .Ltx_begin_return
.Ltx_begin_allocation:
 mov dword ptr [rbx+32],0
 mov eax,4
.Ltx_begin_return:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_record_tx_begin,.-pg_record_tx_begin

.globl pg_record_tx_read
.type pg_record_tx_read,@function
pg_record_tx_read:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov r12,rcx
 mov [rsp+8],esi
 mov [rsp+16],rdx
 mov eax,1
 test r12,r12
 jz .Ltx_read_return
 mov qword ptr [r12],0
 test rbx,rbx
 jz .Ltx_read_return
 cmp dword ptr [rbx+32],0
 jne .Ltx_read_return
 mov eax,3
 cmp dword ptr [rbx+36],0
 jne .Ltx_read_return
 mov eax,1
 cmp qword ptr [rbx+24],0
 je .Ltx_read_return
 mov eax,2
 cmp dword ptr [rbx+40],0
 jne .Ltx_read_return
 mov eax,1
 cmp dword ptr [rsp+8],3
 ja .Ltx_read_return
 cmp qword ptr [rbx+16],0
 je .Ltx_read_return
 mov qword ptr [rsp],0
 mov dword ptr [rbx+32],1
 mov rdi,[rbx+8]
 mov esi,[rsp+8]
 mov rdx,[rsp+16]
 mov rcx,rsp
 call [rbx+16]
 mov dword ptr [rbx+32],0
 test eax,eax
 jnz .Ltx_read_error
 mov rax,[rsp]
 mov [r12],rax
 xor eax,eax
 jmp .Ltx_read_return
.Ltx_read_error:
 cmp eax,1
 mov ebx,5
 mov eax,7
 cmove ebx,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebx
.Ltx_read_return:
 add rsp,24
 pop r12
 pop rbx
 ret
.size pg_record_tx_read,.-pg_record_tx_read

.globl pg_record_tx_stage
.type pg_record_tx_stage,@function
pg_record_tx_stage:
 push rbp
 push rbx
 push r12
 push r13
 sub rsp,24
 mov rbx,rdi
 mov r12d,esi
 mov r13,rdx
 mov ebp,1
 mov qword ptr [rsp],0
 test rbx,rbx
 jz .Ltx_stage_return
 cmp dword ptr [rbx+32],0
 jne .Ltx_stage_return
 mov ebp,3
 cmp dword ptr [rbx+36],0
 jne .Ltx_stage_return
 mov ebp,1
 cmp qword ptr [rbx+24],0
 je .Ltx_stage_return
 mov ebp,1
 cmp r12d,7
 ja .Ltx_stage_return
 cmp r12d,3
 ja .Ltx_stage_document
 mov dword ptr [rbx+40],1
.Ltx_stage_document:
 mov dword ptr [rbx+32],1
 mov rdi,r13
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Ltx_stage_cleanup
 mov rdi,r13
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ltx_stage_cleanup
 mov rdi,r13
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 lea rcx,[rip+.Ltx_types]
 movsxd rsi,dword ptr [rcx+r12*4]
 add rsi,rcx
 mov rdi,rax
 call .Ls_string
 test eax,eax
 jz .Ltx_stage_cleanup
 mov rdi,r13
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ltx_stage_allocation
 mov rdi,[rbx+24]
 mov rsi,[rsp]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ltx_stage_allocation
 mov qword ptr [rsp],0
 xor ebp,ebp
 jmp .Ltx_stage_cleanup
.Ltx_stage_allocation:
 mov ebp,4
.Ltx_stage_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov dword ptr [rbx+32],0
.Ltx_stage_return:
 mov eax,ebp
 add rsp,24
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_stage,.-pg_record_tx_stage

.globl pg_record_tx_discard
.type pg_record_tx_discard,@function
pg_record_tx_discard:
 mov eax,1
 test rdi,rdi
 jz .Ltx_discard_return
 cmp dword ptr [rdi+32],0
 jne .Ltx_discard_return
 push rbx
 mov rbx,rdi
 mov dword ptr [rbx+32],1
 mov dword ptr [rbx+36],1
 mov rdi,[rbx+24]
 mov qword ptr [rbx+24],0
 call json_object_put@PLT
 mov dword ptr [rbx+32],0
 xor eax,eax
 pop rbx
.Ltx_discard_return:
 ret
.size pg_record_tx_discard,.-pg_record_tx_discard

.globl pg_record_tx_commit
.type pg_record_tx_commit,@function
pg_record_tx_commit:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov eax,1
 test rbx,rbx
 jz .Ltx_commit_return
 cmp dword ptr [rbx+32],0
 jne .Ltx_commit_return
 mov eax,3
 cmp dword ptr [rbx+36],0
 jne .Ltx_commit_return
 mov eax,1
 cmp qword ptr [rbx+24],0
 je .Ltx_commit_return
 mov dword ptr [rbx+36],1
 mov dword ptr [rbx+32],1
 mov rdi,[rbx]
 mov rsi,[rbx+24]
 call pg_records_commit
 mov r12d,eax
 mov rdi,[rbx+24]
 mov qword ptr [rbx+24],0
 call json_object_put@PLT
 mov dword ptr [rbx+32],0
 xor eax,eax
 test r12d,r12d
 jz .Ltx_commit_return
 mov eax,6
.Ltx_commit_return:
 add rsp,8
 pop r12
 pop rbx
 ret
.size pg_record_tx_commit,.-pg_record_tx_commit
.section .rodata
.Ltx_types: .long .Ltx_conversation-.Ltx_types,.Ltx_entry-.Ltx_types,.Ltx_task-.Ltx_types,.Ltx_submission-.Ltx_types,.Ltx_create-.Ltx_types,.Ltx_copy-.Ltx_types,.Ltx_change-.Ltx_types,.Ltx_retire-.Ltx_types
.Ltx_conversation: .asciz "conversation"
.Ltx_entry: .asciz "entry"
.Ltx_task: .asciz "task"
.Ltx_submission: .asciz "submission"
.Ltx_create: .asciz "document.create"
.Ltx_copy: .asciz "document.copy"
.Ltx_change: .asciz "document.change"
.Ltx_retire: .asciz "document.retire"
.text
.globl pg_record_tx_submission_change
.type pg_record_tx_submission_change,@function
pg_record_tx_submission_change:
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
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 test rbx,rbx
 jz .Ltx_submission_return
 cmp dword ptr [rbx+32],0
 jne .Ltx_submission_return
 mov ebp,3
 cmp dword ptr [rbx+36],0
 jne .Ltx_submission_return
 mov ebp,1
 cmp qword ptr [rbx+24],0
 je .Ltx_submission_return
 mov dword ptr [rbx+40],1
 test r12,r12
 jz .Ltx_submission_return
 mov rax,9007199254740991
 cmp r12,rax
 ja .Ltx_submission_return
 mov dword ptr [rbx+32],1
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov r15,rax
.Ltx_submission_find:
 test r15,r15
 jz .Ltx_submission_committed
 dec r15
 mov rdi,[rbx+24]
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_submission]
 call .Ls_string
 test eax,eax
 jz .Ltx_submission_find
 mov rdi,r14
 lea rsi,[rip+.Ltx_value]
 call json_object_object_get@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,r12
 jne .Ltx_submission_find
 jmp .Ltx_submission_apply
.Ltx_submission_committed:
 cmp qword ptr [rbx+16],0
 je .Ltx_submission_cleanup
 mov rdi,[rbx+8]
 mov esi,3
 mov rdx,r12
 mov rcx,rsp
 call [rbx+16]
 test eax,eax
 jnz .Ltx_submission_cleanup
 mov r14,[rsp]
.Ltx_submission_apply:
 mov rdi,r14
 mov rsi,r13
 lea rdx,[rsp+8]
 call pg_records_submission_change
 test eax,eax
 jz .Ltx_submission_write
 cmp eax,5
 je .Ltx_submission_allocation
 jmp .Ltx_submission_cleanup
.Ltx_submission_write:
 call json_object_new_object@PLT
 test rax,rax
 jz .Ltx_submission_allocation
 mov [rsp+16],rax
 lea rdi,[rip+.Ltx_submission]
 call json_object_new_string@PLT
 test rax,rax
 jz .Ltx_submission_allocation
 mov r15,rax
 mov rdi,[rsp+16]
 lea rsi,[rip+.Ls_type]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ltx_submission_insert_failed
 mov r15,[rsp+8]
 mov rdi,[rsp+16]
 lea rsi,[rip+.Ltx_value]
 mov rdx,r15
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ltx_submission_allocation
 mov qword ptr [rsp+8],0
 mov rdi,[rbx+24]
 mov rsi,[rsp+16]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ltx_submission_allocation
 mov qword ptr [rsp+16],0
 xor ebp,ebp
 jmp .Ltx_submission_cleanup
.Ltx_submission_insert_failed:
 mov rdi,r15
 call json_object_put@PLT
.Ltx_submission_allocation:
 mov ebp,4
.Ltx_submission_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov dword ptr [rbx+32],0
.Ltx_submission_return:
 mov eax,ebp
 add rsp,40
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_submission_change,.-pg_record_tx_submission_change
.section .rodata
.Ltx_value: .asciz "value"
.Ltx_id: .asciz "id"
.text
.globl pg_record_tx_append_entry
.type pg_record_tx_append_entry,@function
pg_record_tx_append_entry:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,72
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov [rsp+40],rcx
 mov r14,r8
 mov r15,r9
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],0
 test r15,r15
 jz .Ltx_entry_return
 mov qword ptr [r15],0
 test rbx,rbx
 jz .Ltx_entry_return
 cmp dword ptr [rbx+32],0
 jne .Ltx_entry_return
 mov ebp,3
 cmp dword ptr [rbx+36],0
 jne .Ltx_entry_return
 mov ebp,1
 cmp qword ptr [rbx+24],0
 je .Ltx_entry_return
 mov dword ptr [rbx+40],1
 test r14,r14
 jz .Ltx_entry_return
 test r12,r12
 jz .Ltx_entry_return
 mov rax,9007199254740991
 cmp r12,rax
 ja .Ltx_entry_return
 cmp [rsp+40],rax
 ja .Ltx_entry_return
 mov dword ptr [rbx+32],1
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov [rsp+48],rax
.Ltx_entry_conversation:
 cmp qword ptr [rsp+48],0
 je .Ltx_entry_read_conversation
 dec qword ptr [rsp+48]
 mov rdi,[rbx+24]
 mov rsi,[rsp+48]
 call json_object_array_get_idx@PLT
 mov [rsp+56],rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_conversation]
 call .Ls_string
 test eax,eax
 jz .Ltx_entry_conversation
 mov rdi,[rsp+56]
 lea rsi,[rip+.Ltx_value]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,r12
 jne .Ltx_entry_conversation
 jmp .Ltx_entry_mint
.Ltx_entry_read_conversation:
 cmp qword ptr [rbx+16],0
 je .Ltx_entry_cleanup
 mov rdi,[rbx+8]
 xor esi,esi
 mov rdx,r12
 lea rcx,[rsp+24]
 call [rbx+16]
 test eax,eax
 jz .Ltx_entry_conversation_loaded
 cmp eax,1
 jne .Ltx_entry_read_failed
 jmp .Ltx_entry_cleanup
.Ltx_entry_conversation_loaded:
 cmp qword ptr [rsp+24],0
 je .Ltx_entry_cleanup
.Ltx_entry_mint:
 mov qword ptr [rsp+32],0
 mov rdi,[rbx+8]
 lea rsi,[rsp+32]
 call r14
 test eax,eax
 jnz .Ltx_entry_read_failed
 mov rax,[rsp+32]
 test rax,rax
 jz .Ltx_entry_read_failed
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Ltx_entry_read_failed
 mov rdi,r13
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Ltx_entry_cleanup
 mov rdi,r13
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ltx_entry_cleanup
 mov rdi,r13
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ltx_entry_allocation
 mov rdi,[rsp+32]
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_id]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Ltx_entry_allocation
 mov rdi,r12
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_conversation_id]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Ltx_entry_allocation
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_head]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_self]
 call .Ls_string
 test eax,eax
 jz .Ltx_entry_task
 mov rdi,[rsp+32]
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_head]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Ltx_entry_allocation
.Ltx_entry_task:
 mov rdi,[rsp+40]
 test rdi,rdi
 jz .Ltx_entry_no_task
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_by_task]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Ltx_entry_allocation
 jmp .Ltx_entry_snapshot
.Ltx_entry_no_task:
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_by_task]
 call json_object_object_del@PLT
.Ltx_entry_snapshot:
 mov rdi,[rsp]
 lea rsi,[rsp+16]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ltx_entry_allocation
 call json_object_new_object@PLT
 test rax,rax
 jz .Ltx_entry_allocation
 mov [rsp+8],rax
 lea rdi,[rip+.Ltx_entry]
 call json_object_new_string@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ls_type]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Ltx_entry_allocation
 mov rdx,[rsp]
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ltx_value]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ltx_entry_allocation
 mov qword ptr [rsp],0
 mov rdi,[rbx+24]
 mov rsi,[rsp+8]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ltx_entry_allocation
 mov qword ptr [rsp+8],0
 mov rax,[rsp+16]
 mov [r15],rax
 mov qword ptr [rsp+16],0
 xor ebp,ebp
 jmp .Ltx_entry_cleanup
.Ltx_entry_read_failed:
 mov ebp,5
 jmp .Ltx_entry_cleanup
.Ltx_entry_allocation:
 mov ebp,4
.Ltx_entry_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov dword ptr [rbx+32],0
.Ltx_entry_return:
 mov eax,ebp
 add rsp,72
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_append_entry,.-pg_record_tx_append_entry
/* Attach an allocated scalar held in RAX, consuming it on either result. */
.Ltx_entry_attach:
 test rax,rax
 jz .Ltx_entry_attach_missing
 push rbx
 mov rbx,rax
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Ltx_entry_attach_return
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,1
.Ltx_entry_attach_return:
 pop rbx
 ret
.Ltx_entry_attach_missing:
 mov eax,1
 ret
.section .rodata
.Ltx_conversation_id: .asciz "conversationId"
.Ltx_head: .asciz "head"
.Ltx_self: .asciz "self"
.Ltx_by_task: .asciz "byTaskId"
.text
.globl pg_record_tx_create_conversation
.type pg_record_tx_create_conversation,@function
pg_record_tx_create_conversation:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,72
 mov rbx,rdi
 mov r12,rsi
 mov [rsp+40],edx
 mov r13,rcx
 mov r14,r8
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],0
 mov qword ptr [rsp+32],0
 test r14,r14
 jz .Ltx_conversation_return
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Ltx_conversation_return
 cmp dword ptr [rbx+32],0
 jne .Ltx_conversation_return
 mov ebp,3
 cmp dword ptr [rbx+36],0
 jne .Ltx_conversation_return
 mov ebp,1
 cmp qword ptr [rbx+24],0
 je .Ltx_conversation_return
 mov dword ptr [rbx+40],1
 cmp dword ptr [rsp+40],1
 ja .Ltx_conversation_return
 mov rax,9007199254740991
 cmp r12,rax
 ja .Ltx_conversation_return
 cmp dword ptr [rsp+40],0
 je .Ltx_conversation_minted
 test r12,r12
 jnz .Ltx_conversation_return
 mov qword ptr [rsp+48],1
 mov dword ptr [rbx+32],1
 jmp .Ltx_conversation_record
.Ltx_conversation_minted:
 test r13,r13
 jz .Ltx_conversation_return
 mov dword ptr [rbx+32],1
 mov qword ptr [rsp+48],0
 mov rdi,[rbx+8]
 lea rsi,[rsp+48]
 call r13
 test eax,eax
 jnz .Ltx_conversation_read_failed
 mov rax,[rsp+48]
 test rax,rax
 jz .Ltx_conversation_read_failed
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Ltx_conversation_read_failed
 test r12,r12
 jz .Ltx_conversation_record
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov r15,rax
.Ltx_conversation_find_task:
 test r15,r15
 jz .Ltx_conversation_read_task
 dec r15
 mov rdi,[rbx+24]
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_task]
 call .Ls_string
 test eax,eax
 jz .Ltx_conversation_find_task
 mov rdi,r13
 lea rsi,[rip+.Ltx_value]
 call json_object_object_get@PLT
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,r12
 jne .Ltx_conversation_find_task
 jmp .Ltx_conversation_task_owner
.Ltx_conversation_read_task:
 cmp qword ptr [rbx+16],0
 je .Ltx_conversation_cleanup
 mov rdi,[rbx+8]
 mov esi,2
 mov rdx,r12
 lea rcx,[rsp+24]
 call [rbx+16]
 test eax,eax
 jz .Ltx_conversation_task_loaded
 cmp eax,1
 jne .Ltx_conversation_read_failed
 jmp .Ltx_conversation_cleanup
.Ltx_conversation_task_loaded:
 mov r13,[rsp+24]
 test r13,r13
 jz .Ltx_conversation_cleanup
.Ltx_conversation_task_owner:
 mov rdi,r13
 lea rsi,[rip+.Ltx_conversation_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 test rax,rax
 jz .Ltx_conversation_cleanup
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Ltx_conversation_cleanup
 mov [rsp+56],rax
 call json_object_new_object@PLT
 test rax,rax
 jz .Ltx_conversation_allocation
 mov [rsp+32],rax
 mov rdi,[rsp+56]
 call json_object_new_int64@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ltx_conversation_id]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Ltx_conversation_allocation
 mov rdi,r12
 call json_object_new_int64@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ltx_task_id]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Ltx_conversation_allocation
.Ltx_conversation_record:
 call json_object_new_object@PLT
 test rax,rax
 jz .Ltx_conversation_allocation
 mov [rsp],rax
 mov rdi,[rsp+48]
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_id]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Ltx_conversation_allocation
 cmp qword ptr [rsp+32],0
 je .Ltx_conversation_snapshot
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_owner]
 mov rdx,[rsp+32]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ltx_conversation_allocation
 mov qword ptr [rsp+32],0
.Ltx_conversation_snapshot:
 mov rdi,[rsp]
 lea rsi,[rsp+16]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ltx_conversation_allocation
 call json_object_new_object@PLT
 test rax,rax
 jz .Ltx_conversation_allocation
 mov [rsp+8],rax
 lea rdi,[rip+.Ltx_conversation]
 call json_object_new_string@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ls_type]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Ltx_conversation_allocation
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ltx_value]
 mov rdx,[rsp]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ltx_conversation_allocation
 mov qword ptr [rsp],0
 mov rdi,[rbx+24]
 mov rsi,[rsp+8]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ltx_conversation_allocation
 mov qword ptr [rsp+8],0
 mov rax,[rsp+16]
 mov [r14],rax
 mov qword ptr [rsp+16],0
 xor ebp,ebp
 jmp .Ltx_conversation_cleanup
.Ltx_conversation_read_failed:
 mov ebp,5
 jmp .Ltx_conversation_cleanup
.Ltx_conversation_allocation:
 mov ebp,4
.Ltx_conversation_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov dword ptr [rbx+32],0
.Ltx_conversation_return:
 mov eax,ebp
 add rsp,72
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_create_conversation,.-pg_record_tx_create_conversation
.section .rodata
.Ltx_owner: .asciz "owner"
.Ltx_task_id: .asciz "taskId"
.text
.globl pg_records_prepare_fork
.type pg_records_prepare_fork,@function
pg_records_prepare_fork:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,168
 mov rbx,rdi
 mov r12,rsi
 mov r14,rdx
 mov r13,rcx
 mov r15,r8
 mov ebp,1
 xor eax,eax
 mov [rsp],rax
 mov [rsp+8],rax
 mov [rsp+16],rax
 mov [rsp+24],rax
 mov [rsp+32],rax
 mov [rsp+40],rax
 mov [rsp+112],rax
 test r15,r15
 jz .Lfork_return
 mov qword ptr [r15],0
 test rbx,rbx
 jz .Lfork_return
 cmp qword ptr [rbx+8],0
 je .Lfork_return
 cmp qword ptr [rbx+16],0
 je .Lfork_return
 cmp qword ptr [rbx+24],0
 je .Lfork_return
 mov rax,9007199254740991
 test r12,r12
 jz .Lfork_return
 cmp r12,rax
 ja .Lfork_return
 test r13,r13
 jz .Lfork_return
 cmp r13,rax
 ja .Lfork_return
 test r14,r14
 jz .Lfork_return
 cmp r14,rax
 ja .Lfork_return
 mov qword ptr [rsp+64],0
 mov rdi,[rbx]
 mov rsi,r12
 mov rdx,r14
 lea rcx,[rsp+8]
 lea r8,[rsp+64]
 call [rbx+8]
 test eax,eax
 jnz .Lfork_read_failed
 cmp qword ptr [rsp+8],0
 je .Lfork_read_failed
 mov rax,[rsp+64]
 test rax,rax
 jz .Lfork_read_failed
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lfork_read_failed
 mov [rsp+88],rax
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ltx_conversation_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 test rax,rax
 jz .Lfork_cleanup
 mov [rsp+80],rax
 mov qword ptr [rsp+96],0
 mov qword ptr [rsp+104],0
 mov qword ptr [rsp+56],0
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lfork_allocation
 mov [rsp],rax
.Lfork_scan:
 mov qword ptr [rsp+112],0
 mov qword ptr [rsp+120],0
 mov qword ptr [rsp+128],0
 mov rdi,[rbx]
 lea rsi,[rsp+80]
 lea rdx,[rsp+112]
 call [rbx+16]
 test eax,eax
 jnz .Lfork_read_failed
 cmp dword ptr [rsp+128],1
 ja .Lfork_read_failed
 cmp dword ptr [rsp+132],0
 jne .Lfork_read_failed
 mov rdi,[rsp+112]
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lfork_read_failed
 mov qword ptr [rsp+48],0
.Lfork_each:
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp [rsp+48],rax
 jae .Lfork_page_done
 mov rdi,[rsp+112]
 mov rsi,[rsp+48]
 call json_object_array_get_idx@PLT
 mov [rsp+136],rax
 mov rdi,rax
 lea rsi,[rip+.Lfork_scope]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lfork_kind]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_conversation]
 call .Ls_string
 test eax,eax
 jz .Lfork_next
 mov rdi,[rsp+136]
 lea rsi,[rip+.Lfork_policy]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lfork_asof]
 cmp qword ptr [rsp+56],0
 je .Lfork_policy_selected
 lea rsi,[rip+.Lfork_current]
.Lfork_policy_selected:
 call .Ls_string
 test eax,eax
 jz .Lfork_next
 mov qword ptr [rsp+144],0
 mov rdi,[rbx]
 lea rsi,[rsp+144]
 call [rbx+24]
 test eax,eax
 jnz .Lfork_read_failed
 mov rax,[rsp+144]
 test rax,rax
 jz .Lfork_read_failed
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lfork_read_failed
 mov rdi,[rsp+136]
 lea rsi,[rsp+16]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lfork_allocation
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lfork_created]
 call json_object_object_del@PLT
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lfork_retired]
 call json_object_object_del@PLT
 mov rdi,[rsp+144]
 call json_object_new_int64@PLT
 mov rdi,[rsp+16]
 lea rsi,[rip+.Ltx_id]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lfork_allocation
 call json_object_new_object@PLT
 test rax,rax
 jz .Lfork_allocation
 mov [rsp+40],rax
 lea rdi,[rip+.Ltx_conversation]
 call json_object_new_string@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lfork_kind]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lfork_allocation
 mov rdi,r13
 call json_object_new_int64@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Ltx_conversation_id]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lfork_allocation
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lfork_scope]
 mov rdx,[rsp+40]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lfork_allocation
 mov qword ptr [rsp+40],0
 mov qword ptr [rsp+152],0
.Lfork_duplicate:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 cmp [rsp+152],rax
 jae .Lfork_source
 mov rdi,[rsp]
 mov rsi,[rsp+152]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rip+.Lfork_record]
 call json_object_object_get@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lfork_kind]
 call json_object_object_get@PLT
 mov [rsp+160],rax
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lfork_kind]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,[rsp+160]
 call json_object_equal@PLT
 test eax,eax
 jz .Lfork_duplicate_next
 mov rdi,r14
 lea rsi,[rip+.Lfork_key]
 call json_object_object_get@PLT
 mov [rsp+160],rax
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lfork_key]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,[rsp+160]
 call json_object_equal@PLT
 test eax,eax
 jnz .Lfork_cleanup
.Lfork_duplicate_next:
 inc qword ptr [rsp+152]
 jmp .Lfork_duplicate
.Lfork_source:
 call json_object_new_object@PLT
 test rax,rax
 jz .Lfork_allocation
 mov [rsp+32],rax
 mov rdi,[rsp+136]
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_int64@PLT
 mov rdi,rax
 call json_object_new_int64@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ltx_id]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lfork_allocation
 cmp qword ptr [rsp+56],0
 jne .Lfork_current_point
 mov rdi,[rsp+88]
 call json_object_new_int64@PLT
 jmp .Lfork_attach_point
.Lfork_current_point:
 lea rdi,[rip+.Lfork_current]
 call json_object_new_string@PLT
.Lfork_attach_point:
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lfork_at]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lfork_allocation
 call json_object_new_object@PLT
 test rax,rax
 jz .Lfork_allocation
 mov [rsp+24],rax
 mov rdi,rax
 lea rsi,[rip+.Lfork_record]
 mov rdx,[rsp+16]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lfork_allocation
 mov qword ptr [rsp+16],0
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lfork_source_name]
 mov rdx,[rsp+32]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lfork_allocation
 mov qword ptr [rsp+32],0
 mov rdi,[rsp]
 mov rsi,[rsp+24]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lfork_allocation
 mov qword ptr [rsp+24],0
.Lfork_next:
 inc qword ptr [rsp+48]
 jmp .Lfork_each
.Lfork_page_done:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 cmp dword ptr [rsp+128],0
 je .Lfork_pass_done
 mov rax,[rsp+120]
 cmp dword ptr [rsp+104],0
 je .Lfork_cursor_set
 cmp rax,[rsp+96]
 jle .Lfork_read_failed
.Lfork_cursor_set:
 mov [rsp+96],rax
 mov dword ptr [rsp+104],1
 jmp .Lfork_scan
.Lfork_pass_done:
 cmp qword ptr [rsp+56],0
 jne .Lfork_success
 mov qword ptr [rsp+56],1
 mov [rsp+80],r12
 mov qword ptr [rsp+88],-1
 mov qword ptr [rsp+96],0
 mov qword ptr [rsp+104],0
 jmp .Lfork_scan
.Lfork_success:
 mov rax,[rsp]
 mov [r15],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
 jmp .Lfork_cleanup
.Lfork_read_failed:
 mov ebp,5
 jmp .Lfork_cleanup
.Lfork_allocation:
 mov ebp,4
.Lfork_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+112]
 call json_object_put@PLT
.Lfork_return:
 mov eax,ebp
 add rsp,168
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_records_prepare_fork,.-pg_records_prepare_fork
.section .rodata
.Lfork_scope: .asciz "scope"
.Lfork_kind: .asciz "kind"
.Lfork_key: .asciz "key"
.Lfork_policy: .asciz "fork"
.Lfork_asof: .asciz "asOf"
.Lfork_current: .asciz "current"
.Lfork_created: .asciz "createdAt"
.Lfork_retired: .asciz "retiredAt"
.Lfork_record: .asciz "record"
.Lfork_source_name: .asciz "source"
.Lfork_at: .asciz "at"
