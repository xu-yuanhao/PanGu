/* Pi Coding Agent resource-loader/agent-session native adaptation.
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
.Lprompt: .asciz "systemPrompt"
.Lrevision: .asciz "revision"
.Lskills: .asciz "skills"
.Ltemplates: .asciz "templates"
.Ldiagnostics: .asciz "diagnostics"
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
.globl pg_resources_open
.type pg_resources_open,@function
pg_resources_open:
 enter 0
 mov rbx,rdi
 test rdi,rdi
 jz .Lopen_invalid
 pxor xmm0,xmm0
 movdqu [rdi],xmm0
 movdqu [rdi+16],xmm0
 movdqu [rdi+32],xmm0
 movdqu [rdi+48],xmm0
 test rsi,rsi
 jz .Lopen_invalid
 cmp qword ptr [rsi+8],0
 je .Lopen_invalid
 test rdx,rdx
 jz .Lopen_invalid
 mov [rbx],rsi
 mov [rbx+8],rdx
 mov rdi,rdx
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lopen_invalid
 mov rdi,rbx
 call pg_resources_reload
 test eax,eax
 jz .Lopen_end
 mov qword ptr [rbx],0
 mov qword ptr [rbx+8],0
 mov dword ptr [rbx+60],1
 jmp .Lopen_end
.Lopen_invalid:
 test rbx,rbx
 jz 1f
 mov qword ptr [rbx],0
 mov qword ptr [rbx+8],0
 mov dword ptr [rbx+60],1
1: mov eax,4
.Lopen_end:
 exit 0
.size pg_resources_open,.-pg_resources_open
/* Release each independently owned candidate field. */
.Lrelease:
 enter 0
 mov rbx,rdi
 xor r12d,r12d
1: mov rdi,[rbx+r12]
 call json_object_put@PLT
 mov qword ptr [rbx+r12],0
 add r12,8
 cmp r12,32
 jb 1b
 exit 0
.globl pg_resources_reload
.type pg_resources_reload,@function
pg_resources_reload:
 enter 48
 mov rbx,rdi
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 test rbx,rbx
 jz .Lreload_closed
 cmp dword ptr [rbx+60],0
 jne .Lreload_closed
 mov r12,[rbx]
 test r12,r12
 jz .Lreload_closed
 cmp dword ptr [rbx+56],0
 jne .Lreload_busy
 cmp qword ptr [rbx+48],-1
 je .Lreload_fail_unlocked
 mov dword ptr [rbx+56],1
 cmp qword ptr [r12+16],0
 je .Lreload_build
 mov rdi,[r12]
 call qword ptr [r12+16]
 test eax,eax
 jnz .Lreload_busy_locked
.Lreload_build:
 mov rdi,[r12]
 mov rsi,rsp
 call qword ptr [r12+8]
 test eax,eax
 jnz .Lreload_fail
 xor r13d,r13d
.Lreload_shape:
 mov rdi,[rsp+r13]
 mov esi,4
 cmp r13,24
 jne 1f
 mov esi,6
1: call json_object_is_type@PLT
 test eax,eax
 jz .Lreload_fail
 add r13,8
 cmp r13,32
 jb .Lreload_shape
 mov rdi,[rsp]
 lea rsi,[rip+.Lskills]
 call json_object_object_get@PLT
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lreload_fail
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ltemplates]
 call json_object_object_get@PLT
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lreload_fail
 mov rdi,[rsp+24]
 call json_object_get@PLT
 mov r13,rax
 mov rdi,[rbx+8]
 lea rsi,[rip+.Lprompt]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Lreload_adopt
 mov rdi,r13
 call json_object_put@PLT
 jmp .Lreload_fail
.Lreload_adopt:
 lea rdi,[rbx+16]
 call .Lrelease
 movdqu xmm0,[rsp]
 movdqu [rbx+16],xmm0
 movdqu xmm0,[rsp+16]
 movdqu [rbx+32],xmm0
 inc qword ptr [rbx+48]
 mov dword ptr [rbx+56],0
 xor eax,eax
 exit 48
.Lreload_fail:
 mov rdi,rsp
 call .Lrelease
 mov dword ptr [rbx+56],0
.Lreload_fail_unlocked:
 mov eax,5
 exit 48
.Lreload_busy_locked:
 mov dword ptr [rbx+56],0
.Lreload_busy:
 mov eax,3
 exit 48
.Lreload_closed:
 mov eax,7
 exit 48
.size pg_resources_reload,.-pg_resources_reload
.globl pg_resources_close
.type pg_resources_close,@function
pg_resources_close:
 enter 0
 mov rbx,rdi
 test rdi,rdi
 jz 2f
 cmp dword ptr [rbx+56],0
 jne 1f
 lea rdi,[rbx+16]
 call .Lrelease
 mov qword ptr [rbx],0
 mov qword ptr [rbx+8],0
 mov dword ptr [rbx+60],1
2: xor eax,eax
 exit 0
1: mov eax,3
 exit 0
.size pg_resources_close,.-pg_resources_close
/* Set a borrowed JSON ref, undoing the new retain if insertion fails. */
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
.globl pg_resources_snapshot
.type pg_resources_snapshot,@function
pg_resources_snapshot:
 enter 64
 mov rbx,rdi
 mov r12,rsi
 test rsi,rsi
 jz .Lsnapshot_invalid
 mov qword ptr [rsi],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 test rbx,rbx
 jz .Lsnapshot_closed
 cmp dword ptr [rbx+60],0
 jne .Lsnapshot_closed
 cmp qword ptr [rbx+48],0
 je .Lsnapshot_closed
 call json_object_new_object@PLT
 mov r13,rax
 mov [rsp],rax
 test rax,rax
 jz .Lsnapshot_fail
 mov rdi,[rbx+48]
 call json_object_new_uint64@PLT
 test rax,rax
 jz .Lsnapshot_fail
 mov r14,rax
 mov rdi,r13
 lea rsi,[rip+.Lrevision]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz 1f
 mov rdi,r14
 call json_object_put@PLT
 jmp .Lsnapshot_fail
1: mov rdi,r13
 lea rsi,[rip+.Lskills]
 mov rdx,[rbx+16]
 call .Lreference
 test eax,eax
 jnz .Lsnapshot_fail
 mov rdi,r13
 lea rsi,[rip+.Ltemplates]
 mov rdx,[rbx+24]
 call .Lreference
 test eax,eax
 jnz .Lsnapshot_fail
 mov rdi,r13
 lea rsi,[rip+.Lprompt]
 mov rdx,[rbx+40]
 call .Lreference
 test eax,eax
 jnz .Lsnapshot_fail
 call json_object_new_array@PLT
 mov r14,rax
 mov [rsp+8],rax
 test rax,rax
 jz .Lsnapshot_fail
 mov r15d,16
.Lsnapshot_diags:
 mov rdi,[rbx+r15]
 lea rsi,[rip+.Ldiagnostics]
 call json_object_object_get@PLT
 mov [rsp+24],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lsnapshot_next_catalog
 mov qword ptr [rsp+32],0
.Lsnapshot_diag_loop:
 mov rdi,[rsp+24]
 call json_object_array_length@PLT
 cmp [rsp+32],rax
 jae .Lsnapshot_next_catalog
 mov rdi,[rsp+24]
 mov rsi,[rsp+32]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+40],rax
 mov rdi,r14
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz 2f
 mov rdi,[rsp+40]
 call json_object_put@PLT
 jmp .Lsnapshot_fail
2: inc qword ptr [rsp+32]
 jmp .Lsnapshot_diag_loop
.Lsnapshot_next_catalog:
 add r15,8
 cmp r15,40
 jb .Lsnapshot_diags
 mov rdi,r13
 lea rsi,[rip+.Ldiagnostics]
 mov rdx,r14
 call .Lreference
 test eax,eax
 jnz .Lsnapshot_fail
 mov rdi,r13
 lea rsi,[rsp+16]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lsnapshot_fail
 mov rax,[rsp+16]
 mov [r12],rax
 mov qword ptr [rsp+16],0
 mov dword ptr [rsp+48],0
 jmp .Lsnapshot_cleanup
.Lsnapshot_fail:
 mov dword ptr [rsp+48],5
.Lsnapshot_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,[rsp+48]
 exit 64
.Lsnapshot_closed:
 mov eax,7
 exit 64
.Lsnapshot_invalid:
 mov eax,4
 exit 64
.size pg_resources_snapshot,.-pg_resources_snapshot
.globl pg_resources_expand
.type pg_resources_expand,@function
pg_resources_expand:
 test rdi,rdi
 jz .Lexpand_closed
 cmp dword ptr [rdi+60],0
 jne .Lexpand_closed
 cmp qword ptr [rdi+48],0
 je .Lexpand_closed
 push r9
 mov r9,r8
 mov r8,rcx
 mov rcx,rdx
 mov rdx,rsi
 mov rsi,[rdi+16]
 mov rdi,[rdi+24]
 call pg_resource_expand
 add rsp,8
 ret
.Lexpand_closed:
 test r9,r9
 jz 1f
 mov qword ptr [r9],0
1: test rcx,rcx
 jz 2f
 test r8,r8
 jz 2f
 mov byte ptr [rcx],0
2: mov eax,7
 ret
.size pg_resources_expand,.-pg_resources_expand
.section .note.GNU-stack,"",@progbits
