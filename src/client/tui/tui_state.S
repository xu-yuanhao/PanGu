/* tui_state.S — 客户端状态机：submit/cancel/poll 派发，手写汇编原地替换 tui_state.c。
 *
 * 契约见 tui_state.h：忙拒绝、取消仅请求、唯一 run_finished 终态、每次 poll 最多
 * TUI_STATE_POLL_BATCH 条事件、poll 错误置 fatal 且绝不补造终态。
 * 通过 tui_runtime_ops 函数表调用 Runtime（显式函数表，保持替换点）；
 * 渲染经 tui_view_*。事件在栈上按 tui_event 布局传递。
 */
#include "tui_layout.h"
.intel_syntax noprefix
.section .text

/* void tui_state_init(tui_state *s, const tui_runtime_ops *ops, tui_view *view)
 * rdi=s rsi=ops rdx=view */
.globl tui_state_init
.type tui_state_init,@function
tui_state_init:
 .cfi_startproc
 mov qword ptr [rdi + TUI_LI_STATE_OPS], rsi
 mov qword ptr [rdi + TUI_LI_STATE_VIEW], rdx
 mov dword ptr [rdi + TUI_LI_STATE_RUNNING], 0
 mov dword ptr [rdi + TUI_LI_STATE_CANCEL_SHOWN], 0
 mov dword ptr [rdi + TUI_LI_STATE_FATAL], 0
 ret
 .cfi_endproc
.size tui_state_init, .-tui_state_init

/* int tui_state_running(const tui_state *s) / int tui_state_fatal(...) */
.globl tui_state_running
.type tui_state_running,@function
tui_state_running:
 .cfi_startproc
 mov eax, dword ptr [rdi + TUI_LI_STATE_RUNNING]
 ret
 .cfi_endproc
.size tui_state_running, .-tui_state_running

.globl tui_state_fatal
.type tui_state_fatal,@function
tui_state_fatal:
 .cfi_startproc
 mov eax, dword ptr [rdi + TUI_LI_STATE_FATAL]
 ret
 .cfi_endproc
.size tui_state_fatal, .-tui_state_fatal

/* tui_submit_result tui_state_submit(tui_state *s, const char *utf8, size_t byte_len)
 * rdi=s rsi=utf8 rdx=len → eax */
.globl tui_state_submit
.type tui_state_submit,@function
tui_state_submit:
 .cfi_startproc
 push rbx
 push r12
 push r13
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 cmp dword ptr [rbx + TUI_LI_STATE_RUNNING], 0
 je .Lsubmit_go
 mov rdi, qword ptr [rbx + TUI_LI_STATE_VIEW]
 call tui_view_busy_rejected@PLT
 mov eax, TUI_LI_SUBMIT_BUSY
 jmp .Lsubmit_out
.Lsubmit_go:
 mov rax, qword ptr [rbx + TUI_LI_STATE_OPS]
 mov rdi, qword ptr [rax + TUI_LI_OPS_CTX]
 mov rsi, r12
 mov rdx, r13
 call [rax + TUI_LI_OPS_SUBMIT]
 mov r12d, eax /* rc：跨 view 调用保存 */
 test eax, eax
 jnz .Lsubmit_not_ok
 mov dword ptr [rbx + TUI_LI_STATE_RUNNING], 1
 mov dword ptr [rbx + TUI_LI_STATE_CANCEL_SHOWN], 0
 xor eax, eax
 jmp .Lsubmit_out
.Lsubmit_not_ok:
 cmp eax, TUI_LI_SUBMIT_ERROR
 jne .Lsubmit_ret_rc
 mov rdi, qword ptr [rbx + TUI_LI_STATE_VIEW]
 call tui_view_submit_error@PLT
.Lsubmit_ret_rc:
 mov eax, r12d
.Lsubmit_out:
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_state_submit, .-tui_state_submit

/* int tui_state_cancel(tui_state *s) — rdi=s → eax */
.globl tui_state_cancel
.type tui_state_cancel,@function
tui_state_cancel:
 .cfi_startproc
 push rbx
 push r12
 push r13
 mov rbx, rdi
 cmp dword ptr [rbx + TUI_LI_STATE_RUNNING], 0
 je .Lcancel_none
 mov rax, qword ptr [rbx + TUI_LI_STATE_OPS]
 mov rdi, qword ptr [rax + TUI_LI_OPS_CTX]
 call [rax + TUI_LI_OPS_CANCEL]
 mov r12d, eax
 test eax, eax
 jz .Lcancel_out
 cmp dword ptr [rbx + TUI_LI_STATE_CANCEL_SHOWN], 0
 jne .Lcancel_out
 mov rdi, qword ptr [rbx + TUI_LI_STATE_VIEW]
 call tui_view_cancel_requested@PLT
 mov dword ptr [rbx + TUI_LI_STATE_CANCEL_SHOWN], 1
.Lcancel_out:
 mov eax, r12d
 pop r13
 pop r12
 pop rbx
 ret
.Lcancel_none:
 xor eax, eax
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_state_cancel, .-tui_state_cancel

.globl tui_state_attach
.type tui_state_attach,@function
tui_state_attach:
 push rbx
 push r12
 push r13
 sub rsp,80
 mov rbx,rdi
 cmp dword ptr [rbx+TUI_LI_STATE_FATAL],0
 jne .Lattach_bad
 cmp dword ptr [rbx+TUI_LI_STATE_RUNNING],0
 jne .Lattach_idle
 mov rax,[rbx+TUI_LI_STATE_OPS]
 mov rdi,[rax+TUI_LI_OPS_CTX]
 mov rsi,rsp
 call [rax+TUI_LI_OPS_POLL]
 test eax,eax
 js .Lattach_poll_error
 jz .Lattach_idle
 cmp dword ptr [rsp+TUI_LI_EVENT_KIND],TUI_LI_EV_RUN_STARTED
 jne .Lattach_protocol_error
 mov dword ptr [rbx+TUI_LI_STATE_RUNNING],1
 mov dword ptr [rbx+TUI_LI_STATE_CANCEL_SHOWN],0
 mov rdi,[rbx+TUI_LI_STATE_VIEW]
 mov rsi,rsp
 call tui_view_event@PLT
 mov eax,1
 jmp .Lattach_return
.Lattach_protocol_error:
 mov rdi,[rbx+TUI_LI_STATE_VIEW]
 call tui_view_protocol_violation@PLT
 jmp .Lattach_failed
.Lattach_poll_error:
 mov rdi,[rbx+TUI_LI_STATE_VIEW]
 call tui_view_poll_error@PLT
.Lattach_failed:
 mov dword ptr [rbx+TUI_LI_STATE_FATAL],1
.Lattach_bad:
 mov eax,-1
 jmp .Lattach_return
.Lattach_idle:
 xor eax,eax
.Lattach_return:
 add rsp,80
 pop r13
 pop r12
 pop rbx
 ret
.size tui_state_attach,.-tui_state_attach

/* void tui_state_poll(tui_state *s) — rdi=s */
.globl tui_state_poll
.type tui_state_poll,@function
tui_state_poll:
 .cfi_startproc
 push rbx
 push r12
 push r13
 mov rbx, rdi
 cmp dword ptr [rbx + TUI_LI_STATE_RUNNING], 0
 je .Lpoll_out
 cmp dword ptr [rbx + TUI_LI_STATE_FATAL], 0
 jne .Lpoll_out
 xor r13d, r13d /* i = 0 */
.Lpoll_loop:
 cmp r13d, TUI_LI_STATE_POLL_BATCH
 jae .Lpoll_out /* 有限批量：交回主循环 */
 cmp dword ptr [rbx + TUI_LI_STATE_RUNNING], 0
 je .Lpoll_out
 mov rax, qword ptr [rbx + TUI_LI_STATE_OPS]
 mov rdi, qword ptr [rax + TUI_LI_OPS_CTX]
 sub rsp, TUI_LI_EVENT_SIZE + 8 /* tui_event ev：72B+补齐保持16对齐 */
 mov rsi, rsp
 call [rax + TUI_LI_OPS_POLL]
 test eax, eax
 js .Lpoll_error
 jz .Lpoll_idle
 mov rdi, qword ptr [rbx + TUI_LI_STATE_VIEW]
 mov rsi, rsp
 call tui_view_event@PLT
 cmp dword ptr [rsp + TUI_LI_EVENT_KIND], TUI_LI_EV_RUN_FINISHED
 jne .Lpoll_next
 mov dword ptr [rbx + TUI_LI_STATE_RUNNING], 0 /* 唯一终态：本批停止 */
 add rsp, TUI_LI_EVENT_SIZE + 8
 jmp .Lpoll_out
.Lpoll_next:
 add rsp, TUI_LI_EVENT_SIZE + 8
 inc r13d
 jmp .Lpoll_loop
.Lpoll_idle:
 add rsp, TUI_LI_EVENT_SIZE + 8
 jmp .Lpoll_out
.Lpoll_error:
 add rsp, TUI_LI_EVENT_SIZE + 8
 mov dword ptr [rbx + TUI_LI_STATE_FATAL], 1 /* 不补造终态，如实报错 */
 mov dword ptr [rbx + TUI_LI_STATE_RUNNING], 0
 mov rdi, qword ptr [rbx + TUI_LI_STATE_VIEW]
 call tui_view_poll_error@PLT
.Lpoll_out:
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_state_poll, .-tui_state_poll

.section .note.GNU-stack,"",@progbits
