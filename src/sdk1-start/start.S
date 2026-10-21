.intel_syntax noprefix
.text
.globl pangu_sdk2_test_start
.type pangu_sdk2_test_start,@function
pangu_sdk2_test_start:
 and rsp,-16
 call main
 mov edi,eax
 call probe_exit
 ud2
.size pangu_sdk2_test_start,.-pangu_sdk2_test_start
.section .note.GNU-stack,"",@progbits
