#include "src/bootstrap-api/io.h"
.intel_syntax noprefix
.section .rodata
.balign 8
.Lio:
 .long PANGU_IO_ABI_VERSION,PANGU_IO_STRUCT_SIZE
 .quad 0,pangu_file_open,pangu_file_read,pangu_file_write,pangu_file_close,pangu_clock_monotonic,0
.section .text
.globl pangu_io_test_start
.type pangu_io_test_start,@function
pangu_io_test_start:
 .cfi_startproc
 .cfi_undefined rip
 xor ebp,ebp
 and rsp,-16
 lea rdi,[rip+.Lio]
 call io_test
 mov esi,eax
 xor edi,edi
 call pangu_platform_exit
 ud2
 .cfi_endproc
.size pangu_io_test_start,.-pangu_io_test_start
.section .note.GNU-stack,"",@progbits
