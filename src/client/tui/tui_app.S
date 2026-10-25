/* tui_app.S — 客户端组合与主循环，手写汇编原地替换 tui_app.c。
 *
 * 契约见 tui_app.h：poll() 等待 stdin/唤醒管道；闲置无限阻塞、活动期按 tick 推进；
 * 信号处理只做 sig_atomic_t 标志 + 自管道写（write 异步信号安全）；
 * Ctrl-C 忙时请求取消、闲置退出；EOF/外部信号优雅退出并恢复终端。
 *
 * 平台结构：sigaction 布局经 tui_layout.h 常量 + ABI 测试 offsetof 断言；
 * errno 经 glibc 公开的 __errno_location()，不硬编码 TLS 布局。
 */
#include "tui_layout.h"
.intel_syntax noprefix

/* 主循环栈帧布局（rsp 相对，sub APP_FRAME 后） */
.equ OFF_TERM,    0   /* tui_term 96 */
.equ OFF_INPUT,   96  /* tui_input 1056 */
.equ OFF_VIEW,    1152 /* tui_view 48 */
.equ OFF_STATE,   1200 /* tui_state 32 */
.equ OFF_SINK,    1232 /* tui_out_sink 16 */
.equ OFF_WAKE,    1248 /* int wake[2] */
.equ OFF_SAVED,   1256 /* struct sigaction saved[4] 608 */
.equ OFF_SA,      1864 /* struct sigaction sa 152 */
.equ OFF_PFDS,    2016 /* struct pollfd[2] 16 */
.equ OFF_BUF,     2032 /* unsigned char buf[256] */
.equ OFF_REASON,  2288
.equ OFF_TICK,    2292
.equ OFF_COUNT,   2296
.equ OFF_INTERACTIVE, 2300
.equ OFF_K,       2304 /* size_t 字节循环计数 */
.equ OFF_N,       2312 /* ssize_t read 返回值 */
.equ APP_FRAME,   2336

.section .rodata
handled_signals:
 .long TUI_LI_SIGINT, TUI_LI_SIGTERM, TUI_LI_SIGHUP, TUI_LI_SIGWINCH
.Lwake_byte:  .asciz "x"
.Lname_eof:      .asciz "EOF"
.Lname_ctrl_c:   .asciz "Ctrl-C 闲置退出"
.Lname_signal:   .asciz "收到终止信号"
.Lname_poll_err: .asciz "Runtime 错误"
.Lname_unknown:  .asciz "未知"
.Lruntime_dflt:  .asciz "runtime"

/* 信号路径全局最小状态：仅 int 标志与自管道 fd（与被替换 C 版一致） */
.section .data
 .align 4
g_exit_signum:
 .long 0
g_winch:
 .long 0
g_wake_wfd:
 .long -1

.section .text

/* ---- 信号处理路径：置标志 + 自管道写；任何清理都交回主循环 ---- */
.type tui_li_on_exit_signal,@function
tui_li_on_exit_signal:
 mov dword ptr [rip + g_exit_signum], edi
 /* 栈对齐与入口无关化：保存 rsp，按 16 对齐后调用 wake_main */
 push rax
 mov rax, rsp
 and rsp, -16
 call tui_li_wake_main
 mov rsp, rax
 pop rax
 ret
.size tui_li_on_exit_signal, .-tui_li_on_exit_signal

.type tui_li_on_winch,@function
tui_li_on_winch:
 mov dword ptr [rip + g_winch], 1
 push rax
 mov rax, rsp
 and rsp, -16
 call tui_li_wake_main
 mov rsp, rax
 pop rax
 ret
.size tui_li_on_winch, .-tui_li_on_winch

.type tui_li_wake_main,@function
tui_li_wake_main:
 push rax
 mov edi, dword ptr [rip + g_wake_wfd]
 test edi, edi
 js .Lwm_ret
 lea rsi, [rip + .Lwake_byte]
 mov edx, 1
 call write@PLT
.Lwm_ret:
 pop rax
 ret
.size tui_li_wake_main, .-tui_li_wake_main

/* ---- int install_handlers(sa, saved, count)：成功 0，失败 -1 ----
 * rdi=&sa rsi=&saved[4] rdx=&count（每装好一个递增） */
.type tui_li_install_handlers,@function
tui_li_install_handlers:
 push rbx
 push r12
 push r13
 sub rsp, 16 /* [rsp] = 循环变量 i */
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov rdi, rbx /* memset sa, 0, 152 */
 xor eax, eax
 mov ecx, 19
 cld
 rep stosq
 mov qword ptr [rsp], 0
.Lih_loop:
 mov rax, [rsp]
 cmp rax, 4
 jae .Lih_ok
 /* sa.sa_handler = i==3 ? on_winch : on_exit_signal；sa_flags/sa_mask 保持 0 */
 lea rcx, [rip + tui_li_on_exit_signal]
 cmp rax, 3
 jne .Lih_set_handler
 lea rcx, [rip + tui_li_on_winch]
.Lih_set_handler:
 mov qword ptr [rbx + TUI_LI_SA_HANDLER], rcx
 /* sigaction(handled_signals[i], &sa, &saved[i]) */
 lea rdx, [rip + handled_signals]
 mov edi, dword ptr [rdx + rax*4]
 mov rsi, rbx
 imul rax, rax, TUI_LI_SIGACTION_SIZE
 lea rdx, [r12 + rax]
 call sigaction@PLT
 test eax, eax
 jnz .Lih_fail
 inc dword ptr [r13]
 inc qword ptr [rsp]
 jmp .Lih_loop
.Lih_ok:
 xor eax, eax
 jmp .Lih_out
.Lih_fail:
 mov eax, -1
.Lih_out:
 add rsp, 16
 pop r13
 pop r12
 pop rbx
 ret
.size tui_li_install_handlers, .-tui_li_install_handlers

/* ---- void restore_handlers(saved, count)：逆序恢复 ----
 * rdi=saved esi=count */
.type tui_li_restore_handlers,@function
tui_li_restore_handlers:
 push rbx
 push r12
 push rax /* 16 字节对齐占位 */
 mov rbx, rdi
 mov r12d, esi /* 计数：callee-saved，跨 sigaction 调用保存 */
.Lrh_loop:
 test r12d, r12d
 jz .Lrh_out
 dec r12d
 movsxd rax, r12d
 lea rdx, [rip + handled_signals]
 mov edi, dword ptr [rdx + rax*4]
 imul rax, rax, TUI_LI_SIGACTION_SIZE
 lea rsi, [rbx + rax]
 xor edx, edx
 call sigaction@PLT
 jmp .Lrh_loop
.Lrh_out:
 pop rax
 pop r12
 pop rbx
 ret
.size tui_li_restore_handlers, .-tui_li_restore_handlers

/* ---- void drain_wake(int rfd)：读干唤醒管道 ---- */
.type tui_li_drain_wake,@function
tui_li_drain_wake:
 push r12
 sub rsp, 64 /* char tmp[64]，保持 16 对齐 */
 mov r12d, edi
.Ldw_loop:
 mov edi, r12d
 mov rsi, rsp
 mov edx, 64
 call read@PLT
 test rax, rax
 jg .Ldw_loop /* n > 0：继续读干 */
 js .Ldw_chk
 jmp .Ldw_out /* n == 0：管道空 */
.Ldw_chk:
 call __errno_location@PLT
 cmp dword ptr [rax], TUI_LI_EINTR
 je .Ldw_loop
.Ldw_out:
 add rsp, 64
 pop r12
 ret
.size tui_li_drain_wake, .-tui_li_drain_wake

/* ---- int term_write_cb(void *ctx, const char *data, size_t len)：sink 回调 ---- */
.type tui_li_term_write_cb,@function
tui_li_term_write_cb:
 jmp tui_term_write@PLT /* 签名一致，尾跳 */
.size tui_li_term_write_cb, .-tui_li_term_write_cb

/* int tui_app_run(const tui_app_config *cfg) — rdi=cfg → eax */
.globl tui_app_run
.type tui_app_run,@function
tui_app_run:
 .cfi_startproc
 test rdi, rdi
 jz .Lrun_neg1
 mov rax, qword ptr [rdi + TUI_LI_APP_OPS]
 test rax, rax
 jz .Lrun_neg1
 cmp qword ptr [rax + TUI_LI_OPS_SUBMIT], 0
 je .Lrun_neg1
 cmp qword ptr [rax + TUI_LI_OPS_POLL], 0
 je .Lrun_neg1
 cmp qword ptr [rax + TUI_LI_OPS_CANCEL], 0
 je .Lrun_neg1

 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, APP_FRAME
 mov rbx, rdi /* cfg */
 lea r12, [rsp + OFF_TERM]
 lea r13, [rsp + OFF_INPUT]
 lea r14, [rsp + OFF_VIEW]
 lea r15, [rsp + OFF_STATE]
 mov dword ptr [rip + g_exit_signum], 0
 mov dword ptr [rip + g_winch], 0
 mov dword ptr [rsp + OFF_WAKE], -1
 mov dword ptr [rsp + OFF_WAKE + 4], -1
 mov dword ptr [rsp + OFF_COUNT], 0
 mov dword ptr [rsp + OFF_REASON], TUI_LI_EXIT_EOF
 mov eax, dword ptr [rbx + TUI_LI_APP_ANSI]
 mov dword ptr [rsp + OFF_INTERACTIVE], eax
 mov eax, dword ptr [rbx + TUI_LI_APP_TICK_MS]
 test eax, eax
 jnz .Ltick_set
 mov eax, 20 /* TUI_DEFAULT_TICK_MS */
.Ltick_set:
 mov dword ptr [rsp + OFF_TICK], eax

 mov rdi, r12
 mov esi, dword ptr [rbx + TUI_LI_APP_IN_FD]
 mov edx, dword ptr [rbx + TUI_LI_APP_OUT_FD]
 call tui_term_init@PLT
 mov rdi, r13
 call tui_input_init@PLT
 /* sink = { ctx=&term, write=term_write_cb }；按值传参占 rsi/rdx */
 mov qword ptr [rsp + OFF_SINK], r12
 lea rax, [rip + tui_li_term_write_cb]
 mov qword ptr [rsp + OFF_SINK + 8], rax
 mov rdi, r14
 mov rsi, r12
 mov rdx, rax
 mov ecx, dword ptr [rsp + OFF_INTERACTIVE]
 mov r8, r13 /* 交互模式挂编辑器；非交互下面清 NULL */
 test ecx, ecx
 jnz .Lview_init
 xor r8d, r8d
.Lview_init:
 call tui_view_init@PLT
 mov rdi, r15
 mov rsi, qword ptr [rbx + TUI_LI_APP_OPS]
 mov rdx, r14
 call tui_state_init@PLT

 cmp dword ptr [rsp + OFF_INTERACTIVE], 0
 je .Lno_enter
 mov rdi, r12
 call tui_term_enter_interactive@PLT
 test eax, eax
 jnz .Lenter_failed /* 进入失败时未改动终端 */
.Lno_enter:
 lea rdi, [rsp + OFF_WAKE]
 call pipe@PLT
 test eax, eax
 jnz .Lfailure
 /* 两个管道端都设 O_NONBLOCK */
 mov edi, dword ptr [rsp + OFF_WAKE]
 call tui_li_set_nonblock
 test eax, eax
 jnz .Lfailure
 mov edi, dword ptr [rsp + OFF_WAKE + 4]
 call tui_li_set_nonblock
 test eax, eax
 jnz .Lfailure
 lea rdi, [rsp + OFF_SA]
 lea rsi, [rsp + OFF_SAVED]
 lea rdx, [rsp + OFF_COUNT]
 call tui_li_install_handlers
 test eax, eax
 jnz .Lfailure
 mov eax, dword ptr [rsp + OFF_WAKE + 4]
 mov dword ptr [rip + g_wake_wfd], eax

 /* 横幅与初始编辑行 */
 mov rax, qword ptr [rbx + TUI_LI_APP_OPS]
 mov rsi, qword ptr [rax + TUI_LI_OPS_NAME]
 test rsi, rsi
 jnz .Lhave_name
 lea rsi, [rip + .Lruntime_dflt]
.Lhave_name:
 mov rdi, r14
 call tui_view_banner@PLT
 mov rdi,r15
 call tui_state_history@PLT
 test eax,eax
 jns .Lhistory_ready
 mov dword ptr [rsp+OFF_REASON],TUI_LI_EXIT_POLL_ERROR
 jmp .Ldone
.Lhistory_ready:
 cmp dword ptr [rsp + OFF_INTERACTIVE], 0
 je .Lattach_runtime
 mov rdi, r14
 call tui_view_redraw_editor@PLT
.Lattach_runtime:
 mov rdi,r15
 call tui_state_attach@PLT
 test eax,eax
 jns .Lmain_loop
 mov dword ptr [rsp+OFF_REASON],TUI_LI_EXIT_POLL_ERROR
 jmp .Ldone

/* ================= 主循环 ================= */
.Lmain_loop:
 mov eax, dword ptr [rip + g_exit_signum]
 test eax, eax
 jz .Lchk_winch
 mov dword ptr [rsp + OFF_REASON], TUI_LI_EXIT_SIGNAL
 jmp .Ldone
.Lchk_winch:
 mov eax, dword ptr [rip + g_winch]
 test eax, eax
 jz .Ldo_poll
 mov dword ptr [rip + g_winch], 0
 mov rdi, r12
 mov rax, qword ptr [r12 + TUI_LI_TERM_PLATFORM]
 call [rax + TUI_LI_PLATFORM_QUERY_SIZE]
 cmp dword ptr [rsp + OFF_INTERACTIVE], 0
 je .Ldo_poll
 mov rdi, r14
 call tui_view_redraw_editor@PLT
.Ldo_poll:
 mov eax, dword ptr [rbx + TUI_LI_APP_IN_FD]
 mov dword ptr [rsp + OFF_PFDS], eax
 mov word ptr [rsp + OFF_PFDS + 4], TUI_LI_POLLIN
 mov word ptr [rsp + OFF_PFDS + 6], 0
 mov eax, dword ptr [rsp + OFF_WAKE]
 mov dword ptr [rsp + OFF_PFDS + 8], eax
 mov word ptr [rsp + OFF_PFDS + 12], TUI_LI_POLLIN
 mov word ptr [rsp + OFF_PFDS + 14], 0
 /* 闲置无限阻塞；活动期按 tick 定时推进 Runtime */
 mov edx, -1
 cmp dword ptr [r15 + TUI_LI_STATE_RUNNING], 0
 je .Ltimeout_ok
 mov edx, dword ptr [rsp + OFF_TICK]
.Ltimeout_ok:
 lea rdi, [rsp + OFF_PFDS]
 mov esi, 2
 call poll@PLT
 test eax, eax
 js .Lpoll_errno
 /* pfds[1].revents & POLLIN → 读干唤醒管道 */
 test word ptr [rsp + OFF_PFDS + 14], TUI_LI_POLLIN
 jz .Lcheck_stdin
 mov edi, dword ptr [rsp + OFF_WAKE]
 call tui_li_drain_wake
.Lcheck_stdin:
 movzx eax, word ptr [rsp + OFF_PFDS + 6]
 and eax, (TUI_LI_POLLIN | TUI_LI_POLLERR | TUI_LI_POLLHUP)
 test eax, eax
 jz .Lpump
 mov edi, dword ptr [rbx + TUI_LI_APP_IN_FD]
 lea rsi, [rsp + OFF_BUF]
 mov edx, 256
 call read@PLT
 test rax, rax
 jz .Lread_eof
 js .Lread_err
 mov qword ptr [rsp + OFF_N], rax
 mov qword ptr [rsp + OFF_K], 0
.Lkey_loop:
 mov rax, qword ptr [rsp + OFF_K]
 cmp rax, qword ptr [rsp + OFF_N]
 jae .Lpump
 lea rcx, [rsp + OFF_BUF]
 movzx esi, byte ptr [rcx + rax]
 mov rdi, r13
 call tui_input_feed@PLT
 cmp eax, TUI_LI_KEY_ENTER
 je .Lkey_enter
 cmp eax, TUI_LI_KEY_CTRL_C
 je .Lkey_ctrl_c
 cmp eax, TUI_LI_KEY_CTRL_D
 je .Lkey_ctrl_d
 cmp eax, TUI_LI_KEY_OVERFLOW
 je .Lkey_overflow
 cmp eax, TUI_LI_KEY_CHAR
 je .Lkey_redraw
 cmp eax, TUI_LI_KEY_BACKSPACE
 je .Lkey_redraw
 jmp .Lkey_next
.Lkey_enter:
 mov rdi, r13
 call tui_input_empty@PLT
 test eax, eax
 jnz .Lkey_next /* 空提交由上层忽略 */
 mov rdi, r13
 call tui_input_text@PLT
 mov rsi, rax
 mov rdi, r13
 call tui_input_len@PLT
 mov rdx, rax
 mov rdi, r15
 call tui_state_submit@PLT
 test eax, eax
 jnz .Lkey_next /* BUSY/ERROR：内容保留，提示已由 view/state 完成 */
 mov rdi, r14
 call tui_view_finalize_editor@PLT
 mov rdi, r13
 call tui_input_reset@PLT
 jmp .Lkey_next
.Lkey_ctrl_c:
 mov rdi, r15
 call tui_state_running@PLT
 test eax, eax
 jz .Lkey_ctrl_c_idle
 mov rdi, r15
 call tui_state_cancel@PLT
 jmp .Lkey_next
.Lkey_ctrl_c_idle:
 mov dword ptr [rsp + OFF_REASON], TUI_LI_EXIT_CTRL_C_IDLE
 jmp .Ldone
.Lkey_ctrl_d:
 mov dword ptr [rsp + OFF_REASON], TUI_LI_EXIT_EOF
 jmp .Ldone
.Lkey_overflow:
 mov rdi, r14
 call tui_view_input_limit@PLT
 jmp .Lkey_next
.Lkey_redraw:
 mov rdi, r14
 call tui_view_redraw_editor@PLT
.Lkey_next:
 inc qword ptr [rsp + OFF_K]
 jmp .Lkey_loop
.Lread_err:
 call __errno_location@PLT
 mov ecx, dword ptr [rax]
 cmp ecx, TUI_LI_EINTR
 je .Lpump
 cmp ecx, TUI_LI_EAGAIN
 je .Lpump
 mov dword ptr [rsp + OFF_REASON], TUI_LI_EXIT_POLL_ERROR
 jmp .Ldone
.Lread_eof:
 mov dword ptr [rsp + OFF_REASON], TUI_LI_EXIT_EOF
 jmp .Ldone
.Lpoll_errno:
 call __errno_location@PLT
 cmp dword ptr [rax], TUI_LI_EINTR
 je .Lmain_loop
 mov dword ptr [rsp + OFF_REASON], TUI_LI_EXIT_POLL_ERROR
 jmp .Ldone
.Lpump:
 mov rdi, r15
 call tui_state_poll@PLT
 mov rdi, r15
 call tui_state_fatal@PLT
 test eax, eax
 jz .Lmain_loop
 mov dword ptr [rsp + OFF_REASON], TUI_LI_EXIT_POLL_ERROR
 /* ---- 正常退出路径：恢复信号、shutdown、退出说明、恢复终端 ---- */
.Ldone:
 mov dword ptr [rip + g_wake_wfd], -1
 lea rdi, [rsp + OFF_SAVED]
 mov esi, dword ptr [rsp + OFF_COUNT]
 call tui_li_restore_handlers
 call tui_li_shutdown
 mov edi, dword ptr [rsp + OFF_REASON]
 call tui_exit_reason_name
 mov rsi, rax
 mov rdi, r14
 call tui_view_exit_note@PLT
 mov rdi, r12
 call tui_term_restore@PLT
 jmp .Lcleanup_fds
.Lenter_failed:
 mov eax, -1
 jmp .Lframe_out
.Lfailure:
 mov dword ptr [rip + g_wake_wfd], -1
 lea rdi, [rsp + OFF_SAVED]
 mov esi, dword ptr [rsp + OFF_COUNT]
 call tui_li_restore_handlers
 call tui_li_shutdown
 mov rdi, r12
 call tui_term_restore@PLT
 mov eax, -1
 jmp .Lframe_out
.Lcleanup_fds:
 mov edi, dword ptr [rsp + OFF_WAKE]
 call tui_li_close_if_open
 mov edi, dword ptr [rsp + OFF_WAKE + 4]
 call tui_li_close_if_open
 mov eax, dword ptr [rsp + OFF_REASON]
.Lframe_out:
 add rsp, APP_FRAME
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Lrun_neg1:
 mov eax, -1
 ret
 .cfi_endproc
.size tui_app_run, .-tui_app_run

/* ---- 局部：shutdown（cfg 在 rbx，由主函数持有） ---- */
.type tui_li_shutdown,@function
tui_li_shutdown:
 mov rax, qword ptr [rbx + TUI_LI_APP_OPS]
 mov rdi, qword ptr [rax + TUI_LI_OPS_CTX]
 mov rax, qword ptr [rax + TUI_LI_OPS_SHUTDOWN]
 test rax, rax
 jz .Lsd_ret
 jmp rax
.Lsd_ret:
 ret
.size tui_li_shutdown, .-tui_li_shutdown

/* ---- 局部：fcntl(fd, F_SETFL, fl|O_NONBLOCK)；0 成功 ---- */
.type tui_li_set_nonblock,@function
tui_li_set_nonblock:
 push rbx
 mov ebx, edi
 xor edx, edx /* fcntl 变参：F_GETFL 无第三参 */
 mov esi, 3 /* F_GETFL */
 xor eax, eax
 call fcntl@PLT
 test eax, eax
 js .Lsnb_fail
 mov edx, eax
 or edx, 0x800 /* O_NONBLOCK */
 mov edi, ebx
 mov esi, 4 /* F_SETFL */
 xor eax, eax
 call fcntl@PLT
 test eax, eax
 jnz .Lsnb_fail
 xor eax, eax
 pop rbx
 ret
.Lsnb_fail:
 mov eax, -1
 pop rbx
 ret
.size tui_li_set_nonblock, .-tui_li_set_nonblock

/* ---- 局部：fd >= 0 时 close ---- */
.type tui_li_close_if_open,@function
tui_li_close_if_open:
 test edi, edi
 js .Lcio_ret
 push rax
 call close@PLT
 pop rax
.Lcio_ret:
 ret
.size tui_li_close_if_open, .-tui_li_close_if_open

/* const char *tui_exit_reason_name(int reason) */
.globl tui_exit_reason_name
.type tui_exit_reason_name,@function
tui_exit_reason_name:
 .cfi_startproc
 lea rax, [rip + .Lname_unknown]
 cmp edi, TUI_LI_EXIT_EOF
 je .Lret_eof
 cmp edi, TUI_LI_EXIT_CTRL_C_IDLE
 je .Lret_ctrl_c
 cmp edi, TUI_LI_EXIT_SIGNAL
 je .Lret_signal
 cmp edi, TUI_LI_EXIT_POLL_ERROR
 je .Lret_poll_err
 ret
.Lret_eof:
 lea rax, [rip + .Lname_eof]
 ret
.Lret_ctrl_c:
 lea rax, [rip + .Lname_ctrl_c]
 ret
.Lret_signal:
 lea rax, [rip + .Lname_signal]
 ret
.Lret_poll_err:
 lea rax, [rip + .Lname_poll_err]
 ret
 .cfi_endproc
.size tui_exit_reason_name, .-tui_exit_reason_name

.section .note.GNU-stack,"",@progbits
