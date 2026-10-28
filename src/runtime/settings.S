/* Pi Coding Agent settings-manager/package-manager native adaptation.
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
.section .rodata
.Lglobal: .asciz "global"
.Lproject: .asciz "project"
.Lsettings: .asciz "settings"
.Ldiagnostics: .asciz "diagnostics"
.Lbody: .asciz "body"
.Ltype: .asciz "type"
.Lwarning: .asciz "warning"
.Lmessage: .asciz "message"
.Lpath: .asciz "path"
.Linvalid: .asciz "failed to read settings or parse a JSON object"
.Linvalid_paths: .asciz "resource paths must be an array of strings"
.Ljoin: .asciz "%s/%s"
.text
.macro enter bytes=0
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,\bytes+8
.endm
.macro exit bytes=0
 add rsp,\bytes+8
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.globl pg_settings_merge
.type pg_settings_merge,@function
pg_settings_merge:
 xor ecx,ecx
.Lmerge:
 enter 64
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14d,ecx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+24],0
 test r13,r13
 jz .Lmerge_invalid
 mov qword ptr [r13],0
 cmp ecx,64
 jae .Lmerge_fail
 mov rdi,rbx
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lmerge_empty
 mov rdi,rbx
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lmerge_fail
 jmp .Lmerge_override
.Lmerge_empty:
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lmerge_fail
.Lmerge_override:
 mov rdi,r12
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lmerge_success
 mov rdi,r12
 call json_object_iter_begin@PLT
 mov [rsp+8],rax
 mov rdi,r12
 call json_object_iter_end@PLT
 mov [rsp+16],rax
.Lmerge_loop:
 lea rdi,[rsp+8]
 lea rsi,[rsp+16]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lmerge_success
 lea rdi,[rsp+8]
 call json_object_iter_peek_name@PLT
 mov r15,rax
 lea rdi,[rsp+8]
 call json_object_iter_peek_value@PLT
 mov [rsp+32],rax
 mov rdi,[rsp]
 mov rsi,r15
 call json_object_object_get@PLT
 mov [rsp+40],rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lmerge_replace
 mov rdi,[rsp+32]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lmerge_replace
 mov rdi,[rsp+40]
 mov rsi,[rsp+32]
 lea rdx,[rsp+24]
 lea ecx,[r14+1]
 call .Lmerge
 test eax,eax
 jnz .Lmerge_fail
 jmp .Lmerge_set
.Lmerge_replace:
 mov rdi,[rsp+32]
 test rdi,rdi
 jz .Lmerge_set
 lea rsi,[rsp+24]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lmerge_fail
.Lmerge_set:
 mov rdi,[rsp]
 mov rsi,r15
 mov rdx,[rsp+24]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lmerge_fail
 mov qword ptr [rsp+24],0
 lea rdi,[rsp+8]
 call json_object_iter_next@PLT
 jmp .Lmerge_loop
.Lmerge_success:
 mov rax,[rsp]
 mov [r13],rax
 mov qword ptr [rsp],0
 mov dword ptr [rsp+48],0
 jmp .Lmerge_cleanup
.Lmerge_fail:
 mov dword ptr [rsp+48],5
.Lmerge_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov eax,[rsp+48]
 exit 64
.Lmerge_invalid:
 mov eax,4
 exit 64
.size pg_settings_merge,.-pg_settings_merge
.Lset_owned:
 sub rsp,24
 mov [rsp],rdx
 call json_object_object_add@PLT
 test eax,eax
 jz 1f
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,5
1: add rsp,24
 ret
.Lset_string:
 enter 0
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 call json_object_new_string@PLT
 test rax,rax
 jz 1f
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call .Lset_owned
 jmp 2f
1: mov eax,5
2: exit 0
.Ldiagnostic:
 enter 0
 mov rbx,rdi
 mov r12,rsi
 call json_object_new_object@PLT
 mov r13,rax
 test rax,rax
 jz 2f
 mov rdi,rax
 lea rsi,[rip+.Ltype]
 lea rdx,[rip+.Lwarning]
 call .Lset_string
 test eax,eax
 jnz 1f
 mov rdi,r13
 lea rsi,[rip+.Lpath]
 mov rdx,r12
 call .Lset_string
 test eax,eax
 jnz 1f
 mov rdi,r13
 lea rsi,[rip+.Lmessage]
 lea rdx,[rip+.Linvalid]
 call .Lset_string
 test eax,eax
 jnz 1f
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_add@PLT
 test eax,eax
 jz 3f
1: mov rdi,r13
 call json_object_put@PLT
2: mov eax,5
3: exit 0
/* Read one layer with source-equivalent soft parse/IO diagnostics. */
.Lread:
 enter 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rdx],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 test rbx,rbx
 jz .Lread_empty
 mov rdi,rbx
 lea rsi,[rsp]
 call pg_resource_text
 test eax,eax
 jz .Lread_parse
 cmp eax,5
 je .Lread_fail
 mov rdi,rbx
 xor esi,esi
 call access@PLT
 test eax,eax
 jz .Lread_warn
 call __errno_location@PLT
 cmp dword ptr [rax],2
 je .Lread_empty
 cmp dword ptr [rax],20
 je .Lread_empty
 jmp .Lread_warn
.Lread_parse:
 mov rdi,[rsp]
 lea rsi,[rip+.Lbody]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 mov rdi,rax
 call strlen@PLT
 mov r15,rax
 test rax,rax
 jz .Lread_empty
 call json_tokener_new@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lread_fail
 mov rdi,rax
 mov esi,17
 call json_tokener_set_flags@PLT
 mov rdi,[rsp+8]
 mov rsi,r14
 lea edx,[r15+1]
 call json_tokener_parse_ex@PLT
 mov [rsp+16],rax
 mov rdi,[rsp+8]
 call json_tokener_get_error@PLT
 test eax,eax
 jnz .Lread_warn
 mov rdi,[rsp+16]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lread_warn
 mov rax,[rsp+16]
 mov [r13],rax
 mov qword ptr [rsp+16],0
 jmp .Lread_ok
.Lread_warn:
 mov rdi,r12
 mov rsi,rbx
 call .Ldiagnostic
 test eax,eax
 jnz .Lread_fail
.Lread_empty:
 call json_object_new_object@PLT
 test rax,rax
 jz .Lread_fail
 mov [r13],rax
.Lread_ok:
 mov dword ptr [rsp+24],0
 jmp .Lread_cleanup
.Lread_fail:
 mov dword ptr [rsp+24],5
.Lread_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_tokener_free@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,[rsp+24]
 exit 48
.globl pg_settings_load
.type pg_settings_load,@function
pg_settings_load:
 enter 64
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 test rdx,rdx
 jz .Lload_invalid
 mov qword ptr [rdx],0
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 mov qword ptr [rsp+32],0
 call json_object_new_array@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lload_fail
 mov rdi,rbx
 mov rsi,rax
 mov rdx,rsp
 call .Lread
 test eax,eax
 jnz .Lload_fail
 mov rdi,r12
 mov rsi,[rsp+24]
 lea rdx,[rsp+8]
 call .Lread
 test eax,eax
 jnz .Lload_fail
 mov rdi,[rsp]
 mov rsi,[rsp+8]
 lea rdx,[rsp+16]
 call pg_settings_merge
 test eax,eax
 jnz .Lload_fail
 call json_object_new_object@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lload_fail
 xor r14d,r14d
.Lload_fields:
 lea rax,[rip+.Lkeys]
 mov rsi,[rax+r14]
 mov rdi,[rsp+r14]
 call json_object_get@PLT
 mov rdx,rax
 lea rax,[rip+.Lkeys]
 mov rsi,[rax+r14]
 mov rdi,[rsp+32]
 call .Lset_owned
 test eax,eax
 jnz .Lload_fail
 add r14,8
 cmp r14,32
 jb .Lload_fields
 mov rax,[rsp+32]
 mov [r13],rax
 mov qword ptr [rsp+32],0
 mov dword ptr [rsp+40],0
 jmp .Lload_cleanup
.Lload_fail:
 mov dword ptr [rsp+40],5
.Lload_cleanup:
 xor r14d,r14d
1: mov rdi,[rsp+r14]
 call json_object_put@PLT
 add r14,8
 cmp r14,40
 jb 1b
 mov eax,[rsp+40]
 exit 64
.Lload_invalid:
 mov eax,4
 exit 64
.size pg_settings_load,.-pg_settings_load
.section .data.rel.ro
.Lkeys: .quad .Lglobal,.Lproject,.Lsettings,.Ldiagnostics
.text
.globl pg_settings_resource_paths
.type pg_settings_resource_paths,@function
pg_settings_resource_paths:
 enter 8320
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov r15,r8
 test r8,r8
 jz .Lpaths_invalid
 mov qword ptr [r8],0
 mov qword ptr [rsp+8192],0
 mov qword ptr [rsp+8200],0
 call json_object_new_array@PLT
 mov [rsp+8192],rax
 test rax,rax
 jz .Lpaths_fail
.Lpaths_scope:
 lea rsi,[rip+.Lglobal]
 mov rax,r13
 cmp qword ptr [rsp+8200],0
 je 1f
 lea rsi,[rip+.Lproject]
 mov rax,r14
1: mov [rsp+8208],rax
 test rax,rax
 jz .Lpaths_next_scope
 mov rdi,rbx
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,r12
 call json_object_object_get@PLT
 mov [rsp+8216],rax
 test rax,rax
 jz .Lpaths_next_scope
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lpaths_fail
 mov qword ptr [rsp+8224],0
.Lpaths_loop:
 mov rdi,[rsp+8216]
 call json_object_array_length@PLT
 cmp [rsp+8224],rax
 jae .Lpaths_next_scope
 mov rdi,[rsp+8216]
 mov rsi,[rsp+8224]
 call json_object_array_get_idx@PLT
 mov [rsp+8232],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lpaths_fail
 mov rdi,[rsp+8232]
 call json_object_get_string_len@PLT
 mov [rsp+8240],rax
 mov rdi,[rsp+8232]
 call json_object_get_string@PLT
 mov rsi,[rsp+8240]
 mov rdi,rax
 call pg_resource_trim
 mov rdi,rax
 mov rsi,rdx
 mov rdx,rsp
 mov ecx,4096
 call pg_tool_path_resolve
 test eax,eax
 jnz .Lpaths_fail
 cmp byte ptr [rsp],'/'
 je .Lpaths_resolved
 lea rdi,[rsp+4096]
 mov esi,4096
 lea rdx,[rip+.Ljoin]
 mov rcx,[rsp+8208]
 mov r8,rsp
 xor eax,eax
 call snprintf@PLT
 test eax,eax
 js .Lpaths_fail
 cmp eax,4096
 jae .Lpaths_fail
 lea rdi,[rsp+4096]
 movsxd rsi,eax
 mov rdx,rsp
 mov ecx,4096
 call pg_tool_path_resolve
 test eax,eax
 jnz .Lpaths_fail
.Lpaths_resolved:
 mov rdi,rsp
 call json_object_new_string@PLT
 test rax,rax
 jz .Lpaths_fail
 mov [rsp+8248],rax
 mov rdi,[rsp+8192]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz 2f
 mov rdi,[rsp+8248]
 call json_object_put@PLT
 jmp .Lpaths_fail
2: inc qword ptr [rsp+8224]
 jmp .Lpaths_loop
.Lpaths_next_scope:
 inc qword ptr [rsp+8200]
 cmp qword ptr [rsp+8200],2
 jb .Lpaths_scope
 mov rax,[rsp+8192]
 mov [r15],rax
 xor eax,eax
 exit 8320
.Lpaths_fail:
 mov rdi,[rsp+8192]
 call json_object_put@PLT
 mov eax,5
 exit 8320
.Lpaths_invalid:
 mov eax,4
 exit 8320
.size pg_settings_resource_paths,.-pg_settings_resource_paths
.section .note.GNU-stack,"",@progbits
