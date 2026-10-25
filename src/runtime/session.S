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
    PG_NATIVE_FRAME_ALLOC PG_SESSION_STACK
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
    cmp qword ptr [r12+PG_SC_SUB_COUNT],64
    ja .Lreject
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
    je .Lboundary_check
    cmp eax,4
    jne .Lboundary_checked
.Lboundary_check:
    cmp qword ptr [r13+40],0
    jne .Lreject
.Lboundary_checked:
    cmp eax,5
    je .Lsubmission
    cmp eax,0
    je .Lappend
    cmp eax,1
    je .Lsystem
    cmp eax,4
    je .Lprofile
    cmp eax,3
    ja .Lreject
    jmp .Lcheckpoint
.Lsubmission:
    cmp qword ptr [r13+16],1
    jne .Lreject
    cmp qword ptr [r13+40],0
    je .Lreject
    mov rcx,[r13+32]
    test rcx,rcx
    jz .Lreject
    cmp rcx,63
    ja .Lreject
    mov rsi,[r13+24]
    test rsi,rsi
    jz .Lreject
    mov [rsp+32],rcx
    lea rdi,[rsp+PG_SESSION_REQUEST]
    mov [rsp+24],rdi
.Lrequest_copy:
    lodsb
    test al,al
    jz .Lreject
    stosb
    dec rcx
    jnz .Lrequest_copy
    mov byte ptr [rdi],0
    jmp .Lappend
.Lprofile:
    cmp qword ptr [r13+32],0
    je .Lreject
    cmp qword ptr [r13+32],4096
    ja .Lreject
    mov rsi,[r13+24]
    test rsi,rsi
    jz .Lreject
    mov rcx,[r13+32]
    mov [rsp+32],rcx
    lea rdi,[rsp+PG_SESSION_DATA]
    mov [rsp+24],rdi
    rep movsb
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
    cmp r14,PG_SESSION_APPEND_MAX
    ja .Lreject
    cmp r14,2
    jbe .Lappend_bounded
    cmp qword ptr [r13+40],0
    je .Lreject
.Lappend_bounded:
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
    cmp dword ptr [r15+PG_MSG_ERROR],1
    ja .Lreject
    cmp qword ptr [r12+PG_SC_RUN],0
    je .Lmessage_fields
    mov edx,[r12+PG_SC_RUN+PG_CP_PHASE]
    cmp edx,2
    je .Lmessage_tool_result
    cmp edx,5
    je .Lmessage_tool_result
    cmp edx,4
    jne .Lmessage_fields
    cmp qword ptr [r12+PG_SC_RUN+PG_CP_CALL],-1
    je .Lmessage_fields
.Lmessage_tool_result:
    cmp dword ptr [r15],3
    jne .Lreject
.Lmessage_fields:
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
    cmp dword ptr [r15+PG_CP_PHASE],5
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
    cmp dword ptr [rsp],5
    je .Lcheckpoint_added
    cmp dword ptr [rsp],0
    jne .Lcheckpoint_count
.Lcheckpoint_added:
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
    cmp dword ptr [r15+PG_CP_PHASE],5
    je .Lcheckpoint_call
    cmp rax,-1
    jne .Lcheckpoint_has_call
    lea rax,[r14-1]
    call .Lmessage_at
    cmp dword ptr [rax],2
    je .Lreject
    cmp qword ptr [r12+PG_SC_RUN],0
    je .Lcheckpoint_identity
    mov eax,[r12+PG_SC_RUN+PG_CP_PHASE]
    cmp eax,2
    je .Lround_complete
    cmp eax,5
    je .Lround_complete
    cmp eax,4
    jne .Lcheckpoint_identity
.Lround_complete:
    mov rbp,[r12+PG_SC_RUN+PG_CP_CALL]
    cmp rbp,-1
    je .Lcheckpoint_identity
    cmp rbp,r14
    jae .Lreject
    mov rax,rbp
    call .Lmessage_at
    cmp dword ptr [rax],2
    jne .Lreject
    mov esi,1
    jmp .Lround_begin
.Lcheckpoint_has_call:
    cmp dword ptr [r15+PG_CP_PHASE],4
    jne .Lreject
.Lcheckpoint_call:
    cmp dword ptr [r15+PG_CP_TURNS],0
    je .Lreject
    cmp rax,r14
    jae .Lreject
    mov rbp,rax
    call .Lmessage_at
    cmp dword ptr [rax],2
    jne .Lreject
    lea rax,[rbp+1]
    cmp rax,r14
    jne .Lround_pending
    lea rax,[rbp-1]
    call .Lmessage_at
    cmp dword ptr [rax],2
    jne .Lcheckpoint_identity
.Lround_pending:
    # A sequential round has contiguous calls followed by completed results.
    xor esi,esi
.Lround_begin:
    mov r8,rbp
.Lround_start:
    test r8,r8
    jz .Lreject
    lea rax,[r8-1]
    call .Lmessage_at
    cmp dword ptr [rax],2
    jne .Lround_assistant
    dec r8
    mov rax,rbp
    sub rax,r8
    cmp rax,8
    jae .Lreject
    jmp .Lround_start
.Lround_assistant:
    xor ecx,ecx
    cmp dword ptr [rax],1
    sete cl
    mov r9,r8
.Lround_end:
    cmp r9,r14
    jae .Lround_count
    mov rax,r9
    call .Lmessage_at
    cmp dword ptr [rax],2
    jne .Lround_count
    inc r9
    mov rax,r9
    sub rax,r8
    cmp rax,8
    ja .Lreject
    jmp .Lround_end
.Lround_count:
    mov rax,r9
    sub rax,r8
    cmp rax,1
    jbe .Lround_shape
    test ecx,ecx
    jz .Lreject
.Lround_shape:
    mov r10,r14
    sub r10,r9
    test esi,esi
    jnz .Lround_expected
    mov rax,rbp
    sub rax,r8
.Lround_expected:
    cmp r10,rax
    jne .Lreject
    mov rbp,r8
.Lround_unique_outer:
    inc rbp
    cmp rbp,r9
    jae .Lround_results_begin
    mov r11,r8
.Lround_unique_inner:
    cmp r11,rbp
    jae .Lround_unique_outer
    mov rax,rbp
    call .Lmessage_at
    lea rdi,[rax+PG_MSG_ID]
    mov rax,r11
    call .Lmessage_at
    lea rsi,[rax+PG_MSG_ID]
    xor edx,edx
.Lround_unique_id:
    mov cl,[rdi+rdx]
    cmp cl,[rsi+rdx]
    jne .Lround_unique_next
    test cl,cl
    jz .Lreject
    inc edx
    cmp edx,64
    jb .Lround_unique_id
    jmp .Lreject
.Lround_unique_next:
    inc r11
    jmp .Lround_unique_inner
.Lround_results_begin:
    xor r11d,r11d
.Lround_results:
    cmp r11,r10
    jae .Lcheckpoint_identity
    lea rax,[r8+r11]
    call .Lmessage_at
    lea rdi,[rax+PG_MSG_ID]
    lea rax,[r9+r11]
    call .Lmessage_at
    cmp dword ptr [rax],3
    jne .Lreject
    lea rsi,[rax+PG_MSG_ID]
    xor edx,edx
.Lround_id:
    mov cl,[rdi+rdx]
    cmp cl,[rsi+rdx]
    jne .Lreject
    test cl,cl
    jz .Lround_result_next
    inc edx
    cmp edx,64
    jb .Lround_id
    jmp .Lreject
.Lround_result_next:
    inc r11
    jmp .Lround_results
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
    mov rax,[r12+PG_SC_SUB_COUNT]
    test rax,rax
    jz .Lnew_kind
    dec rax
    imul rax,PG_SUB_BYTES
    mov rax,[r12+PG_SC_SUBS+rax]
    cmp [r15],rax
    jbe .Lreject
.Lnew_kind:
    cmp dword ptr [rsp],5
    je .Lnew_submission
    cmp dword ptr [rsp],0
    jne .Lreject
    jmp .Lnew_checkpoint
.Lnew_submission:
    cmp qword ptr [r12+PG_SC_SUB_COUNT],64
    jae .Lreject
    mov rax,[r12+PG_SC_SEQ]
    inc rax
    cmp [r15],rax
    jne .Lreject
    mov rax,[rsp+8]
    cmp dword ptr [rax],0
    jne .Lreject
    cmp dword ptr [rax+PG_MSG_ERROR],0
    jne .Lreject
    cmp byte ptr [rax+PG_MSG_ID],0
    jne .Lreject
    cmp byte ptr [rax+PG_MSG_NAME],0
    jne .Lreject
    mov rdi,r12
    mov rsi,[rsp+24]
    mov rdx,[rsp+32]
    call pg_submission_find
    test rax,rax
    jnz .Lreject
.Lnew_checkpoint:
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
    cmp dword ptr [rsp],5
    je .Lreject
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
    cmp dword ptr [rsp],5
    je .Lreject
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
    cmp dword ptr [rsp],5
    je .Ladopt_append
    cmp dword ptr [rsp],0
    je .Ladopt_append
    cmp dword ptr [rsp],1
    je .Ladopt_system
    cmp dword ptr [rsp],3
    je .Ladopted
    cmp dword ptr [rsp],4
    je .Ladopt_profile
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
.Ladopt_profile:
    lea rdi,[r12+PG_SC_PROFILE]
    xor eax,eax
    mov ecx,512
    rep stosq
    mov rsi,[rsp+24]
    lea rdi,[r12+PG_SC_PROFILE]
    mov rcx,[rsp+32]
    mov [r12+PG_SC_PROFILE_SIZE],rcx
    rep movsb
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
    cmp dword ptr [rsp],5
    jne .Ladopted
    imul rdi,[r12+PG_SC_SUB_COUNT],PG_SUB_BYTES
    lea rdi,[r12+PG_SC_SUBS+rdi]
    mov r14,rdi
    xor eax,eax
    mov ecx,PG_SUB_BYTES/8
    rep stosq
    mov rax,[rsp+40]
    mov rax,[rax]
    mov [r14],rax
    lea rdi,[r14+PG_SUB_KEY]
    mov rsi,[rsp+24]
    mov rcx,[rsp+32]
    rep movsb
    inc qword ptr [r12+PG_SC_SUB_COUNT]
.Ladopted:
    mov rsi,[rsp+40]
    test rsi,rsi
    jz .Ladopt_sequence
    lea rdi,[r12+PG_SC_RUN]
    mov ecx,5
    rep movsq
    mov rcx,[r12+PG_SC_SUB_COUNT]
    lea rdi,[r12+PG_SC_SUBS]
    mov rax,[r12+PG_SC_RUN]
.Lupdate_submission:
    test rcx,rcx
    jz .Ladopt_sequence
    cmp [rdi],rax
    je .Lupdate_found
    add rdi,PG_SUB_BYTES
    dec rcx
    jmp .Lupdate_submission
.Lupdate_found:
    xor eax,eax
    cmp dword ptr [r12+PG_SC_RUN+PG_CP_PHASE],3
    sete al
    mov [rdi+8],eax
    mov eax,[r12+PG_SC_RUN+PG_CP_OUTCOME]
    mov [rdi+12],eax
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
