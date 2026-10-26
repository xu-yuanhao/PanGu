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
