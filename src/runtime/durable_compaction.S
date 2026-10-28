/* Pi Durable compaction.ts / pi-ai estimate.ts, d5b3de22d0e24a3e9c38a9410d3329b099717283.
   MIT upstream notice: docs/licenses/pi.md. */
#include "session_layout.h"
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
/* A user Entry cannot split an assistant from its later results. */
.Lcc_candidate:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rbx
 call json_object_array_get_idx@PLT
 xor esi,esi
 mov rdi,rax
 call json_object_array_get_idx@PLT
 test rax,rax
 jz .Lcc_candidate_no
 CGET rax,role
 mov rdi,rax
 call json_object_get_string@PLT
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Lcc_assistant]
 call strcmp@PLT
 test eax,eax
 jz .Lcc_candidate_yes
 mov rdi,r13
 lea rsi,[rip+.Lcc_user]
 call strcmp@PLT
 test eax,eax
 jnz .Lcc_candidate_no
 mov r13,r12
.Lcc_candidate_before:
 test r13,r13
 jz .Lcc_candidate_yes
 dec r13
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 call json_object_array_length@PLT
 mov rbp,rax
.Lcc_candidate_before_message:
 test rbp,rbp
 jz .Lcc_candidate_before
 dec rbp
 mov rdi,r14
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov [rsp],rax
 CGET rax,role
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcc_assistant]
 call strcmp@PLT
 test eax,eax
 jnz .Lcc_candidate_before_message
 CGET "qword ptr [rsp]",content
 mov [rsp],rax
 mov r13,r12
.Lcc_candidate_after:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lcc_candidate_yes
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 xor ebp,ebp
.Lcc_candidate_after_message:
 mov rdi,r14
 call json_object_array_length@PLT
 cmp rbp,rax
 jae .Lcc_candidate_after_next
 mov rdi,r14
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov [rsp+8],rax
 CGET rax,role
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+16],rax
 mov rdi,rax
 lea rsi,[rip+.Lcc_assistant]
 call strcmp@PLT
 test eax,eax
 jnz .Lcc_candidate_result
 cmp r13,r12
 jne .Lcc_candidate_yes
 test rbp,rbp
 jnz .Lcc_candidate_yes
.Lcc_candidate_result:
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lcc_toolResult]
 call strcmp@PLT
 test eax,eax
 jnz .Lcc_candidate_message_next
 CGET "qword ptr [rsp+8]",toolCallId
 mov [rsp+8],rax
 mov qword ptr [rsp+24],0
.Lcc_candidate_match:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 cmp [rsp+24],rax
 jae .Lcc_candidate_message_next
 mov rdi,[rsp]
 mov rsi,[rsp+24]
 call json_object_array_get_idx@PLT
 mov [rsp+16],rax
 CGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcc_toolCall]
 call strcmp@PLT
 test eax,eax
 jnz .Lcc_candidate_match_next
 CGET "qword ptr [rsp+16]",id
 test rax,rax
 jz .Lcc_candidate_match_next
 mov rdi,rax
 mov rsi,[rsp+8]
 call json_object_equal@PLT
 test eax,eax
 jnz .Lcc_candidate_no
.Lcc_candidate_match_next:
 inc qword ptr [rsp+24]
 jmp .Lcc_candidate_match
.Lcc_candidate_message_next:
 inc rbp
 jmp .Lcc_candidate_after_message
.Lcc_candidate_after_next:
 inc r13
 jmp .Lcc_candidate_after
.Lcc_candidate_yes:
 mov eax,1
 jmp .Lcc_candidate_end
.Lcc_candidate_no:
 xor eax,eax
.Lcc_candidate_end:
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.globl pg_durable_compaction_select
.type pg_durable_compaction_select,@function
pg_durable_compaction_select:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov [rsp],rsi
 mov [rsp+8],rdx
 test rdx,rdx
 jz .Lcc_select_invalid
 mov qword ptr [rdx],0
 test rbx,rbx
 jz .Lcc_select_invalid
 mov qword ptr [rsp+16],0
 CGET rbx,head
 test rax,rax
 setnz al
 movzx eax,al
 mov [rsp+24],rax
 CGET rbx,contributions
 mov r12,rax
 mov rdi,rax
 call json_object_array_length@PLT
 mov r13,rax
 mov [rsp+40],r13
 mov r13,[rsp+24]
 mov qword ptr [rsp+32],-1
.Lcc_select_candidates:
 cmp r13,[rsp+40]
 jae .Lcc_select_candidates_done
 mov rdi,r12
 mov rsi,r13
 call .Lcc_candidate
 test eax,eax
 jz .Lcc_select_candidate_next
 mov [rsp+32],r13
.Lcc_select_candidate_next:
 inc r13
 jmp .Lcc_select_candidates
.Lcc_select_candidates_done:
 mov r13,[rsp+40]
.Lcc_select_reverse:
 cmp r13,[rsp+24]
 jbe .Lcc_select_none
 dec r13
 mov rdi,r12
 mov rsi,r13
 call .Lcc_candidate
 test eax,eax
 jz .Lcc_select_cost
 mov [rsp+32],r13
.Lcc_select_cost:
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 xor ebp,ebp
.Lcc_select_message:
 mov rdi,r14
 call json_object_array_length@PLT
 cmp rbp,rax
 jae .Lcc_select_threshold
 mov rdi,r14
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call .Lcc_tokens
 add [rsp+16],rax
 inc rbp
 jmp .Lcc_select_message
.Lcc_select_threshold:
 mov rax,[rsp+16]
 cmp rax,[rsp]
 jb .Lcc_select_reverse
 cmp qword ptr [rsp+32],-1
 je .Lcc_select_none
 mov r13,[rsp+24]
.Lcc_select_prefix:
 cmp r13,[rsp+32]
 jae .Lcc_select_none
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_array_length@PLT
 test rax,rax
 jnz .Lcc_select_found
 inc r13
 jmp .Lcc_select_prefix
.Lcc_select_found:
 CGET rbx,entries
 mov rdi,rax
 mov rsi,[rsp+32]
 call json_object_array_get_idx@PLT
 CGET rax,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rcx,[rsp+8]
 mov [rcx],rax
 jmp .Lcc_select_none
.Lcc_select_invalid:
 mov eax,4
 jmp .Lcc_select_done
.Lcc_select_none:
 xor eax,eax
.Lcc_select_done:
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_durable_compaction_select,.-pg_durable_compaction_select
.section .rodata
.Lcc_role: .asciz "role"
.Lcc_assistant: .asciz "assistant"
.Lcc_user: .asciz "user"
.Lcc_system: .asciz "system"
.Lcc_toolResult: .asciz "toolResult"
.Lcc_toolCallId: .asciz "toolCallId"
.Lcc_content: .asciz "content"
.Lcc_type: .asciz "type"
.Lcc_text: .asciz "text"
.Lcc_thinking: .asciz "thinking"
.Lcc_name: .asciz "name"
.Lcc_arguments: .asciz "arguments"
.Lcc_id: .asciz "id"
.Lcc_head: .asciz "head"
.Lcc_entries: .asciz "entries"
.Lcc_contributions: .asciz "contributions"
.Lcc_sections: .asciz "sections"
.Lcc_toolsAdded: .asciz "toolsAdded"
.Lcc_toolsRemoved: .asciz "toolsRemoved"
.section .note.GNU-stack,"",@progbits
.text
/* printbuf is json-c owned, with a checked grow operation on every append. */
.Lcc_append:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsi
 call strlen@PLT
 mov rdi,rbx
 mov rsi,r12
 mov edx,eax
 call printbuf_memappend@PLT
 add rsp,8
 pop r12
 pop rbx
 ret
.Lcc_part:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 cmp dword ptr [rbx+8],0
 je .Lcc_part_label
 mov rdi,rbx
 lea rsi,[rip+.Lcc_gap]
 call .Lcc_append
 test eax,eax
 js .Lcc_part_end
.Lcc_part_label:
 mov rdi,rbx
 mov rsi,r12
 call .Lcc_append
.Lcc_part_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.Lcc_append_string:
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
 mov rdi,rbx
 mov rsi,rax
 mov edx,[rsp]
 call printbuf_memappend@PLT
 add rsp,8
 pop r12
 pop rbx
 ret
/* Join content text blocks without interpreting image or thinking payloads. */
.Lcc_text_content:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lcc_text_content_blocks
 mov rdi,rbx
 mov rsi,r12
 call .Lcc_append_string
 mov edx,1
 jmp .Lcc_text_content_end
.Lcc_text_content_blocks:
 xor r13d,r13d
 xor r14d,r14d
.Lcc_text_content_loop:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lcc_text_content_ok
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rbp,rax
 CGET rbp,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcc_text]
 call strcmp@PLT
 test eax,eax
 jnz .Lcc_text_content_next
 test r14,r14
 jz .Lcc_text_content_append
 mov rdi,rbx
 lea rsi,[rip+.Lcc_lf]
 call .Lcc_append
 test eax,eax
 js .Lcc_text_content_end
.Lcc_text_content_append:
 CGET rbp,text
 mov rsi,rax
 mov rdi,rbx
 call .Lcc_append_string
 test eax,eax
 js .Lcc_text_content_end
 inc r14
.Lcc_text_content_next:
 inc r13
 jmp .Lcc_text_content_loop
.Lcc_text_content_ok:
 mov rdx,r14
 xor eax,eax
.Lcc_text_content_end:
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Print a call as name(key=JSON.stringify(value), ...). */
.Lcc_serialize_call:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 CGET r12,name
 mov rsi,rax
 mov rdi,rbx
 call .Lcc_append_string
 test eax,eax
 js .Lcc_serialize_call_end
 mov rdi,rbx
 lea rsi,[rip+.Lcc_open]
 call .Lcc_append
 test eax,eax
 js .Lcc_serialize_call_end
 CGET r12,arguments
 mov r12,rax
 mov rdi,rax
 call json_object_iter_begin@PLT
 mov [rsp],rax
 mov rdi,r12
 call json_object_iter_end@PLT
 mov [rsp+8],rax
 xor ebp,ebp
.Lcc_serialize_arg:
 lea rdi,[rsp]
 lea rsi,[rsp+8]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lcc_serialize_call_close
 test rbp,rbp
 jz .Lcc_serialize_arg_key
 mov rdi,rbx
 lea rsi,[rip+.Lcc_comma]
 call .Lcc_append
 test eax,eax
 js .Lcc_serialize_call_end
.Lcc_serialize_arg_key:
 lea rdi,[rsp]
 call json_object_iter_peek_name@PLT
 mov rsi,rax
 mov rdi,rbx
 call .Lcc_append
 test eax,eax
 js .Lcc_serialize_call_end
 mov rdi,rbx
 lea rsi,[rip+.Lcc_equal]
 call .Lcc_append
 test eax,eax
 js .Lcc_serialize_call_end
 lea rdi,[rsp]
 call json_object_iter_peek_value@PLT
 mov rdi,rax
 mov esi,16
 call json_object_to_json_string_ext@PLT
 mov rdi,rbx
 mov rsi,rax
 call .Lcc_append
 test eax,eax
 js .Lcc_serialize_call_end
 inc rbp
 lea rdi,[rsp]
 call json_object_iter_next@PLT
 jmp .Lcc_serialize_arg
.Lcc_serialize_call_close:
 mov rdi,rbx
 lea rsi,[rip+.Lcc_close]
 call .Lcc_append
.Lcc_serialize_call_end:
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Serialize one visible message; scratch printbuf retains embedded NUL bytes. */
.Lcc_serialize_message:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 CGET r12,role
 mov rdi,rax
 call json_object_get_string@PLT
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Lcc_system]
 call strcmp@PLT
 test eax,eax
 jz .Lcc_serialize_message_ok
 call printbuf_new@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lcc_serialize_message_bad
 CGET r12,content
 mov rsi,rax
 mov rdi,[rsp]
 call .Lcc_text_content
 mov [rsp+8],rdx
 test eax,eax
 js .Lcc_serialize_message_bad
 mov rdi,r13
 lea rsi,[rip+.Lcc_assistant]
 call strcmp@PLT
 test eax,eax
 jz .Lcc_serialize_assistant
 mov rdi,r13
 lea rsi,[rip+.Lcc_user]
 call strcmp@PLT
 test eax,eax
 lea r14,[rip+.Lcc_user_label]
 jz .Lcc_serialize_simple
 lea r14,[rip+.Lcc_tool_label]
 /* The provider receives sanitized UTF-8; truncate at 2000 UTF-16 units. */
 mov rax,[rsp]
 mov rdi,[rax]
 mov edx,[rax+8]
 call .Lcc_count
 mov [rsp+16],rax
 cmp rax,2000
 jbe .Lcc_serialize_simple
 mov rax,[rsp]
 mov rdi,[rax]
 xor ecx,ecx
 xor edx,edx
.Lcc_truncate_byte:
 cmp edx,2000
 jae .Lcc_truncate_end
 movzx eax,byte ptr [rdi+rcx]
 cmp eax,0x80
 jb .Lcc_truncate_ascii
 cmp eax,0xe0
 jb .Lcc_truncate_two
 cmp eax,0xf0
 jb .Lcc_truncate_three
 add edx,2
 add ecx,4
 jmp .Lcc_truncate_byte
.Lcc_truncate_three:
 add ecx,3
 inc edx
 jmp .Lcc_truncate_byte
.Lcc_truncate_two:
 add ecx,2
 inc edx
 jmp .Lcc_truncate_byte
.Lcc_truncate_ascii:
 inc ecx
 inc edx
 jmp .Lcc_truncate_byte
.Lcc_truncate_end:
 mov rax,[rsp]
 mov [rax+8],ecx
 mov byte ptr [rdi+rcx],0
 cmp edx,2000
 je .Lcc_truncate_suffix
 sub ecx,4
 mov [rax+8],ecx
 mov byte ptr [rdi+rcx],0
 mov rdi,rax
 lea rsi,[rip+.Lcc_replacement]
 call .Lcc_append
 test eax,eax
 js .Lcc_serialize_message_bad
.Lcc_truncate_suffix:
 mov rdi,[rsp]
 lea rsi,[rip+.Lcc_truncated]
 mov rdx,[rsp+16]
 sub rdx,2000
 xor eax,eax
 call sprintbuf@PLT
 test eax,eax
 js .Lcc_serialize_message_bad
.Lcc_serialize_simple:
 mov rax,[rsp]
 cmp dword ptr [rax+8],0
 je .Lcc_serialize_message_ok
 mov rdi,rbx
 mov rsi,r14
 call .Lcc_part
 test eax,eax
 js .Lcc_serialize_message_bad
 mov rax,[rsp]
 mov rdi,rbx
 mov rsi,[rax]
 mov edx,[rax+8]
 call printbuf_memappend@PLT
 test eax,eax
 js .Lcc_serialize_message_bad
 jmp .Lcc_serialize_message_ok
.Lcc_serialize_assistant:
 cmp qword ptr [rsp+8],0
 je .Lcc_serialize_calls
 mov rdi,rbx
 lea rsi,[rip+.Lcc_assistant_label]
 call .Lcc_part
 test eax,eax
 js .Lcc_serialize_message_bad
 mov rax,[rsp]
 mov rdi,rbx
 mov rsi,[rax]
 mov edx,[rax+8]
 call printbuf_memappend@PLT
 test eax,eax
 js .Lcc_serialize_message_bad
.Lcc_serialize_calls:
 CGET r12,content
 mov r12,rax
 xor r13d,r13d
 xor r14d,r14d
.Lcc_serialize_call_block:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lcc_serialize_message_ok
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rbp,rax
 CGET rbp,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcc_toolCall]
 call strcmp@PLT
 test eax,eax
 jnz .Lcc_serialize_call_next
 test r14,r14
 jnz .Lcc_serialize_call_separator
 mov rdi,rbx
 lea rsi,[rip+.Lcc_calls_label]
 call .Lcc_part
 jmp .Lcc_serialize_call_ready
.Lcc_serialize_call_separator:
 mov rdi,rbx
 lea rsi,[rip+.Lcc_semicolon]
 call .Lcc_append
.Lcc_serialize_call_ready:
 test eax,eax
 js .Lcc_serialize_message_bad
 mov rdi,rbx
 mov rsi,rbp
 call .Lcc_serialize_call
 test eax,eax
 js .Lcc_serialize_message_bad
 inc r14
.Lcc_serialize_call_next:
 inc r13
 jmp .Lcc_serialize_call_block
.Lcc_serialize_message_ok:
 xor ebp,ebp
 jmp .Lcc_serialize_message_end
.Lcc_serialize_message_bad:
 mov ebp,4
.Lcc_serialize_message_end:
 mov rdi,[rsp]
 test rdi,rdi
 jz .Lcc_serialize_message_release
 call printbuf_free@PLT
.Lcc_serialize_message_release:
 mov eax,ebp
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.globl pg_durable_compaction_request
.type pg_durable_compaction_request,@function
pg_durable_compaction_request:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,80
 mov rbx,rdi
 mov r12,rsi
 mov [rsp+24],rdx
 mov r14,rcx
 mov [rsp+64],r8
 mov qword ptr [rsp+72],0
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+56],0
 test r8,r8
 jz .Lcc_request_bad
 mov qword ptr [r8],0
 test rbx,rbx
 jz .Lcc_request_bad
 test r14,r14
 jz .Lcc_request_bad
 test r12,r12
 jz .Lcc_request_bad
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lcc_request_bad
 CGET rbx,entries
 mov [rsp+32],rax
 CGET rbx,contributions
 mov rbx,rax
 xor r13d,r13d
.Lcc_request_prefix:
 mov rdi,[rsp+32]
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lcc_request_bad
 mov rdi,[rsp+32]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 CGET rax,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,r12
 je .Lcc_request_order
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov [rsp+40],rax
 xor ebp,ebp
.Lcc_request_contribution:
 mov rdi,[rsp+40]
 call json_object_array_length@PLT
 cmp rbp,rax
 jae .Lcc_request_next
 mov rdi,[rsp+40]
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+48],rax
 mov rdi,[rsp]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lcc_request_add_bad
 inc rbp
 jmp .Lcc_request_contribution
.Lcc_request_next:
 inc r13
 jmp .Lcc_request_prefix
.Lcc_request_add_bad:
 mov rdi,[rsp+48]
 call json_object_put@PLT
 jmp .Lcc_request_bad
.Lcc_request_order:
 mov rdi,[rsp]
 lea rsi,[rsp+8]
 call pg_durable_context_order@PLT
 test eax,eax
 jnz .Lcc_request_bad
 call printbuf_new@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lcc_request_bad
 xor r13d,r13d
.Lcc_request_message:
 mov rdi,[rsp+8]
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lcc_request_wrap
 mov rdi,[rsp+8]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rsi,rax
 mov rdi,[rsp+16]
 call .Lcc_serialize_message
 test eax,eax
 jnz .Lcc_request_bad
 inc r13
 jmp .Lcc_request_message
.Lcc_request_wrap:
 call printbuf_new@PLT
 mov [rsp+56],rax
 test rax,rax
 jz .Lcc_request_bad
 mov rdi,rax
 lea rsi,[rip+.Lcc_conversation_open]
 call .Lcc_append
 test eax,eax
 js .Lcc_request_bad
 mov rax,[rsp+16]
 mov rdi,[rsp+56]
 mov rsi,[rax]
 mov edx,[rax+8]
 call printbuf_memappend@PLT
 test eax,eax
 js .Lcc_request_bad
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lcc_conversation_close]
 call .Lcc_append
 test eax,eax
 js .Lcc_request_bad
 cmp qword ptr [rsp+24],0
 je .Lcc_request_copy
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lcc_focus_prefix]
 call .Lcc_append
 test eax,eax
 js .Lcc_request_bad
 mov rdi,[rsp+56]
 mov rsi,[rsp+24]
 call .Lcc_append
 test eax,eax
 js .Lcc_request_bad
.Lcc_request_copy:
 mov rax,[rsp+56]
 mov rdi,[rax]
 mov esi,[rax+8]
 call json_object_new_string_len@PLT
 mov [rsp+72],rax
 test rax,rax
 jz .Lcc_request_bad
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+48],rax
 mov rdi,r14
 xor esi,esi
 mov edx,PG_S_BYTES
 call memset@PLT
 mov rax,[rsp+56]
 mov edx,[rax+8]
 mov [r14+PG_MSG_SIZE],rdx
 mov qword ptr [r14+PG_S_COUNT],1
 mov dword ptr [r14],5
 mov rax,[rsp+48]
 mov [r14+PG_MSG_TEXT],rax
.Lcc_request_system:
 lea rdi,[r14+PG_S_SYSTEM]
 lea rsi,[rip+.Lcc_summary_system]
 call strcpy@PLT
 mov rdi,rax
 call strlen@PLT
 mov [r14+PG_S_SYSTEM_SIZE],rax
 mov rax,[rsp+64]
 mov rdx,[rsp+72]
 mov [rax],rdx
 mov qword ptr [rsp+72],0
 xor ebp,ebp
 jmp .Lcc_request_end
.Lcc_request_bad:
 mov ebp,4
.Lcc_request_end:
 mov rdi,[rsp+72]
 call json_object_put@PLT
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 test rdi,rdi
 jz .Lcc_request_release
 call printbuf_free@PLT
.Lcc_request_release:
 mov rdi,[rsp+56]
 test rdi,rdi
 jz .Lcc_request_return
 call printbuf_free@PLT
.Lcc_request_return:
 mov eax,ebp
 add rsp,80
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lcc_gap: .asciz "\n\n"
.Lcc_lf: .asciz "\n"
.Lcc_open: .asciz "("
.Lcc_close: .asciz ")"
.Lcc_comma: .asciz ", "
.Lcc_semicolon: .asciz "; "
.Lcc_equal: .asciz "="
.Lcc_user_label: .asciz "[User]: "
.Lcc_assistant_label: .asciz "[Assistant]: "
.Lcc_tool_label: .asciz "[Tool result]: "
.Lcc_calls_label: .asciz "[Assistant tool calls]: "
.Lcc_toolCall: .asciz "toolCall"
.Lcc_truncated: .asciz "\n\n[... %lu more characters truncated]"
.Lcc_replacement: .asciz "\357\277\275"
.Lcc_summary_system: .asciz "You are a context summarization assistant. Your task is to read a conversation between a user and an AI assistant, then produce a structured summary following the exact format specified.\n\nDo NOT continue the conversation. Do NOT respond to any questions in the conversation. ONLY output the structured summary."


.Lcc_conversation_open: .asciz "<conversation>\n"
.Lcc_focus_prefix: .asciz "\n\nAdditional focus: "
.Lcc_conversation_close: .asciz "\n</conversation>\n\nThe messages above are a conversation to summarize. Create a structured context checkpoint summary that another LLM will use to continue the work. If the conversation starts with an earlier summary, preserve its information and fold the newer messages into it.\n\nUse this EXACT format:\n\n## Goal\n[What is the user trying to accomplish? Can be multiple items if the session covers different tasks.]\n\n## Constraints & Preferences\n- [Any constraints, preferences, or requirements mentioned by user]\n- [Or \"(none)\" if none were mentioned]\n\n## Progress\n### Done\n- [x] [Completed tasks/changes]\n\n### In Progress\n- [ ] [Current work]\n\n### Blocked\n- [Issues preventing progress, if any]\n\n## Key Decisions\n- **[Decision]**: [Brief rationale]\n\n## Next Steps\n1. [Ordered list of what should happen next]\n\n## Critical Context\n- [Any data, examples, or references needed to continue]\n- [Or \"(none)\" if not applicable]\n\nKeep each section concise. Preserve exact file paths, function names, and error messages."
.text
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
/* Consume owned values, including insertion failure. */
.Lcc_set:
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Lcc_set_done
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,4
.Lcc_set_done:
 pop rbx
 ret
.Lcc_copy:
 CF 16
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 call json_object_get@PLT
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call .Lcc_set
 CE 16
.Lcc_initial:
 push rbx
 mov rbx,rdx
 lea rdi,[rip+.Lcc_initial_json]
 call json_tokener_parse@PLT
 mov [rbx],rax
 test rax,rax
 setz al
 movzx eax,al
 pop rbx
 ret
.globl pg_durable_compaction_init
.type pg_durable_compaction_init,@function
pg_durable_compaction_init:
 mov eax,4
 test rdi,rdi
 jz .Lcc_init_return
 test rsi,rsi
 jz .Lcc_init_return
 mov ecx,8
.Lcc_init_host:
 cmp qword ptr [rsi+rcx],0
 je .Lcc_init_return
 add ecx,8
 cmp ecx,40
 jb .Lcc_init_host
 cmp qword ptr [rsi+64],0
 je .Lcc_init_return
 cmp qword ptr [rsi+72],0
 je .Lcc_init_return
 mov [rdi],rsi
 lea rax,[rip+.Lcc_task_name]
 mov [rdi+8],rax
 mov qword ptr [rdi+16],16
 mov qword ptr [rdi+24],1
 mov [rdi+32],rdi
 lea rax,[rip+.Lcc_initial]
 mov [rdi+40],rax
 lea rax,[rdi+96]
 mov [rdi+48],rax
 mov qword ptr [rdi+56],3
 lea rax,[rip+.Lcc_abort]
 mov [rdi+64],rax
 lea rax,[rip+.Lcc_cleanup]
 mov [rdi+72],rax
 mov qword ptr [rdi+80],0
 lea rax,[rip+.Lcc_cutoff]
 mov [rdi+88],rax
 lea rax,[rip+.Lcc_select_name]
 mov [rdi+96],rax
 mov qword ptr [rdi+104],6
 mov [rdi+112],rdi
 lea rax,[rip+.Lcc_select_phase]
 mov [rdi+120],rax
 lea rax,[rip+.Lcc_cleanup]
 mov [rdi+128],rax
 lea rax,[rip+.Lcc_summarize_name]
 mov [rdi+136],rax
 mov qword ptr [rdi+144],9
 mov [rdi+152],rdi
 lea rax,[rip+.Lcc_summarize_phase]
 mov [rdi+160],rax
 lea rax,[rip+.Lcc_cleanup]
 mov [rdi+168],rax
 lea rax,[rip+.Lcc_place_name]
 mov [rdi+176],rax
 mov qword ptr [rdi+184],5
 mov [rdi+192],rdi
 lea rax,[rip+.Lcc_place_phase]
 mov [rdi+200],rax
 lea rax,[rip+.Lcc_cleanup]
 mov [rdi+208],rax
 xor eax,eax
.Lcc_init_return:
 ret
.Lcc_commit_json:
 CF 16
 mov rbx,rdi
 mov rdi,rsi
 call json_tokener_parse@PLT
 mov r12,rax
 test rax,rax
 jz .Lcc_commit_json_bad
 mov rdi,rbx
 mov rsi,rax
 call .Lcc_commit_state
 mov ebx,eax
 mov rdi,r12
 call json_object_put@PLT
 mov eax,ebx
 neg eax
 CE 16
.Lcc_commit_json_bad:
 mov eax,-5
 CE 16
.Lcc_abort:
 mov rdi,rsi
 lea rsi,[rip+.Lcc_aborted_json]
 jmp .Lcc_commit_json
.Lcc_cutoff:
 mov rdi,rsi
 lea rsi,[rip+.Lcc_cutoff_json]
 jmp .Lcc_commit_json
.Lcc_cleanup:
 CF 16
 mov rbx,rsi
 mov r12,[rsi+56]
 test r12,r12
 jz .Lcc_cleanup_done
 cmp dword ptr [r12+72],0
 je .Lcc_cleanup_values
 cmp dword ptr [r12+76],0
 je .Lcc_cleanup_release
 mov rax,[r12+56]
 test rax,rax
 jz .Lcc_cleanup_release
 mov rdi,[r12+32]
 call rax
.Lcc_cleanup_release:
 mov rax,[r12]
 mov rax,[rax]
 mov rdi,[rax]
 lea rsi,[r12+32]
 call [rax+24]
.Lcc_cleanup_values:
 mov rdi,[r12+16]
 call json_object_put@PLT
 mov rdi,[r12+88]
 call json_object_put@PLT
 mov rdi,[r12+160]
 call json_object_put@PLT
 mov rdi,[r12+24]
 call free@PLT
 mov rdi,[r12+80]
 test rdi,rdi
 jz .Lcc_cleanup_operation
 call printbuf_free@PLT
.Lcc_cleanup_operation:
 mov rdi,r12
 call free@PLT
 mov qword ptr [rbx+56],0
.Lcc_cleanup_done:
 CE 16
.Lcc_select_phase:
 CF 80
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 CGET r13,conversationId
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+48],rax
 mov rdi,[r12+8]
 mov rsi,rax
 mov rdx,-1
 mov rcx,rsp
 call pg_durable_context_read@PLT
 test eax,eax
 jnz .Lcc_select_phase_bad
 CGET r13,input
 CGET rax,keepRecentTokens
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdi,[rsp]
 mov rsi,rax
 lea rdx,[rsp+8]
 call pg_durable_compaction_select@PLT
 test eax,eax
 jnz .Lcc_select_phase_bad
 cmp qword ptr [rsp+8],0
 jne .Lcc_select_phase_prepare
 mov rdi,r12
 lea rsi,[rip+.Lcc_completed_json]
 call .Lcc_commit_json
 mov ebp,eax
 jmp .Lcc_select_phase_end
.Lcc_select_phase_prepare:
 mov rax,[rbx]
 mov rdi,[rax]
 mov rsi,[rsp+48]
 lea rdx,[rsp+16]
 call [rax+8]
 test eax,eax
 jnz .Lcc_select_phase_bad
 CGET "qword ptr [rsp+16]",options
 mov [rsp+56],rax
 test rax,rax
 jnz .Lcc_select_options
 call json_object_new_object@PLT
 mov [rsp+56],rax
 test rax,rax
 jz .Lcc_select_phase_bad
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lcc_options]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_select_phase_bad
.Lcc_select_options:
 mov rdi,[rsp+56]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lcc_select_phase_bad
 CGET r13,input
 CGET rax,reserveTokens
 mov rdi,rax
 call json_object_get_uint64@PLT
 imul rax,rax,4
 xor edx,edx
 mov ecx,5
 div rcx
 mov [rsp+64],rax
 CGET "qword ptr [rsp+56]",maxTokens
 test rax,rax
 jz .Lcc_select_maximum
 mov rdi,rax
 call json_object_get_uint64@PLT
 test rax,rax
 jz .Lcc_select_maximum
 cmp rax,[rsp+64]
 jae .Lcc_select_maximum
 mov [rsp+64],rax
.Lcc_select_maximum:
 CGET "qword ptr [rsp+16]",modelInfo
 CGET rax,maxTokens
 mov rdi,rax
 call json_object_get_uint64@PLT
 test rax,rax
 jz .Lcc_select_maximum_ready
 cmp rax,[rsp+64]
 jae .Lcc_select_maximum_ready
 mov [rsp+64],rax
.Lcc_select_maximum_ready:
 mov rdi,[rsp+64]
 call json_object_new_uint64@PLT
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lcc_maxTokens]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_select_phase_bad
 lea rdi,[rip+.Lcc_summarize_json]
 call json_tokener_parse@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lcc_select_phase_bad
 CGET rax,checkpoint
 mov r14,rax
 mov rdi,r14
 lea rsi,[rip+.Lcc_configuration]
 mov rdx,[rsp+16]
 call .Lcc_copy
 test eax,eax
 jnz .Lcc_select_phase_bad
 mov rdi,[rsp+8]
 call json_object_new_uint64@PLT
 mov rdi,r14
 lea rsi,[rip+.Lcc_firstKept]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_select_phase_bad
 CGET "qword ptr [rsp]",entries
 mov [rsp+32],rax
 xor r13d,r13d
 mov qword ptr [rsp+40],0
.Lcc_select_phase_tail:
 mov rdi,[rsp+32]
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lcc_select_phase_pin
 mov rdi,[rsp+32]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 CGET rax,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rsp+40]
 jbe .Lcc_select_phase_tail_next
 mov [rsp+40],rax
.Lcc_select_phase_tail_next:
 inc r13
 jmp .Lcc_select_phase_tail
.Lcc_select_phase_pin:
 mov rdi,[rsp+40]
 call json_object_new_uint64@PLT
 mov rdi,r14
 lea rsi,[rip+.Lcc_tail]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_select_phase_bad
 mov rdi,r12
 mov rsi,[rsp+24]
 call .Lcc_commit_state
 neg eax
 mov ebp,eax
 jmp .Lcc_select_phase_end
.Lcc_select_phase_bad:
 mov ebp,-4
.Lcc_select_phase_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov eax,ebp
 CE 80
.Lcc_summarize_phase:
 CF 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 cmp qword ptr [r12+56],0
 jne .Lcc_summarize_acquire
 mov edi,1
 mov esi,176
 call calloc@PLT
 mov [r12+56],rax
 test rax,rax
 jz .Lcc_summarize_bad
 mov r14,rax
 mov [rax],rbx
 mov [rax+8],r12
 CGET r13,state
 CGET rax,checkpoint
 mov [rsp+16],rax
 CGET rax,configuration
 mov rdi,rax
 call json_object_get@PLT
 mov [r14+16],rax
 test rax,rax
 jz .Lcc_summarize_bad
 mov edi,1
 mov esi,PG_S_BYTES
 call calloc@PLT
 mov [r14+24],rax
 test rax,rax
 jz .Lcc_summarize_bad
 CGET r13,conversationId
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+24],rax
 CGET "qword ptr [rsp+16]",tail
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdx,rax
 mov rdi,[r12+8]
 mov rsi,[rsp+24]
 mov rcx,rsp
 call pg_durable_context_read@PLT
 test eax,eax
 jnz .Lcc_summarize_bad
 CGET r13,input
 CGET rax,instructions
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+32],rax
 CGET "qword ptr [rsp+16]",firstKept
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdi,[rsp]
 mov rsi,rax
 mov rdx,[rsp+32]
 mov rcx,[r14+24]
 lea r8,[r14+160]
 call pg_durable_compaction_request@PLT
 test eax,eax
 jnz .Lcc_summarize_fail
 call printbuf_new@PLT
 mov [r14+80],rax
 test rax,rax
 jz .Lcc_summarize_bad
 lea rdi,[rip+.Lcc_assistant_json]
 call json_tokener_parse@PLT
 mov [r14+88],rax
 test rax,rax
 jz .Lcc_summarize_bad
.Lcc_summarize_acquire:
 mov r14,[r12+56]
 cmp dword ptr [r14+72],0
 jne .Lcc_summarize_next
 mov rax,[rbx]
 mov rdi,[rax]
 mov rsi,[r14+16]
 lea rdx,[r14+32]
 call [rax+16]
 cmp eax,1
 je .Lcc_summarize_pending
 test eax,eax
 jnz .Lcc_summarize_fail
 mov dword ptr [r14+72],1
 mov rdi,[r14+32]
 mov rsi,[r14+24]
 call [r14+40]
 test eax,eax
 jnz .Lcc_summarize_fail
 mov dword ptr [r14+76],1
.Lcc_summarize_next:
 mov rdi,[r14+32]
 lea rsi,[r14+104]
 call [r14+48]
 test eax,eax
 jnz .Lcc_summarize_fail
 mov eax,[r14+104]
 test eax,eax
 jz .Lcc_summarize_pending
 cmp eax,1
 je .Lcc_summarize_text
 cmp eax,3
 jne .Lcc_summarize_fail
 mov rax,[rbx]
 cmp qword ptr [rax+72],0
 je .Lcc_summarize_fail
 mov rdi,[rax]
 lea rsi,[r14+32]
 mov rdx,[r14+88]
 call [rax+72]
 test eax,eax
 jnz .Lcc_summarize_fail
 CGET "qword ptr [r14+88]",stopReason
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lcc_summarize_fail
 mov rdi,rax
 lea rsi,[rip+.Lcc_stop]
 call strcmp@PLT
 test eax,eax
 jnz .Lcc_summarize_fail
 mov rax,[r14+80]
 mov rdi,[rax]
 mov esi,[rax+8]
 mov [rsp+24],rdi
 mov [rsp+32],rsi
 /* Trim the ASCII whitespace emitted around a structured summary. */
.Lcc_summary_trim_start:
 cmp qword ptr [rsp+32],0
 je .Lcc_summarize_fail
 mov rax,[rsp+24]
 movzx edi,byte ptr [rax]
 call isspace@PLT
 test eax,eax
 jz .Lcc_summary_trim_end
 inc qword ptr [rsp+24]
 dec qword ptr [rsp+32]
 jmp .Lcc_summary_trim_start
.Lcc_summary_trim_end:
 mov rax,[rsp+24]
 add rax,[rsp+32]
 movzx edi,byte ptr [rax-1]
 call isspace@PLT
 test eax,eax
 jz .Lcc_summary_save
 dec qword ptr [rsp+32]
 jmp .Lcc_summary_trim_end
.Lcc_summary_save:
 CGET r13,state
 CGET rax,checkpoint
 mov rdi,rax
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lcc_summarize_bad
 mov rdi,[rsp+24]
 mov esi,[rsp+32]
 call json_object_new_string_len@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lcc_summary]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_summarize_bad
 lea rdi,[rip+.Lcc_place_name]
 call json_object_new_string@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lcc_phase]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_summarize_bad
 lea rdi,[rip+.Lcc_running_json]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lcc_summarize_bad
 mov rdi,rax
 lea rsi,[rip+.Lcc_checkpoint]
 mov rdx,[rsp+8]
 call .Lcc_copy
 test eax,eax
 jnz .Lcc_summarize_bad
 mov rdi,r12
 mov rsi,[rsp+16]
 call .Lcc_commit_state
 mov ebp,eax
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,ebp
 neg eax
 jmp .Lcc_summarize_end
.Lcc_summarize_text:
 mov rax,[r14+80]
 mov rcx,[r14+120]
 test rcx,rcx
 jz .Lcc_summarize_pending
 cmp qword ptr [r14+112],0
 je .Lcc_summarize_fail
 mov edx,[rax+8]
 add rdx,rcx
 jc .Lcc_summarize_fail
 cmp rdx,2147483646
 ja .Lcc_summarize_fail
 mov rdi,rax
 mov rsi,[r14+112]
 mov edx,ecx
 call printbuf_memappend@PLT
 test eax,eax
 js .Lcc_summarize_bad
.Lcc_summarize_pending:
 mov eax,1
 jmp .Lcc_summarize_end
.Lcc_summarize_fail:
 mov rdi,r12
 lea rsi,[rip+.Lcc_failed_json]
 call .Lcc_commit_json
 jmp .Lcc_summarize_end
.Lcc_summarize_bad:
 mov eax,-4
.Lcc_summarize_end:
 mov ebp,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebp
 CE 48
.text
/* Summary placement and terminal receipt share the existing Submission line. */
.Lcc_place_stage:
 CF 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rcx
 mov [rsp+40],rdx
 mov rax,[rbx+8]
 CGET "qword ptr [rax+24]",conversationId
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+32],rax
 mov rax,[rbx+8]
 CGET "qword ptr [rax+24]",owner
 test rax,rax
 jnz .Lcc_place_owned
 mov rdx,[rsp+32]
 mov rax,[rbx]
 mov rax,[rax]
 mov rdi,[rax+64]
 mov rsi,r12
 mov rcx,[rbx+16]
 lea r8,[rsp]
 call pg_durable_inputs_admit@PLT
 test eax,eax
 jnz .Lcc_place_stage_end
 lea r14,[rip+.Lcc_submissionId]
 jmp .Lcc_place_receipt
.Lcc_place_owned:
 mov rax,[rbx+8]
 mov rcx,[rax+16]
 CGET "qword ptr [rbx+16]",entry
 mov rdx,rax
 mov rdi,r12
 mov rsi,[rsp+32]
 mov rax,[rbx]
 mov rax,[rax]
 mov r8,[rax+32]
 lea r9,[rsp]
 mov rax,[rbx+8]
 mov rcx,[rax+16]
 call pg_record_tx_append_entry@PLT
 test eax,eax
 jnz .Lcc_place_stage_end
 mov r14,[rsp]
 CGET r14,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+32],rax
 mov rdi,r14
 call json_object_put@PLT
 mov rax,[rsp+32]
 mov [rsp],rax
 lea r14,[rip+.Lcc_entryId]
.Lcc_place_receipt:
 lea rdi,[rip+.Lcc_completed_json]
 call json_tokener_parse@PLT
 mov [r13],rax
 test rax,rax
 jz .Lcc_place_stage_bad
 CGET rax,outcome
 CGET rax,result
 mov r12,rax
 mov rdi,[rsp]
 call json_object_new_uint64@PLT
 mov rdi,r12
 mov rsi,r14
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_place_stage_end
 mov rdi,[rbx+8]
 mov rsi,[rsp+40]
 mov rdx,[r13]
 mov ecx,1
 call .Lcc_live_update
.Lcc_place_stage_end:
 CE 48
.Lcc_place_stage_bad:
 mov eax,5
 CE 48
.Lcc_place_phase:
 CF 160
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 mov qword ptr [rsp+16],0
 CGET r13,state
 CGET rax,checkpoint
 mov r14,rax
 lea rdi,[rip+.Lcc_write_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lcc_place_bad
 CGET rax,entry
 mov [rsp+24],rax
 CGET rax,data
 mov [rsp+64],rax
 CGET r13,input
 CGET rax,reason
 test rax,rax
 jz .Lcc_place_reason_ready
 mov rdx,rax
 mov rdi,[rsp+64]
 lea rsi,[rip+.Lcc_reason]
 call .Lcc_copy
 test eax,eax
 jnz .Lcc_place_bad
.Lcc_place_reason_ready:
 CGET r14,firstKept
 mov rdx,rax
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lcc_head]
 call .Lcc_copy
 test eax,eax
 jnz .Lcc_place_bad
 call printbuf_new@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lcc_place_bad
 mov rdi,rax
 lea rsi,[rip+.Lcc_summary_prefix]
 call .Lcc_append
 test eax,eax
 js .Lcc_place_bad
 CGET r14,summary
 mov rsi,rax
 mov rdi,[rsp+8]
 call .Lcc_append_string
 test eax,eax
 js .Lcc_place_bad
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lcc_summary_suffix]
 call .Lcc_append
 test eax,eax
 js .Lcc_place_bad
 mov rax,[rsp+8]
 cmp dword ptr [rax+8],2147483646
 ja .Lcc_place_bad
 mov rdi,[rax]
 mov esi,[rax+8]
 call json_object_new_string_len@PLT
 mov [rsp+16],rax
 CGET "qword ptr [rsp+24]",model
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 CGET rax,content
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcc_text]
 mov rdx,[rsp+16]
 mov qword ptr [rsp+16],0
 call .Lcc_set
 test eax,eax
 jnz .Lcc_place_bad
 lea rdi,[rsp+80]
 mov esi,64
 lea rdx,[rip+.Lcc_request_id_format]
 mov rcx,[r12+16]
 xor eax,eax
 call snprintf@PLT
 test eax,eax
 js .Lcc_place_bad
 lea rdi,[rsp+80]
 call json_object_new_string@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lcc_requestId]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_place_bad
 mov [rsp+32],rbx
 mov [rsp+40],r12
 mov rax,[rsp]
 mov [rsp+48],rax
 mov rdi,r12
 lea rsi,[rsp+32]
 lea rdx,[rip+.Lcc_place_stage]
 call pg_task_runner_commit_with@PLT
 neg eax
 mov ebp,eax
 jmp .Lcc_place_end
.Lcc_place_bad:
 mov ebp,-4
.Lcc_place_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 test rdi,rdi
 jz .Lcc_place_release
 call printbuf_free@PLT
.Lcc_place_release:
 mov eax,ebp
 CE 160
.globl pg_durable_compaction_start
.type pg_durable_compaction_start,@function
pg_durable_compaction_start:
 CF 272
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rbp,r8
 mov rdi,rsp
 xor eax,eax
 mov ecx,26
 cld
 rep stosq
 mov dword ptr [rsp+184],4
 test rbp,rbp
 jz .Lcc_start_end
 mov qword ptr [rbp],0
 test rbx,rbx
 jz .Lcc_start_end
 test r12,r12
 jz .Lcc_start_end
 test r13,r13
 jz .Lcc_start_end
 test r14,r14
 jz .Lcc_start_end
 cmp qword ptr [r14],0
 je .Lcc_start_end
 cmp qword ptr [r14+16],2
 jb .Lcc_start_end
 cmp qword ptr [r14+16],1000000
 ja .Lcc_start_end
 lea rdi,[rip+.Lcc_definition_json]
 call json_tokener_parse@PLT
 mov [rsp+136],rax
 lea rdi,[rip+.Lcc_input_json]
 call json_tokener_parse@PLT
 mov [rsp+144],rax
 lea rdi,[rip+.Lcc_options_json]
 call json_tokener_parse@PLT
 mov [rsp+152],rax
 test rax,rax
 jz .Lcc_start_end
 cmp qword ptr [rsp+136],0
 je .Lcc_start_end
 cmp qword ptr [rsp+144],0
 je .Lcc_start_end
 mov rdi,[r14+8]
 call json_object_new_uint64@PLT
 mov rdi,[rsp+144]
 lea rsi,[rip+.Lcc_keepRecentTokens]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_start_end
 mov rdi,[r14+16]
 call json_object_new_uint64@PLT
 mov rdi,[rsp+144]
 lea rsi,[rip+.Lcc_reserveTokens]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_start_end
 cmp qword ptr [r14+24],0
 je .Lcc_start_begin
 mov rdi,[r14+24]
 call json_object_new_string@PLT
 mov rdi,[rsp+144]
 lea rsi,[rip+.Lcc_instructions]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_start_end
.Lcc_start_begin:
 mov rdi,r12
 mov rsi,rsp
 lea rdx,[rsp+48]
 mov rcx,[r13]
 mov r8,[r13+8]
 call pg_durable_session_begin@PLT
 test eax,eax
 jnz .Lcc_start_end
 mov dword ptr [rsp+188],1
 mov [rsp+168],rbx
 mov [rsp+176],r13
 lea rax,[rsp+168]
 mov [rsp+96],rax
 lea rax,[rip+.Lcc_initial]
 mov [rsp+104],rax
 lea rax,[rip+.Lcc_creation_mint]
 mov [rsp+112],rax
 mov rax,[r14]
 mov [rsp+128],rax
 mov rdi,rsp
 mov rsi,[rsp+136]
 mov rdx,[rsp+144]
 mov rcx,[rsp+152]
 lea r8,[rsp+96]
 lea r9,[rsp+160]
 call pg_record_tx_create_task@PLT
 test eax,eax
 jnz .Lcc_start_abort
 lea rax,[rbx+8]
 mov [rsp+208],rax
 mov [rsp+216],r13
 mov rax,[rbx]
 mov rax,[rax+32]
 mov [rsp+224],rax
 mov rax,[r14]
 mov [rsp+232],rax
 mov qword ptr [rsp+240],0
 mov rax,[rsp+144]
 mov [rsp+248],rax
 lea rdi,[rsp+208]
 mov rsi,[rsp+160]
 lea rdx,[rsp+48]
 call .Lcc_live_created
 test eax,eax
 jnz .Lcc_start_abort
 mov rdi,r12
 xor esi,esi
 xor edx,edx
 xor ecx,ecx
 call pg_durable_session_settle@PLT
 mov dword ptr [rsp+188],0
 test eax,eax
 jnz .Lcc_start_end
 mov rax,[rsp+160]
 mov [rbp],rax
 mov dword ptr [rsp+184],0
 jmp .Lcc_start_end
.Lcc_start_abort:
 mov rdi,r12
 call pg_durable_session_abort@PLT
 mov dword ptr [rsp+188],0
.Lcc_start_end:
 mov rdi,[rsp+136]
 call json_object_put@PLT
 mov rdi,[rsp+144]
 call json_object_put@PLT
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov eax,[rsp+184]
 CE 272
.section .rodata
.Lcc_task_name: .asciz "pangu.compaction"
.Lcc_select_name: .asciz "select"
.Lcc_summarize_name: .asciz "summarize"
.Lcc_place_name: .asciz "place"
.Lcc_phase: .asciz "phase"
.Lcc_state: .asciz "state"
.Lcc_checkpoint: .asciz "checkpoint"
.Lcc_configuration: .asciz "configuration"
.Lcc_input: .asciz "input"
.Lcc_keepRecentTokens: .asciz "keepRecentTokens"
.Lcc_reserveTokens: .asciz "reserveTokens"
.Lcc_conversationId: .asciz "conversationId"
.Lcc_firstKept: .asciz "firstKept"
.Lcc_tail: .asciz "tail"
.Lcc_instructions: .asciz "instructions"
.Lcc_summary: .asciz "summary"
.Lcc_stopReason: .asciz "stopReason"
.Lcc_stop: .asciz "stop"
.Lcc_entry: .asciz "entry"
.Lcc_model: .asciz "model"
.Lcc_requestId: .asciz "requestId"
.Lcc_submissionId: .asciz "submissionId"
.Lcc_outcome: .asciz "outcome"
.Lcc_result: .asciz "result"
.Lcc_request_id_format: .asciz "compaction:%lu"
.Lcc_summary_prefix: .asciz "The conversation history before this point was compacted into the following summary:\n\n<summary>\n"
.Lcc_summary_suffix: .asciz "\n</summary>"
.Lcc_initial_json: .asciz "{\"phase\":\"select\"}"
.Lcc_summarize_json: .asciz "{\"status\":\"running\",\"checkpoint\":{\"phase\":\"summarize\"}}"
.Lcc_running_json: .asciz "{\"status\":\"running\"}"
.Lcc_completed_json: .asciz "{\"status\":\"terminal\",\"outcome\":{\"status\":\"completed\",\"result\":{}}}"
.Lcc_aborted_json: .asciz "{\"status\":\"terminal\",\"outcome\":{\"status\":\"aborted\"}}"
.Lcc_cutoff_json: .asciz "{\"status\":\"terminal\",\"outcome\":{\"status\":\"failed\",\"error\":{\"message\":\"Compaction reached the consecutive restart limit\",\"detail\":{\"reason\":\"restart_limit\"}}}}"
.Lcc_failed_json: .asciz "{\"status\":\"terminal\",\"outcome\":{\"status\":\"failed\",\"error\":{\"message\":\"Summarization failed to produce a complete text summary\",\"detail\":{\"reason\":\"model_error\"}}}}"
.Lcc_assistant_json: .asciz "{\"role\":\"assistant\",\"content\":[]}"
.Lcc_definition_json: .asciz "{\"name\":\"pangu.compaction\",\"version\":1}"
.Lcc_options_json: .asciz "{\"ownership\":{\"kind\":\"conversation\"}}"
.Lcc_input_json: .asciz "{\"reason\":\"manual\"}"
.Lcc_write_json: .asciz "{\"type\":\"write\",\"entry\":{\"kind\":\"pangu.compaction\",\"model\":[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"\"}]}],\"data\":{\"reason\":\"manual\"}}}"

.Lcc_options: .asciz "options"
.Lcc_maxTokens: .asciz "maxTokens"
.text
.Lcc_service_start:
 mov r8,rdx
 mov rcx,rsi
 mov rax,rdi
 mov rdi,[rax]
 mov rsi,[rax+8]
 mov rdx,[rax+16]
 jmp pg_durable_compaction_start
.Lcc_service_poll:
 CF 32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rdx],0
 mov rdi,[rbx+24]
 call pg_task_scheduler_poll@PLT
 cmp eax,4
 jae .Lcc_service_poll_failed
 mov rax,[rbx+16]
 mov rdi,[rax]
 mov esi,2
 mov rdx,r12
 mov rcx,r13
 call [rax+8]
 test eax,eax
 jz .Lcc_service_poll_validate
 mov ebp,eax
 mov rdi,[r13]
 call json_object_put@PLT
 mov qword ptr [r13],0
 mov eax,ebp
 jmp .Lcc_service_poll_end
.Lcc_service_poll_validate:
 CGET "qword ptr [r13]",kind
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lcc_service_poll_wrong
 mov rdi,rax
 lea rsi,[rip+.Lcc_task_name]
 call strcmp@PLT
 test eax,eax
 jz .Lcc_service_poll_end
.Lcc_service_poll_wrong:
 mov rdi,[r13]
 call json_object_put@PLT
 mov qword ptr [r13],0
 mov eax,1
 jmp .Lcc_service_poll_end
.Lcc_service_poll_failed:
 mov eax,6
.Lcc_service_poll_end:
 CE 32
.globl pg_durable_compaction_service
.type pg_durable_compaction_service,@function
pg_durable_compaction_service:
 mov [rsi],rdi
 lea rax,[rip+.Lcc_service_start]
 mov [rsi+8],rax
 lea rax,[rip+.Lcc_service_poll]
 mov [rsi+16],rax
 lea rax,[rip+.Lcc_service_release]
 mov [rsi+24],rax
 ret
.section .rodata
.Lcc_kind: .asciz "kind"

.text
.Lcc_creation_mint:
 mov rax,[rdi]
 mov rax,[rax]
 mov rcx,[rdi+8]
 mov rdi,[rcx]
 jmp [rax+32]

.text
.Lcc_service_release:
 jmp json_object_put@PLT

.section .rodata
.Lcc_owner: .asciz "owner"
.Lcc_entryId: .asciz "entryId"
.text
.Lcc_created_initial:
 mov rax,[rdi]
 mov rdi,[rax+24]
 jmp [rax+32]
.Lcc_created_mint:
 mov rax,[rdi+16]
 mov rcx,[rdi+8]
 mov rdi,[rcx]
 jmp rax
.globl pg_durable_compaction_create
.type pg_durable_compaction_create,@function
pg_durable_compaction_create:
 CF 112
 mov rbx,rdi
 mov [rsp+96],rcx
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rsp+40],0
 mov qword ptr [rsp+48],0
 mov qword ptr [r13],0
 call json_object_new_object@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lcc_create_bad
 mov rax,[rbx]
 mov rdi,[rax]
 mov esi,[rax+8]
 call json_object_new_string_len@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lcc_name]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_create_bad
 mov rax,[rbx]
 mov rdi,[rax+16]
 call json_object_new_uint64@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lcc_version]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_create_bad
 lea rdi,[rip+.Lcc_options_json]
 cmp qword ptr [rbx+32],0
 je .Lcc_create_options
 lea rdi,[rip+.Lcc_owned_options_json]
.Lcc_create_options:
 call json_tokener_parse@PLT
 mov [rsp+48],rax
 test rax,rax
 jz .Lcc_create_bad
 cmp qword ptr [rbx+32],0
 je .Lcc_create_background
 CGET rax,ownership
 mov r14,rax
 mov rdi,[rbx+32]
 call json_object_new_uint64@PLT
 mov rdi,r14
 lea rsi,[rip+.Lcc_taskId]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_create_bad
 jmp .Lcc_create_stage
.Lcc_create_background:
 CGET "qword ptr [rbx+40]",reason
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lcc_create_stage
 mov rdi,rax
 lea rsi,[rip+.Lcc_manual_word]
 call strcmp@PLT
 test eax,eax
 jz .Lcc_create_stage
 mov edi,1
 call json_object_new_boolean@PLT
 mov rdi,[rsp+48]
 lea rsi,[rip+.Lcc_background]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_create_bad
.Lcc_create_stage:
 mov [rsp],rbx
 lea rax,[rip+.Lcc_created_initial]
 mov [rsp+8],rax
 lea rax,[rip+.Lcc_created_mint]
 mov [rsp+16],rax
 mov qword ptr [rsp+24],0
 mov rax,[rbx+24]
 mov [rsp+32],rax
 mov rdi,r12
 mov rsi,[rsp+40]
 mov rdx,[rbx+40]
 mov rcx,[rsp+48]
 mov r8,rsp
 mov r9,r13
 call pg_record_tx_create_task@PLT
 test eax,eax
 jnz .Lcc_create_result
 mov rdi,rbx
 mov rsi,[r13]
 mov rdx,[rsp+96]
 call .Lcc_live_created
.Lcc_create_result:
 mov ebp,eax
 jmp .Lcc_create_end
.Lcc_create_bad:
 mov ebp,4
.Lcc_create_end:
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov eax,ebp
 CE 112
.section .rodata
.Lcc_version: .asciz "version"
.Lcc_ownership: .asciz "ownership"
.Lcc_taskId: .asciz "taskId"
.Lcc_owned_options_json: .asciz "{\"ownership\":{\"kind\":\"task\",\"taskId\":0}}"
.text
.Lcc_policy_value:
 push rbx
 mov rbx,rdx
 call json_object_object_get@PLT
 test rax,rax
 jz .Lcc_policy_default
 mov rdi,rax
 call json_object_get_uint64@PLT
 pop rbx
 ret
.Lcc_policy_default:
 mov rax,rbx
 pop rbx
 ret
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
/* estimateContext: newest applicable assistant usage plus trailing estimates. */
.Lcc_estimate_context:
 CF 64
 mov rbx,rdi
 mov [rsp+48],rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 CGET rbx,head
 CGET rax,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+16],rax
 CGET rbx,contributions
 mov r12,rax
 CGET rbx,entries
 mov [rsp+24],rax
 mov rdi,rax
 call json_object_array_length@PLT
 mov r13,rax
.Lcc_estimate_entry:
 test r13,r13
 jz .Lcc_estimate_messages
 dec r13
 mov rdi,[rsp+24]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 CGET rax,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rsp+16]
 jbe .Lcc_estimate_entry
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 call json_object_array_length@PLT
 mov rbp,rax
.Lcc_estimate_assistant:
 test rbp,rbp
 jz .Lcc_estimate_entry
 dec rbp
 mov rdi,r14
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov [rsp+32],rax
 CGET rax,role
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcc_assistant]
 call strcmp@PLT
 test eax,eax
 jnz .Lcc_estimate_assistant
 CGET "qword ptr [rsp+32]",usage
 mov rdi,rax
 call .Lcc_usage_tokens
 test rax,rax
 jz .Lcc_estimate_assistant
 mov [rsp+8],rax
 mov rax,[rsp+32]
 mov [rsp],rax
.Lcc_estimate_messages:
 CGET rbx,messages
 mov r12,rax
 xor r13d,r13d
 cmp qword ptr [rsp],0
 je .Lcc_estimate_system
.Lcc_estimate_find:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lcc_estimate_sum
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 inc r13
 cmp rax,[rsp]
 jne .Lcc_estimate_find
 jmp .Lcc_estimate_sum
.Lcc_estimate_system:
 CGET "qword ptr [rsp+48]",systemPrompt
 mov rdi,rax
 call .Lcc_chars
 lea rax,[rax+rax+6]
 xor edx,edx
 mov ecx,7
 div rcx
 mov [rsp+8],rax
.Lcc_estimate_sum:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lcc_estimate_done
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call .Lcc_tokens
 add [rsp+8],rax
 inc r13
 jmp .Lcc_estimate_sum
.Lcc_estimate_done:
 mov rax,[rsp+8]
 CE 64
.globl pg_durable_compaction_threshold
.type pg_durable_compaction_threshold,@function
pg_durable_compaction_threshold:
 CF 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 CGET r12,modelInfo
 CGET rax,contextWindow
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lcc_threshold_none
 CGET r12,compaction
 mov r14,rax
 test rax,rax
 jz .Lcc_threshold_policy
 CGET r14,enabled
 test rax,rax
 jz .Lcc_threshold_policy
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lcc_threshold_none
.Lcc_threshold_policy:
 mov rdi,r14
 lea rsi,[rip+.Lcc_keepRecentTokens]
 mov edx,20000
 call .Lcc_policy_value
 mov [r13],rax
 mov rdi,r14
 lea rsi,[rip+.Lcc_reserveTokens]
 mov edx,16384
 call .Lcc_policy_value
 mov [r13+8],rax
 mov rdi,r14
 lea rsi,[rip+.Lcc_backgroundTokens]
 mov edx,32768
 call .Lcc_policy_value
 mov [r13+16],rax
 mov rdi,rbx
 mov rsi,r12
 call .Lcc_estimate_context
 mov rcx,[rsp]
 sub rcx,[r13+8]
 mov dword ptr [rsp+16],1
 cmp rax,rcx
 jg .Lcc_threshold_cut
 cmp qword ptr [r13+16],0
 je .Lcc_threshold_none
 sub rcx,[r13+16]
 cmp rax,rcx
 jle .Lcc_threshold_none
 mov dword ptr [rsp+16],2
.Lcc_threshold_cut:
 mov rdi,rbx
 mov rsi,[r13]
 lea rdx,[rsp+8]
 call pg_durable_compaction_select@PLT
 test eax,eax
 jnz .Lcc_threshold_none
 cmp qword ptr [rsp+8],0
 je .Lcc_threshold_none
 mov eax,[rsp+16]
 jmp .Lcc_threshold_end
.Lcc_threshold_none:
 xor eax,eax
.Lcc_threshold_end:
 CE 48
.section .rodata
.Lcc_modelInfo: .asciz "modelInfo"
.Lcc_contextWindow: .asciz "contextWindow"
.Lcc_compaction: .asciz "compaction"
.Lcc_enabled: .asciz "enabled"
.Lcc_backgroundTokens: .asciz "backgroundTokens"
.Lcc_systemPrompt: .asciz "systemPrompt"
.Lcc_messages: .asciz "messages"
.Lcc_usage: .asciz "usage"
.Lcc_totalTokens: .asciz "totalTokens"
.Lcc_usage_fields:
 .long .Lcc_usage_input-.Lcc_usage_fields,.Lcc_usage_output-.Lcc_usage_fields
 .long .Lcc_usage_read-.Lcc_usage_fields,.Lcc_usage_write-.Lcc_usage_fields
.Lcc_usage_input: .asciz "input"
.Lcc_usage_output: .asciz "output"
.Lcc_usage_read: .asciz "cacheRead"
.Lcc_usage_write: .asciz "cacheWrite"

.section .rodata
.Lcc_data: .asciz "data"
.Lcc_reason: .asciz "reason"

.section .rodata
.Lcc_background: .asciz "background"
.Lcc_manual_word: .asciz "manual"

.text
/* runner + next state: update Live and Task on the same mutation line. */
.Lcc_commit_state:
 push rbx
 sub rsp,32
 mov rbx,rdi
 mov [rsp],rdi
 mov [rsp+8],rsi
 mov rdi,rbx
 mov rsi,rsp
 lea rdx,[rip+.Lcc_state_stage]
 call pg_task_runner_commit_with@PLT
 add rsp,32
 pop rbx
 ret
.Lcc_state_stage:
 CF 32
 mov rbx,rdi
 mov r12,rcx
 mov rsi,rdx
 mov rdi,[rbx]
 mov rdx,[rbx+8]
 xor ecx,ecx
 call .Lcc_live_update
 test eax,eax
 jnz .Lcc_state_stage_end
 mov rdi,[rbx+8]
 call json_object_get@PLT
 mov [r12],rax
 xor eax,eax
.Lcc_state_stage_end:
 CE 32
.Lcc_live_initial:
 push rbx
 mov rbx,rdx
 call json_object_new_object@PLT
 mov [rbx],rax
 test rax,rax
 setz al
 movzx eax,al
 pop rbx
 ret
/* runner, document Tx, next Task state, successful placement. */
.Lcc_live_update:
 CF 272
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14d,ecx
 mov rdi,rsp
 xor eax,eax
 mov ecx,34
 rep stosq
 mov [rsp+208],r14d
 lea rax,[rip+.Lcc_live_kind]
 mov [rsp],rax
 mov qword ptr [rsp+8],10
 CGET "qword ptr [rbx+32]",conversationId
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+32],rax
 mov dword ptr [rsp+40],1
 lea rax,[rip+.Lcc_live_definition]
 mov [rsp+48],rax
 lea rax,[rsp]
 mov [rsp+56],rax
 lea rax,[rip+.Lcc_live_initializer]
 mov [rsp+72],rax
 mov rax,[rbx+40]
 mov rax,[rax+24]
 mov rax,[rax]
 mov rax,[rax+32]
 mov [rsp+80],rax
 mov rdi,r12
 lea rsi,[rsp+48]
 lea rdx,[rsp+112]
 call pg_document_tx_doc@PLT
 test eax,eax
 jnz .Lcc_live_bad
 mov rdi,[rsp+112]
 lea rsi,[rsp+120]
 call pg_document_change_snapshot@PLT
 test eax,eax
 jnz .Lcc_live_bad
 CGET r13,status
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcc_terminal_word]
 call strcmp@PLT
 mov [rsp+212],eax
 call json_object_new_array@PLT
 mov [rsp+128],rax
 test rax,rax
 jz .Lcc_live_bad
 CGET "qword ptr [rsp+120]",compactions
 mov r14,rax
 mov qword ptr [rsp+216],0
 test rax,rax
 jz .Lcc_live_list_done
.Lcc_live_each:
 mov rdi,r14
 call json_object_array_length@PLT
 cmp [rsp+216],rax
 jae .Lcc_live_list_done
 mov rdi,r14
 mov rsi,[rsp+216]
 call json_object_array_get_idx@PLT
 mov rbp,rax
 CGET rax,taskId
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rbx+16]
 je .Lcc_live_remove
 mov rdi,rbp
 call json_object_get@PLT
 mov rdi,[rsp+128]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lcc_live_bad
 jmp .Lcc_live_next
.Lcc_live_remove:
 cmp dword ptr [rsp+212],0
 je .Lcc_live_remove_terminal
 mov qword ptr [rsp+224],1
 mov rdi,rbp
 call json_object_get@PLT
 mov rdi,[rsp+128]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lcc_live_bad
 jmp .Lcc_live_next
.Lcc_live_remove_terminal:
 mov rdi,rbp
 call json_object_get@PLT
 mov [rsp+136],rax
.Lcc_live_next:
 inc qword ptr [rsp+216]
 jmp .Lcc_live_each
.Lcc_live_list_done:
 cmp dword ptr [rsp+212],0
 je .Lcc_live_terminal
 cmp qword ptr [rsp+224],0
 jne .Lcc_live_patch
 lea rdi,[rip+.Lcc_live_status_json]
 call json_tokener_parse@PLT
 mov [rsp+144],rax
 test rax,rax
 jz .Lcc_live_bad
 mov rdi,[rbx+16]
 call json_object_new_uint64@PLT
 mov rdi,[rsp+144]
 lea rsi,[rip+.Lcc_taskId]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_live_bad
 CGET "qword ptr [rbx+32]",input
 CGET rax,reason
 mov rdi,[rsp+144]
 lea rsi,[rip+.Lcc_reason]
 mov rdx,rax
 call .Lcc_copy
 test eax,eax
 jnz .Lcc_live_bad
 CGET "qword ptr [rbx+32]",owner
 mov rdi,rax
 call json_object_get_uint64@PLT
 test rax,rax
 setnz dil
 movzx edi,dil
 call json_object_new_boolean@PLT
 mov rdi,[rsp+144]
 lea rsi,[rip+.Lcc_blocking]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_live_bad
 mov rdi,[rsp+128]
 mov rsi,[rsp+144]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lcc_live_bad
 mov qword ptr [rsp+144],0
 jmp .Lcc_live_patch
.Lcc_live_terminal:
 CGET r13,outcome
 CGET rax,status
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcc_failed_word]
 call strcmp@PLT
 test eax,eax
 jnz .Lcc_live_patch
 cmp qword ptr [rsp+136],0
 je .Lcc_live_patch
 lea rdi,[rip+.Lcc_live_failure_json]
 call json_tokener_parse@PLT
 mov [rsp+152],rax
 test rax,rax
 jz .Lcc_live_bad
 CGET "qword ptr [rsp+136]",taskId
 mov rdi,[rsp+152]
 lea rsi,[rip+.Lcc_taskId]
 mov rdx,rax
 call .Lcc_copy
 test eax,eax
 jnz .Lcc_live_bad
 CGET "qword ptr [rsp+136]",reason
 mov rdi,[rsp+152]
 lea rsi,[rip+.Lcc_reason]
 mov rdx,rax
 call .Lcc_copy
 test eax,eax
 jnz .Lcc_live_bad
 CGET r13,outcome
 CGET rax,error
 CGET rax,message
 mov rdi,[rsp+152]
 lea rsi,[rip+.Lcc_error]
 mov rdx,rax
 call .Lcc_copy
 test eax,eax
 jnz .Lcc_live_bad
.Lcc_live_patch:
 lea rdi,[rip+.Lcc_live_patch_json]
 call json_tokener_parse@PLT
 mov [rsp+160],rax
 test rax,rax
 jz .Lcc_live_bad
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rbp,rax
 mov rdi,[rsp+128]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lcc_live_delete_list
 mov rdi,[rsp+128]
 call json_object_get@PLT
 mov rdi,rbp
 mov esi,2
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Lcc_live_bad
 jmp .Lcc_live_failure_patch
.Lcc_live_delete_list:
 lea rdi,[rip+.Lcc_delete_word]
 call json_object_new_string@PLT
 mov rdi,rbp
 xor esi,esi
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Lcc_live_bad
 mov rdi,rbp
 mov esi,2
 mov edx,1
 call json_object_array_del_idx@PLT
.Lcc_live_failure_patch:
 cmp qword ptr [rsp+152],0
 jne .Lcc_live_set_failure
 cmp dword ptr [rsp+208],0
 je .Lcc_live_append_patch
 lea rdi,[rip+.Lcc_live_clear_failure_json]
 call json_tokener_parse@PLT
 mov [rsp+168],rax
 jmp .Lcc_live_add_patch
.Lcc_live_set_failure:
 lea rdi,[rip+.Lcc_live_set_failure_json]
 call json_tokener_parse@PLT
 mov [rsp+168],rax
 test rax,rax
 jz .Lcc_live_bad
 mov rdi,[rsp+152]
 call json_object_get@PLT
 mov rdi,[rsp+168]
 mov esi,2
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Lcc_live_bad
.Lcc_live_add_patch:
 cmp qword ptr [rsp+168],0
 je .Lcc_live_bad
 mov rdi,[rsp+160]
 mov rsi,[rsp+168]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lcc_live_bad
 mov qword ptr [rsp+168],0
.Lcc_live_append_patch:
 mov rdi,[rsp+112]
 mov rsi,[rsp+160]
 call pg_document_change_append@PLT
 test eax,eax
 jnz .Lcc_live_bad
 xor ebp,ebp
 jmp .Lcc_live_end
.Lcc_live_bad:
 mov ebp,5
.Lcc_live_end:
 mov rdi,[rsp+120]
 call json_object_put@PLT
 mov rdi,[rsp+128]
 call json_object_put@PLT
 mov rdi,[rsp+136]
 call json_object_put@PLT
 mov rdi,[rsp+144]
 call json_object_put@PLT
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov rdi,[rsp+160]
 call json_object_put@PLT
 mov rdi,[rsp+168]
 call json_object_put@PLT
 mov eax,ebp
 CE 272
.section .rodata
.Lcc_live_kind: .asciz "pangu.live"
.Lcc_compactions: .asciz "compactions"
.Lcc_status: .asciz "status"
.Lcc_terminal_word: .asciz "terminal"
.Lcc_failed_word: .asciz "failed"
.Lcc_error: .asciz "error"
.Lcc_message: .asciz "message"
.Lcc_blocking: .asciz "blocking"
.Lcc_delete_word: .asciz "d"
.Lcc_live_status_json: .asciz "{\"attempt\":1}"
.Lcc_live_failure_json: .asciz "{}"
.Lcc_live_patch_json: .asciz "[[\"s\",[\"compactions\"],null]]"
.Lcc_live_clear_failure_json: .asciz "[\"d\",[\"compactionFailure\"]]"
.Lcc_live_set_failure_json: .asciz "[\"s\",[\"compactionFailure\"],null]"
.section .data.rel.ro
.balign 8
.Lcc_live_definition:
 .quad .Lcc_live_kind,10,1
 .long 1,1,2,0
 .quad 0,0
.Lcc_live_initializer:
 .quad 0,.Lcc_live_initial

.text
/* Build a borrowed runner view for the shared creation/phase status writer. */
.Lcc_live_created:
 CF 112
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rsp
 xor eax,eax
 mov ecx,14
 rep stosq
 mov rax,[rbx+8]
 mov [rsp+8],rax
 mov [rsp+16],r12
 mov rax,[rbx]
 mov [rsp+40],rax
 call json_object_new_object@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lcc_created_live_bad
 mov rdi,[rbx+24]
 call json_object_new_uint64@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lcc_conversationId]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_created_live_bad
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lcc_input]
 mov rdx,[rbx+40]
 call .Lcc_copy
 test eax,eax
 jnz .Lcc_created_live_bad
 cmp qword ptr [rbx+32],0
 je .Lcc_created_live_state
 mov rdi,[rbx+32]
 call json_object_new_uint64@PLT
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lcc_owner]
 mov rdx,rax
 call .Lcc_set
 test eax,eax
 jnz .Lcc_created_live_bad
.Lcc_created_live_state:
 lea rdi,[rip+.Lcc_running_json]
 call json_tokener_parse@PLT
 mov [rsp+96],rax
 test rax,rax
 jz .Lcc_created_live_bad
 mov rdi,rsp
 mov rsi,r13
 mov rdx,rax
 xor ecx,ecx
 call .Lcc_live_update
 mov ebp,eax
 jmp .Lcc_created_live_end
.Lcc_created_live_bad:
 mov ebp,5
.Lcc_created_live_end:
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,[rsp+96]
 call json_object_put@PLT
 mov eax,ebp
 CE 112
