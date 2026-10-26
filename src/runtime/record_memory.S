.intel_syntax noprefix
.text
.globl pg_record_memory_open
.type pg_record_memory_open,@function
pg_record_memory_open:
 mov eax,2
 test rdi,rdi
 jz .Lmemory_open_return
 cmp dword ptr [rdi+40],0
 jne .Lmemory_open_return
 cmp qword ptr [rdi+24],0
 je .Lmemory_open_fresh
 cmp dword ptr [rdi+44],0
 je .Lmemory_open_return
.Lmemory_open_fresh:
 mov qword ptr [rdi],0
 mov qword ptr [rdi+8],0
 mov qword ptr [rdi+16],0
 mov qword ptr [rdi+24],2
 mov qword ptr [rdi+32],1
 mov qword ptr [rdi+40],0
 xor eax,eax
.Lmemory_open_return:
 ret
.size pg_record_memory_open,.-pg_record_memory_open

.globl pg_record_memory_storage
.type pg_record_memory_storage,@function
pg_record_memory_storage:
 mov rax,rdi
 lea rdx,[rip+.Lmemory_commit]
 ret
.size pg_record_memory_storage,.-pg_record_memory_storage

/* Sorted global ID lower bound. */
.Lmemory_lower:
 xor edx,edx
 mov rcx,[rdi+8]
 mov r10,[rdi]
.Lmemory_lower_loop:
 cmp rdx,rcx
 jae .Lmemory_lower_done
 mov r9,rcx
 sub r9,rdx
 shr r9,1
 add r9,rdx
 mov rax,r9
 shl rax,5
 cmp [r10+rax],rsi
 jae .Lmemory_lower_left
 lea rdx,[r9+1]
 jmp .Lmemory_lower_loop
.Lmemory_lower_left:
 mov rcx,r9
 jmp .Lmemory_lower_loop
.Lmemory_lower_done:
 mov rax,rdx
 ret

.Lmemory_compare:
 mov rax,[rdi]
 cmp rax,[rsi]
 jne .Lmemory_compare_result
 mov rax,[rdi+16]
 cmp rax,[rsi+16]
.Lmemory_compare_result:
 seta al
 setb dl
 movzx eax,al
 movzx edx,dl
 sub eax,edx
 ret

/* Resolve exact JSON string names, including embedded-NUL rejection. */
.Lmemory_table:
 push rbx
 push r12
 push r13
 sub rsp,16
 mov rbx,rdi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lmemory_table_bad
 mov rdi,rbx
 call json_object_get_string_len@PLT
 mov [rsp],eax
 mov rdi,rbx
 call json_object_get_string@PLT
 mov r12,rax
 xor r13d,r13d
.Lmemory_table_loop:
 lea rax,[rip+.Ltable_lengths]
 mov eax,[rax+r13*4]
 cmp eax,[rsp]
 jne .Lmemory_table_next
 lea rax,[rip+.Ltable_names]
 movsxd rsi,dword ptr [rax+r13*4]
 add rsi,rax
 mov rdi,r12
 call strcmp@PLT
 test eax,eax
 jz .Lmemory_table_found
.Lmemory_table_next:
 inc r13d
 cmp r13d,5
 jb .Lmemory_table_loop
.Lmemory_table_bad:
 mov eax,-1
 jmp .Lmemory_table_return
.Lmemory_table_found:
 mov eax,r13d
.Lmemory_table_return:
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret

.Lmemory_commit:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,104
 mov rbx,rdi
 mov r12,rdx
 mov r13,rcx
 mov r14,r8
 xor ebp,ebp
 mov [rsp+8],rsi
 mov qword ptr [rsp],0
 test r14,r14
 jz .Lmemory_commit_invalid
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Lmemory_commit_invalid
 test r12,r12
 jz .Lmemory_commit_invalid
 cmp dword ptr [rbx+40],0
 jne .Lmemory_commit_invalid
 cmp dword ptr [rbx+44],0
 jne .Lmemory_commit_invalid
 cmp qword ptr [rbx+24],0
 je .Lmemory_commit_invalid
 mov rax,9007199254740991
 cmp [rbx+32],rax
 ja .Lmemory_commit_invalid
 mov dword ptr [rbx+40],1
 mov rdi,[rsp+8]
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lmemory_invalid_cleanup
 mov rdi,[rsp+8]
 call pg_document_value_valid
 test eax,eax
 jz .Lmemory_invalid_cleanup
 mov rdi,[rsp+8]
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lmemory_allocation_cleanup
 mov qword ptr [rsp+64],0
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,[rbx+32]
 lea rcx,[rsp+64]
 call .Lmemory_prepare_documents
 test eax,eax
 jnz .Lmemory_prepare_failed
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rax,[rsp+64]
 mov [rsp],rax
 mov rdi,rax
 call json_object_array_length@PLT
 mov [rsp+16],rax
 mov rcx,0x07ffffffffffffff
 cmp rax,rcx
 ja .Lmemory_allocation_cleanup
 test rax,rax
 jz .Lmemory_empty_prepared
 mov rdi,rax
 mov esi,32
 call calloc@PLT
 test rax,rax
 jz .Lmemory_allocation_cleanup
 mov rbp,rax
 mov qword ptr [rsp+24],0
.Lmemory_stage:
 mov rsi,[rsp+24]
 cmp rsi,[rsp+16]
 jae .Lmemory_sort
 mov rdi,[rsp]
 call json_object_array_get_idx@PLT
 mov r15,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lmemory_invalid_cleanup
 mov rdi,r15
 lea rsi,[rip+.Ltype]
 call json_object_object_get@PLT
 mov rdi,rax
 call .Lmemory_table
 cmp eax,4
 ja .Lmemory_invalid_cleanup
 mov [rsp+48],eax
 mov rdi,r15
 lea rsi,[rip+.Lvalue_name]
 call json_object_object_get@PLT
 mov [rsp+56],rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lmemory_invalid_cleanup
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lid]
 call json_object_object_get@PLT
 mov r15,rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,3
 je .Lmemory_integer_id
 cmp eax,2
 jne .Lmemory_invalid_cleanup
 mov rdi,r15
 call json_object_get_double@PLT
 cvttsd2si rax,xmm0
 cvtsi2sd xmm1,rax
 ucomisd xmm0,xmm1
 jp .Lmemory_invalid_cleanup
 jne .Lmemory_invalid_cleanup
 jmp .Lmemory_id_value
.Lmemory_integer_id:
 mov rdi,r15
 call json_object_get_int64@PLT
.Lmemory_id_value:
 test rax,rax
 jle .Lmemory_invalid_cleanup
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lmemory_invalid_cleanup
 mov rcx,[rsp+24]
 mov rdx,rcx
 shl rdx,5
 mov [rbp+rdx],rax
 mov rax,[rsp+56]
 mov [rbp+rdx+8],rax
 mov [rbp+rdx+16],rcx
 mov eax,[rsp+48]
 mov [rbp+rdx+24],eax
 inc qword ptr [rsp+24]
 jmp .Lmemory_stage
.Lmemory_sort:
 mov rdi,rbp
 mov rsi,[rsp+16]
 mov edx,32
 lea rcx,[rip+.Lmemory_compare]
 call qsort@PLT
 mov qword ptr [rsp+24],0
 mov qword ptr [rsp+32],0
.Lmemory_compact:
 mov rax,[rsp+24]
 cmp rax,[rsp+16]
 jae .Lmemory_check_ids
 shl rax,5
 lea r15,[rbp+rax]
 mov rdx,[rsp+32]
 test rdx,rdx
 jz .Lmemory_compact_new
 mov rcx,rdx
 dec rcx
 shl rcx,5
 add rcx,rbp
 mov rax,[r15]
 cmp rax,[rcx]
 jne .Lmemory_compact_new
 mov eax,[r15+24]
 cmp eax,1
 jbe .Lmemory_invalid_cleanup
 cmp eax,[rcx+24]
 jne .Lmemory_invalid_cleanup
 jmp .Lmemory_compact_copy
.Lmemory_compact_new:
 mov rcx,rdx
 shl rcx,5
 add rcx,rbp
 inc qword ptr [rsp+32]
.Lmemory_compact_copy:
 movups xmm0,[r15]
 movups xmm1,[r15+16]
 movups [rcx],xmm0
 movups [rcx+16],xmm1
 inc qword ptr [rsp+24]
 jmp .Lmemory_compact
.Lmemory_check_ids:
 mov qword ptr [rsp+24],0
 mov rax,[rbx+8]
 mov [rsp+40],rax
.Lmemory_check_id:
 mov rax,[rsp+24]
 cmp rax,[rsp+32]
 jae .Lmemory_reserve
 shl rax,5
 lea r15,[rbp+rax]
 mov rdi,rbx
 mov rsi,[r15]
 call .Lmemory_lower
 cmp rax,[rbx+8]
 jae .Lmemory_new_id
 shl rax,5
 add rax,[rbx]
 mov rcx,[r15]
 cmp [rax],rcx
 jne .Lmemory_new_id
 mov ecx,[r15+24]
 cmp ecx,1
 jbe .Lmemory_invalid_cleanup
 cmp [rax+24],ecx
 jne .Lmemory_invalid_cleanup
 jmp .Lmemory_next_id
.Lmemory_new_id:
 inc qword ptr [rsp+40]
 jz .Lmemory_allocation_cleanup
.Lmemory_next_id:
 inc qword ptr [rsp+24]
 jmp .Lmemory_check_id
.Lmemory_reserve:
 mov rdx,[rsp+40]
 mov rax,0x07ffffffffffffff
 cmp rdx,rax
 ja .Lmemory_allocation_cleanup
 cmp rdx,[rbx+16]
 jbe .Lmemory_empty_prepared
 mov rsi,[rbx+16]
 test rsi,rsi
 jnz .Lmemory_capacity_loop
 mov esi,16
.Lmemory_capacity_loop:
 cmp rsi,rdx
 jae .Lmemory_allocate_rows
 cmp rsi,rax
 ja .Lmemory_exact_capacity
 shl rsi,1
 cmp rsi,rax
 jbe .Lmemory_capacity_loop
.Lmemory_exact_capacity:
 mov rsi,rdx
.Lmemory_allocate_rows:
 mov [rsp+40],rsi
 mov rdi,[rbx]
 mov edx,32
 call reallocarray@PLT
 test rax,rax
 jz .Lmemory_allocation_cleanup
 mov [rbx],rax
 mov rax,[rsp+40]
 mov [rbx+16],rax
.Lmemory_empty_prepared:
 mov qword ptr [rsp+24],0
.Lmemory_apply:
 mov rax,[rsp+24]
 test rbp,rbp
 jz .Lmemory_applied
 cmp rax,[rsp+32]
 jae .Lmemory_applied
 shl rax,5
 lea r15,[rbp+rax]
 mov rdi,rbx
 mov rsi,[r15]
 call .Lmemory_lower
 mov rcx,rax
 shl rcx,5
 add rcx,[rbx]
 mov [rsp+72],rcx
 mov qword ptr [rsp+88],0
 cmp rax,[rbx+8]
 jae .Lmemory_insert
 mov rdx,[r15]
 cmp [rcx],rdx
 jne .Lmemory_insert
 mov rax,[rcx+8]
 mov [rsp+88],rax
 jmp .Lmemory_store_row
.Lmemory_insert:
 mov rdx,[rbx+8]
 sub rdx,rax
 shl rdx,5
 mov rsi,rcx
 lea rdi,[rcx+32]
 call memmove@PLT
 inc qword ptr [rbx+8]
.Lmemory_store_row:
 mov rdi,[r15+8]
 call json_object_get@PLT
 mov [rsp+80],rax
 mov rdi,[rsp+88]
 call json_object_put@PLT
 mov rcx,[rsp+72]
 mov rax,[r15]
 mov [rcx],rax
 cmp rax,[rbx+24]
 jb .Lmemory_keep_next_id
 inc rax
 mov [rbx+24],rax
.Lmemory_keep_next_id:
 mov rax,[rsp+80]
 mov [rcx+8],rax
 mov rax,[rbx+32]
 mov [rcx+16],rax
 mov eax,[r15+24]
 mov [rcx+24],eax
 mov dword ptr [rcx+28],0
 inc qword ptr [rsp+24]
 jmp .Lmemory_apply
.Lmemory_applied:
 mov rax,[rbx+32]
 mov [rsp+96],rax
 inc qword ptr [rbx+32]
 mov dword ptr [rbx+40],2
 mov rdi,r13
 mov rsi,rax
 call r12
 mov rax,[rsp+96]
 mov [r14],rax
 xor r15d,r15d
 jmp .Lmemory_commit_cleanup
.Lmemory_prepare_failed:
 cmp eax,3
 je .Lmemory_allocation_cleanup
.Lmemory_invalid_cleanup:
 mov r15d,2
 jmp .Lmemory_commit_cleanup
.Lmemory_allocation_cleanup:
 mov r15d,3
.Lmemory_commit_cleanup:
 mov dword ptr [rbx+40],1
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,rbp
 call free@PLT
 mov dword ptr [rbx+40],0
 mov eax,r15d
 jmp .Lmemory_commit_return
.Lmemory_commit_invalid:
 mov eax,2
.Lmemory_commit_return:
 add rsp,104
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.globl pg_record_memory_mint
.type pg_record_memory_mint,@function
pg_record_memory_mint:
 mov eax,2
 test rsi,rsi
 jz .Lmemory_mint_return
 mov qword ptr [rsi],0
 test rdi,rdi
 jz .Lmemory_mint_return
 cmp qword ptr [rdi+40],0
 jne .Lmemory_mint_return
 mov rdx,[rdi+24]
 test rdx,rdx
 jz .Lmemory_mint_return
 mov rcx,9007199254740991
 cmp rdx,rcx
 ja .Lmemory_mint_return
 mov [rsi],rdx
 inc qword ptr [rdi+24]
 xor eax,eax
.Lmemory_mint_return:
 ret
.size pg_record_memory_mint,.-pg_record_memory_mint

.globl pg_record_memory_read
.type pg_record_memory_read,@function
pg_record_memory_read:
 push rbx
 push r12
 push r13
 sub rsp,16
 mov rbx,rdi
 mov r12d,esi
 mov r13,rcx
 mov qword ptr [rsp],0
 test r13,r13
 jz .Lmemory_read_invalid
 mov qword ptr [r13],0
 test rbx,rbx
 jz .Lmemory_read_invalid
 cmp r12d,3
 ja .Lmemory_read_invalid
 cmp dword ptr [rbx+44],0
 jne .Lmemory_read_invalid
 cmp dword ptr [rbx+40],1
 je .Lmemory_read_invalid
 cmp qword ptr [rbx+24],0
 je .Lmemory_read_invalid
 test rdx,rdx
 jz .Lmemory_read_invalid
 mov rax,9007199254740991
 cmp rdx,rax
 ja .Lmemory_read_invalid
 mov [rsp+8],rdx
 mov rsi,rdx
 call .Lmemory_lower
 cmp rax,[rbx+8]
 jae .Lmemory_read_missing
 shl rax,5
 add rax,[rbx]
 mov rdx,[rsp+8]
 cmp [rax],rdx
 jne .Lmemory_read_missing
 cmp [rax+24],r12d
 jne .Lmemory_read_missing
 mov rdi,[rax+8]
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lmemory_read_allocation
 mov rax,[rsp]
 mov [r13],rax
 xor eax,eax
 jmp .Lmemory_read_return
.Lmemory_read_allocation:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,3
 jmp .Lmemory_read_return
.Lmemory_read_missing:
 mov eax,1
 jmp .Lmemory_read_return
.Lmemory_read_invalid:
 mov eax,2
.Lmemory_read_return:
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret
.size pg_record_memory_read,.-pg_record_memory_read

.globl pg_record_memory_close
.type pg_record_memory_close,@function
pg_record_memory_close:
 test rdi,rdi
 jz .Lmemory_close_bad
 cmp dword ptr [rdi+40],0
 jne .Lmemory_close_bad
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov dword ptr [rbx+40],1
 xor r12d,r12d
.Lmemory_close_row:
 cmp r12,[rbx+8]
 jae .Lmemory_close_free
 mov rax,r12
 shl rax,5
 add rax,[rbx]
 mov rdi,[rax+8]
 call json_object_put@PLT
 inc r12
 jmp .Lmemory_close_row
.Lmemory_close_free:
 mov rdi,[rbx]
 call free@PLT
 mov qword ptr [rbx],0
 mov qword ptr [rbx+8],0
 mov qword ptr [rbx+16],0
 mov dword ptr [rbx+40],0
 mov dword ptr [rbx+44],1
 xor eax,eax
 add rsp,8
 pop r12
 pop rbx
 ret
.Lmemory_close_bad:
 mov eax,2
 ret
.size pg_record_memory_close,.-pg_record_memory_close
.section .rodata
.Ltable_names: .long .Lconversation-.Ltable_names,.Lentry-.Ltable_names,.Ltask-.Ltable_names,.Lsubmission-.Ltable_names,.Ld_prepared-.Ltable_names
.Ltable_lengths: .long 12,5,4,10,17
.Lconversation: .asciz "conversation"
.Lentry: .asciz "entry"
.Ltask: .asciz "task"
.Lsubmission: .asciz "submission"
.Ltype: .asciz "type"
.Lvalue_name: .asciz "value"
.Lid: .asciz "id"
.section .note.GNU-stack,"",@progbits
.text
/* Query scalar comparisons preserve JSON type distinctions. */
.Lscan_number:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rsi
 mov r12,rdi
 call json_object_get_type@PLT
 sub eax,2
 cmp eax,1
 ja .Lscan_number_bad
 mov rdi,r12
 call json_object_get_double@PLT
 cvtsi2sd xmm1,rbx
 ucomisd xmm0,xmm1
 jp .Lscan_number_bad
 jne .Lscan_number_bad
 mov eax,1
 jmp .Lscan_number_return
.Lscan_number_bad:
 xor eax,eax
.Lscan_number_return:
 add rsp,8
 pop r12
 pop rbx
 ret
.Lscan_string:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rsi
 mov r12,rdi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lscan_string_return
 mov rdi,r12
 call json_object_get_string_len@PLT
 cmp rax,[rbx+8]
 jne .Lscan_string_bad
 test eax,eax
 jz .Lscan_string_good
 mov rdi,r12
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,[rbx]
 mov rdx,[rbx+8]
 call memcmp@PLT
 test eax,eax
 jnz .Lscan_string_bad
.Lscan_string_good:
 mov eax,1
 jmp .Lscan_string_return
.Lscan_string_bad:
 xor eax,eax
.Lscan_string_return:
 add rsp,8
 pop r12
 pop rbx
 ret
.Lscan_boolean:
 push rbx
 push r12
 sub rsp,8
 mov ebx,esi
 mov r12,rdi
 mov esi,1
 call json_object_is_type@PLT
 test eax,eax
 jz .Lscan_boolean_return
 mov rdi,r12
 call json_object_get_boolean@PLT
 cmp eax,ebx
 sete al
 movzx eax,al
.Lscan_boolean_return:
 add rsp,8
 pop r12
 pop rbx
 ret

.Lscan_match:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,16
 mov r15,rdi
 mov r14,rsi
 mov eax,[r14]
 cmp eax,1
 je .Lscan_match_entry
 test eax,eax
 jz .Lscan_match_owner
 cmp eax,2
 je .Lscan_match_task
 lea r13,[rip+.Lsubmission_filters]
 mov r12d,2
 jmp .Lscan_match_loop
.Lscan_match_entry:
 test qword ptr [r14+32],512
 jz .Lscan_match_yes
 mov rdi,r15
 lea rsi,[rip+.Lhead_name]
 mov rdx,rsp
 call json_object_object_get_ex@PLT
 jmp .Lscan_match_return
.Lscan_match_task:
 lea r13,[rip+.Ltask_filters]
 mov r12d,5
 jmp .Lscan_match_loop
.Lscan_match_owner:
 cmp qword ptr [r14+32],0
 je .Lscan_match_yes
 mov rdi,r15
 lea rsi,[rip+.Lowner]
 call json_object_object_get@PLT
 mov r15,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lscan_match_no
 lea r13,[rip+.Lowner_filters]
 mov r12d,2
.Lscan_match_loop:
 test r12d,r12d
 jz .Lscan_match_yes
 mov eax,[r13]
 test [r14+32],rax
 jz .Lscan_match_next
 mov rdi,r15
 cmp dword ptr [r13+16],3
 jne .Lscan_match_field
 cmp dword ptr [r14],2
 jne .Lscan_match_field
 lea rsi,[rip+.Lstate]
 call json_object_object_get@PLT
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lscan_match_no
 mov rdi,r15
 lea rsi,[rip+.Lstate]
 call json_object_object_get@PLT
 mov rdi,rax
.Lscan_match_field:
 mov rsi,[r13+8]
 call json_object_object_get@PLT
 mov rdi,rax
 mov esi,[r13+4]
 add rsi,r14
 mov eax,[r13+16]
 test eax,eax
 jz .Lscan_match_numeric
 cmp eax,1
 je .Lscan_match_text
 cmp eax,2
 je .Lscan_match_bool
 mov eax,[r14+96]
 shl rax,4
 lea rsi,[rip+.Lsubmission_statuses]
 cmp dword ptr [r14],2
 jne .Lscan_match_status
 lea rsi,[rip+.Ltask_statuses]
.Lscan_match_status:
 add rsi,rax
.Lscan_match_text:
 call .Lscan_string
 jmp .Lscan_match_result
.Lscan_match_numeric:
 mov rsi,[rsi]
 call .Lscan_number
 jmp .Lscan_match_result
.Lscan_match_bool:
 mov esi,[rsi]
 call .Lscan_boolean
.Lscan_match_result:
 test eax,eax
 jz .Lscan_match_no
.Lscan_match_next:
 add r13,24
 dec r12d
 jmp .Lscan_match_loop
.Lscan_match_yes:
 mov eax,1
 jmp .Lscan_match_return
.Lscan_match_no:
 xor eax,eax
.Lscan_match_return:
 add rsp,16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret

.Lscan_single:
 mov ecx,1
 jmp .Lscan_impl
.globl pg_record_memory_scan
.type pg_record_memory_scan,@function
pg_record_memory_scan:
 xor ecx,ecx
.Lscan_impl:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,120
 mov [rsp+112],ecx
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor ebp,ebp
 mov qword ptr [rsp],0 /* result array */
 mov qword ptr [rsp+8],0 /* copied item */
 mov qword ptr [rsp+16],0 /* segment count */
 mov qword ptr [rsp+24],0 /* segment capacity */
 mov qword ptr [rsp+32],0 /* segment index */
 mov qword ptr [rsp+40],0 /* returned items */
 mov qword ptr [rsp+48],0 /* last returned ID */
 mov dword ptr [rsp+56],0 /* more */
 test r13,r13
 jz .Lscan_invalid_return
 mov qword ptr [r13],0
 mov qword ptr [r13+8],0
 mov qword ptr [r13+16],0
 test rbx,rbx
 jz .Lscan_invalid_return
 test r12,r12
 jz .Lscan_invalid_return
 cmp dword ptr [rbx+44],0
 jne .Lscan_invalid_return
 cmp dword ptr [rbx+40],1
 je .Lscan_invalid_return
 cmp qword ptr [rbx+24],0
 je .Lscan_invalid_return
 cmp dword ptr [r12],3
 ja .Lscan_invalid_return
 cmp dword ptr [r12+4],2
 ja .Lscan_invalid_return
 cmp dword ptr [r12+8],1
 ja .Lscan_invalid_return
 cmp qword ptr [r12+24],0
 je .Lscan_invalid_return
 mov eax,[r12]
 lea rcx,[rip+.Lscan_masks]
 mov eax,[rcx+rax*4]
 not rax
 test [r12+32],rax
 jnz .Lscan_invalid_return
 cmp dword ptr [r12],1
 jne .Lscan_fields
 test qword ptr [r12+32],1
 jz .Lscan_invalid_return
.Lscan_fields:
 xor r14d,r14d
 mov r15,9007199254740991
.Lscan_numeric_fields:
 mov ecx,r14d
 mov eax,1
 shl rax,cl
 test [r12+32],rax
 jz .Lscan_numeric_next
 mov rax,[r12+r14*8+40]
 cmp rax,r15
 ja .Lscan_invalid_return
 cmp r14d,3
 jae .Lscan_numeric_next
 test rax,rax
 jz .Lscan_invalid_return
.Lscan_numeric_next:
 inc r14d
 cmp r14d,5
 jb .Lscan_numeric_fields
 test qword ptr [r12+32],32
 jz .Lscan_validate_status
 mov rax,[r12+88]
 cmp rax,0x7fffffff
 ja .Lscan_invalid_return
 test rax,rax
 jz .Lscan_validate_status
 cmp qword ptr [r12+80],0
 je .Lscan_invalid_return
.Lscan_validate_status:
 test qword ptr [r12+32],64
 jz .Lscan_validate_booleans
 mov eax,3
 cmp dword ptr [r12],2
 jne .Lscan_status_limit
 mov eax,5
.Lscan_status_limit:
 cmp [r12+96],eax
 ja .Lscan_invalid_return
.Lscan_validate_booleans:
 test qword ptr [r12+32],128
 jz .Lscan_background
 cmp dword ptr [r12+100],1
 ja .Lscan_invalid_return
.Lscan_background:
 test qword ptr [r12+32],256
 jz .Lscan_order
 cmp dword ptr [r12+104],1
 ja .Lscan_invalid_return
.Lscan_order:
 mov r14d,1
 cmp dword ptr [r12],1
 jne .Lscan_fallback
 mov r14d,2
.Lscan_fallback:
 cmp dword ptr [r12+8],0
 je .Lscan_no_cursor
 cmp dword ptr [r12+12],2
 ja .Lscan_invalid_return
 mov eax,[r12+12]
 test eax,eax
 jz .Lscan_cursor_order
 mov r14d,eax
.Lscan_cursor_order:
 mov eax,[r12+4]
 test eax,eax
 jz .Lscan_cursor_bound
 cmp eax,r14d
 jne .Lscan_invalid_return
.Lscan_cursor_bound:
 mov rax,[r12+16]
 cmp rax,r15
 jg .Lscan_invalid_return
 mov rcx,r15
 neg rcx
 cmp rax,rcx
 jl .Lscan_invalid_return
 jmp .Lscan_order_ready
.Lscan_no_cursor:
 mov eax,[r12+4]
 test eax,eax
 jz .Lscan_order_ready
 mov r14d,eax
.Lscan_order_ready:
 mov [rsp+60],r14d
 mov qword ptr [rsp+64],0 /* entry lower */
 mov [rsp+72],r15 /* entry upper */
 cmp dword ptr [r12],1
 jne .Lscan_allocate_page
 test qword ptr [r12+32],8
 jz .Lscan_entry_max
 mov rax,[r12+64]
 mov [rsp+64],rax
.Lscan_entry_max:
 test qword ptr [r12+32],16
 jz .Lscan_entry_cursor
 mov rax,[r12+72]
 mov [rsp+72],rax
.Lscan_entry_cursor:
 cmp dword ptr [r12+8],0
 je .Lscan_entry_first
 mov rax,[r12+16]
 cmp r14d,1
 je .Lscan_entry_min_cursor
 dec rax
 cmp rax,[rsp+72]
 jge .Lscan_entry_first
 mov [rsp+72],rax
 jmp .Lscan_entry_first
.Lscan_entry_min_cursor:
 inc rax
 cmp rax,[rsp+64]
 jle .Lscan_entry_first
 mov [rsp+64],rax
.Lscan_entry_first:
 mov rax,[r12+40]
 mov [rsp+80],rax /* current conversation */
.Lscan_segment_build:
 mov rdi,rbx
 mov rsi,[rsp+80]
 call .Lmemory_lower
 cmp rax,[rbx+8]
 jae .Lscan_invalid_cleanup
 shl rax,5
 add rax,[rbx]
 mov rcx,[rsp+80]
 cmp [rax],rcx
 jne .Lscan_invalid_cleanup
 cmp dword ptr [rax+24],0
 jne .Lscan_invalid_cleanup
 mov rax,[rax+8]
 mov [rsp+88],rax /* conversation value */
 mov rax,[rsp+16]
 cmp rax,[rbx+8]
 jae .Lscan_invalid_cleanup
 cmp rax,[rsp+24]
 jb .Lscan_segment_slot
 mov rsi,[rsp+24]
 test rsi,rsi
 jnz .Lscan_segment_double
 mov esi,4
 jmp .Lscan_segment_allocate
.Lscan_segment_double:
 shl rsi,1
.Lscan_segment_allocate:
 mov [rsp+96],rsi
 mov rdi,rbp
 mov edx,16
 call reallocarray@PLT
 test rax,rax
 jz .Lscan_allocation_cleanup
 mov rbp,rax
 mov rax,[rsp+96]
 mov [rsp+24],rax
.Lscan_segment_slot:
 mov rax,[rsp+16]
 shl rax,4
 mov rcx,[rsp+80]
 mov [rbp+rax],rcx
 mov rcx,[rsp+72]
 mov [rbp+rax+8],rcx
 inc qword ptr [rsp+16]
 cmp dword ptr [rsp+60],2
 je .Lscan_descending_ready
.Lscan_parent_step:
 mov rdi,[rsp+88]
 lea rsi,[rip+.Lparent]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lscan_parent_exhausted
 mov [rsp+96],rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lscan_invalid_cleanup
 mov rax,[rsp+72]
 cmp rax,[rsp+64]
 jl .Lscan_parent_exhausted
 mov rdi,[rsp+96]
 lea rsi,[rip+.Lat]
 call json_object_object_get@PLT
 mov rdi,rax
 call .Lscan_safe_number
 test edx,edx
 jz .Lscan_invalid_cleanup
 cmp rax,[rsp+72]
 jge .Lscan_parent_id
 mov [rsp+72],rax
.Lscan_parent_id:
 mov rax,[rsp+72]
 cmp rax,[rsp+64]
 jl .Lscan_parent_exhausted
 mov rdi,[rsp+96]
 lea rsi,[rip+.Lconversation_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call .Lscan_safe_number
 test edx,edx
 jz .Lscan_invalid_cleanup
 test rax,rax
 jle .Lscan_invalid_cleanup
 mov [rsp+80],rax
 jmp .Lscan_segment_build
.Lscan_parent_exhausted:
 cmp dword ptr [rsp+60],2
 je .Lscan_success
.Lscan_segments_ready:
 mov rax,[rsp+16]
 dec rax
 mov [rsp+32],rax
 jmp .Lscan_allocate_page
.Lscan_descending_ready:
 mov rax,[rsp+16]
 dec rax
 mov [rsp+32],rax
.Lscan_allocate_page:
 cmp qword ptr [rsp],0
 jne .Lscan_segment_start
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lscan_allocation_cleanup
 mov [rsp],rax
.Lscan_segment_start:
 cmp dword ptr [rsp+60],1
 jne .Lscan_start_descending
 xor r14d,r14d
 cmp dword ptr [r12],1
 je .Lscan_start_entry_ascending
 cmp dword ptr [r12+8],0
 je .Lscan_row_loop
 mov rsi,[r12+16]
 test rsi,rsi
 js .Lscan_row_loop
 inc rsi
 jmp .Lscan_start_lower
.Lscan_start_entry_ascending:
 mov rsi,[rsp+64]
.Lscan_start_lower:
 mov rdi,rbx
 call .Lmemory_lower
 mov r14,rax
 jmp .Lscan_row_loop
.Lscan_start_descending:
 mov r14,[rbx+8]
 dec r14
 cmp dword ptr [r12],1
 je .Lscan_start_entry_descending
 cmp dword ptr [r12+8],0
 je .Lscan_row_loop
 mov rsi,[r12+16]
 test rsi,rsi
 jle .Lscan_start_empty
 jmp .Lscan_start_upper
.Lscan_start_entry_descending:
 mov rax,[rsp+32]
 shl rax,4
 mov rsi,[rbp+rax+8]
 test rsi,rsi
 js .Lscan_start_empty
 inc rsi
.Lscan_start_upper:
 mov rdi,rbx
 call .Lmemory_lower
 lea r14,[rax-1]
 jmp .Lscan_row_loop
.Lscan_start_empty:
 mov r14,-1
.Lscan_row_loop:
 test r14,r14
 js .Lscan_segment_done
 cmp r14,[rbx+8]
 jae .Lscan_segment_done
 mov r15,r14
 shl r15,5
 add r15,[rbx]
 mov eax,[r12]
 cmp [r15+24],eax
 jne .Lscan_row_next
 cmp eax,1
 je .Lscan_entry_row
 cmp dword ptr [r12+8],0
 je .Lscan_row_match
 mov rax,[r15]
 cmp rax,[r12+16]
 je .Lscan_row_next
 cmp dword ptr [rsp+60],1
 je .Lscan_row_after_ascending
 cmp rax,[r12+16]
 jge .Lscan_row_next
 jmp .Lscan_row_match
.Lscan_row_after_ascending:
 cmp rax,[r12+16]
 jle .Lscan_row_next
 jmp .Lscan_row_match
.Lscan_entry_row:
 mov rax,[r15]
 cmp rax,[rsp+64]
 jl .Lscan_entry_below
 mov rcx,[rsp+32]
 shl rcx,4
 cmp rax,[rbp+rcx+8]
 jg .Lscan_entry_above
 mov rax,[rbp+rcx]
 mov [rsp+104],rax
 mov rdi,[r15+8]
 lea rsi,[rip+.Lconversation_id]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,[rsp+104]
 call .Lscan_number
 test eax,eax
 jz .Lscan_row_next
 jmp .Lscan_row_match
.Lscan_entry_below:
 cmp dword ptr [rsp+60],2
 je .Lscan_segment_done
 jmp .Lscan_row_next
.Lscan_entry_above:
 cmp dword ptr [rsp+60],1
 je .Lscan_segment_done
 jmp .Lscan_row_next
.Lscan_row_match:
 mov rdi,[r15+8]
 mov rsi,r12
 call .Lscan_match
 test eax,eax
 jz .Lscan_row_next
 mov rax,[rsp+40]
 cmp rax,[r12+24]
 jae .Lscan_more
 mov rdi,[r15+8]
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lscan_allocation_cleanup
 mov rdi,[rsp]
 mov rsi,[rsp+8]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lscan_allocation_cleanup
 mov qword ptr [rsp+8],0
 mov rax,[r15]
 mov [rsp+48],rax
 inc qword ptr [rsp+40]
 cmp dword ptr [rsp+112],0
 jne .Lscan_success
.Lscan_row_next:
 cmp dword ptr [rsp+60],1
 je .Lscan_row_up
 dec r14
 jmp .Lscan_row_loop
.Lscan_row_up:
 inc r14
 jmp .Lscan_row_loop
.Lscan_segment_done:
 cmp dword ptr [r12],1
 jne .Lscan_success
 cmp dword ptr [rsp+60],1
 je .Lscan_segment_parent_first
 jmp .Lscan_parent_step
.Lscan_segment_parent_first:
 dec qword ptr [rsp+32]
 jns .Lscan_segment_start
 jmp .Lscan_success
.Lscan_more:
 mov dword ptr [rsp+56],1
.Lscan_success:
 mov rax,[rsp]
 mov [r13],rax
 mov qword ptr [rsp],0
 mov eax,[rsp+60]
 mov [r13+16],eax
 mov eax,[rsp+56]
 mov [r13+20],eax
 test eax,eax
 jz .Lscan_no_next
 mov rax,[rsp+48]
 mov [r13+8],rax
.Lscan_no_next:
 xor r14d,r14d
 jmp .Lscan_cleanup
.Lscan_invalid_cleanup:
 mov r14d,2
 jmp .Lscan_cleanup
.Lscan_allocation_cleanup:
 mov r14d,3
.Lscan_cleanup:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,rbp
 call free@PLT
 mov eax,r14d
 jmp .Lscan_return
.Lscan_invalid_return:
 mov eax,2
.Lscan_return:
 add rsp,120
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_memory_scan,.-pg_record_memory_scan

/* Signed safe integer field for fork metadata. */
.Lscan_safe_number:
 push rbx
 mov rbx,rdi
 call json_object_get_type@PLT
 cmp eax,3
 je .Lscan_safe_integer
 cmp eax,2
 jne .Lscan_safe_bad
 mov rdi,rbx
 call json_object_get_double@PLT
 cvttsd2si rax,xmm0
 cvtsi2sd xmm1,rax
 ucomisd xmm0,xmm1
 jp .Lscan_safe_bad
 jne .Lscan_safe_bad
 jmp .Lscan_safe_bound
.Lscan_safe_integer:
 mov rdi,rbx
 call json_object_get_int64@PLT
.Lscan_safe_bound:
 mov rcx,9007199254740991
 cmp rax,rcx
 jg .Lscan_safe_bad
 neg rcx
 cmp rax,rcx
 jl .Lscan_safe_bad
 mov edx,1
 pop rbx
 ret
.Lscan_safe_bad:
 xor edx,edx
 pop rbx
 ret
.section .rodata
.Lscan_masks: .long 6,537,481,65
.Lowner: .asciz "owner"
.Lstate: .asciz "state"
.Lparent: .asciz "parent"
.Lat: .asciz "at"
.Lconversation_id: .asciz "conversationId"
.Ltask_id: .asciz "taskId"
.Lkind_filter: .asciz "kind"
.Lstatus_filter: .asciz "status"
.Labort_requested: .asciz "abortRequested"
.Lbackground_filter: .asciz "background"
.Lpending: .asciz "pending"
.Lrunning: .asciz "running"
.Lwaiting: .asciz "waiting"
.Lsleeping: .asciz "sleeping"
.Lcompleting: .asciz "completing"
.Lterminal: .asciz "terminal"
.Lqueued: .asciz "queued"
.Lplaced: .asciz "placed"
.Ldone: .asciz "done"
.Lunanswered: .asciz "unanswered"
.section .data.rel.ro
.Lowner_filters:
 .long 2,48
 .quad .Lconversation_id
 .long 0,0
 .long 4,56
 .quad .Ltask_id
 .long 0,0
.Ltask_filters:
 .long 1,40
 .quad .Lconversation_id
 .long 0,0
 .long 32,80
 .quad .Lkind_filter
 .long 1,0
 .long 64,96
 .quad .Lstatus_filter
 .long 3,0
 .long 128,100
 .quad .Labort_requested
 .long 2,0
 .long 256,104
 .quad .Lbackground_filter
 .long 2,0
.Lsubmission_filters:
 .long 1,40
 .quad .Lconversation_id
 .long 0,0
 .long 64,96
 .quad .Lstatus_filter
 .long 3,0
.Ltask_statuses:
 .quad .Lpending,7,.Lrunning,7,.Lwaiting,7,.Lsleeping,8,.Lcompleting,10,.Lterminal,8
.Lsubmission_statuses:
 .quad .Lqueued,6,.Lplaced,6,.Ldone,4,.Lunanswered,10

.text
/* Detach a first page item and release the page container. */
.Lscan_first:
 push rbx
 push r12
 push r13
 sub rsp,32
 mov r12,rdx
 mov qword ptr [r12],0
 mov rdx,rsp
 call .Lscan_single
 mov r13d,eax
 test eax,eax
 jnz .Lscan_first_cleanup
 mov rdi,[rsp]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lscan_first_missing
 mov rdi,[rsp]
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [r12],rax
 jmp .Lscan_first_cleanup
.Lscan_first_missing:
 mov r13d,1
.Lscan_first_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,r13d
 add rsp,32
 pop r13
 pop r12
 pop rbx
 ret

.globl pg_record_memory_entry
.type pg_record_memory_entry,@function
pg_record_memory_entry:
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
 mov r14,rcx
 mov r15,r8
 mov ebp,2
 test r14,r14
 jz .Lentry_lookup_return
 mov qword ptr [r14],0
 test r15,r15
 jz .Lentry_lookup_return
 mov qword ptr [r15],0
 test r13,r13
 jnz .Lentry_lookup_scoped
 mov esi,1
 mov rdx,r12
 mov rcx,r14
 call pg_record_memory_read
 jmp .Lentry_lookup_read
.Lentry_lookup_scoped:
 test r12,r12
 jz .Lentry_lookup_return
 mov rdi,rsp
 xor eax,eax
 mov ecx,14
 rep stosq
 mov dword ptr [rsp],1
 mov qword ptr [rsp+24],1
 mov qword ptr [rsp+32],25
 mov [rsp+40],r13
 mov [rsp+64],r12
 mov [rsp+72],r12
 mov rdi,rbx
 mov rsi,rsp
 mov rdx,r14
 call .Lscan_first
.Lentry_lookup_read:
 mov ebp,eax
 test eax,eax
 jnz .Lentry_lookup_return
 mov rdi,rbx
 mov rsi,r12
 call .Lmemory_lower
 shl rax,5
 add rax,[rbx]
 mov rax,[rax+16]
 mov [r15],rax
.Lentry_lookup_return:
 mov eax,ebp
 add rsp,136
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_memory_entry,.-pg_record_memory_entry

.globl pg_record_memory_head
.type pg_record_memory_head,@function
pg_record_memory_head:
 push rbx
 push r12
 push r13
 sub rsp,112
 mov rbx,rdi
 mov r12,rcx
 mov r13,rdx
 test r12,r12
 jz .Lhead_lookup_invalid
 mov qword ptr [r12],0
 mov rdx,rsi
 mov rdi,rsp
 xor eax,eax
 mov ecx,14
 rep stosq
 mov dword ptr [rsp],1
 mov qword ptr [rsp+24],1
 mov qword ptr [rsp+32],513
 mov [rsp+40],rdx
 cmp r13,-1
 je .Lhead_lookup_query
 or qword ptr [rsp+32],16
 mov [rsp+72],r13
.Lhead_lookup_query:
 mov rdi,rbx
 mov rsi,rsp
 mov rdx,r12
 call .Lscan_first
 jmp .Lhead_lookup_return
.Lhead_lookup_invalid:
 mov eax,2
.Lhead_lookup_return:
 add rsp,112
 pop r13
 pop r12
 pop rbx
 ret
.size pg_record_memory_head,.-pg_record_memory_head
.section .rodata
.Lhead_name: .asciz "head"
.text
/* Internal document states share the provider's sorted global row index. */
.macro DGET object,key
 mov rdi,\object
 lea rsi,[rip+.Ld_\key]
 call json_object_object_get@PLT
.endm
.macro DADD object,key,value
 mov rdi,\object
 lea rsi,[rip+.Ld_\key]
 mov rdx,\value
 call .Ld_attach
.endm
/* Always consumes the supplied owned value. */
.Ld_attach:
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Ld_attach_ok
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,3
 pop rbx
 ret
.Ld_attach_ok:
 xor eax,eax
 pop rbx
 ret
/* Exact JSON string equality with a static C string. */
.Ld_string:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ld_string_return
 mov rdi,rbx
 call json_object_get_string_len@PLT
 mov [rsp],eax
 mov rdi,r12
 call strlen@PLT
 cmp eax,[rsp]
 jne .Ld_string_bad
 mov rdi,rbx
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,r12
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 jmp .Ld_string_return
.Ld_string_bad:
 xor eax,eax
.Ld_string_return:
 add rsp,8
 pop r12
 pop rbx
 ret
/* Positive safe JSON number; zero denotes invalid. */
.Ld_integer:
 push rbx
 mov rbx,rdi
 call json_object_get_type@PLT
 cmp eax,3
 je .Ld_integer_int
 cmp eax,2
 jne .Ld_integer_bad
 mov rdi,rbx
 call json_object_get_double@PLT
 cvttsd2si rax,xmm0
 cvtsi2sd xmm1,rax
 ucomisd xmm0,xmm1
 jp .Ld_integer_bad
 jne .Ld_integer_bad
 jmp .Ld_integer_check
.Ld_integer_int:
 mov rdi,rbx
 call json_object_get_int64@PLT
.Ld_integer_check:
 test rax,rax
 jle .Ld_integer_bad
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Ld_integer_bad
 pop rbx
 ret
.Ld_integer_bad:
 xor eax,eax
 pop rbx
 ret
/* Parse a borrowed record identity into the existing 48-byte native address. */
.Ld_address:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 pxor xmm0,xmm0
 movups [r12],xmm0
 movups [r12+16],xmm0
 movups [r12+32],xmm0
 DGET rbx,kind
 mov r13,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ld_address_bad
 mov rdi,r13
 call json_object_get_string@PLT
 mov [r12],rax
 mov rdi,r13
 call json_object_get_string_len@PLT
 mov [r12+8],rax
 mov rdi,rbx
 lea rsi,[rip+.Ld_key]
 lea rdx,[r12+16]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Ld_address_scope
 mov dword ptr [r12+44],1
 mov r13,[r12+16]
 mov rdi,r13
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ld_address_bad
 mov rdi,r13
 call json_object_get_string@PLT
 mov [r12+16],rax
 mov rdi,r13
 call json_object_get_string_len@PLT
 mov [r12+24],rax
.Ld_address_scope:
 DGET rbx,scope
 mov rbx,rax
 DGET rbx,kind
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Ld_session]
 call .Ld_string
 test eax,eax
 jnz .Ld_address_valid
 mov rdi,r13
 lea rsi,[rip+.Ld_conversation]
 call .Ld_string
 test eax,eax
 jz .Ld_address_task
 mov dword ptr [r12+40],1
 DGET rbx,conversation_id
 jmp .Ld_address_owner
.Ld_address_task:
 mov rdi,r13
 lea rsi,[rip+.Ld_task]
 call .Ld_string
 test eax,eax
 jz .Ld_address_bad
 mov dword ptr [r12+40],2
 DGET rbx,task_id
.Ld_address_owner:
 mov rdi,rax
 call .Ld_integer
 test rax,rax
 jz .Ld_address_bad
 mov [r12+32],rax
.Ld_address_valid:
 mov rdi,r12
 call pg_document_address_valid
 test eax,eax
 jz .Ld_address_bad
 xor eax,eax
 jmp .Ld_address_return
.Ld_address_bad:
 mov eax,2
.Ld_address_return:
 pop r13
 pop r12
 pop rbx
 ret

/* Action: id/create/content/retire/new_state, five qwords; state is owned. */
.Ld_compare:
 mov rax,[rdi]
 cmp rax,[rsi]
 seta al
 setb dl
 movzx eax,al
 movzx edx,dl
 sub eax,edx
 ret

/* memory, detached writes, sequence, result; result owns a normalized array. */
.Lmemory_prepare_documents:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,120
 mov rbx,rdi
 mov r12,rsi
 mov [rsp+16],rdx
 mov [rsp+24],rcx
 mov qword ptr [rcx],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+32],0
 mov qword ptr [rsp+40],0
 mov qword ptr [rsp+48],0
 mov ebp,2
 mov rdi,rbx
 mov rsi,r12
 call .Ld_resolve_copies
 test eax,eax
 jnz .Ld_prepare_status
 mov rdi,r12
 call json_object_array_length@PLT
 mov r14,rax
 mov rdi,rax
 mov esi,40
 call calloc@PLT
 test rax,rax
 jnz .Ld_prepare_array
 test r14,r14
 jnz .Ld_prepare_alloc
.Ld_prepare_array:
 mov [rsp],rax
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Ld_prepare_alloc
 mov [rsp+8],rax
 xor r15d,r15d
.Ld_prepare_each:
 cmp r15,r14
 jae .Ld_prepare_sort
 mov rdi,r12
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov [rsp+56],rax
 DGET rax,type
 mov [rsp+64],rax
 lea rsi,[rip+.Ld_create]
 mov rdi,rax
 call .Ld_string
 test eax,eax
 jnz .Ld_prepare_create
 mov rdi,[rsp+64]
 lea rsi,[rip+.Ld_change]
 call .Ld_string
 test eax,eax
 jnz .Ld_prepare_change
 mov rdi,[rsp+64]
 lea rsi,[rip+.Ld_retire]
 call .Ld_string
 test eax,eax
 jnz .Ld_prepare_retire
 mov rdi,[rsp+64]
 call .Lmemory_table
 cmp eax,3
 ja .Ld_prepare_cleanup
 mov rdi,[rsp+56]
 call json_object_get@PLT
 mov [rsp+72],rax
 mov rdi,[rsp+8]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz .Ld_prepare_next
 mov rdi,[rsp+72]
 call json_object_put@PLT
 jmp .Ld_prepare_alloc
.Ld_prepare_create:
 mov rax,[rsp+32]
 imul rax,40
 add rax,[rsp]
 mov [rsp+80],rax
 mov rdi,[rsp+56]
 lea rsi,[rip+.Ld_record]
 call json_object_object_get@PLT
 mov rcx,[rsp+80]
 mov [rcx+8],rax
 DGET rax,id
 jmp .Ld_prepare_id
.Ld_prepare_change:
 mov rax,[rsp+32]
 imul rax,40
 add rax,[rsp]
 mov [rsp+80],rax
 DGET "qword ptr [rsp+56]",id
 jmp .Ld_prepare_id
.Ld_prepare_retire:
 mov rax,[rsp+32]
 imul rax,40
 add rax,[rsp]
 mov [rsp+80],rax
 mov qword ptr [rax+24],1
 DGET "qword ptr [rsp+56]",id
.Ld_prepare_id:
 mov rdi,rax
 call .Ld_integer
 test rax,rax
 jz .Ld_prepare_cleanup
 mov rcx,[rsp+80]
 mov [rcx],rax
 cmp qword ptr [rcx+24],0
 jne .Ld_prepare_action
 DGET "qword ptr [rsp+56]",content
 mov rcx,[rsp+80]
 mov [rcx+16],rax
 test rax,rax
 jz .Ld_prepare_cleanup
.Ld_prepare_action:
 inc qword ptr [rsp+32]
.Ld_prepare_next:
 inc r15
 jmp .Ld_prepare_each
.Ld_prepare_sort:
 mov rdi,[rsp]
 mov rsi,[rsp+32]
 mov edx,40
 lea rcx,[rip+.Ld_compare]
 call qsort@PLT
 xor r15d,r15d
.Ld_prepare_compact:
 cmp r15,[rsp+32]
 jae .Ld_prepare_build_start
 mov rax,r15
 imul rax,40
 add rax,[rsp]
 mov r12,rax
 mov rcx,[rsp+40]
 test rcx,rcx
 jz .Ld_prepare_compact_new
 dec rcx
 imul rcx,40
 add rcx,[rsp]
 mov rax,[r12]
 cmp rax,[rcx]
 jne .Ld_prepare_compact_new
 mov rax,[r12+16]
 test rax,rax
 jz .Ld_prepare_merge_retire
 cmp qword ptr [rcx+16],0
 jne .Ld_prepare_cleanup
 mov [rcx+16],rax
 mov rax,[r12+8]
 mov [rcx+8],rax
.Ld_prepare_merge_retire:
 mov rax,[r12+24]
 test rax,rax
 jz .Ld_prepare_compact_next
 cmp qword ptr [rcx+24],0
 jne .Ld_prepare_cleanup
 mov [rcx+24],rax
 jmp .Ld_prepare_compact_next
.Ld_prepare_compact_new:
 mov rcx,[rsp+40]
 imul rcx,40
 add rcx,[rsp]
 movups xmm0,[r12]
 movups xmm1,[r12+16]
 movups [rcx],xmm0
 movups [rcx+16],xmm1
 mov rax,[r12+32]
 mov [rcx+32],rax
 inc qword ptr [rsp+40]
.Ld_prepare_compact_next:
 inc r15
 jmp .Ld_prepare_compact
.Ld_prepare_build_start:
 xor r15d,r15d
.Ld_prepare_build:
 cmp r15,[rsp+40]
 jae .Ld_prepare_addresses
 imul r12,r15,40
 add r12,[rsp]
 mov rdi,rbx
 mov rsi,r12
 mov rdx,[rsp+16]
 call .Ld_build_state
 test eax,eax
 jnz .Ld_prepare_status
 inc r15
 jmp .Ld_prepare_build
.Ld_prepare_addresses:
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,[rsp+40]
 call .Ld_check_addresses
 test eax,eax
 jnz .Ld_prepare_status
 xor r15d,r15d
.Ld_prepare_emit:
 cmp r15,[rsp+40]
 jae .Ld_prepare_success
 imul r12,r15,40
 add r12,[rsp]
 call json_object_new_object@PLT
 test rax,rax
 jz .Ld_prepare_alloc
 mov [rsp+48],rax
 lea rdi,[rip+.Ld_prepared]
 call json_object_new_string@PLT
 test rax,rax
 jz .Ld_prepare_alloc
 DADD "qword ptr [rsp+48]",type,rax
 test eax,eax
 jnz .Ld_prepare_alloc
 mov rdi,[r12+32]
 call json_object_get@PLT
 DADD "qword ptr [rsp+48]",value,rax
 test eax,eax
 jnz .Ld_prepare_alloc
 mov rdi,[rsp+8]
 mov rsi,[rsp+48]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ld_prepare_alloc
 mov qword ptr [rsp+48],0
 inc r15
 jmp .Ld_prepare_emit
.Ld_prepare_success:
 mov rcx,[rsp+24]
 mov rax,[rsp+8]
 mov [rcx],rax
 mov qword ptr [rsp+8],0
 xor ebp,ebp
 jmp .Ld_prepare_cleanup
.Ld_prepare_alloc:
 mov ebp,3
 jmp .Ld_prepare_cleanup
.Ld_prepare_status:
 mov ebp,eax
.Ld_prepare_cleanup:
 xor r15d,r15d
.Ld_prepare_free_states:
 cmp r15,[rsp+40]
 jae .Ld_prepare_free
 imul rax,r15,40
 add rax,[rsp]
 mov rdi,[rax+32]
 call json_object_put@PLT
 inc r15
 jmp .Ld_prepare_free_states
.Ld_prepare_free:
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,120
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Allocate one detached replacement state; the provider is unchanged. */
.Ld_build_state:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,152
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],0
 mov qword ptr [rsp+104],0
 mov qword ptr [rsp+112],0
 mov dword ptr [rsp+80],-1
 mov ebp,2
 mov rsi,[r12]
 call .Lmemory_lower
 cmp rax,[rbx+8]
 jae .Ld_build_missing
 shl rax,5
 add rax,[rbx]
 mov rcx,[r12]
 cmp [rax],rcx
 jne .Ld_build_missing
 cmp dword ptr [rax+24],4
 jne .Ld_build_cleanup
 cmp qword ptr [r12+8],0
 jne .Ld_build_cleanup
 mov rax,[rax+8]
 mov [rsp],rax
 DGET rax,record
 mov r15,rax
 mov rdi,rax
 lea rsi,[rip+.Ld_retired_at]
 lea rdx,[rsp+128]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Ld_build_cleanup
 DGET "qword ptr [rsp]",revisions
 mov [rsp+104],rax
 mov rdi,rax
 call json_object_array_length@PLT
 mov [rsp+112],rax
 jmp .Ld_build_record
.Ld_build_missing:
 mov r15,[r12+8]
 test r15,r15
 jz .Ld_build_cleanup
.Ld_build_record:
 mov rdi,r15
 lea rsi,[rsp+32]
 call .Ld_address
 test eax,eax
 jnz .Ld_build_cleanup
 mov dword ptr [rsp+88],1
 cmp dword ptr [rsp+72],1
 jne .Ld_build_copy_record
 DGET r15,history
 mov rbx,rax
 lea rsi,[rip+.Ld_latest]
 mov rdi,rax
 call .Ld_string
 test eax,eax
 jnz .Ld_build_fork
 mov rdi,rbx
 lea rsi,[rip+.Ld_rewindable]
 call .Ld_string
 test eax,eax
 jz .Ld_build_cleanup
 mov dword ptr [rsp+88],0
.Ld_build_fork:
 DGET r15,fork
 mov rbx,rax
 mov rdi,rax
 lea rsi,[rip+.Ld_current]
 call .Ld_string
 test eax,eax
 jnz .Ld_build_copy_record
 mov rdi,rbx
 lea rsi,[rip+.Ld_initial]
 call .Ld_string
 test eax,eax
 jnz .Ld_build_copy_record
 cmp dword ptr [rsp+88],0
 jne .Ld_build_cleanup
 mov rdi,rbx
 lea rsi,[rip+.Ld_asof]
 call .Ld_string
 test eax,eax
 jz .Ld_build_cleanup
.Ld_build_copy_record:
 mov rdi,r15
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ld_build_alloc
 cmp qword ptr [r12+8],0
 je .Ld_build_retire_stamp
 mov rdi,r13
 call json_object_new_int64@PLT
 test rax,rax
 jz .Ld_build_alloc
 DADD "qword ptr [rsp+8]",created_at,rax
 test eax,eax
 jnz .Ld_build_alloc
.Ld_build_retire_stamp:
 cmp qword ptr [r12+24],0
 je .Ld_build_content
 mov rdi,r13
 call json_object_new_int64@PLT
 test rax,rax
 jz .Ld_build_alloc
 DADD "qword ptr [rsp+8]",retired_at,rax
 test eax,eax
 jnz .Ld_build_alloc
.Ld_build_content:
 mov r14,[r12+16]
 test r14,r14
 jz .Ld_build_revisions
 DGET r14,kind
 mov rbx,rax
 lea rsi,[rip+.Ld_base]
 mov rdi,rax
 call .Ld_string
 test eax,eax
 jz .Ld_build_delta
 mov dword ptr [rsp+80],0
 DGET r14,value
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ld_build_cleanup
 jmp .Ld_build_version
.Ld_build_delta:
 mov rdi,rbx
 lea rsi,[rip+.Ld_delta]
 call .Ld_string
 test eax,eax
 jz .Ld_build_cleanup
 cmp qword ptr [r12+8],0
 jne .Ld_build_cleanup
 mov dword ptr [rsp+80],1
 DGET r14,ops
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ld_build_cleanup
.Ld_build_version:
 DGET r14,version
 mov rdi,rax
 call .Ld_integer
 test rax,rax
 jz .Ld_build_cleanup
 mov [rsp+96],rax
 cmp dword ptr [rsp+80],1
 jne .Ld_build_stamp_revision
 mov rsi,[rsp+112]
 test rsi,rsi
 jz .Ld_build_cleanup
 dec rsi
 mov rdi,[rsp+104]
 call json_object_array_get_idx@PLT
 DGET rax,version
 mov rdi,rax
 call .Ld_integer
 cmp rax,[rsp+96]
 jne .Ld_build_cleanup
.Ld_build_stamp_revision:
 mov rdi,r13
 call json_object_new_int64@PLT
 test rax,rax
 jz .Ld_build_alloc
 DADD r14,seq,rax
 test eax,eax
 jnz .Ld_build_alloc
.Ld_build_revisions:
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Ld_build_alloc
 mov [rsp+16],rax
 cmp dword ptr [rsp+88],0
 je .Ld_build_keep_history
 cmp qword ptr [r12+24],0
 jne .Ld_build_state_object
 cmp dword ptr [rsp+80],0
 je .Ld_build_append_content
.Ld_build_keep_history:
 xor r15d,r15d
.Ld_build_copy_revisions:
 cmp r15,[rsp+112]
 jae .Ld_build_append_content
 mov rdi,[rsp+104]
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov rbx,rax
 mov rdi,[rsp+16]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ld_build_revision_failed
 inc r15
 jmp .Ld_build_copy_revisions
.Ld_build_append_content:
 test r14,r14
 jz .Ld_build_state_object
 mov rdi,r14
 call json_object_get@PLT
 mov rbx,rax
 mov rdi,[rsp+16]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ld_build_revision_failed
.Ld_build_state_object:
 call json_object_new_object@PLT
 test rax,rax
 jz .Ld_build_alloc
 mov [rsp+24],rax
 mov rdi,[r12]
 call json_object_new_int64@PLT
 test rax,rax
 jz .Ld_build_alloc
 DADD "qword ptr [rsp+24]",id,rax
 test eax,eax
 jnz .Ld_build_alloc
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 DADD "qword ptr [rsp+24]",record,rdx
 test eax,eax
 jnz .Ld_build_alloc
 mov rdx,[rsp+16]
 mov qword ptr [rsp+16],0
 DADD "qword ptr [rsp+24]",revisions,rdx
 test eax,eax
 jnz .Ld_build_alloc
 mov rax,[rsp+24]
 mov [r12+32],rax
 mov qword ptr [rsp+24],0
 xor ebp,ebp
 jmp .Ld_build_cleanup
.Ld_build_revision_failed:
 mov rdi,rbx
 call json_object_put@PLT
.Ld_build_alloc:
 mov ebp,3
.Ld_build_cleanup:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,152
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Only candidate live incarnations can increase address occupancy. */
.Ld_check_addresses:
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
 xor r14d,r14d
 mov ebp,2
.Ld_check_action:
 cmp r14,r13
 jae .Ld_check_success
 imul rax,r14,40
 add rax,r12
 cmp qword ptr [rax+24],0
 jne .Ld_check_next
 DGET "qword ptr [rax+32]",record
 mov rdi,rax
 mov rsi,rsp
 call .Ld_address
 test eax,eax
 jnz .Ld_check_return
 xor r15d,r15d
.Ld_check_old:
 cmp r15,[rbx+8]
 jae .Ld_check_prior_start
 mov rax,r15
 shl rax,5
 add rax,[rbx]
 cmp dword ptr [rax+24],4
 jne .Ld_check_old_next
 mov rcx,[rax]
 xor edx,edx
.Ld_check_changed:
 cmp rdx,r13
 jae .Ld_check_old_live
 imul rsi,rdx,40
 cmp [r12+rsi],rcx
 je .Ld_check_old_next
 inc rdx
 jmp .Ld_check_changed
.Ld_check_old_live:
 DGET "qword ptr [rax+8]",record
 mov [rsp+112],rax
 DGET rax,retired_at
 test rax,rax
 jnz .Ld_check_old_next
 mov rdi,[rsp+112]
 lea rsi,[rsp+48]
 call .Ld_address
 test eax,eax
 jnz .Ld_check_return
 mov rdi,rsp
 lea rsi,[rsp+48]
 call pg_document_address_equal
 test eax,eax
 jnz .Ld_check_return
.Ld_check_old_next:
 inc r15
 jmp .Ld_check_old
.Ld_check_prior_start:
 xor r15d,r15d
.Ld_check_prior:
 cmp r15,r14
 jae .Ld_check_next
 imul rax,r15,40
 add rax,r12
 cmp qword ptr [rax+24],0
 jne .Ld_check_prior_next
 DGET "qword ptr [rax+32]",record
 mov rdi,rax
 lea rsi,[rsp+48]
 call .Ld_address
 test eax,eax
 jnz .Ld_check_return
 mov rdi,rsp
 lea rsi,[rsp+48]
 call pg_document_address_equal
 test eax,eax
 jnz .Ld_check_return
.Ld_check_prior_next:
 inc r15
 jmp .Ld_check_prior
.Ld_check_next:
 inc r14
 jmp .Ld_check_action
.Ld_check_success:
 xor ebp,ebp
.Ld_check_return:
 mov eax,ebp
 add rsp,136
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.section .rodata
.Ld_type: .asciz "type"
.Ld_id: .asciz "id"
.Ld_record: .asciz "record"
.Ld_content: .asciz "content"
.Ld_value: .asciz "value"
.Ld_revisions: .asciz "revisions"
.Ld_kind: .asciz "kind"
.Ld_key: .asciz "key"
.Ld_scope: .asciz "scope"
.Ld_conversation_id: .asciz "conversationId"
.Ld_task_id: .asciz "taskId"
.Ld_history: .asciz "history"
.Ld_fork: .asciz "fork"
.Ld_version: .asciz "version"
.Ld_ops: .asciz "ops"
.Ld_seq: .asciz "seq"
.Ld_created_at: .asciz "createdAt"
.Ld_retired_at: .asciz "retiredAt"
.Ld_create: .asciz "document.create"
.Ld_change: .asciz "document.change"
.Ld_retire: .asciz "document.retire"
.Ld_prepared: .asciz "document.prepared"
.Ld_session: .asciz "session"
.Ld_conversation: .asciz "conversation"
.Ld_task: .asciz "task"
.Ld_base: .asciz "base"
.Ld_delta: .asciz "delta"
.Ld_latest: .asciz "latest"
.Ld_rewindable: .asciz "rewindable"
.Ld_current: .asciz "current"
.Ld_initial: .asciz "initial"
.Ld_asof: .asciz "asOf"
.text
/* Build a temporary native revision view over borrowed stored JSON. */
.globl pg_record_memory_document
.type pg_record_memory_document,@function
pg_record_memory_document:
 xor r8d,r8d
 jmp .Ld_document_read_common
.Ld_document_read_preparing:
 mov r8d,1
.Ld_document_read_common:
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
 mov r14,rcx
 mov ebp,2
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],0
 mov qword ptr [rsp+32],0
 mov qword ptr [rsp+40],0
 mov qword ptr [rsp+48],0
 test r14,r14
 jz .Ld_read_return
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Ld_read_return
 cmp dword ptr [rbx+44],0
 jne .Ld_read_return
 test r8d,r8d
 jnz .Ld_document_read_allowed
 cmp dword ptr [rbx+40],1
 je .Ld_read_return
.Ld_document_read_allowed:
 cmp qword ptr [rbx+24],0
 je .Ld_read_return
 test r12,r12
 jz .Ld_read_return
 mov rax,9007199254740991
 cmp r12,rax
 ja .Ld_read_return
 cmp r13,-1
 je .Ld_read_lookup
 cmp r13,rax
 ja .Ld_read_return
.Ld_read_lookup:
 mov rsi,r12
 call .Lmemory_lower
 cmp rax,[rbx+8]
 jae .Ld_read_missing
 shl rax,5
 add rax,[rbx]
 cmp [rax],r12
 jne .Ld_read_missing
 cmp dword ptr [rax+24],4
 jne .Ld_read_missing
 DGET "qword ptr [rax+8]",record
 mov [rsp+104],rax
 DGET rax,created_at
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+56],rax
 DGET "qword ptr [rsp+104]",retired_at
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+64],rax
 mov qword ptr [rsp+72],1
 DGET "qword ptr [rsp+104]",scope
 DGET rax,kind
 mov rdi,rax
 lea rsi,[rip+.Ld_conversation]
 call .Ld_string
 test eax,eax
 jz .Ld_read_view
 DGET "qword ptr [rsp+104]",history
 mov rdi,rax
 lea rsi,[rip+.Ld_rewindable]
 call .Ld_string
 test eax,eax
 setz al
 movzx eax,al
 mov [rsp+72],rax
.Ld_read_view:
 /* Lookup again after library calls; stored record remains borrowed. */
 mov rdi,rbx
 mov rsi,r12
 call .Lmemory_lower
 shl rax,5
 add rax,[rbx]
 DGET "qword ptr [rax+8]",revisions
 mov rbx,rax
 mov rdi,rax
 call json_object_array_length@PLT
 mov [rsp+88],rax
 mov rdi,rax
 mov esi,32
 call calloc@PLT
 mov [rsp+80],rax
 test rax,rax
 jnz .Ld_read_decode
 cmp qword ptr [rsp+88],0
 jne .Ld_read_alloc
.Ld_read_decode:
 xor r15d,r15d
.Ld_read_revision:
 cmp r15,[rsp+88]
 jae .Ld_read_replay
 mov rdi,rbx
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov r12,rax
 mov rax,r15
 shl rax,5
 add rax,[rsp+80]
 mov [rsp+112],rax
 DGET r12,seq
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rcx,[rsp+112]
 mov [rcx],rax
 DGET r12,version
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rcx,[rsp+112]
 mov [rcx+8],rax
 DGET r12,kind
 mov rdi,rax
 lea rsi,[rip+.Ld_base]
 call .Ld_string
 test eax,eax
 jz .Ld_read_delta
 DGET r12,value
 jmp .Ld_read_content
.Ld_read_delta:
 mov rcx,[rsp+112]
 mov dword ptr [rcx+16],1
 DGET r12,ops
.Ld_read_content:
 mov rcx,[rsp+112]
 mov [rcx+24],rax
 inc r15
 jmp .Ld_read_revision
.Ld_read_replay:
 lea rdi,[rsp+56]
 mov rsi,r13
 lea rdx,[rsp+24]
 call pg_document_revision_read
 test eax,eax
 jnz .Ld_read_replay_error
 cmp qword ptr [rsp+32],0
 je .Ld_read_missing_cleanup
 call json_object_new_object@PLT
 test rax,rax
 jz .Ld_read_alloc
 mov [rsp],rax
 mov rdi,[rsp+104]
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ld_read_alloc
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 DADD "qword ptr [rsp]",record,rdx
 test eax,eax
 jnz .Ld_read_alloc
 mov rdx,[rsp+24]
 mov qword ptr [rsp+24],0
 DADD "qword ptr [rsp]",value,rdx
 test eax,eax
 jnz .Ld_read_alloc
 mov rdi,[rsp+32]
 call json_object_new_int64@PLT
 test rax,rax
 jz .Ld_read_alloc
 DADD "qword ptr [rsp]",version,rax
 test eax,eax
 jnz .Ld_read_alloc
 mov rdi,[rsp+40]
 call json_object_new_int64@PLT
 test rax,rax
 jz .Ld_read_alloc
 DADD "qword ptr [rsp]",deltas_since_base,rax
 test eax,eax
 jnz .Ld_read_alloc
 mov rax,[rsp]
 mov [r14],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
 jmp .Ld_read_cleanup
.Ld_read_replay_error:
 cmp eax,3
 je .Ld_read_alloc
 jmp .Ld_read_cleanup
.Ld_read_missing:
 mov ebp,1
 jmp .Ld_read_return
.Ld_read_missing_cleanup:
 mov ebp,1
 jmp .Ld_read_cleanup
.Ld_read_alloc:
 mov ebp,3
.Ld_read_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+80]
 call free@PLT
.Ld_read_return:
 mov eax,ebp
 add rsp,136
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_memory_document,.-pg_record_memory_document
.section .rodata
.Ld_deltas_since_base: .asciz "deltasSinceBase"
.text
.globl pg_record_memory_find_document
.type pg_record_memory_find_document,@function
pg_record_memory_find_document:
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
 mov r14,rcx
 mov ebp,2
 mov qword ptr [rsp+48],0
 test r14,r14
 jz .Ld_find_return
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Ld_find_return
 cmp dword ptr [rbx+44],0
 jne .Ld_find_return
 cmp dword ptr [rbx+40],1
 je .Ld_find_return
 cmp qword ptr [rbx+24],0
 je .Ld_find_return
 cmp r13,-1
 je .Ld_find_address
 mov rax,9007199254740991
 cmp r13,rax
 ja .Ld_find_return
.Ld_find_address:
 mov rdi,r12
 call pg_document_address_valid
 test eax,eax
 jz .Ld_find_return
 xor r15d,r15d
.Ld_find_each:
 cmp r15,[rbx+8]
 jae .Ld_find_missing
 mov rax,r15
 shl rax,5
 add rax,[rbx]
 cmp dword ptr [rax+24],4
 jne .Ld_find_next
 DGET "qword ptr [rax+8]",record
 mov [rsp+56],rax
 mov rdi,rax
 mov rsi,rsp
 call .Ld_address
 test eax,eax
 jnz .Ld_find_return
 mov rdi,r12
 mov rsi,rsp
 call pg_document_address_equal
 test eax,eax
 jz .Ld_find_next
 cmp r13,-1
 je .Ld_find_retirement
 DGET "qword ptr [rsp+56]",created_at
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp r13,rax
 jb .Ld_find_next
.Ld_find_retirement:
 DGET "qword ptr [rsp+56]",retired_at
 test rax,rax
 jz .Ld_find_copy
 cmp r13,-1
 je .Ld_find_next
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp r13,rax
 jae .Ld_find_next
.Ld_find_copy:
 mov rdi,[rsp+56]
 lea rsi,[rsp+48]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ld_find_allocation
 mov rax,[rsp+48]
 mov [r14],rax
 xor ebp,ebp
 jmp .Ld_find_return
.Ld_find_allocation:
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov ebp,3
 jmp .Ld_find_return
.Ld_find_next:
 inc r15
 jmp .Ld_find_each
.Ld_find_missing:
 mov ebp,1
.Ld_find_return:
 mov eax,ebp
 add rsp,72
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_memory_find_document,.-pg_record_memory_find_document
.text
/* Record lifetime predicate; does not inspect the retained content policy. */
.Ld_alive:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 cmp r12,-1
 je .Ld_alive_retired
 DGET rbx,created_at
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp r12,rax
 jb .Ld_alive_no
.Ld_alive_retired:
 DGET rbx,retired_at
 test rax,rax
 jz .Ld_alive_yes
 cmp r12,-1
 je .Ld_alive_no
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp r12,rax
 jae .Ld_alive_no
.Ld_alive_yes:
 mov eax,1
 jmp .Ld_alive_return
.Ld_alive_no:
 xor eax,eax
.Ld_alive_return:
 add rsp,8
 pop r12
 pop rbx
 ret

.globl pg_record_memory_scan_documents
.type pg_record_memory_scan_documents,@function
pg_record_memory_scan_documents:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,88
 mov rbx,rdi
 mov r12,rsi
 mov r14,rdx
 mov ebp,2
 mov qword ptr [rsp+48],0
 mov qword ptr [rsp+64],0
 test r14,r14
 jz .Ld_scan_return
 mov qword ptr [r14],0
 mov qword ptr [r14+8],0
 mov qword ptr [r14+16],0
 test rbx,rbx
 jz .Ld_scan_return
 test r12,r12
 jz .Ld_scan_return
 cmp dword ptr [rbx+44],0
 jne .Ld_scan_return
 cmp dword ptr [rbx+40],1
 je .Ld_scan_return
 cmp qword ptr [rbx+24],0
 je .Ld_scan_return
 cmp dword ptr [r12],2
 ja .Ld_scan_return
 cmp dword ptr [r12+4],1
 ja .Ld_scan_return
 cmp dword ptr [r12+56],1
 ja .Ld_scan_return
 cmp dword ptr [r12+60],0
 jne .Ld_scan_return
 cmp qword ptr [r12+40],0
 je .Ld_scan_return
 mov rax,9007199254740991
 cmp qword ptr [r12+32],-1
 je .Ld_scan_owner
 cmp [r12+32],rax
 ja .Ld_scan_return
.Ld_scan_owner:
 mov rcx,[r12+8]
 cmp dword ptr [r12],0
 jne .Ld_scan_owned
 test rcx,rcx
 jnz .Ld_scan_return
 jmp .Ld_scan_cursor
.Ld_scan_owned:
 test rcx,rcx
 jle .Ld_scan_return
 cmp rcx,rax
 ja .Ld_scan_return
.Ld_scan_cursor:
 cmp dword ptr [r12+56],0
 je .Ld_scan_kind
 cmp [r12+48],rax
 jg .Ld_scan_return
 neg rax
 cmp [r12+48],rax
 jl .Ld_scan_return
.Ld_scan_kind:
 cmp dword ptr [r12+4],0
 je .Ld_scan_allocate
 /* Validate UTF-8 slice with the existing address contract. */
 pxor xmm0,xmm0
 movups [rsp],xmm0
 movups [rsp+16],xmm0
 movups [rsp+32],xmm0
 movups xmm0,[r12+16]
 movups [rsp],xmm0
 mov rdi,rsp
 call pg_document_address_valid
 test eax,eax
 jz .Ld_scan_return
.Ld_scan_allocate:
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Ld_scan_allocation
 mov [rsp+48],rax
 xor r15d,r15d
.Ld_scan_each:
 cmp r15,[rbx+8]
 jae .Ld_scan_success
 mov rax,r15
 shl rax,5
 add rax,[rbx]
 cmp dword ptr [rax+24],4
 jne .Ld_scan_next
 cmp dword ptr [r12+56],0
 je .Ld_scan_record
 mov rcx,[r12+48]
 test rcx,rcx
 js .Ld_scan_record
 cmp [rax],rcx
 jbe .Ld_scan_next
.Ld_scan_record:
 DGET "qword ptr [rax+8]",record
 mov r13,rax
 mov rdi,rax
 mov rsi,rsp
 call .Ld_address
 test eax,eax
 jnz .Ld_scan_cleanup
 mov eax,[rsp+40]
 cmp eax,[r12]
 jne .Ld_scan_next
 mov rax,[rsp+32]
 cmp rax,[r12+8]
 jne .Ld_scan_next
 cmp dword ptr [r12+4],0
 je .Ld_scan_lifetime
 mov rdx,[rsp+8]
 cmp rdx,[r12+24]
 jne .Ld_scan_next
 test rdx,rdx
 jz .Ld_scan_lifetime
 mov rdi,[rsp]
 mov rsi,[r12+16]
 call memcmp@PLT
 test eax,eax
 jnz .Ld_scan_next
.Ld_scan_lifetime:
 mov rdi,r13
 mov rsi,[r12+32]
 call .Ld_alive
 test eax,eax
 jz .Ld_scan_next
 mov rax,[rsp+64]
 cmp rax,[r12+40]
 jae .Ld_scan_continuation
 mov rdi,r13
 lea rsi,[rsp+56]
 mov qword ptr [rsp+56],0
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ld_scan_partial_failed
 mov rdi,[rsp+48]
 mov rsi,[rsp+56]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ld_scan_partial_failed
 inc qword ptr [rsp+64]
 DGET r13,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+72],rax
.Ld_scan_next:
 inc r15
 jmp .Ld_scan_each
.Ld_scan_continuation:
 mov rax,[rsp+72]
 mov [r14+8],rax
 mov dword ptr [r14+20],1
.Ld_scan_success:
 mov rax,[rsp+48]
 mov [r14],rax
 mov dword ptr [r14+16],1
 xor ebp,ebp
 jmp .Ld_scan_return
.Ld_scan_partial_failed:
 mov rdi,[rsp+56]
 call json_object_put@PLT
.Ld_scan_allocation:
 mov ebp,3
.Ld_scan_cleanup:
 mov rdi,[rsp+48]
 call json_object_put@PLT
.Ld_scan_return:
 mov eax,ebp
 add rsp,88
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_memory_scan_documents,.-pg_record_memory_scan_documents
.text
/* Return the ID changed by one document command, or zero for other writes. */
.Ld_changed_id:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 DGET rbx,type
 mov r12,rax
 mov rdi,rax
 lea rsi,[rip+.Ld_create]
 call .Ld_string
 test eax,eax
 jnz .Ld_changed_record
 mov rdi,r12
 lea rsi,[rip+.Ld_copy]
 call .Ld_string
 test eax,eax
 jnz .Ld_changed_record
 mov rdi,r12
 lea rsi,[rip+.Ld_change]
 call .Ld_string
 test eax,eax
 jnz .Ld_changed_direct
 mov rdi,r12
 lea rsi,[rip+.Ld_retire]
 call .Ld_string
 test eax,eax
 jz .Ld_changed_return
.Ld_changed_direct:
 DGET rbx,id
 jmp .Ld_changed_number
.Ld_changed_record:
 DGET rbx,record
 DGET rax,id
.Ld_changed_number:
 mov rdi,rax
 call .Ld_integer
.Ld_changed_return:
 add rsp,8
 pop r12
 pop rbx
 ret

/* Resolve copies only in the detached input, before staging any table row. */
.Ld_resolve_copies:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,152
 mov rbx,rdi
 mov r12,rsi
 mov ebp,2
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov rdi,r12
 call json_object_array_length@PLT
 mov r14,rax
 xor r15d,r15d
.Ld_copy_each:
 cmp r15,r14
 jae .Ld_copy_success
 mov rdi,r12
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov r13,rax
 DGET r13,type
 mov rdi,rax
 lea rsi,[rip+.Ld_copy]
 call .Ld_string
 test eax,eax
 jz .Ld_copy_next
 DGET r13,source
 mov [rsp+24],rax
 DGET rax,id
 mov rdi,rax
 call .Ld_integer
 test rax,rax
 jz .Ld_copy_cleanup
 mov [rsp+16],rax
 mov qword ptr [rsp+32],0
.Ld_copy_changed:
 mov rsi,[rsp+32]
 cmp rsi,r14
 jae .Ld_copy_point
 mov rdi,r12
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call .Ld_changed_id
 cmp rax,[rsp+16]
 je .Ld_copy_cleanup
 inc qword ptr [rsp+32]
 jmp .Ld_copy_changed
.Ld_copy_point:
 DGET "qword ptr [rsp+24]",at
 mov [rsp+40],rax
 mov rdi,rax
 lea rsi,[rip+.Ld_current]
 call .Ld_string
 test eax,eax
 jz .Ld_copy_sequence
 mov rdx,-1
 jmp .Ld_copy_read
.Ld_copy_sequence:
 mov rdi,[rsp+40]
 call .Ld_integer
 test rax,rax
 jz .Ld_copy_cleanup
 mov rdx,rax
.Ld_copy_read:
 mov rdi,rbx
 mov rsi,[rsp+16]
 lea rcx,[rsp]
 call .Ld_document_read_preparing
 test eax,eax
 jnz .Ld_copy_read_error
 DGET "qword ptr [rsp]",record
 mov [rsp+24],rax
 mov rdi,rax
 lea rsi,[rsp+48]
 call .Ld_address
 test eax,eax
 jnz .Ld_copy_cleanup
 DGET r13,record
 mov [rsp+40],rax
 mov rdi,rax
 lea rsi,[rsp+96]
 call .Ld_address
 test eax,eax
 jnz .Ld_copy_cleanup
 cmp dword ptr [rsp+88],1
 jne .Ld_copy_cleanup
 cmp dword ptr [rsp+136],1
 jne .Ld_copy_cleanup
 /* A fork changes the owner while retaining definition/family identity. */
 mov rax,[rsp+80]
 mov [rsp+128],rax
 lea rdi,[rsp+48]
 lea rsi,[rsp+96]
 call pg_document_address_equal
 test eax,eax
 jz .Ld_copy_cleanup
 DGET "qword ptr [rsp+24]",history
 mov [rsp+16],rax
 DGET "qword ptr [rsp+40]",history
 mov rdi,rax
 mov rsi,[rsp+16]
 call json_object_equal@PLT
 test eax,eax
 jz .Ld_copy_cleanup
 DGET "qword ptr [rsp+24]",fork
 mov [rsp+16],rax
 DGET "qword ptr [rsp+40]",fork
 mov rdi,rax
 mov rsi,[rsp+16]
 call json_object_equal@PLT
 test eax,eax
 jz .Ld_copy_cleanup
 call json_object_new_object@PLT
 test rax,rax
 jz .Ld_copy_allocation
 mov [rsp+8],rax
 lea rdi,[rip+.Ld_base]
 call json_object_new_string@PLT
 test rax,rax
 jz .Ld_copy_allocation
 DADD "qword ptr [rsp+8]",kind,rax
 test eax,eax
 jnz .Ld_copy_allocation
 DGET "qword ptr [rsp]",version
 mov rdi,rax
 call json_object_get@PLT
 DADD "qword ptr [rsp+8]",version,rax
 test eax,eax
 jnz .Ld_copy_allocation
 DGET "qword ptr [rsp]",value
 mov rdi,rax
 call json_object_get@PLT
 DADD "qword ptr [rsp+8]",value,rax
 test eax,eax
 jnz .Ld_copy_allocation
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 DADD r13,content,rdx
 test eax,eax
 jnz .Ld_copy_allocation
 lea rdi,[rip+.Ld_create]
 call json_object_new_string@PLT
 test rax,rax
 jz .Ld_copy_allocation
 DADD r13,type,rax
 test eax,eax
 jnz .Ld_copy_allocation
 mov rdi,r13
 lea rsi,[rip+.Ld_source]
 call json_object_object_del@PLT
 mov rdi,[rsp]
 call json_object_put@PLT
 mov qword ptr [rsp],0
.Ld_copy_next:
 inc r15
 jmp .Ld_copy_each
.Ld_copy_success:
 xor ebp,ebp
 jmp .Ld_copy_cleanup
.Ld_copy_read_error:
 cmp eax,3
 jne .Ld_copy_cleanup
.Ld_copy_allocation:
 mov ebp,3
.Ld_copy_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,152
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Ld_copy: .asciz "document.copy"
.Ld_source: .asciz "source"
.Ld_at: .asciz "at"
.text
.globl pg_record_memory_queries
.type pg_record_memory_queries,@function
pg_record_memory_queries:
 test rsi,rsi
 jz .Lmemory_queries_return
 mov [rsi],rdi
 mov rax,qword ptr [rip+pg_record_memory_scan@GOTPCREL]
 mov [rsi+8],rax
 mov rax,qword ptr [rip+pg_record_memory_head@GOTPCREL]
 mov [rsi+16],rax
.Lmemory_queries_return:
 ret
.size pg_record_memory_queries,.-pg_record_memory_queries
