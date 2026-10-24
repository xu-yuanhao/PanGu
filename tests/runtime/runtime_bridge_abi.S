.intel_syntax noprefix
.text
.macro WRAPPER name,target
.globl \name
.type \name,@function
\name:
    mov r11,rsp
    and r11d,15
    cmp r11d,8
    je 1f
    ud2
1:
    sub rsp,8
    call \target
    add rsp,8
    mov rcx,0xc1c1c1c1c1c1c1c1
    mov rdx,0xd2d2d2d2d2d2d2d2
    mov rsi,0x5151515151515151
    mov rdi,0xd1d1d1d1d1d1d1d1
    mov r8,0x8888888888888888
    mov r9,0x9999999999999999
    mov r10,0xaaaaaaaaaaaaaaaa
    mov r11,0xbbbbbbbbbbbbbbbb
    ret
.size \name,.-\name
.endm
WRAPPER pg_bridge_test_submit,pg_bridge_fixture_submit
WRAPPER pg_bridge_test_cancel,pg_bridge_fixture_cancel
WRAPPER pg_bridge_test_poll,pg_bridge_fixture_poll
WRAPPER pg_bridge_test_detach,pg_bridge_fixture_detach
.macro CHECK register,value,bit
    mov r10,\value
    cmp \register,r10
    je 1f
    or eax,\bit
1:
.endm
.globl pg_bridge_probe
.type pg_bridge_probe,@function
pg_bridge_probe:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp,24
    mov [rsp+8],rdi
    mov rdi,rsi
    mov rsi,rdx
    mov rdx,rcx
    mov r10,0x7ec4d86101a35b29
    mov [rsp],r10
    mov rbx,0x1020304050607080
    mov rbp,0x2131415161718191
    mov r12,0x32425262728292a2
    mov r13,0x435363738393a3b3
    mov r14,0x5464748494a4b4c4
    mov r15,0x65758595a5b5c5d5
    call [rsp+8]
    xor eax,eax
    CHECK rbx,0x1020304050607080,1
    CHECK rbp,0x2131415161718191,2
    CHECK r12,0x32425262728292a2,4
    CHECK r13,0x435363738393a3b3,8
    CHECK r14,0x5464748494a4b4c4,16
    CHECK r15,0x65758595a5b5c5d5,32
    pushfq
    pop r10
    test r10d,0x400
    jz 1f
    or eax,64
1:
    cld
    mov r10,0x7ec4d86101a35b29
    cmp [rsp],r10
    je 2f
    or eax,128
2:
    add rsp,24
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
.size pg_bridge_probe,.-pg_bridge_probe
.section .note.GNU-stack,"",@progbits
