/* Shared native Base64 adapter for model data and OAuth JWT claims. */
.intel_syntax noprefix
.text
/* Decode atob-compatible JSON Base64 into caller-owned bytes. 1 invalid, 2 allocation failure. */
.globl pg_mimo_base64_decode
.type pg_mimo_base64_decode,@function
pg_mimo_base64_decode:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r12],0
 mov qword ptr [r13],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lbb_invalid
 mov rdi,rbx
 call json_object_get_string_len@PLT
 cmp eax,2147483643
 ja .Lbb_invalid
 mov [rsp+16],rax
 lea rdi,[rax+4]
 call malloc@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbb_native
 mov rdi,rbx
 call json_object_get_string@PLT
 mov rbx,rax
 xor r14d,r14d
 xor ebp,ebp
 xor ecx,ecx
.Lbb_clean:
 cmp rcx,[rsp+16]
 jae .Lbb_padding
 movzx eax,byte ptr [rbx+rcx]
 inc rcx
 cmp eax,32
 je .Lbb_clean
 cmp eax,9
 je .Lbb_clean
 cmp eax,10
 je .Lbb_clean
 cmp eax,12
 je .Lbb_clean
 cmp eax,13
 je .Lbb_clean
 cmp eax,61
 je .Lbb_equal
 test ebp,ebp
 jnz .Lbb_invalid
 cmp eax,65
 jb .Lbb_lower
 cmp eax,90
 jbe .Lbb_store
.Lbb_lower:
 cmp eax,97
 jb .Lbb_digit
 cmp eax,122
 jbe .Lbb_store
.Lbb_digit:
 cmp eax,48
 jb .Lbb_symbol
 cmp eax,57
 jbe .Lbb_store
.Lbb_symbol:
 cmp eax,43
 je .Lbb_store
 cmp eax,47
 jne .Lbb_invalid
 jmp .Lbb_store
.Lbb_equal:
 inc ebp
 cmp ebp,2
 ja .Lbb_invalid
.Lbb_store:
 mov rdx,[rsp]
 mov [rdx+r14],al
 inc r14
 jmp .Lbb_clean
.Lbb_padding:
 mov eax,r14d
 and eax,3
 test ebp,ebp
 jz .Lbb_unpadded
 test eax,eax
 jnz .Lbb_invalid
 jmp .Lbb_decode
.Lbb_unpadded:
 cmp eax,1
 je .Lbb_invalid
 test eax,eax
 jz .Lbb_decode
 mov rdx,[rsp]
.Lbb_pad:
 mov byte ptr [rdx+r14],61
 inc r14
 inc ebp
 test r14d,3
 jnz .Lbb_pad
.Lbb_decode:
 mov rax,[rsp]
 mov byte ptr [rax+r14],0
 lea rdi,[r14+1]
 call malloc@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lbb_native
 mov rdi,rax
 mov rsi,[rsp]
 mov edx,r14d
 call EVP_DecodeBlock@PLT
 test eax,eax
 js .Lbb_invalid
 sub eax,ebp
 js .Lbb_invalid
 mov [r13],rax
 mov rax,[rsp+8]
 mov [r12],rax
 mov qword ptr [rsp+8],0
 xor ebp,ebp
 jmp .Lbb_end
.Lbb_invalid:
 mov ebp,1
 jmp .Lbb_end
.Lbb_native:
 mov ebp,2
.Lbb_end:
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov eax,ebp
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_mimo_base64_decode,.-pg_mimo_base64_decode
.globl pg_mimo_base64_encode
.type pg_mimo_base64_encode,@function
pg_mimo_base64_encode:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 cmp rsi,1610612733
 ja .Lbb_encode_bad
 lea rax,[rsi+2]
 xor edx,edx
 mov ecx,3
 div rcx
 lea rdi,[rax*4+1]
 call malloc@PLT
 mov r13,rax
 test rax,rax
 jz .Lbb_encode_bad
 mov rdi,rax
 mov rsi,rbx
 mov edx,r12d
 call EVP_EncodeBlock@PLT
 test eax,eax
 js .Lbb_encode_release
 mov rdi,r13
 mov esi,eax
 call json_object_new_string_len@PLT
 mov rbx,rax
 jmp .Lbb_encode_free
.Lbb_encode_release:
 xor ebx,ebx
.Lbb_encode_free:
 mov rdi,r13
 call free@PLT
 mov rax,rbx
 jmp .Lbb_encode_end
.Lbb_encode_bad:
 xor eax,eax
.Lbb_encode_end:
 pop r13
 pop r12
 pop rbx
 ret
.size pg_mimo_base64_encode,.-pg_mimo_base64_encode
.section .note.GNU-stack,"",@progbits
