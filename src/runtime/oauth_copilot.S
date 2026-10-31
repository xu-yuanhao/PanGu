/* Pi auth/oauth/github-copilot.ts native stored credential adaptation.
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
.macro CPGET object,key
 mov rdi,\object
 lea rsi,[rip+.CpK_\key]
 call json_object_object_get@PLT
.endm
.text
.globl pg_oauth_copilot_init
.type pg_oauth_copilot_init,@function
pg_oauth_copilot_init:
 lea rax,[rip+.Cp_default]
 mov [rdi],rax
 lea rax,[rip+.Cp_identity]
 mov [rdi+8],rax
 mov rax,qword ptr [rip+pg_oauth_realtime@GOTPCREL]
 mov [rdi+16],rax
 mov rax,qword ptr [rip+pg_oauth_copilot_refresh@GOTPCREL]
 mov [rdi+24],rax
 ret
.size pg_oauth_copilot_init,.-pg_oauth_copilot_init
.globl pg_oauth_copilot_base
.type pg_oauth_copilot_base,@function
pg_oauth_copilot_base:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 lea rsi,[rip+.CpK_access]
 call pg_oauth_string_field@PLT
 test rax,rax
 jz .Cp_base_enterprise
 mov rdi,rax
 lea rsi,[rip+.Cp_proxy]
 call strstr@PLT
 test rax,rax
 jz .Cp_base_enterprise
 lea r12,[rax+9]
 mov rdi,r12
 mov esi,59
 call strchr@PLT
 test rax,rax
 jnz .Cp_proxy_end
 mov rdi,r12
 call strlen@PLT
 add rax,r12
.Cp_proxy_end:
 cmp rax,r12
 je .Cp_base_enterprise
 mov rsi,rax
 sub rsi,r12
 mov rdi,r12
 call strndup@PLT
 mov [rsp],rax
 test rax,rax
 jz .Cp_base_bad
 mov r12,rax
 mov rdi,rax
 lea rsi,[rip+.Cp_proxy_prefix]
 mov edx,6
 call strncmp@PLT
 test eax,eax
 jnz .Cp_proxy_plain
 lea rdx,[r12+6]
 lea rsi,[rip+.Cp_api_format]
 jmp .Cp_base_format
.Cp_proxy_plain:
 mov rdx,r12
 lea rsi,[rip+.Cp_https_format]
 jmp .Cp_base_format
.Cp_base_enterprise:
 mov rdi,rbx
 lea rsi,[rip+.CpK_enterpriseUrl]
 call pg_oauth_string_field@PLT
 test rax,rax
 jz .Cp_base_individual
 mov rdi,rax
 call .Cp_domain
 mov [rsp],rax
 test rax,rax
 jz .Cp_base_individual
 mov rdx,rax
 lea rsi,[rip+.Cp_enterprise_base]
.Cp_base_format:
 lea rdi,[rsp+8]
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Cp_base_bad
 mov rdi,[rsp+8]
 jmp .Cp_base_string
.Cp_base_individual:
 lea rdi,[rip+.Cp_individual]
.Cp_base_string:
 call json_object_new_string@PLT
 mov rbx,rax
 jmp .Cp_base_end
.Cp_base_bad:
 xor ebx,ebx
.Cp_base_end:
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov rax,rbx
 add rsp,24
 pop r12
 pop rbx
 ret
.size pg_oauth_copilot_base,.-pg_oauth_copilot_base
.Cp_domain:
 push rbx
 push r12
 sub rsp,40
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],0
 call curl_url@PLT
 mov r12,rax
 test rax,rax
 jz .Cp_domain_bad
 mov rdi,rbx
 call strdup@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Cp_domain_bad
 mov rdi,rax
 call g_strchug@PLT
 mov rdi,rax
 call g_strchomp@PLT
 mov rbx,rax
 mov rdi,rax
 lea rsi,[rip+.Cp_scheme]
 call strstr@PLT
 test rax,rax
 jnz .Cp_domain_parse
 lea rdi,[rsp]
 lea rsi,[rip+.Cp_https_format]
 mov rdx,rbx
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Cp_domain_bad
 mov rbx,[rsp]
.Cp_domain_parse:
 mov rdi,r12
 xor esi,esi
 mov rdx,rbx
 xor ecx,ecx
 call curl_url_set@PLT
 test eax,eax
 jnz .Cp_domain_bad
 mov rdi,r12
 mov esi,5
 lea rdx,[rsp+8]
 xor ecx,ecx
 call curl_url_get@PLT
 test eax,eax
 jnz .Cp_domain_bad
 mov rdi,[rsp+8]
 mov rsi,-1
 call g_ascii_strdown@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Cp_domain_bad
 mov rdi,rax
 call strdup@PLT
 mov rbx,rax
 jmp .Cp_domain_end
.Cp_domain_bad:
 xor ebx,ebx
.Cp_domain_end:
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call curl_free@PLT
 mov rdi,[rsp+16]
 call free@PLT
 mov rdi,[rsp+24]
 call g_free@PLT
 mov rdi,r12
 call curl_url_cleanup@PLT
 mov rax,rbx
 add rsp,40
 pop r12
 pop rbx
 ret
.globl pg_oauth_copilot_refresh
.type pg_oauth_copilot_refresh,@function
pg_oauth_copilot_refresh:
 push rbx
 push rbp
 push r12
 push r13
 push r14
 push r15
 sub rsp,152
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rdx],0
 xor eax,eax
 mov ecx,19
 mov rdi,rsp
 rep stosq
 mov ebp,7
 xor edi,edi
 call pg_oauth_realtime@PLT
 addsd xmm0,[rip+.Cp_fifteen_seconds]
 movsd [rsp+144],xmm0
 mov rdi,r12
 lea rsi,[rip+.CpK_refresh]
 call pg_oauth_string_field@PLT
 mov r14,rax
 test rax,rax
 jz .Cp_refresh_end
 mov rdi,r12
 lea rsi,[rip+.CpK_enterpriseUrl]
 call pg_oauth_string_field@PLT
 test rax,rax
 jnz .Cp_refresh_domain
 mov rax,[rbx+16]
.Cp_refresh_domain:
 test rax,rax
 jz .Cp_refresh_default_domain
 mov rdi,rax
 call .Cp_domain
 mov [rsp+16],rax
 test rax,rax
 jz .Cp_refresh_default_domain
 jmp .Cp_refresh_endpoint
.Cp_refresh_default_domain:
 lea rax,[rip+.Cp_github]
.Cp_refresh_endpoint:
 mov rdx,rax
 mov rax,[rbx]
 test rax,rax
 jz .Cp_refresh_make_url
 mov rdi,rax
 call strdup@PLT
 mov [rsp+8],rax
 jmp .Cp_refresh_auth
.Cp_refresh_make_url:
 lea rdi,[rsp+8]
 lea rsi,[rip+.Cp_token_format]
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Cp_refresh_end
.Cp_refresh_auth:
 cmp qword ptr [rsp+8],0
 je .Cp_refresh_end
 lea rdi,[rsp]
 lea rsi,[rip+.Cp_bearer_format]
 mov rdx,r14
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Cp_refresh_end
 mov rax,[rsp]
 mov [rsp+80],rax
 lea rax,[rip+.Cp_accept]
 mov [rsp+88],rax
 lea rax,[rip+.Cp_user_agent]
 mov [rsp+96],rax
 lea rax,[rip+.Cp_editor]
 mov [rsp+104],rax
 lea rax,[rip+.Cp_plugin]
 mov [rsp+112],rax
 lea rax,[rip+.Cp_integration]
 mov [rsp+120],rax
 mov rdi,[rsp+8]
 lea rsi,[rsp+80]
 mov edx,6
 movsd xmm0,[rsp+144]
 lea rcx,[rsp+24]
 call pg_oauth_get_json@PLT
 test eax,eax
 jnz .Cp_refresh_end
 mov rdi,[rsp+24]
 call pg_oauth_json_parse@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Cp_refresh_end
 mov rdi,rax
 lea rsi,[rip+.CpK_token]
 call pg_oauth_string_field@PLT
 mov r15,rax
 test rax,rax
 jz .Cp_refresh_end
 CPGET "qword ptr [rsp+32]",expires_at
 mov [rsp+136],rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,2
 je .Cp_expiry
 cmp eax,3
 jne .Cp_refresh_end
.Cp_expiry:
 mov rdi,[rsp+136]
 call json_object_get_double@PLT
 mulsd xmm0,[rip+.Cp_thousand]
 subsd xmm0,[rip+.Cp_margin]
 movsd [rsp+136],xmm0
 lea rdi,[rip+.Cp_template]
 call json_tokener_parse@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Cp_refresh_end
 mov rdi,rax
 lea rsi,[rip+.CpK_refresh]
 mov rdx,r14
 call .Cp_add_string
 test eax,eax
 jnz .Cp_refresh_end
 mov rdi,[rsp+40]
 lea rsi,[rip+.CpK_access]
 mov rdx,r15
 call .Cp_add_string
 test eax,eax
 jnz .Cp_refresh_end
 movsd xmm0,[rsp+136]
 call json_object_new_double@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.CpK_expires]
 mov rdx,rax
 call .Cp_add
 test eax,eax
 jnz .Cp_refresh_end
 cmp qword ptr [rsp+16],0
 je .Cp_refresh_base
 mov rdi,[rsp+40]
 lea rsi,[rip+.CpK_enterpriseUrl]
 mov rdx,[rsp+16]
 call .Cp_add_string
 test eax,eax
 jnz .Cp_refresh_end
.Cp_refresh_base:
 mov rdi,[rsp+40]
 call pg_oauth_copilot_base
 mov [rsp+48],rax
 test rax,rax
 jz .Cp_refresh_end
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Cp_individual]
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 mov [rsp+72],eax
 mov rdx,[rbx+8]
 test rdx,rdx
 jnz .Cp_models_url
 mov rdx,r14
.Cp_models_url:
 lea rdi,[rsp+56]
 lea rsi,[rip+.Cp_models_format]
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Cp_refresh_end
 mov rdi,[rsp]
 call free@PLT
 mov qword ptr [rsp],0
 lea rdi,[rsp]
 lea rsi,[rip+.Cp_bearer_format]
 mov rdx,r15
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Cp_refresh_end
 mov rax,[rsp]
 mov [rsp+80],rax
 lea rax,[rip+.Cp_api_version]
 mov [rsp+128],rax
 xor edi,edi
 call pg_oauth_realtime@PLT
 addsd xmm0,[rip+.Cp_five_seconds]
 minsd xmm0,[rsp+144]
 mov rdi,[rsp+56]
 lea rsi,[rsp+80]
 mov edx,7
 lea rcx,[rsp+64]
 call pg_oauth_get_json@PLT
 test eax,eax
 jnz .Cp_refresh_end
 mov rdi,[rsp+64]
 call pg_oauth_json_parse@PLT
 mov r14,rax
 test rax,rax
 jz .Cp_refresh_end
 mov rdi,rax
 mov esi,[rsp+72]
 lea rdx,[rsp+136]
 mov qword ptr [rsp+136],0
 call pg_copilot_catalog
 mov r15d,eax
 mov rdi,r14
 call json_object_put@PLT
 test r15d,r15d
 jnz .Cp_refresh_end
 mov rdi,[rsp+40]
 lea rsi,[rip+.CpK_availableModelIds]
 mov rdx,[rsp+136]
 mov qword ptr [rsp+136],0
 call .Cp_add
 test eax,eax
 jnz .Cp_refresh_end
 mov rax,[rsp+40]
 mov [r13],rax
 mov qword ptr [rsp+40],0
 xor ebp,ebp
.Cp_refresh_end:
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov rdi,[rsp+16]
 call free@PLT
 mov rdi,[rsp+24]
 call free@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call free@PLT
 mov rdi,[rsp+64]
 call free@PLT
 mov eax,ebp
 add rsp,152
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbp
 pop rbx
 ret
.size pg_oauth_copilot_refresh,.-pg_oauth_copilot_refresh
.globl pg_copilot_catalog
.type pg_copilot_catalog,@function
pg_copilot_catalog:
 push rbx
 push rbp
 push r12
 push r13
 push r14
 push r15
 sub rsp,40
 mov ebp,esi
 mov r13,rdx
 mov qword ptr [rdx],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 lea rsi,[rip+.CpK_data]
 call json_object_object_get@PLT
 mov r12,rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,5
 jne .Cc_bad
 mov rdi,r12
 call json_object_array_length@PLT
 mov r14,rax
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Cc_bad
 call json_object_new_array@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Cc_bad
 xor r15d,r15d
.Cc_each:
 cmp r15,r14
 jae .Cc_select
 mov rdi,r12
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov rbx,rax
 CPGET rbx,id
 mov [rsp+16],rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,6
 jne .Cc_next
 CPGET rbx,capabilities
 CPGET rax,supports
 CPGET rax,tool_calls
 mov [rsp+24],rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,1
 jne .Cc_policy
 mov rdi,[rsp+24]
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Cc_next
.Cc_policy:
 CPGET rbx,policy
 CPGET rax,state
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Cc_picker
 mov rdi,rax
 lea rsi,[rip+.Cp_enabled]
 call strcmp@PLT
 test eax,eax
 jnz .Cc_picker
 mov rdi,[rsp+8]
 mov rsi,[rsp+16]
 call .Cp_push_id
 test eax,eax
 jnz .Cc_bad
.Cc_picker:
 CPGET rbx,model_picker_enabled
 mov [rsp+32],rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,1
 jne .Cc_next
 mov rdi,[rsp+32]
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Cc_next
 mov rdi,[rsp+24]
 test rdi,rdi
 jz .Cc_picker_add
 lea rsi,[rip+.Cp_disabled]
 call strcmp@PLT
 test eax,eax
 jz .Cc_next
.Cc_picker_add:
 mov rdi,[rsp]
 mov rsi,[rsp+16]
 call .Cp_push_id
 test eax,eax
 jnz .Cc_bad
.Cc_next:
 inc r15
 jmp .Cc_each
.Cc_select:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 test rax,rax
 jnz .Cc_take_picker
 test ebp,ebp
 jz .Cc_take_picker
 mov rax,[rsp+8]
 mov qword ptr [rsp+8],0
 jmp .Cc_take
.Cc_take_picker:
 mov rax,[rsp]
 mov qword ptr [rsp],0
.Cc_take:
 mov [r13],rax
 xor ebx,ebx
 jmp .Cc_end
.Cc_bad:
 mov ebx,7
.Cc_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebx
 add rsp,40
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbp
 pop rbx
 ret
.size pg_copilot_catalog,.-pg_copilot_catalog
.Cp_push_id:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsi
 call json_object_get_string_len@PLT
 mov [rsp],eax
 mov rdi,r12
 call json_object_get_string@PLT
 mov rdi,rax
 mov esi,[rsp]
 call json_object_new_string_len@PLT
 mov r12,rax
 test rax,rax
 jz .Cp_push_bad
 mov rdi,rbx
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz .Cp_push_end
 mov rdi,r12
 call json_object_put@PLT
.Cp_push_bad:
 mov eax,7
.Cp_push_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.Cp_add_string:
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
 call .Cp_add
 add rsp,8
 pop r12
 pop rbx
 ret
.Cp_add:
 push rbx
 mov rbx,rdx
 test rdx,rdx
 jz .Cp_add_bad
 call json_object_object_add@PLT
 test eax,eax
 jz .Cp_add_end
 mov rdi,rbx
 call json_object_put@PLT
.Cp_add_bad:
 mov eax,7
.Cp_add_end:
 pop rbx
 ret
.section .rodata
.Cp_identity: .asciz "github-copilot"
.Cp_proxy: .asciz "proxy-ep="
.Cp_proxy_prefix: .asciz "proxy."
.Cp_api_format: .asciz "https://api.%s"
.Cp_https_format: .asciz "https://%s"
.Cp_enterprise_base: .asciz "https://copilot-api.%s"
.Cp_token_format: .asciz "https://api.%s/copilot_internal/v2/token"
.Cp_individual: .asciz "https://api.individual.githubcopilot.com"
.Cp_github: .asciz "github.com"
.Cp_scheme: .asciz "://"
.Cp_models_format: .asciz "%s/models"
.Cp_bearer_format: .asciz "Authorization: Bearer %s"
.Cp_accept: .asciz "Accept: application/json"
.Cp_user_agent: .asciz "User-Agent: GitHubCopilotChat/0.35.0"
.Cp_editor: .asciz "Editor-Version: vscode/1.107.0"
.Cp_plugin: .asciz "Editor-Plugin-Version: copilot-chat/0.35.0"
.Cp_integration: .asciz "Copilot-Integration-Id: vscode-chat"
.Cp_api_version: .asciz "X-GitHub-Api-Version: 2026-06-01"
.Cp_template: .asciz "{\"type\":\"oauth\"}"
.CpK_access: .asciz "access"
.CpK_refresh: .asciz "refresh"
.CpK_token: .asciz "token"
.CpK_expires: .asciz "expires"
.CpK_expires_at: .asciz "expires_at"
.CpK_enterpriseUrl: .asciz "enterpriseUrl"
.CpK_availableModelIds: .asciz "availableModelIds"
.CpK_data: .asciz "data"
.CpK_id: .asciz "id"
.CpK_capabilities: .asciz "capabilities"
.CpK_supports: .asciz "supports"
.CpK_tool_calls: .asciz "tool_calls"
.CpK_policy: .asciz "policy"
.CpK_state: .asciz "state"
.CpK_model_picker_enabled: .asciz "model_picker_enabled"
.Cp_enabled: .asciz "enabled"
.Cp_disabled: .asciz "disabled"
.Cp_thousand: .double 1000
.Cp_margin: .double 300000
.Cp_fifteen_seconds: .double 15000
.Cp_five_seconds: .double 5000
.section .data.rel.ro
.Cp_default: .quad 0,0,0
.section .note.GNU-stack,"",@progbits
.text
/* A malformed availability list leaves the provider's catalog unfiltered. */
.Cp_available:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 lea rsi,[rip+.CpK_type]
 call pg_oauth_string_field@PLT
 test rax,rax
 jz .Ca_unknown
 mov rdi,rax
 lea rsi,[rip+.Cp_oauth]
 call strcmp@PLT
 test eax,eax
 jnz .Ca_unknown
 mov rdi,rbx
 lea rsi,[rip+.CpK_availableModelIds]
 call json_object_object_get@PLT
 mov rbx,rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,5
 jne .Ca_unknown
 mov rdi,rbx
 call json_object_array_length@PLT
 mov r12,rax
 xor r13d,r13d
.Ca_each:
 cmp r13,r12
 jae .Ca_valid
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,6
 jne .Ca_unknown
 inc r13
 jmp .Ca_each
.Ca_valid:
 mov rax,rbx
 jmp .Ca_end
.Ca_unknown:
 xor eax,eax
.Ca_end:
 pop r13
 pop r12
 pop rbx
 ret
.Cp_contains:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 call json_object_array_length@PLT
 mov r13,rax
 xor r14d,r14d
.Ca_contains_each:
 cmp r14,r13
 jae .Ca_contains_missing
 mov rdi,rbx
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov rsi,r12
 call json_object_equal@PLT
 test eax,eax
 jnz .Ca_contains_found
 inc r14
 jmp .Ca_contains_each
.Ca_contains_missing:
 xor eax,eax
 jmp .Ca_contains_end
.Ca_contains_found:
 mov eax,1
.Ca_contains_end:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.globl pg_copilot_model_allowed
.type pg_copilot_model_allowed,@function
pg_copilot_model_allowed:
 push rbx
 push r12
 push r13
 mov r12,rsi
 call .Cp_available
 mov rbx,rax
 test rax,rax
 jz .Ca_model_any
 mov rdi,r12
 call json_object_new_string@PLT
 mov r12,rax
 test rax,rax
 jz .Ca_model_no
 mov rdi,rbx
 mov rsi,rax
 call .Cp_contains
 mov r13d,eax
 mov rdi,r12
 call json_object_put@PLT
 mov eax,r13d
 jmp .Ca_model_end
.Ca_model_any:
 mov eax,1
 jmp .Ca_model_end
.Ca_model_no:
 xor eax,eax
.Ca_model_end:
 pop r13
 pop r12
 pop rbx
 ret
.size pg_copilot_model_allowed,.-pg_copilot_model_allowed
.globl pg_copilot_filter_catalog
.type pg_copilot_filter_catalog,@function
pg_copilot_filter_catalog:
 push rbx
 push rbp
 push r12
 push r13
 push r14
 push r15
 sub rsp,24
 mov rbx,rdi
 mov r13,rdx
 mov qword ptr [rdx],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov rdi,rsi
 call .Cp_available
 mov r12,rax
 test rax,rax
 jnz .Cf_filtered
 mov rdi,rbx
 call json_object_get@PLT
 mov [r13],rax
 xor ebx,ebx
 jmp .Cf_end
.Cf_filtered:
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Cf_bad
 mov rdi,rbx
 call json_object_array_length@PLT
 mov r14,rax
 xor r15d,r15d
.Cf_each:
 cmp r15,r14
 jae .Cf_done
 mov rdi,rbx
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov rbp,rax
 test r12,r12
 jz .Cf_copy
 CPGET rbp,provider
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Cf_copy
 mov rdi,rax
 lea rsi,[rip+.Cp_identity]
 call strcmp@PLT
 test eax,eax
 jnz .Cf_copy
 CPGET rbp,id
 mov rdi,r12
 mov rsi,rax
 call .Cp_contains
 test eax,eax
 jz .Cf_next
.Cf_copy:
 mov rdi,rbp
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Cf_bad
 mov rdi,[rsp]
 mov rsi,[rsp+8]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Cf_bad
 mov qword ptr [rsp+8],0
.Cf_next:
 inc r15
 jmp .Cf_each
.Cf_done:
 mov rax,[rsp]
 mov [r13],rax
 mov qword ptr [rsp],0
 xor ebx,ebx
 jmp .Cf_end
.Cf_bad:
 mov ebx,7
.Cf_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebx
 add rsp,24
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbp
 pop rbx
 ret
.size pg_copilot_filter_catalog,.-pg_copilot_filter_catalog
.section .rodata
.CpK_provider: .asciz "provider"
.section .note.GNU-stack,"",@progbits

.section .rodata
.CpK_type: .asciz "type"
.Cp_oauth: .asciz "oauth"
.section .note.GNU-stack,"",@progbits
