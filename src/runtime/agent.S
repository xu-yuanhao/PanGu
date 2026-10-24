#include "agent_layout.h"
.intel_syntax noprefix
.text

.globl pg_agent_init
.type pg_agent_init,@function
pg_agent_init:
    test rdi,rdi
    jz .Linit_bad
    cmp qword ptr [rsp+16],0
    je .Linit_bad
    cmp qword ptr [rsp+24],0
    je .Linit_bad
    cmp qword ptr [rsp+32],0
    je .Linit_bad
    cmp qword ptr [rsp+40],0
    je .Linit_bad
    mov r8,rdi
    xor eax,eax
    mov ecx,PG_A_BYTES/8
    rep stosq
    lea rdi,[r8+PG_A_MODEL]
    lea rsi,[rsp+8]
    mov ecx,8
    rep movsq
    xor eax,eax
    ret
.Linit_bad:
    mov eax,1
    ret
.size pg_agent_init,.-pg_agent_init

.globl pg_agent_client
.type pg_agent_client,@function
pg_agent_client:
    mov [rdi],rsi
    lea rax,[rip+.Lsubmit]
    mov [rdi+8],rax
    lea rax,[rip+.Lcancel]
    mov [rdi+16],rax
    lea rax,[rip+.Lpoll]
    mov [rdi+24],rax
    mov rax,rdi
    ret
.size pg_agent_client,.-pg_agent_client

.globl pg_agent_set_system
.type pg_agent_set_system,@function
pg_agent_set_system:
    test rdi,rdi
    jz .Lsystem_bad
    cmp dword ptr [rdi+PG_A_STATE],PG_ST_IDLE
    jne .Lsystem_bad
    cmp rdx,4096
    jae .Lsystem_bad
    test rdx,rdx
    jz .Lsystem_valid
    test rsi,rsi
    jz .Lsystem_bad
.Lsystem_valid:
    push rbx
    sub rsp,4096
    mov rbx,rdi
    mov r8,rdx
    mov rdi,rsp
    mov rcx,rdx
    rep movsb
    mov byte ptr [rdi],0
    lea rdi,[rbx+PG_A_SYSTEM]
    mov rsi,rsp
    lea rcx,[r8+1]
    rep movsb
    mov [rbx+PG_A_SYSTEM_SIZE],r8
    add rsp,4096
    pop rbx
    xor eax,eax
    ret
.Lsystem_bad:
    mov eax,1
    ret
.size pg_agent_set_system,.-pg_agent_set_system

.globl pg_agent_reset_session
.type pg_agent_reset_session,@function
pg_agent_reset_session:
    test rdi,rdi
    jz .Lreset_bad
    cmp dword ptr [rdi+PG_A_STATE],PG_ST_IDLE
    jne .Lreset_bad
    mov rcx,[rdi+PG_A_COUNT]
    cmp rcx,64
    ja .Lreset_bad
    mov r8,rdi
    imul rcx,PG_MSG_BYTES
    xor eax,eax
    rep stosb
    mov qword ptr [r8+PG_A_COUNT],0
    ret
.Lreset_bad:
    mov eax,1
    ret
.size pg_agent_reset_session,.-pg_agent_reset_session

.Lcopy:
    mov rcx,rdx
    rep movsb
    mov byte ptr [rdi],0
    ret

.Llength:
    xor eax,eax
.Llength_loop:
    cmp eax,64
    je .Llength_end
    cmp byte ptr [rdi+rax],0
    je .Llength_end
    inc eax
    jmp .Llength_loop
.Llength_end:
    ret

.Lidentifier:
    xor eax,eax
    test rsi,rsi
    jz .Lidentifier_end
    cmp rsi,64
    jae .Lidentifier_end
    test rdi,rdi
    jz .Lidentifier_end
    xor ecx,ecx
.Lidentifier_loop:
    movzx edx,byte ptr [rdi+rcx]
    cmp edx,32
    jb .Lidentifier_end
    cmp edx,127
    je .Lidentifier_end
    inc rcx
    cmp rcx,rsi
    jb .Lidentifier_loop
    mov eax,1
.Lidentifier_end:
    ret

# append(owner, role, bytes, length); publish count after completing the record.
.Lappend:
    cmp qword ptr [rdi+PG_A_COUNT],64
    jae .Lappend_bad
    cmp rcx,4096
    jae .Lappend_bad
    test rcx,rcx
    jz .Lappend_valid
    test rdx,rdx
    jz .Lappend_bad
.Lappend_valid:
    mov r8,rdi
    mov r9d,esi
    mov r10,rdx
    mov r11,rcx
    imul rdx,[rdi+PG_A_COUNT],PG_MSG_BYTES
    add rdx,rdi
    mov rdi,rdx
    xor eax,eax
    mov ecx,PG_MSG_BYTES/8
    rep stosq
    mov [rdx],r9d
    mov [rdx+PG_MSG_SIZE],r11
    lea rdi,[rdx+PG_MSG_TEXT]
    mov rsi,r10
    mov rcx,r11
    rep movsb
    mov byte ptr [rdi],0
    inc qword ptr [r8+PG_A_COUNT]
    mov rax,rdx
    ret
.Lappend_bad:
    xor eax,eax
    ret

.Lclose:
    cmp dword ptr [rdi+PG_A_OPEN],0
    je .Lclose_end
    mov dword ptr [rdi+PG_A_OPEN],0
    mov rax,[rdi+PG_A_MODEL+32]
    mov rdi,[rdi+PG_A_MODEL]
    jmp rax
.Lclose_end:
    ret

.Lfinish:
    push rbx
    push r12
    sub rsp,8
    mov rbx,rdi
    mov r12d,esi
    call .Lclose
    lea rdi,[rbx+PG_A_DRAFT]
    xor eax,eax
    mov ecx,PG_MSG_BYTES/8
    rep stosq
    mov [rbx+PG_A_OUTCOME],r12d
    mov dword ptr [rbx+PG_A_STATE],PG_ST_FINISH
    add rsp,8
    pop r12
    pop rbx
    ret

.Lfail:
    mov r8,rdi
    xor ecx,ecx
.Lfail_length:
    cmp ecx,255
    je .Lfail_copy
    cmp byte ptr [rsi+rcx],0
    je .Lfail_copy
    inc ecx
    jmp .Lfail_length
.Lfail_copy:
    lea rdi,[r8+PG_A_ERROR]
    rep movsb
    mov byte ptr [rdi],0
    mov rdi,r8
    mov esi,1
    jmp .Lfinish

.Lsubmit:
    cmp dword ptr [rdi+PG_A_STATE],PG_ST_IDLE
    jne .Lsubmit_bad
    test rdx,rdx
    jz .Lsubmit_bad
    cmp rdx,4096
    jae .Lsubmit_bad
    test rsi,rsi
    jz .Lsubmit_bad
    cmp qword ptr [rdi+PG_A_COUNT],60
    ja .Lsubmit_bad
    push rbx
    mov rbx,rdi
    mov rcx,rdx
    mov rdx,rsi
    xor esi,esi
    call .Lappend
    test rax,rax
    jz .Lsubmit_pop_bad
    mov dword ptr [rbx+PG_A_CANCEL],0
    mov dword ptr [rbx+PG_A_TURNS],0
    mov byte ptr [rbx+PG_A_ERROR],0
    mov dword ptr [rbx+PG_A_STATE],PG_ST_BEGIN
    pop rbx
    xor eax,eax
    ret
.Lsubmit_pop_bad:
    pop rbx
.Lsubmit_bad:
    mov eax,1
    ret

# rbx owns the run. Only settled text enters model-visible history.
.Lsettle_draft:
    mov rax,[rbx+PG_A_COUNT]
    mov [rbx+PG_A_ASSISTANT],rax
    mov rdi,rbx
    mov esi,PG_ROLE_ASSISTANT
    lea rdx,[rbx+PG_A_DRAFT+PG_MSG_TEXT]
    mov rcx,[rbx+PG_A_DRAFT+PG_MSG_SIZE]
    jmp .Lappend

.Lcancel:
    mov eax,[rdi+PG_A_STATE]
    test eax,eax
    jz .Lcancel_end
    cmp eax,6
    je .Lcancel_end
    mov dword ptr [rdi+PG_A_CANCEL],1
.Lcancel_end:
    ret

.Ltool_event:
    sub rsp,8
    mov r8,rdi
    mov r9,rsi
    mov r10,rdx
    mov dword ptr [r8],PG_EV_TOOL_END
    mov [r8+4],ecx
    lea rax,[r10+PG_MSG_TEXT]
    mov [r8+8],rax
    mov rax,[r10+PG_MSG_SIZE]
    mov [r8+16],rax
    lea rdi,[r10+PG_MSG_ID]
    mov [r8+24],rdi
    call .Llength
    mov [r8+32],rax
    lea rdi,[r9+PG_MSG_NAME]
    mov [r8+40],rdi
    call .Llength
    mov [r8+48],rax
    add rsp,8
    ret

.Lpoll:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp,88
    mov rbx,rdi
    mov r12,rsi
    test r12,r12
    jz .Lpoll_zero
    mov rdi,r12
    xor eax,eax
    mov ecx,7
    rep stosq
    mov eax,[rbx+PG_A_STATE]
    test eax,eax
    jz .Lpoll_zero
    cmp dword ptr [rbx+PG_A_CANCEL],0
    je .Ldispatch
    cmp eax,1
    je .Ldispatch
    cmp eax,6
    je .Ldispatch
    cmp dword ptr [rbx+PG_A_OPEN],0
    je .Lcancel_operation
    mov rdi,[rbx+PG_A_MODEL]
    call [rbx+PG_A_MODEL+24]
.Lcancel_operation:
    cmp dword ptr [rbx+PG_A_STATE],PG_ST_TOOL
    jne .Lcancel_finish
    imul r14,[rbx+PG_A_CALL],PG_MSG_BYTES
    add r14,rbx
    mov rdi,rbx
    mov esi,3
    lea rdx,[rip+.Lcancelled]
    mov ecx,9
    call .Lappend
    test rax,rax
    jz .Lcancel_finish
    mov r13,rax
    lea rdi,[r14+PG_MSG_ID]
    call .Llength
    mov rdx,rax
    lea rsi,[r14+PG_MSG_ID]
    lea rdi,[r13+PG_MSG_ID]
    call .Lcopy
    mov dword ptr [r13+PG_MSG_ERROR],1
    mov rdi,rbx
    mov esi,2
    call .Lfinish
    mov rdi,r12
    mov rsi,r14
    mov rdx,r13
    mov ecx,2
    call .Ltool_event
    jmp .Lpoll_one
.Lcancel_finish:
    mov rdi,rbx
    mov esi,2
    call .Lfinish
.Ldispatch:
    mov eax,[rbx+PG_A_STATE]
    cmp eax,PG_ST_BEGIN
    je .Lbegin
    cmp eax,PG_ST_START
    je .Lstart
    cmp eax,PG_ST_STREAM
    je .Lstream
    cmp eax,PG_ST_TOOL
    je .Ltool
    cmp eax,PG_ST_TOOL_END
    je .Ltool_end
    cmp eax,PG_ST_FINISH
    je .Lterminal
    lea rsi,[rip+.Lerror_state]
    jmp .Lpoll_fail
.Lbegin:
    mov dword ptr [rbx+PG_A_STATE],PG_ST_START
    jmp .Lpoll_one
.Lstart:
    inc dword ptr [rbx+PG_A_TURNS]
    cmp dword ptr [rbx+PG_A_TURNS],16
    ja .Lerror_turns
    mov dword ptr [rbx+PG_A_OPEN],1
    mov rdi,[rbx+PG_A_MODEL]
    mov rsi,rbx
    call [rbx+PG_A_MODEL+8]
    test eax,eax
    jnz .Lerror_start
    cmp qword ptr [rbx+PG_A_COUNT],64
    jae .Lerror_capacity
    lea rdi,[rbx+PG_A_DRAFT]
    xor eax,eax
    mov ecx,PG_MSG_BYTES/8
    rep stosq
    mov dword ptr [rbx+PG_A_DRAFT],PG_ROLE_ASSISTANT
    mov dword ptr [rbx+PG_A_STATE],PG_ST_STREAM
    jmp .Lpoll_zero
.Lstream:
    mov rdi,rsp
    xor eax,eax
    mov ecx,7
    rep stosq
    mov rdi,[rbx+PG_A_MODEL]
    mov rsi,rsp
    call [rbx+PG_A_MODEL+16]
    test eax,eax
    jnz .Lerror_stream
    mov eax,[rsp]
    test eax,eax
    jz .Lpoll_zero
    cmp eax,PG_ME_TEXT
    je .Ltext
    cmp eax,PG_ME_CALL
    je .Lcall
    cmp eax,PG_ME_DONE
    je .Lstream_done
    cmp eax,PG_ME_ERROR
    je .Lerror_model
    jmp .Lerror_event
.Lstream_done:
    call .Lsettle_draft
    test rax,rax
    jz .Lerror_capacity
    mov rdi,rbx
    xor esi,esi
    call .Lfinish
    jmp .Lpoll_zero
.Ltext:
    mov r14,[rsp+16]
    cmp r14,4096
    jae .Lerror_text
    test r14,r14
    jz .Ltext_valid
    cmp qword ptr [rsp+8],0
    je .Lerror_text
.Ltext_valid:
    lea r13,[rbx+PG_A_DRAFT]
    mov rax,4096
    sub rax,[r13+PG_MSG_SIZE]
    cmp r14,rax
    jae .Lerror_text
    lea rdi,[rbx+PG_A_TEXT]
    mov rsi,[rsp+8]
    mov rdx,r14
    call .Lcopy
    lea rdi,[r13+PG_MSG_TEXT]
    add rdi,[r13+PG_MSG_SIZE]
    mov rsi,[rsp+8]
    mov rdx,r14
    call .Lcopy
    add [r13+PG_MSG_SIZE],r14
    mov dword ptr [r12],PG_EV_TEXT
    lea rax,[rbx+PG_A_TEXT]
    mov [r12+8],rax
    mov [r12+16],r14
    jmp .Lpoll_one
.Lcall:
    mov rdi,[rsp+24]
    mov rsi,[rsp+32]
    call .Lidentifier
    test eax,eax
    jz .Lerror_event
    mov rdi,[rsp+40]
    mov rsi,[rsp+48]
    call .Lidentifier
    test eax,eax
    jz .Lerror_event
    cmp qword ptr [rbx+PG_A_COUNT],61
    ja .Lerror_result_capacity
    cmp qword ptr [rsp+16],4096
    jae .Lerror_arguments
    cmp qword ptr [rsp+16],0
    je .Lcall_valid
    cmp qword ptr [rsp+8],0
    je .Lerror_arguments
.Lcall_valid:
    call .Lsettle_draft
    test rax,rax
    jz .Lerror_capacity
    mov rax,[rbx+PG_A_COUNT]
    mov [rbx+PG_A_CALL],rax
    mov rdi,rbx
    mov esi,2
    mov rdx,[rsp+8]
    mov rcx,[rsp+16]
    call .Lappend
    test rax,rax
    jz .Lerror_arguments
    mov r13,rax
    lea rdi,[r13+PG_MSG_ID]
    mov rsi,[rsp+24]
    mov rdx,[rsp+32]
    call .Lcopy
    lea rdi,[r13+PG_MSG_NAME]
    mov rsi,[rsp+40]
    mov rdx,[rsp+48]
    call .Lcopy
    mov rdi,rbx
    call .Lclose
    mov dword ptr [r12],PG_EV_TOOL_BEGIN
    lea rax,[r13+PG_MSG_ID]
    mov [r12+24],rax
    mov rax,[rsp+32]
    mov [r12+32],rax
    lea rax,[r13+PG_MSG_NAME]
    mov [r12+40],rax
    mov rax,[rsp+48]
    mov [r12+48],rax
    mov dword ptr [rbx+PG_A_STATE],PG_ST_TOOL
    jmp .Lpoll_one
.Ltool:
    imul r13,[rbx+PG_A_CALL],PG_MSG_BYTES
    add r13,rbx
    cmp qword ptr [rbx+PG_A_TOOLS+8],0
    je .Ltool_denied
    cmp qword ptr [rbx+PG_A_TOOLS+16],0
    je .Ltool_denied
    lea rdi,[r13+PG_MSG_NAME]
    call .Llength
    mov rdx,rax
    mov rdi,[rbx+PG_A_TOOLS]
    lea rsi,[r13+PG_MSG_NAME]
    lea rcx,[r13+PG_MSG_TEXT]
    mov r8,[r13+PG_MSG_SIZE]
    call [rbx+PG_A_TOOLS+8]
    test eax,eax
    jz .Ltool_denied
    lea rdi,[r13+PG_MSG_NAME]
    call .Llength
    mov rdx,rax
    mov qword ptr [rsp+56],0
    lea rax,[rsp+56]
    mov rdi,[rbx+PG_A_TOOLS]
    lea rsi,[r13+PG_MSG_NAME]
    lea rcx,[r13+PG_MSG_TEXT]
    mov r8,[r13+PG_MSG_SIZE]
    lea r9,[rbx+PG_A_TEXT]
    push rax
    push 4096
    call [rbx+PG_A_TOOLS+16]
    add rsp,16
    mov r15d,eax
    cmp qword ptr [rsp+56],4096
    jb .Ltool_append
    lea rsi,[rip+.Ltool_limit]
    jmp .Ltool_error
.Ltool_denied:
    lea rsi,[rip+.Ltool_unavailable]
.Ltool_error:
    lea rdi,[rbx+PG_A_TEXT]
    mov edx,26
    call .Lcopy
    mov qword ptr [rsp+56],26
    mov r15d,1
.Ltool_append:
    mov rdi,rbx
    mov esi,3
    lea rdx,[rbx+PG_A_TEXT]
    mov rcx,[rsp+56]
    call .Lappend
    test rax,rax
    jz .Lerror_result_capacity
    mov r14,rax
    lea rdi,[r13+PG_MSG_ID]
    call .Llength
    mov rdx,rax
    lea rsi,[r13+PG_MSG_ID]
    lea rdi,[r14+PG_MSG_ID]
    call .Lcopy
    xor eax,eax
    test r15d,r15d
    setne al
    mov [r14+PG_MSG_ERROR],eax
    mov dword ptr [rbx+PG_A_STATE],PG_ST_TOOL_END
    jmp .Lpoll_zero
.Ltool_end:
    mov rax,[rbx+PG_A_COUNT]
    dec rax
    imul r13,rax,PG_MSG_BYTES
    add r13,rbx
    imul r14,[rbx+PG_A_CALL],PG_MSG_BYTES
    add r14,rbx
    xor ecx,ecx
    cmp dword ptr [r13+PG_MSG_ERROR],0
    setne cl
    mov rdi,r12
    mov rsi,r14
    mov rdx,r13
    call .Ltool_event
    mov dword ptr [rbx+PG_A_STATE],PG_ST_START
    jmp .Lpoll_one
.Lterminal:
    mov dword ptr [r12],PG_EV_FINISH
    mov eax,[rbx+PG_A_OUTCOME]
    mov [r12+4],eax
    cmp eax,1
    jne .Lterminal_ready
    lea rdi,[rbx+PG_A_ERROR]
    xor eax,eax
.Lterminal_length:
    cmp byte ptr [rdi+rax],0
    je .Lterminal_error
    inc eax
    cmp eax,255
    jb .Lterminal_length
.Lterminal_error:
    mov [r12+8],rdi
    mov [r12+16],rax
.Lterminal_ready:
    mov dword ptr [rbx+PG_A_STATE],PG_ST_IDLE
    jmp .Lpoll_one
.Lerror_turns:
    lea rsi,[rip+.Lerror_turns_text]
    jmp .Lpoll_fail
.Lerror_start:
    lea rsi,[rip+.Lerror_start_text]
    jmp .Lpoll_fail
.Lerror_capacity:
    lea rsi,[rip+.Lerror_capacity_text]
    jmp .Lpoll_fail
.Lerror_stream:
    lea rsi,[rip+.Lerror_stream_text]
    jmp .Lpoll_fail
.Lerror_model:
    lea rsi,[rip+.Lerror_model_text]
    jmp .Lpoll_fail
.Lerror_text:
    lea rsi,[rip+.Lerror_text_text]
    jmp .Lpoll_fail
.Lerror_event:
    lea rsi,[rip+.Lerror_event_text]
    jmp .Lpoll_fail
.Lerror_result_capacity:
    lea rsi,[rip+.Lerror_result_capacity_text]
    jmp .Lpoll_fail
.Lerror_arguments:
    lea rsi,[rip+.Lerror_arguments_text]
.Lpoll_fail:
    mov rdi,rbx
    call .Lfail
.Lpoll_zero:
    xor eax,eax
    jmp .Lpoll_return
.Lpoll_one:
    mov eax,1
.Lpoll_return:
    add rsp,88
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

.section .rodata
.Lcancelled: .asciz "cancelled"
.Ltool_unavailable: .asciz "tool unavailable or denied"
.Ltool_limit: .asciz "tool output limit exceeded"
.Lerror_state: .asciz "invalid runtime state"
.Lerror_turns_text: .asciz "model turn limit reached"
.Lerror_start_text: .asciz "model request initialization failed"
.Lerror_capacity_text: .asciz "session message capacity exhausted"
.Lerror_stream_text: .asciz "model transport or protocol failed"
.Lerror_model_text: .asciz "model reported an error"
.Lerror_text_text: .asciz "assistant text limit exceeded"
.Lerror_event_text: .asciz "invalid model event or tool identifier"
.Lerror_result_capacity_text: .asciz "session has no room for tool result"
.Lerror_arguments_text: .asciz "tool arguments or session capacity exceeded"
.section .note.GNU-stack,"",@progbits
