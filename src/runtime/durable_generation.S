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
 mov [rdi],rsi
 lea rax,[rip+.Lgg_name]
 mov [rdi+8],rax
 mov qword ptr [rdi+16],16
 mov qword ptr [rdi+24],1
 mov [rdi+32],rdi
 lea rax,[rip+.Lgg_initial]
 mov [rdi+40],rax
 lea rax,[rdi+88]
 mov [rdi+48],rax
 mov qword ptr [rdi+56],3
 lea rax,[rip+.Lgg_abort]
 mov [rdi+64],rax
 lea rax,[rip+.Lgg_cleanup]
 mov [rdi+72],rax
 mov qword ptr [rdi+80],0
 lea rax,[rip+.Lgg_prepare_name]
 mov [rdi+88],rax
 mov qword ptr [rdi+96],7
 mov [rdi+104],rdi
 lea rax,[rip+.Lgg_prepare]
 mov [rdi+112],rax
 lea rax,[rip+.Lgg_cleanup]
 mov [rdi+120],rax
 lea rax,[rip+.Lgg_request_name]
 mov [rdi+128],rax
 mov qword ptr [rdi+136],7
 mov [rdi+144],rdi
 lea rax,[rip+.Lgg_request]
 mov [rdi+152],rax
 lea rax,[rip+.Lgg_cleanup]
 mov [rdi+160],rax
 lea rax,[rip+.Lgg_tools_name]
 mov [rdi+168],rax
 mov qword ptr [rdi+176],5
 mov [rdi+184],rdi
 lea rax,[rip+.Lgg_tools]
 mov [rdi+192],rax
 lea rax,[rip+.Lgg_cleanup]
 mov [rdi+200],rax
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
 mov esi,4320
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
 jz .Lgg_project_role
 mov dword ptr [rsp+24],3
 mov rdi,r13
 lea rsi,[rip+.Lgg_tool_result]
 call strcmp@PLT
 test eax,eax
 jnz .Lgg_project_bad
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
 cmp dword ptr [rsp+24],3
 jne .Lgg_project_text
 GGET r12,tool_call_id
 mov [rsp+16],rax
 GGET r12,tool_name
 mov rbp,rax
 GGET r12,is_error
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov r9d,eax
 mov rcx,[rsp+16]
 mov r8,rbp
.Lgg_project_text:
 mov rsi,[rsp+8]
 mov edx,[rsp+24]
 jmp .Lgg_project_one
.Lgg_project_call:
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
 xor r12d,r12d
.Lgg_context_entry:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lgg_context_ok
 mov rdi,[rsp]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 GGET rax,model
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
 mov rdi,[r14+32]
 mov rsi,[r14+72]
 mov dword ptr [r14+84],1
 call [r14+40]
 test eax,eax
 jnz .Lgg_request_invalid
.Lgg_request_started:
 mov rdi,[r14+32]
 lea rsi,[r14+4200]
 call [r14+48]
 test eax,eax
 jnz .Lgg_request_invalid
 mov eax,[r14+4200]
 test eax,eax
 jz .Lgg_request_pending
 cmp eax,1
 je .Lgg_request_text
 cmp eax,2
 je .Lgg_request_call
 cmp eax,3
 je .Lgg_request_ready
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
 mov rax,[r14+64]
 test rax,rax
 jz .Lgg_request_ready_mark
 mov rdi,[r14+32]
 call rax
.Lgg_request_ready_mark:
 mov dword ptr [r14+84],0
 mov dword ptr [r14+88],1
.Lgg_request_settle:
 mov rdi,rbx
 mov rsi,r14
 lea rdx,[rip+.Lgg_stage]
 call pg_task_runner_commit_with@PLT
 test eax,eax
 jnz .Lgg_request_error
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
 sub rsp,160
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
 mov rax,[rsp+64]
 mov [r13],rax
 mov qword ptr [rsp+64],0
 xor eax,eax
 jmp .Lgg_stage_end
.Lgg_stage_alloc:
 mov eax,5
.Lgg_stage_end:
 mov ebp,eax
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
 add rsp,160
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
 lea rdi,[rip+.Lgg_generation_definition_json]
 call json_tokener_parse@PLT
 mov [rsp+48],rax
 test rax,rax
 jz .Lgg_handoff_alloc
 lea rdi,[rip+.Lgg_generation_options_json]
 call json_tokener_parse@PLT
 mov [rsp+56],rax
 test rax,rax
 jz .Lgg_handoff_alloc
 mov [rsp],rbx
 lea rcx,[rip+.Lgg_initial]
 mov [rsp+8],rcx
 lea rcx,[rip+.Lgg_child_mint]
 mov [rsp+16],rcx
 mov rax,[rbx+8]
 GGET "qword ptr [rax+24]",conversation_id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+32],rax
 lea rdi,[rip+.Lgg_empty_json]
 call json_tokener_parse@PLT
 mov [rsp+72],rax
 test rax,rax
 jz .Lgg_handoff_alloc
 mov rdi,r12
 mov rsi,[rsp+48]
 mov rdx,[rsp+72]
 mov rcx,[rsp+56]
 mov r8,rsp
 lea r9,[rsp+40]
 call pg_record_tx_create_task@PLT
 test eax,eax
 jnz .Lgg_handoff_alloc
 lea rdi,[rip+.Lgg_terminal_json]
 call json_tokener_parse@PLT
 mov [rsp+64],rax
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
 mov rax,[rsp+64]
 mov [r13],rax
 mov qword ptr [rsp+64],0
 xor eax,eax
 jmp .Lgg_handoff_end
.Lgg_handoff_alloc:
 mov eax,5
.Lgg_handoff_end:
 mov ebp,eax
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov rdi,[rsp+72]
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
.Lgg_abort:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rsi
 lea rdi,[rip+.Lgg_abort_json]
 call json_tokener_parse@PLT
 mov r12,rax
 test rax,rax
 jz .Lgg_abort_alloc
 mov rdi,rbx
 mov rsi,rax
 call pg_task_runner_commit@PLT
 mov ebx,eax
 mov rdi,r12
 call json_object_put@PLT
 mov eax,ebx
 neg eax
 jmp .Lgg_abort_end
.Lgg_abort_alloc:
 mov eax,-5
.Lgg_abort_end:
 add rsp,8
 pop r12
 pop rbx
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
.Lgg_empty_json: .asciz "{}"
.Lgg_initial_json: .asciz "{\"phase\":\"prepare\",\"attempt\":1}"
.Lgg_request_json: .asciz "{\"status\":\"running\",\"checkpoint\":{\"phase\":\"request\",\"attempt\":1}}"
.Lgg_assistant_json: .asciz "{\"role\":\"assistant\",\"content\":[]}"
.Lgg_text_json: .asciz "{\"type\":\"text\",\"text\":\"\"}"
.Lgg_call_json: .asciz "{\"type\":\"toolCall\"}"
.Lgg_entry_json: .asciz "{\"kind\":\"pangu.assistant\",\"model\":[]}"
.Lgg_terminal_json: .asciz "{\"status\":\"terminal\",\"outcome\":{\"status\":\"completed\",\"result\":{\"entryId\":0}}}"
.Lgg_tool_definition_json: .asciz "{\"name\":\"pangu.tool\",\"version\":2}"
.Lgg_tool_options_json: .asciz "{\"ownership\":{\"kind\":\"task\",\"taskId\":0}}"
.Lgg_tool_input_json: .asciz "{\"kind\":\"model\",\"assistant\":0,\"callId\":\"\"}"
.Lgg_wait_json: .asciz "{\"status\":\"waiting\",\"checkpoint\":{\"phase\":\"tools\",\"pending\":[]},\"on\":[],\"policy\":\"allSettled\"}"
.Lgg_generation_definition_json: .asciz "{\"name\":\"pangu.generation\",\"version\":1}"
.Lgg_generation_options_json: .asciz "{\"ownership\":{\"kind\":\"conversation\"}}"
.Lgg_abort_json: .asciz "{\"status\":\"terminal\",\"outcome\":{\"status\":\"aborted\"}}"
.section .note.GNU-stack,"",@progbits
