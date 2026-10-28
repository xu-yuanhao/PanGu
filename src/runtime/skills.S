/* Pi Coding Agent skills/resource-loader native adaptation.
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
.Lskills: .asciz "skills"
.Ldiagnostics: .asciz "diagnostics"
.Lname: .asciz "name"
.Ldescription: .asciz "description"
.Lpath: .asciz "path"
.Lcanonical: .asciz "canonical"
.Lkind: .asciz "kind"
.Lbody: .asciz "body"
.Ldefaults: .asciz "defaults"
.Ldisabled: .asciz "disable-model-invocation"
.Lfile: .asciz "filePath"
.Lbase: .asciz "baseDir"
.Ldisable: .asciz "disableModelInvocation"
.Ltype: .asciz "type"
.Lwarning: .asciz "warning"
.Lmessage: .asciz "message"
.Lskillmd: .asciz "SKILL.md"
.Lmodules: .asciz "node_modules"
.Lsuffix: .asciz ".md"
.Ldesc_missing: .asciz "description is required"
.Ldesc_long: .asciz "description exceeds 1024 characters"
.Lname_long: .asciz "name exceeds 64 characters"
.Lname_invalid: .asciz "name contains invalid characters"
.Lname_hyphens: .asciz "name has leading, trailing or consecutive hyphens"
.Lcollision: .asciz "skill name collision; first loaded resource retained"
.Lread_error: .asciz "failed to read skill file"
.Lparse_error: .asciz "failed to parse YAML frontmatter"
.Ldirectory_error: .asciz "failed to scan skills directory"
.Ldepth_error: .asciz "skill directory traversal exceeds 64 levels"
.Lopen: .asciz "\n\n<skills>\nThe following skills provide specialized instructions for specific tasks.\n"
.Lread_hint: .asciz "Use the read tool to load a skill's file when the task matches its description.\n"
.Lbash_hint: .asciz "Use bash to load a skill's file when the task matches its description.\n"
.Lindirect_hint: .asciz "Load a skill's file when the task matches its description.\n"
.Lrelative_hint: .asciz "When a skill file references a relative path, resolve it against the skill directory (parent of SKILL.md / dirname of the path) and use that absolute path in tool commands.\n\n<available_skills>"
.Lentry_open: .asciz "\n  <skill>\n    <name>"
.Lentry_desc: .asciz "</name>\n    <description>"
.Lentry_path: .asciz "</description>\n    <location>"
.Lentry_close: .asciz "</location>\n  </skill>"
.Lclose: .asciz "\n</available_skills>\n</skills>"
.Lamp: .asciz "&amp;"
.Llt: .asciz "&lt;"
.Lgt: .asciz "&gt;"
.Lquot: .asciz "&quot;"
.Lapos: .asciz "&apos;"
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
/* Object string getter; borrowed and type checked. */
.Lfield:
 sub rsp,8
 call json_object_object_get@PLT
 mov [rsp],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz 1f
 mov rdi,[rsp]
 call json_object_get_string@PLT
 jmp 2f
1: xor eax,eax
2: add rsp,8
 ret
/* Add a newly copied string, releasing it if ownership was not accepted. */
.Lstring:
 enter 0
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 call json_object_new_string@PLT
 test rax,rax
 jz 1f
 mov r13,rax
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz 2f
 mov rdi,r13
 call json_object_put@PLT
1: mov eax,5
2: exit 0
.Lreference:
 enter 0
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 call json_object_get@PLT
 mov r13,rax
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz 1f
 mov rdi,r13
 call json_object_put@PLT
 mov eax,5
1: exit 0
/* state[skills,diagnostics,canonical-files,names,canonical-directories]. */
.Ldiag:
 enter 0
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 call json_object_new_object@PLT
 mov r14,rax
 test rax,rax
 jz 2f
 mov rdi,rax
 lea rsi,[rip+.Ltype]
 lea rdx,[rip+.Lwarning]
 call .Lstring
 test eax,eax
 jnz 1f
 mov rdi,r14
 lea rsi,[rip+.Lpath]
 mov rdx,r12
 call .Lstring
 test eax,eax
 jnz 1f
 mov rdi,r14
 lea rsi,[rip+.Lmessage]
 mov rdx,r13
 call .Lstring
 test eax,eax
 jnz 1f
 mov rdi,[rbx+8]
 mov rsi,r14
 call json_object_array_add@PLT
 test eax,eax
 jz 3f
1: mov rdi,r14
 call json_object_put@PLT
2: mov eax,5
3: exit 0
.globl pg_skills_load
.type pg_skills_load,@function
pg_skills_load:
 mov rcx,rdx
 xor edx,edx
 jmp pg_skills_load_config
.size pg_skills_load,.-pg_skills_load
.globl pg_skills_load_config
.type pg_skills_load_config,@function
pg_skills_load_config:
 enter 96
 mov [rsp+56],rdx
 mov rdx,rcx
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 test rdx,rdx
 jz .Lload_invalid
 mov qword ptr [rdx],0
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 mov qword ptr [rsp+48],0
 mov qword ptr [rsp+72],0
 mov qword ptr [rsp+88],0
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lload_fail
 call json_object_new_array@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lload_fail
 mov r14d,16
.Lload_maps:
 call json_object_new_object@PLT
 mov [rsp+r14],rax
 test rax,rax
 jz .Lload_fail
 add r14,8
 cmp r14,40
 jb .Lload_maps
 call json_object_new_object@PLT
 mov [rsp+88],rax
 test rax,rax
 jz .Lload_fail
 test rbx,rbx
 jz .Lload_project
 mov rdi,rsp
 mov rsi,rbx
 mov edx,1
 xor ecx,ecx
 call .Lwalk
 test eax,eax
 jnz .Lload_fail
.Lload_project:
 test r12,r12
 jz .Lload_extras
 mov rdi,rsp
 mov rsi,r12
 mov edx,1
 xor ecx,ecx
 call .Lwalk
 test eax,eax
 jnz .Lload_fail
.Lload_extras:
 cmp qword ptr [rsp+56],0
 je .Lload_result
 mov rdi,[rsp+56]
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lload_fail
 mov qword ptr [rsp+64],0
.Lload_extra_loop:
 mov rdi,[rsp+56]
 call json_object_array_length@PLT
 cmp [rsp+64],rax
 jae .Lload_result
 mov rdi,[rsp+56]
 mov rsi,[rsp+64]
 call json_object_array_get_idx@PLT
 mov [rsp+72],rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz 1f
 mov rdi,[rsp+72]
 lea rsi,[rip+.Lpath]
 call .Lfield
 jmp 2f
1: mov rdi,[rsp+72]
 mov qword ptr [rsp+72],0
 call json_object_get_string@PLT
2: test rax,rax
 jz .Lload_fail
 mov [rsp+80],rax
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov qword ptr [rsp+32],0
 call json_object_new_object@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lload_fail
 mov rsi,[rsp+80]
 mov rdi,rsp
 call .Lload_path
 test eax,eax
 jnz .Lload_fail
 inc qword ptr [rsp+64]
 jmp .Lload_extra_loop
.Lload_result:
 call json_object_new_object@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lload_fail
 mov rdi,rax
 lea rsi,[rip+.Lskills]
 mov rdx,[rsp]
 call .Lreference
 test eax,eax
 jnz .Lload_fail
 mov rdi,[rsp+40]
 lea rsi,[rip+.Ldiagnostics]
 mov rdx,[rsp+8]
 call .Lreference
 test eax,eax
 jnz .Lload_fail
 mov rax,[rsp+40]
 mov [r13],rax
 mov qword ptr [rsp+40],0
 jmp .Lload_cleanup
.Lload_fail:
 mov dword ptr [rsp+48],5
.Lload_cleanup:
 xor r14d,r14d
1: mov rdi,[rsp+r14]
 call json_object_put@PLT
 add r14,8
 cmp r14,48
 jb 1b
 mov rdi,[rsp+88]
 call json_object_put@PLT
 mov eax,[rsp+48]
 exit 96
.Lload_invalid:
 mov eax,4
 exit 96
.size pg_skills_load_config,.-pg_skills_load_config
/* An explicit path is a directory or a Markdown file. */
.Lload_path:
 enter 16
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov rdi,rsi
 mov rsi,rsp
 call pg_resource_entry
 test eax,eax
 jnz .Lpath_error
 cmp qword ptr [rsp],0
 je .Lpath_warning
 mov rdi,[rsp]
 lea rsi,[rip+.Lkind]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_int@PLT
 cmp eax,2
 je .Lpath_directory
 cmp eax,1
 jne .Lpath_warning
 mov rdi,r12
 call strlen@PLT
 cmp rax,3
 jb .Lpath_warning
 lea rdi,[r12+rax-3]
 lea rsi,[rip+.Lsuffix]
 call strcmp@PLT
 test eax,eax
 jnz .Lpath_warning
 mov rdi,[rsp]
 lea rsi,[rip+.Lname]
 call .Lfield
 mov rdi,rax
 lea rsi,[rip+.Lskillmd]
 call strcmp@PLT
 xor edx,edx
 test eax,eax
 sete dl
 mov rdi,rbx
 mov rsi,[rsp]
 call .Lload_file
 jmp .Lpath_end
.Lpath_directory:
 mov rdi,rbx
 mov rsi,r12
 mov edx,1
 xor ecx,ecx
 call .Lwalk
 jmp .Lpath_end
.Lpath_error:
 cmp eax,5
 je .Lpath_end
.Lpath_warning:
 mov rdi,[rbx+72]
 test rdi,rdi
 jz 1f
 lea rsi,[rip+.Ldefaults]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz 1f
 xor eax,eax
 jmp .Lpath_end
1: mov rdi,rbx
 mov rsi,r12
 lea rdx,[rip+.Lread_error]
 call .Ldiag
.Lpath_end:
 mov ebx,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebx
 exit 16
/* Directory snapshot -> root SKILL preference -> children. */
.Lwalk:
 enter 32
 mov rbx,rdi
 mov r12,rsi
 mov r13d,edx
 mov r14d,ecx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 cmp ecx,64
 jb 1f
 lea rdx,[rip+.Ldepth_error]
 call .Ldiag
 jmp .Lwalk_exit
1: mov rdi,r12
 xor esi,esi
 call realpath@PLT
 test rax,rax
 jz .Lwalk_ok
 mov [rsp+8],rax
 mov rdi,[rbx+32]
 mov rsi,rax
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lwalk_ok
 mov rdi,[rbx+32]
 mov rsi,[rsp+8]
 mov rdx,[rsp+8]
 call .Lstring
 test eax,eax
 jnz .Lwalk_exit
 mov rdi,r12
 mov rsi,rsp
 call pg_skills_directory
 test eax,eax
 jz .Lwalk_scan
 cmp eax,5
 je .Lwalk_exit
 mov rdi,rbx
 mov rsi,r12
 lea rdx,[rip+.Ldirectory_error]
 call .Ldiag
 jmp .Lwalk_exit
.Lwalk_scan:
 cmp qword ptr [rsp],0
 je .Lwalk_ok
 xor r15d,r15d
.Lwalk_prefer:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 cmp r15,rax
 jae .Lwalk_children_start
 mov rdi,[rsp]
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov [rsp+16],rax
 mov rdi,rax
 lea rsi,[rip+.Lname]
 call .Lfield
 mov rdi,rax
 lea rsi,[rip+.Lskillmd]
 call strcmp@PLT
 test eax,eax
 jnz .Lwalk_prefer_next
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lkind]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_int@PLT
 cmp eax,1
 jne .Lwalk_prefer_next
 mov rdi,rbx
 mov rsi,[rsp+16]
 mov edx,1
 call .Lload_file
 jmp .Lwalk_exit
.Lwalk_prefer_next:
 inc r15
 jmp .Lwalk_prefer
.Lwalk_children_start:
 xor r15d,r15d
.Lwalk_children:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 cmp r15,rax
 jae .Lwalk_ok
 mov rdi,[rsp]
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov [rsp+16],rax
 mov rdi,rax
 lea rsi,[rip+.Lname]
 call .Lfield
 mov [rsp+24],rax
 cmp byte ptr [rax],'.'
 je .Lwalk_next
 mov rdi,rax
 lea rsi,[rip+.Lmodules]
 call strcmp@PLT
 test eax,eax
 jz .Lwalk_next
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lkind]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_int@PLT
 cmp eax,2
 je .Lwalk_recurse
 cmp eax,1
 jne .Lwalk_next
 test r13d,r13d
 jz .Lwalk_next
 mov rdi,[rsp+24]
 call strlen@PLT
 cmp rax,3
 jb .Lwalk_next
 mov rdi,[rsp+24]
 lea rdi,[rdi+rax-3]
 lea rsi,[rip+.Lsuffix]
 call strcmp@PLT
 test eax,eax
 jnz .Lwalk_next
 mov rdi,rbx
 mov rsi,[rsp+16]
 xor edx,edx
 call .Lload_file
 jmp .Lwalk_check
.Lwalk_recurse:
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lpath]
 call .Lfield
 mov rsi,rax
 mov rdi,rbx
 xor edx,edx
 lea ecx,[r14+1]
 call .Lwalk
.Lwalk_check:
 test eax,eax
 jnz .Lwalk_exit
.Lwalk_next:
 inc r15
 jmp .Lwalk_children
.Lwalk_ok:
 xor eax,eax
.Lwalk_exit:
 mov r14d,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov eax,r14d
 exit 32
/* Complete bounded read and normalized frontmatter extraction. */
.globl pg_resource_document
.type pg_resource_document,@function
pg_resource_document:
 xor edx,edx
 jmp .Lfrontmatter
.size pg_resource_document,.-pg_resource_document
.globl pg_resource_text
.type pg_resource_text,@function
pg_resource_text:
 mov edx,1
 jmp .Lfrontmatter
.size pg_resource_text,.-pg_resource_text
.Lfrontmatter:
 enter 32
 mov [rsp+32],edx
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [r12],0
 mov r13d,-1
 xor r14d,r14d
 mov rdi,rbx
 mov esi,0x80000
 xor edx,edx
 call open@PLT
 test eax,eax
 js .Lfront_fail
 mov r13d,eax
 mov edi,1048577
 call malloc@PLT
 mov r14,rax
 test rax,rax
 jz .Lfront_memory
 xor r15d,r15d
.Lfront_read:
 mov edi,r13d
 lea rsi,[r14+r15]
 mov edx,1048577
 sub rdx,r15
 call read@PLT
 test rax,rax
 js .Lfront_read_error
 jz .Lfront_ready
 add r15,rax
 cmp r15,1048576
 ja .Lfront_fail
 jmp .Lfront_read
.Lfront_read_error:
 call __errno_location@PLT
 cmp dword ptr [rax],4
 je .Lfront_read
 jmp .Lfront_fail
.Lfront_ready:
 mov rdi,r14
 xor esi,esi
 mov rdx,r15
 call memchr@PLT
 test rax,rax
 jnz .Lfront_fail
 xor ecx,ecx
 cmp r15,3
 jb .Lfront_normalize
 cmp byte ptr [r14],0xef
 jne .Lfront_normalize
 cmp byte ptr [r14+1],0xbb
 jne .Lfront_normalize
 cmp byte ptr [r14+2],0xbf
 jne .Lfront_normalize
 mov ecx,3
.Lfront_normalize:
 xor edx,edx
1: cmp rcx,r15
 jae 4f
 mov al,[r14+rcx]
 inc rcx
 cmp al,13
 jne 3f
 mov al,10
 cmp rcx,r15
 jae 3f
 cmp byte ptr [r14+rcx],10
 jne 3f
 inc rcx
3: mov [r14+rdx],al
 inc rdx
 jmp 1b
4: mov byte ptr [r14+rdx],0
 mov [rsp],r14
 mov [rsp+8],rdx
 cmp dword ptr [rsp+32],0
 jne .Lfront_empty
 cmp rdx,4
 jb .Lfront_empty
 cmp word ptr [r14],0x2d2d
 jne .Lfront_empty
 cmp byte ptr [r14+2],'-'
 jne .Lfront_empty
 mov ecx,3
.Lfront_delimiter:
 lea rax,[rcx+4]
 cmp rax,rdx
 ja .Lfront_empty
 cmp dword ptr [r14+rcx],0x2d2d2d0a
 je .Lfront_yaml
 inc rcx
 jmp .Lfront_delimiter
.Lfront_yaml:
 mov [rsp+24],rcx
 lea rdi,[r14+rcx+4]
 lea rsi,[rcx+4]
 sub rdx,rsi
 mov rsi,rdx
 call pg_resource_trim
 mov [rsp],rax
 mov [rsp+8],rdx
 mov rcx,[rsp+24]
 cmp rcx,4
 jbe .Lfront_empty
 lea rdi,[r14+4]
 lea rsi,[rcx-4]
 mov rdx,r12
 call pg_skills_yaml
 test eax,eax
 jnz .Lfront_result
 jmp .Lfront_body
.Lfront_empty:
 call json_object_new_object@PLT
 test rax,rax
 jz .Lfront_memory
 mov [r12],rax
.Lfront_body:
 mov rdi,[rsp]
 mov esi,[rsp+8]
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Lfront_body_fail
 mov [rsp+16],rax
 mov rdi,[r12]
 lea rsi,[rip+.Lbody]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Lfront_result
 mov rdi,[rsp+16]
 call json_object_put@PLT
.Lfront_body_fail:
 mov rdi,[r12]
 call json_object_put@PLT
 mov qword ptr [r12],0
 jmp .Lfront_memory
.Lfront_memory:
 mov eax,5
 jmp .Lfront_result
.Lfront_fail:
 mov eax,1
.Lfront_result:
 mov ebx,eax
 mov rdi,r14
 call free@PLT
 cmp r13d,0
 jl 1f
 mov edi,r13d
 call close@PLT
1: mov eax,ebx
 exit 32
/* ECMAScript whitespace width; zero for a nonspace or invalid scalar. */
.globl pg_resource_space
.type pg_resource_space,@function
pg_resource_space:
 sub rsp,24
 lea rdx,[rsp]
 call utf8proc_iterate@PLT
 test rax,rax
 jle .Lspace_no
 mov [rsp+8],rax
 mov edi,[rsp]
 cmp edi,9
 jb 1f
 cmp edi,13
 jbe .Lspace_yes
1: cmp edi,0xfeff
 je .Lspace_yes
 call utf8proc_category@PLT
 cmp eax,23
 jb .Lspace_no
 cmp eax,25
 ja .Lspace_no
.Lspace_yes:
 mov rax,[rsp+8]
 add rsp,24
 ret
.Lspace_no:
 xor eax,eax
 add rsp,24
 ret
.size pg_resource_space,.-pg_resource_space
.globl pg_resource_trim
.type pg_resource_trim,@function
pg_resource_trim:
 enter 16
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
.Ltrim_lead:
 cmp r13,r12
 jae .Ltrim_empty
 lea rdi,[rbx+r13]
 mov rsi,r12
 sub rsi,r13
 call pg_resource_space
 test rax,rax
 jz .Ltrim_body
 add r13,rax
 jmp .Ltrim_lead
.Ltrim_body:
 mov r14,r13
 mov r15,r13
.Ltrim_scan:
 cmp r13,r12
 jae .Ltrim_end
 lea rdi,[rbx+r13]
 mov rsi,r12
 sub rsi,r13
 call pg_resource_space
 test rax,rax
 jz 1f
 add r13,rax
 jmp .Ltrim_scan
1: lea rdi,[rbx+r13]
 mov rsi,r12
 sub rsi,r13
 mov rdx,rsp
 call utf8proc_iterate@PLT
 test rax,rax
 jg 2f
 mov eax,1
2: add r13,rax
 mov r15,r13
 jmp .Ltrim_scan
.Ltrim_end:
 lea rax,[rbx+r14]
 mov rdx,r15
 sub rdx,r14
 exit 16
.Ltrim_empty:
 lea rax,[rbx+r12]
 xor edx,edx
 exit 16
.size pg_resource_trim,.-pg_resource_trim
/* String UTF-16 length and non-whitespace flag, matching JS validation. */
.Ltext_measure:
 enter 16
 mov rbx,rdi
 call strlen@PLT
 mov r12,rax
 xor r13d,r13d
 xor r14d,r14d
 xor r15d,r15d
1: cmp r13,r12
 jae 6f
 lea rdi,[rbx+r13]
 mov rsi,r12
 sub rsi,r13
 mov rdx,rsp
 call utf8proc_iterate@PLT
 test rax,rax
 jle 7f
 add r13,rax
 inc r14
 mov edi,[rsp]
 cmp edi,65535
 jbe 2f
 inc r14
2: cmp edi,9
 jb 3f
 cmp edi,13
 jbe 1b
3: cmp edi,0xfeff
 je 1b
 call utf8proc_category@PLT
 cmp eax,23
 jb 4f
 cmp eax,25
 jbe 1b
4: mov r15d,1
 jmp 1b
6: mov rax,r14
 mov edx,r15d
 jmp 8f
7: mov rax,-1
 xor edx,edx
8: exit 16
/* entry snapshot remains owned by traversal; accepted skill owns copied data. */
.Lload_file:
 enter 80
 mov rbx,rdi
 mov r12,rsi
 mov [rsp+72],edx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov dword ptr [rsp+64],0
 mov rdi,r12
 lea rsi,[rip+.Lpath]
 call .Lfield
 mov r13,rax
 cmp qword ptr [rbx+72],0
 je .Lfile_select
 mov rdi,[rbx+88]
 mov rsi,r13
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lfile_ok
 mov rdi,[rbx+88]
 mov rsi,r13
 mov rdx,r13
 call .Lstring
 test eax,eax
 jnz .Lfile_memory
.Lfile_select:
 mov rdi,r13
 mov rsi,[rbx+72]
 call pg_resource_selected
 test eax,eax
 js .Lfile_memory
 jz .Lfile_ok
 mov rdi,r12
 lea rsi,[rip+.Lcanonical]
 call .Lfield
 mov r14,rax
 mov rdi,[rbx+16]
 mov rsi,r14
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lfile_ok
 mov rdi,r13
 mov rsi,rsp
 call pg_resource_document
 test eax,eax
 jz .Lfile_description
 cmp eax,5
 je .Lfile_memory
 cmp dword ptr [rsp+72],0
 je .Lfile_ok
 mov rdi,rbx
 mov rsi,r13
 lea rdx,[rip+.Lparse_error]
 call .Ldiag
 jmp .Lfile_result
.Lfile_description:
 mov rdi,[rsp]
 lea rsi,[rip+.Ldescription]
 call .Lfield
 mov [rsp+16],rax
 test rax,rax
 jz .Lfile_no_description
 mov rdi,rax
 call .Ltext_measure
 mov [rsp+40],rax
 test edx,edx
 jz .Lfile_no_description
 jmp .Lfile_name
.Lfile_no_description:
 cmp dword ptr [rsp+72],0
 je .Lfile_ok
 mov rdi,rbx
 mov rsi,r13
 lea rdx,[rip+.Ldesc_missing]
 call .Ldiag
 jmp .Lfile_result
.Lfile_name:
 mov rdi,r13
 call strdup@PLT
 test rax,rax
 jz .Lfile_memory
 mov [rsp+8],rax
 mov rdi,rax
 mov esi,'/'
 call strrchr@PLT
 test rax,rax
 jz .Lfile_memory
 mov byte ptr [rax],0
 mov rdi,[rsp+8]
 mov esi,'/'
 call strrchr@PLT
 inc rax
 mov r15,rax
 mov rdi,[rsp]
 lea rsi,[rip+.Lname]
 call .Lfield
 test rax,rax
 jz 1f
 cmp byte ptr [rax],0
 je 1f
 mov r15,rax
1: mov rdi,r15
 call .Ltext_measure
 cmp rax,64
 jbe 2f
 mov rdi,rbx
 mov rsi,r13
 lea rdx,[rip+.Lname_long]
 call .Ldiag
 test eax,eax
 jnz .Lfile_result
2: mov rdi,r15
 xor ecx,ecx
 xor edx,edx
3: mov al,[rdi+rcx]
 test al,al
 jz 7f
 cmp al,'a'
 jb 4f
 cmp al,'z'
 jbe 6f
4: cmp al,'0'
 jb 5f
 cmp al,'9'
 jbe 6f
5: cmp al,'-'
 je 6f
 mov edx,1
6: inc rcx
 jmp 3b
7: test rcx,rcx
 jnz 8f
 mov edx,1
8: mov [rsp+48],rcx
 test edx,edx
 jz .Lfile_hyphens
 mov rdi,rbx
 mov rsi,r13
 lea rdx,[rip+.Lname_invalid]
 call .Ldiag
 test eax,eax
 jnz .Lfile_result
.Lfile_hyphens:
 cmp byte ptr [r15],'-'
 je 3f
 mov rcx,[rsp+48]
 test rcx,rcx
 jz 5f
 cmp byte ptr [r15+rcx-1],'-'
 je 3f
 xor edx,edx
1: lea rax,[rdx+1]
 cmp rax,rcx
 jae 5f
 cmp word ptr [r15+rdx],0x2d2d
 je 3f
 inc rdx
 jmp 1b
3: mov rdi,rbx
 mov rsi,r13
 lea rdx,[rip+.Lname_hyphens]
 call .Ldiag
 test eax,eax
 jnz .Lfile_result
5: cmp qword ptr [rsp+40],1024
 jbe .Lfile_dedup
 mov rdi,rbx
 mov rsi,r13
 lea rdx,[rip+.Ldesc_long]
 call .Ldiag
 test eax,eax
 jnz .Lfile_result
.Lfile_dedup:
 mov rdi,[rbx+24]
 mov rsi,r15
 call json_object_object_get@PLT
 test rax,rax
 jz .Lfile_create
 mov rdi,rbx
 mov rsi,r13
 lea rdx,[rip+.Lcollision]
 call .Ldiag
 jmp .Lfile_result
.Lfile_create:
 call json_object_new_object@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lfile_memory
 mov rdi,rax
 lea rsi,[rip+.Lname]
 mov rdx,r15
 call .Lstring
 test eax,eax
 jnz .Lfile_drop
 mov rdi,[rsp+24]
 lea rsi,[rip+.Ldescription]
 mov rdx,[rsp+16]
 call .Lstring
 test eax,eax
 jnz .Lfile_drop
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lfile]
 mov rdx,r13
 call .Lstring
 test eax,eax
 jnz .Lfile_drop
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lbase]
 mov rdx,[rsp+8]
 call .Lstring
 test eax,eax
 jnz .Lfile_drop
 mov rdi,[rsp]
 lea rsi,[rip+.Ldisabled]
 call json_object_object_get@PLT
 mov [rsp+32],rax
 mov rdi,rax
 mov esi,1
 call json_object_is_type@PLT
 test eax,eax
 jz 1f
 mov rdi,[rsp+32]
 call json_object_get_boolean@PLT
 mov edi,eax
 jmp 2f
1: xor edi,edi
2: call json_object_new_boolean@PLT
 test rax,rax
 jz .Lfile_drop
 mov [rsp+56],rax
 mov rdi,[rsp+24]
 lea rsi,[rip+.Ldisable]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz 3f
 mov rdi,[rsp+56]
 call json_object_put@PLT
 jmp .Lfile_drop
3: mov rdi,[rbx+16]
 mov rsi,r14
 mov rdx,r14
 call .Lstring
 test eax,eax
 jnz .Lfile_drop
 mov rdi,[rbx+24]
 mov rsi,r15
 mov rdx,r13
 call .Lstring
 test eax,eax
 jnz .Lfile_drop
 mov rdi,[rbx]
 mov rsi,[rsp+24]
 call json_object_array_add@PLT
 test eax,eax
 jz .Lfile_ok
.Lfile_drop:
 mov rdi,[rsp+24]
 call json_object_put@PLT
.Lfile_memory:
 mov eax,5
 jmp .Lfile_result
.Lfile_ok:
 xor eax,eax
.Lfile_result:
 mov [rsp+64],eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov eax,[rsp+64]
 exit 80
/* Prompt writer: r12 output, r13 capacity, r14 size. */
.Lappend:
 enter 0
 mov rbx,rdi
 call strlen@PLT
 mov r15,rax
 lea rax,[r14+rax]
 cmp rax,r13
 jae 1f
 lea rdi,[r12+r14]
 mov rsi,rbx
 lea rdx,[r15+1]
 call memcpy@PLT
 add r14,r15
 xor eax,eax
 jmp 2f
1: mov eax,1
2: /* r14 is the caller's live cursor. */
 mov [rsp+16],r14
 exit 0
.Lxml:
 enter 16
 mov rbx,rdi
 xor r15d,r15d
1: movzx eax,byte ptr [rbx+r15]
 test eax,eax
 jz 8f
 lea rdi,[rip+.Lamp]
 cmp al,'&'
 je 7f
 lea rdi,[rip+.Llt]
 cmp al,'<'
 je 7f
 lea rdi,[rip+.Lgt]
 cmp al,'>'
 je 7f
 lea rdi,[rip+.Lquot]
 cmp al,'"'
 je 7f
 lea rdi,[rip+.Lapos]
 cmp al,39
 je 7f
 mov [rsp],al
 mov byte ptr [rsp+1],0
 mov rdi,rsp
7: call .Lappend
 test eax,eax
 jnz 9f
 inc r15
 jmp 1b
8: xor eax,eax
9: mov [rsp+32],r14
 exit 16
.globl pg_skills_prompt
.type pg_skills_prompt,@function
pg_skills_prompt:
 enter 48
 mov rbx,rdi
 mov [rsp+32],esi
 mov r12,rdx
 mov r13,rcx
 mov [rsp+24],r8
 xor r14d,r14d
 test r8,r8
 jz .Lprompt_invalid
 mov qword ptr [r8],0
 test r12,r12
 jz .Lprompt_invalid
 test r13,r13
 jz .Lprompt_invalid
 mov byte ptr [r12],0
 cmp esi,3
 jae .Lprompt_done
 mov rdi,rbx
 lea rsi,[rip+.Lskills]
 call json_object_object_get@PLT
 mov rbx,rax
 xor r15d,r15d
.Lprompt_loop:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r15,rax
 jae .Lprompt_finish
 mov rdi,rbx
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov [rsp],rax
 mov rdi,rax
 lea rsi,[rip+.Ldisable]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lprompt_next
 test r14,r14
 jnz .Lprompt_entry
 lea rdi,[rip+.Lopen]
 call .Lappend
 test eax,eax
 jnz .Lprompt_fail
 lea rdi,[rip+.Lread_hint]
 cmp dword ptr [rsp+32],0
 je 1f
 lea rdi,[rip+.Lbash_hint]
 cmp dword ptr [rsp+32],1
 je 1f
 lea rdi,[rip+.Lindirect_hint]
1: call .Lappend
 test eax,eax
 jnz .Lprompt_fail
 lea rdi,[rip+.Lrelative_hint]
 call .Lappend
 test eax,eax
 jnz .Lprompt_fail
.Lprompt_entry:
 lea rdi,[rip+.Lentry_open]
 call .Lappend
 test eax,eax
 jnz .Lprompt_fail
 mov rdi,[rsp]
 lea rsi,[rip+.Lname]
 call .Lfield
 mov rdi,rax
 call .Lxml
 test eax,eax
 jnz .Lprompt_fail
 lea rdi,[rip+.Lentry_desc]
 call .Lappend
 test eax,eax
 jnz .Lprompt_fail
 mov rdi,[rsp]
 lea rsi,[rip+.Ldescription]
 call .Lfield
 mov rdi,rax
 call .Lxml
 test eax,eax
 jnz .Lprompt_fail
 lea rdi,[rip+.Lentry_path]
 call .Lappend
 test eax,eax
 jnz .Lprompt_fail
 mov rdi,[rsp]
 lea rsi,[rip+.Lfile]
 call .Lfield
 mov rdi,rax
 call .Lxml
 test eax,eax
 jnz .Lprompt_fail
 lea rdi,[rip+.Lentry_close]
 call .Lappend
 test eax,eax
 jnz .Lprompt_fail
.Lprompt_next:
 inc r15
 jmp .Lprompt_loop
.Lprompt_finish:
 test r14,r14
 jz .Lprompt_done
 lea rdi,[rip+.Lclose]
 call .Lappend
 test eax,eax
 jnz .Lprompt_fail
.Lprompt_done:
 mov rax,[rsp+24]
 mov [rax],r14
 xor eax,eax
 exit 48
.Lprompt_fail:
 mov byte ptr [r12],0
.Lprompt_invalid:
 mov eax,1
 exit 48
.size pg_skills_prompt,.-pg_skills_prompt
.section .note.GNU-stack,"",@progbits
