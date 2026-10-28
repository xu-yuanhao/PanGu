/* Word token classification / zero-edit comparison ported from jsdiff 8.0.4
 * https://github.com/kpdecker/jsdiff (src/diff/word.ts, base.ts).
 * BSD 3-Clause License
 *
 * Copyright (c) 2009-2015, Kevin Decker <kpdecker@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/* tui_diff.S — diff/patch 行级渲染（契约见 tui_diff.h；行分组与 1:1 行内
 * 反色对应 Pi diff.ts 的 run 收集与 renderIntraLineDiff，行内算法为
 * 公共前后缀近似）。
 *
 * 寄存器：rbx=sink、rbp=ansi、r12=text、r13=len、r14=i、r15=行起点偏移。
 * 连续组使用借用源区间扫描；实际 1:1 组借用两条完整源行供行内比较。
 * 主帧 72 字节，六个保存寄存器后保持 SysV 调用对齐。
 * d_render_pair/d_emit_line 全参数化，各自管理所有权和临时帧。
 */
#include "tui_layout.h"
.intel_syntax noprefix

.equ D_LINE,      0
.equ D_LSTART,    8
.equ D_GSTART,    16
.equ D_GRCNT,     24
.equ D_GACNT,     32
.equ D_GRLEN,     40
.equ D_GALEN,     48
.equ D_GASTART,   56
.equ D_FRAME,     72

/* Pair frame owns one allocation containing both expanded contents. Source
 * line/number slices remain borrowed until rendering and free are complete. */
.equ P_OLD,       0
.equ P_OLDLEN,    8
.equ P_NEW,       16
.equ P_NEWLEN,    24
.equ P_OLDNUM,    32
.equ P_OLDNUMLEN, 40
.equ P_NEWNUM,    48
.equ P_NEWNUMLEN, 56
.equ P_OLDCONT,   64
.equ P_NEWCONT,   72
.equ P_OLDCLEN,   80
.equ P_NEWCLEN,   88
.equ P_ALLOC,     96
.equ P_PFX,       104
.equ P_SFX,       112
.equ P_NEWPFX,    120
.equ P_FRAME,     136

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

/* ECMAScript WhiteSpace/LineTerminator, UTF-8 scalar at r10.
 * Returns ecx=byte width or 0; preserves input and parse state registers.
 * Invalid/truncated sequences never become framing. LF is already split. */
.type d_space_width,@function
d_space_width:
 xor ecx, ecx
 cmp r10, rsi
 jae .Lws_out
 movzx edx, byte ptr [rdi+r10]
 cmp edx, 32
 je .Lws_one
 cmp edx, 9
 jb .Lws_out
 cmp edx, 13
 jbe .Lws_one
 mov r11, rsi
 sub r11, r10
 cmp r11, 2
 jb .Lws_out
 cmp edx, 0xc2
 jne .Lws_three
 cmp byte ptr [rdi+r10+1], 0xa0
 jne .Lws_out
 mov ecx, 2
 ret
.Lws_three:
 cmp r11, 3
 jb .Lws_out
 cmp edx, 0xe1
 jne .Lws_e2
 cmp byte ptr [rdi+r10+1], 0x9a
 jne .Lws_out
 cmp byte ptr [rdi+r10+2], 0x80
 je .Lws_yes3
 ret
.Lws_e2:
 cmp edx, 0xe2
 jne .Lws_e3
 cmp byte ptr [rdi+r10+1], 0x81
 je .Lws_205f
 cmp byte ptr [rdi+r10+1], 0x80
 jne .Lws_out
 movzx edx, byte ptr [rdi+r10+2]
 cmp edx, 0x80
 jb .Lws_out
 cmp edx, 0x8a
 jbe .Lws_yes3
 cmp edx, 0xa8
 je .Lws_yes3
 cmp edx, 0xa9
 je .Lws_yes3
 cmp edx, 0xaf
 je .Lws_yes3
 ret
.Lws_205f:
 cmp byte ptr [rdi+r10+2], 0x9f
 je .Lws_yes3
 ret
.Lws_e3:
 cmp edx, 0xe3
 jne .Lws_bom
 cmp byte ptr [rdi+r10+1], 0x80
 jne .Lws_out
 cmp byte ptr [rdi+r10+2], 0x80
 je .Lws_yes3
 ret
.Lws_bom:
 cmp edx, 0xef
 jne .Lws_out
 cmp byte ptr [rdi+r10+1], 0xbb
 jne .Lws_out
 cmp byte ptr [rdi+r10+2], 0xbf
 jne .Lws_out
.Lws_yes3:
 mov ecx, 3
 ret
.Lws_one:
 mov ecx, 1
.Lws_out:
 ret
.size d_space_width,.-d_space_width

/* d_parse(line,len): eax=normalized prefix (0=no match), rdx=num offset,
 * rcx=num bytes, r8=content offset, r9=content bytes. Byte offsets frame
 * complete UTF-8 whitespace scalars; the source number field stays borrowed. */
.type d_parse,@function
d_parse:
 sub rsp, 24
 test rsi, rsi
 jz .Lp_fail
 movzx r9d, byte ptr [rdi]
 mov r10d, 1
 cmp r9d, '-'
 je .Lp_have
 cmp r9d, '+'
 je .Lp_have
 xor r10d, r10d
 call d_space_width
 test ecx, ecx
 jz .Lp_fail
 mov r10, rcx
 mov r9d, ' '
.Lp_have:
 mov [rsp], r10
.Lp_spaces:
 call d_space_width
 test ecx, ecx
 jz .Lp_digits_start
 mov [rsp+8], r10
 add r10, rcx
 jmp .Lp_spaces
.Lp_digits_start:
 mov [rsp+16], r10
 mov r8, r10
.Lp_digits:
 cmp r10, rsi
 jae .Lp_digits_end
 movzx ecx, byte ptr [rdi+r10]
 cmp ecx, '0'
 jb .Lp_digits_end
 cmp ecx, '9'
 ja .Lp_digits_end
 inc r10
 jmp .Lp_digits
.Lp_digits_end:
 cmp r10, r8
 je .Lp_no_digits
 call d_space_width
 test ecx, ecx
 jz .Lp_no_digits
 mov r8, r10
 add r10, rcx
 jmp .Lp_matched
.Lp_no_digits:
 /* Greedy whitespace* backtracks by its final scalar to satisfy separator. */
 mov r10, [rsp+16]
 cmp r10, [rsp]
 jbe .Lp_fail
 mov r8, [rsp+8]
.Lp_matched:
 /* JS dot without /s rejects line terminators in content, including the
  * Unicode separators that may legitimately appear in framing instead. */
 mov r11, r10
.Lp_content:
 cmp r11, rsi
 jae .Lp_content_ok
 movzx edx, byte ptr [rdi+r11]
 cmp edx, 10
 je .Lp_fail
 cmp edx, 13
 je .Lp_fail
 cmp edx, 0xe2
 jne .Lp_content_next
 mov rax, rsi
 sub rax, r11
 cmp rax, 3
 jb .Lp_content_next
 cmp byte ptr [rdi+r11+1], 0x80
 jne .Lp_content_next
 movzx edx, byte ptr [rdi+r11+2]
 cmp edx, 0xa8
 je .Lp_fail
 cmp edx, 0xa9
 je .Lp_fail
.Lp_content_next:
 inc r11
 jmp .Lp_content
.Lp_content_ok:
 mov rax, r9
 mov rdx, [rsp]
 mov rcx, r8
 sub rcx, rdx
 mov r8, r10
 mov r9, rsi
 sub r9, r10
 jmp .Lp_out
.Lp_fail:
 xor eax, eax
.Lp_out:
 add rsp, 24
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
 mov [rsp], r8 /* content offset, borrowed from the complete source line */
 mov r15, r9 /* full content length; no fixed output scratch limit */
 lea rsi, [rip + .Ld_sp]
 cmp eax, '-'
 jne .Lel_prefix_plus
 lea rsi, [rip + .Ld_dash]
 jmp .Lel_prefix_write
.Lel_prefix_plus:
 cmp eax, '+'
 jne .Lel_prefix_write
 lea rsi, [rip + .Ld_plus]
.Lel_prefix_write:
 mov edx, 1
 call d_safe
 mov rsi, r12
 add rsi, [rsp+8]
 mov rdx, [rsp+16]
 call d_safe
 lea rsi, [rip + .Ld_sp]
 mov edx, 1
 call d_safe
 mov rsi, r12
 add rsi, [rsp]
 mov rdx, r15
 call d_tabs_write
 jmp .Lel_tail
.Lel_raw:
 mov rsi, r12
 mov rdx, r13
 call d_tabs_write
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

/* Word classes from jsdiff 8.0.4 word.js extendedWordChars. Scalar tokens
 * outside these ranges are individual punctuation tokens (including CJK). */
.type d_word_kind,@function
d_word_kind:
 cmp eax, '_'
 je .Lwk_yes
 cmp eax, '0'
 jb .Lwk_no
 cmp eax, '9'
 jbe .Lwk_yes
 cmp eax, 'A'
 jb .Lwk_no
 cmp eax, 'Z'
 jbe .Lwk_yes
 cmp eax, 'a'
 jb .Lwk_no
 cmp eax, 'z'
 jbe .Lwk_yes
 cmp eax, 0xad
 je .Lwk_yes
 cmp eax, 0xc0
 jb .Lwk_no
 cmp eax, 0xd6
 jbe .Lwk_yes
 cmp eax, 0xd8
 jb .Lwk_no
 cmp eax, 0xf6
 jbe .Lwk_yes
 cmp eax, 0xf8
 jb .Lwk_no
 cmp eax, 0x2c6
 jbe .Lwk_yes
 cmp eax, 0x2c8
 jb .Lwk_no
 cmp eax, 0x2d7
 jbe .Lwk_yes
 cmp eax, 0x2de
 jb .Lwk_no
 cmp eax, 0x2ff
 jbe .Lwk_yes
 cmp eax, 0x1e00
 jb .Lwk_no
 cmp eax, 0x1eff
 jbe .Lwk_yes
.Lwk_no:
 xor eax, eax
 ret
.Lwk_yes:
 mov eax, 1
 ret
.size d_word_kind,.-d_word_kind

/* Core of one WordDiff token after trim: rdi=text,rsi=len,rdx=offset;
 * returns rax=start,rcx=core bytes,rdx=next offset. Invalid UTF-8 yields -1
 * in rdx so the caller preserves the existing safe-render fallback. */
.type d_next_word_core,@function
d_next_word_core:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 16
 mov r12, rdi
 mov r13, rsi
 mov r14, rdx
.Lwc_space:
 mov rdi, r12
 mov rsi, r13
 mov r10, r14
 call d_space_width
 test ecx, ecx
 jz .Lwc_start
 add r14, rcx
 jmp .Lwc_space
.Lwc_start:
 mov r15, r14
 mov ebx, -1
.Lwc_scalar:
 cmp r14, r13
 jae .Lwc_done
 lea rdi, [r12+r14]
 mov rsi, r13
 sub rsi, r14
 call tui_safe_seq_len@PLT
 test eax, eax
 jz .Lwc_bad
 mov [rsp], rax
 movzx eax, byte ptr [r12+r14]
 cmp qword ptr [rsp], 1
 je .Lwc_kind
 and eax, 31
 cmp qword ptr [rsp], 2
 je .Lwc_decode
 and eax, 15
 cmp qword ptr [rsp], 3
 je .Lwc_decode
 and eax, 7
.Lwc_decode:
 mov ecx, 1
.Lwc_cont:
 shl eax, 6
 lea rdx, [r14+rcx]
 movzx edx, byte ptr [r12+rdx]
 and edx, 63
 or eax, edx
 inc rcx
 cmp rcx, [rsp]
 jb .Lwc_cont
.Lwc_kind:
 call d_word_kind
 cmp ebx, -1
 jne .Lwc_continue
 mov ebx, eax
 add r14, [rsp]
 test ebx, ebx
 jz .Lwc_done
 jmp .Lwc_scalar
.Lwc_continue:
 test eax, eax
 jz .Lwc_done
 add r14, [rsp]
 jmp .Lwc_scalar
.Lwc_done:
 mov rax, r15
 mov rcx, r14
 sub rcx, r15
 mov rdx, r14
 jmp .Lwc_out
.Lwc_bad:
 mov rdx, -1
.Lwc_out:
 add rsp, 16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size d_next_word_core,.-d_next_word_core

/* Zero-edit WordDiff path: compare trimmed token cores in order, not a
 * whitespace-stripped string ("a b" must remain different from "ab"). */
.type d_words_equal,@function
d_words_equal:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 48
 mov r12, rdi
 mov r13, rsi
 mov r14, rdx
 mov r15, rcx
 mov qword ptr [rsp], 0
 mov qword ptr [rsp+8], 0
 test r13, r13
 jz .Lwe_empty
 test r15, r15
 jz .Lwe_no
.Lwe_loop:
 mov rdi, r12
 mov rsi, r13
 mov rdx, [rsp]
 call d_next_word_core
 cmp rdx, -1
 je .Lwe_no
 mov [rsp], rdx
 mov [rsp+16], rax
 mov [rsp+24], rcx
 mov rdi, r14
 mov rsi, r15
 mov rdx, [rsp+8]
 call d_next_word_core
 cmp rdx, -1
 je .Lwe_no
 mov [rsp+8], rdx
 cmp rcx, [rsp+24]
 jne .Lwe_no
 test rcx, rcx
 jz .Lwe_yes
 lea rsi, [r14+rax]
 mov rdi, [rsp+16]
 add rdi, r12
 mov rdx, rcx
 call memcmp@PLT
 test eax, eax
 jnz .Lwe_no
 jmp .Lwe_loop
.Lwe_empty:
 test r15, r15
 jnz .Lwe_no
.Lwe_yes:
 mov eax, 1
 jmp .Lwe_out
.Lwe_no:
 xor eax, eax
.Lwe_out:
 add rsp, 48
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size d_words_equal,.-d_words_equal

/* Complete replacement branch of the same token graph: if no core can
 * match, every token belongs to one removal/insertion run. No path matrix or
 * duplicate token storage is necessary. Invalid source leaves the fallback. */
.type d_words_disjoint,@function
d_words_disjoint:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 48
 mov r12, rdi
 mov r13, rsi
 mov r14, rdx
 mov r15, rcx
 mov qword ptr [rsp], 0
.Lwd_old:
 mov rdi, r12
 mov rsi, r13
 mov rdx, [rsp]
 call d_next_word_core
 cmp rdx, -1
 je .Lwd_no
 mov [rsp], rdx
 test rcx, rcx
 jz .Lwd_yes
 mov [rsp+16], rax
 mov [rsp+24], rcx
 mov qword ptr [rsp+8], 0
.Lwd_new:
 mov rdi, r14
 mov rsi, r15
 mov rdx, [rsp+8]
 call d_next_word_core
 cmp rdx, -1
 je .Lwd_no
 mov [rsp+8], rdx
 test rcx, rcx
 jz .Lwd_old
 cmp rcx, [rsp+24]
 jne .Lwd_new
 lea rsi, [r14+rax]
 mov rdi, [rsp+16]
 add rdi, r12
 mov rdx, rcx
 call memcmp@PLT
 test eax, eax
 jz .Lwd_no
 jmp .Lwd_new
.Lwd_yes:
 mov eax, 1
 jmp .Lwd_out
.Lwd_no:
 xor eax, eax
.Lwd_out:
 add rsp, 48
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size d_words_disjoint,.-d_words_disjoint

/* Move a byte comparison boundary to the containing token core edge.
 * rdi=text,rsi=len,rdx=boundary,r8d=0(start)/1(end); rax=adjusted boundary.
 * Exact edges and whitespace remain unchanged. No source slice is copied. */
.type d_word_boundary,@function
d_word_boundary:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 16
 mov r12, rdi
 mov r13, rsi
 mov r14, rdx
 mov ebx, r8d
 xor r15d, r15d
.Lwb_scan:
 mov rdi, r12
 mov rsi, r13
 mov rdx, r15
 call d_next_word_core
 cmp rdx, -1
 je .Lwb_original
 test rcx, rcx
 jz .Lwb_original
 cmp r14, rax
 jbe .Lwb_original
 lea r15, [rax+rcx]
 cmp r14, r15
 jae .Lwb_scan
 test ebx, ebx
 jz .Lwb_out
 mov rax, r15
 jmp .Lwb_out
.Lwb_original:
 mov rax, r14
.Lwb_out:
 add rsp, 16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size d_word_boundary,.-d_word_boundary

/* d_render_pair(old line,len,new line,len), sink/ansi in rbx/rbp.
 * No fixed content cap. Overflow/OOM preserve complete plain-colored lines. */
.type d_render_pair,@function
d_render_pair:
 push r12
 push r13
 push r14
 push r15
 sub rsp, P_FRAME
 mov [rsp+P_OLD], rdi
 mov [rsp+P_OLDLEN], rsi
 mov [rsp+P_NEW], rdx
 mov [rsp+P_NEWLEN], rcx
 call d_parse
 test eax, eax
 jz .Lfp_plain
 mov rdi, [rsp+P_OLD]
 add rdx, rdi
 mov [rsp+P_OLDNUM], rdx
 mov [rsp+P_OLDNUMLEN], rcx
 add rdi, r8
 mov [rsp+P_OLDCONT], rdi
 mov [rsp+P_OLDCLEN], r9
 mov rdi, [rsp+P_NEW]
 mov rsi, [rsp+P_NEWLEN]
 call d_parse
 test eax, eax
 jz .Lfp_plain
 mov rdi, [rsp+P_NEW]
 add rdx, rdi
 mov [rsp+P_NEWNUM], rdx
 mov [rsp+P_NEWNUMLEN], rcx
 add rdi, r8
 mov [rsp+P_NEWCONT], rdi
 mov [rsp+P_NEWCLEN], r9
 /* Three bytes per source byte suffice for TAB expansion. */
 mov rax, [rsp+P_OLDCLEN]
 add rax, r9
 jc .Lfp_plain
 mov ecx, 3
 mul rcx
 test rdx, rdx
 jnz .Lfp_plain
 mov rdi, rax
 test rdi, rdi
 jnz .Lfp_allocate
 inc rdi
.Lfp_allocate:
 call malloc@PLT
 test rax, rax
 jz .Lfp_plain
 mov [rsp+P_ALLOC], rax
 mov r12, rax
 mov rdi, [rsp+P_OLDCONT]
 mov rsi, [rsp+P_OLDCLEN]
 mov rdx, r12
 lea rcx, [rsi+rsi*2]
 call d_tabexp
 mov r14, rax
 lea r13, [r12+rax]
 mov rdi, [rsp+P_NEWCONT]
 mov rsi, [rsp+P_NEWCLEN]
 mov rdx, r13
 lea rcx, [rsi+rsi*2]
 call d_tabexp
 mov r15, rax
 mov rdi, r12
 mov rsi, r14
 mov rdx, r13
 mov rcx, r15
 call d_words_equal
 test eax, eax
 jz .Lfp_changed_tokens
 /* Like buildValues for keeps, both rendered lines use new token values. */
 mov r12, r13
 mov r14, r15
.Lfp_changed_tokens:
 mov rdi, r12
 mov rsi, r14
 mov rdx, r13
 mov rcx, r15
 call d_words_disjoint
 test eax, eax
 jz .Lfp_shared_tokens
 /* renderIntraLineDiff strips leading whitespace only from the first
  * removed/added part. The two indentation lengths are independent. */
 mov rdi, r12
 mov rsi, r14
 xor edx, edx
 call d_next_word_core
 cmp rdx, -1
 je .Lfp_shared_tokens
 mov [rsp+P_PFX], rax
 mov rdi, r13
 mov rsi, r15
 xor edx, edx
 call d_next_word_core
 cmp rdx, -1
 je .Lfp_shared_tokens
 mov [rsp+P_NEWPFX], rax
 mov qword ptr [rsp+P_SFX], 0
 jmp .Lfp_emit
.Lfp_shared_tokens:
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
	mov cl, byte ptr [r12+r8]
	cmp cl, byte ptr [r13+r8]
	jne .Lfp_pfxd
	inc r8
	jmp .Lfp_pfx
.Lfp_pfxd:
	mov r9, r8
	/* At the shorter end there is no next byte to inspect. */
	cmp r9, rax
	je .Lfp_pfxok
.Lfp_pfxb:
	test r9, r9
	jz .Lfp_pfxok
	mov cl, byte ptr [r12+r9]
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
	mov cl, byte ptr [r12+rcx]
	mov rdx, r15
	dec rdx
	sub rdx, r10
	cmp cl, byte ptr [r13+rdx]
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
	mov cl, byte ptr [r12+rcx]
	and cl, 0xC0
	cmp cl, 0x80
	jne .Lfp_sfxok
	dec r11
	jmp .Lfp_sfxb
.Lfp_sfxok:
	mov [rsp + P_PFX], r9
 mov [rsp+P_NEWPFX], r9
	mov [rsp + P_SFX], r11
 mov [rsp+P_OLDCONT], r11
 mov rdi, r12
 mov rsi, r14
 mov rdx, [rsp+P_PFX]
 xor r8d, r8d
 call d_word_boundary
 mov [rsp+P_PFX], rax
 mov rdi, r13
 mov rsi, r15
 mov rdx, [rsp+P_NEWPFX]
 xor r8d, r8d
 call d_word_boundary
 cmp rax, [rsp+P_PFX]
 jbe .Lfp_prefix_edge
 mov rax, [rsp+P_PFX]
.Lfp_prefix_edge:
 mov [rsp+P_PFX], rax
 mov [rsp+P_NEWPFX], rax
 mov rdi, r12
 mov rsi, r14
 mov rdx, r14
 sub rdx, [rsp+P_SFX]
 mov r8d, 1
 call d_word_boundary
 mov rcx, r14
 sub rcx, rax
 mov [rsp+P_SFX], rcx
 mov rdi, r13
 mov rsi, r15
 mov rdx, r15
 sub rdx, [rsp+P_OLDCONT]
 mov r8d, 1
 call d_word_boundary
 mov rcx, r15
 sub rcx, rax
 cmp rcx, [rsp+P_SFX]
 jbe .Lfp_suffix_edge
 mov rcx, [rsp+P_SFX]
.Lfp_suffix_edge:
 mov [rsp+P_SFX], rcx
.Lfp_emit:

	/* 删除行：-号 行号 头 + 反色中段 + 尾 */
	lea rsi, [rip + .Ld_rem]
	call d_sgr
	lea rsi, [rip + .Ld_dash]
	mov edx, 1
	call d_safe
	mov rsi, [rsp + P_OLDNUM]
	mov rdx, [rsp + P_OLDNUMLEN]
	call d_safe
	lea rsi, [rip + .Ld_sp]
	mov edx, 1
	call d_safe
	mov rsi, r12
	mov rdx, [rsp + P_PFX]
	call d_safe
 mov rax, r14
 sub rax, [rsp+P_PFX]
 sub rax, [rsp+P_SFX]
 jz .Lfp_old_unchanged
	lea rsi, [rip + .Ld_inv]
	call d_sgr
	mov rsi, r12
	add rsi, [rsp + P_PFX]
	mov rdx, r14
	sub rdx, [rsp + P_PFX]
	sub rdx, [rsp + P_SFX]
	call d_safe
	lea rsi, [rip + .Ld_invoff]
	call d_sgr
.Lfp_old_unchanged:
	mov rsi, r12
	add rsi, r14
	sub rsi, [rsp + P_SFX]
	mov rdx, [rsp + P_SFX]
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
	mov rsi, [rsp + P_NEWNUM]
	mov rdx, [rsp + P_NEWNUMLEN]
	call d_safe
	lea rsi, [rip + .Ld_sp]
	mov edx, 1
	call d_safe
	mov rsi, r13
	mov rdx, [rsp + P_NEWPFX]
	call d_safe
 mov rax, r15
 sub rax, [rsp+P_NEWPFX]
 sub rax, [rsp+P_SFX]
 jz .Lfp_new_unchanged
	lea rsi, [rip + .Ld_inv]
	call d_sgr
	mov rsi, r13
	add rsi, [rsp + P_NEWPFX]
	mov rdx, r15
	sub rdx, [rsp + P_NEWPFX]
	sub rdx, [rsp + P_SFX]
	call d_safe
	lea rsi, [rip + .Ld_invoff]
	call d_sgr
.Lfp_new_unchanged:
	mov rsi, r13
	add rsi, r15
	sub rsi, [rsp + P_SFX]
	mov rdx, [rsp + P_SFX]
	call d_safe
	lea rsi, [rip + .Ld_off]
	call d_sgr
	lea rsi, [rip + .Ld_nl]
	mov edx, 1
	call d_safe
 mov rdi, [rsp+P_ALLOC]
 call free@PLT
 jmp .Lfp_out
.Lfp_plain:
 mov rdi, [rsp+P_OLD]
 mov rsi, [rsp+P_OLDLEN]
 mov edx, '-'
 mov ecx, 1
 xor r8d, r8d
 call d_emit_line
 mov rdi, [rsp+P_NEW]
 mov rsi, [rsp+P_NEWLEN]
 mov edx, '+'
 mov ecx, 2
 xor r8d, r8d
 call d_emit_line
.Lfp_out:
 add rsp, P_FRAME
 pop r15
 pop r14
 pop r13
 pop r12
 ret
.size d_render_pair,.-d_render_pair

/* ---- void tui_diff_render(sink, ansi, text, len) ----
 * rdi=sink esi=ansi rdx=text rcx=len */
/* d_line_at(rdx=source offset, r12=text, r13=len): rax=line bytes,
 * rcx=next offset. Borrowed spans stay valid for the whole render call. */
.type d_line_at,@function
d_line_at:
 mov rcx, rdx
.Lla_scan:
 cmp rcx, r13
 jae .Lla_end
 cmp byte ptr [r12+rcx], 10
 je .Lla_newline
 inc rcx
 jmp .Lla_scan
.Lla_newline:
 mov rax, rcx
 sub rax, rdx
 inc rcx
 ret
.Lla_end:
 mov rax, rcx
 sub rax, rdx
 ret
.size d_line_at,.-d_line_at

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
 xor r14d, r14d
.Ldl_line:
 cmp r14, r13
 jae .Ldl_out
 mov r15, r14
 mov rdx, r14
 call d_line_at
 mov [rsp+D_LINE], rax
 mov [rsp+D_LSTART], rcx
 lea rdi, [r12+r15]
 mov rsi, rax
 call d_parse
 cmp eax, '-'
 je .Ldl_group
 /* Context and standalone additions have no pending group. */
 xor ecx, ecx
 cmp eax, '+'
 jne .Ldl_direct
 mov ecx, 2
.Ldl_direct:
 mov edx, eax
 lea rdi, [r12+r15]
 mov rsi, [rsp+D_LINE]
 xor r8d, r8d
 call d_emit_line
 mov r14, [rsp+D_LSTART]
 jmp .Ldl_line
.Ldl_group:
 mov [rsp+D_GSTART], r15
 mov qword ptr [rsp+D_GRCNT], 1
 mov qword ptr [rsp+D_GACNT], 0
 mov rax, [rsp+D_LINE]
 mov [rsp+D_GRLEN], rax
 mov r14, [rsp+D_LSTART]
 /* Count the entire consecutive removal run before deciding 1:1 status.
  * No row array or artificial run boundary is needed for large source runs. */
.Ldl_scan_removed:
 cmp r14, r13
 jae .Ldl_group_ready
 mov rdx, r14
 call d_line_at
 mov [rsp+D_LSTART], rcx
 lea rdi, [r12+r14]
 mov rsi, rax
 call d_parse
 cmp eax, '-'
 jne .Ldl_scan_added
 inc qword ptr [rsp+D_GRCNT]
 mov r14, [rsp+D_LSTART]
 jmp .Ldl_scan_removed
.Ldl_scan_added:
 cmp r14, r13
 jae .Ldl_group_ready
 mov rdx, r14
 call d_line_at
 mov [rsp+D_LINE], rax
 mov [rsp+D_LSTART], rcx
 lea rdi, [r12+r14]
 mov rsi, rax
 call d_parse
 cmp eax, '+'
 jne .Ldl_group_ready
 cmp qword ptr [rsp+D_GACNT], 0
 jne .Ldl_added_count
 mov [rsp+D_GASTART], r14
 mov rax, [rsp+D_LINE]
 mov [rsp+D_GALEN], rax
.Ldl_added_count:
 inc qword ptr [rsp+D_GACNT]
 mov r14, [rsp+D_LSTART]
 jmp .Ldl_scan_added
.Ldl_group_ready:
 cmp qword ptr [rsp+D_GRCNT], 1
 jne .Ldl_group_stream
 cmp qword ptr [rsp+D_GACNT], 1
 jne .Ldl_group_stream
 mov rdi, [rsp+D_GSTART]
 add rdi, r12
 mov rsi, [rsp+D_GRLEN]
 mov rdx, [rsp+D_GASTART]
 add rdx, r12
 mov rcx, [rsp+D_GALEN]
 call d_render_pair
 jmp .Ldl_line
.Ldl_group_stream:
 mov r15, [rsp+D_GSTART]
.Ldl_group_emit:
 cmp r15, r14
 jae .Ldl_line
 mov rdx, r15
 call d_line_at
 mov [rsp+D_LINE], rax
 mov [rsp+D_LSTART], rcx
 mov ecx, 1
 movzx edx, byte ptr [r12+r15]
 cmp edx, '-'
 je .Ldl_group_color
 mov ecx, 2
.Ldl_group_color:
 lea rdi, [r12+r15]
 mov rsi, [rsp+D_LINE]
 xor r8d, r8d
 call d_emit_line
 mov r15, [rsp+D_LSTART]
 jmp .Ldl_group_emit
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

.section .rodata
.Ld_three_spaces: .ascii "   "
.section .text
/* Stream arbitrary-length TAB-expanded content, escaping each borrowed span.
 * A tab cannot occur within a UTF-8 sequence, so span boundaries are scalar-safe. */
.type d_tabs_write,@function
d_tabs_write:
 push r12
 push r13
 push r14
 mov r12, rsi
 mov r13, rdx
 xor r14d, r14d
.Ldtw_loop:
 cmp r14, r13
 jae .Ldtw_tail
 cmp byte ptr [r12 + r14], 9
 jne .Ldtw_next
 mov rsi, r12
 mov rdx, r14
 call d_safe
 lea rsi, [rip + .Ld_three_spaces]
 mov edx, 3
 call d_safe
 lea rax, [r14 + 1]
 add r12, rax
 sub r13, rax
 xor r14d, r14d
 jmp .Ldtw_loop
.Ldtw_next:
 inc r14
 jmp .Ldtw_loop
.Ldtw_tail:
 mov rsi, r12
 mov rdx, r13
 call d_safe
 pop r14
 pop r13
 pop r12
 ret
.size d_tabs_write,.-d_tabs_write
