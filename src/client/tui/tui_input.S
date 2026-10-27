/* tui_input.S — UTF-8 多行编辑、分片按键解析与安全括号粘贴，手写汇编。
 * 不执行终端 I/O；未知转义整序列安全消费（CAN/SUB/Ctrl-C 可中止），
 * 不吞掉其后的正常文本。粘贴期间不产生提交/取消/退出/光标动作；
 * CRLF/CR 折叠为 LF，控制字节过滤，溢出仍消费到结束标记。
 * SysV AMD64：in 指针在 rbx，当前字节在 r12b。
 */
#include "tui_layout.h"
.intel_syntax noprefix
.section .text

/* void tui_input_init(tui_input *in) — 清 len..结构尾（56 字节，buf 不要求清空） */
.globl tui_input_init
.type tui_input_init,@function
tui_input_init:
 .cfi_startproc
 lea rdi, [rdi + TUI_LI_INPUT_LEN]
 xor eax, eax
 mov ecx, 7
 cld
 rep stosq
 ret
 .cfi_endproc
.size tui_input_init, .-tui_input_init

/* 提交后清草稿；粘贴/转义激活时调用方不得提交（Enter 只在普通模式产生）。 */
.globl tui_input_reset
.type tui_input_reset,@function
tui_input_reset:
 .cfi_startproc
 mov qword ptr [rdi + TUI_LI_INPUT_LEN], 0
 mov qword ptr [rdi + TUI_LI_INPUT_CURSOR], 0
 mov dword ptr [rdi + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 mov dword ptr [rdi + TUI_LI_INPUT_UTF8_PENDING], 0
 mov dword ptr [rdi + TUI_LI_INPUT_PASTE_REJECT], 0
 mov dword ptr [rdi + TUI_LI_INPUT_ENTER_CR], 0
 ret
 .cfi_endproc
.size tui_input_reset, .-tui_input_reset

.globl tui_input_text
.type tui_input_text,@function
tui_input_text:
 .cfi_startproc
 mov rax, rdi
 ret
 .cfi_endproc
.size tui_input_text, .-tui_input_text

.globl tui_input_len
.type tui_input_len,@function
tui_input_len:
 .cfi_startproc
 mov rax, qword ptr [rdi + TUI_LI_INPUT_LEN]
 ret
 .cfi_endproc
.size tui_input_len, .-tui_input_len

.globl tui_input_cursor
.type tui_input_cursor,@function
tui_input_cursor:
 .cfi_startproc
 mov rax, qword ptr [rdi + TUI_LI_INPUT_CURSOR]
 ret
 .cfi_endproc
.size tui_input_cursor, .-tui_input_cursor

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

/* int tui_input_incomplete(const tui_input *in)：粘贴或转义序列尚未完成 */
.globl tui_input_incomplete
.type tui_input_incomplete,@function
tui_input_incomplete:
 .cfi_startproc
 xor eax, eax
 cmp dword ptr [rdi + TUI_LI_INPUT_PASTE], 0
 jne .Lincomplete_yes
 cmp dword ptr [rdi + TUI_LI_INPUT_ESC_STATE], 0
 jne .Lincomplete_yes
 ret
.Lincomplete_yes:
 mov eax, 1
 ret
 .cfi_endproc
.size tui_input_incomplete, .-tui_input_incomplete

/* 插入 r12b 到光标处；完整 UTF-8 字符在最后一字节才移动光标。
 * rax：0 成功，-1 容量不足。栈对齐：入口 ≡8，sub 8 后调用 libc。 */
.type li_insert,@function
li_insert:
 sub rsp, 8
 mov rax, qword ptr [rbx + TUI_LI_INPUT_LEN]
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 cmp rax, TUI_LI_INPUT_MAX_BYTES - 1
 jae .Linsert_fail
 lea rdi, [rbx + rcx + 1]
 lea rsi, [rbx + rcx]
 mov rdx, rax
 sub rdx, rcx
 call memmove@PLT
 mov rax, qword ptr [rbx + TUI_LI_INPUT_LEN]
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 mov [rbx + rcx], r12b
 inc rax
 mov qword ptr [rbx + TUI_LI_INPUT_LEN], rax
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 cmp dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 jne .Linsert_continuation
 movzx edi, r12b
 call tui_utf8_seq_len@PLT
 dec eax
 mov dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], eax
 /* 光标是字节偏移：每个已插入字节都前移，多字节拆分输入仍保序；
  * utf8_pending 只约束下一字节必须为连续字节。 */
 inc qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 xor eax, eax
 add rsp, 8
 ret
.Linsert_continuation:
 dec dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING]
 inc qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 xor eax, eax
 add rsp, 8
 ret
.Linsert_fail:
 mov eax, -1
 add rsp, 8
 ret
.size li_insert, .-li_insert

/* 溢出提示节流：同一连续溢出期间只报一次 OVERFLOW。 */
.type li_overflow,@function
li_overflow:
 cmp dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 jne .Loverflow_ignored
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 1
 mov eax, TUI_LI_KEY_OVERFLOW
 ret
.Loverflow_ignored:
 mov eax, TUI_LI_KEY_IGNORED
 ret
.size li_overflow, .-li_overflow

/* Backspace：删除光标前一个 code point（start..cursor），光标落到 start。 */
.type li_backspace,@function
li_backspace:
 push r13
 push r14
 sub rsp, 8
 mov r13, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 test r13, r13
 jz .Lback_noop
 mov rdi, rbx
 mov rsi, r13
 call tui_utf8_last_seq_start@PLT
 mov r14, rax /* 被删字符起始 */
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 lea rdi, [rbx + r14]
 lea rsi, [rbx + r13]
 mov rdx, rcx
 sub rdx, r13
 call memmove@PLT
 mov rax, r13
 sub rax, r14 /* 删除字节数 */
 sub qword ptr [rbx + TUI_LI_INPUT_LEN], rax
 mov [rbx + TUI_LI_INPUT_CURSOR], r14
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 mov dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0 /* 半字符恰在 cursor 前，一并删除 */
 mov eax, TUI_LI_KEY_BACKSPACE
 add rsp, 8
 pop r14
 pop r13
 ret
.Lback_noop:
 mov eax, TUI_LI_KEY_IGNORED
 add rsp, 8
 pop r14
 pop r13
 ret
.size li_backspace, .-li_backspace

/* Delete：删除光标后一个 code point（cursor..cursor+seq）。
 * 钳制到 len：粘贴截断可能留下不完整序列尾。成功时清 utf8_pending
 * （不变式：pending>0 ⇒ 半字符恰在 cursor 处）。 */
.type li_delete,@function
li_delete:
 push r13
 push r14
 sub rsp, 8
 mov r13, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 cmp r13, qword ptr [rbx + TUI_LI_INPUT_LEN]
 jae .Ldel_noop
 movzx edi, byte ptr [rbx + r13]
 call tui_utf8_seq_len@PLT
 mov r14, rax
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 sub rcx, r13
 cmp r14, rcx
 jbe .Ldel_len_ok
 mov r14, rcx
.Ldel_len_ok:
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 sub rcx, r13
 sub rcx, r14
 lea rdi, [rbx + r13]
 mov rsi, rdi
 add rsi, r14
 mov rdx, rcx
 call memmove@PLT
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 sub rcx, r14
 mov qword ptr [rbx + TUI_LI_INPUT_LEN], rcx
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 mov dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 mov eax, TUI_LI_KEY_CHAR
 add rsp, 8
 pop r14
 pop r13
 ret
.Ldel_noop:
 mov eax, TUI_LI_KEY_IGNORED
 add rsp, 8
 pop r14
 pop r13
 ret
.size li_delete, .-li_delete

/* 粘贴内容消费（rbx=in，当前字节 r12b，返回键值）：
 *  1) 被拒整字符的剩余连续字节吞到序列完；
 *  2) 候选 ESC[201~ 增量匹配（任意分片），未匹配前缀整体丢弃；
 *  3) CR 折叠：CRLF→单 LF，孤立 CR 也补成 LF（容量不足按溢出处理）；
 *  4) 其余控制字节过滤，可打印/多字节内容整字符插入，放不下整字符拒绝。 */
.type li_paste,@function
li_paste:
 cmp dword ptr [rbx + TUI_LI_INPUT_PASTE_REJECT], 0
 je .Lp_match
 mov eax, r12d
 and eax, 0xC0
 cmp eax, 0x80
 jne .Lp_reject_done
 dec dword ptr [rbx + TUI_LI_INPUT_PASTE_REJECT]
 jmp .Lpaste_ignored
.Lp_reject_done:
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_REJECT], 0
.Lp_match:
 mov ecx, dword ptr [rbx + TUI_LI_INPUT_PASTE_MATCH]
 test ecx, ecx
 jz .Lp_no_match
 lea rax, [rip + .Lpaste_end]
 movzx eax, byte ptr [rax + rcx]
 cmp r12b, al
 jne .Lp_match_fail
 inc ecx
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_MATCH], ecx
 cmp ecx, 6
 je .Lp_end
 jmp .Lpaste_ignored
.Lp_match_fail:
 /* 已缓冲的未匹配前缀整体丢弃；当前字节重新按内容处理 */
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_MATCH], 0
.Lp_no_match:
 cmp r12b, 0x1B
 jne .Lp_content_entry
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_MATCH], 1
 jmp .Lpaste_ignored
.Lp_content_entry:
 cmp dword ptr [rbx + TUI_LI_INPUT_PASTE_CR], 0
 je .Lp_content
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_CR], 0
 cmp r12b, 10
 je .Lp_crlf_lf
 push r12
 mov r12b, 10
 call li_insert
 pop r12
 test eax, eax
 jnz li_overflow /* 折叠 LF 放不下：当前字节一并丢弃 */
 jmp .Lp_content
.Lp_crlf_lf:
 call li_insert
 test eax, eax
 jnz li_overflow
 jmp .Lpaste_ignored
.Lp_content:
 cmp r12b, 13
 jne .Lp_not_cr
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_CR], 1
 jmp .Lpaste_ignored
.Lp_not_cr:
 cmp r12b, 10
 je .Lp_insert
 cmp r12b, 32
 jb .Lpaste_ignored /* 控制字节过滤（含 ^C/^D/ESC 序列字节） */
 cmp r12b, 127
 je .Lpaste_ignored
 cmp r12b, 128
 jb .Lp_ascii
 cmp dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 jne .Lp_cont
 mov eax, r12d
 and eax, 0xC0
 cmp eax, 0x80
 je .Lpaste_ignored /* 孤立连续字节过滤 */
 movzx edi, r12b
 call tui_utf8_seq_len@PLT
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 add rcx, rax
 cmp rcx, TUI_LI_INPUT_MAX_BYTES - 1
 ja .Lp_reject_lead
 jmp .Lp_insert
.Lp_cont:
 mov eax, r12d
 and eax, 0xC0
 cmp eax, 0x80
 jne .Lpaste_ignored
 jmp .Lp_insert
.Lp_ascii:
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 cmp rcx, TUI_LI_INPUT_MAX_BYTES - 1
 jae li_overflow
.Lp_insert:
 call li_insert
 test eax, eax
 jnz li_overflow
.Lpaste_ignored:
 mov eax, TUI_LI_KEY_IGNORED
 ret
.Lp_reject_lead:
 /* 整字符拒绝：剩余连续字节仍要吞掉，溢出提示走节流 */
 dec eax
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_REJECT], eax
 jmp li_overflow
.Lp_end:
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE], 0
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_MATCH], 0
 cmp dword ptr [rbx + TUI_LI_INPUT_PASTE_CR], 0
 je .Lp_end_ok
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_CR], 0
 push r12
 mov r12b, 10
 call li_insert
 pop r12
 test eax, eax
 jnz li_overflow /* 尾部折叠 LF 溢出：粘贴仍结束，提示已节流 */
.Lp_end_ok:
 mov eax, TUI_LI_KEY_PASTE
 ret
.size li_paste, .-li_paste

.section .rodata
.Lpaste_end: .ascii "\033[201~"
.section .text

/* tui_key_kind tui_input_feed(tui_input *in, unsigned char b) — rdi=in sil=b → eax */
.globl tui_input_feed
.type tui_input_feed,@function
tui_input_feed:
 .cfi_startproc
 push rbx
 push r12
 push r13
 mov rbx, rdi
 movzx r12d, sil
 cmp dword ptr [rbx + TUI_LI_INPUT_PASTE], 0
 jne .Lfeed_paste
 mov eax, dword ptr [rbx + TUI_LI_INPUT_ESC_STATE]
 test eax, eax
 jnz .Lfeed_esc
 cmp r12b, 0x1B
 jne .Lfeed_plain
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 1
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM], 0
 mov dword ptr [rbx + TUI_LI_INPUT_ENTER_CR], 0
 jmp .Lfeed_ignored

.Lfeed_paste:
 call li_paste
 jmp .Lfeed_out

/* ---- 转义序列：Ctrl-C 任意时刻可中止；CAN/SUB 静默中止 ---- */
.Lfeed_esc:
 cmp eax, 1
 je .Lesc_one
 cmp eax, 2
 je .Lesc_csi
 cmp eax, 3
 je .Lesc_ss3
 cmp eax, 4
 je .Lesc_string
 cmp eax, 5
 je .Lesc_st
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 jmp .Lfeed_ignored
.Lesc_abort_ctrl_c:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 mov eax, TUI_LI_KEY_CTRL_C
 jmp .Lfeed_out
.Lesc_abort:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 jmp .Lfeed_ignored
.Lesc_restart:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 1
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM], 0
 jmp .Lfeed_ignored
.Lesc_one:
 cmp r12b, 0x03
 je .Lesc_abort_ctrl_c
 cmp r12b, 0x1B
 je .Lfeed_ignored /* ESC ESC：保持状态 1，重新等待下一字节 */
 cmp r12b, '['
 je .Lesc_start_csi
 cmp r12b, 'O'
 je .Lesc_start_ss3
 cmp r12b, ']'
 je .Lesc_start_string
 cmp r12b, 'P'
 je .Lesc_start_string
 cmp r12b, '_'
 je .Lesc_start_string
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 cmp r12b, 13
 je .Lesc_alt_newline
 cmp r12b, 10
 je .Lesc_alt_newline
 jmp .Lfeed_ignored /* Meta+键：整组合吞掉 */
.Lesc_alt_newline:
 mov r12b, 10
 call li_insert
 test eax, eax
 jnz .Lfeed_overflow
 mov eax, TUI_LI_KEY_CHAR
 jmp .Lfeed_out
.Lesc_start_csi:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 2
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM], 0
 jmp .Lfeed_ignored
.Lesc_start_ss3:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 3
 jmp .Lfeed_ignored
.Lesc_start_string:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 4
 jmp .Lfeed_ignored
.Lesc_ss3:
 cmp r12b, 0x03
 je .Lesc_abort_ctrl_c
 cmp r12b, 0x1B
 je .Lesc_restart
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 cmp r12b, 'D'
 je .Lfeed_left
 cmp r12b, 'C'
 je .Lfeed_right
 cmp r12b, 'H'
 je .Lfeed_home
 cmp r12b, 'F'
 je .Lfeed_end
 cmp r12b, 'M'
 je .Lfeed_enter
 jmp .Lfeed_ignored
.Lesc_string:
 cmp r12b, 7
 je .Lesc_string_done /* BEL 结束 */
 cmp r12b, 0x03
 je .Lesc_abort_ctrl_c
 cmp r12b, 0x18
 je .Lesc_abort
 cmp r12b, 0x1A
 je .Lesc_abort
 cmp r12b, 0x1B
 jne .Lfeed_ignored /* 内容字节全部消费，无上限：靠 BEL/ST/CAN/^C 收尾 */
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 5
 jmp .Lfeed_ignored
.Lesc_string_done:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 jmp .Lfeed_ignored
.Lesc_st:
 cmp r12b, 0x03
 je .Lesc_abort_ctrl_c
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 cmp r12b, 0x5C /* '\'：ST 正常结束 */
 je .Lfeed_ignored
 cmp r12b, 0x1B
 jne .Lfeed_ignored
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 1
 jmp .Lfeed_ignored
.Lesc_csi:
 cmp r12b, 0x03
 je .Lesc_abort_ctrl_c
 cmp r12b, 0x18
 je .Lesc_abort
 cmp r12b, 0x1A
 je .Lesc_abort
 cmp r12b, 0x1B
 je .Lesc_restart
 cmp r12b, '0'
 jb .Lcsi_mid
 cmp r12b, '9'
 ja .Lcsi_mid
 /* 数字参数：超大量数字只消费不再累加（防溢出） */
 mov r13d, dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM]
 cmp r13d, 100000
 jae .Lfeed_ignored
 imul r13d, r13d, 10
 movzx eax, r12b
 sub eax, '0'
 add r13d, eax
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM], r13d
 jmp .Lfeed_ignored
.Lcsi_mid:
 cmp r12b, 0x20
 jb .Lesc_abort /* 其余 C0：中止序列，字节按丢弃 */
 cmp r12b, 0x3F
 jbe .Lfeed_ignored /* 中间字节/参数分隔：继续消费 */
 cmp r12b, 0x7F
 jae .Lesc_abort
 /* 0x40–0x7E：终结字节 → 分派 */
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 mov r13d, dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM]
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM], 0
 cmp r12b, '~'
 je .Lcsi_tilde
 cmp r13d, 0
 jne .Lfeed_ignored
 cmp r12b, 'D'
 je .Lfeed_left
 cmp r12b, 'C'
 je .Lfeed_right
 cmp r12b, 'H'
 je .Lfeed_home
 cmp r12b, 'F'
 je .Lfeed_end
 jmp .Lfeed_ignored
.Lcsi_tilde:
 mov eax, TUI_LI_KEY_IGNORED
 cmp r13d, 200
 je .Lpaste_begin
 cmp r13d, 201
 je .Lfeed_out /* 游离结束标记：忽略 */
 cmp r13d, 3
 je .Lfeed_delete
 cmp r13d, 1
 je .Lfeed_home
 cmp r13d, 7
 je .Lfeed_home
 cmp r13d, 4
 je .Lfeed_end
 cmp r13d, 8
 je .Lfeed_end
 jmp .Lfeed_out
.Lpaste_begin:
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE], 1
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_MATCH], 0
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_CR], 0
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_REJECT], 0
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 jmp .Lfeed_ignored /* utf8_pending 保留：粘贴前的半字符可由内容补全 */

/* ---- 普通按键 ---- */
.Lfeed_plain:
 cmp dword ptr [rbx + TUI_LI_INPUT_ENTER_CR], 0
 je .Lplain_not_lf
 mov dword ptr [rbx + TUI_LI_INPUT_ENTER_CR], 0
 cmp r12b, 10
 je .Lfeed_ignored /* CRLF 的 LF：不二次提交 */
.Lplain_not_lf:
 cmp r12b, 13
 je .Lfeed_enter_cr
 cmp r12b, 10
 je .Lfeed_enter
 cmp r12b, 8
 je .Lfeed_backspace
 cmp r12b, 127
 je .Lfeed_backspace
 cmp r12b, 3
 je .Lfeed_ctrl_c
 cmp r12b, 4
 je .Lfeed_ctrl_d
 cmp r12b, 21
 je .Lfeed_ctrl_u
 cmp r12b, 32
 jb .Lfeed_ignored
 cmp dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 jne .Lplain_cont
 mov eax, r12d
 and eax, 0xC0
 cmp eax, 0x80
 je .Lfeed_ignored /* 孤立连续字节 */
 movzx edi, r12b
 call tui_utf8_seq_len@PLT
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 add rcx, rax
 cmp rcx, TUI_LI_INPUT_MAX_BYTES - 1
 ja .Lfeed_overflow
 jmp .Lplain_insert
.Lplain_cont:
 mov eax, r12d
 and eax, 0xC0
 cmp eax, 0x80
 jne .Lfeed_ignored
.Lplain_insert:
 call li_insert
 test eax, eax
 jnz .Lfeed_overflow
 mov eax, TUI_LI_KEY_CHAR
 jmp .Lfeed_out

/* ---- 光标移动（清 utf8_pending：半字符留在原地成为惰性数据） ---- */
.Lfeed_left:
 mov rsi, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 test rsi, rsi
 jz .Lfeed_ignored
 mov rdi, rbx
 call tui_utf8_last_seq_start@PLT
 mov [rbx + TUI_LI_INPUT_CURSOR], rax
 jmp .Lfeed_cursor
.Lfeed_right:
 mov r13, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 cmp r13, qword ptr [rbx + TUI_LI_INPUT_LEN]
 jae .Lfeed_ignored
 movzx edi, byte ptr [rbx + r13]
 call tui_utf8_seq_len@PLT
 add qword ptr [rbx + TUI_LI_INPUT_CURSOR], rax
 jmp .Lfeed_cursor
.Lfeed_home:
 mov rax, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
.Lhome_loop:
 test rax, rax
 jz .Lhome_set
 cmp byte ptr [rbx + rax - 1], 10
 je .Lhome_set
 dec rax
 jmp .Lhome_loop
.Lhome_set:
 mov qword ptr [rbx + TUI_LI_INPUT_CURSOR], rax
 jmp .Lfeed_cursor
.Lfeed_end:
 mov rax, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
.Lend_loop:
 cmp rax, rcx
 jae .Lend_set
 cmp byte ptr [rbx + rax], 10
 je .Lend_set
 inc rax
 jmp .Lend_loop
.Lend_set:
 mov qword ptr [rbx + TUI_LI_INPUT_CURSOR], rax
.Lfeed_cursor:
 mov dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 mov eax, TUI_LI_KEY_CURSOR
 jmp .Lfeed_out

.Lfeed_enter_cr:
 mov dword ptr [rbx + TUI_LI_INPUT_ENTER_CR], 1
.Lfeed_enter:
 mov eax, TUI_LI_KEY_ENTER
 jmp .Lfeed_out
.Lfeed_backspace:
 call li_backspace
 jmp .Lfeed_out
.Lfeed_delete:
 call li_delete
 jmp .Lfeed_out
.Lfeed_ctrl_c:
 mov eax, TUI_LI_KEY_CTRL_C
 jmp .Lfeed_out
.Lfeed_ctrl_d:
 cmp qword ptr [rbx + TUI_LI_INPUT_LEN], 0
 jne .Lfeed_backspace /* 非空 ^D：保留现有退格语义 */
 mov eax, TUI_LI_KEY_CTRL_D
 jmp .Lfeed_out
.Lfeed_ctrl_u:
 mov qword ptr [rbx + TUI_LI_INPUT_LEN], 0
 mov qword ptr [rbx + TUI_LI_INPUT_CURSOR], 0
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 mov dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 mov eax, TUI_LI_KEY_CHAR
 jmp .Lfeed_out
.Lfeed_overflow:
 call li_overflow
 jmp .Lfeed_out
.Lfeed_ignored:
 mov eax, TUI_LI_KEY_IGNORED
.Lfeed_out:
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_input_feed, .-tui_input_feed

.section .note.GNU-stack,"",@progbits
