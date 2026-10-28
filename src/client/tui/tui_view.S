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
.Lhelp: .asciz "[help] PanGu 快捷键\nEnter: 提交；Alt+Enter: 换行；括号粘贴: 安全多行输入\nLeft/Right: 按码点移动；Home/End: 行首/行尾\nUp/Down: 视觉行移动；顶部/底部: 输入历史与草稿\nBackspace/Delete: 删除；Ctrl-U: 清空草稿\nCtrl-O: 展开所有保留工具并保持新调用展开（运行中也可展开）；Ctrl-R: 搜索输入历史；Tab: 补全本地命令；PageUp/PageDown: 阅读历史/回到最新；F1 或 /hotkeys: 快捷键帮助\nCtrl-C: 运行时请求取消，闲置时退出；Ctrl-D: 空草稿退出，非空退格\n/quit: 通过 Runtime shutdown 退出并恢复终端\n[help] 可直接继续编辑当前草稿\n"
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
 cmp dword ptr [rdi+TUI_LI_VIEW_SHELL], 0
 jne li_shell_suspend
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
 cmp dword ptr [rdi+TUI_LI_VIEW_SHELL], 0
 jne tui_view_redraw_editor
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
 mov qword ptr [rdi + TUI_LI_VIEW_LOGO_START], 0
 mov qword ptr [rdi + TUI_LI_VIEW_LOGO_LAST], 0
 mov qword ptr [rdi + TUI_LI_VIEW_LOGO_PHASE], 0
 mov dword ptr [rdi + TUI_LI_VIEW_LOGO_DIRTY], 1
 mov qword ptr [rdi + TUI_LI_VIEW_SHELL], 0
 mov qword ptr [rdi + TUI_LI_VIEW_OBSERVER], 0
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
 lea rdi, [rbx+TUI_LI_VIEW_LOGO_ASPECT]
 lea rsi, [rbx+TUI_LI_VIEW_LOGO_FLAGS]
 lea rdx, [rbx+TUI_LI_VIEW_LOGO_FG]
 lea rcx, [rbx+TUI_LI_VIEW_LOGO_BG]
 call tui_logo_environment@PLT
 test dword ptr [rbx+TUI_LI_VIEW_LOGO_FLAGS], 4
 jz .Linit_logo_motion
 mov dword ptr [rbx+TUI_LI_VIEW_LOGO_PHASE], 65536
.Linit_logo_motion:
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
 cmp dword ptr [rbx+TUI_LI_VIEW_SHELL], 0
 je .Lred_legacy
 mov rdi, rbx
 call li_shell_editor
 jmp .Lred_prompt
.Lred_legacy:
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
.Lred_prompt:
 mov rdi, rbx
 lea rsi, [rip + .Lprompt]
 call li_vputs
 mov qword ptr [rsp], 0 /* cur_row */
 mov qword ptr [rsp + 8], 2 /* cur_col 默认：空稿/无捕获 */
 mov r14d, 0 /* 视觉行 */
 mov r15d, 2 /* 当前列（提示符后） */
 xor r13d, r13d /* 字节偏移 i */
.Lred_loop:
 cmp dword ptr [rbx+TUI_LI_VIEW_SHELL], 0
 je .Lred_space_ok
 cmp r13, [r12+TUI_LI_INPUT_CURSOR]
 jbe .Lred_space_ok
 mov eax, [rbx+TUI_LI_VIEW_TERMINAL_ROWS]
 sub eax, [rbx+TUI_LI_VIEW_BODY_END]
 sub eax, 3
 add eax, dword ptr [rsp]
 cmp r14d, eax
 jae .Lred_end
.Lred_space_ok:
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
 cmp dword ptr [rbx+TUI_LI_VIEW_SHELL], 0
 je .Lred_relative_cursor
 mov eax, [rbx+TUI_LI_VIEW_TERMINAL_ROWS]
 sub eax, [rbx+TUI_LI_VIEW_BODY_END]
 sub eax, 3 /* input rows */
 mov ecx, r14d
 inc ecx
 sub ecx, eax
 jns .Lred_shell_scrolled
 xor ecx, ecx
.Lred_shell_scrolled:
 mov eax, [rsp]
 sub eax, ecx
 jns .Lred_shell_cursor_valid
 xor eax, eax
.Lred_shell_cursor_valid:
 mov [rsp], eax
 add eax, [rbx+TUI_LI_VIEW_BODY_END]
 add eax, 2
 mov ecx, eax
 mov rdi, rsp
 add rdi, 32
 mov esi, 48
 lea rdx, [rip+.Lshell_cursor_fmt]
 mov r8d, [rsp+8]
 inc r8d
 xor eax, eax
 call snprintf@PLT
 mov rdi, rbx
 lea rsi, [rsp+32]
 call li_vputs
 /* Keep horizontal positioning explicit for terminals and existing consumers. */
 mov rdi, rsp
 add rdi, 32
 mov esi, 48
 lea rdx, [rip+.Lfmt_cha]
 mov ecx, [rsp+8]
 inc ecx
 xor eax, eax
 call snprintf@PLT
 mov rdi, rbx
 lea rsi, [rsp+32]
 call li_vputs
 jmp .Lred_store
.Lred_relative_cursor:
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
 cmp dword ptr [rbx+TUI_LI_VIEW_ANSI], 0
 je .Lbanner_legacy
 cmp qword ptr [rbx+TUI_LI_VIEW_EDITOR], 0
 je .Lbanner_legacy
 cmp dword ptr [rbx+TUI_LI_VIEW_TERMINAL_ROWS], 8
 jb .Lbanner_legacy
 mov dword ptr [rbx+TUI_LI_VIEW_SHELL], 1
 mov rdi, rbx
 call li_shell_start
 jmp .Lbanner_skip
.Lbanner_legacy:
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
 cmp dword ptr [rbx+TUI_LI_VIEW_SHELL], 0
 je .Lexit_frame_done
 mov rdi, rbx
 lea rsi, [rip+.Lshell_reset]
 call li_vputs
 mov dword ptr [rbx+TUI_LI_VIEW_SHELL], 0
.Lexit_frame_done:
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
 cmp dword ptr [rbx+TUI_LI_VIEW_SHELL], 0
 je .Lev_started_mode
 mov dword ptr [rbx+TUI_LI_VIEW_SHELL], 2
.Lev_started_mode:
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
 cmp qword ptr [r12 + TUI_LI_EVENT_DETAIL_LEN], 0
 je .Lev_arguments_done
 cmp qword ptr [r12 + TUI_LI_EVENT_DETAIL], 0
 je .Lev_arguments_done
 mov rdi, rbx
 lea rsi, [rip + .Ltool_arguments]
 call li_vputs
 mov rdi, rbx
 mov rsi, [r12 + TUI_LI_EVENT_DETAIL]
 mov rdx, [r12 + TUI_LI_EVENT_DETAIL_LEN]
 mov ecx, TUI_LI_VIEW_TOOL_SNIPPET
 call li_put_bounded
 mov rdi, rbx
 lea rsi, [rip + .Lnl]
 call li_vputs
.Lev_arguments_done:
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
 mov rdi, rbx
 mov rsi, [r12 + TUI_LI_EVENT_CALL_ID]
 call li_observe_tool
 mov rdi, rbx
 call li_event_resume
 jmp .Lev_out

.Lev_finished:
 cmp dword ptr [rbx+TUI_LI_VIEW_SHELL], 0
 je .Lev_finished_mode
 mov dword ptr [rbx+TUI_LI_VIEW_SHELL], 1
.Lev_finished_mode:
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
 mov rdi, rbx
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
 cmp r13d, 3
 jne .Lhistory_view_done
 /* History fields are explicit borrowed slices, not necessarily NUL-terminated.
  * Build a bounded temporary identity before the independent observer lookup. */
 mov rax, [r12 + TUI_LI_HISTORY_ID_LEN]
 test rax, rax
 jz .Lhistory_view_done
 cmp rax, 63
 ja .Lhistory_view_done
 cmp qword ptr [r12 + TUI_LI_HISTORY_ID], 0
 je .Lhistory_view_done
 sub rsp, 160
 mov [rsp + 64], rax
 mov rdi, rsp
 mov rsi, [r12 + TUI_LI_HISTORY_ID]
 mov rdx, rax
 call memcpy@PLT
 mov rax, [rsp + 64]
 mov byte ptr [rsp + rax], 0
 mov rax, [r12+TUI_LI_HISTORY_NAME_LEN]
 test rax, rax
 jz .Lhistory_restore_default
 cmp rax, 63
 ja .Lhistory_restore_default
 cmp qword ptr [r12+TUI_LI_HISTORY_NAME], 0
 je .Lhistory_restore_default
 mov [rsp+144], rax
 lea rdi, [rsp+80]
 mov rsi, [r12+TUI_LI_HISTORY_NAME]
 mov rdx, rax
 call memcpy@PLT
 mov rax, [rsp+144]
 mov byte ptr [rsp+rax+80], 0
 lea rsi, [rsp+80]
 jmp .Lhistory_restore
.Lhistory_restore_default:
 lea rsi, [rip+.Lhistory_tool_default]
.Lhistory_restore:
 lea rdi, [rbx+TUI_LI_VIEW_TOOL]
 mov rdx, rsp
 mov ecx, [r12+TUI_LI_HISTORY_OK]
 call tui_tool_restore@PLT
 mov rdi, rbx
 mov rsi, rsp
 call li_observe_tool
 add rsp, 160
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
.Ltool_arguments: .asciz "[arguments] "
.Ltranscript_tool: .ascii "\n[tool] "
.set .Ltranscript_tool_len, . - .Ltranscript_tool
.Lread_enter: .ascii "\033[?1049h\033[r\033[?1000h\033[?1006h"
.set .Lread_enter_len, . - .Lread_enter
.Lread_leave: .ascii "\033[?1006l\033[?1000l\033[?1049l"
.set .Lread_leave_len, . - .Lread_leave
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
 cmp dword ptr [r12 + TUI_LI_EVENT_KIND], TUI_LI_EV_TOOL_STARTED
 jne .Lrecord_done
 mov rdi, rbx
 lea rsi, [rip + .Lnl]
 mov edx, 1
 call li_transcript_append
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
 cmp dword ptr [rbx+TUI_LI_VIEW_SHELL], 0
 je .Luser_record
 mov rdi, rbx
 call li_shell_suspend
 cmp qword ptr [rbx+TUI_LI_VIEW_TRANSCRIPT_LEN], 0
 jne .Luser_brand_cleared
 mov rdi, rbx
 lea rsi, [rip+.Lbrand_clear]
 call li_vputs
.Luser_brand_cleared:
 mov rdi, rbx
 lea rsi, [rip+.Ltranscript_user]
 mov edx, OFFSET .Ltranscript_user_len
 call li_safe_vput
 mov rdi, rbx
 mov rsi, r12
 mov rdx, r13
 call li_safe_vput
 mov rdi, rbx
 lea rsi, [rip+.Lnl]
 mov edx, 1
 call li_safe_vput
.Luser_record:
 mov rdi, rbx
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
 mov edx, OFFSET .Lread_enter_len
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
 mov edx, OFFSET .Lread_leave_len
 call li_read_raw
 cmp dword ptr [rbx+TUI_LI_VIEW_SHELL], 0
 je .Llatest_body_ready
 mov rdi, rbx
 call li_shell_region_restore
.Llatest_body_ready:
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

.section .rodata
.Lobs_entry: .asciz "entry"
.Lobs_model: .asciz "model"
.Lobs_details: .asciz "details"
.Lobs_diff_key: .asciz "diff"
.Lobs_patch_key: .asciz "patch"
.Lobs_first: .asciz "firstChangedLine"
.Lobs_is_error: .asciz "isError"
.Lobs_line_fmt: .asciz "[edit] first changed line: %lld\n"
.Lobs_failed: .asciz "[tool] committed details unavailable\n"
.section .text
/* Read owned committed observation, render while JSON alive, then release on
 * every path. Execution is never advanced by this read-only optional service. */
.type li_observe_tool,@function
li_observe_tool:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 96
 mov rbx, rdi
 mov r12, rsi
 mov r13, [rbx + TUI_LI_VIEW_OBSERVER]
 test r13, r13
 jz .Lobs_done
 cmp qword ptr [r13 + 8], 0
 je .Lobs_done
 cmp qword ptr [r13 + 16], 0
 je .Lobs_done
 test r12, r12
 jz .Lobs_done
 mov rdi, r12
 call strlen@PLT
 mov rdx, rax
 mov rsi, r12
 mov rdi, [r13]
 mov qword ptr [rsp], 0
 mov rcx, rsp
 call [r13 + 8]
 test eax, eax
 jnz .Lobs_status
 mov r14, [rsp]
 test r14, r14
 jz .Lobs_error
 mov rdi, r14
 mov esi, 4
 call json_object_is_type@PLT
 test eax, eax
 jz .Lobs_release
 mov rdi, r14
 lea rsi, [rip + .Lobs_entry]
 call json_object_object_get@PLT
 test rax, rax
 jz .Lobs_release
 mov r14, rax
 mov rdi, rax
 mov esi, 4
 call json_object_is_type@PLT
 test eax, eax
 jz .Lobs_release
 mov rdi, rbx
 mov rsi, r14
 call li_observe_tool_status
 mov rdi, r14
 lea rsi, [rip + .Lobs_model]
 call json_object_object_get@PLT
 test rax, rax
 jz .Lobs_release
 mov r14, rax
 mov rdi, rax
 mov esi, 5
 call json_object_is_type@PLT
 test eax, eax
 jz .Lobs_release
 mov rdi, r14
 xor esi, esi
 call json_object_array_get_idx@PLT
 test rax, rax
 jz .Lobs_release
 mov r14, rax
 mov rdi, rax
 mov esi, 4
 call json_object_is_type@PLT
 test eax, eax
 jz .Lobs_release
 mov rdi, rbx
 mov rsi, r14
 call li_observe_images
 /* Like Pi formatEditResult(isError), failed results do not describe an
  * applied edit. Only the committed boolean controls this branch. */
 mov rdi, r14
 lea rsi, [rip + .Lobs_is_error]
 call json_object_object_get@PLT
 mov r15, rax
 mov rdi, rax
 mov esi, 1 /* json_type_boolean */
 call json_object_is_type@PLT
 test eax, eax
 jz .Lobs_text_success
 mov rdi, r15
 call json_object_get_boolean@PLT
 test eax, eax
 jz .Lobs_text_success
 mov dword ptr [rsp+8], 1
 jmp .Lobs_text
.Lobs_text_success:
 mov dword ptr [rsp+8], 0
.Lobs_text:
 mov rdi, rbx
 mov rsi, r14
 mov edx, [rsp+8]
 mov rcx, r12
 call li_observe_result_text
 cmp dword ptr [rsp+8], 0
 jne .Lobs_release
.Lobs_result_details:
 mov rdi, r14
 lea rsi, [rip + .Lobs_details]
 call json_object_object_get@PLT
 test rax, rax
 jz .Lobs_release
 mov r15, rax
 mov rdi, rax
 mov esi, 4
 call json_object_is_type@PLT
 test eax, eax
 jz .Lobs_release
 mov rdi, r15
 lea rsi, [rip + .Lobs_first]
 call json_object_object_get@PLT
 test rax, rax
 jz .Lobs_diff
 mov r14, rax
 mov rdi, rax
 mov esi, 3
 call json_object_is_type@PLT
 test eax, eax
 jz .Lobs_diff
 mov rdi, r14
 call json_object_get_int64@PLT
 test rax, rax
 jle .Lobs_diff
 mov rcx, rax
 lea rdi, [rsp + 16]
 mov esi, 80
 lea rdx, [rip + .Lobs_line_fmt]
 xor eax, eax
 call snprintf@PLT
 cmp eax, 0
 jle .Lobs_diff
 cmp eax, 80
 jge .Lobs_diff
 mov edx, eax
 lea rsi, [rsp + 16]
 mov rdi, rbx
 call li_vput
 mov rdi, rbx
 lea rsi, [rsp + 16]
 call strlen@PLT
 mov rdx, rax
 mov rdi, rbx
 lea rsi, [rsp + 16]
 call li_transcript_append
.Lobs_diff:
 mov rdi, r15
 lea rsi, [rip + .Lobs_diff_key]
 call json_object_object_get@PLT
 test rax, rax
 jnz .Lobs_diff_value
 mov rdi, r15
 lea rsi, [rip + .Lobs_patch_key]
 call json_object_object_get@PLT
 test rax, rax
 jz .Lobs_release
.Lobs_diff_value:
 mov r12, rax
 mov rdi, rax
 mov esi, 6 /* json_type_string */
 call json_object_is_type@PLT
 test eax, eax
 jz .Lobs_release
 mov rdi, r12
 call json_object_get_string_len@PLT
 mov r15d, eax
 mov rdi, r12
 call json_object_get_string@PLT
 mov r12, rax
 mov rdi, rbx
 mov rsi, r12
 mov rdx, r15
 call li_transcript_append
 lea rdi, [rbx + TUI_LI_VIEW_SINK_CTX]
 mov esi, [rbx + TUI_LI_VIEW_ANSI]
 mov rdx, r12
 mov rcx, r15
 call tui_diff_render@PLT
.Lobs_release:
 mov rdi, [rsp]
 call [r13 + 16]
 jmp .Lobs_done
.Lobs_status:
 cmp eax, 1
 je .Lobs_release_if_any
.Lobs_error:
 mov rdi, rbx
 lea rsi, [rip + .Lobs_failed]
 call li_vputs
.Lobs_release_if_any:
 cmp qword ptr [rsp], 0
 jne .Lobs_release
.Lobs_done:
 add rsp, 96
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_observe_tool,.-li_observe_tool

.section .rodata
.Limage_content: .asciz "content"
.Limage_type: .asciz "type"
.Limage_value: .asciz "image"
.Limage_mime: .asciz "mimeType"
.Limage_data: .asciz "data"
.Limage_prefix: .asciz "[image] "
.Limage_unknown: .asciz "image/unknown"
.Limage_size: .asciz " (%d base64 bytes; retained in committed tool result)\n"
.section .text
/* Pi tool result image blocks: metadata fallback only; base64 is never emitted.
 * Traversal borrows from the observation and finishes before owner release. */
.type li_observe_images,@function
li_observe_images:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 128
 mov rbx, rdi
 mov rdi, rsi
 lea rsi, [rip + .Limage_content]
 call json_object_object_get@PLT
 test rax, rax
 jz .Limgobs_done
 mov r12, rax
 mov rdi, rax
 mov esi, 5
 call json_object_is_type@PLT
 test eax, eax
 jz .Limgobs_done
 mov rdi, r12
 call json_object_array_length@PLT
 mov r13, rax
 xor r14d, r14d
.Limgobs_loop:
 cmp r14, r13
 jae .Limgobs_done
 mov rdi, r12
 mov rsi, r14
 inc r14
 call json_object_array_get_idx@PLT
 test rax, rax
 jz .Limgobs_loop
 mov r15, rax
 mov rdi, rax
 mov esi, 4
 call json_object_is_type@PLT
 test eax, eax
 jz .Limgobs_loop
 mov rdi, r15
 lea rsi, [rip + .Limage_type]
 call json_object_object_get@PLT
 mov rdi, rax
 mov esi, 6
 call json_object_is_type@PLT
 test eax, eax
 jz .Limgobs_loop
 mov rdi, r15
 lea rsi, [rip + .Limage_type]
 call json_object_object_get@PLT
 mov rdi, rax
 call json_object_get_string@PLT
 mov rdi, rax
 lea rsi, [rip + .Limage_value]
 call strcmp@PLT
 test eax, eax
 jnz .Limgobs_loop
 mov rdi, rbx
 lea rsi, [rip + .Limage_prefix]
 call li_vputs
 mov rdi, r15
 lea rsi, [rip + .Limage_mime]
 call json_object_object_get@PLT
 mov [rsp], rax
 mov rdi, rax
 mov esi, 6
 call json_object_is_type@PLT
 test eax, eax
 jz .Limgobs_unknown
 mov rdi, [rsp]
 call json_object_get_string_len@PLT
 mov [rsp + 8], eax
 mov rdi, [rsp]
 call json_object_get_string@PLT
 mov rdi, rbx
 mov rsi, rax
 mov edx, [rsp + 8]
 mov ecx, 80
 call li_put_bounded
 jmp .Limgobs_bytes
.Limgobs_unknown:
 mov rdi, rbx
 lea rsi, [rip + .Limage_unknown]
 call li_vputs
.Limgobs_bytes:
 mov rdi, r15
 lea rsi, [rip + .Limage_data]
 call json_object_object_get@PLT
 mov [rsp], rax
 mov rdi, rax
 mov esi, 6
 call json_object_is_type@PLT
 test eax, eax
 jz .Limgobs_zero
 mov rdi, [rsp]
 call json_object_get_string_len@PLT
 jmp .Limgobs_size
.Limgobs_zero:
 xor eax, eax
.Limgobs_size:
 mov ecx, eax
 lea rdi, [rsp + 16]
 mov esi, 112
 lea rdx, [rip + .Limage_size]
 xor eax, eax
 call snprintf@PLT
 test eax, eax
 jle .Limgobs_loop
 cmp eax, 112
 jge .Limgobs_loop
 mov edx, eax
 lea rsi, [rsp + 16]
 mov rdi, rbx
 call li_vput
 jmp .Limgobs_loop
.Limgobs_done:
 add rsp, 128
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_observe_images,.-li_observe_images

.section .rodata
.Lstatus_data: .asciz "data"
.Lstatus_key: .asciz "status"
.Lstatus_prefix: .asciz "[tool committed status] "
.section .text
/* The receipt status is authoritative, including aborted/interrupted. Preserve
 * unknown future strings as safe metadata rather than guessing an outcome. */
.type li_observe_tool_status,@function
li_observe_tool_status:
 push rbx
 push r12
 push r13
 mov rbx, rdi
 mov rdi, rsi
 lea rsi, [rip + .Lstatus_data]
 call json_object_object_get@PLT
 test rax, rax
 jz .Lstatus_done
 mov r12, rax
 mov rdi, rax
 mov esi, 4
 call json_object_is_type@PLT
 test eax, eax
 jz .Lstatus_done
 mov rdi, r12
 lea rsi, [rip + .Lstatus_key]
 call json_object_object_get@PLT
 mov r12, rax
 mov rdi, rax
 mov esi, 6
 call json_object_is_type@PLT
 test eax, eax
 jz .Lstatus_done
 mov rdi, r12
 call json_object_get_string_len@PLT
 mov r13d, eax
 test eax, eax
 jz .Lstatus_done
 mov rdi, rbx
 lea rsi, [rip + .Lstatus_prefix]
 call li_vputs
 mov rdi, r12
 call json_object_get_string@PLT
 mov r12, rax
 mov rdi, rbx
 mov rsi, r12
 mov rdx, r13
 mov ecx, 80
 call li_put_bounded
 mov rdi, rbx
 lea rsi, [rip + .Lnl]
 call li_vputs
 lea rdi, [rip + .Lstatus_prefix]
 call strlen@PLT
 mov rdx, rax
 mov rdi, rbx
 lea rsi, [rip + .Lstatus_prefix]
 call li_transcript_append
 mov rdi, rbx
 mov rsi, r12
 mov rdx, r13
 call li_transcript_append
 mov rdi, rbx
 lea rsi, [rip + .Lnl]
 mov edx, 1
 call li_transcript_append
.Lstatus_done:
 pop r13
 pop r12
 pop rbx
 ret
.size li_observe_tool_status,.-li_observe_tool_status

.section .rodata
.Lerror_text_key: .asciz "text"
.Lerror_text_value: .asciz "text"
.Lerror_text_prefix: .ascii "[tool committed error] "
.set .Lerror_text_prefix_len, .-.Lerror_text_prefix
.Lresult_text_prefix: .ascii "[tool committed output] "
.set .Lresult_text_prefix_len, .-.Lresult_text_prefix
.section .text
/* Pi result content.filter(text).map(text).join(newline); edx marks error.
 * Borrow complete
 * strings while the observation is alive; safely display and copy source into
 * the reader before release. Non-text blocks never become error text. */
.type li_observe_result_text,@function
li_observe_result_text:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 64
 mov rbx, rdi
 mov [rsp+48], edx
 mov [rsp+32], rcx
 mov qword ptr [rsp+40], 0
 mov qword ptr [rsp+16], 0
 mov rdi, rsi
 lea rsi, [rip+.Limage_content]
 call json_object_object_get@PLT
 mov r12, rax
 mov rdi, rax
 mov esi, 5
 call json_object_is_type@PLT
 test eax, eax
 jz .Lerrtext_done
 mov rdi, r12
 call json_object_array_length@PLT
 mov r13, rax
 xor r14d, r14d
.Lerrtext_loop:
 cmp r14, r13
 jae .Lerrtext_end
 mov rdi, r12
 mov rsi, r14
 inc r14
 call json_object_array_get_idx@PLT
 mov r15, rax
 mov rdi, rax
 mov esi, 4
 call json_object_is_type@PLT
 test eax, eax
 jz .Lerrtext_loop
 mov rdi, r15
 lea rsi, [rip+.Limage_type]
 call json_object_object_get@PLT
 mov [rsp], rax
 mov rdi, rax
 mov esi, 6
 call json_object_is_type@PLT
 test eax, eax
 jz .Lerrtext_loop
 mov rdi, [rsp]
 call json_object_get_string_len@PLT
 cmp eax, 4
 jne .Lerrtext_loop
 mov rdi, [rsp]
 call json_object_get_string@PLT
 mov rdi, rax
 lea rsi, [rip+.Lerror_text_value]
 call strcmp@PLT
 test eax, eax
 jnz .Lerrtext_loop
 mov rdi, r15
 lea rsi, [rip+.Lerror_text_key]
 call json_object_object_get@PLT
 mov r15, rax
 mov rdi, rax
 mov esi, 6
 call json_object_is_type@PLT
 test eax, eax
 jz .Lerrtext_loop
 mov rdi, r15
 call json_object_get_string_len@PLT
 mov [rsp+8], rax
 mov rdi, r15
 call json_object_get_string@PLT
 mov [rsp], rax
 cmp qword ptr [rsp+16], 0
 jne .Lerrtext_separator
 cmp dword ptr [rsp+48], 0
 je .Lresult_text_header
 lea rsi, [rip+.Lerror_text_prefix]
 mov edx, OFFSET .Lerror_text_prefix_len
 jmp .Lerrtext_framing
.Lresult_text_header:
 lea rdi, [rbx+TUI_LI_VIEW_TOOL]
 mov rsi, [rsp+32]
 call tui_tool_result_begin@PLT
 mov [rsp+40], eax
 lea rsi, [rip+.Lresult_text_prefix]
 mov edx, OFFSET .Lresult_text_prefix_len
 jmp .Lerrtext_framing
.Lerrtext_separator:
 lea rsi, [rip+.Lnl]
 mov edx, 1
.Lerrtext_framing:
 mov [rsp+24], rdx
 mov r15, rsi
 cmp qword ptr [rsp+16], 0
 je .Lresult_frame_direct
 cmp dword ptr [rsp+40], 0
 je .Lresult_frame_direct
 mov rcx, rdx
 mov rdx, rsi
 mov rsi, [rsp+32]
 lea rdi, [rbx+TUI_LI_VIEW_TOOL]
 call tui_tool_output@PLT
 jmp .Lresult_frame_saved
.Lresult_frame_direct:
 mov rdi, rbx
 call li_safe_vput
.Lresult_frame_saved:
 mov rdi, rbx
 mov rsi, r15
 mov rdx, [rsp+24]
 call li_transcript_append
 mov qword ptr [rsp+16], 1
 cmp dword ptr [rsp+40], 0
 je .Lresult_content_direct
 lea rdi, [rbx+TUI_LI_VIEW_TOOL]
 mov rsi, [rsp+32]
 mov rdx, [rsp]
 mov rcx, [rsp+8]
 call tui_tool_output@PLT
 jmp .Lresult_content_saved
.Lresult_content_direct:
 mov rdi, rbx
 mov rsi, [rsp]
 mov rdx, [rsp+8]
 call li_safe_vput
.Lresult_content_saved:
 mov rdi, rbx
 mov rsi, [rsp]
 mov rdx, [rsp+8]
 call li_transcript_append
 jmp .Lerrtext_loop
.Lerrtext_end:
 cmp qword ptr [rsp+16], 0
 je .Lerrtext_done
 cmp dword ptr [rsp+40], 0
 je .Lresult_end_direct
 lea rdi, [rbx+TUI_LI_VIEW_TOOL]
 mov rsi, [rsp+32]
 call tui_tool_result_end@PLT
 jmp .Lresult_end_saved
.Lresult_end_direct:
 mov rdi, rbx
 lea rsi, [rip+.Lnl]
 mov edx, 1
 call li_safe_vput
.Lresult_end_saved:
 mov rdi, rbx
 lea rsi, [rip+.Lnl]
 mov edx, 1
 call li_transcript_append
.Lerrtext_done:
 add rsp, 64
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_observe_result_text,.-li_observe_result_text

.section .rodata
.Lhistory_tool_default: .asciz "tool"

.section .rodata
.Lshell_start: .asciz "\033[r\033[2J\033[H\033[1mPanGu\033[0m  ·  Agent\r\n"
.Lshell_region: .asciz "\033[2;%ur\033[2;1H\0337"
.Lshell_restore: .asciz "\0338"
.Lshell_editor_fmt: .asciz "\033[r\0338\0337\033[%u;1H\033[J\033[%u;1H\033[2mEnter send · Alt+Enter newline · F1 help\033[0m\033[%u;1H────\r\n"
.Lshell_reset: .asciz "\033[r\033[999;1H\r\n"
.section .text
/* One saved output cursor belongs exclusively to the conversation region.
 * Editor redraws restore/save that cursor and use absolute bottom coordinates. */
.type li_shell_start,@function
li_shell_start:
 push rbx
 sub rsp, 64
 mov rbx, rdi
 mov eax, [rbx+TUI_LI_VIEW_TERMINAL_ROWS]
 sub eax, 6
 cmp eax, 2
 jae .Lshell_body_valid
 mov eax, 2
.Lshell_body_valid:
 mov [rbx+TUI_LI_VIEW_BODY_END], eax
 lea rdi, [rbx+TUI_LI_VIEW_LOGO_ASPECT]
 lea rsi, [rbx+TUI_LI_VIEW_LOGO_FLAGS]
 lea rdx, [rbx+TUI_LI_VIEW_LOGO_FG]
 lea rcx, [rbx+TUI_LI_VIEW_LOGO_BG]
 call tui_logo_environment@PLT
 test dword ptr [rbx+TUI_LI_VIEW_LOGO_FLAGS], 4
 jz .Lshell_logo_policy_ready
 mov dword ptr [rbx+TUI_LI_VIEW_LOGO_PHASE], 65536
.Lshell_logo_policy_ready:
 mov rdi, rbx
 lea rsi, [rip+.Lshell_start]
 call li_vputs
.Lshell_welcome_done:
 mov rdi, rsp
 mov esi, 64
 lea rdx, [rip+.Lshell_region]
 mov ecx, [rbx+TUI_LI_VIEW_BODY_END]
 xor eax, eax
 call snprintf@PLT
 mov rdi, rbx
 mov rsi, rsp
 call li_vputs
 mov dword ptr [rbx+TUI_LI_VIEW_LOGO_DIRTY], 1
 /* Defer welcome until startup history/attach have established eligibility. */
 add rsp, 64
 pop rbx
 ret
.size li_shell_start,.-li_shell_start
.type li_shell_suspend,@function
li_shell_suspend:
 cmp dword ptr [rdi+TUI_LI_VIEW_EDITOR_ON_LINE], 0
 je .Lshell_suspend_done
 mov dword ptr [rdi+TUI_LI_VIEW_EDITOR_ON_LINE], 0
 jmp li_shell_region_restore
.Lshell_suspend_done:
 ret
.size li_shell_suspend,.-li_shell_suspend
.type li_shell_editor,@function
li_shell_editor:
 push rbx
 sub rsp, 160
 mov rbx, rdi
 /* If an event has advanced output, save its current position first. */
 cmp dword ptr [rbx+TUI_LI_VIEW_EDITOR_ON_LINE], 0
 jne .Lshell_editor_saved
 lea rsi, [rip+.Lshell_save]
 call li_vputs
.Lshell_editor_saved:
 mov rdi, rbx
 lea rsi, [rip+.Lshell_ready_header]
 cmp dword ptr [rbx+TUI_LI_VIEW_SHELL], 2
 jne .Lshell_header_write
 lea rsi, [rip+.Lshell_running_header]
.Lshell_header_write:
 call li_vputs
 mov rdi, rsp
 mov esi, 160
 lea rdx, [rip+.Lshell_editor_fmt]
 mov ecx, [rbx+TUI_LI_VIEW_BODY_END]
 inc ecx
 mov r8d, [rbx+TUI_LI_VIEW_TERMINAL_ROWS]
 mov r9d, ecx
 xor eax, eax
 call snprintf@PLT
 mov rdi, rbx
 mov rsi, rsp
 call li_vputs
 mov rdi, rsp
 mov esi, 160
 lea rdx, [rip+.Lshell_input_region]
 mov ecx, [rbx+TUI_LI_VIEW_BODY_END]
 add ecx, 2
 mov r8d, [rbx+TUI_LI_VIEW_TERMINAL_ROWS]
 dec r8d
 mov r9d, ecx
 xor eax, eax
 call snprintf@PLT
 mov rdi, rbx
 mov rsi, rsp
 call li_vputs
 mov dword ptr [rbx+TUI_LI_VIEW_EDITOR_ROWS], 0
 mov dword ptr [rbx+TUI_LI_VIEW_CURSOR_ROW], 0
 add rsp, 160
 pop rbx
 ret
.size li_shell_editor,.-li_shell_editor
.section .rodata
.Lshell_save: .asciz "\0337"

.section .rodata
.Lshell_region_resume: .asciz "\033[2;%ur\0338"
.section .text
.type li_shell_region_restore,@function
li_shell_region_restore:
 push rbx
 sub rsp, 64
 mov rbx, rdi
 mov rdi, rsp
 mov esi, 64
 lea rdx, [rip+.Lshell_region_resume]
 mov ecx, [rbx+TUI_LI_VIEW_BODY_END]
 xor eax, eax
 call snprintf@PLT
 mov rdi, rbx
 mov rsi, rsp
 call li_vputs
 mov dword ptr [rbx+TUI_LI_VIEW_EDITOR_ON_LINE], 0
 add rsp, 64
 pop rbx
 ret
.size li_shell_region_restore,.-li_shell_region_restore
.globl tui_view_relayout
.type tui_view_relayout,@function
tui_view_relayout:
 cmp dword ptr [rdi+TUI_LI_VIEW_SHELL], 0
 je .Lrelayout_ret
 cmp dword ptr [rdi+TUI_LI_VIEW_READING], 0
 jne .Lrelayout_ret
 push rbx
 mov rbx, rdi
 call li_shell_start
 mov dword ptr [rbx+TUI_LI_VIEW_EDITOR_ON_LINE], 0
 mov rdi, rbx
 lea rsi, [rbx+TUI_LI_VIEW_TRANSCRIPT]
 mov rdx, [rbx+TUI_LI_VIEW_TRANSCRIPT_LEN]
 call li_safe_vput
 pop rbx
.Lrelayout_ret:
 ret
.size tui_view_relayout,.-tui_view_relayout

.section .rodata
.Lshell_input_region: .asciz "\033[%u;%ur\033[%u;1H"
.Lshell_cursor_fmt: .asciz "\033[%u;%uH"

.section .rodata
.Lshell_ready_header: .asciz "\033[1;1H\033[K\033[1mPanGu\033[0m  ·  ready"
.Lshell_running_header: .asciz "\033[1;1H\033[K\033[1mPanGu\033[0m  ·  running"

.section .rodata
.Lbrand_clear: .asciz "\033[2;1H\033[J"

.section .rodata
.Llogo_rgb: .asciz "\033[38;2;%u;%u;%um"
.Llogo_position: .asciz "\033[%u;%uH"
.Llogo_fallback: .asciz "PanGu"
.Llogo_hide: .asciz "\033[?25l"
.Llogo_show: .asciz "\033[0m\0338\033[?25h"
.Llogo_dim: .asciz "\033[2m"
.section .text
/* Bounded cell shadow: only changed mask/color is emitted; no screen clear.
 * 1600-byte current/previous arrays are view-owned, no per-frame allocation. */
.type li_logo_draw,@function
li_logo_draw:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 40128
 mov rbx, rdi
 cmp qword ptr [rbx+TUI_LI_VIEW_TRANSCRIPT_LEN], 0
 jne .Llogo_draw_done
 cmp dword ptr [rbx+TUI_LI_VIEW_READING], 0
 jne .Llogo_draw_done
 mov eax, [rbx+TUI_LI_VIEW_COLS]
 sub eax, 4
 cmp eax, 8
 jl .Llogo_draw_done
 cmp eax, 40
 jbe .Llogo_w
 mov eax, 40
.Llogo_w:
 mov [rbx+TUI_LI_VIEW_LOGO_WIDTH], eax
 mov eax, [rbx+TUI_LI_VIEW_BODY_END]
 sub eax, 3
 cmp eax, 3
 jl .Llogo_draw_done
 cmp eax, 20
 jbe .Llogo_h
 mov eax, 20
.Llogo_h:
 mov [rbx+TUI_LI_VIEW_LOGO_HEIGHT], eax
 mov ecx, [rbx+TUI_LI_VIEW_BODY_END]
 dec ecx
 sub ecx, eax
 shr ecx, 1
 add ecx, 2
 mov [rbx+TUI_LI_VIEW_LOGO_TOP], ecx
 mov ecx, [rbx+TUI_LI_VIEW_COLS]
 sub ecx, [rbx+TUI_LI_VIEW_LOGO_WIDTH]
 shr ecx, 1
 inc ecx
 mov [rbx+TUI_LI_VIEW_LOGO_LEFT], ecx
 cmp dword ptr [rbx+TUI_LI_VIEW_LOGO_DIRTY], 0
 je .Llogo_shadow_ready
 lea rdi, [rbx+TUI_LI_VIEW_LOGO_PREV]
 mov esi, 255
 mov edx, 1600
 call memset@PLT
 mov dword ptr [rbx+TUI_LI_VIEW_LOGO_DIRTY], 0
.Llogo_shadow_ready:
 mov rdi, rbx
 call li_shell_suspend
 /* Capture logo writes and issue one physical write per changed frame. */
 mov rax, [rbx+TUI_LI_VIEW_TERMINAL]
 mov [rsp+112], rax
 mov rax, [rbx+TUI_LI_VIEW_TERMINAL+8]
 mov [rsp+120], rax
 lea rax, [rsp+128]
 mov [rsp+96], rax
 mov qword ptr [rsp+104], 0
 lea rax, [rsp+96]
 mov [rbx+TUI_LI_VIEW_TERMINAL], rax
 lea rax, [rip+li_logo_buffer_write]
 mov [rbx+TUI_LI_VIEW_TERMINAL+8], rax
 mov rdi, rbx
 lea rsi, [rip+.Llogo_hide]
 call li_vputs
 test dword ptr [rbx+TUI_LI_VIEW_LOGO_FLAGS], 1
 jz .Llogo_wordmark
 lea rdi, [rbx+TUI_LI_VIEW_LOGO_CELLS]
 mov esi, [rbx+TUI_LI_VIEW_LOGO_WIDTH]
 mov edx, [rbx+TUI_LI_VIEW_LOGO_HEIGHT]
 mov ecx, [rbx+TUI_LI_VIEW_LOGO_ASPECT]
 mov r8d, [rbx+TUI_LI_VIEW_LOGO_PHASE]
 mov r9d, 1600
 call tui_logo_raster@PLT
 mov r15, rax
 xor r12d, r12d
.Llogo_cells:
 cmp r12, r15
 jae .Llogo_draw_restore
 movzx r13d, byte ptr [rbx+r12*2+TUI_LI_VIEW_LOGO_CELLS]
 movzx eax, byte ptr [rbx+r12*2+TUI_LI_VIEW_LOGO_CELLS+1]
 /* Edge coverage scales opacity; 0.44 while closed -> 0.25 at rest. */
 mov ecx, [rbx+TUI_LI_VIEW_LOGO_PHASE]
 imul ecx, ecx, 48
 shr ecx, 16
 mov edx, 112
 sub edx, ecx
 imul eax, edx
 shr eax, 8
 and eax, 248 /* eight-level stable color quantization, no dithering */
 mov [rsp+8], eax
 shl eax, 8
 or eax, r13d
 cmp ax, word ptr [rbx+r12*2+TUI_LI_VIEW_LOGO_PREV]
 je .Llogo_cell_next
 mov word ptr [rbx+r12*2+TUI_LI_VIEW_LOGO_PREV], ax
 mov rax, r12
 xor edx, edx
 mov ecx, [rbx+TUI_LI_VIEW_LOGO_WIDTH]
 div rcx
 add eax, [rbx+TUI_LI_VIEW_LOGO_TOP]
 add edx, [rbx+TUI_LI_VIEW_LOGO_LEFT]
 mov ecx, eax
 mov r8d, edx
 mov rdi, rsp
 add rdi, 32
 mov esi, 64
 lea rdx, [rip+.Llogo_position]
 xor eax, eax
 call snprintf@PLT
 mov rdi, rbx
 lea rsi, [rsp+32]
 call li_vputs
 test r13d, r13d
 jz .Llogo_cell_blank
 test dword ptr [rbx+TUI_LI_VIEW_LOGO_FLAGS], 2
 jz .Llogo_cell_dim
 xor r14d, r14d
.Llogo_channel:
 mov ecx, r14d
 shl ecx, 3
 mov eax, [rbx+TUI_LI_VIEW_LOGO_FG]
 shr eax, cl
 and eax, 255
 mov edx, [rbx+TUI_LI_VIEW_LOGO_BG]
 shr edx, cl
 and edx, 255
 sub eax, edx
 imul eax, [rsp+8]
 sar eax, 8
 add eax, edx
 mov [rsp+16+r14*4], eax
 inc r14d
 cmp r14d, 3
 jb .Llogo_channel
 mov rdi, rsp
 add rdi, 32
 mov esi, 64
 lea rdx, [rip+.Llogo_rgb]
 mov ecx, [rsp+24]
 mov r8d, [rsp+20]
 mov r9d, [rsp+16]
 xor eax, eax
 call snprintf@PLT
 mov rdi, rbx
 lea rsi, [rsp+32]
 call li_vputs
 jmp .Llogo_cell_utf8
.Llogo_cell_dim:
 mov rdi, rbx
 lea rsi, [rip+.Llogo_dim]
 call li_vputs
.Llogo_cell_utf8:
 /* U+2800+mask, exactly one terminal column in supported terminals. */
 mov byte ptr [rsp+32], 0xe2
 mov eax, r13d
 shr eax, 6
 or eax, 0xa0
 mov [rsp+33], al
 mov eax, r13d
 and eax, 63
 or eax, 0x80
 mov [rsp+34], al
 mov rdi, rbx
 lea rsi, [rsp+32]
 mov edx, 3
 call li_vput
 jmp .Llogo_cell_next
.Llogo_cell_blank:
 mov rdi, rbx
 lea rsi, [rip+.Ld_logo_space]
 mov edx, 1
 call li_vput
.Llogo_cell_next:
 inc r12
 jmp .Llogo_cells
.Llogo_wordmark:
 mov ecx, [rbx+TUI_LI_VIEW_BODY_END]
 shr ecx, 1
 inc ecx
 mov r8d, [rbx+TUI_LI_VIEW_COLS]
 sub r8d, 5
 shr r8d, 1
 inc r8d
 lea rdi, [rsp+32]
 mov esi, 64
 lea rdx, [rip+.Llogo_position]
 xor eax, eax
 call snprintf@PLT
 mov rdi, rbx
 lea rsi, [rsp+32]
 call li_vputs
 mov rdi, rbx
 lea rsi, [rip+.Llogo_fallback]
 call li_vputs
.Llogo_draw_restore:
 mov rdi, rbx
 lea rsi, [rip+.Llogo_show]
 call li_vputs
 mov rax, [rsp+112]
 mov [rbx+TUI_LI_VIEW_TERMINAL], rax
 mov rax, [rsp+120]
 mov [rbx+TUI_LI_VIEW_TERMINAL+8], rax
 mov rdi, [rsp+112]
 lea rsi, [rsp+128]
 mov rdx, [rsp+104]
 call rax
.Llogo_draw_done:
 add rsp, 40128
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_logo_draw,.-li_logo_draw
.section .rodata
.Ld_logo_space: .ascii " "
.section .text
/* Deadline-based one-shot opening: 2 seconds, smoothstep interpolation.
 * Input/history/hidden or unsupported marks settle once, then return -1.
 * Caller uses returned timeout to bound poll; finished logos never wake it. */
.globl tui_view_logo_tick
.type tui_view_logo_tick,@function
tui_view_logo_tick:
 push rbx
 push r12
 push r13
 mov rbx, rdi
 mov r12, rsi
 cmp dword ptr [rbx+TUI_LI_VIEW_SHELL], 1
 jne .Llogo_tick_idle
 cmp qword ptr [rbx+TUI_LI_VIEW_TRANSCRIPT_LEN], 0
 jne .Llogo_tick_idle
 cmp dword ptr [rbx+TUI_LI_VIEW_READING], 0
 jne .Llogo_tick_idle
 cmp dword ptr [rbx+TUI_LI_VIEW_COLS], 12
 jb .Llogo_tick_idle
 cmp dword ptr [rbx+TUI_LI_VIEW_BODY_END], 6
 jb .Llogo_tick_idle
 cmp dword ptr [rbx+TUI_LI_VIEW_LOGO_DIRTY], 0
 je .Llogo_tick_policy
 mov rdi, rbx
 call li_logo_draw
 mov rdi, rbx
 call tui_view_redraw_editor
.Llogo_tick_policy:
 mov eax, [rbx+TUI_LI_VIEW_LOGO_FLAGS]
 and eax, 5
 cmp eax, 1
 jne .Llogo_tick_idle
 cmp dword ptr [rbx+TUI_LI_VIEW_LOGO_PHASE], 65536
 jae .Llogo_tick_idle
 mov rax, [rbx+TUI_LI_VIEW_EDITOR]
 cmp qword ptr [rax+TUI_LI_INPUT_LEN], 0
 jne .Llogo_tick_settle
 cmp qword ptr [rbx+TUI_LI_VIEW_LOGO_START], 0
 jne .Llogo_tick_started
 mov [rbx+TUI_LI_VIEW_LOGO_START], r12
 mov [rbx+TUI_LI_VIEW_LOGO_LAST], r12
 mov eax, 50
 jmp .Llogo_tick_out
.Llogo_tick_started:
 mov rax, r12
 sub rax, [rbx+TUI_LI_VIEW_LOGO_LAST]
 cmp rax, 50
 jb .Llogo_tick_wait
 mov rax, r12
 sub rax, [rbx+TUI_LI_VIEW_LOGO_START]
 cmp rax, 2000
 jae .Llogo_tick_settle
 shl rax, 16
 xor edx, edx
 mov ecx, 2000
 div rcx
 mov rcx, rax
 imul rax, rax
 shr rax, 16
 mov edx, 196608
 lea rcx, [rcx+rcx]
 sub rdx, rcx
 imul rax, rdx
 shr rax, 16
 mov [rbx+TUI_LI_VIEW_LOGO_PHASE], eax
 jmp .Llogo_tick_draw
.Llogo_tick_settle:
 mov dword ptr [rbx+TUI_LI_VIEW_LOGO_PHASE], 65536
.Llogo_tick_draw:
 mov [rbx+TUI_LI_VIEW_LOGO_LAST], r12
 mov rdi, rbx
 call li_logo_draw
 mov rdi, rbx
 call tui_view_redraw_editor
 cmp dword ptr [rbx+TUI_LI_VIEW_LOGO_PHASE], 65536
 jae .Llogo_tick_idle
 mov eax, 50
 jmp .Llogo_tick_out
.Llogo_tick_wait:
 mov ecx, 50
 sub ecx, eax
 mov eax, ecx
 jmp .Llogo_tick_out
.Llogo_tick_idle:
 mov eax, -1
.Llogo_tick_out:
 pop r13
 pop r12
 pop rbx
 ret
.size tui_view_logo_tick,.-tui_view_logo_tick

.section .text
/* Physical frame staging (40000-byte static bound, only trusted logo output).
 * At most 800 cells * (cursor + RGB + 3 UTF8 bytes) plus framing. */
.type li_logo_buffer_write,@function
li_logo_buffer_write:
 push rbx
 push r12
 sub rsp, 8
 mov rbx, rdi
 mov r12, rdx
 mov rax, [rbx+8]
 add rax, rdx
 cmp rax, 40000
 ja .Llogo_buffer_full
 mov rdi, [rbx]
 add rdi, [rbx+8]
 call memcpy@PLT
 add [rbx+8], r12
 xor eax, eax
 jmp .Llogo_buffer_done
.Llogo_buffer_full:
 mov eax, -1
.Llogo_buffer_done:
 add rsp, 8
 pop r12
 pop rbx
 ret
.size li_logo_buffer_write,.-li_logo_buffer_write
