.intel_syntax noprefix
.macro CGET object,key
 mov rdi,\object
 lea rsi,[rip+.Ldc_\key]
 call json_object_object_get@PLT
.endm
.text
/* Consume the new JSON value on either insertion result. */
.Ldc_set_owned:
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Ldc_set_return
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,1
.Ldc_set_return:
 pop rbx
 ret
.globl pg_durable_client_emit
.type pg_durable_client_emit,@function
pg_durable_client_emit:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rdi
 mov r12,rsi
 mov eax,1
 test rdi,rdi
 jz .Ldc_emit_end
 test rsi,rsi
 jz .Ldc_emit_end
 cmp dword ptr [rdi+68],0
 jne .Ldc_emit_end
 cmp qword ptr [rsi+16],4096
 jae .Ldc_emit_fail
 cmp qword ptr [rsi+32],64
 jae .Ldc_emit_fail
 cmp qword ptr [rsi+48],64
 jae .Ldc_emit_fail
 mov edi,1
 mov esi,4288
 call calloc@PLT
 test rax,rax
 jz .Ldc_emit_fail
 mov r13,rax
 lea rdi,[rax+8]
 mov rsi,r12
 mov edx,56
 call memcpy@PLT
 xor r14d,r14d
.Ldc_emit_slice:
 mov rax,r14
 shl rax,4
 mov rdx,[r12+rax+16]
 test rdx,rdx
 jz .Ldc_emit_next
 mov rsi,[r12+rax+8]
 test rsi,rsi
 jz .Ldc_emit_free
 lea rbp,[r13+64]
 test r14,r14
 jz .Ldc_emit_copy
 lea rbp,[r13+4160]
 cmp r14,1
 je .Ldc_emit_copy
 lea rbp,[r13+4224]
.Ldc_emit_copy:
 mov rdi,rbp
 call memcpy@PLT
 mov rax,r14
 shl rax,4
 mov [r13+rax+16],rbp
.Ldc_emit_next:
 inc r14
 cmp r14,3
 jb .Ldc_emit_slice
 mov rax,[rbx+40]
 test rax,rax
 jz .Ldc_emit_first
 mov [rax],r13
 jmp .Ldc_emit_tail
.Ldc_emit_first:
 mov [rbx+32],r13
.Ldc_emit_tail:
 mov [rbx+40],r13
 xor eax,eax
 jmp .Ldc_emit_end
.Ldc_emit_free:
 mov rdi,r13
 call free@PLT
.Ldc_emit_fail:
 mov dword ptr [rbx+72],1
 mov eax,1
.Ldc_emit_end:
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_durable_client_emit,.-pg_durable_client_emit
.Ldc_boundary:
 push rbx
 sub rsp,64
 mov rbx,rdi
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 mov qword ptr [rsp+48],0
 mov [rsp],esi
 mov [rsp+4],edx
 cmp esi,4
 jne .Ldc_boundary_emit
 cmp edx,1
 jne .Ldc_boundary_emit
 mov rdi,[rbx+96]
 test rdi,rdi
 jz .Ldc_boundary_emit
 call json_object_get_string@PLT
 mov [rsp+8],rax
 mov rdi,[rbx+96]
 call json_object_get_string_len@PLT
 mov [rsp+16],rax
.Ldc_boundary_emit:
 mov rdi,rbx
 mov rsi,rsp
 call pg_durable_client_emit@PLT
 add rsp,64
 pop rbx
 ret
.globl pg_durable_client_open
.type pg_durable_client_open,@function
pg_durable_client_open:
 push rbx
 mov rbx,rdi
 mov eax,1
 test rdi,rdi
 jz .Ldc_open_end
 test rsi,rsi
 jz .Ldc_open_end
 mov rax,[rsi]
 test rax,rax
 jz .Ldc_open_bad
 cmp dword ptr [rax+172],1
 jne .Ldc_open_bad
 mov rax,[rsi+8]
 test rax,rax
 jz .Ldc_open_bad
 cmp dword ptr [rax+60],1
 jne .Ldc_open_bad
 mov ecx,16
.Ldc_open_validate:
 cmp qword ptr [rsi+rcx],0
 je .Ldc_open_bad
 add rcx,8
 cmp ecx,32
 jb .Ldc_open_validate
 mov rax,[rsi+16]
 mov rdx,[rax]
 cmp rdx,[rsi]
 jne .Ldc_open_bad
 mov rdx,[rsi+8]
 mov rdx,[rdx+8]
 cmp rdx,[rax+8]
 jne .Ldc_open_bad
 mov ecx,4
 cld
 rep movsq
 pxor xmm0,xmm0
 movdqu [rbx+32],xmm0
 movdqu [rbx+48],xmm0
 movdqu [rbx+64],xmm0
 mov qword ptr [rbx+80],0
 mov qword ptr [rbx+96],0
 call json_object_new_object@PLT
 mov [rbx+88],rax
 test rax,rax
 jz .Ldc_open_bad
 mov rax,[rbx+8]
 cmp qword ptr [rax+24],0
 je .Ldc_open_ok
 mov dword ptr [rbx+60],1
 mov rdi,rbx
 xor esi,esi
 xor edx,edx
 call .Ldc_boundary
 jmp .Ldc_open_end
.Ldc_open_ok:
 xor eax,eax
 jmp .Ldc_open_end
.Ldc_open_bad:
 mov eax,1
.Ldc_open_end:
 pop rbx
 ret
.size pg_durable_client_open,.-pg_durable_client_open
.Ldc_submit:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 mov ebp,1
 cmp qword ptr [rbx+16],0
 je .Ldc_submit_end
 cmp dword ptr [rbx+56],0
 jne .Ldc_submit_end
 cmp dword ptr [rbx+60],0
 jne .Ldc_submit_end
 cmp dword ptr [rbx+68],0
 jne .Ldc_submit_end
 cmp dword ptr [rbx+72],0
 jne .Ldc_submit_end
 test r12,r12
 jz .Ldc_submit_end
 test r13,r13
 jz .Ldc_submit_end
 cmp r13,4096
 jae .Ldc_submit_end
 mov dword ptr [rbx+56],1
 lea rdi,[rip+.Ldc_input_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Ldc_submit_unlock
 mov rdi,r12
 mov esi,r13d
 call json_object_new_string_len@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Ldc_content]
 mov rdx,rax
 call .Ldc_set_owned
 test eax,eax
 jnz .Ldc_submit_unlock
 mov rdi,[rbx+16]
 mov rsi,[rbx+24]
 mov rdx,[rsp]
 lea rcx,[rsp+8]
 call pg_durable_inputs_submit@PLT
 test eax,eax
 jnz .Ldc_submit_unlock
 mov rax,[rsp+16]
 mov [rbx+80],rax
 mov dword ptr [rbx+60],1
 mov dword ptr [rbx+64],0
 mov dword ptr [rbx+76],0
 mov rdi,[rbx+96]
 call json_object_put@PLT
 mov qword ptr [rbx+96],0
 mov rdi,rbx
 xor esi,esi
 xor edx,edx
 call .Ldc_boundary
 xor ebp,ebp
.Ldc_submit_unlock:
 mov dword ptr [rbx+56],0
.Ldc_submit_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Ldc_cancel:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,272
 mov rbx,rdi
 pxor xmm0,xmm0
 mov rdi,rsp
 mov ecx,34
 xor eax,eax
 cld
 rep stosq
 cmp dword ptr [rbx+60],1
 jne .Ldc_cancel_end
 cmp dword ptr [rbx+56],0
 jne .Ldc_cancel_end
 mov dword ptr [rbx+64],1
 mov dword ptr [rbx+56],1
 call json_object_new_array@PLT
 mov [rsp+96],rax
 test rax,rax
 jz .Ldc_cancel_error
 mov r14,[rbx+8]
 mov r13,[r14+8]
 mov dword ptr [rsp+112],2
 mov dword ptr [rsp+116],1
 mov qword ptr [rsp+136],256
 mov qword ptr [rsp+144],321
 mov rax,[rbx+24]
 mov [rsp+152],rax
.Ldc_cancel_scan:
 mov rdi,[r13]
 lea rsi,[rsp+112]
 lea rdx,[rsp+224]
 call [r13+16]
 test eax,eax
 jnz .Ldc_cancel_error
 xor r12d,r12d
.Ldc_cancel_collect:
 mov rdi,[rsp+224]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ldc_cancel_page
 mov rdi,[rsp+224]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+104],rax
 mov edi,1
 call json_object_new_boolean@PLT
 test rax,rax
 jz .Ldc_cancel_error
 mov rdi,[rsp+104]
 lea rsi,[rip+.Ldc_abort_requested]
 mov rdx,rax
 call .Ldc_set_owned
 test eax,eax
 jnz .Ldc_cancel_error
 mov rdi,[rsp+96]
 mov rsi,[rsp+104]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Ldc_cancel_error
 mov qword ptr [rsp+104],0
 inc r12
 jmp .Ldc_cancel_collect
.Ldc_cancel_page:
 cmp dword ptr [rsp+244],0
 je .Ldc_cancel_next_status
 mov rax,[rsp+232]
 cmp dword ptr [rsp+120],0
 je .Ldc_cancel_cursor
 cmp rax,[rsp+128]
 jbe .Ldc_cancel_error
.Ldc_cancel_cursor:
 mov [rsp+128],rax
 mov dword ptr [rsp+120],1
 mov dword ptr [rsp+124],1
 jmp .Ldc_cancel_page_release
.Ldc_cancel_next_status:
 inc dword ptr [rsp+208]
 mov dword ptr [rsp+120],0
 mov dword ptr [rsp+124],0
 mov qword ptr [rsp+128],0
.Ldc_cancel_page_release:
 mov rdi,[rsp+224]
 call json_object_put@PLT
 mov qword ptr [rsp+224],0
 cmp dword ptr [rsp+208],5
 jb .Ldc_cancel_scan
.Ldc_cancel_begin:
 mov r13,[r14+8]
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+48]
 mov rcx,[r13]
 mov r8,[r13+8]
 call pg_durable_session_begin@PLT
 test eax,eax
 jnz .Ldc_cancel_error
 xor r12d,r12d
.Ldc_cancel_stage:
 mov rdi,[rsp+96]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ldc_cancel_settle
 mov rdi,[rsp+96]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov rsi,rax
 mov rdi,rsp
 mov rdx,[r13]
 mov rcx,[r13+40]
 call pg_record_tx_set_task@PLT
 test eax,eax
 jnz .Ldc_cancel_abort
 inc r12
 jmp .Ldc_cancel_stage
.Ldc_cancel_settle:
 mov rdi,[rbx]
 xor esi,esi
 xor edx,edx
 xor ecx,ecx
 call pg_durable_session_settle@PLT
 test eax,eax
 jz .Ldc_cancel_unlock
.Ldc_cancel_abort:
 mov rdi,[rbx]
 call pg_durable_session_abort@PLT
.Ldc_cancel_error:
 mov dword ptr [rbx+72],1
.Ldc_cancel_unlock:
 mov dword ptr [rbx+56],0
.Ldc_cancel_end:
 mov rdi,[rsp+96]
 call json_object_put@PLT
 mov rdi,[rsp+104]
 call json_object_put@PLT
 mov rdi,[rsp+224]
 call json_object_put@PLT
 add rsp,272
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Ldc_poll:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 xor eax,eax
 cmp dword ptr [rbx+56],0
 jne .Ldc_poll_end
 cmp dword ptr [rbx+68],0
 jne .Ldc_poll_end
 mov dword ptr [rbx+56],1
 mov rdi,[rbx+48]
 call free@PLT
 mov qword ptr [rbx+48],0
 cmp qword ptr [rbx+32],0
 jne .Ldc_poll_take
 cmp dword ptr [rbx+60],1
 jne .Ldc_poll_empty
 mov rdi,[rbx+8]
 call pg_task_scheduler_poll@PLT
 cmp eax,2
 je .Ldc_poll_finish
 cmp eax,4
 jb .Ldc_poll_take
 mov dword ptr [rbx+72],1
.Ldc_poll_finish:
 mov dword ptr [rbx+60],0
 mov rdi,rbx
 mov esi,4
 xor edx,edx
 cmp dword ptr [rbx+64],0
 je .Ldc_poll_outcome
 mov edx,2
 jmp .Ldc_poll_emit_end
.Ldc_poll_outcome:
 mov eax,[rbx+72]
 or eax,[rbx+76]
 test eax,eax
 setnz dl
 movzx edx,dl
.Ldc_poll_emit_end:
 call .Ldc_boundary
.Ldc_poll_take:
 mov rax,[rbx+32]
 test rax,rax
 jz .Ldc_poll_empty
 mov rcx,[rax]
 mov [rbx+32],rcx
 test rcx,rcx
 jnz .Ldc_poll_copy
 mov qword ptr [rbx+40],0
.Ldc_poll_copy:
 mov [rbx+48],rax
 lea rsi,[rax+8]
 mov rdi,r12
 mov edx,56
 call memcpy@PLT
 mov eax,1
 jmp .Ldc_poll_unlock
.Ldc_poll_empty:
 xor eax,eax
 cmp dword ptr [rbx+72],0
 je .Ldc_poll_unlock
 mov eax,-1
.Ldc_poll_unlock:
 mov dword ptr [rbx+56],0
.Ldc_poll_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.globl pg_durable_client_suspend
.type pg_durable_client_suspend,@function
pg_durable_client_suspend:
 push rbx
 sub rsp,16
 mov rbx,rdi
 mov eax,1
 cmp dword ptr [rbx+56],0
 jne .Ldc_suspend_end
 mov dword ptr [rbx+68],1
 mov rdi,[rbx+8]
 call pg_task_scheduler_close@PLT
 test eax,eax
 jnz .Ldc_suspend_end
.Ldc_suspend_queue:
 mov rdi,[rbx+32]
 test rdi,rdi
 jz .Ldc_suspend_delivered
 mov rax,[rdi]
 mov [rbx+32],rax
 call free@PLT
 jmp .Ldc_suspend_queue
.Ldc_suspend_delivered:
 mov rdi,[rbx+48]
 call free@PLT
 mov qword ptr [rbx+48],0
 mov qword ptr [rbx+40],0
 mov dword ptr [rbx+60],0
 mov rdi,[rbx+88]
 call json_object_put@PLT
 mov qword ptr [rbx+88],0
 mov rdi,[rbx+96]
 call json_object_put@PLT
 mov qword ptr [rbx+96],0
 xor eax,eax
.Ldc_suspend_end:
 add rsp,16
 pop rbx
 ret
.size pg_durable_client_suspend,.-pg_durable_client_suspend
.globl pg_durable_client_service
.type pg_durable_client_service,@function
pg_durable_client_service:
 mov [rdi],rsi
 lea rax,[rip+.Ldc_submit]
 mov [rdi+8],rax
 lea rax,[rip+.Ldc_cancel]
 mov [rdi+16],rax
 lea rax,[rip+.Ldc_poll]
 mov [rdi+24],rax
 mov rax,rdi
 ret
.size pg_durable_client_service,.-pg_durable_client_service
.globl pg_durable_client_fault
.type pg_durable_client_fault,@function
pg_durable_client_fault:
 mov dword ptr [rdi+76],1
 ret
.size pg_durable_client_fault,.-pg_durable_client_fault
.globl pg_durable_client_tool
.type pg_durable_client_tool,@function
pg_durable_client_tool:
 xor ecx,ecx
 xor r8d,r8d
 jmp .Ldc_tool_common
.size pg_durable_client_tool,.-pg_durable_client_tool
.globl pg_durable_client_output
.type pg_durable_client_output,@function
pg_durable_client_output:
 mov r8,rcx
 mov rcx,rdx
 mov edx,2
.Ldc_tool_common:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,160
 mov rbx,rdi
 mov r12,rsi
 mov [rsp+88],edx
 mov [rsp+144],rcx
 mov [rsp+152],r8
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 movdqu [rsp+64],xmm0
 mov rdi,rbx
 cmp dword ptr [rdi+68],0
 jne .Ldc_tool_end
 lea rdi,[rsp+96]
 mov esi,32
 lea rdx,[rip+.Ldc_id_format]
 mov rcx,r12
 xor eax,eax
 call snprintf@PLT
 mov rdi,[rbx+88]
 lea rsi,[rsp+96]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_int@PLT
 cmp dword ptr [rsp+88],0
 je .Ldc_tool_start_once
 cmp eax,1
 jne .Ldc_tool_end
 jmp .Ldc_tool_read
.Ldc_tool_start_once:
 test eax,eax
 jnz .Ldc_tool_end
.Ldc_tool_read:
 mov r13,[rbx+8]
 mov r13,[r13+8]
 mov rdi,[r13]
 mov esi,2
 mov rdx,r12
 mov rcx,rsp
 call [r13+8]
 test eax,eax
 jnz .Ldc_tool_fail
 CGET "qword ptr [rsp]",input
 mov r14,rax
 CGET rax,call_id
 mov rbp,rax
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+48],rax
 mov rdi,rbp
 call json_object_get_string_len@PLT
 mov [rsp+56],rax
 cmp dword ptr [rsp+88],1
 je .Ldc_tool_finished
 CGET r14,assistant
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdx,rax
 mov rdi,[r13]
 mov esi,1
 lea rcx,[rsp+8]
 call [r13+8]
 test eax,eax
 jnz .Ldc_tool_fail
 CGET "qword ptr [rsp+8]",model
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 CGET rax,content
 mov r14,rax
 xor r12d,r12d
.Ldc_tool_find:
 mov rdi,r14
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ldc_tool_fail
 mov rdi,r14
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov [rsp+128],rax
 CGET rax,id
 mov rdi,rax
 mov rsi,rbp
 call json_object_equal@PLT
 test eax,eax
 jnz .Ldc_tool_found
 inc r12
 jmp .Ldc_tool_find
.Ldc_tool_found:
 CGET "qword ptr [rsp+128]",arguments
 mov rdi,rax
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Ldc_tool_fail
 mov [rsp+32],rax
 mov rdi,rax
 call strlen@PLT
 mov [rsp+40],rax
 CGET "qword ptr [rsp+128]",name
 mov rbp,rax
 mov dword ptr [rsp+24],2
 mov dword ptr [rsp+80],1
 jmp .Ldc_tool_name_ready
.Ldc_tool_finished:
 CGET "qword ptr [rsp]",state
 CGET rax,outcome
 CGET rax,result
 CGET rax,entry_id
 test rax,rax
 jz .Ldc_tool_end
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdx,rax
 mov rdi,[r13]
 mov esi,1
 lea rcx,[rsp+16]
 call [r13+8]
 test eax,eax
 jnz .Ldc_tool_fail
 CGET "qword ptr [rsp+16]",model
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov r14,rax
 CGET rax,is_error
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov [rsp+28],eax
 CGET r14,content
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 CGET rax,text
 mov rbp,rax
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+32],rax
 mov rdi,rbp
 call json_object_get_string_len@PLT
 mov [rsp+40],rax
 CGET r14,tool_name
 mov rbp,rax
 mov dword ptr [rsp+24],3
 mov dword ptr [rsp+80],2
.Ldc_tool_name_ready:
 cmp dword ptr [rsp+88],2
 jne .Ldc_tool_name_copy
 mov dword ptr [rsp+24],5
 mov rax,[rsp+144]
 mov [rsp+32],rax
 mov rax,[rsp+152]
 mov [rsp+40],rax
.Ldc_tool_name_copy:
 mov rdi,rbp
 call json_object_get_string@PLT
 mov [rsp+64],rax
 mov rdi,rbp
 call json_object_get_string_len@PLT
 mov [rsp+72],rax
 mov rdi,rbx
 lea rsi,[rsp+24]
 call pg_durable_client_emit@PLT
 test eax,eax
 jnz .Ldc_tool_end
 cmp dword ptr [rsp+88],2
 je .Ldc_tool_end
 mov edi,[rsp+80]
 call json_object_new_int@PLT
 test rax,rax
 jz .Ldc_tool_fail
 mov rdi,[rbx+88]
 lea rsi,[rsp+96]
 mov rdx,rax
 call .Ldc_set_owned
 test eax,eax
 jz .Ldc_tool_end
.Ldc_tool_fail:
 mov dword ptr [rbx+72],1
.Ldc_tool_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 add rsp,160
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_durable_client_output,.-pg_durable_client_output
.globl pg_durable_client_publish
.type pg_durable_client_publish,@function
pg_durable_client_publish:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 cmp dword ptr [rbx+68],0
 jne .Ldc_publish_end
 xor r13d,r13d
.Ldc_publish_each:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Ldc_publish_end
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov [rsp],rax
 CGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Ldc_publish_next
 mov rdi,rax
 lea rsi,[rip+.Ldc_task]
 call strcmp@PLT
 test eax,eax
 jnz .Ldc_publish_next
 CGET "qword ptr [rsp]",value
 CGET rax,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rsi,rax
 mov rdi,rbx
 mov edx,1
 call pg_durable_client_tool@PLT
 CGET "qword ptr [rsp]",value
 mov r14,rax
 CGET rax,kind
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Ldc_publish_next
 mov rdi,rax
 lea rsi,[rip+.Ldc_generation]
 call strcmp@PLT
 test eax,eax
 jnz .Ldc_publish_next
 CGET r14,state
 CGET rax,outcome
 CGET rax,error
 CGET rax,message
 test rax,rax
 jz .Ldc_publish_next
 mov rdi,rax
 call json_object_get@PLT
 mov r14,rax
 mov rdi,[rbx+96]
 call json_object_put@PLT
 mov [rbx+96],r14
 mov dword ptr [rbx+76],1
.Ldc_publish_next:
 inc r13
 jmp .Ldc_publish_each
.Ldc_publish_end:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_durable_client_publish,.-pg_durable_client_publish
.section .rodata
.Ldc_model: .asciz "model"
.Ldc_content: .asciz "content"
.Ldc_background: .asciz "background"
.Ldc_abort_requested: .asciz "abortRequested"
.Ldc_input: .asciz "input"
.Ldc_arguments: .asciz "arguments"
.Ldc_call_id: .asciz "callId"
.Ldc_assistant: .asciz "assistant"
.Ldc_id: .asciz "id"
.Ldc_name: .asciz "name"
.Ldc_state: .asciz "state"
.Ldc_outcome: .asciz "outcome"
.Ldc_result: .asciz "result"
.Ldc_entry_id: .asciz "entryId"
.Ldc_is_error: .asciz "isError"
.Ldc_tool_name: .asciz "toolName"
.Ldc_text: .asciz "text"
.Ldc_type: .asciz "type"
.Ldc_value: .asciz "value"
.Ldc_kind: .asciz "kind"
.Ldc_generation: .asciz "pangu.generation"
.Ldc_error: .asciz "error"
.Ldc_message: .asciz "message"
.Ldc_task: .asciz "task"
.Ldc_id_format: .asciz "%llu"
.Ldc_input_json: .asciz "{\"type\":\"input\",\"content\":\"\",\"whenBusy\":\"reject\"}"
.section .note.GNU-stack,"",@progbits

.text
.globl pg_durable_client_tool_observe
.type pg_durable_client_tool_observe,@function
pg_durable_client_tool_observe:
 test rdi,rdi
 jz .Ldc_observe_invalid
 cmp dword ptr [rdi+68],0
 jne .Ldc_observe_invalid
 mov r8,rcx
 mov rcx,rdx
 mov rdx,rsi
 mov rsi,[rdi+24]
 mov rdi,[rdi+8]
 mov rdi,[rdi+8]
 jmp pg_durable_tool_observe@PLT
.Ldc_observe_invalid:
 test rcx,rcx
 jz .Ldc_observe_invalid_return
 mov qword ptr [rcx],0
.Ldc_observe_invalid_return:
 mov eax,4
 ret
.size pg_durable_client_tool_observe,.-pg_durable_client_tool_observe

.text
.globl pg_durable_client_inputs_service
.type pg_durable_client_inputs_service,@function
pg_durable_client_inputs_service:
 test rdi,rdi
 jz .Ldi_factory_bad
 test rsi,rsi
 jz .Ldi_factory_bad
 cmp qword ptr [rdi+16],0
 je .Ldi_factory_bad
 mov [rsi],rdi
 lea rax,[rip+.Ldi_submit]
 mov [rsi+8],rax
 lea rax,[rip+.Ldi_status]
 mov [rsi+16],rax
 lea rax,[rip+.Ldi_withdraw]
 mov [rsi+24],rax
 lea rax,[rip+.Ldi_list]
 mov [rsi+32],rax
 lea rax,[rip+.Ldi_release]
 mov [rsi+40],rax
 xor eax,eax
 ret
.Ldi_factory_bad:
 mov eax,4
 ret
.size pg_durable_client_inputs_service,.-pg_durable_client_inputs_service
/* Shared check; reads may inspect a failed run, but never a closing client. */
.Ldi_check:
 mov eax,7
 test rdi,rdi
 jz .Ldi_check_end
 cmp dword ptr [rdi+68],0
 jne .Ldi_check_end
 mov rax,[rdi+16]
 test rax,rax
 jz .Ldi_check_closed
 cmp dword ptr [rax+44],1
 jne .Ldi_check_closed
 mov eax,3
 cmp dword ptr [rdi+56],0
 jne .Ldi_check_end
 xor eax,eax
 ret
.Ldi_check_closed:
 mov eax,7
.Ldi_check_end:
 ret
.Ldi_submit:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 test rdx,rdx
 jz .Ldi_submit_invalid
 mov qword ptr [rdx],0
 call .Ldi_check
 test eax,eax
 jnz .Ldi_submit_end
 mov dword ptr [rbx+56],1
 mov rdi,[rbx+16]
 mov rsi,[rbx+24]
 mov rdx,r12
 mov rcx,rsp
 call pg_durable_inputs_submit@PLT
 mov ebp,eax
 test eax,eax
 jnz .Ldi_submit_unlock
 mov rax,[rsp]
 mov [r13],rax
 cmp dword ptr [rbx+60],0
 jne .Ldi_submit_unlock
 cmp dword ptr [rsp+16],2
 je .Ldi_submit_unlock
 cmp qword ptr [rsp+8],0
 je .Ldi_submit_unlock
 mov rax,[rsp+8]
 mov [rbx+80],rax
 mov dword ptr [rbx+60],1
 mov dword ptr [rbx+64],0
 mov dword ptr [rbx+76],0
 mov rdi,[rbx+96]
 call json_object_put@PLT
 mov qword ptr [rbx+96],0
 mov rdi,rbx
 xor esi,esi
 xor edx,edx
 call .Ldc_boundary
.Ldi_submit_unlock:
 mov dword ptr [rbx+56],0
 mov eax,ebp
 jmp .Ldi_submit_end
.Ldi_submit_invalid:
 mov eax,4
.Ldi_submit_end:
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Ldi_status:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 test rdx,rdx
 jz .Ldi_status_invalid
 mov qword ptr [rdx],0
 call .Ldi_check
 test eax,eax
 jnz .Ldi_status_end
 mov rdi,[rbx+16]
 mov rsi,r12
 mov rdx,r13
 call pg_durable_inputs_status@PLT
 test eax,eax
 jnz .Ldi_status_end
 mov rdi,[r13]
 lea rsi,[rip+.Ldc_conversation_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rbx+24]
 je .Ldi_status_ok
 mov rdi,[r13]
 call json_object_put@PLT
 mov qword ptr [r13],0
 mov eax,2
 jmp .Ldi_status_end
.Ldi_status_ok:
 xor eax,eax
 jmp .Ldi_status_end
.Ldi_status_invalid:
 mov eax,4
.Ldi_status_end:
 pop r13
 pop r12
 pop rbx
 ret
.Ldi_withdraw:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 call .Ldi_check
 test eax,eax
 jnz .Ldi_withdraw_end
 mov rdi,[rbx+16]
 mov rsi,[rbx+24]
 mov rdx,r12
 mov rcx,r13
 call pg_durable_inputs_withdraw@PLT
.Ldi_withdraw_end:
 pop r13
 pop r12
 pop rbx
 ret
.Ldi_list:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,144
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 test rcx,rcx
 jz .Ldi_list_invalid
 mov qword ptr [rcx],0
 test r13,r13
 jz .Ldi_list_invalid
 cmp r13,128
 ja .Ldi_list_invalid
 mov rax,9007199254740991
 cmp r12,rax
 ja .Ldi_list_invalid
 call .Ldi_check
 test eax,eax
 jnz .Ldi_list_end
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 movdqu [rsp+64],xmm0
 movdqu [rsp+80],xmm0
 movdqu [rsp+96],xmm0
 movdqu [rsp+112],xmm0
 movdqu [rsp+128],xmm0
 mov dword ptr [rsp],3
 mov dword ptr [rsp+4],1
 test r12,r12
 setnz byte ptr [rsp+8]
 mov dword ptr [rsp+12],1
 mov [rsp+16],r12
 mov [rsp+24],r13
 mov qword ptr [rsp+32],1
 mov rax,[rbx+24]
 mov [rsp+40],rax
 mov rax,[rbx+16]
 mov rax,[rax+8]
 mov rdi,[rax]
 mov rsi,rsp
 lea rdx,[rsp+112]
 call [rax+16]
 mov ebp,eax
 test eax,eax
 jnz .Ldi_list_error
 mov rax,[rsp+112]
 mov [r14],rax
 xor eax,eax
 jmp .Ldi_list_end
.Ldi_list_error:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov eax,6
 jmp .Ldi_list_end
.Ldi_list_invalid:
 mov eax,4
.Ldi_list_end:
 add rsp,144
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.section .rodata
.Ldc_conversation_id: .asciz "conversationId"

.text
.Ldi_release:
 jmp json_object_put@PLT
