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
.Lsearch_match: .asciz "[match] "
.Lsearch_failed: .asciz "[search] Unicode 转换不可用；可 Ctrl-C 恢复草稿\n"
.Lsearch_idle: .asciz "[search] 输入内容开始查找\n"
.Lsearch_no_match: .asciz "[search] 无匹配\n"
.Lhistory_search: .asciz "[history search] 输入查询；Ctrl-R/Up: 较早；Ctrl-S/Down: 较新；Enter: 采用；Esc/Ctrl-C: 恢复草稿\n"
.Lcompletions: .asciz "[commands] /hotkeys  /quit\n> /hotkeys\n  /quit\nUp/Down: 选择；Enter: 填入；Esc/Ctrl-C: 关闭\n"
.Lcompletions_quit: .asciz "[commands] /hotkeys  /quit\n  /hotkeys\n> /quit\nUp/Down: 选择；Enter: 填入；Esc/Ctrl-C: 关闭\n"
.Lhelp: .asciz "[help] PanGu 快捷键\nEnter: 提交；Alt+Enter: 换行；括号粘贴: 安全多行输入\nLeft/Right: 按码点移动；Home/End: 行首/行尾\nUp/Down: 视觉行移动；顶部/底部: 输入历史与草稿\nBackspace/Delete: 删除；Ctrl-U: 清空草稿\nCtrl-O: 展开所有保留工具的隐藏输出（运行中也可展开）；Ctrl-R: 搜索输入历史；Tab: 补全本地命令；PageUp/PageDown: 阅读历史/回到最新；F1 或 /hotkeys: 快捷键帮助\nCtrl-C: 运行时请求取消，闲置时退出；Ctrl-D: 空草稿退出，非空退格\n/quit: 通过 Runtime shutdown 退出并恢复终端\n[help] 可直接继续编辑当前草稿\n"
.Linput_hint:       .asciz "[tui] 输入：Enter 提交，Alt+Enter 插入换行，Ctrl-U 清空草稿，F1 帮助；行首 Up 浏览输入历史，行尾 Down 返回草稿\n"
.Lqmark:            .asciz "?"
.Lhash_paren:       .asciz " (#"
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

/* ---- emit 回调（tui_tool_emit 实现）与内部通道：转义/换行翻译委托
 * tui_safe.S（视图、工具、diff 共享同一实现）。 ----
 * tui_view_raw_put(ctx=v, data, len)：不转义；ANSI 模式 '\n'→"\r\n"。
 * tui_view_safe_put(ctx=v, data, len)：控制字节转义 + 换行翻译。 */
.globl tui_view_raw_put
.type tui_view_raw_put,@function
tui_view_raw_put:
 mov ecx, edx          /* len */
 mov rdx, rsi          /* data（先于 esi 覆写取走） */
 mov esi, dword ptr [rdi + TUI_LI_VIEW_ANSI]
 lea rdi, [rdi + TUI_LI_VIEW_SINK_CTX]
 jmp tui_raw_write@PLT
.size tui_view_raw_put,.-tui_view_raw_put

.globl tui_view_safe_put
.type tui_view_safe_put,@function
tui_view_safe_put:
 mov ecx, edx
 mov rdx, rsi
 mov esi, dword ptr [rdi + TUI_LI_VIEW_ANSI]
 lea rdi, [rdi + TUI_LI_VIEW_SINK_CTX]
 jmp tui_safe_write@PLT
.size tui_view_safe_put,.-tui_view_safe_put

/* ---- 内部：vput(tui_view *v, const char *data, size_t len) ---- */
.type li_vput,@function
li_vput:
 mov ecx, edx          /* len */
 mov rdx, rsi          /* data（先于 esi 覆写取走） */
 mov esi, dword ptr [rdi + TUI_LI_VIEW_ANSI]
 lea rdi, [rdi + TUI_LI_VIEW_SINK_CTX]
 jmp tui_raw_write@PLT
.size li_vput, .-li_vput

/* Escape control bytes in data, preserving complete UTF-8 and newline/tab. */
.type li_safe_vput,@function
li_safe_vput:
 mov ecx, edx
 mov rdx, rsi
 mov esi, dword ptr [rdi + TUI_LI_VIEW_ANSI]
 lea rdi, [rdi + TUI_LI_VIEW_SINK_CTX]
 jmp tui_safe_write@PLT
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
 mov [rdi + TUI_LI_VIEW_TERMINAL], rsi
 mov [rdi + TUI_LI_VIEW_TERMINAL + 8], rdx
 mov qword ptr [rdi + TUI_LI_VIEW_SINK_CTX], rdi
 lea rax, [rip + li_view_emit]
 mov qword ptr [rdi + TUI_LI_VIEW_SINK_WRITE], rax
 mov dword ptr [rdi + TUI_LI_VIEW_READING], 0
 mov dword ptr [rdi + TUI_LI_VIEW_TERMINAL_ROWS], 24
 mov qword ptr [rdi + TUI_LI_VIEW_TRANSCRIPT_LEN], 0
 mov qword ptr [rdi + TUI_LI_VIEW_READ_ANCHOR], 0
 mov qword ptr [rdi + TUI_LI_VIEW_READ_START], 0
 mov dword ptr [rdi + TUI_LI_VIEW_PAGE_ROWS], 10
 mov dword ptr [rdi + TUI_LI_VIEW_DROPPED], 0
 mov dword ptr [rdi + TUI_LI_VIEW_ANSI], ecx
 mov qword ptr [rdi + TUI_LI_VIEW_EDITOR], r8
 test r8, r8
 jz .Linit_no_editor
 mov dword ptr [r8 + TUI_LI_INPUT_COLUMNS], 80
.Linit_no_editor:
 mov dword ptr [rdi + TUI_LI_VIEW_EDITOR_ON_LINE], 0
 mov dword ptr [rdi + TUI_LI_VIEW_COLS], 80
 mov dword ptr [rdi + TUI_LI_VIEW_EDITOR_ROWS], 0
 mov dword ptr [rdi + TUI_LI_VIEW_CURSOR_ROW], 0
 mov dword ptr [rdi + TUI_LI_VIEW_CURSOR_COL], 0
 /* md 渲染器：同一 sink/模式，默认 80 列（set_size 同步更新） */
 push rbx
 push r12
 push rax
 mov rbx, rdi
 mov r12d, ecx
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 mov rsi, qword ptr [rbx + TUI_LI_VIEW_SINK_CTX]
 mov rdx, qword ptr [rbx + TUI_LI_VIEW_SINK_WRITE]
 mov ecx, r12d
 mov r8d, 80
 call tui_md_init@PLT
 /* tool 状态机：emit 回调指向视图自身的安全/原始通道 */
 lea rdi, [rbx + TUI_LI_VIEW_TOOL]
 mov rsi, rbx
 lea rdx, [rip + tui_view_safe_put]
 lea rcx, [rip + tui_view_raw_put]
 mov r8d, r12d
 call tui_tool_init@PLT
 pop rax
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
 mov rax, [rbx + TUI_LI_VIEW_EDITOR]
 test rax, rax
 jz .Lsize_no_editor
 mov dword ptr [rax + TUI_LI_INPUT_COLUMNS], r12d
 mov dword ptr [rax + TUI_LI_INPUT_PREF_VALID], 0
.Lsize_no_editor:
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
 cmp dword ptr [rdi + TUI_LI_VIEW_READING], 0
 jne .Lred_ret
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
 /* A partially typed codepoint can sit before an existing complete suffix.
  * Omit that prefix during every redraw, including stream/resize/paste paths. */
 cmp dword ptr [r12 + TUI_LI_INPUT_UTF8_PENDING], 0
 je .Lred_complete_range
 mov rcx, [r12 + TUI_LI_INPUT_CURSOR]
 cmp r13, rcx
 jae .Lred_complete_range
 lea rdx, [r13 + rax]
 cmp rdx, rcx
 jbe .Lred_complete_range
 mov r13, rcx
 jmp .Lred_loop
.Lred_complete_range:
 mov rcx, qword ptr [r12 + TUI_LI_INPUT_LEN]
 sub rcx, r13
 cmp rax, rcx
 jbe .Lred_len_ok
 mov [rsp], r14
 mov [rsp + 8], r15
 jmp .Lred_have_cur
.Lred_len_ok:
 mov qword ptr [rsp + 16], rax
 mov rdi, r12
 add rdi, r13
 mov rsi, rax
 call tui_utf8_cp_width@PLT
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
 call tui_view_return_latest
 mov r13d, eax /* snprintf 返回值暂存（先格式化，与 C 版顺序一致） */
 /* 退出说明前收束未完模型行与 carry，失败路径不丢已收到的输出 */
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_flush@PLT
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
 call li_transcript_event
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

/* ---- tool_output：经 tui_tool 状态机（行缓冲/预览/保留/展开由其管理） ---- */
.Lev_delta:
 mov rdi, rbx
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT
 mov rdi, rbx
 call li_event_suspend
 lea rdi, [rbx + TUI_LI_VIEW_TOOL]
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_CALL_ID]
 mov rdx, qword ptr [r12 + TUI_LI_EVENT_TEXT]
 mov rcx, qword ptr [r12 + TUI_LI_EVENT_TEXT_LEN]
 call tui_tool_output@PLT
 test eax, eax
 jz .Lev_out
 mov rdi, rbx
 call li_event_resume
 jmp .Lev_out

.Lev_tool_started:
 mov rdi, rbx
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT
 mov rdi, rbx
 call li_event_suspend
 lea rdi, [rbx + TUI_LI_VIEW_TOOL]
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_TOOL_NAME]
 mov rdx, qword ptr [r12 + TUI_LI_EVENT_CALL_ID]
 call tui_tool_started@PLT
 test eax, eax
 jz .Lev_out
 mov rdi, rbx
 call li_event_resume
 jmp .Lev_out

.Lev_tool_finished:
 mov rdi, rbx
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT
 mov rdi, rbx
 call li_event_suspend
 lea rdi, [rbx + TUI_LI_VIEW_TOOL]
 mov rsi, qword ptr [r12 + TUI_LI_EVENT_CALL_ID]
 mov edx, dword ptr [r12 + TUI_LI_EVENT_TOOL_OK]
 mov rcx, qword ptr [r12 + TUI_LI_EVENT_DETAIL]
 mov r8, qword ptr [r12 + TUI_LI_EVENT_DETAIL_LEN]
 call tui_tool_finished@PLT
 test eax, eax
 jz .Lev_out
 mov rdi, rbx
 call li_event_resume
 jmp .Lev_out

.Lev_finished:
 mov rdi, rbx
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_flush@PLT
.Lfin_no_carry:
 mov rdi, rbx
 call li_event_suspend
 lea rdi, [rbx + TUI_LI_VIEW_TOOL]
 call tui_tool_flush@PLT
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
 jmp tui_utf8_cp_width@PLT
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
 mov rdi, rbx
 lea rsi, [rip + .Ltranscript_history]
 mov edx, OFFSET .Ltranscript_history_len
 call li_transcript_append
 mov rdi, rbx
 mov rsi, [r12 + TUI_LI_HISTORY_TEXT]
 mov rdx, [r12 + TUI_LI_HISTORY_TEXT_LEN]
 call li_transcript_append
 mov rdi, rbx
 lea rsi, [rip + .Lnl]
 mov edx, 1
 call li_transcript_append
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

/* void tui_view_tool_expand(tui_view *v) — Ctrl-O：展开当前保留工具调用的
 * 保留输出。展开输出前把编辑行收束进回显，完成后按需重绘草稿。 */
.globl tui_view_tool_expand
.type tui_view_tool_expand,@function
tui_view_tool_expand:
 push rbx
 push r12
 push rax
 mov rbx, rdi
 call li_event_suspend
 lea rdi, [rbx + TUI_LI_VIEW_TOOL]
 call tui_tool_expand@PLT
 test eax, eax
 jz .Lvte_out
 mov rdi, rbx
 call li_event_resume
.Lvte_out:
 pop rax
 pop r12
 pop rbx
 ret
.size tui_view_tool_expand,.-tui_view_tool_expand

.section .note.GNU-stack,"",@progbits

.section .text
/* Pi hotkeys help, adapted to currently implemented native bindings.
 * Flush active model line before the fixed help block, then restore editor. */
.globl tui_view_help
.type tui_view_help,@function
tui_view_help:
 push rbx
 mov rbx, rdi
 call li_event_suspend
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT
 mov rdi, rbx
 lea rsi, [rip + .Lhelp]
 call li_vputs
 mov rdi, rbx
 call tui_view_redraw_editor@PLT
 pop rbx
 ret
.size tui_view_help,.-tui_view_help

.section .text
.globl tui_view_completions
.type tui_view_completions,@function
tui_view_completions:
 push rbx
 mov rbx, rdi
 call li_event_suspend
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT
 mov rdi, rbx
 lea rsi, [rip + .Lcompletions]
 mov rax, [rbx + TUI_LI_VIEW_EDITOR]
 test rax, rax
 jz .Lcompletion_write
 cmp dword ptr [rax + TUI_LI_INPUT_COMPLETION_SEL], 0
 je .Lcompletion_write
 lea rsi, [rip + .Lcompletions_quit]
.Lcompletion_write:
 call li_vputs
 mov rdi, rbx
 call tui_view_redraw_editor@PLT
 pop rbx
 ret
.size tui_view_completions,.-tui_view_completions

.section .text
.globl tui_view_history_search
.type tui_view_history_search,@function
tui_view_history_search:
 push rbx
 mov rbx, rdi
 call li_event_suspend
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT
 mov rdi, rbx
 lea rsi, [rip + .Lhistory_search]
 call li_vputs
 mov rax, [rbx + TUI_LI_VIEW_EDITOR]
 test rax, rax
 jz .Lsearch_draw
 lea rsi, [rip + .Lsearch_failed]
 cmp dword ptr [rax + TUI_LI_INPUT_SEARCH_FAILED], 0
 jne .Lsearch_status
 movsxd rcx, dword ptr [rax + TUI_LI_INPUT_HISTORY_INDEX]
 test rcx, rcx
 js .Lsearch_missing
 push rax
 push rcx
 mov rdi, rbx
 lea rsi, [rip + .Lsearch_match]
 call li_vputs
 pop rcx
 pop rax
 mov edx, [rax + rcx*4 + TUI_LI_INPUT_HISTORY_LENS]
 shl rcx, 10
 lea rsi, [rax + rcx + TUI_LI_INPUT_HISTORY]
 mov rdi, rbx
 call li_vput
 mov rdi, rbx
 lea rsi, [rip + .Lnl]
 call li_vputs
 jmp .Lsearch_draw
.Lsearch_missing:
 lea rsi, [rip + .Lsearch_no_match]
 cmp qword ptr [rax + TUI_LI_INPUT_LEN], 0
 jne .Lsearch_status
 lea rsi, [rip + .Lsearch_idle]
.Lsearch_status:
 mov rdi, rbx
 call li_vputs
.Lsearch_draw:
 mov rdi, rbx
 call tui_view_redraw_editor@PLT
 pop rbx
 ret
.size tui_view_history_search,.-tui_view_history_search

.section .rodata
.Ltranscript_user: .ascii "\n[user] "
.set .Ltranscript_user_len, . - .Ltranscript_user
.Ltranscript_history: .ascii "\n[history] "
.set .Ltranscript_history_len, . - .Ltranscript_history
.Ltranscript_tool: .ascii "\n[tool] "
.set .Ltranscript_tool_len, . - .Ltranscript_tool
.Lread_enter: .ascii "\033[?1049h"
.Lread_leave: .ascii "\033[?1049l"
.Lread_clear: .ascii "\033[H\033[2J"
.Lread_header: .asciz "[history] PgUp/PgDn"
.Lread_dropped: .asciz "[history] older buffered text was evicted"
.Lread_backlog: .asciz "\n[history] output received while reading:\n"
.section .text

/* Physical sink proxy: source events continue updating state during reading,
 * but live repaint writes cannot move the frozen reading viewport. */
.type li_view_emit,@function
li_view_emit:
 cmp dword ptr [rdi + TUI_LI_VIEW_READING], 0
 jne .Lemit_hidden
 mov rax, [rdi + TUI_LI_VIEW_TERMINAL + 8]
 mov rdi, [rdi + TUI_LI_VIEW_TERMINAL]
 jmp rax
.Lemit_hidden:
 xor eax, eax
 ret
.size li_view_emit,.-li_view_emit

/* Owned source transcript, 64KiB. Eviction removes complete old source lines;
 * anchors are adjusted by the same cut, preserving reading position until its
 * actual source bytes are evicted. Oversize chunks retain a UTF-8-safe suffix. */
.type li_transcript_append,@function
li_transcript_append:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 test r13, r13
 jz .Lta_done
 cmp r13, 65536
 jbe .Lta_room_view
 mov dword ptr [rbx + TUI_LI_VIEW_DROPPED], 1
 sub r13, 65536
 add r12, r13
 mov r13d, 65536
.Lta_align_view:
 test r13, r13
 jz .Lta_done
 movzx eax, byte ptr [r12]
 and eax, 192
 cmp eax, 128
 jne .Lta_room_view
 inc r12
 dec r13
 jmp .Lta_align_view
.Lta_room_view:
 mov r15, [rbx + TUI_LI_VIEW_TRANSCRIPT_LEN]
 lea rax, [r15 + r13]
 cmp rax, 65536
 jbe .Lta_copy_view
 xor r14d, r14d
.Lta_find_cut_view:
 cmp r14, r15
 jae .Lta_cut_view
 inc r14
 cmp byte ptr [rbx + r14 + TUI_LI_VIEW_TRANSCRIPT - 1], 10
 jne .Lta_find_cut_view
.Lta_cut_view:
 mov dword ptr [rbx + TUI_LI_VIEW_DROPPED], 1
 lea rdi, [rbx + TUI_LI_VIEW_TRANSCRIPT]
 lea rsi, [rdi + r14]
 mov rdx, r15
 sub rdx, r14
 call memmove@PLT
 sub r15, r14
 mov [rbx + TUI_LI_VIEW_TRANSCRIPT_LEN], r15
 mov rax, [rbx + TUI_LI_VIEW_READ_ANCHOR]
 mov rcx, r14
 shl rcx, 5
 sub rax, rcx
 jnc .Lta_anchor_ok
 xor eax, eax
.Lta_anchor_ok:
 mov [rbx + TUI_LI_VIEW_READ_ANCHOR], rax
 mov rax, [rbx + TUI_LI_VIEW_READ_START]
 sub rax, r14
 jnc .Lta_start_ok
 xor eax, eax
.Lta_start_ok:
 mov [rbx + TUI_LI_VIEW_READ_START], rax
 jmp .Lta_room_view
.Lta_copy_view:
 lea rdi, [rbx + r15 + TUI_LI_VIEW_TRANSCRIPT]
 mov rsi, r12
 mov rdx, r13
 call memcpy@PLT
 add r15, r13
 mov [rbx + TUI_LI_VIEW_TRANSCRIPT_LEN], r15
.Lta_done:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_transcript_append,.-li_transcript_append

/* Called with event in r12, view in rbx; never retains borrowed event slices. */
.type li_transcript_event,@function
li_transcript_event:
 sub rsp, 8
 mov eax, [r12 + TUI_LI_EVENT_KIND]
 cmp eax, TUI_LI_EV_TEXT_DELTA
 je .Lrecord_text
 cmp eax, TUI_LI_EV_TOOL_OUTPUT
 je .Lrecord_text
 cmp eax, TUI_LI_EV_TOOL_STARTED
 je .Lrecord_tool
 cmp eax, TUI_LI_EV_TOOL_FINISHED
 je .Lrecord_detail
 cmp eax, TUI_LI_EV_RUN_FINISHED
 jne .Lrecord_done
 lea rsi, [rip + .Lrun_fin_unknown]
 mov eax, [r12 + TUI_LI_EVENT_RUN_STATUS]
 cmp eax, TUI_LI_RUN_SUCCESS
 jne .Lrecord_error
 lea rsi, [rip + .Lrun_fin_ok]
 jmp .Lrecord_string
.Lrecord_error:
 cmp eax, TUI_LI_RUN_CANCELLED
 jne .Lrecord_failure
 lea rsi, [rip + .Lrun_fin_cancel]
 jmp .Lrecord_string
.Lrecord_failure:
 lea rsi, [rip + .Lrun_fin_err]
 jmp .Lrecord_string
.Lrecord_tool:
 mov rdi, rbx
 lea rsi, [rip + .Ltranscript_tool]
 mov edx, OFFSET .Ltranscript_tool_len
 call li_transcript_append
 mov rsi, [r12 + TUI_LI_EVENT_TOOL_NAME]
 test rsi, rsi
 jz .Lrecord_done
.Lrecord_string:
 mov [rsp], rsi
 mov rdi, rsi
 call strlen@PLT
 mov rsi, [rsp]
 mov rdx, rax
 mov rdi, rbx
 call li_transcript_append
 jmp .Lrecord_done
.Lrecord_detail:
 mov rsi, [r12 + TUI_LI_EVENT_DETAIL]
 mov rdx, [r12 + TUI_LI_EVENT_DETAIL_LEN]
 jmp .Lrecord_bytes
.Lrecord_text:
 mov rsi, [r12 + TUI_LI_EVENT_TEXT]
 mov rdx, [r12 + TUI_LI_EVENT_TEXT_LEN]
.Lrecord_bytes:
 test rsi, rsi
 jz .Lrecord_done
 mov rdi, rbx
 call li_transcript_append
.Lrecord_done:
 add rsp, 8
 ret
.size li_transcript_event,.-li_transcript_event

.globl tui_view_user
.type tui_view_user,@function
tui_view_user:
 push rbx
 push r12
 push r13
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 lea rsi, [rip + .Ltranscript_user]
 mov edx, OFFSET .Ltranscript_user_len
 call li_transcript_append
 mov rdi, rbx
 mov rsi, r12
 mov rdx, r13
 call li_transcript_append
 mov rdi, rbx
 lea rsi, [rip + .Lnl]
 mov edx, 1
 call li_transcript_append
 pop r13
 pop r12
 pop rbx
 ret
.size tui_view_user,.-tui_view_user

/* Direct physical output helpers, bypass the read-mode suppression proxy. */
.type li_read_raw,@function
li_read_raw:
 mov rcx, rdx
 mov rdx, rsi
 mov esi, 1
 lea rdi, [rbx + TUI_LI_VIEW_TERMINAL]
 jmp tui_raw_write@PLT
.size li_read_raw,.-li_read_raw
.type li_read_string,@function
li_read_string:
 push rsi
 mov rdi, rsi
 call strlen@PLT
 pop rsi
 mov rdx, rax
 jmp li_read_raw
.size li_read_string,.-li_read_string


/* A display position packs source byte offset in high bits and a printable
 * escape expansion offset in low 5 bits. No fabricated character is skipped. */
.type li_read_token,@function
li_read_token:
 push rbp
 sub rsp, 48
 mov rbp, rdi
 mov rax, r12
 shr rax, 5
 mov [rsp], rax
 mov rcx, r12
 and ecx, 31
 mov [rsp + 8], rcx
 lea rdi, [rbx + rax + TUI_LI_VIEW_TRANSCRIPT]
 mov [rsp + 24], rdi
 mov qword ptr [rsp + 16], 1
 movzx eax, byte ptr [rdi]
 cmp eax, 10
 je .Lrt_newline
 cmp eax, 9
 je .Lrt_tab
 cmp eax, 32
 jb .Lrt_byte_escape
 cmp eax, 127
 je .Lrt_byte_escape
 cmp eax, 128
 jae .Lrt_unicode
 mov ecx, 1
 mov edx, 1
 mov rsi, rdi
 jmp .Lrt_direct
.Lrt_newline:
 mov edx, 1
 xor ecx, ecx
 mov rsi, rdi
 mov r8d, 1
 jmp .Lrt_advance_direct
.Lrt_tab:
 mov word ptr [rbp], 0x2020
 mov byte ptr [rbp + 2], 32
 mov qword ptr [rsp + 32], 3 /* Pi display TAB policy */
 jmp .Lrt_expanded
.Lrt_unicode:
 mov rsi, [rbx + TUI_LI_VIEW_TRANSCRIPT_LEN]
 sub rsi, [rsp]
 call tui_safe_seq_len@PLT
 test eax, eax
 jz .Lrt_invalid
 mov [rsp + 16], rax
 mov rdi, [rsp + 24]
 cmp byte ptr [rdi], 194
 jne .Lrt_width
 cmp byte ptr [rdi + 1], 159
 ja .Lrt_width
 mov dword ptr [rbp], 0x3030755c /* \u00 */
 movzx eax, byte ptr [rdi + 1]
 lea rsi, [rip + .Lhex]
 mov ecx, eax
 shr ecx, 4
 and eax, 15
 mov cl, [rsi + rcx]
 mov al, [rsi + rax]
 mov [rbp + 4], cl
 mov [rbp + 5], al
 mov qword ptr [rsp + 32], 6
 jmp .Lrt_expanded
.Lrt_width:
 mov rsi, [rsp + 16]
 call tui_utf8_cp_width@PLT
 mov ecx, eax
 cmp ecx, [rbx + TUI_LI_VIEW_COLS]
 ja .Lrt_wide_escape
 mov rsi, [rsp + 24]
 mov edx, [rsp + 16]
 jmp .Lrt_direct
.Lrt_invalid:
 mov rdi, [rsp + 24]
 movzx eax, byte ptr [rdi]
.Lrt_byte_escape:
 mov word ptr [rbp], 0x785c
 lea rsi, [rip + .Lhex]
 mov ecx, eax
 shr ecx, 4
 and eax, 15
 mov cl, [rsi + rcx]
 mov al, [rsi + rax]
 mov [rbp + 2], cl
 mov [rbp + 3], al
 mov qword ptr [rsp + 32], 4
 jmp .Lrt_expanded
.Lrt_wide_escape:
 xor r9d, r9d
 mov rdi, [rsp + 24]
 lea rsi, [rip + .Lhex]
.Lrt_wide_loop:
 movzx eax, byte ptr [rdi + r9]
 mov ecx, eax
 shr ecx, 4
 and eax, 15
 mov cl, [rsi + rcx]
 mov al, [rsi + rax]
 mov word ptr [rbp + r9*4], 0x785c
 mov [rbp + r9*4 + 2], cl
 mov [rbp + r9*4 + 3], al
 inc r9
 cmp r9, [rsp + 16]
 jb .Lrt_wide_loop
 shl r9, 2
 mov [rsp + 32], r9
.Lrt_expanded:
 mov rax, [rsp + 8]
 lea rsi, [rbp + rax]
 inc rax
 cmp rax, [rsp + 32]
 jae .Lrt_expansion_end
 lea rax, [r12 + 1]
 jmp .Lrt_cell
.Lrt_expansion_end:
 mov rax, [rsp]
 add rax, [rsp + 16]
 shl rax, 5
.Lrt_cell:
 mov edx, 1
 mov ecx, 1
 xor r8d, r8d
 jmp .Lrt_return
.Lrt_direct:
 xor r8d, r8d
.Lrt_advance_direct:
 mov rax, [rsp]
 add rax, [rsp + 16]
 shl rax, 5
.Lrt_return:
 add rsp, 48
 pop rbp
 ret
.size li_read_token,.-li_read_token

/* Two bounded streaming passes locate visual rows without a full row table.
 * The same token iterator renders a page, including partial escape expansions. */
.equ PDIR, 0
.equ PEND, 8
.equ PANCHOR, 16
.equ PTARGET, 24
.equ PNEXT, 32
.equ PPTR, 40
.equ PLEN, 48
.equ PWIDTH, 52
.equ PNL, 56
.equ PBUF, 64
.equ PHEAD, 80
.equ PBUDGET, 84
.equ PHEIGHT, 88
.equ PFRAME, 96
.globl tui_view_page
.type tui_view_page,@function
tui_view_page:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, PFRAME
 mov rbx, rdi
 mov [rsp + PDIR], esi
 cmp dword ptr [rbx + TUI_LI_VIEW_ANSI], 0
 je .Lpage_done
 cmp dword ptr [rbx + TUI_LI_VIEW_READING], 0
 jne .Lpage_count_init
 test esi, esi
 jns .Lpage_done
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT
 mov rdi, rbx
 call li_editor_break
 mov rax, [rbx + TUI_LI_VIEW_TRANSCRIPT_LEN]
 mov [rbx + TUI_LI_VIEW_READ_START], rax
 shl rax, 5
 mov [rbx + TUI_LI_VIEW_READ_ANCHOR], rax
 mov dword ptr [rbx + TUI_LI_VIEW_READING], 1
 lea rsi, [rip + .Lread_enter]
 mov edx, 8
 call li_read_raw
.Lpage_count_init:
 mov eax, [rbx + TUI_LI_VIEW_TERMINAL_ROWS]
 test eax, eax
 jnz .Lpage_height_valid
 mov eax, 1
.Lpage_height_valid:
 mov [rsp + PHEIGHT], eax
 xor ecx, ecx
 cmp eax, 1
 jbe .Lpage_headers_set
 inc ecx
 cmp eax, 3
 jb .Lpage_headers_set
 cmp dword ptr [rbx + TUI_LI_VIEW_DROPPED], 0
 je .Lpage_headers_set
 inc ecx
.Lpage_headers_set:
 mov [rsp + PHEAD], ecx
 sub eax, ecx
 cmp eax, [rbx + TUI_LI_VIEW_PAGE_ROWS]
 jbe .Lpage_budget_set
 mov eax, [rbx + TUI_LI_VIEW_PAGE_ROWS]
.Lpage_budget_set:
 test eax, eax
 jnz .Lpage_budget_valid
 mov eax, 1
.Lpage_budget_valid:
 mov [rsp + PBUDGET], eax
 mov rax, [rbx + TUI_LI_VIEW_TRANSCRIPT_LEN]
 shl rax, 5
 mov [rsp + PEND], rax
 xor r12d, r12d /* packed position */
 xor r13d, r13d /* visual row */
 xor r14d, r14d /* column */
 mov qword ptr [rsp + PANCHOR], 0
.Lpage_count:
 cmp r12, [rbx + TUI_LI_VIEW_READ_ANCHOR]
 ja .Lpage_count_token
 mov [rsp + PANCHOR], r13
.Lpage_count_token:
 cmp r12, [rsp + PEND]
 jae .Lpage_choose
 lea rdi, [rsp + PBUF]
 call li_read_token
 test r8d, r8d
 jnz .Lpage_count_lf
 test r14d, r14d
 jz .Lpage_count_fit
 lea edx, [r14 + rcx]
 cmp edx, [rbx + TUI_LI_VIEW_COLS]
 jbe .Lpage_count_fit
 inc r13
 xor r14d, r14d
 jmp .Lpage_count
.Lpage_count_fit:
 add r14d, ecx
 mov r12, rax
 jmp .Lpage_count
.Lpage_count_lf:
 inc r13
 xor r14d, r14d
 mov r12, rax
 jmp .Lpage_count
.Lpage_choose:
 mov r15, [rsp + PANCHOR]
 mov eax, [rsp + PDIR]
 test eax, eax
 jz .Lpage_target
 mov ecx, [rsp + PBUDGET]
 js .Lpage_older
 add r15, rcx
 cmp r15, r13
 ja .Lpage_latest
 jmp .Lpage_target
.Lpage_older:
 sub r15, rcx
 jnc .Lpage_target
 xor r15d, r15d
.Lpage_target:
 mov [rsp + PTARGET], r15
 xor r12d, r12d
 xor r13d, r13d
 xor r14d, r14d
.Lpage_seek:
 cmp r13, r15
 je .Lpage_draw
 cmp r12, [rsp + PEND]
 jae .Lpage_latest
 lea rdi, [rsp + PBUF]
 call li_read_token
 test r8d, r8d
 jnz .Lpage_seek_lf
 test r14d, r14d
 jz .Lpage_seek_fit
 lea edx, [r14 + rcx]
 cmp edx, [rbx + TUI_LI_VIEW_COLS]
 jbe .Lpage_seek_fit
 inc r13
 xor r14d, r14d
 jmp .Lpage_seek
.Lpage_seek_fit:
 add r14d, ecx
 mov r12, rax
 jmp .Lpage_seek
.Lpage_seek_lf:
 inc r13
 xor r14d, r14d
 mov r12, rax
 jmp .Lpage_seek
.Lpage_draw:
 cmp r12, [rsp + PEND]
 jb .Lpage_draw_valid
 cmp dword ptr [rsp + PDIR], 0
 jg .Lpage_latest
.Lpage_draw_valid:
 mov [rbx + TUI_LI_VIEW_READ_ANCHOR], r12
 lea rsi, [rip + .Lread_clear]
 mov edx, 7
 call li_read_raw
 cmp dword ptr [rsp + PHEAD], 0
 je .Lpage_body_init
 lea rsi, [rip + .Lread_header]
 call li_read_label
 cmp dword ptr [rsp + PHEAD], 2
 jb .Lpage_body_init
 lea rsi, [rip + .Lread_dropped]
 call li_read_label
.Lpage_body_init:
 xor r13d, r13d
 xor r14d, r14d
.Lpage_emit:
 cmp r12, [rsp + PEND]
 jae .Lpage_eof
 lea rdi, [rsp + PBUF]
 call li_read_token
 mov [rsp + PNEXT], rax
 mov [rsp + PPTR], rsi
 mov [rsp + PLEN], edx
 mov [rsp + PWIDTH], ecx
 mov [rsp + PNL], r8d
 test r8d, r8d
 jnz .Lpage_emit_token
 test r14d, r14d
 jz .Lpage_emit_token
 lea eax, [r14 + rcx]
 cmp eax, [rbx + TUI_LI_VIEW_COLS]
 jbe .Lpage_emit_token
 mov eax, r13d
 add eax, [rsp + PHEAD]
 inc eax
 cmp eax, [rsp + PHEIGHT]
 jae .Lpage_done
 lea rsi, [rip + .Lnl]
 mov edx, 1
 call li_read_raw
 inc r13d
 xor r14d, r14d
 cmp r13d, [rsp + PBUDGET]
 jae .Lpage_done
 jmp .Lpage_emit
.Lpage_emit_token:
 cmp dword ptr [rsp + PNL], 0
 je .Lpage_token_fits_height
 mov eax, r13d
 add eax, [rsp + PHEAD]
 inc eax
 cmp eax, [rsp + PHEIGHT]
 jae .Lpage_done
.Lpage_token_fits_height:
 mov rsi, [rsp + PPTR]
 mov edx, [rsp + PLEN]
 call li_read_raw
 mov r12, [rsp + PNEXT]
 cmp dword ptr [rsp + PNL], 0
 jne .Lpage_emit_lf
 add r14d, [rsp + PWIDTH]
 jmp .Lpage_emit
.Lpage_emit_lf:
 inc r13d
 xor r14d, r14d
 cmp r13d, [rsp + PBUDGET]
 jb .Lpage_emit
 jmp .Lpage_done
.Lpage_eof:
 test r14d, r14d
 jz .Lpage_done
 mov eax, r13d
 add eax, [rsp + PHEAD]
 inc eax
 cmp eax, [rsp + PHEIGHT]
 jae .Lpage_done
 lea rsi, [rip + .Lnl]
 mov edx, 1
 call li_read_raw
 jmp .Lpage_done
.Lpage_latest:
 mov rdi, rbx
 call tui_view_return_latest
.Lpage_done:
 add rsp, PFRAME
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size tui_view_page,.-tui_view_page

/* Fixed ASCII status text fits one terminal row even after a narrow resize. */
.type li_read_label,@function
li_read_label:
 push r12
 mov r12, rsi
 mov rdi, rsi
 call strlen@PLT
 mov edx, [rbx + TUI_LI_VIEW_COLS]
 cmp rax, rdx
 jbe .Lread_label_len
 mov rax, rdx
.Lread_label_len:
 mov rdx, rax
 mov rsi, r12
 call li_read_raw
 lea rsi, [rip + .Lnl]
 mov edx, 1
 call li_read_raw
 pop r12
 ret
.size li_read_label,.-li_read_label


.globl tui_view_return_latest
.type tui_view_return_latest,@function
tui_view_return_latest:
 cmp dword ptr [rdi + TUI_LI_VIEW_READING], 0
 je .Llatest_ret
 push rbx
 mov rbx, rdi
 lea rdi, [rbx + TUI_LI_VIEW_MD]
 call tui_md_break@PLT
 lea rdi, [rbx + TUI_LI_VIEW_TOOL]
 call tui_tool_flush@PLT
 mov dword ptr [rbx + TUI_LI_VIEW_READING], 0
 lea rsi, [rip + .Lread_leave]
 mov edx, 8
 call li_read_raw
 mov rax, [rbx + TUI_LI_VIEW_TRANSCRIPT_LEN]
 cmp rax, [rbx + TUI_LI_VIEW_READ_START]
 jbe .Llatest_editor
 lea rsi, [rip + .Lread_backlog]
 call li_read_string
 mov rcx, [rbx + TUI_LI_VIEW_TRANSCRIPT_LEN]
 mov rax, [rbx + TUI_LI_VIEW_READ_START]
 sub rcx, rax
 lea rdx, [rbx + rax + TUI_LI_VIEW_TRANSCRIPT]
 lea rdi, [rbx + TUI_LI_VIEW_TERMINAL]
 mov esi, 1
 call tui_safe_write@PLT
.Llatest_editor:
 mov rdi, rbx
 call tui_view_redraw_editor
 pop rbx
.Llatest_ret:
 ret
.size tui_view_return_latest,.-tui_view_return_latest
