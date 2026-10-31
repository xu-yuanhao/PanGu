/* Pi GitHub Copilot device login, account policies and bounded retries native adaptation.
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
.macro CLGET object,key
 mov rdi,\object
 lea rsi,[rip+.ClK_\key]
 call json_object_object_get@PLT
.endm
.text
.globl pg_oauth_copilot_configure
.type pg_oauth_copilot_configure,@function
pg_oauth_copilot_configure:
 mov [rdi+104],rsi
 ret
.size pg_oauth_copilot_configure,.-pg_oauth_copilot_configure
.globl pg_oauth_copilot_begin
.type pg_oauth_copilot_begin,@function
pg_oauth_copilot_begin:
 push rbx
 push rbp
 push r12
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov eax,[rbx+40]
 dec eax
 cmp eax,3
 jbe .Cl_begin_busy
 cmp dword ptr [rbx+40],10
 jae .Cl_begin_busy
 mov rdi,rbx
 call pg_oauth_device_close@PLT
 cmp qword ptr [rbx+104],0
 jne .Cl_begin_config
 lea rax,[rip+.Cl_default_config]
 mov [rbx+104],rax
.Cl_begin_config:
 test r12,r12
 jnz .Cl_begin_domain
 mov rax,[rbx+104]
 mov r12,[rax+32]
.Cl_begin_domain:
 test r12,r12
 jz .Cl_begin_state
 mov rdi,r12
 call strdup@PLT
 mov [rsp],rax
 test rax,rax
 jz .Cl_begin_bad
 mov rdi,rax
 call g_strchug@PLT
 mov rdi,rax
 call g_strchomp@PLT
 cmp byte ptr [rax],0
 je .Cl_begin_state
 mov rdi,rax
 call pg_oauth_copilot_domain@PLT
 mov [rbx+112],rax
 test rax,rax
 jz .Cl_begin_bad
.Cl_begin_state:
 call json_object_new_object@PLT
 mov [rbx+72],rax
 test rax,rax
 jz .Cl_begin_bad
 lea rax,[rip+.Cl_identity]
 mov [rbx+96],rax
 mov rdi,rbx
 xor esi,esi
 call .Cl_url
 mov [rsp+8],rax
 test rax,rax
 jz .Cl_begin_bad
 mov rdi,rbx
 mov esi,11
 mov rdx,rax
 lea rcx,[rip+.Cl_start_form]
 xor r8d,r8d
 call .Cl_send
 test eax,eax
 jnz .Cl_begin_bad
 xor ebp,ebp
 jmp .Cl_begin_end
.Cl_begin_busy:
 mov ebp,3
 jmp .Cl_begin_end
.Cl_begin_bad:
 mov rdi,rbx
 call pg_oauth_device_close@PLT
 mov dword ptr [rbx+40],7
 mov ebp,5
.Cl_begin_end:
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov eax,ebp
 add rsp,16
 pop r12
 pop rbp
 pop rbx
 ret
.size pg_oauth_copilot_begin,.-pg_oauth_copilot_begin
.globl pg_oauth_copilot_poll
.type pg_oauth_copilot_poll,@function
pg_oauth_copilot_poll:
 push rbx
 push rbp
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 xor eax,eax
 mov ecx,8
 mov rdi,rsp
 rep stosq
 mov ebp,[rbx+40]
 cmp ebp,12
 je .Cl_wait
 cmp ebp,17
 je .Cl_retry_wait
 mov rdi,[rbx+16]
 lea rsi,[rsp+8]
 lea rdx,[rsp+16]
 call pg_oauth_http_step@PLT
 cmp eax,1
 je .Cl_poll_end
 test eax,eax
 jz .Cl_response
 cmp ebp,16
 je .Cl_policy_skip
 jmp .Cl_fail
.Cl_response:
 cmp qword ptr [rsp+8],429
 jne .Cl_response_normal
 cmp ebp,15
 je .Cl_rate
 cmp ebp,16
 je .Cl_rate
.Cl_response_normal:
 cmp ebp,16
 je .Cl_policy_response
 cmp qword ptr [rsp+8],200
 jb .Cl_fail
 cmp qword ptr [rsp+8],300
 jae .Cl_fail
 mov rdi,[rsp+16]
 call pg_oauth_json_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Cl_fail
 mov rdi,rbx
 call .Cl_http_close
 cmp ebp,11
 je .Cl_device
 cmp ebp,13
 je .Cl_access
 cmp ebp,14
 je .Cl_token
 cmp ebp,15
 je .Cl_models
 jmp .Cl_fail
.Cl_device:
 mov rdi,[rsp]
 lea rsi,[rip+.ClK_device_code]
 call pg_oauth_string_field@PLT
 test rax,rax
 jz .Cl_fail
 mov rdi,[rsp]
 lea rsi,[rip+.ClK_user_code]
 call pg_oauth_string_field@PLT
 test rax,rax
 jz .Cl_fail
 mov rdi,[rsp]
 lea rsi,[rip+.ClK_verification_uri]
 call pg_oauth_string_field@PLT
 test rax,rax
 jz .Cl_fail
 mov rdi,rax
 call .Cl_verify_url
 test eax,eax
 jnz .Cl_fail
 mov qword ptr [rbx+48],5000
 CLGET "qword ptr [rsp]",interval
 test rax,rax
 jz .Cl_device_expiry
 mov rdi,rax
 call .Cl_millis
 test edx,edx
 jnz .Cl_fail
 cmp rax,1000
 jge .Cl_device_interval
 mov eax,1000
.Cl_device_interval:
 mov [rbx+48],rax
.Cl_device_expiry:
 CLGET "qword ptr [rsp]",expires_in
 mov rdi,rax
 call .Cl_millis
 mov r12,rax
 test edx,edx
 jnz .Cl_fail
 mov rdi,rbx
 call .Cl_now
 test rax,rax
 js .Cl_fail
 mov rdx,rax
 test r12,r12
 jle .Cl_deadline
 add rdx,r12
.Cl_deadline:
 mov [rbx+56],rdx
 add rax,[rbx+48]
 mov [rbx+64],rax
 mov rax,[rsp]
 mov [rbx+24],rax
 mov qword ptr [rsp],0
 mov dword ptr [rbx+40],12
 jmp .Cl_poll_end
.Cl_wait:
 mov rdi,rbx
 call .Cl_now
 test rax,rax
 js .Cl_fail
 cmp rax,[rbx+56]
 jae .Cl_timeout
 cmp rax,[rbx+64]
 jb .Cl_poll_end
 mov rdi,[rbx+24]
 lea rsi,[rip+.ClK_device_code]
 call pg_oauth_string_field@PLT
 mov rsi,rax
 xor edi,edi
 xor edx,edx
 call curl_easy_escape@PLT
 mov [rsp+48],rax
 test rax,rax
 jz .Cl_fail
 lea rdi,[rsp+32]
 lea rsi,[rip+.Cl_access_form]
 mov rdx,rax
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Cl_fail
 mov rdi,rbx
 mov esi,1
 call .Cl_url
 mov [rsp+40],rax
 test rax,rax
 jz .Cl_fail
 mov rdi,rbx
 mov esi,13
 mov rdx,rax
 mov rcx,[rsp+32]
 xor r8d,r8d
 call .Cl_send
 test eax,eax
 jnz .Cl_fail
 jmp .Cl_poll_end
.Cl_access:
 mov rdi,[rsp]
 lea rsi,[rip+.ClK_access_token]
 call pg_oauth_string_field@PLT
 test rax,rax
 jz .Cl_access_error
 mov rdi,[rbx+72]
 lea rsi,[rip+.ClK_githubAccess]
 mov rdx,rax
 call .Cl_string
 test eax,eax
 jnz .Cl_fail
 mov rdi,rbx
 mov esi,2
 call .Cl_url
 mov [rsp+40],rax
 test rax,rax
 jz .Cl_fail
 mov r12,rax
 mov rdi,[rbx+72]
 lea rsi,[rip+.ClK_githubAccess]
 call pg_oauth_string_field@PLT
 mov r8,rax
 mov rdi,rbx
 mov esi,14
 mov rdx,r12
 xor ecx,ecx
 call .Cl_send
 test eax,eax
 jnz .Cl_fail
 jmp .Cl_poll_end
.Cl_access_error:
 mov rdi,[rsp]
 lea rsi,[rip+.ClK_error]
 call pg_oauth_string_field@PLT
 test rax,rax
 jz .Cl_fail
 mov r12,rax
 mov rdi,rax
 lea rsi,[rip+.Cl_pending]
 call strcmp@PLT
 test eax,eax
 jz .Cl_pending_poll
 mov rdi,r12
 lea rsi,[rip+.Cl_slow]
 call strcmp@PLT
 test eax,eax
 jnz .Cl_fail
 mov dword ptr [rbx+44],1
 CLGET "qword ptr [rsp]",interval
 mov rdi,rax
 call .Cl_millis
 test edx,edx
 jnz .Cl_increment
 ucomisd xmm0,[rip+.Cl_zero]
 jbe .Cl_increment
 cmp rax,1000
 jae .Cl_slow_set
 mov eax,1000
.Cl_slow_set:
 mov [rbx+48],rax
 jmp .Cl_pending_poll
.Cl_increment:
 add qword ptr [rbx+48],5000
.Cl_pending_poll:
 mov rdi,rbx
 call .Cl_now
 test rax,rax
 js .Cl_fail
 add rax,[rbx+48]
 mov [rbx+64],rax
 mov dword ptr [rbx+40],12
 jmp .Cl_poll_end
.Cl_token:
 mov rdi,[rbx+72]
 lea rsi,[rip+.ClK_githubAccess]
 call pg_oauth_string_field@PLT
 mov rsi,rax
 mov rdi,[rsp]
 mov rdx,[rbx+112]
 lea rcx,[rbx+32]
 call pg_oauth_copilot_token@PLT
 test eax,eax
 jnz .Cl_fail
 mov rdi,rbx
 call .Cl_reset_retry
 mov rdi,rbx
 mov esi,15
 call .Cl_catalog_request
 test eax,eax
 jnz .Cl_fail
 jmp .Cl_poll_end
.Cl_models:
 mov rdi,[rbx+32]
 call pg_oauth_copilot_base@PLT
 mov r12,rax
 test rax,rax
 jz .Cl_fail
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Cl_individual]
 call strcmp@PLT
 test eax,eax
 sete al
 movzx r13d,al
 mov rdi,r12
 call json_object_put@PLT
 mov rdi,[rsp]
 mov esi,r13d
 lea rdx,[rsp+24]
 call pg_copilot_catalog@PLT
 test eax,eax
 jnz .Cl_fail
 mov rdi,[rbx+32]
 lea rsi,[rip+.ClK_availableModelIds]
 mov rdx,[rsp+24]
 mov qword ptr [rsp+24],0
 call .Cl_add
 test eax,eax
 jnz .Cl_fail
 mov rax,[rbx+104]
 mov rdx,[rax+56]
 mov rdi,[rsp]
 mov esi,r13d
 lea rcx,[rsp+24]
 call pg_copilot_policy_models@PLT
 test eax,eax
 jnz .Cl_fail
 mov rdi,[rbx+72]
 lea rsi,[rip+.ClK_policyModelIds]
 mov rdx,[rsp+24]
 mov qword ptr [rsp+24],0
 call .Cl_add
 test eax,eax
 jnz .Cl_fail
 call json_object_new_array@PLT
 mov rdi,[rbx+72]
 lea rsi,[rip+.ClK_enabledModelIds]
 mov rdx,rax
 call .Cl_add
 test eax,eax
 jnz .Cl_fail
 mov qword ptr [rbx+136],0
.Cl_next_policy:
 CLGET "qword ptr [rbx+72]",policyModelIds
 mov rdi,rax
 call json_object_array_length@PLT
 cmp [rbx+136],rax
 jae .Cl_finish
 mov rdi,rbx
 call .Cl_reset_retry
 mov rdi,rbx
 mov esi,16
 call .Cl_catalog_request
 test eax,eax
 jz .Cl_poll_end
 inc qword ptr [rbx+136]
 jmp .Cl_next_policy
.Cl_policy_response:
 mov rdi,rbx
 call .Cl_http_close
 cmp qword ptr [rsp+8],200
 jb .Cl_policy_skip
 cmp qword ptr [rsp+8],300
 jae .Cl_policy_skip
 CLGET "qword ptr [rbx+72]",policyModelIds
 mov rdi,rax
 mov rsi,[rbx+136]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov r12,rax
 CLGET "qword ptr [rbx+72]",enabledModelIds
 mov rdi,rax
 mov rsi,r12
 call json_object_array_add@PLT
 test eax,eax
 jz .Cl_policy_skip
 mov rdi,r12
 call json_object_put@PLT
 jmp .Cl_fail
.Cl_policy_skip:
 mov rdi,rbx
 call .Cl_http_close
 inc qword ptr [rbx+136]
 jmp .Cl_next_policy
.Cl_rate:
 cmp dword ptr [rbx+128],2
 jae .Cl_rate_exhausted
 mov rdi,rbx
 call .Cl_retry_delay
 test rax,rax
 js .Cl_rate_exhausted
 mov r12,rax
 mov rdi,rbx
 call .Cl_now
 mov rdx,[rbx+120]
 sub rdx,rax
 test rdx,rdx
 jle .Cl_rate_exhausted
 cmp r12,rdx
 jae .Cl_rate_exhausted
 add rax,r12
 mov [rbx+64],rax
 inc dword ptr [rbx+128]
 mov [rbx+132],ebp
 mov rdi,rbx
 call .Cl_http_close
 mov dword ptr [rbx+40],17
 jmp .Cl_poll_end
.Cl_rate_exhausted:
 cmp ebp,16
 je .Cl_finish
 jmp .Cl_fail
.Cl_retry_wait:
 mov rdi,rbx
 call .Cl_now
 cmp rax,[rbx+64]
 jb .Cl_poll_end
 mov rdi,rbx
 mov esi,[rbx+132]
 call .Cl_catalog_request
 test eax,eax
 jz .Cl_poll_end
 cmp dword ptr [rbx+132],16
 je .Cl_policy_skip
 jmp .Cl_fail
.Cl_finish:
 mov rdi,rbx
 call .Cl_http_close
 CLGET "qword ptr [rbx+32]",availableModelIds
 mov r12,rax
 CLGET "qword ptr [rbx+72]",enabledModelIds
 mov rdi,r12
 mov rsi,rax
 lea rdx,[rsp+24]
 call pg_copilot_merge_models@PLT
 test eax,eax
 jnz .Cl_fail
 mov rdi,[rbx+32]
 lea rsi,[rip+.ClK_availableModelIds]
 mov rdx,[rsp+24]
 mov qword ptr [rsp+24],0
 call .Cl_add
 test eax,eax
 jnz .Cl_fail
 mov r12,[rbx]
 mov rdi,[r12]
 lea rsi,[rip+.Cl_identity]
 lea rdx,[rip+.Cl_save]
 mov rcx,[rbx+32]
 lea r8,[rsp+24]
 call qword ptr [r12+24]
 test eax,eax
 jnz .Cl_fail
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov qword ptr [rsp+24],0
 mov rdi,rbx
 call pg_oauth_device_close@PLT
 mov dword ptr [rbx+40],6
 jmp .Cl_poll_end
.Cl_timeout:
 mov r12d,[rbx+44]
 mov rdi,rbx
 call pg_oauth_device_close@PLT
 mov [rbx+44],r12d
 mov dword ptr [rbx+40],9
 jmp .Cl_poll_end
.Cl_fail:
 mov rdi,rbx
 call pg_oauth_device_close@PLT
 mov dword ptr [rbx+40],7
.Cl_poll_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call free@PLT
 mov rdi,[rsp+40]
 call free@PLT
 mov rdi,[rsp+48]
 call curl_free@PLT
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbp
 pop rbx
 ret
.size pg_oauth_copilot_poll,.-pg_oauth_copilot_poll
.Cl_http_close:
 push rbx
 mov rbx,rdi
 mov rdi,[rbx+16]
 call pg_oauth_http_close@PLT
 mov qword ptr [rbx+16],0
 pop rbx
 ret
.Cl_now:
 mov rax,[rdi+104]
 mov rdi,[rax+40]
 mov rax,[rax+48]
 sub rsp,8
 test rax,rax
 jz .Cl_now_default
 call rax
 jmp .Cl_now_convert
.Cl_now_default:
 call pg_oauth_realtime@PLT
.Cl_now_convert:
 cvttsd2si rax,xmm0
 add rsp,8
 ret
.Cl_reset_retry:
 push rbx
 mov rbx,rdi
 call .Cl_now
 add rax,5000
 mov [rbx+120],rax
 mov qword ptr [rbx+128],0
 pop rbx
 ret
.Cl_millis:
 push rbx
 mov rbx,rdi
 call json_object_get_type@PLT
 cmp eax,2
 je .Cl_number
 cmp eax,3
 jne .Cl_number_bad
.Cl_number:
 mov rdi,rbx
 call json_object_get_double@PLT
 mulsd xmm0,[rip+.Cl_thousand]
 cvttsd2si rax,xmm0
 mov rdx,0x8000000000000000
 cmp rax,rdx
 je .Cl_number_bad
 xor edx,edx
 pop rbx
 ret
.Cl_number_bad:
 xor eax,eax
 mov edx,1
 pop rbx
 ret
.Cl_verify_url:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov qword ptr [rsp],0
 call curl_url@PLT
 mov r12,rax
 test rax,rax
 jz .Cl_url_bad
 mov rdi,rax
 xor esi,esi
 mov rdx,rbx
 xor ecx,ecx
 call curl_url_set@PLT
 test eax,eax
 jnz .Cl_url_bad
 mov rdi,r12
 mov esi,1
 mov rdx,rsp
 xor ecx,ecx
 call curl_url_get@PLT
 test eax,eax
 jnz .Cl_url_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Cl_https]
 call strcmp@PLT
 test eax,eax
 jz .Cl_url_good
 mov rdi,[rsp]
 lea rsi,[rip+.Cl_http]
 call strcmp@PLT
 test eax,eax
 jnz .Cl_url_bad
.Cl_url_good:
 xor ebx,ebx
 jmp .Cl_url_end
.Cl_url_bad:
 mov ebx,7
.Cl_url_end:
 mov rdi,[rsp]
 call curl_free@PLT
 mov rdi,r12
 call curl_url_cleanup@PLT
 mov eax,ebx
 add rsp,8
 pop r12
 pop rbx
 ret
.Cl_url:
 push rbx
 sub rsp,16
 mov qword ptr [rsp],0
 mov rbx,rdi
 mov rax,[rdi+104]
 mov rdi,[rax+rsi*8]
 test rdi,rdi
 jz .Cl_make_url
 call strdup@PLT
 jmp .Cl_url_result
.Cl_make_url:
 mov rdx,[rbx+112]
 test rdx,rdx
 jnz .Cl_url_domain
 lea rdx,[rip+.Cl_github]
.Cl_url_domain:
 lea rax,[rip+.Cl_url_formats]
 mov rsi,[rax+rsi*8]
 mov rdi,rsp
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Cl_url_zero
 mov rax,[rsp]
 jmp .Cl_url_result
.Cl_url_zero:
 xor eax,eax
.Cl_url_result:
 add rsp,16
 pop rbx
 ret
.Cl_catalog_request:
 push rbx
 push rbp
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov ebp,esi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov rdi,[rbx+32]
 call pg_oauth_copilot_base@PLT
 mov [rsp],rax
 test rax,rax
 jz .Cl_catalog_bad
 mov rdi,rax
 call json_object_get_string@PLT
 mov r12,rax
 mov rax,[rbx+104]
 cmp qword ptr [rax+24],0
 je .Cl_network_base
 mov r12,[rax+24]
.Cl_network_base:
 cmp ebp,15
 jne .Cl_policy_url
 mov rdx,r12
 lea rsi,[rip+.Cl_models_format]
 jmp .Cl_format_url
.Cl_policy_url:
 CLGET "qword ptr [rbx+72]",policyModelIds
 mov rdi,rax
 mov rsi,[rbx+136]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov rsi,rax
 xor edi,edi
 xor edx,edx
 call curl_easy_escape@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Cl_catalog_bad
 mov rcx,rax
 mov rdx,r12
 lea rsi,[rip+.Cl_policy_format]
.Cl_format_url:
 lea rdi,[rsp+8]
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Cl_catalog_bad
 mov rdi,[rbx+32]
 lea rsi,[rip+.ClK_access]
 call pg_oauth_string_field@PLT
 mov r8,rax
 xor ecx,ecx
 cmp ebp,16
 jne .Cl_catalog_send
 lea rcx,[rip+.Cl_policy_body]
.Cl_catalog_send:
 mov rdi,rbx
 mov esi,ebp
 mov rdx,[rsp+8]
 call .Cl_send
 mov r14d,eax
 jmp .Cl_catalog_end
.Cl_catalog_bad:
 mov r14d,7
.Cl_catalog_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov rdi,[rsp+16]
 call curl_free@PLT
 mov eax,r14d
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbp
 pop rbx
 ret
.Cl_send:
 push rbx
 push rbp
 push r12
 push r13
 push r14
 sub rsp,96
 mov rbx,rdi
 mov ebp,esi
 mov r12,rdx
 mov r13,rcx
 mov r14,r8
 mov qword ptr [rsp+72],0
 mov qword ptr [rsp+80],0
 lea rdi,[rbx+16]
 mov rsi,r12
 xor edx,edx
 test r13,r13
 jz .Cl_send_type
 lea rdx,[rip+.Cl_form_type]
 cmp ebp,16
 jne .Cl_send_type
 lea rdx,[rip+.Cl_json_type]
.Cl_send_type:
 mov rcx,r13
 call pg_oauth_http_start@PLT
 test eax,eax
 jnz .Cl_send_bad
 lea rax,[rip+.Cl_accept]
 mov [rsp],rax
 lea rax,[rip+.Cl_agent]
 mov [rsp+8],rax
 mov r12d,2
 test r14,r14
 jz .Cl_send_timeout
 lea rdi,[rsp+72]
 lea rsi,[rip+.Cl_bearer]
 mov rdx,r14
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Cl_send_bad
 lea rax,[rip+.Cl_editor]
 mov [rsp+16],rax
 lea rax,[rip+.Cl_plugin]
 mov [rsp+24],rax
 lea rax,[rip+.Cl_integration]
 mov [rsp+32],rax
 mov rax,[rsp+72]
 mov [rsp+40],rax
 mov r12d,6
 cmp ebp,15
 jne .Cl_send_policy_headers
 lea rax,[rip+.Cl_api_version]
 mov [rsp+48],rax
 mov r12d,7
.Cl_send_policy_headers:
 cmp ebp,16
 jne .Cl_send_timeout
 lea rax,[rip+.Cl_intent]
 mov [rsp+48],rax
 lea rax,[rip+.Cl_interaction]
 mov [rsp+56],rax
 mov r12d,8
.Cl_send_timeout:
 mov ecx,15000
 cmp ebp,15
 jb .Cl_headers_apply
 mov rdi,rbx
 call .Cl_now
 mov rcx,[rbx+120]
 sub rcx,rax
 jle .Cl_send_bad
 cmp rcx,5000
 jbe .Cl_headers_apply
 mov ecx,5000
.Cl_headers_apply:
 mov rdi,[rbx+16]
 mov rsi,rsp
 mov rdx,r12
 call pg_oauth_http_headers@PLT
 test eax,eax
 jnz .Cl_send_bad
 mov [rbx+40],ebp
 xor r14d,r14d
 jmp .Cl_send_end
.Cl_send_bad:
 mov rdi,rbx
 call .Cl_http_close
 mov r14d,7
.Cl_send_end:
 mov rdi,[rsp+72]
 call free@PLT
 mov eax,r14d
 add rsp,96
 pop r14
 pop r13
 pop r12
 pop rbp
 pop rbx
 ret
.Cl_retry_delay:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov rdi,[rbx+16]
 call pg_oauth_http_retry_after@PLT
 test rax,rax
 jz .Cl_retry_default
 cmp byte ptr [rax],0
 je .Cl_retry_default
 mov r12,rax
 mov rdi,rax
 mov rsi,rsp
 call strtod@PLT
 cmp [rsp],r12
 je .Cl_retry_date
 mulsd xmm0,[rip+.Cl_thousand]
 cvttsd2si rax,xmm0
 mov rdx,0x8000000000000000
 cmp rax,rdx
 je .Cl_retry_invalid
 jmp .Cl_retry_nonnegative
.Cl_retry_date:
 mov rdi,r12
 xor esi,esi
 call curl_getdate@PLT
 cmp rax,-1
 je .Cl_retry_invalid
 imul r12,rax,1000
 mov rdi,rbx
 call .Cl_now
 sub r12,rax
 mov rax,r12
 jmp .Cl_retry_nonnegative
.Cl_retry_default:
 mov eax,500
 mov ecx,[rbx+128]
 shl rax,cl
.Cl_retry_nonnegative:
 test rax,rax
 jns .Cl_retry_end
 xor eax,eax
 jmp .Cl_retry_end
.Cl_retry_invalid:
 mov rax,-1
.Cl_retry_end:
 add rsp,24
 pop r12
 pop rbx
 ret
.Cl_save:
 mov rsi,rdx
 xor edx,edx
 jmp json_object_deep_copy@PLT
.Cl_string:
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
 call .Cl_add
 add rsp,8
 pop r12
 pop rbx
 ret
.Cl_add:
 push rbx
 mov rbx,rdx
 test rdx,rdx
 jz .Cl_add_bad
 call json_object_object_add@PLT
 test eax,eax
 jz .Cl_add_end
 mov rdi,rbx
 call json_object_put@PLT
.Cl_add_bad:
 mov eax,7
.Cl_add_end:
 pop rbx
 ret
.section .rodata
.Cl_identity: .asciz "github-copilot"
.Cl_github: .asciz "github.com"
.Cl_device_format: .asciz "https://%s/login/device/code"
.Cl_access_format: .asciz "https://%s/login/oauth/access_token"
.Cl_token_format: .asciz "https://api.%s/copilot_internal/v2/token"
.Cl_models_format: .asciz "%s/models"
.Cl_policy_format: .asciz "%s/models/%s/policy"
.Cl_start_form: .asciz "client_id=Iv1.b507a08c87ecfe98&scope=read%3Auser"
.Cl_access_form: .asciz "client_id=Iv1.b507a08c87ecfe98&device_code=%s&grant_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3Agrant-type%%3Adevice_code"
.Cl_policy_body: .asciz "{\"state\":\"enabled\"}"
.Cl_form_type: .asciz "Content-Type: application/x-www-form-urlencoded"
.Cl_json_type: .asciz "Content-Type: application/json"
.Cl_accept: .asciz "Accept: application/json"
.Cl_agent: .asciz "User-Agent: GitHubCopilotChat/0.35.0"
.Cl_editor: .asciz "Editor-Version: vscode/1.107.0"
.Cl_plugin: .asciz "Editor-Plugin-Version: copilot-chat/0.35.0"
.Cl_integration: .asciz "Copilot-Integration-Id: vscode-chat"
.Cl_api_version: .asciz "X-GitHub-Api-Version: 2026-06-01"
.Cl_intent: .asciz "openai-intent: chat-policy"
.Cl_interaction: .asciz "x-interaction-type: chat-policy"
.Cl_bearer: .asciz "Authorization: Bearer %s"
.Cl_pending: .asciz "authorization_pending"
.Cl_slow: .asciz "slow_down"
.Cl_individual: .asciz "https://api.individual.githubcopilot.com"
.Cl_http: .asciz "http"
.Cl_https: .asciz "https"
.ClK_device_code: .asciz "device_code"
.ClK_user_code: .asciz "user_code"
.ClK_verification_uri: .asciz "verification_uri"
.ClK_interval: .asciz "interval"
.ClK_expires_in: .asciz "expires_in"
.ClK_access_token: .asciz "access_token"
.ClK_githubAccess: .asciz "githubAccess"
.ClK_error: .asciz "error"
.ClK_access: .asciz "access"
.ClK_availableModelIds: .asciz "availableModelIds"
.ClK_policyModelIds: .asciz "policyModelIds"
.ClK_enabledModelIds: .asciz "enabledModelIds"
.Cl_thousand: .double 1000
.Cl_zero: .double 0
.section .data.rel.ro
.Cl_default_config: .quad 0,0,0,0,0,0,0,0
.Cl_url_formats: .quad .Cl_device_format,.Cl_access_format,.Cl_token_format
.section .note.GNU-stack,"",@progbits
