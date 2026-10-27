/* tui_diff.S — diff/patch 行级渲染（契约见 tui_diff.h；行分组与 1:1 行内
 * 反色对应 Pi diff.ts 的 run 收集与 renderIntraLineDiff，行内算法为
 * 公共前后缀近似）。
 *
 * 寄存器：rbx=sink、rbp=ansi、r12=text、r13=len、r14=i、r15=行起点偏移。
 * 栈帧（主帧 sub rsp,D_FRAME）：R/A 两组运行缓冲（16 行 × 256）+ 计数 +
 * TAB 展开临时缓冲 + 行暂存 + 暂存区。总调整 40+D_FRAME ≡ 8 (mod 16)。
 * 内部子程序 d_flush_pair 进入后主帧槽 X 位于 [rsp + 8 + X]（其自身
 * 再压 4 寄存器 + 8 字节后偏移为 48）；d_emit_line 全参数化不访主帧。
 */
#include "tui_layout.h"
.intel_syntax noprefix

.equ D_RL,     0        /* R 行长表（16 dword） */
.equ D_RLINES, 64       /* R 行内容（16*256） */
.equ D_AL,     4160
.equ D_ALINES, 4224
.equ D_RCNT,   8320     /* dword */
.equ D_ACNT,   8324     /* dword */
.equ D_LINE,   8328     /* 当前行长（dword） */
.equ D_LSTART, 8336     /* 当前行起点偏移（qword） */
.equ D_TMP1,   8344     /* 768 */
.equ D_TMP2,   9112     /* 768 */
.equ D_SCR,    9880     /* 暂存区 */
.equ D_FRAME,  9944
.equ D_RUN_MAX, 16
.equ D_LINE_KEEP, 255 /* TUI_DIFF_LINE_MAX-1（C 宏带 u 后缀，汇编不可用） */

.section .rodata
.Ld_nl:     .asciz "\n"
.Ld_sp:     .asciz " "
.Ld_dash:   .asciz "-"
.Ld_plus:   .asciz "+"
.Ld_ctx:    .asciz "\x1b[2m"
.Ld_rem:    .asciz "\x1b[31m"
.Ld_add:    .asciz "\x1b[32m"
.Ld_inv:    .asciz "\x1b[7m"
.Ld_invoff: .asciz "\x1b[27m"
.Ld_off:    .asciz "\x1b[0m"

.section .text

/* ---- 内部：d_safe(rbx=sink, rbp=ansi, rsi=data, rdx=len) ---- */
.type d_safe,@function
d_safe:
 mov ecx, edx
 mov rdx, rsi
 mov esi, ebp
 mov rdi, rbx
 jmp tui_safe_write@PLT
.size d_safe,.-d_safe

/* ---- 内部：d_raw(rbx=sink, rbp=ansi, rsi=data, rdx=len) ---- */
.type d_raw,@function
d_raw:
 mov ecx, edx
 mov rdx, rsi
 mov esi, ebp
 mov rdi, rbx
 jmp tui_raw_write@PLT
.size d_raw,.-d_raw

/* ---- 内部：d_sgr(rbx=sink, rbp=ansi, rsi=asciz)：SGR 固定串 ---- */
.type d_sgr,@function
d_sgr:
 test ebp, ebp
 jz .Ld_sgr_done
 push rsi
 mov rdi, rsi
 call strlen@PLT
 pop rsi
 mov rdx, rax
 jmp d_raw
.Ld_sgr_done:
 ret
.size d_sgr,.-d_sgr

/* ---- 内部：d_tabexp(rdi=src, rsi=len, rdx=dst, rcx=cap) → rax=写出字节 ---- */
.type d_tabexp,@function
d_tabexp:
 xor r8d, r8d   /* i */
 xor r9d, r9d   /* o */
.Ltx_loop:
 cmp r8, rsi
 jae .Ltx_done
 movzx eax, byte ptr [rdi + r8]
 inc r8
 cmp eax, 9
 jne .Ltx_plain
 lea r10, [r9 + 3]
 cmp r10, rcx
 ja .Ltx_done /* 放不下 3 空格即止（截断） */
 mov byte ptr [rdx + r9], ' '
 mov byte ptr [rdx + r9 + 1], ' '
 mov byte ptr [rdx + r9 + 2], ' '
 add r9, 3
 jmp .Ltx_loop
.Ltx_plain:
 cmp r9, rcx
 jae .Ltx_done
 mov [rdx + r9], al
 inc r9
 jmp .Ltx_loop
.Ltx_done:
 mov rax, r9
 ret
.size d_tabexp,.-d_tabexp

/* ---- 内部：d_parse(rdi=line, rsi=len) → rax=前缀字节(0=不匹配)、
 * rdx=行号偏移、rcx=行号长、r8=内容偏移、r9=内容长。
 * 仅使用 rax/rcx/rdx/r8-r10。 ---- */
.type d_parse,@function
d_parse:
 xor eax, eax
 test rsi, rsi
 jz .Lp_out
 movzx r9d, byte ptr [rdi]
 cmp r9d, '-'
 je .Lp_have
 cmp r9d, '+'
 je .Lp_have
 cmp r9d, ' '
 je .Lp_have
 jmp .Lp_out
.Lp_have:
 mov r10, 1 /* p */
.Lp_scan:
 cmp r10, rsi
 jae .Lp_scanend
 movzx ecx, byte ptr [rdi + r10]
 cmp ecx, ' '
 je .Lp_adv
 cmp ecx, '0'
 jb .Lp_scanend
 cmp ecx, '9'
 ja .Lp_scanend
.Lp_adv:
 inc r10
 jmp .Lp_scan
.Lp_scanend:
 cmp r10, 1
 jbe .Lp_out
 cmp byte ptr [rdi + r10 - 1], ' '
 jne .Lp_out /* 回溯一格后必须停在空格上 */
 mov rax, r9          /* 前缀 */
 mov rdx, 1           /* num_off */
 lea rcx, [r10 - 2]   /* num_len = p-2（可为 0） */
 mov r8, r10          /* cont_off */
 mov r9, rsi
 sub r9, r10          /* cont_len */
.Lp_out:
 ret
.size d_parse,.-d_parse

/* ---- 内部：d_emit_line(rdi=line, esi=len, edx=前缀字节(0=整行原样),
 * ecx=颜色(0 暗 1 红 2 绿), r8=TAB 展开缓冲[768]) ----
 * 解析行后按 "前缀 行号 内容" 重建输出；不匹配时整行原样；SGR 前后包夹。 */
.type d_emit_line,@function
d_emit_line:
 push r12
 push r13
 push r14
 push r15
 sub rsp, 24
 mov [rsp], r8        /* tmp */
 mov r12, rdi
 mov r13d, esi
 mov r14d, edx
 mov r15d, ecx
 cmp r15d, 1
 jne .Lel_c1
 lea rsi, [rip + .Ld_rem]
 jmp .Lel_sgr
.Lel_c1:
 cmp r15d, 2
 jne .Lel_c2
 lea rsi, [rip + .Ld_add]
 jmp .Lel_sgr
.Lel_c2:
 lea rsi, [rip + .Ld_ctx]
.Lel_sgr:
 call d_sgr
 test r14d, r14d
 jz .Lel_raw
 mov rdi, r12
 mov rsi, r13
 call d_parse
 test eax, eax
 jz .Lel_raw
 mov [rsp+8], rdx     /* num_off */
 mov [rsp+16], rcx    /* num_len */
 lea rdi, [r12 + r8]
 mov rsi, r9
 mov rdx, [rsp]
 mov ecx, 768
 call d_tabexp
 mov r15d, eax        /* 内容展开后长度 */
 mov rsi, r12         /* 前缀字节 = 行首字节 */
 mov edx, 1
 call d_safe
 mov rsi, r12
 add rsi, [rsp+8]
 mov rdx, [rsp+16]
 call d_safe
 lea rsi, [rip + .Ld_sp]
 mov edx, 1
 call d_safe
 mov rsi, [rsp]
 mov rdx, r15
 call d_safe
 jmp .Lel_tail
.Lel_raw:
 mov rsi, r12
 mov rdx, r13
 call d_safe
.Lel_tail:
 lea rsi, [rip + .Ld_off]
 call d_sgr
 lea rsi, [rip + .Ld_nl]
 mov edx, 1
 call d_safe
 add rsp, 24
 pop r15
 pop r14
 pop r13
 pop r12
 ret
.size d_emit_line,.-d_emit_line

/* ---- 内部：d_flush_pair —— 收束挂起的删除/新增组。
 * 进入时 rsp = 主帧基址 - 8，主帧槽 X = [rsp + 8 + X]；
 * 自压 4 寄存器 + 8 字节后主帧槽 X = [rsp + 48 + X]。 ---- */
.type d_flush_pair,@function
d_flush_pair:
 push r12
 push r13
 push r14
 push r15
 sub rsp, 8
 mov r12d, dword ptr [rsp + 48 + D_RCNT]
 mov r13d, dword ptr [rsp + 48 + D_ACNT]
 test r12d, r12d
 jnz .Lfp_have
 test r13d, r13d
 jnz .Lfp_have
 jmp .Lfp_out
.Lfp_have:
 cmp r12d, 1
 jne .Lfp_plain
 cmp r13d, 1
 jne .Lfp_plain
 jmp .Lfp_intra
.Lfp_plain:
	xor r14d, r14d
.Lfp_rloop:
	cmp r14d, r12d
	jae .Lfp_rdone
	imul rax, r14, 256
	lea rdi, [rsp + rax + 48 + D_RLINES]
	mov esi, dword ptr [rsp + r14*4 + 48 + D_RL]
	mov edx, '-'         /* 前缀 */
	mov ecx, 1           /* 红 */
	lea r8, [rsp + 48 + D_TMP1]
	call d_emit_line
	inc r14d
	jmp .Lfp_rloop
.Lfp_rdone:
	xor r14d, r14d
.Lfp_aloop:
	cmp r14d, r13d
	jae .Lfp_adone
	imul rax, r14, 256
	lea rdi, [rsp + rax + 48 + D_ALINES]
	mov esi, dword ptr [rsp + r14*4 + 48 + D_AL]
	mov edx, '+'         /* 前缀 */
	mov ecx, 2           /* 绿 */
	lea r8, [rsp + 48 + D_TMP1]
	call d_emit_line
	inc r14d
	jmp .Lfp_aloop
.Lfp_adone:
	jmp .Lfp_reset
/* ---- 1:1 行内反色（公共前后缀近似 diffWords） ---- */
.Lfp_intra:
	xor eax, eax
	lea rdi, [rsp + 48 + D_RLINES]
	mov esi, dword ptr [rsp + 48 + D_RL]
	call d_parse
	test eax, eax
	jz .Lfp_plain
	mov [rsp + 48 + D_SCR], rdx        /* R num_off */
	mov [rsp + 48 + D_SCR + 8], rcx    /* R num_len */
	lea rdi, [rsp + 48 + D_RLINES]
	add rdi, r8
	mov rsi, r9
	lea rdx, [rsp + 48 + D_TMP1]
	mov ecx, 768
	call d_tabexp
	mov r14, rax         /* L1 */
	lea rdi, [rsp + 48 + D_ALINES]
	mov esi, dword ptr [rsp + 48 + D_AL]
	call d_parse
	test eax, eax
	jz .Lfp_plain
	mov [rsp + 48 + D_SCR + 16], rdx   /* A num_off */
	mov [rsp + 48 + D_SCR + 24], rcx   /* A num_len */
	lea rdi, [rsp + 48 + D_ALINES]
	add rdi, r8
	mov rsi, r9
	lea rdx, [rsp + 48 + D_TMP2]
	mov ecx, 768
	call d_tabexp
	mov r15, rax         /* L2 */
	/* 公共前缀 P → r9 */
	mov rax, r14
	cmp rax, r15
	jbe .Lfp_pfxmin
	mov rax, r15
.Lfp_pfxmin:
	xor r8d, r8d
.Lfp_pfx:
	cmp r8, rax
	jae .Lfp_pfxd
	mov cl, byte ptr [rsp + 48 + D_TMP1 + r8]
	cmp cl, byte ptr [rsp + 48 + D_TMP2 + r8]
	jne .Lfp_pfxd
	inc r8
	jmp .Lfp_pfx
.Lfp_pfxd:
	mov r9, r8
.Lfp_pfxb:
	test r9, r9
	jz .Lfp_pfxok
	mov cl, byte ptr [rsp + 48 + D_TMP1 + r9]
	and cl, 0xC0
	cmp cl, 0x80
	jne .Lfp_pfxok
	dec r9
	jmp .Lfp_pfxb
.Lfp_pfxok:
	/* 公共后缀 S → r11，上限 min - P */
	mov rax, r14
	cmp rax, r15
	jbe .Lfp_sfxmin
	mov rax, r15
.Lfp_sfxmin:
	sub rax, r9
	xor r10d, r10d
.Lfp_sfx:
	cmp r10, rax
	jae .Lfp_sfxd
	mov rcx, r14
	dec rcx
	sub rcx, r10
	mov cl, byte ptr [rsp + 48 + D_TMP1 + rcx]
	mov rdx, r15
	dec rdx
	sub rdx, r10
	cmp cl, byte ptr [rsp + 48 + D_TMP2 + rdx]
	jne .Lfp_sfxd
	inc r10
	jmp .Lfp_sfx
.Lfp_sfxd:
	mov r11, r10
.Lfp_sfxb:
	test r11, r11
	jz .Lfp_sfxok
	mov rcx, r14
	sub rcx, r11
	mov cl, byte ptr [rsp + 48 + D_TMP1 + rcx]
	and cl, 0xC0
	cmp cl, 0x80
	jne .Lfp_sfxok
	dec r11
	jmp .Lfp_sfxb
.Lfp_sfxok:
	mov [rsp + 48 + D_SCR + 32], r9
	mov [rsp + 48 + D_SCR + 40], r11
	/* 删除行：-号 行号 头 + 反色中段 + 尾 */
	lea rsi, [rip + .Ld_rem]
	call d_sgr
	lea rsi, [rip + .Ld_dash]
	mov edx, 1
	call d_safe
	mov rsi, [rsp + 48 + D_SCR]
	add rsi, rsp
	add rsi, 48 + D_RLINES
	mov rdx, [rsp + 48 + D_SCR + 8]
	call d_safe
	lea rsi, [rip + .Ld_sp]
	mov edx, 1
	call d_safe
	lea rsi, [rsp + 48 + D_TMP1]
	mov rdx, [rsp + 48 + D_SCR + 32]
	call d_safe
	lea rsi, [rip + .Ld_inv]
	call d_sgr
	lea rsi, [rsp + 48 + D_TMP1]
	add rsi, [rsp + 48 + D_SCR + 32]
	mov rdx, r14
	sub rdx, [rsp + 48 + D_SCR + 32]
	sub rdx, [rsp + 48 + D_SCR + 40]
	call d_safe
	lea rsi, [rip + .Ld_invoff]
	call d_sgr
	lea rsi, [rsp + 48 + D_TMP1]
	add rsi, r14
	sub rsi, [rsp + 48 + D_SCR + 40]
	mov rdx, [rsp + 48 + D_SCR + 40]
	call d_safe
	lea rsi, [rip + .Ld_off]
	call d_sgr
	lea rsi, [rip + .Ld_nl]
	mov edx, 1
	call d_safe
	/* 加行：+号 行号 头 + 反色中段 + 尾 */
	lea rsi, [rip + .Ld_add]
	call d_sgr
	lea rsi, [rip + .Ld_plus]
	mov edx, 1
	call d_safe
	mov rsi, [rsp + 48 + D_SCR + 16]
	add rsi, rsp
	add rsi, 48 + D_ALINES
	mov rdx, [rsp + 48 + D_SCR + 24]
	call d_safe
	lea rsi, [rip + .Ld_sp]
	mov edx, 1
	call d_safe
	lea rsi, [rsp + 48 + D_TMP2]
	mov rdx, [rsp + 48 + D_SCR + 32]
	call d_safe
	lea rsi, [rip + .Ld_inv]
	call d_sgr
	lea rsi, [rsp + 48 + D_TMP2]
	add rsi, [rsp + 48 + D_SCR + 32]
	mov rdx, r15
	sub rdx, [rsp + 48 + D_SCR + 32]
	sub rdx, [rsp + 48 + D_SCR + 40]
	call d_safe
	lea rsi, [rip + .Ld_invoff]
	call d_sgr
	lea rsi, [rsp + 48 + D_TMP2]
	add rsi, r15
	sub rsi, [rsp + 48 + D_SCR + 40]
	mov rdx, [rsp + 48 + D_SCR + 40]
	call d_safe
	lea rsi, [rip + .Ld_off]
	call d_sgr
	lea rsi, [rip + .Ld_nl]
	mov edx, 1
	call d_safe
.Lfp_reset:
	mov dword ptr [rsp + 48 + D_RCNT], 0
	mov dword ptr [rsp + 48 + D_ACNT], 0
.Lfp_out:
	add rsp, 8
	pop r15
	pop r14
	pop r13
	pop r12
	ret
.size d_flush_pair,.-d_flush_pair

/* ---- void tui_diff_render(sink, ansi, text, len) ----
 * rdi=sink esi=ansi rdx=text rcx=len */
.globl tui_diff_render
.type tui_diff_render,@function
tui_diff_render:
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
	sub rsp, D_FRAME
	mov rbx, rdi
	mov ebp, esi
	mov r12, rdx
	mov r13, rcx
	mov dword ptr [rsp + D_RCNT], 0
	mov dword ptr [rsp + D_ACNT], 0
	xor r14d, r14d /* i */
.Ldl_line:
	cmp r14, r13
	jae .Ldl_end
	mov r15, r14 /* 行起点偏移 */
	mov rcx, r14
.Ldl_findnl:
	cmp rcx, r13
	jae .Ldl_nonl
	cmp byte ptr [r12 + rcx], 10
	je .Ldl_havenl
	inc rcx
	jmp .Ldl_findnl
.Ldl_havenl:
	mov rax, rcx
	sub rax, r14
	lea r14, [rcx + 1]
	jmp .Ldl_got
.Ldl_nonl:
	mov rax, r13
	sub rax, r14
	mov r14, r13
.Ldl_got:
	mov dword ptr [rsp + D_LINE], eax
	mov [rsp + D_LSTART], r15
	lea rdi, [r12 + r15]
	mov rsi, rax
	call d_parse
	test eax, eax
	jz .Ldl_rawctx
	cmp eax, '-'
	je .Ldl_removed
	cmp eax, '+'
	je .Ldl_added
	/* 上下文行 */
	cmp dword ptr [rsp + D_RCNT], 0
	ja .Ldl_ctxflush
	cmp dword ptr [rsp + D_ACNT], 0
	ja .Ldl_ctxflush
	jmp .Ldl_ctxout
.Ldl_ctxflush:
	call d_flush_pair
.Ldl_ctxout:
	lea rdi, [r12 + r15]
	mov esi, dword ptr [rsp + D_LINE]
	mov edx, ' '
	xor ecx, ecx
	lea r8, [rsp + D_TMP1]
	call d_emit_line
	jmp .Ldl_line
.Ldl_rawctx:
	cmp dword ptr [rsp + D_RCNT], 0
	ja .Ldl_rawflush
	cmp dword ptr [rsp + D_ACNT], 0
	ja .Ldl_rawflush
	jmp .Ldl_rawout
.Ldl_rawflush:
	call d_flush_pair
.Ldl_rawout:
	lea rdi, [r12 + r15]
	mov esi, dword ptr [rsp + D_LINE]
	xor edx, edx
	xor ecx, ecx
	lea r8, [rsp + D_TMP1]
	call d_emit_line
	jmp .Ldl_line
.Ldl_removed:
	cmp dword ptr [rsp + D_ACNT], 0
	ja .Ldl_remflush
	cmp dword ptr [rsp + D_RCNT], D_RUN_MAX
	jae .Ldl_remflush
	jmp .Ldl_remstore
.Ldl_remflush:
	call d_flush_pair
	jmp .Ldl_removed
.Ldl_remstore:
	lea rdi, [r12 + r15]
	mov esi, dword ptr [rsp + D_LINE]
	mov edx, D_LINE_KEEP
	call tui_utf8_truncate_len@PLT
	mov [rsp + D_SCR], rax
	mov ecx, dword ptr [rsp + D_RCNT]
	mov dword ptr [rsp + D_RL + rcx*4], eax
	imul rdi, rcx, 256
	add rdi, rsp
	add rdi, D_RLINES
	lea rsi, [r12 + r15]
	mov rdx, [rsp + D_SCR]
	call memcpy@PLT
	inc dword ptr [rsp + D_RCNT]
	jmp .Ldl_line
.Ldl_added:
	cmp dword ptr [rsp + D_RCNT], 0
	je .Ldl_addsolo
	cmp dword ptr [rsp + D_ACNT], D_RUN_MAX
	jae .Ldl_addflush
	jmp .Ldl_addstore
.Ldl_addflush:
	call d_flush_pair
	jmp .Ldl_added
.Ldl_addsolo:
	lea rdi, [r12 + r15]
	mov esi, dword ptr [rsp + D_LINE]
	mov edx, '+'
	mov ecx, 2
	lea r8, [rsp + D_TMP1]
	call d_emit_line
	jmp .Ldl_line
.Ldl_addstore:
	lea rdi, [r12 + r15]
	mov esi, dword ptr [rsp + D_LINE]
	mov edx, D_LINE_KEEP
	call tui_utf8_truncate_len@PLT
	mov [rsp + D_SCR], rax
	mov ecx, dword ptr [rsp + D_ACNT]
	mov dword ptr [rsp + D_AL + rcx*4], eax
	imul rdi, rcx, 256
	add rdi, rsp
	add rdi, D_ALINES
	lea rsi, [r12 + r15]
	mov rdx, [rsp + D_SCR]
	call memcpy@PLT
	inc dword ptr [rsp + D_ACNT]
	jmp .Ldl_line
.Ldl_end:
	cmp dword ptr [rsp + D_RCNT], 0
	ja .Ldl_endflush
	cmp dword ptr [rsp + D_ACNT], 0
	ja .Ldl_endflush
	jmp .Ldl_out
.Ldl_endflush:
	call d_flush_pair
.Ldl_out:
	add rsp, D_FRAME
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
	ret
.size tui_diff_render,.-tui_diff_render
