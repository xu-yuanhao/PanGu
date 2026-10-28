/* Pi Coding Agent prompt-templates/agent-session native adaptation.
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
.Ltemplates: .asciz "templates"
.Ldiagnostics: .asciz "diagnostics"
.Lskills: .asciz "skills"
.Lname: .asciz "name"
.Ldescription: .asciz "description"
.Lbody: .asciz "body"
.Lpath: .asciz "path"
.Lfile: .asciz "filePath"
.Lbase: .asciz "baseDir"
.Lkind: .asciz "kind"
.Larg_hint: .asciz "argument-hint"
.Lhint: .asciz "argumentHint"
.Ltype: .asciz "type"
.Lwarning: .asciz "warning"
.Lmessage: .asciz "message"
.Lload_error: .asciz "failed to read or parse prompt template"
.Lsuffix: .asciz ".md"
.Lskill_prefix: .asciz "/skill:"
.Lskill_open: .asciz "<skill name=\""
.Lskill_location: .asciz "\" location=\""
.Lskill_refs: .asciz "\">\nReferences are relative to "
.Lperiod: .asciz ".\n\n"
.Lskill_close: .asciz "\n</skill>"
.Lparagraph: .asciz "\n\n"
.Lspace: .asciz " "
.Lempty: .asciz ""
.Lpattern: .asciz "\\$\\{(\\d+|ARGUMENTS|@):-([^}]*)\\}|\\$\\{@:(\\d+)(?::(\\d+))?\\}|\\$(ARGUMENTS|@|\\d+)"
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
.Lset_ref:
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
/* Complete .md child metadata, preserving per-file diagnostics. */
.globl pg_prompt_templates_load
.type pg_prompt_templates_load,@function
pg_prompt_templates_load:
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
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lload_fail
 call json_object_new_array@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lload_fail
 call json_object_new_array@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lload_fail
 mov rdi,[rsp]
 lea rsi,[rip+.Ltemplates]
 mov rdx,[rsp+8]
 call .Lset_ref
 test eax,eax
 jnz .Lload_fail
 mov rdi,[rsp]
 lea rsi,[rip+.Ldiagnostics]
 mov rdx,[rsp+16]
 call .Lset_ref
 test eax,eax
 jnz .Lload_fail
 test rbx,rbx
 jz 1f
 mov rdi,[rsp]
 mov rsi,rbx
 call .Lload_dir
 test eax,eax
 jnz .Lload_fail
1: test r12,r12
 jz 2f
 mov rdi,[rsp]
 mov rsi,r12
 call .Lload_dir
 test eax,eax
 jnz .Lload_fail
2: mov rax,[rsp]
 mov [r13],rax
 mov qword ptr [rsp],0
 mov dword ptr [rsp+24],0
 jmp .Lload_cleanup
.Lload_fail:
 mov dword ptr [rsp+24],5
.Lload_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,[rsp+24]
 exit 64
.Lload_invalid:
 mov eax,4
 exit 64
.size pg_prompt_templates_load,.-pg_prompt_templates_load
.Lload_dir:
 enter 48
 mov rbx,rdi
 mov rdi,rsi
 mov rsi,rsp
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 call pg_skills_directory
 cmp eax,5
 je .Ldir_done
 test eax,eax
 jnz .Ldir_ok
 cmp qword ptr [rsp],0
 je .Ldir_ok
 xor r12d,r12d
.Ldir_loop:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ldir_ok
 mov rdi,[rsp]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Lkind]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_int@PLT
 cmp eax,1
 jne .Ldir_next
 mov rdi,r13
 lea rsi,[rip+.Lname]
 call .Lfield
 mov r14,rax
 mov rdi,rax
 call strlen@PLT
 cmp rax,3
 jb .Ldir_next
 lea r15,[rax-3]
 lea rdi,[r14+r15]
 lea rsi,[rip+.Lsuffix]
 call strcmp@PLT
 test eax,eax
 jnz .Ldir_next
 mov rdi,r13
 lea rsi,[rip+.Lpath]
 call .Lfield
 mov [rsp+16],rax
 mov rdi,rax
 lea rsi,[rsp+8]
 call pg_resource_document
 test eax,eax
 jz .Ldir_metadata
 cmp eax,5
 je .Ldir_done
 mov rdi,rbx
 mov rsi,[rsp+16]
 call .Ldiagnostic
 test eax,eax
 jnz .Ldir_done
 jmp .Ldir_next
.Ldir_metadata:
 mov rdi,r14
 mov esi,r15d
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Ldir_fail
 mov [rsp+24],rax
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lname]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz 1f
 mov rdi,[rsp+24]
 call json_object_put@PLT
 jmp .Ldir_fail
1: mov rdi,[rsp+16]
 call json_object_new_string@PLT
 test rax,rax
 jz .Ldir_fail
 mov [rsp+24],rax
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lfile]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz 2f
 mov rdi,[rsp+24]
 call json_object_put@PLT
 jmp .Ldir_fail
2: mov rdi,[rsp+8]
 lea rsi,[rip+.Ldescription]
 call .Lfield
 test rax,rax
 jz .Ldir_description
 cmp byte ptr [rax],0
 jne .Ldir_hint
.Ldir_description:
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lbody]
 call .Lfield
 mov rdi,rax
 call .Lfallback_description
 test rax,rax
 jz .Ldir_fail
 mov [rsp+24],rax
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ldescription]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Ldir_hint
 mov rdi,[rsp+24]
 call json_object_put@PLT
 jmp .Ldir_fail
.Ldir_hint:
 mov rdi,[rsp+8]
 lea rsi,[rip+.Larg_hint]
 call .Lfield
 test rax,rax
 jz .Ldir_attach
 cmp byte ptr [rax],0
 je .Ldir_attach
 mov rdi,[rsp+8]
 lea rsi,[rip+.Larg_hint]
 call json_object_object_get@PLT
 mov rdx,rax
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lhint]
 call .Lset_ref
 test eax,eax
 jnz .Ldir_fail
.Ldir_attach:
 mov rdi,rbx
 lea rsi,[rip+.Ltemplates]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,[rsp+8]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ldir_fail
 mov qword ptr [rsp+8],0
.Ldir_next:
 inc r12
 jmp .Ldir_loop
.Ldir_ok:
 xor eax,eax
 jmp .Ldir_done
.Ldir_fail:
 mov eax,5
.Ldir_done:
 mov ebx,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebx
 exit 48
.Ldiagnostic:
 enter 0
 mov rbx,rdi
 mov r12,rsi
 call json_object_new_object@PLT
 mov r13,rax
 test rax,rax
 jz 3f
 lea rdi,[rip+.Lwarning]
 call json_object_new_string@PLT
 mov rdx,rax
 mov rdi,r13
 lea rsi,[rip+.Ltype]
 call .Ladd_owned
 test eax,eax
 jnz 2f
 lea rdi,[rip+.Lload_error]
 call json_object_new_string@PLT
 mov rdx,rax
 mov rdi,r13
 lea rsi,[rip+.Lmessage]
 call .Ladd_owned
 test eax,eax
 jnz 2f
 mov rdi,r12
 call json_object_new_string@PLT
 mov rdx,rax
 mov rdi,r13
 lea rsi,[rip+.Lpath]
 call .Ladd_owned
 test eax,eax
 jnz 2f
 mov rdi,rbx
 lea rsi,[rip+.Ldiagnostics]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,r13
 call json_object_array_add@PLT
 test eax,eax
 jz 4f
2: mov rdi,r13
 call json_object_put@PLT
3: mov eax,5
4: exit 0
.Ladd_owned:
 sub rsp,24
 test rdx,rdx
 jz 1f
 mov [rsp],rdx
 call json_object_object_add@PLT
 test eax,eax
 jz 2f
 mov rdi,[rsp]
 call json_object_put@PLT
1: mov eax,5
2: add rsp,24
 ret
/* First nonempty source line, at most 60 UTF-16 units and optional ellipsis. */
.Lfallback_description:
 enter 272
 mov rbx,rdi
.Ldesc_line:
 mov rdi,rbx
 mov esi,10
 call strchr@PLT
 mov r12,rax
 test rax,rax
 jnz 1f
 mov rdi,rbx
 call strlen@PLT
 lea r12,[rbx+rax]
1: mov rdi,rbx
 mov rsi,r12
 sub rsi,rbx
 call pg_resource_trim
 test rdx,rdx
 jnz .Ldesc_copy
 cmp byte ptr [r12],0
 je .Ldesc_empty
 lea rbx,[r12+1]
 jmp .Ldesc_line
.Ldesc_copy:
 xor r13d,r13d
 xor r14d,r14d
 mov r15,r12
 sub r15,rbx
1: cmp r13,r15
 jae 5f
 lea rdi,[rbx+r13]
 mov rsi,r15
 sub rsi,r13
 lea rdx,[rsp+264]
 call utf8proc_iterate@PLT
 test rax,rax
 jg 2f
 mov eax,1
2: inc r14
 cmp dword ptr [rsp+264],65535
 jbe 3f
 inc r14
3: cmp r14,60
 ja 4f
 add r13,rax
 jmp 1b
4: mov rdi,rsp
 mov rsi,rbx
 mov rdx,r13
 call memcpy@PLT
 mov dword ptr [rsp+r13],0x002e2e2e
 mov rdi,rsp
 lea rsi,[r13+3]
 call json_object_new_string_len@PLT
 jmp 6f
5: mov rdi,rbx
 mov esi,r15d
 call json_object_new_string_len@PLT
 jmp 6f
.Ldesc_empty:
 lea rdi,[rip+.Lempty]
 call json_object_new_string@PLT
6: exit 272
/* Arguments use the same quote/whitespace loop as parseCommandArgs. */
.globl pg_prompt_args
.type pg_prompt_args,@function
pg_prompt_args:
 enter 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 test rdx,rdx
 jz .Largs_invalid
 mov qword ptr [rdx],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],0
 test rbx,rbx
 jnz 1f
 test r12,r12
 jnz .Largs_fail
1: cmp r12,1048576
 ja .Largs_fail
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Largs_fail
 lea rdi,[r12+1]
 call malloc@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Largs_fail
 xor r14d,r14d
 xor r15d,r15d
.Largs_loop:
 cmp r14,r12
 jae .Largs_finish
 movzx eax,byte ptr [rbx+r14]
 cmp byte ptr [rsp+24],0
 je .Largs_outside
 cmp al,[rsp+24]
 jne .Largs_copy
 mov byte ptr [rsp+24],0
 inc r14
 jmp .Largs_loop
.Largs_outside:
 cmp al,34
 je .Largs_quote
 cmp al,39
 je .Largs_quote
 lea rdi,[rbx+r14]
 mov rsi,r12
 sub rsi,r14
 call pg_resource_space
 test rax,rax
 jz .Largs_copy
 add r14,rax
 cmp dword ptr [rsp+16],0
 je .Largs_loop
 call .Largs_push
 test eax,eax
 jnz .Largs_fail
 xor r15d,r15d
 mov dword ptr [rsp+16],0
 jmp .Largs_loop
.Largs_quote:
 mov [rsp+24],al
 mov dword ptr [rsp+16],1
 inc r14
 jmp .Largs_loop
.Largs_copy:
 mov rax,[rsp+8]
 mov dl,[rbx+r14]
 mov [rax+r15],dl
 inc r14
 inc r15
 mov dword ptr [rsp+16],1
 jmp .Largs_loop
.Largs_finish:
 cmp dword ptr [rsp+16],0
 je 1f
 call .Largs_push
 test eax,eax
 jnz .Largs_fail
1: mov rax,[rsp]
 mov [r13],rax
 mov qword ptr [rsp],0
 mov dword ptr [rsp+32],0
 jmp .Largs_cleanup
.Largs_fail:
 mov dword ptr [rsp+32],5
.Largs_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov eax,[rsp+32]
 exit 48
.Largs_invalid:
 mov eax,4
 exit 48
/* Called within the args frame; one return address below its locals. */
.Largs_push:
 sub rsp,8
 mov rdi,[rsp+24]
 mov esi,r15d
 call json_object_new_string_len@PLT
 test rax,rax
 jz 1f
 mov [rsp],rax
 mov rdi,[rsp+16]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz 2f
 mov rdi,[rsp]
 call json_object_put@PLT
1: mov eax,5
2: add rsp,8
 ret
.size pg_prompt_args,.-pg_prompt_args
/* r12/r13/r14 are the bounded output/capacity/cursor for these private writers. */
.Lspan:
 enter 0
 mov rbx,rdi
 mov r15,rsi
 lea rax,[r14+rsi]
 cmp rax,r14
 jb 1f
 cmp rax,r13
 jae 1f
 lea rdi,[r12+r14]
 mov rsi,rbx
 mov rdx,r15
 call memcpy@PLT
 add r14,r15
 mov byte ptr [r12+r14],0
 mov [rsp+16],r14
 xor eax,eax
 jmp 2f
1: mov eax,1
2: exit 0
.Lappend:
 enter 0
 mov rbx,rdi
 call strlen@PLT
 mov rdi,rbx
 mov rsi,rax
 call .Lspan
 mov [rsp+16],r14
 exit 0
/* Decimal bounded index, overflow maps to missing/out-of-range. */
.Lnumber:
 xor eax,eax
 xor ecx,ecx
1: cmp rcx,rsi
 jae 3f
 movzx edx,byte ptr [rdi+rcx]
 sub edx,'0'
 cmp edx,9
 ja 2f
 cmp rax,1048576
 jae 2f
 imul rax,10
 add rax,rdx
 inc rcx
 jmp 1b
2: mov rax,-1
3: ret
/* args,start,end -> joined literal values; output cursor survives. */
.Ljoin:
 enter 16
 mov rbx,rdi
 mov r15,rsi
 mov [rsp],rdx
 mov qword ptr [rsp+8],0
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp [rsp],rax
 jbe 1f
 mov [rsp],rax
1: cmp r15,[rsp]
 jae 4f
 cmp qword ptr [rsp+8],0
 je 2f
 lea rdi,[rip+.Lspace]
 call .Lappend
 test eax,eax
 jnz 5f
2: mov rdi,rbx
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 call .Lappend
 test eax,eax
 jnz 5f
 inc r15
 mov qword ptr [rsp+8],1
 jmp 1b
4: xor eax,eax
5: mov [rsp+32],r14
 exit 16
.globl pg_prompt_substitute
.type pg_prompt_substitute,@function
pg_prompt_substitute:
 enter 144
 mov rbx,rdi
 mov r15,rsi
 mov [rsp+16],rdx
 mov r12,rcx
 mov r13,r8
 mov [rsp+24],r9
 mov qword ptr [rsp+32],0
 mov qword ptr [rsp+40],0
 mov qword ptr [rsp+48],0
 xor r14d,r14d
 test r9,r9
 jz .Lsub_invalid
 mov qword ptr [r9],0
 test r12,r12
 jz .Lsub_invalid
 test r13,r13
 jz .Lsub_invalid
 mov byte ptr [r12],0
 test rbx,rbx
 jz .Lsub_invalid
 lea rdi,[rip+.Lpattern]
 mov rsi,-1
 xor edx,edx
 lea rcx,[rsp+120]
 lea r8,[rsp+128]
 xor r9d,r9d
 call pcre2_compile_8@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lsub_fail
 mov rdi,rax
 xor esi,esi
 call pcre2_match_data_create_from_pattern_8@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lsub_fail
.Lsub_loop:
 mov qword ptr [rsp],0
 mov rdi,[rsp+32]
 mov rsi,rbx
 mov rdx,r15
 mov rcx,[rsp+48]
 xor r8d,r8d
 mov r9,[rsp+40]
 call pcre2_match_8@PLT
 cmp eax,-1
 je .Lsub_tail
 test eax,eax
 js .Lsub_fail
 mov rdi,[rsp+40]
 call pcre2_get_ovector_pointer_8@PLT
 mov [rsp+56],rax
 mov rcx,[rax]
 mov [rsp+72],rcx
 mov rdx,[rax+8]
 mov [rsp+64],rdx
 lea rdi,[rbx+rcx]
 mov rsi,rcx
 sub rsi,[rsp+48]
 mov rdi,rbx
 add rdi,[rsp+48]
 call .Lspan
 test eax,eax
 jnz .Lsub_fail
 mov rax,[rsp+56]
 cmp qword ptr [rax+16],-1
 jne .Lsub_default
 cmp qword ptr [rax+48],-1
 jne .Lsub_slice
 mov rcx,[rax+80]
 mov rdx,[rax+88]
 sub rdx,rcx
 mov qword ptr [rsp+96],0
 jmp .Lsub_target
.Lsub_default:
 mov rcx,[rax+32]
 lea rdi,[rbx+rcx]
 mov [rsp+104],rdi
 mov rdx,[rax+40]
 sub rdx,rcx
 mov [rsp+112],rdx
 mov rcx,[rax+16]
 mov rdx,[rax+24]
 sub rdx,rcx
 mov qword ptr [rsp+96],1
.Lsub_target:
 lea rdi,[rbx+rcx]
 cmp byte ptr [rdi],'@'
 je .Lsub_all
 cmp byte ptr [rdi],'A'
 je .Lsub_all
 mov rsi,rdx
 call .Lnumber
 test rax,rax
 jz .Lsub_missing
 cmp rax,-1
 je .Lsub_missing
 dec rax
 mov [rsp+80],rax
 mov rdi,[rsp+16]
 call json_object_array_length@PLT
 cmp [rsp+80],rax
 jae .Lsub_missing
 mov rdi,[rsp+16]
 mov rsi,[rsp+80]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 cmp qword ptr [rsp+96],0
 je 1f
 cmp byte ptr [rax],0
 je .Lsub_missing
1: call .Lappend
 test eax,eax
 jnz .Lsub_fail
 jmp .Lsub_next
.Lsub_all:
 mov [rsp+88],r14
 mov rdi,[rsp+16]
 xor esi,esi
 mov rdx,-1
 call .Ljoin
 test eax,eax
 jnz .Lsub_fail
 cmp qword ptr [rsp+96],0
 je .Lsub_next
 cmp r14,[rsp+88]
 jne .Lsub_next
.Lsub_missing:
 cmp qword ptr [rsp+96],0
 je .Lsub_next
 mov rdi,[rsp+104]
 mov rsi,[rsp+112]
 call .Lspan
 test eax,eax
 jnz .Lsub_fail
 jmp .Lsub_next
.Lsub_slice:
 mov rcx,[rax+48]
 lea rdi,[rbx+rcx]
 mov rsi,[rax+56]
 sub rsi,rcx
 call .Lnumber
 test rax,rax
 jz 1f
 dec rax
1: mov [rsp+80],rax
 mov rax,[rsp+56]
 cmp qword ptr [rax+64],-1
 je .Lsub_slice_all
 mov rcx,[rax+64]
 lea rdi,[rbx+rcx]
 mov rsi,[rax+72]
 sub rsi,rcx
 call .Lnumber
 mov rdx,[rsp+80]
 add rdx,rax
 jnc .Lsub_slice_emit
.Lsub_slice_all:
 mov rdx,-1
.Lsub_slice_emit:
 mov rdi,[rsp+16]
 mov rsi,[rsp+80]
 call .Ljoin
 test eax,eax
 jnz .Lsub_fail
.Lsub_next:
 mov rax,[rsp+64]
 mov [rsp+48],rax
 jmp .Lsub_loop
.Lsub_tail:
 mov rdi,rbx
 add rdi,[rsp+48]
 mov rsi,r15
 sub rsi,[rsp+48]
 call .Lspan
 test eax,eax
 jnz .Lsub_fail
 mov rax,[rsp+24]
 mov [rax],r14
 mov dword ptr [rsp+136],0
 jmp .Lsub_cleanup
.Lsub_fail:
 mov byte ptr [r12],0
 mov dword ptr [rsp+136],1
.Lsub_cleanup:
 mov rdi,[rsp+40]
 call pcre2_match_data_free_8@PLT
 mov rdi,[rsp+32]
 call pcre2_code_free_8@PLT
 mov eax,[rsp+136]
 exit 144
.Lsub_invalid:
 mov eax,1
 exit 144
.size pg_prompt_substitute,.-pg_prompt_substitute
/* Command lookup owns only detached args/document temporaries. */
.globl pg_resource_expand
.type pg_resource_expand,@function
pg_resource_expand:
 mov rax,[rsp+8]
 enter 112
 mov [rsp+16],rax
 mov [rsp+24],rdi
 mov [rsp+32],rsi
 mov rbx,rdx
 mov r15,rcx
 mov r12,r8
 mov r13,r9
 xor r14d,r14d
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+40],0
 mov qword ptr [rsp+48],0
 test rax,rax
 jz .Lexpand_invalid
 mov qword ptr [rax],0
 test r12,r12
 jz .Lexpand_invalid
 test r13,r13
 jz .Lexpand_invalid
 mov byte ptr [r12],0
 test rbx,rbx
 jz .Lexpand_invalid
 mov rdi,rbx
 xor esi,esi
 mov rdx,r15
 call memchr@PLT
 test rax,rax
 jnz .Lexpand_fail
 test r15,r15
 jz .Lexpand_copy
 cmp byte ptr [rbx],'/'
 jne .Lexpand_copy
 cmp r15,7
 jb .Lexpand_template
 mov rdi,rbx
 lea rsi,[rip+.Lskill_prefix]
 mov edx,7
 call memcmp@PLT
 test eax,eax
 jz .Lexpand_skill
.Lexpand_template:
 mov qword ptr [rsp+56],1
.Lexpand_name_scan:
 mov rax,[rsp+56]
 cmp rax,r15
 jae .Lexpand_name_end
 lea rdi,[rbx+rax]
 mov rsi,r15
 sub rsi,rax
 call pg_resource_space
 test rax,rax
 jnz .Lexpand_name_end
 inc qword ptr [rsp+56]
 jmp .Lexpand_name_scan
.Lexpand_name_end:
 cmp qword ptr [rsp+56],1
 jbe .Lexpand_copy
 mov rdi,[rsp+24]
 lea rsi,[rip+.Ltemplates]
 call json_object_object_get@PLT
 mov [rsp+64],rax
 mov qword ptr [rsp+72],0
.Lexpand_template_find:
 mov rdi,[rsp+64]
 call json_object_array_length@PLT
 cmp [rsp+72],rax
 jae .Lexpand_copy
 mov rdi,[rsp+64]
 mov rsi,[rsp+72]
 call json_object_array_get_idx@PLT
 mov [rsp+80],rax
 mov rdi,rax
 lea rsi,[rip+.Lname]
 call .Lfield
 mov [rsp+88],rax
 mov rdi,rax
 call strlen@PLT
 mov rcx,[rsp+56]
 dec rcx
 cmp rax,rcx
 jne .Lexpand_template_next
 mov rdi,[rsp+88]
 lea rsi,[rbx+1]
 mov rdx,rcx
 call memcmp@PLT
 test eax,eax
 jz .Lexpand_template_found
.Lexpand_template_next:
 inc qword ptr [rsp+72]
 jmp .Lexpand_template_find
.Lexpand_template_found:
 mov rax,[rsp+56]
 lea rdi,[rbx+rax]
 mov rsi,r15
 sub rsi,rax
 mov rdx,rsp
 call pg_prompt_args
 test eax,eax
 jnz .Lexpand_fail
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lbody]
 call .Lfield
 mov rbx,rax
 mov rdi,rax
 call strlen@PLT
 mov rsi,rax
 mov rdi,rbx
 mov rdx,[rsp]
 mov rcx,r12
 mov r8,r13
 mov r9,[rsp+16]
 call pg_prompt_substitute
 test eax,eax
 jnz .Lexpand_fail
 jmp .Lexpand_success
.Lexpand_skill:
 mov qword ptr [rsp+56],7
.Lexpand_skill_name:
 mov rax,[rsp+56]
 cmp rax,r15
 jae .Lexpand_skill_lookup
 cmp byte ptr [rbx+rax],' '
 je .Lexpand_skill_lookup
 inc qword ptr [rsp+56]
 jmp .Lexpand_skill_name
.Lexpand_skill_lookup:
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lskills]
 call json_object_object_get@PLT
 mov [rsp+64],rax
 mov qword ptr [rsp+72],0
.Lexpand_skill_find:
 mov rdi,[rsp+64]
 call json_object_array_length@PLT
 cmp [rsp+72],rax
 jae .Lexpand_template
 mov rdi,[rsp+64]
 mov rsi,[rsp+72]
 call json_object_array_get_idx@PLT
 mov [rsp+80],rax
 mov rdi,rax
 lea rsi,[rip+.Lname]
 call .Lfield
 mov [rsp+88],rax
 mov rdi,rax
 call strlen@PLT
 mov rcx,[rsp+56]
 sub rcx,7
 cmp rax,rcx
 jne .Lexpand_skill_next
 mov rdi,[rsp+88]
 lea rsi,[rbx+7]
 mov rdx,rcx
 call memcmp@PLT
 test eax,eax
 jz .Lexpand_skill_found
.Lexpand_skill_next:
 inc qword ptr [rsp+72]
 jmp .Lexpand_skill_find
.Lexpand_skill_found:
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lfile]
 call .Lfield
 mov rdi,rax
 lea rsi,[rsp+8]
 call pg_resource_document
 test eax,eax
 jnz .Lexpand_template
 lea rdi,[rip+.Lskill_open]
 call .Lappend
 test eax,eax
 jnz .Lexpand_fail
 mov rdi,[rsp+88]
 call .Lappend
 test eax,eax
 jnz .Lexpand_fail
 lea rdi,[rip+.Lskill_location]
 call .Lappend
 test eax,eax
 jnz .Lexpand_fail
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lfile]
 call .Lfield
 mov rdi,rax
 call .Lappend
 test eax,eax
 jnz .Lexpand_fail
 lea rdi,[rip+.Lskill_refs]
 call .Lappend
 test eax,eax
 jnz .Lexpand_fail
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lbase]
 call .Lfield
 mov rdi,rax
 call .Lappend
 test eax,eax
 jnz .Lexpand_fail
 lea rdi,[rip+.Lperiod]
 call .Lappend
 test eax,eax
 jnz .Lexpand_fail
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lbody]
 call .Lfield
 mov [rsp+96],rax
 mov rdi,rax
 call strlen@PLT
 mov rsi,rax
 mov rdi,[rsp+96]
 call pg_resource_trim
 mov rdi,rax
 mov rsi,rdx
 call .Lspan
 test eax,eax
 jnz .Lexpand_fail
 lea rdi,[rip+.Lskill_close]
 call .Lappend
 test eax,eax
 jnz .Lexpand_fail
 mov rax,[rsp+56]
 cmp rax,r15
 jae .Lexpand_written
 inc rax
 lea rdi,[rbx+rax]
 mov rsi,r15
 sub rsi,rax
 call pg_resource_trim
 test rdx,rdx
 jz .Lexpand_written
 mov [rsp+96],rax
 mov [rsp+104],rdx
 lea rdi,[rip+.Lparagraph]
 call .Lappend
 test eax,eax
 jnz .Lexpand_fail
 mov rdi,[rsp+96]
 mov rsi,[rsp+104]
 call .Lspan
 test eax,eax
 jnz .Lexpand_fail
 jmp .Lexpand_written
.Lexpand_copy:
 mov rdi,rbx
 mov rsi,r15
 call .Lspan
 test eax,eax
 jnz .Lexpand_fail
.Lexpand_written:
 mov rax,[rsp+16]
 mov [rax],r14
.Lexpand_success:
 mov dword ptr [rsp+40],0
 jmp .Lexpand_cleanup
.Lexpand_fail:
 mov byte ptr [r12],0
 mov rax,[rsp+16]
 mov qword ptr [rax],0
 mov dword ptr [rsp+40],1
.Lexpand_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,[rsp+40]
 exit 112
.Lexpand_invalid:
 mov eax,1
 exit 112
.size pg_resource_expand,.-pg_resource_expand
.section .note.GNU-stack,"",@progbits
