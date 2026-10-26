/* Pi d5b3de22 Session.#armHoldTimer; MIT: docs/licenses/pi.md. */
.intel_syntax noprefix
.text
.globl pg_hold_timer_open
.type pg_hold_timer_open,@function
pg_hold_timer_open:
 mov eax,1
 test rdi,rdi
 jz .Ltimer_open_end
 test rsi,rsi
 jz .Ltimer_open_end
 test rdx,rdx
 jz .Ltimer_open_end
 test rcx,rcx
 jz .Ltimer_open_end
 cmp dword ptr [rsi+52],1
 jne .Ltimer_open_end
 cmp qword ptr [rcx+8],0
 je .Ltimer_open_end
 cmp qword ptr [rcx+16],0
 je .Ltimer_open_end
 cmp qword ptr [rcx+24],0
 je .Ltimer_open_end
 mov [rdi],rsi
 mov [rdi+8],rdx
 mov rax,[rcx]
 mov [rdi+16],rax
 mov rax,[rcx+8]
 mov [rdi+24],rax
 mov rax,[rcx+16]
 mov [rdi+32],rax
 mov rax,[rcx+24]
 mov [rdi+40],rax
 mov qword ptr [rdi+48],0
 mov dword ptr [rdi+56],0
 mov dword ptr [rdi+60],1
 mov qword ptr [rdi+64],0
 xor eax,eax
.Ltimer_open_end:
 ret
.size pg_hold_timer_open,.-pg_hold_timer_open

/* Latch one control-plane failure; an earlier Session cause wins. */
.Ltimer_fail:
 mov rax,[rdi+8]
 cmp dword ptr [rax+52],0
 jne .Ltimer_failed
 mov [rax+52],esi
.Ltimer_failed:
 mov eax,[rax+52]
 mov [rdi+64],eax
 ret

/* Ticket: controller, provider handle, absolute deadline, state.
   state 0 armed, 1 admitted job, 2 rejected enqueue, 3 executing job. */
.Ltimer_drop:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,[rbx+48]
 test r12,r12
 jz .Ltimer_drop_end
 mov qword ptr [rbx+48],0
 mov rsi,[r12+8]
 test rsi,rsi
 jz .Ltimer_drop_free
 mov rdi,[rbx+16]
 call [rbx+32]
.Ltimer_drop_free:
 mov rdi,r12
 call free@PLT
.Ltimer_drop_end:
 add rsp,8
 pop r12
 pop rbx
 ret

.globl pg_hold_timer_sync
.type pg_hold_timer_sync,@function
pg_hold_timer_sync:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r13d,1
 test rbx,rbx
 jz .Ltimer_sync_end
 cmp dword ptr [rbx+56],0
 jne .Ltimer_sync_end
 cmp dword ptr [rbx+60],1
 jne .Ltimer_sync_end
 mov rax,[rbx+8]
 mov r13d,[rax+52]
 test r13d,r13d
 jnz .Ltimer_sync_end
 mov dword ptr [rbx+56],1
 mov r12,[rbx+48]
 test r12,r12
 jz .Ltimer_sync_deadline
 cmp dword ptr [r12+24],1
 je .Ltimer_sync_ok
.Ltimer_sync_deadline:
 mov rdi,[rbx]
 call pg_document_holds_deadline@PLT
 mov r13,rax
 test r12,r12
 jz .Ltimer_sync_replace
 cmp [r12+16],r13
 je .Ltimer_sync_ok
.Ltimer_sync_replace:
 mov rdi,rbx
 call .Ltimer_drop
 cmp r13,-1
 je .Ltimer_sync_ok
 mov edi,1
 mov esi,32
 call calloc@PLT
 test rax,rax
 jz .Ltimer_sync_allocation
 mov r12,rax
 mov [rax],rbx
 mov [rax+16],r13
 mov [rbx+48],rax
 mov rax,[rbx]
 mov rdi,[rax+24]
 call [rax+32]
 mov rsi,r13
 sub rsi,rax
 jae .Ltimer_delay_ready
 xor esi,esi
.Ltimer_delay_ready:
 mov rdi,[rbx+16]
 lea rdx,[rip+.Ltimer_fire]
 mov rcx,r12
 lea r8,[r12+8]
 call [rbx+24]
 test eax,eax
 jnz .Ltimer_sync_arm_failed
 cmp qword ptr [r12+8],0
 je .Ltimer_sync_arm_failed
.Ltimer_sync_ok:
 xor r13d,r13d
 jmp .Ltimer_sync_unlock
.Ltimer_sync_arm_failed:
 mov rdi,rbx
 call .Ltimer_drop
 mov esi,4
 jmp .Ltimer_sync_fail
.Ltimer_sync_allocation:
 mov esi,5
.Ltimer_sync_fail:
 mov rdi,rbx
 call .Ltimer_fail
 mov r13d,eax
.Ltimer_sync_unlock:
 mov dword ptr [rbx+56],0
.Ltimer_sync_end:
 mov eax,r13d
 pop r13
 pop r12
 pop rbx
 ret
.size pg_hold_timer_sync,.-pg_hold_timer_sync

.Ltimer_fire:
 push rbx
 push r12
 sub rsp,8
 mov r12,rdi
 mov rbx,[r12]
 cmp dword ptr [rbx+60],1
 jne .Ltimer_fire_end
 cmp [rbx+48],r12
 jne .Ltimer_fire_end
 cmp dword ptr [r12+24],0
 jne .Ltimer_fire_end
 mov dword ptr [r12+24],1
 mov rdi,[rbx+16]
 lea rsi,[rip+.Ltimer_run]
 mov rdx,r12
 call [rbx+40]
 test eax,eax
 jz .Ltimer_fire_end
 mov dword ptr [r12+24],2
 mov rdi,rbx
 mov esi,4
 call .Ltimer_fail
.Ltimer_fire_end:
 add rsp,8
 pop r12
 pop rbx
 ret

.Ltimer_run:
 push rbx
 push r12
 sub rsp,8
 mov r12,rdi
 mov rbx,[r12]
 mov dword ptr [rbx+56],1
 mov dword ptr [r12+24],3
 mov qword ptr [rbx+48],0
 mov rsi,[r12+8]
 test rsi,rsi
 jz .Ltimer_job_ready
 mov rdi,[rbx+16]
 call [rbx+32]
.Ltimer_job_ready:
 cmp dword ptr [rbx+60],1
 jne .Ltimer_job_free
 mov rax,[rbx+8]
 cmp dword ptr [rax+52],0
 jne .Ltimer_job_free
 mov rdi,[rbx]
 mov rsi,rax
 call pg_document_holds_flush@PLT
 test eax,eax
 jz .Ltimer_job_free
 mov esi,eax
 mov rdi,rbx
 call .Ltimer_fail
.Ltimer_job_free:
 mov rdi,r12
 call free@PLT
 mov dword ptr [rbx+56],0
 add rsp,8
 pop r12
 pop rbx
 ret

.globl pg_hold_timer_stop
.type pg_hold_timer_stop,@function
pg_hold_timer_stop:
 push rbx
 mov rbx,rdi
 mov eax,-1
 test rbx,rbx
 jz .Ltimer_stop_end
 cmp dword ptr [rbx+56],0
 jne .Ltimer_stop_end
 cmp dword ptr [rbx+60],0
 je .Ltimer_stop_done
 mov dword ptr [rbx+60],2
 mov dword ptr [rbx+56],1
 mov rax,[rbx+48]
 test rax,rax
 jz .Ltimer_stop_closed
 cmp dword ptr [rax+24],1
 jne .Ltimer_stop_drop
 /* The queue owns this ticket until delivery; cancel only its handle. */
 mov rsi,[rax+8]
 test rsi,rsi
 jz .Ltimer_stop_pending
 mov qword ptr [rax+8],0
 mov rdi,[rbx+16]
 call [rbx+32]
.Ltimer_stop_pending:
 mov dword ptr [rbx+56],0
 mov eax,1
 jmp .Ltimer_stop_end
.Ltimer_stop_drop:
 mov rdi,rbx
 call .Ltimer_drop
.Ltimer_stop_closed:
 mov dword ptr [rbx+60],0
 mov dword ptr [rbx+56],0
.Ltimer_stop_done:
 xor eax,eax
.Ltimer_stop_end:
 pop rbx
 ret
.size pg_hold_timer_stop,.-pg_hold_timer_stop
.section .note.GNU-stack,"",@progbits
