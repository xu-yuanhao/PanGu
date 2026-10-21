#include "src/bootstrap-api/region.h"
.intel_syntax noprefix
.section .rodata
.balign 8
.Lallocator:
 .long 1,40
 .quad 0,pangu_test_slots_allocate,pangu_test_slots_epoch,0
.section .text
.globl pangu_region_slots_start
.type pangu_region_slots_start,@function
pangu_region_slots_start:
 .cfi_startproc
 .cfi_undefined rip
 xor ebp,ebp
 and rsp,-16
 lea rdi,[rip+.Lallocator]
 call pangu_region_consumer
 mov esi,eax
 xor edi,edi
 call pangu_platform_exit
 ud2
 .cfi_endproc
.size pangu_region_slots_start,.-pangu_region_slots_start
.section .note.GNU-stack,"",@progbits
