/* Pi auth/oauth/device-code.ts and openai-codex.ts native device login adaptation.
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
.macro DGET object,key
 mov rdi,\object
 lea rsi,[rip+.LdK_\key]
 call json_object_object_get@PLT
.endm
.text
.globl pg_oauth_device_init
.type pg_oauth_device_init,@function
pg_oauth_device_init:
 mov r8,rdi
 xor eax,eax
 mov ecx,9
 rep stosq
 mov [r8],rsi
 test rdx,rdx
 jnz .Ld_config_ready
 lea rdx,[rip+.Ld_default_config]
.Ld_config_ready:
 mov [r8+8],rdx
 ret
.size pg_oauth_device_init,.-pg_oauth_device_init
.globl pg_oauth_device_begin
.type pg_oauth_device_begin,@function
pg_oauth_device_begin:
 push rbx
 mov rbx,rdi
 test rsi,rsi
 jz .Ld_begin_bad
 mov rdi,rsi
 lea rsi,[rip+.Ld_identity]
 call strcmp@PLT
 test eax,eax
 jnz .Ld_begin_bad
 mov eax,[rbx+40]
 dec eax
 cmp eax,3
 jbe .Ld_begin_busy
 mov rdi,rbx
 call pg_oauth_device_close
 mov dword ptr [rbx+40],1
 mov rax,[rbx+8]
 mov rsi,[rax]
 lea rdi,[rbx+16]
 lea rdx,[rip+.Ld_json_type]
 lea rcx,[rip+.Ld_start_body]
 call pg_oauth_http_start@PLT
 test eax,eax
 jz .Ld_begin_end
 mov dword ptr [rbx+40],7
 jmp .Ld_begin_end
.Ld_begin_bad:
 mov eax,5
 jmp .Ld_begin_end
.Ld_begin_busy:
 mov eax,3
.Ld_begin_end:
 pop rbx
 ret
.size pg_oauth_device_begin,.-pg_oauth_device_begin
.globl pg_oauth_device_close
.type pg_oauth_device_close,@function
pg_oauth_device_close:
 push rbx
 mov rbx,rdi
 mov rdi,[rbx+16]
 call pg_oauth_http_close@PLT
 mov qword ptr [rbx+16],0
 mov rdi,[rbx+24]
 call json_object_put@PLT
 mov qword ptr [rbx+24],0
 mov rdi,[rbx+32]
 call json_object_put@PLT
 mov qword ptr [rbx+32],0
 mov qword ptr [rbx+40],0
 mov qword ptr [rbx+48],0
 mov qword ptr [rbx+56],0
 mov qword ptr [rbx+64],0
 pop rbx
 ret
.size pg_oauth_device_close,.-pg_oauth_device_close
.globl pg_oauth_device_cancel
.type pg_oauth_device_cancel,@function
pg_oauth_device_cancel:
 mov eax,[rdi+40]
 dec eax
 cmp eax,3
 ja .Ld_cancel_done
 push rbx
 mov rbx,rdi
 call pg_oauth_device_close
 mov dword ptr [rbx+40],8
 pop rbx
.Ld_cancel_done:
 ret
.size pg_oauth_device_cancel,.-pg_oauth_device_cancel
.globl pg_oauth_device_poll
.type pg_oauth_device_poll,@function
pg_oauth_device_poll:
 push rbx
 push rbp
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov r12,rsi
 xor eax,eax
 mov ecx,8
 mov rdi,rsp
 rep stosq
 mov qword ptr [r12],0
 mov ebp,[rbx+40]
 cmp ebp,2
 je .Ld_waiting
 cmp ebp,1
 je .Ld_http
 cmp ebp,3
 je .Ld_http
 cmp ebp,4
 jne .Ld_snapshot
.Ld_http:
 mov rdi,[rbx+16]
 lea rsi,[rsp+8]
 lea rdx,[rsp+16]
 call pg_oauth_http_step@PLT
 cmp eax,1
 je .Ld_snapshot
 test eax,eax
 jnz .Ld_failed
 cmp ebp,3
 jne .Ld_http_parse
 cmp qword ptr [rsp+8],403
 je .Ld_pending
 cmp qword ptr [rsp+8],404
 je .Ld_pending
.Ld_http_parse:
 mov rdi,[rsp+16]
 call .Ld_parse
 mov [rsp],rax
 test rax,rax
 jz .Ld_failed
 mov rdi,[rbx+16]
 call pg_oauth_http_close@PLT
 mov qword ptr [rbx+16],0
 cmp qword ptr [rsp+8],200
 jb .Ld_http_error
 cmp qword ptr [rsp+8],300
 jae .Ld_http_error
 cmp ebp,1
 je .Ld_started
 cmp ebp,3
 je .Ld_exchange
 jmp .Ld_complete
.Ld_http_error:
 cmp ebp,3
 jne .Ld_failed
 DGET "qword ptr [rsp]",error
 mov r13,rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,4
 jne .Ld_error_string
 DGET r13,code
 mov r13,rax
.Ld_error_string:
 mov rdi,r13
 call .Ld_string
 test rax,rax
 jz .Ld_failed
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Ld_pending_error]
 call strcmp@PLT
 test eax,eax
 jz .Ld_pending
 mov rdi,r13
 lea rsi,[rip+.Ld_slow_error]
 call strcmp@PLT
 test eax,eax
 jnz .Ld_failed
 add qword ptr [rbx+48],5000
 mov dword ptr [rbx+44],1
 jmp .Ld_pending
.Ld_started:
 DGET "qword ptr [rsp]",device_auth_id
 mov rdi,rax
 call .Ld_string
 test rax,rax
 jz .Ld_failed
 DGET "qword ptr [rsp]",user_code
 mov rdi,rax
 call .Ld_string
 test rax,rax
 jz .Ld_failed
 DGET "qword ptr [rsp]",interval
 mov rdi,rax
 call .Ld_interval
 test rax,rax
 jz .Ld_failed
 mov [rbx+48],rax
 mov rdi,rbx
 call .Ld_now
 test rax,rax
 js .Ld_failed
 mov [rbx+64],rax
 add rax,900000
 mov [rbx+56],rax
 mov rax,[rsp]
 mov [rbx+24],rax
 mov qword ptr [rsp],0
 mov dword ptr [rbx+40],2
 jmp .Ld_snapshot
.Ld_waiting:
 mov rdi,rbx
 call .Ld_now
 test rax,rax
 js .Ld_failed
 cmp rax,[rbx+56]
 jae .Ld_timed_out
 cmp rax,[rbx+64]
 jb .Ld_snapshot
 call json_object_new_object@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Ld_failed
 mov rdi,rax
 mov rsi,[rbx+24]
 lea rdx,[rip+.LdK_device_auth_id]
 call .Ld_copy
 test eax,eax
 jnz .Ld_failed
 mov rdi,[rsp+24]
 mov rsi,[rbx+24]
 lea rdx,[rip+.LdK_user_code]
 call .Ld_copy
 test eax,eax
 jnz .Ld_failed
 mov rdi,[rsp+24]
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Ld_failed
 mov rcx,rax
 mov rax,[rbx+8]
 mov rsi,[rax+8]
 lea rdi,[rbx+16]
 lea rdx,[rip+.Ld_json_type]
 call pg_oauth_http_start@PLT
 test eax,eax
 jnz .Ld_failed
 mov dword ptr [rbx+40],3
 jmp .Ld_snapshot
.Ld_pending:
 mov rdi,[rbx+16]
 call pg_oauth_http_close@PLT
 mov qword ptr [rbx+16],0
 mov rdi,rbx
 call .Ld_now
 test rax,rax
 js .Ld_failed
 add rax,[rbx+48]
 mov [rbx+64],rax
 mov dword ptr [rbx+40],2
 jmp .Ld_snapshot
.Ld_exchange:
 DGET "qword ptr [rsp]",authorization_code
 mov rdi,rax
 call .Ld_string
 test rax,rax
 jz .Ld_failed
 mov rsi,rax
 xor edi,edi
 xor edx,edx
 call curl_easy_escape@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Ld_failed
 DGET "qword ptr [rsp]",code_verifier
 mov rdi,rax
 call .Ld_string
 test rax,rax
 jz .Ld_failed
 mov rsi,rax
 xor edi,edi
 xor edx,edx
 call curl_easy_escape@PLT
 mov [rsp+48],rax
 test rax,rax
 jz .Ld_failed
 mov rax,[rbx+8]
 mov rsi,[rax+32]
 xor edi,edi
 xor edx,edx
 call curl_easy_escape@PLT
 mov r14,rax
 test rax,rax
 jz .Ld_failed
 lea rdi,[rsp+32]
 lea rsi,[rip+.Ld_exchange_form]
 mov rdx,[rsp+40]
 mov rcx,[rsp+48]
 mov r8,r14
 xor eax,eax
 call asprintf@PLT
 mov ebp,eax
 mov rdi,r14
 call curl_free@PLT
 test ebp,ebp
 js .Ld_failed
 mov rax,[rbx+8]
 mov rsi,[rax+16]
 lea rdi,[rbx+16]
 lea rdx,[rip+.Ld_form_type]
 mov rcx,[rsp+32]
 call pg_oauth_http_start@PLT
 test eax,eax
 jnz .Ld_failed
 mov dword ptr [rbx+40],4
 jmp .Ld_snapshot
.Ld_complete:
 mov rdi,rbx
 call .Ld_now
 test rax,rax
 js .Ld_failed
 cvtsi2sd xmm0,rax
 mov rdi,[rsp]
 lea rsi,[rbx+32]
 call pg_oauth_codex_token@PLT
 test eax,eax
 jnz .Ld_failed
 mov r14,[rbx]
 test r14,r14
 jz .Ld_failed
 cmp qword ptr [r14+24],0
 je .Ld_failed
 mov rdi,[r14]
 lea rsi,[rip+.Ld_identity]
 lea rdx,[rip+.Ld_save]
 mov rcx,[rbx+32]
 lea r8,[rsp+56]
 call qword ptr [r14+24]
 test eax,eax
 jnz .Ld_failed
 mov rdi,rbx
 call pg_oauth_device_close
 mov dword ptr [rbx+40],6
 jmp .Ld_snapshot
.Ld_timed_out:
 mov r13d,[rbx+44]
 mov rdi,rbx
 call pg_oauth_device_close
 mov [rbx+44],r13d
 mov dword ptr [rbx+40],9
 jmp .Ld_snapshot
.Ld_failed:
 mov rdi,rbx
 call pg_oauth_device_close
 mov dword ptr [rbx+40],7
.Ld_snapshot:
 mov rdi,rbx
 mov rsi,r12
 call .Ld_public
 mov ebp,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call free@PLT
 mov rdi,[rsp+40]
 call curl_free@PLT
 mov rdi,[rsp+48]
 call curl_free@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbp
 pop rbx
 ret
.size pg_oauth_device_poll,.-pg_oauth_device_poll
.Ld_now:
 mov rax,[rdi+8]
 mov rdi,[rax+40]
 mov rax,[rax+48]
 test rax,rax
 jz .Ld_default_now
 sub rsp,8
 call rax
 jmp .Ld_now_convert
.Ld_default_now:
 sub rsp,8
 call pg_oauth_realtime@PLT
.Ld_now_convert:
 cvttsd2si rax,xmm0
 add rsp,8
 ret
.Ld_string:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 call json_object_get_type@PLT
 cmp eax,6
 jne .Ld_string_bad
 mov rdi,rbx
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Ld_string_bad
 mov [rsp],rax
 mov rdi,rbx
 call json_object_get_string@PLT
 mov r12,rax
 mov rdi,rax
 call strlen@PLT
 cmp rax,[rsp]
 jne .Ld_string_bad
 mov rax,r12
 jmp .Ld_string_end
.Ld_string_bad:
 xor eax,eax
.Ld_string_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.Ld_interval:
 push rbx
 sub rsp,16
 mov rbx,rdi
 call json_object_get_type@PLT
 cmp eax,6
 je .Ld_interval_string
 cmp eax,2
 je .Ld_interval_number
 cmp eax,3
 jne .Ld_interval_bad
.Ld_interval_number:
 mov rdi,rbx
 call json_object_get_double@PLT
 jmp .Ld_interval_scale
.Ld_interval_string:
 mov rdi,rbx
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rsp]
 call strtod@PLT
 mov rax,[rsp]
.Ld_interval_tail:
 movzx edx,byte ptr [rax]
 test edx,edx
 jz .Ld_interval_scale
 cmp edx,32
 je .Ld_interval_tail_next
 sub edx,9
 cmp edx,4
 ja .Ld_interval_bad
.Ld_interval_tail_next:
 inc rax
 jmp .Ld_interval_tail
.Ld_interval_scale:
 ucomisd xmm0,[rip+.Ld_zero]
 jb .Ld_interval_bad
 jp .Ld_interval_bad
 mulsd xmm0,[rip+.Ld_thousand]
 cvttsd2si rax,xmm0
 test rax,rax
 js .Ld_interval_bad
 cmp rax,1000
 jae .Ld_interval_end
 mov eax,1000
 jmp .Ld_interval_end
.Ld_interval_bad:
 xor eax,eax
.Ld_interval_end:
 add rsp,16
 pop rbx
 ret
.Ld_parse:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 call strlen@PLT
 cmp rax,2147483646
 ja .Ld_parse_bad
 mov [rsp],rax
 call json_tokener_new@PLT
 mov r12,rax
 test rax,rax
 jz .Ld_parse_bad
 mov rdi,rax
 mov esi,17
 call json_tokener_set_flags@PLT
 mov rdi,r12
 mov rsi,rbx
 mov rdx,[rsp]
 inc edx
 call json_tokener_parse_ex@PLT
 mov rbx,rax
 mov rdi,r12
 call json_tokener_get_error@PLT
 test eax,eax
 jz .Ld_parse_free
 mov rdi,rbx
 call json_object_put@PLT
 xor ebx,ebx
.Ld_parse_free:
 mov rdi,r12
 call json_tokener_free@PLT
 mov rax,rbx
 jmp .Ld_parse_end
.Ld_parse_bad:
 xor eax,eax
.Ld_parse_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.Ld_copy:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rdx
 mov rdi,rsi
 mov rsi,rdx
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call .Ld_add
 add rsp,8
 pop r12
 pop rbx
 ret
.Ld_add:
 push rbx
 mov rbx,rdx
 test rdx,rdx
 jz .Ld_add_bad
 call json_object_object_add@PLT
 test eax,eax
 jz .Ld_add_end
 mov rdi,rbx
 call json_object_put@PLT
.Ld_add_bad:
 mov eax,7
.Ld_add_end:
 pop rbx
 ret
.Ld_save:
 mov rsi,rdx
 xor edx,edx
 jmp json_object_deep_copy@PLT
.Ld_public:
 push rbx
 push rbp
 push r12
 mov rbx,rdi
 mov r12,rsi
 call json_object_new_object@PLT
 mov rbp,rax
 test rax,rax
 jz .Ld_public_bad
 mov eax,[rbx+40]
 cmp eax,9
 ja .Ld_public_release
 lea rdx,[rip+.Ld_statuses]
 mov rdi,[rdx+rax*8]
 call json_object_new_string@PLT
 mov rdi,rbp
 lea rsi,[rip+.LdK_status]
 mov rdx,rax
 call .Ld_add
 test eax,eax
 jnz .Ld_public_release
 cmp dword ptr [rbx+40],9
 jne .Ld_public_device
 lea rdi,[rip+.Ld_timeout]
 cmp dword ptr [rbx+44],0
 je .Ld_public_message
 lea rdi,[rip+.Ld_slow_timeout]
.Ld_public_message:
 call json_object_new_string@PLT
 mov rdi,rbp
 lea rsi,[rip+.LdK_message]
 mov rdx,rax
 call .Ld_add
 test eax,eax
 jnz .Ld_public_release
.Ld_public_device:
 cmp qword ptr [rbx+24],0
 je .Ld_public_done
 DGET "qword ptr [rbx+24]",user_code
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 call json_object_new_string@PLT
 mov rdi,rbp
 lea rsi,[rip+.LdK_user_code]
 mov rdx,rax
 call .Ld_add
 test eax,eax
 jnz .Ld_public_release
 mov rax,[rbx+8]
 mov rdi,[rax+24]
 call json_object_new_string@PLT
 mov rdi,rbp
 lea rsi,[rip+.LdK_verification_uri]
 mov rdx,rax
 call .Ld_add
 test eax,eax
 jnz .Ld_public_release
 mov rdi,[rbx+64]
 call json_object_new_uint64@PLT
 mov rdi,rbp
 lea rsi,[rip+.LdK_next_poll]
 mov rdx,rax
 call .Ld_add
 test eax,eax
 jnz .Ld_public_release
.Ld_public_done:
 mov [r12],rbp
 xor eax,eax
 jmp .Ld_public_end
.Ld_public_release:
 mov rdi,rbp
 call json_object_put@PLT
.Ld_public_bad:
 mov eax,7
.Ld_public_end:
 pop r12
 pop rbp
 pop rbx
 ret
.globl pg_oauth_device_service
.type pg_oauth_device_service,@function
pg_oauth_device_service:
 mov [rsi],rdi
 mov rax,qword ptr [rip+pg_oauth_device_begin@GOTPCREL]
 mov [rsi+8],rax
 mov rax,qword ptr [rip+pg_oauth_device_poll@GOTPCREL]
 mov [rsi+16],rax
 mov rax,qword ptr [rip+pg_oauth_device_cancel@GOTPCREL]
 mov [rsi+24],rax
 lea rax,[rip+.Ld_list]
 mov [rsi+32],rax
 lea rax,[rip+.Ld_remove]
 mov [rsi+40],rax
 ret
.size pg_oauth_device_service,.-pg_oauth_device_service
.Ld_list:
 mov rax,[rdi]
 mov rdi,[rax]
 jmp qword ptr [rax+16]
.Ld_remove:
 mov rax,[rdi]
 mov rdi,[rax]
 jmp qword ptr [rax+32]
.section .rodata
.Ld_identity: .asciz "openai-codex"
.Ld_json_type: .asciz "Content-Type: application/json"
.Ld_form_type: .asciz "Content-Type: application/x-www-form-urlencoded"
.Ld_start_body: .asciz "{\"client_id\":\"app_EMoamEEZ73f0CkXaXp7hrann\"}"
.Ld_start_url: .asciz "https://auth.openai.com/api/accounts/deviceauth/usercode"
.Ld_poll_url: .asciz "https://auth.openai.com/api/accounts/deviceauth/token"
.Ld_token_url: .asciz "https://auth.openai.com/oauth/token"
.Ld_verify_url: .asciz "https://auth.openai.com/codex/device"
.Ld_redirect: .asciz "https://auth.openai.com/deviceauth/callback"
.Ld_exchange_form: .asciz "grant_type=authorization_code&client_id=app_EMoamEEZ73f0CkXaXp7hrann&code=%s&code_verifier=%s&redirect_uri=%s"
.Ld_pending_error: .asciz "deviceauth_authorization_pending"
.Ld_slow_error: .asciz "slow_down"
.Ld_idle: .asciz "idle"
.Ld_requesting: .asciz "requesting"
.Ld_device_code: .asciz "device_code"
.Ld_pending_status: .asciz "pending"
.Ld_exchanging: .asciz "exchanging"
.Ld_complete_status: .asciz "complete"
.Ld_failed_status: .asciz "failed"
.Ld_cancelled: .asciz "cancelled"
.Ld_timed_out_status: .asciz "timed_out"
.Ld_timeout: .asciz "Device flow timed out"
.Ld_slow_timeout: .asciz "Device flow timed out after one or more slow_down responses. This is often caused by clock drift in WSL or VM environments. Please sync or restart the VM clock and try again."
.LdK_device_auth_id: .asciz "device_auth_id"
.LdK_user_code: .asciz "user_code"
.LdK_interval: .asciz "interval"
.LdK_authorization_code: .asciz "authorization_code"
.LdK_code_verifier: .asciz "code_verifier"
.LdK_error: .asciz "error"
.LdK_code: .asciz "code"
.LdK_status: .asciz "status"
.LdK_message: .asciz "message"
.LdK_verification_uri: .asciz "verification_uri"
.LdK_next_poll: .asciz "next_poll"
.Ld_zero: .double 0
.Ld_thousand: .double 1000
.section .data.rel.ro
.Ld_default_config: .quad .Ld_start_url,.Ld_poll_url,.Ld_token_url,.Ld_verify_url,.Ld_redirect,0,0
.Ld_statuses: .quad .Ld_idle,.Ld_requesting,.Ld_device_code,.Ld_pending_status,.Ld_exchanging,.Ld_exchanging,.Ld_complete_status,.Ld_failed_status,.Ld_cancelled,.Ld_timed_out_status
.section .note.GNU-stack,"",@progbits
