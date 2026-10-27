#include "session_layout.h"
.intel_syntax noprefix
.macro GGET object,key
 mov rdi,\object
 lea rsi,[rip+.Lgg_\key]
 call json_object_object_get@PLT
.endm
.text
.Lgg_set:
 test rdx,rdx
 jz .Lgg_set_bad
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Lgg_set_end
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,5
.Lgg_set_end:
 pop rbx
 ret
.Lgg_set_bad:
 mov eax,5
 ret
.Lgg_copy_field:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 call json_object_get@PLT
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call .Lgg_set
 add rsp,8
 pop r12
 pop rbx
 ret
.Lgg_initial:
 push rbx
 mov rbx,rdx
 lea rdi,[rip+.Lgg_initial_json]
 call json_tokener_parse@PLT
 mov [rbx],rax
 test rax,rax
 setz al
 movzx eax,al
 pop rbx
 ret
.globl pg_durable_generation_init
.type pg_durable_generation_init,@function
pg_durable_generation_init:
 mov eax,1
 test rdi,rdi
 jz .Lgg_init_return
 test rsi,rsi
 jz .Lgg_init_return
 mov ecx,8
.Lgg_init_host:
 cmp qword ptr [rsi+rcx],0
 je .Lgg_init_return
 add rcx,8
 cmp rcx,40
 jb .Lgg_init_host
 cmp qword ptr [rsi+48],0
 je .Lgg_init_return
 cmp qword ptr [rsi+64],0
 je .Lgg_init_return
 mov [rdi],rsi
 lea rax,[rip+.Lgg_name]
 mov [rdi+8],rax
 mov qword ptr [rdi+16],16
 mov qword ptr [rdi+24],1
 mov [rdi+32],rdi
 lea rax,[rip+.Lgg_initial]
 mov [rdi+40],rax
 lea rax,[rdi+96]
 mov [rdi+48],rax
 mov qword ptr [rdi+56],5
 lea rax,[rip+.Lgg_abort]
 mov [rdi+64],rax
 lea rax,[rip+.Lgg_cleanup]
 mov [rdi+72],rax
 mov qword ptr [rdi+80],0
 lea rax,[rip+.Lgg_cutoff_handler]
 mov [rdi+88],rax
 lea rax,[rip+.Lgg_prepare_name]
 mov [rdi+96],rax
 mov qword ptr [rdi+104],7
 mov [rdi+112],rdi
 lea rax,[rip+.Lgg_prepare]
 mov [rdi+120],rax
 lea rax,[rip+.Lgg_cleanup]
 mov [rdi+128],rax
 lea rax,[rip+.Lgg_request_name]
 mov [rdi+136],rax
 mov qword ptr [rdi+144],7
 mov [rdi+152],rdi
 lea rax,[rip+.Lgg_request]
 mov [rdi+160],rax
 lea rax,[rip+.Lgg_cleanup]
 mov [rdi+168],rax
 lea rax,[rip+.Lgg_tools_name]
 mov [rdi+176],rax
 mov qword ptr [rdi+184],5
 mov [rdi+192],rdi
 lea rax,[rip+.Lgg_tools]
 mov [rdi+200],rax
 lea rax,[rip+.Lgg_cleanup]
 mov [rdi+208],rax
 lea rax,[rip+.Lgg_retry_name]
 mov [rdi+216],rax
 mov qword ptr [rdi+224],5
 mov [rdi+232],rdi
 lea rax,[rip+.Lgg_retry]
 mov [rdi+240],rax
 lea rax,[rip+.Lgg_cleanup]
 mov [rdi+248],rax
 lea rax,[rip+.Lgg_poll_name]
 mov [rdi+256],rax
 mov qword ptr [rdi+264],4
 mov [rdi+272],rdi
 lea rax,[rip+.Lgg_poll]
 mov [rdi+280],rax
 lea rax,[rip+.Lgg_cleanup]
 mov [rdi+288],rax
 xor eax,eax
.Lgg_init_return:
 ret
.size pg_durable_generation_init,.-pg_durable_generation_init
/* operation: module,runner,configuration,assistant,model[40],session,
   acquired/started/ready, reserved, retained text[4096],size; stage scratch. */
.Lgg_operation:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rsi
 mov r12,rdi
 cmp qword ptr [rbx+56],0
 jne .Lgg_operation_ok
 mov edi,1
 mov esi,4416
 call calloc@PLT
 test rax,rax
 jz .Lgg_operation_bad
 mov [rax],r12
 mov [rax+8],rbx
 mov [rbx+56],rax
.Lgg_operation_ok:
 xor eax,eax
 jmp .Lgg_operation_end
.Lgg_operation_bad:
 mov eax,5
.Lgg_operation_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.Lgg_cleanup:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rsi
 mov r12,[rsi+56]
 test r12,r12
 jz .Lgg_cleanup_end
 cmp dword ptr [r12+80],0
 je .Lgg_cleanup_json
 cmp dword ptr [r12+84],0
 je .Lgg_cleanup_release
 mov rax,[r12+56]
 test rax,rax
 jz .Lgg_cleanup_close
 mov rdi,[r12+32]
 call rax
.Lgg_cleanup_close:
 mov rax,[r12+64]
 test rax,rax
 jz .Lgg_cleanup_release
 mov rdi,[r12+32]
 call rax
.Lgg_cleanup_release:
 mov rax,[r12]
 mov rax,[rax]
 mov rdi,[rax]
 lea rsi,[r12+32]
 call [rax+24]
.Lgg_cleanup_json:
 mov rdi,[r12+4352]
 call json_object_put@PLT
 mov rdi,[r12+4360]
 call json_object_put@PLT
 mov rdi,[r12+16]
 call json_object_put@PLT
 mov rdi,[r12+24]
 call json_object_put@PLT
 mov rdi,[r12+72]
 call free@PLT
 mov rdi,r12
 call free@PLT
 mov qword ptr [rbx+56],0
.Lgg_cleanup_end:
 add rsp,8
 pop r12
 pop rbx
 ret
/* Fetch a fixed conversation page. Cursor budgets are inherited from pg_session. */
.Lgg_scan:
 push rbx
 sub rsp,112
 mov rbx,rdi
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 movdqu [rsp+64],xmm0
 movdqu [rsp+80],xmm0
 movdqu [rsp+96],xmm0
 mov dword ptr [rsp],1
 mov dword ptr [rsp+4],esi
 mov [rsp+24],rdx
 mov qword ptr [rsp+32],17
 cmp r8,-1
 jne .Lgg_scan_bound
 mov qword ptr [rsp+32],1
.Lgg_scan_bound:
 mov [rsp+40],rcx
 mov [rsp+72],r8
 mov rax,[rbx+8]
 mov rdi,[rax]
 mov rsi,rsp
 mov rdx,r9
 call [rax+16]
 add rsp,112
 pop rbx
 ret
.Lgg_prepare:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov r12,rdi
 mov rbx,rsi
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 call .Lgg_operation
 test eax,eax
 jnz .Lgg_prepare_end
 mov r14,[rbx+56]
 GGET "qword ptr [rbx+24]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov r13,rax
 mov rax,[r12]
 mov rdi,[rax]
 mov rsi,r13
 lea rdx,[r14+16]
 call [rax+8]
 test eax,eax
 jnz .Lgg_prepare_invalid
 mov rdi,[r14+16]
 call pg_document_value_valid@PLT
 test eax,eax
 jz .Lgg_prepare_invalid
 mov rdi,rbx
 mov esi,2
 mov edx,1
 mov rcx,r13
 mov r8,-1
 mov r9,rsp
 call .Lgg_scan
 test eax,eax
 jnz .Lgg_prepare_invalid
 mov rdi,[rsp]
 xor esi,esi
 call json_object_array_get_idx@PLT
 test rax,rax
 jz .Lgg_prepare_invalid
 GGET rax,id
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+24],rax
 lea rdi,[rip+.Lgg_request_json]
 call json_tokener_parse@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lgg_prepare_alloc
 GGET rax,checkpoint
 mov rbp,rax
 mov rdi,rax
 lea rsi,[rip+.Lgg_configuration]
 mov rdx,[r14+16]
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_prepare_end
 mov rdi,rbp
 lea rsi,[rip+.Lgg_cutoff]
 mov rdx,[rsp+24]
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_prepare_end
 GGET "qword ptr [rbx+32]",state
 GGET rax,checkpoint
 GGET rax,attempt
 test rax,rax
 jz .Lgg_prepare_attempt_ready
 mov rdi,rbp
 lea rsi,[rip+.Lgg_attempt]
 mov rdx,rax
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_prepare_end
.Lgg_prepare_attempt_ready:
 mov rdi,rbx
 mov rsi,[rsp+32]
 call pg_task_runner_commit@PLT
 jmp .Lgg_prepare_end
.Lgg_prepare_alloc:
 mov eax,5
 jmp .Lgg_prepare_end
.Lgg_prepare_invalid:
 mov eax,4
.Lgg_prepare_end:
 mov ebp,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov eax,ebp
 neg eax
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Append a model protocol message; UTF-8 bytes are already validated JSON. */
.Lgg_add_message:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov ebp,edx
 mov r13,rcx
 mov r14,r8
 mov [rsp],r9d
 mov rax,[rbx+PG_S_COUNT]
 cmp rax,64
 jae .Lgg_add_bad
 imul rax,PG_MSG_BYTES
 add rbx,rax
 mov [rbx],ebp
 mov rdi,r12
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lgg_add_bad
 mov rdi,r12
 call json_object_get_string_len@PLT
 cmp eax,4096
 jae .Lgg_add_bad
 mov [rbx+PG_MSG_SIZE],rax
 mov [rsp+8],rax
 mov rdi,r12
 call json_object_get_string@PLT
 lea rdi,[rbx+PG_MSG_TEXT]
 mov rsi,rax
 mov rdx,[rsp+8]
 call memcpy@PLT
 cmp ebp,4
 je .Lgg_add_thinking_name
 cmp ebp,2
 jb .Lgg_add_done
 mov rdi,r13
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lgg_add_bad
 cmp eax,64
 jae .Lgg_add_bad
 mov [rsp+8],rax
 mov rdi,r13
 call json_object_get_string@PLT
 test rax,rax
 jz .Lgg_add_bad
 lea rdi,[rbx+PG_MSG_ID]
 mov rsi,rax
 mov rdx,[rsp+8]
 call memcpy@PLT
 .Lgg_add_thinking_name:
 mov rdi,r14
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lgg_add_bad
 cmp eax,64
 jae .Lgg_add_bad
 mov [rsp+8],rax
 mov rdi,r14
 call json_object_get_string@PLT
 test rax,rax
 jz .Lgg_add_bad
 lea rdi,[rbx+PG_MSG_NAME]
 mov rsi,rax
 mov rdx,[rsp+8]
 call memcpy@PLT
 mov eax,[rsp]
 mov [rbx+PG_MSG_ERROR],eax
.Lgg_add_done:
 xor eax,eax
 jmp .Lgg_add_end
.Lgg_add_bad:
 mov eax,4
.Lgg_add_end:
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Project text/toolCall/toolResult JSON content into existing model ABI. */
.Lgg_project_message:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov dword ptr [rsp+24],0
 GGET r12,role
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lgg_project_bad
 mov r13,rax
 lea rsi,[rip+.Lgg_user]
 mov rdi,rax
 call strcmp@PLT
 test eax,eax
 jz .Lgg_project_role
 mov dword ptr [rsp+24],1
 mov rdi,r13
 lea rsi,[rip+.Lgg_assistant]
 call strcmp@PLT
 test eax,eax
 jz .Lgg_project_assistant
 mov dword ptr [rsp+24],3
 mov rdi,r13
 lea rsi,[rip+.Lgg_tool_result]
 call strcmp@PLT
 test eax,eax
 jnz .Lgg_project_bad
jmp .Lgg_project_role
.Lgg_project_assistant:
 GGET r12,stop_reason
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lgg_project_role
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Lgg_error_word]
 call strcmp@PLT
 test eax,eax
 jz .Lgg_project_ok
 mov rdi,r13
 lea rsi,[rip+.Lgg_aborted_word]
 call strcmp@PLT
 test eax,eax
 jz .Lgg_project_ok
 mov rdi,r13
 lea rsi,[rip+.Lgg_deferred_word]
 call strcmp@PLT
 test eax,eax
 jz .Lgg_project_ok
.Lgg_project_role:
 GGET r12,content
 mov r13,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lgg_project_array
 cmp dword ptr [rsp+24],0
 jne .Lgg_project_bad
 mov rsi,r13
 xor edx,edx
 xor ecx,ecx
 xor r8d,r8d
 xor r9d,r9d
 jmp .Lgg_project_add
.Lgg_project_array:
 mov rdi,r13
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lgg_project_bad
 cmp dword ptr [rsp+24],3
 je .Lgg_project_tool_text
 xor r14d,r14d
.Lgg_project_block:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lgg_project_ok
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov [rsp],rax
 GGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lgg_project_bad
 mov rdi,rax
 lea rsi,[rip+.Lgg_text]
 call strcmp@PLT
 test eax,eax
 jnz .Lgg_project_call
 GGET "qword ptr [rsp]",text
 mov [rsp+8],rax
 mov ecx,0
 mov r8d,0
 xor r9d,r9d
 mov rsi,[rsp+8]
 mov edx,[rsp+24]
 jmp .Lgg_project_one
.Lgg_project_call:
 GGET "qword ptr [rsp]",type
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lgg_thinking]
 call strcmp@PLT
 test eax,eax
 jnz .Lgg_project_normal_call
 cmp dword ptr [rsp+24],1
 jne .Lgg_project_bad
 GGET "qword ptr [rsp]",thinking
 mov [rsp+8],rax
 GGET "qword ptr [rsp]",thinking_signature
 mov r8,rax
 mov rdi,rbx
 mov rsi,[rsp+8]
 mov edx,4
 xor ecx,ecx
 xor r9d,r9d
 call .Lgg_add_message
 test eax,eax
 jnz .Lgg_project_bad
 inc qword ptr [rbx+PG_S_COUNT]
 inc r14
 jmp .Lgg_project_block
.Lgg_project_normal_call:
 cmp dword ptr [rsp+24],1
 jne .Lgg_project_bad
 GGET "qword ptr [rsp]",type
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lgg_tool_call]
 call strcmp@PLT
 test eax,eax
 jnz .Lgg_project_bad
 GGET "qword ptr [rsp]",arguments
 mov rdi,rax
 mov esi,256
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Lgg_project_bad
 mov rdi,rax
 call json_object_new_string@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lgg_project_bad
 GGET "qword ptr [rsp]",id
 mov [rsp+16],rax
 GGET "qword ptr [rsp]",name_key
 mov r8,rax
 mov rcx,[rsp+16]
 mov rsi,[rsp+8]
 mov edx,2
 xor r9d,r9d
.Lgg_project_one:
 mov rdi,rbx
 call .Lgg_add_message
 mov ebp,eax
 /* Only a call serializes a temporary JSON string. */
 mov rax,[rsp]
 GGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lgg_tool_call]
 call strcmp@PLT
 test eax,eax
 jnz .Lgg_project_one_result
 mov rdi,[rsp+8]
 call json_object_put@PLT
.Lgg_project_one_result:
 test ebp,ebp
 jnz .Lgg_project_bad
 inc qword ptr [rbx+PG_S_COUNT]
 inc r14
 jmp .Lgg_project_block
.Lgg_project_tool_text:
 mov rdi,r13
 call .Lgg_join_tool_text
 test rax,rax
 jz .Lgg_project_bad
 mov [rsp+8],rax
 GGET r12,tool_call_id
 mov [rsp+16],rax
 GGET r12,tool_name
 mov rbp,rax
 GGET r12,is_error
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov r9d,eax
 mov rdi,rbx
 mov rsi,[rsp+8]
 mov edx,3
 mov rcx,[rsp+16]
 mov r8,rbp
 call .Lgg_add_message
 mov ebp,eax
 mov rdi,[rsp+8]
 call json_object_put@PLT
 test ebp,ebp
 jnz .Lgg_project_bad
 inc qword ptr [rbx+PG_S_COUNT]
 jmp .Lgg_project_ok
.Lgg_project_add:
 mov rdi,rbx
 call .Lgg_add_message
 test eax,eax
 jnz .Lgg_project_bad
 inc qword ptr [rbx+PG_S_COUNT]
.Lgg_project_ok:
 xor eax,eax
 jmp .Lgg_project_end
.Lgg_project_bad:
 mov eax,4
.Lgg_project_end:
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lgg_project:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 mov qword ptr [rsp+16],0
 mov edi,1
 mov esi,PG_S_BYTES
 call calloc@PLT
 mov [rbx+72],rax
 test rax,rax
 jz .Lgg_context_alloc
 GGET "qword ptr [rbx+16]",system_prompt
 test rax,rax
 jz .Lgg_context_entries
 mov r12,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lgg_context_bad
 mov rdi,r12
 call json_object_get_string_len@PLT
 cmp eax,4096
 jae .Lgg_context_bad
 mov r13,rax
 mov rcx,[rbx+72]
 mov [rcx+PG_S_SYSTEM_SIZE],rax
 mov rdi,r12
 call json_object_get_string@PLT
 mov rdi,[rbx+72]
 add rdi,PG_S_SYSTEM
 mov rsi,rax
 mov rdx,r13
 call memcpy@PLT
.Lgg_context_entries:
 mov r12,[rbx+8]
 GGET "qword ptr [r12+32]",state
 GGET rax,checkpoint
 GGET rax,cutoff
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov r14,rax
 GGET "qword ptr [r12+32]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rcx,rax
 mov rdi,r12
 mov esi,1
 mov edx,256
 mov r8,r14
 mov r9,rsp
 call .Lgg_scan
 test eax,eax
 jnz .Lgg_context_bad
 cmp dword ptr [rsp+20],0
 jne .Lgg_context_bad
 mov qword ptr [rsp+24],0
 xor r12d,r12d
.Lgg_context_head:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lgg_context_head_done
 mov rdi,[rsp]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 GGET rax,head
 test rax,rax
 jz .Lgg_context_head_next
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+24],rax
.Lgg_context_head_next:
 inc r12
 jmp .Lgg_context_head
.Lgg_context_head_done:
 xor r12d,r12d
.Lgg_context_entry:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lgg_context_ok
 mov rdi,[rsp]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov [rsp+32],rax
 GGET rax,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rsp+24]
 jb .Lgg_context_next
 GGET "qword ptr [rsp+32]",model
 mov r13,rax
 test rax,rax
 jz .Lgg_context_next
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lgg_context_bad
 xor r14d,r14d
.Lgg_context_message:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lgg_context_next
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rsi,rax
 mov rdi,[rbx+72]
 call .Lgg_project_message
 test eax,eax
 jnz .Lgg_context_bad
 inc r14
 jmp .Lgg_context_message
.Lgg_context_next:
 inc r12
 jmp .Lgg_context_entry
.Lgg_context_ok:
 xor ebp,ebp
 jmp .Lgg_context_end
.Lgg_context_alloc:
 mov ebp,5
 jmp .Lgg_context_end
.Lgg_context_bad:
 mov ebp,4
.Lgg_context_end:
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
/* Detach terminal provider tool calls before releasing its next-event borrow. */
.Lgg_call:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 lea rdi,[rip+.Lgg_call_json]
 call json_tokener_parse@PLT
 mov r13,rax
 test rax,rax
 jz .Lgg_call_alloc
 mov rdi,[r12+16]
 mov esi,[r12+24]
 call json_object_new_string_len@PLT
 mov rdi,r13
 lea rsi,[rip+.Lgg_id]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_call_alloc
 mov rdi,[r12+32]
 mov esi,[r12+40]
 call json_object_new_string_len@PLT
 mov rdi,r13
 lea rsi,[rip+.Lgg_name_key]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_call_alloc
 mov rdi,[r12]
 mov rsi,[r12+8]
 cmp rsi,4096
 jae .Lgg_call_invalid
 call strndup@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgg_call_alloc
 mov rdi,rax
 call json_tokener_parse@PLT
 mov [rsp+8],rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lgg_call_invalid
 mov rdi,r13
 lea rsi,[rip+.Lgg_arguments]
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 call .Lgg_set
 test eax,eax
 jnz .Lgg_call_alloc
 GGET "qword ptr [rbx+24]",content
 mov rdi,rax
 mov rsi,r13
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lgg_call_alloc
 xor r13d,r13d
 xor ebp,ebp
 jmp .Lgg_call_end
.Lgg_call_invalid:
 mov ebp,4
 jmp .Lgg_call_end
.Lgg_call_alloc:
 mov ebp,5
.Lgg_call_end:
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,r13
 call json_object_put@PLT
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lgg_request:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rsi
 mov r12,rdi
 call .Lgg_operation
 test eax,eax
 jnz .Lgg_request_error
 mov r14,[rbx+56]
 mov rdi,rbx
 call .Lgg_poll_wait
 test eax,eax
 js .Lgg_request_end
 cmp eax,1
 je .Lgg_request_reslept
 cmp dword ptr [r14+4400],0
 jne .Lgg_request_live_ready
 mov rdi,r14
 xor esi,esi
 call .Lgg_live_write
 test eax,eax
 jnz .Lgg_request_error
 mov dword ptr [r14+4400],1
.Lgg_request_live_ready:
 cmp dword ptr [r14+88],0
 jne .Lgg_request_settle
 cmp qword ptr [r14+16],0
 jne .Lgg_request_acquire
 GGET "qword ptr [rbx+32]",state
 GGET rax,checkpoint
 GGET rax,configuration
 mov rdi,rax
 call json_object_get@PLT
 mov [r14+16],rax
 test rax,rax
 jz .Lgg_request_invalid
 mov rdi,r14
 call .Lgg_project
 test eax,eax
 jnz .Lgg_request_error
 lea rdi,[rip+.Lgg_assistant_json]
 call json_tokener_parse@PLT
 mov [r14+24],rax
 test rax,rax
 jz .Lgg_request_alloc
.Lgg_request_acquire:
 cmp dword ptr [r14+80],0
 jne .Lgg_request_started
 mov rax,[r12]
 mov rdi,[rax]
 mov rsi,[r14+16]
 lea rdx,[r14+32]
 call [rax+16]
 cmp eax,1
 je .Lgg_request_pending
 test eax,eax
 jnz .Lgg_request_invalid
 mov dword ptr [r14+80],1
 cmp qword ptr [r14+40],0
 je .Lgg_request_invalid
 cmp qword ptr [r14+48],0
 je .Lgg_request_invalid
 GGET "qword ptr [rbx+32]",state
 GGET rax,checkpoint
 GGET rax,handle
 test rax,rax
 jz .Lgg_request_stream_start
 mov rcx,[r12]
 cmp qword ptr [rcx+80],0
 je .Lgg_request_invalid
 mov rdi,[rcx]
 lea rsi,[r14+32]
 mov rdx,rax
 mov dword ptr [r14+84],1
 call [rcx+80]
 jmp .Lgg_request_start_result
.Lgg_request_stream_start:
 mov rdi,[r14+32]
 mov rsi,[r14+72]
 mov dword ptr [r14+84],1
 call [r14+40]
.Lgg_request_start_result:
 test eax,eax
 jnz .Lgg_request_model_error
.Lgg_request_started:
 mov rdi,[r14+32]
 lea rsi,[r14+4200]
 call [r14+48]
 test eax,eax
 jnz .Lgg_request_model_error
 mov eax,[r14+4200]
 test eax,eax
 jz .Lgg_request_pending
 cmp eax,1
 je .Lgg_request_text
 cmp eax,2
 je .Lgg_request_call
 cmp eax,3
 je .Lgg_request_ready
 cmp eax,4
 je .Lgg_request_model_error
 cmp eax,5
 jne .Lgg_request_invalid
 mov r13,[r14+4208]
 mov rbp,[r14+4216]
 cmp rbp,8
 ja .Lgg_request_invalid
 test rbp,rbp
 jz .Lgg_request_invalid
.Lgg_request_calls:
 mov rdi,r14
 mov rsi,r13
 call .Lgg_call
 test eax,eax
 jnz .Lgg_request_error
 add r13,48
 dec rbp
 jnz .Lgg_request_calls
 jmp .Lgg_request_ready
.Lgg_request_call:
 mov rdi,r14
 lea rsi,[r14+4208]
 call .Lgg_call
 test eax,eax
 jnz .Lgg_request_error
 jmp .Lgg_request_ready
.Lgg_request_text:
 mov r13,[r14+4216]
 mov rax,[r14+4192]
 add rax,r13
 jc .Lgg_request_invalid
 cmp rax,4096
 jae .Lgg_request_invalid
 mov rdi,[r14+4192]
 lea rdi,[r14+rdi+96]
 mov rsi,[r14+4208]
 mov rdx,r13
 test rsi,rsi
 jz .Lgg_request_invalid
 call memcpy@PLT
 add [r14+4192],r13
 mov rdi,r14
 mov esi,1
 call .Lgg_live_write
 test eax,eax
 jnz .Lgg_request_error
 mov rax,[r12]
 mov r8,[rax+40]
 test r8,r8
 jz .Lgg_request_pending
 mov rdi,[rax]
 mov rsi,[rbx+16]
 mov rdx,[r14+4208]
 mov rcx,r13
 call r8
 jmp .Lgg_request_pending
.Lgg_request_model_error:
 mov rdi,r12
 mov rsi,rbx
 call .Lgg_failure
 test eax,eax
 jnz .Lgg_request_end
.Lgg_request_ready:
 /* Place preceding streamed text before terminal tool calls. */
 cmp qword ptr [r14+4192],0
 je .Lgg_request_ready_close
 lea rdi,[rip+.Lgg_text_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgg_request_alloc
 lea rdi,[r14+96]
 mov esi,[r14+4192]
 call json_object_new_string_len@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lgg_text]
 mov rdx,rax
 call .Lgg_set
 mov ebp,eax
 test eax,eax
 jnz .Lgg_request_text_fail
 GGET "qword ptr [r14+24]",content
 mov rdi,rax
 xor esi,esi
 mov rdx,[rsp]
 call json_object_array_insert_idx@PLT
 test eax,eax
 jz .Lgg_request_text_added
 mov ebp,5
 jmp .Lgg_request_text_fail
.Lgg_request_text_added:
 mov qword ptr [rsp],0
.Lgg_request_text_fail:
 mov rdi,[rsp]
 call json_object_put@PLT
 test ebp,ebp
 jnz .Lgg_request_alloc
.Lgg_request_ready_close:
 GGET "qword ptr [r14+24]",error_message
 test rax,rax
 jnz .Lgg_request_metadata_done
 mov rax,[r12]
 cmp qword ptr [rax+72],0
 je .Lgg_request_metadata_done
 mov rdi,[rax]
 lea rsi,[r14+32]
 mov rdx,[r14+24]
 call [rax+72]
 test eax,eax
 jnz .Lgg_request_invalid
.Lgg_request_metadata_done:
 mov rax,[r14+64]
 test rax,rax
 jz .Lgg_request_ready_mark
 mov rdi,[r14+32]
 call rax
.Lgg_request_ready_mark:
 mov dword ptr [r14+84],0
 mov dword ptr [r14+88],1
.Lgg_request_settle:
 GGET "qword ptr [r14+24]",deferred
 test rax,rax
 jz .Lgg_request_final_settle
 mov rdi,r14
 call .Lgg_defer
 test eax,eax
 jnz .Lgg_request_error
 xor eax,eax
 jmp .Lgg_request_end
.Lgg_request_final_settle:
 mov rdi,rbx
 mov rsi,r14
 lea rdx,[rip+.Lgg_stage]
 call pg_task_runner_commit_with@PLT
 test eax,eax
 jnz .Lgg_request_error
 xor eax,eax
 jmp .Lgg_request_end
.Lgg_request_reslept:
 xor eax,eax
 jmp .Lgg_request_end
.Lgg_request_pending:
 mov eax,1
 jmp .Lgg_request_end
.Lgg_request_alloc:
 mov eax,5
 jmp .Lgg_request_error
.Lgg_request_invalid:
 mov eax,4
.Lgg_request_error:
 neg eax
.Lgg_request_end:
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Stage assistant, owned Tool tasks and next durable state as one batch. */
.Lgg_stage:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,224
 mov rbx,rdi
 mov r12,rsi
 mov r13,rcx
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
 movdqu [rsp+144],xmm0
 movdqu [rsp+160],xmm0
 movdqu [rsp+176],xmm0
 movdqu [rsp+192],xmm0
 movdqu [rsp+208],xmm0
 mov [rsp+216],rdx
 mov dword ptr [rsp+192],1
 mov rax,[rbx]
 mov rax,[rax]
 mov rax,[rax+64]
 mov [rsp+208],rax
 mov rax,[rbx+8]
 GGET "qword ptr [rax+32]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+200],rax
 mov rdi,[rsp+208]
 mov rsi,r12
 mov rdx,rax
 lea rcx,[rsp+160]
 call pg_durable_inputs_prepare@PLT
 test eax,eax
 jnz .Lgg_stage_end
 mov rdi,[rbx+8]
 mov rsi,r12
 mov rdx,[rsp+216]
 mov ecx,2
 xor r8d,r8d
 xor r9d,r9d
 call .Lgg_live_apply
 test eax,eax
 jnz .Lgg_stage_end
 lea rdi,[rip+.Lgg_entry_json]
 call json_tokener_parse@PLT
 mov [rsp+48],rax
 test rax,rax
 jz .Lgg_stage_alloc
 mov rdi,[rbx+24]
 call json_object_get@PLT
 mov rbp,rax
 GGET "qword ptr [rsp+48]",model
 mov rdi,rax
 mov rsi,rbp
 call json_object_array_add@PLT
 test eax,eax
 jz .Lgg_stage_assistant_added
 mov rdi,rbp
 call json_object_put@PLT
 jmp .Lgg_stage_alloc
.Lgg_stage_assistant_added:
 /* model[] owns the extra assistant reference. */
 mov rax,[rbx+8]
 GGET "qword ptr [rax+24]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+144],rax
 mov rsi,rax
 mov rax,[rbx+8]
 mov rcx,[rax+16]
 mov rdi,r12
 mov rdx,[rsp+48]
 mov r8,[rbx]
 mov r8,[r8]
 mov r8,[r8+32]
 lea r9,[rsp+56]
 call pg_record_tx_append_entry@PLT
 test eax,eax
 jnz .Lgg_stage_alloc
 GGET "qword ptr [rsp+56]",id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rbx+4256],rax
 cmp qword ptr [rbx+4360],0
 je .Lgg_stage_success
 mov rdi,[rbx+4360]
 call json_object_get@PLT
 mov [rsp+64],rax
 mov dword ptr [rsp+192],2
 jmp .Lgg_stage_output
.Lgg_stage_success:
 lea rdi,[rip+.Lgg_terminal_json]
 call json_tokener_parse@PLT
 mov [rsp+64],rax
 test rax,rax
 jz .Lgg_stage_alloc
 GGET rax,outcome
 GGET rax,result
 mov r14,rax
 GGET "qword ptr [rsp+56]",id
 mov rdi,r14
 lea rsi,[rip+.Lgg_entry_id]
 mov rdx,rax
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_stage_end
 call json_object_new_array@PLT
 mov [rsp+72],rax
 test rax,rax
 jz .Lgg_stage_alloc
 lea rdi,[rip+.Lgg_tool_definition_json]
 call json_tokener_parse@PLT
 mov [rsp+80],rax
 test rax,rax
 jz .Lgg_stage_alloc
 lea rdi,[rip+.Lgg_tool_options_json]
 call json_tokener_parse@PLT
 mov [rsp+88],rax
 test rax,rax
 jz .Lgg_stage_alloc
 GGET rax,ownership
 mov r14,rax
 mov rax,[rbx+8]
 mov rdi,[rax+16]
 call json_object_new_int64@PLT
 mov rdi,r14
 lea rsi,[rip+.Lgg_task_id]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_stage_end
 mov rax,[rbx]
 mov rax,[rax]
 mov rax,[rax+48]
 mov [rsp],rbx
 lea rcx,[rip+.Lgg_child_initial]
 mov [rsp+8],rcx
 lea rcx,[rip+.Lgg_child_mint]
 mov [rsp+16],rcx
 mov rcx,[rsp+144]
 mov [rsp+32],rcx
 GGET "qword ptr [rbx+24]",content
 mov [rsp+128],rax
 xor r14d,r14d
.Lgg_stage_call:
 mov rdi,[rsp+128]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lgg_stage_decision
 mov rdi,[rsp+128]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov [rsp+136],rax
 GGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lgg_tool_call]
 call strcmp@PLT
 test eax,eax
 jnz .Lgg_stage_next
 lea rdi,[rip+.Lgg_tool_input_json]
 call json_tokener_parse@PLT
 mov [rsp+96],rax
 test rax,rax
 jz .Lgg_stage_alloc
 GGET "qword ptr [rsp+56]",id
 mov rdi,[rsp+96]
 lea rsi,[rip+.Lgg_assistant]
 mov rdx,rax
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_stage_end
 GGET "qword ptr [rsp+136]",id
 mov rdi,[rsp+96]
 lea rsi,[rip+.Lgg_call_id]
 mov rdx,rax
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_stage_end
 mov rdi,r12
 mov rsi,[rsp+80]
 mov rdx,[rsp+96]
 mov rcx,[rsp+88]
 mov r8,rsp
 lea r9,[rsp+120]
 call pg_record_tx_create_task@PLT
 test eax,eax
 jnz .Lgg_stage_alloc
 mov rdi,[rsp+120]
 call json_object_new_int64@PLT
 test rax,rax
 jz .Lgg_stage_alloc
 mov rdi,[rsp+72]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lgg_stage_alloc
 mov rdi,[rsp+96]
 call json_object_put@PLT
 mov qword ptr [rsp+96],0
.Lgg_stage_next:
 inc r14
 jmp .Lgg_stage_call
.Lgg_stage_decision:
 mov rdi,[rsp+72]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lgg_stage_output
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov qword ptr [rsp+64],0
 lea rdi,[rip+.Lgg_wait_json]
 call json_tokener_parse@PLT
 mov [rsp+64],rax
 test rax,rax
 jz .Lgg_stage_alloc
 mov rdi,rax
 lea rsi,[rip+.Lgg_on]
 mov rdx,[rsp+72]
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_stage_end
 GGET "qword ptr [rsp+64]",checkpoint
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lgg_tools_key]
 mov rdx,[rsp+72]
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_stage_end
 GGET "qword ptr [rsp+56]",id
 mov rdi,r14
 lea rsi,[rip+.Lgg_assistant]
 mov rdx,rax
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_stage_end
.Lgg_stage_output:
 GGET "qword ptr [rsp+64]",status
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lgg_terminal_word]
 call strcmp@PLT
 test eax,eax
 jnz .Lgg_stage_return_state
 mov rdi,[rsp+208]
 mov rsi,r12
 mov rdx,[rsp+160]
 mov ecx,[rsp+192]
 mov r8,[rbx+4256]
 lea r9,[rsp+168]
 call pg_durable_inputs_apply@PLT
 test eax,eax
 jnz .Lgg_stage_end
 mov rdi,[rsp+168]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lgg_stage_return_state
 mov rdi,[rsp+208]
 mov rsi,r12
 mov rdx,[rsp+200]
 mov rcx,[rsp+168]
 lea r8,[rsp+184]
 call pg_durable_inputs_start@PLT
 test eax,eax
 jnz .Lgg_stage_end
.Lgg_stage_return_state:
 mov rax,[rsp+64]
 mov [r13],rax
 mov qword ptr [rsp+64],0
 xor eax,eax
 jmp .Lgg_stage_end
.Lgg_stage_alloc:
 mov eax,5
.Lgg_stage_end:
 mov ebp,eax
 mov rdi,[rsp+160]
 call json_object_put@PLT
 mov rdi,[rsp+168]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov rdi,[rsp+72]
 call json_object_put@PLT
 mov rdi,[rsp+80]
 call json_object_put@PLT
 mov rdi,[rsp+88]
 call json_object_put@PLT
 mov rdi,[rsp+96]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,224
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lgg_child_initial:
 mov rax,[rdi]
 mov rax,[rax]
 mov rax,[rax+48]
 mov rdi,[rax+24]
 jmp [rax+32]
.Lgg_child_mint:
 mov rax,[rdi]
 mov rax,[rax]
 mov rcx,[rax+32]
 mov rdi,[rdi+8]
 mov rdi,[rdi+8]
 mov rdi,[rdi]
 jmp rcx
.Lgg_handoff:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,80
 mov rbx,rdi
 mov r12,rsi
 mov r13,rcx
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 movdqu [rsp+64],xmm0
 mov rax,[rbx]
 mov rax,[rax]
 mov r14,[rax+64]
 mov rax,[rbx+8]
 GGET "qword ptr [rax+32]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+32],rax
 mov rdi,r14
 mov rsi,r12
 mov rdx,rax
 mov rcx,rsp
 call pg_durable_inputs_prepare@PLT
 test eax,eax
 jnz .Lgg_handoff_end
 mov rdi,r14
 mov rsi,r12
 mov rdx,[rsp]
 xor ecx,ecx
 xor r8d,r8d
 lea r9,[rsp+16]
 call pg_durable_inputs_apply@PLT
 test eax,eax
 jnz .Lgg_handoff_end
 cmp dword ptr [rsp+24],0
 je .Lgg_handoff_start
 mov rdi,[rsp+16]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lgg_handoff_state
.Lgg_handoff_start:
 mov rdi,r14
 mov rsi,r12
 mov rdx,[rsp+32]
 mov rcx,[rsp+16]
 lea r8,[rsp+40]
 call pg_durable_inputs_start@PLT
 test eax,eax
 jnz .Lgg_handoff_end
.Lgg_handoff_state:
 lea rdi,[rip+.Lgg_terminal_json]
 call json_tokener_parse@PLT
 mov [rsp+48],rax
 test rax,rax
 jz .Lgg_handoff_alloc
 GGET rax,outcome
 GGET rax,result
 mov r14,rax
 mov rax,[rbx+8]
 GGET "qword ptr [rax+32]",state
 GGET rax,checkpoint
 GGET rax,assistant
 mov rdi,r14
 lea rsi,[rip+.Lgg_entry_id]
 mov rdx,rax
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_handoff_end
 mov rax,[rsp+48]
 mov [r13],rax
 mov qword ptr [rsp+48],0
 xor eax,eax
 jmp .Lgg_handoff_end
.Lgg_handoff_alloc:
 mov eax,5
.Lgg_handoff_end:
 mov ebp,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,80
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lgg_tools:
 push rbx
 mov rbx,rsi
 call .Lgg_operation
 test eax,eax
 jnz .Lgg_tools_error
 mov rdi,rbx
 mov rsi,[rbx+56]
 lea rdx,[rip+.Lgg_handoff]
 call pg_task_runner_commit_with@PLT
.Lgg_tools_error:
 neg eax
 pop rbx
 ret
.Lgg_abort_stage:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rcx
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 mov [rsp+32],rdx
 mov rax,[rbx+40]
 mov rax,[rax+24]
 mov rax,[rax]
 mov r14,[rax+64]
 GGET "qword ptr [rbx+32]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rdi,r14
 mov rsi,r12
 mov rdx,rax
 mov rcx,rsp
 call pg_durable_inputs_prepare@PLT
 test eax,eax
 jnz .Lgg_abort_stage_end
 mov rdi,rbx
 mov rsi,r12
 mov rdx,[rsp+32]
 mov ecx,3
 xor r8d,r8d
 xor r9d,r9d
 call .Lgg_live_apply
 test eax,eax
 jnz .Lgg_abort_stage_end
 mov rdi,r14
 mov rsi,r12
 mov rdx,[rsp]
 mov ecx,3
 cmp qword ptr [rbx+48],0
 jne .Lgg_end_boundary_ready
 GGET "qword ptr [rbx+24]",abort_requested
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov ecx,3
 test eax,eax
 jnz .Lgg_end_boundary_ready
 mov rax,[rbx+24]
 GGET rax,restarts
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rbx+64]
 mov ecx,3
 jb .Lgg_end_boundary_ready
 mov ecx,5
.Lgg_end_boundary_ready:
 mov [rsp+40],ecx
 mov rdi,r14
 mov rsi,r12
 mov rdx,[rsp]
 xor r8d,r8d
 lea r9,[rsp+8]
 call pg_durable_inputs_apply@PLT
 test eax,eax
 jnz .Lgg_abort_stage_end
 lea rdi,[rip+.Lgg_abort_json]
 cmp dword ptr [rsp+40],5
 jne .Lgg_end_json_ready
 lea rdi,[rip+.Lgg_cutoff_json]
.Lgg_end_json_ready:
 call json_tokener_parse@PLT
 mov [r13],rax
 test rax,rax
 jz .Lgg_abort_stage_alloc
 xor eax,eax
 jmp .Lgg_abort_stage_end
.Lgg_abort_stage_alloc:
 mov eax,5
.Lgg_abort_stage_end:
 mov ebp,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lgg_abort:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rsi
 mov r12,rdi
 call .Lgg_operation
 test eax,eax
 jnz .Lgg_abort_error
 GGET "qword ptr [rbx+32]",state
 GGET rax,checkpoint
 mov [rsp],rax
 GGET rax,handle
 test rax,rax
 jz .Lgg_abort_commit
 mov rax,[r12]
 cmp qword ptr [rax+88],0
 je .Lgg_abort_commit
 mov rsi,[rbx+56]
 cmp dword ptr [rsi+80],0
 jne .Lgg_abort_cancel
 GGET "qword ptr [rsp]",configuration
 mov rsi,rax
 mov rax,[r12]
 mov rdi,[rax]
 mov rdx,[rbx+56]
 add rdx,32
 call [rax+16]
 cmp eax,1
 je .Lgg_abort_pending
 test eax,eax
 jnz .Lgg_abort_commit
 mov rsi,[rbx+56]
 mov dword ptr [rsi+80],1
.Lgg_abort_cancel:
 GGET "qword ptr [rsp]",handle
 mov rdx,rax
 mov rax,[r12]
 mov rdi,[rax]
 mov rsi,[rbx+56]
 add rsi,32
 call [rax+88]
 cmp eax,1
 je .Lgg_abort_pending
 test eax,eax
 jz .Lgg_abort_commit
 mov rax,[rbx+8]
 cmp qword ptr [rax+48],0
 je .Lgg_abort_commit
 mov rdi,[rax]
 mov rsi,[rbx+16]
 mov edx,-1
 call [rax+48]
.Lgg_abort_commit:
 mov rdi,rbx
 mov rsi,rbx
 lea rdx,[rip+.Lgg_abort_stage]
 call pg_task_runner_commit_with@PLT
.Lgg_abort_error:
 neg eax
 jmp .Lgg_abort_return
.Lgg_abort_pending:
 mov eax,1
.Lgg_abort_return:
 add rsp,8
 pop r12
 pop rbx
 ret
/* Model failures are committed facts; an operation owns the candidate through
   the assistant/state transaction and may retry its commit without new IO. */
.Lgg_failure:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,80
 mov r12,rdi
 mov rbx,rsi
 mov r14,[rsi+56]
 lea rax,[rip+.Lgg_default_failure]
 mov [r14+4320],rax
 mov qword ptr [r14+4328],20
 mov rax,[r12]
 cmp qword ptr [rax+56],0
 je .Lgg_failure_described
 mov rdi,[rax]
 lea rsi,[r14+32]
 lea rdx,[r14+4320]
 call [rax+56]
.Lgg_failure_described:
 cmp qword ptr [r14+4320],0
 je .Lgg_failure_invalid
 cmp qword ptr [r14+4328],4096
 jae .Lgg_failure_invalid
 mov rdi,[r14+4320]
 mov esi,[r14+4328]
 call json_object_new_string_len@PLT
 mov rdi,[r14+24]
 lea rsi,[rip+.Lgg_error_message]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_failure_error
 lea rdi,[rip+.Lgg_error_word]
 call json_object_new_string@PLT
 mov rdi,[r14+24]
 lea rsi,[rip+.Lgg_stop_reason]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_failure_error
 /* Resolve current policy; the next prepare obtains fresh request settings. */
 GGET "qword ptr [rbx+32]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov rsi,rax
 mov rax,[r12]
 mov rdi,[rax]
 lea rdx,[r14+4352]
 call [rax+8]
 test eax,eax
 jnz .Lgg_failure_invalid
 GGET "qword ptr [r14+4352]",retry_key
 mov r13,rax
 mov qword ptr [rsp+16],3
 mov qword ptr [rsp+24],2000
 mov qword ptr [rsp+32],60000
 mov dword ptr [rsp+40],1
 test r13,r13
 jz .Lgg_failure_policy
 GGET r13,enabled
 test rax,rax
 jz .Lgg_failure_max
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov [rsp+40],eax
.Lgg_failure_max:
 GGET r13,max_retries
 test rax,rax
 jz .Lgg_failure_base
 mov rdi,rax
 call json_object_get_int64@PLT
 test rax,rax
 js .Lgg_failure_invalid
 mov [rsp+16],rax
.Lgg_failure_base:
 GGET r13,base_delay
 test rax,rax
 jz .Lgg_failure_cap
 mov rdi,rax
 call json_object_get_int64@PLT
 test rax,rax
 js .Lgg_failure_invalid
 mov [rsp+24],rax
.Lgg_failure_cap:
 GGET r13,max_delay
 test rax,rax
 jz .Lgg_failure_policy
 mov rdi,rax
 call json_object_get_int64@PLT
 test rax,rax
 js .Lgg_failure_invalid
 mov [rsp+32],rax
.Lgg_failure_policy:
 GGET "qword ptr [rbx+32]",state
 GGET rax,checkpoint
 GGET rax,attempt
 mov rdi,rax
 call json_object_get_uint64@PLT
 test rax,rax
 jnz .Lgg_failure_attempt
 mov eax,1
.Lgg_failure_attempt:
 mov [rsp+8],rax
 cmp dword ptr [r14+4336],0
 je .Lgg_failure_terminal
 cmp dword ptr [rsp+40],0
 je .Lgg_failure_terminal
 cmp rax,[rsp+16]
 ja .Lgg_failure_terminal
 mov rcx,rax
 dec rcx
 mov rax,[rsp+24]
 mov rdx,[rsp+32]
 cmp rax,rdx
 cmova rax,rdx
 test rax,rax
 jz .Lgg_failure_delay
.Lgg_failure_double:
 test rcx,rcx
 jz .Lgg_failure_delay
 cmp rax,rdx
 jae .Lgg_failure_delay
 shl rax,1
 jc .Lgg_failure_saturate
 cmp rax,rdx
 cmova rax,rdx
 dec rcx
 jmp .Lgg_failure_double
.Lgg_failure_saturate:
 mov rax,rdx
.Lgg_failure_delay:
 mov [rsp+24],rax
 mov rax,[rbx+8]
 mov rdi,[rax]
 call [rax+40]
 cvtsi2sd xmm1,qword ptr [rsp+24]
 addsd xmm0,xmm1
 movsd [rsp+56],xmm0
 lea rdi,[rip+.Lgg_retry_json]
 call json_tokener_parse@PLT
 mov [r14+4360],rax
 test rax,rax
 jz .Lgg_failure_alloc
 GGET rax,checkpoint
 mov rbp,rax
 mov rdi,[rsp+8]
 call json_object_new_int64@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lgg_attempt]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_failure_error
 movsd xmm0,[rsp+56]
 call json_object_new_double@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lgg_until]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_failure_error
 movsd xmm0,[rsp+56]
 call json_object_new_double@PLT
 mov rdi,[r14+4360]
 lea rsi,[rip+.Lgg_until]
 mov rdx,rax
 call .Lgg_set
 jmp .Lgg_failure_error
.Lgg_failure_terminal:
 lea rdi,[rip+.Lgg_failed_json]
 call json_tokener_parse@PLT
 mov [r14+4360],rax
 test rax,rax
 jz .Lgg_failure_alloc
 GGET rax,outcome
 GGET rax,error_key
 mov rbp,rax
 GGET "qword ptr [r14+24]",error_message
 mov rdi,rbp
 lea rsi,[rip+.Lgg_message_key]
 mov rdx,rax
 call .Lgg_copy_field
 jmp .Lgg_failure_error
.Lgg_failure_invalid:
 mov eax,4
 jmp .Lgg_failure_error
.Lgg_failure_alloc:
 mov eax,5
.Lgg_failure_error:
 neg eax
 add rsp,80
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lgg_retry:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rsi
 mov qword ptr [rsp],0
 GGET "qword ptr [rsi+32]",state
 GGET rax,checkpoint
 mov r12,rax
 GGET rax,until
 mov rdi,rax
 call json_object_get_double@PLT
 movsd [rsp+8],xmm0
 mov rax,[rbx+8]
 mov rdi,[rax]
 call [rax+40]
 ucomisd xmm0,[rsp+8]
 jb .Lgg_retry_early
 lea rdi,[rip+.Lgg_reprepare_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgg_retry_alloc
 GGET rax,checkpoint
 mov r13,rax
 GGET r12,attempt
 mov rdi,rax
 call json_object_get_uint64@PLT
 inc rax
 mov rdi,rax
 call json_object_new_int64@PLT
 mov rdi,r13
 lea rsi,[rip+.Lgg_attempt]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_retry_error
 jmp .Lgg_retry_commit
.Lgg_retry_early:
 lea rdi,[rip+.Lgg_retry_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgg_retry_alloc
 mov rdi,rax
 lea rsi,[rip+.Lgg_checkpoint]
 mov rdx,r12
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_retry_error
 movsd xmm0,[rsp+8]
 call json_object_new_double@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lgg_until]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_retry_error
.Lgg_retry_commit:
 mov rdi,rbx
 mov rsi,[rsp]
 call pg_task_runner_commit@PLT
 jmp .Lgg_retry_error
.Lgg_retry_alloc:
 mov eax,5
.Lgg_retry_error:
 mov ebp,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 neg eax
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.section .rodata
.Lgg_name: .asciz "pangu.generation"
.Lgg_prepare_name: .asciz "prepare"
.Lgg_request_name: .asciz "request"
.Lgg_tools_name: .asciz "tools"
.Lgg_name_key: .asciz "name"
.Lgg_arguments: .asciz "arguments"
.Lgg_user: .asciz "user"
.Lgg_assistant: .asciz "assistant"
.Lgg_tool_result: .asciz "toolResult"
.Lgg_tool_call: .asciz "toolCall"
.Lgg_text: .asciz "text"
.Lgg_type: .asciz "type"
.Lgg_id: .asciz "id"
.Lgg_model: .asciz "model"
.Lgg_content: .asciz "content"
.Lgg_role: .asciz "role"
.Lgg_state: .asciz "state"
.Lgg_checkpoint: .asciz "checkpoint"
.Lgg_configuration: .asciz "configuration"
.Lgg_cutoff: .asciz "cutoff"
.Lgg_conversation_id: .asciz "conversationId"
.Lgg_system_prompt: .asciz "systemPrompt"
.Lgg_tool_call_id: .asciz "toolCallId"
.Lgg_tool_name: .asciz "toolName"
.Lgg_is_error: .asciz "isError"
.Lgg_outcome: .asciz "outcome"
.Lgg_result: .asciz "result"
.Lgg_entry_id: .asciz "entryId"
.Lgg_ownership: .asciz "ownership"
.Lgg_task_id: .asciz "taskId"
.Lgg_call_id: .asciz "callId"
.Lgg_tools_key: .asciz "tools"
.Lgg_on: .asciz "on"
.Lgg_retry_name: .asciz "retry"
.Lgg_attempt: .asciz "attempt"
.Lgg_until: .asciz "until"
.Lgg_retry_key: .asciz "retry"
.Lgg_enabled: .asciz "enabled"
.Lgg_max_retries: .asciz "maxRetries"
.Lgg_base_delay: .asciz "baseDelayMs"
.Lgg_max_delay: .asciz "maxAgentDelayMs"
.Lgg_error_key: .asciz "error"
.Lgg_message_key: .asciz "message"
.Lgg_error_message: .asciz "errorMessage"
.Lgg_stop_reason: .asciz "stopReason"
.Lgg_error_word: .asciz "error"
.Lgg_aborted_word: .asciz "aborted"
.Lgg_deferred_word: .asciz "deferred"
.Lgg_default_failure: .asciz "Model request failed"
.Lgg_failed_json: .asciz "{\"status\":\"terminal\",\"outcome\":{\"status\":\"failed\",\"error\":{\"message\":\"Model request failed\",\"detail\":{\"reason\":\"model_error\"}}}}"
.Lgg_retry_json: .asciz "{\"status\":\"sleeping\",\"checkpoint\":{\"phase\":\"retry\",\"attempt\":1,\"until\":0},\"until\":0}"
.Lgg_reprepare_json: .asciz "{\"status\":\"running\",\"checkpoint\":{\"phase\":\"prepare\",\"attempt\":1}}"
.Lgg_initial_json: .asciz "{\"phase\":\"prepare\",\"attempt\":1}"
.Lgg_request_json: .asciz "{\"status\":\"running\",\"checkpoint\":{\"phase\":\"request\",\"attempt\":1}}"
.Lgg_assistant_json: .asciz "{\"role\":\"assistant\",\"content\":[]}"
.Lgg_text_json: .asciz "{\"type\":\"text\",\"text\":\"\"}"
.Lgg_call_json: .asciz "{\"type\":\"toolCall\"}"
.Lgg_entry_json: .asciz "{\"kind\":\"pangu.assistant\",\"model\":[]}"
.Lgg_head: .asciz "head"
.Lgg_status: .asciz "status"
.Lgg_terminal_word: .asciz "terminal"
.Lgg_terminal_json: .asciz "{\"status\":\"terminal\",\"outcome\":{\"status\":\"completed\",\"result\":{\"entryId\":0}}}"
.Lgg_tool_definition_json: .asciz "{\"name\":\"pangu.tool\",\"version\":2}"
.Lgg_tool_options_json: .asciz "{\"ownership\":{\"kind\":\"task\",\"taskId\":0}}"
.Lgg_tool_input_json: .asciz "{\"kind\":\"model\",\"assistant\":0,\"callId\":\"\"}"
.Lgg_wait_json: .asciz "{\"status\":\"waiting\",\"checkpoint\":{\"phase\":\"tools\",\"pending\":[]},\"on\":[],\"policy\":\"allSettled\"}"
.Lgg_abort_json: .asciz "{\"status\":\"terminal\",\"outcome\":{\"status\":\"aborted\"}}"
.text
.globl pg_durable_generation_history
.type pg_durable_generation_history,@function
pg_durable_generation_history:
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
 mov rdi,rsp
 mov ecx,18
 xor eax,eax
 cld
 rep stosq
 mov [rsp+8],rbx
 lea rdi,[rip+.Lgg_history_json]
 call json_tokener_parse@PLT
 mov [rsp+120],rax
 test rax,rax
 jz .Lgg_history_alloc
 mov rdi,r12
 call json_object_new_int64@PLT
 mov rdi,[rsp+120]
 lea rsi,[rip+.Lgg_conversation_id]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_history_result
 mov rax,[rsp+120]
 mov [rsp+32],rax
 mov edi,1
 mov esi,4384
 call calloc@PLT
 mov [rsp+128],rax
 test rax,rax
 jz .Lgg_history_alloc
 mov [rax+8],rsp
 mov [rax+16],r13
 mov rdi,rax
 call .Lgg_project
 test eax,eax
 jnz .Lgg_history_result
 mov rax,[rsp+128]
 mov rdi,r14
 mov rsi,[rax+72]
 mov edx,PG_S_BYTES
 call memcpy@PLT
 xor r12d,r12d
 xor r13d,r13d
.Lgg_history_visible:
 cmp r12,[r14+PG_S_COUNT]
 jae .Lgg_history_visible_done
 imul rax,r12,PG_MSG_BYTES
 cmp dword ptr [r14+rax],4
 je .Lgg_history_visible_next
 cmp r12,r13
 je .Lgg_history_visible_keep
 lea rsi,[r14+rax]
 imul rax,r13,PG_MSG_BYTES
 lea rdi,[r14+rax]
 mov edx,PG_MSG_BYTES
 call memmove@PLT
.Lgg_history_visible_keep:
 inc r13
.Lgg_history_visible_next:
 inc r12
 jmp .Lgg_history_visible
.Lgg_history_visible_done:
 mov [r14+PG_S_COUNT],r13
 xor eax,eax
 jmp .Lgg_history_result
.Lgg_history_alloc:
 mov eax,5
.Lgg_history_result:
 mov ebp,eax
 mov rax,[rsp+128]
 test rax,rax
 jz .Lgg_history_release
 mov rdi,[rax+72]
 call free@PLT
 mov rdi,[rsp+128]
 call free@PLT
.Lgg_history_release:
 mov rdi,[rsp+120]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,144
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_durable_generation_history,.-pg_durable_generation_history
.section .rodata
.Lgg_history_json: .asciz "{\"conversationId\":1,\"state\":{\"checkpoint\":{\"cutoff\":9007199254740991}}}"
.section .note.GNU-stack,"",@progbits

.text
/* OpenAI-completions joins a tool result's text blocks with LF. */
.Lgg_join_tool_text:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 xor r12d,r12d
 xor r13d,r13d
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov edi,1
 mov esi,4096
 call calloc@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgj_end
.Lgj_loop:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lgj_finish
 mov rdi,rbx
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov r14,rax
 GGET r14,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lgj_end
 mov rdi,rax
 lea rsi,[rip+.Lgg_text]
 call strcmp@PLT
 test eax,eax
 jnz .Lgj_end
 GGET r14,text
 mov rbp,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lgj_end
 mov rdi,rbp
 call json_object_get_string_len@PLT
 mov [rsp+16],rax
 lea rcx,[r13+rax]
 test r12,r12
 jz .Lgj_size
 inc rcx
.Lgj_size:
 cmp rcx,4096
 jae .Lgj_end
 test r12,r12
 jz .Lgj_copy
 mov rax,[rsp]
 mov byte ptr [rax+r13],10
 inc r13
.Lgj_copy:
 mov rdi,rbp
 call json_object_get_string@PLT
 mov rsi,rax
 mov rdi,[rsp]
 add rdi,r13
 mov rdx,[rsp+16]
 call memcpy@PLT
 add r13,[rsp+16]
 inc r12
 jmp .Lgj_loop
.Lgj_finish:
 mov rdi,[rsp]
 mov esi,r13d
 call json_object_new_string_len@PLT
 mov [rsp+8],rax
.Lgj_end:
 mov rdi,[rsp]
 call free@PLT
 mov rax,[rsp+8]
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.section .rodata
.Lgg_thinking: .asciz "thinking"
.Lgg_thinking_signature: .asciz "thinkingSignature"

.text
.Lgg_cutoff_handler:
 jmp .Lgg_abort
.section .rodata
.Lgg_restarts: .asciz "restarts"
.Lgg_cutoff_json: .asciz "{\"status\":\"terminal\",\"outcome\":{\"status\":\"failed\",\"error\":{\"message\":\"Generation reached the consecutive restart limit\",\"detail\":{\"reason\":\"restart_limit\"}}}}"

.section .rodata
.Lgg_abort_requested: .asciz "abortRequested"

/* Pi generation streamResponse/convertPartial; conversation-scoped trailing
   text deltas use Session deferred writes. Final facts clear the same draft. */
.text
.Lgg_live_initial:
 push rbx
 mov rbx,rdx
 call json_object_new_object@PLT
 mov [rbx],rax
 test rax,rax
 setz al
 movzx eax,al
 pop rbx
 ret
/* runner, records, documents, mode(reset/append/clear/abort), delta, bytes */
.Lgg_live_apply:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,208
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov [rsp+160],ecx
 mov [rsp+168],r9
 mov [rsp+176],r8
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
 movdqu [rsp+144],xmm0
 mov qword ptr [rsp+184],0
 lea rax,[rip+.Lgg_live_kind]
 mov [rsp],rax
 mov qword ptr [rsp+8],10
 GGET "qword ptr [rbx+32]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+32],rax
 mov dword ptr [rsp+40],1
 lea rax,[rip+.Lgg_live_definition]
 mov [rsp+48],rax
 lea rax,[rsp]
 mov [rsp+56],rax
 lea rax,[rip+.Lgg_live_initializer]
 mov [rsp+72],rax
 mov rax,[rbx+40]
 mov rax,[rax+24]
 mov rax,[rax]
 mov rax,[rax+32]
 mov [rsp+80],rax
 mov rdi,r13
 lea rsi,[rsp+48]
 lea rdx,[rsp+112]
 call pg_document_tx_doc@PLT
 test eax,eax
 jnz .Lgg_live_bad
 mov rdi,[rsp+112]
 lea rsi,[rsp+120]
 call pg_document_change_snapshot@PLT
 test eax,eax
 jnz .Lgg_live_bad
 GGET "qword ptr [rsp+120]",generation
 GGET rax,message
 mov [rsp+136],rax
 cmp dword ptr [rsp+160],1
 je .Lgg_live_append
 cmp dword ptr [rsp+160],2
 je .Lgg_live_clear
 test rax,rax
 jz .Lgg_live_clear
 # Detach before changing the partial's terminal reason.
 mov rdi,rax
 lea rsi,[rsp+144]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lgg_live_alloc
 lea rdi,[rip+.Lgg_aborted_word]
 call json_object_new_string@PLT
 mov rdi,[rsp+144]
 lea rsi,[rip+.Lgg_stop_reason]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_live_end
 lea rdi,[rip+.Lgg_entry_json]
 call json_tokener_parse@PLT
 mov [rsp+152],rax
 test rax,rax
 jz .Lgg_live_alloc
 mov rdi,[rsp+144]
 call json_object_get@PLT
 mov r14,rax
 GGET "qword ptr [rsp+152]",model
 mov rdi,rax
 mov rsi,r14
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lgg_live_array_error
 mov rdi,r12
 mov rsi,[rsp+32]
 mov rdx,[rsp+152]
 mov rcx,[rbx+16]
 lea r9,[rsp+184]
 mov rax,[rbx+40]
 mov rax,[rax+24]
 mov rax,[rax]
 mov r8,[rax+32]
 call pg_record_tx_append_entry@PLT
 test eax,eax
 jnz .Lgg_live_bad
.Lgg_live_clear:
 lea rdi,[rip+.Lgg_live_clear_json]
 call json_tokener_parse@PLT
 mov [rsp+128],rax
 test rax,rax
 jz .Lgg_live_alloc
 jmp .Lgg_live_edit
.Lgg_live_append:
 cmp qword ptr [rsp+136],0
 jne .Lgg_live_append_existing
 lea rdi,[rip+.Lgg_live_start_json]
 call json_tokener_parse@PLT
 mov [rsp+128],rax
 test rax,rax
 jz .Lgg_live_alloc
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov esi,2
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,[rbx+16]
 call json_object_new_uint64@PLT
 mov rdi,r14
 lea rsi,[rip+.Lgg_task_id]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_live_end
 GGET r14,message
 GGET rax,content
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,[rsp+176]
 mov esi,[rsp+168]
 call json_object_new_string_len@PLT
 mov rdi,r14
 lea rsi,[rip+.Lgg_text]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_live_end
 jmp .Lgg_live_edit
.Lgg_live_append_existing:
 lea rdi,[rip+.Lgg_live_append_json]
 call json_tokener_parse@PLT
 mov [rsp+128],rax
 test rax,rax
 jz .Lgg_live_alloc
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,[rsp+176]
 mov esi,[rsp+168]
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Lgg_live_alloc
 mov rdi,r14
 mov esi,2
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Lgg_live_alloc
.Lgg_live_edit:
 mov rdi,[rsp+112]
 mov rsi,[rsp+128]
 call pg_document_change_append@PLT
 test eax,eax
 jnz .Lgg_live_bad
 xor eax,eax
 jmp .Lgg_live_end
.Lgg_live_array_error:
 mov rdi,r14
 call json_object_put@PLT
.Lgg_live_alloc:
 mov eax,5
 jmp .Lgg_live_end
.Lgg_live_bad:
 mov eax,4
.Lgg_live_end:
 mov ebp,eax
 mov rdi,[rsp+120]
 call json_object_put@PLT
 mov rdi,[rsp+128]
 call json_object_put@PLT
 mov rdi,[rsp+144]
 call json_object_put@PLT
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov rdi,[rsp+184]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,208
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* operation, reset or append. No phase transition or provider call occurs. */
.Lgg_live_write:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,112
 mov rbx,rdi
 mov r12d,esi
 mov r13,[rbx+8]
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 movdqu [rsp+64],xmm0
 movdqu [rsp+80],xmm0
 mov qword ptr [rsp+96],0
 mov rax,[r13+8]
 mov rdi,[rax]
 call [rax+40]
 movsd [rsp+104],xmm0
 test r12d,r12d
 jz .Lgg_live_write_begin
 mov rax,[rbx+4192]
 cmp rax,[rbx+4392]
 je .Lgg_live_write_ok
 cmp qword ptr [rbx+4392],0
 je .Lgg_live_write_begin
 subsd xmm0,[rbx+4384]
 ucomisd xmm0,[rip+.Lgg_live_interval]
 jb .Lgg_live_write_ok
.Lgg_live_write_begin:
 mov r14,[r13]
 mov rax,[r13+8]
 mov rdi,r14
 mov rsi,rsp
 lea rdx,[rsp+48]
 mov rcx,[rax]
 mov r8,[rax+8]
 call pg_durable_session_begin@PLT
 test eax,eax
 jnz .Lgg_live_write_bad
 mov rdi,r13
 mov rsi,rsp
 lea rdx,[rsp+48]
 mov ecx,r12d
 mov rax,[rbx+4392]
 lea r8,[rbx+rax+96]
 mov r9,[rbx+4192]
 sub r9,rax
 call .Lgg_live_apply
 test eax,eax
 jnz .Lgg_live_write_abort
 mov rdi,r14
 mov esi,r12d
 mov edx,1000
 xor ecx,ecx
 call pg_durable_session_settle@PLT
 test eax,eax
 jnz .Lgg_live_write_bad
 mov rax,[rsp+104]
 mov [rbx+4384],rax
 mov rax,[rbx+4192]
 mov [rbx+4392],rax
.Lgg_live_write_ok:
 xor eax,eax
 jmp .Lgg_live_write_end
.Lgg_live_write_abort:
 mov ebp,eax
 mov rdi,r14
 call pg_durable_session_abort@PLT
 mov eax,ebp
 jmp .Lgg_live_write_end
.Lgg_live_write_bad:
 mov eax,6
.Lgg_live_write_end:
 add rsp,112
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lgg_live_kind: .asciz "pangu.live"
.Lgg_generation: .asciz "generation"
.Lgg_message: .asciz "message"
.Lgg_live_clear_json: .asciz "[[\"d\",[\"generation\"]]]"
.Lgg_live_start_json: .asciz "[[\"s\",[\"generation\"],{\"taskId\":0,\"message\":{\"role\":\"assistant\",\"content\":[{\"type\":\"text\",\"text\":\"\"}]}}]]"
.Lgg_live_append_json: .asciz "[[\"a\",[\"generation\",\"message\",\"content\",0,\"text\"],\"\"]]"
.balign 8
.Lgg_live_interval: .double 100.0
.section .data.rel.ro
.balign 8
.Lgg_live_definition:
 .quad .Lgg_live_kind,10,1
 .long 1,1,2,0
 .quad 0,0
.Lgg_live_initializer:
 .quad 0,.Lgg_live_initial

.text
.Lgg_poll:
 jmp .Lgg_request
/* Re-sleep an early wake without invoking the provider. */
.Lgg_poll_wait:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov qword ptr [rsp],0
 GGET "qword ptr [rbx+32]",state
 GGET rax,checkpoint
 mov r12,rax
 GGET rax,poll_at
 test rax,rax
 jz .Lgg_poll_wait_ready
 mov rdi,rax
 call json_object_get_double@PLT
 movsd [rsp+8],xmm0
 mov rax,[rbx+8]
 mov rdi,[rax]
 call [rax+40]
 ucomisd xmm0,[rsp+8]
 jae .Lgg_poll_wait_ready
 lea rdi,[rip+.Lgg_retry_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgg_poll_wait_alloc
 mov rdi,rax
 lea rsi,[rip+.Lgg_checkpoint]
 mov rdx,r12
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_poll_wait_error
 movsd xmm0,[rsp+8]
 call json_object_new_double@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lgg_until]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_poll_wait_error
 mov rdi,rbx
 mov rsi,[rsp]
 call pg_task_runner_commit@PLT
 test eax,eax
 jnz .Lgg_poll_wait_error
 mov r12d,1
 jmp .Lgg_poll_wait_return
.Lgg_poll_wait_alloc:
 mov eax,5
.Lgg_poll_wait_error:
 neg eax
 mov r12d,eax
 jmp .Lgg_poll_wait_return
.Lgg_poll_wait_ready:
 xor r12d,r12d
.Lgg_poll_wait_return:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,r12d
 add rsp,24
 pop r12
 pop rbx
 ret
/* Persist provider handle with the pinned request configuration and cutoff. */
.Lgg_defer:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,[rdi+8]
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov rax,[rbx]
 mov rax,[rax]
 cmp qword ptr [rax+80],0
 je .Lgg_defer_invalid
 GGET "qword ptr [rbx+24]",stop_reason
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lgg_defer_invalid
 mov rdi,rax
 lea rsi,[rip+.Lgg_deferred_word]
 call strcmp@PLT
 test eax,eax
 jnz .Lgg_defer_invalid
 GGET "qword ptr [r12+32]",state
 GGET rax,checkpoint
 mov rdi,rax
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lgg_defer_alloc
 GGET "qword ptr [rbx+24]",deferred
 mov r13,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lgg_defer_invalid
 GGET r13,id
 mov rdi,rax
 call json_object_get_string_len@PLT
 test eax,eax
 jle .Lgg_defer_invalid
 GGET r13,poll_after
 test rax,rax
 jz .Lgg_defer_default
 mov r14,rax
 mov rdi,rax
 call json_object_get_type@PLT
 cmp eax,2
 je .Lgg_defer_numeric
 cmp eax,3
 jne .Lgg_defer_invalid
.Lgg_defer_numeric:
 mov rdi,r14
 call json_object_get_double@PLT
 xorpd xmm1,xmm1
 ucomisd xmm0,xmm1
 jb .Lgg_defer_invalid
 jp .Lgg_defer_invalid
 jmp .Lgg_defer_delay
.Lgg_defer_default:
 movsd xmm0,[rip+.Lgg_poll_default]
.Lgg_defer_delay:
 movsd [rsp+16],xmm0
 mov rax,[r12+8]
 mov rdi,[rax]
 call [rax+40]
 addsd xmm0,[rsp+16]
 movsd [rsp+16],xmm0
 GGET "qword ptr [rsp+8]",poll_at
 test rax,rax
 jz .Lgg_defer_time
 mov rdi,rax
 call json_object_get_double@PLT
 addsd xmm0,[rip+.Lgg_poll_one]
 maxsd xmm0,[rsp+16]
 movsd [rsp+16],xmm0
.Lgg_defer_time:
 movsd xmm0,[rsp+16]
 movq rax,xmm0
 shr rax,52
 and eax,0x7ff
 cmp eax,0x7ff
 je .Lgg_defer_invalid
 lea rdi,[rip+.Lgg_poll_name]
 call json_object_new_string@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lgg_phase]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_defer_end
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lgg_handle]
 mov rdx,r13
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_defer_end
 movsd xmm0,[rsp+16]
 call json_object_new_double@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lgg_poll_at]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_defer_end
 lea rdi,[rip+.Lgg_retry_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgg_defer_alloc
 mov rdi,rax
 lea rsi,[rip+.Lgg_checkpoint]
 mov rdx,[rsp+8]
 call .Lgg_copy_field
 test eax,eax
 jnz .Lgg_defer_end
 movsd xmm0,[rsp+16]
 call json_object_new_double@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lgg_until]
 mov rdx,rax
 call .Lgg_set
 test eax,eax
 jnz .Lgg_defer_end
 mov rdi,r12
 mov rsi,[rsp]
 call pg_task_runner_commit@PLT
 jmp .Lgg_defer_end
.Lgg_defer_alloc:
 mov eax,5
 jmp .Lgg_defer_end
.Lgg_defer_invalid:
 mov eax,4
.Lgg_defer_end:
 mov ebp,eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lgg_poll_name: .asciz "poll"
.Lgg_handle: .asciz "handle"
.Lgg_deferred: .asciz "deferred"
.Lgg_poll_at: .asciz "pollAt"
.Lgg_poll_after: .asciz "pollAfterMs"
.balign 8
.Lgg_poll_default: .double 5000.0
.Lgg_poll_one: .double 1.0

.section .rodata
.Lgg_phase: .asciz "phase"
