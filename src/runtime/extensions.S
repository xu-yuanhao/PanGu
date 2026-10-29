/* Pi Durable harness/{registry,agent,prompt}.ts; d5b3de22d0e24a3e9c38a9410d3329b099717283.
   MIT, docs/licenses/pi.md. */
.intel_syntax noprefix
.macro XF n
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\n
.endm
.macro XE n
 add rsp,\n
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.text
.globl pg_extensions_close
.type pg_extensions_close,@function
pg_extensions_close:
 push rbx
 mov rbx,rdi
 mov rdi,[rbx+1184]
 call free@PLT
 mov rdi,[rbx+1200]
 call free@PLT
 cmp qword ptr [rbx+1240],0
 je .Lx_close_clear
 mov rdi,[rbx+1232]
 call [rbx+1240]
.Lx_close_clear:
 mov rdi,rbx
 xor eax,eax
 mov ecx,156
 rep stosq
 pop rbx
 ret
/* Search structs whose first field is a C-string key; return count if absent. */
.Lx_find:
 XF 16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 xor ebp,ebp
.Lx_find_each:
 cmp rbp,r12
 jae .Lx_find_end
 mov rax,rbp
 imul rax,r13
 mov rdi,[rbx+rax]
 mov rsi,r14
 call strcmp@PLT
 test eax,eax
 jz .Lx_find_end
 inc rbp
 jmp .Lx_find_each
.Lx_find_end:
 mov rax,rbp
 XE 16
/* Exact byte membership in an optional stable-name array. */
.Lx_contains:
 XF 16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
 test rbx,rbx
 jz .Lx_contains_no
.Lx_contains_each:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lx_contains_no
 mov rdi,rbx
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov [rsp],rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 cmp rax,r13
 jne .Lx_contains_next
 mov rdi,[rsp]
 call json_object_get_string@PLT
 test rax,rax
 jz .Lx_contains_next
 mov rdi,rax
 mov rsi,r12
 mov rdx,r13
 call memcmp@PLT
 test eax,eax
 jz .Lx_contains_yes
.Lx_contains_next:
 inc r14
 jmp .Lx_contains_each
.Lx_contains_no:
 xor eax,eax
 jmp .Lx_contains_end
.Lx_contains_yes:
 mov eax,1
.Lx_contains_end:
 XE 16
/* Select a stable name once; missing installed names are skipped. */
.Lx_choose:
 XF 32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rcx
 mov rsi,r12
 mov rdx,r13
 call .Lx_contains
 test eax,eax
 jnz .Lx_choose_ok
 lea r14,[rip+.Lx_coding]
 cmp r13,6
 jne .Lx_choose_installed
 mov rdi,r12
 lea rsi,[rip+.Lx_coding_name]
 mov edx,6
 call memcmp@PLT
 test eax,eax
 jz .Lx_choose_found
.Lx_choose_installed:
 mov rax,[rbx+1216]
 test rax,rax
 jz .Lx_choose_ok
 mov [rsp],rax
 xor ebp,ebp
.Lx_choose_each:
 mov rax,[rsp]
 cmp rbp,[rax+16]
 jae .Lx_choose_ok
 mov rax,[rax+8]
 mov r14,[rax+rbp*8]
 mov rdi,[r14]
 call strlen@PLT
 cmp rax,r13
 jne .Lx_choose_next
 mov rdi,[r14]
 mov rsi,r12
 mov rdx,r13
 call memcmp@PLT
 test eax,eax
 jz .Lx_choose_found
.Lx_choose_next:
 inc rbp
 jmp .Lx_choose_each
.Lx_choose_found:
 xor ebp,ebp
.Lx_choose_duplicate:
 cmp rbp,[rbx+1208]
 jae .Lx_choose_append
 mov rax,[rbx+1200]
 cmp r14,[rax+rbp*8]
 je .Lx_choose_ok
 inc rbp
 jmp .Lx_choose_duplicate
.Lx_choose_append:
 mov rax,[rbx+1200]
 mov rcx,[rbx+1208]
 mov [rax+rcx*8],r14
 inc qword ptr [rbx+1208]
.Lx_choose_ok:
 xor eax,eax
 XE 32
/* Select every name of a validated array, with an optional remove list. */
.Lx_select_array:
 XF 16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
 test r12,r12
 jz .Lx_array_ok
 mov rdi,r12
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lx_array_bad
.Lx_array_each:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lx_array_ok
 mov rdi,r12
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov [rsp],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lx_array_bad
 mov rdi,[rsp]
 call json_object_get_string@PLT
 mov rbp,rax
 mov rdi,[rsp]
 call json_object_get_string_len@PLT
 mov rdi,rbx
 mov rsi,rbp
 mov edx,eax
 mov rcx,r13
 call .Lx_choose
 inc r14
 jmp .Lx_array_each
.Lx_array_bad:
 mov eax,5
 jmp .Lx_array_end
.Lx_array_ok:
 xor eax,eax
.Lx_array_end:
 XE 16
/* Install or replace in place, retaining first insertion order. */
.Lx_tool:
 XF 16
 mov rbx,rdi
 mov r12,rsi
 mov rsi,[rbx+1152]
 mov edx,72
 mov rcx,[r12]
 call .Lx_find
 cmp rax,16
 jae .Lx_tool_bad
 cmp rax,[rbx+1152]
 jb .Lx_tool_copy
 inc qword ptr [rbx+1152]
.Lx_tool_copy:
 imul rax,rax,72
 lea rdi,[rbx+rax]
 mov rsi,r12
 mov ecx,9
 rep movsq
 xor eax,eax
 jmp .Lx_tool_end
.Lx_tool_bad:
 mov eax,5
.Lx_tool_end:
 XE 16
.Lx_section:
 XF 16
 mov rbx,rdi
 mov r12,rsi
 mov rdi,[rbx+1184]
 mov rsi,[rbx+1192]
 mov edx,40
 mov rcx,[r12]
 call .Lx_find
 mov r13,rax
 cmp rax,[rbx+1192]
 jb .Lx_section_copy
 cmp rax,2147483647
 jae .Lx_section_bad
 lea rsi,[rax+1]
 imul rsi,rsi,40
 mov rdi,[rbx+1184]
 call realloc@PLT
 test rax,rax
 jz .Lx_section_bad
 mov [rbx+1184],rax
 inc qword ptr [rbx+1192]
.Lx_section_copy:
 mov rdi,r13
 imul rdi,rdi,40
 add rdi,[rbx+1184]
 mov rsi,r12
 mov ecx,5
 rep movsq
 xor eax,eax
 jmp .Lx_section_end
.Lx_section_bad:
 mov eax,5
.Lx_section_end:
 XE 16
/* key syntax from registry.ts; instructions belongs to AgentState. */
.Lx_key:
 test rdi,rdi
 jz .Lx_key_bad
 mov al,[rdi]
 cmp al,'a'
 jb .Lx_key_bad
 cmp al,'z'
 ja .Lx_key_bad
 xor ecx,ecx
.Lx_key_each:
 mov al,[rdi+rcx]
 test al,al
 jz .Lx_key_reserved
 cmp al,'a'
 jb .Lx_key_other
 cmp al,'z'
 jbe .Lx_key_next
.Lx_key_other:
 cmp al,'0'
 jb .Lx_key_symbol
 cmp al,'9'
 jbe .Lx_key_next
.Lx_key_symbol:
 cmp al,'_'
 je .Lx_key_next
 cmp al,'-'
 jne .Lx_key_bad
.Lx_key_next:
 inc rcx
 jmp .Lx_key_each
.Lx_key_reserved:
 sub rsp,8
 lea rsi,[rip+.Lx_instructions]
 call strcmp@PLT
 test eax,eax
 setnz al
 movzx eax,al
 add rsp,8
 ret
.Lx_key_bad:
 xor eax,eax
 ret
/* Provider is trusted native code; descriptor shape and intra-extension names
   are validated before any renderer/wrapper executes. */
.Lx_validate:
 XF 32
 mov rbx,rdi
 test rbx,rbx
 jz .Lx_validate_ok
 cmp dword ptr [rbx],6
 jne .Lx_validate_bad
 cmp dword ptr [rbx+4],72
 jne .Lx_validate_bad
 cmp qword ptr [rbx+16],2147483646
 ja .Lx_validate_bad
 cmp qword ptr [rbx+16],0
 je .Lx_validate_ok
 cmp qword ptr [rbx+8],0
 je .Lx_validate_bad
 xor r12d,r12d
.Lx_validate_extension:
 cmp r12,[rbx+16]
 jae .Lx_validate_ok
 mov rax,[rbx+8]
 mov r13,[rax+r12*8]
 test r13,r13
 jz .Lx_validate_bad
 cmp qword ptr [r13],0
 je .Lx_validate_bad
 mov rdi,[r13]
 cmp byte ptr [rdi],0
 je .Lx_validate_bad
 lea rsi,[rip+.Lx_coding_name]
 call strcmp@PLT
 test eax,eax
 jz .Lx_validate_bad
 xor r14d,r14d
.Lx_validate_previous:
 cmp r14,r12
 jae .Lx_validate_tools
 mov rax,[rbx+8]
 mov rax,[rax+r14*8]
 mov rdi,[r13]
 mov rsi,[rax]
 call strcmp@PLT
 test eax,eax
 jz .Lx_validate_bad
 inc r14
 jmp .Lx_validate_previous
.Lx_validate_tools:
 cmp qword ptr [r13+16],16
 ja .Lx_validate_bad
 cmp qword ptr [r13+16],0
 je .Lx_validate_sections_start
 cmp qword ptr [r13+8],0
 je .Lx_validate_bad
 xor r14d,r14d
.Lx_validate_tool_each:
 cmp r14,[r13+16]
 jae .Lx_validate_sections_start
 imul rax,r14,72
 mov rdx,[r13+8]
 mov rcx,[rdx+rax]
 test rcx,rcx
 jz .Lx_validate_bad
 mov rdi,rdx
 mov rsi,r14
 mov edx,72
 call .Lx_find
 cmp rax,r14
 jne .Lx_validate_bad
 inc r14
 jmp .Lx_validate_tool_each
.Lx_validate_sections_start:
 cmp qword ptr [r13+32],2147483647
 ja .Lx_validate_bad
 cmp qword ptr [r13+32],0
 je .Lx_validate_wraps
 cmp qword ptr [r13+24],0
 je .Lx_validate_bad
 xor r14d,r14d
.Lx_validate_section_each:
 cmp r14,[r13+32]
 jae .Lx_validate_wraps
 mov rax,r14
 imul rax,rax,40
 add rax,[r13+24]
 mov [rsp],rax
 cmp qword ptr [rax+16],0
 je .Lx_validate_bad
 mov rdi,[rax]
 call .Lx_key
 test eax,eax
 jz .Lx_validate_bad
 mov rax,[rsp]
 mov rcx,[rax]
 mov rdi,[r13+24]
 mov rsi,r14
 mov edx,40
 call .Lx_find
 cmp rax,r14
 jne .Lx_validate_bad
 inc r14
 jmp .Lx_validate_section_each
.Lx_validate_wraps:
 cmp qword ptr [r13+48],2147483647
 ja .Lx_validate_bad
 cmp qword ptr [r13+48],0
 je .Lx_validate_next
 cmp qword ptr [r13+40],0
 je .Lx_validate_bad
 xor r14d,r14d
.Lx_validate_wrap_each:
 cmp r14,[r13+48]
 jae .Lx_validate_next
 mov rax,r14
 shl rax,5
 add rax,[r13+40]
 cmp dword ptr [rax],1
 jb .Lx_validate_bad
 cmp dword ptr [rax],2
 ja .Lx_validate_bad
 cmp qword ptr [rax+8],0
 je .Lx_validate_bad
 cmp qword ptr [rax+24],0
 je .Lx_validate_bad
 inc r14
 jmp .Lx_validate_wrap_each
.Lx_validate_next:
 cmp qword ptr [r13+64],2147483647
 ja .Lx_validate_bad
 cmp qword ptr [r13+64],0
 je .Lx_validate_next_done
 cmp qword ptr [r13+56],0
 je .Lx_validate_bad
 xor r14d,r14d
.Lx_validate_hook_each:
 cmp r14,[r13+64]
 jae .Lx_validate_next_done
 mov rax,r14
 shl rax,5
 add rax,[r13+56]
 cmp dword ptr [rax],1
 jb .Lx_validate_bad
 cmp dword ptr [rax],7
 ja .Lx_validate_bad
 cmp qword ptr [rax+16],0
 je .Lx_validate_bad
 inc r14
 jmp .Lx_validate_hook_each
.Lx_validate_next_done:
 inc r12
 jmp .Lx_validate_extension
.Lx_validate_bad:
 mov eax,5
 jmp .Lx_validate_end
.Lx_validate_ok:
 xor eax,eax
.Lx_validate_end:
 XE 32
.globl pg_extensions_resolve
.type pg_extensions_resolve,@function
pg_extensions_resolve:
 XF 1280
 mov rbx,rcx
 mov r12,rdi
 mov r13,rdx
 mov [rsp+1184],rsi
 mov rdi,rbx
 xor eax,eax
 mov ecx,156
 rep stosq
 test r12,r12
 jz .Lx_resolve_provider
 cmp dword ptr [r12],6
 jne .Lx_resolve_bad
 cmp dword ptr [r12+4],72
 jne .Lx_resolve_bad
 cmp qword ptr [r12+56],0
 je .Lx_resolve_provider
 cmp qword ptr [r12+64],0
 je .Lx_resolve_bad
 mov rdi,[r12+48]
 lea rsi,[rsp+1240]
 lea rdx,[rbx+1232]
 call [r12+56]
 test eax,eax
 jnz .Lx_resolve_bad
 mov rax,[r12+64]
 mov [rbx+1240],rax
 mov r12,[rsp+1240]
.Lx_resolve_provider:
 mov [rbx+1216],r12
 mov rdi,r12
 call .Lx_validate
 test eax,eax
 jnz .Lx_resolve_bad
 mov esi,1
 test r12,r12
 jz .Lx_resolve_allocate
 add rsi,[r12+16]
.Lx_resolve_allocate:
 mov rdi,rsi
 mov esi,8
 call calloc@PLT
 mov [rbx+1200],rax
 test rax,rax
 jz .Lx_resolve_bad
 mov rdi,[rsp+1184]
 lea rsi,[rip+.Lx_extensions]
 call json_object_object_get@PLT
 mov [rsp+1192],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lx_resolve_edits
 mov rdi,rbx
 mov rsi,[rsp+1192]
 xor edx,edx
 call .Lx_select_array
 test eax,eax
 jnz .Lx_resolve_bad
 jmp .Lx_resolve_compose
.Lx_resolve_edits:
 mov qword ptr [rsp+1200],0
 mov qword ptr [rsp+1208],0
 cmp qword ptr [rsp+1192],0
 je .Lx_resolve_defaults
 mov rdi,[rsp+1192]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lx_resolve_bad
 mov rdi,[rsp+1192]
 lea rsi,[rip+.Lx_remove]
 call json_object_object_get@PLT
 mov [rsp+1200],rax
 test rax,rax
 jz .Lx_resolve_add
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lx_resolve_bad
.Lx_resolve_add:
 mov rdi,[rsp+1192]
 lea rsi,[rip+.Lx_add]
 call json_object_object_get@PLT
 mov [rsp+1208],rax
.Lx_resolve_defaults:
 test r12,r12
 jz .Lx_resolve_all
 cmp qword ptr [r12+24],0
 je .Lx_resolve_all
 mov rdi,rbx
 mov rsi,[r12+24]
 mov rdx,[rsp+1200]
 call .Lx_select_array
 test eax,eax
 jnz .Lx_resolve_bad
 jmp .Lx_resolve_added
.Lx_resolve_all:
 mov rdi,rbx
 lea rsi,[rip+.Lx_coding_name]
 mov edx,6
 mov rcx,[rsp+1200]
 call .Lx_choose
 test r12,r12
 jz .Lx_resolve_added
 xor r14d,r14d
.Lx_resolve_all_each:
 cmp r14,[r12+16]
 jae .Lx_resolve_added
 mov rax,[r12+8]
 mov rax,[rax+r14*8]
 mov rbp,[rax]
 mov rdi,rbp
 call strlen@PLT
 mov rdi,rbx
 mov rsi,rbp
 mov rdx,rax
 mov rcx,[rsp+1200]
 call .Lx_choose
 inc r14
 jmp .Lx_resolve_all_each
.Lx_resolve_added:
 mov rdi,rbx
 mov rsi,[rsp+1208]
 mov rdx,[rsp+1200]
 call .Lx_select_array
 test eax,eax
 jnz .Lx_resolve_bad
.Lx_resolve_compose:
 xor r14d,r14d
.Lx_compose_each:
 cmp r14,[rbx+1208]
 jae .Lx_resolve_wraps
 mov rax,[rbx+1200]
 mov rbp,[rax+r14*8]
 lea rax,[rip+.Lx_coding]
 cmp rbp,rax
 jne .Lx_compose_extension
 mov [rsp+1216],r13
 mov rax,[r13+1152]
 mov [rsp+1224],rax
 jmp .Lx_compose_tools_start
.Lx_compose_extension:
 mov rax,[rbp+8]
 mov [rsp+1216],rax
 mov rax,[rbp+16]
 mov [rsp+1224],rax
.Lx_compose_tools_start:
 mov qword ptr [rsp+1232],0
.Lx_compose_tools:
 mov rax,[rsp+1232]
 cmp rax,[rsp+1224]
 jae .Lx_compose_sections_start
 imul rsi,rax,72
 add rsi,[rsp+1216]
 mov rdi,rbx
 call .Lx_tool
 test eax,eax
 jnz .Lx_resolve_bad
 inc qword ptr [rsp+1232]
 jmp .Lx_compose_tools
.Lx_compose_sections_start:
 mov qword ptr [rsp+1232],0
.Lx_compose_sections:
 mov rax,[rsp+1232]
 cmp rax,[rbp+32]
 jae .Lx_compose_next
 mov rsi,rax
 imul rsi,rsi,40
 add rsi,[rbp+24]
 mov rdi,rbx
 call .Lx_section
 test eax,eax
 jnz .Lx_resolve_bad
 inc qword ptr [rsp+1232]
 jmp .Lx_compose_sections
.Lx_compose_next:
 inc r14
 jmp .Lx_compose_each
.Lx_resolve_wraps:
 mov rdi,rbx
 call .Lx_wraps
 test eax,eax
 jnz .Lx_resolve_bad
 /* Validate final tool descriptors into independent fresh registry storage. */
 mov rdi,rsp
 mov rsi,rbx
 mov rdx,[rbx+1152]
 call pg_tool_registry_init@PLT
 test eax,eax
 jnz .Lx_resolve_bad
 mov rdi,rbx
 mov rsi,rsp
 mov ecx,148
 rep movsq
 mov dword ptr [rbx+1224],1
 mov rdx,[rbx+1152]
 cmp rdx,[r13+1152]
 jne .Lx_resolve_ok
 imul rdx,rdx,72
 mov rdi,rbx
 mov rsi,r13
 call memcmp@PLT
 test eax,eax
 jnz .Lx_resolve_ok
 mov dword ptr [rbx+1224],0
.Lx_resolve_ok:
 xor eax,eax
 jmp .Lx_resolve_end
.Lx_resolve_bad:
 mov rdi,rbx
 call pg_extensions_close
 mov eax,5
.Lx_resolve_end:
 XE 1280
.section .rodata
.Lx_coding_name: .asciz "coding"
.Lx_extensions: .asciz "extensions"
.Lx_instructions: .asciz "instructions"
.Lx_remove: .asciz "remove"
.Lx_add: .asciz "add"
.Lx_system: .asciz "systemPrompt"
.Lx_open_tag: .asciz "<%s>\n"
.Lx_close_tag: .asciz "\n</%s>"
.section .data.rel.ro
.balign 8
.Lx_coding: .quad .Lx_coding_name,0,0,0,0,0,0,0,0,0,0
.section .note.GNU-stack,"",@progbits
.text
/* Report is observational; native callbacks return errors instead of throwing. */
.Lx_report:
 mov rax,[rdi+1216]
 test rax,rax
 jz .Lx_report_end
 cmp qword ptr [rax+40],0
 je .Lx_report_end
 mov rdi,[rax+32]
 jmp qword ptr [rax+40]
.Lx_report_end:
 ret
.Lx_wraps:
 XF 144
 mov rbx,rdi
 xor r12d,r12d
.Lx_wrap_extension:
 cmp r12,[rbx+1208]
 jae .Lx_wrap_ok
 mov rax,[rbx+1200]
 mov r13,[rax+r12*8]
 xor r14d,r14d
.Lx_wrap_each:
 cmp r14,[r13+48]
 jae .Lx_wrap_next_extension
 mov rbp,r14
 shl rbp,5
 add rbp,[r13+40]
 mov [rsp+104],rbp
 mov qword ptr [rsp+112],72
 mov rdi,rbx
 mov rsi,[rbx+1152]
 cmp dword ptr [rbp],1
 je .Lx_wrap_find
 mov qword ptr [rsp+112],40
 mov rdi,[rbx+1184]
 mov rsi,[rbx+1192]
.Lx_wrap_find:
 mov [rsp+120],rdi
 mov [rsp+128],rsi
 mov edx,[rsp+112]
 mov rcx,[rbp+8]
 call .Lx_find
 cmp rax,[rsp+128]
 jae .Lx_wrap_next
 mov [rsp+96],rax
 imul rax,[rsp+112]
 add rax,[rsp+120]
 mov [rsp+88],rax
 mov rdi,[rbp+16]
 mov rsi,rax
 mov rdx,rsp
 call [rbp+24]
 mov [rsp+136],eax
 test eax,eax
 jnz .Lx_wrap_drop
 mov rdi,[rsp]
 test rdi,rdi
 jz .Lx_wrap_renamed
 mov rsi,[rbp+8]
 call strcmp@PLT
 test eax,eax
 jnz .Lx_wrap_renamed
 cmp qword ptr [rsp+112],40
 jne .Lx_wrap_copy
 cmp qword ptr [rsp+16],0
 je .Lx_wrap_renamed
.Lx_wrap_copy:
 mov rdi,[rsp+88]
 mov rsi,rsp
 mov ecx,[rsp+112]
 rep movsb
 jmp .Lx_wrap_next
.Lx_wrap_renamed:
 mov dword ptr [rsp+136],5
.Lx_wrap_drop:
 mov rdi,[rsp+88]
 mov rsi,rdi
 add rsi,[rsp+112]
 mov rdx,[rsp+128]
 sub rdx,[rsp+96]
 dec rdx
 imul rdx,[rsp+112]
 call memmove@PLT
 cmp dword ptr [rbp],1
 jne .Lx_wrap_drop_section
 dec qword ptr [rbx+1152]
 jmp .Lx_wrap_report
.Lx_wrap_drop_section:
 dec qword ptr [rbx+1192]
.Lx_wrap_report:
 mov rdi,rbx
 mov rsi,[r13]
 mov rdx,[rbp+8]
 mov ecx,[rsp+136]
 call .Lx_report
.Lx_wrap_next:
 inc r14
 jmp .Lx_wrap_each
.Lx_wrap_next_extension:
 inc r12
 jmp .Lx_wrap_extension
.Lx_wrap_ok:
 xor eax,eax
 XE 144
/* Render sections into the saved request, preserving byte lengths through the
   existing owned JSON string appender. */
.globl pg_extension_prepare_poll
.type pg_extension_prepare_poll,@function
pg_extension_prepare_poll:
 XF 80
 mov rbx,rdi
 mov r13,rsi
 mov rdi,rsp
 xor eax,eax
 mov ecx,10
 rep stosq
 mov [rsp+72],rbx
 mov [rbx+64],r13
 mov qword ptr [r13],0
 mov r12,[rbx+8]
 lea r13,[rbx+16]
 mov rbx,[rbx]
 cmp qword ptr [r13],0
 je .Lx_prepare_initialize
 mov rax,[rsp+72]
 cmp dword ptr [rax+60],0
 jne .Lx_prepare_completed
 mov r14,[rax+24]
 mov rcx,[rax+32]
 mov [rsp+16],rcx
 mov rcx,[rax+40]
 mov [rsp+56],rcx
 mov qword ptr [rax+32],0
 mov qword ptr [rax+40],0
 jmp .Lx_prepare_section
.Lx_prepare_initialize:
 mov rdi,r12
 mov rsi,r13
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lx_prepare_bad
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lx_prepare_bad
 xor r14d,r14d
.Lx_prepare_name:
 cmp r14,[rbx+1208]
 jae .Lx_prepare_names_done
 mov rax,[rbx+1200]
 mov rax,[rax+r14*8]
 mov rdi,[rax]
 call json_object_new_string@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lx_prepare_bad
 mov rdi,[rsp]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lx_prepare_bad
 mov qword ptr [rsp+8],0
 inc r14
 jmp .Lx_prepare_name
.Lx_prepare_names_done:
 mov rdi,[r13]
 lea rsi,[rip+.Lx_extensions]
 mov rdx,[rsp]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lx_prepare_bad
 mov qword ptr [rsp],0
 mov rdi,[r13]
 lea rsi,[rip+.Lx_system]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+16],rax
 call json_object_new_object@PLT
 mov [rsp+56],rax
 test rax,rax
 jz .Lx_prepare_bad
 mov rdi,[rsp+16]
 test rdi,rdi
 jz .Lx_prepare_section_start
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lx_prepare_section_start
 mov rdi,[rsp+16]
 call json_object_get@PLT
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lx_pangu_section]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lx_prepare_bad
.Lx_prepare_section_start:
 xor r14d,r14d
.Lx_prepare_section:
 cmp r14,[rbx+1192]
 jae .Lx_prepare_prompt_done
 mov rbp,r14
 imul rbp,rbp,40
 add rbp,[rbx+1184]
 mov qword ptr [rsp+24],0
 mov rdi,[rbp+8]
 mov rsi,r12
 lea rdx,[rsp+24]
 call [rbp+16]
 cmp eax,1
 je .Lx_render_pending
 mov rcx,[rsp+72]
 mov dword ptr [rcx+56],0
 mov qword ptr [rcx+48],0
 test eax,eax
 jnz .Lx_render_report
 cmp qword ptr [rsp+24],0
 je .Lx_render_next
 mov rdi,[rsp+24]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lx_render_invalid
 cmp dword ptr [rbp+24],0
 je .Lx_render_append
 lea rdi,[rsp+32]
 lea rsi,[rip+.Lx_open_tag]
 mov rdx,[rbp]
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lx_prepare_bad
 mov rdi,[rsp+32]
 call json_object_new_string@PLT
 mov [rsp+40],rax
 mov rdi,[rsp+32]
 call free@PLT
 mov qword ptr [rsp+32],0
 cmp qword ptr [rsp+40],0
 je .Lx_prepare_bad
 lea rdi,[rsp+40]
 mov rsi,[rsp+24]
 xor edx,edx
 call pg_context_append@PLT
 test eax,eax
 jnz .Lx_prepare_bad
 lea rdi,[rsp+32]
 lea rsi,[rip+.Lx_close_tag]
 mov rdx,[rbp]
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lx_prepare_bad
 mov rdi,[rsp+32]
 call json_object_new_string@PLT
 mov [rsp+48],rax
 mov rdi,[rsp+32]
 call free@PLT
 mov qword ptr [rsp+32],0
 cmp qword ptr [rsp+48],0
 je .Lx_prepare_bad
 lea rdi,[rsp+40]
 mov rsi,[rsp+48]
 xor edx,edx
 call pg_context_append@PLT
 test eax,eax
 jnz .Lx_prepare_bad
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov qword ptr [rsp+48],0
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rax,[rsp+40]
 mov [rsp+24],rax
 mov qword ptr [rsp+40],0
.Lx_render_append:
 mov rdi,[rsp+24]
 call json_object_get@PLT
 mov rdi,[rsp+56]
 mov rsi,[rbp]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lx_prepare_bad
 lea rdi,[rsp+16]
 mov rsi,[rsp+24]
 mov edx,2
 call pg_context_append@PLT
 test eax,eax
 jnz .Lx_prepare_bad
 jmp .Lx_render_next
.Lx_render_invalid:
 mov eax,5
.Lx_render_report:
 mov [rsp+64],eax
 mov rdi,rbx
 mov rsi,[rbp]
 call .Lx_section_owner
 mov rsi,rax
 mov rdi,rbx
 mov rdx,[rbp]
 mov ecx,[rsp+64]
 call .Lx_report
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov qword ptr [rsp+24],0
 mov rdi,r12
 lea rsi,[rip+.Lx_shown]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lx_render_next
 mov rdi,rax
 mov rsi,[rbp]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lx_render_next
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+24],rax
 jmp .Lx_render_append
.Lx_render_next:
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov qword ptr [rsp+24],0
 inc r14
 jmp .Lx_prepare_section
.Lx_prepare_prompt_done:
 mov rdi,[r13]
 lea rsi,[rip+.Lx_prompt_sections]
 mov rdx,[rsp+56]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lx_prepare_bad
 mov qword ptr [rsp+56],0
 mov rdi,[r13]
 lea rsi,[rip+.Lx_system]
 mov rdx,[rsp+16]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lx_prepare_bad
 mov qword ptr [rsp+16],0
 mov rax,[rsp+72]
 mov dword ptr [rax+60],1
.Lx_prepare_completed:
 mov rdi,[r13]
 call json_object_get@PLT
 mov rcx,[rsp+72]
 mov rcx,[rcx+64]
 mov [rcx],rax
 xor ebp,ebp
 jmp .Lx_prepare_end
.Lx_render_pending:
 cmp qword ptr [rbp+32],0
 je .Lx_render_invalid
 mov rax,[rsp+72]
 mov [rax+24],r14
 mov rcx,[rsp+16]
 mov [rax+32],rcx
 mov rcx,[rsp+56]
 mov [rax+40],rcx
 mov [rax+48],rbp
 mov dword ptr [rax+56],1
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+56],0
 mov ebp,1
 jmp .Lx_prepare_end
.Lx_prepare_bad:
 mov rdi,[r13]
 call json_object_put@PLT
 mov qword ptr [r13],0
 mov ebp,-5
.Lx_prepare_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call free@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov eax,ebp
 XE 80
.section .rodata
.Lx_render_name: .asciz "section"
.Lx_shown: .asciz "shown"
.Lx_prompt_sections: .asciz "promptSections"
.Lx_pangu_section: .asciz "pangu"

.text
/* Same owner rule as agent.ts sectionExtension: the last selected extension
   providing or wrapping this key supplies its report identity. */
.Lx_section_owner:
 XF 16
 mov rbx,rdi
 mov r12,rsi
 lea rbp,[rip+.Lx_render_name]
 xor r13d,r13d
.Lx_owner_extension:
 cmp r13,[rbx+1208]
 jae .Lx_owner_end
 mov rax,[rbx+1200]
 mov r14,[rax+r13*8]
 mov rdi,[r14+24]
 mov rsi,[r14+32]
 mov edx,40
 mov rcx,r12
 call .Lx_find
 cmp rax,[r14+32]
 jae .Lx_owner_wraps
 mov rbp,[r14]
.Lx_owner_wraps:
 mov qword ptr [rsp],0
.Lx_owner_wrap:
 mov rax,[rsp]
 cmp rax,[r14+48]
 jae .Lx_owner_next
 shl rax,5
 add rax,[r14+40]
 cmp dword ptr [rax],2
 jne .Lx_owner_wrap_next
 mov rdi,[rax+8]
 mov rsi,r12
 call strcmp@PLT
 test eax,eax
 jnz .Lx_owner_wrap_next
 mov rbp,[r14]
.Lx_owner_wrap_next:
 inc qword ptr [rsp]
 jmp .Lx_owner_wrap
.Lx_owner_next:
 inc r13
 jmp .Lx_owner_extension
.Lx_owner_end:
 mov rax,rbp
 XE 16

.text
/* Resolve explicit byte names to a descriptor without assuming NUL termination. */
.Lx_descriptor:
 XF 16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
.Lx_descriptor_each:
 cmp r14,[rbx+1152]
 jae .Lx_descriptor_missing
 imul rbp,r14,72
 add rbp,rbx
 mov rdi,[rbp]
 call strlen@PLT
 cmp rax,r13
 jne .Lx_descriptor_next
 mov rdi,[rbp]
 mov rsi,r12
 mov rdx,r13
 call memcmp@PLT
 test eax,eax
 jz .Lx_descriptor_end
.Lx_descriptor_next:
 inc r14
 jmp .Lx_descriptor_each
.Lx_descriptor_missing:
 xor ebp,ebp
.Lx_descriptor_end:
 mov rax,rbp
 XE 16
/* Restore native service arguments, returning the selected descriptor in rax. */
.Lx_dispatch_route:
 sub rsp,72
 mov [rsp],rdi
 mov [rsp+8],rsi
 mov [rsp+16],rdx
 mov [rsp+24],rcx
 mov [rsp+32],r8
 mov [rsp+40],r9
 mov rdi,[rdi+112]
 call .Lx_descriptor
 mov [rsp+48],rax
 test rax,rax
 jz .Lx_dispatch_composed
 mov rdi,[rsp]
 mov rdi,[rdi+120]
 mov rsi,[rsp+8]
 mov rdx,[rsp+16]
 call .Lx_descriptor
 test rax,rax
 jz .Lx_dispatch_composed
 mov rdi,rax
 mov rsi,[rsp+48]
 mov edx,72
 call memcmp@PLT
 test eax,eax
 jnz .Lx_dispatch_composed
 mov rax,[rsp]
 jmp .Lx_dispatch_restore
.Lx_dispatch_composed:
 mov rax,[rsp]
 add rax,56
.Lx_dispatch_restore:
 mov rdi,[rsp]
 mov rsi,[rsp+8]
 mov rdx,[rsp+16]
 mov rcx,[rsp+24]
 mov r8,[rsp+32]
 mov r9,[rsp+40]
 add rsp,72
 ret
.globl pg_extension_tools_service
.type pg_extension_tools_service,@function
pg_extension_tools_service:
 mov [rdi],rsi
 lea rax,[rip+.Lx_dispatch_authorize]
 mov [rdi+8],rax
 lea rax,[rip+.Lx_dispatch_execute]
 mov [rdi+16],rax
 lea rax,[rip+.Lx_dispatch_replay]
 mov [rdi+24],rax
 lea rax,[rip+.Lx_dispatch_next]
 mov [rdi+32],rax
 lea rax,[rip+.Lx_dispatch_cancel]
 mov [rdi+40],rax
 lea rax,[rip+.Lx_dispatch_close]
 mov [rdi+48],rax
 mov rax,rdi
 ret
.Lx_dispatch_authorize:
 sub rsp,8
 call .Lx_dispatch_route
 add rsp,8
 mov rdi,[rax]
 jmp qword ptr [rax+8]
.Lx_dispatch_execute:
 sub rsp,8
 call .Lx_dispatch_route
 add rsp,8
 mov [rdi+128],rax
 mov rdi,[rax]
 jmp qword ptr [rax+16]
.Lx_dispatch_replay:
 sub rsp,8
 call .Lx_dispatch_route
 add rsp,8
 mov rdi,[rax]
 mov rax,[rax+24]
 test rax,rax
 jz .Lx_dispatch_return
 jmp rax
.Lx_dispatch_next:
 mov rax,[rdi+128]
 test rax,rax
 jz .Lx_dispatch_no_next
 mov rdi,[rax]
 mov rax,[rax+32]
 test rax,rax
 jz .Lx_dispatch_no_next
 jmp rax
.Lx_dispatch_no_next:
 mov eax,1
 ret
.Lx_dispatch_cancel:
 mov rax,[rdi+128]
 test rax,rax
 jz .Lx_dispatch_return
 mov rdi,[rax]
 mov rax,[rax+40]
 test rax,rax
 jz .Lx_dispatch_return
 jmp rax
.Lx_dispatch_close:
 mov rax,[rdi+128]
 test rax,rax
 jz .Lx_dispatch_return
 mov rdi,[rax]
 mov rax,[rax+48]
 test rax,rax
 jz .Lx_dispatch_return
 jmp rax
.Lx_dispatch_return:
 ret
.text
/* Hook state: result, private request, decision, extension index, hook index,
   active hook, pending, event. Selected extension descriptors remain leased. */
.globl pg_extension_hook_open
.type pg_extension_hook_open,@function
pg_extension_hook_open:
 XF 32
 mov rbx,rdi
 mov r12d,esi
 mov r13,rdx
 mov r14,rcx
 mov qword ptr [r14],0
 mov edi,1
 mov esi,64
 call calloc@PLT
 test rax,rax
 jz .Lxh_open_bad
 mov [r14],rax
 mov [rax],rbx
 mov [rax+52],r12d
 mov rdi,rbx
 mov esi,r12d
 call .Lxh_has_event
 test eax,eax
 jnz .Lxh_open_copy
 mov rdi,r13
 call json_object_get@PLT
 mov rdx,[r14]
 mov [rdx+8],rax
 xor eax,eax
 jmp .Lxh_open_end
.Lxh_open_copy:
 mov rax,[r14]
 mov rdi,r13
 lea rsi,[rax+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lxh_open_release
 xor eax,eax
 jmp .Lxh_open_end
.Lxh_open_release:
 mov rdi,[r14]
 call pg_extension_hook_close
 mov qword ptr [r14],0
.Lxh_open_bad:
 mov eax,5
.Lxh_open_end:
 XE 32
.globl pg_extension_hook_close
.type pg_extension_hook_close,@function
pg_extension_hook_close:
 push rbx
 mov rbx,rdi
 test rdi,rdi
 jz .Lxh_close_end
 cmp dword ptr [rbx+48],0
 je .Lxh_close_json
 mov rax,[rbx+40]
 test rax,rax
 jz .Lxh_close_json
 cmp qword ptr [rax+24],0
 je .Lxh_close_json
 mov rdi,[rax+8]
 mov rsi,[rbx+8]
 call [rax+24]
.Lxh_close_json:
 mov rdi,[rbx+8]
 call json_object_put@PLT
 mov rdi,[rbx+16]
 call json_object_put@PLT
 mov rdi,[rbx+56]
 test rdi,rdi
 jz .Lxh_close_state
 call pg_extensions_close
 mov rdi,[rbx+56]
 call free@PLT
.Lxh_close_state:
 mov rdi,rbx
 call free@PLT
.Lxh_close_end:
 pop rbx
 ret
/* Move an owned decision field into a request object, preserving ownership on
   failure. No callbacks run while applying a completed decision. */
.Lxh_set:
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Lxh_set_end
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,5
.Lxh_set_end:
 pop rbx
 ret
.globl pg_extension_hook_poll
.type pg_extension_hook_poll,@function
pg_extension_hook_poll:
 XF 48
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [r12],0
.Lxh_poll_extension:
 mov r13,[rbx]
 mov rax,[rbx+24]
 cmp rax,[r13+1208]
 jae .Lxh_poll_done
 mov rdx,[r13+1200]
 mov r14,[rdx+rax*8]
.Lxh_poll_hook:
 mov rax,[rbx+32]
 cmp rax,[r14+64]
 jae .Lxh_poll_next_extension
 shl rax,5
 add rax,[r14+56]
 mov rbp,rax
 mov edx,[rbx+52]
 cmp edx,[rbp]
 jne .Lxh_poll_next_hook
 mov [rbx+40],rbp
 mov qword ptr [rbx+16],0
 mov rdi,[rbp+8]
 mov rsi,[rbx+8]
 lea rdx,[rbx+16]
 call [rbp+16]
 cmp eax,1
 je .Lxh_poll_pending
 mov dword ptr [rbx+48],0
 test eax,eax
 jnz .Lxh_poll_callback_error
 cmp qword ptr [rbx+16],0
 je .Lxh_poll_next_hook
 mov rdi,[rbx+16]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lxh_poll_invalid
 mov rdi,[rbx+16]
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lxh_poll_invalid
 cmp dword ptr [rbx+52],7
 je .Lxh_poll_compact
 cmp dword ptr [rbx+52],3
 je .Lxh_poll_before_request
 cmp dword ptr [rbx+52],5
 je .Lxh_poll_yield
 cmp dword ptr [rbx+52],2
 je .Lxh_poll_after
 cmp dword ptr [rbx+52],1
 jne .Lxh_poll_next_hook
 mov rdi,[rbx+16]
 lea rsi,[rip+.Lxh_block]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lxh_poll_arguments
 mov [rsp],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lxh_poll_invalid
 mov rdi,[rsp]
 call json_object_get@PLT
 mov rdi,[rbx+8]
 lea rsi,[rip+.Lxh_block]
 mov rdx,rax
 call .Lxh_set
 test eax,eax
 jnz .Lxh_poll_bad
 jmp .Lxh_poll_stop
.Lxh_poll_arguments:
 mov rdi,[rbx+16]
 lea rsi,[rip+.Lxh_arguments]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lxh_poll_next_hook
 mov [rsp],rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lxh_poll_invalid
 mov rdi,[rbx+8]
 lea rsi,[rip+.Lxh_call]
 call json_object_object_get@PLT
 mov [rsp+8],rax
 mov rdi,[rsp]
 call json_object_get@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lxh_arguments]
 mov rdx,rax
 call .Lxh_set
 test eax,eax
 jnz .Lxh_poll_bad
 jmp .Lxh_poll_next_hook
.Lxh_poll_compact:
 mov rdi,[rbx+16]
 lea rsi,[rip+.Lxh_decline]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lxh_poll_compact_accept
 mov rdi,[rbx+16]
 lea rsi,[rip+.Lxh_summary]
 call json_object_object_get@PLT
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lxh_poll_invalid
.Lxh_poll_compact_accept:
 mov rdi,[rbx+16]
 call json_object_get@PLT
 mov rdi,[rbx+8]
 lea rsi,[rip+.Lxh_decision]
 mov rdx,rax
 call .Lxh_set
 test eax,eax
 jnz .Lxh_poll_bad
 jmp .Lxh_poll_stop
.Lxh_poll_before_request:
 mov rdi,[rbx+16]
 lea rsi,[rip+.Lxh_messages]
 call json_object_object_get@PLT
 mov [rsp],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lxh_poll_invalid
 mov rdi,[rsp]
 call json_object_get@PLT
 mov rdi,[rbx+8]
 lea rsi,[rip+.Lxh_messages]
 mov rdx,rax
 call .Lxh_set
 test eax,eax
 jnz .Lxh_poll_bad
 jmp .Lxh_poll_next_hook
.Lxh_poll_yield:
 mov rdi,[rbx+16]
 lea rsi,[rip+.Lxh_continue]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lxh_poll_next_hook
 mov [rsp],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jnz .Lxh_poll_yield_valid
 mov rdi,[rsp]
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lxh_poll_invalid
.Lxh_poll_yield_valid:
 mov rdi,[rsp]
 call json_object_get@PLT
 mov rdi,[rbx+8]
 lea rsi,[rip+.Lxh_continue]
 mov rdx,rax
 call .Lxh_set
 test eax,eax
 jnz .Lxh_poll_bad
 jmp .Lxh_poll_stop
.Lxh_poll_after:
 mov rdi,[rbx+16]
 lea rsi,[rip+.Lxh_output]
 call json_object_object_get@PLT
 mov [rsp+24],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lxh_poll_invalid
 mov rdi,[rsp+24]
 call json_object_get_string_len@PLT
 cmp eax,4096
 ja .Lxh_poll_invalid
 mov rdi,[rbx+16]
 lea rsi,[rip+.Lxh_diagnostics]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lxh_poll_after_install
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lxh_poll_invalid
.Lxh_poll_after_install:
 mov rdi,[rbx+8]
 lea rsi,[rip+.Lxh_result]
 mov rdx,[rbx+16]
 mov qword ptr [rbx+16],0
 call .Lxh_set
 test eax,eax
 jnz .Lxh_poll_bad
 jmp .Lxh_poll_next_hook
.Lxh_poll_invalid:
 mov eax,-5
.Lxh_poll_callback_error:
 mov [rsp+16],eax
 mov rdi,r13
 mov rsi,[r14]
 mov eax,[rbx+52]
 lea rdx,[rip+.Lxh_events]
 mov rdx,[rdx+rax*8]
.Lxh_poll_report:
 mov ecx,[rsp+16]
 call .Lx_report
 cmp dword ptr [rbx+52],1
 jne .Lxh_poll_next_hook
 lea rdi,[rip+.Lxh_failure]
 call json_object_new_string@PLT
 test rax,rax
 jz .Lxh_poll_bad
 mov rdi,[rbx+8]
 lea rsi,[rip+.Lxh_block]
 mov rdx,rax
 call .Lxh_set
 test eax,eax
 jnz .Lxh_poll_bad
.Lxh_poll_stop:
 mov rax,[r13+1208]
 mov [rbx+24],rax
 jmp .Lxh_poll_done
.Lxh_poll_next_hook:
 mov rdi,[rbx+16]
 call json_object_put@PLT
 mov qword ptr [rbx+16],0
 mov qword ptr [rbx+40],0
 inc qword ptr [rbx+32]
 jmp .Lxh_poll_hook
.Lxh_poll_next_extension:
 inc qword ptr [rbx+24]
 mov qword ptr [rbx+32],0
 jmp .Lxh_poll_extension
.Lxh_poll_pending:
 mov dword ptr [rbx+48],1
 mov rdi,[rbx+16]
 call json_object_put@PLT
 mov qword ptr [rbx+16],0
 mov eax,1
 jmp .Lxh_poll_end
.Lxh_poll_done:
 mov rdi,[rbx+16]
 call json_object_put@PLT
 mov qword ptr [rbx+16],0
 mov rdi,[rbx+8]
 call json_object_get@PLT
 mov [r12],rax
 xor eax,eax
 jmp .Lxh_poll_end
.Lxh_poll_bad:
 mov eax,-5
.Lxh_poll_end:
 XE 48
.section .rodata
.Lxh_call: .asciz "call"
.Lxh_result: .asciz "result"
.Lxh_arguments: .asciz "arguments"
.Lxh_block: .asciz "block"
.Lxh_before: .asciz "beforeTool"
.Lxh_after: .asciz "afterTool"
.Lxh_failure: .asciz "beforeTool hook failed"

.section .rodata
.Lxh_output: .asciz "output"
.Lxh_diagnostics: .asciz "diagnostics"
.text
/* Generation hooks own a selected snapshot independently of the model lease;
   tools can borrow their already selected execution snapshot. */
.globl pg_extension_hook_resolve_open
.type pg_extension_hook_resolve_open,@function
pg_extension_hook_resolve_open:
 XF 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14d,ecx
 mov [rsp],r8
 mov [rsp+8],r9
 mov qword ptr [r9],0
 mov edi,1
 mov esi,1248
 call calloc@PLT
 mov rbp,rax
 test rax,rax
 jz .Lxh_owned_bad
 mov rdi,rbx
 mov rsi,r12
 mov rdx,r13
 mov rcx,rbp
 call pg_extensions_resolve
 test eax,eax
 jnz .Lxh_owned_release
 mov rdi,rbp
 mov esi,r14d
 mov rdx,[rsp]
 mov rcx,[rsp+8]
 call pg_extension_hook_open
 test eax,eax
 jnz .Lxh_owned_release
 mov rax,[rsp+8]
 mov rax,[rax]
 mov [rax+56],rbp
 xor eax,eax
 jmp .Lxh_owned_end
.Lxh_owned_release:
 mov rdi,rbp
 call pg_extensions_close
 mov rdi,rbp
 call free@PLT
.Lxh_owned_bad:
 mov eax,5
.Lxh_owned_end:
 XE 48
.section .rodata
.Lxh_messages: .asciz "messages"
.Lxh_continue: .asciz "continue"
.Lxh_before_request: .asciz "beforeRequest"
.Lxh_after_response: .asciz "afterResponse"
.Lxh_yield: .asciz "onYield"
.Lxh_after_tools: .asciz "afterTools"
.Lxh_before_compact: .asciz "beforeCompact"
.Lxh_decline: .asciz "decline"
.Lxh_summary: .asciz "summary"
.Lxh_decision: .asciz "decision"
.section .data.rel.ro
.balign 8
.Lxh_events: .quad 0,.Lxh_before,.Lxh_after,.Lxh_before_request,.Lxh_after_response,.Lxh_yield,.Lxh_after_tools,.Lxh_before_compact
.text
.globl pg_extension_hook_poll_event
.type pg_extension_hook_poll_event,@function
pg_extension_hook_poll_event:
 XF 32
 mov rbx,rdi
 mov r12d,esi
 mov r13,rdx
 mov r14,rcx
 mov qword ptr [r14],0
 cmp [rbx+52],r12d
 je .Lxh_event_poll
 cmp dword ptr [rbx+48],0
 jne .Lxh_event_bad
 mov qword ptr [rsp],0
 mov rdi,[rbx]
 mov esi,r12d
 call .Lxh_has_event
 test eax,eax
 jnz .Lxh_event_copy
 mov rdi,r13
 call json_object_get@PLT
 mov [rsp],rax
 jmp .Lxh_event_install
.Lxh_event_copy:
 mov rdi,r13
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lxh_event_release
.Lxh_event_install:
 mov rdi,[rbx+8]
 call json_object_put@PLT
 mov rdi,[rbx+16]
 call json_object_put@PLT
 mov rax,[rsp]
 mov [rbx+8],rax
 mov qword ptr [rbx+16],0
 mov qword ptr [rbx+24],0
 mov qword ptr [rbx+32],0
 mov qword ptr [rbx+40],0
 mov [rbx+52],r12d
.Lxh_event_poll:
 mov rdi,rbx
 mov rsi,r14
 call pg_extension_hook_poll
 jmp .Lxh_event_end
.Lxh_event_release:
 mov rdi,[rsp]
 call json_object_put@PLT
.Lxh_event_bad:
 mov eax,-5
.Lxh_event_end:
 XE 32
.text
.Lxh_has_event:
 mov rax,[rdi+1200]
 mov r8,[rdi+1208]
 xor ecx,ecx
.Lxh_has_extension:
 cmp rcx,r8
 jae .Lxh_has_no
 mov rdx,[rax+rcx*8]
 mov r9,[rdx+56]
 mov r10,[rdx+64]
 xor r11d,r11d
.Lxh_has_hook:
 cmp r11,r10
 jae .Lxh_has_next
 mov rdi,r11
 shl rdi,5
 cmp esi,[r9+rdi]
 je .Lxh_has_yes
 inc r11
 jmp .Lxh_has_hook
.Lxh_has_next:
 inc rcx
 jmp .Lxh_has_extension
.Lxh_has_yes:
 mov eax,1
 ret
.Lxh_has_no:
 xor eax,eax
 ret

.text
/* Publication validates native tool contracts and schemas without executing
   plugin callbacks. Resolution reuses the immutable published descriptors. */
.globl pg_extensions_validate
.type pg_extensions_validate,@function
pg_extensions_validate:
 XF 1200
 mov rbx,rdi
 call .Lx_validate
 test eax,eax
 jnz .Lx_validate_full_end
 test rbx,rbx
 jz .Lx_validate_full_end
 xor r12d,r12d
.Lx_validate_full_each:
 cmp r12,[rbx+16]
 jae .Lx_validate_full_ok
 mov rax,[rbx+8]
 mov rax,[rax+r12*8]
 mov rsi,[rax+8]
 mov rdx,[rax+16]
 mov rdi,rsp
 call pg_tool_registry_init@PLT
 test eax,eax
 jnz .Lx_validate_full_bad
 inc r12
 jmp .Lx_validate_full_each
.Lx_validate_full_ok:
 mov rdi,rbx
 call pg_task_extensions_validate@PLT
 jmp .Lx_validate_full_end
.Lx_validate_full_bad:
 mov eax,5
.Lx_validate_full_end:
 XE 1200

.text
/* State: composed result, input, output, index, prompt, sections, active,
   pending/completed, transient output address. */
.globl pg_extension_prepare_open
.type pg_extension_prepare_open,@function
pg_extension_prepare_open:
 XF 16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r13],0
 mov edi,1
 mov esi,72
 call calloc@PLT
 mov r14,rax
 test rax,rax
 jz .Lxp_open_bad
 mov [rax],rbx
 mov rdi,r12
 lea rsi,[rax+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lxp_open_release
 mov [r13],r14
 xor eax,eax
 jmp .Lxp_open_end
.Lxp_open_release:
 mov rdi,r14
 call pg_extension_prepare_close
.Lxp_open_bad:
 mov eax,5
.Lxp_open_end:
 XE 16
.globl pg_extension_prepare_close
.type pg_extension_prepare_close,@function
pg_extension_prepare_close:
 test rdi,rdi
 jz .Lxp_close_return
 push rbx
 mov rbx,rdi
 cmp dword ptr [rbx+56],0
 je .Lxp_close_json
 mov rax,[rbx+48]
 mov rdi,[rax+8]
 mov rsi,[rbx+8]
 call [rax+32]
.Lxp_close_json:
 mov rdi,[rbx+8]
 call json_object_put@PLT
 mov rdi,[rbx+16]
 call json_object_put@PLT
 mov rdi,[rbx+32]
 call json_object_put@PLT
 mov rdi,[rbx+40]
 call json_object_put@PLT
 mov rdi,rbx
 call free@PLT
 pop rbx
.Lxp_close_return:
 ret
.globl pg_extensions_prepare
.type pg_extensions_prepare,@function
pg_extensions_prepare:
 XF 16
 mov rbx,rdx
 mov qword ptr [rbx],0
 mov qword ptr [rsp],0
 mov rdx,rsp
 call pg_extension_prepare_open
 mov r12d,eax
 test eax,eax
 jnz .Lxp_sync_end
 mov rdi,[rsp]
 mov rsi,rbx
 call pg_extension_prepare_poll
 mov r12d,eax
 test eax,eax
 jns .Lxp_sync_pending
 neg r12d
 jmp .Lxp_sync_close
.Lxp_sync_pending:
 test eax,eax
 jz .Lxp_sync_close
 mov r12d,3
.Lxp_sync_close:
 mov rdi,[rsp]
 call pg_extension_prepare_close
.Lxp_sync_end:
 mov eax,r12d
 XE 16
