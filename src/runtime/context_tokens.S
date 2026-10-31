/* Pi estimate.ts, fixed d5b3de22d0e24a3e9c38a9410d3329b099717283; MIT: docs/licenses/pi.md. */
.intel_syntax noprefix
.macro CGET object,key
 mov rdi,\object
 lea rsi,[rip+.Lcc_\key]
 call json_object_object_get@PLT
.endm
.text
/* JS string length counts UTF-16 units; committed JSON has validated UTF-8. */
.Lcc_chars:
 push rbx
 mov rbx,rdi
 call json_object_get_string_len@PLT
 mov edx,eax
 mov rdi,rbx
 push rdx
 sub rsp,8
 call json_object_get_string@PLT
 add rsp,8
 pop rdx
 mov rdi,rax
 jmp .Lcc_chars_count
.Lcc_chars_count:
 call .Lcc_count
 pop rbx
 ret
.Lcc_count:
 xor eax,eax
 test rdx,rdx
 jz .Lcc_chars_done
 xor ecx,ecx
.Lcc_chars_byte:
 movzx esi,byte ptr [rdi+rcx]
 cmp esi,0x80
 jb .Lcc_chars_one
 cmp esi,0xc0
 jb .Lcc_chars_next
 cmp esi,0xf0
 jb .Lcc_chars_one
 inc rax
.Lcc_chars_one:
 inc rax
.Lcc_chars_next:
 inc rcx
 cmp rcx,rdx
 jb .Lcc_chars_byte
.Lcc_chars_done:
 ret
.Lcc_serialized_chars:
 push rbx
 mov esi,16
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jnz .Lct_serialized_ready
 lea rax,[rip+.Lct_unserializable]
.Lct_serialized_ready:
 mov rbx,rax
 mov rdi,rax
 call strlen@PLT
 mov rdx,rax
 mov rdi,rbx
 jmp .Lcc_chars_count
/* Content text and images use pi-ai's 4800 estimated image characters. */
.Lcc_content_chars:
 push rbp
 push rbx
 push r12
 mov rbx,rdi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lcc_content_array
 mov rdi,rbx
 call .Lcc_chars
 jmp .Lcc_content_done
.Lcc_content_array:
 xor ebp,ebp
 xor r12d,r12d
.Lcc_content_loop:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lcc_content_end
 mov rdi,rbx
 mov rsi,r12
 call json_object_array_get_idx@PLT
 CGET rax,text
 test rax,rax
 jz .Lcc_content_image
 mov rdi,rax
 call .Lcc_chars
 add rbp,rax
 jmp .Lcc_content_next
.Lcc_content_image:
 add rbp,4800
.Lcc_content_next:
 inc r12
 jmp .Lcc_content_loop
.Lcc_content_end:
 mov rax,rbp
.Lcc_content_done:
 pop r12
 pop rbx
 pop rbp
 ret
/* Per-message ceil(chars / 3.5), with separate tool declaration estimates. */
.Lcc_tokens:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 xor ebp,ebp
 mov qword ptr [rsp],0
 CGET rbx,role
 mov rdi,rax
 call json_object_get_string@PLT
 mov r12,rax
 mov rdi,rax
 lea rsi,[rip+.Lcc_assistant]
 call strcmp@PLT
 test eax,eax
 jz .Lcc_tokens_assistant
 mov rdi,r12
 lea rsi,[rip+.Lcc_system]
 call strcmp@PLT
 test eax,eax
 jz .Lcc_tokens_system
 CGET rbx,content
 mov rdi,rax
 call .Lcc_content_chars
 mov rbp,rax
 jmp .Lcc_tokens_round
.Lcc_tokens_assistant:
 CGET rbx,content
 mov r12,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lct_assistant_blocks
 mov rdi,r12
 call .Lcc_chars
 mov rbp,rax
 jmp .Lcc_tokens_round
.Lct_assistant_blocks:
 xor r13d,r13d
.Lcc_tokens_block:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lcc_tokens_round
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 CGET r14,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcc_text]
 call strcmp@PLT
 test eax,eax
 jnz .Lcc_tokens_thinking
 CGET r14,text
 jmp .Lcc_tokens_string
.Lcc_tokens_thinking:
 CGET r14,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcc_thinking]
 call strcmp@PLT
 test eax,eax
 jnz .Lcc_tokens_call
 CGET r14,thinking
.Lcc_tokens_string:
 mov rdi,rax
 call .Lcc_chars
 add rbp,rax
 jmp .Lcc_tokens_block_next
.Lcc_tokens_call:
 CGET r14,name
 mov rdi,rax
 call .Lcc_chars
 add rbp,rax
 CGET r14,arguments
 mov rdi,rax
 call .Lcc_serialized_chars
 add rbp,rax
.Lcc_tokens_block_next:
 inc r13
 jmp .Lcc_tokens_block
.Lcc_tokens_system:
 CGET rbx,content
 mov r12,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lcc_tokens_system_blocks
 mov rdi,r12
 call .Lcc_chars
 mov rbp,rax
 jmp .Lcc_tokens_sections
.Lcc_tokens_system_blocks:
 xor r13d,r13d
.Lcc_tokens_system_block:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lcc_tokens_sections
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 CGET rax,text
 mov rdi,rax
 call .Lcc_chars
 add rbp,rax
 test r13,r13
 jz .Lcc_tokens_system_block_next
 inc rbp
.Lcc_tokens_system_block_next:
 inc r13
 jmp .Lcc_tokens_system_block
.Lcc_tokens_sections:
 CGET rbx,sections
 test rax,rax
 jz .Lcc_tokens_tools
 mov r12,rax
 mov rdi,rax
 call json_object_iter_begin@PLT
 mov [rsp+8],rax
 mov rdi,r12
 call json_object_iter_end@PLT
 mov [rsp+16],rax
.Lcc_tokens_section:
 lea rdi,[rsp+8]
 lea rsi,[rsp+16]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lcc_tokens_tools
 lea rdi,[rsp+8]
 call json_object_iter_peek_value@PLT
 test rax,rax
 jz .Lcc_tokens_section_next
 mov rdi,rax
 call .Lcc_chars
 test rax,rax
 jz .Lcc_tokens_section_next
 test rbp,rbp
 jz .Lcc_tokens_section_append
 add rbp,2
.Lcc_tokens_section_append:
 add rbp,rax
.Lcc_tokens_section_next:
 lea rdi,[rsp+8]
 call json_object_iter_next@PLT
 jmp .Lcc_tokens_section
.Lcc_tokens_tools:
 CGET rbx,toolsAdded
 mov rdi,rax
 call .Lcc_tokens_tools_one
 add [rsp],rax
 CGET rbx,toolsRemoved
 mov rdi,rax
 call .Lcc_tokens_tools_one
 add [rsp],rax
.Lcc_tokens_round:
 lea rax,[rbp+rbp+6]
 xor edx,edx
 mov ecx,7
 div rcx
 add rax,[rsp]
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lcc_tokens_tools_one:
 test rdi,rdi
 jz .Lcc_tokens_tools_zero
 push rbx
 mov rbx,rdi
 call json_object_array_length@PLT
 test rax,rax
 jz .Lcc_tokens_tools_empty
 mov rdi,rbx
 call .Lcc_serialized_chars
 lea rax,[rax+rax+6]
 xor edx,edx
 mov ecx,7
 div rcx
 pop rbx
 ret
.Lcc_tokens_tools_empty:
 pop rbx
.Lcc_tokens_tools_zero:
 xor eax,eax
 ret

.globl pg_context_message_tokens
.type pg_context_message_tokens,@function
.set pg_context_message_tokens,.Lcc_tokens
.globl pg_context_utf16_units
.type pg_context_utf16_units,@function
pg_context_utf16_units:
 mov rdx,rsi
 jmp .Lcc_count
.section .rodata
.Lcc_arguments: .asciz "arguments"
.Lcc_assistant: .asciz "assistant"
.Lcc_content: .asciz "content"
.Lcc_name: .asciz "name"
.Lcc_role: .asciz "role"
.Lcc_sections: .asciz "sections"
.Lcc_system: .asciz "system"
.Lcc_text: .asciz "text"
.Lcc_thinking: .asciz "thinking"
.Lcc_toolsAdded: .asciz "toolsAdded"
.Lcc_toolsRemoved: .asciz "toolsRemoved"
.Lcc_type: .asciz "type"
.Lct_unserializable: .asciz "[unserializable]"

.macro CF size
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\size
.endm
.macro CE size
 add rsp,\size
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.text
.Lcc_usage_tokens:
 CF 16
 mov rbx,rdi
 CGET rbx,totalTokens
 mov rdi,rax
 call json_object_get_uint64@PLT
 test rax,rax
 jnz .Lcc_usage_end
 xor r12d,r12d
 xor ebp,ebp
.Lcc_usage_component:
 lea rax,[rip+.Lcc_usage_fields]
 movsxd rsi,dword ptr [rax+r12*4]
 add rsi,rax
 mov rdi,rbx
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 add rbp,rax
 inc r12
 cmp r12,4
 jb .Lcc_usage_component
 mov rax,rbp
.Lcc_usage_end:
 CE 16

.globl pg_context_usage_tokens
.type pg_context_usage_tokens,@function
.set pg_context_usage_tokens,.Lcc_usage_tokens
.section .rodata
.Lcc_totalTokens: .asciz "totalTokens"
.Lcc_usage_input: .asciz "input"
.Lcc_usage_output: .asciz "output"
.Lcc_usage_read: .asciz "cacheRead"
.Lcc_usage_write: .asciz "cacheWrite"
.Lcc_usage_fields:
 .long .Lcc_usage_input-.Lcc_usage_fields,.Lcc_usage_output-.Lcc_usage_fields
 .long .Lcc_usage_read-.Lcc_usage_fields,.Lcc_usage_write-.Lcc_usage_fields

.text
.globl pg_context_estimate
.type pg_context_estimate,@function
pg_context_estimate:
 CF 32
 mov rbx,rdi
 xor r12d,r12d
 xor r13d,r13d
 xor ebp,ebp
 mov rax,0xfff0000000000000
 mov [rsp],rax
 mov rdi,rbx
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lct_scan
 CGET rbx,systemPrompt
 test rax,rax
 jz .Lct_context_messages
 mov rdi,rax
 call .Lcc_chars
 lea rax,[rax+rax+6]
 xor edx,edx
 mov ecx,7
 div rcx
 mov rbp,rax
 test rax,rax
 jz .Lct_context_messages
 mov qword ptr [rsp],0
.Lct_context_messages:
 CGET rbx,messages
 mov rbx,rax
.Lct_scan:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lct_trailing
 mov rdi,rbx
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov r14,rax
 CGET r14,timestamp
 mov [rsp+16],rax
 mov rdi,rax
 call json_object_get_double@PLT
 movsd [rsp+8],xmm0
 CGET r14,role
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lct_prefix
 mov rdi,rax
 lea rsi,[rip+.Lcc_assistant]
 call strcmp@PLT
 test eax,eax
 jnz .Lct_prefix
 cmp qword ptr [rsp+16],0
 je .Lct_prefix
 movsd xmm0,[rsp+8]
 ucomisd xmm0,[rsp]
 jp .Lct_prefix
 jb .Lct_prefix
 CGET r14,stopReason
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lct_usage
 mov rdi,rax
 lea rsi,[rip+.Lcc_aborted]
 call strcmp@PLT
 test eax,eax
 jz .Lct_prefix
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lcc_error]
 call strcmp@PLT
 test eax,eax
 jz .Lct_prefix
.Lct_usage:
 CGET r14,usage
 mov rdi,rax
 call pg_context_usage_tokens@PLT
 test rax,rax
 jz .Lct_prefix
 mov rbp,rax
 lea r13,[r12+1]
.Lct_prefix:
 movsd xmm0,[rsp+8]
 maxsd xmm0,[rsp]
 movsd [rsp],xmm0
 inc r12
 jmp .Lct_scan
.Lct_trailing:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lct_estimate_done
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call pg_context_message_tokens@PLT
 add rbp,rax
 jnc .Lct_trailing_next
 mov rbp,-1
.Lct_trailing_next:
 inc r13
 jmp .Lct_trailing
.Lct_estimate_done:
 mov rax,rbp
 CE 32
.size pg_context_estimate,.-pg_context_estimate
.section .rodata
.Lcc_timestamp: .asciz "timestamp"
.Lcc_stopReason: .asciz "stopReason"
.Lcc_usage: .asciz "usage"
.Lcc_aborted: .asciz "aborted"
.Lcc_error: .asciz "error"
.section .note.GNU-stack,"",@progbits

.section .rodata
.Lcc_systemPrompt: .asciz "systemPrompt"
.Lcc_messages: .asciz "messages"
