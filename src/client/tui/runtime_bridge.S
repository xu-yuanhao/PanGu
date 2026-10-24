#include "runtime_bridge_layout.h"
#include "tui_layout.h"
.intel_syntax noprefix
.text
.globl pg_tui_bridge_init
.type pg_tui_bridge_init,@function
pg_tui_bridge_init:
    test rdi,rdi
    jz .Linit_bad
    cmp qword ptr [rsp+16],0
    je .Linit_bad
    cmp qword ptr [rsp+24],0
    je .Linit_bad
    cmp qword ptr [rsp+32],0
    je .Linit_bad
    mov r8,rdi
    xor eax,eax
    mov ecx,PG_B_BYTES/8
    rep stosq
    mov rdi,r8
    lea rsi,[rsp+8]
    mov ecx,4
    rep movsq
    lea rax,[rip+.Lruntime_name]
    mov [r8+PG_B_OPS+TUI_LI_OPS_NAME],rax
    mov [r8+PG_B_OPS+TUI_LI_OPS_CTX],r8
    lea rax,[rip+.Lsubmit]
    mov [r8+PG_B_OPS+TUI_LI_OPS_SUBMIT],rax
    lea rax,[rip+.Lcancel]
    mov [r8+PG_B_OPS+TUI_LI_OPS_CANCEL],rax
    lea rax,[rip+.Lpoll]
    mov [r8+PG_B_OPS+TUI_LI_OPS_POLL],rax
    lea rax,[rip+.Lshutdown]
    mov [r8+PG_B_OPS+TUI_LI_OPS_SHUTDOWN],rax
    xor eax,eax
    ret
.Linit_bad:
    mov eax,1
    ret
.size pg_tui_bridge_init,.-pg_tui_bridge_init

.Lsubmit:
    cmp dword ptr [rdi+PG_B_STOPPED],0
    jne .Lsubmit_error
    cmp dword ptr [rdi+PG_B_ACTIVE],0
    jne .Lsubmit_busy
    push rbx
    mov rbx,rdi
    mov rdi,[rbx]
    call [rbx+8]
    test eax,eax
    jnz .Lsubmit_pop_error
    mov dword ptr [rbx+PG_B_ACTIVE],1
    pop rbx
    xor eax,eax
    ret
.Lsubmit_pop_error:
    pop rbx
.Lsubmit_error:
    mov eax,TUI_LI_SUBMIT_ERROR
    ret
.Lsubmit_busy:
    mov eax,TUI_LI_SUBMIT_BUSY
    ret
.Lcancel:
    cmp dword ptr [rdi+PG_B_ACTIVE],0
    je .Lcancel_none
    cmp dword ptr [rdi+PG_B_STOPPED],0
    jne .Lcancel_none
    push rbx
    mov rbx,rdi
    mov rdi,[rbx]
    call [rbx+16]
    pop rbx
    mov eax,1
    ret
.Lcancel_none:
    xor eax,eax
    ret

.Lfield:
    test rsi,rsi
    jz .Lfield_bad
    test rdx,rdx
    jz .Lfield_bad
    cmp rdx,64
    jae .Lfield_bad
    xor ecx,ecx
.Lfield_check:
    cmp byte ptr [rsi+rcx],0
    je .Lfield_bad
    inc rcx
    cmp rcx,rdx
    jb .Lfield_check
    mov rcx,rdx
    rep movsb
    mov byte ptr [rdi],0
    xor eax,eax
    ret
.Lfield_bad:
    mov eax,1
    ret

.Lpoll:
    push rbx
    push r12
    push r13
    sub rsp,64
    mov rbx,rdi
    mov r12,rsi
    test r12,r12
    jz .Lpoll_bad
    mov rdi,r12
    xor eax,eax
    mov ecx,TUI_LI_EVENT_SIZE/8
    rep stosq
    cmp dword ptr [rbx+PG_B_STOPPED],0
    jne .Lpoll_zero
    mov rdi,rsp
    mov ecx,7
    rep stosq
    mov rdi,[rbx]
    mov rsi,rsp
    call [rbx+24]
    test eax,eax
    jle .Lpoll_return
    mov eax,[rsp]
    cmp eax,4
    ja .Lpoll_bad
    lea ecx,[rax+1]
    mov [r12+TUI_LI_EVENT_KIND],ecx
    test eax,eax
    jz .Lstarted
    cmp eax,1
    je .Ltext
    cmp eax,4
    je .Lfinished
    lea rdi,[rbx+PG_B_ID]
    mov rsi,[rsp+24]
    mov rdx,[rsp+32]
    call .Lfield
    test eax,eax
    jnz .Lpoll_bad
    lea rdi,[rbx+PG_B_NAME]
    mov rsi,[rsp+40]
    mov rdx,[rsp+48]
    call .Lfield
    test eax,eax
    jnz .Lpoll_bad
    lea rax,[rbx+PG_B_ID]
    mov [r12+TUI_LI_EVENT_CALL_ID],rax
    lea rax,[rbx+PG_B_NAME]
    mov [r12+TUI_LI_EVENT_TOOL_NAME],rax
    xor eax,eax
    cmp dword ptr [rsp+4],0
    sete al
    mov [r12+TUI_LI_EVENT_TOOL_OK],eax
    jmp .Ldetail
.Lstarted:
    mov dword ptr [rbx+PG_B_ACTIVE],1
    jmp .Lpoll_one
.Ltext:
    cmp qword ptr [rsp+16],0
    je .Ltext_valid
    cmp qword ptr [rsp+8],0
    je .Lpoll_bad
.Ltext_valid:
    mov rax,[rsp+8]
    mov [r12+TUI_LI_EVENT_TEXT],rax
    mov rax,[rsp+16]
    mov [r12+TUI_LI_EVENT_TEXT_LEN],rax
    jmp .Lpoll_one
.Lfinished:
    mov eax,[rsp+4]
    cmp eax,2
    ja .Lpoll_bad
    inc eax
    mov [r12+TUI_LI_EVENT_RUN_STATUS],eax
    mov dword ptr [rbx+PG_B_ACTIVE],0
.Ldetail:
    cmp qword ptr [rsp+16],0
    je .Ldetail_valid
    cmp qword ptr [rsp+8],0
    je .Lpoll_bad
.Ldetail_valid:
    mov rax,[rsp+8]
    mov [r12+TUI_LI_EVENT_DETAIL],rax
    mov rax,[rsp+16]
    mov [r12+TUI_LI_EVENT_DETAIL_LEN],rax
.Lpoll_one:
    mov eax,1
    jmp .Lpoll_return
.Lpoll_zero:
    xor eax,eax
    jmp .Lpoll_return
.Lpoll_bad:
    mov eax,-1
.Lpoll_return:
    add rsp,64
    pop r13
    pop r12
    pop rbx
    ret

.Lshutdown:
    push rbx
    push r12
    push r13
    sub rsp,80
    mov rbx,rdi
    cmp dword ptr [rbx+PG_B_STOPPED],0
    jne .Lshutdown_return
    cmp dword ptr [rbx+PG_B_ACTIVE],0
    je .Lshutdown_stop
    mov rdi,[rbx]
    call [rbx+16]
    xor r12d,r12d
.Lshutdown_drain:
    cmp r12d,8
    jae .Lshutdown_stop
    cmp dword ptr [rbx+PG_B_ACTIVE],0
    je .Lshutdown_stop
    mov rdi,rbx
    mov rsi,rsp
    call .Lpoll
    test eax,eax
    js .Lshutdown_stop
    inc r12d
    jmp .Lshutdown_drain
.Lshutdown_stop:
    mov dword ptr [rbx+PG_B_STOPPED],1
.Lshutdown_return:
    add rsp,80
    pop r13
    pop r12
    pop rbx
    ret
.section .rodata
.Lruntime_name: .asciz "native-agent / MiMo"
.section .note.GNU-stack,"",@progbits
