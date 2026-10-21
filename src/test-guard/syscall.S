.intel_syntax noprefix
.section .text
.globl guard_syscall
.type guard_syscall,@function
guard_syscall:
 .cfi_startproc
 mov rax,rdi
 mov rdi,rsi
 mov rsi,rdx
 mov rdx,rcx
 mov r10,r8
 mov r8,r9
 mov r9,QWORD PTR [rsp+8]
 syscall
 ret
 .cfi_endproc
.size guard_syscall,.-guard_syscall
.section .note.GNU-stack,"",@progbits
