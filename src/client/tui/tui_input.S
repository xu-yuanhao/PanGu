/* tui_input.S — 一行 UTF-8 输入编辑（纯状态，无 I/O），手写汇编原地替换 tui_input.c。
 *
 * 契约见 tui_input.h：行尾编辑、整字符拒绝、^U 清行、^C/^D、ESC 序列吞掉
 * （CSI 参数上限 16 字节）；布局常量来自 tui_layout.h（编译期断言）。
 * SysV AMD64：跨 tui_utf8_* 调用的状态保存在 rbx/r12（callee-saved）。
 */
#include "tui_layout.h"
.intel_syntax noprefix
.section .text

/* void tui_input_init(tui_input *in) — 全状态清零（buf 内容不要求清空）。 */
.globl tui_input_init
.type tui_input_init,@function
tui_input_init:
 .cfi_startproc
 mov qword ptr [rdi + TUI_LI_INPUT_LEN], 0
 mov dword ptr [rdi + TUI_LI_INPUT_PENDING_NEED], 0
 mov dword ptr [rdi + TUI_LI_INPUT_REJECT_SKIP], 0
 mov dword ptr [rdi + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 mov dword ptr [rdi + TUI_LI_INPUT_ESC_STATE], 0
 mov dword ptr [rdi + TUI_LI_INPUT_ESC_COUNT], 0
 ret
 .cfi_endproc
.size tui_input_init, .-tui_input_init

/* void tui_input_reset(tui_input *in) — 提交后清空内容，不动 ESC 状态机。 */
.globl tui_input_reset
.type tui_input_reset,@function
tui_input_reset:
 .cfi_startproc
 mov qword ptr [rdi + TUI_LI_INPUT_LEN], 0
 mov dword ptr [rdi + TUI_LI_INPUT_PENDING_NEED], 0
 mov dword ptr [rdi + TUI_LI_INPUT_REJECT_SKIP], 0
 mov dword ptr [rdi + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 ret
 .cfi_endproc
.size tui_input_reset, .-tui_input_reset

/* const char *tui_input_text(const tui_input *in) */
.globl tui_input_text
.type tui_input_text,@function
tui_input_text:
 .cfi_startproc
 mov rax, rdi
 ret
 .cfi_endproc
.size tui_input_text, .-tui_input_text

/* size_t tui_input_len(const tui_input *in) */
.globl tui_input_len
.type tui_input_len,@function
tui_input_len:
 .cfi_startproc
 mov rax, qword ptr [rdi + TUI_LI_INPUT_LEN]
 ret
 .cfi_endproc
.size tui_input_len, .-tui_input_len

/* int tui_input_empty(const tui_input *in) */
.globl tui_input_empty
.type tui_input_empty,@function
tui_input_empty:
 .cfi_startproc
 xor eax, eax
 cmp qword ptr [rdi + TUI_LI_INPUT_LEN], 0
 sete al
 ret
 .cfi_endproc
.size tui_input_empty, .-tui_input_empty

/* tui_key_kind tui_input_feed(tui_input *in, unsigned char b) — rdi=in sil=b → eax */
.globl tui_input_feed
.type tui_input_feed,@function
tui_input_feed:
 .cfi_startproc
 push rbx
 push r12
 push rax /* 16 字节对齐占位：跨 tui_utf8_* 调用 */
 mov rbx, rdi
 movzx r12d, sil

 /* ---- esc_feed：处于转义序列时先消费该字节 ---- */
 mov eax, dword ptr [rbx + TUI_LI_INPUT_ESC_STATE]
 test eax, eax
 jnz .Lesc_active
 cmp r12b, 0x1B
 jne .Lnot_esc
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 1
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_COUNT], 0
 jmp .Lret_ignored
.Lesc_active:
 cmp eax, 1
 je .Lesc1
 cmp eax, 2
 je .Lesc2
 cmp eax, 3
 je .Lesc3
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0 /* 防呆：未知状态复位 */
 jmp .Lret_ignored
.Lesc1:
 xor eax, eax /* 默认 Meta+键：吞掉并回空闲 */
 cmp r12b, '['
 jne .Lesc1_not_csi
 mov eax, 2
 jmp .Lesc1_set
.Lesc1_not_csi:
 cmp r12b, 'O'
 jne .Lesc1_set
 mov eax, 3
.Lesc1_set:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], eax
 jmp .Lret_ignored
.Lesc2:
 inc dword ptr [rbx + TUI_LI_INPUT_ESC_COUNT]
 cmp dword ptr [rbx + TUI_LI_INPUT_ESC_COUNT], 16
 ja .Lesc2_give_up /* 参数超上限：坏序列放弃 */
 cmp r12b, 0x40
 jb .Lret_ignored
 cmp r12b, 0x7E
 ja .Lret_ignored
.Lesc2_give_up:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0 /* 结束字节 */
 jmp .Lret_ignored
.Lesc3:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 jmp .Lret_ignored
.Lnot_esc:

 /* ---- 被拒序列的剩余连续字节 ---- */
 cmp dword ptr [rbx + TUI_LI_INPUT_REJECT_SKIP], 0
 je .Lno_skip
 mov eax, r12d
 and eax, 0xC0
 cmp eax, 0x80
 jne .Lclear_skip
 dec dword ptr [rbx + TUI_LI_INPUT_REJECT_SKIP]
 jmp .Lret_ignored
.Lclear_skip:
 mov dword ptr [rbx + TUI_LI_INPUT_REJECT_SKIP], 0
.Lno_skip:

 /* ---- 多字节序列的后续字节直接追加 ---- */
 cmp dword ptr [rbx + TUI_LI_INPUT_PENDING_NEED], 0
 je .Lno_pending
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 mov [rbx + rcx], r12b
 inc rcx
 mov qword ptr [rbx + TUI_LI_INPUT_LEN], rcx
 dec dword ptr [rbx + TUI_LI_INPUT_PENDING_NEED]
 jmp .Lret_char
.Lno_pending:

 /* ---- 控制键分派 ---- */
 cmp r12b, 0x0D
 je .Lenter
 cmp r12b, 0x0A
 je .Lenter
 cmp r12b, 0x08
 je .Lbackspace
 cmp r12b, 0x7F
 je .Lbackspace
 cmp r12b, 0x03
 je .Lret_ctrl_c
 cmp r12b, 0x04
 je .Lctrl_d
 cmp r12b, 0x15
 je .Lctrl_u
 cmp r12b, 0x09
 je .Lret_ignored
 cmp r12b, 0x20
 jae .Lprintable
 jmp .Lret_ignored /* 其余控制字节忽略 */

.Lenter:
 mov eax, TUI_LI_KEY_ENTER
 jmp .Lout
.Lbackspace:
 cmp qword ptr [rbx + TUI_LI_INPUT_LEN], 0
 je .Lret_ignored /* 空行退格无效 */
 jmp .Ldel_last_cp
.Lctrl_d:
 cmp qword ptr [rbx + TUI_LI_INPUT_LEN], 0
 jne .Ldel_last_cp /* 非空 ^D：readline 退格 */
 mov eax, TUI_LI_KEY_CTRL_D
 jmp .Lout
.Ldel_last_cp:
 mov rdi, rbx /* len = last_seq_start(buf, len) */
 mov rsi, qword ptr [rbx + TUI_LI_INPUT_LEN]
 call tui_utf8_last_seq_start@PLT
 mov qword ptr [rbx + TUI_LI_INPUT_LEN], rax
 mov eax, TUI_LI_KEY_BACKSPACE
 jmp .Lout
.Lctrl_u:
 mov qword ptr [rbx + TUI_LI_INPUT_LEN], 0
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 mov eax, TUI_LI_KEY_CHAR
 jmp .Lout

 /* ---- 可打印/UTF-8 首字节：整个序列放得下才追加 ---- */
.Lprintable:
 mov eax, r12d
 and eax, 0xC0
 cmp eax, 0x80
 je .Lret_ignored /* 序列外孤立连续字节：按行为吞掉 */
 movzx edi, r12b
 call tui_utf8_seq_len@PLT /* rax = need（1..4） */
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 lea rdx, [rcx + rax]
 cmp rdx, TUI_LI_INPUT_MAX_BYTES
 ja .Lreject_seq
 mov [rbx + rcx], r12b
 lea rcx, [rcx + 1]
 mov qword ptr [rbx + TUI_LI_INPUT_LEN], rcx
 dec eax
 mov dword ptr [rbx + TUI_LI_INPUT_PENDING_NEED], eax
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 jmp .Lret_char
.Lreject_seq:
 dec eax
 mov dword ptr [rbx + TUI_LI_INPUT_REJECT_SKIP], eax /* 吞掉剩余连续字节 */
 mov eax, dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH]
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 1
 test eax, eax
 jnz .Lret_ignored /* 节流：仍满期间只报一次 */
 mov eax, TUI_LI_KEY_OVERFLOW
 jmp .Lout

.Lret_ctrl_c:
 mov eax, TUI_LI_KEY_CTRL_C
 jmp .Lout
.Lret_char:
 mov eax, TUI_LI_KEY_CHAR
 jmp .Lout
.Lret_ignored:
 mov eax, TUI_LI_KEY_IGNORED
.Lout:
 pop rcx /* 弹出对齐占位：rcx 为 caller-saved 死寄存器，保护 eax 返回值 */
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_input_feed, .-tui_input_feed

.section .note.GNU-stack,"",@progbits
