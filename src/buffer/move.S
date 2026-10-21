#include "src/bootstrap-api/buffer.h"
.intel_syntax noprefix
.section .text
.globl pangu_buffer_move
.type pangu_buffer_move,@function
pangu_buffer_move:
 .cfi_startproc
 cld
 test rdx,rdx
 jz .Ldone
 cmp rdi,rsi
 jbe .Lforward
 mov rax,rdi
 sub rax,rsi
 cmp rax,rdx
 jae .Lforward
 lea rdi,[rdi+rdx-1]
 lea rsi,[rsi+rdx-1]
 mov rcx,rdx
 std
 rep movsb
 cld
 ret
.Lforward:
 mov rcx,rdx
 rep movsb
.Ldone:
 ret
 .cfi_endproc
.size pangu_buffer_move,.-pangu_buffer_move
.section .note.GNU-stack,"",@progbits
