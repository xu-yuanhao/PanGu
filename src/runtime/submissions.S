#include "session_layout.h"
.intel_syntax noprefix
.text
.globl pg_submission_find
.type pg_submission_find,@function
pg_submission_find:
    test rdi,rdi
    jz .Lnone
    test rsi,rsi
    jz .Lnone
    test rdx,rdx
    jz .Lnone
    cmp rdx,63
    ja .Lnone
    mov r8,[rdi+PG_SC_SUB_COUNT]
    cmp r8,64
    ja .Lnone
    xor ecx,ecx
.Lkey:
    cmp byte ptr [rsi+rcx],0
    je .Lnone
    inc rcx
    cmp rcx,rdx
    jb .Lkey
    lea rax,[rdi+PG_SC_SUBS]
.Lrecord:
    test r8,r8
    jz .Lnone
    cmp byte ptr [rax+PG_SUB_KEY+rdx],0
    jne .Lnext
    xor ecx,ecx
.Lcompare:
    mov r9b,[rsi+rcx]
    cmp [rax+PG_SUB_KEY+rcx],r9b
    jne .Lnext
    inc rcx
    cmp rcx,rdx
    jb .Lcompare
    ret
.Lnext:
    add rax,PG_SUB_BYTES
    dec r8
    jmp .Lrecord
.Lnone:
    xor eax,eax
    ret
.size pg_submission_find,.-pg_submission_find
.section .note.GNU-stack,"",@progbits
