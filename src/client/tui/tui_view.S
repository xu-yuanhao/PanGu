/* Scrollback rendering; UTF-8 carry joins precede untrusted control escaping. */
#include "tui_layout.h"
.intel_syntax noprefix

.section .rodata
.Lcrlf:             .asciz "\r\n"
.Lcleartail:        .asciz "\r\033[J"
.Lprompt:           .asciz "> "
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

/* ---- 内部：event_suspend(v)：事件输出前结束编辑行 ---- */
.type li_event_suspend,@function
li_event_suspend:
 cmp dword ptr [rdi + TUI_LI_VIEW_ANSI], 0
 je .Lsusp_ret
 cmp dword ptr [rdi + TUI_LI_VIEW_EDITOR_ON_LINE], 0
 je .Lsusp_ret
 push rbx
 push r12
 push rax
 mov rbx, rdi
 lea rsi, [rip + .Lcrlf]
 call li_vputs
 mov dword ptr [rbx + TUI_LI_VIEW_EDITOR_ON_LINE], 0
 pop rax
 pop r12
 pop rbx
.Lsusp_ret:
 ret
.size li_event_suspend, .-li_event_suspend

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
 ret
 .cfi_endproc
.size tui_view_init, .-tui_view_init

/* void tui_view_redraw_editor(tui_view *v) — 整行重绘（"\r"+清行尾+提示符+缓冲） */
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
 push rax
 mov rbx, rdi
 mov r12, rax /* editor（buf 偏移 0） */
 cmp dword ptr [rbx + TUI_LI_VIEW_EDITOR_ON_LINE], 0
 je .Lred_no_clear
 mov rdi, rbx
 lea rsi, [rip + .Lcleartail]
 call li_vputs
.Lred_no_clear:
 mov rdi, rbx
 lea rsi, [rip + .Lprompt]
 call li_vputs
 mov rdi, rbx
 mov rsi, r12
 mov rdx, qword ptr [r12 + TUI_LI_INPUT_LEN]
 call li_vput
 mov dword ptr [rbx + TUI_LI_VIEW_EDITOR_ON_LINE], 1
 pop rax
 pop r12
 pop rbx
.Lred_ret:
 ret
 .cfi_endproc
.size tui_view_redraw_editor, .-tui_view_redraw_editor

/* void tui_view_finalize_editor(tui_view *v) */
.globl tui_view_finalize_editor
.type tui_view_finalize_editor,@function
tui_view_finalize_editor:
 .cfi_startproc
 cmp dword ptr [rdi + TUI_LI_VIEW_ANSI], 0
 je .Lfin_ret
 cmp dword ptr [rdi + TUI_LI_VIEW_EDITOR_ON_LINE], 0
 je .Lfin_ret
 push rbx
 push r12
 push rax
 mov rbx, rdi
 lea rsi, [rip + .Lcrlf]
 call li_vputs
 mov dword ptr [rbx + TUI_LI_VIEW_EDITOR_ON_LINE], 0
 pop rax
 pop r12
 pop rbx
.Lfin_ret:
 ret
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
.Lbanner_skip:
 mov dword ptr [rbx + TUI_LI_VIEW_EDITOR_ON_LINE], 0
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
 mov rdi, rsp
 mov esi, 160
 lea rdx, [rip + .Lfmt_exit]
 mov rcx, r12
 xor eax, eax
 call snprintf@PLT
 mov r13d, eax
 cmp dword ptr [rbx + TUI_LI_VIEW_ANSI], 0
 je .Lexit_no_susp
 cmp dword ptr [rbx + TUI_LI_VIEW_EDITOR_ON_LINE], 0
 je .Lexit_no_susp
 mov rdi, rbx
 lea rsi, [rip + .Lcrlf]
 call li_vputs
.Lexit_no_susp:
 mov dword ptr [rbx + TUI_LI_VIEW_EDITOR_ON_LINE], 0
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
 je .Lev_delta
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

/* ---- text_delta：carry 拼接 + 尾部不完整序列留存 ---- */
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
