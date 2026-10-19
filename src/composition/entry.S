#include "src/bootstrap-api/bootstrap.h"
.intel_syntax noprefix
.section .rodata
.balign 8
.Lplatform_api:
    .long PANGU_PLATFORM_ABI_VERSION
    .long PANGU_PLATFORM_STRUCT_SIZE
    .quad 0
    .quad pangu_platform_exit
    .quad 0
.section .text
.globl pangu_start
.type pangu_start, @function
pangu_start:
    .cfi_startproc
    .cfi_undefined rip
    xor ebp, ebp
    and rsp, -16
    lea rdi, [rip + .Lplatform_api]
    xor esi, esi
    call pangu_bootstrap_run
    /* Contract failure is a nonzero process result, never a successful exit. */
    xor edi, edi
    mov esi, 125
    call pangu_platform_exit
    ud2
    .cfi_endproc
.size pangu_start, .-pangu_start
.section .note.GNU-stack,"",@progbits
