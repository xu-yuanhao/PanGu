/* tui_tool.S — 工具执行展示状态机（契约与 Pi 行为差异见 tui_tool.h）。
 *
 * 寄存器约定（内部例程）：rbx=t、r12=rec 贯穿；put/raw 回调与 libc 调用按
 * C ABI clobber caller-saved，跨调用的值放 callee-saved 或栈槽。
 * 栈对齐：入口 rsp≡8 (mod 16)，push n*8 + sub S 后调用点需 ≡0，即总调整 ≡8 (mod 16)。
 */
#include "tui_layout.h"
.intel_syntax noprefix

.section .rodata
.Ltool_pfx:    .asciz "[tool] "
.Ltool_pfx_l = . - .Ltool_pfx - 1
.Lhash:        .asciz " (#"
.Lhash_l = . - .Lhash - 1
.Lrp_sfx:      .asciz ") "
.Lrp_l = . - .Lrp_sfx - 1
.Lstarted_sfx: .asciz "started\n"
.Lstarted_l = . - .Lstarted_sfx - 1
.Lok:          .asciz "ok"
.Lok_l = . - .Lok - 1
.Lerr:         .asciz "error"
.Lerr_l = . - .Lerr - 1
.Lcolon:       .asciz ": "
.Lcolon_l = . - .Lcolon - 1
.Lexpand_hdr:  .asciz "[expand] #"
.Lexpand_l = . - .Lexpand_hdr - 1
.Llossy_note:  .asciz " (earlier output dropped)"
.Llossy_l = . - .Llossy_note - 1
.Lnl:          .asciz "\n"
.Lq:           .asciz "?"
.Lsgr_bold:    .asciz "\x1b[1m"
.Lsgr_bold_l = . - .Lsgr_bold - 1
.Lsgr_off_b:   .asciz "\x1b[22m"
.Lsgr_offb_l = . - .Lsgr_off_b - 1
.Lsgr_ok:      .asciz "\x1b[32m"
.Lsgr_ok_l = . - .Lsgr_ok - 1
.Lsgr_err:     .asciz "\x1b[31m"
.Lsgr_err_l = . - .Lsgr_err - 1
.Lsgr_dim:     .asciz "\x1b[2m"
.Lsgr_dim_l = . - .Lsgr_dim - 1
.Lsgr_off:     .asciz "\x1b[0m"
.Lsgr_off_l = . - .Lsgr_off - 1
.Lfmt_hint:    .asciz "... (%u more lines, to expand)"
.Lfmt_marker:  .asciz "…[+%zu 字节截断]"
.Lfmt_image:   .asciz "[Image: [%s] %ux%u]"
.Limg_unknown: .asciz "image/unknown"
.Lpending_expand: .asciz "[tool] output available: Ctrl-O to expand\n"
.Lpending_expand_l = . - .Lpending_expand - 1

.section .text

/* ---- 内部：emit_put(rbx=t, rsi=data, rdx=len) ---- */
.type tt_put,@function
tt_put:
 mov rax, qword ptr [rbx + TUI_LI_TOOL_EMIT_PUT]
 mov rdi, qword ptr [rbx + TUI_LI_TOOL_EMIT_CTX]
 jmp rax
.size tt_put,.-tt_put

/* ---- 内部：emit_raw(rbx=t, rsi=data, rdx=len) ---- */
.type tt_raw,@function
tt_raw:
 mov rax, qword ptr [rbx + TUI_LI_TOOL_EMIT_RAW]
 mov rdi, qword ptr [rbx + TUI_LI_TOOL_EMIT_CTX]
 jmp rax
.size tt_raw,.-tt_raw

/* ---- 内部：slot(rbx=t, rax=idx) → rax=rec 指针 ---- */
.type tt_slot,@function
tt_slot:
 imul rax, rax, TUI_LI_REC_SIZE
 add rax, rbx
 add rax, TUI_LI_TOOL_SLOT0
 ret
.size tt_slot,.-tt_slot

/* ---- void tui_tool_init(t, ctx, put, raw, ansi) ----
 * rdi=t rsi=ctx rdx=put rcx=raw r8d=ansi */
.globl tui_tool_init
.type tui_tool_init,@function
tui_tool_init:
 mov qword ptr [rdi + TUI_LI_TOOL_EMIT_CTX], rsi
 mov qword ptr [rdi + TUI_LI_TOOL_EMIT_PUT], rdx
 mov qword ptr [rdi + TUI_LI_TOOL_EMIT_RAW], rcx
 mov dword ptr [rdi + TUI_LI_TOOL_ANSI], r8d
 mov dword ptr [rdi + TUI_LI_TOOL_ACTIVE], 0
 mov dword ptr [rdi + TUI_LI_TOOL_LAST_FIN], TUI_LI_TOOL_SLOTS /* 无 */
 push rbx
 mov rbx, rdi
 lea rdi, [rbx + TUI_LI_TOOL_SLOT0]
 xor esi, esi
 mov edx, TUI_LI_TOOL_SIZE - TUI_LI_TOOL_SLOT0
 call memset@PLT
 pop rbx
 ret
.size tui_tool_init,.-tui_tool_init

/* ---- 内部：tt_find(rdi=t, rsi=id, rdx=idlen) → eax=下标或 TUI_LI_TOOL_SLOTS。
 * 两遍扫描：先匹配 pending 槽，再匹配任意槽（同 ID 重启）。 ---- */
.type tt_find,@function
tt_find:
 push rbx
 push r12
 push r13
 push r14
 sub rsp, 24
 mov [rsp], rsi       /* id */
 mov [rsp+8], rdx     /* idlen */
 mov rbx, rdi
 xor r13d, r13d       /* pass 0：仅 pending */
.Lfind_pass:
 xor r14d, r14d       /* idx */
.Lfind_loop:
 cmp r14d, TUI_LI_TOOL_SLOTS
 jae .Lfind_pass_end
 imul rax, r14, TUI_LI_REC_SIZE
 lea r12, [rbx + rax + TUI_LI_TOOL_SLOT0]
 test r13d, r13d
 jnz .Lfind_any
 cmp dword ptr [r12 + TUI_LI_REC_STATE], 0
 jne .Lfind_next
.Lfind_any:
 mov eax, dword ptr [r12 + TUI_LI_REC_ID_LEN]
 cmp rax, [rsp+8]
 jne .Lfind_next
 test eax, eax
 jz .Lfind_next
 lea rdi, [r12 + TUI_LI_REC_ID]
 mov rsi, [rsp]
 mov rdx, [rsp+8]
 call memcmp@PLT
 test eax, eax
 jnz .Lfind_next
 mov eax, r14d
 jmp .Lfind_out
.Lfind_next:
 inc r14d
 jmp .Lfind_loop
.Lfind_pass_end:
 test r13d, r13d
 jnz .Lfind_none
 mov r13d, 1
 jmp .Lfind_pass
.Lfind_none:
 mov eax, TUI_LI_TOOL_SLOTS
.Lfind_out:
 add rsp, 24
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size tt_find,.-tt_find

/* ---- 内部：tt_new(rdi=t) → eax=新槽下标（轮转置换） ---- */
.type tt_new,@function
tt_new:
 mov ecx, dword ptr [rdi + TUI_LI_TOOL_ACTIVE]
 inc ecx
 cmp ecx, TUI_LI_TOOL_SLOTS
 jb .Lnew_ok
 xor ecx, ecx
.Lnew_ok:
 mov dword ptr [rdi + TUI_LI_TOOL_ACTIVE], ecx
 mov eax, ecx
 ret
.size tt_new,.-tt_new

/* ---- 内部：tt_reset(rdi=rec, rsi=name, rdx=id)：码点边界有界复制 + 清零 ---- */
.type tt_reset,@function
tt_reset:
 push rbx
 push r12
 push r13
 sub rsp, 16
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov rdi, r12
 call strlen@PLT
 mov rdi, r12
 mov rsi, rax
 mov edx, TUI_LI_TOOL_NAME_MAX - 1
 call tui_utf8_truncate_len@PLT
 mov dword ptr [rbx + TUI_LI_REC_NAME_LEN], eax
 lea rdi, [rbx + TUI_LI_REC_NAME]
 mov rsi, r12
 mov rdx, rax
 call memcpy@PLT
 mov rdi, r13
 call strlen@PLT
 mov rdi, r13
 mov rsi, rax
 mov edx, TUI_LI_TOOL_ID_MAX - 1
 call tui_utf8_truncate_len@PLT
 mov dword ptr [rbx + TUI_LI_REC_ID_LEN], eax
 lea rdi, [rbx + TUI_LI_REC_ID]
 mov rsi, r13
 mov rdx, rax
 call memcpy@PLT
 mov dword ptr [rbx + TUI_LI_REC_STATE], 0
 mov dword ptr [rbx + TUI_LI_REC_EXPANDED], 0
 mov dword ptr [rbx + TUI_LI_REC_OUTLINES], 0
 mov dword ptr [rbx + TUI_LI_REC_SHOWN], 0
 mov dword ptr [rbx + TUI_LI_REC_HINTED], 0
 mov dword ptr [rbx + TUI_LI_REC_TAIL_LEN], 0
 mov dword ptr [rbx + TUI_LI_REC_LOSSY], 0
 mov dword ptr [rbx + TUI_LI_REC_LINE_LEN], 0
 add rsp, 16
 pop r13
 pop r12
 pop rbx
 ret
.size tt_reset,.-tt_reset

/* ---- 内部：tt_line_emit(rbx=t, r12=rec, rsi=data, edx=len)：计一条输出行。
 * 预览期内实时输出（补 '\n'），预览满后转入保留尾部（尾部自补 '\n'）。 ---- */
.type tt_line_emit,@function
tt_line_emit:
 push rbx
 push r12
 sub rsp, 8
 inc dword ptr [r12 + TUI_LI_REC_OUTLINES]
 cmp dword ptr [r12 + TUI_LI_REC_EXPANDED], 0
 jne .Lle_visible
 mov eax, dword ptr [r12 + TUI_LI_REC_SHOWN]
 cmp eax, TUI_LI_TOOL_PREVIEW
 jae .Lle_tail
.Lle_visible:
 call tt_put
 lea rsi, [rip + .Lnl]
 mov edx, 1
 call tt_put
 inc dword ptr [r12 + TUI_LI_REC_SHOWN]
.Lle_out:
 add rsp, 8
 pop r12
 pop rbx
 ret
.Lle_tail:
 call tt_tail_append /* 合成 '\n' 在 tt_tail_append 内补 */
 jmp .Lle_out
.size tt_line_emit,.-tt_line_emit

/* ---- 内部：tt_line_flush(rbx=t, r12=rec) → eax：行缓冲非空则作为完整行输出 ---- */
.type tt_line_flush,@function
tt_line_flush:
 cmp dword ptr [r12 + TUI_LI_REC_LINE_LEN], 0
 je .Llf_zero
 lea rsi, [r12 + TUI_LI_REC_LINE]
 mov edx, dword ptr [r12 + TUI_LI_REC_LINE_LEN]
 sub rsp, 8
 call tt_line_emit
 add rsp, 8
 mov dword ptr [r12 + TUI_LI_REC_LINE_LEN], 0
 mov eax, 1
 ret
.Llf_zero:
 xor eax, eax
 ret
.size tt_line_flush,.-tt_line_flush

/* ---- 内部：tt_tail_append(r12=rec, rsi=data, edx=len)：保留尾部 + '\n'。
 * 溢出按整行丢最旧；单行超限整行丢弃并置 lossy。 ---- */
.type tt_tail_append,@function
tt_tail_append:
 push r12
 push r13
 push r14
 push r15
 sub rsp, 8
 mov r13, rdx         /* len */
 mov r15, rsi         /* data */
 lea rax, [rdx + 1]   /* need = len+1 */
 cmp rax, TUI_LI_TOOL_TAIL_MAX
 ja .Lta_lossy
 mov ecx, dword ptr [r12 + TUI_LI_REC_TAIL_LEN]
 add ecx, edx
 inc ecx              /* tail_len + need */
 cmp ecx, TUI_LI_TOOL_TAIL_MAX
 jbe .Lta_room
 mov r14d, ecx
 sub r14d, TUI_LI_TOOL_TAIL_MAX /* 需释放的字节数 */
 xor ecx, ecx
.Lta_scan:
 cmp ecx, dword ptr [r12 + TUI_LI_REC_TAIL_LEN]
 jae .Lta_drop_all
 cmp byte ptr [r12 + TUI_LI_REC_TAIL + rcx], 10
 jne .Lta_next
 lea eax, [ecx + 1]
 cmp eax, r14d
 jae .Lta_cut
.Lta_next:
 inc ecx
 jmp .Lta_scan
.Lta_cut:
 mov dword ptr [r12 + TUI_LI_REC_LOSSY], 1
 lea r9d, [ecx + 1]               /* cut = 行尾位置+1 */
 mov r8d, dword ptr [r12 + TUI_LI_REC_TAIL_LEN]
 sub r8d, r9d                     /* rem */
 lea rdi, [r12 + TUI_LI_REC_TAIL]
 lea rsi, [r12 + TUI_LI_REC_TAIL]
 add rsi, rcx
 inc rsi                          /* tail + cut */
 mov rdx, r8
 sub dword ptr [r12 + TUI_LI_REC_TAIL_LEN], r9d
 call memmove@PLT
 jmp .Lta_room
.Lta_drop_all:
 mov dword ptr [r12 + TUI_LI_REC_TAIL_LEN], 0
 mov dword ptr [r12 + TUI_LI_REC_LOSSY], 1
.Lta_room:
 mov ecx, dword ptr [r12 + TUI_LI_REC_TAIL_LEN]
 lea rdi, [r12 + TUI_LI_REC_TAIL]
 add rdi, rcx
 mov rsi, r15
 mov rdx, r13
 call memcpy@PLT
 mov ecx, dword ptr [r12 + TUI_LI_REC_TAIL_LEN]
 add ecx, r13d
 mov byte ptr [r12 + TUI_LI_REC_TAIL + rcx], 10 /* 合成行尾（含完成时的尾行） */
 inc ecx
 mov dword ptr [r12 + TUI_LI_REC_TAIL_LEN], ecx
.Lta_out:
 add rsp, 8
 pop r15
 pop r14
 pop r13
 pop r12
 ret
.Lta_lossy:
 mov dword ptr [r12 + TUI_LI_REC_LOSSY], 1
 jmp .Lta_out
.size tt_tail_append,.-tt_tail_append

/* ---- int tui_tool_started(t, name, call_id) ----
 * rdi=t rsi=name rdx=call_id。槽匹配/轮转 + 重置 + 头行。 */
.globl tui_tool_started
.type tui_tool_started,@function
tui_tool_started:
 push rbx
 push r12
 push r13
 push r14
 push r15
 push rbp
 sub rsp, 8
 mov rbx, rdi
 test rsi, rsi
 jnz .Lst_name_arg
 lea rsi, [rip + .Lq]
.Lst_name_arg:
 mov r12, rsi
 test rdx, rdx
 jnz .Lst_id_arg
 lea rdx, [rip + .Lq]
.Lst_id_arg:
 mov r13, rdx
 mov rdi, r13
 call strlen@PLT
 mov r14, rax
 mov rdi, rbx
 mov rsi, r13
 mov rdx, r14
 call tt_find
 cmp eax, TUI_LI_TOOL_SLOTS
 je .Lst_fresh
 call tt_slot
 jmp .Lst_have
.Lst_fresh:
 mov rdi, rbx
 call tt_new
 call tt_slot
.Lst_have:
 mov rbp, rax        /* rec */
 mov rdi, rbp
 mov rsi, r12
 mov rdx, r13
 call tt_reset
 lea rsi, [rip + .Ltool_pfx]
 mov edx, OFFSET .Ltool_pfx_l
 call tt_raw
 cmp dword ptr [rbx + TUI_LI_TOOL_ANSI], 0
 je .Lst_body
 lea rsi, [rip + .Lsgr_bold]
 mov edx, OFFSET .Lsgr_bold_l
 call tt_raw
.Lst_body:
 lea rsi, [rbp + TUI_LI_REC_NAME]
 mov edx, dword ptr [rbp + TUI_LI_REC_NAME_LEN]
 call tt_put
 cmp dword ptr [rbx + TUI_LI_TOOL_ANSI], 0
 je .Lst_tail
 lea rsi, [rip + .Lsgr_off_b]
 mov edx, OFFSET .Lsgr_offb_l
 call tt_raw
.Lst_tail:
 lea rsi, [rip + .Lhash]
 mov edx, OFFSET .Lhash_l
 call tt_raw
 lea rsi, [rbp + TUI_LI_REC_ID]
 mov edx, dword ptr [rbp + TUI_LI_REC_ID_LEN]
 call tt_put
 lea rsi, [rip + .Lrp_sfx]
 mov edx, OFFSET .Lrp_l
 call tt_raw
 lea rsi, [rip + .Lstarted_sfx]
 mov edx, OFFSET .Lstarted_l
 call tt_raw
 mov eax, 1
 add rsp, 8
 pop rbp
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size tui_tool_started,.-tui_tool_started

/* ---- int tui_tool_output(t, call_id, text, len) ----
 * rdi=t rsi=call_id rdx=text rcx=len。行缓冲累积 + 预览/保留分发。
 * 栈：[0]=call_id [8]=idlen [16]=wrote [24]=keep 暂存。 */
.globl tui_tool_output
.type tui_tool_output,@function
tui_tool_output:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 32
 mov [rsp], rsi
 mov rbx, rdi
 mov r14, rdx         /* text */
 mov r15, rcx         /* len */
 test rsi, rsi
 jz .Lo_none
 mov rdi, rsi
 call strlen@PLT
 mov [rsp+8], rax
 mov rdi, rbx
 mov rsi, [rsp]
 mov rdx, [rsp+8]
 call tt_find
 cmp eax, TUI_LI_TOOL_SLOTS
 je .Lo_none
 call tt_slot
 mov r12, rax         /* rec */
 mov qword ptr [rsp+16], 0
 xor r13d, r13d       /* i */
.Lo_scan:
 cmp r13, r15
 jae .Lo_done
 movzx eax, byte ptr [r14 + r13]
 inc r13
 cmp eax, 10
 je .Lo_nl
 mov ecx, dword ptr [r12 + TUI_LI_REC_LINE_LEN]
 cmp ecx, TUI_LI_TOOL_LINE_MAX
 jae .Lo_full /* 防御：split 后不应再满 */
 mov byte ptr [r12 + TUI_LI_REC_LINE + rcx], al
 inc ecx
 mov dword ptr [r12 + TUI_LI_REC_LINE_LEN], ecx
 cmp ecx, TUI_LI_TOOL_LINE_MAX
 jb .Lo_scan
.Lo_full:
	/* 满行：按码点边界拆分，keep 作为一条行输出，余量回移 */
	mov rdi, r12
	add rdi, TUI_LI_REC_LINE
	mov esi, ecx
	mov edx, TUI_LI_TOOL_LINE_MAX
	call tui_utf8_truncate_len@PLT
	test rax, rax
	jnz .Lo_keep_ok
	mov eax, 1 /* 首字节即非法序列：保底推进，避免卡死 */
.Lo_keep_ok:
	mov [rsp+24], rax
	lea rsi, [r12 + TUI_LI_REC_LINE]
	mov edx, eax
	call tt_line_emit
	mov rax, [rsp+24]
	lea rdi, [r12 + TUI_LI_REC_LINE]
	lea rsi, [r12 + TUI_LI_REC_LINE]
	add rsi, rax
	mov rdx, TUI_LI_TOOL_LINE_MAX
	sub rdx, rax
	call memmove@PLT
	mov rax, [rsp+24]
	mov ecx, TUI_LI_TOOL_LINE_MAX
	sub ecx, eax
	mov dword ptr [r12 + TUI_LI_REC_LINE_LEN], ecx
	mov qword ptr [rsp+16], 1
	jmp .Lo_scan
.Lo_nl:
 /* An explicit LF closes even an empty source line. Terminal flush, by
  * contrast, must not synthesize an extra empty line after a trailing LF. */
 lea rsi, [r12 + TUI_LI_REC_LINE]
 mov edx, dword ptr [r12 + TUI_LI_REC_LINE_LEN]
 call tt_line_emit
 mov dword ptr [r12 + TUI_LI_REC_LINE_LEN], 0
	mov qword ptr [rsp+16], 1
	jmp .Lo_scan
.Lo_done:
	mov rax, [rsp+16]
	jmp .Lo_out
.Lo_none:
	xor eax, eax
.Lo_out:
	add rsp, 32
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	ret
.size tui_tool_output,.-tui_tool_output

/* ---- int tui_tool_finished(t, call_id, ok, detail, detail_len) ----
 * rdi=t rsi=call_id edx=ok rcx=detail r8d=detail_len。
 * 栈：[0]=detail [8]=dlen [16..23]=idlen [24..31]=idx [32..95]=提示缓冲(64)
 * [96..103]=id 指针 [104..167]=截断标记缓冲(64) [168..175]=keep。
 * 总调整 48+184=232 ≡ 8 (mod 16)。 */
.globl tui_tool_finished
.type tui_tool_finished,@function
tui_tool_finished:
	push rbx
	push r12
	push r13
	push r14
	push r15
	push rbp
	sub rsp, 184
	mov [rsp], rcx       /* detail */
	mov [rsp+8], r8      /* dlen */
	mov r14d, edx        /* ok */
	mov rbx, rdi
	test rsi, rsi
	jnz .Lfin_id
	lea rsi, [rip + .Lq]
.Lfin_id:
	mov rdi, rsi
	mov [rsp+96], rsi    /* id 指针 */
	call strlen@PLT
	mov [rsp+16], rax
	mov rdi, rbx
	mov rsi, [rsp+96]
	mov rdx, [rsp+16]
	call tt_find
	mov [rsp+24], rax
	cmp eax, TUI_LI_TOOL_SLOTS
	je .Lfin_fresh
	call tt_slot
	mov r12, rax
	jmp .Lfin_have
.Lfin_fresh:
	mov rdi, rbx
	call tt_new
	mov [rsp+24], rax
	call tt_slot
	mov r12, rax
	mov rdi, r12
	lea rsi, [rip + .Lq]
	mov rdx, [rsp+96]
	call tt_reset
.Lfin_have:
	mov r13, [rsp+24]    /* idx */
	call tt_line_flush   /* 行缓冲冲刷（rbx=t, r12=rec） */
	test r14d, r14d
	jz .Lfin_errstate
	mov dword ptr [r12 + TUI_LI_REC_STATE], 1
	jmp .Lfin_state_done
.Lfin_errstate:
	mov dword ptr [r12 + TUI_LI_REC_STATE], 2
.Lfin_state_done:
	mov [rbx + TUI_LI_TOOL_LAST_FIN], r13d
	/* 状态行 */
	lea rsi, [rip + .Ltool_pfx]
	mov edx, OFFSET .Ltool_pfx_l
	call tt_raw
	cmp dword ptr [rbx + TUI_LI_TOOL_ANSI], 0
	je .Lfin_name
	lea rsi, [rip + .Lsgr_bold]
	mov edx, OFFSET .Lsgr_bold_l
	call tt_raw
.Lfin_name:
	lea rsi, [r12 + TUI_LI_REC_NAME]
	mov edx, dword ptr [r12 + TUI_LI_REC_NAME_LEN]
	call tt_put
	cmp dword ptr [rbx + TUI_LI_TOOL_ANSI], 0
	je .Lfin_mid
	lea rsi, [rip + .Lsgr_off_b]
	mov edx, OFFSET .Lsgr_offb_l
	call tt_raw
.Lfin_mid:
	lea rsi, [rip + .Lhash]
	mov edx, OFFSET .Lhash_l
	call tt_raw
	lea rsi, [r12 + TUI_LI_REC_ID]
	mov edx, dword ptr [r12 + TUI_LI_REC_ID_LEN]
	call tt_put
	lea rsi, [rip + .Lrp_sfx]
	mov edx, OFFSET .Lrp_l
	call tt_raw
	cmp dword ptr [rbx + TUI_LI_TOOL_ANSI], 0
	je .Lfin_status
	lea rsi, [rip + .Lsgr_ok]
	mov edx, OFFSET .Lsgr_ok_l
	test r14d, r14d
	jnz .Lfin_sgr
	lea rsi, [rip + .Lsgr_err]
	mov edx, OFFSET .Lsgr_err_l
.Lfin_sgr:
	call tt_raw
.Lfin_status:
	test r14d, r14d
	jz .Lfin_word_err
	lea rsi, [rip + .Lok]
	mov edx, OFFSET .Lok_l
	jmp .Lfin_word
.Lfin_word_err:
	lea rsi, [rip + .Lerr]
	mov edx, OFFSET .Lerr_l
.Lfin_word:
	call tt_raw
	cmp dword ptr [rbx + TUI_LI_TOOL_ANSI], 0
	je .Lfin_detail
	lea rsi, [rip + .Lsgr_off]
	mov edx, OFFSET .Lsgr_off_l
	call tt_raw
.Lfin_detail:
	cmp qword ptr [rsp], 0
	je .Lfin_hint
	cmp qword ptr [rsp+8], 0
	je .Lfin_hint
	lea rsi, [rip + .Lcolon]
	mov edx, OFFSET .Lcolon_l
	call tt_raw
	mov rdi, [rsp]
	mov rsi, [rsp+8]
	mov edx, TUI_LI_VIEW_TOOL_SNIPPET
	call tui_utf8_truncate_len@PLT
	mov [rsp+168], rax
	mov rdi, rbx
	mov rsi, [rsp]
	mov rdx, rax
	call tt_put
	/* 截断标记：保持既有有界展示契约（…[+N 字节截断]） */
	mov rax, [rsp+168]
	cmp rax, [rsp+8]
	jae .Lfin_hint
	lea rdi, [rsp+104]
	mov esi, 64
	lea rdx, [rip + .Lfmt_marker]
	mov rcx, [rsp+8]
	sub rcx, rax
	xor eax, eax
	call snprintf@PLT
	test eax, eax
	jle .Lfin_hint
	cmp eax, 63
	jle .Lfin_mlen
	mov eax, 63
.Lfin_mlen:
	lea rsi, [rsp+104]
	mov edx, eax
	call tt_put
.Lfin_hint:
	lea rsi, [rip + .Lnl]
	mov edx, 1
	call tt_raw
	/* 隐藏行提示 */
	mov eax, dword ptr [r12 + TUI_LI_REC_OUTLINES]
	cmp eax, dword ptr [r12 + TUI_LI_REC_SHOWN]
	jbe .Lfin_out
	cmp dword ptr [r12 + TUI_LI_REC_HINTED], 0
	jne .Lfin_out
	mov dword ptr [r12 + TUI_LI_REC_HINTED], 1
	cmp dword ptr [rbx + TUI_LI_TOOL_ANSI], 0
	je .Lfin_hint_txt
	lea rsi, [rip + .Lsgr_dim]
	mov edx, OFFSET .Lsgr_dim_l
	call tt_raw
.Lfin_hint_txt:
	mov eax, dword ptr [r12 + TUI_LI_REC_OUTLINES]
	sub eax, dword ptr [r12 + TUI_LI_REC_SHOWN]
	lea rdi, [rsp + 32]
	mov esi, 64
	lea rdx, [rip + .Lfmt_hint]
	mov ecx, eax
	xor eax, eax
	call snprintf@PLT
	test eax, eax
	jle .Lfin_hint_plain
	cmp eax, 64
	jge .Lfin_hint_plain
	lea rsi, [rsp + 32]
	mov edx, eax
	call tt_put
.Lfin_hint_plain:
	cmp dword ptr [rbx + TUI_LI_TOOL_ANSI], 0
	je .Lfin_hint_nl
	lea rsi, [rip + .Lsgr_off]
	mov edx, OFFSET .Lsgr_off_l
	call tt_raw
.Lfin_hint_nl:
	lea rsi, [rip + .Lnl]
	mov edx, 1
	call tt_put
.Lfin_out:
	mov eax, 1
	add rsp, 184
	pop rbp
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	ret
.size tui_tool_finished,.-tui_tool_finished

/* Ctrl-O applies to all currently retained call components (Pi containers).
 * Scrolling output cannot retract lines: expansion remains one-way. */
.globl tui_tool_expand
.type tui_tool_expand,@function
tui_tool_expand:
 push rbx
 push r12
 push r13
 mov rbx, rdi
 xor r12d, r12d
 xor r13d, r13d
.Lexpand_all:
 mov rdi, rbx
 mov esi, r12d
 call tt_expand_one
 or r13d, eax
 inc r12d
 cmp r12d, TUI_LI_TOOL_SLOTS
 jb .Lexpand_all
 mov eax, r13d
 pop r13
 pop r12
 pop rbx
 ret
.size tui_tool_expand,.-tui_tool_expand

/* Expand a retained slot; no borrowed arguments survive a sink callback. */
.type tt_expand_one,@function
tt_expand_one:
 push rbx
 push r12
 sub rsp, 8
 mov rbx, rdi
 mov eax, esi
 call tt_slot
 cmp dword ptr [rax + TUI_LI_REC_NAME_LEN], 0
 je .Lex_none
	mov r12, rax
	cmp dword ptr [r12 + TUI_LI_REC_EXPANDED], 0
	jne .Lex_none
 cmp dword ptr [r12 + TUI_LI_REC_STATE], 0
 je .Lex_enable
	mov eax, dword ptr [r12 + TUI_LI_REC_OUTLINES]
	cmp eax, dword ptr [r12 + TUI_LI_REC_SHOWN]
	jbe .Lex_none
.Lex_enable:
	mov dword ptr [r12 + TUI_LI_REC_EXPANDED], 1
	lea rsi, [rip + .Lexpand_hdr]
	mov edx, OFFSET .Lexpand_l
	call tt_raw
	lea rsi, [r12 + TUI_LI_REC_ID]
	mov edx, dword ptr [r12 + TUI_LI_REC_ID_LEN]
	call tt_put
	cmp dword ptr [r12 + TUI_LI_REC_LOSSY], 0
	je .Lex_nl
	lea rsi, [rip + .Llossy_note]
	mov edx, OFFSET .Llossy_l
	call tt_raw
.Lex_nl:
	lea rsi, [rip + .Lnl]
	mov edx, 1
	call tt_raw
	cmp dword ptr [r12 + TUI_LI_REC_TAIL_LEN], 0
	je .Lex_one
	lea rsi, [r12 + TUI_LI_REC_TAIL]
	mov edx, dword ptr [r12 + TUI_LI_REC_TAIL_LEN]
	call tt_put
.Lex_one:
 mov eax, dword ptr [r12 + TUI_LI_REC_OUTLINES]
 mov dword ptr [r12 + TUI_LI_REC_SHOWN], eax
 mov dword ptr [r12 + TUI_LI_REC_TAIL_LEN], 0
	mov eax, 1
	add rsp, 8
	pop r12
	pop rbx
	ret
.Lex_none:
	xor eax, eax
	add rsp, 8
	pop r12
	pop rbx
	ret
.size tt_expand_one,.-tt_expand_one

/* ---- size_t tui_tool_image_note(buf, cap, mime, w, h) ----
 * rdi=buf rsi=cap rdx=mime ecx=w r8d=h。放不下/失败返回 0。 */
.globl tui_tool_image_note
.type tui_tool_image_note,@function
tui_tool_image_note:
	push rbx
	push r12
	push rbp
	mov rbx, rdi
	mov r12d, esi
	test rdx, rdx
	jnz .Limg_have
	lea rdx, [rip + .Limg_unknown]
.Limg_have:
	mov r9d, r8d         /* h */
	mov r8d, ecx         /* w */
	mov rcx, rdx         /* mime */
	lea rdx, [rip + .Lfmt_image]
	xor eax, eax
	call snprintf@PLT
	mov ebp, eax
	test ebp, ebp
	jle .Limg_fail
	cmp ebp, r12d
	jae .Limg_fail /* 截断视为失败 */
	mov eax, ebp
	pop rbp
	pop r12
	pop rbx
	ret
.Limg_fail:
	xor eax, eax
	pop rbp
	pop r12
	pop rbx
	ret
.size tui_tool_image_note,.-tui_tool_image_note

/* Terminal boundary: preserve partial output even when no tool_finished arrived.
 * State/outcome remain owned by Runtime. Callee-saved loop state survives sinks. */
.globl tui_tool_flush
.type tui_tool_flush,@function
tui_tool_flush:
 push rbx
 push r12
 push r13
 push r14
 sub rsp, 8
 mov rbx, rdi
 xor r13d, r13d
 xor r14d, r14d
.Ltf_loop:
 mov eax, r13d
 call tt_slot
 mov r12, rax
 cmp dword ptr [r12 + TUI_LI_REC_NAME_LEN], 0
 je .Ltf_next
 cmp dword ptr [r12 + TUI_LI_REC_LINE_LEN], 0
 je .Ltf_hidden
 call tt_line_flush
 mov r14d, 1
.Ltf_hidden:
 cmp dword ptr [r12 + TUI_LI_REC_STATE], 0
 jne .Ltf_next
 mov eax, dword ptr [r12 + TUI_LI_REC_OUTLINES]
 cmp eax, dword ptr [r12 + TUI_LI_REC_SHOWN]
 jbe .Ltf_next
 mov dword ptr [rbx + TUI_LI_TOOL_LAST_FIN], r13d
 cmp dword ptr [r12 + TUI_LI_REC_HINTED], 0
 jne .Ltf_next
 mov dword ptr [r12 + TUI_LI_REC_HINTED], 1
 lea rsi, [rip + .Lpending_expand]
 mov edx, OFFSET .Lpending_expand_l
 call tt_raw
 mov r14d, 1
.Ltf_next:
 inc r13d
 cmp r13d, TUI_LI_TOOL_SLOTS
 jb .Ltf_loop
 mov eax, r14d
 add rsp, 8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size tui_tool_flush,.-tui_tool_flush
