#include "process_layout.h"
.intel_syntax noprefix
.section .rodata
.Lshell: .asciz "/bin/bash"
.Ldash_c: .asciz "-c"
.Lnull: .asciz "/dev/null"
.Lsignals: .long 1,2,3,13,15,17
.balign 8
.Lempty_env: .quad 0
.text

.globl pg_process_init
.type pg_process_init,@function
pg_process_init:
 test rdi,rdi
 jz .Linit_done
 mov r8,rdi
 xor eax,eax
 mov ecx,PG_P_BYTES/8
 rep stosq
 mov dword ptr [r8+PG_P_PID],-1
 mov qword ptr [r8+PG_P_OUTPUT],-1
.Linit_done:
 ret
.size pg_process_init,.-pg_process_init

.globl pg_process_service
.type pg_process_service,@function
pg_process_service:
 mov [rdi],rsi
 lea rax,[rip+pg_process_start]
 mov [rdi+8],rax
 lea rax,[rip+pg_process_next]
 mov [rdi+16],rax
 lea rax,[rip+pg_process_cancel]
 mov [rdi+24],rax
 lea rax,[rip+pg_process_close]
 mov [rdi+32],rax
 mov rax,rdi
 ret
.size pg_process_service,.-pg_process_service

/* Monotonic milliseconds or -1 with errno. */
.Lnow:
 push rbx
 sub rsp,16
 mov edi,1
 mov rsi,rsp
 call clock_gettime@PLT
 test eax,eax
 jnz .Lclock_error
 mov rbx,[rsp]
 test rbx,rbx
 js .Lclock_overflow
 imul rbx,rbx,1000
 jo .Lclock_overflow
 mov rax,[rsp+8]
 xor edx,edx
 mov ecx,1000000
 div rcx
 add rbx,rax
 jo .Lclock_overflow
 mov rax,rbx
 jmp .Lclock_return
.Lclock_overflow:
 call __errno_location@PLT
 mov dword ptr [rax],75
.Lclock_error:
 mov rax,-1
.Lclock_return:
 add rsp,16
 pop rbx
 ret

#define ACTION 4096
#define ATTR 4176
#define SIGSET 4512
#define SIGACT 4640
#define OUT_PIPE 4792
#define ERR_PIPE 4800
#define ARGV 4808
#define OWNED 4840
#define FAILURE 4844
#define CHILD 4848
#define ENV 4856
#define STAMP 4864
#define START_FRAME 4904
.globl pg_process_start
.type pg_process_start,@function
pg_process_start:
 test rdi,rdi
 jz .Lstart_invalid
 test rsi,rsi
 jz .Lstart_invalid
 cmp dword ptr [rdi+PG_P_STATE],0
 jne .Lstart_busy
 cmp rcx,4096
 jae .Lstart_invalid
 test rcx,rcx
 jz .Lstart_save
 test rdx,rdx
 jz .Lstart_invalid
.Lstart_save:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,4096
 mov byte ptr [rsp],0
 sub rsp,808
 mov rbp,rsp
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov dword ptr [rbp+OWNED],0
 mov dword ptr [rbp+FAILURE],22
 mov qword ptr [rbp+OUT_PIPE],-1
 mov qword ptr [rbp+ERR_PIPE],-1
 cmp dword ptr [r12],0
 jl .Lstart_cleanup
 cmp qword ptr [r12+24],2147483647
 ja .Lstart_cleanup
 mov r15,[r12+8]
 test r15,r15
 jnz .Lcheck_shell
 lea r15,[rip+.Lshell]
.Lcheck_shell:
 cmp byte ptr [r15],'/'
 jne .Lstart_cleanup
 mov rdi,r15
 mov esi,4096
 call strnlen@PLT
 cmp rax,4096
 jae .Lstart_cleanup
 test r14,r14
 jz .Lcommand_empty
 mov rdi,r13
 xor esi,esi
 mov rdx,r14
 call memchr@PLT
 test rax,rax
 jnz .Lstart_cleanup
 mov rdi,rbp
 mov rsi,r13
 mov rdx,r14
 call memcpy@PLT
.Lcommand_empty:
 mov byte ptr [rbp+r14],0
 mov edi,17
 xor esi,esi
 lea rdx,[rbp+SIGACT]
 call sigaction@PLT
 test eax,eax
 jnz .Lstart_errno
 cmp qword ptr [rbp+SIGACT],0
 jne .Lnot_waitable
 test dword ptr [rbp+SIGACT+PG_P_SA_FLAGS],2
 jnz .Lnot_waitable
 call .Lnow
 cmp rax,-1
 je .Lstart_errno
 mov [rbp+STAMP],rax
 mov rax,[r12+16]
 test rax,rax
 jnz .Lenv_ready
 lea rax,[rip+.Lempty_env]
.Lenv_ready:
 mov [rbp+ENV],rax
 lea rdi,[rbp+OUT_PIPE]
 call .Lpipe
 test eax,eax
 jnz .Lstart_errno
 lea rdi,[rbp+ERR_PIPE]
 call .Lpipe
 test eax,eax
 jnz .Lstart_errno
 lea rdi,[rbp+ACTION]
 call posix_spawn_file_actions_init@PLT
 test eax,eax
 jnz .Lstart_error_code
 or dword ptr [rbp+OWNED],1
 lea rdi,[rbp+ATTR]
 call posix_spawnattr_init@PLT
 test eax,eax
 jnz .Lstart_error_code
 or dword ptr [rbp+OWNED],2
 lea rdi,[rbp+ACTION]
 mov esi,[r12]
 call posix_spawn_file_actions_addfchdir_np@PLT
 test eax,eax
 jnz .Lstart_error_code
 lea rdi,[rbp+ACTION]
 xor esi,esi
 lea rdx,[rip+.Lnull]
 xor ecx,ecx
 xor r8d,r8d
 call posix_spawn_file_actions_addopen@PLT
 test eax,eax
 jnz .Lstart_error_code
 lea rdi,[rbp+ACTION]
 mov esi,[rbp+OUT_PIPE+4]
 mov edx,1
 call posix_spawn_file_actions_adddup2@PLT
 test eax,eax
 jnz .Lstart_error_code
 lea rdi,[rbp+ACTION]
 mov esi,[rbp+ERR_PIPE+4]
 mov edx,2
 call posix_spawn_file_actions_adddup2@PLT
 test eax,eax
 jnz .Lstart_error_code
 lea rdi,[rbp+ACTION]
 mov esi,3
 call posix_spawn_file_actions_addclosefrom_np@PLT
 test eax,eax
 jnz .Lstart_error_code
 lea rdi,[rbp+SIGSET]
 call sigemptyset@PLT
 test eax,eax
 jnz .Lstart_errno
 lea rdi,[rbp+ATTR]
 lea rsi,[rbp+SIGSET]
 call posix_spawnattr_setsigmask@PLT
 test eax,eax
 jnz .Lstart_error_code
 xor r14d,r14d
.Ldefaults:
 lea rax,[rip+.Lsignals]
 mov esi,[rax+r14*4]
 lea rdi,[rbp+SIGSET]
 call sigaddset@PLT
 test eax,eax
 jnz .Lstart_errno
 inc r14
 cmp r14,6
 jb .Ldefaults
 lea rdi,[rbp+ATTR]
 lea rsi,[rbp+SIGSET]
 call posix_spawnattr_setsigdefault@PLT
 test eax,eax
 jnz .Lstart_error_code
 lea rdi,[rbp+ATTR]
 xor esi,esi
 call posix_spawnattr_setpgroup@PLT
 test eax,eax
 jnz .Lstart_error_code
 lea rdi,[rbp+ATTR]
 mov esi,14
 call posix_spawnattr_setflags@PLT
 test eax,eax
 jnz .Lstart_error_code
 mov [rbp+ARGV],r15
 lea rax,[rip+.Ldash_c]
 mov [rbp+ARGV+8],rax
 mov [rbp+ARGV+16],rbp
 mov qword ptr [rbp+ARGV+24],0
 lea rdi,[rbp+CHILD]
 mov rsi,r15
 lea rdx,[rbp+ACTION]
 lea rcx,[rbp+ATTR]
 lea r8,[rbp+ARGV]
 mov r9,[rbp+ENV]
 call posix_spawn@PLT
 test eax,eax
 jnz .Lstart_error_code
 mov eax,[rbp+CHILD]
 mov [rbx+PG_P_PID],eax
 mov eax,[rbp+OUT_PIPE]
 mov [rbx+PG_P_OUTPUT],eax
 mov dword ptr [rbp+OUT_PIPE],-1
 mov eax,[rbp+ERR_PIPE]
 mov [rbx+PG_P_OUTPUT+4],eax
 mov dword ptr [rbp+ERR_PIPE],-1
 mov qword ptr [rbx+PG_P_EOF],0
 mov qword ptr [rbx+PG_P_EXITED],0
 mov qword ptr [rbx+PG_P_SIGNAL],0
 mov qword ptr [rbx+PG_P_ERROR],0
 mov qword ptr [rbx+PG_P_IDLE],0
 mov rax,[r12+24]
 test rax,rax
 jz .Ldeadline_ready
 add rax,[rbp+STAMP]
.Ldeadline_ready:
 mov [rbx+PG_P_DEADLINE],rax
 mov dword ptr [rbx+PG_P_STATE],1
 mov dword ptr [rbp+FAILURE],0
 jmp .Lstart_cleanup
.Lnot_waitable:
 mov eax,95
 jmp .Lstart_error_code
.Lstart_errno:
 call __errno_location@PLT
 mov eax,[rax]
.Lstart_error_code:
 mov [rbp+FAILURE],eax
.Lstart_cleanup:
 test dword ptr [rbp+OWNED],1
 jz .Ldestroy_attr
 lea rdi,[rbp+ACTION]
 call posix_spawn_file_actions_destroy@PLT
.Ldestroy_attr:
 test dword ptr [rbp+OWNED],2
 jz .Lclose_start_fds
 lea rdi,[rbp+ATTR]
 call posix_spawnattr_destroy@PLT
.Lclose_start_fds:
 xor r14d,r14d
.Lclose_start_loop:
 mov edi,[rbp+OUT_PIPE+r14*4]
 test edi,edi
 js .Lclose_start_next
 call close@PLT
.Lclose_start_next:
 inc r14
 cmp r14,4
 jb .Lclose_start_loop
 mov eax,[rbp+FAILURE]
 add rsp,START_FRAME
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lstart_invalid:
 mov eax,22
 ret
.Lstart_busy:
 mov eax,16
 ret
.size pg_process_start,.-pg_process_start

/* Pipe read end nonblocking; promote write sources away from stdin/out/err. */
.Lpipe:
 push rbx
 mov rbx,rdi
 mov esi,524288
 call pipe2@PLT
 test eax,eax
 jnz .Lpipe_return
 mov edi,[rbx]
 mov esi,3
 xor eax,eax
 call fcntl@PLT
 test eax,eax
 js .Lpipe_return
 or eax,2048
 mov edx,eax
 mov edi,[rbx]
 mov esi,4
 xor eax,eax
 call fcntl@PLT
 test eax,eax
 js .Lpipe_return
 cmp dword ptr [rbx+4],3
 jge .Lpipe_good
 mov edi,[rbx+4]
 mov esi,1030
 mov edx,3
 xor eax,eax
 call fcntl@PLT
 test eax,eax
 js .Lpipe_return
 mov esi,eax
 mov edi,[rbx+4]
 mov [rbx+4],eax
 call close@PLT
.Lpipe_good:
 xor eax,eax
.Lpipe_return:
 pop rbx
 ret

.globl pg_process_cancel
.type pg_process_cancel,@function
pg_process_cancel:
 test rdi,rdi
 jz .Lcancel_done
 cmp dword ptr [rdi+PG_P_STATE],1
 jne .Lcancel_done
 cmp dword ptr [rdi+PG_P_REASON],0
 jne .Lcancel_done
 mov dword ptr [rdi+PG_P_REASON],1
.Lcancel_done:
 ret
.size pg_process_cancel,.-pg_process_cancel

/* Store the first OS error. rbx is the instance throughout poll helpers. */
.Lrecord_error:
 cmp dword ptr [rbx+PG_P_ERROR],0
 jne .Lhave_error
 mov [rbx+PG_P_ERROR],eax
.Lhave_error:
 cmp dword ptr [rbx+PG_P_REASON],0
 jne .Lrecord_done
 mov dword ptr [rbx+PG_P_REASON],4
.Lrecord_done:
 ret

.Lstop:
 sub rsp,8
 cmp dword ptr [rbx+PG_P_KILLED],0
 jne .Lstop_done
 cmp dword ptr [rbx+PG_P_PID],0
 jle .Lstop_done
 mov edi,[rbx+PG_P_PID]
 neg edi
 mov esi,9
 call kill@PLT
 test eax,eax
 jz .Lstopped
 mov edi,[rbx+PG_P_PID]
 mov esi,9
 call kill@PLT
 test eax,eax
 jz .Lstopped
 call __errno_location@PLT
 mov eax,[rax]
 cmp eax,3
 je .Lstopped
 call .Lrecord_error
 jmp .Lstop_done
.Lstopped:
 mov dword ptr [rbx+PG_P_KILLED],1
.Lstop_done:
 add rsp,8
 ret

.Lclose_outputs:
 push r13
 xor r13d,r13d
.Lclose_output_loop:
 mov edi,[rbx+PG_P_OUTPUT+r13*4]
 test edi,edi
 js .Lclose_output_next
 mov dword ptr [rbx+PG_P_OUTPUT+r13*4],-1
 call close@PLT
.Lclose_output_next:
 inc r13
 cmp r13,2
 jb .Lclose_output_loop
 pop r13
 ret

.globl pg_process_next
.type pg_process_next,@function
pg_process_next:
 test rdi,rdi
 jz .Lnext_invalid
 test rsi,rsi
 jz .Lnext_invalid
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,152
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsi
 xor eax,eax
 mov ecx,5
 rep stosq
 cmp dword ptr [rbx+PG_P_STATE],1
 jne .Lnext_wait
 call .Lnow
 mov r14,rax
 cmp rax,-1
 jne .Lcheck_timeout
 call __errno_location@PLT
 mov eax,[rax]
 call .Lrecord_error
.Lcheck_timeout:
 cmp dword ptr [rbx+PG_P_REASON],0
 jne .Lrequest_stop
 cmp qword ptr [rbx+PG_P_DEADLINE],0
 je .Lobserve
 cmp r14,[rbx+PG_P_DEADLINE]
 jb .Lobserve
 mov dword ptr [rbx+PG_P_REASON],2
.Lrequest_stop:
 call .Lstop
.Lobserve:
 cmp dword ptr [rbx+PG_P_EXITED],0
 jne .Lread_begin
 mov rdi,rsp
 xor eax,eax
 mov ecx,PG_P_SIGINFO_BYTES/8
 rep stosq
 mov edi,1
 mov esi,[rbx+PG_P_PID]
 mov rdx,rsp
 mov ecx,PG_P_WAITID_FLAGS
 call waitid@PLT
 test eax,eax
 jnz .Lwait_error
 mov eax,[rsp+PG_P_SI_PID]
 test eax,eax
 jz .Lread_begin
 cmp eax,[rbx+PG_P_PID]
 jne .Lbad_wait_info
 mov eax,[rsp+PG_P_SI_STATUS]
 mov ecx,[rsp+PG_P_SI_CODE]
 cmp ecx,1
 je .Lobserved_exit
 cmp ecx,2
 je .Lobserved_signal
 cmp ecx,3
 jne .Lbad_wait_info
.Lobserved_signal:
 mov [rbx+PG_P_SIGNAL],eax
 add eax,128
.Lobserved_exit:
 mov [rbx+PG_P_EXIT_CODE],eax
 mov dword ptr [rbx+PG_P_EXITED],1
 lea rax,[r14+100]
 mov [rbx+PG_P_IDLE],rax
 jmp .Lread_begin
.Lbad_wait_info:
 mov eax,5
 call .Lrecord_error
 call .Lstop
 jmp .Lnext_wait
.Lwait_error:
 call __errno_location@PLT
 mov eax,[rax]
 cmp eax,4
 je .Lnext_wait
 call .Lrecord_error
 cmp eax,10
 je .Lost_child
 call .Lstop
 jmp .Lnext_wait
.Lost_child:
 mov dword ptr [rbx+PG_P_PID],-1
 mov dword ptr [rbx+PG_P_EXITED],1
 mov dword ptr [rbx+PG_P_EXIT_CODE],1
 mov dword ptr [rbx+PG_P_REASON],4
 call .Lclose_outputs
 jmp .Lterminal
.Lread_begin:
 cmp dword ptr [rbx+PG_P_REASON],0
 je .Lread_prepare
 cmp dword ptr [rbx+PG_P_EXITED],0
 je .Lread_prepare
 cmp r14,-1
 je .Lfinish
 cmp r14,[rbx+PG_P_IDLE]
 jae .Lfinish
.Lread_prepare:
 xor r15d,r15d
.Lread_stream:
 mov r13d,[rbx+PG_P_NEXT]
 xor r13d,r15d
 mov edi,[rbx+PG_P_OUTPUT+r13*4]
 test edi,edi
 js .Lread_next
 lea rsi,[rbx+PG_P_BUFFER]
 mov edx,4096
 call read@PLT
 test rax,rax
 js .Lread_error
 jz .Lread_eof
 mov dword ptr [r12],1
 mov [r12+4],r13d
 lea rcx,[rbx+PG_P_BUFFER]
 mov [r12+8],rcx
 mov [r12+16],rax
 xor r13d,1
 mov [rbx+PG_P_NEXT],r13d
 cmp dword ptr [rbx+PG_P_EXITED],0
 je .Lnext_event
 cmp dword ptr [rbx+PG_P_REASON],0
 jne .Lnext_event
 lea rax,[r14+100]
 mov [rbx+PG_P_IDLE],rax
 jmp .Lnext_event
.Lread_error:
 call __errno_location@PLT
 mov eax,[rax]
 cmp eax,11
 je .Lread_next
 cmp eax,4
 je .Lread_next
 call .Lrecord_error
 call .Lstop
.Lread_eof:
 mov edi,[rbx+PG_P_OUTPUT+r13*4]
 mov dword ptr [rbx+PG_P_OUTPUT+r13*4],-1
 call close@PLT
 mov eax,1
 mov ecx,r13d
 shl eax,cl
 or [rbx+PG_P_EOF],eax
.Lread_next:
 inc r15
 cmp r15,2
 jb .Lread_stream
 cmp dword ptr [rbx+PG_P_EXITED],0
 je .Lnext_wait
 cmp qword ptr [rbx+PG_P_OUTPUT],-1
 je .Lfinish
 cmp r14,-1
 je .Lfinish
 cmp r14,[rbx+PG_P_IDLE]
 jb .Lnext_wait
.Lfinish:
 cmp dword ptr [rbx+PG_P_REASON],0
 je .Lfinish_close
 call .Lstop
.Lfinish_close:
 call .Lclose_outputs
 mov edi,[rbx+PG_P_PID]
 lea rsi,[rsp+128]
 mov edx,1
 call waitpid@PLT
 test eax,eax
 jz .Lnext_wait
 js .Lreap_error
 mov dword ptr [rbx+PG_P_PID],-1
 jmp .Lterminal
.Lreap_error:
 call __errno_location@PLT
 mov eax,[rax]
 cmp eax,4
 je .Lnext_wait
 call .Lrecord_error
 cmp eax,10
 jne .Lnext_wait
 mov dword ptr [rbx+PG_P_PID],-1
 mov dword ptr [rbx+PG_P_REASON],4
.Lterminal:
 mov dword ptr [rbx+PG_P_STATE],2
 mov dword ptr [r12],2
 mov eax,[rbx+PG_P_EXIT_CODE]
 mov [r12+24],eax
 mov eax,[rbx+PG_P_SIGNAL]
 mov [r12+28],eax
 mov eax,[rbx+PG_P_REASON]
 mov [r12+32],eax
 mov eax,[rbx+PG_P_ERROR]
 mov [r12+36],eax
.Lnext_event:
 mov eax,1
 jmp .Lnext_return
.Lnext_wait:
 xor eax,eax
.Lnext_return:
 add rsp,152
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lnext_invalid:
 mov eax,-1
 ret
.size pg_process_next,.-pg_process_next

.globl pg_process_close
.type pg_process_close,@function
pg_process_close:
 test rdi,rdi
 jz .Lclose_invalid
 push rbx
 sub rsp,48
 mov rbx,rdi
 cmp dword ptr [rbx+PG_P_STATE],1
 jne .Lclose_reset
 cmp dword ptr [rbx+PG_P_REASON],0
 jne .Lclose_poll
 mov dword ptr [rbx+PG_P_REASON],3
.Lclose_poll:
 mov rdi,rbx
 mov rsi,rsp
 call pg_process_next
 cmp dword ptr [rbx+PG_P_STATE],1
 je .Lclose_pending
.Lclose_reset:
 mov rdi,rbx
 call pg_process_init
 xor eax,eax
 jmp .Lclose_return
.Lclose_pending:
 mov eax,1
.Lclose_return:
 add rsp,48
 pop rbx
 ret
.Lclose_invalid:
 mov eax,-1
 ret
.size pg_process_close,.-pg_process_close
.section .note.GNU-stack,"",@progbits
