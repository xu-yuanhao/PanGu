.intel_syntax noprefix
.text
.globl pangu_abi_guard_frame
.type pangu_abi_guard_frame,@function
pangu_abi_guard_frame:
 push rbx
 push rbp
 push r12
 push r13
 push r14
 push r15
 sub rsp,104
 mov r10,rdi
 mov rax,[rip+.Lactive]
 mov [r10+136],rax
 mov [r10+128],rsp
 mov [rip+.Lactive],r10
 mov QWORD PTR [r10+120],0
 test rsp,15
 jnz .Lbad_alignment
 mov rax,[r10+56]
 mov [rsp+0],rax
 mov rax,[r10+64]
 mov [rsp+8],rax
 mov rax,[r10+72]
 mov [rsp+16],rax
 mov rax,[r10+80]
 mov [rsp+24],rax
 mov rax,[r10+88]
 mov [rsp+32],rax
 mov rax,[r10+96]
 mov [rsp+40],rax
 mov rax,[r10+104]
 mov [rsp+48],rax
 movabs rax,0x13579bdf2468ace0
 mov [rsp+56],rax
 mov [rsp+64],rax
 mov [rsp+72],rax
 mov [rsp+80],rax
 mov [rsp+88],rax
 mov [rsp+96],rax
 movabs rbx,0x1122334455667788
 movabs rbp,0x2233445566778899
 movabs r12,0x33445566778899aa
 movabs r13,0x445566778899aabb
 movabs r14,0x5566778899aabbcc
 movabs r15,0x66778899aabbccdd
 mov rdi,[r10+8]
 mov rsi,[r10+16]
 mov rdx,[r10+24]
 mov rcx,[r10+32]
 mov r8,[r10+40]
 mov r9,[r10+48]
 call [r10]
 mov r10,[rip+.Lactive]
 mov [r10+112],rax
 xor edx,edx
 cmp rsp,[r10+128]
 je .Lsp_ok
 or edx,128
.Lsp_ok:
 mov rsp,[r10+128]
 movabs rcx,0x1122334455667788
 cmp rbx,rcx
 je .Lreg0_ok
 or edx,1
.Lreg0_ok:
 movabs rcx,0x2233445566778899
 cmp rbp,rcx
 je .Lreg1_ok
 or edx,2
.Lreg1_ok:
 movabs rcx,0x33445566778899aa
 cmp r12,rcx
 je .Lreg2_ok
 or edx,4
.Lreg2_ok:
 movabs rcx,0x445566778899aabb
 cmp r13,rcx
 je .Lreg3_ok
 or edx,8
.Lreg3_ok:
 movabs rcx,0x5566778899aabbcc
 cmp r14,rcx
 je .Lreg4_ok
 or edx,16
.Lreg4_ok:
 movabs rcx,0x66778899aabbccdd
 cmp r15,rcx
 je .Lreg5_ok
 or edx,32
.Lreg5_ok:
 pushfq
 pop rcx
 test ecx,1024
 jz .Ldf_ok
 or edx,64
.Ldf_ok:
 cld
 movabs rcx,0x13579bdf2468ace0
 cmp [rsp+56],rcx
 je .Lcanary56_ok
 or edx,256
.Lcanary56_ok:
 cmp [rsp+64],rcx
 je .Lcanary64_ok
 or edx,256
.Lcanary64_ok:
 cmp [rsp+72],rcx
 je .Lcanary72_ok
 or edx,256
.Lcanary72_ok:
 cmp [rsp+80],rcx
 je .Lcanary80_ok
 or edx,256
.Lcanary80_ok:
 cmp [rsp+88],rcx
 je .Lcanary88_ok
 or edx,256
.Lcanary88_ok:
 cmp [rsp+96],rcx
 je .Lcanary96_ok
 or edx,256
.Lcanary96_ok:
 mov [r10+120],rdx
 .Lrestore:
 mov rax,[r10+136]
 mov [rip+.Lactive],rax
 add rsp,104
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbp
 pop rbx
 ret
.Lbad_alignment:
 mov QWORD PTR [r10+120],512
 jmp .Lrestore
.size pangu_abi_guard_frame,.-pangu_abi_guard_frame
.globl start_abi_bad_rbx
.type start_abi_bad_rbx,@function
start_abi_bad_rbx:
 xor ebx,ebx
 ret
.size start_abi_bad_rbx,.-start_abi_bad_rbx
.globl start_abi_bad_df
.type start_abi_bad_df,@function
start_abi_bad_df:
 std
 ret
.size start_abi_bad_df,.-start_abi_bad_df
.globl start_abi_bad_stack
.type start_abi_bad_stack,@function
start_abi_bad_stack:
 mov QWORD PTR [rsp+64],0
 ret
.size start_abi_bad_stack,.-start_abi_bad_stack
.bss
.balign 8
.Lactive:
 .zero 8
.section .note.GNU-stack,"",@progbits
