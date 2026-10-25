.intel_syntax noprefix
.text
fixture_start:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 inc dword ptr [rbx+8]
 mov rdi,r12
 mov rsi,r13
 lea rdx,[rip+seed]
 call pg_host_get@PLT
 test rax,rax
 jz failed
 mov eax,[rax]
 mov [rbx],eax
 mov rdi,r12
 mov rsi,r13
 lea rdx,[rip+output]
 mov rcx,rbx
 call pg_host_provide@PLT
#ifdef START_FAIL
 mov eax,1
#endif
 jmp start_return
failed:
 mov eax,1
start_return:
 pop r13
 pop r12
 pop rbx
 ret
fixture_stop:
 inc dword ptr [rdi+12]
 cmp dword ptr [rdi+4],0
 je stopped
 dec dword ptr [rdi+4]
 mov eax,1
 ret
stopped:
 xor eax,eax
 ret
.section .rodata
name: .asciz "native-fixture"
#ifdef MISSING_DEP
seed: .asciz "missing"
#else
seed: .asciz "seed"
#endif
output: .asciz "native.output"
.data
state:
 .long 0
#ifdef START_FAIL
 .long 3
#else
 .long 0
#endif
 .long 0,0
.section .data.rel.ro
requires: .quad seed
provides: .quad output
plugin: .quad name,state,requires,1,provides,1,fixture_start,fixture_stop
#ifdef NO_ENTRY
.globl other_entry
other_entry:
#else
.globl pangu_plugin_v1
.type pangu_plugin_v1,@object
pangu_plugin_v1:
#endif
#ifdef BAD_ABI
 .long 99
#else
 .long 1
#endif
 .long 32,64,0
 .quad plugin,1
.section .note.GNU-stack,"",@progbits
