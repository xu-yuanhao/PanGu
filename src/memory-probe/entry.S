#include "src/bootstrap-api/memory.h"
.intel_syntax noprefix
.section .rodata
.balign 8
.Lmemory_api:
 .long PANGU_MEMORY_ABI_VERSION,PANGU_MEMORY_STRUCT_SIZE
 .quad 0,pangu_memory_map,pangu_memory_unmap,0
.section .text
.globl pangu_memory_test_start
.type pangu_memory_test_start,@function
pangu_memory_test_start:
 .cfi_startproc
 .cfi_undefined rip
 xor ebp,ebp
 and rsp,-16
 lea rdi,[rip+.Lmemory_api]
 call memory_test
 mov esi,eax
 xor edi,edi
 call pangu_platform_exit
 ud2
 .cfi_endproc
.size pangu_memory_test_start,.-pangu_memory_test_start
.section .note.GNU-stack,"",@progbits
