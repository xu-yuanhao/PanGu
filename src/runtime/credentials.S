/* Pi auth/credential-store.ts and Coding Agent auth-storage.ts native adaptation.
MIT License

Copyright (c) 2025 Mario Zechner

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.*/
.intel_syntax noprefix
.text
.globl pg_credentials_file_init
.type pg_credentials_file_init,@function
pg_credentials_file_init:
 test rdi,rdi
 jz .Lci_bad
 test rsi,rsi
 jz .Lci_bad
 cmp byte ptr [rsi],0
 je .Lci_bad
 mov [rdi],rsi
 lea rax,[rip+.Lci_read]
 mov [rdi+8],rax
 lea rax,[rip+.Lci_list]
 mov [rdi+16],rax
 lea rax,[rip+.Lci_modify]
 mov [rdi+24],rax
 lea rax,[rip+.Lci_remove]
 mov [rdi+32],rax
 xor eax,eax
 ret
.Lci_bad:
 mov eax,5
 ret
.size pg_credentials_file_init,.-pg_credentials_file_init
.Lci_read:
 mov r8,rdx
 xor edx,edx
 xor ecx,ecx
 xor r9d,r9d
 jmp .Lci_call
.Lci_list:
 mov r8,rsi
 xor esi,esi
 xor edx,edx
 xor ecx,ecx
 mov r9d,1
 jmp .Lci_call
.Lci_modify:
 mov r9d,2
 jmp .Lci_call
.Lci_remove:
 xor edx,edx
 xor ecx,ecx
 xor r8d,r8d
 mov r9d,3
.Lci_call:
 push rbx
 push r12
 sub rsp,72
 mov r12,r8
 mov qword ptr [rsp+32],0
 mov [rsp],r9
 mov [rsp+8],rsi
 mov [rsp+16],rdx
 mov [rsp+24],rcx
 test r8,r8
 jz .Lci_output_ready
 mov qword ptr [r8],0
.Lci_output_ready:
 cmp r9d,1
 je .Lci_valid
 test rsi,rsi
 jz .Lci_invalid
 cmp byte ptr [rsi],0
 je .Lci_invalid
 cmp r9d,2
 jne .Lci_valid
 test rdx,rdx
 jz .Lci_invalid
.Lci_valid:
 xor esi,esi
 cmp r9d,2
 setae sil
 lea rdx,[rip+.Lci_visit]
 mov rcx,rsp
 call pg_credentials_file_access@PLT
 mov ebx,eax
 test eax,eax
 jnz .Lci_drop
 test r12,r12
 jz .Lci_drop
 mov rax,[rsp+32]
 mov [r12],rax
 mov qword ptr [rsp+32],0
.Lci_drop:
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov eax,ebx
 jmp .Lci_return
.Lci_invalid:
 mov eax,5
.Lci_return:
 add rsp,72
 pop r12
 pop rbx
 ret
.Lci_visit:
 push rbx
 push rbp
 push r12
 push r13
 push r14
 push r15
 sub rsp,56
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov [rsp+48],rcx
 xor eax,eax
 mov [rsp],rax
 mov [rsp+8],rax
 mov [rsp+16],rax
 mov [rsp+40],rax
 mov ebp,7
 cmp r13,2147483646
 ja .Lci_visit_end
 cmp r13,3
 jb .Lci_parse
 cmp byte ptr [r12],239
 jne .Lci_parse
 cmp byte ptr [r12+1],187
 jne .Lci_parse
 cmp byte ptr [r12+2],191
 jne .Lci_parse
 add r12,3
 sub r13,3
.Lci_parse:
 mov rdi,r12
 xor esi,esi
 mov rdx,r13
 call memchr@PLT
 test rax,rax
 jnz .Lci_bad_data
 call json_tokener_new@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lci_visit_end
 mov rdi,rax
 mov esi,17
 call json_tokener_set_flags@PLT
 mov rdi,[rsp+8]
 mov rsi,r12
 lea edx,[r13+1]
 call json_tokener_parse_ex@PLT
 mov [rsp],rax
 mov rdi,[rsp+8]
 call json_tokener_get_error@PLT
 test eax,eax
 jnz .Lci_bad_data
 mov rdi,[rsp]
 call json_object_get_type@PLT
 cmp eax,4
 jne .Lci_bad_data
 mov rdi,[rsp]
 call json_object_iter_begin@PLT
 mov [rsp+24],rax
 mov rdi,[rsp]
 call json_object_iter_end@PLT
 mov [rsp+32],rax
.Lci_validate_each:
 lea rdi,[rsp+24]
 lea rsi,[rsp+32]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lci_operation
 lea rdi,[rsp+24]
 call json_object_iter_peek_value@PLT
 mov rdi,rax
 call .Lci_credential_valid
 test eax,eax
 jnz .Lci_bad_data
 lea rdi,[rsp+24]
 call json_object_iter_next@PLT
 jmp .Lci_validate_each
.Lci_operation:
 cmp dword ptr [rbx],1
 je .Lci_listing
 mov rdi,[rsp]
 mov rsi,[rbx+8]
 call json_object_object_get@PLT
 mov r14,rax
 cmp dword ptr [rbx],2
 je .Lci_mutate
 cmp dword ptr [rbx],3
 je .Lci_delete
 jmp .Lci_result
.Lci_mutate:
 mov rdi,[rbx+24]
 mov rsi,r14
 lea rdx,[rsp+16]
 call qword ptr [rbx+16]
 mov ebp,eax
 test eax,eax
 jnz .Lci_visit_end
 cmp qword ptr [rsp+16],0
 je .Lci_result
 mov rdi,[rsp+16]
 call .Lci_credential_valid
 test eax,eax
 jnz .Lci_bad_data
 mov rdi,[rsp]
 mov rsi,[rbx+8]
 mov rdx,[rsp+16]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lci_allocation
 mov r14,[rsp+16]
 mov qword ptr [rsp+16],0
 mov rdi,r14
 lea rsi,[rbx+32]
 call .Lci_clone
 test eax,eax
 jnz .Lci_allocation
 jmp .Lci_serialize
.Lci_delete:
 test r14,r14
 jz .Lci_success
 mov rdi,[rsp]
 mov rsi,[rbx+8]
 call json_object_object_del@PLT
.Lci_serialize:
 mov rdi,[rsp]
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Lci_allocation
 mov rdi,rax
 call strdup@PLT
 test rax,rax
 jz .Lci_allocation
 mov rcx,[rsp+48]
 mov [rcx],rax
 jmp .Lci_success
.Lci_result:
 test r14,r14
 jz .Lci_success
 mov rdi,r14
 lea rsi,[rbx+32]
 call .Lci_clone
 test eax,eax
 jnz .Lci_allocation
 jmp .Lci_success
.Lci_listing:
 call json_object_new_array@PLT
 mov [rbx+32],rax
 test rax,rax
 jz .Lci_allocation
 mov rdi,[rsp]
 call json_object_iter_begin@PLT
 mov [rsp+24],rax
.Lci_list_each:
 lea rdi,[rsp+24]
 lea rsi,[rsp+32]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lci_success
 call json_object_new_object@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lci_allocation
 lea rdi,[rsp+24]
 call json_object_iter_peek_name@PLT
 mov rdi,rax
 call json_object_new_string@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lci_provider_id]
 mov rdx,rax
 call .Lci_add
 test eax,eax
 jnz .Lci_allocation
 lea rdi,[rsp+24]
 call json_object_iter_peek_value@PLT
 mov rdi,rax
 lea rsi,[rip+.Lci_type]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lci_type]
 mov rdx,rax
 call .Lci_add
 test eax,eax
 jnz .Lci_allocation
 mov rdi,[rbx+32]
 mov rsi,[rsp+40]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lci_allocation
 mov qword ptr [rsp+40],0
 lea rdi,[rsp+24]
 call json_object_iter_next@PLT
 jmp .Lci_list_each
.Lci_success:
 xor ebp,ebp
 jmp .Lci_visit_end
.Lci_bad_data:
 mov ebp,5
 jmp .Lci_visit_end
.Lci_allocation:
 mov ebp,7
.Lci_visit_end:
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_tokener_free@PLT
 mov eax,ebp
 add rsp,56
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbp
 pop rbx
 ret
.Lci_clone:
 sub rsp,8
 mov qword ptr [rsi],0
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 setne al
 movzx eax,al
 imul eax,eax,7
 add rsp,8
 ret
.Lci_add:
 push rbx
 mov rbx,rdx
 test rdx,rdx
 jz .Lci_add_bad
 call json_object_object_add@PLT
 test eax,eax
 jz .Lci_add_end
 mov rdi,rbx
 call json_object_put@PLT
.Lci_add_bad:
 mov eax,7
.Lci_add_end:
 pop rbx
 ret
/* Type-tagged credentials match Pi's public Credential union. */
.Lci_credential_valid:
 push rbx
 push r12
 push r13
 sub rsp,32
 mov rbx,rdi
 call json_object_get_type@PLT
 cmp eax,4
 jne .Lcv_bad
 mov rdi,rbx
 lea rsi,[rip+.Lci_type]
 call json_object_object_get@PLT
 mov rdi,rax
 call .Lcv_string
 test rax,rax
 jz .Lcv_bad
 mov r12,rax
 mov rdi,rax
 lea rsi,[rip+.Lci_api_key]
 call strcmp@PLT
 test eax,eax
 jnz .Lcv_oauth
 mov rdi,rbx
 lea rsi,[rip+.Lci_key]
 lea rdx,[rsp]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lcv_env
 mov rdi,[rsp]
 call .Lcv_string
 test rax,rax
 jz .Lcv_bad
.Lcv_env:
 mov rdi,rbx
 lea rsi,[rip+.Lci_env]
 lea rdx,[rsp]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lcv_ok
 mov rdi,[rsp]
 call json_object_get_type@PLT
 cmp eax,4
 jne .Lcv_bad
 mov rdi,[rsp]
 call json_object_iter_begin@PLT
 mov [rsp+8],rax
 mov rdi,[rsp]
 call json_object_iter_end@PLT
 mov [rsp+16],rax
.Lcv_env_each:
 lea rdi,[rsp+8]
 lea rsi,[rsp+16]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lcv_ok
 lea rdi,[rsp+8]
 call json_object_iter_peek_value@PLT
 mov rdi,rax
 call .Lcv_string
 test rax,rax
 jz .Lcv_bad
 lea rdi,[rsp+8]
 call json_object_iter_next@PLT
 jmp .Lcv_env_each
.Lcv_oauth:
 mov rdi,r12
 lea rsi,[rip+.Lci_oauth]
 call strcmp@PLT
 test eax,eax
 jnz .Lcv_bad
 mov rdi,rbx
 lea rsi,[rip+.Lci_access]
 call json_object_object_get@PLT
 mov rdi,rax
 call .Lcv_string
 test rax,rax
 jz .Lcv_bad
 mov rdi,rbx
 lea rsi,[rip+.Lci_refresh]
 call json_object_object_get@PLT
 mov rdi,rax
 call .Lcv_string
 test rax,rax
 jz .Lcv_bad
 mov rdi,rbx
 lea rsi,[rip+.Lci_expires]
 call json_object_object_get@PLT
 mov r13,rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,3
 je .Lcv_number
 cmp eax,2
 jne .Lcv_bad
.Lcv_number:
 mov rdi,r13
 call json_object_get_double@PLT
 andpd xmm0,[rip+.Lcv_absolute]
 ucomisd xmm0,[rip+.Lcv_maximum]
 ja .Lcv_bad
 jp .Lcv_bad
.Lcv_ok:
 xor eax,eax
 jmp .Lcv_end
.Lcv_bad:
 mov eax,5
.Lcv_end:
 add rsp,32
 pop r13
 pop r12
 pop rbx
 ret
.Lcv_string:
 push rbx
 mov rbx,rdi
 call json_object_get_type@PLT
 cmp eax,6
 jne .Lcv_no_string
 mov rdi,rbx
 call json_object_get_string@PLT
 pop rbx
 ret
.Lcv_no_string:
 xor eax,eax
 pop rbx
 ret
.section .rodata
.Lci_provider_id: .asciz "providerId"
.Lci_type: .asciz "type"
.Lci_api_key: .asciz "api_key"
.Lci_oauth: .asciz "oauth"
.Lci_key: .asciz "key"
.Lci_env: .asciz "env"
.Lci_access: .asciz "access"
.Lci_refresh: .asciz "refresh"
.Lci_expires: .asciz "expires"
.balign 16
.Lcv_absolute: .quad 0x7fffffffffffffff,0xffffffffffffffff
.Lcv_maximum: .quad 0x7fefffffffffffff
.section .note.GNU-stack,"",@progbits
