/* tui_utf8.S — 最小 UTF-8 字节序列辅助，手写汇编原地替换 tui_utf8.c。
 *
 * 契约见 tui_utf8.h：只做字节级推断/回扫/截断，不校验完整合法性；
 * 非法首字节按单字节放行。SysV AMD64：只触碰 caller-saved 寄存器。
 */
.intel_syntax noprefix
.section .text

/* size_t tui_utf8_seq_len(unsigned char lead) — rdi(低8位)=首字节 → rax */
.globl tui_utf8_seq_len
.type tui_utf8_seq_len,@function
tui_utf8_seq_len:
 .cfi_startproc
 movzx eax, dil
 cmp eax, 0x80
 jb .Lseq1
 and eax, 0xE0
 cmp eax, 0xC0
 je .Lseq2
 movzx eax, dil
 and eax, 0xF0
 cmp eax, 0xE0
 je .Lseq3
 movzx eax, dil
 and eax, 0xF8
 cmp eax, 0xF0
 je .Lseq4
.Lseq1:
 mov eax, 1
 ret
.Lseq2:
 mov eax, 2
 ret
.Lseq3:
 mov eax, 3
 ret
.Lseq4:
 mov eax, 4
 ret
 .cfi_endproc
.size tui_utf8_seq_len, .-tui_utf8_seq_len

/* size_t tui_utf8_last_seq_start(const char *buf, size_t len)
 * rdi=buf rsi=len → rax：从末尾回扫连续字节（最多3次）找序列起始偏移。 */
.globl tui_utf8_last_seq_start
.type tui_utf8_last_seq_start,@function
tui_utf8_last_seq_start:
 .cfi_startproc
 test rsi, rsi
 jz .Llss_zero
 lea rcx, [rsi-1]          /* rcx = i */
.Llss_loop:
 test rcx, rcx
 jz .Llss_ret              /* i == 0：不再后退 */
 movzx eax, byte ptr [rdi+rcx]
 and eax, 0xC0
 cmp eax, 0x80
 jne .Llss_ret             /* 非连续字节：i 即序列起始 */
 mov rax, rsi
 sub rax, rcx
 cmp rax, 4
 jae .Llss_ret             /* 已回扫 3 字节：4 字节序列上限 */
 sub rcx, 1
 jmp .Llss_loop
.Llss_ret:
 mov rax, rcx
 ret
.Llss_zero:
 xor eax, eax
 ret
 .cfi_endproc
.size tui_utf8_last_seq_start, .-tui_utf8_last_seq_start

/* size_t tui_utf8_incomplete_tail(const char *buf, size_t len)
 * rdi=buf rsi=len → rax：末尾不完整序列的字节数（0..3）。 */
.globl tui_utf8_incomplete_tail
.type tui_utf8_incomplete_tail,@function
tui_utf8_incomplete_tail:
 .cfi_startproc
 push rbx
 push r12
 push r13
 mov rbx, rdi
 mov r12, rsi
 test r12, r12
 jz .Ltail_zero
 call tui_utf8_last_seq_start@PLT   /* rax = start */
 cmp rax, r12
 jae .Ltail_zero                     /* start >= len：仅 len==0 可能 */
 mov r13, rax                        /* r13 = start */
 movzx edi, byte ptr [rbx+r13]
 call tui_utf8_seq_len@PLT           /* rax = seq */
 cmp rax, 1
 jbe .Ltail_zero
 mov rcx, r12
 sub rcx, r13                        /* rcx = have */
 cmp rcx, rax
 jae .Ltail_zero
 mov rax, rcx                        /* 0 < have < seq：不完整前缀 */
 jmp .Ltail_out
.Ltail_zero:
 xor eax, eax
.Ltail_out:
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_utf8_incomplete_tail, .-tui_utf8_incomplete_tail

/* size_t tui_utf8_truncate_len(const char *buf, size_t len, size_t max_bytes)
 * rdi=buf rsi=len rdx=max → rax：不超 max 且不拆断序列的最大保留字节数。 */
.globl tui_utf8_truncate_len
.type tui_utf8_truncate_len,@function
tui_utf8_truncate_len:
 .cfi_startproc
 push rbx
 push r12
 push r13
 mov rbx, rdi
 mov r12, rdx               /* max：跨调用保存于 callee-saved */
 cmp rsi, rdx
 jbe .Ltr_ret_len           /* len <= max：原样 */
 xor r13d, r13d             /* keep = 0 */
.Ltr_loop:
 cmp r13, r12
 jae .Ltr_ret_keep          /* keep >= max */
 movzx edi, byte ptr [rbx+r13]
 call tui_utf8_seq_len@PLT  /* rax = seq */
 lea rcx, [r13+rax]
 cmp rcx, r12
 ja .Ltr_ret_keep           /* keep + seq > max：不放这个字符 */
 mov r13, rcx
 jmp .Ltr_loop
.Ltr_ret_len:
 mov rax, rsi
 jmp .Ltr_out
.Ltr_ret_keep:
 mov rax, r13
.Ltr_out:
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_utf8_truncate_len, .-tui_utf8_truncate_len

.section .note.GNU-stack,"",@progbits

.section .text
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

/* 表项计数必须在首次使用（.text 的 cmp）之前定义：.set 按序处理，
 * 文件尾部定义会让 cmp 把符号当绝对内存操作数（cmp ecx, [N]）而非立即数。 */
.set TUI_W_ZERO_COUNT, (.Lw_zero_end - .Lw_zero) / 4
.set TUI_W_WIDE_COUNT, (.Lw_wide_end - .Lw_wide) / 4

.globl tui_utf8_cp_width
.type tui_utf8_cp_width,@function
tui_utf8_cp_width:
 test rsi, rsi
 jnz .Lw_nonempty
 xor eax, eax
 ret
.Lw_nonempty:
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
 cmp ecx, OFFSET TUI_W_ZERO_COUNT
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
 cmp ecx, OFFSET TUI_W_WIDE_COUNT
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
.size tui_utf8_cp_width, .-tui_utf8_cp_width
