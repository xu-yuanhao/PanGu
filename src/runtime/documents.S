.intel_syntax noprefix
.text
.globl pg_document_address_valid
.type pg_document_address_valid,@function
pg_document_address_valid:
 xor eax,eax
 test rdi,rdi
 jz .Lvalid_return
 mov rcx,[rdi+8]
 cmp rcx,0x7fffffff
 ja .Lvalid_return
 test rcx,rcx
 jz .Lvalid_kind
 cmp qword ptr [rdi],0
 je .Lvalid_return
.Lvalid_kind:
 cmp dword ptr [rdi+40],2
 ja .Lvalid_return
 cmp dword ptr [rdi+44],1
 ja .Lvalid_return
 mov rcx,[rdi+32]
 cmp dword ptr [rdi+40],0
 jne .Lvalid_owner
 test rcx,rcx
 jnz .Lvalid_return
 jmp .Lvalid_key
.Lvalid_owner:
 mov rdx,9007199254740991
 cmp rcx,rdx
 jg .Lvalid_return
 neg rdx
 cmp rcx,rdx
 jl .Lvalid_return
.Lvalid_key:
 cmp dword ptr [rdi+44],0
 je .Lvalid_absent
 mov rcx,[rdi+24]
 cmp rcx,0x7fffffff
 ja .Lvalid_return
 test rcx,rcx
 jz .Lvalid_success
 cmp qword ptr [rdi+16],0
 je .Lvalid_return
 jmp .Lvalid_success
.Lvalid_absent:
 cmp qword ptr [rdi+24],0
 jne .Lvalid_return
.Lvalid_success:
 mov eax,1
.Lvalid_return:
 ret
.size pg_document_address_valid,.-pg_document_address_valid

.globl pg_document_address_equal
.type pg_document_address_equal,@function
pg_document_address_equal:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 call pg_document_address_valid
 test eax,eax
 jz .Lequal_return
 mov rdi,r12
 call pg_document_address_valid
 test eax,eax
 jz .Lequal_return
 mov rax,[rbx+40]
 cmp rax,[r12+40]
 jne .Lequal_no
 mov rax,[rbx+32]
 cmp rax,[r12+32]
 jne .Lequal_no
 mov rdx,[rbx+8]
 cmp rdx,[r12+8]
 jne .Lequal_no
 test rdx,rdx
 jz .Lequal_key
 mov rdi,[rbx]
 mov rsi,[r12]
 call memcmp@PLT
 test eax,eax
 jnz .Lequal_no
.Lequal_key:
 mov rdx,[rbx+24]
 cmp rdx,[r12+24]
 jne .Lequal_no
 test rdx,rdx
 jz .Lequal_yes
 mov rdi,[rbx+16]
 mov rsi,[r12+16]
 call memcmp@PLT
 test eax,eax
 jnz .Lequal_no
.Lequal_yes:
 mov eax,1
 jmp .Lequal_return
.Lequal_no:
 xor eax,eax
.Lequal_return:
 add rsp,8
 pop r12
 pop rbx
 ret
.size pg_document_address_equal,.-pg_document_address_equal

/* Array takes the value on success; release it ourselves on insertion failure. */
.Larray_add:
 sub rsp,24
 mov [rsp],rsi
 call json_object_array_add@PLT
 test eax,eax
 jz .Larray_added
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,1
.Larray_added:
 add rsp,24
 ret

.globl pg_document_address_encode
.type pg_document_address_encode,@function
pg_document_address_encode:
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
 xor r15d,r15d
 mov ebp,1
 test r14,r14
 jz .Lencode_return
 mov qword ptr [r14],0
 test r12,r12
 jz .Lencode_return
 call pg_document_address_valid
 test eax,eax
 jz .Lencode_return
 mov ebp,3
 mov edi,4
 call json_object_new_array_ext@PLT
 test rax,rax
 jz .Lencode_return
 mov r15,rax
 mov rdi,[rbx]
 mov esi,[rbx+8]
 test esi,esi
 jnz .Lencode_kind_string
 lea rdi,[rip+.Lempty]
.Lencode_kind_string:
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Lencode_cleanup
 mov rdi,r15
 mov rsi,rax
 call .Larray_add
 test eax,eax
 jnz .Lencode_cleanup
 mov eax,[rbx+40]
 lea rcx,[rip+.Lscopes]
 mov eax,[rcx+rax*4]
 add rcx,rax
 mov rdi,rcx
 call json_object_new_string@PLT
 test rax,rax
 jz .Lencode_cleanup
 mov rdi,r15
 mov rsi,rax
 call .Larray_add
 test eax,eax
 jnz .Lencode_cleanup
 xor esi,esi
 cmp dword ptr [rbx+40],0
 je .Lencode_owner
 mov rdi,[rbx+32]
 call json_object_new_int64@PLT
 test rax,rax
 jz .Lencode_cleanup
 mov rsi,rax
.Lencode_owner:
 mov rdi,r15
 call .Larray_add
 test eax,eax
 jnz .Lencode_cleanup
 xor esi,esi
 cmp dword ptr [rbx+44],0
 je .Lencode_key
 mov rdi,[rbx+16]
 mov esi,[rbx+24]
 test esi,esi
 jnz .Lencode_key_string
 lea rdi,[rip+.Lempty]
.Lencode_key_string:
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Lencode_cleanup
 mov rsi,rax
.Lencode_key:
 mov rdi,r15
 call .Larray_add
 test eax,eax
 jnz .Lencode_cleanup
 mov rdi,r15
 mov esi,16 /* JSON_C_TO_STRING_NOSLASHESCAPE, compact JSON. */
 mov rdx,rsp
 call json_object_to_json_string_length@PLT
 test rax,rax
 jz .Lencode_cleanup
 mov ebp,2
 mov rdx,[rsp]
 cmp rdx,r13
 jae .Lencode_cleanup
 mov [rsp+8],rdx
 mov rdi,r12
 mov rsi,rax
 inc rdx
 call memcpy@PLT
 mov rdx,[rsp+8]
 mov [r14],rdx
 xor ebp,ebp
.Lencode_cleanup:
 mov rdi,r15
 call json_object_put@PLT
.Lencode_return:
 mov eax,ebp
 add rsp,24
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_address_encode,.-pg_document_address_encode
.globl pg_document_check
.type pg_document_check,@function
pg_document_check:
 mov eax,1
 test rdi,rdi
 jz .Lcheck_return
 test rsi,rsi
 jz .Lcheck_return
 mov rcx,[rdi+16]
 test rcx,rcx
 jz .Lcheck_return
 mov r8,9007199254740991
 cmp rcx,r8
 ja .Lcheck_return
 test rdx,rdx
 jz .Lcheck_return
 cmp rdx,r8
 ja .Lcheck_return
 cmp dword ptr [rdi+24],2
 ja .Lcheck_return
 cmp dword ptr [rdi+36],1
 ja .Lcheck_return
 mov r8d,[rdi+24]
 cmp r8d,1
 jne .Lcheck_plain_definition
 mov r9d,[rdi+28]
 dec r9d
 cmp r9d,1
 ja .Lcheck_return
 mov r9d,[rdi+32]
 dec r9d
 cmp r9d,2
 ja .Lcheck_return
 cmp dword ptr [rdi+28],1
 jne .Lcheck_definition_ready
 cmp dword ptr [rdi+32],3
 je .Lcheck_return
 jmp .Lcheck_definition_ready
.Lcheck_plain_definition:
 cmp qword ptr [rdi+28],0
 jne .Lcheck_return
.Lcheck_definition_ready:
 cmp r8d,[rsi+40]
 jne .Lcheck_semantics
 cmp r8d,1
 jne .Lcheck_version
 mov r8d,[rdi+28]
 cmp r8d,[rsi+56]
 jne .Lcheck_semantics
 mov r8d,[rdi+32]
 cmp r8d,[rsi+60]
 jne .Lcheck_semantics
.Lcheck_version:
 cmp rdx,rcx
 ja .Lcheck_newer
 je .Lcheck_success
 cmp qword ptr [rdi+48],0
 je .Lcheck_migration
.Lcheck_success:
 xor eax,eax
 ret
.Lcheck_semantics:
 mov eax,4
 ret
.Lcheck_newer:
 mov eax,5
 ret
.Lcheck_migration:
 mov eax,6
.Lcheck_return:
 ret
.size pg_document_check,.-pg_document_check

.globl pg_document_value_valid
.type pg_document_value_valid,@function
pg_document_value_valid:
 xor esi,esi
 xor edx,edx
.Lvalue:
 test rdi,rdi
 jz .Lvalue_null
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov [rsp],rdi
 mov [rsp+8],rsi
 call json_object_get_type@PLT
 cmp eax,6
 ja .Lvalue_bad
 cmp eax,2
 je .Lvalue_double
 cmp eax,4
 je .Lvalue_container
 cmp eax,5
 je .Lvalue_container
 jmp .Lvalue_good
.Lvalue_double:
 mov rdi,rbx
 call json_object_get_double@PLT
 movq rax,xmm0
 mov rcx,0x7ff0000000000000
 and rax,rcx
 cmp rax,rcx
 je .Lvalue_bad
 jmp .Lvalue_good
.Lvalue_container:
 cmp r13,256
 jae .Lvalue_bad
 mov r14d,eax
 mov rax,r12
.Lvalue_ancestor:
 test rax,rax
 jz .Lvalue_children
 cmp [rax],rbx
 je .Lvalue_bad
 mov rax,[rax+8]
 jmp .Lvalue_ancestor
.Lvalue_children:
 cmp r14d,5
 je .Lvalue_array
 mov rdi,rbx
 call json_object_iter_begin@PLT
 mov [rsp+16],rax
 mov rdi,rbx
 call json_object_iter_end@PLT
 mov [rsp+24],rax
.Lvalue_object_loop:
 lea rdi,[rsp+16]
 lea rsi,[rsp+24]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lvalue_good
 lea rdi,[rsp+16]
 call json_object_iter_peek_value@PLT
 mov rdi,rax
 mov rsi,rsp
 lea rdx,[r13+1]
 call .Lvalue
 test eax,eax
 jz .Lvalue_bad
 lea rdi,[rsp+16]
 call json_object_iter_next@PLT
 jmp .Lvalue_object_loop
.Lvalue_array:
 mov rdi,rbx
 call json_object_array_length@PLT
 mov r15,rax
 xor r14d,r14d
.Lvalue_array_loop:
 cmp r14,r15
 jae .Lvalue_good
 mov rdi,rbx
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov rsi,rsp
 lea rdx,[r13+1]
 call .Lvalue
 test eax,eax
 jz .Lvalue_bad
 inc r14
 jmp .Lvalue_array_loop
.Lvalue_good:
 mov eax,1
 jmp .Lvalue_return
.Lvalue_bad:
 xor eax,eax
.Lvalue_return:
 add rsp,32
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Lvalue_null:
 mov eax,1
 ret
.size pg_document_value_valid,.-pg_document_value_valid

.globl pg_document_materialize
.type pg_document_materialize,@function
pg_document_materialize:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,24
 mov rbx,rdi
 mov r12,rdx
 mov r13,rcx
 mov r14,r8
 xor r15d,r15d
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 test r14,r14
 jz .Lmaterialize_return
 mov qword ptr [r14],0
 call pg_document_check
 test eax,eax
 jnz .Lmaterialize_check_failed
 mov rdi,r13
 mov esi,4 /* json_type_object */
 call json_object_is_type@PLT
 test eax,eax
 jz .Lmaterialize_return
 mov rdi,r13
 call pg_document_value_valid
 test eax,eax
 jz .Lmaterialize_return
 mov ebp,3
 mov rdi,r13
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lmaterialize_cleanup
 cmp r12,[rbx+16]
 je .Lmaterialize_same
 mov rdi,[rbx+40]
 mov rsi,[rsp]
 mov rdx,r12
 lea rcx,[rsp+8]
 call [rbx+48]
 mov ebp,7
 test eax,eax
 jnz .Lmaterialize_cleanup
 mov rdi,[rsp+8]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lmaterialize_cleanup
 mov rdi,[rsp+8]
 call pg_document_value_valid
 test eax,eax
 jz .Lmaterialize_cleanup
 mov ebp,3
 mov rdi,[rsp+8]
 lea rsi,[rsp+16]
 mov qword ptr [rsp+16],0
 xor edx,edx
 call json_object_deep_copy@PLT
 mov r15,[rsp+16]
 test eax,eax
 jnz .Lmaterialize_cleanup
 mov [r14],r15
 xor r15d,r15d
 xor ebp,ebp
 jmp .Lmaterialize_cleanup
.Lmaterialize_same:
 mov rax,[rsp]
 mov [r14],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
.Lmaterialize_cleanup:
 mov rdi,r15
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp]
 call json_object_put@PLT
 jmp .Lmaterialize_return
.Lmaterialize_check_failed:
 mov ebp,eax
.Lmaterialize_return:
 mov eax,ebp
 add rsp,24
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_materialize,.-pg_document_materialize

.section .rodata
.Lscopes: .long .Lsession-.Lscopes,.Lconversation-.Lscopes,.Ltask-.Lscopes
.Lsession: .asciz "session"
.Lconversation: .asciz "conversation"
.Ltask: .asciz "task"
.Lempty: .asciz ""
.section .note.GNU-stack,"",@progbits
.text
.Laccess_name:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Laccess_name_return
 mov rdi,rbx
 call json_object_get_string_len@PLT
 mov [rsp],eax
 mov rdi,r12
 call strlen@PLT
 cmp eax,[rsp]
 jne .Laccess_name_no
 mov rdi,rbx
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,r12
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 jmp .Laccess_name_return
.Laccess_name_no:
 xor eax,eax
.Laccess_name_return:
 add rsp,8
 pop r12
 pop rbx
 ret
/* Nonnegative safe integral JSON number; RDX indicates validity. */
.globl pg_document_safe_number
.hidden pg_document_safe_number
.type pg_document_safe_number,@function
pg_document_safe_number:
.Laccess_number:
 push rbx
 mov rbx,rdi
 call json_object_get_type@PLT
 cmp eax,3
 je .Laccess_number_int
 cmp eax,2
 jne .Laccess_number_bad
 mov rdi,rbx
 call json_object_get_double@PLT
 cvttsd2si rax,xmm0
 cvtsi2sd xmm1,rax
 ucomisd xmm0,xmm1
 jp .Laccess_number_bad
 jne .Laccess_number_bad
 jmp .Laccess_number_check
.Laccess_number_int:
 mov rdi,rbx
 call json_object_get_int64@PLT
.Laccess_number_check:
 test rax,rax
 js .Laccess_number_bad
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Laccess_number_bad
 mov edx,1
 pop rbx
 ret
.Laccess_number_bad:
 xor eax,eax
 xor edx,edx
 pop rbx
 ret
.size pg_document_safe_number,.-pg_document_safe_number
.macro AGET object,key
 mov rdi,\object
 lea rsi,[rip+.La_\key]
 call json_object_object_get@PLT
.endm
.globl pg_document_record_decode
.type pg_document_record_decode,@function
pg_document_record_decode:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov r12,rsi
 mov ebp,1
 test r12,r12
 jz .Laccess_decode_return
 pxor xmm0,xmm0
 movups [r12],xmm0
 movups [r12+16],xmm0
 movups [r12+32],xmm0
 movups [r12+48],xmm0
 movups [rsp],xmm0
 movups [rsp+16],xmm0
 movups [rsp+32],xmm0
 movups [rsp+48],xmm0
 mov rdi,rbx
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Laccess_decode_return
 AGET rbx,kind
 mov r13,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Laccess_decode_return
 mov rdi,r13
 call json_object_get_string@PLT
 mov [rsp],rax
 mov rdi,r13
 call json_object_get_string_len@PLT
 mov eax,eax
 mov [rsp+8],rax
 mov rdi,rbx
 lea rsi,[rip+.La_key]
 lea rdx,[rsp+16]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Laccess_decode_scope
 mov dword ptr [rsp+44],1
 mov r13,[rsp+16]
 mov rdi,r13
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Laccess_decode_return
 mov rdi,r13
 call json_object_get_string@PLT
 mov [rsp+16],rax
 mov rdi,r13
 call json_object_get_string_len@PLT
 mov eax,eax
 mov [rsp+24],rax
.Laccess_decode_scope:
 AGET rbx,id
 mov rdi,rax
 call .Laccess_number
 test rax,rax
 jz .Laccess_decode_return
 mov [rsp+48],rax
 AGET rbx,scope
 mov r13,rax
 AGET r13,kind
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lsession]
 call .Laccess_name
 test eax,eax
 jnz .Laccess_decode_valid
 mov rdi,r14
 lea rsi,[rip+.Lconversation]
 call .Laccess_name
 test eax,eax
 jz .Laccess_decode_task
 mov dword ptr [rsp+40],1
 AGET r13,conversation
 jmp .Laccess_decode_owner
.Laccess_decode_task:
 mov rdi,r14
 lea rsi,[rip+.Ltask]
 call .Laccess_name
 test eax,eax
 jz .Laccess_decode_return
 mov dword ptr [rsp+40],2
 AGET r13,task
.Laccess_decode_owner:
 mov rdi,rax
 call .Laccess_number
 test rax,rax
 jz .Laccess_decode_return
 mov [rsp+32],rax
 cmp dword ptr [rsp+40],1
 jne .Laccess_decode_valid
 AGET rbx,history
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.La_latest]
 call .Laccess_name
 mov dword ptr [rsp+56],1
 test eax,eax
 jnz .Laccess_decode_fork
 mov rdi,r14
 lea rsi,[rip+.La_rewindable]
 call .Laccess_name
 test eax,eax
 jz .Laccess_decode_return
 mov dword ptr [rsp+56],2
.Laccess_decode_fork:
 AGET rbx,fork
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.La_current]
 call .Laccess_name
 mov dword ptr [rsp+60],1
 test eax,eax
 jnz .Laccess_decode_valid
 mov rdi,r14
 lea rsi,[rip+.La_initial]
 call .Laccess_name
 mov dword ptr [rsp+60],2
 test eax,eax
 jnz .Laccess_decode_valid
 mov rdi,r14
 lea rsi,[rip+.La_asof]
 call .Laccess_name
 test eax,eax
 jz .Laccess_decode_return
 mov dword ptr [rsp+60],3
.Laccess_decode_valid:
 mov rdi,rsp
 call pg_document_address_valid
 test eax,eax
 jz .Laccess_decode_return
 movups xmm0,[rsp]
 movups xmm1,[rsp+16]
 movups xmm2,[rsp+32]
 movups xmm3,[rsp+48]
 movups [r12],xmm0
 movups [r12+16],xmm1
 movups [r12+32],xmm2
 movups [r12+48],xmm3
 xor ebp,ebp
.Laccess_decode_return:
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_record_decode,.-pg_document_record_decode

.globl pg_document_loaded_open
.type pg_document_loaded_open,@function
pg_document_loaded_open:
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
 mov r14,rcx
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 test rbx,rbx
 jz .Lloaded_open_return
 cmp dword ptr [rbx+112],0
 jne .Lloaded_open_return
 cmp dword ptr [rbx+116],0
 jne .Lloaded_open_return
 test r12,r12
 jz .Lloaded_open_return
 test r13,r13
 jz .Lloaded_open_return
 cmp qword ptr [r12+8],0
 je .Lloaded_open_return
 cmp qword ptr [r12+16],0
 je .Lloaded_open_return
 mov dword ptr [rbx+112],1
 mov rdi,r14
 call pg_document_address_valid
 test eax,eax
 jz .Lloaded_open_cleanup
 mov rax,[r13+8]
 cmp rax,[r14+8]
 jne .Lloaded_open_cleanup
 cmp dword ptr [r13+36],1
 ja .Lloaded_open_cleanup
 mov ecx,[r13+36]
 cmp ecx,[r14+44]
 jne .Lloaded_open_cleanup
 test rax,rax
 jz .Lloaded_open_kind_ready
 mov rdi,[r13]
 test rdi,rdi
 jz .Lloaded_open_cleanup
 mov rsi,[r14]
 mov rdx,rax
 call memcmp@PLT
 test eax,eax
 jnz .Lloaded_open_cleanup
.Lloaded_open_kind_ready:
 mov rdi,[r12]
 mov rsi,r14
 mov rdx,-1
 mov rcx,rsp
 call [r12+8]
 test eax,eax
 jz .Lloaded_open_found
 cmp eax,1
 jne .Lloaded_open_storage
 mov ebp,8
 jmp .Lloaded_open_cleanup
.Lloaded_open_found:
 mov rdi,[rsp]
 lea rsi,[rbx+24]
 call pg_document_record_decode
 test eax,eax
 jnz .Lloaded_open_cleanup
 lea rdi,[rbx+24]
 mov rsi,r14
 call pg_document_address_equal
 test eax,eax
 jz .Lloaded_open_cleanup
 mov r15,[rbx+72]
 mov rdi,[r12]
 mov rsi,r15
 mov rdx,-1
 lea rcx,[rsp+8]
 call [r12+16]
 test eax,eax
 jnz .Lloaded_open_storage
 AGET "qword ptr [rsp+8]",record
 mov [rsp+24],rax
 mov rdi,rax
 lea rsi,[rbx+24]
 call pg_document_record_decode
 test eax,eax
 jnz .Lloaded_open_cleanup
 cmp [rbx+72],r15
 jne .Lloaded_open_cleanup
 lea rdi,[rbx+24]
 mov rsi,r14
 call pg_document_address_equal
 test eax,eax
 jz .Lloaded_open_cleanup
 AGET "qword ptr [rsp+8]",version
 mov rdi,rax
 call .Laccess_number
 test rax,rax
 jz .Lloaded_open_cleanup
 mov [rbx+88],rax
 mov rax,[r13+16]
 mov [rbx+96],rax
 AGET "qword ptr [rsp+8]",deltas
 mov rdi,rax
 call .Laccess_number
 test edx,edx
 jz .Lloaded_open_cleanup
 mov [rbx+104],rax
 AGET "qword ptr [rsp+8]",value
 mov rcx,rax
 mov rdi,r13
 lea rsi,[rbx+24]
 mov rdx,[rbx+88]
 lea r8,[rsp+16]
 call pg_document_materialize
 mov ebp,eax
 test eax,eax
 jnz .Lloaded_open_cleanup
 mov rdi,[rsp+24]
 call json_object_get@PLT
 mov [rbx],rax
 mov rax,[rsp+16]
 mov [rbx+8],rax
 mov qword ptr [rsp+16],0
 mov [rbx+16],r13
 mov dword ptr [rbx+116],1
 xor ebp,ebp
 jmp .Lloaded_open_cleanup
.Lloaded_open_storage:
 mov ebp,9
.Lloaded_open_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 test ebp,ebp
 jz .Lloaded_open_success
 mov rdi,rbx
 xor eax,eax
 mov ecx,15
 rep stosq
 jmp .Lloaded_open_return
.Lloaded_open_success:
 mov dword ptr [rbx+112],0
.Lloaded_open_return:
 mov eax,ebp
 add rsp,40
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_loaded_open,.-pg_document_loaded_open

.globl pg_document_loaded_close
.type pg_document_loaded_close,@function
pg_document_loaded_close:
 mov eax,1
 test rdi,rdi
 jz .Lloaded_close_return
 cmp dword ptr [rdi+112],0
 jne .Lloaded_close_return
 push rbx
 mov rbx,rdi
 mov dword ptr [rbx+112],1
 mov rdi,[rbx]
 call json_object_put@PLT
 mov rdi,[rbx+8]
 call json_object_put@PLT
 mov rdi,rbx
 xor eax,eax
 mov ecx,15
 rep stosq
 xor eax,eax
 pop rbx
.Lloaded_close_return:
 ret
.size pg_document_loaded_close,.-pg_document_loaded_close
.section .rodata
.La_kind: .asciz "kind"
.La_key: .asciz "key"
.La_id: .asciz "id"
.La_scope: .asciz "scope"
.La_conversation: .asciz "conversationId"
.La_task: .asciz "taskId"
.La_history: .asciz "history"
.La_fork: .asciz "fork"
.La_latest: .asciz "latest"
.La_rewindable: .asciz "rewindable"
.La_current: .asciz "current"
.La_initial: .asciz "initial"
.La_asof: .asciz "asOf"
.La_record: .asciz "record"
.La_version: .asciz "version"
.La_deltas: .asciz "deltasSinceBase"
.La_value: .asciz "value"
.text
/* Cache node: all prev/next, idle prev/next, since, reserved, loaded[120]. */
.Lcache_hash:
 mov r10,rdi
 mov rax,0xcbf29ce484222325
 mov r8,0x100000001b3
 xor rax,[r10+32]
 imul rax,r8
 xor rax,[r10+40]
 imul rax,r8
 xor rax,[r10+8]
 imul rax,r8
 xor rax,[r10+24]
 imul rax,r8
 mov rdx,[r10]
 mov rcx,[r10+8]
.Lcache_hash_kind:
 test rcx,rcx
 jz .Lcache_hash_key_start
 movzx r9d,byte ptr [rdx]
 xor rax,r9
 imul rax,r8
 inc rdx
 dec rcx
 jmp .Lcache_hash_kind
.Lcache_hash_key_start:
 mov rdx,[r10+16]
 mov rcx,[r10+24]
.Lcache_hash_key:
 test rcx,rcx
 jz .Lcache_hash_return
 movzx r9d,byte ptr [rdx]
 xor rax,r9
 imul rax,r8
 inc rdx
 dec rcx
 jmp .Lcache_hash_key
.Lcache_hash_return:
 ret
.Lcache_equal:
 jmp pg_document_address_equal@PLT

/* Remove node from idle chain if present. */
.Lcache_unidle:
 mov rax,[rsi+16]
 mov rdx,[rsi+24]
 test rax,rax
 jnz .Lcache_unidle_prev
 cmp [rdi+16],rsi
 jne .Lcache_unidle_return
 mov [rdi+16],rdx
 jmp .Lcache_unidle_next
.Lcache_unidle_prev:
 mov [rax+24],rdx
.Lcache_unidle_next:
 test rdx,rdx
 jz .Lcache_unidle_tail
 mov [rdx+16],rax
 jmp .Lcache_unidle_clear
.Lcache_unidle_tail:
 mov [rdi+24],rax
.Lcache_unidle_clear:
 mov qword ptr [rsi+16],0
 mov qword ptr [rsi+24],0
.Lcache_unidle_return:
 ret
.Lcache_touch_node:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 cmp qword ptr [rbx+96],0
 je .Lcache_touch_clock
 mov rdi,[rbx+64]
 mov rsi,[r12+120]
 call [rbx+96]
 test eax,eax
 jnz .Lcache_touch_pinned
.Lcache_touch_clock:
 mov rdi,[rbx+64]
 call [rbx+72]
 mov r13,rax
 cmp qword ptr [r12+16],0
 jne .Lcache_touch_since
 cmp [rbx+16],r12
 jne .Lcache_touch_move
.Lcache_touch_since:
 cmp r13,[r12+32]
 jb .Lcache_touch_return
 mov rcx,r13
 sub rcx,[r12+32]
 mov rax,[rbx+80]
 xor edx,edx
 mov esi,10
 div rsi
 test rdx,rdx
 jz .Lcache_touch_round_ready
 inc rax
.Lcache_touch_round_ready:
 cmp rax,1000
 jbe .Lcache_touch_granule
 mov eax,1000
.Lcache_touch_granule:
 cmp rcx,rax
 jb .Lcache_touch_return
.Lcache_touch_move:
 mov rdi,rbx
 mov rsi,r12
 call .Lcache_unidle
 mov [r12+32],r13
 mov rax,[rbx+24]
 mov [r12+16],rax
 test rax,rax
 jz .Lcache_touch_first
 mov [rax+24],r12
 jmp .Lcache_touch_tail
.Lcache_touch_first:
 mov [rbx+16],r12
.Lcache_touch_tail:
 mov [rbx+24],r12
 jmp .Lcache_touch_return
.Lcache_touch_pinned:
 mov rdi,rbx
 mov rsi,r12
 call .Lcache_unidle
.Lcache_touch_return:
 pop r13
 pop r12
 pop rbx
 ret

.Lcache_drop_node:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,[rbx]
 lea rsi,[r12+72]
 call lh_table_delete@PLT
 mov rdi,rbx
 mov rsi,r12
 call .Lcache_unidle
 mov rax,[r12]
 mov rdx,[r12+8]
 test rax,rax
 jz .Lcache_drop_head
 mov [rax+8],rdx
 jmp .Lcache_drop_next
.Lcache_drop_head:
 mov [rbx+8],rdx
.Lcache_drop_next:
 test rdx,rdx
 jz .Lcache_drop_close
 mov [rdx],rax
.Lcache_drop_close:
 dec qword ptr [rbx+32]
 cmp qword ptr [rbx+32],0
 jne .Lcache_drop_loaded
 cmp dword ptr [rbx+108],0
 jne .Lcache_drop_loaded
 mov rdi,[rbx]
 mov esi,16
 call lh_table_resize@PLT
.Lcache_drop_loaded:
 lea rdi,[r12+48]
 call pg_document_loaded_close
 mov rdi,r12
 call free@PLT
 add rsp,8
 pop r12
 pop rbx
 ret

.globl pg_document_cache_open
.type pg_document_cache_open,@function
pg_document_cache_open:
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
 jz .Lcache_open_return
 test r12,r12
 jz .Lcache_open_return
 test r13,r13
 jz .Lcache_open_return
 cmp dword ptr [rbx+104],0
 jne .Lcache_open_return
 cmp qword ptr [rbx],0
 jne .Lcache_open_return
 cmp qword ptr [r12+8],0
 je .Lcache_open_return
 cmp qword ptr [r12+16],0
 je .Lcache_open_return
 cmp qword ptr [r13+8],0
 je .Lcache_open_return
 mov dword ptr [rbx+104],1
 mov edi,16
 xor esi,esi
 lea rdx,[rip+.Lcache_hash]
 lea rcx,[rip+.Lcache_equal]
 call lh_table_new@PLT
 test rax,rax
 jz .Lcache_open_allocation
 mov [rbx],rax
 mov qword ptr [rbx+8],0
 mov qword ptr [rbx+16],0
 mov qword ptr [rbx+24],0
 mov qword ptr [rbx+32],0
 movups xmm0,[r12]
 movups [rbx+40],xmm0
 mov rax,[r12+16]
 mov [rbx+56],rax
 mov rax,[r13]
 mov [rbx+64],rax
 mov rax,[r13+8]
 mov [rbx+72],rax
 mov [rbx+80],r14
 mov rax,[r13+16]
 mov [rbx+88],rax
 mov rax,[r13+24]
 mov [rbx+96],rax
 mov qword ptr [rbx+104],0
 xor eax,eax
 jmp .Lcache_open_return
.Lcache_open_allocation:
 mov dword ptr [rbx+104],0
 mov eax,3
.Lcache_open_return:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_document_cache_open,.-pg_document_cache_open

.globl pg_document_cache_get
.type pg_document_cache_get,@function
pg_document_cache_get:
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
 mov r14,rcx
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 lea rdi,[rsp+16]
 xor eax,eax
 mov ecx,15
 rep stosq
 test r14,r14
 jz .Lcache_get_return
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Lcache_get_return
 test r12,r12
 jz .Lcache_get_return
 cmp qword ptr [rbx],0
 je .Lcache_get_return
 cmp qword ptr [rbx+104],0
 jne .Lcache_get_return
 mov dword ptr [rbx+104],1
 mov rdi,r13
 call pg_document_address_valid
 test eax,eax
 jz .Lcache_get_cleanup
 mov rax,[r12+8]
 cmp rax,[r13+8]
 jne .Lcache_get_cleanup
 mov ecx,[r12+36]
 cmp ecx,[r13+44]
 jne .Lcache_get_cleanup
 test rax,rax
 jz .Lcache_get_key_ready
 mov rdi,[r12]
 test rdi,rdi
 jz .Lcache_get_cleanup
 mov rsi,[r13]
 mov rdx,rax
 call memcmp@PLT
 test eax,eax
 jnz .Lcache_get_cleanup
.Lcache_get_key_ready:
 mov rdi,[rbx]
 mov rsi,r13
 mov rdx,rsp
 call lh_table_lookup_ex@PLT
 test eax,eax
 jz .Lcache_get_load
 mov r15,[rsp]
 mov rax,[r12+16]
 cmp rax,[r15+144]
 je .Lcache_get_hit
 cmp qword ptr [rbx+88],0
 je .Lcache_get_drop
 mov rdi,[rbx+64]
 lea rsi,[r15+48]
 mov dword ptr [rbx+104],2
 call [rbx+88]
 mov dword ptr [rbx+104],1
 test eax,eax
 jnz .Lcache_get_flush_failed
.Lcache_get_drop:
 mov rdi,rbx
 mov rsi,r15
 call .Lcache_drop_node
 mov qword ptr [rsp],0
.Lcache_get_load:
 cmp qword ptr [rbx+32],0x3fffffff
 jae .Lcache_get_capacity
 lea rdi,[rsp+16]
 lea rsi,[rbx+40]
 mov rdx,r12
 mov rcx,r13
 call pg_document_loaded_open
 mov ebp,eax
 test eax,eax
 jnz .Lcache_get_cleanup
 mov edi,1
 mov esi,168
 call calloc@PLT
 test rax,rax
 jz .Lcache_get_allocation
 mov r15,rax
 mov [rsp+8],rax
 lea rdi,[rax+48]
 lea rsi,[rsp+16]
 mov ecx,15
 rep movsq
 lea rdi,[rsp+16]
 xor eax,eax
 mov ecx,15
 rep stosq
 mov rdi,[rbx]
 lea rsi,[r15+72]
 mov rdx,r15
 call lh_table_insert@PLT
 test eax,eax
 jnz .Lcache_get_allocation
 mov rax,[rbx+8]
 mov [r15+8],rax
 test rax,rax
 jz .Lcache_get_first
 mov [rax],r15
.Lcache_get_first:
 mov [rbx+8],r15
 inc qword ptr [rbx+32]
 mov qword ptr [rsp+8],0
.Lcache_get_hit:
 mov rdi,r12
 lea rsi,[r15+72]
 mov rdx,[r15+136]
 call pg_document_check
 test eax,eax
 jnz .Lcache_get_check_failed
 mov rdi,rbx
 mov rsi,r15
 call .Lcache_touch_node
 lea rax,[r15+48]
 mov [r14],rax
 xor ebp,ebp
 jmp .Lcache_get_cleanup
.Lcache_get_check_failed:
 mov ebp,eax
 jmp .Lcache_get_cleanup
.Lcache_get_flush_failed:
 mov ebp,9
 jmp .Lcache_get_cleanup
.Lcache_get_capacity:
 mov ebp,2
 jmp .Lcache_get_cleanup
.Lcache_get_allocation:
 mov ebp,3
.Lcache_get_cleanup:
 cmp qword ptr [rsp+8],0
 je .Lcache_get_temp_close
 mov rdi,[rsp+8]
 add rdi,48
 call pg_document_loaded_close
 mov rdi,[rsp+8]
 call free@PLT
.Lcache_get_temp_close:
 lea rdi,[rsp+16]
 call pg_document_loaded_close
 mov dword ptr [rbx+104],0
.Lcache_get_return:
 mov eax,ebp
 add rsp,152
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_cache_get,.-pg_document_cache_get

.globl pg_document_cache_touch
.type pg_document_cache_touch,@function
pg_document_cache_touch:
 xor edx,edx
 jmp .Lcache_touch_public_common
.globl pg_document_cache_touch_flushing
.type pg_document_cache_touch_flushing,@function
pg_document_cache_touch_flushing:
 mov edx,2
.Lcache_touch_public_common:
 push rbx
 sub rsp,32
 mov rbx,rdi
 mov eax,1
 test rbx,rbx
 jz .Lcache_public_touch_return
 cmp qword ptr [rbx],0
 je .Lcache_public_touch_return
 cmp dword ptr [rbx+108],0
 jne .Lcache_public_touch_return
 cmp [rbx+104],edx
 jne .Lcache_public_touch_return
 mov [rsp+16],edx
 mov [rsp+8],rsi
 test edx,edx
 jz .Lcache_touch_guard_public
 mov dword ptr [rbx+104],3
 jmp .Lcache_touch_guard_ready
.Lcache_touch_guard_public:
 mov dword ptr [rbx+104],1
.Lcache_touch_guard_ready:
 mov rdi,rsi
 call pg_document_address_valid
 test eax,eax
 jz .Lcache_public_touch_invalid
 mov rdi,[rbx]
 mov rsi,[rsp+8]
 mov rdx,rsp
 call lh_table_lookup_ex@PLT
 test eax,eax
 jz .Lcache_public_touch_done
 mov rdi,rbx
 mov rsi,[rsp]
 call .Lcache_touch_node
.Lcache_public_touch_done:
 xor eax,eax
 jmp .Lcache_public_touch_settled
.Lcache_public_touch_invalid:
 mov eax,1
.Lcache_public_touch_settled:
 mov ecx,[rsp+16]
 mov [rbx+104],ecx
.Lcache_public_touch_return:
 add rsp,32
 pop rbx
 ret
.size pg_document_cache_touch,.-pg_document_cache_touch
.size pg_document_cache_touch_flushing,.-pg_document_cache_touch_flushing

.globl pg_document_cache_evict
.type pg_document_cache_evict,@function
pg_document_cache_evict:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov r12,rdx
 mov eax,1
 test rbx,rbx
 jz .Lcache_evict_return
 cmp qword ptr [rbx],0
 je .Lcache_evict_return
 cmp qword ptr [rbx+104],0
 jne .Lcache_evict_return
 mov [rsp+8],rsi
 mov dword ptr [rbx+104],1
 mov rdi,rsi
 call pg_document_address_valid
 test eax,eax
 jz .Lcache_evict_invalid
 mov rdi,[rbx]
 mov rsi,[rsp+8]
 mov rdx,rsp
 call lh_table_lookup_ex@PLT
 test eax,eax
 jz .Lcache_evict_done
 mov rsi,[rsp]
 cmp [rsi+120],r12
 jne .Lcache_evict_done
 mov rdi,rbx
 call .Lcache_drop_node
.Lcache_evict_done:
 xor eax,eax
 jmp .Lcache_evict_settled
.Lcache_evict_invalid:
 mov eax,1
.Lcache_evict_settled:
 mov dword ptr [rbx+104],0
.Lcache_evict_return:
 add rsp,24
 pop r12
 pop rbx
 ret
.size pg_document_cache_evict,.-pg_document_cache_evict

.globl pg_document_cache_sweep
.type pg_document_cache_sweep,@function
pg_document_cache_sweep:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov eax,1
 test rbx,rbx
 jz .Lcache_sweep_return
 cmp qword ptr [rbx],0
 je .Lcache_sweep_return
 cmp qword ptr [rbx+104],0
 jne .Lcache_sweep_return
 mov dword ptr [rbx+104],1
 mov rdi,[rbx+64]
 call [rbx+72]
 mov r12,rax
.Lcache_sweep_each:
 mov r13,[rbx+16]
 test r13,r13
 jz .Lcache_sweep_done
 cmp qword ptr [rbx+96],0
 je .Lcache_sweep_time
 mov rdi,[rbx+64]
 mov rsi,[r13+120]
 call [rbx+96]
 test eax,eax
 jz .Lcache_sweep_time
 mov rdi,rbx
 mov rsi,r13
 call .Lcache_unidle
 jmp .Lcache_sweep_each
.Lcache_sweep_time:
 cmp r12,[r13+32]
 jb .Lcache_sweep_done
 mov rax,r12
 sub rax,[r13+32]
 cmp rax,[rbx+80]
 jb .Lcache_sweep_done
 mov rdi,rbx
 mov rsi,r13
 call .Lcache_drop_node
 jmp .Lcache_sweep_each
.Lcache_sweep_done:
.Lcache_sweep_settled:
 mov dword ptr [rbx+104],0
 xor eax,eax
.Lcache_sweep_return:
 pop r13
 pop r12
 pop rbx
 ret
.size pg_document_cache_sweep,.-pg_document_cache_sweep

.globl pg_document_cache_retention
.type pg_document_cache_retention,@function
pg_document_cache_retention:
 mov eax,1
 test rdi,rdi
 jz .Lcache_retention_return
 cmp qword ptr [rdi],0
 je .Lcache_retention_return
 cmp qword ptr [rdi+104],0
 jne .Lcache_retention_return
 mov [rdi+80],rsi
 xor eax,eax
.Lcache_retention_return:
 ret
.size pg_document_cache_retention,.-pg_document_cache_retention

.globl pg_document_cache_close
.type pg_document_cache_close,@function
pg_document_cache_close:
 push rbx
 mov eax,1
 test rdi,rdi
 jz .Lcache_close_return
 mov rbx,rdi
 cmp dword ptr [rbx+104],0
 jne .Lcache_close_return
 mov dword ptr [rbx+104],1
 mov dword ptr [rbx+108],1
.Lcache_close_each:
 mov rsi,[rbx+8]
 test rsi,rsi
 jz .Lcache_close_table
 mov rdi,rbx
 call .Lcache_drop_node
 jmp .Lcache_close_each
.Lcache_close_table:
 mov rdi,[rbx]
 test rdi,rdi
 jz .Lcache_close_clear
 call lh_table_free@PLT
.Lcache_close_clear:
 mov rdi,rbx
 xor eax,eax
 mov ecx,14
 rep stosq
 mov dword ptr [rbx+108],1
 xor eax,eax
.Lcache_close_return:
 pop rbx
 ret
.size pg_document_cache_close,.-pg_document_cache_close
.text
/* Attach allocated scalar RAX to object RDI/key RSI; consumes on either result. */
.Lcreate_attach:
 test rax,rax
 jz .Lcreate_attach_missing
 push rbx
 mov rbx,rax
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Lcreate_attach_return
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,1
.Lcreate_attach_return:
 pop rbx
 ret
.Lcreate_attach_missing:
 mov eax,1
 ret

.globl pg_document_record_create
.type pg_document_record_create,@function
pg_document_record_create:
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
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 test r14,r14
 jz .Lrecord_create_return
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Lrecord_create_return
 mov rdi,r12
 call pg_document_address_valid
 test eax,eax
 jz .Lrecord_create_return
 test r13,r13
 jz .Lrecord_create_return
 mov rax,9007199254740991
 cmp r13,rax
 ja .Lrecord_create_return
 movups xmm0,[r12]
 movups xmm1,[r12+16]
 movups xmm2,[r12+32]
 movups [rsp+16],xmm0
 movups [rsp+32],xmm1
 movups [rsp+48],xmm2
 mov [rsp+64],r13
 mov rax,[rbx+28]
 mov [rsp+72],rax
 mov rdi,rbx
 lea rsi,[rsp+16]
 mov rdx,[rbx+16]
 call pg_document_check
 mov ebp,eax
 test eax,eax
 jnz .Lrecord_create_return
 mov ebp,1
 mov eax,[rbx+36]
 cmp eax,[r12+44]
 jne .Lrecord_create_return
 mov rdx,[rbx+8]
 cmp rdx,[r12+8]
 jne .Lrecord_create_return
 test rdx,rdx
 jz .Lrecord_create_allocate
 mov rdi,[rbx]
 test rdi,rdi
 jz .Lrecord_create_return
 mov rsi,[r12]
 call memcmp@PLT
 test eax,eax
 jnz .Lrecord_create_return
.Lrecord_create_allocate:
 mov ebp,3
 call json_object_new_object@PLT
 test rax,rax
 jz .Lrecord_create_return
 mov [rsp],rax
 mov rdi,r13
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.La_id]
 call .Lcreate_attach
 test eax,eax
 jnz .Lrecord_create_cleanup
 mov rdi,[r12]
 mov esi,[r12+8]
 test esi,esi
 jnz .Lrecord_create_kind
 lea rdi,[rip+.Lempty]
.Lrecord_create_kind:
 call json_object_new_string_len@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.La_kind]
 call .Lcreate_attach
 test eax,eax
 jnz .Lrecord_create_cleanup
 cmp dword ptr [r12+44],0
 je .Lrecord_create_scope
 mov rdi,[r12+16]
 mov esi,[r12+24]
 test esi,esi
 jnz .Lrecord_create_key
 lea rdi,[rip+.Lempty]
.Lrecord_create_key:
 call json_object_new_string_len@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.La_key]
 call .Lcreate_attach
 test eax,eax
 jnz .Lrecord_create_cleanup
.Lrecord_create_scope:
 call json_object_new_object@PLT
 test rax,rax
 jz .Lrecord_create_cleanup
 mov [rsp+8],rax
 mov eax,[r12+40]
 lea rcx,[rip+.Lscopes]
 movsxd rdi,dword ptr [rcx+rax*4]
 add rdi,rcx
 call json_object_new_string@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.La_kind]
 call .Lcreate_attach
 test eax,eax
 jnz .Lrecord_create_cleanup
 cmp dword ptr [r12+40],0
 je .Lrecord_create_attach_scope
 mov rdi,[r12+32]
 call json_object_new_int64@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.La_conversation]
 cmp dword ptr [r12+40],1
 je .Lrecord_create_owner
 lea rsi,[rip+.La_task]
.Lrecord_create_owner:
 call .Lcreate_attach
 test eax,eax
 jnz .Lrecord_create_cleanup
.Lrecord_create_attach_scope:
 mov rdi,[rsp]
 lea rsi,[rip+.La_scope]
 mov rdx,[rsp+8]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lrecord_create_cleanup
 mov qword ptr [rsp+8],0
 cmp dword ptr [r12+40],1
 jne .Lrecord_create_success
 lea rdi,[rip+.La_latest]
 cmp dword ptr [rbx+28],1
 je .Lrecord_create_history
 lea rdi,[rip+.La_rewindable]
.Lrecord_create_history:
 call json_object_new_string@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.La_history]
 call .Lcreate_attach
 test eax,eax
 jnz .Lrecord_create_cleanup
 mov eax,[rbx+32]
 lea rcx,[rip+.Lcreate_forks]
 movsxd rdi,dword ptr [rcx+rax*4-4]
 add rdi,rcx
 call json_object_new_string@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.La_fork]
 call .Lcreate_attach
 test eax,eax
 jnz .Lrecord_create_cleanup
.Lrecord_create_success:
 mov rax,[rsp]
 mov [r14],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
.Lrecord_create_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
.Lrecord_create_return:
 mov eax,ebp
 add rsp,88
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_record_create,.-pg_document_record_create

.globl pg_document_initialize
.type pg_document_initialize,@function
pg_document_initialize:
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
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 test r14,r14
 jz .Linitialize_return
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Linitialize_return
 test r13,r13
 jz .Linitialize_return
 cmp qword ptr [r13+8],0
 je .Linitialize_return
 cmp dword ptr [rbx+36],1
 ja .Linitialize_return
 cmp qword ptr [rbx+16],0
 je .Linitialize_return
 mov rax,9007199254740991
 cmp [rbx+16],rax
 ja .Linitialize_return
 cmp dword ptr [rbx+36],0
 je .Linitialize_callback
 mov rdi,r12
 call pg_document_value_valid
 test eax,eax
 jz .Linitialize_return
 test r12,r12
 jz .Linitialize_callback
 mov rdi,r12
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Linitialize_allocation
.Linitialize_callback:
 mov rdi,[r13]
 mov rsi,[rsp]
 lea rdx,[rsp+8]
 call [r13+8]
 mov ebp,10
 test eax,eax
 jnz .Linitialize_cleanup
 mov rdi,[rsp+8]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Linitialize_cleanup
 mov rdi,[rsp+8]
 call pg_document_value_valid
 test eax,eax
 jz .Linitialize_cleanup
 mov rdi,[rsp+8]
 lea rsi,[rsp+16]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Linitialize_allocation
 mov rax,[rsp+16]
 mov [r14],rax
 mov qword ptr [rsp+16],0
 xor ebp,ebp
 jmp .Linitialize_cleanup
.Linitialize_allocation:
 mov ebp,3
.Linitialize_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
.Linitialize_return:
 mov eax,ebp
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_initialize,.-pg_document_initialize
.section .rodata
.Lcreate_forks: .long .La_current-.Lcreate_forks,.La_initial-.Lcreate_forks,.La_asof-.Lcreate_forks

.text
.globl pg_document_cache_reserve
.type pg_document_cache_reserve,@function
pg_document_cache_reserve:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rdx
 mov eax,1
 test r12,r12
 jz .Lreserve_return
 pxor xmm0,xmm0
 movups [r12],xmm0
 movups [r12+16],xmm0
 test rbx,rbx
 jz .Lreserve_return
 cmp qword ptr [rbx],0
 je .Lreserve_return
 cmp qword ptr [rbx+104],0
 jne .Lreserve_return
 mov eax,2
 mov r13,[rbx+32]
 add r13,rsi
 jc .Lreserve_return
 cmp r13,0x3fffffff
 ja .Lreserve_return
 mov dword ptr [rbx+104],1
 lea rsi,[r13*2+1]
 cmp esi,16
 mov eax,16
 cmovb esi,eax
 mov rdi,[rbx]
 call lh_table_resize@PLT
 test eax,eax
 jnz .Lreserve_allocation
 mov [r12],rbx
 mov rax,[rbx]
 mov [r12+8],rax
 mov [r12+16],r13
 mov rax,[rbx+32]
 mov [r12+24],rax
 xor eax,eax
 jmp .Lreserve_done
.Lreserve_allocation:
 mov eax,3
.Lreserve_done:
 mov dword ptr [rbx+104],0
.Lreserve_return:
 pop r13
 pop r12
 pop rbx
 ret
.size pg_document_cache_reserve,.-pg_document_cache_reserve

.globl pg_document_install_prepare
.type pg_document_install_prepare,@function
pg_document_install_prepare:
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
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 test r15,r15
 jz .Linstall_prepare_return
 cmp qword ptr [r15+24],0
 jne .Linstall_prepare_return
 cmp qword ptr [r15+8],0
 jne .Linstall_prepare_return
 test rbx,rbx
 jz .Linstall_prepare_return
 test r12,r12
 jz .Linstall_prepare_return
 test r13,r13
 jz .Linstall_prepare_return
 cmp qword ptr [rbx],0
 je .Linstall_prepare_return
 cmp qword ptr [rbx+104],0
 jne .Linstall_prepare_return
 mov dword ptr [rbx+104],1
 mov rdi,r13
 lea rsi,[rip+.Li_created]
 lea rdx,[rsp+8]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Linstall_prepare_cleanup
 mov rdi,r13
 lea rsi,[rip+.Li_retired]
 lea rdx,[rsp+8]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Linstall_prepare_cleanup
 mov edi,1
 mov esi,168
 call calloc@PLT
 test rax,rax
 jz .Linstall_prepare_allocation
 mov [rsp],rax
 mov rdi,r13
 lea rsi,[rax+72]
 call pg_document_record_decode
 mov ebp,eax
 test eax,eax
 jnz .Linstall_prepare_cleanup
 mov rax,[rsp]
 mov rdi,r12
 lea rsi,[rax+72]
 mov rdx,[r12+16]
 call pg_document_check
 mov ebp,eax
 test eax,eax
 jnz .Linstall_prepare_cleanup
 mov rax,[rsp]
 mov rdi,r12
 lea rsi,[rax+72]
 mov rdx,[rax+120]
 lea rcx,[rax+48]
 call pg_document_record_create
 mov ebp,eax
 test eax,eax
 jnz .Linstall_prepare_cleanup
 mov rax,[rsp]
 mov rdi,[rax+48]
 lea rsi,[rax+72]
 call pg_document_record_decode
 mov ebp,eax
 test eax,eax
 jnz .Linstall_prepare_cleanup
 mov rax,[rsp]
 mov rdi,r12
 lea rsi,[rax+72]
 mov rdx,[r12+16]
 mov rcx,r14
 lea r8,[rax+56]
 call pg_document_materialize
 mov ebp,eax
 test eax,eax
 jnz .Linstall_prepare_cleanup
 mov rax,[rsp]
 mov [rax+64],r12
 mov rdx,[r12+16]
 mov [rax+136],rdx
 mov [rax+144],rdx
 mov dword ptr [rax+164],1
 xor edi,edi
 call json_object_new_int64@PLT
 test rax,rax
 jz .Linstall_prepare_allocation
 mov [rsp+8],rax
 mov rdx,rax
 mov rax,[rsp]
 mov rdi,[rax+48]
 lea rsi,[rip+.Li_created]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linstall_prepare_stamp_fail
 mov rax,[rsp+8]
 mov [r15+16],rax
 mov rax,[rsp]
 mov [r15+8],rax
 mov [r15],rbx
 mov dword ptr [r15+24],1
 mov qword ptr [rsp],0
 xor ebp,ebp
 jmp .Linstall_prepare_cleanup
.Linstall_prepare_stamp_fail:
 mov rdi,[rsp+8]
 call json_object_put@PLT
.Linstall_prepare_allocation:
 mov ebp,3
.Linstall_prepare_cleanup:
 mov rdi,[rsp]
 test rdi,rdi
 jz .Linstall_prepare_unlock
 add rdi,48
 call pg_document_loaded_close
 mov rdi,[rsp]
 call free@PLT
.Linstall_prepare_unlock:
 mov dword ptr [rbx+104],0
.Linstall_prepare_return:
 mov eax,ebp
 add rsp,24
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_install_prepare,.-pg_document_install_prepare

.globl pg_document_install_apply
.type pg_document_install_apply,@function
pg_document_install_apply:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,24
 mov r12,rdi
 mov r13,rsi
 mov r14,rdx
 mov r15,rcx
 mov ebp,1
 test r12,r12
 jz .Linstall_apply_return
 test r13,r13
 jz .Linstall_apply_return
 cmp qword ptr [r12+24],1
 jne .Linstall_apply_return
 test r14,r14
 jz .Linstall_apply_return
 mov rax,9007199254740991
 cmp r14,rax
 ja .Linstall_apply_return
 mov rbx,[r12]
 test rbx,rbx
 jz .Linstall_apply_return
 cmp rbx,[r13]
 jne .Linstall_apply_return
 cmp qword ptr [rbx+104],0
 jne .Linstall_apply_return
 mov rax,[rbx]
 test rax,rax
 jz .Linstall_apply_return
 cmp rax,[r13+8]
 jne .Linstall_apply_return
 mov rax,[rbx+32]
 cmp rax,[r13+24]
 jne .Linstall_apply_return
 cmp rax,[r13+16]
 jae .Linstall_apply_return
 mov rax,[r12+8]
 test rax,rax
 jz .Linstall_apply_return
 mov [rsp],rax
 mov dword ptr [rbx+104],1
 mov dword ptr [r12+28],1
 mov rdi,[rbx]
 lea rsi,[rax+72]
 lea rdx,[rsp+8]
 call lh_table_lookup_ex@PLT
 test eax,eax
 jnz .Linstall_apply_duplicate
 mov rdi,[r12+16]
 mov rsi,r14
 call json_object_set_int64@PLT
 test eax,eax
 jz .Linstall_apply_invalid
 mov rax,[rsp]
 mov rdi,[rbx]
 lea rsi,[rax+72]
 mov rdx,rax
 call lh_table_insert@PLT
 test eax,eax
 jnz .Linstall_apply_allocation
 mov rdx,[rsp]
 mov rax,[rbx+8]
 mov [rdx+8],rax
 test rax,rax
 jz .Linstall_apply_head
 mov [rax],rdx
.Linstall_apply_head:
 mov [rbx+8],rdx
 mov [rdx+32],r15
 mov rax,[rbx+24]
 mov [rdx+16],rax
 test rax,rax
 jz .Linstall_apply_idle
 mov [rax+24],rdx
 jmp .Linstall_apply_tail
.Linstall_apply_idle:
 mov [rbx+16],rdx
.Linstall_apply_tail:
 mov [rbx+24],rdx
 inc qword ptr [rbx+32]
 inc qword ptr [r13+24]
 mov qword ptr [r12+8],0
 mov qword ptr [r12+16],0
 mov dword ptr [r12+24],2
 xor ebp,ebp
 jmp .Linstall_apply_unlock
.Linstall_apply_duplicate:
 mov ebp,4
 jmp .Linstall_apply_unlock
.Linstall_apply_invalid:
 mov ebp,1
 jmp .Linstall_apply_unlock
.Linstall_apply_allocation:
 mov ebp,3
.Linstall_apply_unlock:
 mov dword ptr [r12+28],0
 mov dword ptr [rbx+104],0
.Linstall_apply_return:
 mov eax,ebp
 add rsp,24
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_install_apply,.-pg_document_install_apply

.globl pg_document_install_discard
.type pg_document_install_discard,@function
pg_document_install_discard:
 mov eax,1
 test rdi,rdi
 jz .Linstall_discard_return
 cmp dword ptr [rdi+28],0
 jne .Linstall_discard_return
 push rbx
 mov rbx,rdi
 mov dword ptr [rbx+28],1
 mov rdi,[rbx+8]
 test rdi,rdi
 jz .Linstall_discard_clear
 add rdi,48
 call pg_document_loaded_close
 mov rdi,[rbx+8]
 call free@PLT
.Linstall_discard_clear:
 pxor xmm0,xmm0
 movups [rbx],xmm0
 movups [rbx+16],xmm0
 xor eax,eax
 pop rbx
.Linstall_discard_return:
 ret
.size pg_document_install_discard,.-pg_document_install_discard
.section .rodata
.Li_created: .asciz "createdAt"
.Li_retired: .asciz "retiredAt"

.text
.globl pg_document_cache_find_record
.type pg_document_cache_find_record,@function
pg_document_cache_find_record:
 push rbp
 push rbx
 push r12
 push r13
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov ebp,1
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 test r13,r13
 jz .Lcache_record_return
 mov qword ptr [r13],0
 test rbx,rbx
 jz .Lcache_record_return
 cmp qword ptr [rbx],0
 je .Lcache_record_return
 cmp qword ptr [rbx+104],0
 jne .Lcache_record_return
 mov dword ptr [rbx+104],1
 mov rdi,r12
 call pg_document_address_valid
 test eax,eax
 jz .Lcache_record_done
 mov rdi,[rbx]
 mov rsi,r12
 mov rdx,rsp
 call lh_table_lookup_ex@PLT
 test eax,eax
 jz .Lcache_record_storage
 mov rdi,rbx
 mov rsi,[rsp]
 call .Lcache_touch_node
 mov rax,[rsp]
 mov rdi,[rax+48]
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lcache_record_allocation
 jmp .Lcache_record_ok
.Lcache_record_storage:
 mov rdi,[rbx+40]
 mov rsi,r12
 mov rdx,-1
 lea rcx,[rsp+8]
 call [rbx+48]
 mov ebp,8
 cmp eax,1
 je .Lcache_record_done
 mov ebp,9
 test eax,eax
 jnz .Lcache_record_done
.Lcache_record_ok:
 cmp qword ptr [rsp+8],0
 je .Lcache_record_storage_bad
 mov rax,[rsp+8]
 mov [r13],rax
 mov qword ptr [rsp+8],0
 xor ebp,ebp
 jmp .Lcache_record_done
.Lcache_record_storage_bad:
 mov ebp,9
 jmp .Lcache_record_done
.Lcache_record_allocation:
 mov ebp,3
.Lcache_record_done:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov dword ptr [rbx+104],0
.Lcache_record_return:
 mov eax,ebp
 add rsp,24
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_document_cache_find_record,.-pg_document_cache_find_record

.globl pg_document_cache_evict_reserved
.type pg_document_cache_evict_reserved,@function
pg_document_cache_evict_reserved:
 push rbx
 push r12
 push r13
 sub rsp,16
 mov rbx,rdi
 mov r12,rdx
 mov r13,rcx
 mov eax,1
 test rbx,rbx
 jz .Levict_reserved_return
 test r13,r13
 jz .Levict_reserved_return
 cmp rbx,[r13]
 jne .Levict_reserved_return
 mov rax,[rbx]
 test rax,rax
 jz .Levict_reserved_invalid
 cmp rax,[r13+8]
 jne .Levict_reserved_invalid
 cmp qword ptr [rbx+104],0
 jne .Levict_reserved_invalid
 mov rax,[rbx+32]
 cmp rax,[r13+24]
 jne .Levict_reserved_invalid
 mov [rsp+8],rsi
 mov dword ptr [rbx+104],1
 mov rdi,rsi
 call pg_document_address_valid
 test eax,eax
 jz .Levict_reserved_bad_address
 mov rdi,[rbx]
 mov rsi,[rsp+8]
 mov rdx,rsp
 call lh_table_lookup_ex@PLT
 test eax,eax
 jz .Levict_reserved_ok
 mov rsi,[rsp]
 cmp [rsi+120],r12
 jne .Levict_reserved_ok
 /* Suppress empty-index shrinking while the prepared batch owns capacity. */
 mov dword ptr [rbx+108],1
 mov rdi,rbx
 call .Lcache_drop_node
 mov dword ptr [rbx+108],0
 dec qword ptr [r13+24]
.Levict_reserved_ok:
 xor eax,eax
 jmp .Levict_reserved_done
.Levict_reserved_bad_address:
 mov eax,1
.Levict_reserved_done:
 mov dword ptr [rbx+104],0
 jmp .Levict_reserved_return
.Levict_reserved_invalid:
 mov eax,1
.Levict_reserved_return:
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret
.size pg_document_cache_evict_reserved,.-pg_document_cache_evict_reserved
