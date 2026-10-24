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
