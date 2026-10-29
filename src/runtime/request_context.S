/* Pi pi-ai api/openai-completions.ts convertMessages native adaptation.
   Source d5b3de22d0e24a3e9c38a9410d3329b099717283; MIT. */
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
.macro RF bytes
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,\bytes+8
.endm
.macro RE bytes
 add rsp,\bytes+8
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.macro RG object,key
 mov rdi,\object
 lea rsi,[rip+.Lr_\key]
 call json_object_object_get@PLT
.endm
.text
.Lr_set:
 test rdx,rdx
 jz .Lr_set_bad
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Lr_set_end
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,5
.Lr_set_end:
 pop rbx
 ret
.Lr_set_bad:
 mov eax,5
 ret
.Lr_copy:
 RF 32
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov rdi,rdx
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lr_copy_bad
 mov rdi,rbx
 mov rsi,r12
 mov rdx,[rsp]
 call .Lr_set
 RE 32
.Lr_copy_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,5
 RE 32
.Lr_string:
 push rbx
 mov rbx,rdi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lr_string_end
 mov rdi,rbx
 call json_object_get_string@PLT
.Lr_string_end:
 pop rbx
 ret
.globl pg_context_append
.type pg_context_append,@function
pg_context_append:
 RF 32
 mov rbx,rdi
 mov r12,rsi
 mov r13d,edx
 mov qword ptr [rsp],0
 mov ebp,5
 cmp edx,2
 ja .Lr_append_end
 mov rdi,rsi
 call .Lr_string
 test rax,rax
 jz .Lr_append_end
 mov rdi,r12
 call json_object_get_string_len@PLT
 mov r14,rax
 test eax,eax
 jz .Lr_append_ok
 mov rdi,[rbx]
 call json_object_get_string_len@PLT
 mov r15,rax
 test rax,rax
 jnz .Lr_append_size
 xor r13d,r13d
.Lr_append_size:
 lea rdi,[r15+r14]
 add rdi,r13
 cmp rdi,2147483647
 ja .Lr_append_end
 mov [rsp+8],rdi
 inc rdi
 call malloc@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lr_append_end
 test r15,r15
 jz .Lr_append_new
 mov rdi,[rbx]
 call json_object_get_string@PLT
 mov rsi,rax
 mov rdi,[rsp]
 mov rdx,r15
 call memcpy@PLT
.Lr_append_new:
 xor eax,eax
.Lr_append_separator:
 cmp rax,r13
 jae .Lr_append_copy
 mov rcx,[rsp]
 add rcx,r15
 mov byte ptr [rcx+rax],10
 inc rax
 jmp .Lr_append_separator
.Lr_append_copy:
 mov rdi,r12
 call json_object_get_string@PLT
 mov rsi,rax
 mov rdi,[rsp]
 add rdi,r15
 add rdi,r13
 mov rdx,r14
 call memcpy@PLT
 mov rax,[rsp]
 mov rcx,[rsp+8]
 mov byte ptr [rax+rcx],0
 mov rdi,rax
 mov esi,ecx
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Lr_append_end
 mov r12,rax
 mov rdi,[rbx]
 call json_object_put@PLT
 mov [rbx],r12
.Lr_append_ok:
 xor ebp,ebp
.Lr_append_end:
 mov rdi,[rsp]
 call free@PLT
 mov eax,ebp
 RE 32
.globl pg_context_text
.type pg_context_text,@function
pg_context_text:
 RF 32
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov rdi,rbx
 call .Lr_string
 test rax,rax
 jz .Lr_text_array
 mov r12,rax
 mov rdi,rbx
 call json_object_get_string_len@PLT
 mov rdi,r12
 mov esi,eax
 call json_object_new_string_len@PLT
 RE 32
.Lr_text_array:
 mov rdi,rbx
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lr_text_bad
 lea rdi,[rip+.Lr_empty]
 call json_object_new_string@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lr_text_bad
 xor r12d,r12d
.Lr_text_block:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lr_text_done
 mov rdi,rbx
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov r13,rax
 RG rax,type
 mov rdi,rax
 call .Lr_string
 test rax,rax
 jz .Lr_text_bad
 mov rdi,rax
 lea rsi,[rip+.Lr_text]
 call strcmp@PLT
 test eax,eax
 jz .Lr_text_accept
 mov rdi,r13
 call .Lr_image_part
 test rax,rax
 jz .Lr_text_bad
 mov rdi,rax
 call json_object_put@PLT
 jmp .Lr_text_next
.Lr_text_accept:
 RG r13,text
 lea rdi,[rsp]
 mov rsi,rax
 mov edx,1
 call pg_context_append
 test eax,eax
 jnz .Lr_text_bad
.Lr_text_next:
 inc r12
 jmp .Lr_text_block
.Lr_text_done:
 mov rax,[rsp]
 RE 32
.Lr_text_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 xor eax,eax
 RE 32
/* Append a display row; its independent strings survive the canonical view. */
.Lr_history:
 test rdi,rdi
 jz .Lr_history_skip
 RF 64
 mov rbx,rdi
 mov r12d,esi
 mov r13,rdx
 mov r14,rcx
 mov r15,r8
 mov [rsp+8],r9d
 mov qword ptr [rsp],0
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lr_history_bad
 mov edi,r12d
 call json_object_new_int@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lr_kind]
 mov rdx,rax
 call .Lr_set
 test eax,eax
 jnz .Lr_history_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lr_text]
 mov rdx,r13
 call .Lr_copy
 test eax,eax
 jnz .Lr_history_bad
 cmp r12d,2
 jb .Lr_history_ok_field
 mov rdi,[rsp]
 lea rsi,[rip+.Lr_id]
 mov rdx,r14
 call .Lr_copy
 test eax,eax
 jnz .Lr_history_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lr_name]
 mov rdx,r15
 call .Lr_copy
 test eax,eax
 jnz .Lr_history_bad
.Lr_history_ok_field:
 xor edi,edi
 cmp dword ptr [rsp+8],0
 sete dil
 call json_object_new_boolean@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lr_ok]
 mov rdx,rax
 call .Lr_set
 test eax,eax
 jnz .Lr_history_bad
 mov rdi,rbx
 mov rsi,[rsp]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lr_history_bad
 mov qword ptr [rsp],0
 xor ebx,ebx
 jmp .Lr_history_end
.Lr_history_bad:
 mov ebx,5
.Lr_history_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebx
 RE 64
.Lr_history_skip:
 xor eax,eax
 ret
.globl pg_context_messages
.type pg_context_messages,@function
pg_context_messages:
 xor ecx,ecx
 xor r8d,r8d
 xor edx,edx
 jmp .Lr_project
.globl pg_context_project
.type pg_context_project,@function
pg_context_project:
 xor ecx,ecx
 xor r8d,r8d
 mov edx,1
 jmp .Lr_project
.globl pg_context_messages_model
.type pg_context_messages_model,@function
pg_context_messages_model:
 mov rcx,rsi
 mov rsi,rdx
 mov r8d,1
 xor edx,edx
.Lr_project:
 test rdi,rdi
 jz .Lr_invalid_input
 test rsi,rsi
 jz .Lr_invalid_input
 RF 192
 mov rbx,rdi
 mov r12,rsi
 mov r14d,edx
 mov r15,rcx
 mov ebp,r8d
 mov qword ptr [r12],0
 mov rdi,rsp
 xor eax,eax
 mov ecx,24
 rep stosq
 mov [rsp+144],r14d
 mov [rsp+160],r15
 mov [rsp+168],ebp
 mov dword ptr [rsp+176],1
 test ebp,ebp
 jz .Lri_capability_ready
 mov rdi,r15
 call .Lri_capability
 mov [rsp+176],eax
.Lri_capability_ready:
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lr_project_bad
 cmp dword ptr [rsp+144],0
 je .Lr_project_system
 call json_object_new_array@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lr_project_bad
.Lr_project_system:
 RG rbx,systemPrompt
 test rax,rax
 jz .Lr_project_messages
 mov r13,rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lr_project_messages
 lea rdi,[rip+.Lr_system_wire]
 call json_tokener_parse@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lr_project_bad
 mov rdi,rax
 lea rsi,[rip+.Lr_content]
 mov rdx,r13
 call .Lr_copy
 test eax,eax
 jnz .Lr_project_bad
 mov rdi,[rsp]
 mov rsi,[rsp+40]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lr_project_bad
 mov qword ptr [rsp+40],0
.Lr_project_messages:
 RG rbx,messages
 mov [rsp+104],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lr_project_bad
 xor r13d,r13d
.Lr_project_message:
 mov rdi,[rsp+104]
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lr_project_finish
 mov rdi,[rsp+104]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov [rsp+24],rax
 RG rax,role
 mov rdi,rax
 call .Lr_string
 test rax,rax
 jz .Lr_project_bad
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lr_system]
 call strcmp@PLT
 test eax,eax
 jz .Lr_project_next
 mov rdi,r14
 lea rsi,[rip+.Lr_user]
 call strcmp@PLT
 test eax,eax
 jz .Lr_project_user
 mov rdi,r14
 lea rsi,[rip+.Lr_toolResult]
 call strcmp@PLT
 test eax,eax
 jz .Lr_project_result
 mov rdi,r14
 lea rsi,[rip+.Lr_assistant]
 call strcmp@PLT
 test eax,eax
 jnz .Lr_project_bad
 lea rdi,[rip+.Lr_assistant_wire]
 call json_tokener_parse@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lr_project_bad
 RG "qword ptr [rsp+24]",content
 mov [rsp+112],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lr_project_bad
 lea rdi,[rip+.Lr_empty]
 call json_object_new_string@PLT
 mov [rsp+48],rax
 test rax,rax
 jz .Lr_project_bad
 lea rdi,[rip+.Lr_empty]
 call json_object_new_string@PLT
 mov [rsp+56],rax
 test rax,rax
 jz .Lr_project_bad
 call json_object_new_array@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lr_project_bad
 xor r15d,r15d
.Lr_project_block:
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp r15,rax
 jae .Lr_project_assistant_finish
 mov rdi,[rsp+112]
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov r14,rax
 RG rax,type
 mov rdi,rax
 call .Lr_string
 test rax,rax
 jz .Lr_project_bad
 mov rbp,rax
 mov rdi,rax
 lea rsi,[rip+.Lr_text]
 call strcmp@PLT
 test eax,eax
 jnz .Lr_project_thinking
 RG r14,text
 lea rdi,[rsp+48]
 mov rsi,rax
 xor edx,edx
 call pg_context_append
 test eax,eax
 jnz .Lr_project_bad
 jmp .Lr_project_block_next
.Lr_project_thinking:
 mov rdi,rbp
 lea rsi,[rip+.Lr_thinking]
 call strcmp@PLT
 test eax,eax
 jnz .Lr_project_call
 RG r14,thinkingSignature
 test rax,rax
 jz .Lr_project_bad
 mov rdi,rax
 call .Lr_string
 test rax,rax
 jz .Lr_project_bad
 mov rdi,rax
 lea rsi,[rip+.Lr_reasoning_content]
 call strcmp@PLT
 test eax,eax
 jnz .Lr_project_bad
 RG r14,thinking
 lea rdi,[rsp+56]
 mov rsi,rax
 mov edx,1
 call pg_context_append
 test eax,eax
 jnz .Lr_project_bad
 jmp .Lr_project_block_next
.Lr_project_call:
 mov rdi,rbp
 lea rsi,[rip+.Lr_toolCall]
 call strcmp@PLT
 test eax,eax
 jnz .Lr_project_bad
 lea rdi,[rip+.Lr_call_wire]
 call json_tokener_parse@PLT
 mov [rsp+64],rax
 test rax,rax
 jz .Lr_project_bad
 RG r14,id
 mov [rsp+120],rax
 mov rdi,rax
 call .Lr_string
 test rax,rax
 jz .Lr_project_bad
 cmp byte ptr [rax],0
 je .Lr_project_bad
 mov rdi,[rsp+64]
 lea rsi,[rip+.Lr_id]
 mov rdx,[rsp+120]
 call .Lr_copy
 test eax,eax
 jnz .Lr_project_bad
 RG "qword ptr [rsp+64]",function
 mov [rsp+72],rax
 RG r14,name
 mov [rsp+128],rax
 mov rdi,rax
 call .Lr_string
 test rax,rax
 jz .Lr_project_bad
 cmp byte ptr [rax],0
 je .Lr_project_bad
 mov rdi,[rsp+72]
 lea rsi,[rip+.Lr_name]
 mov rdx,[rsp+128]
 call .Lr_copy
 test eax,eax
 jnz .Lr_project_bad
 RG r14,arguments
 test rax,rax
 jz .Lr_project_bad
 mov rdi,rax
 mov esi,256
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Lr_project_bad
 mov rdi,rax
 call json_object_new_string@PLT
 mov [rsp+80],rax
 test rax,rax
 jz .Lr_project_bad
 mov rdi,[rsp+72]
 lea rsi,[rip+.Lr_arguments]
 mov rdx,rax
 call .Lr_copy
 test eax,eax
 jnz .Lr_project_bad
 mov rdi,[rsp+32]
 mov rsi,[rsp+64]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lr_project_bad
 mov qword ptr [rsp+64],0
 mov rdi,[rsp+80]
 call json_object_put@PLT
 mov qword ptr [rsp+80],0
.Lr_project_block_next:
 inc r15
 jmp .Lr_project_block
.Lr_project_assistant_finish:
 mov rdi,[rsp+48]
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lr_project_assistant_think
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lr_content]
 mov rdx,[rsp+48]
 call .Lr_copy
 test eax,eax
 jnz .Lr_project_bad
 mov rdi,[rsp+8]
 mov esi,1
 mov rdx,[rsp+48]
 xor ecx,ecx
 xor r8d,r8d
 xor r9d,r9d
 call .Lr_history
 test eax,eax
 jnz .Lr_project_bad
.Lr_project_assistant_think:
 mov rdi,[rsp+56]
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lr_project_assistant_calls
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lr_reasoning_content]
 mov rdx,[rsp+56]
 call .Lr_copy
 test eax,eax
 jnz .Lr_project_bad
.Lr_project_assistant_calls:
 mov rdi,[rsp+32]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lr_project_assistant_ready
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lr_tool_calls]
 mov rdx,[rsp+32]
 call .Lr_copy
 test eax,eax
 jnz .Lr_project_bad
.Lr_project_assistant_ready:
 xor r15d,r15d
.Lr_project_call_history:
 mov rdi,[rsp+32]
 call json_object_array_length@PLT
 cmp r15,rax
 jae .Lr_project_append
 mov rdi,[rsp+32]
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov r14,rax
 RG rax,function
 mov rbp,rax
 RG rax,arguments
 mov [rsp+120],rax
 RG rbp,name
 mov [rsp+128],rax
 RG r14,id
 mov rdi,[rsp+8]
 mov esi,2
 mov rdx,[rsp+120]
 mov rcx,rax
 mov r8,[rsp+128]
 xor r9d,r9d
 call .Lr_history
 test eax,eax
 jnz .Lr_project_bad
 inc r15
 jmp .Lr_project_call_history
.Lr_project_user:
 lea rdi,[rip+.Lr_user_wire]
 call json_tokener_parse@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lr_project_bad
 RG "qword ptr [rsp+24]",content
 mov [rsp+112],rax
 mov rdi,rax
 call pg_context_text
 mov [rsp+48],rax
 test rax,rax
 jz .Lr_project_bad
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lr_content]
 mov rdx,[rsp+112]
 call .Lr_copy
 test eax,eax
 jnz .Lr_project_bad
 mov rdi,[rsp+8]
 xor esi,esi
 mov rdx,[rsp+48]
 xor ecx,ecx
 xor r8d,r8d
 xor r9d,r9d
 call .Lr_history
 test eax,eax
 jnz .Lr_project_bad
 jmp .Lr_project_append
.Lr_project_result:
 lea rdi,[rip+.Lr_tool_wire]
 call json_tokener_parse@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lr_project_bad
 RG "qword ptr [rsp+24]",content
 mov [rsp+184],rax
 mov rdi,rax
 call pg_context_text
 mov [rsp+48],rax
 test rax,rax
 jz .Lr_project_bad
 mov rdx,rax
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lr_content]
 call .Lr_copy
 test eax,eax
 jnz .Lr_project_bad
 RG "qword ptr [rsp+24]",toolCallId
 mov [rsp+120],rax
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lr_tool_call_id]
 mov rdx,rax
 call .Lr_copy
 test eax,eax
 jnz .Lr_project_bad
 RG "qword ptr [rsp+24]",toolName
 mov [rsp+128],rax
 RG "qword ptr [rsp+24]",isError
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov r9d,eax
 mov rdi,[rsp+8]
 mov esi,3
 mov rdx,[rsp+48]
 mov rcx,[rsp+120]
 mov r8,[rsp+128]
 call .Lr_history
 test eax,eax
 jnz .Lr_project_bad
 mov rdi,[rsp+48]
 call json_object_get_string@PLT
 mov r14,rax
 mov rdi,[rsp+48]
 call json_object_get_string_len@PLT
 mov esi,eax
 mov rdi,r14
 call pg_context_blank
 test eax,eax
 jz .Lr_project_append
 lea rdi,[rip+.Lr_no_output]
 mov [rsp+120],rdi
 mov rdi,[rsp+184]
 call .Lri_has_image
 test eax,eax
 jz .Lri_placeholder_ready
 lea rax,[rip+.Lri_attached]
 mov [rsp+120],rax
.Lri_placeholder_ready:
 mov rdi,[rsp+120]
 call json_object_new_string@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lr_content]
 mov rdx,rax
 call .Lr_set
 test eax,eax
 jnz .Lr_project_bad
.Lr_project_append:
 mov rdi,[rsp]
 mov rsi,[rsp+40]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lr_project_bad
 mov qword ptr [rsp+40],0
 RG "qword ptr [rsp+24]",role
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lr_toolResult]
 call strcmp@PLT
 test eax,eax
 jnz .Lr_project_next
 cmp dword ptr [rsp+176],0
 je .Lr_project_next
 mov rdi,[rsp+184]
 lea rsi,[rsp+152]
 call .Lri_collect
 test eax,eax
 jnz .Lr_project_bad
 lea rsi,[r13+1]
 mov rdi,[rsp+104]
 call json_object_array_get_idx@PLT
 RG rax,role
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lri_group_end
 mov rdi,rax
 lea rsi,[rip+.Lr_toolResult]
 call strcmp@PLT
 test eax,eax
 jz .Lr_project_next
.Lri_group_end:
 cmp qword ptr [rsp+152],0
 je .Lr_project_next
 lea rdi,[rip+.Lri_user_json]
 call json_tokener_parse@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lr_project_bad
 mov rdi,rax
 lea rsi,[rip+.Lr_content]
 mov rdx,[rsp+152]
 call .Lr_set
 test eax,eax
 jnz .Lr_project_bad
 mov qword ptr [rsp+152],0
 mov rdi,[rsp]
 mov rsi,[rsp+40]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lr_project_bad
 mov qword ptr [rsp+40],0
.Lr_project_next:
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov qword ptr [rsp+32],0
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov qword ptr [rsp+48],0
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov qword ptr [rsp+56],0
 inc r13
 jmp .Lr_project_message
.Lr_project_finish:
 call json_object_new_object@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lr_project_bad
 mov rdi,rax
 lea rsi,[rip+.Lr_messages]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lr_set
 test eax,eax
 jnz .Lr_project_bad
 cmp qword ptr [rsp+8],0
 je .Lr_project_finished_rows
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lr_history_key]
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 call .Lr_set
 test eax,eax
 jnz .Lr_project_bad
.Lr_project_finished_rows:
 mov rax,[rsp+16]
 mov [r12],rax
 mov qword ptr [rsp+16],0
 xor ebx,ebx
 jmp .Lr_project_end
.Lr_project_bad:
 mov ebx,5
.Lr_project_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov rdi,[rsp+80]
 call json_object_put@PLT
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov eax,ebx
 RE 192
.section .rodata
.Lr_empty: .asciz ""
.Lr_no_output: .asciz "(no tool output)"
.Lr_type: .asciz "type"
.Lr_text: .asciz "text"
.Lr_kind: .asciz "kind"
.Lr_id: .asciz "id"
.Lr_name: .asciz "name"
.Lr_ok: .asciz "ok"
.Lr_role: .asciz "role"
.Lr_user: .asciz "user"
.Lr_assistant: .asciz "assistant"
.Lr_toolResult: .asciz "toolResult"
.Lr_system: .asciz "system"
.Lr_content: .asciz "content"
.Lr_messages: .asciz "messages"
.Lr_history_key: .asciz "history"
.Lr_systemPrompt: .asciz "systemPrompt"
.Lr_thinking: .asciz "thinking"
.Lr_thinkingSignature: .asciz "thinkingSignature"
.Lr_reasoning_content: .asciz "reasoning_content"
.Lr_toolCall: .asciz "toolCall"
.Lr_arguments: .asciz "arguments"
.Lr_function: .asciz "function"
.Lr_tool_calls: .asciz "tool_calls"
.Lr_toolCallId: .asciz "toolCallId"
.Lr_toolName: .asciz "toolName"
.Lr_tool_call_id: .asciz "tool_call_id"
.Lr_isError: .asciz "isError"
.Lr_system_wire: .asciz "{\"role\":\"system\"}"
.Lr_user_wire: .asciz "{\"role\":\"user\"}"
.Lr_assistant_wire: .asciz "{\"role\":\"assistant\"}"
.Lr_tool_wire: .asciz "{\"role\":\"tool\"}"
.Lr_call_wire: .asciz "{\"type\":\"function\",\"function\":{}}"
.section .note.GNU-stack,"",@progbits

.text
.globl pg_context_blank
.type pg_context_blank,@function
/* ECMAScript trim's WhiteSpace and LineTerminator set, on UTF-8 bytes. */
pg_context_blank:
 test rsi,rsi
 jz .Ltool_blank_yes
 movzx eax,byte ptr [rdi]
 cmp eax,32
 je .Ltool_blank_one
 cmp eax,9
 jb .Ltool_blank_no
 cmp eax,13
 jbe .Ltool_blank_one
 cmp rsi,2
 jb .Ltool_blank_no
 cmp word ptr [rdi],0xa0c2
 je .Ltool_blank_two
 cmp rsi,3
 jb .Ltool_blank_no
 movzx eax,word ptr [rdi]
 movzx edx,byte ptr [rdi+2]
 cmp eax,0x80e2
 jne .Ltool_blank_other
 cmp edx,0x80
 jb .Ltool_blank_no
 cmp edx,0x8a
 jbe .Ltool_blank_three
 cmp edx,0xa8
 je .Ltool_blank_three
 cmp edx,0xa9
 je .Ltool_blank_three
 cmp edx,0xaf
 je .Ltool_blank_three
 jmp .Ltool_blank_no
.Ltool_blank_other:
 shl edx,16
 or eax,edx
 cmp eax,0x809ae1
 je .Ltool_blank_three
 cmp eax,0x9f81e2
 je .Ltool_blank_three
 cmp eax,0x8080e3
 je .Ltool_blank_three
 cmp eax,0xbfbbef
 jne .Ltool_blank_no
.Ltool_blank_three:
 add rdi,3
 sub rsi,3
 jmp pg_context_blank
.Ltool_blank_two:
 add rdi,2
 sub rsi,2
 jmp pg_context_blank
.Ltool_blank_one:
 inc rdi
 dec rsi
 jmp pg_context_blank
.Ltool_blank_yes:
 mov eax,1
 ret
.Ltool_blank_no:
 xor eax,eax
 ret

.size pg_context_blank,.-pg_context_blank

.text
.Lr_invalid_input:
 mov eax,5
 ret

.text
/* Validate canonical image data and build one independent OpenAI image part. */
.Lr_image_part:
 RF 48
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 RG rbx,type
 mov rdi,rax
 call .Lr_string
 test rax,rax
 jz .Lri_bad
 mov rdi,rax
 lea rsi,[rip+.Lri_image]
 call strcmp@PLT
 test eax,eax
 jnz .Lri_bad
 mov rdi,rbx
 lea rsi,[rip+.Lri_data]
 call json_object_object_get@PLT
 mov rdi,rax
 call .Lr_string
 test rax,rax
 jz .Lri_bad
 mov r12,rax
 mov rdi,rbx
 lea rsi,[rip+.Lri_mime]
 call json_object_object_get@PLT
 mov rdi,rax
 call .Lr_string
 test rax,rax
 jz .Lri_bad
 mov r13,rax
 lea rdi,[rsp]
 lea rsi,[rip+.Lri_url_format]
 mov rdx,r13
 mov rcx,r12
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lri_bad
 lea rdi,[rip+.Lri_part_json]
 call json_tokener_parse@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lri_bad
 mov rdi,rax
 lea rsi,[rip+.Lri_image_url]
 call json_object_object_get@PLT
 mov rbx,rax
 mov rdi,[rsp]
 call json_object_new_string@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lri_url]
 mov rdx,rax
 call .Lr_set
 test eax,eax
 jnz .Lri_bad
 mov rdi,[rsp]
 call free@PLT
 mov rax,[rsp+8]
 RE 48
.Lri_bad:
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 xor eax,eax
 RE 48
.section .rodata
.Lri_image: .asciz "image"
.Lri_data: .asciz "data"
.Lri_mime: .asciz "mimeType"
.Lri_url: .asciz "url"
.Lri_image_url: .asciz "image_url"
.Lri_url_format: .asciz "data:%s;base64,%s"
.Lri_part_json: .asciz "{\"type\":\"image_url\",\"image_url\":{}}"

.text
.Lri_capability:
 RF 0
 mov rbx,rdi
 RG rbx,modelInfo
 test rax,rax
 cmovnz rbx,rax
 mov rdi,rbx
 lea rsi,[rip+.Lri_input]
 call json_object_object_get@PLT
 mov rbx,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lri_cap_no
 xor r12d,r12d
.Lri_cap_each:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lri_cap_no
 mov rdi,rbx
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lri_cap_next
 mov rdi,rax
 lea rsi,[rip+.Lri_image]
 call strcmp@PLT
 test eax,eax
 jz .Lri_cap_yes
.Lri_cap_next:
 inc r12
 jmp .Lri_cap_each
.Lri_cap_no:
 xor eax,eax
 RE 0
.Lri_cap_yes:
 mov eax,1
 RE 0
.Lri_has_image:
 RF 0
 mov rbx,rdi
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lri_has_no
 xor r12d,r12d
.Lri_has_each:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lri_has_no
 mov rdi,rbx
 mov rsi,r12
 call json_object_array_get_idx@PLT
 RG rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lri_has_next
 mov rdi,rax
 lea rsi,[rip+.Lri_image]
 call strcmp@PLT
 test eax,eax
 jz .Lri_has_yes
.Lri_has_next:
 inc r12
 jmp .Lri_has_each
.Lri_has_no:
 xor eax,eax
 RE 0
.Lri_has_yes:
 mov eax,1
 RE 0
.Lri_collect:
 RF 16
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
 mov qword ptr [rsp],0
 mov rdi,rbx
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lri_collect_ok
.Lri_collect_each:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lri_collect_ok
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 RG rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lri_image]
 call strcmp@PLT
 test eax,eax
 jnz .Lri_collect_next
 mov rdi,r14
 call .Lr_image_part
 mov [rsp],rax
 test rax,rax
 jz .Lri_collect_bad
 cmp qword ptr [r12],0
 jne .Lri_collect_append
 lea rdi,[rip+.Lri_images_json]
 call json_tokener_parse@PLT
 mov [r12],rax
 test rax,rax
 jz .Lri_collect_bad
.Lri_collect_append:
 mov rdi,[r12]
 mov rsi,[rsp]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lri_collect_bad
 mov qword ptr [rsp],0
.Lri_collect_next:
 inc r13
 jmp .Lri_collect_each
.Lri_collect_ok:
 xor eax,eax
 RE 16
.Lri_collect_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,5
 RE 16
.section .rodata
.Lr_modelInfo: .asciz "modelInfo"
.Lri_input: .asciz "input"
.Lri_attached: .asciz "(see attached image)"
.Lri_images_json: .asciz "[{\"type\":\"text\",\"text\":\"Attached image(s) from tool result:\"}]"
.Lri_user_json: .asciz "{\"role\":\"user\"}"
