#include "src/bootstrap-api/bootstrap.h"
.intel_syntax noprefix
.section .text
.globl pangu_platform_exit
.type pangu_platform_exit, @function
pangu_platform_exit:
    .cfi_startproc
    /* SysV (context, status) -> Linux exit_group(status); no libc. */
    mov edi, esi
    mov eax, 231
    syscall
    ud2
    .cfi_endproc
.size pangu_platform_exit, .-pangu_platform_exit
.section .note.GNU-stack,"",@progbits
