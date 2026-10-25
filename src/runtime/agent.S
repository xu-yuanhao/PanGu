#include "agent_layout.h"
.intel_syntax noprefix
.text
.macro PROFILE_GUARD owner,bad
    cmp qword ptr [\owner+PG_A_COMMITS+PG_SC_PROFILE_SIZE],0
    je 1f
    cmp dword ptr [\owner+PG_A_VERIFIED],1
    jne \bad
1:
.endm

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
    mov rax,[rsp+80]
    or rax,[rsp+88]
    or rax,[rsp+96]
    jz .Linit_tools_valid
    cmp qword ptr [rsp+80],0
    je .Linit_bad
    cmp qword ptr [rsp+88],0
    je .Linit_bad
    cmp qword ptr [rsp+96],0
    je .Linit_bad
.Linit_tools_valid:
    mov r8,rdi
    xor eax,eax
    mov ecx,PG_A_BYTES/8
    rep stosq
    lea rdi,[r8+PG_A_MODEL]
    lea rsi,[rsp+8]
    mov ecx,12
    rep movsq
    lea rax,[rip+pg_session_volatile_commit]
    mov [r8+PG_A_COMMITS+8],rax
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

.globl pg_agent_submissions
.type pg_agent_submissions,@function
pg_agent_submissions:
    mov [rdi],rsi
    lea rax,[rip+.Lsubmission_submit]
    mov [rdi+8],rax
    lea rax,[rip+.Lsubmission_find]
    mov [rdi+16],rax
    mov rax,rdi
    ret
.size pg_agent_submissions,.-pg_agent_submissions

.Lrequest_valid:
    test rsi,rsi
    jz .Lrequest_bad
    test rdx,rdx
    jz .Lrequest_bad
    cmp rdx,63
    ja .Lrequest_bad
    xor r10d,r10d
.Lrequest_byte:
    cmp byte ptr [rsi+r10],0
    je .Lrequest_bad
    inc r10
    cmp r10,rdx
    jb .Lrequest_byte
    xor eax,eax
    ret
.Lrequest_bad:
    mov eax,2
    ret

.Lsubmission_find:
    test rdi,rdi
    jz .Lrequest_bad
    test rcx,rcx
    jz .Lrequest_bad
    push rbx
    mov rbx,rcx
    call .Lrequest_valid
    test eax,eax
    jnz .Lsubmission_find_return
    add rdi,PG_A_COMMITS
    call pg_submission_find
    test rax,rax
    jz .Lsubmission_missing
    mov rsi,rax
    mov rdi,rbx
    mov ecx,PG_SUB_BYTES/8
    rep movsq
    xor eax,eax
    jmp .Lsubmission_find_return
.Lsubmission_missing:
    mov eax,1
.Lsubmission_find_return:
    pop rbx
    ret

.Lsubmission_submit:
    test rdi,rdi
    jz .Lrequest_bad
    test r9,r9
    jz .Lrequest_bad
    PROFILE_GUARD rdi,.Lrequest_bad
    cmp qword ptr [rdi+PG_A_COMMITS+PG_SC_BUSY],0
    jne .Lrequest_bad
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp,16
    mov rbx,rdi
    mov r12,rcx
    mov r13,r8
    mov r14,r9
    mov r15,rsi
    mov [rsp],rdx
    call .Lrequest_valid
    test eax,eax
    jnz .Lsubmission_submit_return
    lea rdi,[rbx+PG_A_COMMITS]
    call pg_submission_find
    test rax,rax
    jnz .Lsubmission_result
    mov rdi,rbx
    mov rsi,r12
    mov rdx,r13
    mov rcx,r15
    mov r8,[rsp]
    call .Ladmit
    test eax,eax
    jnz .Lsubmission_submit_return
    mov rax,[rbx+PG_A_COMMITS+PG_SC_SUB_COUNT]
    test rax,rax
    jz .Lsubmission_internal_error
    dec rax
    imul rax,PG_SUB_BYTES
    lea rax,[rbx+PG_A_COMMITS+PG_SC_SUBS+rax]
.Lsubmission_result:
    mov rdi,r14
    mov rsi,rax
    mov ecx,PG_SUB_BYTES/8
    rep movsq
    xor eax,eax
    jmp .Lsubmission_submit_return
.Lsubmission_internal_error:
    mov eax,2
.Lsubmission_submit_return:
    add rsp,16
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret

.globl pg_agent_bind_storage
.type pg_agent_bind_storage,@function
pg_agent_bind_storage:
    test rdi,rdi
    jz .Lbind_bad
    test rdx,rdx
    jz .Lbind_bad
    cmp dword ptr [rdi+PG_A_STATE],PG_ST_IDLE
    jne .Lbind_bad
    cmp qword ptr [rdi+PG_A_COMMITS+16],0
    jne .Lbind_bad
    cmp qword ptr [rdi+PG_A_COUNT],0
    jne .Lbind_bad
    cmp qword ptr [rdi+PG_A_SYSTEM_SIZE],0
    jne .Lbind_bad
    cmp qword ptr [rdi+PG_A_COMMITS+24],0
    jne .Lbind_bad
    mov [rdi+PG_A_COMMITS],rsi
    mov [rdi+PG_A_COMMITS+8],rdx
    xor eax,eax
    ret
.Lbind_bad:
    mov eax,1
    ret
.size pg_agent_bind_storage,.-pg_agent_bind_storage

.globl pg_agent_bind_profile
.type pg_agent_bind_profile,@function
pg_agent_bind_profile:
    test rdi,rdi
    jz .Lprofile_bad
    cmp dword ptr [rdi+PG_A_STATE],0
    jne .Lprofile_bad
    cmp qword ptr [rdi+PG_A_COMMITS+PG_SC_BUSY],0
    jne .Lprofile_bad
    mov dword ptr [rdi+PG_A_VERIFIED],0
    test rsi,rsi
    jz .Lprofile_bad
    test rdx,rdx
    jz .Lprofile_bad
    cmp rdx,4096
    ja .Lprofile_bad
    mov r8,rdi
    mov r9,rsi
    mov r10,rdx
    cmp [rdi+PG_A_COMMITS+PG_SC_PROFILE_SIZE],rdx
    jne .Lprofile_replace
    mov rcx,rdx
    lea rdi,[rdi+PG_A_COMMITS+PG_SC_PROFILE]
    repe cmpsb
    jne .Lprofile_replace
    mov dword ptr [r8+PG_A_VERIFIED],1
    xor eax,eax
    ret
.Lprofile_replace:
    push rbx
    sub rsp,48
    mov rbx,r8
    mov dword ptr [rsp],4
    mov [rsp+24],r9
    mov [rsp+32],r10
    mov qword ptr [rsp+40],0
    mov rdi,rbx
    lea rsi,[rbx+PG_A_COMMITS]
    mov rdx,rsp
    call pg_session_commit
    test eax,eax
    jnz .Lprofile_return
    mov dword ptr [rbx+PG_A_VERIFIED],1
.Lprofile_return:
    add rsp,48
    pop rbx
    ret
.Lprofile_bad:
    mov eax,1
    ret
.size pg_agent_bind_profile,.-pg_agent_bind_profile

.globl pg_agent_set_system
.type pg_agent_set_system,@function
pg_agent_set_system:
    test rdi,rdi
    jz .Lsystem_bad
    PROFILE_GUARD rdi,.Lsystem_bad
    cmp dword ptr [rdi+PG_A_STATE],PG_ST_IDLE
    jne .Lsystem_bad
    sub rsp,56
    mov dword ptr [rsp],1
    mov qword ptr [rsp+40],0
    mov [rsp+24],rsi
    mov [rsp+32],rdx
    mov rdx,rsp
    lea rsi,[rdi+PG_A_COMMITS]
    call pg_session_commit
    add rsp,56
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
    PROFILE_GUARD rdi,.Lreset_bad
    cmp dword ptr [rdi+PG_A_STATE],PG_ST_IDLE
    jne .Lreset_bad
    sub rsp,56
    mov dword ptr [rsp],2
    mov qword ptr [rsp+40],0
    mov rdx,rsp
    lea rsi,[rdi+PG_A_COMMITS]
    call pg_session_commit
    add rsp,56
    ret
.Lreset_bad:
    mov eax,1
    ret
.size pg_agent_reset_session,.-pg_agent_reset_session

# Commit one detached batch, returning its last adopted message or NULL.
.Lcommit_records:
    push rbx
    sub rsp,48
    mov rbx,rdi
    mov dword ptr [rsp],0
    mov [rsp+8],rsi
    mov [rsp+16],rdx
    lea rax,[rdi+PG_A_NEXT]
    mov [rsp+40],rax
    mov rdx,rsp
    lea rsi,[rdi+PG_A_COMMITS]
    call pg_session_commit
    test eax,eax
    jnz .Lrecords_bad
    mov rax,[rbx+PG_A_COUNT]
    dec rax
    imul rax,PG_MSG_BYTES
    add rax,rbx
    jmp .Lrecords_end
.Lrecords_bad:
    xor eax,eax
.Lrecords_end:
    add rsp,48
    pop rbx
    ret

# next(owner, phase, outcome, call_index)
.Lnext_checkpoint:
    mov r8,rdi
    mov r9d,esi
    mov r10d,edx
    mov r11,rcx
    lea rsi,[r8+PG_A_COMMITS+PG_SC_RUN]
    lea rdi,[r8+PG_A_NEXT]
    mov ecx,5
    rep movsq
    cmp [r8+PG_A_NEXT+PG_CP_PHASE],r9d
    je .Lnext_same_phase
    mov dword ptr [r8+PG_A_NEXT+PG_CP_RESTARTS],0
.Lnext_same_phase:
    mov [r8+PG_A_NEXT+PG_CP_PHASE],r9d
    mov [r8+PG_A_NEXT+PG_CP_OUTCOME],r10d
    mov [r8+PG_A_NEXT+PG_CP_CALL],r11
    mov eax,[r8+PG_A_TURNS]
    mov [r8+PG_A_NEXT+PG_CP_TURNS],eax
    ret

.Lcommit_checkpoint:
    sub rsp,56
    mov dword ptr [rsp],3
    lea rax,[rdi+PG_A_NEXT]
    mov [rsp+40],rax
    mov rdx,rsp
    lea rsi,[rdi+PG_A_COMMITS]
    call pg_session_commit
    add rsp,56
    ret

.globl pg_agent_resume
.type pg_agent_resume,@function
pg_agent_resume:
    test rdi,rdi
    jz .Lresume_bad
    PROFILE_GUARD rdi,.Lresume_bad
    cmp esi,1
    ja .Lresume_bad
    cmp dword ptr [rdi+PG_A_STATE],0
    jne .Lresume_bad
    cmp dword ptr [rdi+PG_A_OPEN],0
    jne .Lresume_bad
    cmp qword ptr [rdi+PG_A_COMMITS+PG_SC_BUSY],0
    jne .Lresume_bad
    cmp qword ptr [rdi+PG_A_COMMITS+PG_SC_RUN],0
    je .Lresume_none
    mov eax,[rdi+PG_A_COMMITS+PG_SC_RUN+PG_CP_PHASE]
    cmp eax,3
    je .Lresume_none
    test esi,esi
    jnz .Lresume_allowed
    cmp dword ptr [rdi+PG_A_COMMITS+PG_SC_RUN+PG_CP_RESTARTS],5
    jae .Lresume_parked
.Lresume_allowed:
    push rbx
    push r12
    sub rsp,8
    mov rbx,rdi
    mov r12d,esi
    mov ecx,[rbx+PG_A_COMMITS+PG_SC_RUN+PG_CP_TURNS]
    mov [rbx+PG_A_TURNS],ecx
    mov esi,eax
    xor edx,edx
    mov rcx,[rbx+PG_A_COMMITS+PG_SC_RUN+PG_CP_CALL]
    call .Lnext_checkpoint
    xor eax,eax
    test r12d,r12d
    jnz .Lresume_counter
    mov eax,[rbx+PG_A_NEXT+PG_CP_RESTARTS]
.Lresume_counter:
    inc eax
    mov [rbx+PG_A_NEXT+PG_CP_RESTARTS],eax
    mov rdi,rbx
    call .Lcommit_checkpoint
    test eax,eax
    jnz .Lresume_pop_bad
    mov rax,[rbx+PG_A_NEXT+PG_CP_CALL]
    mov [rbx+PG_A_CALL],rax
    mov dword ptr [rbx+PG_A_GROUP_CANCELLED],0
    mov qword ptr [rbx+PG_A_GROUP_END],0
    cmp rax,-1
    je .Lresume_group_done
.Lresume_group_scan:
    cmp rax,[rbx+PG_A_COUNT]
    jae .Lresume_group_done
    imul rcx,rax,PG_MSG_BYTES
    cmp dword ptr [rbx+rcx],PG_ROLE_CALL
    jne .Lresume_group_done
    inc rax
    mov [rbx+PG_A_GROUP_END],rax
    jmp .Lresume_group_scan
.Lresume_group_done:
    mov dword ptr [rbx+PG_A_CANCEL],0
    mov byte ptr [rbx+PG_A_ERROR],0
    mov dword ptr [rbx+PG_A_STATE],PG_ST_BEGIN
    xor eax,eax
    jmp .Lresume_pop
.Lresume_pop_bad:
    mov eax,1
.Lresume_pop:
    add rsp,8
    pop r12
    pop rbx
    ret
.Lresume_parked:
    mov eax,2
    ret
.Lresume_bad:
    mov eax,1
    ret
.Lresume_none:
    xor eax,eax
    ret
.size pg_agent_resume,.-pg_agent_resume

.globl pg_agent_suspend
.type pg_agent_suspend,@function
pg_agent_suspend:
    test rdi,rdi
    jz .Lsuspend_bad
    cmp dword ptr [rdi+PG_A_COMMITS+PG_SC_BUSY],0
    jne .Lsuspend_bad
    push rbx
    mov rbx,rdi
    cmp dword ptr [rbx+PG_A_TOOL_OPEN],0
    je .Lsuspend_release
    mov dword ptr [rbx+PG_A_STATE],PG_ST_SUSPENDING
    mov rdi,[rbx+PG_A_TOOLS]
    call [rbx+PG_A_TOOLS+40]
    mov rdi,[rbx+PG_A_TOOLS]
    call [rbx+PG_A_TOOLS+48]
    test eax,eax
    jz .Lsuspend_tool_closed
    mov edx,1
    cmp eax,1
    jne .Lsuspend_return_pending
    mov edx,2
.Lsuspend_return_pending:
    mov eax,edx
    pop rbx
    ret
.Lsuspend_tool_closed:
    mov dword ptr [rbx+PG_A_TOOL_OPEN],0
.Lsuspend_release:
    mov rdi,rbx
    call .Lclose
    lea rdi,[rbx+PG_A_DRAFT]
    xor eax,eax
    mov ecx,PG_MSG_BYTES/8
    rep stosq
    mov dword ptr [rbx+PG_A_STATE],PG_ST_IDLE
    mov dword ptr [rbx+PG_A_CANCEL],0
    pop rbx
    xor eax,eax
    ret
.Lsuspend_bad:
    mov eax,1
    ret
.size pg_agent_suspend,.-pg_agent_suspend

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

# prepare(owner, role, bytes, length); adopted only by .Lcommit_records.
.Lprepare:
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
    lea rdx,[rdi+PG_A_PENDING]
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
    cmp dword ptr [rbx+PG_A_TOOL_OPEN],0
    je .Lfinish_released
    mov rdi,[rbx+PG_A_TOOLS]
    call [rbx+PG_A_TOOLS+40]
    mov rdi,[rbx+PG_A_TOOLS]
    call [rbx+PG_A_TOOLS+48]
    test eax,eax
    jz .Lfinish_tool_closed
    mov [rbx+PG_A_OUTCOME],r12d
    mov dword ptr [rbx+PG_A_STATE],PG_ST_FAIL_CLOSE
    jmp .Lfinish_return
.Lfinish_tool_closed:
    mov dword ptr [rbx+PG_A_TOOL_OPEN],0
.Lfinish_released:
    cmp dword ptr [rbx+PG_A_COMMITS+PG_SC_FAILED],0
    jne .Lfinish_close
    cmp qword ptr [rbx+PG_A_COMMITS+PG_SC_RUN],0
    je .Lfinish_close
    cmp dword ptr [rbx+PG_A_COMMITS+PG_SC_RUN+PG_CP_PHASE],3
    je .Lfinish_close
    mov esi,3
    mov edx,r12d
    mov rcx,-1
    call .Lnext_checkpoint
    mov rdi,rbx
    call .Lcommit_checkpoint
    test eax,eax
    jz .Lfinish_close
    mov dword ptr [rbx+PG_A_COMMITS+PG_SC_FAILED],1
    mov r12d,1
    lea rdi,[rbx+PG_A_ERROR]
    lea rsi,[rip+.Lerror_storage_text]
    mov edx,46
    call .Lcopy
.Lfinish_close:
    mov rdi,rbx
    call .Lclose
    lea rdi,[rbx+PG_A_DRAFT]
    xor eax,eax
    mov ecx,PG_MSG_BYTES/8
    rep stosq
    mov [rbx+PG_A_OUTCOME],r12d
    mov dword ptr [rbx+PG_A_STATE],PG_ST_FINISH
.Lfinish_return:
    add rsp,8
    pop r12
    pop rbx
    ret

.Lstorage_fail:
    mov dword ptr [rdi+PG_A_COMMITS+PG_SC_FAILED],1
    lea rsi,[rip+.Lerror_storage_text]
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
    xor ecx,ecx
    xor r8d,r8d
.Ladmit:
    test rdi,rdi
    jz .Lsubmit_bad
    PROFILE_GUARD rdi,.Lsubmit_bad
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
    sub rsp,64
    mov rbx,rdi
    mov [rsp+48],rcx
    mov [rsp+56],r8
    mov rcx,rdx
    mov rdx,rsi
    xor esi,esi
    call .Lprepare
    test rax,rax
    jz .Lsubmit_pop_bad
    mov rax,[rbx+PG_A_COMMITS+PG_SC_SEQ]
    inc rax
    mov [rbx+PG_A_NEXT],rax
    mov dword ptr [rbx+PG_A_NEXT+PG_CP_PHASE],1
    mov dword ptr [rbx+PG_A_NEXT+PG_CP_TURNS],0
    mov qword ptr [rbx+PG_A_NEXT+PG_CP_CALL],-1
    mov qword ptr [rbx+PG_A_NEXT+PG_CP_OUTCOME],0
    mov rax,[rbx+PG_A_COUNT]
    mov [rbx+PG_A_NEXT+PG_CP_INPUT],rax
    mov dword ptr [rsp],0
    cmp qword ptr [rsp+56],0
    je .Ladmit_kind
    mov dword ptr [rsp],5
.Ladmit_kind:
    lea rax,[rbx+PG_A_PENDING]
    mov [rsp+8],rax
    mov qword ptr [rsp+16],1
    mov rax,[rsp+48]
    mov [rsp+24],rax
    mov rax,[rsp+56]
    mov [rsp+32],rax
    lea rax,[rbx+PG_A_NEXT]
    mov [rsp+40],rax
    mov rdi,rbx
    lea rsi,[rbx+PG_A_COMMITS]
    mov rdx,rsp
    call pg_session_commit
    test eax,eax
    jnz .Lsubmit_pop_bad
    mov dword ptr [rbx+PG_A_CANCEL],0
    mov dword ptr [rbx+PG_A_TURNS],0
    mov byte ptr [rbx+PG_A_ERROR],0
    mov dword ptr [rbx+PG_A_STATE],PG_ST_BEGIN
    add rsp,64
    pop rbx
    xor eax,eax
    ret
.Lsubmit_pop_bad:
    add rsp,64
    pop rbx
.Lsubmit_bad:
    mov eax,1
    ret

# rbx owns the run. Only settled text enters model-visible history.
.Lsettle_draft:
    sub rsp,8
    mov rax,[rbx+PG_A_COUNT]
    mov [rbx+PG_A_ASSISTANT],rax
    mov rdi,rbx
    mov esi,3
    xor edx,edx
    mov rcx,-1
    call .Lnext_checkpoint
    mov rdi,rbx
    lea rsi,[rbx+PG_A_DRAFT]
    mov edx,1
    add rsp,8
    jmp .Lcommit_records

.Lcancel:
    mov eax,[rdi+PG_A_STATE]
    test eax,eax
    jz .Lcancel_end
    cmp eax,6
    je .Lcancel_end
    cmp dword ptr [rdi+PG_A_CANCEL],0
    jne .Lcancel_end
    cmp dword ptr [rdi+PG_A_COMMITS+PG_SC_RUN+PG_CP_PHASE],3
    je .Lcancel_end
    push rbx
    mov rbx,rdi
    mov esi,4
    xor edx,edx
    mov rcx,[rbx+PG_A_COMMITS+PG_SC_RUN+PG_CP_CALL]
    call .Lnext_checkpoint
    mov rdi,rbx
    call .Lcommit_checkpoint
    test eax,eax
    jnz .Lcancel_store_failed
    mov dword ptr [rbx+PG_A_CANCEL],1
    pop rbx
    ret
.Lcancel_store_failed:
    mov rdi,rbx
    call .Lstorage_fail
    pop rbx
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

/* rdi=agent, rsi/rdx=name. NULL metadata defaults to unsafe. */
.Lsafe_to_replay:
    mov rax,[rdi+PG_A_TOOLS+24]
    test rax,rax
    jz .Lpolicy_unsafe
    mov rdi,[rdi+PG_A_TOOLS]
    jmp rax
.Lpolicy_unsafe:
    xor eax,eax
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
    cmp eax,PG_ST_SUSPENDING
    je .Lpoll_zero
    cmp eax,PG_ST_FAIL_CLOSE
    je .Ldispatch
    cmp dword ptr [rbx+PG_A_CANCEL],0
    je .Ldispatch
    cmp eax,PG_ST_TOOL_END
    je .Ldispatch
    cmp eax,PG_ST_NEXT_TOOL
    je .Lcancel_next
    cmp eax,PG_ST_TOOL_WAIT
    je .Lcancel_async
    cmp eax,PG_ST_TOOL_CLOSE
    je .Lcancel_async
    cmp eax,1
    je .Ldispatch
    cmp eax,6
    je .Ldispatch
    cmp eax,7
    je .Ldispatch
    cmp dword ptr [rbx+PG_A_OPEN],0
    je .Lcancel_operation
    mov rdi,[rbx+PG_A_MODEL]
    call [rbx+PG_A_MODEL+24]
    jmp .Lcancel_operation
.Lcancel_next:
    mov rax,[rbx+PG_A_COMMITS+PG_SC_RUN+PG_CP_CALL]
    mov [rbx+PG_A_CALL],rax
    jmp .Lcancel_tool
.Lcancel_async:
    mov rdi,[rbx+PG_A_TOOLS]
    call [rbx+PG_A_TOOLS+40]
    jmp .Ldispatch
.Lcancel_operation:
    cmp dword ptr [rbx+PG_A_STATE],PG_ST_TOOL
    je .Lcancel_tool
    cmp dword ptr [rbx+PG_A_STATE],PG_ST_RECOVER_EXECUTE
    jne .Lcancel_finish
.Lcancel_tool:
    imul r13,[rbx+PG_A_CALL],PG_MSG_BYTES
    add r13,rbx
    lea rdi,[rbx+PG_A_TEXT]
    lea rsi,[rip+.Lcancelled]
    mov edx,9
    call .Lcopy
    mov qword ptr [rsp+56],9
    mov r15d,1
    jmp .Ltool_append
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
    cmp eax,PG_ST_TOOL_WAIT
    je .Ltool_wait
    cmp eax,PG_ST_TOOL_CLOSE
    je .Ltool_close
    cmp eax,PG_ST_FAIL_CLOSE
    je .Lfinish_wait
    cmp eax,PG_ST_FINISH
    je .Lterminal
    cmp eax,PG_ST_NEXT_TOOL
    je .Lnext_tool
    cmp eax,PG_ST_RECOVER_TOOL
    je .Lrecover_tool
    cmp eax,PG_ST_RECOVER_EXECUTE
    je .Lrecover_execute
    lea rsi,[rip+.Lerror_state]
    jmp .Lpoll_fail
.Lbegin:
    mov dword ptr [rbx+PG_A_STATE],PG_ST_START
    cmp dword ptr [rbx+PG_A_COMMITS+PG_SC_RUN+PG_CP_PHASE],2
    je .Lbegin_tool
    cmp dword ptr [rbx+PG_A_COMMITS+PG_SC_RUN+PG_CP_PHASE],5
    je .Lbegin_tool
    cmp dword ptr [rbx+PG_A_COMMITS+PG_SC_RUN+PG_CP_PHASE],4
    jne .Lpoll_one
    cmp qword ptr [rbx+PG_A_COMMITS+PG_SC_RUN+PG_CP_CALL],-1
    jne .Lbegin_tool
    mov dword ptr [rbx+PG_A_CANCEL],1
    jmp .Lpoll_one
.Lbegin_tool:
    mov dword ptr [rbx+PG_A_STATE],PG_ST_RECOVER_TOOL
    jmp .Lpoll_one
.Lnext_tool:
    mov rax,[rbx+PG_A_COMMITS+PG_SC_RUN+PG_CP_CALL]
    mov [rbx+PG_A_CALL],rax
    mov dword ptr [rbx+PG_A_STATE],PG_ST_TOOL
    jmp .Ltool_begin
.Lrecover_tool:
    mov dword ptr [rbx+PG_A_STATE],PG_ST_RECOVER_EXECUTE
.Ltool_begin:
    imul r13,[rbx+PG_A_CALL],PG_MSG_BYTES
    add r13,rbx
    mov dword ptr [r12],PG_EV_TOOL_BEGIN
    lea rdi,[r13+PG_MSG_ID]
    mov [r12+24],rdi
    call .Llength
    mov [r12+32],rax
    lea rdi,[r13+PG_MSG_NAME]
    mov [r12+40],rdi
    call .Llength
    mov [r12+48],rax
    cmp dword ptr [rbx+PG_A_COMMITS+PG_SC_RUN+PG_CP_PHASE],4
    jne .Lpoll_one
    mov dword ptr [rbx+PG_A_STATE],PG_ST_TOOL
    mov dword ptr [rbx+PG_A_CANCEL],1
    jmp .Lpoll_one
.Lrecover_execute:
    imul r13,[rbx+PG_A_CALL],PG_MSG_BYTES
    add r13,rbx
    cmp dword ptr [rbx+PG_A_COMMITS+PG_SC_RUN+PG_CP_PHASE],2
    jne .Ltool_interrupted
    lea rdi,[r13+PG_MSG_NAME]
    call .Llength
    mov rdx,rax
    lea rsi,[r13+PG_MSG_NAME]
    mov rdi,rbx
    call .Lsafe_to_replay
    cmp eax,1
    jne .Ltool_interrupted
    mov dword ptr [rbx+PG_A_STATE],PG_ST_TOOL
    jmp .Ltool
.Ltool_interrupted:
    lea rdi,[rip+.Linterrupted]
    call .Llength
    mov [rsp+56],rax
    mov rdx,rax
    lea rsi,[rip+.Linterrupted]
    lea rdi,[rbx+PG_A_TEXT]
    call .Lcopy
    mov r15d,1
    jmp .Ltool_append
.Lstart:
    PROFILE_GUARD rbx,.Lerror_profile
    cmp qword ptr [rbx+PG_A_COUNT],64
    jae .Lerror_capacity
    cmp dword ptr [rbx+PG_A_TURNS],16
    jae .Lerror_turns
    inc dword ptr [rbx+PG_A_TURNS]
    mov rdi,rbx
    mov esi,1
    xor edx,edx
    mov rcx,-1
    call .Lnext_checkpoint
    mov rdi,rbx
    call .Lcommit_checkpoint
    test eax,eax
    jnz .Lerror_storage
    mov dword ptr [rbx+PG_A_OPEN],1
    mov rdi,[rbx+PG_A_MODEL]
    mov rsi,rbx
    call [rbx+PG_A_MODEL+8]
    test eax,eax
    jnz .Lerror_start
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
    cmp eax,PG_ME_BATCH
    je .Lbatch
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
    jz .Lerror_storage
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
.Lbatch:
    mov rax,[rsp+16]
    cmp rax,2
    jb .Lerror_event
    cmp rax,8
    ja .Lerror_event
    cmp qword ptr [rsp+8],0
    je .Lerror_event
    mov [rsp+72],rax
    mov rdx,[rsp+8]
    mov [rsp+64],rdx
    lea rax,[rax+rax+2]
    add rax,[rbx+PG_A_COUNT]
    cmp rax,64
    ja .Lerror_result_capacity
    mov qword ptr [rsp+80],0
.Lbatch_copy:
    mov rax,[rsp+80]
    imul rax,48
    add rax,[rsp+64]
    mov rbp,rax
    mov rdi,[rbp+16]
    mov rsi,[rbp+24]
    call .Lidentifier
    test eax,eax
    jz .Lerror_event
    mov rdi,[rbp+32]
    mov rsi,[rbp+40]
    call .Lidentifier
    test eax,eax
    jz .Lerror_event
    cmp qword ptr [rbp+8],4096
    jae .Lerror_arguments
    cmp qword ptr [rbp+8],0
    je .Lbatch_slot
    cmp qword ptr [rbp],0
    je .Lerror_arguments
.Lbatch_slot:
    mov rax,[rsp+80]
    inc rax
    imul rax,PG_MSG_BYTES
    lea r13,[rbx+PG_A_DRAFT+rax]
    mov rdi,r13
    xor eax,eax
    mov ecx,PG_MSG_BYTES/8
    rep stosq
    mov dword ptr [r13],PG_ROLE_CALL
    mov rax,[rbp+8]
    mov [r13+PG_MSG_SIZE],rax
    lea rdi,[r13+PG_MSG_TEXT]
    mov rsi,[rbp]
    mov rdx,rax
    call .Lcopy
    lea rdi,[r13+PG_MSG_ID]
    mov rsi,[rbp+16]
    mov rdx,[rbp+24]
    call .Lcopy
    lea rdi,[r13+PG_MSG_NAME]
    mov rsi,[rbp+32]
    mov rdx,[rbp+40]
    call .Lcopy
    inc qword ptr [rsp+80]
    mov rax,[rsp+80]
    cmp rax,[rsp+72]
    jb .Lbatch_copy
    mov rax,[rbx+PG_A_COUNT]
    mov [rbx+PG_A_ASSISTANT],rax
    inc rax
    mov [rbx+PG_A_CALL],rax
    add rax,[rsp+72]
    mov [rbx+PG_A_GROUP_END],rax
    lea r13,[rbx+PG_A_PENDING]
    jmp .Lintent_first
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
    cmp qword ptr [rbx+PG_A_COUNT],60
    ja .Lerror_result_capacity
    cmp qword ptr [rsp+16],4096
    jae .Lerror_arguments
    cmp qword ptr [rsp+16],0
    je .Lcall_valid
    cmp qword ptr [rsp+8],0
    je .Lerror_arguments
.Lcall_valid:
    mov rax,[rbx+PG_A_COUNT]
    mov [rbx+PG_A_ASSISTANT],rax
    inc rax
    mov [rbx+PG_A_CALL],rax
    inc rax
    mov [rbx+PG_A_GROUP_END],rax
    mov rdi,rbx
    mov esi,2
    mov rdx,[rsp+8]
    mov rcx,[rsp+16]
    call .Lprepare
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
.Lintent_first:
    mov dword ptr [rbx+PG_A_GROUP_CANCELLED],0
    mov dword ptr [rbx+PG_A_DRAFT],PG_ROLE_ASSISTANT
    lea rdi,[r13+PG_MSG_NAME]
    call .Llength
    mov rdx,rax
    lea rsi,[r13+PG_MSG_NAME]
    mov rdi,rbx
    call .Lsafe_to_replay
    mov esi,5
    cmp eax,1
    jne .Lintent_policy
    mov esi,2
.Lintent_policy:
    mov rdi,rbx
    xor edx,edx
    mov rcx,[rbx+PG_A_CALL]
    call .Lnext_checkpoint
    mov rdi,rbx
    lea rsi,[rbx+PG_A_DRAFT]
    mov rdx,[rbx+PG_A_GROUP_END]
    sub rdx,[rbx+PG_A_ASSISTANT]
    call .Lcommit_records
    test rax,rax
    jz .Lerror_storage
    mov rdi,rbx
    call .Lclose
    mov dword ptr [rbx+PG_A_STATE],PG_ST_TOOL
    jmp .Ltool_begin
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
    xor eax,eax
    cmp qword ptr [rbx+PG_A_TOOLS+48],0
    setne al
    mov [rbx+PG_A_TOOL_OPEN],eax
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
    jmp .Ltool_received
.Ltool_wait:
    imul r13,[rbx+PG_A_CALL],PG_MSG_BYTES
    add r13,rbx
    mov qword ptr [rsp+56],0
    mov rdi,[rbx+PG_A_TOOLS]
    lea rsi,[rbx+PG_A_TEXT]
    mov edx,4096
    lea rcx,[rsp+56]
    call [rbx+PG_A_TOOLS+32]
.Ltool_received:
    mov r15d,eax
    cmp qword ptr [rsp+56],4096
    jae .Ltool_bad_limit
    cmp eax,4
    jne .Ltool_complete_status
    xor r15d,r15d
    jmp .Ltool_complete
.Ltool_complete_status:
    cmp eax,1
    jbe .Ltool_complete
    cmp eax,3
    ja .Ltool_bad_status
    cmp dword ptr [rbx+PG_A_TOOL_OPEN],0
    je .Ltool_bad_status
    mov dword ptr [rbx+PG_A_STATE],PG_ST_TOOL_WAIT
    cmp eax,3
    je .Ltool_progress
    cmp qword ptr [rsp+56],0
    jne .Ltool_bad_status
    jmp .Lpoll_zero
.Ltool_progress:
    mov dword ptr [r12],PG_EV_TOOL_OUTPUT
    lea rax,[rbx+PG_A_TEXT]
    mov [r12+8],rax
    mov rax,[rsp+56]
    mov [r12+16],rax
    lea rdi,[r13+PG_MSG_ID]
    mov [r12+24],rdi
    call .Llength
    mov [r12+32],rax
    lea rdi,[r13+PG_MSG_NAME]
    mov [r12+40],rdi
    call .Llength
    mov [r12+48],rax
    jmp .Lpoll_one
.Ltool_bad_limit:
    lea rsi,[rip+.Ltool_limit]
    jmp .Ltool_error
.Ltool_bad_status:
    lea rsi,[rip+.Ltool_status_error]
    jmp .Ltool_error
.Ltool_complete:
    mov [rbx+PG_A_TOOL_STATUS],r15d
    mov rax,[rsp+56]
    mov [rbx+PG_A_TOOL_SIZE],rax
    mov dword ptr [rbx+PG_A_STATE],PG_ST_TOOL_CLOSE
.Ltool_close:
    cmp dword ptr [rbx+PG_A_TOOL_OPEN],0
    je .Ltool_released
    mov rdi,[rbx+PG_A_TOOLS]
    call [rbx+PG_A_TOOLS+48]
    cmp eax,1
    je .Lpoll_zero
    test eax,eax
    jz .Ltool_closed_ok
    mov rdi,[rbx+PG_A_TOOLS]
    call [rbx+PG_A_TOOLS+40]
    lea rdi,[rip+.Ltool_cleanup_error]
    call .Llength
    mov [rbx+PG_A_TOOL_SIZE],rax
    mov rdx,rax
    lea rsi,[rip+.Ltool_cleanup_error]
    lea rdi,[rbx+PG_A_TEXT]
    call .Lcopy
    mov dword ptr [rbx+PG_A_TOOL_STATUS],1
    jmp .Lpoll_zero
.Ltool_closed_ok:
    mov dword ptr [rbx+PG_A_TOOL_OPEN],0
.Ltool_released:
    imul r13,[rbx+PG_A_CALL],PG_MSG_BYTES
    add r13,rbx
    mov r15d,[rbx+PG_A_TOOL_STATUS]
    mov rax,[rbx+PG_A_TOOL_SIZE]
    mov [rsp+56],rax
    cmp dword ptr [rbx+PG_A_CANCEL],0
    je .Ltool_append
    mov r15d,1
    test rax,rax
    jnz .Ltool_append
    lea rsi,[rip+.Lcancelled]
    lea rdi,[rbx+PG_A_TEXT]
    mov edx,9
    call .Lcopy
    mov qword ptr [rsp+56],9
    jmp .Ltool_append
.Lfinish_wait:
    mov rdi,rbx
    mov esi,[rbx+PG_A_OUTCOME]
    call .Lfinish
    jmp .Lpoll_zero
.Ltool_denied:
    lea rsi,[rip+.Ltool_unavailable]
.Ltool_error:
    mov r14,rsi
    cmp dword ptr [rbx+PG_A_TOOL_OPEN],0
    je .Ltool_error_copy
    mov rdi,[rbx+PG_A_TOOLS]
    call [rbx+PG_A_TOOLS+40]
.Ltool_error_copy:
    mov rsi,r14
    lea rdi,[rbx+PG_A_TEXT]
    mov edx,26
    call .Lcopy
    mov qword ptr [rsp+56],26
    mov r15d,1
    jmp .Ltool_complete
.Ltool_append:
    mov rdi,rbx
    mov esi,3
    lea rdx,[rbx+PG_A_TEXT]
    mov rcx,[rsp+56]
    call .Lprepare
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
    cmp dword ptr [rbx+PG_A_CANCEL],0
    jne .Lcancel_group
    mov rcx,[rbx+PG_A_CALL]
    inc rcx
    mov esi,1
    cmp rcx,[rbx+PG_A_GROUP_END]
    jae .Ltool_model_checkpoint
    imul rbp,rcx,PG_MSG_BYTES
    add rbp,rbx
    lea rdi,[rbp+PG_MSG_NAME]
    call .Llength
    mov rdx,rax
    lea rsi,[rbp+PG_MSG_NAME]
    mov rdi,rbx
    call .Lsafe_to_replay
    mov esi,5
    cmp eax,1
    jne .Ltool_next_checkpoint
    mov esi,2
.Ltool_next_checkpoint:
    mov rcx,[rbx+PG_A_CALL]
    inc rcx
    jmp .Ltool_checkpoint
.Ltool_model_checkpoint:
    mov rcx,-1
.Ltool_checkpoint:
    mov rdi,rbx
    xor edx,edx
    call .Lnext_checkpoint
    mov rdi,rbx
    mov rsi,r14
    mov edx,1
    call .Lcommit_records
    test rax,rax
    jz .Lerror_storage
    jmp .Ltool_committed
.Lcancel_group:
    mov qword ptr [rsp+72],1
.Lcancel_suffix:
    mov rax,[rbx+PG_A_CALL]
    add rax,[rsp+72]
    cmp rax,[rbx+PG_A_GROUP_END]
    jae .Lcancel_group_commit
    imul rbp,rax,PG_MSG_BYTES
    add rbp,rbx
    mov rax,[rsp+72]
    imul rax,PG_MSG_BYTES
    lea r14,[rbx+PG_A_PENDING+rax]
    mov rdi,r14
    xor eax,eax
    mov ecx,PG_MSG_BYTES/8
    rep stosq
    mov dword ptr [r14],PG_ROLE_RESULT
    mov dword ptr [r14+PG_MSG_ERROR],1
    mov qword ptr [r14+PG_MSG_SIZE],9
    lea rdi,[r14+PG_MSG_TEXT]
    lea rsi,[rip+.Lcancelled]
    mov edx,9
    call .Lcopy
    lea rdi,[rbp+PG_MSG_ID]
    call .Llength
    mov rdx,rax
    lea rsi,[rbp+PG_MSG_ID]
    lea rdi,[r14+PG_MSG_ID]
    call .Lcopy
    inc qword ptr [rsp+72]
    jmp .Lcancel_suffix
.Lcancel_group_commit:
    mov rdi,rbx
    mov esi,3
    mov edx,2
    mov rcx,-1
    call .Lnext_checkpoint
    mov rdi,rbx
    lea rsi,[rbx+PG_A_PENDING]
    mov rdx,[rsp+72]
    call .Lcommit_records
    test rax,rax
    jz .Lerror_storage
    mov dword ptr [rbx+PG_A_GROUP_CANCELLED],1
    mov dword ptr [rbx+PG_A_STATE],PG_ST_TOOL_END
    jmp .Ltool_end
.Ltool_committed:
    mov dword ptr [rbx+PG_A_STATE],PG_ST_TOOL_END
    jmp .Lpoll_zero
.Ltool_end:
    mov rax,[rbx+PG_A_COUNT]
    dec rax
    cmp dword ptr [rbx+PG_A_GROUP_CANCELLED],0
    je .Ltool_end_result
    inc rax
    sub rax,[rbx+PG_A_GROUP_END]
    add rax,[rbx+PG_A_CALL]
.Ltool_end_result:
    imul r13,rax,PG_MSG_BYTES
    add r13,rbx
    imul r14,[rbx+PG_A_CALL],PG_MSG_BYTES
    add r14,rbx
    xor ecx,ecx
    cmp dword ptr [r13+PG_MSG_ERROR],0
    setne cl
    cmp dword ptr [rbx+PG_A_GROUP_CANCELLED],0
    je .Ltool_end_outcome
    mov ecx,2
.Ltool_end_outcome:
    mov rdi,r12
    mov rsi,r14
    mov rdx,r13
    call .Ltool_event
    cmp dword ptr [rbx+PG_A_GROUP_CANCELLED],0
    jne .Ltool_end_cancelled
    mov dword ptr [rbx+PG_A_STATE],PG_ST_START
    mov rax,[rbx+PG_A_CALL]
    inc rax
    cmp rax,[rbx+PG_A_GROUP_END]
    jae .Lpoll_one
    mov dword ptr [rbx+PG_A_STATE],PG_ST_NEXT_TOOL
    jmp .Lpoll_one
.Ltool_end_cancelled:
    inc qword ptr [rbx+PG_A_CALL]
    mov rax,[rbx+PG_A_CALL]
    cmp rax,[rbx+PG_A_GROUP_END]
    jb .Lpoll_one
    mov rdi,rbx
    mov esi,2
    call .Lfinish
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
.Lerror_profile:
    lea rsi,[rip+.Lerror_profile_text]
    jmp .Lpoll_fail
.Lerror_storage:
    mov rdi,rbx
    call .Lstorage_fail
    jmp .Lpoll_zero
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
.Linterrupted: .asciz "Tool was interrupted and may have partially run"
.Lcancelled: .asciz "cancelled"
.Ltool_status_error: .asciz "invalid tool status result"
.Ltool_cleanup_error: .asciz "tool cleanup failed; ownership retained"
.Ltool_unavailable: .asciz "tool unavailable or denied"
.Ltool_limit: .asciz "tool output limit exceeded"
.Lerror_profile_text: .asciz "runtime profile is not verified"
.Lerror_storage_text: .asciz "session storage commit failed; session stopped"
.Lerror_state: .asciz "invalid runtime state"
.Lerror_turns_text: .asciz "model turn limit reached"
.Lerror_start_text: .asciz "model request initialization failed"
.Lerror_capacity_text: .asciz "session message capacity exhausted"
.Lerror_stream_text: .asciz "model transport or protocol failed"
.Lerror_model_text: .asciz "model reported an error"
.Lerror_text_text: .asciz "assistant text limit exceeded"
.Lerror_event_text: .asciz "invalid model event or tool identifier"
.Lerror_result_capacity_text: .asciz "session has no room for tool results and follow-up reply"
.Lerror_arguments_text: .asciz "tool arguments or session capacity exceeded"
.section .note.GNU-stack,"",@progbits
