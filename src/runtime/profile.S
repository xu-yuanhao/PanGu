#include "profile_layout.h"
.intel_syntax noprefix
.text
.globl pg_profile_encode
.type pg_profile_encode,@function
pg_profile_encode:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp,40
    mov rbx,rdi
    mov r12,rsi
    mov r13,rdx
    mov r14,rcx
    test r14,r14
    jz .Lbad
    mov qword ptr [r14],0
    test rbx,rbx
    jz .Lbad
    test r12,r12
    jz .Lbad
    cmp r13,32
    jb .Lbad
    cmp r13,4096
    jbe .Lcapacity
    mov r13d,4096
.Lcapacity:
    mov rax,0x0000314746434750
    mov [r12],rax
    mov rax,[rbx+PG_P_DEVICE]
    mov [r12+8],rax
    mov rax,[rbx+PG_P_INODE]
    mov [r12+16],rax
    mov qword ptr [r12+24],0
    mov r15d,32
    xor ebp,ebp
.Lstrings:
    mov rax,rbp
    shl rax,4
    mov rsi,[rbx+rax]
    mov rdx,[rbx+rax+8]
    test rdx,rdx
    jz .Lbad
    call .Lappend
    test eax,eax
    jnz .Lbad
    inc ebp
    cmp ebp,4
    jb .Lstrings
    mov rax,[rbx+PG_P_TOOLS]
    test rax,rax
    jz .Lsuccess
    mov [rsp],rax
    mov rdx,[rax+1280]
    cmp rdx,16
    ja .Lbad
    mov [r12+24],edx
    mov [rsp+8],rdx
    xor ebp,ebp
.Ltool:
    cmp rbp,[rsp+8]
    jae .Lsuccess
    imul rax,rbp,80
    add rax,[rsp]
    mov [rsp+16],rax
    mov qword ptr [rsp+24],0
.Ltool_field:
    mov rax,[rsp+16]
    mov rcx,[rsp+24]
    mov rsi,[rax+rcx]
    test rsi,rsi
    jz .Lbad
    xor edx,edx
.Llength:
    cmp rdx,4096
    jae .Lbad
    cmp byte ptr [rsi+rdx],0
    je .Llength_end
    inc rdx
    jmp .Llength
.Llength_end:
    call .Lappend
    test eax,eax
    jnz .Lbad
    add qword ptr [rsp+24],8
    cmp qword ptr [rsp+24],24
    jb .Ltool_field
    inc rbp
    jmp .Ltool
.Lsuccess:
    mov [r14],r15
    xor eax,eax
    jmp .Lreturn
.Lbad:
    mov eax,1
.Lreturn:
    add rsp,40
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
# Append u32 length and bytes, using the caller's bounded output cursor.
.Lappend:
    cmp rdx,4096
    ja .Lappend_bad
    lea rax,[r15+rdx+4]
    cmp rax,r13
    ja .Lappend_bad
    test rdx,rdx
    jz .Lcopy
    test rsi,rsi
    jz .Lappend_bad
    xor ecx,ecx
.Lnul:
    cmp byte ptr [rsi+rcx],0
    je .Lappend_bad
    inc rcx
    cmp rcx,rdx
    jb .Lnul
.Lcopy:
    mov [r12+r15],edx
    lea rdi,[r12+r15+4]
    mov rcx,rdx
    rep movsb
    mov r15,rax
    xor eax,eax
    ret
.Lappend_bad:
    mov eax,1
    ret
.size pg_profile_encode,.-pg_profile_encode
.section .note.GNU-stack,"",@progbits
