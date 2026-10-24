.intel_syntax noprefix
.text
.globl pangu_guard_left_start
.type pangu_guard_left_start,@function
pangu_guard_left_start:
 xor edi,edi
 jmp guard_fault_common
.size pangu_guard_left_start,.-pangu_guard_left_start
.globl pangu_guard_right_start
.type pangu_guard_right_start,@function
pangu_guard_right_start:
 mov edi,1
guard_fault_common:
 and rsp,-16
 call guard_fault
 mov edi,eax
 call probe_exit
 ud2
.size pangu_guard_right_start,.-pangu_guard_right_start
.section .note.GNU-stack,"",@progbits
