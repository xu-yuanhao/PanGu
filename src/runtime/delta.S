.intel_syntax noprefix
.text
/* Nonnegative integral counts; values above signed range saturate for clamping. */
.Ldelta_count:
 push rbx
 mov rbx,rdi
 call json_object_get_type@PLT
 cmp eax,3
 je .Lcount_integer
 cmp eax,2
 jne .Lcount_bad
 mov rdi,rbx
 call json_object_get_double@PLT
 pxor xmm1,xmm1
 ucomisd xmm0,xmm1
 jp .Lcount_bad
 jb .Lcount_bad
 cvttsd2si rax,xmm0
 test rax,rax
 js .Lcount_saturated
 cvtsi2sd xmm1,rax
 ucomisd xmm0,xmm1
 jne .Lcount_bad
 jmp .Lcount_good
.Lcount_integer:
 mov rdi,rbx
 call json_object_get_int64@PLT
 test rax,rax
 js .Lcount_bad
.Lcount_good:
 mov edx,1
 pop rbx
 ret
.Lcount_saturated:
 mov rax,-1
 jmp .Lcount_good
.Lcount_bad:
 xor edx,edx
 pop rbx
 ret

/* Key result borrows a string or uses the caller's 32-byte scratch. */
.Ldelta_key:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 call json_object_get_type@PLT
 cmp eax,6
 jne .Lkey_number
 mov rdi,rbx
 call json_object_get_string_len@PLT
 mov r13d,eax
 mov rdi,rbx
 call json_object_get_string@PLT
 mov rbx,rax
 mov rdi,rax
 xor esi,esi
 mov edx,r13d
 call memchr@PLT
 test rax,rax
 jnz .Lkey_bad
 xor r13d,r13d
.Lkey_reserved:
 lea rax,[rip+.Lreserved]
 movsxd rsi,dword ptr [rax+r13*4]
 add rsi,rax
 mov rdi,rbx
 call strcmp@PLT
 test eax,eax
 jz .Lkey_bad
 inc r13
 cmp r13,3
 jb .Lkey_reserved
 mov rax,rbx
 jmp .Lkey_return
.Lkey_number:
 mov rdi,rbx
 call .Ldelta_count
 test edx,edx
 jz .Lkey_bad
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lkey_bad
 mov rcx,rax
 mov rdi,r12
 mov esi,32
 lea rdx,[rip+.Lunsigned]
 xor eax,eax
 call snprintf@PLT
 test eax,eax
 js .Lkey_bad
 mov rax,r12
 jmp .Lkey_return
.Lkey_bad:
 xor eax,eax
.Lkey_return:
 pop r13
 pop r12
 pop rbx
 ret

/* Container access distinguishes missing properties from JSON null. */
.Ldelta_get:
 push rbx
 push r12
 push r13
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 call json_object_get_type@PLT
 cmp eax,4
 je .Lget_object
 cmp eax,5
 jne .Lget_bad
 mov rdi,r12
 call .Ldelta_count
 test edx,edx
 jz .Lget_bad
 mov [rsp],rax
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp [rsp],rax
 jae .Lget_bad
 mov rdi,rbx
 mov rsi,[rsp]
 call json_object_array_get_idx@PLT
 mov [r13],rax
 xor eax,eax
 jmp .Lget_return
.Lget_object:
 mov rdi,r12
 mov rsi,rsp
 call .Ldelta_key
 test rax,rax
 jz .Lget_bad
 mov rdi,rbx
 mov rsi,rax
 mov rdx,r13
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lget_bad
 xor eax,eax
 jmp .Lget_return
.Lget_bad:
 mov eax,1
.Lget_return:
 add rsp,32
 pop r13
 pop r12
 pop rbx
 ret

/* On success the container consumes value; failure leaves ownership to caller. */
.Ldelta_set:
 push rbx
 push r12
 push r13
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 call json_object_get_type@PLT
 cmp eax,4
 je .Lset_object
 cmp eax,5
 jne .Lset_bad
 mov rdi,r12
 call .Ldelta_count
 test edx,edx
 jz .Lset_bad
 mov [rsp],rax
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp [rsp],rax
 ja .Lset_bad
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r13
 call json_object_array_put_idx@PLT
 jmp .Lset_result
.Lset_object:
 mov rdi,r12
 mov rsi,rsp
 call .Ldelta_key
 test rax,rax
 jz .Lset_bad
 mov rdi,rbx
 mov rsi,rax
 mov rdx,r13
 call json_object_object_add@PLT
.Lset_result:
 test eax,eax
 jz .Lset_return
 mov eax,3
 jmp .Lset_return
.Lset_bad:
 mov eax,1
.Lset_return:
 add rsp,32
 pop r13
 pop r12
 pop rbx
 ret

.Ldelta_resolve:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 xor r15d,r15d
.Lresolve_loop:
 cmp r15,r13
 jae .Lresolve_done
 mov rdi,r12
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov rdi,rbx
 mov rsi,rax
 mov rdx,rsp
 call .Ldelta_get
 test eax,eax
 jnz .Lresolve_return
 mov rbx,[rsp]
 inc r15
 jmp .Lresolve_loop
.Lresolve_done:
 mov [r14],rbx
 xor eax,eax
.Lresolve_return:
 add rsp,16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret

/* Append a borrowed value, releasing the extra reference if insertion fails. */
.Ldelta_append:
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
 jz .Lappend_return
 mov rdi,r12
 call json_object_put@PLT
 mov eax,3
.Lappend_return:
 add rsp,8
 pop r12
 pop rbx
 ret

.globl pg_document_delta_apply
.type pg_document_delta_apply,@function
pg_document_delta_apply:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,232
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+56],0
 mov qword ptr [rsp+64],0
 mov qword ptr [rsp+72],0
 test r13,r13
 jz .Ldelta_return
 mov qword ptr [r13],0
 mov rdi,r12
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldelta_cleanup
 mov rdi,rbx
 call pg_document_value_valid
 test eax,eax
 jz .Ldelta_cleanup
 mov rdi,r12
 call pg_document_value_valid
 test eax,eax
 jz .Ldelta_cleanup
 mov ebp,3
 test rbx,rbx
 jz .Ldelta_copy_ops
 mov rdi,rbx
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ldelta_cleanup
.Ldelta_copy_ops:
 mov rdi,r12
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ldelta_cleanup
 mov rbx,[rsp]
 mov r12,[rsp+8]
 mov rdi,r12
 call json_object_array_length@PLT
 mov [rsp+224],rax
 xor r14d,r14d
.Ldelta_op:
 cmp r14,[rsp+224]
 jae .Ldelta_success
 mov ebp,1
 mov rdi,r12
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov r15,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldelta_cleanup
 mov rdi,r15
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov [rsp+48],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldelta_cleanup
 mov rdi,[rsp+48]
 call json_object_get_string_len@PLT
 cmp eax,1
 jne .Ldelta_cleanup
 mov rdi,[rsp+48]
 call json_object_get_string@PLT
 movzx eax,byte ptr [rax]
 mov [rsp+80],eax
 mov rdi,r15
 call json_object_array_length@PLT
 mov rcx,2
 cmp dword ptr [rsp+80],114
 je .Ldelta_arity
 cmp dword ptr [rsp+80],100
 je .Ldelta_arity
 mov ecx,3
 cmp dword ptr [rsp+80],115
 je .Ldelta_arity
 cmp dword ptr [rsp+80],97
 je .Ldelta_arity
 cmp dword ptr [rsp+80],116
 je .Ldelta_arity
 cmp dword ptr [rsp+80],109
 je .Ldelta_arity
 cmp dword ptr [rsp+80],112
 jne .Ldelta_cleanup
 mov ecx,5
.Ldelta_arity:
 cmp rax,rcx
 jne .Ldelta_cleanup
 cmp dword ptr [rsp+80],114
 je .Ldelta_replace
 mov rdi,r15
 mov esi,1
 call json_object_array_get_idx@PLT
 mov [rsp+16],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldelta_cleanup
 mov rdi,[rsp+16]
 call json_object_array_length@PLT
 mov [rsp+24],rax
 test rax,rax
 jnz .Ldelta_path_check
 cmp dword ptr [rsp+80],112
 je .Ldelta_path_ready
 cmp dword ptr [rsp+80],109
 jne .Ldelta_cleanup
 jmp .Ldelta_path_ready
.Ldelta_path_check:
 mov qword ptr [rsp+128],0
.Ldelta_path_loop:
 mov rsi,[rsp+128]
 cmp rsi,[rsp+24]
 jae .Ldelta_path_ready
 mov rdi,[rsp+16]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rsp+192]
 call .Ldelta_key
 test rax,rax
 jz .Ldelta_cleanup
 inc qword ptr [rsp+128]
 jmp .Ldelta_path_loop
.Ldelta_path_ready:
 mov rdx,[rsp+24]
 test rdx,rdx
 jz .Ldelta_root_target
 dec rdx
 mov rdi,rbx
 mov rsi,[rsp+16]
 lea rcx,[rsp+32]
 call .Ldelta_resolve
 test eax,eax
 jnz .Ldelta_cleanup
 mov rdi,[rsp+16]
 mov rsi,[rsp+24]
 dec rsi
 call json_object_array_get_idx@PLT
 mov [rsp+40],rax
 cmp dword ptr [rsp+80],112
 je .Ldelta_read_target
 cmp dword ptr [rsp+80],109
 je .Ldelta_read_target
 cmp dword ptr [rsp+80],115
 je .Ldelta_set_value
 cmp dword ptr [rsp+80],100
 je .Ldelta_delete
.Ldelta_read_target:
 mov rdi,[rsp+32]
 mov rsi,[rsp+40]
 lea rdx,[rsp+48]
 call .Ldelta_get
 test eax,eax
 jnz .Ldelta_cleanup
 jmp .Ldelta_target_ready
.Ldelta_root_target:
 mov [rsp+48],rbx
.Ldelta_target_ready:
 cmp dword ptr [rsp+80],112
 je .Ldelta_splice
 cmp dword ptr [rsp+80],109
 je .Ldelta_permute
 jmp .Ldelta_text
.Ldelta_replace:
 mov rdi,r15
 mov esi,1
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+56],rax
 jmp .Ldelta_replace_owned
.Ldelta_set_value:
 mov rdi,r15
 mov esi,2
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+56],rax
 jmp .Ldelta_write_owned
.Ldelta_delete:
 mov rdi,[rsp+32]
 call json_object_get_type@PLT
 cmp eax,4
 je .Ldelta_delete_object
 cmp eax,5
 jne .Ldelta_cleanup
 mov rdi,[rsp+40]
 call .Ldelta_count
 test edx,edx
 jz .Ldelta_cleanup
 mov [rsp+96],rax
 mov rdi,[rsp+32]
 call json_object_array_length@PLT
 cmp [rsp+96],rax
 jae .Ldelta_cleanup
 mov rdi,[rsp+32]
 mov rsi,[rsp+96]
 mov edx,1
 call json_object_array_del_idx@PLT
 test eax,eax
 jnz .Ldelta_cleanup
 jmp .Ldelta_next
.Ldelta_delete_object:
 mov rdi,[rsp+40]
 lea rsi,[rsp+192]
 call .Ldelta_key
 test rax,rax
 jz .Ldelta_cleanup
 mov rdi,[rsp+32]
 mov rsi,rax
 call json_object_object_del@PLT
 jmp .Ldelta_next
.Ldelta_text:
 mov rdi,[rsp+48]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldelta_cleanup
 mov rdi,[rsp+48]
 call json_object_get_string_len@PLT
 mov [rsp+152],rax
 mov rdi,[rsp+48]
 call json_object_get_string@PLT
 mov [rsp+144],rax
 mov rdi,r15
 mov esi,2
 call json_object_array_get_idx@PLT
 mov [rsp+112],rax
 cmp dword ptr [rsp+80],116
 je .Ldelta_truncate
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldelta_cleanup
 mov rdi,[rsp+112]
 call json_object_get_string_len@PLT
 mov [rsp+168],rax
 mov [rsp+160],r14
.Ldelta_append_run:
 mov rax,[rsp+160]
 inc rax
 cmp rax,[rsp+224]
 jae .Ldelta_append_prepare
 mov [rsp+128],rax
 mov rdi,r12
 mov rsi,rax
 call json_object_array_get_idx@PLT
 mov [rsp+120],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldelta_append_prepare
 mov rdi,[rsp+120]
 call json_object_array_length@PLT
 cmp rax,3
 jne .Ldelta_append_prepare
 mov rdi,[rsp+120]
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov [rsp+112],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldelta_append_prepare
 mov rdi,[rsp+112]
 call json_object_get_string_len@PLT
 cmp eax,1
 jne .Ldelta_append_prepare
 mov rdi,[rsp+112]
 call json_object_get_string@PLT
 cmp byte ptr [rax],97
 jne .Ldelta_append_prepare
 mov rdi,[rsp+120]
 mov esi,1
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov rsi,[rsp+16]
 call json_object_equal@PLT
 test eax,eax
 jz .Ldelta_append_prepare
 mov rdi,[rsp+120]
 mov esi,2
 call json_object_array_get_idx@PLT
 mov [rsp+112],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldelta_append_prepare
 mov rdi,[rsp+112]
 call json_object_get_string_len@PLT
 add [rsp+168],rax
 jc .Ldelta_capacity
 mov rax,[rsp+128]
 mov [rsp+160],rax
 jmp .Ldelta_append_run
.Ldelta_append_prepare:
 mov rax,[rsp+168]
 add rax,[rsp+152]
 jc .Ldelta_capacity
 cmp rax,0x7fffffff
 ja .Ldelta_capacity
 mov [rsp+176],rax
 lea rdi,[rax+1]
 mov ebp,3
 call malloc@PLT
 test rax,rax
 jz .Ldelta_cleanup
 mov [rsp+72],rax
 mov rdi,rax
 mov rsi,[rsp+144]
 mov rdx,[rsp+152]
 call memcpy@PLT
 mov [rsp+128],r14
 mov rax,[rsp+152]
 mov [rsp+104],rax
.Ldelta_append_copy:
 mov rdi,r12
 mov rsi,[rsp+128]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov esi,2
 call json_object_array_get_idx@PLT
 mov [rsp+112],rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 mov [rsp+96],rax
 mov rdi,[rsp+112]
 call json_object_get_string@PLT
 mov rsi,rax
 mov rdi,[rsp+72]
 add rdi,[rsp+104]
 mov rdx,[rsp+96]
 call memcpy@PLT
 mov rax,[rsp+96]
 add [rsp+104],rax
 mov rax,[rsp+128]
 inc rax
 mov [rsp+128],rax
 cmp rax,[rsp+160]
 jbe .Ldelta_append_copy
 mov r14,[rsp+160]
 mov rdi,[rsp+72]
 mov esi,[rsp+176]
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Ldelta_cleanup
 mov [rsp+56],rax
 jmp .Ldelta_write_owned
.Ldelta_truncate:
 mov rdi,[rsp+112]
 call .Ldelta_count
 test edx,edx
 jz .Ldelta_cleanup
 mov [rsp+176],rax
 mov qword ptr [rsp+128],0
 cmp rax,[rsp+152]
 jb .Ldelta_units_loop
 mov rax,[rsp+152]
 mov [rsp+128],rax
 jmp .Ldelta_text_suffix
.Ldelta_units_loop:
 cmp qword ptr [rsp+176],0
 je .Ldelta_text_suffix
 mov rax,[rsp+128]
 cmp rax,[rsp+152]
 jae .Ldelta_text_suffix
 mov rdi,[rsp+144]
 add rdi,rax
 cmp byte ptr [rdi],0x80
 jae .Ldelta_units_unicode
 inc qword ptr [rsp+128]
 dec qword ptr [rsp+176]
 jmp .Ldelta_units_loop
.Ldelta_units_unicode:
 mov rsi,[rsp+152]
 sub rsi,rax
 lea rdx,[rsp+184]
 call utf8proc_iterate@PLT
 test rax,rax
 jle .Ldelta_cleanup
 mov ecx,1
 cmp dword ptr [rsp+184],0x10000
 jb .Ldelta_unit_count
 mov ecx,2
.Ldelta_unit_count:
 cmp [rsp+176],rcx
 jb .Ldelta_cleanup
 sub [rsp+176],rcx
 add [rsp+128],rax
 jmp .Ldelta_units_loop
.Ldelta_text_suffix:
 mov rdi,[rsp+144]
 add rdi,[rsp+128]
 mov rsi,[rsp+152]
 sub rsi,[rsp+128]
 mov ebp,3
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Ldelta_cleanup
 mov [rsp+56],rax
 jmp .Ldelta_write_owned

.Ldelta_splice:
 mov rdi,[rsp+48]
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldelta_cleanup
 mov rdi,[rsp+48]
 call json_object_array_length@PLT
 mov [rsp+88],rax
 mov rdi,r15
 mov esi,2
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call .Ldelta_count
 test edx,edx
 jz .Ldelta_cleanup
 cmp rax,[rsp+88]
 jbe .Ldelta_splice_index
 mov rax,[rsp+88]
.Ldelta_splice_index:
 mov [rsp+96],rax
 mov rdi,r15
 mov esi,3
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call .Ldelta_count
 test edx,edx
 jz .Ldelta_cleanup
 mov rcx,[rsp+88]
 sub rcx,[rsp+96]
 cmp rax,rcx
 jbe .Ldelta_splice_remove
 mov rax,rcx
.Ldelta_splice_remove:
 mov [rsp+104],rax
 mov rdi,r15
 mov esi,4
 call json_object_array_get_idx@PLT
 mov [rsp+112],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldelta_cleanup
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 mov [rsp+120],rax
 add rax,[rsp+88]
 jc .Ldelta_capacity
 sub rax,[rsp+104]
 mov ecx,0xffffffff
 cmp rax,rcx
 ja .Ldelta_capacity
 mov [rsp+136],rax
 jmp .Ldelta_array_create
.Ldelta_permute:
 mov rdi,[rsp+48]
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldelta_cleanup
 mov rdi,[rsp+48]
 call json_object_array_length@PLT
 mov [rsp+88],rax
 mov [rsp+136],rax
 mov rdi,r15
 mov esi,2
 call json_object_array_get_idx@PLT
 mov [rsp+112],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldelta_cleanup
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp rax,[rsp+88]
 jne .Ldelta_cleanup
 test rax,rax
 jz .Ldelta_array_create
 mov rdi,rax
 mov esi,1
 mov ebp,3
 call calloc@PLT
 test rax,rax
 jz .Ldelta_cleanup
 mov [rsp+72],rax
.Ldelta_array_create:
 xor edi,edi
 mov ebp,3
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Ldelta_cleanup
 mov [rsp+64],rax
 mov qword ptr [rsp+128],0
.Ldelta_array_loop:
 mov rax,[rsp+128]
 cmp rax,[rsp+136]
 jae .Ldelta_array_done
 cmp dword ptr [rsp+80],109
 je .Ldelta_permutation_item
 mov rsi,rax
 mov rdi,[rsp+48]
 cmp rax,[rsp+96]
 jb .Ldelta_array_source
 mov rcx,[rsp+96]
 add rcx,[rsp+120]
 cmp rax,rcx
 jae .Ldelta_array_suffix
 mov rdi,[rsp+112]
 sub rsi,[rsp+96]
 jmp .Ldelta_array_source
.Ldelta_array_suffix:
 sub rsi,[rsp+120]
 add rsi,[rsp+104]
 jmp .Ldelta_array_source
.Ldelta_permutation_item:
 mov ebp,1
 mov rdi,[rsp+112]
 mov rsi,rax
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call .Ldelta_count
 test edx,edx
 jz .Ldelta_cleanup
 cmp rax,[rsp+88]
 jae .Ldelta_cleanup
 mov rcx,[rsp+72]
 cmp byte ptr [rcx+rax],0
 jne .Ldelta_cleanup
 mov byte ptr [rcx+rax],1
 mov rdi,[rsp+48]
 mov rsi,rax
.Ldelta_array_source:
 call json_object_array_get_idx@PLT
 mov rsi,rax
 mov rdi,[rsp+64]
 call .Ldelta_append
 mov ebp,eax
 test eax,eax
 jnz .Ldelta_cleanup
 inc qword ptr [rsp+128]
 jmp .Ldelta_array_loop
.Ldelta_array_done:
 mov rax,[rsp+64]
 mov [rsp+56],rax
 mov qword ptr [rsp+64],0
 cmp qword ptr [rsp+24],0
 je .Ldelta_replace_owned
.Ldelta_write_owned:
 mov rdi,[rsp+32]
 mov rsi,[rsp+40]
 mov rdx,[rsp+56]
 call .Ldelta_set
 mov ebp,eax
 test eax,eax
 jnz .Ldelta_cleanup
 mov qword ptr [rsp+56],0
 jmp .Ldelta_next
.Ldelta_replace_owned:
 mov rdi,rbx
 call json_object_put@PLT
 mov rbx,[rsp+56]
 mov [rsp],rbx
 mov qword ptr [rsp+56],0
.Ldelta_next:
 mov rdi,[rsp+72]
 call free@PLT
 mov qword ptr [rsp+72],0
 inc r14
 jmp .Ldelta_op
.Ldelta_capacity:
 mov ebp,2
 jmp .Ldelta_cleanup
.Ldelta_success:
 mov [r13],rbx
 mov qword ptr [rsp],0
 xor ebp,ebp
.Ldelta_cleanup:
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov rdi,[rsp+72]
 call free@PLT
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
.Ldelta_return:
 mov eax,ebp
 add rsp,232
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_delta_apply,.-pg_document_delta_apply
.globl pg_document_delta_batches
.type pg_document_delta_batches,@function
pg_document_delta_batches:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rsp],0
 mov dword ptr [rsp+24],1
 test r13,r13
 jz .Lbatches_return
 mov qword ptr [r13],0
 mov rdi,r12
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lbatches_cleanup
 mov rdi,r12
 call pg_document_value_valid
 test eax,eax
 jz .Lbatches_cleanup
 mov rdi,r12
 call json_object_array_length@PLT
 mov [rsp+16],rax
 xor edi,edi
 mov dword ptr [rsp+24],3
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lbatches_cleanup
 mov [rsp],rax
 xor r14d,r14d
.Lbatches_outer:
 cmp r14,[rsp+16]
 jae .Lbatches_apply
 mov rdi,r12
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov [rsp+8],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 mov dword ptr [rsp+24],1
 test eax,eax
 jz .Lbatches_cleanup
 xor r15d,r15d
.Lbatches_inner:
 mov rdi,[rsp+8]
 call json_object_array_length@PLT
 cmp r15,rax
 jae .Lbatches_next
 mov rdi,[rsp+8]
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov rdi,[rsp]
 mov rsi,rax
 call .Ldelta_append
 mov [rsp+24],eax
 test eax,eax
 jnz .Lbatches_cleanup
 inc r15
 jmp .Lbatches_inner
.Lbatches_next:
 inc r14
 jmp .Lbatches_outer
.Lbatches_apply:
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r13
 call pg_document_delta_apply
 mov [rsp+24],eax
.Lbatches_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
.Lbatches_return:
 mov eax,[rsp+24]
 add rsp,32
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_document_delta_batches,.-pg_document_delta_batches

.section .rodata
.Lunsigned: .asciz "%llu"
.Lreserved: .long .Lproto-.Lreserved,.Lconstructor-.Lreserved,.Lprototype-.Lreserved
.Lproto: .asciz "__proto__"
.Lconstructor: .asciz "constructor"
.Lprototype: .asciz "prototype"
.section .note.GNU-stack,"",@progbits
