/* Pi PKCE and OpenAI Codex browser login native adaptation.
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
.macro BGET object,key
 mov rdi,\object
 lea rsi,[rip+.LbK_\key]
 call json_object_object_get@PLT
.endm
.text
.globl pg_oauth_pkce
.type pg_oauth_pkce,@function
pg_oauth_pkce:
 push rbx
 push r12
 push r13
 sub rsp,208
 mov r12,rdi
 mov qword ptr [rdi],0
 xor ebx,ebx
 mov rdi,rsp
 mov esi,32
 call RAND_bytes@PLT
 cmp eax,1
 jne .Lpk_bad
 lea rdi,[rsp+32]
 mov rsi,rsp
 mov edx,32
 call EVP_EncodeBlock@PLT
 lea rdi,[rsp+32]
 call .Lpk_url
 lea rdi,[rsp+32]
 mov esi,43
 lea rdx,[rsp+96]
 call SHA256@PLT
 test rax,rax
 jz .Lpk_bad
 lea rdi,[rsp+128]
 lea rsi,[rsp+96]
 mov edx,32
 call EVP_EncodeBlock@PLT
 lea rdi,[rsp+128]
 call .Lpk_url
 mov rdi,rsp
 mov esi,16
 call RAND_bytes@PLT
 cmp eax,1
 jne .Lpk_bad
 xor ecx,ecx
 lea r8,[rip+.Lpk_hex]
.Lpk_state:
 movzx eax,byte ptr [rsp+rcx]
 mov edx,eax
 shr edx,4
 and eax,15
 mov dl,[r8+rdx]
 mov al,[r8+rax]
 mov [rsp+176+rcx*2],dl
 mov [rsp+177+rcx*2],al
 inc ecx
 cmp ecx,16
 jb .Lpk_state
 call json_object_new_object@PLT
 mov rbx,rax
 test rax,rax
 jz .Lpk_bad
 mov rdi,rbx
 lea rsi,[rip+.LbK_verifier]
 lea rdx,[rsp+32]
 call .Lb_add_string
 test eax,eax
 jnz .Lpk_bad
 mov rdi,rbx
 lea rsi,[rip+.LbK_challenge]
 lea rdx,[rsp+128]
 call .Lb_add_string
 test eax,eax
 jnz .Lpk_bad
 lea rdi,[rsp+176]
 mov esi,32
 call json_object_new_string_len@PLT
 mov rdi,rbx
 lea rsi,[rip+.LbK_state]
 mov rdx,rax
 call .Lb_add
 test eax,eax
 jnz .Lpk_bad
 mov [r12],rbx
 xor eax,eax
 jmp .Lpk_end
.Lpk_bad:
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,7
.Lpk_end:
 add rsp,208
 pop r13
 pop r12
 pop rbx
 ret
.size pg_oauth_pkce,.-pg_oauth_pkce
.Lpk_url:
 mov rax,rdi
.Lpk_url_each:
 mov dl,[rax]
 test dl,dl
 jz .Lpk_url_end
 cmp dl,61
 je .Lpk_url_zero
 cmp dl,43
 jne .Lpk_url_slash
 mov byte ptr [rax],45
.Lpk_url_slash:
 cmp dl,47
 jne .Lpk_url_next
 mov byte ptr [rax],95
.Lpk_url_next:
 inc rax
 jmp .Lpk_url_each
.Lpk_url_zero:
 mov byte ptr [rax],0
.Lpk_url_end:
 ret
.globl pg_oauth_authorization_parse
.type pg_oauth_authorization_parse,@function
pg_oauth_authorization_parse:
 push rbx
 push r12
 push r13
 sub rsp,48
 mov r12,rsi
 mov qword ptr [rsi],0
 xor eax,eax
 mov [rsp],rax
 mov [rsp+8],rax
 mov [rsp+16],rax
 mov [rsp+24],rax
 xor ebx,ebx
 test rdi,rdi
 jz .Lap_bad
 call strdup@PLT
 mov r13,rax
 mov [rsp+24],rax
 test rax,rax
 jz .Lap_bad
 mov rdi,rax
 call g_strchug@PLT
 mov rdi,rax
 call g_strchomp@PLT
 cmp byte ptr [r13],0
 je .Lap_build
 mov rdi,r13
 mov esi,1
 mov rdx,rsp
 call pg_oauth_query_fields@PLT
 test eax,eax
 jz .Lap_build
 cmp eax,1
 jne .Lap_bad
 mov rdi,r13
 mov esi,35
 call strchr@PLT
 test rax,rax
 jz .Lap_query
 mov byte ptr [rax],0
 lea rdi,[rax+1]
 call strdup@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lap_bad
 mov rdi,rax
 mov esi,35
 call strchr@PLT
 test rax,rax
 jz .Lap_raw
 mov byte ptr [rax],0
 jmp .Lap_raw
.Lap_query:
 mov rdi,r13
 lea rsi,[rip+.Lb_code_equal]
 call strstr@PLT
 test rax,rax
 jz .Lap_raw
 mov rdi,r13
 xor esi,esi
 mov rdx,rsp
 call pg_oauth_query_fields@PLT
 test eax,eax
 jnz .Lap_bad
 jmp .Lap_build
.Lap_raw:
 mov rdi,r13
 call strdup@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lap_bad
.Lap_build:
 call json_object_new_object@PLT
 mov rbx,rax
 test rax,rax
 jz .Lap_bad
 xor r13d,r13d
.Lap_fields:
 mov rdx,[rsp+r13*8]
 test rdx,rdx
 jz .Lap_next
 lea rax,[rip+.Lb_query_keys]
 mov rsi,[rax+r13*8]
 mov rdi,rbx
 call .Lb_add_string
 test eax,eax
 jnz .Lap_bad
.Lap_next:
 inc r13d
 cmp r13d,3
 jb .Lap_fields
 mov [r12],rbx
 xor ebx,ebx
 xor r13d,r13d
 jmp .Lap_end
.Lap_bad:
 mov r13d,7
.Lap_end:
 mov rdi,rbx
 call json_object_put@PLT
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov rdi,[rsp+16]
 call free@PLT
 mov rdi,[rsp+24]
 call free@PLT
 mov eax,r13d
 add rsp,48
 pop r13
 pop r12
 pop rbx
 ret
.size pg_oauth_authorization_parse,.-pg_oauth_authorization_parse
.globl pg_oauth_browser_configure
.type pg_oauth_browser_configure,@function
pg_oauth_browser_configure:
 mov [rdi+80],rsi
 ret
.size pg_oauth_browser_configure,.-pg_oauth_browser_configure
.globl pg_oauth_browser_begin
.type pg_oauth_browser_begin,@function
pg_oauth_browser_begin:
 push rbx
 push r12
 push r13
 sub rsp,32
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 test rsi,rsi
 jz .Lb_begin_bad
 mov rdi,rsi
 lea rsi,[rip+.Lb_identity]
 call strcmp@PLT
 test eax,eax
 jnz .Lb_begin_bad
 cmp dword ptr [rbx+40],10
 je .Lb_begin_busy
 mov eax,[rbx+40]
 dec eax
 cmp eax,3
 jbe .Lb_begin_busy
 mov rdi,rbx
 call pg_oauth_device_close@PLT
 cmp qword ptr [rbx+80],0
 jne .Lb_configured
 lea rax,[rip+.Lb_default_config]
 mov [rbx+80],rax
.Lb_configured:
 lea rdi,[rbx+72]
 call pg_oauth_pkce@PLT
 test eax,eax
 jnz .Lb_begin_failed
 mov rax,[rbx+80]
 mov rsi,[rax+16]
 xor edi,edi
 xor edx,edx
 call curl_easy_escape@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lb_begin_failed
 BGET "qword ptr [rbx+72]",challenge
 mov rdi,rax
 call json_object_get_string@PLT
 mov r12,rax
 BGET "qword ptr [rbx+72]",state
 mov rdi,rax
 call json_object_get_string@PLT
 mov r13,rax
 lea rdi,[rsp+8]
 lea rsi,[rip+.Lb_url_format]
 mov rax,[rbx+80]
 mov rdx,[rax]
 mov rcx,[rsp]
 mov r8,r12
 mov r9,r13
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lb_begin_failed
 mov rdi,[rbx+72]
 lea rsi,[rip+.LbK_url]
 mov rdx,[rsp+8]
 call .Lb_add_string
 test eax,eax
 jnz .Lb_begin_failed
 mov dword ptr [rbx+40],10
 mov rax,[rbx+80]
 mov r12,[rax+24]
 mov r13d,[rax+32]
 lea rdx,[rip+.Lb_default_config]
 cmp rax,rdx
 jne .Lb_callback_host_ready
 lea rdi,[rip+.Lb_callback_host_env]
 call getenv@PLT
 test rax,rax
 jz .Lb_callback_host_ready
 cmp byte ptr [rax],0
 je .Lb_callback_host_ready
 mov r12,rax
.Lb_callback_host_ready:
 mov rdi,r12
 mov esi,r13d
 mov rdx,rbx
 lea rcx,[rip+.Lb_callback]
 call pg_oauth_callback_open@PLT
 mov [rbx+88],rax
 xor r12d,r12d
 jmp .Lb_begin_end
.Lb_begin_busy:
 mov r12d,3
 jmp .Lb_begin_end
.Lb_begin_bad:
 mov r12d,5
 jmp .Lb_begin_end
.Lb_begin_failed:
 mov rdi,rbx
 call pg_oauth_device_close@PLT
 mov dword ptr [rbx+40],7
 mov r12d,7
.Lb_begin_end:
 mov rdi,[rsp]
 call curl_free@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov eax,r12d
 add rsp,32
 pop r13
 pop r12
 pop rbx
 ret
.size pg_oauth_browser_begin,.-pg_oauth_browser_begin
.globl pg_oauth_browser_submit
.type pg_oauth_browser_submit,@function
pg_oauth_browser_submit:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov rdi,rsi
 mov rsi,rsp
 mov qword ptr [rsp],0
 cmp dword ptr [rbx+40],10
 je .Lb_submit_parse
 mov r12d,3
 jmp .Lb_submit_end
.Lb_submit_parse:
 call pg_oauth_authorization_parse@PLT
 test eax,eax
 jnz .Lb_submit_failed
 mov rdi,rbx
 mov rsi,[rsp]
 xor edx,edx
 call .Lb_accept
 test eax,eax
 jnz .Lb_submit_failed
 mov rdi,[rbx+88]
 call pg_oauth_callback_close@PLT
 mov qword ptr [rbx+88],0
 xor r12d,r12d
 jmp .Lb_submit_end
.Lb_submit_failed:
 mov rdi,rbx
 call pg_oauth_device_close@PLT
 mov dword ptr [rbx+40],7
 mov r12d,5
.Lb_submit_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,r12d
 add rsp,8
 pop r12
 pop rbx
 ret
.size pg_oauth_browser_submit,.-pg_oauth_browser_submit
.Lb_callback:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov r12,rdx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 test esi,esi
 jz .Lb_cb_route
 mov rdi,rdx
 lea rsi,[rip+.Lb_callback_path]
 call pg_oauth_callback_path@PLT
 test eax,eax
 jz .Lb_cb_route
 cmp dword ptr [rbx+40],10
 jne .Lb_cb_claimed
 lea rdi,[rsp+8]
 lea rsi,[rip+.Lb_local_url]
 mov rdx,r12
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lb_cb_bad
 mov rdi,[rsp+8]
 mov rsi,rsp
 call pg_oauth_authorization_parse@PLT
 test eax,eax
 jnz .Lb_cb_bad
 BGET "qword ptr [rsp]",state
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lb_cb_bad
 mov [rsp+16],rax
 BGET "qword ptr [rbx+72]",state
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,[rsp+16]
 call strcmp@PLT
 test eax,eax
 jnz .Lb_cb_bad
 BGET "qword ptr [rsp]",error
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lb_cb_accept
 cmp byte ptr [rax],0
 jne .Lb_cb_error
.Lb_cb_accept:
 mov rdi,rbx
 mov rsi,[rsp]
 mov edx,1
 call .Lb_accept
 test eax,eax
 jnz .Lb_cb_bad
 mov r12d,200
 jmp .Lb_cb_end
.Lb_cb_error:
 mov dword ptr [rbx+40],7
.Lb_cb_bad:
 mov r12d,400
 jmp .Lb_cb_end
.Lb_cb_route:
 mov r12d,404
 jmp .Lb_cb_end
.Lb_cb_claimed:
 mov r12d,409
.Lb_cb_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov eax,r12d
 add rsp,24
 pop r12
 pop rbx
 ret
.Lb_accept:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov r14d,edx
 cmp dword ptr [rbx+40],10
 jne .Lb_accept_bad
 BGET r12,state
 mov rdi,rax
 call json_object_get_string@PLT
 mov r13,rax
 test rax,rax
 jz .Lb_state_missing
 cmp byte ptr [rax],0
 jne .Lb_state_compare
.Lb_state_missing:
 test r14d,r14d
 jnz .Lb_accept_bad
 jmp .Lb_code
.Lb_state_compare:
 BGET "qword ptr [rbx+72]",state
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,r13
 call strcmp@PLT
 test eax,eax
 jnz .Lb_accept_bad
.Lb_code:
 BGET r12,code
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lb_accept_bad
 cmp byte ptr [rax],0
 je .Lb_accept_bad
 mov r13,rax
 call json_object_new_object@PLT
 mov r14,rax
 test rax,rax
 jz .Lb_accept_bad
 mov rdi,rax
 lea rsi,[rip+.LbK_authorization_code]
 mov rdx,r13
 call .Lb_add_string
 test eax,eax
 jnz .Lb_accept_release
 BGET "qword ptr [rbx+72]",verifier
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,r14
 lea rsi,[rip+.LbK_code_verifier]
 mov rdx,rax
 call .Lb_add_string
 test eax,eax
 jnz .Lb_accept_release
 mov rdi,rbx
 mov rsi,r14
 mov rax,[rbx+80]
 mov rdx,[rax+16]
 mov rcx,[rax+8]
 call pg_oauth_login_exchange@PLT
 mov r13d,eax
 jmp .Lb_accept_put
.Lb_accept_release:
 mov r13d,7
.Lb_accept_put:
 mov rdi,r14
 call json_object_put@PLT
 mov eax,r13d
 jmp .Lb_accept_end
.Lb_accept_bad:
 mov eax,5
.Lb_accept_end:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.globl pg_oauth_browser_poll
.type pg_oauth_browser_poll,@function
pg_oauth_browser_poll:
 push rbx
 mov rbx,rdi
 mov rdi,[rbx+88]
 call pg_oauth_callback_pump@PLT
 cmp dword ptr [rbx+40],10
 je .Lb_poll_end
 mov rdi,[rbx+88]
 call pg_oauth_callback_close@PLT
 mov qword ptr [rbx+88],0
 cmp dword ptr [rbx+40],7
 jne .Lb_poll_end
 mov rdi,rbx
 call pg_oauth_device_close@PLT
 mov dword ptr [rbx+40],7
.Lb_poll_end:
 pop rbx
 ret
.size pg_oauth_browser_poll,.-pg_oauth_browser_poll
.Lb_add_string:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 call json_object_new_string@PLT
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call .Lb_add
 add rsp,8
 pop r12
 pop rbx
 ret
.Lb_add:
 push rbx
 mov rbx,rdx
 test rdx,rdx
 jz .Lb_add_bad
 call json_object_object_add@PLT
 test eax,eax
 jz .Lb_add_end
 mov rdi,rbx
 call json_object_put@PLT
.Lb_add_bad:
 mov eax,7
.Lb_add_end:
 pop rbx
 ret
.section .rodata
.Lpk_hex: .asciz "0123456789abcdef"
.Lb_identity: .asciz "openai-codex"
.Lb_code_equal: .asciz "code="
.Lb_callback_path: .asciz "/auth/callback"
.Lb_local_url: .asciz "http://localhost%s"
.Lb_authorize: .asciz "https://auth.openai.com/oauth/authorize"
.Lb_token: .asciz "https://auth.openai.com/oauth/token"
.Lb_redirect: .asciz "http://localhost:1455/auth/callback"
.Lb_host: .asciz "127.0.0.1"
.Lb_callback_host_env: .asciz "PANGU_OAUTH_CALLBACK_HOST"
.Lb_url_format: .asciz "%s?response_type=code&client_id=app_EMoamEEZ73f0CkXaXp7hrann&redirect_uri=%s&scope=openid+profile+email+offline_access&code_challenge=%s&code_challenge_method=S256&state=%s&id_token_add_organizations=true&codex_cli_simplified_flow=true&originator=pangu"
.LbK_code: .asciz "code"
.LbK_state: .asciz "state"
.LbK_error: .asciz "error"
.LbK_verifier: .asciz "verifier"
.LbK_challenge: .asciz "challenge"
.LbK_url: .asciz "url"
.LbK_authorization_code: .asciz "authorization_code"
.LbK_code_verifier: .asciz "code_verifier"
.section .data.rel.ro
.Lb_query_keys: .quad .LbK_code,.LbK_state,.LbK_error
.Lb_default_config: .quad .Lb_authorize,.Lb_token,.Lb_redirect,.Lb_host,1455
.section .note.GNU-stack,"",@progbits
