#include "host_layout.h"
.intel_syntax noprefix
.equ LIBRARY,PG_H_BYTES
.equ STATE,PG_H_BYTES+8
.equ ERROR,PG_H_BYTES+12
.text
.globl pg_native_open
.type pg_native_open,@function
pg_native_open:
 test rdi,rdi
 jz .Lnative_bad
 test rsi,rsi
 jz .Lnative_bad
 cmp dword ptr [rdi+STATE],0
 je .Lnative_available
 cmp dword ptr [rdi+STATE],3
 jne .Lnative_bad
.Lnative_available:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 cmp byte ptr [r12],47
 jne .Lnative_invalid
 mov rdi,r12
 mov esi,4096
 call strnlen@PLT
 cmp rax,4096
 jae .Lnative_invalid
 mov dword ptr [rbx+STATE],1
 mov dword ptr [rbx+ERROR],0
 mov rdi,r12
 mov esi,2 /* RTLD_NOW | RTLD_LOCAL on Linux. */
 call dlopen@PLT
 test rax,rax
 jz .Lnative_open_failed
 mov [rbx+LIBRARY],rax
 mov rdi,rax
 lea rsi,[rip+.Lnative_symbol]
 call dlsym@PLT
 test rax,rax
 jz .Lnative_symbol_failed
 cmp dword ptr [rax],1
 jne .Lnative_abi_failed
 cmp dword ptr [rax+4],32
 jne .Lnative_abi_failed
 cmp dword ptr [rax+8],64
 jne .Lnative_abi_failed
 cmp dword ptr [rax+12],0
 jne .Lnative_abi_failed
 mov rdi,rbx
 mov rdx,[rax+16]
 mov rcx,[rax+24]
 test r13,r13
 jz .Lnative_root
 mov rsi,r13
 call pg_host_init_child
 jmp .Lnative_prepared
.Lnative_root:
 mov rsi,rdx
 mov rdx,rcx
 call pg_host_init
.Lnative_prepared:
 test eax,eax
 jnz .Lnative_host_failed
 mov rdi,rbx
 call pg_host_start
 test eax,eax
 jnz .Lnative_host_failed
 mov dword ptr [rbx+STATE],2
 xor eax,eax
 jmp .Lnative_open_return
.Lnative_invalid:
 mov dword ptr [rbx+ERROR],1
 jmp .Lnative_open_fail
.Lnative_open_failed:
 mov dword ptr [rbx+ERROR],2
 mov dword ptr [rbx+STATE],3
 jmp .Lnative_open_fail
.Lnative_symbol_failed:
 mov dword ptr [rbx+ERROR],3
 jmp .Lnative_cleanup
.Lnative_abi_failed:
 mov dword ptr [rbx+ERROR],4
 jmp .Lnative_cleanup
.Lnative_host_failed:
 mov dword ptr [rbx+ERROR],5
.Lnative_cleanup:
 mov dword ptr [rbx+STATE],2
 mov rdi,rbx
 call pg_native_close
.Lnative_open_fail:
 mov eax,1
.Lnative_open_return:
 pop r13
 pop r12
 pop rbx
 ret
.Lnative_bad:
 mov eax,1
 ret
.size pg_native_open,.-pg_native_open

.globl pg_native_close
.type pg_native_close,@function
pg_native_close:
 test rdi,rdi
 jz .Lnative_close_bad
 mov eax,[rdi+STATE]
 test eax,eax
 jz .Lnative_close_idle
 cmp eax,3
 je .Lnative_close_idle
 cmp eax,2
 jne .Lnative_close_bad
 push rbx
 mov rbx,rdi
 call pg_host_close
 test eax,eax
 jnz .Lnative_close_return
 mov rdi,[rbx+LIBRARY]
 test rdi,rdi
 jz .Lnative_closed
 mov dword ptr [rbx+STATE],1
 call dlclose@PLT
 mov dword ptr [rbx+STATE],2
 test eax,eax
 jnz .Lnative_unload_failed
 mov qword ptr [rbx+LIBRARY],0
.Lnative_closed:
 mov dword ptr [rbx+STATE],3
 xor eax,eax
 jmp .Lnative_close_return
.Lnative_unload_failed:
 cmp dword ptr [rbx+ERROR],0
 jne .Lnative_keep_error
 mov dword ptr [rbx+ERROR],6
.Lnative_keep_error:
 mov eax,-1
.Lnative_close_return:
 pop rbx
 ret
.Lnative_close_idle:
 xor eax,eax
 ret
.Lnative_close_bad:
 mov eax,-1
 ret
.size pg_native_close,.-pg_native_close
.section .rodata
.Lnative_symbol: .asciz "pangu_plugin_v1"
.section .note.GNU-stack,"",@progbits
