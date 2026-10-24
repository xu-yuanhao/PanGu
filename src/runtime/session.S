#include "session_layout.h"
.intel_syntax noprefix
.text
.globl pg_session_volatile_commit
.type pg_session_volatile_commit,@function
pg_session_volatile_commit:
    inc rdx
    mov [rcx],rdx
    xor eax,eax
    ret
.size pg_session_volatile_commit,.-pg_session_volatile_commit

.globl pg_session_commit
.type pg_session_commit,@function
pg_session_commit:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp,PG_SESSION_STACK
    mov rbx,rdi
    mov r12,rsi
    mov r13,rdx
    test rbx,rbx
    jz .Lreject
    test r12,r12
    jz .Lreject
    test r13,r13
    jz .Lreject
    cmp dword ptr [r12+PG_SC_BUSY],0
    jne .Lreject
    cmp dword ptr [r12+PG_SC_FAILED],0
    jne .Lreject
    cmp qword ptr [r12+8],0
    je .Lreject
    cmp qword ptr [r12+PG_SC_SEQ],-1
    je .Lreject
    cmp qword ptr [rbx+PG_S_COUNT],64
    ja .Lreject
    xor eax,eax
    mov rdi,rsp
    mov ecx,6
    rep stosq
    mov eax,[r13]
    mov [rsp],eax
    cmp eax,1
    je .Lboundary_check
    cmp eax,2
    jne .Lboundary_checked
.Lboundary_check:
    cmp qword ptr [r13+40],0
    jne .Lreject
.Lboundary_checked:
    cmp eax,0
    je .Lappend
    cmp eax,1
    je .Lsystem
    cmp eax,3
    ja .Lreject
    jmp .Lcheckpoint
.Lsystem:
    mov rcx,[r13+32]
    cmp rcx,4096
    jae .Lreject
    mov rsi,[r13+24]
    test rcx,rcx
    jz .Lsystem_copy
    test rsi,rsi
    jz .Lreject
.Lsystem_copy:
    mov [rsp+32],rcx
    lea rdi,[rsp+PG_SESSION_DATA]
    mov [rsp+24],rdi
    rep movsb
    mov byte ptr [rdi],0
    jmp .Lcheckpoint
.Lappend:
    mov r14,[r13+16]
    test r14,r14
    jz .Lreject
    cmp r14,2
    ja .Lreject
    mov rax,64
    sub rax,[rbx+PG_S_COUNT]
    cmp r14,rax
    ja .Lreject
    mov r15,[r13+8]
    test r15,r15
    jz .Lreject
    mov [rsp+16],r14
    lea rdi,[rsp+PG_SESSION_DATA]
    mov [rsp+8],rdi
    imul rcx,r14,PG_MSG_BYTES
    mov rsi,r15
    rep movsb
    lea r15,[rsp+PG_SESSION_DATA]
.Lvalidate:
    cmp dword ptr [r15],3
    ja .Lreject
    mov rax,[r15+PG_MSG_SIZE]
    cmp rax,4096
    jae .Lreject
    cmp byte ptr [r15+PG_MSG_TEXT+rax],0
    jne .Lreject
    cmp dword ptr [r15],2
    jb .Lvalidated
    lea rdi,[r15+PG_MSG_ID]
    call .Lid
    test eax,eax
    jz .Lreject
    cmp dword ptr [r15],2
    jne .Lvalidated
    lea rdi,[r15+PG_MSG_NAME]
    call .Lid
    test eax,eax
    jz .Lreject
.Lvalidated:
    add r15,PG_MSG_BYTES
    dec r14
    jnz .Lvalidate
.Lcheckpoint:
    mov rax,[r13+40]
    test rax,rax
    jz .Lno_checkpoint
    mov rsi,rax
    lea rdi,[rsp+PG_SESSION_CP]
    mov ecx,5
    rep movsq
    lea r15,[rsp+PG_SESSION_CP]
    mov [rsp+40],r15
    cmp qword ptr [r15],0
    je .Lreject
    cmp dword ptr [r15+PG_CP_PHASE],1
    jb .Lreject
    cmp dword ptr [r15+PG_CP_PHASE],4
    ja .Lreject
    cmp dword ptr [r15+PG_CP_TURNS],16
    ja .Lreject
    cmp dword ptr [r15+PG_CP_RESTARTS],5
    ja .Lreject
    cmp dword ptr [r15+PG_CP_OUTCOME],2
    ja .Lreject
    cmp dword ptr [r15+PG_CP_PHASE],3
    je .Lcheckpoint_outcome
    cmp dword ptr [r15+PG_CP_OUTCOME],0
    jne .Lreject
.Lcheckpoint_outcome:
    mov r14,[rbx+PG_S_COUNT]
    cmp dword ptr [rsp],0
    jne .Lcheckpoint_count
    add r14,[rsp+16]
.Lcheckpoint_count:
    mov rax,[r15+PG_CP_INPUT]
    cmp rax,r14
    jae .Lreject
    call .Lmessage_at
    cmp dword ptr [rax],0
    jne .Lreject
    mov rax,[r15+PG_CP_CALL]
    cmp dword ptr [r15+PG_CP_PHASE],2
    je .Lcheckpoint_call
    cmp rax,-1
    jne .Lcheckpoint_has_call
    lea rax,[r14-1]
    call .Lmessage_at
    cmp dword ptr [rax],2
    je .Lreject
    jmp .Lcheckpoint_identity
.Lcheckpoint_has_call:
    cmp dword ptr [r15+PG_CP_PHASE],4
    jne .Lreject
.Lcheckpoint_call:
    cmp dword ptr [r15+PG_CP_TURNS],0
    je .Lreject
    inc rax
    cmp rax,r14
    jne .Lreject
    dec rax
    call .Lmessage_at
    cmp dword ptr [rax],2
    jne .Lreject
.Lcheckpoint_identity:
    mov rax,[r12+PG_SC_RUN]
    cmp [r15],rax
    jb .Lreject
    je .Lsame_run
    test rax,rax
    jz .Lnew_run
    cmp dword ptr [r12+PG_SC_RUN+PG_CP_PHASE],3
    jne .Lreject
.Lnew_run:
    cmp dword ptr [rsp],0
    jne .Lreject
    cmp dword ptr [r15+PG_CP_PHASE],1
    jne .Lreject
    cmp qword ptr [rsp+16],1
    jne .Lreject
    cmp dword ptr [r15+PG_CP_TURNS],0
    jne .Lreject
    cmp dword ptr [r15+PG_CP_RESTARTS],0
    jne .Lreject
    mov rax,[rbx+PG_S_COUNT]
    cmp [r15+PG_CP_INPUT],rax
    jne .Lreject
    jmp .Lstore
.Lsame_run:
    cmp dword ptr [r12+PG_SC_RUN+PG_CP_PHASE],3
    jne .Lsame_active
    cmp dword ptr [r15+PG_CP_PHASE],3
    jne .Lreject
    mov eax,[r12+PG_SC_RUN+PG_CP_OUTCOME]
    cmp [r15+PG_CP_OUTCOME],eax
    jne .Lreject
.Lsame_active:
    mov rax,[r12+PG_SC_RUN+PG_CP_INPUT]
    cmp [r15+PG_CP_INPUT],rax
    jne .Lreject
    mov eax,[r12+PG_SC_RUN+PG_CP_TURNS]
    cmp [r15+PG_CP_TURNS],eax
    jb .Lreject
    jmp .Lstore
.Lno_checkpoint:
    cmp dword ptr [rsp],3
    je .Lreject
    cmp qword ptr [r12+PG_SC_RUN],0
    je .Lstore
    cmp dword ptr [r12+PG_SC_RUN+PG_CP_PHASE],3
    jne .Lreject
.Lstore:
    mov dword ptr [r12+PG_SC_BUSY],1
    mov qword ptr [rsp+48],0
    mov rdi,[r12]
    mov rsi,rsp
    mov rdx,[r12+PG_SC_SEQ]
    lea rcx,[rsp+48]
    call [r12+8]
    cmp eax,1
    je .Lstore_reject
    test eax,eax
    jnz .Luncertain
    mov rax,[r12+PG_SC_SEQ]
    inc rax
    cmp [rsp+48],rax
    jne .Luncertain
    cmp dword ptr [rsp],0
    je .Ladopt_append
    cmp dword ptr [rsp],1
    je .Ladopt_system
    cmp dword ptr [rsp],3
    je .Ladopted
    lea rdi,[r12+PG_SC_RUN]
    xor eax,eax
    mov ecx,5
    rep stosq
    mov rcx,[rbx+PG_S_COUNT]
    imul rcx,PG_MSG_BYTES
    mov rdi,rbx
    xor eax,eax
    rep stosb
    mov qword ptr [rbx+PG_S_COUNT],0
    jmp .Ladopted
.Ladopt_system:
    mov rsi,[rsp+24]
    lea rdi,[rbx+PG_S_SYSTEM]
    mov rcx,[rsp+32]
    mov [rbx+PG_S_SYSTEM_SIZE],rcx
    inc rcx
    rep movsb
    jmp .Ladopted
.Ladopt_append:
    imul rdi,[rbx+PG_S_COUNT],PG_MSG_BYTES
    add rdi,rbx
    mov rsi,[rsp+8]
    imul rcx,[rsp+16],PG_MSG_BYTES
    rep movsb
    mov rax,[rsp+16]
    add [rbx+PG_S_COUNT],rax
.Ladopted:
    mov rsi,[rsp+40]
    test rsi,rsi
    jz .Ladopt_sequence
    lea rdi,[r12+PG_SC_RUN]
    mov ecx,5
    rep movsq
.Ladopt_sequence:
    mov rax,[rsp+48]
    mov [r12+PG_SC_SEQ],rax
    mov dword ptr [r12+PG_SC_BUSY],0
    xor eax,eax
    jmp .Lreturn
.Luncertain:
    mov dword ptr [r12+PG_SC_FAILED],1
.Lstore_reject:
    mov dword ptr [r12+PG_SC_BUSY],0
.Lreject:
    mov eax,1
.Lreturn:
    add rsp,PG_SESSION_STACK
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
.Lmessage_at:
    cmp rax,[rbx+PG_S_COUNT]
    jae .Lmessage_new
    imul rax,PG_MSG_BYTES
    add rax,rbx
    ret
.Lmessage_new:
    sub rax,[rbx+PG_S_COUNT]
    imul rax,PG_MSG_BYTES
    add rax,[rsp+16]
    ret
.Lid:
    xor ecx,ecx
.Lid_loop:
    movzx eax,byte ptr [rdi+rcx]
    test eax,eax
    jz .Lid_end
    cmp eax,32
    jb .Lid_bad
    cmp eax,127
    je .Lid_bad
    inc ecx
    cmp ecx,64
    jb .Lid_loop
.Lid_bad:
    xor eax,eax
    ret
.Lid_end:
    xor eax,eax
    test ecx,ecx
    setne al
    ret
.size pg_session_commit,.-pg_session_commit
.section .note.GNU-stack,"",@progbits
