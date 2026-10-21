#include "src/bootstrap-api/memory.h"
.intel_syntax noprefix
.section .text
.globl pangu_memory_map
.type pangu_memory_map,@function
pangu_memory_map:
 .cfi_startproc
 test rdx,rdx
 jz .Lmap_invalid
 test rsi,rsi
 jz .Lmap_invalid
 mov rax,rsi
 add rax,4095
 jc .Lmap_invalid
 and rax,-4096
 cmp QWORD PTR [rdx],0
 jne .Lmap_busy
 cmp QWORD PTR [rdx+8],0
 jne .Lmap_busy
 push r12
 .cfi_adjust_cfa_offset 8
 .cfi_offset r12,-16
 push r13
 .cfi_adjust_cfa_offset 8
 .cfi_offset r13,-24
 mov r12,rdx
 mov r13,rax
 xor edi,edi
 mov rsi,rax
 mov edx,3
 mov r10d,34
 mov r8,-1
 xor r9d,r9d
 mov eax,9
 syscall
 cmp rax,-4095
 jae .Lmap_error
 mov QWORD PTR [r12],rax
 mov QWORD PTR [r12+8],r13
 xor eax,eax
 jmp .Lmap_restore
.Lmap_error:
 cmp rax,-12
 mov eax,8
 jne .Lmap_restore
 mov eax,2
.Lmap_restore:
 pop r13
 .cfi_adjust_cfa_offset -8
 .cfi_restore r13
 pop r12
 .cfi_adjust_cfa_offset -8
 .cfi_restore r12
 ret
.Lmap_invalid:
 mov eax,1
 ret
.Lmap_busy:
 mov eax,5
 ret
 .cfi_endproc
.size pangu_memory_map,.-pangu_memory_map
.globl pangu_memory_unmap
.type pangu_memory_unmap,@function
pangu_memory_unmap:
 .cfi_startproc
 test rsi,rsi
 jz .Lunmap_invalid
 mov rdi,QWORD PTR [rsi]
 mov rdx,QWORD PTR [rsi+8]
 test rdi,rdi
 jz .Lunmap_invalid
 test rdx,rdx
 jz .Lunmap_invalid
 test rdi,4095
 jnz .Lunmap_invalid
 test rdx,4095
 jnz .Lunmap_invalid
 mov rax,rdi
 add rax,rdx
 jc .Lunmap_invalid
 push r12
 .cfi_adjust_cfa_offset 8
 .cfi_offset r12,-16
 mov r12,rsi
 mov rsi,rdx
 mov eax,11
 syscall
 test rax,rax
 jnz .Lunmap_error
 mov QWORD PTR [r12],0
 mov QWORD PTR [r12+8],0
 xor eax,eax
 jmp .Lunmap_restore
.Lunmap_error:
 mov eax,8
.Lunmap_restore:
 pop r12
 .cfi_adjust_cfa_offset -8
 .cfi_restore r12
 ret
.Lunmap_invalid:
 mov eax,1
 ret
 .cfi_endproc
.size pangu_memory_unmap,.-pangu_memory_unmap
.section .note.GNU-stack,"",@progbits
