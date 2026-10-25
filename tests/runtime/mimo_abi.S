/* mimo_abi.S — MiMo 汇编批次的 ABI 探针与受控破坏回调。
 *
 * __wrap_*：先转发 __real_ 再破坏 caller-saved（保留 rax），并在入口断言
 * rsp≡8(mod 16)，从而逐外部调用点验证 mimo*.S 的栈对齐与"caller-saved
 * 不跨调用保存活跃值"。pg_mimo_abi_probe* 以哨兵校验被测函数保持
 * callee-saved/DF/栈哨兵；pg_mimo_abi_corrupt 故意破坏 rbx 并置 DF，
 * 作为探针自检的负例。 */
.intel_syntax noprefix
.text

.macro WRAP_TRASH name
.globl __wrap_\name
.type __wrap_\name,@function
__wrap_\name:
    mov r11,rsp
    and r11d,15
    cmp r11d,8
    je 1f
    ud2
1:
    sub rsp,8
    call __real_\name
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
.size __wrap_\name,.-__wrap_\name
.endm
WRAP_TRASH strlen
WRAP_TRASH strcmp
WRAP_TRASH strncmp
WRAP_TRASH memchr
WRAP_TRASH memcpy
WRAP_TRASH strchr
WRAP_TRASH free
WRAP_TRASH getenv
WRAP_TRASH strdup
WRAP_TRASH json_object_object_add
WRAP_TRASH json_object_array_add
WRAP_TRASH json_object_new_string
WRAP_TRASH json_object_new_string_len
WRAP_TRASH json_object_put
WRAP_TRASH json_tokener_parse_ex
WRAP_TRASH json_tokener_get_error
WRAP_TRASH json_object_get_string
WRAP_TRASH json_object_is_type
WRAP_TRASH json_object_object_get_ex

.macro CHECK register,value,bit
    mov r10,\value
    cmp \register,r10
    je 1f
    or eax,\bit
1:
.endm

/* ---- unsigned pg_mimo_abi_probe3(fn, m, line, event)：三参被测函数。 ---- */
.globl pg_mimo_abi_probe3
.type pg_mimo_abi_probe3,@function
pg_mimo_abi_probe3:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp,24
    mov [rsp+8],rdi                    /* fn */
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
.size pg_mimo_abi_probe3,.-pg_mimo_abi_probe3

/* ---- unsigned pg_mimo_abi_probe2(fn, m, session)：二参被测函数。 ---- */
.globl pg_mimo_abi_probe2
.type pg_mimo_abi_probe2,@function
pg_mimo_abi_probe2:
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
.size pg_mimo_abi_probe2,.-pg_mimo_abi_probe2

.globl pg_mimo_abi_corrupt
.type pg_mimo_abi_corrupt,@function
pg_mimo_abi_corrupt:
    xor ebx,ebx
    std
    ret
.size pg_mimo_abi_corrupt,.-pg_mimo_abi_corrupt
.section .note.GNU-stack,"",@progbits
