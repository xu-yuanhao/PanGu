/* tui_md.S — 流式行级 Markdown 渲染器，手写汇编（纯逻辑，经 sink 输出）。
 *
 * 移植 Pi packages/tui/src/components/markdown.ts 的必要块级行为：
 * 段落、标题、列表（有序/嵌套/任务）、引用、围栏代码块、分隔线、管道表格、
 * 链接与流式未闭合块；内联支持代码、粗体、斜体、删除线、
 * [text](url) 与 <autolink>。与 Pi 的差异（记录于 TODO）：滚动回显无法重写
 * 已滚出内容，段落不跨源行重排，每源行独立按终端列宽折行；宽度复用
 * tui_view_cp_width 的码点粒度近似。
 *
 * 结构：md_process_line 为块状态机（围栏→表格→空行→开栏→分隔线→标题→
 * 引用→列表→含|行入候选→段落）。所有字面量经 md_validate/md_put_safe
 * 转义（C1 控制区 \u00XX、非法/控制字节 \xHH、TAB→3 空格，与视图工具
 * 输出通道语义一致）。围栏内疑似部分闭栏行经 hold 暂扣一拍，消除流式
 * 收尾闪烁（对应 Pi trimPartialClosingFences）。
 *
 * SysV AMD64：内部函数保持 rbx/r12-r15；带 PLT 调用的帧保持 16 字节对齐。
 */
#include "tui_layout.h"
.intel_syntax noprefix

/* 样式位 → SGR 码：1=bold(1) 2=italic(3) 4=underline(4) 8=strike(9) 16=code(7) */
.equ MD_BOLD,     1
.equ MD_ITALIC,   2
.equ MD_UNDERLINE, 4
.equ MD_STRIKE,   8
.equ MD_CODE,     16

/* 内联上下文（输出包装帧栈上，56 字节） */
.equ MDC_MD,    0
.equ MDC_BUF,   8
.equ MDC_LEN,   16
.equ MDC_CAP,   24
.equ MDC_COL,   32
.equ MDC_CONT,  40
.equ MDC_MASK,  48
.equ MDC_SIZE,  56

.equ MD_STAGE_CAP, 2304

.section .rodata
.Lmd_crlf:      .ascii "\r\n"
.Lmd_lf:        .ascii "\n"
.Lmd_sp3:       .ascii "   "
.Lmd_indent:    .ascii "  "
.Lmd_qbar:      .ascii "\xe2\x94\x82 "                  /* │ 空格 */
.Lmd_cellemd:   .ascii " \xe2\x94\x82"                  /* 空格 │ */
.Lmd_hbar:      .ascii "\xe2\x94\x80"                   /* ─ */
.Lmd_ellipsis:  .ascii "\xe2\x80\xa6"                   /* … */
.Lmd_topleft:   .ascii "\xe2\x94\x8c\xe2\x94\x80"       /* ┌─ */
.Lmd_topright:  .ascii "\xe2\x94\x80\xe2\x94\x90"       /* ─┐ */
.Lmd_midleft:   .ascii "\xe2\x94\x9c\xe2\x94\x80"       /* ├─ */
.Lmd_midright:  .ascii "\xe2\x94\x80\xe2\x94\xa4"       /* ─┤ */
.Lmd_botleft:   .ascii "\xe2\x94\x94\xe2\x94\x80"       /* └─ */
.Lmd_botright:  .ascii "\xe2\x94\x80\xe2\x94\x98"       /* ┘ */
.Lmd_colmid:    .ascii "\xe2\x94\x80\xe2\x94\xac\xe2\x94\x80" /* ─┬─ */
.Lmd_rowmid:    .ascii "\xe2\x94\x80\xe2\x94\xbc\xe2\x94\x80" /* ─┼─ */
.Lmd_botmid:    .ascii "\xe2\x94\x80\xe2\x94\xb4\xe2\x94\x80" /* ─┴─ */
.Lmd_cellsep:   .ascii " \xe2\x94\x82 "                 /* 空格│空格 */
.Lmd_hex:       .ascii "0123456789abcdef"
.Lmd_sgr_codes: .ascii "013497"   /* 按位序 1..5：bold italic underline strike code */
.Lmd_http:      .ascii "http://"
.Lmd_https:     .ascii "https://"
.Lmd_mailto:    .ascii "mailto:"
.Lmd_punct:     .ascii "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"
.Lmd_linkpre:   .ascii " ("
.Lmd_linkpost:  .ascii ")"
.Lmd_sp32:      .ascii "                                "

.section .text

/* ============ 入口 ============ */

/* void tui_md_init(tui_md*, sink.ctx, sink.write, ansi, cols) */
.globl tui_md_init
.type tui_md_init,@function
tui_md_init:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov qword ptr [rbx + TUI_LI_MD_SINK_CTX], rsi
 mov qword ptr [rbx + TUI_LI_MD_SINK_WRITE], rdx
 mov dword ptr [rbx + TUI_LI_MD_ANSI], ecx
 test r8d, r8d
 jnz .Lmdinit_cols
 mov r8d, 1
.Lmdinit_cols:
 mov dword ptr [rbx + TUI_LI_MD_COLS], r8d
 /* 清状态字（line_len 起，至表格区前）：1128 字节 */
 lea rdi, [rbx + TUI_LI_MD_LINE_LEN]
 xor eax, eax
 mov ecx, 141
 cld
 rep stosq
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_md_init,.-tui_md_init

/* void tui_md_set_cols(tui_md*, unsigned cols) */
.globl tui_md_set_cols
.type tui_md_set_cols,@function
tui_md_set_cols:
 .cfi_startproc
 test esi, esi
 jnz .Lsetc_ok
 mov esi, 1
.Lsetc_ok:
 mov dword ptr [rdi + TUI_LI_MD_COLS], esi
 ret
 .cfi_endproc
.size tui_md_set_cols,.-tui_md_set_cols

/* int tui_md_text(tui_md *md, const char *bytes, size_t len) */
.globl tui_md_text
.type tui_md_text,@function
tui_md_text:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 16
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 xor r14d, r14d
 mov r15d, dword ptr [rbx + TUI_LI_MD_WROTE_ANY]
.Ltxt_loop:
 cmp r14, r13
 jae .Ltxt_done
 movzx eax, byte ptr [r12 + r14]
 cmp eax, 13
 je .Ltxt_next
 cmp eax, 10
 je .Ltxt_newline
 mov edx, dword ptr [rbx + TUI_LI_MD_LINE_LEN]
 cmp edx, TUI_LI_MD_LINE_MAX - 1
 jae .Ltxt_split
 mov byte ptr [rbx + TUI_LI_MD_LINE + rdx], al
 inc dword ptr [rbx + TUI_LI_MD_LINE_LEN]
 jmp .Ltxt_next
.Ltxt_split:
 /* 行满：回退到码点边界，前段先按段落行输出，余段前移 */
 mov ecx, edx
.Ltxt_kback:
 test ecx, ecx
 jz .Ltxt_kset
 mov eax, ecx
 dec eax
 movzx eax, byte ptr [rbx + TUI_LI_MD_LINE + rax]
 and eax, 0xC0
 cmp eax, 0x80
 jne .Ltxt_kset
 dec ecx
 jmp .Ltxt_kback
.Ltxt_kset:
 test ecx, ecx
 jnz .Ltxt_khave
 mov ecx, 1
.Ltxt_khave:
 /* k 存栈槽：caller-saved 寄存器过不了下面的调用 */
 mov qword ptr [rsp], rcx
 mov rdi, rbx
 lea rsi, [rbx + TUI_LI_MD_LINE]
 mov edx, dword ptr [rsp]
 xor ecx, ecx
 xor r8d, r8d
 call md_plain_line
 /* memmove(line, line+k, len-k)；当前字节在调用后重取 */
 mov rdi, rbx
 add rdi, TUI_LI_MD_LINE
 mov rsi, rdi
 add rsi, qword ptr [rsp]
 mov edx, dword ptr [rbx + TUI_LI_MD_LINE_LEN]
 sub edx, dword ptr [rsp]
 call memmove@PLT
 mov eax, dword ptr [rsp]
 sub dword ptr [rbx + TUI_LI_MD_LINE_LEN], eax
 movzx eax, byte ptr [r12 + r14]
 mov edx, dword ptr [rbx + TUI_LI_MD_LINE_LEN]
 mov byte ptr [rbx + TUI_LI_MD_LINE + rdx], al
 inc dword ptr [rbx + TUI_LI_MD_LINE_LEN]
 jmp .Ltxt_next
.Ltxt_newline:
 mov rdi, rbx
 lea rsi, [rbx + TUI_LI_MD_LINE]
 mov edx, dword ptr [rbx + TUI_LI_MD_LINE_LEN]
 call md_process_line
 mov dword ptr [rbx + TUI_LI_MD_LINE_LEN], 0
.Ltxt_next:
 inc r14
 jmp .Ltxt_loop
.Ltxt_done:
 mov eax, dword ptr [rbx + TUI_LI_MD_WROTE_ANY]
 cmp eax, r15d
 setne al
 movzx eax, al
 add rsp, 16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_md_text,.-tui_md_text

/* void tui_md_break(tui_md *md) — 未完行先按当前块语义输出 */
.globl tui_md_break
.type tui_md_break,@function
tui_md_break:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 cmp dword ptr [rbx + TUI_LI_MD_LINE_LEN], 0
 je .Lbrk_out
 mov rdi, rbx
 lea rsi, [rbx + TUI_LI_MD_LINE]
 mov edx, dword ptr [rbx + TUI_LI_MD_LINE_LEN]
 call md_process_line
 mov dword ptr [rbx + TUI_LI_MD_LINE_LEN], 0
.Lbrk_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_md_break,.-tui_md_break

/* void tui_md_flush(tui_md *md) — run 终态收尾 */
.globl tui_md_flush
.type tui_md_flush,@function
tui_md_flush:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 cmp dword ptr [rbx + TUI_LI_MD_LINE_LEN], 0
 je .Lfl_noline
 mov rdi, rbx
 lea rsi, [rbx + TUI_LI_MD_LINE]
 mov edx, dword ptr [rbx + TUI_LI_MD_LINE_LEN]
 call md_process_line
 mov dword ptr [rbx + TUI_LI_MD_LINE_LEN], 0
.Lfl_noline:
 mov dword ptr [rbx + TUI_LI_MD_FENCE], 0
 mov dword ptr [rbx + TUI_LI_MD_HOLD_LEN], 0
 cmp dword ptr [rbx + TUI_LI_MD_TABLE_ON], 0
 je .Lfl_notab
 mov rdi, rbx
 call md_render_table
 mov dword ptr [rbx + TUI_LI_MD_TABLE_ON], 0
 mov dword ptr [rbx + TUI_LI_MD_TABLE_ROWS_N], 0
.Lfl_notab:
 cmp dword ptr [rbx + TUI_LI_MD_CAND_LEN], 0
 je .Lfl_nocand
 mov rdi, rbx
 lea rsi, [rbx + TUI_LI_MD_CAND]
 mov edx, dword ptr [rbx + TUI_LI_MD_CAND_LEN]
 xor ecx, ecx
 xor r8d, r8d
 call md_plain_line
 mov dword ptr [rbx + TUI_LI_MD_CAND_LEN], 0
.Lfl_nocand:
 mov dword ptr [rbx + TUI_LI_MD_LAST_BLANK], 0
 mov dword ptr [rbx + TUI_LI_MD_WROTE_ANY], 0
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_md_flush,.-tui_md_flush

/* ============ 输出原语 ============ */

/* md_put_raw(rdi=md, rsi=ptr, rdx=len) — sink 直写 */
.type md_put_raw,@function
md_put_raw:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 test r13, r13
 jz .Lmpr_out
 mov rdi, qword ptr [rbx + TUI_LI_MD_SINK_CTX]
 mov rsi, r12
 mov rdx, r13
 mov rax, qword ptr [rbx + TUI_LI_MD_SINK_WRITE]
 call rax
.Lmpr_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_put_raw,.-md_put_raw

/* md_nl(rdi=md) — 行分隔（ANSI=\r\n，降级=\n） */
.type md_nl,@function
md_nl:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 lea rsi, [rip + .Lmd_crlf]
 mov edx, 2
 cmp dword ptr [rbx + TUI_LI_MD_ANSI], 0
 jne .Lmnl_go
 lea rsi, [rip + .Lmd_lf]
 mov edx, 1
.Lmnl_go:
 mov rdi, rbx
 call md_put_raw
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_nl,.-md_nl

/* md_sgr_build(rdi=buf, esi=mask) → eax 长度；"\x1b[0;1;3m" 形 */
.type md_sgr_build,@function
md_sgr_build:
 .cfi_startproc
 mov byte ptr [rdi], 0x1B
 mov byte ptr [rdi + 1], '['
 mov byte ptr [rdi + 2], '0'
 mov edx, 3
 mov ecx, esi
 mov r8d, 1
 lea r9, [rip + .Lmd_sgr_codes]
.Lsgr_loop:
 test ecx, ecx
 jz .Lsgr_done
 mov al, [r9 + r8]
 test ecx, 1
 jz .Lsgr_next
 mov byte ptr [rdi + rdx], ';'
 mov byte ptr [rdi + rdx + 1], al
 add edx, 2
.Lsgr_next:
 shr ecx, 1
 inc r8d
 jmp .Lsgr_loop
.Lsgr_done:
 mov byte ptr [rdi + rdx], 'm'
 inc edx
 mov eax, edx
 ret
 .cfi_endproc
.size md_sgr_build,.-md_sgr_build

/* md_stage_flush(rdi=ctx) */
.type md_stage_flush,@function
md_stage_flush:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12, qword ptr [rbx + MDC_LEN]
 test r12, r12
 jz .Lmsf_out
 mov rax, qword ptr [rbx + MDC_MD]
 mov rdi, qword ptr [rax + TUI_LI_MD_SINK_CTX]
 mov rsi, qword ptr [rbx + MDC_BUF]
 mov rdx, r12
 call qword ptr [rax + TUI_LI_MD_SINK_WRITE]
 mov qword ptr [rbx + MDC_LEN], 0
.Lmsf_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_stage_flush,.-md_stage_flush

/* md_stage(rdi=ctx, rsi=src, rdx=n) — 追加并按需冲写 */
.type md_stage,@function
md_stage:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
.Lmst_loop:
 test r13, r13
 jz .Lmst_out
 mov r14, qword ptr [rbx + MDC_CAP]
 sub r14, qword ptr [rbx + MDC_LEN]
 test r14, r14
 jnz .Lmst_have
 mov rdi, rbx
 call md_stage_flush
 jmp .Lmst_loop
.Lmst_have:
 mov r15, r14
 cmp r15, r13
 jbe .Lmst_copy
 mov r15, r13
.Lmst_copy:
 mov rdi, qword ptr [rbx + MDC_BUF]
 add rdi, qword ptr [rbx + MDC_LEN]
 mov rsi, r12
 mov rdx, r15
 call memcpy@PLT
 add qword ptr [rbx + MDC_LEN], r15
 add r12, r15
 sub r13, r15
 jmp .Lmst_loop
.Lmst_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_stage,.-md_stage

/* md_stage_wrap(rdi=ctx) — 冲写、换行、续行缩进、重盖当前样式 */
.type md_stage_wrap,@function
md_stage_wrap:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 32
 mov rbx, rdi
 call md_stage_flush
 mov r12, qword ptr [rbx + MDC_MD]
 mov rdi, r12
 call md_nl
 mov r13, qword ptr [rbx + MDC_CONT]
.Lmsw_sp:
 test r13, r13
 jz .Lmsw_mask
 mov r14, r13
 cmp r14, 32
 jbe .Lmsw_chk
 mov r14d, 32
.Lmsw_chk:
 mov rdi, r12
 lea rsi, [rip + .Lmd_sp32]
 mov rdx, r14
 call md_put_raw
 sub r13, r14
 jmp .Lmsw_sp
.Lmsw_mask:
 mov rax, qword ptr [rbx + MDC_MASK]
 test rax, rax
 jz .Lmsw_col
 cmp dword ptr [r12 + TUI_LI_MD_ANSI], 0
 je .Lmsw_col
 mov rdi, rsp
 mov esi, eax
 call md_sgr_build
 mov rdi, rbx
 mov rsi, rsp
 mov rdx, rax
 call md_stage
.Lmsw_col:
 mov rax, qword ptr [rbx + MDC_CONT]
 mov qword ptr [rbx + MDC_COL], rax
 add rsp, 32
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_stage_wrap,.-md_stage_wrap

/* md_set_style(rdi=ctx, esi=mask) — 样式切换写 SGR（降级无字节） */
.type md_set_style,@function
md_set_style:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 48
 mov rbx, rdi
 mov r13d, esi
 mov rax, r13
 cmp rax, qword ptr [rbx + MDC_MASK]
 je .Lmss_out
 mov r12, qword ptr [rbx + MDC_MD]
 cmp dword ptr [r12 + TUI_LI_MD_ANSI], 0
 je .Lmss_store
 mov rdi, rsp
 mov esi, r13d
 call md_sgr_build
 mov rdi, rbx
 mov rsi, rsp
 mov rdx, rax
 call md_stage
.Lmss_store:
 mov rax, r13
 mov qword ptr [rbx + MDC_MASK], rax
.Lmss_out:
 add rsp, 48
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_set_style,.-md_set_style

/* md_validate(rdi=escbuf8, rsi=ptr, rdx=avail)
 * → eax=消费字节, ecx=显示宽度, r8=形式指针, r9=形式长度。
 * 合法多字节原样；TAB→3空格；C1 区→\u00XX；非法/控制→\xHH。 */
.type md_validate,@function
md_validate:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 movzx r15d, byte ptr [r12]
 cmp r15d, 9
 je .Lv_tab
 cmp r15d, 32
 jb .Lv_esc1
 cmp r15d, 127
 je .Lv_esc1
 cmp r15d, 128
 jb .Lv_raw1
 cmp r15d, 194
 je .Lv_c2
 jb .Lv_esc1
 cmp r15d, 245
 ja .Lv_esc1
 mov edi, r15d
 call tui_utf8_seq_len@PLT
 mov r14, rax
 cmp r14, r13
 ja .Lv_esc1
 mov rcx, 1
.Lv_cont:
 cmp rcx, r14
 jae .Lv_bounds
 mov edx, ecx
 movzx edx, byte ptr [r12 + rdx]
 sub edx, 128
 cmp edx, 63
 ja .Lv_esc1
 inc rcx
 jmp .Lv_cont
.Lv_bounds:
 movzx edx, byte ptr [r12]
 movzx ecx, byte ptr [r12 + 1]
 cmp edx, 0xE0
 jne .Lv_sur
 cmp ecx, 0xA0
 jb .Lv_esc1
.Lv_sur:
 cmp edx, 0xED
 jne .Lv_pl
 cmp ecx, 0x9F
 ja .Lv_esc1
.Lv_pl:
 cmp edx, 0xF0
 jne .Lv_mx
 cmp ecx, 0x90
 jb .Lv_esc1
.Lv_mx:
 cmp edx, 0xF4
 jne .Lv_ok
 cmp ecx, 0x8F
 ja .Lv_esc1
.Lv_ok:
 mov rdi, r12
 mov rsi, r14
 call tui_view_cp_width@PLT
 mov ecx, eax
 mov eax, r14d
 mov r8, r12
 mov r9, r14
 jmp .Lv_out
.Lv_tab:
 lea r8, [rip + .Lmd_sp3]
 mov r9d, 3
 mov ecx, 3
 mov eax, 1
 jmp .Lv_out
.Lv_raw1:
 mov r8, r12
 mov r9d, 1
 mov ecx, 1
 mov eax, 1
 jmp .Lv_out
.Lv_esc1:
 mov byte ptr [rbx], 0x5C
 mov byte ptr [rbx + 1], 0x78
 lea rdx, [rip + .Lmd_hex]
 mov ecx, r15d
 shr ecx, 4
 mov al, [rdx + rcx]
 mov byte ptr [rbx + 2], al
 mov ecx, r15d
 and ecx, 15
 mov al, [rdx + rcx]
 mov byte ptr [rbx + 3], al
 mov r8, rbx
 mov r9d, 4
 mov ecx, 4
 mov eax, 1
 jmp .Lv_out
.Lv_c2:
 cmp r13, 2
 jb .Lv_esc1
 movzx ecx, byte ptr [r12 + 1]
 cmp ecx, 128
 jb .Lv_c2raw
 cmp ecx, 159
 ja .Lv_c2raw
 mov byte ptr [rbx], 0x5C
 mov byte ptr [rbx + 1], 0x75
 mov byte ptr [rbx + 2], 0x30
 mov byte ptr [rbx + 3], 0x30
 lea rdx, [rip + .Lmd_hex]
 mov eax, ecx
 shr eax, 4
 mov al, [rdx + rax]
 mov byte ptr [rbx + 4], al
 mov eax, ecx
 and eax, 15
 mov al, [rdx + rax]
 mov byte ptr [rbx + 5], al
 mov r8, rbx
 mov r9d, 6
 mov ecx, 6
 mov eax, 2
 jmp .Lv_out
.Lv_c2raw:
 mov rdi, r12
 mov esi, 2
 call tui_view_cp_width@PLT
 mov ecx, eax
 mov eax, 2
 mov r8, r12
 mov r9d, 2
.Lv_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_validate,.-md_validate

/* md_put_safe(rdi=ctx, rsi=ptr, rdx=avail) → eax 消费字节（含折行） */
.type md_put_safe,@function
md_put_safe:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 32
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov rdi, rsp
 mov rsi, r12
 mov rdx, r13
 call md_validate
 mov r14, rax
 mov r15, rcx
 mov qword ptr [rsp + 8], r8    /* 形式指针（escbuf 在 [rsp]，不可复用） */
 mov qword ptr [rsp + 16], r9
 mov rax, qword ptr [rbx + MDC_MD]
 mov edx, dword ptr [rax + TUI_LI_MD_COLS]
 mov r8, qword ptr [rbx + MDC_COL]
 mov r9, qword ptr [rbx + MDC_CONT]
 add r8, r15
 cmp r8, rdx
 jbe .Lmps_emit
 mov rdx, qword ptr [rbx + MDC_COL]
 cmp rdx, r9
 jbe .Lmps_emit
 mov rdi, rbx
 call md_stage_wrap
.Lmps_emit:
 add qword ptr [rbx + MDC_COL], r15
 mov rdi, rbx
 mov rsi, qword ptr [rsp + 8]
 mov rdx, qword ptr [rsp + 16]
 call md_stage
 mov eax, r14d
 add rsp, 32
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_put_safe,.-md_put_safe

/* ============ 内联扫描（递归，样式经 ctx 状态迁移） ============ */

/* md_inline(rdi=ctx, rsi=src, rdx=len, ecx=mask) */
.type md_inline,@function
md_inline:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 64
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 xor r14d, r14d
 mov r15d, ecx
 mov rax, qword ptr [rbx + MDC_MASK]
 mov qword ptr [rsp], rax
 mov rdi, rbx
 mov esi, r15d
 call md_set_style
.Lin_loop:
 cmp r14, r13
 jae .Lin_close
 movzx eax, byte ptr [r12 + r14]
 cmp eax, 0x5C
 je .Lin_bs
 cmp eax, 0x60
 je .Lin_bt
 cmp eax, '*'
 je .Lin_star
 cmp eax, '_'
 je .Lin_us
 cmp eax, '~'
 je .Lin_tilde
 cmp eax, '['
 je .Lin_link
 cmp eax, '<'
 je .Lin_autolink
 jmp .Lin_lit
/* 默认/字面：转义输出一个码点 */
.Lin_lit:
 mov rdi, rbx
 lea rsi, [r12 + r14]
 mov rdx, r13
 sub rdx, r14
 call md_put_safe
 add r14, rax
 jmp .Lin_loop
/* 双标记（星号对、下划线对、双波浪线）未闭合：两字符按字面输出，
 * 不作单标记重试（对应 marked 未闭合 delimiter run 保留原文） */
.Lin_lit2:
 mov rdi, rbx
 lea rsi, [r12 + r14]
 mov rdx, r13
 sub rdx, r14
 call md_put_safe
 mov rdi, rbx
 lea rsi, [r12 + r14 + 1]
 mov rdx, r13
 sub rdx, r14
 dec rdx
 call md_put_safe
 add r14, 2
 jmp .Lin_loop
/* \punct → 输出标点本身 */
.Lin_bs:
 lea rax, [r14 + 1]
 cmp rax, r13
 jae .Lin_lit
 movzx r10d, byte ptr [r12 + r14 + 1]
 lea rsi, [rip + .Lmd_punct]
 mov ecx, 94
.Lbs_scan:
 test ecx, ecx
 jz .Lin_lit
 cmp byte ptr [rsi], r10b
 je .Lbs_hit
 inc rsi
 dec ecx
 jmp .Lbs_scan
.Lbs_hit:
 mov rdi, rbx
 lea rsi, [r12 + r14 + 1]
 mov edx, 1
 call md_put_safe
 add r14, 2
 jmp .Lin_loop
/* `code`：等长反引号闭合 */
.Lin_bt:
 mov qword ptr [rsp + 8], 1
.Lbt_cnt:
 mov rax, r14
 add rax, qword ptr [rsp + 8]
 cmp rax, r13
 jae .Lbt_find
 cmp byte ptr [r12 + rax], 0x60
 jne .Lbt_find
 cmp qword ptr [rsp + 8], 16
 jae .Lbt_find
 inc qword ptr [rsp + 8]
 jmp .Lbt_cnt
.Lbt_find:
 mov rax, r14
 add rax, qword ptr [rsp + 8]
 mov qword ptr [rsp + 16], rax
.Lbt_scan:
 mov rax, qword ptr [rsp + 16]
 cmp rax, r13
 jae .Lin_lit
 cmp byte ptr [r12 + rax], 0x60
 jne .Lbt_adv
 mov qword ptr [rsp + 24], 1
.Lbt_m:
 mov rax, qword ptr [rsp + 16]
 add rax, qword ptr [rsp + 24]
 cmp rax, r13
 jae .Lbt_mdone
 cmp byte ptr [r12 + rax], 0x60
 jne .Lbt_mdone
 cmp qword ptr [rsp + 24], 16
 jae .Lbt_mdone
 inc qword ptr [rsp + 24]
 jmp .Lbt_m
.Lbt_mdone:
 mov rax, qword ptr [rsp + 24]
 cmp rax, qword ptr [rsp + 8]
 jae .Lbt_hit
 mov rax, qword ptr [rsp + 16]
 add rax, qword ptr [rsp + 24]
 mov qword ptr [rsp + 16], rax
 jmp .Lbt_scan
.Lbt_adv:
 inc qword ptr [rsp + 16]
 jmp .Lbt_scan
.Lbt_hit:
 mov rdi, rbx
 mov rsi, r12
 add rsi, r14
 add rsi, qword ptr [rsp + 8]
 mov rdx, qword ptr [rsp + 16]
 sub rdx, r14
 sub rdx, qword ptr [rsp + 8]
 mov ecx, r15d
 or ecx, MD_CODE
 call md_inline
 mov rax, qword ptr [rsp + 16]
 add rax, qword ptr [rsp + 24]
 mov r14, rax
 jmp .Lin_loop
/* **bold** / *em* */
.Lin_star:
 lea rax, [r14 + 1]
 cmp rax, r13
 jb .Lstar_chk
 jmp .Lin_lit
.Lstar_chk:
 cmp byte ptr [r12 + r14 + 1], '*'
 je .Lstar_bold
 mov rcx, r14
 inc rcx
.Lstar_f1:
 cmp rcx, r13
 jae .Lin_lit
 cmp byte ptr [r12 + rcx], '*'
 je .Lstar_h1
 inc rcx
 jmp .Lstar_f1
.Lstar_h1:
 mov qword ptr [rsp + 40], rcx
 mov rdi, rbx
 lea rsi, [r12 + r14 + 1]
 mov rdx, rcx
 sub rdx, r14
 dec rdx
 mov ecx, r15d
 or ecx, MD_ITALIC
 call md_inline
 mov rcx, qword ptr [rsp + 40]
 lea r14, [rcx + 1]
 jmp .Lin_loop
.Lstar_bold:
 mov rcx, r14
 add rcx, 2
.Lstar_f2:
 lea rax, [rcx + 1]
 cmp rax, r13
 jae .Lin_lit2
 cmp byte ptr [r12 + rcx], '*'
 jne .Lstar_f2n
 cmp byte ptr [r12 + rcx + 1], '*'
 je .Lstar_h2
.Lstar_f2n:
 inc rcx
 jmp .Lstar_f2
.Lstar_h2:
 mov qword ptr [rsp + 40], rcx
 mov rdi, rbx
 lea rsi, [r12 + r14 + 2]
 mov rdx, rcx
 sub rdx, r14
 sub rdx, 2
 mov ecx, r15d
 or ecx, MD_BOLD
 call md_inline
 mov rcx, qword ptr [rsp + 40]
 lea r14, [rcx + 2]
 jmp .Lin_loop
/* __bold__ / _em_ */
.Lin_us:
 lea rax, [r14 + 1]
 cmp rax, r13
 jb .Lus_chk
 jmp .Lin_lit
.Lus_chk:
 cmp byte ptr [r12 + r14 + 1], '_'
 je .Lus_bold
 mov rcx, r14
 inc rcx
.Lus_f1:
 cmp rcx, r13
 jae .Lin_lit
 cmp byte ptr [r12 + rcx], '_'
 je .Lus_h1
 inc rcx
 jmp .Lus_f1
.Lus_h1:
 mov qword ptr [rsp + 40], rcx
 mov rdi, rbx
 lea rsi, [r12 + r14 + 1]
 mov rdx, rcx
 sub rdx, r14
 dec rdx
 mov ecx, r15d
 or ecx, MD_ITALIC
 call md_inline
 mov rcx, qword ptr [rsp + 40]
 lea r14, [rcx + 1]
 jmp .Lin_loop
.Lus_bold:
 mov rcx, r14
 add rcx, 2
.Lus_f2:
 lea rax, [rcx + 1]
 cmp rax, r13
 jae .Lin_lit2
 cmp byte ptr [r12 + rcx], '_'
 jne .Lus_f2n
 cmp byte ptr [r12 + rcx + 1], '_'
 je .Lus_h2
.Lus_f2n:
 inc rcx
 jmp .Lus_f2
.Lus_h2:
 mov qword ptr [rsp + 40], rcx
 mov rdi, rbx
 lea rsi, [r12 + r14 + 2]
 mov rdx, rcx
 sub rdx, r14
 sub rdx, 2
 mov ecx, r15d
 or ecx, MD_BOLD
 call md_inline
 mov rcx, qword ptr [rsp + 40]
 lea r14, [rcx + 2]
 jmp .Lin_loop
/* ~~strike~~（单 ~ 字面） */
.Lin_tilde:
 lea rax, [r14 + 1]
 cmp rax, r13
 jb .Ltl_chk
 jmp .Lin_lit
.Ltl_chk:
 cmp byte ptr [r12 + r14 + 1], '~'
 jne .Lin_lit
 mov rcx, r14
 add rcx, 2
.Ltl_f:
 lea rax, [rcx + 1]
 cmp rax, r13
 jae .Lin_lit2
 cmp byte ptr [r12 + rcx], '~'
 jne .Ltl_fn
 cmp byte ptr [r12 + rcx + 1], '~'
 je .Ltl_h
.Ltl_fn:
 inc rcx
 jmp .Ltl_f
.Ltl_h:
 mov qword ptr [rsp + 40], rcx
 mov rdi, rbx
 lea rsi, [r12 + r14 + 2]
 mov rdx, rcx
 sub rdx, r14
 sub rdx, 2
 mov ecx, r15d
 or ecx, MD_STRIKE
 call md_inline
 mov rcx, qword ptr [rsp + 40]
 lea r14, [rcx + 2]
 jmp .Lin_loop
/* [text](url)：text≠url 时附 " (url)"；mailto: 前缀剥除后相等则只显文本
 * （Pi markdown.ts:739-755 的 href/text 比较） */
.Lin_link:
 mov rcx, r14
 inc rcx
.Llk_fb:
 cmp rcx, r13
 jae .Lin_lit
 cmp byte ptr [r12 + rcx], ']'
 je .Llk_gb
 inc rcx
 jmp .Llk_fb
.Llk_gb:
 lea rax, [rcx + 1]
 cmp rax, r13
 jae .Lin_lit
 cmp byte ptr [r12 + rcx + 1], '('
 jne .Lin_lit
 mov qword ptr [rsp + 40], rcx     /* j = ']' 位置 */
 lea rax, [rcx + 2]
 mov qword ptr [rsp + 32], rax     /* p = URL 起点（扫描期间固定） */
 mov rcx, rax
.Llk_fp:
 cmp rcx, r13
 jae .Lin_lit
 cmp byte ptr [r12 + rcx], ')'
 je .Llk_gp
 inc rcx
 jmp .Llk_fp
.Llk_gp:
 mov qword ptr [rsp + 48], rcx     /* k = ')' 位置 */
 mov rdi, rbx
 lea rsi, [r12 + r14 + 1]
 mov rdx, qword ptr [rsp + 40]
 sub rdx, r14
 dec rdx
 mov ecx, r15d
 call md_inline
 mov rdx, qword ptr [rsp + 40]
 sub rdx, r14
 dec rdx
 mov qword ptr [rsp + 56], rdx     /* tlen */
 mov rax, qword ptr [rsp + 48]
 sub rax, qword ptr [rsp + 32]     /* ulen */
 cmp rdx, rax
 je .Llk_cmp
 /* ulen == tlen+7 且 url 以 mailto: 开头 → 剥前缀再比较 */
 lea rcx, [rdx + 7]
 cmp rcx, rax
 jne .Llk_url
 mov rdi, r12
 add rdi, qword ptr [rsp + 32]
 lea rsi, [rip + .Lmd_mailto]
 mov edx, 7
 call memcmp@PLT
 test eax, eax
 jnz .Llk_url
 mov rdi, r12
 add rdi, qword ptr [rsp + 32]
 add rdi, 7
 lea rsi, [r12 + r14 + 1]
 mov rdx, qword ptr [rsp + 56]
 call memcmp@PLT
 test eax, eax
 jz .Llk_done
 jmp .Llk_url
.Llk_cmp:
 mov rdi, r12
 add rdi, qword ptr [rsp + 32]
 lea rsi, [r12 + r14 + 1]
 mov rdx, qword ptr [rsp + 56]
 call memcmp@PLT
 test eax, eax
 jz .Llk_done
.Llk_url:
 mov rdi, rbx
 lea rsi, [rip + .Lmd_linkpre]
 mov edx, 2
 call md_stage
.Llk_uw:
 mov rax, qword ptr [rsp + 32]
 cmp rax, qword ptr [rsp + 48]
 jae .Llk_up
 mov rdi, rbx
 mov rsi, r12
 add rsi, rax
 mov rdx, qword ptr [rsp + 48]
 sub rdx, rax
 call md_put_safe
 add qword ptr [rsp + 32], rax
 jmp .Llk_uw
.Llk_up:
 mov rdi, rbx
 lea rsi, [rip + .Lmd_linkpost]
 mov edx, 1
 call md_stage
.Llk_done:
 mov r14, qword ptr [rsp + 48]
 inc r14
 jmp .Lin_loop
/* <autolink>：http(s)/mailto 去尖括号；mailto: 剥前缀 */
.Lin_autolink:
 mov rcx, r14
 inc rcx
.Lal_f:
 cmp rcx, r13
 jae .Lin_lit
 cmp byte ptr [r12 + rcx], '>'
 je .Lal_g
 inc rcx
 jmp .Lal_f
.Lal_g:
 mov qword ptr [rsp + 32], rcx     /* '>' 位置跨 memcmp 保存 */
 mov rdx, rcx
 sub rdx, r14
 dec rdx
 cmp rdx, 8
 jb .Lal_t7
 lea rdi, [r12 + r14 + 1]
 lea rsi, [rip + .Lmd_https]
 mov edx, 8
 call memcmp@PLT
 test eax, eax
 jz .Lal_hit
.Lal_t7:
 mov rdx, qword ptr [rsp + 32]
 sub rdx, r14
 dec rdx
 cmp rdx, 7
 jb .Lin_lit
 lea rdi, [r12 + r14 + 1]
 lea rsi, [rip + .Lmd_http]
 mov edx, 7
 call memcmp@PLT
 test eax, eax
 jz .Lal_hit
 lea rdi, [r12 + r14 + 1]
 lea rsi, [rip + .Lmd_mailto]
 mov edx, 7
 call memcmp@PLT
 test eax, eax
 jz .Lal_mhit
 jmp .Lin_lit
.Lal_hit:
 lea rax, [r14 + 1]
 mov qword ptr [rsp + 40], rax
 jmp .Lal_w
.Lal_mhit:
 lea rax, [r14 + 8]                /* 跳过 "mailto:" */
 mov qword ptr [rsp + 40], rax
.Lal_w:
 mov rax, qword ptr [rsp + 40]
 cmp rax, qword ptr [rsp + 32]
 jae .Lal_d
 mov rdi, rbx
 mov rsi, r12
 add rsi, rax
 mov rdx, qword ptr [rsp + 32]
 sub rdx, rax
 call md_put_safe
 add qword ptr [rsp + 40], rax
 jmp .Lal_w
.Lal_d:
 mov r14, qword ptr [rsp + 32]
 inc r14
 jmp .Lin_loop
.Lin_close:
 mov rdi, rbx
 mov esi, dword ptr [rsp]
 call md_set_style
 add rsp, 64
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_inline,.-md_inline

/* ============ 行发射器（共享 staging 上下文） ============ */

/* md_plain_line(rdi=md, rsi=s, rdx=len, ecx=mask, r8d=cont) — 段落行 */
.type md_plain_line,@function
md_plain_line:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 2368
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov r14d, ecx
 mov r15, r8
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_MD], rbx
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_BUF], rsp
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_LEN], 0
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_CAP], MD_STAGE_CAP
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_COL], 0
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_CONT], r15
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_MASK], 0
 test r13, r13
 jz .Lpln_flush
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 mov rsi, r12
 mov rdx, r13
 mov ecx, r14d
 call md_inline
.Lpln_flush:
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 call md_stage_flush
 mov rdi, rbx
 call md_nl
 mov dword ptr [rbx + TUI_LI_MD_WROTE_ANY], 1
 mov dword ptr [rbx + TUI_LI_MD_LAST_BLANK], 0
 add rsp, 2368
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_plain_line,.-md_plain_line

/* md_list_line(rdi=md, rsi=s, rdx=len, rcx=prefix, r8d=plen, r9d=cont) */
.type md_list_line,@function
md_list_line:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 2400
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov qword ptr [rsp + 2360], rcx    /* prefix */
 mov qword ptr [rsp + 2368], r8     /* plen */
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_MD], rbx
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_BUF], rsp
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_LEN], 0
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_CAP], MD_STAGE_CAP
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_COL], 0
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_CONT], r9
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_MASK], 0
 test r8, r8
 jz .Llst_body
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 mov rsi, qword ptr [rsp + 2360]
 mov rdx, qword ptr [rsp + 2368]
 call md_stage
 mov rax, qword ptr [rsp + 2368]
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_COL], rax
.Llst_body:
 test r13, r13
 jz .Llst_flush
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 mov rsi, r12
 mov rdx, r13
 xor ecx, ecx
 call md_inline
.Llst_flush:
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 call md_stage_flush
 mov rdi, rbx
 call md_nl
 mov dword ptr [rbx + TUI_LI_MD_WROTE_ANY], 1
 mov dword ptr [rbx + TUI_LI_MD_LAST_BLANK], 0
 add rsp, 2400
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_list_line,.-md_list_line

/* md_quote_line(rdi=md, rsi=s, rdx=len, ecx=depth) */
.type md_quote_line,@function
md_quote_line:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 2400
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov r14d, ecx
 mov r15d, ecx                  /* 深度跨 md_stage 保存（rcx 被破坏） */
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_MD], rbx
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_BUF], rsp
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_LEN], 0
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_CAP], MD_STAGE_CAP
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_COL], 0
 lea rax, [r15 + r15]
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_CONT], rax
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_MASK], 0
 test r13, r13
 jnz .Lqt_bar
 test r14d, r14d                /* 空内容：末级只画 │，不带尾随空格 */
 jz .Lqt_flush
 dec r14d
.Lqt_bar:
 test r14d, r14d
 jz .Lqt_bars_done
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 lea rsi, [rip + .Lmd_qbar]
 mov edx, 4
 call md_stage
 dec r14d
 jmp .Lqt_bar
.Lqt_bars_done:
 test r13, r13
 jnz .Lqt_body
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 lea rsi, [rip + .Lmd_qbar]
 mov edx, 3                     /* 裸 │ */
 call md_stage
 jmp .Lqt_flush
.Lqt_body:
 lea eax, [r15 + r15]
 movzx eax, ax
 mov dword ptr [rsp + MD_STAGE_CAP + MDC_COL], eax
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 mov rsi, r12
 mov rdx, r13
 xor ecx, ecx
 call md_inline
.Lqt_flush:
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 call md_stage_flush
 mov rdi, rbx
 call md_nl
 mov dword ptr [rbx + TUI_LI_MD_WROTE_ANY], 1
 mov dword ptr [rbx + TUI_LI_MD_LAST_BLANK], 0
 add rsp, 2400
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_quote_line,.-md_quote_line

/* md_head_line(rdi=md, rsi=s, rdx=len, ecx=depth) */
.type md_head_line,@function
md_head_line:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 2400
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov r14d, ecx
 cmp r14d, 2
 ja .Lhd_deep
 mov ecx, MD_BOLD
 cmp r14d, 1
 jne .Lhd_sh
 or ecx, MD_UNDERLINE
.Lhd_sh:
 mov rdi, rbx
 mov rsi, r12
 mov rdx, r13
 xor r8d, r8d
 call md_plain_line
 jmp .Lhd_out
.Lhd_deep:
 /* '#'×depth + ' ' 前缀，内容加粗 */
 xor ecx, ecx
.Lhd_fill:
 cmp ecx, r14d
 jae .Lhd_sp
 mov byte ptr [rsp + 2360 + rcx], '#'
 inc ecx
 jmp .Lhd_fill
.Lhd_sp:
 mov byte ptr [rsp + 2360 + rcx], ' '
 inc ecx
 mov qword ptr [rsp + 2376], rcx    /* k = depth+1 */
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_MD], rbx
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_BUF], rsp
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_LEN], 0
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_CAP], MD_STAGE_CAP
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_COL], 0
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_CONT], rcx
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_MASK], 0
 lea rsi, [rsp + 2360]
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 mov rdx, rcx
 call md_stage
 mov rax, qword ptr [rsp + 2376]
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_COL], rax
 test r13, r13
 jz .Lhd_flush
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 mov rsi, r12
 mov rdx, r13
 mov ecx, MD_BOLD
 call md_inline
.Lhd_flush:
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 call md_stage_flush
 mov rdi, rbx
 call md_nl
 mov dword ptr [rbx + TUI_LI_MD_WROTE_ANY], 1
 mov dword ptr [rbx + TUI_LI_MD_LAST_BLANK], 0
.Lhd_out:
 add rsp, 2400
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_head_line,.-md_head_line

/* md_code_line(rdi=md, rsi=s, rdx=len) — 代码内容：缩进+转义+折行 */
.type md_code_line,@function
md_code_line:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 2400
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_MD], rbx
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_BUF], rsp
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_LEN], 0
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_CAP], MD_STAGE_CAP
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_COL], 0
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_CONT], 2
 mov qword ptr [rsp + MD_STAGE_CAP + MDC_MASK], 0
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 lea rsi, [rip + .Lmd_indent]
 mov edx, 2
 call md_stage
 mov dword ptr [rsp + MD_STAGE_CAP + MDC_COL], 2
 xor r14d, r14d
.Lcd_walk:
 cmp r14, r13
 jae .Lcd_flush
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 mov rsi, r12
 add rsi, r14
 mov rdx, r13
 sub rdx, r14
 call md_put_safe
 add r14, rax
 jmp .Lcd_walk
.Lcd_flush:
 mov rdi, rsp
 add rdi, MD_STAGE_CAP
 call md_stage_flush
 mov rdi, rbx
 call md_nl
 mov dword ptr [rbx + TUI_LI_MD_WROTE_ANY], 1
 mov dword ptr [rbx + TUI_LI_MD_LAST_BLANK], 0
 add rsp, 2400
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_code_line,.-md_code_line

/* md_put_esc(rdi=md, rsi=s, rdx=len) — 无折行整段转义（边界 2048） */
.type md_put_esc,@function
md_put_esc:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 2064
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 xor r14d, r14d
 xor r15d, r15d
.Lpe_loop:
 cmp r15, r13
 jae .Lpe_out
 mov rdi, rsp
 add rdi, 2048
 mov rsi, r12
 add rsi, r15
 mov rdx, r13
 sub rdx, r15
 call md_validate
 mov r10, rax
 mov rdx, r14
 add rdx, r9
 cmp rdx, 2048
 jbe .Lpe_fit
 mov rdi, rbx
 mov rsi, rsp
 mov rdx, r14
 call md_put_raw
 xor r14d, r14d
.Lpe_fit:
 mov rdi, rsp
 add rdi, r14
 mov rsi, r8
 mov rdx, r9
 call memcpy@PLT
 add r14, r9
 add r15, r10
 jmp .Lpe_loop
.Lpe_out:
 mov rdi, rbx
 mov rsi, rsp
 mov rdx, r14
 call md_put_raw
 add rsp, 2064
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_put_esc,.-md_put_esc

/* md_esc_line(rdi=md, rsi=s, rdx=len) — 原文转义整行（表格回退路径） */
.type md_esc_line,@function
md_esc_line:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov rdi, rbx
 mov rsi, r12
 mov rdx, r13
 call md_put_esc
 mov rdi, rbx
 call md_nl
 mov dword ptr [rbx + TUI_LI_MD_WROTE_ANY], 1
 mov dword ptr [rbx + TUI_LI_MD_LAST_BLANK], 0
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_esc_line,.-md_esc_line

/* md_bar_run(rdi=md, esi=n) — n 个 ─ */
.type md_bar_run,@function
md_bar_run:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12d, esi
.Lbr_loop:
 test r12d, r12d
 jz .Lbr_out
 mov rdi, rbx
 lea rsi, [rip + .Lmd_hbar]
 mov edx, 3
 call md_put_raw
 dec r12d
 jmp .Lbr_loop
.Lbr_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_bar_run,.-md_bar_run

/* md_sgr_direct(rdi=md, esi=mask) — 直写 SGR（表格表头） */
.type md_sgr_direct,@function
md_sgr_direct:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 32
 mov rbx, rdi
 mov r12d, esi
 cmp dword ptr [rbx + TUI_LI_MD_ANSI], 0
 je .Lsgd_out
 mov rdi, rsp
 mov esi, r12d
 call md_sgr_build
 mov rdi, rbx
 mov rsi, rsp
 mov rdx, rax
 call md_put_raw
.Lsgd_out:
 add rsp, 32
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_sgr_direct,.-md_sgr_direct

/* ============ 表格 ============ */

/* md_cell_width(rdi=md, rsi=cell, rdx=len) → eax 宽度（≤24 封顶） */
.type md_cell_width,@function
md_cell_width:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 16
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 xor r14d, r14d
 xor r15d, r15d
.Lcw_loop:
 cmp r15, r13
 jae .Lcw_out
 mov rdi, rsp
 mov rsi, r12
 add rsi, r15
 mov rdx, r13
 sub rdx, r15
 call md_validate
 add r14, rcx
 add r15, rax
 cmp r14d, 24
 ja .Lcw_out
 jmp .Lcw_loop
.Lcw_out:
 cmp r14d, 24
 jbe .Lcw_ret
 mov r14d, 24
.Lcw_ret:
 mov eax, r14d
 add rsp, 16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_cell_width,.-md_cell_width

/* md_cell_emit(rdi=md, rsi=cell, rdx=len, ecx=limit) → eax 输出宽度
 * 超宽按码点截断并补 …（预留 1 列）。 */
.type md_cell_emit,@function
md_cell_emit:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 32
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov qword ptr [rsp + 16], rcx     /* limit */
 mov qword ptr [rsp + 8], 0        /* acc */
 xor r15d, r15d
.Lce_loop:
 cmp r15, r13
 jae .Lce_tail
 mov rdi, rsp
 mov rsi, r12
 add rsi, r15
 mov rdx, r13
 sub rdx, r15
 call md_validate
 /* cons/w 存栈槽：caller-saved，过不了下面的 md_put_raw 调用 */
 mov qword ptr [rsp + 24], rax      /* cons */
 mov qword ptr [rsp], rcx           /* w */
 /* 放不下，或恰好放满但后面还有内容（需给 … 留位）才截断 */
 mov rax, qword ptr [rsp + 8]
 add rax, qword ptr [rsp]
 cmp rax, qword ptr [rsp + 16]
 ja .Lce_tail
 jne .Lce_emit
 mov rax, r15
 add rax, qword ptr [rsp + 24]
 cmp rax, r13
 jb .Lce_tail
.Lce_emit:
 mov rdi, rbx
 mov rsi, r8
 mov rdx, r9
 call md_put_raw
 mov rax, qword ptr [rsp]
 add qword ptr [rsp + 8], rax
 mov rax, qword ptr [rsp + 24]
 add r15, rax
 jmp .Lce_loop
.Lce_tail:
 cmp r15, r13
 jae .Lce_out
 mov rdi, rbx
 lea rsi, [rip + .Lmd_ellipsis]
 mov edx, 3
 call md_put_raw
 inc qword ptr [rsp + 8]
.Lce_out:
 mov eax, dword ptr [rsp + 8]
 add rsp, 32
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_cell_emit,.-md_cell_emit

/* md_row_parse(rdi=cells(16 qword), rsi=row, rdx=len) → eax 列数（≤8） */
.type md_row_parse,@function
md_row_parse:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 16
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 xor r14d, r14d
 xor r15d, r15d
.Lrp_ws1:
 cmp r14, r13
 jae .Lrp_lead
 movzx eax, byte ptr [r12 + r14]
 cmp eax, ' '
 je .Lrp_ws1a
 cmp eax, 9
 jne .Lrp_lead
.Lrp_ws1a:
 inc r14
 jmp .Lrp_ws1
.Lrp_lead:
 cmp r14, r13
 jae .Lrp_done
 cmp byte ptr [r12 + r14], '|'
 jne .Lrp_cell
 inc r14
.Lrp_cell:
 cmp r14, r13
 jae .Lrp_done
 movzx eax, byte ptr [r12 + r14]
 cmp eax, ' '
 je .Lrp_cws
 cmp eax, 9
 jne .Lrp_cstart
.Lrp_cws:
 inc r14
 jmp .Lrp_cell
.Lrp_cstart:
 mov qword ptr [rsp], r14
.Lrp_scan:
 cmp r14, r13
 jae .Lrp_end
 cmp byte ptr [r12 + r14], '|'
 je .Lrp_end
 inc r14
 jmp .Lrp_scan
.Lrp_end:
 mov rdx, r14
.Lrp_trim:
 cmp rdx, qword ptr [rsp]
 jbe .Lrp_store
 movzx eax, byte ptr [r12 + rdx - 1]
 cmp eax, ' '
 je .Lrp_trimd
 cmp eax, 9
 jne .Lrp_store
.Lrp_trimd:
 dec rdx
 jmp .Lrp_trim
.Lrp_store:
 cmp r15d, 8
 jae .Lrp_next
 mov rax, r15
 shl rax, 4
 mov rcx, qword ptr [rsp]
 mov qword ptr [rbx + rax], rcx
 mov qword ptr [rbx + rax + 8], rdx
 sub qword ptr [rbx + rax + 8], rcx
.Lrp_next:
 inc r15d
 cmp r14, r13
 jae .Lrp_done
 cmp byte ptr [r12 + r14], '|'
 jne .Lrp_done
 inc r14
 cmp r14, r13
 jae .Lrp_done
 jmp .Lrp_cell
.Lrp_done:
 /* 计数与存储同样 cap 8：多余列只算已存储的，渲染循环不得越界 */
 cmp r15d, 8
 jbe .Lrp_donecap
 mov r15d, 8
.Lrp_donecap:
 mov eax, r15d
 add rsp, 16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_row_parse,.-md_row_parse

/* md_is_delim(rsi=s, rdx=len) → eax 1/0：GFM 表格分隔行 */
.type md_is_delim,@function
md_is_delim:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rsi
 mov r12, rdx
 xor r13d, r13d
 xor r14d, r14d                     /* 见过 '-' */
 /* 前导空格 ≤3 */
 xor r15d, r15d
.Ldl_sp:
 cmp r13, r12
 jae .Ldl_pipe0
 cmp byte ptr [rbx + r13], ' '
 jne .Ldl_pipe0
 inc r13
 inc r15d
 cmp r15d, 4
 jb .Ldl_sp
 jmp .Ldl_fail
.Ldl_pipe0:
 cmp r13, r12
 jae .Ldl_fail
 cmp byte ptr [rbx + r13], '|'
 jne .Ldl_row
 inc r13
.Ldl_row:
 /* 空白 */
.Ldl_ws:
 cmp r13, r12
 jae .Ldl_fail
 movzx eax, byte ptr [rbx + r13]
 cmp eax, ' '
 je .Ldl_wsa
 cmp eax, 9
 jne .Ldl_colon
.Ldl_wsa:
 inc r13
 jmp .Ldl_ws
.Ldl_colon:
 cmp byte ptr [rbx + r13], ':'
 jne .Ldl_dash
 inc r13
.Ldl_dash:
 xor r15d, r15d
.Ldl_dl:
 cmp r13, r12
 jae .Ldl_fail
 cmp byte ptr [rbx + r13], '-'
 jne .Ldl_dend
 inc r13
 inc r15d
 jmp .Ldl_dl
.Ldl_dend:
 test r15d, r15d
 jz .Ldl_fail
 mov r14d, 1
 cmp byte ptr [rbx + r13], ':'
 jne .Ldl_ws2
 inc r13
.Ldl_ws2:
 cmp r13, r12
 jae .Ldl_tail
 movzx eax, byte ptr [rbx + r13]
 cmp eax, ' '
 je .Ldl_ws2a
 cmp eax, 9
 jne .Ldl_sep
.Ldl_ws2a:
 inc r13
 jmp .Ldl_ws2
.Ldl_sep:
 cmp byte ptr [rbx + r13], '|'
 jne .Ldl_tail
 inc r13
 /* 尾管道后行尾即成功（列已完成后 r14=1） */
 cmp r13, r12
 jae .Ldl_end
 jmp .Ldl_row
.Ldl_tail:
 cmp r13, r12
 jae .Ldl_end
 cmp byte ptr [rbx + r13], '|'
 jne .Ldl_tws
 inc r13
.Ldl_tws:
 cmp r13, r12
 jae .Ldl_end
 movzx eax, byte ptr [rbx + r13]
 cmp eax, ' '
 je .Ldl_twsa
 cmp eax, 9
 jne .Ldl_fail
.Ldl_twsa:
 inc r13
 jmp .Ldl_tws
.Ldl_end:
 mov eax, r14d
 jmp .Ldl_out
.Ldl_fail:
 xor eax, eax
.Ldl_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_is_delim,.-md_is_delim

/* md_is_hr(rsi=s, rdx=len) → eax：-、_、* 讇 3 个及以上（可混空格） */
.type md_is_hr,@function
md_is_hr:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rsi
 mov r12, rdx
 xor r13d, r13d
 xor r15d, r15d
.Lhr_sp:
 cmp r13, r12
 jae .Lhr_fail
 cmp byte ptr [rbx + r13], ' '
 jne .Lhr_char
 inc r13
 inc r15d
 jmp .Lhr_sp
.Lhr_char:
 cmp r15d, 4
 jae .Lhr_fail
 movzx r14d, byte ptr [rbx + r13]
 cmp r14d, '-'
 je .Lhr_scan
 cmp r14d, '*'
 je .Lhr_scan
 cmp r14d, '_'
 jne .Lhr_fail
.Lhr_scan:
 cmp r13, r12
 jae .Lhr_done
 movzx eax, byte ptr [rbx + r13]
 cmp eax, r14d
 jne .Lhr_other
 inc r13
 inc r15d
 jmp .Lhr_scan
.Lhr_other:
 cmp eax, ' '
 je .Lhr_skip
 cmp eax, 9
 jne .Lhr_fail
.Lhr_skip:
 inc r13
 jmp .Lhr_scan
.Lhr_done:
 cmp r15d, 3
 jb .Lhr_fail
 mov eax, 1
 jmp .Lhr_out
.Lhr_fail:
 xor eax, eax
.Lhr_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_is_hr,.-md_is_hr

/* md_pad(rdi=md, esi=n) — n 个空格 */
.type md_pad,@function
md_pad:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12d, esi
.Lpd_loop:
 test r12d, r12d
 jz .Lpd_out
 mov r13d, r12d
 cmp r13d, 32
 jbe .Lpd_m
 mov r13d, 32
.Lpd_m:
 mov rdi, rbx
 lea rsi, [rip + .Lmd_sp32]
 mov rdx, r13
 call md_put_raw
 sub r12d, r13d
 jmp .Lpd_loop
.Lpd_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_pad,.-md_pad

/* md_row_line(rdi=md, rsi=row, rdx=len, rcx=ncols, r8=colw, r9d=bold) */
.type md_row_line,@function
md_row_line:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 160
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov qword ptr [rsp + 128], rcx     /* ncols */
 mov qword ptr [rsp + 136], r8      /* colw */
 mov qword ptr [rsp + 144], r9      /* bold */
 mov rdi, rsp
 mov rsi, r12
 mov rdx, r13
 call md_row_parse
 mov qword ptr [rsp + 152], rax     /* n */
 cmp qword ptr [rsp + 144], 0
 je .Lrl_nb
 mov rdi, rbx
 mov esi, MD_BOLD
 call md_sgr_direct
.Lrl_nb:
 mov rdi, rbx
 lea rsi, [rip + .Lmd_qbar]
 mov edx, 4
 call md_put_raw
 xor r14d, r14d
.Lrl_c:
 cmp r14, qword ptr [rsp + 128]
 jae .Lrl_end
 xor r15d, r15d                     /* w */
 cmp r14, qword ptr [rsp + 152]
 jae .Lrl_pad
 mov rax, r14
 shl rax, 4
 mov rsi, r12
 add rsi, qword ptr [rsp + rax]
 mov rdx, qword ptr [rsp + rax + 8]
 mov rdi, rbx
 mov rcx, qword ptr [rsp + 136]
 mov ecx, dword ptr [rcx + r14*4]
 call md_cell_emit
 mov r15d, eax
.Lrl_pad:
 mov rdi, rbx
 mov rsi, qword ptr [rsp + 136]
 mov esi, dword ptr [rsi + r14*4]
 sub esi, r15d
 jbe .Lrl_sep
 call md_pad
.Lrl_sep:
 lea rax, [r14 + 1]
 cmp rax, qword ptr [rsp + 128]
 jae .Lrl_end
 mov rdi, rbx
 lea rsi, [rip + .Lmd_cellsep]
 mov edx, 5
 call md_put_raw
 inc r14
 jmp .Lrl_c
.Lrl_end:
 mov rdi, rbx
 lea rsi, [rip + .Lmd_cellemd]
 mov edx, 4
 call md_put_raw
 cmp qword ptr [rsp + 144], 0
 je .Lrl_nl
 mov rdi, rbx
 xor esi, esi
 call md_sgr_direct
.Lrl_nl:
 mov rdi, rbx
 call md_nl
 add rsp, 160
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_row_line,.-md_row_line

/* md_render_table(rdi=md) — 渲染缓冲表格；过窄回退原文 */
.type md_render_table,@function
md_render_table:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 256
 mov rbx, rdi
 mov r12d, dword ptr [rbx + TUI_LI_MD_TABLE_ROWS_N]
 mov qword ptr [rsp + 160], 0
 /* 列宽 = 各行单元格宽度最大值 */
 xor r13d, r13d
.Lrt_zw:
 mov dword ptr [rsp + 128 + r13*4], 0
 inc r13d
 cmp r13d, 8
 jb .Lrt_zw
 mov qword ptr [rsp + 176], 0       /* r */
.Lrt_p1:
 mov rax, qword ptr [rsp + 176]
 cmp rax, r12
 jae .Lrt_p1done
 imul rdx, rax, TUI_LI_MD_ROW_MAX
 mov rax, rbx
 add rax, TUI_LI_MD_TABLE
 add rax, rdx
 mov qword ptr [rsp + 200], rax     /* row */
 mov rdx, qword ptr [rsp + 176]
 mov edx, dword ptr [rbx + rdx*4 + TUI_LI_MD_TABLE_LENS]
 mov qword ptr [rsp + 208], rdx
 mov rdi, rsp
 mov rsi, rax
 mov rdx, qword ptr [rsp + 208]
 call md_row_parse
 mov qword ptr [rsp + 192], rax
 mov qword ptr [rsp + 184], 0       /* c */
.Lrt_p1c:
 mov rax, qword ptr [rsp + 184]
 cmp rax, qword ptr [rsp + 192]
 jae .Lrt_p1n
 shl rax, 4
 mov rsi, qword ptr [rsp + 200]
 add rsi, qword ptr [rsp + rax]
 mov rdx, qword ptr [rsp + rax + 8]
 mov rdi, rbx
 call md_cell_width
 mov ecx, dword ptr [rsp + 184]
 mov edx, dword ptr [rsp + 128 + rcx*4]
 cmp eax, edx
 jbe .Lrt_p1cx
 mov dword ptr [rsp + 128 + rcx*4], eax
.Lrt_p1cx:
 inc qword ptr [rsp + 184]
 jmp .Lrt_p1c
.Lrt_p1n:
 mov rdx, qword ptr [rsp + 192]
 cmp rdx, qword ptr [rsp + 160]
 jbe .Lrt_p1r
 mov qword ptr [rsp + 160], rdx
.Lrt_p1r:
 inc qword ptr [rsp + 176]
 jmp .Lrt_p1
.Lrt_p1done:
 /* total = Σcolw + 3n + 1 */
 mov rax, qword ptr [rsp + 160]
 lea rax, [rax + rax*2]
 inc rax
 mov qword ptr [rsp + 168], rax
 mov qword ptr [rsp + 184], 0
.Lrt_tot:
 mov rax, qword ptr [rsp + 184]
 cmp rax, qword ptr [rsp + 160]
 jae .Lrt_totdone
 mov ecx, dword ptr [rsp + 128 + rax*4]
 add qword ptr [rsp + 168], rcx
 inc qword ptr [rsp + 184]
 jmp .Lrt_tot
.Lrt_totdone:
 mov rax, qword ptr [rsp + 168]
 cmp eax, dword ptr [rbx + TUI_LI_MD_COLS]
 jbe .Lrt_emit
 /* 过窄回退：原文转义逐行 */
 mov qword ptr [rsp + 176], 0
.Lrt_fb:
 mov rax, qword ptr [rsp + 176]
 cmp rax, r12
 jae .Lrt_reset
 imul rdx, rax, TUI_LI_MD_ROW_MAX
 mov rsi, rbx
 add rsi, TUI_LI_MD_TABLE
 add rsi, rdx
 mov rdx, rax
 mov edx, dword ptr [rbx + rdx*4 + TUI_LI_MD_TABLE_LENS]
 mov rdi, rbx
 call md_esc_line
 inc qword ptr [rsp + 176]
 jmp .Lrt_fb
.Lrt_emit:
 /* 顶边框 ┌─ ─┬─ ─┐ */
 mov rdi, rbx
 lea rsi, [rip + .Lmd_topleft]
 mov edx, 6
 call md_put_raw
 mov qword ptr [rsp + 184], 0
.Lrt_top:
 mov rax, qword ptr [rsp + 184]
 cmp rax, qword ptr [rsp + 160]
 jae .Lrt_topend
 mov esi, dword ptr [rsp + 128 + rax*4]
 mov rdi, rbx
 call md_bar_run
 mov rax, qword ptr [rsp + 184]
 inc rax
 cmp rax, qword ptr [rsp + 160]
 jae .Lrt_topend
 mov qword ptr [rsp + 184], rax
 mov rdi, rbx
 lea rsi, [rip + .Lmd_colmid]
 mov edx, 9
 call md_put_raw
 jmp .Lrt_top
.Lrt_topend:
 mov rdi, rbx
 lea rsi, [rip + .Lmd_topright]
 mov edx, 6
 call md_put_raw
 mov rdi, rbx
 call md_nl
 /* 表头（加粗） */
 imul rdx, qword ptr [rsp + 176], 1
 mov rsi, rbx
 add rsi, TUI_LI_MD_TABLE
 mov rdx, 0
 mov edx, dword ptr [rbx + TUI_LI_MD_TABLE_LENS]
 mov rdi, rbx
 mov rcx, qword ptr [rsp + 160]
 lea r8, [rsp + 128]
 mov r9d, 1
 call md_row_line
 /* 行分隔与数据行 */
 mov qword ptr [rsp + 176], 1
.Lrt_rows:
 mov rax, qword ptr [rsp + 176]
 cmp rax, r12
 jae .Lrt_bottom
 imul rdx, rax, TUI_LI_MD_ROW_MAX
 mov rax, rbx
 add rax, TUI_LI_MD_TABLE
 add rax, rdx
 mov rsi, rax
 mov rdx, qword ptr [rsp + 176]
 mov edx, dword ptr [rbx + rdx*4 + TUI_LI_MD_TABLE_LENS]
 mov rdi, rbx
 mov rcx, qword ptr [rsp + 160]
 lea r8, [rsp + 128]
 xor r9d, r9d
 call md_row_line
 mov rax, qword ptr [rsp + 176]
 inc rax
 cmp rax, r12
 jae .Lrt_bottom
 mov qword ptr [rsp + 176], rax
 /* 分隔线 ├─ ─┼─ ─┤ */
 mov rdi, rbx
 lea rsi, [rip + .Lmd_midleft]
 mov edx, 6
 call md_put_raw
 mov qword ptr [rsp + 184], 0
.Lrt_sep:
 mov rax, qword ptr [rsp + 184]
 cmp rax, qword ptr [rsp + 160]
 jae .Lrt_sepend
 mov esi, dword ptr [rsp + 128 + rax*4]
 mov rdi, rbx
 call md_bar_run
 mov rax, qword ptr [rsp + 184]
 inc rax
 cmp rax, qword ptr [rsp + 160]
 jae .Lrt_sepend
 mov qword ptr [rsp + 184], rax
 mov rdi, rbx
 lea rsi, [rip + .Lmd_rowmid]
 mov edx, 9
 call md_put_raw
 jmp .Lrt_sep
.Lrt_sepend:
 mov rdi, rbx
 lea rsi, [rip + .Lmd_midright]
 mov edx, 6
 call md_put_raw
 mov rdi, rbx
 call md_nl
 jmp .Lrt_rows
.Lrt_bottom:
 /* 底边框 └─ ─┴─ ─┘ */
 mov rdi, rbx
 lea rsi, [rip + .Lmd_botleft]
 mov edx, 6
 call md_put_raw
 mov qword ptr [rsp + 184], 0
.Lrt_bot:
 mov rax, qword ptr [rsp + 184]
 cmp rax, qword ptr [rsp + 160]
 jae .Lrt_botend
 mov esi, dword ptr [rsp + 128 + rax*4]
 mov rdi, rbx
 call md_bar_run
 mov rax, qword ptr [rsp + 184]
 inc rax
 cmp rax, qword ptr [rsp + 160]
 jae .Lrt_botend
 mov qword ptr [rsp + 184], rax
 mov rdi, rbx
 lea rsi, [rip + .Lmd_botmid]
 mov edx, 9
 call md_put_raw
 jmp .Lrt_bot
.Lrt_botend:
 mov rdi, rbx
 lea rsi, [rip + .Lmd_botright]
 mov edx, 6
 call md_put_raw
 mov rdi, rbx
 call md_nl
.Lrt_reset:
 mov dword ptr [rbx + TUI_LI_MD_TABLE_ROWS_N], 0
 mov dword ptr [rbx + TUI_LI_MD_WROTE_ANY], 1
 mov dword ptr [rbx + TUI_LI_MD_LAST_BLANK], 0
 add rsp, 256
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size md_render_table,.-md_render_table

/* ============ 块状态机 ============ */

/* md_process_line(rdi=md, rsi=line, rdx=len) — 一条完整源行的分发 */
.type md_process_line,@function
md_process_line:
 .cfi_startproc
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 128
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 cmp dword ptr [rbx + TUI_LI_MD_FENCE], 0
 jne .Lpl_fence
 cmp dword ptr [rbx + TUI_LI_MD_TABLE_ON], 0
 jne .Lpl_table
 cmp dword ptr [rbx + TUI_LI_MD_CAND_LEN], 0
 jne .Lpl_cand
 jmp .Lpl_norm

/* ---- 表格缓冲中 ---- */
.Lpl_table:
 test r13, r13
 jz .Lpl_tabclose
 xor r14d, r14d
 mov r15, r13
.Lpl_tscan:
 test r15, r15
 jz .Lpl_tscand
 mov rax, r13
 sub rax, r15
 cmp byte ptr [r12 + rax], '|'
	jne .Lpl_tscan1
	mov r14d, 1
	jmp .Lpl_tscand
.Lpl_tscan1:
	dec r15
	jmp .Lpl_tscan
.Lpl_tscand:
	test r14d, r14d
	jz .Lpl_tabclose
	cmp dword ptr [rbx + TUI_LI_MD_TABLE_ROWS_N], TUI_LI_MD_TABLE_ROWS
	jae .Lpl_tabfull
	/* 追加行（截 255 字节）；长度/行号入栈跨 memcpy */
	mov eax, dword ptr [rbx + TUI_LI_MD_TABLE_ROWS_N]
	mov qword ptr [rsp + 8], rax
	imul rdx, rax, TUI_LI_MD_ROW_MAX
	lea rdi, [rbx + TUI_LI_MD_TABLE]
	add rdi, rdx
	mov rsi, r12
	mov rdx, r13
	cmp rdx, TUI_LI_MD_ROW_MAX - 1
	jbe .Lpl_tabcp
	mov edx, TUI_LI_MD_ROW_MAX - 1
.Lpl_tabcp:
	mov qword ptr [rsp + 16], rdx
	call memcpy@PLT
	mov rax, qword ptr [rsp + 8]
	mov rdx, qword ptr [rsp + 16]
	mov dword ptr [rbx + rax*4 + TUI_LI_MD_TABLE_LENS], edx
	inc dword ptr [rbx + TUI_LI_MD_TABLE_ROWS_N]
	jmp .Lpl_ret
.Lpl_tabfull:
	mov rdi, rbx
	call md_render_table
	mov dword ptr [rbx + TUI_LI_MD_TABLE_ON], 0
	mov dword ptr [rbx + TUI_LI_MD_TABLE_ROWS_N], 0
	mov rdi, rbx
	mov rsi, r12
	mov rdx, r13
	call md_esc_line
	jmp .Lpl_ret
.Lpl_tabclose:
	mov rdi, rbx
	call md_render_table
	mov dword ptr [rbx + TUI_LI_MD_TABLE_ON], 0
	mov dword ptr [rbx + TUI_LI_MD_TABLE_ROWS_N], 0
	jmp .Lpl_norm

/* ---- 表头候选待定 ---- */
.Lpl_cand:
	mov rsi, r12
	mov rdx, r13
	call md_is_delim
	test eax, eax
	jz .Lpl_candtext
	/* 分隔行成立：候选为表头 */
	mov dword ptr [rbx + TUI_LI_MD_TABLE_ON], 1
	mov dword ptr [rbx + TUI_LI_MD_TABLE_ROWS_N], 0
	lea rdi, [rbx + TUI_LI_MD_TABLE]
	mov rsi, rbx
	lea rsi, [rbx + TUI_LI_MD_CAND]
	/* dword 零扩展加载：qword 会带上相邻 TABLE_ON，钳制后污染 TABLE_LENS */
	mov edx, dword ptr [rbx + TUI_LI_MD_CAND_LEN]
	mov dword ptr [rbx + TUI_LI_MD_CAND_LEN], 0
	cmp rdx, TUI_LI_MD_ROW_MAX - 1
	jbe .Lpl_candcp
	mov edx, TUI_LI_MD_ROW_MAX - 1
.Lpl_candcp:
	mov qword ptr [rsp + 8], rdx
	call memcpy@PLT
	mov rdx, qword ptr [rsp + 8]
	mov dword ptr [rbx + TUI_LI_MD_TABLE_LENS], edx
	mov dword ptr [rbx + TUI_LI_MD_TABLE_ROWS_N], 1
	jmp .Lpl_ret
.Lpl_candtext:
	mov rdi, rbx
	lea rsi, [rbx + TUI_LI_MD_CAND]
	mov edx, dword ptr [rbx + TUI_LI_MD_CAND_LEN]
	xor ecx, ecx
	xor r8d, r8d
	call md_plain_line
	mov dword ptr [rbx + TUI_LI_MD_CAND_LEN], 0
	jmp .Lpl_norm

/* ---- 普通分发 ---- */
.Lpl_norm:
	/* 空行：连续空行折叠，流首空行抑制 */
	xor r14d, r14d
	xor r15, r15
.Lpl_bscan:
	cmp r15, r13
	jae .Lpl_bscand
	movzx eax, byte ptr [r12 + r15]
	cmp eax, ' '
	je .Lpl_bnext
	cmp eax, 9
	jne .Lpl_notblank
.Lpl_bnext:
	inc r15
	jmp .Lpl_bscan
.Lpl_bscand:
	mov r14d, 1
.Lpl_notblank:
	test r14d, r14d
	jz .Lpl_content
	cmp dword ptr [rbx + TUI_LI_MD_LAST_BLANK], 0
	jne .Lpl_ret
	cmp dword ptr [rbx + TUI_LI_MD_WROTE_ANY], 0
	je .Lpl_ret
	mov rdi, rbx
	call md_nl
	mov dword ptr [rbx + TUI_LI_MD_LAST_BLANK], 1
	jmp .Lpl_ret
.Lpl_content:
	mov dword ptr [rbx + TUI_LI_MD_LAST_BLANK], 0
	/* 前导空格 */
	xor r14d, r14d
.Lpl_psp:
	cmp r14, r13
	jae .Lpl_pdone
	cmp byte ptr [r12 + r14], ' '
	jne .Lpl_pdone
	inc r14
	jmp .Lpl_psp
.Lpl_pdone:
	mov qword ptr [rsp], r14
	/* 围栏开栏（≤3 空格） */
	cmp r14, 3
	ja .Lpl_nofence
	mov rax, r13
	sub rax, r14
	cmp rax, 3
	jb .Lpl_nofence
	movzx r15d, byte ptr [r12 + r14]
	cmp r15d, 0x60
	je .Lpl_fc
	cmp r15d, 0x7E
	jne .Lpl_nofence
.Lpl_fc:
	/* 数围栏字符（cap 32） */
	xor ecx, ecx
	mov rdx, r14
.Lpl_fcnt:
	cmp rdx, r13
	jae .Lpl_fcntd
	movzx eax, byte ptr [r12 + rdx]
	cmp eax, r15d
	jne .Lpl_fcntd
	inc rdx
	inc ecx
	cmp ecx, 32
	jae .Lpl_fcntd
	jmp .Lpl_fcnt
.Lpl_fcntd:
	cmp ecx, 3
	jb .Lpl_nofence
	/* 边框：围栏字符 + 语言（转义） */
	mov rdi, rbx
	mov rsi, r12
	add rsi, r14
	mov rdx, rcx
	mov qword ptr [rsp + 8], rcx
	call md_put_raw
	/* 语言 [p+n, len) 去两端空白 */
	mov rax, qword ptr [rsp + 8]
	add rax, r14
	mov qword ptr [rsp + 16], rax    /* ls */
	mov qword ptr [rsp + 24], r13    /* le */
.Lpl_fl:
	mov rax, qword ptr [rsp + 16]
	cmp rax, r13
	jae .Lpl_fr
	cmp byte ptr [r12 + rax], ' '
	je .Lpl_fla
	jmp .Lpl_fr
.Lpl_fla:
	inc qword ptr [rsp + 16]
	jmp .Lpl_fl
.Lpl_fr:
	mov rax, qword ptr [rsp + 24]
	cmp rax, qword ptr [rsp + 16]
	jbe .Lpl_frd
	movzx ecx, byte ptr [r12 + rax - 1]
	cmp ecx, ' '
	je .Lpl_fra
	cmp ecx, 9
	jne .Lpl_frd
.Lpl_fra:
	dec qword ptr [rsp + 24]
	jmp .Lpl_fr
.Lpl_frd:
	mov rax, qword ptr [rsp + 24]
	cmp rax, qword ptr [rsp + 16]
	jbe .Lpl_fnolang
	mov rdi, rbx
	mov rsi, r12
	add rsi, qword ptr [rsp + 16]
	mov rdx, rax
	sub rdx, qword ptr [rsp + 16]
	call md_put_esc
.Lpl_fnolang:
	mov rdi, rbx
	call md_nl
	mov dword ptr [rbx + TUI_LI_MD_FENCE], 1
	mov dword ptr [rbx + TUI_LI_MD_FENCE_CHAR], r15d
	mov eax, dword ptr [rsp + 8]
	mov dword ptr [rbx + TUI_LI_MD_FENCE_LEN], eax
	mov dword ptr [rbx + TUI_LI_MD_WROTE_ANY], 1
	jmp .Lpl_ret
.Lpl_nofence:
	/* 分隔线 */
	mov rsi, r12
	mov rdx, r13
	call md_is_hr
	test eax, eax
	jz .Lpl_nohr
	mov esi, dword ptr [rbx + TUI_LI_MD_COLS]
	cmp esi, 80
	jbe .Lpl_hrn
	mov esi, 80
.Lpl_hrn:
	mov rdi, rbx
	call md_bar_run
	mov rdi, rbx
	call md_nl
	mov dword ptr [rbx + TUI_LI_MD_WROTE_ANY], 1
	jmp .Lpl_ret
.Lpl_nohr:
	/* 标题 #×1..6 */
	mov rax, qword ptr [rsp]
	movzx ecx, byte ptr [r12 + rax]
	cmp ecx, '#'
	je .Lpl_head
	jmp .Lpl_nohead
.Lpl_head:
	mov qword ptr [rsp + 64], 0
.Lpl_hcnt:
	mov rax, qword ptr [rsp + 64]
	mov rdx, rax
	add rdx, qword ptr [rsp]
	cmp rdx, r13
	jae .Lpl_hcntd
	movzx ecx, byte ptr [r12 + rdx]
	cmp ecx, '#'
	jne .Lpl_hcntd
	inc qword ptr [rsp + 64]
	cmp qword ptr [rsp + 64], 7
	jae .Lpl_hcntd
	jmp .Lpl_hcnt
.Lpl_hcntd:
	/* 有效：1..6 个 # 且后随空格或行尾 */
	mov rax, qword ptr [rsp + 64]
	test rax, rax
	jz .Lpl_nohead
	cmp rax, 6
	ja .Lpl_nohead
	add rax, qword ptr [rsp]
	cmp rax, r13
	jae .Lpl_hvalid
	movzx ecx, byte ptr [r12 + rax]
	cmp ecx, ' '
	jne .Lpl_nohead
.Lpl_hvalid:
	mov rax, qword ptr [rsp + 64]
	add rax, qword ptr [rsp]
	mov qword ptr [rsp + 16], rax    /* cs */
	mov qword ptr [rsp + 24], r13    /* ce */
.Lpl_hls:
	mov rax, qword ptr [rsp + 16]
	cmp rax, r13
	jae .Lpl_hte
	cmp byte ptr [r12 + rax], ' '
	jne .Lpl_hte
	inc qword ptr [rsp + 16]
	jmp .Lpl_hls
.Lpl_hte:
	mov rax, qword ptr [rsp + 24]
	cmp rax, qword ptr [rsp + 16]
	jbe .Lpl_hok
	movzx ecx, byte ptr [r12 + rax - 1]
	cmp ecx, ' '
	je .Lpl_htea
	cmp ecx, 9
	jne .Lpl_hok
.Lpl_htea:
	dec qword ptr [rsp + 24]
	jmp .Lpl_hte
.Lpl_hok:
	mov rdi, rbx
	mov rsi, r12
	add rsi, qword ptr [rsp + 16]
	mov rdx, qword ptr [rsp + 24]
	sub rdx, qword ptr [rsp + 16]
	mov ecx, dword ptr [rsp + 64]
	call md_head_line
	jmp .Lpl_ret
.Lpl_nohead:
	/* 引用 > */
	mov rax, qword ptr [rsp]
	movzx ecx, byte ptr [r12 + rax]
	cmp ecx, '>'
	je .Lpl_quote
	jmp .Lpl_noquote
.Lpl_quote:
	mov rax, qword ptr [rsp]
	mov qword ptr [rsp + 8], rax     /* p2 */
	mov qword ptr [rsp + 16], 0      /* depth */
.Lpl_qloop:
	mov rax, qword ptr [rsp + 8]
	cmp rax, r13
	jae .Lpl_qdone
	movzx ecx, byte ptr [r12 + rax]
	cmp ecx, '>'
	jne .Lpl_qdone
	inc qword ptr [rsp + 16]
	inc qword ptr [rsp + 8]
.Lpl_qws:
	mov rax, qword ptr [rsp + 8]
	cmp rax, r13
	jae .Lpl_qloop
	movzx ecx, byte ptr [r12 + rax]
	cmp ecx, ' '
	je .Lpl_qwsa
	cmp ecx, 9
	jne .Lpl_qloop
.Lpl_qwsa:
	inc qword ptr [rsp + 8]
	jmp .Lpl_qws
.Lpl_qdone:
	cmp qword ptr [rsp + 16], 6
	jbe .Lpl_qcap
	mov qword ptr [rsp + 16], 6
.Lpl_qcap:
	mov rdi, rbx
	mov rsi, r12
	add rsi, qword ptr [rsp + 8]
	mov rdx, r13
	sub rdx, qword ptr [rsp + 8]
	mov ecx, dword ptr [rsp + 16]
	call md_quote_line
	jmp .Lpl_ret
.Lpl_noquote:
	/* 列表 */
	mov qword ptr [rsp + 24], 0      /* m = p */
	mov rax, qword ptr [rsp]
	mov qword ptr [rsp + 24], rax
	mov qword ptr [rsp + 72], 0      /* ns */
	mov dword ptr [rsp + 80], 0      /* numlen */
	mov dword ptr [rsp + 88], 0      /* ordered */
	mov rax, qword ptr [rsp + 24]
	movzx ecx, byte ptr [r12 + rax]
	cmp ecx, '*'
	je .Lpl_lmark
	cmp ecx, '+'
	je .Lpl_lmark
	cmp ecx, '-'
	je .Lpl_lmark
	cmp ecx, '0'
	jb .Lpl_nolist
	cmp ecx, '9'
	ja .Lpl_nolist
	/* 有序：数字串 + '.'/')' */
	mov qword ptr [rsp + 72], rax
	mov dword ptr [rsp + 80], 0
	mov dword ptr [rsp + 88], 1
.Lpl_ldap:
	mov rax, qword ptr [rsp + 24]
	cmp rax, r13
	jae .Lpl_lterm
	movzx ecx, byte ptr [r12 + rax]
	cmp ecx, '0'
	jb .Lpl_lterm
	cmp ecx, '9'
	ja .Lpl_lterm
	cmp dword ptr [rsp + 80], 9
	jae .Lpl_lterm
	inc qword ptr [rsp + 24]
	inc dword ptr [rsp + 80]
	jmp .Lpl_ldap
.Lpl_lterm:
	mov rax, qword ptr [rsp + 24]
	cmp rax, r13
	jae .Lpl_nolist
	movzx ecx, byte ptr [r12 + rax]
	cmp ecx, '.'
	je .Lpl_ltermok
	cmp ecx, ')'
	jne .Lpl_nolist
.Lpl_ltermok:
	cmp dword ptr [rsp + 80], 0
	je .Lpl_nolist
	inc qword ptr [rsp + 24]
	jmp .Lpl_lafter
.Lpl_lmark:
	inc qword ptr [rsp + 24]
.Lpl_lafter:
	/* marker 后必须空格或行尾 */
	mov rax, qword ptr [rsp + 24]
	cmp rax, r13
	jae .Lpl_lspace
	movzx ecx, byte ptr [r12 + rax]
	cmp ecx, ' '
	jne .Lpl_nolist
.Lpl_lspace:
	mov rax, qword ptr [rsp + 24]
	cmp rax, r13
	jae .Lpl_lbuild
	cmp byte ptr [r12 + rax], ' '
	jne .Lpl_lbuild
	inc qword ptr [rsp + 24]
	jmp .Lpl_lspace
.Lpl_lbuild:
	/* 前缀：depth×"  " + 标记 + ' '（+任务框） */
	mov rax, qword ptr [rsp]
	shr rax, 1
	cmp rax, 6
	jbe .Lpl_ldc
	mov eax, 6
.Lpl_ldc:
	mov qword ptr [rsp + 96], rax    /* depth */
	mov qword ptr [rsp + 16], 0      /* k */
.Lpl_ldloop:
	cmp qword ptr [rsp + 96], 0
	jbe .Lpl_ldone
	mov rax, qword ptr [rsp + 16]
	mov byte ptr [rsp + 32 + rax], ' '
	mov byte ptr [rsp + 33 + rax], ' '
	add qword ptr [rsp + 16], 2
	dec qword ptr [rsp + 96]
	jmp .Lpl_ldloop
.Lpl_ldone:
	cmp dword ptr [rsp + 88], 0
	je .Lpl_lbullet
	/* 数字 + '.' */
	mov rcx, qword ptr [rsp + 72]
	mov rdx, qword ptr [rsp + 16]
.Lpl_lnum:
	mov eax, dword ptr [rsp + 80]
	cmp rax, 0
	je .Lpl_lnumd
	movzx eax, byte ptr [r12 + rcx]
	mov byte ptr [rsp + 32 + rdx], al
	inc rcx
	inc rdx
	dec dword ptr [rsp + 80]
	jmp .Lpl_lnum
.Lpl_lnumd:
	mov byte ptr [rsp + 32 + rdx], '.'
	inc rdx
	mov qword ptr [rsp + 16], rdx
	jmp .Lpl_lsp
.Lpl_lbullet:
	mov rax, qword ptr [rsp + 16]
	mov byte ptr [rsp + 32 + rax], '-'
	inc rax
	mov qword ptr [rsp + 16], rax
.Lpl_lsp:
	mov rax, qword ptr [rsp + 16]
	mov byte ptr [rsp + 32 + rax], ' '
	inc rax
	mov qword ptr [rsp + 16], rax
	/* 任务项 [ ]/[x] */
	mov rax, qword ptr [rsp + 24]
	lea rdx, [rax + 4]
	cmp rdx, r13
	ja .Lpl_lempty
	cmp byte ptr [r12 + rax], '['
	jne .Lpl_lempty
	movzx ecx, byte ptr [r12 + rax + 2]
	cmp ecx, ']'
	jne .Lpl_lempty
	cmp byte ptr [r12 + rax + 3], ' '
	jne .Lpl_lempty
	movzx ecx, byte ptr [r12 + rax + 1]
	cmp ecx, ' '
	je .Lpl_ltodo
	cmp ecx, 'x'
	je .Lpl_lx
	cmp ecx, 'X'
	jne .Lpl_lempty
.Lpl_lx:
	mov rax, qword ptr [rsp + 16]
	mov byte ptr [rsp + 32 + rax], '['
	mov byte ptr [rsp + 33 + rax], 'x'
	mov byte ptr [rsp + 34 + rax], ']'
	mov byte ptr [rsp + 35 + rax], ' '
	add rax, 4
	mov qword ptr [rsp + 16], rax
	jmp .Lpl_ltodo2
.Lpl_ltodo:
	mov rax, qword ptr [rsp + 16]
	mov byte ptr [rsp + 32 + rax], '['
	mov byte ptr [rsp + 33 + rax], ' '
	mov byte ptr [rsp + 34 + rax], ']'
	mov byte ptr [rsp + 35 + rax], ' '
	add rax, 4
	mov qword ptr [rsp + 16], rax
.Lpl_ltodo2:
	add qword ptr [rsp + 24], 4
.Lpl_lempty:
	mov rdi, rbx
	mov rsi, r12
	add rsi, qword ptr [rsp + 24]
	mov rdx, r13
	sub rdx, qword ptr [rsp + 24]
	lea rcx, [rsp + 32]
	mov r8, qword ptr [rsp + 16]
	/* 空项：去掉前缀尾随空格（“-” 不输出 “- ”） */
	test rdx, rdx
	jnz .Lpl_lgo
	test r8, r8
	jz .Lpl_lgo
	cmp byte ptr [rsp + 32 + r8 - 1], ' '
	jne .Lpl_lgo
	dec r8
.Lpl_lgo:
	mov r9, r8
	call md_list_line
	jmp .Lpl_ret
.Lpl_nolist:
	/* 含 | → 表头候选 */
	xor r15d, r15d
	mov rax, r13
.Lpl_pscan:
	test rax, rax
	jz .Lpl_pscand
	cmp byte ptr [r12 + rax - 1], '|'
	jne .Lpl_pscan1
	mov r15d, 1
	jmp .Lpl_pscand
.Lpl_pscan1:
	dec rax
	jmp .Lpl_pscan
.Lpl_pscand:
	test r15d, r15d
	jz .Lpl_para
	lea rdi, [rbx + TUI_LI_MD_CAND]
	mov rsi, r12
	mov rdx, r13
	cmp rdx, TUI_LI_MD_LINE_MAX - 1
	jbe .Lpl_pcp
	mov edx, TUI_LI_MD_LINE_MAX - 1
.Lpl_pcp:
	mov qword ptr [rsp + 8], rdx
	call memcpy@PLT
	mov rdx, qword ptr [rsp + 8]
	mov dword ptr [rbx + TUI_LI_MD_CAND_LEN], edx
	jmp .Lpl_ret
.Lpl_para:
	/* 段落：去尾部空白 */
	mov qword ptr [rsp + 24], r13
.Lpl_ptrim:
	mov rax, qword ptr [rsp + 24]
	mov rdx, qword ptr [rsp]
	cmp rax, rdx
	jbe .Lpl_pok
	movzx ecx, byte ptr [r12 + rax - 1]
	cmp ecx, ' '
	je .Lpl_ptra
	cmp ecx, 9
	jne .Lpl_pok
.Lpl_ptra:
	dec qword ptr [rsp + 24]
	jmp .Lpl_ptrim
.Lpl_pok:
	mov rdi, rbx
	mov rsi, r12
	add rsi, qword ptr [rsp]
	mov rdx, qword ptr [rsp + 24]
	sub rdx, qword ptr [rsp]
	xor ecx, ecx
	xor r8d, r8d
	call md_plain_line
	jmp .Lpl_ret

/* ---- 围栏内容 ---- */
.Lpl_fence:
	xor r14d, r14d
.Lpl_fq:
	cmp r14, r13
	jae .Lpl_fqd
	cmp byte ptr [r12 + r14], ' '
	jne .Lpl_fqd
	inc r14
	cmp r14, 4
	jae .Lpl_fqd
	jmp .Lpl_fq
.Lpl_fqd:
	cmp r14, 4
	jae .Lpl_fcont
	/* 闭栏字符运行 */
	mov ecx, dword ptr [rbx + TUI_LI_MD_FENCE_CHAR]
	mov rdx, r14
	xor r15d, r15d
.Lpl_fc2:
	cmp rdx, r13
	jae .Lpl_fc2d
	movzx eax, byte ptr [r12 + rdx]
	cmp eax, ecx
	jne .Lpl_fc2d
	inc rdx
	inc r15d
	jmp .Lpl_fc2
.Lpl_fc2d:
	test r15d, r15d
	jz .Lpl_fcont
	cmp rdx, r13
	jne .Lpl_fcont
	/* 整行仅闭栏字符 */
	cmp r15d, dword ptr [rbx + TUI_LI_MD_FENCE_LEN]
	jb .Lpl_fpartial
	cmp dword ptr [rbx + TUI_LI_MD_HOLD_LEN], 0
	je .Lpl_fclose
	mov qword ptr [rsp + 8], r14
	mov qword ptr [rsp + 16], r15
	mov rdi, rbx
	lea rsi, [rbx + TUI_LI_MD_HOLD]
	mov edx, dword ptr [rbx + TUI_LI_MD_HOLD_LEN]
	call md_code_line
	mov dword ptr [rbx + TUI_LI_MD_HOLD_LEN], 0
	mov r14, qword ptr [rsp + 8]
	mov r15, qword ptr [rsp + 16]
.Lpl_fclose:
	mov rdi, rbx
	mov rsi, r12
	add rsi, r14
	mov rdx, r15
	call md_put_raw
	mov rdi, rbx
	call md_nl
	mov dword ptr [rbx + TUI_LI_MD_FENCE], 0
	mov dword ptr [rbx + TUI_LI_MD_WROTE_ANY], 1
	jmp .Lpl_ret
.Lpl_fpartial:
	/* 疑似部分闭栏：暂扣一拍 */
	cmp dword ptr [rbx + TUI_LI_MD_HOLD_LEN], 0
	je .Lpl_fhold
	mov rdi, rbx
	lea rsi, [rbx + TUI_LI_MD_HOLD]
	mov edx, dword ptr [rbx + TUI_LI_MD_HOLD_LEN]
	call md_code_line
.Lpl_fhold:
	lea rdi, [rbx + TUI_LI_MD_HOLD]
	mov rsi, r12
	mov rdx, r13
	cmp rdx, TUI_LI_MD_LINE_MAX - 1
	jbe .Lpl_fhc
	mov edx, TUI_LI_MD_LINE_MAX - 1
.Lpl_fhc:
	mov qword ptr [rsp + 8], rdx
	call memcpy@PLT
	mov rdx, qword ptr [rsp + 8]
	mov dword ptr [rbx + TUI_LI_MD_HOLD_LEN], edx
	jmp .Lpl_ret
.Lpl_fcont:
	cmp dword ptr [rbx + TUI_LI_MD_HOLD_LEN], 0
	je .Lpl_fcontent
	mov rdi, rbx
	lea rsi, [rbx + TUI_LI_MD_HOLD]
	mov edx, dword ptr [rbx + TUI_LI_MD_HOLD_LEN]
	call md_code_line
	mov dword ptr [rbx + TUI_LI_MD_HOLD_LEN], 0
.Lpl_fcontent:
	mov rdi, rbx
	mov rsi, r12
	mov rdx, r13
	call md_code_line
.Lpl_ret:
	add rsp, 128
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	ret
 .cfi_endproc
.size md_process_line,.-md_process_line

.section .note.GNU-stack,"",@progbits
