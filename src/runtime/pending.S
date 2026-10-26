/* Source: Pi d5b3de22 PendingOps/storeHeld; MIT notice: docs/licenses/pi.md. */
.intel_syntax noprefix
.text
/* Read a decoded tuple's one-byte opcode, or -1 for malformed shape. */
.Lpending_code:
 push rbx
 mov rbx,rdi
 call json_object_get_type@PLT
 cmp eax,5
 jne .Lpending_code_bad
 mov rdi,rbx
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rbx,rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,6
 jne .Lpending_code_bad
 mov rdi,rbx
 call json_object_get_string_len@PLT
 cmp eax,1
 jne .Lpending_code_bad
 mov rdi,rbx
 call json_object_get_string@PLT
 movzx eax,byte ptr [rax]
 pop rbx
 ret
.Lpending_code_bad:
 mov eax,-1
 pop rbx
 ret

/* Append a borrowed JSON value, retaining it on success only. */
.Lpending_ref:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov rdi,rsi
 call json_object_get@PLT
 mov r12,rax
 mov rdi,rbx
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz .Lpending_ref_return
 mov rdi,r12
 call json_object_put@PLT
 mov eax,3
.Lpending_ref_return:
 add rsp,8
 pop r12
 pop rbx
 ret

/* Canonical path JSON keeps numeric 1 and 1.0 identical while preserving
   strings, embedded NUL, and segment boundaries. Returned array owns its key. */
.Lpending_path:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [r12],0
 mov qword ptr [rsp],0
 mov ebp,1
 mov rdi,rbx
 call json_object_get_type@PLT
 cmp eax,5
 jne .Lpending_path_done
 mov rdi,rbx
 call json_object_array_length@PLT
 test rax,rax
 jz .Lpending_path_done
 cmp rax,0x7fffffff
 ja .Lpending_path_capacity
 mov r14,rax
 mov edi,eax
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lpending_path_allocation
 mov [rsp],rax
 xor r13d,r13d
.Lpending_path_each:
 cmp r13,r14
 jae .Lpending_path_ok
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov [rsp+8],rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,6
 je .Lpending_path_string
 mov rdi,[rsp+8]
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Lpending_path_done
 mov rdi,rax
 call json_object_new_int64@PLT
 test rax,rax
 jz .Lpending_path_allocation
 mov [rsp+8],rax
 mov rdi,[rsp]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz .Lpending_path_next
 mov rdi,[rsp+8]
 call json_object_put@PLT
 jmp .Lpending_path_allocation
.Lpending_path_string:
 mov rdi,[rsp]
 mov rsi,[rsp+8]
 call .Lpending_ref
 test eax,eax
 jnz .Lpending_path_allocation
.Lpending_path_next:
 inc r13
 jmp .Lpending_path_each
.Lpending_path_ok:
 mov rax,[rsp]
 mov [r12],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
 jmp .Lpending_path_done
.Lpending_path_capacity:
 mov ebp,2
 jmp .Lpending_path_done
.Lpending_path_allocation:
 mov ebp,3
.Lpending_path_done:
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

/* Merge two append/string-set leaves into a fresh immutable tuple. */
.Lpending_join:
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
 mov qword ptr [r13],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov ebp,3
 mov rdi,rbx
 mov esi,2
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 mov [rsp+24],rax
 mov rdi,r12
 mov esi,2
 call json_object_array_get_idx@PLT
 mov r15,rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 add rax,[rsp+24]
 cmp rax,0x7fffffff
 ja .Lpending_join_capacity
 mov [rsp+32],rax
 lea rdi,[rax+1]
 call malloc@PLT
 test rax,rax
 jz .Lpending_join_done
 mov [rsp],rax
 mov rdi,r14
 call json_object_get_string@PLT
 mov rdi,[rsp]
 mov rsi,rax
 mov rdx,[rsp+24]
 call memcpy@PLT
 mov rdi,r15
 call json_object_get_string@PLT
 mov rdi,[rsp]
 add rdi,[rsp+24]
 mov rsi,rax
 mov rdx,[rsp+32]
 sub rdx,[rsp+24]
 call memcpy@PLT
 mov rdi,[rsp]
 mov esi,[rsp+32]
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Lpending_join_done
 mov [rsp+16],rax
 mov edi,3
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lpending_join_done
 mov [rsp+8],rax
 mov rdi,rbx
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rdi,[rsp+8]
 mov rsi,rax
 call .Lpending_ref
 test eax,eax
 jnz .Lpending_join_done
 mov rdi,rbx
 mov esi,1
 call json_object_array_get_idx@PLT
 mov rdi,[rsp+8]
 mov rsi,rax
 call .Lpending_ref
 test eax,eax
 jnz .Lpending_join_done
 mov rdi,[rsp+8]
 mov rsi,[rsp+16]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lpending_join_done
 mov qword ptr [rsp+16],0
 mov rax,[rsp+8]
 mov [r13],rax
 mov qword ptr [rsp+8],0
 xor ebp,ebp
 jmp .Lpending_join_done
.Lpending_join_capacity:
 mov ebp,2
.Lpending_join_done:
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
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

/* Work owns ops/map. Return 11 when the source cap selects replace. */
.Lpending_fold:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,56
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
.Lpending_fold_each:
 mov ebp,1
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lpending_fold_ok
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 call .Lpending_code
 mov [rsp+16],eax
 cmp eax,-1
 je .Lpending_fold_done
 cmp eax,114
 je .Lpending_fold_root
 cmp eax,97
 je .Lpending_fold_leaf
 cmp eax,115
 jne .Lpending_fold_clear
 mov rdi,r14
 mov esi,2
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,4
 je .Lpending_fold_clear
 cmp eax,5
 je .Lpending_fold_clear
.Lpending_fold_leaf:
 mov rdi,r14
 call json_object_array_length@PLT
 cmp rax,3
 jne .Lpending_fold_done
 cmp dword ptr [rsp+16],97
 jne .Lpending_fold_path
 mov rdi,r14
 mov esi,2
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,6
 jne .Lpending_fold_done
.Lpending_fold_path:
 mov rdi,r14
 mov esi,1
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov rsi,rsp
 call .Lpending_path
 mov ebp,eax
 test eax,eax
 jnz .Lpending_fold_done
 mov rdi,[rsp]
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Lpending_fold_allocation
 mov [rsp+24],rax
 mov rdi,[rbx+8]
 mov rsi,rax
 call json_object_object_get@PLT
 test rax,rax
 jz .Lpending_fold_index
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+32],rax
 mov rsi,rax
 mov rdi,[rbx]
 call json_object_array_get_idx@PLT
 mov r15,rax
 cmp dword ptr [rsp+16],115
 je .Lpending_fold_replace_leaf
 mov rdi,r15
 mov esi,2
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,6
 jne .Lpending_fold_index
 mov rdi,r15
 mov rsi,r14
 lea rdx,[rsp+8]
 call .Lpending_join
 mov ebp,eax
 test eax,eax
 jnz .Lpending_fold_done
 jmp .Lpending_fold_replace
.Lpending_fold_replace_leaf:
 mov rdi,r14
 call json_object_get@PLT
 mov [rsp+8],rax
.Lpending_fold_replace:
 mov rdi,[rbx]
 mov rsi,[rsp+32]
 mov rdx,[rsp+8]
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Lpending_fold_allocation
 mov qword ptr [rsp+8],0
 jmp .Lpending_fold_next
.Lpending_fold_index:
 mov rdi,[rbx]
 call json_object_array_length@PLT
 mov rdi,rax
 call json_object_new_int64@PLT
 test rax,rax
 jz .Lpending_fold_allocation
 mov [rsp+8],rax
 mov rdi,[rbx+8]
 mov rsi,[rsp+24]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lpending_fold_allocation
 mov qword ptr [rsp+8],0
 jmp .Lpending_fold_append
.Lpending_fold_root:
 mov rdi,[rbx]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lpending_fold_clear
 mov rdi,[rbx]
 xor esi,esi
 mov rdx,rax
 call json_object_array_del_idx@PLT
 test eax,eax
 jnz .Lpending_fold_invalid
.Lpending_fold_clear:
 mov rdi,[rbx+8]
 call json_object_object_length@PLT
 test eax,eax
 jz .Lpending_fold_append
 call json_object_new_object@PLT
 test rax,rax
 jz .Lpending_fold_allocation
 mov rdi,[rbx+8]
 mov [rbx+8],rax
 call json_object_put@PLT
.Lpending_fold_append:
 mov rdi,[rbx]
 mov rsi,r14
 call .Lpending_ref
 test eax,eax
 jnz .Lpending_fold_allocation
 mov rdi,[rbx]
 call json_object_array_length@PLT
 cmp rax,10000
 ja .Lpending_fold_overflow
.Lpending_fold_next:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov qword ptr [rsp],0
 inc r13
 jmp .Lpending_fold_each
.Lpending_fold_overflow:
 mov ebp,11
 jmp .Lpending_fold_done
.Lpending_fold_invalid:
 mov ebp,1
 jmp .Lpending_fold_done
.Lpending_fold_allocation:
 mov ebp,3
 jmp .Lpending_fold_done
.Lpending_fold_ok:
 xor ebp,ebp
.Lpending_fold_done:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,56
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.globl pg_pending_ops_open
.type pg_pending_ops_open,@function
pg_pending_ops_open:
 mov eax,1
 test rdi,rdi
 jz .Lpending_open_return
 cmp dword ptr [rdi+20],0
 jne .Lpending_open_return
 cmp dword ptr [rdi+24],0
 jne .Lpending_open_return
 cmp qword ptr [rdi],0
 jne .Lpending_open_return
 cmp qword ptr [rdi+8],0
 jne .Lpending_open_return
 pxor xmm0,xmm0
 movups [rdi],xmm0
 movups [rdi+16],xmm0
 mov dword ptr [rdi+24],1
 xor eax,eax
.Lpending_open_return:
 ret
.size pg_pending_ops_open,.-pg_pending_ops_open

.globl pg_pending_ops_push
.type pg_pending_ops_push,@function
pg_pending_ops_push:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,40
 mov rbx,rdi
 mov r12,rsi
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 test rbx,rbx
 jz .Lpending_push_return
 cmp dword ptr [rbx+20],0
 jne .Lpending_push_return
 cmp dword ptr [rbx+24],1
 jne .Lpending_push_return
 mov dword ptr [rbx+20],1
 mov rdi,r12
 call json_object_get_type@PLT
 cmp eax,5
 jne .Lpending_push_done
 mov rdi,r12
 call json_object_array_length@PLT
 test rax,rax
 jz .Lpending_push_ok
 cmp rax,0x7fffffff
 ja .Lpending_push_capacity
 mov rdi,r12
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call .Lpending_code
 cmp eax,-1
 je .Lpending_push_done
 mov r13d,[rbx+16]
 cmp eax,114
 je .Lpending_push_validate
 cmp r13d,3
 je .Lpending_push_ok
.Lpending_push_validate:
 mov [rsp+24],eax
 mov rdi,r12
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lpending_push_done
 cmp dword ptr [rsp+24],114
 je .Lpending_push_batch
 test r13d,r13d
 jz .Lpending_push_batch
 mov rdi,r12
 lea rsi,[rsp+16]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lpending_push_allocation
 cmp r13d,1
 je .Lpending_push_from_batch
 cmp r13d,2
 jne .Lpending_push_done
 mov rdi,[rbx]
 call json_object_array_length@PLT
 mov r15,rax
 mov edi,eax
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lpending_push_allocation
 mov [rsp],rax
 xor r14d,r14d
.Lpending_push_clone_each:
 cmp r14,r15
 jae .Lpending_push_clone_map
 mov rdi,[rbx]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,[rsp]
 mov rsi,rax
 call .Lpending_ref
 test eax,eax
 jnz .Lpending_push_allocation
 inc r14
 jmp .Lpending_push_clone_each
.Lpending_push_clone_map:
 mov rdi,[rbx+8]
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lpending_push_allocation
 jmp .Lpending_push_fold_new
.Lpending_push_from_batch:
 xor edi,edi
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lpending_push_allocation
 mov [rsp],rax
 call json_object_new_object@PLT
 test rax,rax
 jz .Lpending_push_allocation
 mov [rsp+8],rax
 mov rdi,rsp
 mov rsi,[rbx]
 call .Lpending_fold
 cmp eax,11
 je .Lpending_push_overflow
 mov ebp,eax
 test eax,eax
 jnz .Lpending_push_done
.Lpending_push_fold_new:
 mov rdi,rsp
 mov rsi,[rsp+16]
 call .Lpending_fold
 cmp eax,11
 je .Lpending_push_overflow
 mov ebp,eax
 test eax,eax
 jnz .Lpending_push_done
 mov r13d,2
 jmp .Lpending_push_install
.Lpending_push_batch:
 mov rdi,r12
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lpending_push_allocation
 mov r13d,1
 jmp .Lpending_push_install
.Lpending_push_overflow:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov r13d,3
.Lpending_push_install:
 mov rdi,[rbx]
 mov rax,[rsp]
 mov [rbx],rax
 mov qword ptr [rsp],0
 call json_object_put@PLT
 mov rdi,[rbx+8]
 mov rax,[rsp+8]
 mov [rbx+8],rax
 mov qword ptr [rsp+8],0
 mov [rbx+16],r13d
 call json_object_put@PLT
.Lpending_push_ok:
 xor ebp,ebp
 jmp .Lpending_push_done
.Lpending_push_capacity:
 mov ebp,2
 jmp .Lpending_push_done
.Lpending_push_allocation:
 mov ebp,3
.Lpending_push_done:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov dword ptr [rbx+20],0
.Lpending_push_return:
 mov eax,ebp
 add rsp,40
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_pending_ops_push,.-pg_pending_ops_push

.globl pg_pending_ops_size
.type pg_pending_ops_size,@function
pg_pending_ops_size:
 xor eax,eax
 test rdi,rdi
 jz .Lpending_size_return
 cmp dword ptr [rdi+20],0
 jne .Lpending_size_return
 cmp dword ptr [rdi+24],1
 jne .Lpending_size_return
 mov ecx,[rdi+16]
 test ecx,ecx
 jz .Lpending_size_return
 cmp ecx,3
 je .Lpending_size_one
 mov rdi,[rdi]
 jmp json_object_array_length@PLT
.Lpending_size_one:
 mov eax,1
.Lpending_size_return:
 ret
.size pg_pending_ops_size,.-pg_pending_ops_size

.globl pg_pending_ops_take
.type pg_pending_ops_take,@function
pg_pending_ops_take:
 push rbp
 push rbx
 push r12
 mov rbx,rdi
 mov r12,rsi
 mov ebp,1
 test r12,r12
 jz .Lpending_take_return
 pxor xmm0,xmm0
 movups [r12],xmm0
 test rbx,rbx
 jz .Lpending_take_return
 cmp dword ptr [rbx+20],0
 jne .Lpending_take_return
 cmp dword ptr [rbx+24],1
 jne .Lpending_take_return
 mov dword ptr [rbx+20],1
 cmp dword ptr [rbx+16],0
 je .Lpending_take_empty
 cmp dword ptr [rbx+16],3
 je .Lpending_take_replace
 mov rax,[rbx]
 mov [r12],rax
 mov qword ptr [rbx],0
 jmp .Lpending_take_reset
.Lpending_take_empty:
 xor edi,edi
 call json_object_new_array_ext@PLT
 mov ebp,3
 test rax,rax
 jz .Lpending_take_done
 mov [r12],rax
 jmp .Lpending_take_reset
.Lpending_take_replace:
 mov dword ptr [r12+8],1
.Lpending_take_reset:
 mov rdi,[rbx+8]
 mov qword ptr [rbx+8],0
 call json_object_put@PLT
 mov dword ptr [rbx+16],0
 xor ebp,ebp
.Lpending_take_done:
 mov dword ptr [rbx+20],0
.Lpending_take_return:
 mov eax,ebp
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_pending_ops_take,.-pg_pending_ops_take

.globl pg_pending_ops_close
.type pg_pending_ops_close,@function
pg_pending_ops_close:
 mov eax,1
 test rdi,rdi
 jz .Lpending_close_return
 cmp dword ptr [rdi+20],0
 jne .Lpending_close_return
 push rbx
 mov rbx,rdi
 mov dword ptr [rbx+20],1
 mov rdi,[rbx]
 call json_object_put@PLT
 mov rdi,[rbx+8]
 call json_object_put@PLT
 pxor xmm0,xmm0
 movups [rbx],xmm0
 movups [rbx+16],xmm0
 xor eax,eax
 pop rbx
.Lpending_close_return:
 ret
.size pg_pending_ops_close,.-pg_pending_ops_close
.section .note.GNU-stack,"",@progbits

.text
.Lpending_member:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 call json_object_get@PLT
 mov r13,rax
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Lpending_member_return
 mov rdi,r13
 call json_object_put@PLT
 mov eax,3
.Lpending_member_return:
 pop r13
 pop r12
 pop rbx
 ret

.globl pg_document_store_held
.type pg_document_store_held,@function
pg_document_store_held:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,88
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov r15,r9
 mov [rsp+80],r8d
 lea rdi,[rsp]
 xor eax,eax
 mov ecx,10
 rep stosq
 mov ebp,1
 test r15,r15
 jz .Lheld_store_return
 pxor xmm0,xmm0
 movups [r15],xmm0
 test rbx,rbx
 jz .Lheld_store_return
 cmp dword ptr [rbx+20],0
 jne .Lheld_store_return
 cmp dword ptr [rbx+24],1
 jne .Lheld_store_return
 mov dword ptr [rbx+20],1
 cmp dword ptr [rsp+80],1
 ja .Lheld_store_done
 test r14,r14
 jz .Lheld_store_done
 mov rax,9007199254740991
 cmp r14,rax
 ja .Lheld_store_done
 mov rdi,r13
 call json_object_get_type@PLT
 cmp eax,4
 jne .Lheld_store_done
 mov rdi,r13
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lheld_store_done
 mov rdi,[rbx]
 call json_object_get@PLT
 mov [rsp],rax
 mov rdi,[rbx+8]
 call json_object_get@PLT
 mov [rsp+8],rax
 mov eax,[rbx+16]
 mov [rsp+16],eax
 mov dword ptr [rsp+24],1
 mov rdi,rsp
 mov rsi,r12
 call pg_pending_ops_push
 mov ebp,eax
 test eax,eax
 jnz .Lheld_store_done
 mov rdi,rsp
 lea rsi,[rsp+32]
 call pg_pending_ops_take
 mov ebp,eax
 test eax,eax
 jnz .Lheld_store_done
 cmp dword ptr [rsp+40],0
 je .Lheld_store_selected
 mov dword ptr [rsp+80],1
.Lheld_store_selected:
 cmp dword ptr [rsp+80],0
 je .Lheld_store_content
 mov rdi,r13
 lea rsi,[rsp+48]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lheld_store_allocation
 cmp dword ptr [rsp+40],0
 je .Lheld_store_content
 mov edi,1
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lheld_store_allocation
 mov [rsp+32],rax
 mov edi,2
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lheld_store_allocation
 mov [rsp+64],rax
 lea rdi,[rip+.Lheld_r]
 mov esi,1
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Lheld_store_allocation
 mov [rsp+72],rax
 mov rdi,[rsp+64]
 mov rsi,rax
 call .Lpending_ref
 test eax,eax
 jnz .Lheld_store_allocation
 mov rdi,[rsp+64]
 mov rsi,[rsp+48]
 call .Lpending_ref
 test eax,eax
 jnz .Lheld_store_allocation
 mov rdi,[rsp+32]
 mov rsi,[rsp+64]
 call .Lpending_ref
 test eax,eax
 jnz .Lheld_store_allocation
 mov rdi,[rsp+72]
 call json_object_put@PLT
 mov qword ptr [rsp+72],0
.Lheld_store_content:
 call json_object_new_object@PLT
 test rax,rax
 jz .Lheld_store_allocation
 mov [rsp+56],rax
 mov rdi,r14
 call json_object_new_int64@PLT
 test rax,rax
 jz .Lheld_store_allocation
 mov [rsp+72],rax
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lheld_version]
 mov rdx,rax
 call .Lpending_member
 test eax,eax
 jnz .Lheld_store_allocation
 mov rdi,[rsp+72]
 call json_object_put@PLT
 mov qword ptr [rsp+72],0
 lea rdi,[rip+.Lheld_delta]
 mov esi,5
 cmp dword ptr [rsp+80],0
 je .Lheld_store_kind
 lea rdi,[rip+.Lheld_base]
 mov esi,4
.Lheld_store_kind:
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Lheld_store_allocation
 mov [rsp+72],rax
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lheld_kind]
 mov rdx,rax
 call .Lpending_member
 test eax,eax
 jnz .Lheld_store_allocation
 lea rsi,[rip+.Lheld_ops]
 mov rdx,[rsp+32]
 cmp dword ptr [rsp+80],0
 je .Lheld_store_payload
 lea rsi,[rip+.Lheld_value]
 mov rdx,[rsp+48]
.Lheld_store_payload:
 mov rdi,[rsp+56]
 call .Lpending_member
 test eax,eax
 jnz .Lheld_store_allocation
 mov rdi,[rbx]
 mov qword ptr [rbx],0
 call json_object_put@PLT
 mov rdi,[rbx+8]
 mov qword ptr [rbx+8],0
 call json_object_put@PLT
 mov dword ptr [rbx+16],0
 mov rax,[rsp+56]
 mov [r15],rax
 mov qword ptr [rsp+56],0
 mov rax,[rsp+32]
 mov [r15+8],rax
 mov qword ptr [rsp+32],0
 xor ebp,ebp
 jmp .Lheld_store_done
.Lheld_store_allocation:
 mov ebp,3
.Lheld_store_done:
 mov rdi,rsp
 call pg_pending_ops_close
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov rdi,[rsp+72]
 call json_object_put@PLT
 mov dword ptr [rbx+20],0
.Lheld_store_return:
 mov eax,ebp
 add rsp,88
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_store_held,.-pg_document_store_held
.section .rodata
.Lheld_r: .asciz "r"
.Lheld_version: .asciz "version"
.Lheld_kind: .asciz "kind"
.Lheld_ops: .asciz "ops"
.Lheld_value: .asciz "value"
.Lheld_base: .asciz "base"
.Lheld_delta: .asciz "delta"
