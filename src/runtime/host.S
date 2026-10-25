#include "host_layout.h"
.intel_syntax noprefix
.text
/* Internal phases: empty=0, prepared=1, starting=2, active=3,
   closing=4, closed=5. Per-plugin states use the same active/closing tags. */
.Lname:
 test rdi,rdi
 jz .Lname_bad
 sub rsp,8
 mov esi,64
 call strnlen@PLT
 add rsp,8
 test rax,rax
 jz .Lname_bad
 cmp rax,64
 setb al
 movzx eax,al
 ret
.Lname_bad:
 xor eax,eax
 ret

/* Returns a declared entry, independent of activation. */
.Lentry:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
.Lentry_loop:
 cmp r13,[rbx+PG_H_SERVICES]
 jae .Lentry_missing
 imul rax,r13,24
 mov rdi,[rbx+PG_H_ENTRIES+rax]
 mov rsi,r12
 call strcmp@PLT
 test eax,eax
 jz .Lentry_found
 inc r13
 jmp .Lentry_loop
.Lentry_found:
 imul rax,r13,24
 lea rax,[rbx+PG_H_ENTRIES+rax]
 jmp .Lentry_return
.Lentry_missing:
 xor eax,eax
.Lentry_return:
 pop r13
 pop r12
 pop rbx
 ret

.globl pg_host_init
.type pg_host_init,@function
pg_host_init:
 xor ecx,ecx
 jmp .Linit_scope
.size pg_host_init,.-pg_host_init

.globl pg_host_init_child
.type pg_host_init_child,@function
pg_host_init_child:
 test rsi,rsi
 jz .Linit_null
 cmp rdi,rsi
 je .Linit_null
 cmp dword ptr [rsi+PG_H_PHASE],3
 jne .Linit_null
 cmp dword ptr [rsi+PG_H_DISPATCHING],0
 jne .Linit_null
 cmp qword ptr [rsi+PG_H_CHILDREN],32
 jae .Linit_null
 mov r8,rsi
 xor r9d,r9d
.Linit_depth:
 cmp dword ptr [r8+PG_H_PHASE],3
 jne .Linit_null
 cmp dword ptr [r8+PG_H_DISPATCHING],0
 jne .Linit_null
 inc r9d
 cmp r9d,32
 ja .Linit_null
 mov r8,[r8+PG_H_PARENT]
 test r8,r8
 jnz .Linit_depth
 mov rax,rsi
 mov rsi,rdx
 mov rdx,rcx
 mov rcx,rax
.Linit_scope:
 test rdi,rdi
 jz .Linit_null
 cmp dword ptr [rdi+PG_H_PHASE],0
 je .Linit_available
 cmp dword ptr [rdi+PG_H_PHASE],5
 jne .Linit_null
.Linit_available:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov [rsp+24],rcx
 xor esi,esi
 mov edx,PG_H_BYTES
 call memset@PLT
 mov rax,[rsp+24]
 mov [rbx+PG_H_PARENT],rax
 mov dword ptr [rbx+PG_H_ERROR_OWNER],-1
 cmp r13,32
 ja .Linit_invalid
 test r13,r13
 jz .Linit_inputs
 test r12,r12
 jz .Linit_invalid
.Linit_inputs:
 mov [rbx+PG_H_PLUGINS],r12
 mov [rbx+PG_H_COUNT],r13
 xor r14d,r14d
.Linit_plugin:
 cmp r14,r13
 jae .Ltopology_begin
 mov r15,r14
 shl r15,6
 add r15,r12
 mov [rbx+PG_H_ERROR_OWNER],r14d
 mov rdi,[r15]
 call .Lname
 test eax,eax
 jz .Linit_invalid
 cmp qword ptr [r15+48],0
 je .Linit_invalid
 cmp qword ptr [r15+56],0
 je .Linit_invalid
 cmp qword ptr [r15+24],32
 ja .Linit_invalid
 cmp qword ptr [r15+40],32
 ja .Linit_invalid
 cmp qword ptr [r15+24],0
 je .Linit_provides_ptr
 cmp qword ptr [r15+16],0
 je .Linit_invalid
.Linit_provides_ptr:
 cmp qword ptr [r15+40],0
 je .Linit_duplicate_names
 cmp qword ptr [r15+32],0
 je .Linit_invalid
.Linit_duplicate_names:
 mov qword ptr [rsp],0
.Linit_duplicate_loop:
 mov rax,[rsp]
 cmp rax,r14
 jae .Linit_requirements
 shl rax,6
 mov rdi,[r12+rax]
 mov rsi,[r15]
 call strcmp@PLT
 test eax,eax
 jz .Linit_duplicate
 inc qword ptr [rsp]
 jmp .Linit_duplicate_loop
.Linit_requirements:
 mov qword ptr [rsp],0
.Linit_require_loop:
 mov rax,[rsp]
 cmp rax,[r15+24]
 jae .Linit_exports
 mov rcx,[r15+16]
 mov rdi,[rcx+rax*8]
 call .Lname
 test eax,eax
 jz .Linit_invalid
 inc qword ptr [rsp]
 jmp .Linit_require_loop
.Linit_exports:
 mov qword ptr [rsp],0
.Linit_export_loop:
 mov rax,[rsp]
 cmp rax,[r15+40]
 jae .Linit_next_plugin
 cmp qword ptr [rbx+PG_H_SERVICES],32
 jae .Linit_invalid
 mov rcx,[r15+32]
 mov rax,[rcx+rax*8]
 mov [rsp+8],rax
 mov rdi,rax
 call .Lname
 test eax,eax
 jz .Linit_invalid
 mov rdi,rbx
 mov rsi,[rsp+8]
 call .Lentry
 test rax,rax
 jnz .Linit_duplicate
 imul rax,[rbx+PG_H_SERVICES],24
 mov rcx,[rsp+8]
 mov [rbx+PG_H_ENTRIES+rax],rcx
 mov [rbx+PG_H_ENTRIES+rax+16],r14
 inc qword ptr [rbx+PG_H_SERVICES]
 inc qword ptr [rsp]
 jmp .Linit_export_loop
.Linit_next_plugin:
 inc r14
 jmp .Linit_plugin
.Ltopology_begin:
 xor r14d,r14d
.Lcheck_requirements_plugin:
 cmp r14,r13
 jae .Ltopology_checked
 mov r15,r14
 shl r15,6
 add r15,r12
 mov qword ptr [rsp],0
.Lcheck_requirements_name:
 mov rax,[rsp]
 cmp rax,[r15+24]
 jae .Lcheck_requirements_next
 mov rcx,[r15+16]
 mov rsi,[rcx+rax*8]
 mov [rsp+8],rsi
 mov rdi,rbx
 call .Lentry
 test rax,rax
 jnz .Lcheck_requirement_ready
 mov rdi,[rbx+PG_H_PARENT]
 mov rsi,[rsp+8]
 call pg_host_find
 test rax,rax
 jz .Linit_missing
 cmp qword ptr [rbx+PG_H_SERVICES],32
 jae .Linit_invalid
 mov rdx,rax
 imul rax,[rbx+PG_H_SERVICES],24
 mov rcx,[rsp+8]
 mov [rbx+PG_H_ENTRIES+rax],rcx
 mov [rbx+PG_H_ENTRIES+rax+8],rdx
 mov qword ptr [rbx+PG_H_ENTRIES+rax+16],-1
 inc qword ptr [rbx+PG_H_SERVICES]
.Lcheck_requirement_ready:
 inc qword ptr [rsp]
 jmp .Lcheck_requirements_name
.Lcheck_requirements_next:
 inc r14
 jmp .Lcheck_requirements_plugin
.Ltopology_checked:
 mov qword ptr [rsp+16],0
.Ltopology_pass:
 mov rax,[rsp+16]
 cmp rax,r13
 je .Linit_prepared
 mov [rsp+24],rax
 xor r14d,r14d
.Ltopology_plugin:
 cmp r14,r13
 jae .Ltopology_progress
 cmp dword ptr [rbx+PG_H_STATES+r14*4],1
 je .Ltopology_next
 mov r15,r14
 shl r15,6
 add r15,r12
 mov qword ptr [rsp],0
.Ltopology_requirement:
 mov rax,[rsp]
 cmp rax,[r15+24]
 jae .Ltopology_ready
 mov rcx,[r15+16]
 mov rsi,[rcx+rax*8]
 mov rdi,rbx
 call .Lentry
 test rax,rax
 jz .Linit_missing
 mov rax,[rax+16]
 cmp rax,-1
 je .Ltopology_inherited
 cmp dword ptr [rbx+PG_H_STATES+rax*4],1
 jne .Ltopology_next
.Ltopology_inherited:
 inc qword ptr [rsp]
 jmp .Ltopology_requirement
.Ltopology_ready:
 mov dword ptr [rbx+PG_H_STATES+r14*4],1
 mov rax,[rsp+16]
 mov [rbx+PG_H_ORDER+rax*4],r14d
 inc qword ptr [rsp+16]
.Ltopology_next:
 inc r14
 jmp .Ltopology_plugin
.Ltopology_progress:
 mov rax,[rsp+16]
 cmp rax,[rsp+24]
 jne .Ltopology_pass
 mov dword ptr [rbx+PG_H_ERROR_OWNER],-1
 mov eax,4
 jmp .Linit_failed
.Linit_prepared:
 mov dword ptr [rbx+PG_H_ERROR_OWNER],-1
 lea rdi,[rbx+PG_H_STATES]
 xor esi,esi
 mov edx,128
 call memset@PLT
 mov dword ptr [rbx+PG_H_PHASE],1
 mov rax,[rbx+PG_H_PARENT]
 test rax,rax
 jz .Linit_attached
 mov rcx,[rax+PG_H_CHILD]
 mov [rbx+PG_H_NEXT],rcx
 test rcx,rcx
 jz .Linit_first_child
 mov [rcx+PG_H_PREVIOUS],rbx
.Linit_first_child:
 mov [rax+PG_H_CHILD],rbx
 inc qword ptr [rax+PG_H_CHILDREN]
.Linit_attached:
 xor eax,eax
 jmp .Linit_return
.Linit_invalid:
 mov eax,1
 jmp .Linit_failed
.Linit_duplicate:
 mov eax,2
 jmp .Linit_failed
.Linit_missing:
 mov [rbx+PG_H_ERROR_OWNER],r14d
 mov eax,3
.Linit_failed:
 mov [rbx+PG_H_ERROR],eax
 mov qword ptr [rbx+PG_H_PARENT],0
 mov dword ptr [rbx+PG_H_PHASE],5
 mov eax,1
.Linit_return:
 add rsp,32
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Linit_null:
 mov eax,1
 ret
.size pg_host_init_child,.-pg_host_init_child

.globl pg_host_provide
.type pg_host_provide,@function
pg_host_provide:
 test rdi,rdi
 jz .Lprovide_bad
 test rcx,rcx
 jz .Lprovide_bad
 cmp dword ptr [rdi+PG_H_PHASE],2
 jne .Lprovide_bad
 cmp rsi,[rdi+PG_H_CURRENT]
 jne .Lprovide_bad
 cmp rsi,[rdi+PG_H_COUNT]
 jae .Lprovide_bad
 push rbx
 push r12
 push r13
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rcx
 mov [rsp],rdx
 mov rdi,rdx
 call .Lname
 test eax,eax
 jz .Lprovide_pop_bad
 mov rdi,rbx
 mov rsi,[rsp]
 call .Lentry
 test rax,rax
 jz .Lprovide_pop_bad
 cmp [rax+16],r12
 jne .Lprovide_pop_bad
 cmp qword ptr [rax+8],0
 jne .Lprovide_pop_bad
 mov [rax+8],r13
 xor eax,eax
 jmp .Lprovide_return
.Lprovide_pop_bad:
 mov eax,1
.Lprovide_return:
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret
.Lprovide_bad:
 mov eax,1
 ret
.size pg_host_provide,.-pg_host_provide

.globl pg_host_get
.type pg_host_get,@function
pg_host_get:
 test rdi,rdi
 jz .Lget_bad
 cmp rsi,[rdi+PG_H_COUNT]
 jae .Lget_bad
 mov eax,[rdi+PG_H_STATES+rsi*4]
 sub eax,2
 cmp eax,2
 ja .Lget_bad
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rdx
 call .Lname
 test eax,eax
 jz .Lget_pop_bad
 shl r12,6
 add r12,[rbx+PG_H_PLUGINS]
 xor r14d,r14d
.Lget_requirement:
 cmp r14,[r12+24]
 jae .Lget_pop_bad
 mov rax,[r12+16]
 mov rdi,[rax+r14*8]
 mov rsi,r13
 call strcmp@PLT
 test eax,eax
 jz .Lget_declared
 inc r14
 jmp .Lget_requirement
.Lget_declared:
 mov rdi,rbx
 mov rsi,r13
 call .Lentry
 test rax,rax
 jz .Lget_pop_bad
 mov rcx,[rax+16]
 cmp rcx,-1
 je .Lget_inherited
 cmp dword ptr [rbx+PG_H_STATES+rcx*4],3
 jne .Lget_pop_bad
.Lget_inherited:
 mov rax,[rax+8]
 jmp .Lget_return
.Lget_pop_bad:
 xor eax,eax
.Lget_return:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Lget_bad:
 xor eax,eax
 ret
.size pg_host_get,.-pg_host_get

.globl pg_host_find
.type pg_host_find,@function
pg_host_find:
 test rdi,rdi
 jz .Lfind_bad
 cmp dword ptr [rdi+PG_H_PHASE],3
 jne .Lfind_bad
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsi
 call .Lname
 test eax,eax
 jz .Lfind_pop_bad
 mov rdi,rbx
 mov rsi,r12
 call .Lentry
 test rax,rax
 jz .Lfind_parent
 mov rcx,[rax+16]
 cmp rcx,-1
 je .Lfind_inherited
 cmp dword ptr [rbx+PG_H_STATES+rcx*4],3
 jne .Lfind_pop_bad
.Lfind_inherited:
 mov rax,[rax+8]
 jmp .Lfind_return
.Lfind_parent:
 mov rdi,[rbx+PG_H_PARENT]
 mov rsi,r12
 call pg_host_find
 jmp .Lfind_return
.Lfind_pop_bad:
 xor eax,eax
.Lfind_return:
 add rsp,8
 pop r12
 pop rbx
 ret
.Lfind_bad:
 xor eax,eax
 ret
.size pg_host_find,.-pg_host_find

.globl pg_host_start
.type pg_host_start,@function
pg_host_start:
 test rdi,rdi
 jz .Lstart_bad
 cmp dword ptr [rdi+PG_H_PHASE],1
 jne .Lstart_bad
 mov rax,[rdi+PG_H_PARENT]
 test rax,rax
 jz .Lstart_parent_ready
.Lstart_ancestor:
 cmp dword ptr [rax+PG_H_PHASE],3
 jne .Lstart_bad
 mov rax,[rax+PG_H_PARENT]
 test rax,rax
 jnz .Lstart_ancestor
.Lstart_parent_ready:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov dword ptr [rbx+PG_H_PHASE],2
.Lstart_plugin:
 mov rax,[rbx+PG_H_LIVE]
 cmp rax,[rbx+PG_H_COUNT]
 jae .Lstart_success
 mov r12d,[rbx+PG_H_ORDER+rax*4]
 mov [rbx+PG_H_CURRENT],r12
 mov dword ptr [rbx+PG_H_STATES+r12*4],2
 inc qword ptr [rbx+PG_H_LIVE]
 mov rax,r12
 shl rax,6
 add rax,[rbx+PG_H_PLUGINS]
 mov rdi,[rax+8]
 mov rsi,rbx
 mov rdx,r12
 mov dword ptr [rbx+PG_H_DISPATCHING],1
 call [rax+48]
 mov dword ptr [rbx+PG_H_DISPATCHING],0
 test eax,eax
 jnz .Lstart_callback_failed
 xor r13d,r13d
.Lstart_exports:
 cmp r13,[rbx+PG_H_SERVICES]
 jae .Lstart_active
 imul rax,r13,24
 cmp [rbx+PG_H_ENTRIES+rax+16],r12
 jne .Lstart_next_export
 cmp qword ptr [rbx+PG_H_ENTRIES+rax+8],0
 je .Lstart_unpublished
.Lstart_next_export:
 inc r13
 jmp .Lstart_exports
.Lstart_active:
 mov dword ptr [rbx+PG_H_STATES+r12*4],3
 jmp .Lstart_plugin
.Lstart_success:
 mov dword ptr [rbx+PG_H_PHASE],3
 mov qword ptr [rbx+PG_H_CURRENT],-1
 xor eax,eax
 jmp .Lstart_return
.Lstart_callback_failed:
 mov dword ptr [rbx+PG_H_ERROR],5
 jmp .Lstart_cleanup
.Lstart_unpublished:
 mov dword ptr [rbx+PG_H_ERROR],6
.Lstart_cleanup:
 mov [rbx+PG_H_ERROR_OWNER],r12d
 mov rdi,rbx
 call pg_host_close
 mov eax,1
.Lstart_return:
 pop r13
 pop r12
 pop rbx
 ret
.Lstart_bad:
 mov eax,1
 ret
.size pg_host_start,.-pg_host_start

/* Preflight the subtree before a close can mutate any sibling or parent. */
.Lsubtree_busy:
 cmp dword ptr [rdi+PG_H_DISPATCHING],0
 jne .Lsubtree_busy_yes
 push rbx
 mov rbx,[rdi+PG_H_CHILD]
.Lsubtree_busy_child:
 test rbx,rbx
 jz .Lsubtree_busy_no
 mov rdi,rbx
 call .Lsubtree_busy
 test eax,eax
 jnz .Lsubtree_busy_pop
 mov rbx,[rbx+PG_H_NEXT]
 jmp .Lsubtree_busy_child
.Lsubtree_busy_no:
 xor eax,eax
.Lsubtree_busy_pop:
 pop rbx
 ret
.Lsubtree_busy_yes:
 mov eax,1
 ret

.globl pg_host_close
.type pg_host_close,@function
pg_host_close:
 test rdi,rdi
 jz .Lclose_bad
 cmp dword ptr [rdi+PG_H_DISPATCHING],0
 jne .Lclose_bad
 mov eax,[rdi+PG_H_PHASE]
 test eax,eax
 jz .Lclose_idle
 cmp eax,5
 je .Lclose_idle
 cmp eax,4
 ja .Lclose_bad
 push rbx
 push r12
 push r13
 mov rbx,rdi
 call .Lsubtree_busy
 test eax,eax
 jz .Lclose_available
 mov eax,-1
 jmp .Lclose_return
.Lclose_available:
 mov dword ptr [rbx+PG_H_PHASE],4
.Lclose_child:
 mov rdi,[rbx+PG_H_CHILD]
 test rdi,rdi
 jz .Lclose_plugin
 mov dword ptr [rbx+PG_H_DISPATCHING],1
 call pg_host_close
 mov dword ptr [rbx+PG_H_DISPATCHING],0
 test eax,eax
 jz .Lclose_child
 cmp eax,1
 je .Lclose_return
 cmp dword ptr [rbx+PG_H_ERROR],0
 jne .Lclose_return
 mov dword ptr [rbx+PG_H_ERROR],7
 mov dword ptr [rbx+PG_H_ERROR_OWNER],-1
 jmp .Lclose_return
.Lclose_plugin:
 mov rax,[rbx+PG_H_LIVE]
 test rax,rax
 jz .Lclose_released
 dec rax
 mov r12d,[rbx+PG_H_ORDER+rax*4]
 mov [rbx+PG_H_CURRENT],r12
 mov dword ptr [rbx+PG_H_STATES+r12*4],4
 mov rax,r12
 shl rax,6
 add rax,[rbx+PG_H_PLUGINS]
 mov rdi,[rax+8]
 mov dword ptr [rbx+PG_H_DISPATCHING],1
 call [rax+56]
 mov dword ptr [rbx+PG_H_DISPATCHING],0
 cmp eax,1
 je .Lclose_return
 test eax,eax
 jnz .Lclose_error
 mov dword ptr [rbx+PG_H_STATES+r12*4],5
 xor r13d,r13d
.Lclose_exports:
 cmp r13,[rbx+PG_H_SERVICES]
 jae .Lclose_next
 imul rax,r13,24
 cmp [rbx+PG_H_ENTRIES+rax+16],r12
 jne .Lclose_next_export
 mov qword ptr [rbx+PG_H_ENTRIES+rax+8],0
.Lclose_next_export:
 inc r13
 jmp .Lclose_exports
.Lclose_next:
 dec qword ptr [rbx+PG_H_LIVE]
 jmp .Lclose_plugin
.Lclose_error:
 cmp dword ptr [rbx+PG_H_ERROR],0
 jne .Lclose_keep_error
 mov dword ptr [rbx+PG_H_ERROR],7
 mov [rbx+PG_H_ERROR_OWNER],r12d
.Lclose_keep_error:
 mov eax,-1
 jmp .Lclose_return
.Lclose_released:
 mov rax,[rbx+PG_H_PARENT]
 test rax,rax
 jz .Lclose_detached
 mov rcx,[rbx+PG_H_NEXT]
 mov rdx,[rbx+PG_H_PREVIOUS]
 test rdx,rdx
 jz .Lclose_first
 mov [rdx+PG_H_NEXT],rcx
 jmp .Lclose_link
.Lclose_first:
 mov [rax+PG_H_CHILD],rcx
.Lclose_link:
 test rcx,rcx
 jz .Lclose_unlink
 mov [rcx+PG_H_PREVIOUS],rdx
.Lclose_unlink:
 dec qword ptr [rax+PG_H_CHILDREN]
 mov qword ptr [rbx+PG_H_PARENT],0
 mov qword ptr [rbx+PG_H_NEXT],0
 mov qword ptr [rbx+PG_H_PREVIOUS],0
.Lclose_detached:
 mov dword ptr [rbx+PG_H_PHASE],5
 mov qword ptr [rbx+PG_H_CURRENT],-1
 xor eax,eax
.Lclose_return:
 pop r13
 pop r12
 pop rbx
 ret
.Lclose_idle:
 xor eax,eax
 ret
.Lclose_bad:
 mov eax,-1
 ret
.size pg_host_close,.-pg_host_close
.section .note.GNU-stack,"",@progbits
