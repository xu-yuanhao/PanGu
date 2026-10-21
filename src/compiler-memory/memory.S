.intel_syntax noprefix
.text
.globl memcpy
.type memcpy,@function
memcpy:
 mov rax,rdi
 mov rcx,rdx
 rep movsb
 ret
.size memcpy,.-memcpy
.globl memset
.type memset,@function
memset:
 mov r8,rdi
 mov eax,esi
 mov rcx,rdx
 rep stosb
 mov rax,r8
 ret
.size memset,.-memset
.globl memmove
.type memmove,@function
memmove:
 mov rax,rdi
 test rdx,rdx
 jz .Ldone
 cmp rdi,rsi
 jbe .Lforward
 mov rcx,rdi
 sub rcx,rsi
 cmp rcx,rdx
 jae .Lforward
 lea rdi,[rdi+rdx-1]
 lea rsi,[rsi+rdx-1]
 mov rcx,rdx
 std
 rep movsb
 cld
 ret
.Lforward:
 mov rcx,rdx
 rep movsb
.Ldone:
 ret
.size memmove,.-memmove
.section .note.GNU-stack,"",@progbits
