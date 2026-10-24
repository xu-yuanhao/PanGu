.intel_syntax noprefix
.text
.globl probe_write
.type probe_write,@function
probe_write:
 mov eax,1
 syscall
 ret
.globl probe_exit
.type probe_exit,@function
probe_exit:
 mov eax,60
 syscall
 ud2
.section .note.GNU-stack,"",@progbits
