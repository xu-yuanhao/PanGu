/* Pi models.ts/provider-composer.ts and Durable compactionPolicy native adaptation.
   MIT source: earendil-works/pi d5b3de22d0e24a3e9c38a9410d3329b099717283. */
/* MIT License

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
.macro MF bytes
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,\bytes+8
.endm
.macro ME bytes
 add rsp,\bytes+8
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.macro MG object,key
 mov rdi,\object
 lea rsi,[rip+.Lm_\key]
 call json_object_object_get@PLT
.endm
.text
.Lm_set:
 test rdx,rdx
 jz .Lm_set_bad
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Lm_set_end
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,5
.Lm_set_end:
 pop rbx
 ret
.Lm_set_bad:
 mov eax,5
 ret
.Lm_copy:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov rdi,rdx
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lm_copy_bad
 mov rdi,rbx
 mov rsi,r12
 mov rdx,[rsp]
 call .Lm_set
 jmp .Lm_copy_end
.Lm_copy_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,5
.Lm_copy_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.globl pg_models_load
.type pg_models_load,@function
pg_models_load:
 test rsi,rsi
 jz .Lm_invalid_output
 MF 48
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [r12],0
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 mov qword ptr [rsp+32],0
 mov rdi,rbx
 xor esi,esi
 mov rdx,rsp
 call pg_settings_load@PLT
 test eax,eax
 jnz .Lm_load_error
 lea rdi,[rip+pg_models_builtin_json]
 call json_tokener_parse@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lm_load_error
 MG "qword ptr [rsp]",global
 MG rax,providers
 mov rsi,rax
 mov rdi,[rsp+8]
 lea rdx,[rsp+16]
 call pg_settings_merge@PLT
 test eax,eax
 jnz .Lm_load_error
 call json_object_new_object@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lm_load_error
 mov rdi,rax
 lea rsi,[rip+.Lm_providers]
 mov rdx,[rsp+16]
 mov qword ptr [rsp+16],0
 call .Lm_set
 test eax,eax
 jnz .Lm_load_error
 MG "qword ptr [rsp]",diagnostics
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lm_diagnostics]
 mov rdx,rax
 call .Lm_copy
 test eax,eax
 jnz .Lm_load_error
 mov rax,[rsp+24]
 mov [r12],rax
 mov qword ptr [rsp+24],0
 xor ebx,ebx
 jmp .Lm_load_end
.Lm_load_error:
 mov ebx,5
.Lm_load_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov eax,ebx
 ME 48
/* Endpoint is the selected base URL, optionally followed by /chat/completions. */
.Lm_endpoint_matches:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 test rdi,rdi
 jz .Lm_endpoint_no
 test rsi,rsi
 jz .Lm_endpoint_no
 mov rdi,rsi
 call strlen@PLT
 mov rdx,rax
.Lm_endpoint_trim:
 test rdx,rdx
 jz .Lm_endpoint_no
 cmp byte ptr [r12+rdx-1],'/'
 jne .Lm_endpoint_compare
 dec rdx
 jmp .Lm_endpoint_trim
.Lm_endpoint_compare:
 mov rdi,rbx
 mov rsi,r12
 mov [rsp],rdx
 call strncmp@PLT
 test eax,eax
 jnz .Lm_endpoint_no
 mov rax,[rsp]
 lea rdi,[rbx+rax]
 cmp byte ptr [rdi],0
 je .Lm_endpoint_yes
 lea rsi,[rip+.Lm_chat_suffix]
 call strcmp@PLT
 test eax,eax
 jnz .Lm_endpoint_no
.Lm_endpoint_yes:
 mov eax,1
 jmp .Lm_endpoint_end
.Lm_endpoint_no:
 xor eax,eax
.Lm_endpoint_end:
 add rsp,8
 pop r12
 pop rbx
 ret
/* Numeric metadata is public, finite, positive and safe in persisted JSON. */
.Lm_positive:
 push rbx
 mov rbx,rdi
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jnz .Lm_positive_value
 mov rdi,rbx
 mov esi,2
 call json_object_is_type@PLT
 test eax,eax
 jz .Lm_positive_end
.Lm_positive_value:
 mov rdi,rbx
 call json_object_get_double@PLT
 xorpd xmm1,xmm1
 ucomisd xmm0,xmm1
 jbe .Lm_positive_no
 jp .Lm_positive_no
 ucomisd xmm0,qword ptr [rip+.Lm_safe_limit]
 ja .Lm_positive_no
 mov eax,1
 jmp .Lm_positive_end
.Lm_positive_no:
 xor eax,eax
.Lm_positive_end:
 pop rbx
 ret
.Lm_string:
 push rbx
 mov rbx,rdi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lm_string_none
 mov rdi,rbx
 call json_object_get_string@PLT
 cmp byte ptr [rax],0
 jne .Lm_string_end
.Lm_string_none:
 xor eax,eax
.Lm_string_end:
 pop rbx
 ret
.globl pg_models_describe
.type pg_models_describe,@function
pg_models_describe:
 test rdx,rdx
 jz .Lm_invalid_output
 MF 128
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r13],0
 mov qword ptr [rsp],0
 MG rdi,providers
 mov rbx,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lm_describe_unknown
 MG r12,model
 mov [rsp+88],rax
 mov rdi,rax
 call .Lm_string
 mov [rsp+32],rax
 test rax,rax
 jz .Lm_describe_error
 MG r12,modelProvider
 mov rdi,rax
 call .Lm_string
 mov [rsp+40],rax
 MG r12,endpoint
 mov rdi,rax
 call .Lm_string
 mov [rsp+48],rax
 mov rdi,rbx
 call json_object_iter_begin@PLT
 mov [rsp+112],rax
 mov rdi,rbx
 call json_object_iter_end@PLT
 mov [rsp+120],rax
.Lm_describe_provider:
 lea rdi,[rsp+112]
 lea rsi,[rsp+120]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lm_describe_unknown
 lea rdi,[rsp+112]
 call json_object_iter_peek_name@PLT
 mov [rsp+56],rax
 cmp qword ptr [rsp+40],0
 je .Lm_describe_candidate
 mov rdi,rax
 mov rsi,[rsp+40]
 call strcmp@PLT
 test eax,eax
 jnz .Lm_describe_next
.Lm_describe_candidate:
 lea rdi,[rsp+112]
 call json_object_iter_peek_value@PLT
 mov [rsp+64],rax
 mov rdi,rax
 mov rsi,[rsp+32]
 call json_object_object_get@PLT
 mov [rsp+8],rax
 mov qword ptr [rsp+16],0
 MG "qword ptr [rsp+64]",models
 test rax,rax
 jz .Lm_describe_custom_ready
 mov r14,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lm_describe_error
 xor r15d,r15d
.Lm_describe_custom:
 mov rdi,r14
 call json_object_array_length@PLT
 cmp r15,rax
 jae .Lm_describe_custom_ready
 mov rdi,r14
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov rbp,rax
 MG rax,id
 mov rdi,rax
 call .Lm_string
 test rax,rax
 jz .Lm_describe_error
 mov rdi,rax
 mov rsi,[rsp+32]
 call strcmp@PLT
 test eax,eax
 jz .Lm_describe_custom_found
 inc r15
 jmp .Lm_describe_custom
.Lm_describe_custom_found:
 mov [rsp+16],rbp
.Lm_describe_custom_ready:
 mov rax,[rsp+8]
 or rax,[rsp+16]
 jz .Lm_describe_next
 MG "qword ptr [rsp+64]",modelOverrides
 mov rdi,rax
 mov rsi,[rsp+32]
 call json_object_object_get@PLT
 mov [rsp+24],rax
 MG "qword ptr [rsp+16]",baseUrl
 test rax,rax
 jnz .Lm_describe_base_ready
 MG "qword ptr [rsp+64]",baseUrl
 test rax,rax
 jnz .Lm_describe_base_ready
 MG "qword ptr [rsp+8]",baseUrl
.Lm_describe_base_ready:
 mov [rsp+72],rax
 mov rdi,rax
 call .Lm_string
 test rax,rax
 jz .Lm_describe_error
 cmp byte ptr [rax],0
 je .Lm_describe_error
 cmp qword ptr [rsp+40],0
 jne .Lm_describe_api
 mov rdi,[rsp+48]
 mov rsi,rax
 call .Lm_endpoint_matches
 test eax,eax
 jz .Lm_describe_next
.Lm_describe_api:
 MG "qword ptr [rsp+16]",api
 test rax,rax
 jnz .Lm_describe_api_ready
 MG "qword ptr [rsp+64]",api
 test rax,rax
 jnz .Lm_describe_api_ready
 MG "qword ptr [rsp+8]",api
.Lm_describe_api_ready:
 mov [rsp+80],rax
 mov rdi,rax
 call .Lm_string
 test rax,rax
 jz .Lm_describe_error
 cmp byte ptr [rax],0
 je .Lm_describe_error
 lea rdi,[rip+.Lm_metadata_defaults]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lm_describe_error
 mov rdi,rax
 lea rsi,[rip+.Lm_id]
 mov rdx,[rsp+88]
 call .Lm_copy
 test eax,eax
 jnz .Lm_describe_error
 mov rdi,[rsp+56]
 call json_object_new_string@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lm_provider]
 mov rdx,rax
 call .Lm_set
 test eax,eax
 jnz .Lm_describe_error
 mov rdi,[rsp]
 lea rsi,[rip+.Lm_api]
 mov rdx,[rsp+80]
 call .Lm_copy
 test eax,eax
 jnz .Lm_describe_error
 xor ebp,ebp
.Lm_describe_number:
 lea rax,[rip+.Lm_number_keys]
 movsxd r14,dword ptr [rax+rbp*4]
 add r14,rax
 mov rdi,[rsp+24]
 mov rsi,r14
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lm_describe_number_found
 mov rdi,[rsp+16]
 test rdi,rdi
 jnz .Lm_describe_number_source
 mov rdi,[rsp+8]
.Lm_describe_number_source:
 mov rsi,r14
 call json_object_object_get@PLT
 test rax,rax
 jz .Lm_describe_number_next
.Lm_describe_number_found:
 mov r15,rax
 mov rdi,rax
 call .Lm_positive
 test eax,eax
 jz .Lm_describe_error
 mov rdi,[rsp]
 mov rsi,r14
 mov rdx,r15
 call .Lm_copy
 test eax,eax
 jnz .Lm_describe_error
.Lm_describe_number_next:
 inc ebp
 cmp ebp,2
 jb .Lm_describe_number
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lm_input]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lm_input_found
 mov rdi,[rsp+16]
 test rdi,rdi
 jnz .Lm_input_source
 mov rdi,[rsp+8]
.Lm_input_source:
 lea rsi,[rip+.Lm_input]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lm_describe_compat
.Lm_input_found:
 mov r15,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lm_describe_error
 xor ebp,ebp
.Lm_input_each:
 mov rdi,r15
 call json_object_array_length@PLT
 cmp rbp,rax
 jae .Lm_input_copy
 mov rdi,r15
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call .Lm_string
 test rax,rax
 jz .Lm_describe_error
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lm_input_text]
 call strcmp@PLT
 test eax,eax
 jz .Lm_input_next
 mov rdi,r14
 lea rsi,[rip+.Lm_input_image]
 call strcmp@PLT
 test eax,eax
 jnz .Lm_describe_error
.Lm_input_next:
 inc rbp
 jmp .Lm_input_each
.Lm_input_copy:
 mov rdi,[rsp]
 lea rsi,[rip+.Lm_input]
 mov rdx,r15
 call .Lm_copy
 test eax,eax
 jnz .Lm_describe_error
.Lm_describe_compat:
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lm_input_limits]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lm_input_limits_found
 mov rdi,[rsp+16]
 test rdi,rdi
 jnz .Lm_input_limits_source
 mov rdi,[rsp+8]
.Lm_input_limits_source:
 lea rsi,[rip+.Lm_input_limits]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lm_input_limits_done
.Lm_input_limits_found:
 mov r15,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lm_describe_error
 mov rdi,[rsp]
 lea rsi,[rip+.Lm_input_limits]
 mov rdx,r15
 call .Lm_copy
 test eax,eax
 jnz .Lm_describe_error
.Lm_input_limits_done:
 xor ebp,ebp
.Lm_compat_each:
 mov rdi,[rsp+8]
 cmp ebp,1
 jne .Lm_compat_custom
 mov rdi,[rsp+64]
.Lm_compat_custom:
 cmp ebp,2
 jne .Lm_compat_override
 mov rdi,[rsp+16]
.Lm_compat_override:
 cmp ebp,3
 jne .Lm_compat_get
 mov rdi,[rsp+24]
.Lm_compat_get:
 lea rsi,[rip+.Lm_compat]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lm_compat_next
 mov r15,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lm_describe_error
 mov rdi,[rsp]
 lea rsi,[rip+.Lm_compat]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,r15
 mov qword ptr [rsp+96],0
 lea rdx,[rsp+96]
 call pg_settings_merge@PLT
 test eax,eax
 jnz .Lm_describe_error
 mov rdi,[rsp]
 lea rsi,[rip+.Lm_compat]
 mov rdx,[rsp+96]
 call .Lm_set
 test eax,eax
 jnz .Lm_describe_error
.Lm_compat_next:
 inc ebp
 cmp ebp,4
 jb .Lm_compat_each
 jmp .Lm_describe_success
.Lm_describe_next:
 lea rdi,[rsp+112]
 call json_object_iter_next@PLT
 jmp .Lm_describe_provider
.Lm_describe_unknown:
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lm_describe_error
.Lm_describe_success:
 mov rax,[rsp]
 mov [r13],rax
 xor eax,eax
 ME 128
.Lm_describe_error:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,5
 ME 128
.globl pg_models_compaction_policy
.type pg_models_compaction_policy,@function
pg_models_compaction_policy:
 test rsi,rsi
 jz .Lm_invalid_output
 MF 64
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [r12],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 MG rbx,compaction
 mov r13,rax
 MG rbx,modelInfo
 MG rax,provider
 test rax,rax
 jnz .Lm_policy_provider
 MG rbx,modelProvider
.Lm_policy_provider:
 mov rdi,rax
 call .Lm_string
 mov r14,rax
 MG rbx,model
 mov rdi,rax
 call .Lm_string
 test rax,rax
 jz .Lm_policy_defaults
 test r14,r14
 jz .Lm_policy_defaults
 mov rcx,rax
 mov rdx,r14
 lea rdi,[rsp+8]
 lea rsi,[rip+.Lm_model_key]
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lm_policy_error
 MG r13,modelOverrides
 mov rdi,rax
 mov rsi,[rsp+8]
 call json_object_object_get@PLT
 mov [rsp+16],rax
.Lm_policy_defaults:
 lea rdi,[rip+.Lm_policy_defaults_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lm_policy_error
 xor ebx,ebx
.Lm_policy_field:
 lea rax,[rip+.Lm_policy_keys]
 movsxd r14,dword ptr [rax+rbx*4]
 add r14,rax
 mov rdi,[rsp+16]
 mov rsi,r14
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lm_policy_copy
 mov rdi,r13
 mov rsi,r14
 call json_object_object_get@PLT
 test rax,rax
 jz .Lm_policy_next
.Lm_policy_copy:
 mov r15,rax
 test ebx,ebx
 jnz .Lm_policy_tokens
 mov rdi,rax
 mov esi,1
 call json_object_is_type@PLT
 test eax,eax
 jz .Lm_policy_error
 jmp .Lm_policy_set
.Lm_policy_tokens:
 mov rdi,r15
 call .Lm_positive
 test eax,eax
 jnz .Lm_policy_set
 mov rdi,r15
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lm_policy_error
 mov rdi,r15
 call json_object_get_int64@PLT
 test rax,rax
 jnz .Lm_policy_error
.Lm_policy_set:
 mov rdi,[rsp]
 mov rsi,r14
 mov rdx,r15
 call .Lm_copy
 test eax,eax
 jnz .Lm_policy_error
.Lm_policy_next:
 inc ebx
 cmp ebx,4
 jb .Lm_policy_field
 mov rax,[rsp]
 mov [r12],rax
 mov qword ptr [rsp],0
 xor ebx,ebx
 jmp .Lm_policy_end
.Lm_policy_error:
 mov ebx,5
.Lm_policy_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov eax,ebx
 ME 64
.section .rodata
.Lm_global: .asciz "global"
.Lm_providers: .asciz "providers"
.Lm_diagnostics: .asciz "diagnostics"
.Lm_model: .asciz "model"
.Lm_modelProvider: .asciz "modelProvider"
.Lm_endpoint: .asciz "endpoint"
.Lm_models: .asciz "models"
.Lm_id: .asciz "id"
.Lm_modelOverrides: .asciz "modelOverrides"
.Lm_baseUrl: .asciz "baseUrl"
.Lm_api: .asciz "api"
.Lm_provider: .asciz "provider"
.Lm_modelInfo: .asciz "modelInfo"
.Lm_compaction: .asciz "compaction"
.Lm_contextWindow: .asciz "contextWindow"
.Lm_maxTokens: .asciz "maxTokens"
.Lm_chat_suffix: .asciz "/chat/completions"
.Lm_model_key: .asciz "%s/%s"
.Lm_metadata_defaults: .asciz "{\"contextWindow\":128000,\"maxTokens\":16384}"
.Lm_policy_defaults_json: .asciz "{\"enabled\":true,\"keepRecentTokens\":20000,\"reserveTokens\":16384,\"backgroundTokens\":32768}"
.Lm_enabled: .asciz "enabled"
.Lm_keepRecentTokens: .asciz "keepRecentTokens"
.Lm_reserveTokens: .asciz "reserveTokens"
.Lm_backgroundTokens: .asciz "backgroundTokens"
.Lm_number_keys: .long .Lm_contextWindow-.Lm_number_keys,.Lm_maxTokens-.Lm_number_keys
.Lm_policy_keys: .long .Lm_enabled-.Lm_policy_keys,.Lm_keepRecentTokens-.Lm_policy_keys,.Lm_reserveTokens-.Lm_policy_keys,.Lm_backgroundTokens-.Lm_policy_keys
.balign 8
.Lm_safe_limit: .double 9007199254740991.0
.section .note.GNU-stack,"",@progbits

.text
.Lm_invalid_output:
 mov eax,5
 ret

.section .rodata
.Lm_input: .asciz "input"
.Lm_input_text: .asciz "text"
.Lm_input_image: .asciz "image"

.Lm_compat: .asciz "compat"

.Lm_input_limits: .asciz "inputLimits"

.text
/* frame: catalog, result, seen, provider, configuration, descriptor. */
.Lml_add:
 MF 16
 mov rbx,rdi
 mov r12,rsi
 mov rdi,[rbx+16]
 mov rsi,r12
 lea rdx,[rsp]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Lml_add_ok
 call json_object_new_object@PLT
 mov [rbx+32],rax
 test rax,rax
 jz .Lml_add_bad
 mov rdi,r12
 call json_object_new_string@PLT
 mov rdx,rax
 mov rdi,[rbx+32]
 lea rsi,[rip+.Lm_model]
 call .Lm_set
 test eax,eax
 jnz .Lml_add_bad
 mov rdi,[rbx+24]
 call json_object_new_string@PLT
 mov rdx,rax
 mov rdi,[rbx+32]
 lea rsi,[rip+.Lm_modelProvider]
 call .Lm_set
 test eax,eax
 jnz .Lml_add_bad
 mov rdi,[rbx]
 mov rsi,[rbx+32]
 lea rdx,[rbx+40]
 call pg_models_describe
 test eax,eax
 jnz .Lml_add_release
 mov rdi,[rbx+40]
 call json_object_object_length@PLT
 test eax,eax
 jz .Lml_add_release
 mov rdi,[rbx+8]
 mov rsi,[rbx+40]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lml_add_bad
 mov qword ptr [rbx+40],0
 mov edi,1
 call json_object_new_boolean@PLT
 mov rdx,rax
 mov rdi,[rbx+16]
 mov rsi,r12
 call .Lm_set
 test eax,eax
 jnz .Lml_add_bad
.Lml_add_release:
 mov rdi,[rbx+32]
 call json_object_put@PLT
 mov qword ptr [rbx+32],0
 mov rdi,[rbx+40]
 call json_object_put@PLT
 mov qword ptr [rbx+40],0
.Lml_add_ok:
 xor eax,eax
 ME 16
.Lml_add_bad:
 mov eax,5
 ME 16
.globl pg_models_list
.type pg_models_list,@function
pg_models_list:
 MF 128
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rsp
 xor eax,eax
 mov ecx,16
 rep stosq
 mov [rsp],rbx
 test r13,r13
 jz .Lml_invalid
 mov qword ptr [r13],0
 call json_object_new_array@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lml_bad
 MG rbx,providers
 mov r14,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lml_success
 mov rdi,r14
 call json_object_iter_begin@PLT
 mov [rsp+48],rax
 mov rdi,r14
 call json_object_iter_end@PLT
 mov [rsp+56],rax
.Lml_provider:
 lea rdi,[rsp+48]
 lea rsi,[rsp+56]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lml_success
 lea rdi,[rsp+48]
 call json_object_iter_peek_name@PLT
 mov [rsp+24],rax
 test r12,r12
 jz .Lml_provider_selected
 mov rdi,rax
 mov rsi,r12
 call strcmp@PLT
 test eax,eax
 jnz .Lml_next_provider
.Lml_provider_selected:
 lea rdi,[rsp+48]
 call json_object_iter_peek_value@PLT
 mov r14,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lml_next_provider
 call json_object_new_object@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lml_bad
 mov rdi,r14
 call json_object_iter_begin@PLT
 mov [rsp+64],rax
 mov rdi,r14
 call json_object_iter_end@PLT
 mov [rsp+72],rax
.Lml_builtin:
 lea rdi,[rsp+64]
 lea rsi,[rsp+72]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lml_custom_begin
 lea rdi,[rsp+64]
 call json_object_iter_peek_value@PLT
 mov rdi,rax
 lea rsi,[rip+.Lm_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call .Lm_string
 test rax,rax
 jz .Lml_builtin_next
 mov rdi,rsp
 mov rsi,rax
 call .Lml_add
 test eax,eax
 jnz .Lml_bad
.Lml_builtin_next:
 lea rdi,[rsp+64]
 call json_object_iter_next@PLT
 jmp .Lml_builtin
.Lml_custom_begin:
 MG r14,models
 mov r15,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lml_provider_end
 xor ebp,ebp
.Lml_custom:
 mov rdi,r15
 call json_object_array_length@PLT
 cmp rbp,rax
 jae .Lml_provider_end
 mov rdi,r15
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rip+.Lm_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call .Lm_string
 test rax,rax
 jz .Lml_bad
 mov rdi,rsp
 mov rsi,rax
 call .Lml_add
 test eax,eax
 jnz .Lml_bad
 inc rbp
 jmp .Lml_custom
.Lml_provider_end:
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov qword ptr [rsp+16],0
.Lml_next_provider:
 lea rdi,[rsp+48]
 call json_object_iter_next@PLT
 jmp .Lml_provider
.Lml_success:
 mov rax,[rsp+8]
 mov [r13],rax
 mov qword ptr [rsp+8],0
 xor ebp,ebp
 jmp .Lml_end
.Lml_invalid:
 mov ebp,4
 jmp .Lml_end
.Lml_bad:
 mov ebp,5
.Lml_end:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov eax,ebp
 ME 128
.section .note.GNU-stack,"",@progbits
