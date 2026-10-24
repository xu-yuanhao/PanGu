/* tui_term.S — 终端组合原语，手写汇编（自 tui_term.c 拆出）。
 *
 * 承担：struct tui_term 初始化/TTY 探测、平台表分派、循环写满（EINTR 重试）。
 * termios/winsize 平台结构适配保留在 tui_term.c（c-bridge/platform-layout）。
 * 平台表经 GOTPCREL 取址，兼容 PIE/非 PIE 链接。
 */
#include "tui_layout.h"
.intel_syntax noprefix
.section .text

/* void tui_term_init(tui_term *t, int in_fd, int out_fd) — rdi rsi edx */
.globl tui_term_init
.type tui_term_init,@function
tui_term_init:
 .cfi_startproc
 push rbx
 push r12
 push rax
 mov rbx, rdi
 mov dword ptr [rbx + TUI_LI_TERM_IN_FD], esi
 mov dword ptr [rbx + TUI_LI_TERM_OUT_FD], edx
 /* saved/saved_valid/raw_active/tty 标志清零（偏移 8..95，88 字节） */
 lea rdi, [rbx + 8]
 xor eax, eax
 mov ecx, 11
 cld
 rep stosq
 mov rax, qword ptr [rip + tui_term_linux_platform@GOTPCREL]
 mov qword ptr [rbx + TUI_LI_TERM_PLATFORM], rax
 mov word ptr [rbx + TUI_LI_TERM_COLS], 80
 mov word ptr [rbx + TUI_LI_TERM_ROWS], 24
 mov edi, dword ptr [rbx + TUI_LI_TERM_IN_FD]
 call isatty@PLT
 cmp eax, 1
 sete al
 movzx eax, al
 mov dword ptr [rbx + TUI_LI_TERM_IN_IS_TTY], eax
 mov edi, dword ptr [rbx + TUI_LI_TERM_OUT_FD]
 call isatty@PLT
 cmp eax, 1
 sete al
 movzx eax, al
 mov dword ptr [rbx + TUI_LI_TERM_OUT_IS_TTY], eax
 mov rdi, rbx
 mov rax, qword ptr [rbx + TUI_LI_TERM_PLATFORM]
 call [rax + TUI_LI_PLATFORM_QUERY_SIZE]
 pop rax
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_term_init, .-tui_term_init

/* int tui_term_enter_interactive(tui_term *t) — 两端都是 TTY 才进入 raw */
.globl tui_term_enter_interactive
.type tui_term_enter_interactive,@function
tui_term_enter_interactive:
 .cfi_startproc
 cmp dword ptr [rdi + TUI_LI_TERM_IN_IS_TTY], 0
 je .Lei_fail
 cmp dword ptr [rdi + TUI_LI_TERM_OUT_IS_TTY], 0
 je .Lei_fail
 push rbx
 push r12
 push rax
 mov rbx, rdi
 mov rdi, rbx
 mov rax, qword ptr [rbx + TUI_LI_TERM_PLATFORM]
 call [rax + TUI_LI_PLATFORM_ENTER_RAW]
 test eax, eax
 jnz .Lei_fail_pop
 mov rdi, rbx
 mov rax, qword ptr [rbx + TUI_LI_TERM_PLATFORM]
 call [rax + TUI_LI_PLATFORM_QUERY_SIZE]
 xor eax, eax
 pop rcx /* 对齐占位弹入死寄存器，保护 eax 返回值 */
 pop r12
 pop rbx
 ret
.Lei_fail_pop:
 mov eax, -1
 pop rax
 pop r12
 pop rbx
.Lei_fail:
 mov eax, -1
 ret
 .cfi_endproc
.size tui_term_enter_interactive, .-tui_term_enter_interactive

/* void tui_term_restore(tui_term *t) — 可重复调用；经平台表还原 */
.globl tui_term_restore
.type tui_term_restore,@function
tui_term_restore:
 .cfi_startproc
 cmp dword ptr [rdi + TUI_LI_TERM_RAW_ACTIVE], 0
 jne .Lttr_do
 cmp dword ptr [rdi + TUI_LI_TERM_SAVED_VALID], 0
 je .Lttr_ret
.Lttr_do:
 mov rax, qword ptr [rdi + TUI_LI_TERM_PLATFORM]
 jmp qword ptr [rax + TUI_LI_PLATFORM_RESTORE]
.Lttr_ret:
 ret
 .cfi_endproc
.size tui_term_restore, .-tui_term_restore

/* int tui_term_write(tui_term *t, const char *data, size_t len) — 循环写满 */
.globl tui_term_write
.type tui_term_write,@function
tui_term_write:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 xor r14d, r14d /* off */
.Ltw_loop:
 cmp r14, r13
 jae .Ltw_ok
 mov edi, dword ptr [rbx + TUI_LI_TERM_OUT_FD]
 lea rsi, [r12 + r14]
 mov rdx, r13
 sub rdx, r14
 call write@PLT
 test rax, rax
 js .Ltw_err
 jz .Ltw_fail /* n == 0：无法推进 */
 add r14, rax
 jmp .Ltw_loop
.Ltw_err:
 call __errno_location@PLT
 cmp dword ptr [rax], TUI_LI_EINTR
 je .Ltw_loop
.Ltw_fail:
 mov eax, -1
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Ltw_ok:
 xor eax, eax
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_term_write, .-tui_term_write

/* int tui_term_is_interactive(const tui_term *t) */
.globl tui_term_is_interactive
.type tui_term_is_interactive,@function
tui_term_is_interactive:
 .cfi_startproc
 mov eax, dword ptr [rdi + TUI_LI_TERM_RAW_ACTIVE]
 ret
 .cfi_endproc
.size tui_term_is_interactive, .-tui_term_is_interactive

.section .note.GNU-stack,"",@progbits
