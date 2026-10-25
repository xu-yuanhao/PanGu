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
 cmp r13d,4
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
 mov rdi,[rsp]
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
 cmp eax,3
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
.Ltable_names: .long .Lconversation-.Ltable_names,.Lentry-.Ltable_names,.Ltask-.Ltable_names,.Lsubmission-.Ltable_names
.Ltable_lengths: .long 12,5,4,10
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
