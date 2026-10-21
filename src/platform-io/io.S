#include "src/bootstrap-api/io.h"
.intel_syntax noprefix
.section .text
.globl pangu_file_open
.type pangu_file_open,@function
pangu_file_open:
 .cfi_startproc
 test rcx,rcx
 jz .Lopen_invalid
 cmp DWORD PTR [rcx],-1
 jne .Lopen_busy
 test rsi,rsi
 jz .Lopen_invalid
 cmp BYTE PTR [rsi],0
 je .Lopen_invalid
 cmp edx,1
 ja .Lopen_invalid
 push r12
 .cfi_adjust_cfa_offset 8
 .cfi_offset r12,-16
 push r13
 .cfi_adjust_cfa_offset 8
 .cfi_offset r13,-24
 sub rsp,144
 .cfi_adjust_cfa_offset 144
 mov r12,rcx
 test edx,edx
 mov edx,0xa0800
 jz .Lopen_call
 mov edx,0xa08c1
.Lopen_call:
 mov edi,-100
 mov r10d,384
 mov eax,257
 syscall
 cmp rax,-4095
 jae .Lopen_os_error
 mov r13d,eax
 mov edi,eax
 mov rsi,rsp
 mov eax,5
 syscall
 test rax,rax
 jnz .Lopen_stat_error
 mov eax,DWORD PTR [rsp+24]
 and eax,0xf000
 cmp eax,0x8000
 jne .Lopen_not_regular
 mov DWORD PTR [r12],r13d
 xor eax,eax
 jmp .Lopen_restore
.Lopen_os_error:
 cmp rax,-2
 je .Lopen_missing
 cmp rax,-17
 je .Lopen_exists
 mov eax,8
 jmp .Lopen_restore
.Lopen_missing:
 mov eax,3
 jmp .Lopen_restore
.Lopen_exists:
 mov eax,4
 jmp .Lopen_restore
.Lopen_stat_error:
 mov edi,r13d
 mov eax,3
 syscall
 mov eax,8
 jmp .Lopen_restore
.Lopen_not_regular:
 mov edi,r13d
 mov eax,3
 syscall
 mov eax,9
.Lopen_restore:
 add rsp,144
 .cfi_adjust_cfa_offset -144
 pop r13
 .cfi_adjust_cfa_offset -8
 .cfi_restore r13
 pop r12
 .cfi_adjust_cfa_offset -8
 .cfi_restore r12
 ret
.Lopen_invalid:
 mov eax,1
 ret
.Lopen_busy:
 mov eax,5
 ret
 .cfi_endproc
.size pangu_file_open,.-pangu_file_open
/* Shared transfer body: r9 holds read(0)/write(1), never a user-selected syscall. */
.globl pangu_file_read
.type pangu_file_read,@function
pangu_file_read:
 xor r9d,r9d
 jmp .Ltransfer
.size pangu_file_read,.-pangu_file_read
.globl pangu_file_write
.type pangu_file_write,@function
pangu_file_write:
 mov r9d,1
 jmp .Ltransfer
.size pangu_file_write,.-pangu_file_write
.Ltransfer:
 .cfi_startproc
 test r8,r8
 jz .Ltransfer_invalid
 mov QWORD PTR [r8],0
 test esi,esi
 js .Ltransfer_invalid
 cmp rcx,0x7ffff000
 ja .Ltransfer_invalid
 test rcx,rcx
 jz .Ltransfer_valid
 test rdx,rdx
 jz .Ltransfer_invalid
.Ltransfer_valid:
 push r12
 .cfi_adjust_cfa_offset 8
 .cfi_offset r12,-16
 mov r12,r8
 mov edi,esi
 mov rsi,rdx
 mov rdx,rcx
.Ltransfer_retry:
 mov eax,r9d
 syscall
 cmp rax,-4
 je .Ltransfer_retry
 test rax,rax
 js .Ltransfer_error
 mov QWORD PTR [r12],rax
 xor eax,eax
 jmp .Ltransfer_restore
.Ltransfer_error:
 mov eax,8
.Ltransfer_restore:
 pop r12
 .cfi_adjust_cfa_offset -8
 .cfi_restore r12
 ret
.Ltransfer_invalid:
 mov eax,1
 ret
 .cfi_endproc
.globl pangu_file_close
.type pangu_file_close,@function
pangu_file_close:
 .cfi_startproc
 test rsi,rsi
 jz .Lclose_invalid
 mov edi,DWORD PTR [rsi]
 test edi,edi
 js .Lclose_invalid
 mov DWORD PTR [rsi],-1
 mov eax,3
 syscall
 test rax,rax
 jnz .Lclose_error
 xor eax,eax
 ret
.Lclose_error:
 mov eax,8
 ret
.Lclose_invalid:
 mov eax,1
 ret
 .cfi_endproc
.size pangu_file_close,.-pangu_file_close
.globl pangu_clock_monotonic
.type pangu_clock_monotonic,@function
pangu_clock_monotonic:
 .cfi_startproc
 test rsi,rsi
 jz .Lclock_invalid
 push r12
 .cfi_adjust_cfa_offset 8
 .cfi_offset r12,-16
 sub rsp,16
 .cfi_adjust_cfa_offset 16
 mov r12,rsi
 mov QWORD PTR [r12],0
 mov edi,1
 mov rsi,rsp
 mov eax,228
 syscall
 test rax,rax
 jnz .Lclock_error
 mov rax,QWORD PTR [rsp]
 mov rcx,QWORD PTR [rsp+8]
 test rax,rax
 js .Lclock_error
 cmp rcx,1000000000
 jae .Lclock_error
 mov edi,1000000000
 mul rdi
 test rdx,rdx
 jnz .Lclock_overflow
 add rax,rcx
 jc .Lclock_overflow
 mov QWORD PTR [r12],rax
 xor eax,eax
 jmp .Lclock_restore
.Lclock_error:
 mov eax,8
 jmp .Lclock_restore
.Lclock_overflow:
 mov eax,11
.Lclock_restore:
 add rsp,16
 .cfi_adjust_cfa_offset -16
 pop r12
 .cfi_adjust_cfa_offset -8
 .cfi_restore r12
 ret
.Lclock_invalid:
 mov eax,1
 ret
 .cfi_endproc
.size pangu_clock_monotonic,.-pangu_clock_monotonic
.section .note.GNU-stack,"",@progbits
