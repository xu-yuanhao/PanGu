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
