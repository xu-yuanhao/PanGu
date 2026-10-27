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

/* 主循环栈帧布局（rsp 相对，sub APP_FRAME 后）。
 * 缓冲区偏移从 tui_layout.h 的实际结构体尺寸派生：view 内嵌 md 后尺寸大增，
 * 手写偏移曾停留在旧 64 字节 view，view_init 越界打穿调用者栈帧。 */
.equ OFF_TERM,    0                                  /* tui_term */
.equ OFF_INPUT,   OFF_TERM + TUI_LI_TERM_SIZE        /* tui_input */
.equ OFF_VIEW,    OFF_INPUT + TUI_LI_INPUT_SIZE      /* tui_view（内嵌 md） */
.equ OFF_STATE,   OFF_VIEW + TUI_LI_VIEW_SIZE        /* tui_state */
.equ OFF_SINK,    OFF_STATE + TUI_LI_STATE_SIZE      /* tui_out_sink 16 */
.equ OFF_WAKE,    OFF_SINK + 16                      /* int wake[2] */
.equ OFF_SAVED,   OFF_WAKE + 8                       /* struct sigaction saved[4] */
.equ OFF_SA,      OFF_SAVED + 4 * TUI_LI_SIGACTION_SIZE
.equ OFF_PFDS,    OFF_SA + TUI_LI_SIGACTION_SIZE     /* struct pollfd[2] 16 */
.equ OFF_BUF,     OFF_PFDS + 16                      /* unsigned char buf[256] */
.equ OFF_REASON,  OFF_BUF + 256
.equ OFF_TICK,    OFF_REASON + 4
.equ OFF_COUNT,   OFF_TICK + 4
.equ OFF_INTERACTIVE, OFF_COUNT + 4
.equ OFF_K,       OFF_INTERACTIVE + 4                /* size_t，8 对齐 */
.equ OFF_N,       OFF_K + 8
.equ OFF_ESC_DEADLINE, OFF_N + 8
.equ OFF_ESC_POLL, OFF_ESC_DEADLINE + 8
.equ APP_FRAME,   (OFF_ESC_POLL + 4 + 15) & -16

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
.Lcmd_hotkeys: .ascii "/hotkeys"
.Lcmd_quit: .ascii "/quit"
.Lname_quit: .asciz "/quit 退出"

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
 mov qword ptr [rsp + OFF_ESC_DEADLINE], 0
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
 /* 初始编辑区宽度取实际终端列（term_init 已探测） */
 mov rdi, r14
 movzx esi, word ptr [r12 + TUI_LI_TERM_COLS]
 call tui_view_set_size@PLT
 movzx eax, word ptr [r12 + TUI_LI_TERM_ROWS]
 mov [r14 + TUI_LI_VIEW_TERMINAL_ROWS], eax
 cmp eax, 4
 jae .Lread_rows_normal
 mov eax, 4
.Lread_rows_normal:
 sub eax, 3
 cmp eax, 10
 jbe .Lread_rows_set
 mov eax, 10
.Lread_rows_set:
 mov [r14 + TUI_LI_VIEW_PAGE_ROWS], eax
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
 /* 列数变化：收束旧编辑区（旧几何不可靠）后按新宽度重绘 */
 mov rdi, r14
 movzx esi, word ptr [r12 + TUI_LI_TERM_COLS]
 call tui_view_set_size@PLT
 movzx eax, word ptr [r12 + TUI_LI_TERM_ROWS]
 mov [r14 + TUI_LI_VIEW_TERMINAL_ROWS], eax
 cmp eax, 4
 jae .Lread_resize_rows_normal
 mov eax, 4
.Lread_resize_rows_normal:
 sub eax, 3
 cmp eax, 10
 jbe .Lread_resize_rows_set
 mov eax, 10
.Lread_resize_rows_set:
 mov [r14 + TUI_LI_VIEW_PAGE_ROWS], eax
 cmp dword ptr [r14 + TUI_LI_VIEW_READING], 0
 je .Lread_resize_done
 mov rdi, r14
 xor esi, esi
 call tui_view_page@PLT
.Lread_resize_done:
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
 mov [rsp + OFF_ESC_POLL], edx
 cmp dword ptr [r13 + TUI_LI_INPUT_PASTE], 0
 jne .Lescape_timer_off
 cmp dword ptr [r13 + TUI_LI_INPUT_ESC_STATE], 1
 jne .Lescape_timer_off
 mov eax, [r13 + TUI_LI_INPUT_SEARCH_ACTIVE]
 or eax, [r13 + TUI_LI_INPUT_COMPLETION_OPEN]
 or eax, [r14 + TUI_LI_VIEW_READING]
 jz .Lescape_timer_off
 call tui_li_milliseconds
 mov rcx, [rsp + OFF_ESC_DEADLINE]
 test rcx, rcx
 jnz .Lescape_deadline_set
 lea rcx, [rax + 10] /* Pi stdin-buffer default Escape timeout */
 mov [rsp + OFF_ESC_DEADLINE], rcx
.Lescape_deadline_set:
 sub rcx, rax
 jns .Lescape_remaining
 xor ecx, ecx
.Lescape_remaining:
 mov edx, [rsp + OFF_ESC_POLL]
 test edx, edx
 js .Lescape_timeout_use
 cmp ecx, edx
 jae .Lescape_poll
.Lescape_timeout_use:
 mov edx, ecx
 jmp .Lescape_poll
.Lescape_timer_off:
 mov qword ptr [rsp + OFF_ESC_DEADLINE], 0
 mov edx, [rsp + OFF_ESC_POLL]
.Lescape_poll:
 lea rdi, [rsp + OFF_PFDS]
 mov esi, 2
 call poll@PLT
 test eax, eax
 js .Lpoll_errno
 jnz .Lpoll_has_events
 cmp qword ptr [rsp + OFF_ESC_DEADLINE], 0
 je .Lpump
 call tui_li_milliseconds
 cmp rax, [rsp + OFF_ESC_DEADLINE]
 jb .Lpump
 mov qword ptr [rsp + OFF_ESC_DEADLINE], 0
 cmp dword ptr [r14 + TUI_LI_VIEW_READING], 0
 je .Lescape_input_control
 mov dword ptr [r13 + TUI_LI_INPUT_ESC_STATE], 0
 mov rdi, r14
 call tui_view_return_latest@PLT
 jmp .Lpump
.Lescape_input_control:
 mov rdi, r13
 call tui_input_escape_timeout@PLT
 cmp eax, TUI_LI_KEY_CURSOR
 jne .Lpump
 mov rdi, r14
 call tui_view_redraw_editor@PLT
 jmp .Lpump
.Lpoll_has_events:
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
 cmp dword ptr [r14 + TUI_LI_VIEW_READING], 0
 je .Lread_feed_byte
 cmp dword ptr [r13 + TUI_LI_INPUT_PASTE], 0
 jne .Lread_feed_byte
 cmp esi, 3
 je .Lkey_read_return
 cmp esi, 4
 je .Lkey_read_return
 cmp esi, 13
 je .Lkey_read_return
 cmp esi, 10
 je .Lkey_read_return
.Lread_feed_byte:
 mov rdi, r13
 call tui_input_feed@PLT
 cmp eax, TUI_LI_KEY_PAGE_UP
 je .Lkey_page_up
 cmp eax, TUI_LI_KEY_PAGE_DOWN
 je .Lkey_page_down
 cmp dword ptr [r14 + TUI_LI_VIEW_READING], 0
 je .Lkey_normal
 cmp eax, TUI_LI_KEY_ENTER
 je .Lkey_read_return
 cmp eax, TUI_LI_KEY_CTRL_C
 je .Lkey_read_return
 cmp eax, TUI_LI_KEY_CTRL_D
 je .Lkey_read_return
 cmp eax, TUI_LI_KEY_IGNORED
 je .Lkey_next
 cmp eax, TUI_LI_KEY_NONE
 je .Lkey_next
 mov [rsp + OFF_ESC_POLL], eax
 mov rdi, r14
 call tui_view_return_latest@PLT
 mov eax, [rsp + OFF_ESC_POLL]
 jmp .Lkey_normal
.Lkey_normal:
 cmp eax, TUI_LI_KEY_ENTER
 je .Lkey_enter
 cmp eax, TUI_LI_KEY_CTRL_C
 je .Lkey_ctrl_c
 cmp eax, TUI_LI_KEY_CTRL_D
 je .Lkey_ctrl_d
 cmp eax, TUI_LI_KEY_SEARCH
 je .Lkey_search
 cmp eax, TUI_LI_KEY_COMPLETIONS
 je .Lkey_completions
 cmp eax, TUI_LI_KEY_HELP
 je .Lkey_help
 cmp eax, TUI_LI_KEY_CTRL_O
 je .Lkey_ctrl_o
 cmp eax, TUI_LI_KEY_OVERFLOW
 je .Lkey_overflow
 cmp eax, TUI_LI_KEY_CHAR
 je .Lkey_char_redraw
 cmp eax, TUI_LI_KEY_BACKSPACE
 je .Lkey_redraw
 cmp eax, TUI_LI_KEY_CURSOR
 je .Lkey_redraw
 cmp eax, TUI_LI_KEY_PASTE
 je .Lkey_redraw
 jmp .Lkey_next
.Lkey_page_up:
 mov esi, -1
 jmp .Lkey_page
.Lkey_page_down:
 mov esi, 1
.Lkey_page:
 mov rdi, r14
 call tui_view_page@PLT
 jmp .Lkey_next
.Lkey_read_return:
 mov rdi, r14
 call tui_view_return_latest@PLT
 jmp .Lkey_next
.Lkey_enter:
 mov rdi, r13
 call tui_input_empty@PLT
 test eax, eax
 jnz .Lkey_next /* 空提交由上层忽略 */
 mov rdi, r13
 call tui_input_incomplete@PLT
 test eax, eax
 jnz .Lkey_next /* 未完成粘贴/转义不提交（防御；feed 契约已保证） */
 /* Exact local command dispatch precedes Runtime submit (Pi interactive-mode).
  * No prefix matching: multiline examples and /hotkeysX remain model text. */
 cmp qword ptr [r13 + TUI_LI_INPUT_LEN], 5
 jne .Lkey_check_hotkeys
 mov rdi, r13
 lea rsi, [rip + .Lcmd_quit]
 mov edx, 5
 call memcmp@PLT
 test eax, eax
 jnz .Lkey_submit
 mov rdi, r14
 call tui_view_finalize_editor@PLT
 mov rdi, r13
 call tui_input_reset@PLT
 mov dword ptr [rsp + OFF_REASON], TUI_LI_EXIT_COMMAND
 jmp .Ldone
.Lkey_check_hotkeys:
 cmp qword ptr [r13 + TUI_LI_INPUT_LEN], 8
 jne .Lkey_submit
 mov rdi, r13
 lea rsi, [rip + .Lcmd_hotkeys]
 mov edx, 8
 call memcmp@PLT
 test eax, eax
 jnz .Lkey_submit
 mov rdi, r14
 call tui_view_finalize_editor@PLT
 mov rdi, r13
 call tui_input_remember@PLT
 mov rdi, r13
 call tui_input_reset@PLT
 mov rdi, r14
 call tui_view_help@PLT
 jmp .Lkey_next
.Lkey_submit:
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
 mov rsi, r13
 mov rdx, [r13 + TUI_LI_INPUT_LEN]
 call tui_view_user@PLT
 mov rdi, r14
 call tui_view_finalize_editor@PLT
 mov rdi, r13
 call tui_input_remember@PLT
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
.Lkey_search:
 mov rdi, r14
 call tui_view_history_search@PLT
 jmp .Lkey_next
.Lkey_completions:
 mov rdi, r14
 call tui_view_completions@PLT
 jmp .Lkey_next
.Lkey_help:
 mov rdi, r14
 call tui_view_help@PLT
 jmp .Lkey_next
.Lkey_ctrl_o:
 /* Pi app.tools.expand（ctrl+o）：展开当前保留工具调用的保留输出 */
 mov rdi, r14
 call tui_view_tool_expand@PLT
 jmp .Lkey_next
.Lkey_overflow:
 mov rdi, r14
 call tui_view_input_limit@PLT
 jmp .Lkey_next
.Lkey_char_redraw:
 /* 多字节序列未完成：等最后一字节到齐再整字重绘（Pi editor 以码点为编辑单位） */
 cmp dword ptr [r13 + TUI_LI_INPUT_UTF8_PENDING], 0
 jne .Lkey_next
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

/* CLOCK_MONOTONIC host clock; scoped control deadlines only. */
.type tui_li_milliseconds,@function
tui_li_milliseconds:
 sub rsp, 24
 mov edi, 1
 mov rsi, rsp
 call clock_gettime@PLT
 test eax, eax
 jne .Lclock_failure
 mov rax, [rsp]
 imul rax, rax, 1000
 mov rcx, rax
 mov rax, [rsp + 8]
 xor edx, edx
 mov esi, 1000000
 div rsi
 add rax, rcx
 jmp .Lclock_done
.Lclock_failure:
 mov rax, -1
.Lclock_done:
 add rsp, 24
 ret
.size tui_li_milliseconds,.-tui_li_milliseconds

/* ---- 局部：shutdown（cfg 在 rbx，由主函数持有） ---- */
.type tui_li_shutdown,@function
tui_li_shutdown:
 sub rsp,8
.Lsd_again:
 mov rax, qword ptr [rbx + TUI_LI_APP_OPS]
 mov rdi, qword ptr [rax + TUI_LI_OPS_CTX]
 mov rax, qword ptr [rax + TUI_LI_OPS_SHUTDOWN]
 test rax, rax
 jz .Lsd_ret
 call rax
 test eax,eax
 jz .Lsd_ret
 xor edi,edi
 xor esi,esi
 mov edx,10
 call poll@PLT
 jmp .Lsd_again
.Lsd_ret:
 add rsp,8
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
 cmp edi, TUI_LI_EXIT_COMMAND
 je .Lret_command
 cmp edi, TUI_LI_EXIT_EOF
 je .Lret_eof
 cmp edi, TUI_LI_EXIT_CTRL_C_IDLE
 je .Lret_ctrl_c
 cmp edi, TUI_LI_EXIT_SIGNAL
 je .Lret_signal
 cmp edi, TUI_LI_EXIT_POLL_ERROR
 je .Lret_poll_err
 ret
.Lret_command:
 lea rax, [rip + .Lname_quit]
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
