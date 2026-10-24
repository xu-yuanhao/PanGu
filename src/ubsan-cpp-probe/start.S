.intel_syntax noprefix
.text
.globl pangu_ubsan_test_start
.type pangu_ubsan_test_start,@function
pangu_ubsan_test_start:
 mov rdi,[rsp]
 and rsp,-16
 call pangu_sanitizer_probe
 mov edi,eax
 call probe_exit
 ud2
.size pangu_ubsan_test_start,.-pangu_ubsan_test_start
.section .note.GNU-stack,"",@progbits
