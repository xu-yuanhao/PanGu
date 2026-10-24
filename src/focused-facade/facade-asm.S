#include "src/t14-abi/client-abi-v1.h"
.text
.global facade_asm_query
.type facade_asm_query,@function
facade_asm_query:
 sub $8,%rsp
 mov $PANGU_LIFECYCLE_ABI,%edi
 mov $PANGU_LIFECYCLE_API_SIZE,%esi
 mov $PANGU_LIFECYCLE_TARGET,%edx
 xor %ecx,%ecx
 call pangu_lifecycle_query
 add $8,%rsp
 ret
.size facade_asm_query,.-facade_asm_query
.section .note.GNU-stack,"",@progbits
