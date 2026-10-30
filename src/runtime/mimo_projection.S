/* mimo_projection.S — MiMo 请求投影（encode_request）。
 *
 * 契约见 mimo.h/pg_session（session.h）：system/user/assistant/tool_call/tool_result
 * 投影为 OpenAI Chat Completions messages；assistant 文本与紧随的 tool_call 合并为
 * 一条带 tool_calls 的 assistant 消息；call/result 按 call_id 严格配对，孤儿或错配
 * 整体拒绝（返回 1），不静默修补。空 assistant 单独跳过；system 仅在非空时前置。
 * 生成的 request 字符串归 pg_mimo 所有（strdup），旧 request 先释放；失败路径不
 * 泄漏已构造的 json-c 对象树。工具 registry 为借用，只读取 name/description/schema。
 * json-c 经 PLT 调用；caller-saved 不跨调用保存任何活跃值。布局见 mimo_layout.h。
 */
#include "mimo_layout.h"
.intel_syntax noprefix

.section .rodata
.Lstr_pangu_model: .asciz "PANGU_MODEL_NAME"
.Lstr_env_model:     .asciz "MIMO_MODEL"
.Lstr_default_model: .asciz "mimo-v2.6-pro"
.Lstr_model:         .asciz "model"
.Lstr_stream:        .asciz "stream"
.Lstr_messages:      .asciz "messages"
.Lstr_max_tokens:    .asciz "max_tokens"
.Lstr_thinking:      .asciz "thinking"
.Lstr_type:          .asciz "type"
.Lstr_disabled:      .asciz "disabled"
.Lstr_role:          .asciz "role"
.Lstr_system:        .asciz "system"
.Lstr_user:          .asciz "user"
.Lstr_assistant:     .asciz "assistant"
.Lstr_tool:          .asciz "tool"
.Lstr_content:       .asciz "content"
.Lstr_tool_calls:    .asciz "tool_calls"
.Lstr_id:            .asciz "id"
.Lstr_function:      .asciz "function"
.Lstr_name:          .asciz "name"
.Lstr_arguments:     .asciz "arguments"
.Lstr_tool_call_id:  .asciz "tool_call_id"
.Lstr_description:   .asciz "description"
.Lstr_parameters:    .asciz "parameters"
.Lstr_tools:         .asciz "tools"

.section .text

/* ---- li_field(obj, key, value)：obj[key] = new_string(value)。文件内私有。 ---- */
.type li_field,@function
li_field:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 call json_object_new_string@PLT
 test rax,rax
 jz .Lfield_bad
 mov r13,rax
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Lfield_return
 mov rdi,r13
 call json_object_put@PLT
.Lfield_bad:
 mov eax,1
.Lfield_return:
 pop r13
 pop r12
 pop rbx
 ret
.size li_field, .-li_field

/* ---- int pg_mimo_encode_request(pg_mimo *m, const pg_session *s)
 * 栈帧 128 字节：[0]=root [8]=messages [16]=tools数组 [24]=tool [32]=fn
 * [40]=def/schema暂存 [48]=i/工具下标 [56]=calls [64]=call [72]=func
 * [80]=pair/registry [88]=iterator/count [96]=leaf [104]=owner bits
 * [112]=group start [120]=group end。
 * Owners: messages1/v2/calls4/call8/func16/tools32/tool64/fn128/thinking256/leaf512.
 * Clear an owner only after successful insertion into its parent. ---- */
.globl pg_mimo_encode_request
.type pg_mimo_encode_request,@function
pg_mimo_encode_request:
 push rbx
 push r12
 push r13
 push r14
 push r15                  /* 入口 rsp≡8；5 压栈后 ≡0 */
 sub rsp, 144
 mov rbx, rdi              /* m */
 mov r12, rsi              /* s */
 mov qword ptr [rsp + 104],0
 test rdi, rdi
 jz .Lret_bad
 test r12, r12
 jz .Lret_bad
 mov rax, qword ptr [r12 + PG_MM_SES_COUNT]
 cmp rax, PG_MM_MESSAGE_LIMIT
 ja .Lret_bad
 mov rax, qword ptr [r12 + PG_MM_SES_SYSTEM_SIZE]
 cmp rax, PG_MM_TEXT_LIMIT
 jae .Lret_bad
 /* free(m->request); m->request = 0；之后失败即整体无效 */
 mov rdi, qword ptr [rbx + PG_MM_REQUEST]
 call free@PLT
 mov qword ptr [rbx + PG_MM_REQUEST], 0
 call json_object_new_object@PLT
 test rax, rax
 jz .Lret_bad
 mov qword ptr [rsp], rax              /* root */
 call json_object_new_array@PLT
 test rax, rax
 jz .Linvalid
 mov qword ptr [rsp + 8], rax          /* messages */
 or qword ptr [rsp + 104],1
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 test rdi,rdi
 jz .Lconfigured_model_environment
 lea rsi,[rip+.Lstr_model]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 jmp 1f
.Lconfigured_model_environment:
 lea rdi,[rip+.Lstr_pangu_model]
 call getenv@PLT
 test rax,rax
 jnz 1f
 lea rdi, [rip + .Lstr_env_model]
 call getenv@PLT
 test rax, rax
 jnz 1f
 lea rax, [rip + .Lstr_default_model]
1:
 mov rdi, rax
 call json_object_new_string@PLT
 test rax,rax
 jz .Linvalid
 mov [rsp + 96],rax
 or qword ptr [rsp + 104],512
 mov rdx, rax
 mov rdi, qword ptr [rsp]
 lea rsi, [rip + .Lstr_model]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-513
 mov edi, 1
 call json_object_new_boolean@PLT
 test rax,rax
 jz .Linvalid
 mov [rsp + 96],rax
 or qword ptr [rsp + 104],512
 mov rdx, rax
 mov rdi, qword ptr [rsp]
 lea rsi, [rip + .Lstr_stream]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-513
 mov rdx, qword ptr [rsp + 8]
 mov rdi, qword ptr [rsp]
 lea rsi, [rip + .Lstr_messages]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-2
 mov edi, 512
 call json_object_new_int@PLT
 test rax,rax
 jz .Linvalid
 mov [rsp + 96],rax
 or qword ptr [rsp + 104],512
 mov rdx, rax
 mov rdi, qword ptr [rsp]
 lea rsi, [rip + .Lstr_max_tokens]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-513
 call json_object_new_object@PLT
 test rax, rax
 jz .Linvalid
 mov qword ptr [rsp + 80], rax         /* thinking */
 or qword ptr [rsp + 104],256
 mov rdi, rax
 lea rsi, [rip + .Lstr_type]
 lea rdx, [rip + .Lstr_disabled]
 call li_field
 test eax,eax
 jnz .Linvalid
 mov rdx, qword ptr [rsp + 80]
 mov rdi, qword ptr [rsp]
 lea rsi, [rip + .Lstr_thinking]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-257
 lea rdi,[rip+.Lopts_stream_usage_json]
 call json_tokener_parse@PLT
 test rax,rax
 jz .Linvalid
 mov [rsp+96],rax
 or qword ptr [rsp+104],512
 mov rdi,[rsp]
 lea rsi,[rip+.Lopts_stream_options]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp+104],-513
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 mov rsi,[rsp]
 call pg_mimo_apply_options
 test eax,eax
 jnz .Linvalid
 cmp qword ptr [r12+PG_MM_SES_COUNT],1
 jne .Lfixed_system
 cmp dword ptr [r12+PG_MM_MSG_ROLE],PG_MM_ROLE_CONTEXT_VIEW
 jne .Lfixed_system
 cmp qword ptr [r12+PG_MM_MSG_TEXT],0
 je .Linvalid
 mov rdi,[r12+PG_MM_MSG_TEXT]
 mov rsi,[rbx+PG_MM_CONFIGURATION]
 lea rdx,[rsp+136]
 mov qword ptr [rsp+136],0
 call pg_context_messages_model@PLT
 test eax,eax
 jnz .Linvalid
 mov rdi,[rsp+136]
 lea rsi,[rip+.Lstr_messages]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+8],rax
 or qword ptr [rsp+104],1
 mov rdi,[rsp]
 lea rsi,[rip+.Lstr_messages]
 mov rdx,rax
 call json_object_object_add@PLT
 mov r14d,eax
 test eax,eax
 jnz .Lcontext_messages_retained
 and qword ptr [rsp+104],-2
.Lcontext_messages_retained:
 mov rdi,[rsp+136]
 call json_object_put@PLT
 test r14d,r14d
 jnz .Linvalid
 jmp .Lmsg_done
.Lfixed_system:
 /* 可选 system 前置消息 */
 cmp qword ptr [r12 + PG_MM_SES_SYSTEM_SIZE], 0
 je .Lno_system
 call json_object_new_object@PLT
 test rax, rax
 jz .Linvalid
 mov r15, rax                          /* v */
 or qword ptr [rsp + 104],2
 mov rdi, r15
 lea rsi, [rip + .Lstr_role]
 lea rdx, [rip + .Lstr_system]
 call li_field
 test eax,eax
 jnz .Linvalid
 lea rdi, [r12 + PG_MM_SES_SYSTEM_PROMPT]
 mov rsi, qword ptr [r12 + PG_MM_SES_SYSTEM_SIZE]   /* < 4096，int 安全 */
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Linvalid
 mov [rsp + 96],rax
 or qword ptr [rsp + 104],512
 mov rdx, rax
 mov rdi, r15
 lea rsi, [rip + .Lstr_content]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-513
 mov rdi, qword ptr [rsp + 8]
 mov rsi, r15
 call json_object_array_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-3
.Lno_system:
 mov qword ptr [rsp+112],0
 mov qword ptr [rsp+120],0
 mov qword ptr [rsp + 48], 0           /* i = 0 */
.Lmsg_loop:
 mov qword ptr [rsp+128],0
 mov rax, qword ptr [rsp + 48]
 cmp rax, qword ptr [r12 + PG_MM_SES_COUNT]
 jae .Lmsg_done
 imul rax, rax, PG_MM_MSG_BYTES
 lea r13, [r12 + rax]                  /* p = &messages[i] */
 mov r14, r13                          /* text = p */
 cmp dword ptr [r13+PG_MM_MSG_ROLE],PG_MM_ROLE_USER_VIEW
 je .Luser_view
 mov rax, qword ptr [r13 + PG_MM_MSG_SIZE]
 cmp rax, PG_MM_TEXT_LIMIT
 jae .Linvalid
 mov eax, dword ptr [r13 + PG_MM_MSG_ROLE]
 cmp eax,4
 ja .Linvalid
 cmp eax,4
 jne .Lthinking_dispatch_done
 mov [rsp+128],r13
 /* A thinking row belongs to the immediately following assistant/tool group. */
 mov rax,[rsp+48]
 inc rax
 cmp rax,[r12+PG_MM_SES_COUNT]
 jae .Lthinking_standalone
 imul rcx,rax,PG_MM_MSG_BYTES
 mov edx,[r12+rcx]
 cmp edx,PG_MM_ROLE_ASSISTANT
 je .Lthinking_join
 cmp edx,PG_MM_ROLE_TOOL_CALL
 jne .Lthinking_standalone
.Lthinking_join:
 mov [rsp+48],rax
 lea r13,[r12+rcx]
 mov r14,r13
 cmp qword ptr [r13+PG_MM_MSG_SIZE],4096
 jae .Linvalid
 mov eax,[r13]
 jmp .Lthinking_dispatch_done
.Lthinking_standalone:
 jmp .Lplain_build
.Luser_view:
 cmp qword ptr [r13+PG_MM_MSG_SIZE],2147483647
 ja .Linvalid
 cmp qword ptr [r13+PG_MM_MSG_TEXT],0
 je .Linvalid
 mov eax,PG_MM_ROLE_USER_VIEW
 jmp .Lthinking_dispatch_done
.Lthinking_dispatch_done:
 cmp eax, PG_MM_ROLE_ASSISTANT
 jne .Lrole_dispatch
 /* assistant：紧跟 tool_call 则合并，否则空文本跳过 */
 mov rax, qword ptr [rsp + 48]
 inc rax
 cmp rax, qword ptr [r12 + PG_MM_SES_COUNT]
 jae .Lempty_assistant
 mov rcx, rax
 imul rcx, rcx, PG_MM_MSG_BYTES
 cmp dword ptr [r12 + rcx + PG_MM_MSG_ROLE], PG_MM_ROLE_TOOL_CALL
 jne .Lempty_assistant
 mov qword ptr [rsp + 48], rax         /* ++i */
 lea r13, [r12 + rcx]                  /* p = &messages[i]（tool_call） */
 mov rax, qword ptr [r13 + PG_MM_MSG_SIZE]
 cmp rax, PG_MM_TEXT_LIMIT
 jae .Linvalid
 mov eax, dword ptr [r13 + PG_MM_MSG_ROLE]
 jmp .Lrole_dispatch
.Lempty_assistant:
 cmp qword ptr [rsp+128],0
 jne .Lrole_dispatch
 cmp qword ptr [r13 + PG_MM_MSG_SIZE], 0
 jne .Lrole_dispatch
 jmp .Lmsg_next
.Lrole_dispatch:
 mov eax,dword ptr [r13 + PG_MM_MSG_ROLE]
 cmp eax, PG_MM_ROLE_TOOL_CALL
 jne .Lplain_message
 /* Validate the complete contiguous call group and its ordered results. */
 mov rax,[rsp+48]
 mov [rsp+112],rax
 mov [rsp+120],rax
.Lgroup_scan:
 mov rax,[rsp+120]
 cmp rax,[r12+PG_MM_SES_COUNT]
 jae .Linvalid
 imul rcx,rax,PG_MM_MSG_BYTES
 cmp dword ptr [r12+rcx+PG_MM_MSG_ROLE],PG_MM_ROLE_TOOL_CALL
 jne .Lgroup_scanned
 inc qword ptr [rsp+120]
 sub rax,[rsp+112]
 cmp rax,PG_MM_CALL_LIMIT
 jae .Linvalid
 jmp .Lgroup_scan
.Lgroup_scanned:
 mov rax,[rsp+120]
 sub rax,[rsp+112]
 add rax,[rsp+120]
 cmp rax,[r12+PG_MM_SES_COUNT]
 ja .Linvalid
 mov rax,[rsp+112]
 mov [rsp+88],rax
.Lgroup_validate:
 mov rax,[rsp+88]
 imul rcx,rax,PG_MM_MSG_BYTES
 lea r13,[r12+rcx]
 cmp qword ptr [r13+PG_MM_MSG_SIZE],PG_MM_TEXT_LIMIT
 jae .Linvalid
 lea rdi,[r13+PG_MM_MSG_CALL_ID]
 xor esi,esi
 mov edx,PG_MM_ID_LIMIT
 call memchr@PLT
 test rax,rax
 jz .Linvalid
 cmp byte ptr [r13+PG_MM_MSG_CALL_ID],0
 je .Linvalid
 lea rdi,[r13+PG_MM_MSG_TOOL_NAME]
 xor esi,esi
 mov edx,PG_MM_ID_LIMIT
 call memchr@PLT
 test rax,rax
 jz .Linvalid
 cmp byte ptr [r13+PG_MM_MSG_TOOL_NAME],0
 je .Linvalid
 mov rax,[rsp+112]
 mov [rsp+40],rax
.Lgroup_unique:
 mov rax,[rsp+40]
 cmp rax,[rsp+88]
 jae .Lgroup_pair
 imul rax,rax,PG_MM_MSG_BYTES
 lea rsi,[r12+rax+PG_MM_MSG_CALL_ID]
 lea rdi,[r13+PG_MM_MSG_CALL_ID]
 call strcmp@PLT
 test eax,eax
 jz .Linvalid
 inc qword ptr [rsp+40]
 jmp .Lgroup_unique
.Lgroup_pair:
 mov rax,[rsp+88]
 sub rax,[rsp+112]
 add rax,[rsp+120]
 imul rax,rax,PG_MM_MSG_BYTES
 lea rax,[r12+rax]
 mov [rsp+80],rax
 cmp dword ptr [rax+PG_MM_MSG_ROLE],PG_MM_ROLE_TOOL_RESULT
 jne .Linvalid
 lea rdi,[rax+PG_MM_MSG_CALL_ID]
 xor esi,esi
 mov edx,PG_MM_ID_LIMIT
 call memchr@PLT
 test rax,rax
 jz .Linvalid
 mov rax,[rsp+80]
 lea rsi,[rax+PG_MM_MSG_CALL_ID]
 lea rdi,[r13+PG_MM_MSG_CALL_ID]
 call strcmp@PLT
 test eax,eax
 jnz .Linvalid
 inc qword ptr [rsp+88]
 mov rax,[rsp+88]
 cmp rax,[rsp+120]
 jb .Lgroup_validate
 mov rax,[rsp+112]
 imul rax,rax,PG_MM_MSG_BYTES
 lea r13,[r12+rax]
 /* v = {role:"assistant", tool_calls:[{id,type,function:{name,arguments}}]} */
 call json_object_new_object@PLT
 test rax, rax
 jz .Linvalid
 mov r15, rax
 or qword ptr [rsp + 104],2
 mov rdi, r15
 lea rsi, [rip + .Lstr_role]
 lea rdx, [rip + .Lstr_assistant]
 call li_field
 test eax,eax
 jnz .Linvalid
 call json_object_new_array@PLT
 test rax, rax
 jz .Loom_v
 mov qword ptr [rsp + 56], rax         /* calls */
 or qword ptr [rsp + 104],4
.Lgroup_build:
 call json_object_new_object@PLT
 test rax, rax
 jz .Loom_calls
 mov qword ptr [rsp + 64], rax         /* call */
 or qword ptr [rsp + 104],8
 call json_object_new_object@PLT
 test rax, rax
 jz .Loom_call
 mov qword ptr [rsp + 72], rax         /* func */
 or qword ptr [rsp + 104],16
 mov rdi, qword ptr [rsp + 64]
 lea rsi, [rip + .Lstr_id]
 lea rdx, [r13 + PG_MM_MSG_CALL_ID]
 call li_field
 test eax,eax
 jnz .Linvalid
 mov rdi, qword ptr [rsp + 64]
 lea rsi, [rip + .Lstr_type]
 lea rdx, [rip + .Lstr_function]
 call li_field
 test eax,eax
 jnz .Linvalid
 mov rdi, qword ptr [rsp + 72]
 lea rsi, [rip + .Lstr_name]
 lea rdx, [r13 + PG_MM_MSG_TOOL_NAME]
 call li_field
 test eax,eax
 jnz .Linvalid
 lea rdi, [r13 + PG_MM_MSG_TEXT]
 mov rsi, qword ptr [r13 + PG_MM_MSG_SIZE]
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Linvalid
 mov [rsp + 96],rax
 or qword ptr [rsp + 104],512
 mov rdx, rax
 mov rdi, qword ptr [rsp + 72]
 lea rsi, [rip + .Lstr_arguments]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-513
 mov rdx, qword ptr [rsp + 72]
 mov rdi, qword ptr [rsp + 64]
 lea rsi, [rip + .Lstr_function]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-17
 mov rdi, qword ptr [rsp + 56]
 mov rsi, qword ptr [rsp + 64]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-9
 inc qword ptr [rsp+48]
 mov rax,[rsp+48]
 cmp rax,[rsp+120]
 jae .Lgroup_built
 imul rax,rax,PG_MM_MSG_BYTES
 lea r13,[r12+rax]
 jmp .Lgroup_build
.Lgroup_built:
 dec qword ptr [rsp+48]
 mov rax,[rsp+112]
 imul rax,rax,PG_MM_MSG_BYTES
 lea r13,[r12+rax]
 mov rdx, qword ptr [rsp + 56]
 mov rdi, r15
 lea rsi, [rip + .Lstr_tool_calls]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-5
 cmp r14, r13                          /* text != p 且非空才带 content */
 je .Lmsg_append
 cmp qword ptr [r14 + PG_MM_MSG_SIZE], 0
 je .Lmsg_append
 lea rdi, [r14 + PG_MM_MSG_TEXT]
 mov rsi, qword ptr [r14 + PG_MM_MSG_SIZE]
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Linvalid
 mov [rsp + 96],rax
 or qword ptr [rsp + 104],512
 mov rdx, rax
 mov rdi, r15
 lea rsi, [rip + .Lstr_content]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-513
 jmp .Lmsg_append
.Lplain_message:
 cmp eax, PG_MM_ROLE_TOOL_RESULT
 jne .Lplain_build
 /* Map each result back to its call within the preceding validated group. */
 mov rax,[rsp+48]
 cmp rax,[rsp+120]
 jb .Linvalid
 sub rax,[rsp+120]
 mov rcx,[rsp+120]
 sub rcx,[rsp+112]
 cmp rax,rcx
 jae .Linvalid
 add rax,[rsp+112]
 imul rax,rax,PG_MM_MSG_BYTES
 lea rdx,[r12+rax]
 mov [rsp+80],rdx
 lea rdi, [r13 + PG_MM_MSG_CALL_ID]
 xor esi, esi
 mov edx, PG_MM_ID_LIMIT
 call memchr@PLT
 test rax, rax
 jz .Linvalid
 mov rdx, qword ptr [rsp + 80]
 lea rdi, [r13 + PG_MM_MSG_CALL_ID]
 lea rsi, [rdx + PG_MM_MSG_CALL_ID]
 call strcmp@PLT
 test eax, eax
 jnz .Linvalid
.Lplain_build:
 call json_object_new_object@PLT
 test rax, rax
 jz .Linvalid
 mov r15, rax                          /* v */
 or qword ptr [rsp + 104],2
 mov eax, dword ptr [r13 + PG_MM_MSG_ROLE]
 test eax, eax
 jz .Lbuild_user
 cmp eax,PG_MM_ROLE_USER_VIEW
 je .Lbuild_user
 cmp eax, PG_MM_ROLE_TOOL_RESULT
 je .Lbuild_tool
 mov rdi, r15
 lea rsi, [rip + .Lstr_role]
 lea rdx, [rip + .Lstr_assistant]
 call li_field
 test eax,eax
 jnz .Linvalid
 cmp dword ptr [r13],4
 je .Lmsg_append
 jmp .Lbuild_content
.Lbuild_user:
 mov rdi, r15
 lea rsi, [rip + .Lstr_role]
 lea rdx, [rip + .Lstr_user]
 call li_field
 test eax,eax
 jnz .Linvalid
 jmp .Lbuild_content
.Lbuild_tool:
 mov rdi, r15
 lea rsi, [rip + .Lstr_role]
 lea rdx, [rip + .Lstr_tool]
 call li_field
 test eax,eax
 jnz .Linvalid
 lea rdi, [r13 + PG_MM_MSG_TEXT]
 mov rsi, qword ptr [r13 + PG_MM_MSG_SIZE]
 call .Ltool_blank
 lea rdi, [r13 + PG_MM_MSG_TEXT]
 mov rsi, qword ptr [r13 + PG_MM_MSG_SIZE]
 test eax,eax
 jz .Ltool_text_ready
 lea rdi,[rip+.Ltool_no_output]
 mov esi,16
.Ltool_text_ready:
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Linvalid
 mov [rsp + 96],rax
 or qword ptr [rsp + 104],512
 mov rdx, rax
 mov rdi, r15
 lea rsi, [rip + .Lstr_content]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-513
 mov rdi, r15
 lea rsi, [rip + .Lstr_tool_call_id]
 lea rdx, [r13 + PG_MM_MSG_CALL_ID]
 call li_field
 test eax,eax
 jnz .Linvalid
 jmp .Lmsg_append
.Lbuild_content:
 lea rdi, [r13 + PG_MM_MSG_TEXT]
 cmp dword ptr [r13+PG_MM_MSG_ROLE],PG_MM_ROLE_USER_VIEW
 jne .Lcontent_pointer_ready
 mov rdi,[r13+PG_MM_MSG_TEXT]
.Lcontent_pointer_ready:
 mov rsi, qword ptr [r13 + PG_MM_MSG_SIZE]
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Linvalid
 mov [rsp + 96],rax
 or qword ptr [rsp + 104],512
 mov rdx, rax
 mov rdi, r15
 lea rsi, [rip + .Lstr_content]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-513
.Lmsg_append:
 mov rax,[rsp+128]
 test rax,rax
 jz .Lthinking_appended
 lea rdi,[rax+PG_MM_MSG_TOOL_NAME]
 xor esi,esi
 mov edx,PG_MM_ID_LIMIT
 call memchr@PLT
 test rax,rax
 jz .Linvalid
 mov rax,[rsp+128]
 lea rdi,[rax+PG_MM_MSG_TOOL_NAME]
 lea rsi,[rip+.Lreasoning_content_key]
 call strcmp@PLT
 test eax,eax
 jnz .Linvalid
 mov rax,[rsp+128]
 lea rdi,[rax+PG_MM_MSG_TEXT]
 mov rsi,[rax+PG_MM_MSG_SIZE]
 call .Ltool_blank
 test eax,eax
 jnz .Lthinking_appended
 mov rax,[rsp+128]
 lea rdi,[rax+PG_MM_MSG_TEXT]
 mov rsi,[rax+PG_MM_MSG_SIZE]
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Linvalid
 mov [rsp+96],rax
 or qword ptr [rsp+104],512
 mov rdi,r15
 lea rsi,[rip+.Lreasoning_content_key]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp+104],-513
.Lthinking_appended:
 mov rdi, qword ptr [rsp + 8]
 mov rsi, r15
 call json_object_array_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-3
.Lmsg_next:
 inc qword ptr [rsp + 48]
 jmp .Lmsg_loop
.Lmsg_done:
 /* registry 非空时输出完整工具定义 */
 mov rax, qword ptr [rbx + PG_MM_TOOLS]
 test rax, rax
 jz .Lserialize
 mov rcx, qword ptr [rax + PG_MM_REG_COUNT]
 cmp rcx,16
 ja .Linvalid
 test rcx, rcx
 jz .Lserialize
 mov qword ptr [rsp + 80], rax         /* registry */
 mov qword ptr [rsp + 88], rcx         /* count */
 call json_object_new_array@PLT
 test rax, rax
 jz .Linvalid
 mov qword ptr [rsp + 16], rax         /* tools 数组 */
 or qword ptr [rsp + 104],32
 mov qword ptr [rsp + 48], 0           /* index */
.Ltool_loop:
 mov rax, qword ptr [rsp + 48]
 cmp rax, qword ptr [rsp + 88]
 jae .Ltool_done
 imul rax, rax, PG_MM_DEF_BYTES
 add rax, qword ptr [rsp + 80]
 mov qword ptr [rsp + 40], rax         /* def */
 call json_object_new_object@PLT
 test rax, rax
 jz .Loom_tools
 mov qword ptr [rsp + 24], rax         /* tool */
 or qword ptr [rsp + 104],64
 call json_object_new_object@PLT
 test rax, rax
 jz .Loom_tool
 mov qword ptr [rsp + 32], rax         /* fn */
 or qword ptr [rsp + 104],128
 mov rdi, qword ptr [rsp + 24]
 lea rsi, [rip + .Lstr_type]
 lea rdx, [rip + .Lstr_function]
 call li_field
 test eax,eax
 jnz .Linvalid
 mov rax, qword ptr [rsp + 40]
 mov rdx, qword ptr [rax + PG_MM_DEF_NAME]
 mov rdi, qword ptr [rsp + 32]
 lea rsi, [rip + .Lstr_name]
 call li_field
 test eax,eax
 jnz .Linvalid
 mov rax, qword ptr [rsp + 40]
 mov rdx, qword ptr [rax + PG_MM_DEF_DESCRIPTION]
 mov rdi, qword ptr [rsp + 32]
 lea rsi, [rip + .Lstr_description]
 call li_field
 test eax,eax
 jnz .Linvalid
 mov rax, qword ptr [rsp + 40]
 mov rdi, qword ptr [rax + PG_MM_DEF_SCHEMA]
 call json_tokener_parse@PLT
 test rax,rax
 jz .Linvalid
 mov [rsp + 96],rax
 or qword ptr [rsp + 104],512
 test rax, rax
 jz .Lschema_bad
 mov rdx, rax
 mov rdi, qword ptr [rsp + 32]
 lea rsi, [rip + .Lstr_parameters]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-513
 mov rdx, qword ptr [rsp + 32]
 mov rdi, qword ptr [rsp + 24]
 lea rsi, [rip + .Lstr_function]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-129
 mov rdi, qword ptr [rsp + 16]
 mov rsi, qword ptr [rsp + 24]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-65
 inc qword ptr [rsp + 48]
 jmp .Ltool_loop
.Ltool_done:
 mov rdx, qword ptr [rsp + 16]
 mov rdi, qword ptr [rsp]
 lea rsi, [rip + .Lstr_tools]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp + 104],-33
.Lserialize:
 mov rdi,rbx
 call pg_mimo_uses_responses
 test eax,eax
 jz .Lserialize_ready
 mov rdi,[rsp]
 call .Lrp_request
 test eax,eax
 jnz .Linvalid
.Lserialize_ready:
 mov rdi, qword ptr [rsp]
 mov esi, PG_MM_TO_JSON_PLAIN
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Linvalid
 mov rdi, rax
 call strdup@PLT
 mov qword ptr [rbx + PG_MM_REQUEST], rax
 mov rdi, qword ptr [rsp]
 call json_object_put@PLT
 cmp qword ptr [rbx + PG_MM_REQUEST], 0
 je .Lret_bad
 xor eax, eax
 jmp .Lout
.Lschema_bad:
.Loom_tool:
.Loom_tools:
.Loom_call:
.Loom_calls:
.Loom_v:
.Linvalid:
 test qword ptr [rsp + 104],512
 jz .Lcleanup_512
 mov rdi,[rsp + 96]
 call json_object_put@PLT
.Lcleanup_512:
 test qword ptr [rsp + 104],16
 jz .Lcleanup_16
 mov rdi,[rsp + 72]
 call json_object_put@PLT
.Lcleanup_16:
 test qword ptr [rsp + 104],8
 jz .Lcleanup_8
 mov rdi,[rsp + 64]
 call json_object_put@PLT
.Lcleanup_8:
 test qword ptr [rsp + 104],4
 jz .Lcleanup_4
 mov rdi,[rsp + 56]
 call json_object_put@PLT
.Lcleanup_4:
 test qword ptr [rsp + 104],128
 jz .Lcleanup_128
 mov rdi,[rsp + 32]
 call json_object_put@PLT
.Lcleanup_128:
 test qword ptr [rsp + 104],64
 jz .Lcleanup_64
 mov rdi,[rsp + 24]
 call json_object_put@PLT
.Lcleanup_64:
 test qword ptr [rsp + 104],32
 jz .Lcleanup_32
 mov rdi,[rsp + 16]
 call json_object_put@PLT
.Lcleanup_32:
 test qword ptr [rsp + 104],256
 jz .Lcleanup_256
 mov rdi,[rsp + 80]
 call json_object_put@PLT
.Lcleanup_256:
 test qword ptr [rsp + 104],2
 jz .Lcleanup_2
 mov rdi,r15
 call json_object_put@PLT
.Lcleanup_2:
 test qword ptr [rsp + 104],1
 jz .Lcleanup_1
 mov rdi,[rsp + 8]
 call json_object_put@PLT
.Lcleanup_1:
 mov rdi,[rsp]
 call json_object_put@PLT
.Lret_bad:
 mov eax, 1
.Lout:
 add rsp, 144
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_mimo_encode_request, .-pg_mimo_encode_request

.section .note.GNU-stack,"",@progbits

.text
.Ltool_blank:
 jmp pg_context_blank@PLT
.section .rodata
.Ltool_no_output: .asciz "(no tool output)"

.text
.globl pg_mimo_apply_options
.hidden pg_mimo_apply_options
.type pg_mimo_apply_options,@function
pg_mimo_apply_options:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
 mov qword ptr [rsp+16],0
 test rdi,rdi
 jz .Lopts_success
 lea rsi,[rip+.Lopts_options]
 mov rdx,rsp
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lopts_success
 mov r13,[rsp]
 mov rdi,r13
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lopts_bad
 lea r14,[rip+.Lstr_max_tokens]
 mov rdi,r13
 lea rsi,[rip+.Lopts_max_field]
 lea rdx,[rsp+8]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Lopts_max_field_ready
 mov rdi,rbx
 lea rsi,[rip+.Lopts_max_field]
 call .Lcompat_get
 mov [rsp+8],rax
 test rax,rax
 jz .Lopts_maximum
.Lopts_max_field_ready:
 mov rdi,[rsp+8]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lopts_bad
 mov rdi,[rsp+8]
 call json_object_get_string_len@PLT
 mov ebp,eax
 mov rdi,[rsp+8]
 call json_object_get_string@PLT
 mov [rsp+24],rax
 cmp ebp,10
 jne .Lopts_completion
 mov rdi,rax
 mov rsi,r14
 call strcmp@PLT
 test eax,eax
 jnz .Lopts_bad
 jmp .Lopts_maximum
.Lopts_completion:
 cmp ebp,21
 jne .Lopts_bad
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lopts_completion_tokens]
 call strcmp@PLT
 test eax,eax
 jnz .Lopts_bad
 lea r14,[rip+.Lopts_completion_tokens]
.Lopts_maximum:
 mov rdi,r13
 lea rsi,[rip+.Lopts_maximum_key]
 lea rdx,[rsp+8]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lopts_default_maximum
 mov rdi,[rsp+8]
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lopts_bad
 mov rdi,[rsp+8]
 call json_object_get_int64@PLT
 test rax,rax
 jle .Lopts_bad
 cmp rax,2147483647
 ja .Lopts_bad
 mov rdi,[rsp+8]
 call json_object_get@PLT
 jmp .Lopts_maximum_value
.Lopts_default_maximum:
 mov edi,512
 call json_object_new_int@PLT
.Lopts_maximum_value:
 mov [rsp+16],rax
 test rax,rax
 jz .Lopts_bad
 mov rdi,r12
 lea rsi,[rip+.Lstr_max_tokens]
 call json_object_object_del@PLT
 mov rdi,r12
 lea rsi,[rip+.Lopts_completion_tokens]
 call json_object_object_del@PLT
 mov rdi,r12
 mov rsi,r14
 mov rdx,[rsp+16]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lopts_bad
 mov qword ptr [rsp+16],0
 mov rdi,r13
 lea rsi,[rip+.Lopts_temperature]
 lea rdx,[rsp+8]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lopts_reasoning
 mov rdi,[rsp+8]
 mov esi,2
 call json_object_is_type@PLT
 test eax,eax
 jnz .Lopts_number
 mov rdi,[rsp+8]
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lopts_bad
.Lopts_number:
 mov rdi,[rsp+8]
 call json_object_get_double@PLT
 movq rax,xmm0
 shr rax,52
 and eax,2047
 cmp eax,2047
 je .Lopts_bad
 mov rdi,[rsp+8]
 call json_object_get@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lopts_bad
 mov rdi,r12
 lea rsi,[rip+.Lopts_temperature]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lopts_bad
 mov qword ptr [rsp+16],0
.Lopts_reasoning:
 mov rdi,r13
 lea rsi,[rip+.Lopts_reasoning_effort]
 lea rdx,[rsp+8]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lopts_success
 mov rdi,[rsp+8]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lopts_bad
 mov rdi,[rsp+8]
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lopts_bad
 cmp eax,32
 ja .Lopts_bad
 mov rdi,[rsp+8]
 call json_object_get@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lopts_bad
 mov rdi,r12
 lea rsi,[rip+.Lopts_reasoning_effort_wire]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lopts_bad
 mov qword ptr [rsp+16],0
 lea rdi,[rip+.Lopts_thinking_enabled]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lopts_bad
 mov rdi,r12
 lea rsi,[rip+.Lopts_thinking_key]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lopts_bad
 mov qword ptr [rsp+16],0
.Lopts_success:
 mov rdi,rbx
 mov rsi,r12
 call .Lcompat_usage
 mov rdi,rbx
 mov rsi,r12
 call .Lcompat_thinking
 mov ebp,eax
 jmp .Lopts_end
.Lopts_bad:
 mov ebp,1
.Lopts_end:
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_mimo_apply_options,.-pg_mimo_apply_options
.section .rodata
.Lopts_options: .asciz "options"
.Lopts_max_field: .asciz "maxTokensField"
.Lopts_maximum_key: .asciz "maxTokens"
.Lopts_completion_tokens: .asciz "max_completion_tokens"
.Lopts_temperature: .asciz "temperature"

.section .rodata
.Lopts_stream_options: .asciz "stream_options"
.Lopts_stream_usage_json: .asciz "{\"include_usage\":true}"

.section .rodata
.Lreasoning_content_key: .asciz "reasoning_content"

.section .rodata
.Lopts_reasoning_effort: .asciz "reasoningEffort"
.Lopts_thinking_key: .asciz "thinking"
.Lopts_thinking_enabled: .asciz "{\"type\":\"enabled\"}"

.section .rodata
.Lopts_reasoning_effort_wire: .asciz "reasoning_effort"

.text
/* Borrow public compatibility metadata from the selected model descriptor. */
.Lcompat_get:
 push rbx
 mov rbx,rsi
 lea rsi,[rip+.Lcompat_model_info]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcompat_key]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,rbx
 call json_object_object_get@PLT
 pop rbx
 ret
.Lcompat_usage:
 push rbx
 mov rbx,rsi
 lea rsi,[rip+.Lcompat_usage_key]
 call .Lcompat_get
 test rax,rax
 jz .Lcompat_usage_ok
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lcompat_usage_ok
 mov rdi,rbx
 lea rsi,[rip+.Lopts_stream_options]
 call json_object_object_del@PLT
.Lcompat_usage_ok:
 xor eax,eax
 pop rbx
 ret
.section .rodata
.Lcompat_model_info: .asciz "modelInfo"
.Lcompat_key: .asciz "compat"
.Lcompat_usage_key: .asciz "supportsUsageInStreaming"
.section .note.GNU-stack,"",@progbits

.text
/* Resolve provider-specific thinking framing after ordinary option encoding. */
.Lcompat_thinking:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov ebp,1
 test rbx,rbx
 jz .Lct_done
 lea rsi,[rip+.Lct_format]
 call .Lcompat_get
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 test rax,rax
 jnz .Lct_explicit
 xor ebp,ebp
 mov rdi,rbx
 lea rsi,[rip+.Lct_endpoint]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 test rax,rax
 jz .Lct_framing
 mov rdi,rax
 lea rsi,[rip+.Lct_deepseek_host]
 call strstr@PLT
 test rax,rax
 jnz .Lct_deepseek_mode
 mov rdi,r14
 lea rsi,[rip+.Lct_mimo_host]
 call strstr@PLT
 test rax,rax
 jz .Lct_framing
.Lct_deepseek_mode:
 mov ebp,1
 jmp .Lct_framing
.Lct_explicit:
 xor ebp,ebp
 lea r13,[rip+.Lct_formats]
.Lct_format_each:
 mov rsi,[r13]
 test rsi,rsi
 jz .Lct_framing
 mov rdi,r14
 call strcmp@PLT
 inc ebp
 test eax,eax
 jz .Lct_framing
 add r13,8
 cmp ebp,4
 jb .Lct_format_each
 xor ebp,ebp
.Lct_framing:
 mov rdi,r12
 lea rsi,[rip+.Lopts_reasoning_effort_wire]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp],rax
 mov r13,rax
 mov qword ptr [rsp+16],0
 test rax,rax
 setnz byte ptr [rsp+16]
 mov qword ptr [rsp+24],0
 mov rdi,rbx
 lea rsi,[rip+.Lcompat_model_info]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lct_level_map]
 call json_object_object_get@PLT
 mov r14,rax
 test rax,rax
 jz .Lct_mapping_ready
 lea rsi,[rip+.Lct_off]
 test r13,r13
 jz .Lct_lookup_map
 mov rdi,r13
 call json_object_get_string@PLT
 mov rsi,rax
.Lct_lookup_map:
 mov rdi,r14
 lea rdx,[rsp+8]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lct_mapping_ready
 mov rax,[rsp+8]
 mov qword ptr [rsp+8],0
 test rax,rax
 jnz .Lct_map_string
 cmp qword ptr [rsp+16],0
 jne .Lct_mapping_ready
 mov qword ptr [rsp+24],1
 jmp .Lct_mapping_ready
.Lct_map_string:
 mov r14,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lct_mapping_ready
 mov rdi,r14
 call json_object_get@PLT
 mov r14,rax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov [rsp],r14
 mov r13,r14
 mov rdi,r13
 call json_object_get@PLT
 mov rdx,rax
 mov rdi,r12
 lea rsi,[rip+.Lopts_reasoning_effort_wire]
 call .Lct_set
 test eax,eax
 jnz .Lct_bad
.Lct_mapping_ready:
 cmp ebp,1
 jne .Lct_non_deepseek
 cmp qword ptr [rsp+24],0
 je .Lct_effort_support
 mov rdi,r12
 lea rsi,[rip+.Lopts_thinking_key]
 call json_object_object_del@PLT
 jmp .Lct_effort_support
.Lct_non_deepseek:
 mov rdi,r12
 lea rsi,[rip+.Lopts_thinking_key]
 call json_object_object_del@PLT
 test ebp,ebp
 jz .Lct_effort_support
 cmp ebp,2
 je .Lct_zai
 cmp ebp,3
 je .Lct_qwen
 /* OpenRouter uses reasoning.effort and no top-level reasoning_effort. */
 cmp qword ptr [rsp+24],0
 jne .Lct_effort_support
 call json_object_new_object@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lct_bad
 test r13,r13
 jz .Lct_none
 mov rdi,r13
 call json_object_get@PLT
 jmp .Lct_router_effort
.Lct_none:
 lea rdi,[rip+.Lct_none_text]
 call json_object_new_string@PLT
.Lct_router_effort:
 mov rdx,rax
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lct_effort]
 call .Lct_set
 test eax,eax
 jnz .Lct_bad
 mov rdi,r12
 lea rsi,[rip+.Lopts_reasoning_effort_wire]
 call json_object_object_del@PLT
 lea r14,[rip+.Lct_reasoning]
 jmp .Lct_install
.Lct_zai:
 lea rdi,[rip+.Lct_zai_off]
 cmp qword ptr [rsp+16],0
 je .Lct_zai_parse
 lea rdi,[rip+.Lct_zai_on]
.Lct_zai_parse:
 call json_tokener_parse@PLT
 mov [rsp+8],rax
 lea r14,[rip+.Lopts_thinking_key]
 jmp .Lct_install
.Lct_qwen:
 xor edi,edi
 cmp qword ptr [rsp+16],0
 setne dil
 call json_object_new_boolean@PLT
 mov [rsp+8],rax
 lea r14,[rip+.Lct_enable]
.Lct_install:
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 mov rdi,r12
 mov rsi,r14
 call .Lct_set
 test eax,eax
 jnz .Lct_bad
.Lct_effort_support:
 mov rdi,rbx
 lea rsi,[rip+.Lct_supports_effort]
 call .Lcompat_get
 test rax,rax
 jz .Lct_done
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lct_done
 mov rdi,r12
 lea rsi,[rip+.Lopts_reasoning_effort_wire]
 call json_object_object_del@PLT
.Lct_done:
 xor ebp,ebp
 jmp .Lct_end
.Lct_bad:
 mov ebp,1
.Lct_end:
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
.Lct_set:
 test rdx,rdx
 jz .Lct_set_bad
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Lct_set_end
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,1
.Lct_set_end:
 pop rbx
 ret
.Lct_set_bad:
 mov eax,1
 ret
.section .rodata
.Lct_format: .asciz "thinkingFormat"
.Lct_endpoint: .asciz "endpoint"
.Lct_deepseek_host: .asciz "deepseek.com"
.Lct_mimo_host: .asciz "xiaomimimo.com"
.Lct_deepseek: .asciz "deepseek"
.Lct_zai_format: .asciz "zai"
.Lct_qwen_format: .asciz "qwen"
.Lct_router_format: .asciz "openrouter"
.Lct_reasoning: .asciz "reasoning"
.Lct_effort: .asciz "effort"
.Lct_none_text: .asciz "none"
.Lct_enable: .asciz "enable_thinking"
.Lct_supports_effort: .asciz "supportsReasoningEffort"
.Lct_zai_on: .asciz "{\"type\":\"enabled\",\"clear_thinking\":false}"
.Lct_zai_off: .asciz "{\"type\":\"disabled\"}"
.section .data.rel.ro
.Lct_formats: .quad .Lct_deepseek,.Lct_zai_format,.Lct_qwen_format,.Lct_router_format,0
.section .note.GNU-stack,"",@progbits

.section .rodata
.Lct_level_map: .asciz "thinkingLevelMap"
.Lct_off: .asciz "off"
.section .note.GNU-stack,"",@progbits

.text
/* Selection belongs to the request identity, shared by encoder and decoder. */
.globl pg_mimo_uses_responses
.type pg_mimo_uses_responses,@function
pg_mimo_uses_responses:
 mov rdi,[rdi+PG_MM_CONFIGURATION]
 jmp pg_mimo_config_responses
.size pg_mimo_uses_responses,.-pg_mimo_uses_responses
.globl pg_mimo_config_responses
.type pg_mimo_config_responses,@function
pg_mimo_config_responses:
 push rbx
 mov rbx,rdi
 mov rdi,rbx
 lea rsi,[rip+.Lrp_api]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lrp_api_value
 mov rdi,rbx
 lea rsi,[rip+.Lrp_info]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lrp_api]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lrp_api_value
 mov rdi,rbx
 lea rsi,[rip+.Lrp_endpoint]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jnz .Lrp_endpoint_value
 lea rdi,[rip+.Lrp_env]
 call getenv@PLT
 test rax,rax
 jz .Lrp_no
.Lrp_endpoint_value:
 mov rdi,rax
 lea rsi,[rip+.Lrp_suffix]
 call strstr@PLT
 test rax,rax
 setne al
 movzx eax,al
 pop rbx
 ret
.Lrp_api_value:
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lrp_no
 mov rdi,rax
 lea rsi,[rip+.Lrp_responses]
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 pop rbx
 ret
.Lrp_no:
 xor eax,eax
 pop rbx
 ret
.size pg_mimo_config_responses,.-pg_mimo_config_responses

/* Copy a field to another key, retaining its JSON owner. Missing is optional. */
.Lrp_copy:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rcx
 mov rdi,rsi
 mov rsi,rdx
 call json_object_object_get@PLT
 test rax,rax
 jz .Lrp_copy_ok
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call .Lrp_set
 jmp .Lrp_copy_end
.Lrp_copy_ok:
 xor eax,eax
.Lrp_copy_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.macro RPGET obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lrp_\key]
 call json_object_object_get@PLT
.endm
.macro RPCOPY dst,src,from,to
 mov rdi,\dst
 mov rsi,\src
 lea rdx,[rip+.Lrp_\from]
 lea rcx,[rip+.Lrp_\to]
 call .Lrp_copy
 test eax,eax
 jnz .Lrp_bad
.endm
.macro RPDEL obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lrp_\key]
 call json_object_object_del@PLT
.endm
/* Parse and attach immediately, so the root owns every intermediate. */
.Lrp_append:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rsi
 call json_tokener_parse@PLT
 mov r12,rax
 test rax,rax
 jz .Lrp_append_end
 mov rdi,rbx
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz .Lrp_append_ok
 mov rdi,r12
 call json_object_put@PLT
 xor r12d,r12d
.Lrp_append_ok:
 mov rax,r12
.Lrp_append_end:
 add rsp,8
 pop r12
 pop rbx
 ret

/* Chat projection and Responses share canonical context and tool selection. */
.Lrp_request:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 RPGET rbx,messages
 mov r12,rax
 call json_object_new_array@PLT
 mov r13,rax
 test rax,rax
 jz .Lrp_bad
 mov rdi,rbx
 lea rsi,[rip+.Lrp_input]
 mov rdx,r13
 call .Lrp_set
 test eax,eax
 jnz .Lrp_bad
 xor r14d,r14d
.Lrp_message:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lrp_tools_handler
 mov rdi,r12
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbp,rax
 RPGET rbp,responsesOutput
 test rax,rax
 jz .Lrp_generic_content
 mov [rsp+32],rax
 mov qword ptr [rsp+40],0
.Lrp_replay_item:
 mov rdi,[rsp+32]
 call json_object_array_length@PLT
 cmp [rsp+40],rax
 jae .Lrp_next
 mov rdi,[rsp+32]
 mov rsi,[rsp+40]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+48],rax
 mov rdi,r13
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz .Lrp_replay_next
 mov rdi,[rsp+48]
 call json_object_put@PLT
 jmp .Lrp_bad
.Lrp_replay_next:
 inc qword ptr [rsp+40]
 jmp .Lrp_replay_item
.Lrp_generic_content:
 RPGET rbp,content
 test rax,rax
 jz .Lrp_calls
 mov [rsp],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lrp_message_add
 mov qword ptr [rsp+8],0
.Lrp_content_handler:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 cmp [rsp+8],rax
 jae .Lrp_message_add
 mov rdi,[rsp]
 mov rsi,[rsp+8]
 call json_object_array_get_idx@PLT
 mov [rsp+16],rax
 RPGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lrp_image_url]
 call strcmp@PLT
 test eax,eax
 jz .Lrp_image
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lrp_type]
 lea rdx,[rip+.Lrp_input_text]
 call li_field
 test eax,eax
 jnz .Lrp_bad
 RPGET rbp,role
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lrp_assistant]
 call strcmp@PLT
 test eax,eax
 jnz .Lrp_content_next
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lrp_type]
 lea rdx,[rip+.Lrp_output_text]
 call li_field
 test eax,eax
 jnz .Lrp_bad
 jmp .Lrp_content_next
.Lrp_image:
 mov rax,[rsp+16]
 RPGET rax,image_url
 mov [rsp+24],rax
 RPCOPY "qword ptr [rsp+16]","qword ptr [rsp+24]",url,image_url
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lrp_type]
 lea rdx,[rip+.Lrp_input_image]
 call li_field
 test eax,eax
 jnz .Lrp_bad
.Lrp_content_next:
 inc qword ptr [rsp+8]
 jmp .Lrp_content_handler
.Lrp_message_add:
 RPGET rbp,role
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lrp_tool]
 call strcmp@PLT
 test eax,eax
 jz .Lrp_tool_result
 mov rdi,rbp
 call json_object_get@PLT
 mov rdi,r13
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz .Lrp_calls
 mov rdi,rbp
 call json_object_put@PLT
 jmp .Lrp_bad
.Lrp_calls:
 RPGET rbp,tool_calls
 mov [rsp+32],rax
 mov qword ptr [rsp+40],0
 test rax,rax
 jz .Lrp_next
.Lrp_call:
 mov rdi,[rsp+32]
 call json_object_array_length@PLT
 cmp [rsp+40],rax
 jae .Lrp_calls_done
 mov rdi,[rsp+32]
 mov rsi,[rsp+40]
 call json_object_array_get_idx@PLT
 mov [rsp+48],rax
 lea rdi,[rip+.Lrp_call_json]
 mov rsi,r13
 call .Lrp_append
 mov [rsp+56],rax
 test rax,rax
 jz .Lrp_bad
 RPCOPY "qword ptr [rsp+56]","qword ptr [rsp+48]",id,call_id
 mov rax,[rsp+48]
 RPGET rax,function
 mov [rsp+48],rax
 RPCOPY "qword ptr [rsp+56]","qword ptr [rsp+48]",name,name
 RPCOPY "qword ptr [rsp+56]","qword ptr [rsp+48]",arguments,arguments
 inc qword ptr [rsp+40]
 jmp .Lrp_call
.Lrp_calls_done:
 RPDEL rbp,tool_calls
 jmp .Lrp_next
.Lrp_tool_result:
 lea rdi,[rip+.Lrp_result_json]
 mov rsi,r13
 call .Lrp_append
 mov [rsp+56],rax
 test rax,rax
 jz .Lrp_bad
 RPCOPY "qword ptr [rsp+56]",rbp,tool_call_id,call_id
 RPCOPY "qword ptr [rsp+56]",rbp,content,output
 mov rax,[rsp+56]
 RPGET rax,output
 mov [rsp],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lrp_next
 mov rdi,[rsp]
 call json_object_get_string_len@PLT
 mov [rsp+8],rax
 mov rdi,[rsp]
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,[rsp+8]
 call pg_context_blank@PLT
 test eax,eax
 jz .Lrp_next
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lrp_output]
 lea rdx,[rip+.Lrp_no_output]
 call li_field
 test eax,eax
 jnz .Lrp_bad
.Lrp_next:
 inc r14
 jmp .Lrp_message
.Lrp_tools_handler:
 RPGET rbx,tools
 mov r12,rax
 test rax,rax
 jz .Lrp_options
 xor r14d,r14d
.Lrp_tool_loop:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lrp_options
 mov rdi,r12
 mov rsi,r14
 call json_object_array_get_idx@PLT
 RPGET rax,function
 mov rbp,rax
 test rax,rax
 jz .Lrp_bad
 mov rdi,rbp
 lea rsi,[rip+.Lrp_type]
 lea rdx,[rip+.Lrp_function]
 call li_field
 test eax,eax
 jnz .Lrp_bad
 mov rdi,rbp
 call json_object_get@PLT
 mov rdi,r12
 mov rsi,r14
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jz .Lrp_tool_inserted
 mov rdi,rbp
 call json_object_put@PLT
 jmp .Lrp_bad
.Lrp_tool_inserted:
 inc r14
 jmp .Lrp_tool_loop
.Lrp_options:
 RPGET rbx,max_tokens
 test rax,rax
 jnz .Lrp_max
 RPGET rbx,max_completion_tokens
 test rax,rax
 jz .Lrp_reasoning_handler
.Lrp_max:
 mov rdi,rax
 call json_object_get_int64@PLT
 cmp rax,16
 mov edx,16
 cmovl rax,rdx
 mov rdi,rax
 call json_object_new_int64@PLT
 test rax,rax
 jz .Lrp_bad
 mov rdi,rbx
 lea rsi,[rip+.Lrp_max_output_tokens]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lrp_bad
.Lrp_reasoning_handler:
 RPGET rbx,reasoning_effort
 mov rbp,rax
 test rax,rax
 jz .Lrp_clean
 lea rdi,[rip+.Lrp_reason_json]
 call json_tokener_parse@PLT
 test rax,rax
 jz .Lrp_bad
 mov r12,rax
 mov rdi,rbx
 lea rsi,[rip+.Lrp_reasoning]
 mov rdx,r12
 call .Lrp_set
 test eax,eax
 jnz .Lrp_bad
 mov rdi,rbp
 call json_object_get@PLT
 mov rdi,r12
 lea rsi,[rip+.Lrp_effort]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lrp_bad
.Lrp_clean:
 RPDEL rbx,messages
 RPDEL rbx,stream_options
 RPDEL rbx,max_tokens
 RPDEL rbx,max_completion_tokens
 RPDEL rbx,reasoning_effort
 RPDEL rbx,thinking
 xor edi,edi
 call json_object_new_boolean@PLT
 test rax,rax
 jz .Lrp_bad
 mov rdi,rbx
 lea rsi,[rip+.Lrp_store]
 mov rdx,rax
 call .Lrp_set
 jmp .Lrp_end
.Lrp_bad:
 mov eax,1
.Lrp_end:
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lrp_api: .asciz "api"
.Lrp_info: .asciz "modelInfo"
.Lrp_endpoint: .asciz "endpoint"
.Lrp_env: .asciz "PANGU_MODEL_ENDPOINT"
.Lrp_suffix: .asciz "/responses"
.Lrp_responses: .asciz "openai-responses"
.Lrp_messages: .asciz "messages"
.Lrp_input: .asciz "input"
.Lrp_type: .asciz "type"
.Lrp_role: .asciz "role"
.Lrp_content: .asciz "content"
.Lrp_tool: .asciz "tool"
.Lrp_assistant: .asciz "assistant"
.Lrp_image_url: .asciz "image_url"
.Lrp_url: .asciz "url"
.Lrp_input_image: .asciz "input_image"
.Lrp_input_text: .asciz "input_text"
.Lrp_output_text: .asciz "output_text"
.Lrp_tool_calls: .asciz "tool_calls"
.Lrp_tool_call_id: .asciz "tool_call_id"
.Lrp_call_id: .asciz "call_id"
.Lrp_id: .asciz "id"
.Lrp_tools: .asciz "tools"
.Lrp_function: .asciz "function"
.Lrp_name: .asciz "name"
.Lrp_arguments: .asciz "arguments"
.Lrp_output: .asciz "output"
.Lrp_max_tokens: .asciz "max_tokens"
.Lrp_max_completion_tokens: .asciz "max_completion_tokens"
.Lrp_max_output_tokens: .asciz "max_output_tokens"
.Lrp_reasoning_effort: .asciz "reasoning_effort"
.Lrp_reasoning: .asciz "reasoning"
.Lrp_effort: .asciz "effort"
.Lrp_thinking: .asciz "thinking"
.Lrp_stream_options: .asciz "stream_options"
.Lrp_store: .asciz "store"
.Lrp_call_json: .asciz "{\"type\":\"function_call\"}"
.Lrp_result_json: .asciz "{\"type\":\"function_call_output\"}"
.Lrp_reason_json: .asciz "{\"summary\":\"auto\"}"

.section .rodata
.Lrp_no_output: .asciz "(no tool output)"

.text
.Lrp_set:
 push rbx
 mov rbx,rdx
 test rdx,rdx
 jz .Lrp_set_bad
 call json_object_object_add@PLT
 test eax,eax
 jz .Lrp_set_end
 mov rdi,rbx
 call json_object_put@PLT
.Lrp_set_bad:
 mov eax,1
.Lrp_set_end:
 pop rbx
 ret

.text
.macro RHGET obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lrh_\key]
 call json_object_object_get@PLT
.endm
.macro RHCOPY dst,src,from,to
 mov rdi,\dst
 mov rsi,\src
 lea rdx,[rip+.Lrh_\from]
 lea rcx,[rip+.Lrh_\to]
 call .Lrp_copy
 test eax,eax
 jnz .Lrh_bad
.endm
/* Canonical content keeps signatures beside their blocks. Reconstruct the
   provider items only for the current request, respecting model identity. */
.globl pg_mimo_responses_history
.type pg_mimo_responses_history,@function
pg_mimo_responses_history:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsp
 xor eax,eax
 mov ecx,8
 rep stosq
 mov [rsp+8],rdx
 mov qword ptr [rdx],0
 RHGET rbx,content
 mov r13,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lrh_bad
 RHGET rbx,model
 mov [rsp+24],rax
 RHGET r12,model
 mov rdi,rax
 mov rsi,[rsp+24]
 call json_object_equal@PLT
 test eax,eax
 jz .Lrh_identity_ready
 RHGET rbx,provider
 mov [rsp+24],rax
 test rax,rax
 jz .Lrh_identity_ready
 RHGET r12,modelProvider
 test rax,rax
 jnz .Lrh_provider_ready
 RHGET r12,modelInfo
 RHGET rax,provider
 test rax,rax
 jz .Lrh_identity_ready
.Lrh_provider_ready:
 mov rdi,rax
 mov rsi,[rsp+24]
 call json_object_equal@PLT
 test eax,eax
 jz .Lrh_identity_ready
 RHGET rbx,api
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lrh_identity_ready
 mov rdi,rax
 lea rsi,[rip+.Lrp_responses]
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 mov [rsp+16],eax
.Lrh_identity_ready:
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lrh_bad
 xor r14d,r14d
.Lrh_each:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lrh_publish
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbp,rax
 RHGET rbp,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lrh_bad
 mov [rsp+24],rax
 mov rdi,rax
 lea rsi,[rip+.Lrh_thinking]
 call strcmp@PLT
 test eax,eax
 jz .Lrh_think
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lrh_toolCall]
 call strcmp@PLT
 test eax,eax
 jz .Lrh_tool
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lrh_text]
 call strcmp@PLT
 test eax,eax
 jnz .Lrh_bad
 lea rdi,[rip+.Lrh_text_json]
 mov rsi,[rsp]
 call .Lrp_append
 mov [rsp+32],rax
 test rax,rax
 jz .Lrh_bad
 RHGET rax,content
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov [rsp+40],rax
 RHCOPY "qword ptr [rsp+40]",rbp,text,text
 cmp dword ptr [rsp+16],0
 je .Lrh_next
 RHGET rbp,textSignature
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lrh_next
 cmp byte ptr [rax],'{'
 jne .Lrh_next
 mov rdi,rax
 call json_tokener_parse@PLT
 mov [rsp+48],rax
 test rax,rax
 jz .Lrh_bad
 RHCOPY "qword ptr [rsp+32]","qword ptr [rsp+48]",id,id
 RHCOPY "qword ptr [rsp+32]","qword ptr [rsp+48]",phase,phase
 jmp .Lrh_next
.Lrh_think:
 cmp dword ptr [rsp+16],0
 je .Lrh_next
 RHGET rbp,thinkingSignature
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lrh_next
 cmp byte ptr [rax],'{'
 jne .Lrh_next
 mov rdi,rax
 call json_tokener_parse@PLT
 mov [rsp+48],rax
 test rax,rax
 jz .Lrh_bad
 RHGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lrh_bad
 mov rdi,rax
 lea rsi,[rip+.Lrh_reasoning]
 call strcmp@PLT
 test eax,eax
 jnz .Lrh_bad
 mov rdi,[rsp]
 mov rsi,[rsp+48]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lrh_bad
 mov qword ptr [rsp+48],0
 jmp .Lrh_next
.Lrh_tool:
 lea rdi,[rip+.Lrp_call_json]
 mov rsi,[rsp]
 call .Lrp_append
 mov [rsp+32],rax
 test rax,rax
 jz .Lrh_bad
 RHCOPY "qword ptr [rsp+32]",rbp,id,call_id
 RHCOPY "qword ptr [rsp+32]",rbp,name,name
 RHGET rbp,arguments
 mov rdi,rax
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Lrh_bad
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lrh_arguments]
 mov rdx,rax
 call li_field
 test eax,eax
 jnz .Lrh_bad
 cmp dword ptr [rsp+16],0
 je .Lrh_next
 RHGET rbp,responseItemId
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lrh_namespace_handler
 mov rdi,rax
 lea rsi,[rip+.Lrh_fc]
 mov edx,3
 call strncmp@PLT
 test eax,eax
 jnz .Lrh_namespace_handler
 RHCOPY "qword ptr [rsp+32]",rbp,responseItemId,id
.Lrh_namespace_handler:
 RHCOPY "qword ptr [rsp+32]",rbp,namespace,namespace
.Lrh_next:
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov qword ptr [rsp+48],0
 inc r14
 jmp .Lrh_each
.Lrh_publish:
 mov rax,[rsp]
 mov rdx,[rsp+8]
 mov [rdx],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
 jmp .Lrh_cleanup
.Lrh_bad:
 mov ebp,1
.Lrh_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_mimo_responses_history,.-pg_mimo_responses_history
.section .rodata
.Lrh_content: .asciz "content"
.Lrh_model: .asciz "model"
.Lrh_provider: .asciz "provider"
.Lrh_modelProvider: .asciz "modelProvider"
.Lrh_api: .asciz "api"
.Lrh_type: .asciz "type"
.Lrh_text: .asciz "text"
.Lrh_thinking: .asciz "thinking"
.Lrh_reasoning: .asciz "reasoning"
.Lrh_toolCall: .asciz "toolCall"
.Lrh_textSignature: .asciz "textSignature"
.Lrh_thinkingSignature: .asciz "thinkingSignature"
.Lrh_id: .asciz "id"
.Lrh_phase: .asciz "phase"
.Lrh_call_id: .asciz "call_id"
.Lrh_name: .asciz "name"
.Lrh_arguments: .asciz "arguments"
.Lrh_responseItemId: .asciz "responseItemId"
.Lrh_namespace: .asciz "namespace"
.Lrh_fc: .asciz "fc_"
.Lrh_text_json: .asciz "{\"role\":\"assistant\",\"content\":[{\"type\":\"output_text\",\"annotations\":[]}]}"

.section .rodata
.Lrp_responsesOutput: .asciz "responsesOutput"

.section .rodata
.Lrh_modelInfo: .asciz "modelInfo"
