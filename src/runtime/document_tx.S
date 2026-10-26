.intel_syntax noprefix
.text
/* Settlement context: entries/count, original hooks, error, cache,
   reservation[32], monotonic time, new-incarnation count, managed scope/scan. */
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
 mov qword ptr [rsp+16],0
.Ldocs_prepare_each:
 cmp r13,[rbx+8]
 jae .Ldocs_validate
 imul r14,r13,152
 add r14,[rbx]
 cmp qword ptr [r14+8],0
 je .Ldocs_prepare_next
 mov rdi,[r14+8]
 lea rsi,[r14+16]
 call pg_document_change_prepare@PLT
 test eax,eax
 jnz .Ldocs_finalize_doc_error
 mov r15,[r14]
 test dword ptr [r14+148],1
 jz .Ldocs_plan_loaded
 mov rdi,r15
 mov rsi,[r14+16]
 lea rdx,[r14+112]
 call .Ldocs_create_write
 test eax,eax
 jnz .Ldocs_finalize_doc_error
 mov dword ptr [r14+144],1
 test dword ptr [r14+148],8
 jz .Ldocs_append_write
 mov rax,[r15+168]
 mov [r14+120],rax
 mov rdi,[r14+112]
 call json_object_get@PLT
 mov [rsp+8],rax
 mov rdi,[r12+24]
 mov rsi,[r14+120]
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jz .Ldocs_prepare_next
 mov rdi,[rsp+8]
 call json_object_put@PLT
 jmp .Ldocs_finalize_allocation
.Ldocs_plan_loaded:
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
 test dword ptr [r14+148],2
 jz .Ldocs_prepare_advance
 mov rax,[r14]
 mov rdi,[rax+72]
 lea rsi,[rsp+16]
 call .Ldocs_retire_write
 test eax,eax
 jnz .Ldocs_finalize_doc_error
 mov rdi,[r12+24]
 mov rsi,[rsp+16]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ldocs_finalize_allocation
 mov qword ptr [rsp+16],0
 mov dword ptr [rbx+44],1
.Ldocs_prepare_advance:
 inc r13
 jmp .Ldocs_prepare_each
.Ldocs_validate:
 xor r13d,r13d
.Ldocs_validate_sources:
 cmp r13,[rbx+8]
 jae .Ldocs_validate_batch
 imul r14,r13,152
 add r14,[rbx]
 test dword ptr [r14+148],8
 jz .Ldocs_validate_source_next
 mov rax,[r14]
 mov rdi,[rax+160]
 lea rsi,[rip+.Lcopy_source]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ldoc_write_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rsi,rax
 mov rdi,r12
 call pg_record_tx_check_copy_source@PLT
 test eax,eax
 jnz .Ldocs_finalize_invalid
.Ldocs_validate_source_next:
 inc r13
 jmp .Ldocs_validate_sources
.Ldocs_validate_batch:
 mov rdi,r12
 call pg_record_tx_validate_batch@PLT
 test eax,eax
 jnz .Ldocs_finalize_invalid
 cmp qword ptr [rbx+104],0
 je .Ldocs_lifecycle_ready
 mov rdi,rbx
 mov rsi,r12
 call .Ldocs_terminal_retire
 test eax,eax
 jnz .Ldocs_finalize_doc_error
 mov rdi,r12
 call pg_record_tx_validate_batch@PLT
 test eax,eax
 jnz .Ldocs_finalize_invalid
.Ldocs_lifecycle_ready:
 xor r13d,r13d
.Ldocs_checkpoint_each:
 cmp r13,[rbx+8]
 jae .Ldocs_finalize_reserve
 imul r14,r13,152
 add r14,[rbx]
 mov r15,[r14]
 cmp qword ptr [r14+8],0
 je .Ldocs_adoption_next
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
 test dword ptr [r14+148],1
 jz .Ldocs_adoption_loaded
 test dword ptr [r14+148],2
 jnz .Ldocs_adoption_next
 mov rax,[r15+152]
 cmp qword ptr [rbx+48],0
 je .Ldocs_install_cache
 cmp rax,[rbx+48]
 jne .Ldocs_finalize_invalid
.Ldocs_install_cache:
 mov [rbx+48],rax
 mov rdi,rax
 mov rsi,[r15+16]
 mov rdx,[r15]
 mov rcx,[r14+16]
 lea r8,[r15+120]
 call pg_document_install_prepare@PLT
 test eax,eax
 jnz .Ldocs_finalize_doc_error
 inc qword ptr [rbx+96]
 jmp .Ldocs_adoption_next
.Ldocs_adoption_loaded:
 lea rdi,[r14+32]
 mov rsi,r15
 lea rdx,[r14+16]
 cmp qword ptr [rbx+120],0
 je .Ldocs_adoption_durable
 mov rax,[r15+88]
 cmp rax,[r15+96]
 jne .Ldocs_adoption_durable
 call pg_document_adoption_prepare_memory@PLT
 jmp .Ldocs_adoption_prepared
.Ldocs_adoption_durable:
 mov ecx,[r14+144]
 call pg_document_adoption_prepare@PLT
.Ldocs_adoption_prepared:
 test eax,eax
 jnz .Ldocs_finalize_doc_error
.Ldocs_adoption_next:
 inc r13
 jmp .Ldocs_checkpoint_each
.Ldocs_finalize_reserve:
 cmp qword ptr [rbx+96],0
 jne .Ldocs_need_reserve
 cmp dword ptr [rbx+44],0
 je .Ldocs_finalize_ok
.Ldocs_need_reserve:
 cmp qword ptr [rbx+48],0
 je .Ldocs_finalize_ok
 mov r15,[rbx+48]
 cmp qword ptr [r15+104],0
 jne .Ldocs_finalize_invalid
 mov dword ptr [r15+104],1
 mov rdi,[r15+64]
 call [r15+72]
 mov [rbx+88],rax
 mov dword ptr [r15+104],0
 mov rdi,r15
 mov rsi,[rbx+96]
 lea rdx,[rbx+56]
 call pg_document_cache_reserve@PLT
 test eax,eax
 jnz .Ldocs_finalize_doc_error
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
 mov rdi,[rsp+16]
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
 jae .Ldocs_retire_begin
 imul r15,r14,152
 add r15,[rbx]
 test dword ptr [r15+148],5
 jnz .Ldocs_adopt_next
 lea rdi,[r15+32]
 call pg_document_adoption_apply@PLT
 test eax,eax
 jnz .Ldocs_adopt_return
.Ldocs_adopt_next:
 inc r14
 jmp .Ldocs_adopt_each
.Ldocs_retire_begin:
 xor r14d,r14d
.Ldocs_retire_each:
 cmp r14,[rbx+8]
 jae .Ldocs_install_begin
 imul r15,r14,152
 add r15,[rbx]
 mov eax,[r15+148]
 and eax,3
 cmp eax,2
 jne .Ldocs_retire_next
 mov rax,[r15]
 mov rdi,[rbx+48]
 lea rsi,[rax+24]
 mov rdx,[rax+72]
 lea rcx,[rbx+56]
 call pg_document_cache_evict_reserved@PLT
 test eax,eax
 jnz .Ldocs_adopt_return
.Ldocs_retire_next:
 inc r14
 jmp .Ldocs_retire_each
.Ldocs_install_begin:
 xor r14d,r14d
.Ldocs_install_each:
 cmp r14,[rbx+8]
 jae .Ldocs_adopt_host
 imul r15,r14,152
 add r15,[rbx]
 mov eax,[r15+148]
 and eax,3
 cmp eax,1
 jne .Ldocs_install_next
 mov rdi,[r15]
 add rdi,120
 lea rsi,[rbx+56]
 mov rdx,r13
 mov rcx,[rbx+88]
 call pg_document_install_apply@PLT
 test eax,eax
 jnz .Ldocs_adopt_return
.Ldocs_install_next:
 inc r14
 jmp .Ldocs_install_each
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
 xor ecx,ecx
 xor r8d,r8d
 xor r9d,r9d
 xor r10d,r10d
 xor r11d,r11d
.Ldocs_commit_common:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,152
 mov [rsp+120],r10
 mov [rsp+128],r11
 mov qword ptr [rsp+136],0
 mov qword ptr [rsp+144],0
 mov [rsp+104],r8
 mov [rsp+112],r9
 mov [rsp+48],rcx
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
 jne .Ldocs_commit_has_edit
 mov eax,[r15+148]
 and eax,-9
 cmp eax,6
 jne .Ldocs_commit_return
.Ldocs_commit_has_edit:
 cmp qword ptr [r15+16],0
 jne .Ldocs_commit_return
 cmp qword ptr [r15+24],0
 jne .Ldocs_commit_return
 cmp qword ptr [r15+112],0
 jne .Ldocs_commit_return
 cmp dword ptr [r15+144],0
 jne .Ldocs_commit_return
 cmp dword ptr [r15+148],15
 ja .Ldocs_commit_return
 test dword ptr [r15+148],6
 jz .Ldocs_commit_flags_ready
 cmp qword ptr [rsp+48],0
 je .Ldocs_commit_return
.Ldocs_commit_flags_ready:
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
 test rdx,rdx
 jz .Ldocs_commit_unique_id
 cmp [r12+rax+8],rdx
 je .Ldocs_commit_return
.Ldocs_commit_unique_id:
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
 pxor xmm0,xmm0
 mov qword ptr [rsp+56],0
 movups [rsp+64],xmm0
 movups [rsp+80],xmm0
 mov qword ptr [rsp+96],0
 mov [r15+24],rsp
 lea rax,[rip+.Ldocs_adopt]
 mov [r15+32],rax
 lea rax,[rip+.Ldocs_publish]
 mov [r15+40],rax
 mov rdi,rbx
 lea rsi,[rip+.Ldocs_finalize]
 mov rdx,rsp
 xor ecx,ecx
 cmp qword ptr [rsp+120],0
 je .Ldocs_commit_route_ready
 lea rcx,[rip+.Ldocs_defer_route]
.Ldocs_commit_route_ready:
 call pg_record_tx_commit_routed@PLT
 mov ebp,eax
 test eax,eax
 jz .Ldocs_commit_restore
 cmp dword ptr [rsp+40],0
 je .Ldocs_commit_restore
 mov ebp,[rsp+40]
.Ldocs_commit_restore:
 mov r12,[rsp]
 mov r13,[rsp+8]
 mov rax,[rsp+16]
 mov [r15+24],rax
 mov rax,[rsp+24]
 mov [r15+32],rax
 mov rax,[rsp+32]
 mov [r15+40],rax
 cmp dword ptr [rsp+144],0
 je .Ldocs_commit_host_done
 mov dword ptr [r15+48],1
 mov rax,[rsp+120]
 mov rdi,[rax]
 mov rsi,[rsp+136]
 call [rax+24]
 mov dword ptr [r15+48],0
.Ldocs_commit_host_done:
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
 mov dword ptr [r15+144],0
 test dword ptr [r15+148],1
 jz .Ldocs_cleanup_next
 mov rdi,[r15]
 add rdi,120
 call pg_document_install_discard@PLT
.Ldocs_cleanup_next:
 inc r14
 jmp .Ldocs_commit_cleanup
.Ldocs_commit_return:
 mov eax,ebp
 add rsp,152
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
 mov qword ptr [rsp+48],0
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
 mov rax,[rbx+24]
 mov [rsp+16],rax
.Ldoc_scope_get_existing:
 cmp qword ptr [rsp+16],0
 je .Ldoc_scope_get_load
 dec qword ptr [rsp+16]
 mov rax,[rsp+16]
 imul rax,152
 add rax,[rbx+16]
 mov [rsp+40],rax
 mov rdi,[rax]
 add rdi,24
 mov rsi,r13
 call pg_document_address_equal@PLT
 test eax,eax
 jnz .Ldoc_scope_get_reuse
 jmp .Ldoc_scope_get_existing
.Ldoc_scope_get_reuse:
 mov rax,[rsp+40]
 test dword ptr [rax+148],2
 jz .Ldoc_scope_get_live
 mov ebp,8
 jmp .Ldoc_scope_get_cleanup
.Ldoc_scope_get_live:
 mov rax,[rax+8]
 mov [r14],rax
 xor ebp,ebp
 jmp .Ldoc_scope_get_cleanup
.Ldoc_scope_get_load:
 mov rdi,rbx
 mov rsi,r12
 mov rdx,r13
 mov rcx,rsp
 call .Ldocs_copy_load
 mov ebp,eax
 cmp eax,8
 je .Ldoc_scope_get_committed
 test eax,eax
 jnz .Ldoc_scope_get_cleanup
 mov qword ptr [rsp+48],9
 jmp .Ldoc_scope_get_edit
.Ldoc_scope_get_committed:
 mov rdi,[rbx+8]
 mov rsi,r12
 mov rdx,r13
 mov rcx,rsp
 call pg_document_cache_get@PLT
 mov ebp,eax
 test eax,eax
 jnz .Ldoc_scope_get_cleanup
.Ldoc_scope_get_edit:
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
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,[rsp+8]
 mov rcx,[rsp+24]
 mov r8,[rsp+32]
 mov r9d,[rsp+48]
 call .Ldoc_scope_append
 mov ebp,eax
 test eax,eax
 jnz .Ldoc_scope_get_cleanup
 mov rax,[rsp+8]
 mov [r14],rax
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+48],0
.Ldoc_scope_get_cleanup:
 cmp qword ptr [rsp+8],0
 je .Ldoc_scope_get_clear
 mov rdi,[rsp+8]
 call pg_document_change_discard@PLT
 mov rdi,[rsp+8]
 call free@PLT
.Ldoc_scope_get_clear:
 cmp qword ptr [rsp+48],0
 je .Ldoc_scope_get_unlock
 mov rdi,[rsp]
 mov rdi,[rdi+160]
 call json_object_put@PLT
 mov rdi,[rsp]
 call pg_document_loaded_close@PLT
 mov rdi,[rsp]
 call free@PLT
.Ldoc_scope_get_unlock:
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
 imul rax,r12,152
 add rax,[rbx+16]
 test dword ptr [rax+148],5
 jz .Ldoc_scope_release_next
 mov r13,[rax]
 test dword ptr [rax+148],8
 jz .Ldoc_scope_release_install
 mov rdi,[r13+160]
 call json_object_put@PLT
.Ldoc_scope_release_install:
 lea rdi,[r13+120]
 call pg_document_install_discard@PLT
 mov rdi,r13
 call pg_document_loaded_close@PLT
 mov rdi,r13
 call free@PLT
.Ldoc_scope_release_next:
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
 xor esi,esi
 jmp pg_document_tx_settle
.size pg_document_tx_finish,.-pg_document_tx_finish
.globl pg_document_tx_settle
.type pg_document_tx_settle,@function
pg_document_tx_settle:
 xor edx,edx
 xor ecx,ecx
 jmp .Ldoc_scope_settle_common
.size pg_document_tx_settle,.-pg_document_tx_settle
.globl pg_document_tx_deferred
.type pg_document_tx_deferred,@function
pg_document_tx_deferred:
 mov eax,1
 test rdx,rdx
 jz .Ldoc_scope_deferred_invalid
 cmp qword ptr [rdx+8],0
 je .Ldoc_scope_deferred_invalid
 cmp qword ptr [rdx+16],0
 je .Ldoc_scope_deferred_invalid
 cmp qword ptr [rdx+24],0
 je .Ldoc_scope_deferred_invalid
 jmp .Ldoc_scope_settle_common
.Ldoc_scope_deferred_invalid:
 ret
.Ldoc_scope_settle_common:
 push rbx
 push r12
 sub rsp,24
 mov [rsp+8],rdx
 mov [rsp+16],rcx
 mov [rsp],rsi
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
 mov rcx,[rbx+8]
 mov r8,rbx
 mov r9,[rsp]
 mov r10,[rsp+8]
 mov r11,[rsp+16]
 call .Ldocs_commit_common
 mov r12d,eax
 mov rdi,rbx
 call .Ldoc_scope_release
 mov eax,r12d
.Ldoc_scope_finish_return:
 add rsp,24
 pop r12
 pop rbx
 ret
.size pg_document_tx_deferred,.-pg_document_tx_deferred

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

.text
/* Append a managed entry; edits/loaded allocations are stable across growth. */
.Ldoc_scope_append:
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
 mov r14,rcx
 mov r15,r8
 mov [rsp],r9d
 mov eax,2
 mov rsi,[rbx+32]
 cmp [rbx+24],rsi
 jb .Ldoc_append_install
 test rsi,rsi
 jnz .Ldoc_append_grow
 mov esi,4
 jmp .Ldoc_append_allocate
.Ldoc_append_grow:
 mov rcx,0x007fffffffffffff
 cmp rsi,rcx
 jae .Ldoc_append_return
 shl rsi,1
.Ldoc_append_allocate:
 mov [rsp+8],rsi
 mov rdi,[rbx+16]
 mov edx,152
 call reallocarray@PLT
 test rax,rax
 jz .Ldoc_append_allocation
 mov [rbx+16],rax
 mov rax,[rsp+8]
 mov [rbx+32],rax
.Ldoc_append_install:
 imul rbp,[rbx+24],152
 add rbp,[rbx+16]
 mov rdi,rbp
 xor eax,eax
 mov ecx,19
 rep stosq
 mov [rbp],r12
 mov [rbp+8],r13
 mov [rbp+128],r14
 mov [rbp+136],r15
 mov eax,[rsp]
 mov [rbp+148],eax
 inc qword ptr [rbx+24]
 xor eax,eax
 jmp .Ldoc_append_return
.Ldoc_append_allocation:
 mov eax,3
.Ldoc_append_return:
 add rsp,24
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Private created handle: loaded[120], install[32], borrowed cache[8].
   Entry.reserved=1 owns this allocation until managed finish/abort. */
.globl pg_document_tx_doc
.type pg_document_tx_doc,@function
pg_document_tx_doc:
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
 test r13,r13
 jz .Ldoc_create_return
 mov qword ptr [r13],0
 test r12,r12
 jz .Ldoc_create_return
 mov rsi,[r12]
 mov rdx,[r12+8]
 mov rcx,[r12+40]
 mov r8,[r12+48]
 mov r9,r13
 call pg_document_tx_get
 mov ebp,eax
 cmp eax,8
 jne .Ldoc_create_return
 mov ebp,1
 cmp qword ptr [r12+32],0
 je .Ldoc_create_return
 cmp qword ptr [r12+24],0
 je .Ldoc_create_return
 mov r15,[rbx]
 mov dword ptr [rbx+40],1
 mov dword ptr [r15+32],1
 mov rdi,r15
 mov rsi,[r12+8]
 call pg_record_tx_require_document_owner@PLT
 test eax,eax
 jnz .Ldoc_create_cleanup
 mov rdi,[r12]
 mov rsi,[r12+16]
 mov rdx,[r12+24]
 lea rcx,[rsp+16]
 call pg_document_initialize@PLT
 mov ebp,eax
 test eax,eax
 jnz .Ldoc_create_cleanup
 mov rdi,[r15+8]
 lea rsi,[rsp+24]
 call [r12+32]
 test eax,eax
 jnz .Ldoc_create_mint_error
 mov ebp,3
 mov edi,1
 mov esi,160
 call calloc@PLT
 test rax,rax
 jz .Ldoc_create_cleanup
 mov r14,rax
 mov [rsp],rax
 mov rcx,[rbx+8]
 mov [rax+152],rcx
 mov rdi,[r12]
 mov rsi,[r12+8]
 mov rdx,[rsp+24]
 mov rcx,rax
 call pg_document_record_create@PLT
 mov ebp,eax
 test eax,eax
 jnz .Ldoc_create_cleanup
 mov rdi,[r14]
 lea rsi,[r14+24]
 call pg_document_record_decode@PLT
 mov ebp,eax
 test eax,eax
 jnz .Ldoc_create_cleanup
 mov rax,[rsp+16]
 mov [r14+8],rax
 mov qword ptr [rsp+16],0
 mov rax,[r12]
 mov [r14+16],rax
 mov rax,[rax+16]
 mov [r14+88],rax
 mov [r14+96],rax
 mov dword ptr [r14+116],1
 mov edi,1
 mov esi,32
 call calloc@PLT
 mov ebp,3
 test rax,rax
 jz .Ldoc_create_cleanup
 mov [rsp+8],rax
 mov rdi,r14
 mov rsi,rax
 call pg_document_loaded_begin@PLT
 mov ebp,eax
 test eax,eax
 jnz .Ldoc_create_cleanup
 mov rdi,rbx
 mov rsi,r14
 mov rdx,[rsp+8]
 mov rcx,[r12+40]
 mov r8,[r12+48]
 mov r9d,1
 call .Ldoc_scope_append
 mov ebp,eax
 test eax,eax
 jnz .Ldoc_create_cleanup
 mov rax,[rsp+8]
 mov [r13],rax
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 jmp .Ldoc_create_cleanup
.Ldoc_create_mint_error:
 mov ebp,9
.Ldoc_create_cleanup:
 mov rdi,[rsp+8]
 test rdi,rdi
 jz .Ldoc_create_loaded_cleanup
 call pg_document_change_discard@PLT
 mov rdi,[rsp+8]
 call free@PLT
.Ldoc_create_loaded_cleanup:
 mov rdi,[rsp]
 test rdi,rdi
 jz .Ldoc_create_seed_cleanup
 call pg_document_loaded_close@PLT
 mov rdi,[rsp]
 call free@PLT
.Ldoc_create_seed_cleanup:
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov dword ptr [r15+32],0
 mov dword ptr [rbx+40],0
.Ldoc_create_return:
 mov eax,ebp
 add rsp,40
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_tx_doc,.-pg_document_tx_doc

.text
/* Consume RAX scalar/reference into RDI object at RSI key. */
.Ldocs_attach:
 test rax,rax
 jz .Ldocs_attach_error
 push rbx
 mov rbx,rax
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Ldocs_attach_done
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,1
.Ldocs_attach_done:
 pop rbx
 ret
.Ldocs_attach_error:
 mov eax,1
 ret

/* New incarnation always stores a base, including an empty edit. */
.Ldocs_create_write:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
 xor r15d,r15d
 mov ebp,3
 mov qword ptr [r13],0
 call json_object_new_object@PLT
 test rax,rax
 jz .Ldocs_create_write_done
 mov r14,rax
 call json_object_new_object@PLT
 test rax,rax
 jz .Ldocs_create_write_done
 mov r15,rax
 lea rdi,[rip+.Ldoc_write_create]
 call json_object_new_string@PLT
 mov rdi,r14
 lea rsi,[rip+.Ldoc_write_type]
 call .Ldocs_attach
 test eax,eax
 jnz .Ldocs_create_write_done
 mov rdi,[rbx]
 call json_object_get@PLT
 mov rdi,r14
 lea rsi,[rip+.Ldoc_write_record]
 call .Ldocs_attach
 test eax,eax
 jnz .Ldocs_create_write_done
 lea rdi,[rip+.Ldoc_write_base]
 call json_object_new_string@PLT
 mov rdi,r15
 lea rsi,[rip+.Ldoc_write_kind]
 call .Ldocs_attach
 test eax,eax
 jnz .Ldocs_create_write_done
 mov rdi,[rbx+96]
 call json_object_new_int64@PLT
 mov rdi,r15
 lea rsi,[rip+.Ldoc_write_version]
 call .Ldocs_attach
 test eax,eax
 jnz .Ldocs_create_write_done
 mov rdi,r12
 call json_object_get@PLT
 mov rdi,r15
 lea rsi,[rip+.Ldoc_write_value]
 call .Ldocs_attach
 test eax,eax
 jnz .Ldocs_create_write_done
 mov rdi,r14
 lea rsi,[rip+.Ldoc_write_content]
 mov rdx,r15
 call json_object_object_add@PLT
 test eax,eax
 jnz .Ldocs_create_write_done
 xor r15d,r15d
 mov [r13],r14
 xor r14d,r14d
 xor ebp,ebp
.Ldocs_create_write_done:
 mov rdi,r14
 call json_object_put@PLT
 mov rdi,r15
 call json_object_put@PLT
 mov eax,ebp
 add rsp,8
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Ldoc_write_type: .asciz "type"
.Ldoc_write_create: .asciz "document.create"
.Ldoc_write_record: .asciz "record"
.Ldoc_write_kind: .asciz "kind"
.Ldoc_write_base: .asciz "base"
.Ldoc_write_version: .asciz "version"
.Ldoc_write_value: .asciz "value"
.Ldoc_write_content: .asciz "content"

.text
.Ldocs_retire_write:
 push rbp
 push rbx
 push r12
 push r13
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
 mov ebp,3
 mov qword ptr [r12],0
 call json_object_new_object@PLT
 test rax,rax
 jz .Ldocs_retire_write_done
 mov r13,rax
 lea rdi,[rip+.Ldoc_write_retire]
 call json_object_new_string@PLT
 mov rdi,r13
 lea rsi,[rip+.Ldoc_write_type]
 call .Ldocs_attach
 test eax,eax
 jnz .Ldocs_retire_write_done
 mov rdi,rbx
 call json_object_new_int64@PLT
 mov rdi,r13
 lea rsi,[rip+.Ldoc_write_id]
 call .Ldocs_attach
 test eax,eax
 jnz .Ldocs_retire_write_done
 mov [r12],r13
 xor r13d,r13d
 xor ebp,ebp
.Ldocs_retire_write_done:
 mov rdi,r13
 call json_object_put@PLT
 mov eax,ebp
 add rsp,8
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Ldoc_write_retire: .asciz "document.retire"
.Ldoc_write_id: .asciz "id"

.text
.globl pg_document_tx_retire
.type pg_document_tx_retire,@function
pg_document_tx_retire:
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
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+16],6
 test rbx,rbx
 jz .Ldoc_retire_return
 test r12,r12
 jz .Ldoc_retire_return
 cmp qword ptr [rbx+40],0
 jne .Ldoc_retire_return
 mov r15,[rbx]
 test r15,r15
 jz .Ldoc_retire_return
 cmp qword ptr [r15+32],0
 jne .Ldoc_retire_return
 cmp qword ptr [r15+24],0
 je .Ldoc_retire_return
 mov rdi,r13
 call pg_document_address_valid@PLT
 test eax,eax
 jz .Ldoc_retire_return
 mov eax,[r12+24]
 cmp eax,[r13+40]
 jne .Ldoc_retire_return
 mov eax,[r12+36]
 cmp eax,[r13+44]
 jne .Ldoc_retire_return
 mov rdx,[r12+8]
 cmp rdx,[r13+8]
 jne .Ldoc_retire_return
 test rdx,rdx
 jz .Ldoc_retire_identity
 mov rdi,[r12]
 test rdi,rdi
 jz .Ldoc_retire_return
 mov rsi,[r13]
 call memcmp@PLT
 test eax,eax
 jnz .Ldoc_retire_return
.Ldoc_retire_identity:
 mov dword ptr [rbx+40],1
 mov dword ptr [r15+32],1
 mov r14,[rbx+24]
.Ldoc_retire_find:
 test r14,r14
 jz .Ldoc_retire_lookup
 dec r14
 imul rax,r14,152
 add rax,[rbx+16]
 mov [rsp+8],rax
 mov rdi,[rax]
 add rdi,24
 mov rsi,r13
 call pg_document_address_equal@PLT
 test eax,eax
 jz .Ldoc_retire_find
 mov rax,[rsp+8]
 or dword ptr [rax+148],2
 xor ebp,ebp
 jmp .Ldoc_retire_cleanup
.Ldoc_retire_lookup:
 mov rdi,r15
 mov rsi,r13
 lea rdx,[rsp+8]
 lea rcx,[rsp+16]
 call .Ldocs_copy_find
 mov ebp,eax
 cmp eax,8
 je .Ldoc_retire_committed
 test eax,eax
 jnz .Ldoc_retire_cleanup
 mov edi,1
 mov esi,176
 call calloc@PLT
 mov ebp,3
 test rax,rax
 jz .Ldoc_retire_cleanup
 mov [rsp],rax
 mov r14,rax
 mov rax,[rsp+16]
 mov [r14+168],rax
 mov qword ptr [rsp+16],14
 mov rdi,[rsp+8]
 call json_object_get@PLT
 mov [r14+160],rax
 mov rdi,rax
 lea rsi,[rip+.Ldoc_write_record]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [r14],rax
 jmp .Ldoc_retire_record
.Ldoc_retire_committed:
 mov qword ptr [rsp+16],6
 mov edi,1
 mov esi,160
 call calloc@PLT
 mov ebp,3
 test rax,rax
 jz .Ldoc_retire_cleanup
 mov [rsp],rax
 mov rdi,[rbx+8]
 mov rsi,r13
 mov rdx,rax
 call pg_document_cache_find_record@PLT
 mov ebp,eax
 cmp eax,8
 je .Ldoc_retire_missing
 test eax,eax
 jnz .Ldoc_retire_cleanup
.Ldoc_retire_record:
 mov r14,[rsp]
 mov rdi,[r14]
 lea rsi,[r14+24]
 call pg_document_record_decode@PLT
 mov ebp,eax
 test eax,eax
 jnz .Ldoc_retire_cleanup
 lea rdi,[r14+24]
 mov rsi,r13
 call pg_document_address_equal@PLT
 mov ebp,4
 test eax,eax
 jz .Ldoc_retire_cleanup
 mov rdi,r12
 lea rsi,[r14+24]
 mov rdx,[r12+16]
 call pg_document_check@PLT
 mov ebp,eax
 test eax,eax
 jnz .Ldoc_retire_cleanup
 mov [r14+16],r12
 mov rax,[rbx+8]
 mov [r14+152],rax
 mov rdi,rbx
 mov rsi,r14
 xor edx,edx
 xor ecx,ecx
 xor r8d,r8d
 mov r9d,[rsp+16]
 call .Ldoc_scope_append
 mov ebp,eax
 test eax,eax
 jnz .Ldoc_retire_cleanup
 mov qword ptr [rsp],0
 jmp .Ldoc_retire_cleanup
.Ldoc_retire_missing:
 xor ebp,ebp
.Ldoc_retire_cleanup:
 mov rdi,[rsp]
 test rdi,rdi
 jz .Ldoc_retire_unlock
 test dword ptr [rsp+16],8
 jz .Ldoc_retire_close_loaded
 mov rdi,[rdi+160]
 call json_object_put@PLT
 mov rdi,[rsp]
.Ldoc_retire_close_loaded:
 call pg_document_loaded_close@PLT
 mov rdi,[rsp]
 call free@PLT
.Ldoc_retire_unlock:
 mov dword ptr [r15+32],0
 mov dword ptr [rbx+40],0
.Ldoc_retire_return:
 mov eax,ebp
 add rsp,24
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_tx_retire,.-pg_document_tx_retire

.text
/* Append a retirement once; context flags retain reservation ownership. */
.Ldocs_terminal_emit:
 push rbx
 push r12
 push r13
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rsp],0
 xor eax,eax
 test dword ptr [r13+148],2
 jnz .Lterminal_emit_return
 mov rax,[r13]
 mov rdi,[rax+72]
 mov rsi,rsp
 call .Ldocs_retire_write
 test eax,eax
 jnz .Lterminal_emit_return
 mov rdi,[r12+24]
 mov rsi,[rsp]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lterminal_emit_error
 or dword ptr [r13+148],2
 mov dword ptr [rbx+44],1
 xor eax,eax
 jmp .Lterminal_emit_return
.Lterminal_emit_error:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,3
.Lterminal_emit_return:
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret

/* Stack: task cursor/candidate/committed/id, index, query[64], page[24],
   page index/last ID, private node, spare, spare, seen-ID linkhash.
   Seen-ID keys encode safe IDs as opaque uintptr_t values, never dereferenced. */
.Ldocs_terminal_retire:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,184
 mov rbx,rdi
 mov r12,rsi
 mov r13,[rbx+104]
 mov r14,[rbx+112]
 mov qword ptr [rsp],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+104],0
 mov qword ptr [rsp+144],0
 mov qword ptr [rsp+176],0
 mov ebp,1
.Lterminal_task_next:
 mov rdi,r12
 mov rsi,rsp
 lea rdx,[rsp+8]
 call pg_record_tx_terminal_next@PLT
 test eax,eax
 jnz .Lterminal_tasks_ok
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ldoc_write_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 test rax,rax
 jz .Lterminal_tasks_done
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lterminal_tasks_done
 mov [rsp+24],rax
 cmp qword ptr [r12+16],0
 je .Lterminal_tasks_done
 mov rdi,[r12+8]
 mov esi,2
 mov rdx,rax
 lea rcx,[rsp+16]
 call [r12+16]
 cmp eax,1
 je .Lterminal_owner_missing
 test eax,eax
 jnz .Lterminal_tasks_storage
 cmp qword ptr [rsp+16],0
 je .Lterminal_tasks_storage
 mov rdi,[rsp+16]
 lea rsi,[rip+.Ldoc_write_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rsp+24]
 jne .Lterminal_tasks_done
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lterminal_state]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lterminal_status]
 call json_object_object_get@PLT
 mov r15,rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 cmp eax,8
 jne .Lterminal_committed_open
 mov rdi,r15
 call json_object_get_string@PLT
 test rax,rax
 jz .Lterminal_tasks_done
 mov rdi,rax
 lea rsi,[rip+.Lterminal_name]
 mov edx,8
 call memcmp@PLT
 test eax,eax
 jz .Lterminal_tasks_done
.Lterminal_committed_open:
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lterminal_conversation]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+168],rax
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lterminal_conversation]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rsp+168]
 jne .Lterminal_tasks_done
 jmp .Lterminal_owner_new
.Lterminal_owner_missing:
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov qword ptr [rsp+16],0
.Lterminal_owner_new:
 cmp qword ptr [rsp+176],0
 jne .Lterminal_mark_begin
 mov edi,16
 xor esi,esi
 call lh_kptr_table_new@PLT
 test rax,rax
 jz .Lterminal_tasks_allocation
 mov [rsp+176],rax
 mov qword ptr [rsp+32],0
.Lterminal_index_each:
 mov rax,[rsp+32]
 cmp rax,[r13+24]
 jae .Lterminal_mark_begin
 imul rax,152
 add rax,[r13+16]
 mov rax,[rax]
 mov rsi,[rax+72]
 mov rdi,[rsp+176]
 mov edx,1
 call lh_table_insert@PLT
 test eax,eax
 jnz .Lterminal_tasks_allocation
 inc qword ptr [rsp+32]
 jmp .Lterminal_index_each
.Lterminal_mark_begin:
 mov qword ptr [rsp+32],0
.Lterminal_mark_each:
 mov rax,[rsp+32]
 cmp rax,[r13+24]
 jae .Lterminal_scan_begin
 imul rax,152
 add rax,[r13+16]
 mov rdx,[rax]
 cmp dword ptr [rdx+64],2
 jne .Lterminal_mark_next
 mov rcx,[rsp+24]
 cmp [rdx+56],rcx
 jne .Lterminal_mark_next
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call .Ldocs_terminal_emit
 test eax,eax
 jnz .Lterminal_tasks_allocation
.Lterminal_mark_next:
 inc qword ptr [rsp+32]
 jmp .Lterminal_mark_each
.Lterminal_scan_begin:
 cmp qword ptr [rsp+16],0
 je .Lterminal_task_cleanup
 test r14,r14
 jz .Lterminal_tasks_done
 cmp qword ptr [r14+8],0
 je .Lterminal_tasks_done
 lea rdi,[rsp+40]
 xor eax,eax
 mov ecx,8
 rep stosq
 mov dword ptr [rsp+40],2
 mov rax,[rsp+24]
 mov [rsp+48],rax
 mov qword ptr [rsp+72],-1
 mov qword ptr [rsp+80],256
.Lterminal_scan_page:
 pxor xmm0,xmm0
 movups [rsp+104],xmm0
 mov qword ptr [rsp+120],0
 mov rdi,[r14]
 lea rsi,[rsp+40]
 lea rdx,[rsp+104]
 call [r14+8]
 test eax,eax
 jnz .Lterminal_tasks_storage
 mov rdi,[rsp+104]
 call json_object_get_type@PLT
 cmp eax,5
 jne .Lterminal_tasks_done
 cmp dword ptr [rsp+124],1
 ja .Lterminal_tasks_done
 mov rdi,[rsp+104]
 call json_object_array_length@PLT
 cmp rax,256
 ja .Lterminal_tasks_done
 mov rax,[rsp+88]
 mov [rsp+136],rax
 mov qword ptr [rsp+128],0
.Lterminal_scan_item:
 mov rdi,[rsp+104]
 call json_object_array_length@PLT
 mov rsi,[rsp+128]
 cmp rsi,rax
 jae .Lterminal_scan_advance
 mov rdi,[rsp+104]
 call json_object_array_get_idx@PLT
 mov r15,rax
 mov rdi,rax
 lea rsi,[rip+.Ldoc_write_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rsp+136]
 jbe .Lterminal_tasks_done
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lterminal_tasks_done
 mov [rsp+136],rax
 mov rdi,[rsp+176]
 mov rsi,rax
 lea rdx,[rsp+160]
 call lh_table_lookup_ex@PLT
 test eax,eax
 jnz .Lterminal_scan_item_next
 mov edi,1
 mov esi,160
 call calloc@PLT
 test rax,rax
 jz .Lterminal_tasks_allocation
 mov [rsp+144],rax
 mov rdi,r15
 call json_object_get@PLT
 mov rcx,[rsp+144]
 mov [rcx],rax
 mov rdi,rax
 lea rsi,[rcx+24]
 call pg_document_record_decode@PLT
 test eax,eax
 jnz .Lterminal_tasks_done
 mov rax,[rsp+144]
 cmp dword ptr [rax+64],2
 jne .Lterminal_tasks_done
 mov rcx,[rsp+24]
 cmp [rax+56],rcx
 jne .Lterminal_tasks_done
 mov rcx,[rbx+48]
 mov [rax+152],rcx
 mov rdi,r13
 mov rsi,rax
 xor edx,edx
 xor ecx,ecx
 xor r8d,r8d
 mov r9d,4
 call .Ldoc_scope_append
 cmp eax,3
 je .Lterminal_tasks_allocation
 test eax,eax
 jnz .Lterminal_tasks_done
 mov qword ptr [rsp+144],0
 mov rax,[r13+16]
 mov [rbx],rax
 mov rax,[r13+24]
 mov [rbx+8],rax
 dec rax
 imul rax,152
 add rax,[r13+16]
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call .Ldocs_terminal_emit
 test eax,eax
 jnz .Lterminal_tasks_allocation
 mov rdi,[rsp+176]
 mov rsi,[rsp+136]
 mov edx,1
 call lh_table_insert@PLT
 test eax,eax
 jnz .Lterminal_tasks_allocation
.Lterminal_scan_item_next:
 inc qword ptr [rsp+128]
 jmp .Lterminal_scan_item
.Lterminal_scan_advance:
 cmp dword ptr [rsp+124],0
 je .Lterminal_page_last
 cmp qword ptr [rsp+128],0
 je .Lterminal_tasks_done
 mov rax,[rsp+112]
 cmp rax,[rsp+88]
 jbe .Lterminal_tasks_done
 cmp rax,[rsp+136]
 jb .Lterminal_tasks_done
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lterminal_tasks_done
 mov [rsp+88],rax
 mov dword ptr [rsp+96],1
 mov rdi,[rsp+104]
 call json_object_put@PLT
 mov qword ptr [rsp+104],0
 jmp .Lterminal_scan_page
.Lterminal_page_last:
 mov rdi,[rsp+104]
 call json_object_put@PLT
 mov qword ptr [rsp+104],0
.Lterminal_task_cleanup:
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov qword ptr [rsp+16],0
 jmp .Lterminal_task_next
.Lterminal_tasks_ok:
 xor ebp,ebp
 jmp .Lterminal_tasks_done
.Lterminal_tasks_storage:
 mov ebp,9
 jmp .Lterminal_tasks_done
.Lterminal_tasks_allocation:
 mov ebp,3
.Lterminal_tasks_done:
 mov rdi,[rsp+144]
 test rdi,rdi
 jz .Lterminal_tasks_page_cleanup
 call pg_document_loaded_close@PLT
 mov rdi,[rsp+144]
 call free@PLT
.Lterminal_tasks_page_cleanup:
 mov rdi,[rsp+104]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+176]
 test rdi,rdi
 jz .Lterminal_tasks_return
 call lh_table_free@PLT
.Lterminal_tasks_return:
 mov eax,ebp
 add rsp,184
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lterminal_state: .asciz "state"
.Lterminal_status: .asciz "status"
.Lterminal_name: .asciz "terminal"
.Lterminal_conversation: .asciz "conversationId"

.text
/* Locate a borrowed staged copy; its array index remains stable until settle. */
.Ldocs_copy_find:
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
 mov ebp,8
 mov qword ptr [r13],0
 mov qword ptr [r14],0
 cmp dword ptr [r12+40],1
 jne .Lcopy_find_return
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 mov r15,rax
.Lcopy_find_each:
 test r15,r15
 jz .Lcopy_find_return
 dec r15
 mov rdi,[rbx+24]
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov [rsp+64],rax
 mov rdi,rax
 lea rsi,[rip+.Ldoc_write_type]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string_len@PLT
 cmp eax,13
 jne .Lcopy_find_each
 mov rdi,[rsp+64]
 lea rsi,[rip+.Ldoc_write_type]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lcopy_find_each
 mov rdi,rax
 lea rsi,[rip+.Lcopy_name]
 mov edx,13
 call memcmp@PLT
 test eax,eax
 jnz .Lcopy_find_each
 mov rdi,[rsp+64]
 lea rsi,[rip+.Ldoc_write_record]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,rsp
 call pg_document_record_decode@PLT
 test eax,eax
 jnz .Lcopy_find_invalid
 mov rdi,rsp
 mov rsi,r12
 call pg_document_address_equal@PLT
 test eax,eax
 jz .Lcopy_find_each
 mov rax,[rsp+64]
 mov [r13],rax
 mov [r14],r15
 xor ebp,ebp
 jmp .Lcopy_find_return
.Lcopy_find_invalid:
 mov ebp,1
.Lcopy_find_return:
 mov eax,ebp
 add rsp,72
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Created copy handle: loaded/install/cache, owned original write, index. */
.Ldocs_copy_load:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,120
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 xor r15d,r15d
 mov qword ptr [r14],0
 mov qword ptr [rsp+16],0
 mov rdi,[rbx]
 mov rsi,r13
 mov rdx,rsp
 lea rcx,[rsp+8]
 call .Ldocs_copy_find
 mov ebp,eax
 test eax,eax
 jnz .Lcopy_load_done
 mov edi,1
 mov esi,176
 call calloc@PLT
 test rax,rax
 jz .Lcopy_load_allocation
 mov r15,rax
 mov rcx,[rbx+8]
 mov [rax+152],rcx
 mov rax,[rsp+8]
 mov [r15+168],rax
 mov rdi,[rsp]
 call json_object_get@PLT
 mov [r15+160],rax
 mov rdi,rax
 lea rsi,[rip+.Ldoc_write_record]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [r15],rax
 mov rdi,rax
 lea rsi,[r15+24]
 call pg_document_record_decode@PLT
 mov ebp,eax
 test eax,eax
 jnz .Lcopy_load_done
 mov rdi,[rsp]
 lea rsi,[rip+.Lcopy_source]
 call json_object_object_get@PLT
 mov [rsp+112],rax
 mov rdi,rax
 lea rsi,[rip+.Ldoc_write_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Lcopy_load_invalid
 test rax,rax
 jz .Lcopy_load_invalid
 mov [rsp+24],rax
 mov rdi,[rsp+112]
 lea rsi,[rip+.Lcopy_at]
 call json_object_object_get@PLT
 mov [rsp+112],rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,6
 je .Lcopy_load_current
 mov rdi,[rsp+112]
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Lcopy_load_invalid
 jmp .Lcopy_load_point
.Lcopy_load_current:
 mov rdi,[rsp+112]
 call json_object_get_string_len@PLT
 cmp eax,7
 jne .Lcopy_load_invalid
 mov rdi,[rsp+112]
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcopy_current]
 mov edx,7
 call memcmp@PLT
 test eax,eax
 jnz .Lcopy_load_invalid
 mov rax,-1
.Lcopy_load_point:
 mov [rsp+32],rax
 mov rax,[rbx+8]
 mov rdi,[rax+40]
 mov rsi,[rsp+24]
 mov rdx,[rsp+32]
 lea rcx,[rsp+16]
 call [rax+56]
 test eax,eax
 jnz .Lcopy_load_storage
 cmp qword ptr [rsp+16],0
 je .Lcopy_load_storage
 mov rdi,[rsp+16]
 lea rsi,[rip+.Ldoc_write_record]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rsp+40]
 call pg_document_record_decode@PLT
 mov ebp,eax
 test eax,eax
 jnz .Lcopy_load_done
 mov rax,[rsp+88]
 cmp rax,[rsp+24]
 jne .Lcopy_load_semantics
 cmp dword ptr [rsp+80],1
 jne .Lcopy_load_semantics
 cmp dword ptr [r15+64],1
 jne .Lcopy_load_semantics
 mov rax,[rsp+96]
 cmp rax,[r15+80]
 jne .Lcopy_load_semantics
 mov rax,[r15+56]
 mov [rsp+72],rax
 lea rdi,[rsp+40]
 lea rsi,[r15+24]
 call pg_document_address_equal@PLT
 test eax,eax
 jz .Lcopy_load_semantics
 mov rdi,[rsp+16]
 lea rsi,[rip+.Ldoc_write_version]
 call json_object_object_get@PLT
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Lcopy_load_invalid
 test rax,rax
 jz .Lcopy_load_invalid
 mov [rsp+104],rax
 mov rdi,[rsp+16]
 lea rsi,[rip+.Ldoc_write_value]
 call json_object_object_get@PLT
 mov rcx,rax
 mov rdi,r12
 lea rsi,[r15+24]
 mov rdx,[rsp+104]
 lea r8,[r15+8]
 call pg_document_materialize@PLT
 mov ebp,eax
 test eax,eax
 jnz .Lcopy_load_done
 mov [r15+16],r12
 mov rax,[r12+16]
 mov [r15+88],rax
 mov [r15+96],rax
 mov dword ptr [r15+116],1
 mov [r14],r15
 xor r15d,r15d
 xor ebp,ebp
 jmp .Lcopy_load_done
.Lcopy_load_storage:
 mov ebp,9
 jmp .Lcopy_load_done
.Lcopy_load_semantics:
 mov ebp,4
 jmp .Lcopy_load_done
.Lcopy_load_invalid:
 mov ebp,1
 jmp .Lcopy_load_done
.Lcopy_load_allocation:
 mov ebp,3
.Lcopy_load_done:
 test r15,r15
 jz .Lcopy_load_stored_cleanup
 mov rdi,[r15+160]
 call json_object_put@PLT
 mov rdi,r15
 call pg_document_loaded_close@PLT
 mov rdi,r15
 call free@PLT
.Lcopy_load_stored_cleanup:
 mov rdi,[rsp+16]
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
.section .rodata
.Lcopy_name: .asciz "document.copy"
.Lcopy_source: .asciz "source"
.Lcopy_at: .asciz "at"
.Lcopy_current: .asciz "current"

.text
/* Memory-only routing reuses prepared value guards, then queues its ops through
   the injected hold service. No Storage/durable publication hooks run here. */
.Ldocs_defer_route:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 mov r15,[rbx+120]
 mov rdi,[r12+24]
 call json_object_array_length@PLT
 mov [rsp],rax
 xor r13d,r13d
 xor r14d,r14d
.Ldefer_eligible_each:
 cmp r13,[rbx+8]
 jae .Ldefer_eligible_count
 imul rax,r13,152
 add rax,[rbx]
 test dword ptr [rax+148],2
 jnz .Ldefer_fallback
 cmp qword ptr [rax+112],0
 je .Ldefer_eligible_next
 cmp qword ptr [rax+8],0
 je .Ldefer_fallback
 test dword ptr [rax+148],1
 jnz .Ldefer_fallback
 mov rdx,[rax]
 cmp dword ptr [rdx+64],1
 jne .Ldefer_fallback
 cmp dword ptr [rdx+80],1
 jne .Ldefer_fallback
 cmp dword ptr [rdx+84],2
 jne .Ldefer_fallback
 mov rcx,[rdx+88]
 cmp rcx,[rdx+96]
 jne .Ldefer_fallback
 inc r14
.Ldefer_eligible_next:
 inc r13
 jmp .Ldefer_eligible_each
.Ldefer_eligible_count:
 cmp r14,[rsp]
 jne .Ldefer_fallback
 test r14,r14
 jz .Ldefer_fallback
 xor r13d,r13d
.Ldefer_retarget_each:
 cmp r13,[rbx+8]
 jae .Ldefer_host_prepare
 imul rax,r13,152
 add rax,[rbx]
 cmp qword ptr [rax+112],0
 je .Ldefer_retarget_next
 lea rdi,[rax+32]
 call pg_document_adoption_defer@PLT
 test eax,eax
 jnz .Ldefer_invalid
.Ldefer_retarget_next:
 inc r13
 jmp .Ldefer_retarget_each
.Ldefer_host_prepare:
 mov dword ptr [rbx+144],1
 mov rdi,[r15]
 mov rsi,[rbx]
 mov rdx,[rbx+8]
 mov rcx,[rbx+128]
 lea r8,[rbx+136]
 call [r15+8]
 cmp eax,3
 je .Ldefer_allocation
 test eax,eax
 jnz .Ldefer_invalid
 mov rax,[r12]
 cmp dword ptr [rax+52],0
 jne .Ldefer_adoption_error
 xor r13d,r13d
.Ldefer_adopt_each:
 cmp r13,[rbx+8]
 jae .Ldefer_host_apply
 imul rax,r13,152
 add rax,[rbx]
 cmp qword ptr [rax+112],0
 je .Ldefer_adopt_next
 lea rdi,[rax+32]
 call pg_document_adoption_apply@PLT
 test eax,eax
 jnz .Ldefer_adoption_error
.Ldefer_adopt_next:
 inc r13
 jmp .Ldefer_adopt_each
.Ldefer_host_apply:
 mov rdi,[r15]
 mov rsi,[rbx+136]
 call [r15+16]
 test eax,eax
 jnz .Ldefer_adoption_error
 cmp qword ptr [r15+32],0
 je .Ldefer_ok
 mov rdi,[r15]
 mov rsi,[rbx]
 mov rdx,[rbx+8]
 call [r15+32]
.Ldefer_ok:
 mov eax,1
 jmp .Ldefer_return
.Ldefer_fallback:
 xor r13d,r13d
.Ldefer_persist_each:
 cmp r13,[rbx+8]
 jae .Ldefer_persist_ready
 imul rax,r13,152
 add rax,[rbx]
 test dword ptr [rax+148],5
 jnz .Ldefer_persist_next
 cmp qword ptr [rax+8],0
 je .Ldefer_persist_next
 mov rdx,[rax+72]
 cmp dword ptr [rax+144],1
 je .Ldefer_persist_base
 cmp dword ptr [rax+144],2
 jne .Ldefer_persist_count
 mov rcx,9007199254740991
 cmp rdx,rcx
 jae .Ldefer_invalid
 inc rdx
 jmp .Ldefer_persist_count
.Ldefer_persist_base:
 xor edx,edx
.Ldefer_persist_count:
 mov [rax+96],rdx
 mov rcx,[rax+64]
 mov [rax+88],rcx
.Ldefer_persist_next:
 inc r13
 jmp .Ldefer_persist_each
.Ldefer_persist_ready:
 xor eax,eax
 jmp .Ldefer_return
.Ldefer_adoption_error:
 mov rax,[r12]
 cmp dword ptr [rax+52],0
 jne .Ldefer_adoption_failed
 mov dword ptr [rax+52],4
.Ldefer_adoption_failed:
 mov dword ptr [rbx+40],6
 mov eax,2
 jmp .Ldefer_return
.Ldefer_allocation:
 mov dword ptr [rbx+40],4
 mov eax,2
 jmp .Ldefer_return
.Ldefer_invalid:
 mov dword ptr [rbx+40],1
 mov eax,2
.Ldefer_return:
 add rsp,24
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
