#include "runtime_bridge_layout.h"
#include "tui_layout.h"
#include "../../runtime/session_layout.h"
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
    lea rax,[rip+.Lhistory]
    mov [r8+PG_B_OPS+TUI_LI_OPS_HISTORY],rax
    xor eax,eax
    ret
.Linit_bad:
    mov eax,1
    ret
.size pg_tui_bridge_init,.-pg_tui_bridge_init

.globl pg_tui_bridge_set_detach
.type pg_tui_bridge_set_detach,@function
pg_tui_bridge_set_detach:
    test rdi,rdi
    jz .Ldetach_bad
    test rsi,rsi
    jz .Ldetach_bad
    cmp dword ptr [rdi+PG_B_ACTIVE],0
    jne .Ldetach_bad
    cmp dword ptr [rdi+PG_B_STOPPED],0
    jne .Ldetach_bad
    mov [rdi+PG_B_DETACH],rsi
    mov [rdi+PG_B_DETACH_CONTEXT],rdx
    xor eax,eax
    ret
.Ldetach_bad:
    mov eax,1
    ret
.size pg_tui_bridge_set_detach,.-pg_tui_bridge_set_detach

.globl pg_tui_bridge_set_history
.type pg_tui_bridge_set_history,@function
pg_tui_bridge_set_history:
    test rdi,rdi
    jz .Ldetach_bad
    test rsi,rsi
    jz .Ldetach_bad
    cmp dword ptr [rdi+PG_B_ACTIVE],0
    jne .Ldetach_bad
    cmp dword ptr [rdi+PG_B_STOPPED],0
    jne .Ldetach_bad
    mov rax,[rsi+PG_S_COUNT]
    cmp rax,64
    ja .Ldetach_bad
    mov [rdi+PG_B_HISTORY],rsi
    mov [rdi+PG_B_HISTORY_COUNT],rax
    xor eax,eax
    ret
.size pg_tui_bridge_set_history,.-pg_tui_bridge_set_history

.Lhistory:
    test rdx,rdx
    jz .Lhistory_bad
    cmp dword ptr [rdi+PG_B_STOPPED],0
    jne .Lhistory_bad
    mov r8,[rdi+PG_B_HISTORY]
    test r8,r8
    jz .Lhistory_end
    mov rax,[rdi+PG_B_HISTORY_COUNT]
    test rax,rax
    js .Lhistory_dynamic
    cmp rax,[r8+PG_S_COUNT]
    jne .Lhistory_bad
    cmp rsi,rax
    jae .Lhistory_end
    mov r10,rsi
    imul rax,rsi,PG_MSG_BYTES
    add r8,rax
    mov r9,rdx
    mov eax,[r8]
    cmp eax,3
    ja .Lhistory_bad
    mov rcx,[r8+PG_MSG_SIZE]
    cmp rcx,4096
    jae .Lhistory_bad
    cmp byte ptr [r8+PG_MSG_TEXT+rcx],0
    jne .Lhistory_bad
    cmp dword ptr [r8+PG_MSG_ERROR],1
    ja .Lhistory_bad
    mov rdi,r9
    xor eax,eax
    mov ecx,TUI_LI_HISTORY_SIZE/8
    rep stosq
    mov eax,[r8]
    mov [r9+TUI_LI_HISTORY_KIND],eax
    lea rax,[r8+PG_MSG_TEXT]
    mov [r9+TUI_LI_HISTORY_TEXT],rax
    mov rax,[r8+PG_MSG_SIZE]
    mov [r9+TUI_LI_HISTORY_TEXT_LEN],rax
    xor eax,eax
    cmp dword ptr [r8+PG_MSG_ERROR],0
    sete al
    mov [r9+TUI_LI_HISTORY_OK],eax
    cmp dword ptr [r8],2
    jb .Lhistory_one
    lea rsi,[r8+PG_MSG_ID]
    call .Lhistory_length
    test edx,edx
    jz .Lhistory_bad
    mov [r9+TUI_LI_HISTORY_ID],rsi
    mov [r9+TUI_LI_HISTORY_ID_LEN],rdx
    cmp dword ptr [r8],2
    je .Lhistory_name
    test r10,r10
    jz .Lhistory_bad
    lea r11,[r8-PG_MSG_BYTES]
    xor edi,edi
.Lhistory_results_back:
    dec r10
    cmp dword ptr [r11],3
    jne .Lhistory_calls_back
    inc edi
    cmp edi,8
    jae .Lhistory_bad
    test r10,r10
    jz .Lhistory_bad
    sub r11,PG_MSG_BYTES
    jmp .Lhistory_results_back
.Lhistory_calls_back:
    cmp dword ptr [r11],2
    jne .Lhistory_bad
    mov rax,r11
    mov ecx,1
.Lhistory_group_start:
    test r10,r10
    jz .Lhistory_group_found
    cmp dword ptr [r11-PG_MSG_BYTES],2
    jne .Lhistory_group_found
    dec r10
    sub r11,PG_MSG_BYTES
    inc ecx
    cmp ecx,8
    ja .Lhistory_bad
    jmp .Lhistory_group_start
.Lhistory_group_found:
    cmp edi,ecx
    jae .Lhistory_bad
    imul rdi,rdi,PG_MSG_BYTES
    add r11,rdi
    xor ecx,ecx
.Lhistory_match:
    mov al,[r8+PG_MSG_ID+rcx]
    cmp [r11+PG_MSG_ID+rcx],al
    jne .Lhistory_bad
    inc rcx
    cmp rcx,rdx
    jbe .Lhistory_match
    mov r8,r11
.Lhistory_name:
    lea rsi,[r8+PG_MSG_NAME]
    call .Lhistory_length
    test edx,edx
    jz .Lhistory_bad
    mov [r9+TUI_LI_HISTORY_NAME],rsi
    mov [r9+TUI_LI_HISTORY_NAME_LEN],rdx
.Lhistory_one:
    mov eax,1
    ret
.Lhistory_length:
    xor edx,edx
.Lhistory_length_byte:
    cmp byte ptr [rsi+rdx],0
    je .Lhistory_length_end
    inc edx
    cmp edx,64
    jb .Lhistory_length_byte
    xor edx,edx
.Lhistory_length_end:
    ret
.Lhistory_end:
    xor eax,eax
    ret
.Lhistory_bad:
    mov eax,-1
    ret

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
    cmp dword ptr [rbx+PG_B_STOPPED],1
    je .Lpoll_zero
    mov rdi,rsp
    mov ecx,7
    rep stosq
    mov rdi,[rbx]
    mov rsi,rsp
    call [rbx+24]
    test eax,eax
    jle .Lpoll_return
    mov eax,[rsp]
    /* Committed usage is available through the optional observation binding. */
    cmp eax,6
    jb .Lpoll_execution_event
    cmp eax,12
    jbe .Lpoll_zero
.Lpoll_execution_event:
    cmp eax,5
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
    cmp dword ptr [rsp],5
    je .Ltext
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
    cmp dword ptr [rbx+PG_B_STOPPED],1
    je .Lshutdown_released
    cmp qword ptr [rbx+PG_B_DETACH],0
    jne .Lshutdown_detach
    cmp dword ptr [rbx+PG_B_ACTIVE],0
    je .Lshutdown_stop
    cmp dword ptr [rbx+PG_B_STOPPED],2
    je .Lshutdown_resume
    mov dword ptr [rbx+PG_B_STOPPED],2
    mov rdi,[rbx]
    call [rbx+16]
.Lshutdown_resume:
    xor r12d,r12d
.Lshutdown_drain:
    cmp dword ptr [rbx+PG_B_ACTIVE],0
    je .Lshutdown_stop
    cmp r12d,8
    jae .Lshutdown_pending
    mov rdi,rbx
    mov rsi,rsp
    call .Lpoll
    test eax,eax
    js .Lshutdown_error
    inc r12d
    jmp .Lshutdown_drain
.Lshutdown_detach:
    mov dword ptr [rbx+PG_B_STOPPED],2
    mov rdi,[rbx+PG_B_DETACH_CONTEXT]
    call [rbx+PG_B_DETACH]
    test eax,eax
    jz .Lshutdown_stop
    cmp eax,2
    je .Lshutdown_pending
.Lshutdown_error:
    mov dword ptr [rbx+PG_B_STOP_ERROR],1
.Lshutdown_pending:
    mov eax,1
    jmp .Lshutdown_return
.Lshutdown_stop:
    mov dword ptr [rbx+PG_B_STOPPED],1
.Lshutdown_released:
    xor eax,eax
.Lshutdown_return:
    add rsp,80
    pop r13
    pop r12
    pop rbx
    ret
.section .rodata
.Lruntime_name: .asciz "PanGu Durable"
.section .note.GNU-stack,"",@progbits

.text
.globl pg_tui_bridge_set_tool_observer
.type pg_tui_bridge_set_tool_observer,@function
pg_tui_bridge_set_tool_observer:
 mov eax,1
 test rdi,rdi
 jz .Lobserver_bind_end
 test rsi,rsi
 jz .Lobserver_bind_end
 cmp dword ptr [rdi+PG_B_STOPPED],0
 jne .Lobserver_bind_end
 cmp qword ptr [rsi+8],0
 je .Lobserver_bind_end
 cmp qword ptr [rsi+16],0
 je .Lobserver_bind_end
 movdqu xmm0,[rsi]
 movdqu [rdi+PG_B_TOOL_OBSERVER],xmm0
 mov rax,[rsi+16]
 mov [rdi+PG_B_TOOL_OBSERVER+16],rax
 xor eax,eax
.Lobserver_bind_end:
 ret
.size pg_tui_bridge_set_tool_observer,.-pg_tui_bridge_set_tool_observer
.globl pg_tui_bridge_tool_observer
.type pg_tui_bridge_tool_observer,@function
pg_tui_bridge_tool_observer:
 xor eax,eax
 test rdi,rdi
 jz .Lobserver_get_end
 cmp dword ptr [rdi+PG_B_STOPPED],0
 jne .Lobserver_get_end
 cmp qword ptr [rdi+PG_B_TOOL_OBSERVER+8],0
 je .Lobserver_get_end
 lea rax,[rdi+PG_B_TOOL_OBSERVER]
.Lobserver_get_end:
 ret
.size pg_tui_bridge_tool_observer,.-pg_tui_bridge_tool_observer
.text
.globl pg_tui_runtime_tool_observer
.type pg_tui_runtime_tool_observer,@function
pg_tui_runtime_tool_observer:
 xor eax,eax
 test rdi,rdi
 jz .Lobserver_ops_end
 lea rcx,[rip+.Lsubmit]
 cmp [rdi+TUI_LI_OPS_SUBMIT],rcx
 jne .Lobserver_ops_end
 lea rcx,[rip+.Lpoll]
 cmp [rdi+TUI_LI_OPS_POLL],rcx
 jne .Lobserver_ops_end
 mov rdi,[rdi+TUI_LI_OPS_CTX]
 jmp pg_tui_bridge_tool_observer
.Lobserver_ops_end:
 ret
.size pg_tui_runtime_tool_observer,.-pg_tui_runtime_tool_observer

.text
.globl pg_tui_bridge_set_history_rows
.type pg_tui_bridge_set_history_rows,@function
pg_tui_bridge_set_history_rows:
 test rdi,rdi
 jz .Ldetach_bad
 test rsi,rsi
 jz .Ldetach_bad
 cmp dword ptr [rdi+PG_B_ACTIVE],0
 jne .Ldetach_bad
 cmp dword ptr [rdi+PG_B_STOPPED],0
 jne .Ldetach_bad
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsi
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lhistory_rows_bad
 mov rdi,r12
 call json_object_array_length@PLT
 test rax,rax
 js .Lhistory_rows_bad
 bts rax,63
 mov [rbx+PG_B_HISTORY],r12
 mov [rbx+PG_B_HISTORY_COUNT],rax
 xor eax,eax
 jmp .Lhistory_rows_return
.Lhistory_rows_bad:
 mov eax,1
.Lhistory_rows_return:
 add rsp,8
 pop r12
 pop rbx
 ret
.Lhistory_dynamic:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 btr rax,63
 cmp rsi,rax
 jae .Lhistory_dynamic_end
 mov rdi,r8
 call json_object_array_length@PLT
 mov rcx,[rbx+PG_B_HISTORY_COUNT]
 btr rcx,63
 cmp rax,rcx
 jne .Lhistory_dynamic_bad
 mov rdi,[rbx+PG_B_HISTORY]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,r13
 xor eax,eax
 mov ecx,TUI_LI_HISTORY_SIZE/8
 rep stosq
 mov rdi,r14
 lea rsi,[rip+.Lbh_kind]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,3
 ja .Lhistory_dynamic_bad
 mov [r13+TUI_LI_HISTORY_KIND],eax
 mov rdi,r14
 lea rsi,[rip+.Lbh_text]
 call json_object_object_get@PLT
 mov [rsp],rax
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lhistory_dynamic_bad
 mov [r13+TUI_LI_HISTORY_TEXT],rax
 mov rdi,[rsp]
 call json_object_get_string_len@PLT
 mov [r13+TUI_LI_HISTORY_TEXT_LEN],rax
 mov rdi,r14
 lea rsi,[rip+.Lbh_ok]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov [r13+TUI_LI_HISTORY_OK],eax
 cmp dword ptr [r13+TUI_LI_HISTORY_KIND],2
 jb .Lhistory_dynamic_ready
 mov rdi,r14
 lea rsi,[rip+.Lbh_id]
 call json_object_object_get@PLT
 mov [rsp],rax
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lhistory_dynamic_bad
 mov [r13+TUI_LI_HISTORY_ID],rax
 mov rdi,[rsp]
 call json_object_get_string_len@PLT
 mov [r13+TUI_LI_HISTORY_ID_LEN],rax
 mov rdi,r14
 lea rsi,[rip+.Lbh_name]
 call json_object_object_get@PLT
 mov [rsp],rax
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lhistory_dynamic_bad
 mov [r13+TUI_LI_HISTORY_NAME],rax
 mov rdi,[rsp]
 call json_object_get_string_len@PLT
 mov [r13+TUI_LI_HISTORY_NAME_LEN],rax
.Lhistory_dynamic_ready:
 mov eax,1
 jmp .Lhistory_dynamic_return
.Lhistory_dynamic_bad:
 mov eax,-1
 jmp .Lhistory_dynamic_return
.Lhistory_dynamic_end:
 xor eax,eax
.Lhistory_dynamic_return:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.section .rodata
.Lbh_kind: .asciz "kind"
.Lbh_text: .asciz "text"
.Lbh_ok: .asciz "ok"
.Lbh_id: .asciz "id"
.Lbh_name: .asciz "name"
