/* Pi Coding Agent system-prompt/resource-loader native adaptation.
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
#include "tools_layout.h"
.intel_syntax noprefix
.section .rodata
.Lpath: .asciz "path"
.Lread: .asciz "read"
.text
.globl pg_prompt_from_file
.type pg_prompt_from_file,@function
pg_prompt_from_file:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,56
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov r15,r8
 mov rbp,r9
 mov qword ptr [rsp],0
 mov qword ptr [rsp+48],0
 mov dword ptr [rsp+40],1
 test rbp,rbp
 jz .Lreturn
 mov qword ptr [rbp],0
 test r14,r14
 jz .Lreturn
 test r15,r15
 jz .Lreturn
 mov byte ptr [r14],0
 test rbx,rbx
 jz .Lreturn
 cmp qword ptr [rbx+PG_TL_SVC_AUTHORIZE],0
 je .Lreturn
 cmp qword ptr [rbx+PG_TL_SVC_EXECUTE],0
 je .Lreturn
 test r12,r12
 jz .Lreturn
 test r13,r13
 jz .Lreturn
 cmp r13,4096
 jae .Lreturn
 mov rdi,r12
 xor esi,esi
 mov rdx,r13
 call memchr@PLT
 test rax,rax
 jnz .Lreturn
 call json_object_new_object@PLT
 test rax,rax
 jz .Lreturn
 mov [rsp],rax
 mov rdi,r12
 mov esi,r13d
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Lcleanup
 mov [rsp+8],rax
 mov qword ptr [rsp+48],1
 mov rdi,[rsp]
 lea rsi,[rip+.Lpath]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lcleanup
 mov qword ptr [rsp+48],0
 mov rdi,[rsp]
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Lcleanup
 mov [rsp+16],rax
 mov rdi,rax
 call strlen@PLT
 cmp rax,4096
 jae .Lcleanup
 mov [rsp+24],rax
 mov rdi,[rbx]
 lea rsi,[rip+.Lread]
 mov edx,4
 mov rcx,[rsp+16]
 mov r8,rax
 call [rbx+PG_TL_SVC_AUTHORIZE]
 cmp eax,1
 jne .Lcleanup
 mov qword ptr [rsp+32],0
 mov rdi,[rbx]
 lea rsi,[rip+.Lread]
 mov edx,4
 mov rcx,[rsp+16]
 mov r8,[rsp+24]
 mov r9,r14
 lea rax,[rsp+32]
 push rax
 push r15
 call [rbx+PG_TL_SVC_EXECUTE]
 add rsp,16
 test eax,eax
 jnz .Lcleanup
 mov rax,[rsp+32]
 cmp rax,r15
 jae .Lcleanup
 cmp rax,4096
 jae .Lcleanup
 mov [rbp],rax
 mov byte ptr [r14+rax],0
 mov dword ptr [rsp+40],0
.Lcleanup:
 cmp qword ptr [rsp+48],0
 je .Lroot
 mov rdi,[rsp+8]
 call json_object_put@PLT
.Lroot:
 mov rdi,[rsp]
 test rdi,rdi
 jz .Lreturn
 call json_object_put@PLT
.Lreturn:
 cmp dword ptr [rsp+40],0
 je .Lout
 test rbp,rbp
 jz .Lout
 mov qword ptr [rbp],0
 test r14,r14
 jz .Lout
 test r15,r15
 jz .Lout
 mov byte ptr [r14],0
.Lout:
 mov eax,[rsp+40]
 add rsp,56
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_prompt_from_file,.-pg_prompt_from_file
.section .note.GNU-stack,"",@progbits
.text
.globl pg_prompt_workspace
.type pg_prompt_workspace,@function
pg_prompt_workspace:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,4096
 mov byte ptr [rsp],0
 sub rsp,72
 mov rbp,rsp
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r15,rcx
 xor r14d,r14d
 mov qword ptr [rbp+4112],0
 mov qword ptr [rbp+4128],0
 test rcx,rcx
 jz .Lpw_invalid
 mov qword ptr [rcx],0
 test r12,r12
 jz .Lpw_invalid
 test r13,r13
 jz .Lpw_invalid
 mov byte ptr [r12],0
 test rbx,rbx
 jz .Lpw_invalid
 cmp qword ptr [rbx],0
 je .Lpw_fail
 cmp qword ptr [rbx+16],0
 je .Lpw_fail
 mov rax,[rbx+24]
 test rax,rax
 jz .Lpw_fail
 cmp qword ptr [rax+PG_TL_REG_COUNT],PG_TL_LIMIT
 ja .Lpw_fail
 lea rdi,[rip+.Lpw_system_path]
 call .Lpw_read
 cmp eax,1
 je .Lpw_default
 test eax,eax
 jnz .Lpw_fail
 mov rdi,[rbp+4136]
 cmp byte ptr [rdi],0
 je .Lpw_default
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
 mov qword ptr [rbp+4112],1
 jmp .Lpw_addendum
.Lpw_default:
 lea rdi,[rip+.Lpw_preamble]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
 lea rdi,[rip+.Lpw_tools_open]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
 mov qword ptr [rbp+4120],0
.Lpw_tools:
 mov rax,[rbx+24]
 mov rcx,[rbp+4120]
 cmp rcx,[rax+PG_TL_REG_COUNT]
 jae .Lpw_tools_end
 imul rcx,PG_TL_DEF_BYTES
 mov rax,[rax+rcx]
 mov [rbp+4144],rax
 xor edx,edx
.Lpw_match:
 cmp edx,4
 jae .Lpw_tools_next
 mov [rbp+4152],rdx
 lea rcx,[rip+.Lpw_names]
 mov rsi,[rcx+rdx*8]
 mov rdi,[rbp+4144]
 call strcmp@PLT
 test eax,eax
 jz .Lpw_matched
 mov rdx,[rbp+4152]
 inc edx
 jmp .Lpw_match
.Lpw_matched:
 mov rcx,[rbp+4152]
 bts qword ptr [rbp+4128],rcx
 lea rax,[rip+.Lpw_snippets]
 mov rdi,[rax+rcx*8]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
.Lpw_tools_next:
 inc qword ptr [rbp+4120]
 jmp .Lpw_tools
.Lpw_tools_end:
 cmp qword ptr [rbp+4128],0
 jne .Lpw_tools_close
 lea rdi,[rip+.Lpw_none]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
.Lpw_tools_close:
 lea rdi,[rip+.Lpw_tools_suffix]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
 test qword ptr [rbp+4128],2
 jz .Lpw_guidelines
 lea rdi,[rip+.Lpw_search_rule]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
.Lpw_guidelines:
 mov qword ptr [rbp+4120],0
.Lpw_rule_loop:
 mov rcx,[rbp+4120]
 cmp ecx,4
 jae .Lpw_rules_end
 bt [rbp+4128],rcx
 jnc .Lpw_rule_next
 lea rax,[rip+.Lpw_rules]
 mov rdi,[rax+rcx*8]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
.Lpw_rule_next:
 inc qword ptr [rbp+4120]
 jmp .Lpw_rule_loop
.Lpw_rules_end:
 lea rdi,[rip+.Lpw_rules_suffix]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
.Lpw_addendum:
 lea rdi,[rip+.Lpw_append_path]
 call .Lpw_read
 cmp eax,1
 je .Lpw_context_begin
 test eax,eax
 jnz .Lpw_fail
 mov rax,[rbp+4136]
 cmp byte ptr [rax],0
 je .Lpw_context_begin
 lea rdi,[rip+.Lpw_addendum_open]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
 mov rdi,[rbp+4136]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
 lea rdi,[rip+.Lpw_addendum_close]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
.Lpw_context_begin:
 mov qword ptr [rbp+4120],0
.Lpw_context:
 mov rcx,[rbp+4120]
 cmp ecx,5
 jae .Lpw_cwd
 lea rax,[rip+.Lpw_context_names]
 mov rdi,[rax+rcx*8]
 call .Lpw_read
 cmp eax,1
 je .Lpw_context_next
 test eax,eax
 jnz .Lpw_fail
 lea rdi,[rip+.Lpw_context_open]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
 mov rdi,[rbp+4104]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
 lea rdi,[rip+.Lpw_context_path_end]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
 mov rdi,[rbp+4136]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
 lea rdi,[rip+.Lpw_context_close]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
 jmp .Lpw_cwd
.Lpw_context_next:
 inc qword ptr [rbp+4120]
 jmp .Lpw_context
.Lpw_cwd:
 lea rdi,[rip+.Lpw_cwd_open]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
 mov rdi,[rbx+32]
 mov rsi,[rbx+40]
 test rdi,rdi
 jz .Lpw_fail
 cmp rsi,4096
 jae .Lpw_fail
 call .Lpw_append_slice
 test eax,eax
 jnz .Lpw_fail
 lea rdi,[rip+.Lpw_cwd_close]
 call .Lpw_append
 test eax,eax
 jnz .Lpw_fail
 mov [r15],r14
 xor eax,eax
 jmp .Lpw_end
.Lpw_fail:
 mov byte ptr [r12],0
 mov qword ptr [r15],0
.Lpw_invalid:
 mov eax,1
.Lpw_end:
 add rsp,4168
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_prompt_workspace,.-pg_prompt_workspace
.Lpw_append:
 push rdi
 call strlen@PLT
 mov rsi,rax
 pop rdi
.Lpw_append_slice:
 lea rax,[r14+rsi]
 cmp rax,r14
 jb .Lpw_append_bad
 cmp rax,r13
 jae .Lpw_append_bad
 cmp rax,4096
 jae .Lpw_append_bad
 sub rsp,24
 mov [rsp],rsi
 mov rdx,rsi
 mov rsi,rdi
 lea rdi,[r12+r14]
 call memcpy@PLT
 add r14,[rsp]
 mov byte ptr [r12+r14],0
 add rsp,24
 xor eax,eax
 ret
.Lpw_append_bad:
 mov eax,1
 ret
.Lpw_read:
 sub rsp,8
 mov [rbp+4104],rdi
 mov rsi,rdi
 mov rdi,[rbx+8]
 call [rbx+16]
 test eax,eax
 jz .Lpw_read_missing
 cmp eax,1
 jne .Lpw_read_bad
 mov rdi,[rbx]
 mov rsi,[rbp+4104]
 mov rdi,rsi
 call strlen@PLT
 mov rdx,rax
 mov rdi,[rbx]
 mov rsi,[rbp+4104]
 mov rcx,rbp
 mov r8d,4096
 lea r9,[rbp+4096]
 call pg_prompt_from_file
 test eax,eax
 jnz .Lpw_read_bad
 mov rax,rbp
 cmp qword ptr [rbp+4096],3
 jb .Lpw_read_bom_done
 cmp byte ptr [rbp],0xef
 jne .Lpw_read_bom_done
 cmp word ptr [rbp+1],0xbfbb
 jne .Lpw_read_bom_done
 add rax,3
.Lpw_read_bom_done:
 mov [rbp+4136],rax
 xor eax,eax
 jmp .Lpw_read_end
.Lpw_read_missing:
 mov eax,1
 jmp .Lpw_read_end
.Lpw_read_bad:
 mov eax,2
.Lpw_read_end:
 add rsp,8
 ret
.section .rodata
.Lpw_preamble: .asciz "You are an expert coding assistant operating inside PanGu, a coding agent harness. You help users by reading files, executing commands, editing code, and writing new files."
.Lpw_tools_open: .asciz "\n\n<tools>\n"
.Lpw_none: .asciz "(none)\n"
.Lpw_tools_suffix: .asciz "\nIn addition to the tools above, you may have access to other custom tools depending on the project.\n</tools>\n\n<rules>\n"
.Lpw_search_rule: .asciz "- Use bash for file operations like ls, rg, find\n"
.Lpw_rules_suffix: .asciz "- Be concise in your responses\n- Show file paths clearly when working with files\n</rules>"
.Lpw_read_snippet: .asciz "- read: Read file contents\n"
.Lpw_bash_snippet: .asciz "- bash: Execute bash commands (ls, grep, find, etc.)\n"
.Lpw_edit_snippet: .asciz "- edit: Make precise file edits with exact text replacement, including multiple disjoint edits in one call\n"
.Lpw_write_snippet: .asciz "- write: Create or overwrite files\n"
.Lpw_read_rule: .asciz "- Use read to examine files instead of cat or sed.\n"
.Lpw_bash_rule: .asciz "- Commands execute in the configured workspace with PATH and LANG available.\n"
.Lpw_edit_rule: .asciz "- Use edit for precise changes (edits[].oldText must match exactly)\n- When changing multiple separate locations in one file, use one edit call with multiple entries in edits[] instead of multiple edit calls\n- Each edits[].oldText is matched against the original file, not after earlier edits are applied. Do not emit overlapping or nested edits. Merge nearby changes into one edit.\n- Keep edits[].oldText as small as possible while still being unique in the file. Do not pad with large unchanged regions.\n"
.Lpw_write_rule: .asciz "- Use write only for new files or complete rewrites.\n"
.Lpw_addendum_open: .asciz "\n\n<addendum>\n"
.Lpw_addendum_close: .asciz "\n</addendum>"
.Lpw_context_open: .asciz "\n\n<project_context>\nProject-specific instructions and guidelines:\n\n<project_instructions path=\""
.Lpw_context_path_end: .asciz "\">\n"
.Lpw_context_close: .asciz "\n</project_instructions>\n</project_context>"
.Lpw_cwd_open: .asciz "\n\n<cwd>\n"
.Lpw_cwd_close: .asciz "\n</cwd>"
.Lpw_system_path: .asciz ".pangu/SYSTEM.md"
.Lpw_append_path: .asciz ".pangu/APPEND_SYSTEM.md"
.Lpw_agents_override: .asciz "AGENTS.override.md"
.Lpw_agents: .asciz "AGENTS.md"
.Lpw_agents_caps: .asciz "AGENTS.MD"
.Lpw_claude: .asciz "CLAUDE.md"
.Lpw_claude_caps: .asciz "CLAUDE.MD"
.Lpw_read_name: .asciz "read"
.Lpw_bash_name: .asciz "bash"
.Lpw_edit_name: .asciz "edit"
.Lpw_write_name: .asciz "write"
.section .data.rel.ro
.Lpw_names: .quad .Lpw_read_name,.Lpw_bash_name,.Lpw_edit_name,.Lpw_write_name
.Lpw_snippets: .quad .Lpw_read_snippet,.Lpw_bash_snippet,.Lpw_edit_snippet,.Lpw_write_snippet
.Lpw_rules: .quad .Lpw_read_rule,.Lpw_bash_rule,.Lpw_edit_rule,.Lpw_write_rule
.Lpw_context_names: .quad .Lpw_agents_override,.Lpw_agents,.Lpw_agents_caps,.Lpw_claude,.Lpw_claude_caps
