#include "src/t14-abi/client-entry-v1.h"
/* Fixed ABI1 context offsets: size64, userdata32, local instance index48. */
.text
.global pangu_test_asm_entry
.type pangu_test_asm_entry,@function
pangu_test_asm_entry:
 cmpl $1,0(%rdi)
 jne .Lbad_entry
 cmpl $64,4(%rdi)
 jne .Lbad_entry
 mov 32(%rdi),%rax
 test %rax,%rax
 jz .Lbad_entry
 mov 48(%rdi),%ecx
 cmp (%rax),%ecx
 jne .Lbad_entry
 jmp entry_asm_dispatch
.Lbad_entry:
 mov $1,%eax
 ret
.size pangu_test_asm_entry,.-pangu_test_asm_entry
.section .note.GNU-stack,"",@progbits
