/* Pi auth/resolve.ts and auth/oauth/openai-codex.ts native adaptation.
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
.globl pg_oauth_resolve
.type pg_oauth_resolve,@function
pg_oauth_resolve:
 push rbx
 push rbp
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov rbp,rsi
 mov r12,rdx
 mov r13,rcx
 mov r14,r8
 test r8,r8
 jz .Lor_invalid
 mov qword ptr [r8],0
 test rbx,rbx
 jz .Lor_invalid
 test rbp,rbp
 jz .Lor_invalid
 test r12,r12
 jz .Lor_invalid
 cmp qword ptr [r12+16],0
 je .Lor_invalid
 cmp qword ptr [r12+24],0
 je .Lor_invalid
 mov rdi,rbp
 mov rsi,[r12+8]
 test rsi,rsi
 jz .Lor_invalid
 call strcmp@PLT
 test eax,eax
 jnz .Lor_invalid
 mov rdi,r13
 mov rsi,r12
 call .Lor_needs_refresh
 cmp eax,1
 je .Lor_refresh
 test eax,eax
 jnz .Lor_invalid
 mov rdi,r13
 call json_object_get@PLT
 mov [r14],rax
 xor eax,eax
 jmp .Lor_end
.Lor_refresh:
 cmp qword ptr [rbx+24],0
 je .Lor_invalid
 mov rdi,[rbx]
 mov rsi,rbp
 lea rdx,[rip+.Lor_update]
 mov rcx,r12
 mov r8,r14
 call qword ptr [rbx+24]
 test eax,eax
 jz .Lor_post_refresh
 mov ebp,eax
 mov rdi,[r14]
 mov qword ptr [r14],0
 call json_object_put@PLT
 mov eax,ebp
 jmp .Lor_end
.Lor_post_refresh:
 mov rdi,[r14]
 call .Lor_is_oauth
 test eax,eax
 jnz .Lor_success
 mov rdi,[r14]
 call json_object_put@PLT
 mov qword ptr [r14],0
 jmp .Lor_invalid
.Lor_success:
 xor eax,eax
 jmp .Lor_end
.Lor_invalid:
 mov eax,5
.Lor_end:
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbp
 pop rbx
 ret
.size pg_oauth_resolve,.-pg_oauth_resolve
.Lor_update:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rdx],0
 mov rdi,rsi
 call .Lor_is_oauth
 test eax,eax
 jz .Lor_update_ok
 mov rdi,r12
 mov rsi,rbx
 call .Lor_needs_refresh
 cmp eax,1
 jne .Lor_update_result
 mov rdi,[rbx]
 mov rsi,r12
 mov rdx,r13
 call qword ptr [rbx+24]
 jmp .Lor_update_result
.Lor_update_ok:
 xor eax,eax
.Lor_update_result:
 pop r13
 pop r12
 pop rbx
 ret
.Lor_is_oauth:
 sub rsp,8
 lea rsi,[rip+.Lo_type]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lor_not_oauth
 mov rdi,rax
 lea rsi,[rip+.Lo_oauth]
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 add rsp,8
 ret
.Lor_not_oauth:
 xor eax,eax
 add rsp,8
 ret
/* -1 invalid clock/credential, 0 fresh, 1 refresh within five minutes. */
.Lor_needs_refresh:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 call .Lor_is_oauth
 test eax,eax
 jz .Lor_refresh_invalid
 mov rdi,[r12]
 call qword ptr [r12+16]
 ucomisd xmm0,[rip+.Lo_zero]
 jb .Lor_refresh_invalid
 jp .Lor_refresh_invalid
 addsd xmm0,[rip+.Lo_window]
 movsd [rsp],xmm0
 mov rdi,rbx
 lea rsi,[rip+.Lo_expires]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lor_refresh_invalid
 mov rdi,rax
 call json_object_get_double@PLT
 ucomisd xmm0,[rsp]
 jp .Lor_refresh_invalid
 setbe al
 movzx eax,al
 jmp .Lor_refresh_end
.Lor_refresh_invalid:
 mov eax,-1
.Lor_refresh_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.globl pg_oauth_codex_init
.type pg_oauth_codex_init,@function
pg_oauth_codex_init:
 lea rax,[rip+.Lo_endpoint]
 mov [rdi],rax
 lea rax,[rip+.Lo_identity]
 mov [rdi+8],rax
 mov rax,qword ptr [rip+pg_oauth_realtime@GOTPCREL]
 mov [rdi+16],rax
 mov rax,qword ptr [rip+pg_oauth_codex_refresh@GOTPCREL]
 mov [rdi+24],rax
 ret
.size pg_oauth_codex_init,.-pg_oauth_codex_init
.globl pg_oauth_codex_refresh
.type pg_oauth_codex_refresh,@function
pg_oauth_codex_refresh:
 push rbx
 push rbp
 push r12
 push r13
 push r14
 push r15
 sub rsp,40
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rdx],0
 xor eax,eax
 mov ecx,5
 mov rdi,rsp
 rep stosq
 mov ebp,7
 mov rdi,r12
 lea rsi,[rip+.Lo_refresh]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Loc_refresh_end
 mov rsi,rax
 xor edi,edi
 xor edx,edx
 call curl_easy_escape@PLT
 mov [rsp],rax
 test rax,rax
 jz .Loc_refresh_end
 lea rdi,[rsp+8]
 lea rsi,[rip+.Lo_form]
 mov rdx,rax
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Loc_refresh_end
 mov rdi,rbx
 mov rsi,[rsp+8]
 lea rdx,[rsp+16]
 call pg_oauth_post_form@PLT
 test eax,eax
 jnz .Loc_refresh_end
 call json_tokener_new@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Loc_refresh_end
 mov rdi,rax
 mov esi,17
 call json_tokener_set_flags@PLT
 mov rdi,[rsp+16]
 call strlen@PLT
 cmp rax,2147483646
 ja .Loc_refresh_end
 lea edx,[rax+1]
 mov rsi,[rsp+16]
 mov rdi,[rsp+24]
 call json_tokener_parse_ex@PLT
 mov [rsp+32],rax
 mov rdi,[rsp+24]
 call json_tokener_get_error@PLT
 test eax,eax
 jnz .Loc_refresh_end
 xor edi,edi
 call pg_oauth_realtime@PLT
 mov rdi,[rsp+32]
 mov rsi,r13
 call pg_oauth_codex_token@PLT
 mov ebp,eax
.Loc_refresh_end:
 mov rdi,[rsp]
 call curl_free@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov rdi,[rsp+16]
 call free@PLT
 mov rdi,[rsp+24]
 call json_tokener_free@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,40
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbp
 pop rbx
 ret
.size pg_oauth_codex_refresh,.-pg_oauth_codex_refresh
.Loc_token_string:
 push rbx
 push r12
 sub rsp,8
 call json_object_object_get@PLT
 mov rbx,rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,6
 jne .Loc_token_bad
 mov rdi,rbx
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Loc_token_bad
 mov [rsp],rax
 mov rdi,rbx
 call json_object_get_string@PLT
 mov r12,rax
 mov rdi,rax
 call strlen@PLT
 cmp rax,[rsp]
 jne .Loc_token_bad
 mov rax,r12
 jmp .Loc_token_end
.Loc_token_bad:
 xor eax,eax
.Loc_token_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.Lo_add:
 push rbx
 mov rbx,rdx
 test rdx,rdx
 jz .Lo_add_bad
 call json_object_object_add@PLT
 test eax,eax
 jz .Lo_add_end
 mov rdi,rbx
 call json_object_put@PLT
.Lo_add_bad:
 mov eax,7
.Lo_add_end:
 pop rbx
 ret
/* JWT claims are decoded for provider account routing, not signature trust. */
.globl pg_oauth_codex_account
.type pg_oauth_codex_account,@function
pg_oauth_codex_account:
 push rbx
 push r12
 push r13
 sub rsp,48
 mov rbx,rdi
 mov r12,rsi
 xor eax,eax
 mov [rsi],rax
 mov [rsp],rax
 mov [rsp+8],rax
 mov [rsp+16],rax
 mov [rsp+24],rax
 mov [rsp+32],rax
 mov [rsp+40],rax
 test rdi,rdi
 jz .Loc_account_bad
 mov esi,46
 call strchr@PLT
 test rax,rax
 jz .Loc_account_bad
 lea r13,[rax+1]
 mov rdi,r13
 mov esi,46
 call strchr@PLT
 test rax,rax
 jz .Loc_account_bad
 mov rbx,rax
 lea rdi,[rax+1]
 mov esi,46
 call strchr@PLT
 test rax,rax
 jnz .Loc_account_bad
 mov rsi,rbx
 sub rsi,r13
 mov rdi,r13
 call strndup@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Loc_account_bad
 mov rcx,rax
.Loc_url_byte:
 mov al,[rcx]
 test al,al
 jz .Loc_url_ready
 cmp al,45
 jne .Loc_url_under
 mov byte ptr [rcx],43
.Loc_url_under:
 cmp al,95
 jne .Loc_url_next
 mov byte ptr [rcx],47
.Loc_url_next:
 inc rcx
 jmp .Loc_url_byte
.Loc_url_ready:
 mov rdi,[rsp+24]
 call json_object_new_string@PLT
 mov [rsp],rax
 test rax,rax
 jz .Loc_account_bad
 mov rdi,rax
 lea rsi,[rsp+8]
 lea rdx,[rsp+16]
 call pg_mimo_base64_decode@PLT
 test eax,eax
 jnz .Loc_account_bad
 mov rdi,[rsp+8]
 xor esi,esi
 mov rdx,[rsp+16]
 call memchr@PLT
 test rax,rax
 jnz .Loc_account_bad
 mov rdi,[rsp+24]
 call free@PLT
 mov qword ptr [rsp+24],0
 mov rdi,[rsp+8]
 mov rsi,[rsp+16]
 call strndup@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Loc_account_bad
 call json_tokener_new@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Loc_account_bad
 mov rdi,rax
 mov esi,17
 call json_tokener_set_flags@PLT
 mov rdx,[rsp+16]
 cmp rdx,2147483646
 ja .Loc_account_bad
 inc edx
 mov rdi,[rsp+32]
 mov rsi,[rsp+24]
 call json_tokener_parse_ex@PLT
 mov [rsp+40],rax
 mov rdi,[rsp+32]
 call json_tokener_get_error@PLT
 test eax,eax
 jnz .Loc_account_bad
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lo_claim]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lo_account]
 call .Loc_token_string
 test rax,rax
 jz .Loc_account_bad
 mov rdi,rax
 call json_object_new_string@PLT
 mov [r12],rax
.Loc_account_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov rdi,[rsp+24]
 call free@PLT
 mov rdi,[rsp+32]
 call json_tokener_free@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 cmp qword ptr [r12],0
 setz al
 movzx eax,al
 add rsp,48
 pop r13
 pop r12
 pop rbx
 ret
.size pg_oauth_codex_account,.-pg_oauth_codex_account
.section .rodata
.Lo_type: .asciz "type"
.Lo_oauth: .asciz "oauth"
.Lo_expires: .asciz "expires"
.Lo_refresh: .asciz "refresh"
.Lo_access: .asciz "access"
.Lo_identity: .asciz "openai-codex"
.Lo_endpoint: .asciz "https://auth.openai.com/oauth/token"
.Lo_form: .asciz "grant_type=refresh_token&refresh_token=%s&client_id=app_EMoamEEZ73f0CkXaXp7hrann"
.Lo_access_token: .asciz "access_token"
.Lo_refresh_token: .asciz "refresh_token"
.Lo_expires_in: .asciz "expires_in"
.Lo_template: .asciz "{\"type\":\"oauth\"}"
.Lo_claim: .asciz "https://api.openai.com/auth"
.Lo_account: .asciz "chatgpt_account_id"
.Lo_account_id: .asciz "accountId"
.balign 16
.Lo_absolute: .quad 0x7fffffffffffffff,0xffffffffffffffff
.Lo_maximum: .quad 0x7fefffffffffffff
.Lo_zero: .double 0
.Lo_window: .double 300000
.Lo_thousand: .double 1000
.section .note.GNU-stack,"",@progbits

.text
.globl pg_oauth_codex_token
.type pg_oauth_codex_token,@function
pg_oauth_codex_token:
 push rbx
 push rbp
 push r12
 push r13
 push r14
 push r15
 sub rsp,88
 mov rbx,rdi
 mov r13,rsi
 mov qword ptr [rsi],0
 mov qword ptr [rsp+40],0
 mov qword ptr [rsp+56],0
 mov [rsp+32],rdi
 movsd [rsp+80],xmm0
 mov ebp,7
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lo_access_token]
 call .Loc_token_string
 mov r14,rax
 test rax,rax
 jz .Lot_end
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lo_refresh_token]
 call .Loc_token_string
 mov r15,rax
 test rax,rax
 jz .Lot_end
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lo_expires_in]
 call json_object_object_get@PLT
 mov [rsp+72],rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,2
 je .Loc_expiry_number
 cmp eax,3
 jne .Lot_end
.Loc_expiry_number:
 mov rdi,[rsp+72]
 call json_object_get_double@PLT
 mulsd xmm0,[rip+.Lo_thousand]
 addsd xmm0,[rsp+80]
 movapd xmm1,xmm0
 andpd xmm1,[rip+.Lo_absolute]
 ucomisd xmm1,[rip+.Lo_maximum]
 ja .Lot_end
 jp .Lot_end
 movsd [rsp+80],xmm0
 mov rdi,r14
 lea rsi,[rsp+56]
 call pg_oauth_codex_account
 test eax,eax
 jnz .Lot_end
 lea rdi,[rip+.Lo_template]
 call json_tokener_parse@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lot_end
 mov rdi,r14
 call json_object_new_string@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lo_access]
 mov rdx,rax
 call .Lo_add
 test eax,eax
 jnz .Lot_end
 mov rdi,r15
 call json_object_new_string@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lo_refresh]
 mov rdx,rax
 call .Lo_add
 test eax,eax
 jnz .Lot_end
 movsd xmm0,[rsp+80]
 call json_object_new_double@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lo_expires]
 mov rdx,rax
 call .Lo_add
 test eax,eax
 jnz .Lot_end
 mov rdi,[rsp+56]
 call json_object_get@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lo_account_id]
 mov rdx,rax
 call .Lo_add
 test eax,eax
 jnz .Lot_end
 mov rax,[rsp+40]
 mov [r13],rax
 mov qword ptr [rsp+40],0
 xor ebp,ebp
.Lot_end:
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,88
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbp
 pop rbx
 ret
.size pg_oauth_codex_token,.-pg_oauth_codex_token
.section .note.GNU-stack,"",@progbits
