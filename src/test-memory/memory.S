#include "src/bootstrap-api/memory.h"
.intel_syntax noprefix
.section .bss
.balign 4096
.Lpage: .zero 4096
.Lactive: .long 0
.section .text
.globl pangu_memory_map
.type pangu_memory_map,@function
pangu_memory_map:
 test rdx,rdx
 jz .Linvalid
 test rsi,rsi
 jz .Linvalid
 cmp QWORD PTR [rdx],0
 jne .Lbusy
 cmp QWORD PTR [rdx+8],0
 jne .Lbusy
 cmp rsi,4096
 ja .Lnomem
 cmp DWORD PTR [rip+.Lactive],0
 jne .Lbusy
 mov r8,rdx
 lea rdi,[rip+.Lpage]
 mov ecx,4096
 xor eax,eax
 cld
 rep stosb
 lea rax,[rip+.Lpage]
 mov QWORD PTR [r8],rax
 mov QWORD PTR [r8+8],4096
 mov DWORD PTR [rip+.Lactive],1
 xor eax,eax
 ret
.Linvalid:
 mov eax,1
 ret
.Lbusy:
 mov eax,5
 ret
.Lnomem:
 mov eax,2
 ret
.size pangu_memory_map,.-pangu_memory_map
.globl pangu_memory_unmap
.type pangu_memory_unmap,@function
pangu_memory_unmap:
 test rsi,rsi
 jz .Linvalid
 cmp DWORD PTR [rip+.Lactive],1
 jne .Linvalid
 lea rax,[rip+.Lpage]
 cmp QWORD PTR [rsi],rax
 jne .Linvalid
 cmp QWORD PTR [rsi+8],4096
 jne .Linvalid
 mov DWORD PTR [rip+.Lactive],0
 mov QWORD PTR [rsi],0
 mov QWORD PTR [rsi+8],0
 xor eax,eax
 ret
.size pangu_memory_unmap,.-pangu_memory_unmap
.section .note.GNU-stack,"",@progbits
