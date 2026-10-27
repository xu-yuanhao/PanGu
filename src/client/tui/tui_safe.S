/* tui_safe.S — 安全输出原语（自 tui_view.S 通道提取共享；行为与原实现逐字节一致）。
 * rbx=sink 贯穿两个例程；sp_put 走 sink.write，段写出经 tui_raw_write（含换行翻译）。
 */
#include "tui_layout.h"
.intel_syntax noprefix

.section .rodata
.Lcrlf: .asciz "\r\n"
.Lhex:  .ascii "0123456789abcdef"

.section .text

/* ---- 内部：sp_put(rbx=sink, rsi=data, rdx=len) ---- */
.type sp_put,@function
sp_put:
 mov rax, qword ptr [rbx + TUI_LI_SINK_WRITE]
 mov rdi, qword ptr [rbx + TUI_LI_SINK_CTX]
 jmp rax
.size sp_put,.-sp_put

/* ---- void tui_raw_write(const tui_out_sink *sink, int ansi, const char *data, size_t len)
 * rdi=sink esi=ansi rdx=data rcx=len。ansi=1 时 '\n' → "\r\n"。 ---- */
.globl tui_raw_write
.type tui_raw_write,@function
tui_raw_write:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 16
 mov rbx, rdi
 mov dword ptr [rsp], esi /* ansi */
 mov r12, rdx
 mov r13, rcx
 test r13, r13
 jz .Lraw_done
 cmp dword ptr [rsp], 0
 je .Lraw_plain
 xor r14d, r14d /* i */
 xor r15d, r15d /* off */
.Lraw_scan:
 cmp r14, r13
 jae .Lraw_tail
 cmp byte ptr [r12 + r14], 10 /* '\n' */
 jne .Lraw_next
 cmp r14, r15
 jbe .Lraw_no_pre
 lea rsi, [r12 + r15]
 mov rdx, r14
 sub rdx, r15
 call sp_put
.Lraw_no_pre:
 lea rsi, [rip + .Lcrlf]
 mov edx, 2
 call sp_put
 lea r15, [r14 + 1]
.Lraw_next:
 inc r14
 jmp .Lraw_scan
.Lraw_tail:
 cmp r13, r15
 jbe .Lraw_fin
 lea rsi, [r12 + r15]
 mov rdx, r13
 sub rdx, r15
 call sp_put
.Lraw_fin:
 add rsp, 16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Lraw_plain:
 mov rax, qword ptr [rbx + TUI_LI_SINK_WRITE]
 mov rdi, qword ptr [rbx + TUI_LI_SINK_CTX]
 mov rsi, r12
 mov rdx, r13
 call rax
.Lraw_done:
 add rsp, 16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size tui_raw_write,.-tui_raw_write

/* ---- void tui_safe_write(const tui_out_sink *sink, int ansi, const char *data, size_t len)
 * 栈槽（sub rsp,32）：[0]=ansi [8]=标记字节长度 [12]=转义字节值 [16..]=标记缓冲。
 * 非法字节 → "\xHH"；U+0080–U+009F 完整两字节序列 → "\u00XX"；其余按
 * tui_utf8_seq_len + 连续字节/上界校验后原样放行。 ---- */
.globl tui_safe_write
.type tui_safe_write,@function
tui_safe_write:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 32
 mov rbx, rdi
 mov dword ptr [rsp], esi /* ansi */
 mov r12, rdx
 mov r13, rcx
 xor r14d, r14d
 xor r15d, r15d
.Lsafe_scan:
 cmp r14, r13
 jae .Lsafe_tail
 movzx eax, byte ptr [r12 + r14]
 cmp eax, 32
 jae .Lsafe_non_c0
 cmp eax, 9 /* '\t' */
 je .Lsafe_next
 cmp eax, 10 /* '\n' */
 je .Lsafe_next
 jmp .Lsafe_byte
.Lsafe_non_c0:
 cmp eax, 127 /* DEL */
 je .Lsafe_byte
 cmp eax, 128
 jb .Lsafe_next
 cmp eax, 160 /* C1 */
 jb .Lsafe_byte
 cmp eax, 194 /* 160..193 非法首字节 */
 jb .Lsafe_byte
 cmp eax, 244
 ja .Lsafe_byte
 cmp eax, 194
 jne .Lsafe_utf8
 lea rcx, [r14 + 1]
 cmp rcx, r13
 jae .Lsafe_byte /* C2 后缺字节 */
 movzx eax, byte ptr [r12 + rcx]
 cmp eax, 128
 jb .Lsafe_utf8
 cmp eax, 159
 ja .Lsafe_utf8
 mov dword ptr [rsp + 8], 2
 jmp .Lsafe_escape /* U+0080–U+009F → \u00XX */
.Lsafe_utf8:
 lea rdi, [r12 + r14]
 mov rsi, r13
 sub rsi, r14
 call tui_safe_seq_len@PLT
 test eax, eax
 jz .Lsafe_incomplete
.Lsafe_advance:
 add r14, rax
 jmp .Lsafe_scan
.Lsafe_incomplete:
 movzx eax, byte ptr [r12 + r14]
.Lsafe_byte:
 mov dword ptr [rsp + 8], 1
.Lsafe_escape:
 mov [rsp + 12], eax
 cmp r14, r15
 je .Lsafe_marker
 mov rdi, rbx
 mov esi, dword ptr [rsp]
 lea rdx, [r12 + r15]
 mov rcx, r14
 sub rcx, r15
 call tui_raw_write@PLT
.Lsafe_marker:
 mov eax, [rsp + 12]
 lea rsi, [rip + .Lhex]
 mov ecx, eax
 and eax, 15
 shr ecx, 4
 mov al, [rsi + rax]
 mov cl, [rsi + rcx]
 mov word ptr [rsp + 16], 0x785c /* "\x" */
 mov [rsp + 18], cl
 mov [rsp + 19], al
 mov edx, 4
 cmp dword ptr [rsp + 8], 2
 jne .Lsafe_emit
 mov dword ptr [rsp + 16], 0x3030755c /* "\u00" */
 mov [rsp + 20], cl
 mov [rsp + 21], al
 mov edx, 6
.Lsafe_emit:
 mov rsi, rsp
 add rsi, 16
 call sp_put
 mov eax, dword ptr [rsp + 8]
 add r14, rax
 mov r15, r14
 jmp .Lsafe_scan
.Lsafe_next:
 inc r14
 jmp .Lsafe_scan
.Lsafe_tail:
 cmp r13, r15
 jbe .Lsafe_done
 mov rdi, rbx
 mov esi, dword ptr [rsp]
 lea rdx, [r12 + r15]
 mov rcx, r13
 sub rcx, r15
 call tui_raw_write@PLT
.Lsafe_done:
 add rsp, 32
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size tui_safe_write,.-tui_safe_write

.section .text
/* Valid scalar sequence length, zero for malformed/incomplete input. Shared by
 * escaping and visual pagination; no source byte is read outside [data,len). */
.globl tui_safe_seq_len
.type tui_safe_seq_len,@function
tui_safe_seq_len:
 xor eax, eax
 test rsi, rsi
 jz .Lseq_bad_safe
 movzx edx, byte ptr [rdi]
 cmp edx, 128
 jb .Lseq_ascii_safe
 cmp edx, 194
 jb .Lseq_bad_safe
 cmp edx, 244
 ja .Lseq_bad_safe
 mov r8d, 2
 cmp edx, 224
 jb .Lseq_size_safe
 inc r8d
 cmp edx, 240
 jb .Lseq_size_safe
 inc r8d
.Lseq_size_safe:
 cmp rsi, r8
 jb .Lseq_bad_safe
 movzx ecx, byte ptr [rdi + 1]
 cmp edx, 224
 jne .Lseq_ed_safe
 cmp ecx, 160
 jb .Lseq_bad_safe
.Lseq_ed_safe:
 cmp edx, 237
 jne .Lseq_f0_safe
 cmp ecx, 159
 ja .Lseq_bad_safe
.Lseq_f0_safe:
 cmp edx, 240
 jne .Lseq_f4_safe
 cmp ecx, 144
 jb .Lseq_bad_safe
.Lseq_f4_safe:
 cmp edx, 244
 jne .Lseq_cont_safe
 cmp ecx, 143
 ja .Lseq_bad_safe
.Lseq_cont_safe:
 mov ecx, 1
.Lseq_loop_safe:
 movzx edx, byte ptr [rdi + rcx]
 sub edx, 128
 cmp edx, 63
 ja .Lseq_bad_safe
 inc ecx
 cmp rcx, r8
 jb .Lseq_loop_safe
 mov eax, r8d
 ret
.Lseq_ascii_safe:
 mov eax, 1
.Lseq_bad_safe:
 ret
.size tui_safe_seq_len,.-tui_safe_seq_len
