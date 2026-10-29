#include "bash_tool_layout.h"
#include "tools_layout.h"
#include "execution_env.h"
.intel_syntax noprefix
.section .rodata
.Lname: .asciz "bash"
.Ldescription: .asciz "Execute a bash command in the configured working directory (not a sandbox). Streams stdout/stderr and returns their UTF-8 tail, up to 3072 bytes/2000 lines. Truncated output has a private raw output file. Capture over 16 MiB cancels with a partial-file error. Optional timeout in seconds, no default. Commands may have side effects and interrupted commands are not automatically replayed."
.Lschema: .asciz "{\"type\":\"object\",\"properties\":{\"command\":{\"type\":\"string\"},\"timeout\":{\"type\":\"number\",\"exclusiveMinimum\":0,\"maximum\":2147483.647}},\"required\":[\"command\"],\"additionalProperties\":false}"
.Lcommand: .asciz "command"
.Ltimeout: .asciz "timeout"
.Ltemplate: .asciz "/tmp/pangu-output-XXXXXX"
.Linvalid: .asciz "Invalid bash arguments or active operation"
.Lprepare_invalid: .asciz "Bash execution preparation rejected the command"
.Lstatus: .asciz "\n[exit=%d; reason=%s]\n"
.Lfull: .asciz "[earlier output omitted; full raw output: %s]\n"
.Lpartial: .asciz "[output capture failed or limit reached; partial raw output: %s]\n"
.Lnormal: .asciz "normal"
.Lcancelled: .asciz "cancelled"
.Ltimedout: .asciz "timeout"
.Lclosed: .asciz "closed"
.Lfailed: .asciz "failed"
.balign 8
.Lthousand: .double 1000.0
.Lmax_seconds: .double 2147483.647
.section .data.rel.ro
.Lreasons: .quad .Lnormal,.Lcancelled,.Ltimedout,.Lclosed,.Lfailed
.Lasync: .quad .Lnext,.Lcancel,.Lclose,0,0
.text

.globl pg_bash_tool_init
.type pg_bash_tool_init,@function
pg_bash_tool_init:
 test rdi,rdi
 jz .Linit_bad
 test rsi,rsi
 jz .Linit_bad
 cmp dword ptr [rsi],PG_EXECUTION_ENV_ABI
 jne .Linit_bad
 cmp dword ptr [rsi+4],PG_EE_BYTES
 jb .Linit_bad
 cmp dword ptr [rsi+PG_EE_DIRECTORY],0
 jl .Linit_bad
 cmp qword ptr [rsi+PG_EE_PROCESS+8],0
 je .Linit_bad
 cmp qword ptr [rsi+PG_EE_PROCESS+16],0
 je .Linit_bad
 cmp qword ptr [rsi+PG_EE_PROCESS+24],0
 je .Linit_bad
 cmp qword ptr [rsi+PG_EE_PROCESS+32],0
 je .Linit_bad
 mov r8,rdi
 mov r9,rsi
 xor eax,eax
 mov ecx,PG_BH_BYTES/8
 rep stosq
 mov rdi,r8
 mov [r8+PG_BH_ENVIRONMENT],r9
 mov dword ptr [r8+PG_BH_FILE],-1
 xor eax,eax
 ret
.Linit_bad:
 mov eax,1
 ret
.size pg_bash_tool_init,.-pg_bash_tool_init

/* Parse arguments without side effects; optional detached command/size/timeout outputs. */
.Lparse:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,80
 mov r12,rdi
 mov r13,rsi
 mov [rsp+24],rdx
 mov [rsp+32],rcx
 mov [rsp+40],r8
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+64],0
 test r12,r12
 jz .Lparse_bad
 test r13,r13
 jz .Lparse_bad
 cmp r13,4096
 jae .Lparse_bad
 mov rdi,r12
 xor esi,esi
 mov rdx,r13
 call memchr@PLT
 test rax,rax
 jnz .Lparse_bad
 xor ecx,ecx
.Lparse_escapes:
 cmp rcx,r13
 jae .Lparse_token_new
 cmp byte ptr [r12+rcx],92
 jne .Lparse_escape_next
 lea rax,[rcx+5]
 cmp rax,r13
 jae .Lparse_escape_skip
 cmp byte ptr [r12+rcx+1],'u'
 jne .Lparse_escape_skip
 cmp dword ptr [r12+rcx+2],0x30303030
 je .Lparse_bad
.Lparse_escape_skip:
 inc rcx
.Lparse_escape_next:
 inc rcx
 jmp .Lparse_escapes
.Lparse_token_new:
 call json_tokener_new@PLT
 test rax,rax
 jz .Lparse_bad
 mov [rsp],rax
 mov rdi,rax
 mov esi,17
 call json_tokener_set_flags@PLT
 mov rdi,[rsp]
 mov rsi,r12
 mov edx,r13d
 call json_tokener_parse_ex@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lparse_bad
 mov rdi,[rsp]
 call json_tokener_get_error@PLT
 test eax,eax
 jnz .Lparse_bad
 mov rdi,[rsp]
 call json_tokener_get_parse_end@PLT
.Lparse_tail:
 cmp rax,r13
 jae .Lparse_object
 movzx ecx,byte ptr [r12+rax]
 cmp ecx,32
 je .Lparse_space
 cmp ecx,9
 je .Lparse_space
 cmp ecx,10
 je .Lparse_space
 cmp ecx,13
 jne .Lparse_bad
.Lparse_space:
 inc rax
 jmp .Lparse_tail
.Lparse_object:
 mov rdi,[rsp+8]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lparse_bad
 mov rdi,[rsp+8]
 call json_object_object_length@PLT
 cmp eax,1
 jb .Lparse_bad
 cmp eax,2
 ja .Lparse_bad
 mov [rsp+72],eax
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lcommand]
 lea rdx,[rsp+16]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lparse_bad
 mov rdi,[rsp+16]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lparse_bad
 mov rdi,[rsp+16]
 call json_object_get_string_len@PLT
 mov r14d,eax
 cmp r14,4096
 jae .Lparse_bad
 mov [rsp+56],r14
 mov rdi,[rsp+16]
 call json_object_get_string@PLT
 mov [rsp+48],rax
 mov rdi,rax
 xor esi,esi
 mov rdx,r14
 call memchr@PLT
 test rax,rax
 jnz .Lparse_bad
 mov rdi,[rsp+8]
 lea rsi,[rip+.Ltimeout]
 lea rdx,[rsp+16]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lparse_no_timeout
 cmp dword ptr [rsp+72],2
 jne .Lparse_bad
 mov rdi,[rsp+16]
 call json_object_get_type@PLT
 cmp eax,2
 je .Lparse_number
 cmp eax,3
 jne .Lparse_bad
.Lparse_number:
 mov rdi,[rsp+16]
 call json_object_get_double@PLT
 pxor xmm1,xmm1
 ucomisd xmm0,xmm1
 jbe .Lparse_bad
 jp .Lparse_bad
 ucomisd xmm0,[rip+.Lmax_seconds]
 ja .Lparse_bad
 mulsd xmm0,[rip+.Lthousand]
 cvttsd2si rax,xmm0
 test rax,rax
 jnz .Lparse_store_timeout
 mov eax,1
.Lparse_store_timeout:
 mov [rsp+64],rax
 jmp .Lparse_copy
.Lparse_no_timeout:
 cmp dword ptr [rsp+72],1
 jne .Lparse_bad
.Lparse_copy:
 mov rdi,[rsp+24]
 test rdi,rdi
 jz .Lparse_size
 mov rsi,[rsp+48]
 mov rdx,[rsp+56]
 inc rdx
 call memcpy@PLT
.Lparse_size:
 mov rdi,[rsp+32]
 test rdi,rdi
 jz .Lparse_time
 mov rax,[rsp+56]
 mov [rdi],rax
.Lparse_time:
 mov rdi,[rsp+40]
 test rdi,rdi
 jz .Lparse_ok
 mov rax,[rsp+64]
 mov [rdi],rax
.Lparse_ok:
 xor ebx,ebx
 jmp .Lparse_cleanup
.Lparse_bad:
 mov ebx,1
.Lparse_cleanup:
 mov rdi,[rsp+8]
 test rdi,rdi
 jz .Lparse_token
 call json_object_put@PLT
.Lparse_token:
 mov rdi,[rsp]
 test rdi,rdi
 jz .Lparse_return
 call json_tokener_free@PLT
.Lparse_return:
 mov eax,ebx
 add rsp,80
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Lvalidate:
 mov rdi,rsi
 mov rsi,rdx
 xor edx,edx
 xor ecx,ecx
 xor r8d,r8d
 sub rsp,8
 call .Lparse
 xor eax,1
 add rsp,8
 ret
.Lauthorize:
 xor eax,eax
 test rdi,rdi
 jz .Lauth_ret
 mov rdx,[rdi+PG_BH_ENVIRONMENT]
 test rdx,rdx
 jz .Lauth_ret
 cmp dword ptr [rdx+PG_EE_DIRECTORY],0
 jl .Lauth_ret
 mov eax,1
.Lauth_ret:
 ret

.Lexecute:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,4160
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov r15,r8
 mov [rsp+4152],r9
 test r9,r9
 jz .Lexecute_bad_direct
 mov qword ptr [r9],0
 test rbx,rbx
 jz .Lexecute_bad_direct
 test r14,r14
 jz .Lexecute_bad_direct
 cmp r15,4096
 jb .Lexecute_bad_direct
 cmp dword ptr [rbx+PG_BH_STAGE],0
 je .Lexecute_parse
 cmp dword ptr [rbx+PG_BH_STAGE],6
 jne .Lexecute_invalid
.Lexecute_parse:
 mov rdi,r12
 mov rsi,r13
 mov rdx,rsp
 lea rcx,[rsp+4096]
 lea r8,[rsp+4104]
 call .Lparse
 test eax,eax
 jnz .Lexecute_invalid
 mov rax,[rbx+PG_BH_ENVIRONMENT]
 mov [rsp+4112],rsp
 mov rcx,[rsp+4096]
 mov [rsp+4120],rcx
 mov ecx,[rax+PG_EE_DIRECTORY]
 mov [rsp+4128],ecx
 mov qword ptr [rsp+4132],0
 mov rcx,[rax+PG_EE_ENVIRONMENT]
 mov [rsp+4136],rcx
 mov r9,[rax+PG_EE_PREPARE]
 test r9,r9
 jz .Lprepare_ready
 mov rdi,[rax+PG_EE_CONTEXT]
 lea rsi,[rsp+4112]
 call r9
 test eax,eax
 jnz .Lexecute_prepare_invalid
.Lprepare_ready:
 mov rax,[rsp+4120]
 cmp rax,4095
 ja .Lexecute_invalid
 mov rcx,[rsp+4112]
 test rcx,rcx
 jnz .Lprepare_command_present
 test rax,rax
 jnz .Lexecute_invalid
.Lprepare_command_present:
 test rax,rax
 jz .Lprepare_command_checked
 mov rdi,rcx
 xor esi,esi
 mov rdx,rax
 call memchr@PLT
 test rax,rax
 jnz .Lexecute_invalid
.Lprepare_command_checked:
 cmp dword ptr [rsp+4128],0
 jl .Lexecute_invalid
 mov rax,[rsp+4112]
 mov [rsp+4144],rax
 mov rax,[rsp+4120]
 mov [rsp+4096],rax
 movsxd rax,dword ptr [rsp+4128]
 mov [rsp+4112],rax
 mov qword ptr [rsp+4120],0
 mov rax,[rsp+4136]
 mov [rsp+4128],rax
 mov rax,[rsp+4104]
 mov [rsp+4136],rax
 lea rdi,[rbx+PG_BH_FILE]
 xor esi,esi
 mov edx,PG_BH_BYTES-PG_BH_FILE
 call memset@PLT
 mov dword ptr [rbx+PG_BH_FILE],-1
 mov dword ptr [rbx+PG_BH_STAGE],1
 lea rdi,[rbx+PG_BH_DECODER]
 call pg_stream_decoder_init@PLT
 lea rdi,[rbx+PG_BH_DECODER+28]
 call pg_stream_decoder_init@PLT
 lea rdi,[rbx+PG_BH_PATH]
 lea rsi,[rip+.Ltemplate]
 call strcpy@PLT
 lea rdi,[rbx+PG_BH_PATH]
 mov esi,524288
 call mkostemp@PLT
 mov [rbx+PG_BH_FILE],eax
 test eax,eax
 js .Lexecute_temp_failed
 mov rax,[rbx+PG_BH_ENVIRONMENT]
 mov rdi,[rax+PG_EE_PROCESS]
 lea rsi,[rsp+4112]
 mov rdx,[rsp+4144]
 mov rcx,[rsp+4096]
 call [rax+PG_EE_PROCESS+8]
 test eax,eax
 jnz .Lexecute_failed
 mov eax,2
 jmp .Lexecute_return
.Lexecute_temp_failed:
 mov byte ptr [rbx+PG_BH_PATH],0
.Lexecute_failed:
 mov dword ptr [rbx+PG_BH_STAGE],4
 mov dword ptr [rbx+PG_BH_FLAGS],2
 mov dword ptr [rbx+PG_BH_REASON],4
 mov dword ptr [rbx+PG_BH_EXIT],1
 mov eax,2
 jmp .Lexecute_return
.Lexecute_invalid:
 mov rdi,r14
 lea rsi,[rip+.Linvalid]
 call strcpy@PLT
 lea rdi,[rip+.Linvalid]
 call strlen@PLT
 mov rdx,[rsp+4152]
 mov [rdx],rax
.Lexecute_bad_direct:
 mov eax,1
 jmp .Lexecute_return
.Lexecute_prepare_invalid:
 mov rdi,r14
 lea rsi,[rip+.Lprepare_invalid]
 call strcpy@PLT
 lea rdi,[rip+.Lprepare_invalid]
 call strlen@PLT
 mov rdx,[rsp+4152]
 mov [rdx],rax
 mov eax,1
.Lexecute_return:
 add rsp,4160
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret

/* Retain decoded tail, then trim a split scalar and excess lines. Context in rbx. */
.Lretain:
 push r12
 push r13
 push r14
 mov r12,[rbx+PG_BH_PENDING_N]
 mov r13,[rbx+PG_BH_TAIL_N]
 cmp r12,PG_BH_RETAIN
 jb .Lretain_append
 lea rax,[r12+r13]
 cmp rax,PG_BH_RETAIN
 jbe .Lretain_large
 or dword ptr [rbx+PG_BH_FLAGS],1
.Lretain_large:
 lea rsi,[rbx+PG_BH_PENDING+r12-PG_BH_RETAIN]
 lea rdi,[rbx+PG_BH_TAIL]
 mov edx,PG_BH_RETAIN
 call memcpy@PLT
 mov r13d,PG_BH_RETAIN
 jmp .Lretain_trim
.Lretain_append:
 lea r14,[r12+r13]
 cmp r14,PG_BH_RETAIN
 jbe .Lretain_copy
 or dword ptr [rbx+PG_BH_FLAGS],1
 sub r14,PG_BH_RETAIN
 sub r13,r14
 lea rdi,[rbx+PG_BH_TAIL]
 lea rsi,[rdi+r14]
 mov rdx,r13
 call memmove@PLT
.Lretain_copy:
 lea rdi,[rbx+PG_BH_TAIL+r13]
 lea rsi,[rbx+PG_BH_PENDING]
 mov rdx,r12
 call memcpy@PLT
 add r13,r12
.Lretain_trim:
 xor r14d,r14d
.Lretain_cont:
 cmp r14,r13
 jae .Lretain_drop
 mov al,[rbx+PG_BH_TAIL+r14]
 and al,0xc0
 cmp al,0x80
 jne .Lretain_drop
 inc r14
 jmp .Lretain_cont
.Lretain_drop:
 test r14,r14
 jz .Lretain_lines
 or dword ptr [rbx+PG_BH_FLAGS],1
 sub r13,r14
 lea rdi,[rbx+PG_BH_TAIL]
 lea rsi,[rdi+r14]
 mov rdx,r13
 call memmove@PLT
.Lretain_lines:
 mov [rbx+PG_BH_TAIL_N],r13
 test r13,r13
 jz .Lretain_return
 xor edx,edx
 cmp byte ptr [rbx+PG_BH_TAIL+r13-1],10
 sete dl
 xor edx,1
 mov rcx,r13
.Lretain_line_scan:
 test rcx,rcx
 jz .Lretain_return
 dec rcx
 cmp byte ptr [rbx+PG_BH_TAIL+rcx],10
 jne .Lretain_line_scan
 inc edx
 cmp edx,2000
 jbe .Lretain_line_scan
 inc rcx
 sub r13,rcx
 mov [rbx+PG_BH_TAIL_N],r13
 or dword ptr [rbx+PG_BH_FLAGS],1
 lea rdi,[rbx+PG_BH_TAIL]
 lea rsi,[rdi+rcx]
 mov rdx,r13
 call memmove@PLT
.Lretain_return:
 pop r14
 pop r13
 pop r12
 ret

/* Raw spill; limited to one process chunk, retry only short writes/EINTR. */
.Lwrite_raw:
 push r12
 push r13
 push r14
 mov r12,rsi
 mov r13,rdx
 test dword ptr [rbx+PG_BH_FLAGS],2
 jnz .Lwrite_return
 mov eax,PG_BH_LIMIT
 sub rax,[rbx+PG_BH_CAPTURED]
 cmp r13,rax
 jbe .Lwrite_loop
 mov r13,rax
 or dword ptr [rbx+PG_BH_FLAGS],18
.Lwrite_loop:
 test r13,r13
 jz .Lwrite_check
 mov edi,[rbx+PG_BH_FILE]
 mov rsi,r12
 mov rdx,r13
 call write@PLT
 test rax,rax
 jg .Lwrite_progress
 jz .Lwrite_fail
 call __errno_location@PLT
 cmp dword ptr [rax],4
 je .Lwrite_loop
 jmp .Lwrite_fail
.Lwrite_progress:
 add [rbx+PG_BH_CAPTURED],rax
 add r12,rax
 sub r13,rax
 jmp .Lwrite_loop
.Lwrite_fail:
 or dword ptr [rbx+PG_BH_FLAGS],2
.Lwrite_check:
 test dword ptr [rbx+PG_BH_FLAGS],2
 jz .Lwrite_return
 mov rdi,rbx
 call .Lcancel
.Lwrite_return:
 pop r14
 pop r13
 pop r12
 ret

.Lnext:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,64
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 test r14,r14
 jz .Lnext_invalid
 mov qword ptr [r14],0
 test rbx,rbx
 jz .Lnext_invalid
 test r12,r12
 jz .Lnext_invalid
 cmp r13,4096
 jb .Lnext_invalid
 mov rax,[rbx+PG_BH_POS]
 cmp rax,[rbx+PG_BH_PENDING_N]
 jb .Lemit
 mov qword ptr [rbx+PG_BH_POS],0
 mov qword ptr [rbx+PG_BH_PENDING_N],0
 mov eax,[rbx+PG_BH_STAGE]
 cmp eax,1
 je .Lpoll
 cmp eax,2
 je .Lflush
 cmp eax,3
 je .Lflush
 cmp eax,4
 je .Lfinal
 jmp .Lnext_invalid
.Lpoll:
 mov rax,[rbx+PG_BH_ENVIRONMENT]
 mov rdi,[rax+PG_EE_PROCESS]
 mov rsi,rsp
 call [rax+PG_EE_PROCESS+16]
 test eax,eax
 jz .Lwait
 js .Lprovider_error
 cmp eax,1
 jne .Lprovider_error
 cmp dword ptr [rsp],1
 je .Loutput
 cmp dword ptr [rsp],2
 jne .Lprovider_error
 mov eax,[rsp+24]
 mov [rbx+PG_BH_EXIT],eax
 mov eax,[rsp+32]
 cmp eax,4
 ja .Lprovider_error
 mov [rbx+PG_BH_REASON],eax
 mov dword ptr [rbx+PG_BH_STAGE],2
 jmp .Lwait
.Loutput:
 cmp dword ptr [rsp+4],1
 ja .Lprovider_error
 cmp qword ptr [rsp+16],4096
 ja .Lprovider_error
 cmp qword ptr [rsp+16],0
 je .Lwait
 cmp qword ptr [rsp+8],0
 je .Lprovider_error
 mov rsi,[rsp+8]
 mov rdx,[rsp+16]
 call .Lwrite_raw
 mov eax,[rsp+4]
 imul eax,eax,28
 lea rdi,[rbx+PG_BH_DECODER+rax]
 mov rsi,[rsp+8]
 mov rdx,[rsp+16]
 xor ecx,ecx
 jmp .Ldecode
.Lflush:
 sub eax,2
 imul eax,eax,28
 lea rdi,[rbx+PG_BH_DECODER+rax]
 xor esi,esi
 xor edx,edx
 mov ecx,1
 inc dword ptr [rbx+PG_BH_STAGE]
.Ldecode:
 lea r8,[rbx+PG_BH_PENDING]
 mov r9d,12296
 lea rax,[rbx+PG_BH_PENDING_N]
 sub rsp,16
 mov [rsp],rax
 call pg_stream_decode@PLT
 add rsp,16
 test eax,eax
 jnz .Lprovider_error
 call .Lretain
 cmp qword ptr [rbx+PG_BH_PENDING_N],0
 je .Lwait
.Lemit:
 mov r15,[rbx+PG_BH_PENDING_N]
 sub r15,[rbx+PG_BH_POS]
 lea rax,[r13-1]
 cmp r15,rax
 cmova r15,rax
 lea rsi,[rbx+PG_BH_PENDING]
 add rsi,[rbx+PG_BH_POS]
 mov rdi,r12
 mov rdx,r15
 call memcpy@PLT
 add [rbx+PG_BH_POS],r15
 mov [r14],r15
 mov eax,3
 jmp .Lnext_return
.Lprovider_error:
 or dword ptr [rbx+PG_BH_FLAGS],2
 mov dword ptr [rbx+PG_BH_REASON],4
 mov dword ptr [rbx+PG_BH_EXIT],1
 mov dword ptr [rbx+PG_BH_STAGE],2
 mov rdi,rbx
 call .Lcancel
.Lwait:
 mov eax,2
 jmp .Lnext_return
.Lfinal:
 call .Lfinish_file
 xor r15d,r15d
 mov rdi,r12
 mov rsi,r13
 lea rdx,[rip+.Lstatus]
 mov ecx,[rbx+PG_BH_EXIT]
 mov eax,[rbx+PG_BH_REASON]
 lea r8,[rip+.Lreasons]
 mov r8,[r8+rax*8]
 xor eax,eax
 call snprintf@PLT
 test eax,eax
 js .Lnext_invalid
 add r15,rax
 cmp byte ptr [rbx+PG_BH_PATH],0
 je .Lfinal_done
 test dword ptr [rbx+PG_BH_FLAGS],3
 jz .Lfinal_done
 lea rdx,[rip+.Lfull]
 test dword ptr [rbx+PG_BH_FLAGS],2
 jz .Lfinal_path
 lea rdx,[rip+.Lpartial]
.Lfinal_path:
 lea rdi,[r12+r15]
 mov rsi,r13
 sub rsi,r15
 lea rcx,[rbx+PG_BH_PATH]
 xor eax,eax
 call snprintf@PLT
 test eax,eax
 js .Lnext_invalid
 add r15,rax
.Lfinal_done:
 mov rdx,[rbx+PG_BH_TAIL_N]
 lea rax,[r15+rdx]
 cmp rax,r13
 jae .Lnext_invalid
 lea rdi,[r12+r15]
 lea rsi,[rbx+PG_BH_TAIL]
 call memcpy@PLT
 add r15,[rbx+PG_BH_TAIL_N]
 mov byte ptr [r12+r15],0
 mov [r14],r15
 mov dword ptr [rbx+PG_BH_STAGE],5
 xor eax,eax
 cmp dword ptr [rbx+PG_BH_EXIT],0
 jne .Lfinal_error
 cmp dword ptr [rbx+PG_BH_REASON],0
 jne .Lfinal_error
 test dword ptr [rbx+PG_BH_FLAGS],2
 jnz .Lfinal_error
 jmp .Lnext_return
.Lfinal_error:
 mov eax,1
 jmp .Lnext_return
.Lnext_invalid:
 mov eax,1
.Lnext_return:
 add rsp,64
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret

/* Finish the output artifact before publishing its result. */
.Lfinish_file:
 sub rsp,8
 mov edi,[rbx+PG_BH_FILE]
 test edi,edi
 js .Lfile_unlink
 mov dword ptr [rbx+PG_BH_FILE],-1
 call close@PLT
 test eax,eax
 jz .Lfile_unlink
 or dword ptr [rbx+PG_BH_FLAGS],2
.Lfile_unlink:
 cmp qword ptr [rbx+PG_BH_CAPTURED],0
 je .Lfile_remove
 test dword ptr [rbx+PG_BH_FLAGS],3
 jnz .Lfile_return
.Lfile_remove:
 cmp byte ptr [rbx+PG_BH_PATH],0
 je .Lfile_return
 lea rdi,[rbx+PG_BH_PATH]
 call unlink@PLT
 test eax,eax
 jz .Lfile_cleared
 call __errno_location@PLT
 cmp dword ptr [rax],2
 je .Lfile_cleared
 or dword ptr [rbx+PG_BH_FLAGS],2
 jmp .Lfile_return
.Lfile_cleared:
 mov byte ptr [rbx+PG_BH_PATH],0
.Lfile_return:
 add rsp,8
 ret
.Lcancel:
 test rdi,rdi
 jz .Lcancel_return
 mov rdx,[rdi+PG_BH_ENVIRONMENT]
 mov rax,[rdx+PG_EE_PROCESS+24]
 mov rdi,[rdx+PG_EE_PROCESS]
 jmp rax
.Lcancel_return:
 ret
.Lclose:
 push rbx
 mov rbx,rdi
 test rbx,rbx
 jz .Lclose_ok
 mov rax,[rbx+PG_BH_ENVIRONMENT]
 mov rdi,[rax+PG_EE_PROCESS]
 call [rax+PG_EE_PROCESS+32]
 test eax,eax
 jnz .Lclose_return
 cmp dword ptr [rbx+PG_BH_STAGE],5
 je .Lclose_released
 cmp dword ptr [rbx+PG_BH_STAGE],6
 je .Lclose_ok
 mov edi,[rbx+PG_BH_FILE]
 test edi,edi
 js .Lclose_unlink
 mov dword ptr [rbx+PG_BH_FILE],-1
 call close@PLT
.Lclose_unlink:
 cmp byte ptr [rbx+PG_BH_PATH],0
 je .Lclose_released
 lea rdi,[rbx+PG_BH_PATH]
 call unlink@PLT
 test eax,eax
 jz .Lclose_clear
 call __errno_location@PLT
 cmp dword ptr [rax],2
 jne .Lclose_error
.Lclose_clear:
 mov byte ptr [rbx+PG_BH_PATH],0
.Lclose_released:
 mov dword ptr [rbx+PG_BH_STAGE],6
.Lclose_ok:
 xor eax,eax
 jmp .Lclose_return
.Lclose_error:
 mov eax,-1
.Lclose_return:
 pop rbx
 ret

.globl pg_bash_tool_definition
.type pg_bash_tool_definition,@function
pg_bash_tool_definition:
 lea rax,[rip+.Lname]
 mov [rdi+PG_TL_DEF_NAME],rax
 lea rax,[rip+.Ldescription]
 mov [rdi+PG_TL_DEF_DESCRIPTION],rax
 lea rax,[rip+.Lschema]
 mov [rdi+PG_TL_DEF_SCHEMA],rax
 mov [rdi+PG_TL_DEF_CONTEXT],rsi
 lea rax,[rip+.Lvalidate]
 mov [rdi+PG_TL_DEF_VALIDATE],rax
 lea rax,[rip+.Lauthorize]
 mov [rdi+PG_TL_DEF_AUTHORIZE],rax
 lea rax,[rip+.Lexecute]
 mov [rdi+PG_TL_DEF_EXECUTE],rax
 mov qword ptr [rdi+PG_TL_DEF_REPLAY_SAFE],0
 lea rax,[rip+.Lasync]
 mov [rdi+PG_TL_DEF_ASYNC],rax
 lea rax,[rip+.Lbash_output_limits]
 mov [rdi+PG_TL_DEF_OUTPUT_LIMITS],rax
 mov rax,rdi
 ret
.size pg_bash_tool_definition,.-pg_bash_tool_definition
.section .note.GNU-stack,"",@progbits
.text
.globl pg_bash_tool_snapshot
.type pg_bash_tool_snapshot,@function
pg_bash_tool_snapshot:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,128
 mov rbx,rdi
 mov r13,rsi
 xor r12d,r12d
 mov ebp,1
 test rsi,rsi
 jz .Lbhs_end
 mov qword ptr [rsi],0
 test rdi,rdi
 jz .Lbhs_end
 cmp dword ptr [rdi+PG_BH_STAGE],5
 jne .Lbhs_end
 mov ebp,5
 lea rdi,[rip+.Lbhs_json]
 call json_tokener_parse@PLT
 mov r12,rax
 test rax,rax
 jz .Lbhs_end
 mov rdi,rax
 lea rsi,[rip+.Lbhs_structured]
 call json_object_object_get@PLT
 mov r14,rax
 lea rdi,[rbx+PG_BH_TAIL]
 mov esi,[rbx+PG_BH_TAIL_N]
 call json_object_new_string_len@PLT
 mov rdi,r14
 lea rsi,[rip+.Lbhs_output]
 mov rdx,rax
 call .Lbhs_set
 test eax,eax
 jnz .Lbhs_end
 mov edi,[rbx+PG_BH_FLAGS]
 and edi,1
 call json_object_new_boolean@PLT
 mov rdi,r14
 lea rsi,[rip+.Lbhs_truncated]
 mov rdx,rax
 call .Lbhs_set
 test eax,eax
 jnz .Lbhs_end
 mov edi,[rbx+PG_BH_EXIT]
 call json_object_new_int@PLT
 mov rdi,r14
 lea rsi,[rip+.Lbhs_exit]
 mov rdx,rax
 call .Lbhs_set
 test eax,eax
 jnz .Lbhs_end
 cmp byte ptr [rbx+PG_BH_PATH],0
 je .Lbhs_error
 lea rdi,[rbx+PG_BH_PATH]
 call json_object_new_string@PLT
 mov rdi,r14
 lea rsi,[rip+.Lbhs_path]
 mov rdx,rax
 call .Lbhs_set
 test eax,eax
 jnz .Lbhs_end
 mov rdi,rsp
 mov esi,128
 lea rdx,[rip+.Lbhs_full_format]
 test dword ptr [rbx+PG_BH_FLAGS],2
 jz .Lbhs_path_message
 lea rdx,[rip+.Lbhs_partial_format]
.Lbhs_path_message:
 lea rcx,[rbx+PG_BH_PATH]
 xor eax,eax
 call snprintf@PLT
 mov rdi,r12
 lea rsi,[rip+.Lbhs_full_code]
 mov rdx,rsp
 lea rcx,[rip+.Lbhs_info]
 call .Lbhs_diagnostic
 test eax,eax
 jnz .Lbhs_end
.Lbhs_error:
 mov eax,[rbx+PG_BH_REASON]
 test eax,eax
 jnz .Lbhs_reason
 test dword ptr [rbx+PG_BH_FLAGS],2
 jnz .Lbhs_capture_error
 cmp dword ptr [rbx+PG_BH_EXIT],0
 je .Lbhs_truncation
 mov rdi,rsp
 mov esi,128
 lea rdx,[rip+.Lbhs_exit_format]
 mov ecx,[rbx+PG_BH_EXIT]
 xor eax,eax
 call snprintf@PLT
 lea rsi,[rip+.Lbhs_exit_code]
 mov rdx,rsp
 jmp .Lbhs_report_error
.Lbhs_reason:
 lea rsi,[rip+.Lbhs_aborted_code]
 lea rdx,[rip+.Lbhs_aborted_message]
 cmp eax,1
 je .Lbhs_report_error
 cmp eax,3
 je .Lbhs_report_error
 lea rsi,[rip+.Lbhs_timeout_code]
 lea rdx,[rip+.Lbhs_timeout_message]
 cmp eax,2
 je .Lbhs_report_error
.Lbhs_capture_error:
 lea rsi,[rip+.Lbhs_capture_code]
 lea rdx,[rip+.Lbhs_capture_message]
.Lbhs_report_error:
 mov rdi,r12
 lea rcx,[rip+.Lbhs_error_severity]
 call .Lbhs_diagnostic
 test eax,eax
 jnz .Lbhs_end
.Lbhs_truncation:
 test dword ptr [rbx+PG_BH_FLAGS],1
 jz .Lbhs_success
 mov rdi,r12
 lea rsi,[rip+.Lbhs_truncated_code]
 lea rdx,[rip+.Lbhs_truncated_message]
 lea rcx,[rip+.Lbhs_warn]
 call .Lbhs_diagnostic
 test eax,eax
 jnz .Lbhs_end
.Lbhs_success:
 mov [r13],r12
 xor r12d,r12d
 xor ebp,ebp
.Lbhs_end:
 mov rdi,r12
 call json_object_put@PLT
 mov eax,ebp
 add rsp,128
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_bash_tool_snapshot,.-pg_bash_tool_snapshot
.Lbhs_set:
 test rdx,rdx
 jz .Lbhs_set_bad
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Lbhs_set_end
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,5
.Lbhs_set_end:
 pop rbx
 ret
.Lbhs_set_bad:
 mov eax,5
 ret
/* root,code,message,severity; temporary diagnostic transfers only on add. */
.Lbhs_diagnostic:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 call json_object_new_object@PLT
 mov r15,rax
 test rax,rax
 jz .Lbhs_diagnostic_bad
 mov rdi,r12
 call json_object_new_string@PLT
 mov rdi,r15
 lea rsi,[rip+.Lbhs_code]
 mov rdx,rax
 call .Lbhs_set
 test eax,eax
 jnz .Lbhs_diagnostic_bad
 mov rdi,r13
 call json_object_new_string@PLT
 mov rdi,r15
 lea rsi,[rip+.Lbhs_message]
 mov rdx,rax
 call .Lbhs_set
 test eax,eax
 jnz .Lbhs_diagnostic_bad
 mov rdi,r14
 call json_object_new_string@PLT
 mov rdi,r15
 lea rsi,[rip+.Lbhs_severity]
 mov rdx,rax
 call .Lbhs_set
 test eax,eax
 jnz .Lbhs_diagnostic_bad
 mov rdi,rbx
 lea rsi,[rip+.Lbhs_diagnostics]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,r15
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lbhs_diagnostic_bad
 xor r15d,r15d
 xor ebx,ebx
 jmp .Lbhs_diagnostic_end
.Lbhs_diagnostic_bad:
 mov ebx,5
.Lbhs_diagnostic_end:
 mov rdi,r15
 call json_object_put@PLT
 mov eax,ebx
 add rsp,16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.section .rodata
.Lbhs_json: .asciz "{\"structuredOutput\":{\"output\":\"\",\"truncated\":false,\"exitCode\":0},\"diagnostics\":[]}"
.Lbhs_structured: .asciz "structuredOutput"
.Lbhs_output: .asciz "output"
.Lbhs_truncated: .asciz "truncated"
.Lbhs_exit: .asciz "exitCode"
.Lbhs_path: .asciz "fullOutputPath"
.Lbhs_diagnostics: .asciz "diagnostics"
.Lbhs_code: .asciz "code"
.Lbhs_message: .asciz "message"
.Lbhs_severity: .asciz "severity"
.Lbhs_info: .asciz "info"
.Lbhs_warn: .asciz "warn"
.Lbhs_error_severity: .asciz "error"
.Lbhs_full_code: .asciz "full_output"
.Lbhs_full_format: .asciz "Full output: %s"
.Lbhs_partial_format: .asciz "Partial output: %s"
.Lbhs_exit_code: .asciz "exit_code"
.Lbhs_exit_format: .asciz "Command exited with code %d"
.Lbhs_timeout_code: .asciz "timeout"
.Lbhs_timeout_message: .asciz "Command timed out"
.Lbhs_aborted_code: .asciz "aborted"
.Lbhs_aborted_message: .asciz "Command aborted"
.Lbhs_capture_code: .asciz "output_error"
.Lbhs_capture_message: .asciz "Command output capture failed or reached its limit"
.Lbhs_truncated_code: .asciz "output_truncated"
.Lbhs_truncated_message: .asciz "Output truncated to the retained tail"

.section .data.rel.ro
.balign 8
.Lbash_output_limits: .quad 51200,2000
 .long 1,0
 .quad .Lbash_structured_schema
.section .rodata
.Lbash_structured_schema: .asciz "{\"type\":\"object\",\"properties\":{\"output\":{\"type\":\"string\"},\"truncated\":{\"type\":\"boolean\"},\"fullOutputPath\":{\"type\":\"string\"},\"exitCode\":{\"type\":\"number\"}},\"required\":[\"output\",\"truncated\",\"exitCode\"]}"
