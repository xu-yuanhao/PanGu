/* Pi Durable generation.ts prepare and prompt.ts renderSections, d5b3de22.
   MIT, docs/licenses/pi.md. Native continuation and phase-owned snapshots. */
.intel_syntax noprefix
.macro PF n
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\n
.endm
.macro PE n
 add rsp,\n
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm

.text
/* Preparation owns inputs, composed snapshot and renderer through phase cleanup.
   Layout: preparer, inputs[3], renderer, rendered, composed[1376], offered[1312], final. */
.globl pg_prompt_preparer_poll
.type pg_prompt_preparer_poll,@function
pg_prompt_preparer_poll:
 PF 32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov qword ptr [r14],0
 mov qword ptr [rsp],0
 mov rbp,[r13]
 test rbp,rbp
 jnz .Lpp_poll
 mov edi,1
 mov esi,2752
 call calloc@PLT
 mov rbp,rax
 test rax,rax
 jz .Lpp_bad
 mov [r13],rax
 mov [rax],rbx
 mov rdi,[rbx+16]
 mov rsi,r12
 lea rdx,[rbp+8]
 call [rbx+24]
 test eax,eax
 jnz .Lpp_bad
 mov rdi,[rbx]
 mov rsi,[rbp+8]
 mov rdx,[rbx+8]
 lea rcx,[rbp+48]
 call pg_extensions_resolve@PLT
 test eax,eax
 jnz .Lpp_bad
 lea rdi,[rbp+48]
 mov rsi,[rbp+16]
 lea rdx,[rbp+32]
 call pg_extension_prepare_open@PLT
 test eax,eax
 jnz .Lpp_bad
.Lpp_poll:
 cmp dword ptr [rbp+2744],1
 je .Lpp_completed
 cmp qword ptr [rbp+2736],0
 jne .Lpp_bad
 mov rdi,[rbp+32]
 lea rsi,[rbp+40]
 call pg_extension_prepare_poll@PLT
 cmp eax,1
 je .Lpp_pending
 test eax,eax
 jnz .Lpp_bad
 mov rdi,[rbp+40]
 mov rsi,[rbp+8]
 lea rdx,[rbp+2736]
 call pg_agent_state_prepare@PLT
 test eax,eax
 jnz .Lpp_bad
 lea rdi,[rbp+48]
 mov rsi,[rbp+2736]
 mov edx,1
 lea rcx,[rbp+1424]
 call pg_agent_tools_select@PLT
 test eax,eax
 jnz .Lpp_bad
 lea rdi,[rbp+1424]
 mov rsi,rsp
 call pg_agent_tools_names@PLT
 test eax,eax
 jnz .Lpp_bad
 mov rdi,[rbp+2736]
 lea rsi,[rip+.Lpp_offered]
 mov rdx,[rsp]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lpp_bad
 mov qword ptr [rsp],0
 mov rdi,[rbx+16]
 call [rbx+40]
 cvttsd2si rcx,xmm0
 mov rdi,[rbp+24]
 mov rsi,[rbp+2736]
 lea rdx,[rbp+1424]
 mov r8,[rbp+8]
 call pg_prompt_configure
 test eax,eax
 jnz .Lpp_bad
 mov rdi,[rbx+16]
 mov rsi,[rbp+2736]
 call [rbx+32]
 test eax,eax
 jnz .Lpp_bad
 mov rdi,[rbp+2736]
 lea rsi,[rip+.Lpp_shown]
 call json_object_object_del@PLT
.Lpp_completed:
 mov dword ptr [rbp+2744],1
 mov rdi,[rbp+2736]
 call json_object_get@PLT
 mov [r14],rax
 xor r12d,r12d
 jmp .Lpp_end
.Lpp_pending:
 mov r12d,1
 jmp .Lpp_end
.Lpp_bad:
 mov r12d,-5
.Lpp_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,r12d
 PE 32
.globl pg_prompt_preparer_close
.type pg_prompt_preparer_close,@function
pg_prompt_preparer_close:
 test rsi,rsi
 jz .Lpp_close_return
 push rbx
 mov rbx,rsi
 mov rdi,[rbx+32]
 call pg_extension_prepare_close@PLT
 lea rdi,[rbx+48]
 call pg_extensions_close@PLT
 mov rdi,[rbx+8]
 call json_object_put@PLT
 mov rdi,[rbx+16]
 call json_object_put@PLT
 mov rdi,[rbx+24]
 call json_object_put@PLT
 mov rdi,[rbx+40]
 call json_object_put@PLT
 mov rdi,[rbx+2736]
 call json_object_put@PLT
 mov rdi,rbx
 call free@PLT
 pop rbx
.Lpp_close_return:
 ret
.section .rodata
.Lpp_offered: .asciz "offeredTools"
.Lpp_shown: .asciz "shown"
