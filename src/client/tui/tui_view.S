/* Scrollback rendering; UTF-8 carry joins precede untrusted control escaping. */
#include "tui_layout.h"
.intel_syntax noprefix

.section .rodata
.Lcrlf:             .asciz "\r\n"
.Lcleartail:        .asciz "\r\033[J"
.Lfmt_cuu:          .asciz "\r\033[%uA"
.Lfmt_cud:          .asciz "\033[%uB"
.Lfmt_cha:          .asciz "\033[%uG"
.Lprompt:           .asciz "> "
.Linput_hint:       .asciz "[tui] 输入：Enter 提交，Alt+Enter 插入换行，Ctrl-U 清空草稿\n"
.Lqmark:            .asciz "?"
.Ltool_pfx:         .asciz "[tool] "
.Lhash_paren:       .asciz " (#"
.Lparen_started:    .asciz ") started\n"
.Lfin_ok:           .asciz ") finished ok: "
.Lfin_err:          .asciz ") finished error: "
.Lnl:               .asciz "\n"
.Lrun_started:      .asciz "[run] started\n"
.Lrun_fin_ok:       .asciz "[run] finished: success\n"
.Lrun_fin_err:      .asciz "[run] finished: error: "
.Lrun_fin_cancel:   .asciz "[run] finished: cancelled\n"
.Lrun_fin_unknown:  .asciz "[run] finished: unknown\n"
.Lbusy:             .asciz "[tui] 忙：当前 run 进行中，提交被拒绝，输入已保留\n"
.Lsubmit_err:       .asciz "[tui] 提交失败（Runtime 拒绝），输入已保留\n"
.Linput_limit:      .asciz "[tui] 输入已达长度上限，继续输入被拒绝\n"
.Lcancel_note:      .asciz "[tui] 已请求取消，等待终态（期间继续显示实际到达的事件）\n"
.Lpoll_err:         .asciz "[tui] Runtime poll 错误；客户端未自行补造终态，退出\n"
.Lproto_violation:  .asciz "[tui] 协议违约：终态后仍收到事件，已丢弃该事件\n"
.Lfmt_banner:       .asciz "[tui] 盘古薄客户端（事件源: %s）\n"
.Lfmt_exit:         .asciz "[tui] 退出（%s），终端已恢复\n"
.Lfmt_marker:       .asciz "…[+%zu 字节截断]"

.Lhistory_user: .asciz "[history] user: "
.Lhistory_assistant: .asciz "[history] assistant: "
.Lhistory_call: .asciz "[history] call "
.Lhistory_result: .asciz "[history] result "
.Lhistory_args: .asciz "): "
.Lhistory_ok: .asciz ") ok: "
.Lhistory_error: .asciz ") error: "
.Lhex: .ascii "0123456789abcdef"

.section .text

/* ---- 内部：sink.write(ctx, data, len)，返回值忽略。rbx=view，rsi/rdx=数据 ---- */
.type li_sink_write,@function
li_sink_write:
 mov rax, qword ptr [rbx + TUI_LI_VIEW_SINK_WRITE]
 mov rdi, qword ptr [rbx + TUI_LI_VIEW_SINK_CTX]
 jmp rax
.size li_sink_write, .-li_sink_write

/* ---- 内部：vput(tui_view *v, const char *data, size_t len)
 * ANSI 模式把 '\n' 翻译为 "\r\n"；降级模式原样写出。 ---- */
.type li_vput,@function
li_vput:
 test rdx, rdx
 jz .Lvput_done
 cmp dword ptr [rdi + TUI_LI_VIEW_ANSI], 0
 je .Lvput_plain
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 xor r14d, r14d /* i */
 xor r15d, r15d /* off */
.Lvput_scan:
 cmp r14, r13
 jae .Lvput_tail
 cmp byte ptr [r12 + r14], 10 /* '\n' */
 jne .Lvput_next
 cmp r14, r15
 jbe .Lvput_no_pre
 mov rdi, rbx
 lea rsi, [r12 + r15]
 mov rdx, r14
 sub rdx, r15
 call li_sink_write
.Lvput_no_pre:
 mov rdi, rbx
 lea rsi, [rip + .Lcrlf]
 mov edx, 2
 call li_sink_write
 lea r15, [r14 + 1]
.Lvput_next:
 inc r14
 jmp .Lvput_scan
.Lvput_tail:
 cmp r13, r15
 jbe .Lvput_fin
 mov rdi, rbx
 lea rsi, [r12 + r15]
 mov rdx, r13
 sub rdx, r15
 call li_sink_write
.Lvput_fin:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
.Lvput_done:
 ret
.Lvput_plain:
 mov rax, qword ptr [rdi + TUI_LI_VIEW_SINK_WRITE]
 mov rdi, qword ptr [rdi + TUI_LI_VIEW_SINK_CTX]
 jmp rax
.size li_vput, .-li_vput

/* Escape control bytes in data, preserving complete UTF-8 and newline/tab. */
.type li_safe_vput,@function
li_safe_vput:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
 xor r15d,r15d
.Lsafe_scan:
 cmp r14,r13
 jae .Lsafe_tail
 movzx eax,byte ptr [r12+r14]
 cmp eax,32
 jae .Lsafe_non_c0
 cmp eax,9
 je .Lsafe_next
 cmp eax,10
 je .Lsafe_next
 jmp .Lsafe_byte
.Lsafe_non_c0:
 cmp eax,127
 je .Lsafe_byte
 cmp eax,128
 jb .Lsafe_next
 cmp eax,160
 jb .Lsafe_byte
 cmp eax,194
 jb .Lsafe_byte
 cmp eax,244
 ja .Lsafe_byte
 cmp eax,194
 jne .Lsafe_utf8
 lea rcx,[r14+1]
 cmp rcx,r13
 jae .Lsafe_byte
 movzx eax,byte ptr [r12+rcx]
 cmp eax,128
 jb .Lsafe_utf8
 cmp eax,159
 ja .Lsafe_utf8
 mov qword ptr [rsp+8],2
 jmp .Lsafe_escape
.Lsafe_utf8:
 movzx edi,byte ptr [r12+r14]
 call tui_utf8_seq_len
 mov rcx,r13
 sub rcx,r14
 cmp rax,rcx
 ja .Lsafe_incomplete
 mov ecx,1
.Lsafe_continuation:
 cmp rcx,rax
 jae .Lsafe_sequence_bounds
 lea rdx,[r14+rcx]
 movzx edx,byte ptr [r12+rdx]
 sub edx,128
 cmp edx,63
 ja .Lsafe_incomplete
 inc ecx
 jmp .Lsafe_continuation
.Lsafe_sequence_bounds:
 movzx edx,byte ptr [r12+r14]
 movzx ecx,byte ptr [r12+r14+1]
 cmp edx,224
 jne .Lsafe_surrogate
 cmp ecx,160
 jb .Lsafe_incomplete
.Lsafe_surrogate:
 cmp edx,237
 jne .Lsafe_plane
 cmp ecx,159
 ja .Lsafe_incomplete
.Lsafe_plane:
 cmp edx,240
 jne .Lsafe_maximum
 cmp ecx,144
 jb .Lsafe_incomplete
.Lsafe_maximum:
 cmp edx,244
 jne .Lsafe_advance
 cmp ecx,143
 ja .Lsafe_incomplete
.Lsafe_advance:
 add r14,rax
 jmp .Lsafe_scan
.Lsafe_incomplete:
 movzx eax,byte ptr [r12+r14]
.Lsafe_byte:
 mov qword ptr [rsp+8],1
.Lsafe_escape:
 mov [rsp+4],eax
 cmp r14,r15
 je .Lsafe_marker
 mov rdi,rbx
 lea rsi,[r12+r15]
 mov rdx,r14
 sub rdx,r15
 call li_vput
.Lsafe_marker:
 mov eax,[rsp+4]
 lea rsi,[rip+.Lhex]
 mov ecx,eax
 and eax,15
 shr ecx,4
 mov al,[rsi+rax]
 mov cl,[rsi+rcx]
 mov word ptr [rsp],0x785c
 mov [rsp+2],cl
 mov [rsp+3],al
 mov edx,4
 cmp qword ptr [rsp+8],2
 jne .Lsafe_emit
 mov dword ptr [rsp],0x3030755c
 mov [rsp+4],cl
 mov [rsp+5],al
 mov edx,6
.Lsafe_emit:
 mov rdi,rbx
 mov rsi,rsp
 call li_vput
 add r14,[rsp+8]
 mov r15,r14
 jmp .Lsafe_scan
.Lsafe_next:
 inc r14
 jmp .Lsafe_scan
.Lsafe_tail:
 cmp r13,r15
 jbe .Lsafe_done
 mov rdi,rbx
 lea rsi,[r12+r15]
 mov rdx,r13
 sub rdx,r15
 call li_vput
.Lsafe_done:
 add rsp,16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_safe_vput,.-li_safe_vput

/* ---- 内部：vputs(tui_view *v, const char *s) ---- */
.type li_vputs,@function
li_vputs:
 push rbx
 push r12
 push rax /* 16 字节对齐占位 */
 mov rbx, rdi
 mov r12, rsi
 mov rdi, rsi
 call strlen@PLT
 mov rdi, rbx
 mov rsi, r12
 mov rdx, rax
 call li_vput
 pop rax
 pop r12
 pop rbx
 ret
.size li_vputs, .-li_vputs

/* ---- 内部：event_suspend(v)：事件输出前结束编辑区（滚动入回显） ---- */
.type li_event_suspend,@function
li_event_suspend:
 jmp li_editor_break
.size li_event_suspend, .-li_event_suspend

/* ---- 内部：editor_break(v)：下移到编辑区末行并结束该行，重置几何 ----
 * 事件输出、提交定稿、resize 换行均经此收束；文本留在滚动回显。 */
.type li_editor_break,@function
li_editor_break:
 cmp dword ptr [rdi + TUI_LI_VIEW_ANSI], 0
 je .Lbrk_ret
 cmp dword ptr [rdi + TUI_LI_VIEW_EDITOR_ON_LINE], 0
 je .Lbrk_ret
 push rbx
 push r12
 push rax /* 24 字节保持 16 对齐 */
 mov rbx, rdi
 mov r12d, dword ptr [rbx + TUI_LI_VIEW_EDITOR_ROWS]
 dec r12d
 sub r12d, dword ptr [rbx + TUI_LI_VIEW_CURSOR_ROW]
 jle .Lbrk_nl
 sub rsp, 32 /* char seq[32] */
 mov rdi, rsp
 mov esi, 32
 lea rdx, [rip + .Lfmt_cud]
 mov ecx, r12d
 xor eax, eax
 call snprintf@PLT
 test eax, eax
 jg .Lbrk_cud_write
 add rsp, 32
 jmp .Lbrk_nl
.Lbrk_cud_write:
 mov rdi, rbx
 mov rsi, rsp
 call li_vputs
 add rsp, 32
.Lbrk_nl:
 mov rdi, rbx
 lea rsi, [rip + .Lcrlf]
 call li_vputs
 mov dword ptr [rbx + TUI_LI_VIEW_EDITOR_ON_LINE], 0
 mov dword ptr [rbx + TUI_LI_VIEW_EDITOR_ROWS], 0
 mov dword ptr [rbx + TUI_LI_VIEW_CURSOR_ROW], 0
 mov dword ptr [rbx + TUI_LI_VIEW_CURSOR_COL], 0
 pop rax
 pop r12
 pop rbx
.Lbrk_ret:
 ret
.size li_editor_break, .-li_editor_break

/* ---- 内部：cp_width(seq, len) → eax ∈ {0,1,2}：code point 显示宽度近似。
 * 零宽：组合附加符号/零宽字符/变体选择符；宽 2：CJK/全角/主要 emoji 平面。
 * 与编辑器同为 code point 粒度，不声明完整字素簇/东亚宽度全表。 */
.align 4
.Lw_zero:
 .long 0x0300, 0x036F
 .long 0x200B, 0x200F
 .long 0xFE00, 0xFE0F
.Lw_zero_end:
.Lw_wide:
 .long 0x1100, 0x115F
 .long 0x2E80, 0x303E
 .long 0x3041, 0x33FF
 .long 0x3400, 0x4DBF
 .long 0x4E00, 0x9FFF
 .long 0xA000, 0xA4CF
 .long 0xAC00, 0xD7A3
 .long 0xF900, 0xFAFF
 .long 0xFE30, 0xFE4F
 .long 0xFF00, 0xFF60
 .long 0x1F300, 0x1F64F
 .long 0x1F680, 0x1F6FF
 .long 0x1F900, 0x1F9FF
 .long 0x20000, 0x2FFFD
 .long 0x30000, 0x3FFFD
.Lw_wide_end:

.type li_cp_width,@function
li_cp_width:
 movzx eax, byte ptr [rdi]
 cmp eax, 0x80
 jb .Lw_one
 mov edx, eax
 and edx, 0xE0
 cmp edx, 0xC0
 jne .Lw_3
 cmp rsi, 2
 jb .Lw_one
 movzx eax, byte ptr [rdi + 1]
 and eax, 0x3F
 movzx edx, byte ptr [rdi]
 and edx, 0x1F
 shl edx, 6
 or eax, edx
 jmp .Lw_scan
.Lw_3:
 mov edx, eax
 and edx, 0xF0
 cmp edx, 0xE0
 jne .Lw_4
 cmp rsi, 3
 jb .Lw_one
 movzx eax, byte ptr [rdi + 1]
 and eax, 0x3F
 movzx edx, byte ptr [rdi + 2]
 and edx, 0x3F
 shl eax, 6
 or eax, edx
 movzx edx, byte ptr [rdi]
 and edx, 0x0F
 shl edx, 12
 or eax, edx
 jmp .Lw_scan
.Lw_4:
 mov edx, eax
 and edx, 0xF8
 cmp edx, 0xF0
 jne .Lw_one /* 非法首字节按 1 */
 cmp rsi, 4
 jb .Lw_one
 movzx eax, byte ptr [rdi + 1]
 and eax, 0x3F
 movzx edx, byte ptr [rdi + 2]
 and edx, 0x3F
 shl eax, 6
 or eax, edx
 movzx edx, byte ptr [rdi + 3]
 and edx, 0x3F
 shl eax, 6
 or eax, edx
 movzx edx, byte ptr [rdi]
 and edx, 0x07
 shl edx, 18
 or eax, edx
.Lw_scan:
 lea rdx, [rip + .Lw_zero]
 xor ecx, ecx
.Lw_zero_loop:
 cmp ecx, TUI_W_ZERO_COUNT
 jae .Lw_wide_scan
 cmp eax, dword ptr [rdx + rcx*4]
 jb .Lw_zero_next
 cmp eax, dword ptr [rdx + rcx*4 + 4]
 jbe .Lw_ret0
.Lw_zero_next:
 add ecx, 2
 jmp .Lw_zero_loop
.Lw_wide_scan:
 lea rdx, [rip + .Lw_wide]
 xor ecx, ecx
.Lw_wide_loop:
 cmp ecx, TUI_W_WIDE_COUNT
 jae .Lw_one
 cmp eax, dword ptr [rdx + rcx*4]
 jb .Lw_wide_next
 cmp eax, dword ptr [rdx + rcx*4 + 4]
 jbe .Lw_ret2
.Lw_wide_next:
 add ecx, 2
 jmp .Lw_wide_loop
.Lw_ret0:
 xor eax, eax
 ret
.Lw_ret2:
 mov eax, 2
 ret
.Lw_one:
 mov eax, 1
 ret
.size li_cp_width, .-li_cp_width

/* ---- 内部：event_resume_editor(v)：编辑缓冲非空则重绘编辑行 ---- */
.type li_event_resume,@function
li_event_resume:
 mov rax, qword ptr [rdi + TUI_LI_VIEW_EDITOR]
 test rax, rax
 jz .Lres_ret
 cmp qword ptr [rax + TUI_LI_INPUT_LEN], 0
 je .Lres_ret
 jmp tui_view_redraw_editor@PLT
.Lres_ret:
 ret
.size li_event_resume, .-li_event_resume

/* ---- 内部：put_bounded(v, data, len, cap)：有界展示 + 截断标记 ---- */
.type li_put_bounded,@function
li_put_bounded:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12, rsi /* data */
 mov r13, rdx /* len */
 mov r14, rcx /* cap */
 mov rdi, r12
 mov rsi, r13
 mov rdx, r14
 call tui_utf8_truncate_len@PLT /* rax = keep */
 mov r15, rax
 mov rdi, rbx
 mov rsi, r12
 mov rdx, r15
 call li_safe_vput
 cmp r13, r15
 jbe .Lpb_done /* 未截断 */
 sub rsp, 64 /* char marker[64]，保持 16 对齐 */
 mov rdi, rsp
 mov esi, 64
 lea rdx, [rip + .Lfmt_marker]
 mov rcx, r13
 sub rcx, r15
 xor eax, eax /* varargs：无 SSE 参数 */
 call snprintf@PLT
 test eax, eax
 jle .Lpb_pop /* n <= 0 不写 */
 cmp eax, 63
 jle .Lpb_len_ok
 mov eax, 63 /* snprintf 返回“应有长度”时按实际缓冲截断 */
.Lpb_len_ok:
 mov edx, eax
 mov rdi, rbx
 mov rsi, rsp
 call li_vput
.Lpb_pop:
 add rsp, 64
.Lpb_done:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_put_bounded, .-li_put_bounded

/* ---- 内部：put_field(v, s, cap)：事件内字符串字段（NULL 显示 "?"） ---- */
.type li_put_field,@function
li_put_field:
 test rsi, rsi
 jnz .Lpf_have
 lea rsi, [rip + .Lqmark]
 jmp li_vputs
.Lpf_have:
 push rbx
 push r12
 push r13
 mov rbx, rdi
 mov r12, rsi /* s */
 mov r13, rdx /* cap */
 mov rdi, r12
 call strlen@PLT
 mov rdi, rbx
 mov rsi, r12
 mov rdx, rax
 mov rcx, r13
 call li_put_bounded
 pop r13
 pop r12
 pop rbx
 ret
.size li_put_field, .-li_put_field

/* void tui_view_init(tui_view *v, tui_out_sink sink, int ansi, tui_input *editor)
 * rdi=v rsi=sink.ctx rdx=sink.write ecx=ansi r8=editor */
.globl tui_view_init
.type tui_view_init,@function
tui_view_init:
 .cfi_startproc
 mov qword ptr [rdi + TUI_LI_VIEW_SINK_CTX], rsi
 mov qword ptr [rdi + TUI_LI_VIEW_SINK_WRITE], rdx
 mov dword ptr [rdi + TUI_LI_VIEW_ANSI], ecx
 mov qword ptr [rdi + TUI_LI_VIEW_EDITOR], r8
 mov dword ptr [rdi + TUI_LI_VIEW_EDITOR_ON_LINE], 0
 mov qword ptr [rdi + TUI_LI_VIEW_CARRY_LEN], 0
 mov dword ptr [rdi + TUI_LI_VIEW_COLS], 80
 mov dword ptr [rdi + TUI_LI_VIEW_EDITOR_ROWS], 0
 mov dword ptr [rdi + TUI_LI_VIEW_CURSOR_ROW], 0
 mov dword ptr [rdi + TUI_LI_VIEW_CURSOR_COL], 0
 /* md 渲染器：同一 sink/模式，默认 80 列（set_size 同步更新） */
 push rbx
 push r12
 mov rbx, rdi
 mov r12d, ecx
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 mov rsi, qword ptr [rbx + TUI_LI_VIEW_SINK_CTX]
 mov rdx, qword ptr [rbx + TUI_LI_VIEW_SINK_WRITE]
 mov ecx, r12d
 mov r8d, 80
 call tui_md_init@PLT
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_view_init, .-tui_view_init

.globl tui_view_set_size
.type tui_view_set_size,@function
tui_view_set_size:
 .cfi_startproc
 test esi, esi
 jnz .Lsize_nonzero
 mov esi, 1
.Lsize_nonzero:
 cmp esi, dword ptr [rdi + TUI_LI_VIEW_COLS]
 je .Lsize_ret
 /* 列数变化（resize）：旧几何不可靠，先把编辑区收束进滚动回显再更新 */
 push rbx
 push r12
 push rax /* 24 字节保持 16 对齐 */
 mov rbx, rdi
 mov r12d, esi
 mov rdi, rbx
 call li_editor_break
 mov dword ptr [rbx + TUI_LI_VIEW_COLS], r12d
 /* md 折行宽度跟随（滚动回显不重排，仅影响后续行） */
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 mov esi, r12d
 call tui_md_set_cols@PLT
 pop rax
 pop r12
 pop rbx
.Lsize_ret:
 ret
 .cfi_endproc
.size tui_view_set_size, .-tui_view_set_size

/* void tui_view_redraw_editor(tui_view *v) — 多行/窄终端重绘 + 真实光标定位
 * 栈槽（sub rsp,80）：[0]=cur_row [8]=cur_col [16]=seq_len [24]=w [32..]=转义序列缓冲。
 * 逻辑换行写 "\n"（ANSI 模式经 li_vput 译为 "\r\n"）；提交字节不受换行影响。 */
.globl tui_view_redraw_editor
.type tui_view_redraw_editor,@function
tui_view_redraw_editor:
 .cfi_startproc
 cmp dword ptr [rdi + TUI_LI_VIEW_ANSI], 0
 je .Lred_ret
 mov rax, qword ptr [rdi + TUI_LI_VIEW_EDITOR]
 test rax, rax
 jz .Lred_ret
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 80
 mov rbx, rdi
 mov r12, rax /* editor（buf 偏移 0） */
 /* 擦除旧编辑区：先上移到编辑区首行，再清到屏幕尾 */
 cmp dword ptr [rbx + TUI_LI_VIEW_EDITOR_ON_LINE], 0
 je .Lred_clear
 mov ecx, dword ptr [rbx + TUI_LI_VIEW_CURSOR_ROW]
 test ecx, ecx
 jz .Lred_clear
 mov rdi, rsp
 add rdi, 32
 mov esi, 32
 lea rdx, [rip + .Lfmt_cuu]
 xor eax, eax
 call snprintf@PLT
 test eax, eax
 jle .Lred_clear
 mov rdi, rbx
 lea rsi, [rsp + 32]
 call li_vputs
.Lred_clear:
 mov rdi, rbx
 lea rsi, [rip + .Lcleartail]
 call li_vputs
 mov rdi, rbx
 lea rsi, [rip + .Lprompt]
 call li_vputs
 mov qword ptr [rsp], 0 /* cur_row */
 mov qword ptr [rsp + 8], 2 /* cur_col 默认：空稿/无捕获 */
 mov r14d, 0 /* 视觉行 */
 mov r15d, 2 /* 当前列（提示符后） */
 xor r13d, r13d /* 字节偏移 i */
.Lred_loop:
 mov rax, qword ptr [r12 + TUI_LI_INPUT_LEN]
 cmp r13, rax
 jae .Lred_end
 movzx eax, byte ptr [r12 + r13]
 cmp al, 10
 jne .Lred_not_nl
 /* 光标恰在换行符前：捕获当前位置 */
 mov rax, qword ptr [r12 + TUI_LI_INPUT_CURSOR]
 cmp rax, r13
 jne .Lred_nl_write
 mov [rsp], r14
 mov [rsp + 8], r15
.Lred_nl_write:
 mov rdi, rbx
 lea rsi, [rip + .Lnl]
 call li_vputs
 inc r14d
 mov r15d, 0
 inc r13
 jmp .Lred_loop
.Lred_not_nl:
 cmp al, 32
 jb .Lred_skip /* 防御：缓冲不含控制字节 */
 cmp al, 127
 je .Lred_skip
 mov edi, eax
 call tui_utf8_seq_len@PLT
 mov rcx, qword ptr [r12 + TUI_LI_INPUT_LEN]
 sub rcx, r13
 cmp rax, rcx
 jbe .Lred_len_ok
 mov rax, rcx
.Lred_len_ok:
 mov qword ptr [rsp + 16], rax
 mov rdi, r12
 add rdi, r13
 mov rsi, rax
 call li_cp_width
 mov qword ptr [rsp + 24], rax
 /* 自动换行：本行已有内容且整字符放不下 */
 test r15d, r15d
 jz .Lred_place
 mov edx, r15d
 add edx, eax
 cmp edx, dword ptr [rbx + TUI_LI_VIEW_COLS]
 jbe .Lred_place
 mov rdi, rbx
 lea rsi, [rip + .Lnl]
 call li_vputs
 inc r14d
 mov r15d, 0
.Lred_place:
 /* 光标在本字符内或恰在其前：捕获 (row, col) */
 mov rax, qword ptr [r12 + TUI_LI_INPUT_CURSOR]
 cmp rax, r13
 jb .Lred_no_cur
 mov rcx, r13
 add rcx, qword ptr [rsp + 16]
 cmp rax, rcx
 jae .Lred_no_cur
 mov [rsp], r14
 mov [rsp + 8], r15
.Lred_no_cur:
 mov rdi, rbx
 mov rsi, r12
 add rsi, r13
 mov rdx, qword ptr [rsp + 16]
 call li_vput
 mov eax, dword ptr [rsp + 24]
 add r15d, eax
 mov rax, qword ptr [rsp + 16]
 add r13, rax
 jmp .Lred_loop
.Lred_skip:
 inc r13
 jmp .Lred_loop
.Lred_end:
 mov rax, qword ptr [r12 + TUI_LI_INPUT_CURSOR]
 cmp rax, r13
 jne .Lred_have_cur
 mov [rsp], r14
 mov [rsp + 8], r15
.Lred_have_cur:
 /* 真实光标从文本末尾移回编辑位置 */
 mov rax, [rsp]
 cmp r14, rax
 jbe .Lred_row_done
 mov ecx, r14d
 sub ecx, dword ptr [rsp]
 mov rdi, rsp
 add rdi, 32
 mov esi, 32
 lea rdx, [rip + .Lfmt_cuu]
 xor eax, eax
 call snprintf@PLT
 test eax, eax
 jle .Lred_row_done
 mov rdi, rbx
 lea rsi, [rsp + 32]
 call li_vputs
.Lred_row_done:
 mov ecx, dword ptr [rsp + 8]
 cmp ecx, r15d
 je .Lred_store
 inc ecx
 mov rdi, rsp
 add rdi, 32
 mov esi, 32
 lea rdx, [rip + .Lfmt_cha]
 xor eax, eax
 call snprintf@PLT
 test eax, eax
 jle .Lred_store
 mov rdi, rbx
 lea rsi, [rsp + 32]
 call li_vputs
.Lred_store:
 mov eax, r14d
 inc eax
 mov dword ptr [rbx + TUI_LI_VIEW_EDITOR_ROWS], eax
 mov eax, dword ptr [rsp]
 mov dword ptr [rbx + TUI_LI_VIEW_CURSOR_ROW], eax
 mov eax, dword ptr [rsp + 8]
 mov dword ptr [rbx + TUI_LI_VIEW_CURSOR_COL], eax
 mov dword ptr [rbx + TUI_LI_VIEW_EDITOR_ON_LINE], 1
 add rsp, 80
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
.Lred_ret:
 ret
 .cfi_endproc
.size tui_view_redraw_editor, .-tui_view_redraw_editor

/* void tui_view_finalize_editor(tui_view *v)：文本留在滚动回显，重置几何 */
.globl tui_view_finalize_editor
.type tui_view_finalize_editor,@function
tui_view_finalize_editor:
 .cfi_startproc
 jmp li_editor_break
 .cfi_endproc
.size tui_view_finalize_editor, .-tui_view_finalize_editor

/* void tui_view_banner(tui_view *v, const char *runtime_name) */
.globl tui_view_banner
.type tui_view_banner,@function
tui_view_banner:
 .cfi_startproc
 push rbx
 push r12
 push rax
 sub rsp, 128 /* char line[128] */
 mov rbx, rdi
 mov r12, rsi
 mov rdi, rsp
 mov esi, 128
 lea rdx, [rip + .Lfmt_banner]
 mov rcx, r12
 xor eax, eax
 call snprintf@PLT
 test eax, eax
 jle .Lbanner_skip
 cmp eax, 127
 jle .Lbanner_len
 mov eax, 127
.Lbanner_len:
 mov edx, eax
 mov rdi, rbx
 mov rsi, rsp
 call li_vput
 mov rdi, rbx
 lea rsi, [rip + .Linput_hint]
 call li_vputs
.Lbanner_skip:
 mov dword ptr [rbx + TUI_LI_VIEW_EDITOR_ON_LINE], 0
 mov dword ptr [rbx + TUI_LI_VIEW_EDITOR_ROWS], 0
 mov dword ptr [rbx + TUI_LI_VIEW_CURSOR_ROW], 0
 mov dword ptr [rbx + TUI_LI_VIEW_CURSOR_COL], 0
 add rsp, 128
 pop rax
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_view_banner, .-tui_view_banner

/* void tui_view_exit_note(tui_view *v, const char *reason) */
.globl tui_view_exit_note
.type tui_view_exit_note,@function
tui_view_exit_note:
 .cfi_startproc
 push rbx
 push r12
 push r13
 sub rsp, 160 /* char line[160] */
 mov rbx, rdi
 mov r12, rsi
 mov r13d, eax /* snprintf 返回值暂存（先格式化，与 C 版顺序一致） */
 /* 退出说明前收束未完模型行与 carry，失败路径不丢已收到的输出 */
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_flush@PLT
 mov rdi, rbx
 call li_finish_carry
 mov rdi, rsp
 mov esi, 160
 lea rdx, [rip + .Lfmt_exit]
 mov rcx, r12
 xor eax, eax
 call snprintf@PLT
 mov r13d, eax
 mov rdi, rbx
 call li_editor_break /* 多行编辑区同样收束后再写退出说明 */
 test r13d, r13d
 jle .Lexit_done
 cmp r13d, 159
 jle .Lexit_len
 mov r13d, 159
.Lexit_len:
 mov edx, r13d
 mov rdi, rbx
 mov rsi, rsp
 call li_vput
.Lexit_done:
 add rsp, 160
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_view_exit_note, .-tui_view_exit_note

/* 运行态提示：统一 suspend → 消息 →（poll_error 无 resume）resume ---- */

/* void tui_view_busy_rejected(tui_view *v) 等：逐个显式实现，避免宏藏控制流 */
.globl tui_view_busy_rejected
.type tui_view_busy_rejected,@function
tui_view_busy_rejected:
 .cfi_startproc
 push rbx
 push r12
 push rax
 mov rbx, rdi
 mov rdi, rbx
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT /* 冲出未完模型行再写状态行 */
 mov rdi, rbx
 call li_event_suspend
 lea rsi, [rip + .Lbusy]
 mov rdi, rbx
 call li_vputs
 mov rdi, rbx
 call li_event_resume
 pop rax
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_view_busy_rejected, .-tui_view_busy_rejected

.globl tui_view_submit_error
.type tui_view_submit_error,@function
tui_view_submit_error:
 .cfi_startproc
 push rbx
 push r12
 push rax
 mov rbx, rdi
 mov rdi, rbx
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT /* 冲出未完模型行再写状态行 */
 mov rdi, rbx
 call li_event_suspend
 lea rsi, [rip + .Lsubmit_err]
 mov rdi, rbx
 call li_vputs
 mov rdi, rbx
 call li_event_resume
 pop rax
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_view_submit_error, .-tui_view_submit_error

.globl tui_view_input_limit
.type tui_view_input_limit,@function
tui_view_input_limit:
 .cfi_startproc
 push rbx
 push r12
 push rax
 mov rbx, rdi
 mov rdi, rbx
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT /* 冲出未完模型行再写状态行 */
 mov rdi, rbx
 call li_event_suspend
 lea rsi, [rip + .Linput_limit]
 mov rdi, rbx
 call li_vputs
 mov rdi, rbx
 call li_event_resume
 pop rax
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_view_input_limit, .-tui_view_input_limit

.globl tui_view_cancel_requested
.type tui_view_cancel_requested,@function
tui_view_cancel_requested:
 .cfi_startproc
 push rbx
 push r12
 push rax
 mov rbx, rdi
 mov rdi, rbx
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT /* 冲出未完模型行再写状态行 */
 mov rdi, rbx
 call li_event_suspend
 lea rsi, [rip + .Lcancel_note]
 mov rdi, rbx
 call li_vputs
 mov rdi, rbx
 call li_event_resume
 pop rax
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_view_cancel_requested, .-tui_view_cancel_requested

/* poll 错误后客户端退出：只挂起，不重绘编辑行 */
.globl tui_view_poll_error
.type tui_view_poll_error,@function
tui_view_poll_error:
 .cfi_startproc
 push rbx
 push r12
 push rax
 mov rbx, rdi
 /* 失败退出不补造终态，但已缓冲的模型文本必须可见后再退出 */
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_flush@PLT
 mov rdi, rbx
 call li_finish_carry
 mov rdi, rbx
 call li_event_suspend
 lea rsi, [rip + .Lpoll_err]
 mov rdi, rbx
 call li_vputs
 pop rax
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_view_poll_error, .-tui_view_poll_error

.globl tui_view_protocol_violation
.type tui_view_protocol_violation,@function
tui_view_protocol_violation:
 .cfi_startproc
 push rbx
 push r12
 push rax
 mov rbx, rdi
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_flush@PLT
 mov rdi, rbx
 call li_finish_carry
 mov rdi, rbx
 call li_event_suspend
 lea rsi, [rip + .Lproto_violation]
 mov rdi, rbx
 call li_vputs
 mov rdi, rbx
 call li_event_resume
 pop rax
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_view_protocol_violation, .-tui_view_protocol_violation

/* A role/tool boundary never joins an incomplete sequence from another stream. */
li_finish_carry:
 push rbx
 mov rbx,rdi
 cmp qword ptr [rbx+TUI_LI_VIEW_CARRY_LEN],0
 je .Lfinish_carry_done
 call li_event_suspend
 mov rdi,rbx
 lea rsi,[rbx+TUI_LI_VIEW_CARRY]
 mov rdx,[rbx+TUI_LI_VIEW_CARRY_LEN]
 call li_safe_vput
 mov qword ptr [rbx+TUI_LI_VIEW_CARRY_LEN],0
.Lfinish_carry_done:
 pop rbx
 ret

/* void tui_view_event(tui_view *v, const tui_event *ev) */
.globl tui_view_event
.type tui_view_event,@function
tui_view_event:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi /* v */
 mov r12, rsi /* ev */
 mov eax, dword ptr [r12 + TUI_LI_EVENT_KIND]
 cmp eax, TUI_LI_EV_RUN_STARTED
 je .Lev_started
 cmp eax, TUI_LI_EV_TEXT_DELTA
 je .Lev_md
 cmp eax, TUI_LI_EV_TOOL_OUTPUT
 je .Lev_delta
 cmp eax, TUI_LI_EV_TOOL_STARTED
 je .Lev_tool_started
 cmp eax, TUI_LI_EV_TOOL_FINISHED
 je .Lev_tool_finished
 cmp eax, TUI_LI_EV_RUN_FINISHED
 je .Lev_finished
 jmp .Lev_out

.Lev_started:
 mov rdi, rbx
 call li_event_suspend
 lea rsi, [rip + .Lrun_started]
 mov rdi, rbx
 call li_vputs
 jmp .Lev_out

/* ---- text_delta：流式 Markdown 渲染（完整行输出，工具输出另走原文通道） ---- */
.Lev_md:
 mov rdi, rbx
 call li_event_suspend
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_TEXT]
 mov rdx, qword ptr [r12 + TUI_LI_EVENT_TEXT_LEN]
 call tui_md_text@PLT
 test eax, eax
 jz .Lev_md_out
 mov rdi, rbx
 call li_event_resume
.Lev_md_out:
 jmp .Lev_out

/* ---- tool_output：carry 拼接 + 尾部不完整序列留存（原文转义） ---- */
.Lev_delta:
 sub rsp, 16 /* [rsp] = tail */
 mov qword ptr [rsp], 0
 xor r13d, r13d /* off */
 xor r14d, r14d /* flush_carry */
 xor r15d, r15d /* written */
 mov rcx, qword ptr [rbx + TUI_LI_VIEW_CARRY_LEN]
 test rcx, rcx
 jz .Ldelta_scan
 movzx edi, byte ptr [rbx + TUI_LI_VIEW_CARRY]
 call tui_utf8_seq_len@PLT /* rax = cl0 */
 mov rcx, qword ptr [rbx + TUI_LI_VIEW_CARRY_LEN]
 cmp rax, rcx
 jbe .Lcarry_ready /* cl0 <= carry_len：need=0，直接冲掉 */
 mov rdx, rax
 sub rdx, rcx /* rdx = need */
 cmp qword ptr [r12 + TUI_LI_EVENT_TEXT_LEN], rdx
 jb .Lcarry_absorb
 mov r13, rdx /* off = need：本段前缀补全 carry */
 mov r14d, 1
 jmp .Ldelta_scan
.Lcarry_ready:
 mov r14d, 1
 jmp .Ldelta_scan
.Lcarry_absorb:
 /* 本段全部并入 carry；不提前写出（遵守“绝不写出半个序列”） */
 lea rdi, [rbx + TUI_LI_VIEW_CARRY]
 add rdi, rcx
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_TEXT]
 mov rdx, qword ptr [r12 + TUI_LI_EVENT_TEXT_LEN]
 call memcpy@PLT
 mov rax, qword ptr [r12 + TUI_LI_EVENT_TEXT_LEN]
 add qword ptr [rbx + TUI_LI_VIEW_CARRY_LEN], rax
 mov r13, rax /* off = text_len：本段无输出 */
.Ldelta_scan:
 /* tail = incomplete_tail(text+off, text_len-off) */
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_TEXT]
 add rsi, r13
 mov rdx, qword ptr [r12 + TUI_LI_EVENT_TEXT_LEN]
 sub rdx, r13
 mov rdi, rsi
 mov rsi, rdx
 call tui_utf8_incomplete_tail@PLT
 mov [rsp], rax /* tail */
 mov r15, qword ptr [r12 + TUI_LI_EVENT_TEXT_LEN]
 sub r15, r13
 sub r15, rax /* written = rem - tail */
 test r14d, r14d
 jz .Ldelta_no_flush
 mov rdi, rbx
 call li_event_suspend
 lea rdi,[rsp+8]
 lea rsi,[rbx+TUI_LI_VIEW_CARRY]
 mov rdx,[rbx+TUI_LI_VIEW_CARRY_LEN]
 call memcpy
 lea rdi,[rsp+8]
 add rdi,[rbx+TUI_LI_VIEW_CARRY_LEN]
 test r13,r13
 jz .Lcarry_joined
 mov rsi,[r12+TUI_LI_EVENT_TEXT]
 mov rdx,r13
 call memcpy
.Lcarry_joined:
 mov rdx,[rbx+TUI_LI_VIEW_CARRY_LEN]
 add rdx,r13
 mov rdi,rbx
 lea rsi,[rsp+8]
 call li_safe_vput
 mov qword ptr [rbx+TUI_LI_VIEW_CARRY_LEN],0
 test r15,r15
 jz .Ldelta_tailcopy
 mov rdi,rbx
 mov rsi,[r12+TUI_LI_EVENT_TEXT]
 add rsi,r13
 mov rdx,r15
 call li_safe_vput
 jmp .Ldelta_tailcopy
.Ldelta_no_flush:
 test r15, r15
 jz .Ldelta_tailcopy
 mov rdi, rbx
 call li_event_suspend
 mov rdi, rbx
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_TEXT]
 add rsi, r13
 mov rdx, r15
 call li_safe_vput
.Ldelta_tailcopy:
 mov rax, [rsp]
 test rax, rax
 jz .Ldelta_resume_chk
 lea rdi, [rbx + TUI_LI_VIEW_CARRY]
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_TEXT]
 add rsi, r13
 add rsi, r15
 mov rdx, rax
 call memcpy@PLT
 mov rax, [rsp]
 mov qword ptr [rbx + TUI_LI_VIEW_CARRY_LEN], rax
.Ldelta_resume_chk:
 mov rdi, rbx
 test r14d, r14d
 jnz .Ldelta_resume
 test r15, r15
 jz .Ldelta_out16
.Ldelta_resume:
 call li_event_resume
.Ldelta_out16:
 add rsp, 16
 jmp .Lev_out

.Lev_tool_started:
 mov rdi, rbx
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT
 mov rdi,rbx
 call li_finish_carry
 mov rdi, rbx
 call li_event_suspend
 lea rsi, [rip + .Ltool_pfx]
 mov rdi, rbx
 call li_vputs
 mov rdi, rbx
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_TOOL_NAME]
 mov edx, 48
 call li_put_field
 lea rsi, [rip + .Lhash_paren]
 mov rdi, rbx
 call li_vputs
 mov rdi, rbx
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_CALL_ID]
 mov edx, 48
 call li_put_field
 lea rsi, [rip + .Lparen_started]
 mov rdi, rbx
 call li_vputs
 jmp .Lev_out

.Lev_tool_finished:
 mov rdi, rbx
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT
 mov rdi,rbx
 call li_finish_carry
 mov rdi, rbx
 call li_event_suspend
 lea rsi, [rip + .Ltool_pfx]
 mov rdi, rbx
 call li_vputs
 mov rdi, rbx
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_TOOL_NAME]
 mov edx, 48
 call li_put_field
 lea rsi, [rip + .Lhash_paren]
 mov rdi, rbx
 call li_vputs
 mov rdi, rbx
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_CALL_ID]
 mov edx, 48
 call li_put_field
 mov r13d, dword ptr [r12 + TUI_LI_EVENT_TOOL_OK]
 lea rsi, [rip + .Lfin_ok]
 test r13d, r13d
 jnz .Ltoolf_ok
 lea rsi, [rip + .Lfin_err]
.Ltoolf_ok:
 mov rdi, rbx
 call li_vputs
 mov rdi, rbx
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_DETAIL]
 mov rdx, qword ptr [r12 + TUI_LI_EVENT_DETAIL_LEN]
 mov rcx, TUI_LI_VIEW_TOOL_SNIPPET
 call li_put_bounded
 lea rsi, [rip + .Lnl]
 mov rdi, rbx
 call li_vputs
 jmp .Lev_out

.Lev_finished:
 mov rdi, rbx
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_flush@PLT
 mov rdi,rbx
 call li_finish_carry
.Lfin_no_carry:
 mov rdi, rbx
 call li_event_suspend
 mov eax, dword ptr [r12 + TUI_LI_EVENT_RUN_STATUS]
 cmp eax, TUI_LI_RUN_SUCCESS
 je .Lfin_success
 cmp eax, TUI_LI_RUN_ERROR
 je .Lfin_error
 cmp eax, TUI_LI_RUN_CANCELLED
 je .Lfin_cancelled
 lea rsi, [rip + .Lrun_fin_unknown]
 mov rdi, rbx
 call li_vputs
 jmp .Lfin_redraw
.Lfin_success:
 lea rsi, [rip + .Lrun_fin_ok]
 mov rdi, rbx
 call li_vputs
 jmp .Lfin_redraw
.Lfin_error:
 lea rsi, [rip + .Lrun_fin_err]
 mov rdi, rbx
 call li_vputs
 mov rdi, rbx
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_DETAIL]
 mov rdx, qword ptr [r12 + TUI_LI_EVENT_DETAIL_LEN]
 mov rcx, TUI_LI_VIEW_ERROR_DETAIL
 call li_put_bounded
 lea rsi, [rip + .Lnl]
 mov rdi, rbx
 call li_vputs
 jmp .Lfin_redraw
.Lfin_cancelled:
 lea rsi, [rip + .Lrun_fin_cancel]
 mov rdi, rbx
 call li_vputs
.Lfin_redraw:
 /* 唯一终态后编辑行重新可用（含空缓冲） */
 mov rax, qword ptr [rbx + TUI_LI_VIEW_EDITOR]
 test rax, rax
 jz .Lev_out
 cmp dword ptr [rbx + TUI_LI_VIEW_ANSI], 0
 je .Lev_out
 mov rdi, rbx
 call tui_view_redraw_editor@PLT
.Lev_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_view_event, .-tui_view_event

/* 码点显示宽度（客户端内部共享）：md 渲染折行复用 */
.globl tui_view_cp_width
.type tui_view_cp_width,@function
tui_view_cp_width:
 jmp li_cp_width
.size tui_view_cp_width,.-tui_view_cp_width

.globl tui_view_history
.type tui_view_history,@function
tui_view_history:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13d,[r12+TUI_LI_HISTORY_KIND]
 cmp r13d,3
 ja .Lhistory_view_done
 call li_event_suspend
 lea rsi,[rip+.Lhistory_user]
 test r13d,r13d
 je .Lhistory_view_prefix
 lea rsi,[rip+.Lhistory_assistant]
 cmp r13d,1
 je .Lhistory_view_prefix
 lea rsi,[rip+.Lhistory_call]
 cmp r13d,2
 je .Lhistory_view_prefix
 lea rsi,[rip+.Lhistory_result]
.Lhistory_view_prefix:
 mov rdi,rbx
 call li_vputs
 cmp r13d,2
 jb .Lhistory_view_text
 mov rdi,rbx
 mov rsi,[r12+TUI_LI_HISTORY_NAME]
 mov rdx,[r12+TUI_LI_HISTORY_NAME_LEN]
 call li_safe_vput
 mov rdi,rbx
 lea rsi,[rip+.Lhash_paren]
 call li_vputs
 mov rdi,rbx
 mov rsi,[r12+TUI_LI_HISTORY_ID]
 mov rdx,[r12+TUI_LI_HISTORY_ID_LEN]
 call li_safe_vput
 lea rsi,[rip+.Lhistory_args]
 cmp r13d,2
 je .Lhistory_view_suffix
 lea rsi,[rip+.Lhistory_ok]
 cmp dword ptr [r12+TUI_LI_HISTORY_OK],0
 jne .Lhistory_view_suffix
 lea rsi,[rip+.Lhistory_error]
.Lhistory_view_suffix:
 mov rdi,rbx
 call li_vputs
.Lhistory_view_text:
 mov rdi,rbx
 mov rsi,[r12+TUI_LI_HISTORY_TEXT]
 mov rdx,[r12+TUI_LI_HISTORY_TEXT_LEN]
 mov ecx,4095
 cmp r13d,2
 jb .Lhistory_view_body
 mov ecx,TUI_LI_VIEW_TOOL_SNIPPET
.Lhistory_view_body:
 call li_put_bounded
 mov rdi,rbx
 lea rsi,[rip+.Lnl]
 call li_vputs
.Lhistory_view_done:
 pop r13
 pop r12
 pop rbx
 ret
.size tui_view_history,.-tui_view_history

.section .note.GNU-stack,"",@progbits

.set TUI_W_ZERO_COUNT, (.Lw_zero_end - .Lw_zero) / 4
.set TUI_W_WIDE_COUNT, (.Lw_wide_end - .Lw_wide) / 4
