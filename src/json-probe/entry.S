#include "src/bootstrap-api/buffer.h"
.intel_syntax noprefix
.section .rodata
.balign 8
.Lmemory:
 .long 1,40
 .quad 0,pangu_memory_map,pangu_memory_unmap,0
.section .text
.globl pangu_json_test_start
.type pangu_json_test_start,@function
pangu_json_test_start:
 .cfi_startproc
 .cfi_undefined rip
 mov rsi,[rsp]
 dec rsi
 xor ebp,ebp
 and rsp,-16
 lea rdi,[rip+.Lmemory]
 call json_test
 mov esi,eax
 xor edi,edi
 call pangu_platform_exit
 ud2
 .cfi_endproc
.size pangu_json_test_start,.-pangu_json_test_start
.section .note.GNU-stack,"",@progbits
