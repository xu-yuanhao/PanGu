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
 call json_object_get_userdata@PLT
 xor edx,edx
 test rax,rax
 jz .Lrecords_copy_ready
 lea rdx,[rip+.Ltask_copy_plain]
.Lrecords_copy_ready:
 mov rdi,r12
 lea rsi,[rsp+8]
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
 cmp dword ptr [rbx+52],0
 jne .Lrecords_first_failure
 mov [rbx+52],r14d
.Lrecords_first_failure:
 mov r14d,[rbx+52]
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
 cmp dword ptr [rbx+52],0
 jne .Lrecords_adopt_return
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
 cmp dword ptr [rbx+44],0
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
 cmp dword ptr [rsp+8],2
 jne .Ltx_read_provider
 mov rdi,rbx
 mov rsi,[rsp+16]
 mov rdx,rsp
 call .Ltask_cached_read
 jmp .Ltx_read_provider_done
.Ltx_read_provider:
 mov rdi,[rbx+8]
 mov esi,[rsp+8]
 mov rdx,[rsp+16]
 mov rcx,rsp
 call [rbx+16]
.Ltx_read_provider_done:
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
 cmp r12d,2
 jne .Ltask_stage_slot_ready
 mov rdi,r13
 lea rsi,[rip+.Ltx_value]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number
 test rax,rax
 jz .Ltx_stage_cleanup
 mov rsi,rax
 mov rdi,rbx
 call .Ltask_slot
 test rax,rax
 jz .Ltx_stage_allocation
.Ltask_stage_slot_ready:
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
 cmp dword ptr [rdi+44],0
 jne .Ltx_discard_return
 push rbx
 mov rbx,rdi
 mov dword ptr [rbx+32],1
 mov dword ptr [rbx+36],1
 mov rdi,[rbx+24]
 mov qword ptr [rbx+24],0
 call pg_record_tx_array_release
 mov dword ptr [rbx+32],0
 xor eax,eax
 pop rbx
.Ltx_discard_return:
 ret
.size pg_record_tx_discard,.-pg_record_tx_discard

.globl pg_record_tx_commit
.type pg_record_tx_commit,@function
pg_record_tx_commit:
 xor esi,esi
 xor edx,edx
 xor ecx,ecx
 jmp .Ltx_commit_common
.globl pg_record_tx_commit_finalized
.type pg_record_tx_commit_finalized,@function
pg_record_tx_commit_finalized:
 xor ecx,ecx
 jmp .Ltx_commit_common
.globl pg_record_tx_commit_routed
.type pg_record_tx_commit_routed,@function
pg_record_tx_commit_routed:
.Ltx_commit_common:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,8
 mov [rsp],rcx
 mov rbx,rdi
 mov r13,rsi
 mov r14,rdx
 mov eax,1
 test rbx,rbx
 jz .Ltx_commit_return
 cmp dword ptr [rbx+32],0
 jne .Ltx_commit_return
 cmp dword ptr [rbx+44],0
 jne .Ltx_commit_return
 mov eax,3
 cmp dword ptr [rbx+36],0
 jne .Ltx_commit_return
 mov eax,1
 cmp qword ptr [rbx+24],0
 je .Ltx_commit_return
 mov dword ptr [rbx+36],1
 mov dword ptr [rbx+32],1
 mov rdi,[rbx+24]
 call .Ltx_copy_conflicts
 test eax,eax
 jnz .Ltx_commit_plan_failed
 mov rdi,rbx
 call .Ltx_validate_owners
 test eax,eax
 jnz .Ltx_commit_plan_failed
 test r13,r13
 jz .Ltx_commit_storage
 mov rdi,r14
 mov rsi,rbx
 call r13
 test eax,eax
 jnz .Ltx_commit_plan_failed
 mov rdi,rbx
 call pg_record_tx_validate_batch
 test eax,eax
 jnz .Ltx_commit_plan_failed
.Ltx_commit_storage:
 cmp qword ptr [rsp],0
 je .Ltx_commit_admit
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 test rax,rax
 jz .Ltx_commit_admit
 mov r12,[rbx]
 cmp dword ptr [r12+48],0
 jne .Ltx_commit_admit
 cmp qword ptr [r12+52],0
 jne .Ltx_commit_admit
 cmp qword ptr [r12+8],0
 je .Ltx_commit_admit
 mov rax,9007199254740991
 cmp [r12+16],rax
 ja .Ltx_commit_admit
 mov rdi,[rbx+24]
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Ltx_commit_admit
 mov dword ptr [r12+48],1
 mov rdi,r14
 mov rsi,rbx
 call [rsp]
 mov dword ptr [r12+48],0
 test eax,eax
 jz .Ltx_commit_admit
 cmp eax,1
 jne .Ltx_commit_plan_failed
 xor r12d,r12d
 jmp .Ltx_commit_consumed
.Ltx_commit_admit:
 mov rdi,[rbx]
 mov rsi,[rbx+24]
 call pg_records_commit
 mov r12d,eax
.Ltx_commit_consumed:
 mov rdi,[rbx+24]
 mov qword ptr [rbx+24],0
 call pg_record_tx_array_release
 mov dword ptr [rbx+32],0
 xor eax,eax
 test r12d,r12d
 jz .Ltx_commit_return
 mov eax,6
 jmp .Ltx_commit_return
.Ltx_commit_plan_failed:
 mov rdi,[rbx+24]
 mov qword ptr [rbx+24],0
 call pg_record_tx_array_release
 mov dword ptr [rbx+32],0
 mov eax,1
.Ltx_commit_return:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_record_tx_commit,.-pg_record_tx_commit
.size pg_record_tx_commit_finalized,.-pg_record_tx_commit_finalized
.size pg_record_tx_commit_routed,.-pg_record_tx_commit_routed
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
.text
/* Stack-local context for preparing a child without mutating the live batch. */
.Lfork_read_bridge:
 mov rax,[rdi+8]
 mov rdi,[rdi]
 test rax,rax
 jz .Lfork_read_bridge_missing
 jmp rax
.Lfork_read_bridge_missing:
 mov eax,2
 ret
.Lfork_mint_bridge:
 mov rax,[rdi+16]
 mov rdi,[rax]
 jmp qword ptr [rax+24]

.globl pg_record_tx_fork_conversation
.type pg_record_tx_fork_conversation,@function
pg_record_tx_fork_conversation:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,184
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov [rsp+144],rcx
 mov r14,r8
 mov r15,r9
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],0
 mov qword ptr [rsp+32],0
 mov qword ptr [rsp+40],0
 pxor xmm0,xmm0
 movups [rsp+64],xmm0
 movups [rsp+80],xmm0
 movups [rsp+96],xmm0
 test r15,r15
 jz .Ltx_fork_return
 mov qword ptr [r15],0
 test rbx,rbx
 jz .Ltx_fork_return
 cmp dword ptr [rbx+32],0
 jne .Ltx_fork_return
 mov ebp,3
 cmp dword ptr [rbx+36],0
 jne .Ltx_fork_return
 mov ebp,1
 cmp qword ptr [rbx+24],0
 je .Ltx_fork_return
 mov dword ptr [rbx+40],1
 test r14,r14
 jz .Ltx_fork_return
 cmp qword ptr [r14+24],0
 je .Ltx_fork_return
 mov rax,9007199254740991
 test r12,r12
 jz .Ltx_fork_return
 cmp r12,rax
 ja .Ltx_fork_return
 test r13,r13
 jz .Ltx_fork_return
 cmp r13,rax
 ja .Ltx_fork_return
 mov dword ptr [rbx+32],1
 mov rax,[rbx+8]
 mov [rsp+112],rax
 mov rax,[rbx+16]
 mov [rsp+120],rax
 mov [rsp+128],r14
 lea rdi,[rsp+64]
 mov rsi,[rbx]
 lea rdx,[rsp+112]
 lea rcx,[rip+.Lfork_read_bridge]
 call pg_record_tx_begin
 test eax,eax
 jnz .Ltx_fork_status
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov [rsp+48],rax
 mov qword ptr [rsp+56],0
.Ltx_fork_copy_prior:
 mov rsi,[rsp+56]
 cmp rsi,[rsp+48]
 jae .Ltx_fork_create
 mov rdi,[rbx+24]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+40],rax
 mov rdi,[rsp+88]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ltx_fork_allocation
 mov qword ptr [rsp+40],0
 inc qword ptr [rsp+56]
 jmp .Ltx_fork_copy_prior
.Ltx_fork_create:
 lea rdi,[rsp+64]
 mov rsi,[rsp+144]
 xor edx,edx
 lea rcx,[rip+.Lfork_mint_bridge]
 mov r8,rsp
 call pg_record_tx_create_conversation
 test eax,eax
 jnz .Ltx_fork_status
 call json_object_new_object@PLT
 test rax,rax
 jz .Ltx_fork_allocation
 mov [rsp+8],rax
 mov rdi,r12
 call json_object_new_int64@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ltx_conversation_id]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Ltx_fork_allocation
 mov rdi,r13
 call json_object_new_int64@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lfork_at]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Ltx_fork_allocation
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_parent]
 mov rdx,[rsp+8]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ltx_fork_allocation
 mov qword ptr [rsp+8],0
 mov rdi,[rsp]
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ltx_fork_allocation
 mov rdi,[rsp+88]
 mov rsi,[rsp+48]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_value]
 mov rdx,[rsp+8]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ltx_fork_allocation
 mov qword ptr [rsp+8],0
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rcx,rax
 mov rdi,r14
 mov rsi,r12
 mov rdx,r13
 lea r8,[rsp+16]
 call pg_records_prepare_fork
 test eax,eax
 jnz .Ltx_fork_status
 mov qword ptr [rsp+56],0
.Ltx_fork_plans:
 mov rdi,[rsp+16]
 call json_object_array_length@PLT
 cmp [rsp+56],rax
 jae .Ltx_fork_validate
 mov rdi,[rsp+16]
 mov rsi,[rsp+56]
 call json_object_array_get_idx@PLT
 mov [rsp+152],rax
 call json_object_new_object@PLT
 test rax,rax
 jz .Ltx_fork_allocation
 mov [rsp+24],rax
 lea rdi,[rip+.Ltx_copy]
 call json_object_new_string@PLT
 mov rdi,[rsp+24]
 lea rsi,[rip+.Ls_type]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Ltx_fork_allocation
 mov rdi,[rsp+152]
 lea rsi,[rip+.Lfork_record]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+40],rax
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lfork_record]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ltx_fork_allocation
 mov qword ptr [rsp+40],0
 mov rdi,[rsp+152]
 lea rsi,[rip+.Lfork_source_name]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+40],rax
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lfork_source_name]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ltx_fork_allocation
 mov qword ptr [rsp+40],0
 mov rdi,[rsp+88]
 mov rsi,[rsp+24]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ltx_fork_allocation
 mov qword ptr [rsp+24],0
 inc qword ptr [rsp+56]
 jmp .Ltx_fork_plans
.Ltx_fork_validate:
 mov rdi,[rsp+88]
 call .Ltx_copy_conflicts
 test eax,eax
 jnz .Ltx_fork_cleanup
 mov rdi,[rbx+24]
 mov rsi,[rsp+88]
 call pg_record_tx_array_transfer
 mov rdi,[rbx+24]
 mov rax,[rsp+88]
 mov [rbx+24],rax
 mov qword ptr [rsp+88],0
 call json_object_put@PLT
 mov rax,[rsp]
 mov [r15],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
 jmp .Ltx_fork_cleanup
.Ltx_fork_allocation:
 mov ebp,4
 jmp .Ltx_fork_cleanup
.Ltx_fork_status:
 mov ebp,eax
.Ltx_fork_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+88]
 call json_object_put@PLT
 mov dword ptr [rbx+32],0
.Ltx_fork_return:
 mov eax,ebp
 add rsp,184
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_fork_conversation,.-pg_record_tx_fork_conversation

/* One typed document command's changed incarnation ID, or zero. */
.Ltx_changed_document:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov rdi,rbx
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov r12,rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_create]
 call .Ls_string
 test eax,eax
 jnz .Ltx_changed_record
 mov rdi,r12
 lea rsi,[rip+.Ltx_copy]
 call .Ls_string
 test eax,eax
 jnz .Ltx_changed_record
 mov rdi,r12
 lea rsi,[rip+.Ltx_change]
 call .Ls_string
 test eax,eax
 jnz .Ltx_changed_direct
 mov rdi,r12
 lea rsi,[rip+.Ltx_retire]
 call .Ls_string
 test eax,eax
 jz .Ltx_changed_return
.Ltx_changed_direct:
 mov rdi,rbx
 jmp .Ltx_changed_id
.Ltx_changed_record:
 mov rdi,rbx
 lea rsi,[rip+.Lfork_record]
 call json_object_object_get@PLT
 mov rdi,rax
.Ltx_changed_id:
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
.Ltx_changed_return:
 add rsp,8
 pop r12
 pop rbx
 ret
.Ltx_copy_conflicts:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,16
 mov rbx,rdi
 call json_object_array_length@PLT
 mov r12,rax
 xor r13d,r13d
.Ltx_conflict_each:
 cmp r13,r12
 jae .Ltx_conflict_good
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_copy]
 call .Ls_string
 test eax,eax
 jz .Ltx_conflict_next
 mov rdi,r14
 lea rsi,[rip+.Lfork_source_name]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov r14,rax
 test r14,r14
 jz .Ltx_conflict_bad
 mov rdi,rbx
 mov rsi,r14
 call .Ltx_source_conflict
 test eax,eax
 jnz .Ltx_conflict_bad
.Ltx_conflict_next:
 inc r13
 jmp .Ltx_conflict_each
.Ltx_conflict_good:
 mov rdi,rbx
 call .Ltx_parent_current_conflict
 jmp .Ltx_conflict_return
.Ltx_conflict_bad:
 mov eax,1
.Ltx_conflict_return:
 add rsp,16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.section .rodata
.Ltx_parent: .asciz "parent"

.text
.Ltx_parent_current_conflict:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,16
 mov rbx,rdi
 call json_object_array_length@PLT
 mov r12,rax
 xor r13d,r13d
.Ltx_parent_each:
 cmp r13,r12
 jae .Ltx_parent_good
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov [rsp],rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_create]
 call .Ls_string
 test eax,eax
 jnz .Ltx_parent_record
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_copy]
 call .Ls_string
 test eax,eax
 jz .Ltx_parent_next
.Ltx_parent_record:
 mov rdi,r14
 lea rsi,[rip+.Lfork_record]
 call json_object_object_get@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lfork_policy]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lfork_current]
 call .Ls_string
 test eax,eax
 jz .Ltx_parent_next
 mov rdi,r14
 lea rsi,[rip+.Lfork_scope]
 call json_object_object_get@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lfork_kind]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_conversation]
 call .Ls_string
 test eax,eax
 jz .Ltx_parent_next
 mov rdi,r14
 lea rsi,[rip+.Ltx_conversation_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov r14,rax
 xor r15d,r15d
.Ltx_parent_find:
 cmp r15,r12
 jae .Ltx_parent_next
 mov rdi,rbx
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov [rsp+8],rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_conversation]
 call .Ls_string
 test eax,eax
 jz .Ltx_parent_find_next
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ltx_value]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_parent]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_conversation_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,r14
 je .Ltx_parent_bad
.Ltx_parent_find_next:
 inc r15
 jmp .Ltx_parent_find
.Ltx_parent_next:
 inc r13
 jmp .Ltx_parent_each
.Ltx_parent_good:
 xor eax,eax
 jmp .Ltx_parent_return
.Ltx_parent_bad:
 mov eax,1
.Ltx_parent_return:
 add rsp,16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.text
.globl pg_record_tx_scan
.type pg_record_tx_scan,@function
pg_record_tx_scan:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov ebp,1
 test r14,r14
 jz .Ltx_scan_return
 mov qword ptr [r14],0
 mov qword ptr [r14+8],0
 mov qword ptr [r14+16],0
 test rbx,rbx
 jz .Ltx_scan_return
 cmp dword ptr [rbx+32],0
 jne .Ltx_scan_return
 mov ebp,3
 cmp dword ptr [rbx+36],0
 jne .Ltx_scan_return
 mov ebp,1
 cmp qword ptr [rbx+24],0
 je .Ltx_scan_return
 mov ebp,2
 cmp dword ptr [rbx+40],0
 jne .Ltx_scan_return
 mov ebp,1
 test r12,r12
 jz .Ltx_scan_return
 test r13,r13
 jz .Ltx_scan_return
 cmp qword ptr [r12+8],0
 je .Ltx_scan_return
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov dword ptr [rbx+32],1
 mov rdi,[r12]
 mov rsi,r13
 mov rdx,rsp
 call [r12+8]
 test eax,eax
 jnz .Ltx_scan_failed
 movups xmm0,[rsp]
 movups [r14],xmm0
 mov rax,[rsp+16]
 mov [r14+16],rax
 xor ebp,ebp
 jmp .Ltx_scan_settled
.Ltx_scan_failed:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov ebp,5
.Ltx_scan_settled:
 mov dword ptr [rbx+32],0
.Ltx_scan_return:
 mov eax,ebp
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_scan,.-pg_record_tx_scan

.globl pg_record_tx_latest_head
.type pg_record_tx_latest_head,@function
pg_record_tx_latest_head:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov ebp,1
 test r14,r14
 jz .Ltx_latest_return
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Ltx_latest_return
 cmp dword ptr [rbx+32],0
 jne .Ltx_latest_return
 mov ebp,3
 cmp dword ptr [rbx+36],0
 jne .Ltx_latest_return
 mov ebp,1
 cmp qword ptr [rbx+24],0
 je .Ltx_latest_return
 mov ebp,2
 cmp dword ptr [rbx+40],0
 jne .Ltx_latest_return
 mov ebp,1
 test r12,r12
 jz .Ltx_latest_return
 cmp qword ptr [r12+16],0
 je .Ltx_latest_return
 mov qword ptr [rsp],0
 mov dword ptr [rbx+32],1
 mov rdi,[r12]
 mov rsi,r13
 mov rdx,-1
 mov rcx,rsp
 call [r12+16]
 test eax,eax
 jnz .Ltx_latest_failed
 mov rax,[rsp]
 mov [r14],rax
 xor ebp,ebp
 jmp .Ltx_latest_settled
.Ltx_latest_failed:
 mov ebp,5
 cmp eax,1
 jne .Ltx_latest_release
 mov ebp,7
.Ltx_latest_release:
 mov rdi,[rsp]
 call json_object_put@PLT
.Ltx_latest_settled:
 mov dword ptr [rbx+32],0
.Ltx_latest_return:
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_latest_head,.-pg_record_tx_latest_head

.text
.globl pg_record_tx_create
.type pg_record_tx_create,@function
pg_record_tx_create:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov ebp,1
 mov qword ptr [rsp],0
 test r14,r14
 jz .Ltx_create_return
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Ltx_create_return
 cmp dword ptr [rbx+32],0
 jne .Ltx_create_return
 mov ebp,3
 cmp dword ptr [rbx+36],0
 jne .Ltx_create_return
 mov ebp,1
 cmp qword ptr [rbx+24],0
 je .Ltx_create_return
 test r12,r12
 jz .Ltx_create_return
 test r13,r13
 jz .Ltx_create_return
 cmp dword ptr [r12+28],0
 jne .Ltx_create_return
 cmp dword ptr [r12+24],1
 ja .Ltx_create_return
 cmp dword ptr [rbx+44],-1
 je .Ltx_create_return
 cmp qword ptr [r12+8],0
 jne .Ltx_create_fork_check
 cmp qword ptr [r12+16],0
 jne .Ltx_create_return
 jmp .Ltx_create_admit
.Ltx_create_fork_check:
 cmp dword ptr [r12+24],0
 jne .Ltx_create_return
 cmp qword ptr [r13+16],0
 je .Ltx_create_return
.Ltx_create_admit:
 inc dword ptr [rbx+44]
 mov rdi,rbx
 cmp qword ptr [r12+8],0
 jne .Ltx_create_fork
 mov rsi,[r12]
 mov edx,[r12+24]
 mov rcx,[r13+8]
 mov r8,rsp
 call pg_record_tx_create_conversation
 jmp .Ltx_create_assembled
.Ltx_create_fork:
 mov rsi,[r12+8]
 mov rdx,[r12+16]
 mov rcx,[r12]
 mov r8,[r13+16]
 mov r9,rsp
 call pg_record_tx_fork_conversation
.Ltx_create_assembled:
 mov ebp,eax
 test eax,eax
 jnz .Ltx_create_complete
 cmp qword ptr [r13+24],0
 je .Ltx_create_success
 mov rdi,[r13]
 mov rsi,rbx
 mov rdx,[rsp]
 call [r13+24]
 test eax,eax
 jz .Ltx_create_success
 mov ebp,8
 jmp .Ltx_create_complete
.Ltx_create_success:
 mov rax,[rsp]
 mov [r14],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
.Ltx_create_complete:
 dec dword ptr [rbx+44]
 mov rdi,[rsp]
 call json_object_put@PLT
.Ltx_create_return:
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_create,.-pg_record_tx_create
.text
/* Latest staged task, otherwise an owned committed read in *owned. */
.Ltx_current_task:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r13],0
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov r14,rax
.Ltx_current_task_each:
 test r14,r14
 jz .Ltx_current_task_read
 dec r14
 mov rdi,[rbx+24]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov r15,rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_task]
 call .Ls_string
 test eax,eax
 jz .Ltx_current_task_each
 mov rdi,r15
 lea rsi,[rip+.Ltx_value]
 call json_object_object_get@PLT
 mov r15,rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,r12
 jne .Ltx_current_task_each
 mov rax,r15
 xor edx,edx
 jmp .Ltx_current_task_return
.Ltx_current_task_read:
 cmp qword ptr [rbx+16],0
 je .Ltx_current_task_error
 mov rdi,rbx
 mov rsi,r12
 mov rdx,r13
 call .Ltask_cached_read
 test eax,eax
 jnz .Ltx_current_task_error
 mov rax,[r13]
 test rax,rax
 jz .Ltx_current_task_error
 xor edx,edx
 jmp .Ltx_current_task_return
.Ltx_current_task_error:
 xor eax,eax
 mov edx,1
.Ltx_current_task_return:
 add rsp,16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret

.Ltx_validate_owners:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,40
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov ebp,1
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov r12,rax
 xor r13d,r13d
.Ltx_owner_each:
 cmp r13,r12
 jae .Ltx_owner_good
 mov rdi,[rbx+24]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov [rsp+16],rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_conversation]
 call .Ls_string
 test eax,eax
 jnz .Ltx_owner_conversation
 mov rdi,[rsp+16]
 lea rsi,[rip+.Ltx_task]
 call .Ls_string
 test eax,eax
 jz .Ltx_owner_next
 mov rdi,r14
 lea rsi,[rip+.Ltx_value]
 call json_object_object_get@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_owner]
 call json_object_object_get@PLT
 test rax,rax
 jz .Ltx_owner_next
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov r15,rax
 mov rdi,r14
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+24],rax
 mov rdi,rbx
 mov rsi,rax
 lea rdx,[rsp]
 call .Ltx_current_task
 test edx,edx
 jnz .Ltx_owner_cleanup
 cmp rax,r14
 jne .Ltx_owner_release_next
 /* Existing tasks retaining an owner are not newly owned work. */
 cmp qword ptr [rbx+16],0
 je .Ltx_owner_cleanup
 mov rdi,[rbx+8]
 mov esi,2
 mov rdx,[rsp+24]
 lea rcx,[rsp+8]
 call [rbx+16]
 test eax,eax
 jz .Ltx_owner_release_next
 cmp eax,1
 jne .Ltx_owner_cleanup
 jmp .Ltx_owner_check
.Ltx_owner_conversation:
 mov rdi,r14
 lea rsi,[rip+.Ltx_value]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_owner]
 call json_object_object_get@PLT
 test rax,rax
 jz .Ltx_owner_next
 mov rdi,rax
 lea rsi,[rip+.Ltx_task_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov r15,rax
.Ltx_owner_check:
 test r15,r15
 jz .Ltx_owner_cleanup
 mov rax,9007199254740991
 cmp r15,rax
 ja .Ltx_owner_cleanup
 mov rdi,[rsp]
 call json_object_put@PLT
 mov qword ptr [rsp],0
 mov rdi,rbx
 mov rsi,r15
 lea rdx,[rsp]
 call .Ltx_current_task
 test edx,edx
 jnz .Ltx_owner_cleanup
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_abort_requested]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Ltx_owner_cleanup
 mov rdi,r14
 lea rsi,[rip+.Ltx_state]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ls_status]
 call json_object_object_get@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_terminal]
 call .Ls_string
 test eax,eax
 jnz .Ltx_owner_cleanup
 mov rdi,r14
 lea rsi,[rip+.Ltx_completing]
 call .Ls_string
 test eax,eax
 jnz .Ltx_owner_cleanup
.Ltx_owner_release_next:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov qword ptr [rsp],0
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov qword ptr [rsp+8],0
.Ltx_owner_next:
 inc r13
 jmp .Ltx_owner_each
.Ltx_owner_good:
 xor ebp,ebp
.Ltx_owner_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,40
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Ltx_abort_requested: .asciz "abortRequested"
.Ltx_state: .asciz "state"
.Ltx_terminal: .asciz "terminal"
.Ltx_completing: .asciz "completing"

.text
.globl pg_record_tx_validate_batch
.type pg_record_tx_validate_batch,@function
pg_record_tx_validate_batch:
 push rbx
 mov rbx,rdi
 mov eax,1
 test rbx,rbx
 jz .Ltx_validate_batch_return
 cmp qword ptr [rbx+24],0
 je .Ltx_validate_batch_return
 mov rdi,[rbx+24]
 call .Ltx_copy_conflicts
 test eax,eax
 jnz .Ltx_validate_batch_return
 mov rdi,rbx
 call .Ltx_validate_owners
.Ltx_validate_batch_return:
 pop rbx
 ret
.size pg_record_tx_validate_batch,.-pg_record_tx_validate_batch
.text
/* Internal Doc acquisition guard: only a staged terminal task blocks access. */
.globl pg_record_tx_check_document_task
.hidden pg_record_tx_check_document_task
.type pg_record_tx_check_document_task,@function
pg_record_tx_check_document_task:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov r13,rax
.Ldocument_task_each:
 test r13,r13
 jz .Ldocument_task_allowed
 dec r13
 mov rdi,[rbx+24]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_task]
 call .Ls_string
 test eax,eax
 jz .Ldocument_task_each
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
 jne .Ldocument_task_each
 mov rdi,r14
 lea rsi,[rip+.Ltx_state]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ls_status]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_terminal]
 call .Ls_string
 jmp .Ldocument_task_return
.Ldocument_task_allowed:
 xor eax,eax
.Ldocument_task_return:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_record_tx_check_document_task,.-pg_record_tx_check_document_task

.text
.globl pg_record_tx_require_document_owner
.hidden pg_record_tx_require_document_owner
.type pg_record_tx_require_document_owner,@function
pg_record_tx_require_document_owner:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov ebp,1
 mov qword ptr [rsp],0
 mov r12,[rsi+32]
 mov eax,[rsi+40]
 test eax,eax
 jz .Ldocument_owner_ok
 cmp eax,2
 jne .Ldocument_owner_conversation
 lea rdx,[rsp]
 mov rsi,r12
 call .Ltx_current_task
 test edx,edx
 jnz .Ldocument_owner_done
 mov rdi,rax
 lea rsi,[rip+.Ltx_state]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ls_status]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_terminal]
 call .Ls_string
 test eax,eax
 jnz .Ldocument_owner_done
 jmp .Ldocument_owner_ok
.Ldocument_owner_conversation:
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov r13,rax
.Ldocument_owner_each:
 test r13,r13
 jz .Ldocument_owner_read
 dec r13
 mov rdi,[rbx+24]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_conversation]
 call .Ls_string
 test eax,eax
 jz .Ldocument_owner_each
 mov rdi,r14
 lea rsi,[rip+.Ltx_value]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,r12
 jne .Ldocument_owner_each
 jmp .Ldocument_owner_ok
.Ldocument_owner_read:
 cmp qword ptr [rbx+16],0
 je .Ldocument_owner_done
 mov rdi,[rbx+8]
 xor esi,esi
 mov rdx,r12
 mov rcx,rsp
 call [rbx+16]
 test eax,eax
 jnz .Ldocument_owner_done
 cmp qword ptr [rsp],0
 je .Ldocument_owner_done
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,r12
 jne .Ldocument_owner_done
.Ldocument_owner_ok:
 xor ebp,ebp
.Ldocument_owner_done:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_require_document_owner,.-pg_record_tx_require_document_owner

.text
/* Visit only the latest staged terminal candidate per Task ID. */
.globl pg_record_tx_terminal_next
.hidden pg_record_tx_terminal_next
.type pg_record_tx_terminal_next,@function
pg_record_tx_terminal_next:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r13],0
 mov qword ptr [rsp],0
 mov ebp,1
.Lterminal_next_each:
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov rsi,[r12]
 cmp rsi,rax
 jae .Lterminal_next_done
 inc qword ptr [r12]
 mov rdi,[rbx+24]
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_task]
 call .Ls_string
 test eax,eax
 jz .Lterminal_next_each
 mov rdi,r14
 lea rsi,[rip+.Ltx_value]
 call json_object_object_get@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_state]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ls_status]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_terminal]
 call .Ls_string
 test eax,eax
 jz .Lterminal_next_each
 mov rdi,r14
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rsi,rax
 mov rdi,rbx
 mov rdx,rsp
 call .Ltx_current_task
 test edx,edx
 jnz .Lterminal_next_done
 cmp rax,r14
 jne .Lterminal_next_each
 mov [r13],r14
 xor ebp,ebp
.Lterminal_next_done:
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
.size pg_record_tx_terminal_next,.-pg_record_tx_terminal_next

.text
.globl pg_record_tx_check_copy_source
.hidden pg_record_tx_check_copy_source
.type pg_record_tx_check_copy_source,@function
pg_record_tx_check_copy_source:
 mov rdi,[rdi+24]
.Ltx_source_conflict:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 call json_object_array_length@PLT
 mov r13,rax
 xor r14d,r14d
.Ltx_source_each:
 cmp r14,r13
 jae .Ltx_source_good
 mov rdi,rbx
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call .Ltx_changed_document
 cmp rax,r12
 je .Ltx_source_bad
 inc r14
 jmp .Ltx_source_each
.Ltx_source_good:
 xor eax,eax
 jmp .Ltx_source_return
.Ltx_source_bad:
 mov eax,1
.Ltx_source_return:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_record_tx_check_copy_source,.-pg_record_tx_check_copy_source
.text
.globl pg_record_tx_submission_by_request
.type pg_record_tx_submission_by_request,@function
pg_record_tx_submission_by_request:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov qword ptr [rsp],0
 mov eax,1
 test r14,r14
 jz .Ltx_request_end
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Ltx_request_end
 cmp dword ptr [rbx+32],0
 jne .Ltx_request_end
 mov eax,3
 cmp dword ptr [rbx+36],0
 jne .Ltx_request_end
 mov eax,1
 cmp qword ptr [rbx+24],0
 je .Ltx_request_end
 mov eax,2
 cmp dword ptr [rbx+40],0
 jne .Ltx_request_end
 mov eax,1
 test r12,r12
 jz .Ltx_request_end
 test r13,r13
 jz .Ltx_request_end
 cmp qword ptr [r12+8],0
 je .Ltx_request_end
 mov dword ptr [rbx+32],1
 mov rdi,[r12]
 mov rsi,r13
 mov rdx,rsp
 call [r12+8]
 mov dword ptr [rbx+32],0
 test eax,eax
 jnz .Ltx_request_error
 mov rax,[rsp]
 mov [r14],rax
 xor eax,eax
 jmp .Ltx_request_end
.Ltx_request_error:
 mov r12d,5
 cmp eax,1
 jne .Ltx_request_release
 mov r12d,7
.Ltx_request_release:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,r12d
.Ltx_request_end:
 add rsp,24
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_record_tx_submission_by_request,.-pg_record_tx_submission_by_request
.section .note.GNU-stack,"",@progbits
.text
.globl pg_record_tx_entry
.type pg_record_tx_entry,@function
pg_record_tx_entry:
 push rbx
 push r12
 push r13
 sub rsp,16
 mov rbx,rdx
 mov r12,rcx
 mov r13,rdi
 mov eax,1
 test r12,r12
 jz .Ltyped_entry_end
 mov qword ptr [r12],0
 test rbx,rbx
 jz .Ltyped_entry_read
 cmp qword ptr [rbx+8],0x7fffffff
 ja .Ltyped_entry_end
 cmp qword ptr [rbx+8],0
 je .Ltyped_entry_read
 cmp qword ptr [rbx],0
 je .Ltyped_entry_end
.Ltyped_entry_read:
 mov rdx,rsi
 mov esi,1
 mov rcx,r12
 call pg_record_tx_read@PLT
 test eax,eax
 jnz .Ltyped_entry_end
 test rbx,rbx
 jz .Ltyped_entry_end
 mov dword ptr [r13+32],1
 mov rdi,[r12]
 lea rsi,[rip+.Ltyped_entry_kind]
 call json_object_object_get@PLT
 mov [rsp],rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,6
 jne .Ltyped_entry_missing
 mov rdi,[rsp]
 call json_object_get_string_len@PLT
 cmp rax,[rbx+8]
 jne .Ltyped_entry_missing
 test rax,rax
 jz .Ltyped_entry_match
 mov rdi,[rsp]
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,[rbx]
 mov rdx,[rbx+8]
 call memcmp@PLT
 test eax,eax
 jnz .Ltyped_entry_missing
.Ltyped_entry_match:
 xor eax,eax
 jmp .Ltyped_entry_unlock
.Ltyped_entry_missing:
 mov rdi,[r12]
 mov qword ptr [r12],0
 call json_object_put@PLT
 mov eax,7
.Ltyped_entry_unlock:
 mov dword ptr [r13+32],0
.Ltyped_entry_end:
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret
.size pg_record_tx_entry,.-pg_record_tx_entry
.section .rodata
.Ltyped_entry_kind: .asciz "kind"
.section .note.GNU-stack,"",@progbits
.text
.globl pg_record_tx_create_submission
.type pg_record_tx_create_submission,@function
pg_record_tx_create_submission:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,80
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rdi,rsp
 xor eax,eax
 mov ecx,10
 rep stosq
 mov ebp,1
 test r14,r14
 jz .Lsubmission_create_end
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Lsubmission_create_end
 cmp dword ptr [rbx+32],0
 jne .Lsubmission_create_end
 mov ebp,3
 cmp dword ptr [rbx+36],0
 jne .Lsubmission_create_end
 mov ebp,1
 cmp qword ptr [rbx+24],0
 je .Lsubmission_create_end
 mov dword ptr [rbx+40],1
 test r13,r13
 jz .Lsubmission_create_end
 mov dword ptr [rbx+32],1
 mov rdi,r12
 lea rsi,[rip+.Ltx_conversation_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number
 test rax,rax
 jz .Lsubmission_create_cleanup
 mov [rsp+64],rax
 mov dword ptr [rsp+72],1
 mov rdi,rbx
 lea rsi,[rsp+32]
 call pg_record_tx_require_document_owner
 test eax,eax
 jnz .Lsubmission_create_cleanup
 mov rdi,[rbx+8]
 lea rsi,[rsp+24]
 call r13
 test eax,eax
 jnz .Lsubmission_create_read_error
 mov rax,[rsp+24]
 test rax,rax
 jz .Lsubmission_create_read_error
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lsubmission_create_read_error
 mov rdi,r12
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lsubmission_create_cleanup
 mov rdi,r12
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lsubmission_create_cleanup
 mov rdi,r12
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lsubmission_create_allocation
 mov rdi,[rsp+24]
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_id]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lsubmission_create_allocation
 mov rdi,[rsp]
 lea rsi,[rsp+16]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lsubmission_create_allocation
 call json_object_new_object@PLT
 test rax,rax
 jz .Lsubmission_create_allocation
 mov [rsp+8],rax
 lea rdi,[rip+.Ltx_submission]
 call json_object_new_string@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ls_type]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lsubmission_create_allocation
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ltx_value]
 mov rdx,[rsp]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lsubmission_create_allocation
 mov qword ptr [rsp],0
 mov rdi,[rbx+24]
 mov rsi,[rsp+8]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lsubmission_create_allocation
 mov qword ptr [rsp+8],0
 mov rax,[rsp+16]
 mov [r14],rax
 mov qword ptr [rsp+16],0
 xor ebp,ebp
 jmp .Lsubmission_create_cleanup
.Lsubmission_create_read_error:
 mov ebp,5
 jmp .Lsubmission_create_cleanup
.Lsubmission_create_allocation:
 mov ebp,4
.Lsubmission_create_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov dword ptr [rbx+32],0
.Lsubmission_create_end:
 mov eax,ebp
 add rsp,80
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_create_submission,.-pg_record_tx_create_submission
.section .note.GNU-stack,"",@progbits
.text
.globl pg_record_tx_staged_tasks
.type pg_record_tx_staged_tasks,@function
pg_record_tx_staged_tasks:
 mov edx,2
 jmp .Lcandidates
.globl pg_record_tx_staged_conversations
.type pg_record_tx_staged_conversations,@function
pg_record_tx_staged_conversations:
 xor edx,edx
.Lcandidates:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,56
 mov rbx,rdi
 mov r12,rsi
 mov r13d,edx
 mov rdi,rsp
 xor eax,eax
 mov ecx,7
 rep stosq
 mov ebp,1
 test r12,r12
 jz .Lcandidates_end
 mov qword ptr [r12],0
 test rbx,rbx
 jz .Lcandidates_end
 cmp dword ptr [rbx+32],0
 jne .Lcandidates_end
 mov ebp,3
 cmp dword ptr [rbx+36],0
 jne .Lcandidates_end
 mov ebp,1
 cmp qword ptr [rbx+24],0
 je .Lcandidates_end
 mov dword ptr [rbx+32],1
 mov ebp,4
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lcandidates_cleanup
 mov [rsp],rax
 test r13d,r13d
 jz .Lcandidates_start
 mov edi,16
 xor esi,esi
 call lh_kptr_table_new@PLT
 test rax,rax
 jz .Lcandidates_cleanup
 mov [rsp+8],rax
.Lcandidates_start:
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov r15,rax
 xor r14d,r14d
.Lcandidates_each:
 cmp r14,r15
 jae .Lcandidates_copy
 mov rdi,[rbx+24]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov [rsp+24],rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 lea rsi,[rip+.Ltx_conversation]
 test r13d,r13d
 jz .Lcandidates_name
 lea rsi,[rip+.Ltx_task]
.Lcandidates_name:
 mov rdi,rax
 call .Ls_string
 test eax,eax
 jz .Lcandidates_next
 mov rdi,[rsp+24]
 lea rsi,[rip+.Ltx_value]
 call json_object_object_get@PLT
 mov [rsp+24],rax
 test r13d,r13d
 jz .Lcandidates_append
 mov rdi,rax
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number
 test rax,rax
 jz .Lcandidates_invalid
 mov [rsp+40],rax
 mov rdi,[rsp+8]
 mov rsi,rax
 lea rdx,[rsp+32]
 call lh_table_lookup_ex@PLT
 test eax,eax
 jz .Lcandidates_first
 mov rsi,[rsp+32]
 dec rsi
 mov rdi,[rsp+24]
 call json_object_get@PLT
 mov [rsp+48],rax
 mov rdi,[rsp]
 mov rsi,[rsp+32]
 dec rsi
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Lcandidates_cleanup
 mov qword ptr [rsp+48],0
 jmp .Lcandidates_next
.Lcandidates_first:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 lea rdx,[rax+1]
 mov rdi,[rsp+8]
 mov rsi,[rsp+40]
 call lh_table_insert@PLT
 test eax,eax
 jnz .Lcandidates_cleanup
.Lcandidates_append:
 mov rdi,[rsp+24]
 call json_object_get@PLT
 mov [rsp+48],rax
 mov rdi,[rsp]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lcandidates_cleanup
 mov qword ptr [rsp+48],0
.Lcandidates_next:
 inc r14
 jmp .Lcandidates_each
.Lcandidates_copy:
 test r13d,r13d
 jz .Lcandidates_order_ready
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,[rsp+8]
 call .Ltask_order_candidates
 test rax,rax
 jz .Lcandidates_cleanup
 mov r13,rax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov [rsp],r13
.Lcandidates_order_ready:
 mov rdi,[rsp]
 lea rsi,[rsp+16]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lcandidates_cleanup
 mov rax,[rsp+16]
 mov [r12],rax
 mov qword ptr [rsp+16],0
 xor ebp,ebp
 jmp .Lcandidates_cleanup
.Lcandidates_invalid:
 mov ebp,1
.Lcandidates_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 test rdi,rdi
 jz .Lcandidates_unlocked
 call lh_table_free@PLT
.Lcandidates_unlocked:
 mov dword ptr [rbx+32],0
.Lcandidates_end:
 mov eax,ebp
 add rsp,56
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_staged_tasks,.-pg_record_tx_staged_tasks
.size pg_record_tx_staged_conversations,.-pg_record_tx_staged_conversations
.section .note.GNU-stack,"",@progbits
.text
/* The candidate JSON array owns ordered Task read metadata as userdata. */
.Ltask_slot:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 xor r14d,r14d
 xor ebp,ebp
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov rdi,[rbx+24]
 call json_object_get_userdata@PLT
 mov r13,rax
 test rax,rax
 jnz .Lslot_index_ready
 call json_object_new_object@PLT
 test rax,rax
 jz .Lslot_done
 mov r13,rax
 mov rdi,[rbx+24]
 mov rsi,rax
 xor edx,edx
 call json_object_set_userdata@PLT
.Lslot_index_ready:
 mov rdi,r12
 call json_object_new_int64@PLT
 test rax,rax
 jz .Lslot_done
 mov [rsp],rax
 mov rdi,rax
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Lslot_done
 mov r12,rax
 mov rdi,r13
 mov rsi,rax
 call json_object_object_get@PLT
 test rax,rax
 jz .Lslot_create
 mov rbp,rax
 jmp .Lslot_done
.Lslot_create:
 mov edi,2
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lslot_done
 mov r14,rax
 xor edi,edi
 call json_object_new_int64@PLT
 test rax,rax
 jz .Lslot_done
 mov [rsp+8],rax
 mov rdi,r14
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lslot_done
 mov qword ptr [rsp+8],0
 mov rdi,r14
 xor esi,esi
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lslot_done
 mov rdi,r13
 mov rsi,r12
 mov rdx,r14
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lslot_done
 mov rbp,r14
 xor r14d,r14d
.Lslot_done:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,r14
 call json_object_put@PLT
 mov rax,rbp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Read once per ID; cache missing and provider errors as well as successful
   committed records. Returned records are detached from the retained cache. */
.Ltask_cached_read:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r13],0
 mov qword ptr [rsp],0
 mov ebp,2
 call .Ltask_slot
 test rax,rax
 jz .Ltask_read_end
 mov r14,rax
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov [rsp+8],rax
 mov rdi,rax
 call json_object_get_int@PLT
 test eax,eax
 jnz .Ltask_read_status
 mov rdi,[rbx+8]
 mov esi,2
 mov rdx,r12
 mov rcx,rsp
 call [rbx+16]
 test eax,eax
 jz .Ltask_read_success
 cmp eax,1
 je .Ltask_read_missing
 mov ebp,3
 jmp .Ltask_read_record_status
.Ltask_read_missing:
 mov ebp,2
 jmp .Ltask_read_record_status
.Ltask_read_success:
 mov rdi,r14
 mov esi,1
 mov rdx,[rsp]
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Ltask_read_end
 mov qword ptr [rsp],0
 mov ebp,1
.Ltask_read_record_status:
 mov rdi,[rsp+8]
 mov esi,ebp
 call json_object_set_int@PLT
 mov eax,ebp
.Ltask_read_status:
 dec eax
 mov ebp,eax
 test eax,eax
 jnz .Ltask_read_end
 mov rdi,r14
 mov esi,1
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov rsi,r13
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jz .Ltask_read_end
 mov rdi,[r13]
 call json_object_put@PLT
 mov qword ptr [r13],0
 mov ebp,2
.Ltask_read_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .note.GNU-stack,"",@progbits

.text
.Ltask_copy_plain:
 sub rsp,8
 call json_c_shallow_copy_default@PLT
 add rsp,8
 test eax,eax
 js .Ltask_copy_plain_end
 mov eax,2
.Ltask_copy_plain_end:
 ret
.section .note.GNU-stack,"",@progbits

.text
.globl pg_record_tx_array_release
.hidden pg_record_tx_array_release
pg_record_tx_array_release:
 test rdi,rdi
 jz .Larray_release_end
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 call json_object_get_userdata@PLT
 mov r12,rax
 mov rdi,rbx
 xor esi,esi
 xor edx,edx
 call json_object_set_userdata@PLT
 mov rdi,r12
 call json_object_put@PLT
 mov rdi,rbx
 call json_object_put@PLT
 add rsp,8
 pop r12
 pop rbx
.Larray_release_end:
 ret
.globl pg_record_tx_array_transfer
.hidden pg_record_tx_array_transfer
pg_record_tx_array_transfer:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 call json_object_get_userdata@PLT
 mov r13,rax
 mov rdi,rbx
 xor esi,esi
 xor edx,edx
 call json_object_set_userdata@PLT
 mov rdi,r12
 mov rsi,r13
 xor edx,edx
 call json_object_set_userdata@PLT
 pop r13
 pop r12
 pop rbx
 ret
.section .note.GNU-stack,"",@progbits
.text
.Ltask_order_candidates:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rsi
 mov r12,rdx
 xor ebp,ebp
 mov qword ptr [rsp+24],0
 mov rdi,[rdi+24]
 call json_object_get_userdata@PLT
 test rax,rax
 jnz .Ltask_order_map
 mov rdi,rbx
 call json_object_get@PLT
 mov rbp,rax
 jmp .Ltask_order_end
.Ltask_order_map:
 mov r14,rax
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Ltask_order_end
 mov r13,rax
 mov rdi,r14
 call json_object_iter_begin@PLT
 mov [rsp],rax
 mov rdi,r14
 call json_object_iter_end@PLT
 mov [rsp+8],rax
.Ltask_order_each:
 mov rdi,rsp
 lea rsi,[rsp+8]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Ltask_order_success
 mov rdi,rsp
 call json_object_iter_peek_name@PLT
 mov rdi,rax
 xor esi,esi
 mov edx,10
 call strtoull@PLT
 mov rdi,r12
 mov rsi,rax
 lea rdx,[rsp+16]
 call lh_table_lookup_ex@PLT
 test eax,eax
 jz .Ltask_order_next
 mov rdi,rbx
 mov rsi,[rsp+16]
 dec rsi
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+24],rax
 mov rdi,r13
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ltask_order_failed
 mov qword ptr [rsp+24],0
.Ltask_order_next:
 mov rdi,rsp
 call json_object_iter_next@PLT
 jmp .Ltask_order_each
.Ltask_order_failed:
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,r13
 call json_object_put@PLT
 jmp .Ltask_order_end
.Ltask_order_success:
 mov rbp,r13
.Ltask_order_end:
 mov rax,rbp
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .note.GNU-stack,"",@progbits
.text
.globl pg_record_tx_set_task
.type pg_record_tx_set_task,@function
pg_record_tx_set_task:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,88
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsp
 xor eax,eax
 mov r8,rcx
 mov ecx,11
 rep stosq
 mov [rsp],rdx
 mov [rsp+8],r8
 mov qword ptr [rsp+40],-1
 mov ebp,1
 test rbx,rbx
 jz .Lset_task_end
 cmp dword ptr [rbx+32],0
 jne .Lset_task_end
 mov ebp,3
 cmp dword ptr [rbx+36],0
 jne .Lset_task_end
 mov ebp,1
 cmp qword ptr [rbx+24],0
 je .Lset_task_end
 mov dword ptr [rbx+40],1
 cmp qword ptr [rsp+8],0
 je .Lset_task_end
 mov dword ptr [rbx+32],1
 mov rdi,r12
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lset_task_cleanup
 mov rdi,r12
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number
 test rax,rax
 jz .Lset_task_cleanup
 mov [rsp+32],rax
 mov rdi,rbx
 mov rsi,rax
 call .Ltask_slot
 test rax,rax
 jz .Lset_task_allocation
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov r13,rax
.Lset_task_find:
 test r13,r13
 jz .Lset_task_found
 dec r13
 mov rdi,[rbx+24]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_task]
 call .Ls_string
 test eax,eax
 jz .Lset_task_find
 mov rdi,r14
 lea rsi,[rip+.Ltx_value]
 call json_object_object_get@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number
 cmp rax,[rsp+32]
 jne .Lset_task_find
 mov [rsp+48],r14
 mov [rsp+40],r13
 mov rdi,r14
 lea rsi,[rip+.Ltx_state]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ls_status]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_terminal]
 call .Ls_string
 test eax,eax
 jnz .Lset_task_cleanup
 mov rdi,r14
 lea rsi,[rip+.Ltx_conversation_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number
 mov r15,rax
 mov rdi,r12
 lea rsi,[rip+.Ltx_conversation_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number
 cmp rax,r15
 jne .Lset_task_cleanup
.Lset_task_found:
 mov rdi,r12
 lea rsi,[rip+.Ltx_state]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ls_status]
 call json_object_object_get@PLT
 mov [rsp+56],rax
 mov rdi,r12
 lea rsi,[rsp+16]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lset_task_allocation
 xor r13d,r13d
.Lset_task_stamp:
 cmp r13d,2
 jae .Lset_task_write
 lea r14,[rip+.Ltask_started]
 lea r15,[rip+.Ltask_running]
 test r13d,r13d
 jz .Lset_task_stamp_key
 lea r14,[rip+.Ltask_ended]
 lea r15,[rip+.Ltx_terminal]
.Lset_task_stamp_key:
 mov rdi,[rsp+48]
 mov rsi,r14
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lset_task_stamp_copy
 mov rdi,r12
 mov rsi,r14
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lset_task_stamp_next
 mov rdi,[rsp+56]
 mov rsi,r15
 call .Ls_string
 test eax,eax
 jz .Lset_task_stamp_next
 mov rdi,[rsp]
 call [rsp+8]
 call json_object_new_double@PLT
 test rax,rax
 jz .Lset_task_allocation
 mov [rsp+72],rax
 jmp .Lset_task_stamp_attach
.Lset_task_stamp_copy:
 mov rdi,rax
 lea rsi,[rsp+72]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lset_task_allocation
.Lset_task_stamp_attach:
 mov rdi,[rsp+16]
 mov rsi,r14
 mov rdx,[rsp+72]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lset_task_allocation
 mov qword ptr [rsp+72],0
.Lset_task_stamp_next:
 inc r13d
 jmp .Lset_task_stamp
.Lset_task_write:
 mov rdi,[rsp+16]
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lset_task_cleanup
 call json_object_new_object@PLT
 test rax,rax
 jz .Lset_task_allocation
 mov [rsp+24],rax
 lea rdi,[rip+.Ltx_task]
 call json_object_new_string@PLT
 mov rdi,[rsp+24]
 lea rsi,[rip+.Ls_type]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lset_task_allocation
 mov rdi,[rsp+24]
 lea rsi,[rip+.Ltx_value]
 mov rdx,[rsp+16]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lset_task_allocation
 mov qword ptr [rsp+16],0
 cmp qword ptr [rsp+40],-1
 je .Lset_task_append
 mov rdi,[rbx+24]
 mov rsi,[rsp+40]
 mov rdx,[rsp+24]
 call json_object_array_put_idx@PLT
 jmp .Lset_task_installed
.Lset_task_append:
 mov rdi,[rbx+24]
 mov rsi,[rsp+24]
 call json_object_array_add@PLT
.Lset_task_installed:
 test eax,eax
 jnz .Lset_task_allocation
 mov qword ptr [rsp+24],0
 xor ebp,ebp
 jmp .Lset_task_cleanup
.Lset_task_allocation:
 mov ebp,4
.Lset_task_cleanup:
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+72]
 call json_object_put@PLT
 mov dword ptr [rbx+32],0
.Lset_task_end:
 mov eax,ebp
 add rsp,88
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_set_task,.-pg_record_tx_set_task
.section .rodata
.Ltask_started: .asciz "startedAt"
.Ltask_ended: .asciz "endedAt"
.Ltask_running: .asciz "running"
.section .note.GNU-stack,"",@progbits
.text
.globl pg_record_tx_task_options
.type pg_record_tx_task_options,@function
pg_record_tx_task_options:
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
 mov rdi,rsp
 xor eax,eax
 mov ecx,9
 rep stosq
 mov ebp,1
 test r13,r13
 jz .Ltask_options_end
 mov qword ptr [r13],0
 test rbx,rbx
 jz .Ltask_options_end
 cmp dword ptr [rbx+32],0
 jne .Ltask_options_end
 mov ebp,3
 cmp dword ptr [rbx+36],0
 jne .Ltask_options_end
 mov ebp,1
 cmp qword ptr [rbx+24],0
 je .Ltask_options_end
 mov dword ptr [rbx+40],1
 mov dword ptr [rbx+32],1
 mov rdi,r12
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ltask_options_cleanup
 mov rdi,r12
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Ltask_options_cleanup
 mov rdi,r12
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ltask_options_allocation
 mov rdi,r12
 lea rsi,[rip+.Ltask_submission_ids]
 lea rdx,[rsp+8]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Ltask_options_success
 mov rdi,[rsp+8]
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ltask_options_cleanup
 mov rdi,[rsp]
 lea rsi,[rip+.Ltask_trace]
 call json_object_object_del@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Ltask_links]
 call json_object_object_del@PLT
 mov rdi,[rsp+8]
 call json_object_array_length@PLT
 mov [rsp+16],rax
 xor r14d,r14d
.Ltask_options_each:
 cmp r14,[rsp+16]
 jae .Ltask_options_success
 mov rdi,[rsp+8]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call pg_document_safe_number
 test rax,rax
 jz .Ltask_options_cleanup
 mov [rsp+24],rax
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov r15,rax
.Ltask_options_find:
 test r15,r15
 jz .Ltask_options_read
 dec r15
 mov rdi,[rbx+24]
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov [rsp+32],rax
 mov rdi,rax
 lea rsi,[rip+.Ls_type]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltx_submission]
 call .Ls_string
 test eax,eax
 jz .Ltask_options_find
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ltx_value]
 call json_object_object_get@PLT
 mov [rsp+32],rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number
 cmp rax,[rsp+24]
 jne .Ltask_options_find
 jmp .Ltask_options_trace
.Ltask_options_read:
 cmp qword ptr [rbx+16],0
 je .Ltask_options_read_error
 mov rdi,[rbx+8]
 mov esi,3
 mov rdx,[rsp+24]
 lea rcx,[rsp+40]
 call [rbx+16]
 test eax,eax
 jz .Ltask_options_read_ok
 cmp eax,1
 jne .Ltask_options_read_error
 mov qword ptr [rsp+32],0
 jmp .Ltask_options_trace
.Ltask_options_read_ok:
 mov rax,[rsp+40]
 mov [rsp+32],rax
.Ltask_options_trace:
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ltask_trace]
 lea rdx,[rsp+48]
 call json_object_object_get_ex@PLT
 test r14,r14
 jnz .Ltask_options_link
 cmp qword ptr [rsp+48],0
 je .Ltask_options_root
 mov rdi,[rsp+48]
 lea rsi,[rsp+56]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ltask_options_allocation
 jmp .Ltask_options_set_trace
.Ltask_options_root:
 lea rdi,[rip+.Ltask_root]
 call json_object_new_string@PLT
 test rax,rax
 jz .Ltask_options_allocation
 mov [rsp+56],rax
.Ltask_options_set_trace:
 mov rdi,[rsp]
 lea rsi,[rip+.Ltask_trace]
 mov rdx,[rsp+56]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ltask_options_allocation
 mov qword ptr [rsp+56],0
 jmp .Ltask_options_next
.Ltask_options_link:
 test eax,eax
 jz .Ltask_options_next
 cmp qword ptr [rsp+64],0
 jne .Ltask_options_link_ready
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Ltask_options_allocation
 mov [rsp+64],rax
.Ltask_options_link_ready:
 cmp qword ptr [rsp+48],0
 je .Ltask_options_link_null
 mov rdi,[rsp+48]
 lea rsi,[rsp+56]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ltask_options_allocation
.Ltask_options_link_null:
 mov rdi,[rsp+64]
 mov rsi,[rsp+56]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ltask_options_allocation
 mov qword ptr [rsp+56],0
.Ltask_options_next:
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov qword ptr [rsp+40],0
 mov qword ptr [rsp+48],0
 inc r14
 jmp .Ltask_options_each
.Ltask_options_success:
 cmp qword ptr [rsp+64],0
 je .Ltask_options_return_owned
 mov rdi,[rsp]
 lea rsi,[rip+.Ltask_links]
 mov rdx,[rsp+64]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ltask_options_allocation
 mov qword ptr [rsp+64],0
.Ltask_options_return_owned:
 mov rax,[rsp]
 mov [r13],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
 jmp .Ltask_options_cleanup
.Ltask_options_read_error:
 mov ebp,5
 jmp .Ltask_options_cleanup
.Ltask_options_allocation:
 mov ebp,4
.Ltask_options_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov dword ptr [rbx+32],0
.Ltask_options_end:
 mov eax,ebp
 add rsp,72
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_task_options,.-pg_record_tx_task_options
.section .rodata
.Ltask_submission_ids: .asciz "submissionIds"
.Ltask_trace: .asciz "trace"
.Ltask_links: .asciz "links"
.Ltask_root: .asciz "root"
.section .note.GNU-stack,"",@progbits
.text
/* A null JSON value is valid; clone/attach consumes only its private copy. */
.Lcreate_task_copy:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 test rdx,rdx
 jz .Lcreate_task_copy_attach
 mov rdi,rdx
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lcreate_task_copy_bad
.Lcreate_task_copy_attach:
 mov rdi,rbx
 mov rsi,r12
 mov rdx,[rsp]
 call json_object_object_add@PLT
 test eax,eax
 jz .Lcreate_task_copy_end
.Lcreate_task_copy_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,1
.Lcreate_task_copy_end:
 add rsp,24
 pop r12
 pop rbx
 ret
.globl pg_record_tx_create_task
.type pg_record_tx_create_task,@function
pg_record_tx_create_task:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,136
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,r8
 mov r15,r9
 mov [rsp+128],rcx
 mov rdi,rsp
 xor eax,eax
 mov ecx,16
 rep stosq
 mov ebp,1
 test r15,r15
 jz .Lcreate_task_end
 mov qword ptr [r15],0
 test r14,r14
 jz .Lcreate_task_end
 cmp qword ptr [r14+8],0
 je .Lcreate_task_end
 cmp qword ptr [r14+16],0
 je .Lcreate_task_end
 mov rdi,rbx
 mov rsi,[rsp+128]
 mov rdx,rsp
 call pg_record_tx_task_options
 mov ebp,eax
 test eax,eax
 jnz .Lcreate_task_end
 mov dword ptr [rbx+32],1
 mov ebp,1
 mov rdi,r12
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lcreate_task_cleanup
 mov rdi,[rsp]
 lea rsi,[rip+.Lcreate_ownership]
 call json_object_object_get@PLT
 lea rsi,[rip+.Lcreate_kind]
 mov rdi,rax
 call json_object_object_get@PLT
 mov [rsp+120],rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_task]
 call .Ls_string
 test eax,eax
 jz .Lcreate_task_conversation_owned
 mov rdi,[rsp]
 lea rsi,[rip+.Lcreate_ownership]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcreate_task_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number
 test rax,rax
 jz .Lcreate_task_cleanup
 mov rdi,rbx
 mov rsi,rax
 lea rdx,[rsp+8]
 call .Ltx_current_task
 test edx,edx
 jnz .Lcreate_task_cleanup
 mov [rsp+112],rax
 mov rdi,rax
 lea rsi,[rip+.Ltx_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number
 mov [rsp+56],rax
 mov rdi,[rsp+112]
 lea rsi,[rip+.Ltx_conversation_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number
 mov [rsp+96],rax
 mov rdi,[rsp]
 lea rsi,[rip+.Lcreate_background]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_conversation_id]
 lea rdx,[rsp+120]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lcreate_task_require
 mov rdi,[rsp+120]
 call pg_document_safe_number
 cmp rax,[rsp+96]
 jne .Lcreate_task_cleanup
 jmp .Lcreate_task_require
.Lcreate_task_conversation_owned:
 mov rdi,[rsp+120]
 lea rsi,[rip+.Ltx_conversation]
 call .Ls_string
 test eax,eax
 jz .Lcreate_task_cleanup
 mov rax,[r14+32]
 mov [rsp+96],rax
 mov rdi,[rsp]
 lea rsi,[rip+.Ltx_conversation_id]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lcreate_task_require
 mov rdi,rax
 call pg_document_safe_number
 mov [rsp+96],rax
.Lcreate_task_require:
 cmp qword ptr [rsp+96],0
 je .Lcreate_task_cleanup
 mov dword ptr [rsp+104],1
 mov rdi,rbx
 lea rsi,[rsp+64]
 call pg_record_tx_require_document_owner
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rdi,[r14]
 mov rsi,r13
 lea rdx,[rsp+16]
 call [r14+8]
 mov ebp,8
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rdi,[r14]
 lea rsi,[rsp+48]
 call [r14+16]
 mov ebp,5
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rax,[rsp+48]
 test rax,rax
 jz .Lcreate_task_cleanup
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lcreate_task_cleanup
 mov rdi,[rsp]
 lea rsi,[rip+.Ltask_trace]
 call json_object_object_get@PLT
 mov [rsp+120],rax
 mov rdi,rax
 lea rsi,[rip+.Ltask_root]
 call .Ls_string
 mov [rsp+80],eax
 test eax,eax
 jnz .Lcreate_task_record
 cmp qword ptr [rsp+120],0
 jne .Lcreate_task_record
 cmp qword ptr [r14+24],0
 je .Lcreate_task_record
 mov rdi,[r14]
 lea rsi,[rsp+24]
 call [r14+24]
 mov ebp,8
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rax,[rsp+24]
 mov [rsp+120],rax
.Lcreate_task_record:
 mov ebp,4
 call json_object_new_object@PLT
 test rax,rax
 jz .Lcreate_task_cleanup
 mov [rsp+32],rax
 mov rdi,[rsp+48]
 call json_object_new_int64@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ltx_id]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rdi,[rsp+96]
 call json_object_new_int64@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ltx_conversation_id]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rdi,r12
 lea rsi,[rip+.Lcreate_name]
 call json_object_object_get@PLT
 mov rdx,rax
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lcreate_kind]
 call .Lcreate_task_copy
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rdi,r12
 lea rsi,[rip+.Lcreate_version]
 call json_object_object_get@PLT
 mov rdx,rax
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lcreate_version]
 call .Lcreate_task_copy
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lcreate_input]
 mov rdx,r13
 call .Lcreate_task_copy
 test eax,eax
 jnz .Lcreate_task_cleanup
 cmp dword ptr [rsp+80],0
 jne .Lcreate_task_links
 cmp qword ptr [rsp+120],0
 je .Lcreate_task_links
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ltask_trace]
 mov rdx,[rsp+120]
 call .Lcreate_task_copy
 test eax,eax
 jnz .Lcreate_task_cleanup
.Lcreate_task_links:
 mov rdi,[rsp]
 lea rsi,[rip+.Ltask_links]
 lea rdx,[rsp+120]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lcreate_task_owner
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ltask_links]
 mov rdx,[rsp+120]
 call .Lcreate_task_copy
 test eax,eax
 jnz .Lcreate_task_cleanup
.Lcreate_task_owner:
 cmp qword ptr [rsp+56],0
 je .Lcreate_task_background
 mov rdi,[rsp+56]
 call json_object_new_int64@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ltx_owner]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lcreate_task_cleanup
.Lcreate_task_background:
 mov rdi,[rsp]
 lea rsi,[rip+.Lcreate_background]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov edi,eax
 call json_object_new_boolean@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lcreate_background]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lcreate_task_cleanup
 xor edi,edi
 call json_object_new_boolean@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lcreate_abort]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rdi,[rsp]
 lea rsi,[rip+.Lcreate_abandon]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lcreate_task_state
 mov edi,1
 call json_object_new_boolean@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lcreate_abandon]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lcreate_task_cleanup
.Lcreate_task_state:
 call json_object_new_object@PLT
 test rax,rax
 jz .Lcreate_task_cleanup
 mov [rsp+40],rax
 lea rdi,[rip+.Lcreate_pending]
 call json_object_new_string@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Ls_status]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lcreate_checkpoint]
 mov rdx,[rsp+16]
 call .Lcreate_task_copy
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rdi,[rsp+32]
 lea rsi,[rip+.Ltx_state]
 mov rdx,[rsp+40]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov qword ptr [rsp+40],0
 mov rdi,[rsp+32]
 call pg_document_value_valid@PLT
 mov ebp,1
 test eax,eax
 jz .Lcreate_task_cleanup
 call json_object_new_object@PLT
 mov ebp,4
 test rax,rax
 jz .Lcreate_task_cleanup
 mov [rsp+40],rax
 lea rdi,[rip+.Ltx_task]
 call json_object_new_string@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Ls_type]
 call .Ltx_entry_attach
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rdi,[rsp+40]
 lea rsi,[rip+.Ltx_value]
 mov rdx,[rsp+32]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov qword ptr [rsp+32],0
 mov dword ptr [rbx+32],0
 mov rdi,rbx
 mov esi,2
 mov rdx,[rsp+40]
 call pg_record_tx_stage
 mov ebp,eax
 test eax,eax
 jnz .Lcreate_task_cleanup
 mov rax,[rsp+48]
 mov [r15],rax
.Lcreate_task_cleanup:
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
 mov dword ptr [rbx+32],0
.Lcreate_task_end:
 mov eax,ebp
 add rsp,136
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_tx_create_task,.-pg_record_tx_create_task
.section .rodata
.Lcreate_ownership: .asciz "ownership"
.Lcreate_task_id: .asciz "taskId"
.Lcreate_kind: .asciz "kind"
.Lcreate_name: .asciz "name"
.Lcreate_version: .asciz "version"
.Lcreate_input: .asciz "input"
.Lcreate_background: .asciz "background"
.Lcreate_abort: .asciz "abortRequested"
.Lcreate_abandon: .asciz "abandonOnRestart"
.Lcreate_pending: .asciz "pending"
.Lcreate_checkpoint: .asciz "checkpoint"
.section .note.GNU-stack,"",@progbits
