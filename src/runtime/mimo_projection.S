/* mimo_projection.S — 原生模型请求投影（encode_request）。
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
 mov rdi,rbx
 call pg_mimo_uses_google
 test eax,eax
 jz .Lmsg_append
 lea rdx,[r13+PG_MM_MSG_TOOL_NAME]
 cmp byte ptr [rdx],0
 jne .Lg_legacy_result_name
 mov rax,[rsp+48]
 sub rax,[rsp+120]
 add rax,[rsp+112]
 imul rax,rax,PG_MM_MSG_BYTES
 lea rdx,[r12+rax+PG_MM_MSG_TOOL_NAME]
.Lg_legacy_result_name:
 mov rdi,r15
 lea rsi,[rip+.Lap_toolName]
 call li_field
 test eax,eax
 jnz .Linvalid
 mov edi,[r13+PG_MM_MSG_IS_ERROR]
 call json_object_new_boolean@PLT
 mov rdi,r15
 lea rsi,[rip+.Lap_isError]
 mov rdx,rax
 call .Lrp_set
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
 mov rax,[rsp+40]
 mov rax,[rax+72]
 test rax,rax
 jz .Lsampling_ready
 mov rdi,[rax+32]
 test rdi,rdi
 jz .Lsampling_ready
 call json_tokener_parse@PLT
 test rax,rax
 jz .Linvalid
 mov [rsp+96],rax
 or qword ptr [rsp+104],512
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lsampling_key]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Linvalid
 and qword ptr [rsp+104],-513
.Lsampling_ready:
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
 mov rdi,[rsp]
 mov rsi,[rbx+PG_MM_CONFIGURATION]
 mov rdx,r12
 call .Lsb_room
 mov [rsp+136],rax
 mov rdi,[rsp]
 mov rsi,rax
 mov rdx,[rbx+PG_MM_CONFIGURATION]
 call .Lsb_cap
 test eax,eax
 jnz .Linvalid
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lbp_sampling_ready
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.LbpK_options]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.LbpK_toolChoice]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lbp_sampling_ready
 mov rdi,rax
 lea rsi,[rip+.LbpK_none]
 call strcmp@PLT
 test eax,eax
 jnz .Lbp_sampling_ready
 mov rdi,[rsp]
 lea rsi,[rip+.Lbd_tools]
 call json_object_object_del@PLT
.Lbp_sampling_ready:
 mov rdi,[rsp]
 mov rsi,[rbx+PG_MM_CONFIGURATION]
 call .Lsampling_admit
 test eax,eax
 jnz .Linvalid
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lserialize_anthropic
 mov rdi,[rsp]
 mov rsi,[rbx+PG_MM_CONFIGURATION]
 call .Lap_request
 test eax,eax
 jnz .Linvalid
 mov rdi,[rsp]
 mov rsi,[rbx+PG_MM_CONFIGURATION]
 call .Lbd_request
 test eax,eax
 jnz .Linvalid
 jmp .Lserialize_ready
.Lserialize_anthropic:
 mov rdi,rbx
 call pg_mimo_uses_anthropic
 test eax,eax
 jz .Lserialize_openai
 mov rdi,[rsp]
 mov rsi,[rbx+PG_MM_CONFIGURATION]
 call .Lap_request
 test eax,eax
 jnz .Linvalid
 jmp .Lserialize_ready
.Lserialize_openai:
 mov rdi,rbx
 call pg_mimo_uses_google
 test eax,eax
 jz .Lserialize_responses
 mov rdi,[rsp]
 mov rsi,[rbx+PG_MM_CONFIGURATION]
 call .Lap_request
 test eax,eax
 jnz .Linvalid
 mov rdi,[rsp]
 mov rsi,[rbx+PG_MM_CONFIGURATION]
 call .Lg_request
 test eax,eax
 jnz .Linvalid
 jmp .Lserialize_ready
.Lserialize_responses:
 mov rdi,rbx
 call pg_mimo_uses_responses
 test eax,eax
 jz .Lserialize_ready
 mov rdi,[rsp]
 mov rsi,[rbx+PG_MM_CONFIGURATION]
 call .Lrp_request
 test eax,eax
 jnz .Linvalid
.Lserialize_ready:
 mov rdi,[rsp]
 mov rsi,[rbx+PG_MM_CONFIGURATION]
 call .Lsb_bedrock_adjust
 test eax,eax
 jnz .Linvalid
 mov rdi,[rsp]
 mov rsi,[rsp+136]
 mov rdx,[rbx+PG_MM_CONFIGURATION]
 call .Lsb_cap
 test eax,eax
 jnz .Linvalid
 mov rdi,[rsp]
 mov rsi,[rbx+PG_MM_CONFIGURATION]
 call .Lcd_request
 test eax,eax
 jnz .Linvalid
 mov rdi,[rsp]
 mov rsi,rbx
 call .Loc_cache
 test eax,eax
 jnz .Linvalid
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
 jz .Lopts_no_options
 mov r13,[rsp]
 mov rdi,r13
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lopts_bad
.Lopts_no_options:
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
 mov rdi,rbx
 lea rsi,[rip+.Lcompat_model_info]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lopts_maximum_key]
 call json_object_object_get@PLT
 mov [rsp+8],rax
 test rax,rax
 jnz .Lsb_default_known
 mov edi,512
 call json_object_new_int@PLT
 jmp .Lopts_maximum_value
.Lsb_default_known:
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
 sub rsp,80
 mov rbx,rdi
 mov [rsp+64],rsi
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
 mov rax,[rsp+64]
 RPGET rax,azureDeploymentName
 test rax,rax
 jz .Lrp_deployment_ready
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lrp_model_field]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lrp_bad
.Lrp_deployment_ready:
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
 add rsp,80
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

.section .rodata
.Lrp_responsesOutput: .asciz "responsesOutput"

.text
.globl pg_mimo_uses_anthropic
.type pg_mimo_uses_anthropic,@function
pg_mimo_uses_anthropic:
 mov rdi,[rdi+PG_MM_CONFIGURATION]
 jmp pg_mimo_config_anthropic
.size pg_mimo_uses_anthropic,.-pg_mimo_uses_anthropic

.macro APGET obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lap_\key]
 call json_object_object_get@PLT
.endm
.macro APCOPY dst,src,from,to
 mov rdi,\dst
 mov rsi,\src
 lea rdx,[rip+.Lap_\from]
 lea rcx,[rip+.Lap_\to]
 call .Lrp_copy
 test eax,eax
 jnz .Lap_bad
.endm
/* Convert the shared conversation projection to Messages wire blocks. The
   request root owns all appended objects; only the pending arrays are local. */
.text
.Lap_request:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,80
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsp
 xor eax,eax
 mov ecx,10
 rep stosq
 APGET rbx,messages
 mov r13,rax
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lap_bad
 call json_object_new_array@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lap_bad
 xor r14d,r14d
.Lap_each:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lap_messages_done
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbp,rax
 APGET rbp,role
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lap_bad
 mov [rsp+24],rax
 mov rdi,rax
 lea rsi,[rip+.Lap_system]
 call strcmp@PLT
 test eax,eax
 jz .Lap_system_row
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lap_tool]
 call strcmp@PLT
 test eax,eax
 jz .Lap_tool_row
 mov qword ptr [rsp+16],0
 lea rdi,[rip+.Lap_message_json]
 mov rsi,[rsp]
 call .Lrp_append
 mov [rsp+32],rax
 test rax,rax
 jz .Lap_bad
 APCOPY rax,rbp,role,role
 APGET rbp,anthropicContent
 test rax,rax
 jz .Lap_regular
 mov rdi,rax
 call json_object_get@PLT
 jmp .Lap_attach_content
.Lap_regular:
 APGET rbp,content
 mov rdi,rax
 call .Lap_convert_content
 test rax,rax
 jz .Lap_bad
 mov [rsp+40],rax
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lap_content]
 mov rdx,rax
 mov qword ptr [rsp+40],0
 call .Lrp_set
 test eax,eax
 jnz .Lap_bad
 APGET rbp,tool_calls
 mov [rsp+48],rax
 mov qword ptr [rsp+56],0
.Lap_calls:
 mov rdi,[rsp+48]
 test rdi,rdi
 jz .Lap_next
 call json_object_array_length@PLT
 cmp [rsp+56],rax
 jae .Lap_next
 mov rdi,[rsp+48]
 mov rsi,[rsp+56]
 call json_object_array_get_idx@PLT
 mov [rsp+64],rax
 APGET "qword ptr [rsp+32]",content
 mov rsi,rax
 lea rdi,[rip+.Lap_use_json]
 call .Lrp_append
 mov [rsp+72],rax
 test rax,rax
 jz .Lap_bad
 APCOPY rax,"qword ptr [rsp+64]",id,id
 APGET "qword ptr [rsp+64]",function
 mov [rsp+64],rax
 APCOPY "qword ptr [rsp+72]",rax,name,name
 APGET "qword ptr [rsp+64]",arguments
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 call json_tokener_parse@PLT
 mov rdi,[rsp+72]
 lea rsi,[rip+.Lap_input]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_bad
 inc qword ptr [rsp+56]
 jmp .Lap_calls
.Lap_attach_content:
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lap_content]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_bad
 jmp .Lap_next
.Lap_system_row:
 lea rdi,[rip+.Lap_text_json]
 mov rsi,[rsp+8]
 call .Lrp_append
 test rax,rax
 jz .Lap_bad
 APCOPY rax,rbp,content,text
 jmp .Lap_next
.Lap_tool_row:
 cmp qword ptr [rsp+16],0
 jne .Lap_tool_group
 lea rdi,[rip+.Lap_user_json]
 mov rsi,[rsp]
 call .Lrp_append
 test rax,rax
 jz .Lap_bad
 APGET rax,content
 mov [rsp+16],rax
.Lap_tool_group:
 lea rdi,[rip+.Lap_result_json]
 mov rsi,[rsp+16]
 call .Lrp_append
 mov [rsp+32],rax
 test rax,rax
 jz .Lap_bad
 APCOPY rax,rbp,tool_call_id,tool_use_id
 APCOPY "qword ptr [rsp+32]",rbp,isError,is_error
 APCOPY "qword ptr [rsp+32]",rbp,toolName,toolName
 APGET rbp,content
 mov rdi,rax
 call .Lap_convert_content
 test rax,rax
 jz .Lap_bad
 jmp .Lap_attach_content
.Lap_next:
 inc r14
 jmp .Lap_each
.Lap_messages_done:
 mov rdi,r12
 call pg_mimo_config_google
 test eax,eax
 jnz .Lap_google_messages_done
 mov rdi,r12
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lbd_split_error_images
 mov rdi,r12
 call .Lbd_claude
 test eax,eax
 jz .Lbd_error_images_ready
.Lbd_split_error_images:
 mov rdi,[rsp]
 call .Lap_error_images
 test eax,eax
 jnz .Lap_bad
.Lbd_error_images_ready:
 mov rdi,r12
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jnz .Lap_google_root
 mov rdi,[rsp]
 call .Lap_normalize_messages
 test eax,eax
 jnz .Lap_bad
 jmp .Lap_google_root
.Lap_google_messages_done:
 mov rdi,r12
 call pg_mimo_config_google
 test eax,eax
 jz .Lap_google_root
 APGET r12,model
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 call pg_google_requires_id
 test eax,eax
 jz .Lap_google_root
 mov rdi,[rsp]
 call .Lap_normalize_messages
 test eax,eax
 jnz .Lap_bad
.Lap_google_root:
 mov rdi,rbx
 lea rsi,[rip+.Lap_messages]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lap_bad
 mov rdi,[rsp+8]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lap_tools_begin
 mov rdi,rbx
 lea rsi,[rip+.Lap_system]
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 call .Lrp_set
 test eax,eax
 jnz .Lap_bad
.Lap_tools_begin:
 APGET rbx,tools
 mov r13,rax
 test rax,rax
 jz .Lap_options_begin
 xor r14d,r14d
.Lap_tool_def:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lap_options_begin
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbp,rax
 APGET rbp,function
 mov [rsp+24],rax
 APCOPY rbp,rax,name,name
 APCOPY rbp,"qword ptr [rsp+24]",description,description
 APCOPY rbp,"qword ptr [rsp+24]",strict,strict
 APGET "qword ptr [rsp+24]",parameters
 mov rdi,rbp
 mov rsi,rax
 mov rdx,r12
 call .Lap_tool_schema
 test eax,eax
 jnz .Lap_bad
 mov rdi,rbp
 lea rsi,[rip+.Lap_function]
 call json_object_object_del@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lap_type]
 call json_object_object_del@PLT
 inc r14
 jmp .Lap_tool_def
.Lap_options_begin:
 mov rdi,rbx
 lea rsi,[rip+.Lap_stream_options]
 call json_object_object_del@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lap_reasoning_effort]
 call json_object_object_del@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lap_max_completion_tokens]
 call json_object_object_del@PLT
 APGET r12,options
 mov r13,rax
 APGET rax,maxTokens
 test rax,rax
 jnz .Lap_max_ready
 APGET r12,modelInfo
 APGET rax,maxTokens
 test rax,rax
 jz .Lap_success
.Lap_max_ready:
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lap_max_tokens]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_bad
.Lap_success:
 mov rdi,r12
 call pg_mimo_config_google
 test eax,eax
 jnz .Lap_success_ready
 mov rdi,r12
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jnz .Lap_success_ready
 mov rdi,rbx
 mov rsi,r12
 call .Lap_cache_options
 test eax,eax
 jnz .Lap_bad
 mov rdi,rbx
 mov rsi,r12
 call .Lap_provider_options
 test eax,eax
 jnz .Lap_bad
 mov rdi,rbx
 mov rsi,r12
 call .Lap_thinking_options
 test eax,eax
 jnz .Lap_bad
.Lap_success_ready:
 xor ebp,ebp
 jmp .Lap_cleanup
.Lap_bad:
 mov ebp,1
.Lap_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,80
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Owned array of text/image blocks from shared Chat content. */
.Lap_convert_content:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov qword ptr [rsp],0
 call json_object_new_array@PLT
 mov r12,rax
 test rax,rax
 jz .Lap_content_end
 test rbx,rbx
 jz .Lap_content_ok
 mov rdi,rbx
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lap_content_array
 mov rdi,rbx
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lap_content_ok
 lea rdi,[rip+.Lap_text_json]
 mov rsi,r12
 call .Lrp_append
 mov r13,rax
 test rax,rax
 jz .Lap_content_bad
 mov rdi,rbx
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lap_content_ok
 mov rdi,rbx
 call json_object_get@PLT
 mov rdi,r13
 lea rsi,[rip+.Lap_text]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_content_bad
 jmp .Lap_content_ok
.Lap_content_array:
 mov rdi,rbx
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lap_content_bad
 xor r13d,r13d
.Lap_content_each:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lap_content_ok
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 APGET r14,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lap_content_bad
 mov rdi,rax
 lea rsi,[rip+.Lap_text]
 call strcmp@PLT
 test eax,eax
 jnz .Lap_image
 mov rdi,r14
 call json_object_get@PLT
 mov rdi,r12
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz .Lap_content_next
 mov rdi,r14
 call json_object_put@PLT
 jmp .Lap_content_bad
.Lap_image:
 APGET r14,image_url
 APGET rax,url
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 test rax,rax
 jz .Lap_content_bad
 mov rdi,rax
 lea rsi,[rip+.Lap_data_prefix]
 mov edx,5
 call strncmp@PLT
 test eax,eax
 jnz .Lap_content_bad
 mov rdi,r14
 lea rsi,[rip+.Lap_base64]
 call strstr@PLT
 test rax,rax
 jz .Lap_content_bad
 mov rbp,rax
 lea rdi,[rip+.Lap_image_json]
 mov rsi,r12
 call .Lrp_append
 test rax,rax
 jz .Lap_content_bad
 APGET rax,source
 mov [rsp],rax
 lea rdi,[r14+5]
 mov rsi,rbp
 sub rsi,rdi
 call json_object_new_string_len@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lap_media_type]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_content_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lap_data]
 lea rdx,[rbp+8]
 call li_field
 test eax,eax
 jnz .Lap_content_bad
.Lap_content_next:
 inc r13
 jmp .Lap_content_each
.Lap_content_bad:
 mov rdi,r12
 call json_object_put@PLT
 xor r12d,r12d
.Lap_content_ok:
 mov rax,r12
.Lap_content_end:
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lap_messages: .asciz "messages"
.Lap_system: .asciz "system"
.Lap_role: .asciz "role"
.Lap_content: .asciz "content"
.Lap_type: .asciz "type"
.Lap_text: .asciz "text"
.Lap_tool: .asciz "tool"
.Lap_tool_calls: .asciz "tool_calls"
.Lap_function: .asciz "function"
.Lap_arguments: .asciz "arguments"
.Lap_id: .asciz "id"
.Lap_name: .asciz "name"
.Lap_input: .asciz "input"
.Lap_tools: .asciz "tools"
.Lap_description: .asciz "description"
.Lap_parameters: .asciz "parameters"
.Lap_input_schema: .asciz "input_schema"
.Lap_tool_call_id: .asciz "tool_call_id"
.Lap_tool_use_id: .asciz "tool_use_id"
.Lap_isError: .asciz "isError"
.Lap_is_error: .asciz "is_error"
.Lap_anthropicContent: .asciz "anthropicContent"
.Lap_stream_options: .asciz "stream_options"
.Lap_reasoning_effort: .asciz "reasoning_effort"
.Lap_max_completion_tokens: .asciz "max_completion_tokens"
.Lap_options: .asciz "options"
.Lap_maxTokens: .asciz "maxTokens"
.Lap_modelInfo: .asciz "modelInfo"
.Lap_max_tokens: .asciz "max_tokens"
.Lap_image_url: .asciz "image_url"
.Lap_url: .asciz "url"
.Lap_source: .asciz "source"
.Lap_media_type: .asciz "media_type"
.Lap_data: .asciz "data"
.Lap_data_prefix: .asciz "data:"
.Lap_base64: .asciz ";base64,"
.Lap_image_json: .asciz "{\"type\":\"image\",\"source\":{\"type\":\"base64\"}}"
.Lap_text_json: .asciz "{\"type\":\"text\",\"text\":\"\"}"
.Lap_message_json: .asciz "{}"
.Lap_user_json: .asciz "{\"role\":\"user\",\"content\":[]}"
.Lap_use_json: .asciz "{\"type\":\"tool_use\"}"
.Lap_result_json: .asciz "{\"type\":\"tool_result\",\"is_error\":false}"

.text
/* Anthropic's wire ID restriction applies symmetrically to uses and results. */
.Lap_normalize_messages:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,96
 mov rbx,rdi
 xor r12d,r12d
.Lap_norm_message:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lap_norm_ok
 mov rdi,rbx
 mov rsi,r12
 call json_object_array_get_idx@PLT
 APGET rax,content
 mov r13,rax
 xor r14d,r14d
.Lap_norm_block:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lap_norm_next_message
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbp,rax
 APGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lap_norm_next
 mov [rsp+80],rax
 mov rdi,rax
 lea rsi,[rip+.Lap_use_type]
 call strcmp@PLT
 lea rcx,[rip+.Lap_id]
 test eax,eax
 jz .Lap_norm_key
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lap_result_type]
 call strcmp@PLT
 test eax,eax
 jnz .Lap_norm_next
 lea rcx,[rip+.Lap_tool_use_id]
.Lap_norm_key:
 mov [rsp+72],rcx
 mov rdi,rbp
 mov rsi,rcx
 call json_object_object_get@PLT
 mov [rsp+80],rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lap_norm_bad
 mov edx,64
 cmp eax,edx
 cmova eax,edx
 mov [rsp+88],eax
 mov rdi,[rsp+80]
 call json_object_get_string@PLT
 xor ecx,ecx
.Lap_norm_char:
 movzx edx,byte ptr [rax+rcx]
 cmp dl,'a'
 jb .Lap_norm_upper
 cmp dl,'z'
 jbe .Lap_norm_store
.Lap_norm_upper:
 cmp dl,'A'
 jb .Lap_norm_digit
 cmp dl,'Z'
 jbe .Lap_norm_store
.Lap_norm_digit:
 cmp dl,'0'
 jb .Lap_norm_other
 cmp dl,'9'
 jbe .Lap_norm_store
.Lap_norm_other:
 cmp dl,'-'
 je .Lap_norm_store
 mov dl,'_'
.Lap_norm_store:
 mov [rsp+rcx],dl
 inc ecx
 cmp ecx,[rsp+88]
 jb .Lap_norm_char
 mov rdi,rsp
 mov esi,ecx
 call json_object_new_string_len@PLT
 mov rdi,rbp
 mov rsi,[rsp+72]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_norm_bad
.Lap_norm_next:
 inc r14
 jmp .Lap_norm_block
.Lap_norm_next_message:
 inc r12
 jmp .Lap_norm_message
.Lap_norm_ok:
 xor eax,eax
 jmp .Lap_norm_end
.Lap_norm_bad:
 mov eax,1
.Lap_norm_end:
 add rsp,96
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Native reasoningEffort maps to Pi's simple-options budgets. Direct provider
   options retain their budget/adaptive meaning. */
.Lap_thinking_options:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+24],0
 APGET r12,options
 mov r13,rax
 APGET rax,reasoningEffort
 mov [rsp+8],rax
 APGET r13,thinkingEnabled
 test rax,rax
 jz .Lap_thinking_effort
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lap_thinking_disabled
 jmp .Lap_thinking_enabled
.Lap_thinking_effort:
 cmp qword ptr [rsp+8],0
 je .Lap_thinking_disabled
 mov rdi,[rsp+8]
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lap_off]
 call strcmp@PLT
 test eax,eax
 jz .Lap_thinking_disabled
.Lap_thinking_enabled:
 mov rdi,rbx
 lea rsi,[rip+.Lap_temperature]
 call json_object_object_del@PLT
 APGET r12,modelInfo
 APGET rax,compat
 APGET rax,forceAdaptiveThinking
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lap_thinking_adaptive
 lea rdi,[rip+.Lap_thinking_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lap_thinking_bad
 mov r14d,1024
 cmp qword ptr [rsp+8],0
 je .Lap_thinking_budget
 mov r14d,16384
 mov rdi,[rsp+8]
 call json_object_get_string@PLT
 mov [rsp+16],rax
 mov rdi,rax
 lea rsi,[rip+.Lap_minimal]
 call strcmp@PLT
 test eax,eax
 jz .Lap_budget_minimal
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lap_low]
 call strcmp@PLT
 test eax,eax
 jz .Lap_budget_low
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lap_medium]
 call strcmp@PLT
 test eax,eax
 jnz .Lap_thinking_budget
 mov r14d,8192
 jmp .Lap_thinking_budget
.Lap_budget_minimal:
 mov r14d,1024
 jmp .Lap_thinking_budget
.Lap_budget_low:
 mov r14d,2048
.Lap_thinking_budget:
 cmp qword ptr [rsp+8],0
 je .Lsb_flat_budget
 APGET r13,thinkingBudgets
 mov [rsp+32],rax
 test rax,rax
 jz .Lsb_flat_budget
 mov rdi,[rsp+16]
 lea rsi,[rip+.LbcK_xhigh]
 call strcmp@PLT
 test eax,eax
 jz .Lsb_high_budget
 mov rdi,[rsp+16]
 lea rsi,[rip+.LbcK_max]
 call strcmp@PLT
 test eax,eax
 jz .Lsb_high_budget
 mov rsi,[rsp+16]
 jmp .Lsb_custom_budget
.Lsb_high_budget:
 lea rsi,[rip+.LbcK_high]
.Lsb_custom_budget:
 mov rdi,[rsp+32]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lsb_flat_budget
 mov rdi,rax
 call json_object_get_int64@PLT
 test rax,rax
 js .Lap_thinking_bad
 mov r14,rax
.Lsb_flat_budget:
 APGET r13,thinkingBudgetTokens
 test rax,rax
 jz .Lap_thinking_cap
 mov rdi,rax
 call json_object_get_int64@PLT
 test rax,rax
 jle .Lap_thinking_bad
 mov r14,rax
.Lap_thinking_cap:
 APGET rbx,max_tokens
 mov rdi,rax
 call json_object_get_int64@PLT
 mov rbp,rax
 APGET r12,modelInfo
 APGET rax,maxTokens
 test rax,rax
 jz .Lap_thinking_cap_known
 mov rdi,rax
 call json_object_get_int64@PLT
 mov rbp,rax
.Lap_thinking_cap_known:
 APGET r13,maxTokens
 test rax,rax
 jz .Lap_thinking_adjusted
 mov rdi,rax
 call json_object_get_int64@PLT
 add rax,r14
 jc .Lap_thinking_bad
 cmp rax,rbp
 cmovb rbp,rax
.Lap_thinking_adjusted:
 mov rax,rbp
 sub rax,1024
 xor ecx,ecx
 test rax,rax
 cmovs rax,rcx
 cmp r14,rax
 cmova r14,rax
 mov rdi,rbp
 call json_object_new_int64@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lap_max_tokens]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_thinking_bad
 mov rdi,r14
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lap_budget_tokens]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_thinking_bad
 jmp .Lap_thinking_adopt
.Lap_thinking_adaptive:
 lea rdi,[rip+.Lap_adaptive_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lap_thinking_bad
 APGET r13,effort
 test rax,rax
 jnz .Lap_thinking_effort_value
 mov rax,[rsp+8]
 test rax,rax
 jz .Lap_thinking_adopt
.Lap_thinking_effort_value:
 mov [rsp+16],rax
 lea rdi,[rip+.Lap_empty_json]
 call json_tokener_parse@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lap_thinking_bad
 mov rdi,[rsp+16]
 call json_object_get@PLT
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lap_effort]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_thinking_bad
 mov rdi,rbx
 lea rsi,[rip+.Lap_output_config]
 mov rdx,[rsp+24]
 mov qword ptr [rsp+24],0
 call .Lrp_set
 test eax,eax
 jnz .Lap_thinking_bad
 jmp .Lap_thinking_adopt
.Lap_thinking_disabled:
 mov rdi,[rsp]
 call json_object_put@PLT
 lea rdi,[rip+.Lap_disabled_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lap_thinking_bad
 jmp .Lap_display_ready
.Lap_thinking_adopt:
 APGET r13,thinkingDisplay
 test rax,rax
 jz .Lap_display_ready
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lap_display]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_thinking_bad
.Lap_display_ready:
 mov rdi,rbx
 lea rsi,[rip+.Lap_thinking]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lap_thinking_bad
 xor ebp,ebp
 jmp .Lap_thinking_end
.Lap_thinking_bad:
 mov ebp,1
.Lap_thinking_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lap_use_type: .asciz "tool_use"
.Lap_result_type: .asciz "tool_result"
.Lap_thinking: .asciz "thinking"
.Lap_reasoningEffort: .asciz "reasoningEffort"
.Lap_thinkingEnabled: .asciz "thinkingEnabled"
.Lap_thinkingBudgetTokens: .asciz "thinkingBudgetTokens"
.Lap_compat: .asciz "compat"
.Lap_forceAdaptiveThinking: .asciz "forceAdaptiveThinking"
.Lap_effort: .asciz "effort"
.Lap_output_config: .asciz "output_config"
.Lap_temperature: .asciz "temperature"
.Lap_budget_tokens: .asciz "budget_tokens"
.Lap_off: .asciz "off"
.Lap_minimal: .asciz "minimal"
.Lap_low: .asciz "low"
.Lap_medium: .asciz "medium"
.Lap_thinking_json: .asciz "{\"type\":\"enabled\",\"display\":\"summarized\"}"
.Lap_adaptive_json: .asciz "{\"type\":\"adaptive\",\"display\":\"summarized\"}"
.Lap_disabled_json: .asciz "{\"type\":\"disabled\"}"
.Lap_empty_json: .asciz "{}"

/* Provider options follow buildParams: strings become typed tool choices,
   object choices retain disable_parallel_tool_use; metadata exposes user_id. */
.text
.Lap_provider_options:
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
 APGET r12,options
 mov r13,rax
 APGET rax,toolChoice
 mov r14,rax
 test rax,rax
 jz .Lap_provider_metadata
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lap_provider_choice_object
 lea rdi,[rip+.Lap_empty_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lap_provider_bad
 mov rdi,r14
 call json_object_get@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lap_type]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_provider_bad
 jmp .Lap_provider_choice_adopt
.Lap_provider_choice_object:
 mov rdi,r14
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lap_provider_bad
 mov rdi,r14
 lea rsi,[rsp]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lap_provider_bad
.Lap_provider_choice_adopt:
 mov rdi,rbx
 lea rsi,[rip+.Lap_tool_choice]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lap_provider_bad
.Lap_provider_metadata:
 APGET r12,modelInfo
 APGET rax,compat
 APGET rax,supportsTemperature
 test rax,rax
 jz .Lap_provider_temperature_ready
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lap_provider_temperature_ready
 mov rdi,rbx
 lea rsi,[rip+.Lap_temperature]
 call json_object_object_del@PLT
.Lap_provider_temperature_ready:
 APGET r13,metadata
 APGET rax,user_id
 mov r14,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lap_provider_display
 lea rdi,[rip+.Lap_empty_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lap_provider_bad
 mov rdi,r14
 call json_object_get@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lap_user_id]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_provider_bad
 mov rdi,rbx
 lea rsi,[rip+.Lap_metadata]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lap_provider_bad
.Lap_provider_display:
 /* Fallbacks carry model identity only, as in the upstream request. */
 APGET r12,modelInfo
 APGET rax,compat
 APGET rax,allowedFallbackModels
 mov r13,rax
 test rax,rax
 jz .Lap_provider_ok
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lap_provider_bad
 mov rdi,r13
 call json_object_array_length@PLT
 test rax,rax
 jz .Lap_provider_ok
 call json_object_new_array@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lap_provider_bad
 xor r14d,r14d
.Lap_provider_fallback:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lap_provider_fallback_adopt
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbp,rax
 lea rdi,[rip+.Lap_empty_json]
 mov rsi,[rsp+8]
 call .Lrp_append
 test rax,rax
 jz .Lap_provider_bad
 mov rdi,rax
 mov rsi,rbp
 lea rdx,[rip+.Lap_model]
 mov rcx,rdx
 call .Lrp_copy
 test eax,eax
 jnz .Lap_provider_bad
 inc r14
 jmp .Lap_provider_fallback
.Lap_provider_fallback_adopt:
 mov rdi,rbx
 lea rsi,[rip+.Lap_fallbacks]
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 call .Lrp_set
 test eax,eax
 jnz .Lap_provider_bad
.Lap_provider_ok:
 xor ebp,ebp
 jmp .Lap_provider_end
.Lap_provider_bad:
 mov ebp,1
.Lap_provider_end:
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
.Lap_toolChoice: .asciz "toolChoice"
.Lap_tool_choice: .asciz "tool_choice"
.Lap_metadata: .asciz "metadata"
.Lap_user_id: .asciz "user_id"
.Lap_allowedFallbackModels: .asciz "allowedFallbackModels"
.Lap_fallbacks: .asciz "fallbacks"
.Lap_model: .asciz "model"

.section .rodata
.Lap_thinkingDisplay: .asciz "thinkingDisplay"
.Lap_display: .asciz "display"

/* Upstream cache breakpoints: system blocks, last tool, last user block. */
.text
.Lap_cache_options:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 APGET r12,modelInfo
 APGET rax,compat
 mov r13,rax
 APGET r12,options
 APGET rax,cacheRetention
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jnz .Lap_cache_retention
 lea rdi,[rip+.Lap_cache_env]
 call getenv@PLT
 test rax,rax
 jz .Lap_cache_short
.Lap_cache_retention:
 mov [rsp+8],rax
 mov rdi,rax
 lea rsi,[rip+.Lap_none]
 call strcmp@PLT
 test eax,eax
 jz .Lap_cache_ok
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lap_long]
 call strcmp@PLT
 test eax,eax
 jnz .Lap_cache_short
 APGET r13,supportsLongCacheRetention
 test rax,rax
 jz .Lap_cache_long
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lap_cache_short
.Lap_cache_long:
 lea rdi,[rip+.Lap_cache_long_json]
 jmp .Lap_cache_create
.Lap_cache_short:
 lea rdi,[rip+.Lap_cache_short_json]
.Lap_cache_create:
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lap_cache_bad
 APGET rbx,system
 mov r14,rax
 xor ebp,ebp
.Lap_cache_system:
 mov rdi,r14
 test rdi,rdi
 jz .Lap_cache_tools
 call json_object_array_length@PLT
 cmp rbp,rax
 jae .Lap_cache_tools
 mov rdi,r14
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov rsi,[rsp]
 call .Lap_cache_attach
 test eax,eax
 jnz .Lap_cache_bad
 inc rbp
 jmp .Lap_cache_system
.Lap_cache_tools:
 APGET r13,supportsCacheControlOnTools
 test rax,rax
 jz .Lap_cache_last_tool
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lap_cache_message
.Lap_cache_last_tool:
 APGET rbx,tools
 mov rdi,rax
 mov rsi,[rsp]
 call .Lap_cache_last
 test eax,eax
 jnz .Lap_cache_bad
.Lap_cache_message:
 APGET rbx,messages
 mov r14,rax
 mov rdi,rax
 call json_object_array_length@PLT
 test rax,rax
 jz .Lap_cache_ok
 mov rdi,r14
 lea rsi,[rax-1]
 call json_object_array_get_idx@PLT
 mov r14,rax
 APGET rax,role
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lap_cache_bad
 mov rdi,rax
 lea rsi,[rip+.Lap_user]
 call strcmp@PLT
 test eax,eax
 jnz .Lap_cache_ok
 APGET r14,content
 mov rdi,rax
 mov rsi,[rsp]
 call .Lap_cache_last
 test eax,eax
 jnz .Lap_cache_bad
.Lap_cache_ok:
 xor ebp,ebp
 jmp .Lap_cache_end
.Lap_cache_bad:
 mov ebp,1
.Lap_cache_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lap_cache_last:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 test rdi,rdi
 jz .Lap_cache_last_empty
 call json_object_array_length@PLT
 test rax,rax
 jz .Lap_cache_last_empty
 mov rdi,rbx
 lea rsi,[rax-1]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov rsi,r12
 call .Lap_cache_attach
 jmp .Lap_cache_last_end
.Lap_cache_last_empty:
 xor eax,eax
.Lap_cache_last_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.Lap_cache_attach:
 push rbx
 mov rbx,rdi
 mov rdi,rsi
 call json_object_get@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lap_cache_control]
 mov rdx,rax
 call .Lrp_set
 pop rbx
 ret
.section .rodata
.Lap_cacheRetention: .asciz "cacheRetention"
.Lap_supportsLongCacheRetention: .asciz "supportsLongCacheRetention"
.Lap_supportsCacheControlOnTools: .asciz "supportsCacheControlOnTools"
.Lap_cache_control: .asciz "cache_control"
.Lap_cache_env: .asciz "PANGU_CACHE_RETENTION"
.Lap_none: .asciz "none"
.Lap_long: .asciz "long"
.Lap_user: .asciz "user"
.Lap_cache_short_json: .asciz "{\"type\":\"ephemeral\"}"
.Lap_cache_long_json: .asciz "{\"type\":\"ephemeral\",\"ttl\":\"1h\"}"

/* Move error-result images after every tool_result in the same user turn.
   Three local arrays own the candidate message, image tail and current result. */
.text
.Lap_error_images:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,96
 mov rbx,rdi
 xor r12d,r12d
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
.Lap_ei_message:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lap_ei_ok
 mov rdi,rbx
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov [rsp+24],rax
 APGET rax,role
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lap_user]
 call strcmp@PLT
 test eax,eax
 jnz .Lap_ei_next_message
 APGET "qword ptr [rsp+24]",content
 mov r13,rax
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lap_ei_bad
 call json_object_new_array@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lap_ei_bad
 xor r14d,r14d
.Lap_ei_block:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lap_ei_tail
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbp,rax
 APGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lap_result_type]
 call strcmp@PLT
 test eax,eax
 jnz .Lap_ei_keep
 APGET rbp,is_error
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lap_ei_remove_name
 APGET rbp,content
 mov [rsp+32],rax
 call json_object_new_array@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lap_ei_bad
 mov qword ptr [rsp+40],0
 mov qword ptr [rsp+48],0
 mov qword ptr [rsp+56],0
.Lap_ei_part:
 mov rdi,[rsp+32]
 call json_object_array_length@PLT
 cmp [rsp+40],rax
 jae .Lap_ei_result_done
 mov rdi,[rsp+32]
 mov rsi,[rsp+40]
 call json_object_array_get_idx@PLT
 mov [rsp+64],rax
 APGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lap_image_type]
 call strcmp@PLT
 test eax,eax
 jnz .Lap_ei_text
 cmp qword ptr [rsp+48],0
 jne .Lap_ei_placeholder
 lea rdi,[rip+.Lap_text_json]
 mov rsi,[rsp+8]
 call .Lrp_append
 mov [rsp+72],rax
 test rax,rax
 jz .Lap_ei_bad
 APGET rbp,tool_use_id
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+80],rax
 APGET rbp,toolName
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jnz .Lap_ei_name
 lea rax,[rip+.Lap_empty_name]
.Lap_ei_name:
 mov rdi,rax
 call strlen@PLT
 mov [rsp+88],rax
 mov rdi,[rsp+80]
 call strlen@PLT
 add rax,[rsp+88]
 add rax,64
 mov rdi,rax
 call malloc@PLT
 test rax,rax
 jz .Lap_ei_bad
 mov [rsp+88],rax
 APGET rbp,toolName
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jnz .Lap_ei_caption_name
 lea rax,[rip+.Lap_empty_name]
.Lap_ei_caption_name:
 mov rcx,rax
 mov rdx,[rsp+80]
 mov rdi,[rsp+88]
 lea rsi,[rip+.Lap_caption_format]
 xor eax,eax
 call sprintf@PLT
 mov rdi,[rsp+72]
 lea rsi,[rip+.Lap_text]
 mov rdx,[rsp+88]
 call li_field
 mov [rsp+80],eax
 mov rdi,[rsp+88]
 call free@PLT
 cmp dword ptr [rsp+80],0
 jne .Lap_ei_bad
 mov qword ptr [rsp+48],1
.Lap_ei_placeholder:
 cmp qword ptr [rsp+56],0
 jne .Lap_ei_move_image
 lea rdi,[rip+.Lap_placeholder_json]
 mov rsi,[rsp+16]
 call .Lrp_append
 test rax,rax
 jz .Lap_ei_bad
 mov qword ptr [rsp+56],1
.Lap_ei_move_image:
 mov rdi,[rsp+64]
 call json_object_get@PLT
 mov rdi,[rsp+8]
 mov rsi,rax
 call .Lap_ei_append_owned
 test eax,eax
 jnz .Lap_ei_bad
 jmp .Lap_ei_part_next
.Lap_ei_text:
 mov qword ptr [rsp+56],0
 mov rdi,[rsp+64]
 call json_object_get@PLT
 mov rdi,[rsp+16]
 mov rsi,rax
 call .Lap_ei_append_owned
 test eax,eax
 jnz .Lap_ei_bad
.Lap_ei_part_next:
 inc qword ptr [rsp+40]
 jmp .Lap_ei_part
.Lap_ei_result_done:
 mov rdi,rbp
 lea rsi,[rip+.Lap_content]
 mov rdx,[rsp+16]
 mov qword ptr [rsp+16],0
 call .Lrp_set
 test eax,eax
 jnz .Lap_ei_bad
.Lap_ei_remove_name:
 mov rdi,rbp
 lea rsi,[rip+.Lap_toolName]
 call json_object_object_del@PLT
.Lap_ei_keep:
 mov rdi,rbp
 call json_object_get@PLT
 mov rdi,[rsp]
 mov rsi,rax
 call .Lap_ei_append_owned
 test eax,eax
 jnz .Lap_ei_bad
 inc r14
 jmp .Lap_ei_block
.Lap_ei_tail:
 xor r14d,r14d
.Lap_ei_tail_each:
 mov rdi,[rsp+8]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lap_ei_adopt
 mov rdi,[rsp+8]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,[rsp]
 mov rsi,rax
 call .Lap_ei_append_owned
 test eax,eax
 jnz .Lap_ei_bad
 inc r14
 jmp .Lap_ei_tail_each
.Lap_ei_adopt:
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lap_content]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lap_ei_bad
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov qword ptr [rsp+8],0
.Lap_ei_next_message:
 inc r12
 jmp .Lap_ei_message
.Lap_ei_ok:
 xor ebp,ebp
 jmp .Lap_ei_end
.Lap_ei_bad:
 mov ebp,1
.Lap_ei_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,96
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lap_ei_append_owned:
 push rbx
 mov rbx,rsi
 call json_object_array_add@PLT
 test eax,eax
 jz .Lap_ei_append_end
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,1
.Lap_ei_append_end:
 pop rbx
 ret
.section .rodata
.Lap_toolName: .asciz "toolName"
.Lap_image_type: .asciz "image"
.Lap_empty_name: .asciz ""
.Lap_caption_format: .asciz "Images from the error result of tool call %s (%s):"
.Lap_placeholder_json: .asciz "{\"type\":\"text\",\"text\":\"(image attached after the tool results)\"}"

/* Legacy Messages schemas take properties/required; eager streaming defaults
   on, unless the target's compatibility descriptor disables it. */
.text
.Lap_tool_schema:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rdx
 call pg_mimo_config_google
 test eax,eax
 jnz .Lap_schema_preserve
 mov rdi,r13
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jnz .Lap_schema_preserve
 APGET rbx,strict
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lap_schema_legacy
.Lap_schema_preserve:
 mov rdi,r12
 call json_object_get@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lap_input_schema]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_schema_bad
 mov rdi,r13
 call pg_mimo_config_google
 test eax,eax
 jnz .Lap_schema_ok
 mov rdi,r13
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jnz .Lap_schema_ok
 APGET r12,properties
 test rax,rax
 jnz .Lap_schema_compat
 call json_object_new_object@PLT
 mov rdi,r12
 lea rsi,[rip+.Lap_properties]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_schema_bad
 jmp .Lap_schema_compat
.Lap_schema_legacy:
 lea rdi,[rip+.Lap_legacy_schema]
 call json_tokener_parse@PLT
 mov r14,rax
 test rax,rax
 jz .Lap_schema_bad
 mov rdi,rbx
 lea rsi,[rip+.Lap_input_schema]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_schema_bad
 mov rdi,r14
 mov rsi,r12
 lea rdx,[rip+.Lap_properties]
 mov rcx,rdx
 call .Lrp_copy
 test eax,eax
 jnz .Lap_schema_bad
 mov rdi,r14
 mov rsi,r12
 lea rdx,[rip+.Lap_required]
 mov rcx,rdx
 call .Lrp_copy
 test eax,eax
 jnz .Lap_schema_bad
 .Lap_schema_compat:
 APGET r13,modelInfo
 APGET rax,compat
 APGET rax,supportsEagerToolInputStreaming
 test rax,rax
 jz .Lap_schema_eager
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lap_schema_ok
.Lap_schema_eager:
 mov edi,1
 call json_object_new_boolean@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lap_eager_input_streaming]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_schema_bad
.Lap_schema_ok:
 xor eax,eax
 jmp .Lap_schema_end
.Lap_schema_bad:
 mov eax,1
.Lap_schema_end:
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lap_supportsTemperature: .asciz "supportsTemperature"
.Lap_supportsEagerToolInputStreaming: .asciz "supportsEagerToolInputStreaming"
.Lap_eager_input_streaming: .asciz "eager_input_streaming"
.Lap_properties: .asciz "properties"
.Lap_required: .asciz "required"
.Lap_legacy_schema: .asciz "{\"type\":\"object\",\"properties\":{},\"required\":[]}"

/* Pi OpenAI prompt cache: the identity is PanGu's committed sessionId;
   clamp by Unicode characters rather than UTF-8 bytes. */
.text
.Loc_cache:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov rdi,r12
 call pg_mimo_uses_anthropic
 test eax,eax
 jnz .Loc_ok
 mov rdi,r12
 call pg_mimo_uses_google
 test eax,eax
 jnz .Loc_ok
 mov rdi,[r12+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jnz .Loc_ok
 mov rdi,r12
 call pg_mimo_uses_responses
 mov [rsp+24],eax
 mov r12,[r12+PG_MM_CONFIGURATION]
 APGET r12,modelInfo
 APGET rax,compat
 mov r13,rax
 APGET r12,options
 mov r14,rax
 APGET rax,cacheRetention
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jnz .Loc_retention
 mov rdi,r12
 call pg_mimo_config_codex@PLT
 test eax,eax
 jnz .Loc_retention_ready
 lea rdi,[rip+.Lap_cache_env]
 call getenv@PLT
 test rax,rax
 jz .Loc_retention_ready
.Loc_retention:
 mov [rsp+32],rax
 mov rdi,rax
 lea rsi,[rip+.Lap_none]
 call strcmp@PLT
 test eax,eax
 jnz .Loc_check_long
 mov qword ptr [rsp],1
 jmp .Loc_retention_ready
.Loc_check_long:
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lap_long]
 call strcmp@PLT
 test eax,eax
 jnz .Loc_retention_ready
 mov qword ptr [rsp],2
.Loc_retention_ready:
 mov ebp,[rsp+24]
 test ebp,ebp
 jnz .Loc_response_long
 APGET r12,endpoint
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Loc_chat_long
 mov rdi,rax
 lea rsi,[rip+.Loc_openai_domain]
 call strstr@PLT
 test rax,rax
 jz .Loc_chat_long
 mov qword ptr [rsp+8],1
.Loc_chat_long:
 APGET r13,supportsLongCacheRetention
 test rax,rax
 jz .Loc_long_ready
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov ebp,eax
 jmp .Loc_long_ready
.Loc_response_long:
 mov qword ptr [rsp+8],1
 mov ebp,1
 APGET r13,supportsLongCacheRetention
 test rax,rax
 jz .Loc_long_ready
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov ebp,eax
.Loc_long_ready:
 mov rdi,r12
 call pg_mimo_config_codex@PLT
 test eax,eax
 jnz .Loc_key
 cmp qword ptr [rsp],2
 jne .Loc_key
 test ebp,ebp
 jz .Loc_key
 mov qword ptr [rsp+8],1
 APGET r13,supportsExplicitPromptCacheMode
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Loc_long_retention
 cmp dword ptr [rsp+24],0
 je .Loc_long_retention
 lea rdi,[rip+.Loc_ttl_json]
 call json_tokener_parse@PLT
 mov rdi,rbx
 lea rsi,[rip+.Loc_prompt_cache_options]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Loc_bad
 jmp .Loc_key
.Loc_long_retention:
 mov rdi,rbx
 lea rsi,[rip+.Loc_prompt_cache_retention]
 lea rdx,[rip+.Loc_24h]
 call li_field
 test eax,eax
 jnz .Loc_bad
.Loc_key:
 cmp qword ptr [rsp],1
 jne .Loc_key_enabled
 mov rdi,r12
 call pg_mimo_config_codex@PLT
 test eax,eax
 jnz .Loc_ok
 cmp dword ptr [rsp+24],0
 je .Loc_ok
 APGET r13,supportsExplicitPromptCacheMode
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Loc_ok
 lea rdi,[rip+.Loc_explicit_json]
 call json_tokener_parse@PLT
 mov rdi,rbx
 lea rsi,[rip+.Loc_prompt_cache_options]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Loc_bad
 jmp .Loc_ok
.Loc_key_enabled:
 cmp qword ptr [rsp+8],0
 je .Loc_ok
 APGET r14,sessionId
 test rax,rax
 jnz .Loc_identity
 APGET r12,sessionId
.Loc_identity:
 test rax,rax
 jz .Loc_ok
 mov r14,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Loc_bad
 mov rdi,r14
 call json_object_get_string_len@PLT
 mov [rsp+40],eax
 mov rdi,r14
 call json_object_get_string@PLT
 mov rdi,rax
 xor ecx,ecx
 xor esi,esi
.Loc_char:
 cmp ecx,[rsp+40]
 jae .Loc_publish
 movzx edx,byte ptr [rdi+rcx]
 and edx,192
 cmp edx,128
 je .Loc_char_next
 cmp esi,64
 jae .Loc_publish
 inc esi
.Loc_char_next:
 inc ecx
 jmp .Loc_char
.Loc_publish:
 mov esi,ecx
 call json_object_new_string_len@PLT
 mov rdi,rbx
 lea rsi,[rip+.Loc_prompt_cache_key]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Loc_bad
.Loc_ok:
 xor ebp,ebp
 jmp .Loc_end
.Loc_bad:
 mov ebp,1
.Loc_end:
 mov eax,ebp
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Loc_openai_domain: .asciz "api.openai.com"
.Loc_prompt_cache_key: .asciz "prompt_cache_key"
.Loc_prompt_cache_retention: .asciz "prompt_cache_retention"
.Loc_prompt_cache_options: .asciz "prompt_cache_options"
.Loc_24h: .asciz "24h"
.Loc_ttl_json: .asciz "{\"ttl\":\"30m\"}"
.Loc_explicit_json: .asciz "{\"mode\":\"explicit\"}"
.Lap_sessionId: .asciz "sessionId"
.Lap_supportsExplicitPromptCacheMode: .asciz "supportsExplicitPromptCacheMode"
.section .rodata
.Lap_endpoint: .asciz "endpoint"

/* Ordinary sampling preserves prefer fallback; required constrained sampling
   must be fulfilled before transmission. */
.text
.Lsampling_admit:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov [rsp],rsi
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+32],0
 APGET rdi,tools
 mov rbx,rax
 xor r12d,r12d
.Lsampling_each:
 test rbx,rbx
 jz .Lsampling_ok
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lsampling_ok
 mov rdi,rbx
 mov rsi,r12
 call json_object_array_get_idx@PLT
 APGET rax,function
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Lsampling_key]
 call json_object_object_get@PLT
 mov r14,rax
 test rax,rax
 jz .Lsampling_next
 mov rdi,rax
 mov esi,1
 call json_object_is_type@PLT
 test eax,eax
 jz .Lsampling_object
 mov rdi,r14
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lsampling_bad
 jmp .Lsampling_remove
.Lsampling_object:
 mov rdi,r14
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lsampling_bad
 APGET r14,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lsampling_bad
 mov rdi,rax
 lea rsi,[rip+.Lsampling_json_schema]
 call strcmp@PLT
 test eax,eax
 jnz .Lsampling_remove
 mov rdi,r14
 lea rsi,[rip+.Lsampling_strict]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lsampling_bad
 mov [rsp+24],rax
 mov rdi,rax
 lea rsi,[rip+.Lsampling_require]
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 mov [rsp+40],eax
 test eax,eax
 jnz .Lsampling_policy
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lsampling_prefer]
 call strcmp@PLT
 test eax,eax
 jnz .Lsampling_bad
.Lsampling_policy:
 mov rdi,[rsp]
 call pg_mimo_config_anthropic
 mov [rsp+16],eax
 mov rdi,[rsp]
 call pg_mimo_config_google
 test eax,eax
 jz .Lsampling_non_google_support
 APGET "qword ptr [rsp]",model
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 test rax,rax
 jz .Lsampling_unsupported
 mov rdi,rax
 lea rsi,[rip+.Lg_gemini_prefix]
 mov edx,7
 call strncasecmp@PLT
 test eax,eax
 jnz .Lsampling_unsupported
 mov rdi,r14
 call pg_google_requires_id
 jmp .Lsampling_support_value
.Lsampling_non_google_support:
 APGET "qword ptr [rsp]",modelInfo
 APGET rax,compat
 mov rdi,rax
 lea rsi,[rip+.Lsampling_supportsMode]
 cmp dword ptr [rsp+16],0
 je .Lsampling_support_key
 lea rsi,[rip+.Lsampling_supportsTools]
.Lsampling_support_key:
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
.Lsampling_support_value:
 test eax,eax
 jz .Lsampling_unsupported
 APGET r13,parameters
 mov rdi,rax
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lsampling_bad
 mov rdi,[rsp+8]
 mov esi,[rsp+16]
 xor edx,edx
 call .Lst_node
 cmp eax,2
 je .Lsampling_bad
 test eax,eax
 jnz .Lsampling_clone_unsupported
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lst_type]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lsampling_clone_unsupported
 mov rdi,rax
 lea rsi,[rip+.Lst_object_type]
 call strcmp@PLT
 test eax,eax
 jnz .Lsampling_clone_unsupported
 mov rdi,r13
 lea rsi,[rip+.Lap_parameters]
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 call .Lrp_set
 test eax,eax
 jnz .Lsampling_bad
 mov edi,1
 call json_object_new_boolean@PLT
 mov rdi,r13
 lea rsi,[rip+.Lsampling_strict]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lsampling_bad
 jmp .Lsampling_remove
.Lsampling_clone_unsupported:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov qword ptr [rsp+8],0
.Lsampling_unsupported:
 cmp dword ptr [rsp+40],0
 jne .Lsampling_bad
.Lsampling_remove:
 mov rdi,r13
 lea rsi,[rip+.Lsampling_key]
 call json_object_object_del@PLT
.Lsampling_next:
 inc r12
 jmp .Lsampling_each
.Lsampling_ok:
 xor ebp,ebp
 jmp .Lsampling_end
.Lsampling_bad:
 mov ebp,1
.Lsampling_end:
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
.section .rodata
.Lsampling_key: .asciz "constrainedSampling"
.Lsampling_strict: .asciz "strict"
.Lsampling_require: .asciz "require"
.Lsampling_json_schema: .asciz "json_schema"

.section .rodata
.Lsampling_prefer: .asciz "prefer"

.macro SNGET obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lst_\key]
 call json_object_object_get@PLT
.endm
/* Mutate a detached schema. 0 success, 1 unsupported schema, 2 native failure. */
.text
.Lst_node:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,96
 mov rbx,rdi
 mov r12d,esi
 mov r13d,edx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 cmp edx,128
 jae .Lst_native_bad
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lst_unsupported
 lea r14,[rip+.Lst_unsupported_keys]
 xor ebp,ebp
.Lst_check_key:
 mov rsi,[r14+rbp*8]
 test rsi,rsi
 jz .Lst_provider_keys
 mov rdi,rbx
 lea rdx,[rsp+80]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Lst_unsupported
 inc ebp
 jmp .Lst_check_key
.Lst_provider_keys:
 test r12d,r12d
 jz .Lst_union
 lea r14,[rip+.Lst_anthropic_keys]
 xor ebp,ebp
.Lst_anthropic_key:
 mov rsi,[r14+rbp*8]
 test rsi,rsi
 jz .Lst_anthropic_min
 mov rdi,rbx
 lea rdx,[rsp+80]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Lst_unsupported
 inc ebp
 jmp .Lst_anthropic_key
.Lst_anthropic_min:
 mov rdi,rbx
 lea rsi,[rip+.Lst_minItems]
 lea rdx,[rsp+80]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lst_anthropic_format
 mov rdi,[rsp+80]
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jnz .Lst_min_number
 mov rdi,[rsp+80]
 mov esi,2
 call json_object_is_type@PLT
 test eax,eax
 jz .Lst_unsupported
.Lst_min_number:
 mov rdi,[rsp+80]
 call json_object_get_double@PLT
 pxor xmm1,xmm1
 ucomisd xmm0,xmm1
 jp .Lst_unsupported
 je .Lst_anthropic_format
 ucomisd xmm0,qword ptr [rip+.Lst_one]
 jne .Lst_unsupported
.Lst_anthropic_format:
 mov rdi,rbx
 lea rsi,[rip+.Lst_format]
 lea rdx,[rsp+80]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lst_union
 mov rdi,[rsp+80]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lst_unsupported
 mov rdi,[rsp+80]
 call json_object_get_string@PLT
 mov [rsp+88],rax
 lea r14,[rip+.Lst_formats]
 xor ebp,ebp
.Lst_format_each:
 mov rsi,[r14+rbp*8]
 test rsi,rsi
 jz .Lst_unsupported
 mov rdi,[rsp+88]
 call strcmp@PLT
 test eax,eax
 jz .Lst_union
 inc ebp
 jmp .Lst_format_each
.Lst_union:
 mov rdi,rbx
 lea rsi,[rip+.Lst_anyOf]
 lea rdx,[rsp+24]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lst_items
 mov rdi,[rsp+24]
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lst_unsupported
 mov rdi,[rsp+24]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lst_unsupported
 xor r14d,r14d
.Lst_variant:
 mov rdi,[rsp+24]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lst_items
 mov rdi,[rsp+24]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbp,rax
 mov rdi,rax
 call .Lst_structured
 test eax,eax
 jnz .Lst_unsupported
 mov rdi,rbp
 mov esi,r12d
 lea edx,[r13+1]
 call .Lst_node
 test eax,eax
 jnz .Lst_child_bad
 inc r14
 jmp .Lst_variant
.Lst_items:
 mov rdi,rbx
 lea rsi,[rip+.Lst_items_key]
 lea rdx,[rsp+24]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lst_object
 mov rdi,[rsp+24]
 mov esi,r12d
 lea edx,[r13+1]
 call .Lst_node
 test eax,eax
 jnz .Lst_child_bad
.Lst_object:
 SNGET rbx,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lst_not_object
 mov rdi,rax
 lea rsi,[rip+.Lst_object_type]
 call strcmp@PLT
 test eax,eax
 jz .Lst_object_rules
.Lst_not_object:
 mov rdi,rbx
 lea rsi,[rip+.Lst_properties]
 lea rdx,[rsp+80]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Lst_unsupported
 jmp .Lst_success
.Lst_object_rules:
 mov rdi,rbx
 lea rsi,[rip+.Lst_additionalProperties]
 lea rdx,[rsp+80]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lst_properties_begin
 mov rdi,[rsp+80]
 mov esi,1
 call json_object_is_type@PLT
 test eax,eax
 jz .Lst_unsupported
 mov rdi,[rsp+80]
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lst_unsupported
.Lst_properties_begin:
 mov rdi,rbx
 lea rsi,[rip+.Lst_properties]
 lea rdx,[rsp+32]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lst_properties_absent
 mov rdi,[rsp+32]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lst_unsupported
 jmp .Lst_required_begin
.Lst_properties_absent:
 mov qword ptr [rsp+32],0
.Lst_required_begin:
 call json_object_new_object@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lst_native_bad
 mov rdi,rbx
 lea rsi,[rip+.Lst_required]
 lea rdx,[rsp+40]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lst_required_absent
 mov rdi,[rsp+40]
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lst_unsupported
 xor r14d,r14d
.Lst_required_validate:
 mov rdi,[rsp+40]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lst_iter_begin
 mov rdi,[rsp+40]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbp,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lst_unsupported
 mov rdi,rbp
 call json_object_get_string@PLT
 mov [rsp+88],rax
 cmp qword ptr [rsp+32],0
 je .Lst_unsupported
 mov rdi,[rsp+32]
 mov rsi,rax
 lea rdx,[rsp+80]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lst_unsupported
 mov rdi,rbp
 call json_object_get@PLT
 mov rdi,[rsp+16]
 mov rsi,[rsp+88]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lst_native_bad
 inc r14
 jmp .Lst_required_validate
.Lst_required_absent:
 mov qword ptr [rsp+40],0
.Lst_iter_begin:
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lst_native_bad
 cmp qword ptr [rsp+32],0
 je .Lst_publish
 mov rdi,[rsp+32]
 call json_object_iter_begin@PLT
 mov [rsp+48],rax
 mov rdi,[rsp+32]
 call json_object_iter_end@PLT
 mov [rsp+56],rax
.Lst_property:
 lea rdi,[rsp+48]
 lea rsi,[rsp+56]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lst_publish
 lea rdi,[rsp+48]
 call json_object_iter_peek_name@PLT
 mov [rsp+64],rax
 lea rdi,[rsp+48]
 call json_object_iter_peek_value@PLT
 mov r14,rax
 mov rdi,rax
 mov esi,r12d
 lea edx,[r13+1]
 call .Lst_node
 test eax,eax
 jnz .Lst_child_bad
 mov rdi,[rsp+16]
 mov rsi,[rsp+64]
 lea rdx,[rsp+80]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Lst_property_name
.Lst_optional:
 mov rdi,r14
 mov esi,r13d
 call .Lst_allows_null
 test eax,eax
 jnz .Lst_property_name
 lea rdi,[rip+.Lst_optional_json]
 call json_tokener_parse@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lst_native_bad
 SNGET rax,anyOf
 mov rbp,rax
 mov rdi,r14
 call json_object_get@PLT
 mov rdi,rbp
 xor esi,esi
 mov rdx,rax
 call json_object_array_put_idx@PLT
 test eax,eax
 jz .Lst_optional_adopt
 mov rdi,r14
 call json_object_put@PLT
 jmp .Lst_native_bad
.Lst_optional_adopt:
 mov rdi,[rsp+32]
 mov rsi,[rsp+64]
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 call .Lrp_set
 test eax,eax
 jnz .Lst_native_bad
.Lst_property_name:
 mov rdi,[rsp+64]
 call json_object_new_string@PLT
 test rax,rax
 jz .Lst_native_bad
 mov rdi,[rsp]
 mov rsi,rax
 call .Lap_ei_append_owned
 test eax,eax
 jnz .Lst_native_bad
 lea rdi,[rsp+48]
 call json_object_iter_next@PLT
 jmp .Lst_property
.Lst_publish:
 mov rdi,rbx
 lea rsi,[rip+.Lst_required]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lst_native_bad
 xor edi,edi
 call json_object_new_boolean@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lst_additionalProperties]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lst_native_bad
.Lst_success:
 xor ebp,ebp
 jmp .Lst_end
.Lst_child_bad:
 mov ebp,eax
 jmp .Lst_end
.Lst_unsupported:
 mov ebp,1
 jmp .Lst_end
.Lst_native_bad:
 mov ebp,2
.Lst_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,96
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Structured anyOf branches are outside Pi's strict schema subset. */
.Lst_structured:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov rdi,rbx
 lea rsi,[rip+.Lst_properties]
 sub rsp,16
 lea rdx,[rsp]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Lst_structured_yes
 mov rdi,rbx
 lea rsi,[rip+.Lst_items_key]
 lea rdx,[rsp]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Lst_structured_yes
 SNGET rbx,type
 mov r12,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jnz .Lst_structured_array
 mov rdi,r12
 call json_object_get_string@PLT
 mov rdi,rax
 call .Lst_structured_name
 jmp .Lst_structured_end
.Lst_structured_array:
 xor r13d,r13d
.Lst_structured_each:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lst_structured_no
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 call .Lst_structured_name
 test eax,eax
 jnz .Lst_structured_yes
 inc r13
 jmp .Lst_structured_each
.Lst_structured_yes:
 mov eax,1
 jmp .Lst_structured_end
.Lst_structured_no:
 xor eax,eax
.Lst_structured_end:
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret
.Lst_structured_name:
 test rdi,rdi
 jz .Lst_name_no
 push rbx
 mov rbx,rdi
 lea rsi,[rip+.Lst_object_type]
 call strcmp@PLT
 test eax,eax
 jz .Lst_name_yes
 mov rdi,rbx
 lea rsi,[rip+.Lst_array_type]
 call strcmp@PLT
 test eax,eax
 setz al
 movzx eax,al
 pop rbx
 ret
.Lst_name_yes:
 mov eax,1
 pop rbx
 ret
.Lst_name_no:
 xor eax,eax
 ret
/* schemaAllowsNull: explicit type, const/enum null or a nullable anyOf. */
.Lst_allows_null:
 push rbx
 push r12
 push r13
 sub rsp,16
 mov rbx,rdi
 mov r13d,esi
 cmp esi,128
 jae .Lst_null_no
 SNGET rbx,type
 mov r12,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lst_null_type
 xor r13d,r13d
.Lst_null_types:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lst_null_values
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lst_null_types_next
 mov rdi,rax
 lea rsi,[rip+.Lst_null_type_name]
 call strcmp@PLT
 test eax,eax
 jz .Lst_null_yes
.Lst_null_types_next:
 inc r13
 jmp .Lst_null_types
.Lst_null_type:
 mov rdi,r12
 call json_object_get_string@PLT
 test rax,rax
 jz .Lst_null_values
 mov rdi,rax
 lea rsi,[rip+.Lst_null_type_name]
 call strcmp@PLT
 test eax,eax
 jz .Lst_null_yes
.Lst_null_values:
 mov rdi,rbx
 lea rsi,[rip+.Lst_const]
 lea rdx,[rsp]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lst_null_enum
 cmp qword ptr [rsp],0
 je .Lst_null_yes
.Lst_null_enum:
 SNGET rbx,enum
 mov r12,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lst_null_union
 xor r13d,r13d
.Lst_null_enum_each:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lst_null_union
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 test rax,rax
 jz .Lst_null_yes
 inc r13
 jmp .Lst_null_enum_each
.Lst_null_union:
 SNGET rbx,anyOf
 mov r12,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lst_null_no
 xor r13d,r13d
.Lst_null_union_each:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lst_null_no
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov esi,1
 call .Lst_allows_null
 test eax,eax
 jnz .Lst_null_yes
 inc r13
 jmp .Lst_null_union_each
.Lst_null_yes:
 mov eax,1
 jmp .Lst_null_end
.Lst_null_no:
 xor eax,eax
.Lst_null_end:
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret

.section .rodata
.Lap_strict: .asciz "strict"
.Lsampling_supportsMode: .asciz "supportsStrictMode"
.Lsampling_supportsTools: .asciz "supportsStrictTools"
.Lst_badkey_0: .asciz "$ref"
.Lst_badkey_1: .asciz "$defs"
.Lst_badkey_2: .asciz "definitions"
.Lst_badkey_3: .asciz "allOf"
.Lst_badkey_4: .asciz "oneOf"
.Lst_badkey_5: .asciz "patternProperties"
.Lst_badkey_6: .asciz "dependentSchemas"
.Lst_badkey_7: .asciz "dependencies"
.Lst_badkey_8: .asciz "unevaluatedProperties"
.Lst_badkey_9: .asciz "propertyNames"
.Lst_badkey_10: .asciz "contains"
.Lst_badkey_11: .asciz "prefixItems"
.Lst_badkey_12: .asciz "not"
.Lst_badkey_13: .asciz "if"
.Lst_badkey_14: .asciz "then"
.Lst_badkey_15: .asciz "else"
.Lst_anthrokey_0: .asciz "minimum"
.Lst_anthrokey_1: .asciz "maximum"
.Lst_anthrokey_2: .asciz "exclusiveMinimum"
.Lst_anthrokey_3: .asciz "exclusiveMaximum"
.Lst_anthrokey_4: .asciz "multipleOf"
.Lst_anthrokey_5: .asciz "maxItems"
.Lst_anthrokey_6: .asciz "uniqueItems"
.Lst_anthrokey_7: .asciz "minContains"
.Lst_anthrokey_8: .asciz "maxContains"
.Lst_anthrokey_9: .asciz "minProperties"
.Lst_anthrokey_10: .asciz "maxProperties"
.Lst_format_0: .asciz "date-time"
.Lst_format_1: .asciz "time"
.Lst_format_2: .asciz "date"
.Lst_format_3: .asciz "duration"
.Lst_format_4: .asciz "email"
.Lst_format_5: .asciz "hostname"
.Lst_format_6: .asciz "uri"
.Lst_format_7: .asciz "ipv4"
.Lst_format_8: .asciz "ipv6"
.Lst_format_9: .asciz "uuid"
.Lst_type: .asciz "type"
.Lst_anyOf: .asciz "anyOf"
.Lst_items_key: .asciz "items"
.Lst_properties: .asciz "properties"
.Lst_required: .asciz "required"
.Lst_additionalProperties: .asciz "additionalProperties"
.Lst_object_type: .asciz "object"
.Lst_array_type: .asciz "array"
.Lst_const: .asciz "const"
.Lst_enum: .asciz "enum"
.Lst_null_type_name: .asciz "null"
.Lst_minItems: .asciz "minItems"
.Lst_format: .asciz "format"
.Lst_optional_json: .asciz "{\"anyOf\":[null,{\"type\":\"null\"}]}"
.section .data.rel.ro
.Lst_unsupported_keys: .quad .Lst_badkey_0,.Lst_badkey_1,.Lst_badkey_2,.Lst_badkey_3,.Lst_badkey_4,.Lst_badkey_5,.Lst_badkey_6,.Lst_badkey_7,.Lst_badkey_8,.Lst_badkey_9,.Lst_badkey_10,.Lst_badkey_11,.Lst_badkey_12,.Lst_badkey_13,.Lst_badkey_14,.Lst_badkey_15,0
.Lst_anthropic_keys: .quad .Lst_anthrokey_0,.Lst_anthrokey_1,.Lst_anthrokey_2,.Lst_anthrokey_3,.Lst_anthrokey_4,.Lst_anthrokey_5,.Lst_anthrokey_6,.Lst_anthrokey_7,.Lst_anthrokey_8,.Lst_anthrokey_9,.Lst_anthrokey_10,0
.Lst_formats: .quad .Lst_format_0,.Lst_format_1,.Lst_format_2,.Lst_format_3,.Lst_format_4,.Lst_format_5,.Lst_format_6,.Lst_format_7,.Lst_format_8,.Lst_format_9,0

.section .rodata
.balign 8
.Lst_one: .double 1.0

/* Anthropic retries one rejected strict-prefer request, never a started stream.
   Required tools keep their converted schemas. The registry is phase-borrowed. */
.globl pg_mimo_anthropic_strict_retry
.type pg_mimo_anthropic_strict_retry,@function
.text
pg_mimo_anthropic_strict_retry:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,80
 mov rbx,rdi
 mov rdi,rsp
 xor eax,eax
 mov ecx,10
 rep stosq
 cmp qword ptr [rbx+PG_MM_HTTP],400
 jne .Lsr_bad
 cmp dword ptr [rbx+PG_MM_STRICT_RETRY],0
 jne .Lsr_bad
 cmp qword ptr [rbx+PG_MM_RESPONSE_ID],0
 jne .Lsr_bad
 mov rdi,rbx
 call pg_mimo_uses_anthropic
 test eax,eax
 jz .Lsr_bad
 lea rdi,[rbx+PG_MM_BODY]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lsr_bad
 APGET rax,error
 mov [rsp+24],rax
 APGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lsr_bad
 mov rdi,rax
 lea rsi,[rip+.Lsr_invalid_request]
 call strcmp@PLT
 test eax,eax
 jnz .Lsr_bad
 APGET "qword ptr [rsp+24]",message
 mov [rsp+32],rax
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 test rax,rax
 jz .Lsr_bad
 xor r12d,r12d
 lea r13,[rip+.Lsr_limits]
.Lsr_match:
 mov rsi,[r13+r12*8]
 test rsi,rsi
 jz .Lsr_bad
 mov rdi,r14
 call strstr@PLT
 test rax,rax
 jnz .Lsr_request
 inc r12
 jmp .Lsr_match
.Lsr_request:
 mov rdi,[rbx+PG_MM_REQUEST]
 call json_tokener_parse@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lsr_bad
 APGET rax,tools
 mov r12,rax
 test rax,rax
 jz .Lsr_bad
 xor r13d,r13d
.Lsr_tool:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lsr_publish
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 APGET rax,strict
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lsr_next
 APGET r14,name
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+40],rax
 mov qword ptr [rsp+48],0
.Lsr_find:
 mov rbp,[rbx+PG_MM_TOOLS]
 test rbp,rbp
 jz .Lsr_bad
 mov rax,[rsp+48]
 cmp rax,[rbp+PG_MM_REG_COUNT]
 jae .Lsr_next
 imul rax,rax,PG_MM_DEF_BYTES
 add rbp,rax
 mov rdi,[rbp+PG_MM_DEF_NAME]
 mov rsi,[rsp+40]
 call strcmp@PLT
 test eax,eax
 jz .Lsr_definition
 inc qword ptr [rsp+48]
 jmp .Lsr_find
.Lsr_definition:
 mov rax,[rbp+72]
 test rax,rax
 jz .Lsr_next
 mov rdi,[rax+32]
 test rdi,rdi
 jz .Lsr_next
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lsr_bad
 APGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lsr_bad
 mov rdi,rax
 lea rsi,[rip+.Lsampling_json_schema]
 call strcmp@PLT
 test eax,eax
 jnz .Lsr_release_next
 APGET "qword ptr [rsp+16]",strict
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lsr_bad
 mov rdi,rax
 lea rsi,[rip+.Lsampling_prefer]
 call strcmp@PLT
 test eax,eax
 jnz .Lsr_release_next
 mov rdi,[rbp+PG_MM_DEF_SCHEMA]
 call json_tokener_parse@PLT
 mov [rsp+56],rax
 test rax,rax
 jz .Lsr_bad
 mov rdi,r14
 lea rsi,[rip+.Lap_strict]
 call json_object_object_del@PLT
 mov rdi,r14
 mov rsi,[rsp+56]
 mov rdx,[rbx+PG_MM_CONFIGURATION]
 call .Lap_tool_schema
 test eax,eax
 jnz .Lsr_bad
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov qword ptr [rsp+56],0
 inc qword ptr [rsp+64]
.Lsr_release_next:
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov qword ptr [rsp+16],0
.Lsr_next:
 inc r13
 jmp .Lsr_tool
.Lsr_publish:
 cmp qword ptr [rsp+64],0
 je .Lsr_bad
 mov rdi,[rsp+8]
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Lsr_bad
 mov rdi,rax
 call strdup@PLT
 mov [rsp+72],rax
 test rax,rax
 jz .Lsr_bad
 lea rdi,[rip+.Lsr_diagnostic]
 call json_tokener_parse@PLT
 mov r12,rax
 test rax,rax
 jz .Lsr_bad
 mov [rbx+PG_MM_STRICT_DIAGNOSTICS],rax
 mov rdi,r12
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov r14,rax
 # Timestamp storage reuses two scratch counters after tool traversal.
 lea rdi,[rsp+40]
 xor esi,esi
 call gettimeofday@PLT
 test eax,eax
 jnz .Lsr_bad
 mov rax,[rsp+48]
 xor edx,edx
 mov ecx,1000
 div rcx
 imul rdi,[rsp+40],1000
 add rdi,rax
 call json_object_new_int64@PLT
 mov rdi,r14
 lea rsi,[rip+.Lsr_timestamp]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lsr_bad
 APGET r14,error
 mov rdi,rax
 mov rsi,[rsp+24]
 lea rdx,[rip+.Lap_message]
 mov rcx,rdx
 call .Lrp_copy
 test eax,eax
 jnz .Lsr_bad
 mov rdi,[rbx+PG_MM_REQUEST]
 call free@PLT
 mov rax,[rsp+72]
 mov [rbx+PG_MM_REQUEST],rax
 mov qword ptr [rsp+72],0
 mov dword ptr [rbx+PG_MM_STRICT_RETRY],1
 xor ebp,ebp
 jmp .Lsr_end
.Lsr_bad:
 mov ebp,1
.Lsr_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov rdi,[rsp+72]
 call free@PLT
 mov eax,ebp
 add rsp,80
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_mimo_anthropic_strict_retry,.-pg_mimo_anthropic_strict_retry
.section .rodata
.Lap_error: .asciz "error"
.Lap_message: .asciz "message"
.Lsr_invalid_request: .asciz "invalid_request_error"
.Lsr_limit_grammar: .asciz "The compiled grammar is too large"
.Lsr_limit_count: .asciz "Too many strict tools ("
.Lsr_limit_unions: .asciz "Schemas contains too many parameters with union types ("
.Lsr_diagnostic: .asciz "[{\"type\":\"anthropic_strict_tools_fallback\",\"error\":{},\"details\":{\"reason\":\"strict_tool_limits_exceeded\"}}]"
.section .data.rel.ro
.Lsr_limits: .quad .Lsr_limit_grammar,.Lsr_limit_count,.Lsr_limit_unions,0

.section .rodata
.Lsr_timestamp: .asciz "timestamp"
.text
.globl pg_mimo_uses_google
.type pg_mimo_uses_google,@function
pg_mimo_uses_google:
 mov rdi,[rdi+PG_MM_CONFIGURATION]
 jmp pg_mimo_config_google
.size pg_mimo_uses_google,.-pg_mimo_uses_google

.macro GG obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lg_\key]
 call json_object_object_get@PLT
.endm
.macro GC dst,src,from,to
 mov rdi,\dst
 mov rsi,\src
 lea rdx,[rip+.Lg_\from]
 lea rcx,[rip+.Lg_\to]
 call .Lrp_copy
 test eax,eax
 jnz .Lg_bad
.endm
/* Shared Content/Part wire layout for Generative Language and Vertex. */
.text
.Lg_request:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+48],0
 GG rbx,messages
 mov r13,rax
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lg_bad
 xor r14d,r14d
.Lg_message:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lg_system_begin
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbp,rax
 lea rdi,[rip+.Lg_content_json]
 mov rsi,[rsp]
 call .Lrp_append
 mov [rsp+24],rax
 test rax,rax
 jz .Lg_bad
 GC rax,rbp,role,role
 GG rbp,role
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lg_assistant]
 call strcmp@PLT
 test eax,eax
 jnz .Lg_message_parts
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lg_role]
 lea rdx,[rip+.Lg_model]
 call li_field
 test eax,eax
 jnz .Lg_bad
.Lg_message_parts:
 GG rbp,content
 mov rdi,rax
 mov rsi,r12
 lea rdx,[rsp+8]
 call .Lg_parts
 test eax,eax
 jnz .Lg_bad
 mov rdi,[rsp+8]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lg_empty_message
 mov rdi,[rsp]
 mov rsi,[rsp+24]
 mov rdx,[rsp+8]
 call .Lg_split_parts
 test eax,eax
 jnz .Lg_bad
 jmp .Lg_message_parts_done
.Lg_empty_message:
 mov rdi,[rsp]
 call json_object_array_length@PLT
 lea rsi,[rax-1]
 mov rdi,[rsp]
 mov edx,1
 call json_object_array_del_idx@PLT
 test eax,eax
 jnz .Lg_bad
.Lg_message_parts_done:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov qword ptr [rsp+8],0
 inc r14
 jmp .Lg_message
.Lg_system_begin:
 mov rdi,rbx
 lea rsi,[rip+.Lg_contents]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lg_bad
 GG rbx,system
 test rax,rax
 jz .Lg_config
 mov rdi,rax
 mov rsi,r12
 lea rdx,[rsp+8]
 call .Lg_parts
 test eax,eax
 jnz .Lg_bad
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lg_bad
 mov rdi,rax
 lea rsi,[rip+.Lg_parts_key]
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 call .Lrp_set
 test eax,eax
 jnz .Lg_bad
 mov rdi,rbx
 lea rsi,[rip+.Lg_systemInstruction]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lg_bad
.Lg_config:
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lg_bad
 GC rax,rbx,max_tokens,maxOutputTokens
 GC "qword ptr [rsp]",rbx,temperature,temperature
 mov rdi,rbx
 lea rsi,[rip+.Lg_generationConfig]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lg_bad
 GG rbx,tools
 mov r13,rax
 test rax,rax
 jz .Lg_clean
 lea rdi,[rip+.Lg_tools_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lg_bad
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 GG rax,functionDeclarations
 mov rbp,rax
 xor r14d,r14d
.Lg_tool:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lg_tools_adopt
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov [rsp+24],rax
 lea rdi,[rip+.Lap_empty_json]
 mov rsi,rbp
 call .Lrp_append
 mov [rsp+32],rax
 test rax,rax
 jz .Lg_bad
 GC rax,"qword ptr [rsp+24]",name,name
 GC "qword ptr [rsp+32]","qword ptr [rsp+24]",description,description
 GC "qword ptr [rsp+32]","qword ptr [rsp+24]",input_schema,parametersJsonSchema
 GG "qword ptr [rsp+32]",parametersJsonSchema
 mov rdi,rax
 xor esi,esi
 xor edx,edx
 call .Lg_const_enum
 test eax,eax
 jnz .Lg_bad
 GG "qword ptr [rsp+24]",strict
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lg_tool_next
 mov qword ptr [rsp+48],1
.Lg_tool_next:
 inc r14
 jmp .Lg_tool
.Lg_tools_adopt:
 mov rdi,rbx
 lea rsi,[rip+.Lg_tools]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lg_bad
 mov rdi,rbx
 mov rsi,r12
 mov edx,[rsp+48]
 call .Lg_tool_mode
 test eax,eax
 jnz .Lg_bad
.Lg_clean:
 mov rdi,rbx
 mov rsi,r12
 call .Lgt_options
 test eax,eax
 jnz .Lg_bad
 lea r13,[rip+.Lg_delete_keys]
 xor r14d,r14d
.Lg_delete:
 mov rsi,[r13+r14*8]
 test rsi,rsi
 jz .Lg_ok
 mov rdi,rbx
 call json_object_object_del@PLT
 inc r14
 jmp .Lg_delete
.Lg_ok:
 xor ebp,ebp
 jmp .Lg_end
.Lg_bad:
 mov ebp,1
.Lg_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Translate already-owned content blocks, preserving nested result/image data. */
.Lg_parts:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,80
 mov rbx,rdi
 mov r12,rsi
 mov [rsp+8],rdx
 mov qword ptr [rdx],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+48],0
 mov qword ptr [rsp+56],0
 mov qword ptr [rsp+64],0
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lg_parts_bad
 xor r13d,r13d
.Lg_part:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lg_parts_ok
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 GG rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lg_parts_bad
 mov rdi,rax
 lea rsi,[rip+.Lg_text]
 call strcmp@PLT
 test eax,eax
 jz .Lg_text_part
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lg_image]
 call strcmp@PLT
 test eax,eax
 jz .Lg_image_part
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lg_tool_use]
 call strcmp@PLT
 test eax,eax
 jz .Lg_call_part
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lg_tool_result]
 call strcmp@PLT
 test eax,eax
 jz .Lg_result_part
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lg_thinking]
 call strcmp@PLT
 test eax,eax
 jz .Lg_thinking_part
 jmp .Lg_parts_bad
.Lg_text_part:
 lea rdi,[rip+.Lap_empty_json]
 mov rsi,[rsp]
 call .Lrp_append
 mov rbp,rax
 test rax,rax
 jz .Lg_parts_bad
 mov rdi,rax
 mov rsi,r14
 lea rdx,[rip+.Lg_text]
 mov rcx,rdx
 call .Lrp_copy
 test eax,eax
 jnz .Lg_parts_bad
 mov rdi,rbp
 mov rsi,r14
 lea rdx,[rip+.Lg_googleSignature]
 call .Lg_copy_signature
 test eax,eax
 jnz .Lg_parts_bad
 jmp .Lg_part_next
.Lg_thinking_part:
 lea rdi,[rip+.Lg_thought_json]
 mov rsi,[rsp]
 call .Lrp_append
 mov rbp,rax
 test rax,rax
 jz .Lg_parts_bad
 mov rdi,rax
 mov rsi,r14
 lea rdx,[rip+.Lg_thinking]
 lea rcx,[rip+.Lg_text]
 call .Lrp_copy
 test eax,eax
 jnz .Lg_parts_bad
 mov rdi,rbp
 mov rsi,r14
 lea rdx,[rip+.Lg_signature]
 call .Lg_copy_signature
 test eax,eax
 jnz .Lg_parts_bad
 jmp .Lg_part_next
.Lg_image_part:
 lea rdi,[rip+.Lg_image_json]
 mov rsi,[rsp]
 call .Lrp_append
 test rax,rax
 jz .Lg_parts_bad
 GG rax,inlineData
 mov rbp,rax
 GG r14,source
 mov [rsp+32],rax
 mov rdi,rbp
 mov rsi,rax
 lea rdx,[rip+.Lg_media_type]
 lea rcx,[rip+.Lg_mimeType]
 call .Lrp_copy
 test eax,eax
 jnz .Lg_parts_bad
 mov rdi,rbp
 mov rsi,[rsp+32]
 lea rdx,[rip+.Lg_data]
 mov rcx,rdx
 call .Lrp_copy
 test eax,eax
 jnz .Lg_parts_bad
 jmp .Lg_part_next
.Lg_call_part:
 lea rdi,[rip+.Lg_call_json]
 mov rsi,[rsp]
 call .Lrp_append
 test rax,rax
 jz .Lg_parts_bad
 GG rax,functionCall
 mov rbp,rax
 mov rdi,rax
 mov rsi,r14
 lea rdx,[rip+.Lg_name]
 mov rcx,rdx
 call .Lrp_copy
 test eax,eax
 jnz .Lg_parts_bad
 mov rdi,rbp
 mov rsi,r14
 lea rdx,[rip+.Lg_input]
 lea rcx,[rip+.Lg_args]
 call .Lrp_copy
 test eax,eax
 jnz .Lg_parts_bad
 GG r12,model
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 call pg_google_requires_id
 test eax,eax
 jz .Lg_call_signature
 mov rdi,rbp
 mov rsi,r14
 lea rdx,[rip+.Lg_id]
 mov rcx,rdx
 call .Lrp_copy
 test eax,eax
 jnz .Lg_parts_bad
.Lg_call_signature:
 # The signature belongs to the enclosing Part, not functionCall.
 mov rdi,[rsp]
 call json_object_array_length@PLT
 lea rsi,[rax-1]
 mov rdi,[rsp]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 mov rsi,r14
 lea rdx,[rip+.Lg_googleSignature]
 call .Lg_copy_signature
 test eax,eax
 jnz .Lg_parts_bad
 jmp .Lg_part_next
.Lg_result_part:
 lea rdi,[rip+.Lg_result_json]
 mov rsi,[rsp]
 call .Lrp_append
 test rax,rax
 jz .Lg_parts_bad
 GG rax,functionResponse
 mov rbp,rax
 mov rdi,rax
 mov rsi,r14
 lea rdx,[rip+.Lg_toolName]
 lea rcx,[rip+.Lg_name]
 call .Lrp_copy
 test eax,eax
 jnz .Lg_parts_bad
 GG r12,model
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 call pg_google_requires_id
 test eax,eax
 jz .Lg_result_parts
 mov rdi,rbp
 mov rsi,r14
 lea rdx,[rip+.Lg_tool_use_id]
 lea rcx,[rip+.Lg_id]
 call .Lrp_copy
 test eax,eax
 jnz .Lg_parts_bad
.Lg_result_parts:
 GG r14,content
 mov rdi,rax
 mov rsi,r12
 lea rdx,[rsp+16]
 call .Lg_parts
 test eax,eax
 jnz .Lg_parts_bad
 lea rdi,[rip+.Lap_empty_json]
 call json_tokener_parse@PLT
 mov [rsp+56],rax
 test rax,rax
 jz .Lg_parts_bad
 call json_object_new_array@PLT
 mov [rsp+64],rax
 test rax,rax
 jz .Lg_parts_bad
 mov qword ptr [rsp+40],0
 mov qword ptr [rsp+48],0
.Lg_result_text:
 mov rdi,[rsp+16]
 call json_object_array_length@PLT
 cmp [rsp+40],rax
 jae .Lg_result_value
 mov rdi,[rsp+16]
 mov rsi,[rsp+40]
 call json_object_array_get_idx@PLT
 mov [rsp+72],rax
 GG rax,text
 test rax,rax
 jz .Lg_result_image
 lea rdi,[rsp+48]
 mov rsi,rax
 mov edx,1
 call pg_context_append
 test eax,eax
 jnz .Lg_parts_bad
 jmp .Lg_result_next
.Lg_result_image:
 GG "qword ptr [rsp+72]",inlineData
 test rax,rax
 jz .Lg_result_next
 mov rdi,[rsp+72]
 call json_object_get@PLT
 mov rdi,[rsp+64]
 mov rsi,rax
 call .Lap_ei_append_owned
 test eax,eax
 jnz .Lg_parts_bad
.Lg_result_next:
 inc qword ptr [rsp+40]
 jmp .Lg_result_text
.Lg_result_value:
 cmp qword ptr [rsp+48],0
 je .Lg_result_empty_value
 mov rdi,[rsp+48]
 call json_object_get_string_len@PLT
 mov [rsp+32],eax
 mov rdi,[rsp+48]
 call json_object_get_string@PLT
 mov rdi,rax
 mov esi,[rsp+32]
 call pg_context_blank
 test eax,eax
 jz .Lg_result_value_ready
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov qword ptr [rsp+48],0
.Lg_result_empty_value:
 mov rdi,[rsp+64]
 call json_object_array_length@PLT
 lea rdi,[rip+.Lg_empty]
 test rax,rax
 jz .Lg_result_placeholder
 lea rdi,[rip+.Lg_attached]
.Lg_result_placeholder:
 call json_object_new_string@PLT
 mov [rsp+48],rax
 test rax,rax
 jz .Lg_parts_bad
.Lg_result_value_ready:
 GG r14,is_error
 mov rdi,rax
 call json_object_get_boolean@PLT
 lea rsi,[rip+.Lg_output]
 test eax,eax
 jz .Lg_result_key
 lea rsi,[rip+.Lg_error]
.Lg_result_key:
 mov rdi,[rsp+56]
 mov rdx,[rsp+48]
 mov qword ptr [rsp+48],0
 call .Lrp_set
 test eax,eax
 jnz .Lg_parts_bad
 mov rdi,rbp
 lea rsi,[rip+.Lg_response]
 mov rdx,[rsp+56]
 mov qword ptr [rsp+56],0
 call .Lrp_set
 test eax,eax
 jnz .Lg_parts_bad
 mov rdi,[rsp+64]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lg_result_images_done
 mov rdi,r12
 call .Lg_old_model
 test eax,eax
 jnz .Lg_legacy_result_images
 mov rdi,rbp
 lea rsi,[rip+.Lg_parts_key]
 mov rdx,[rsp+64]
 mov qword ptr [rsp+64],0
 call .Lrp_set
 test eax,eax
 jnz .Lg_parts_bad
 jmp .Lg_result_images_done
.Lg_legacy_result_images:
 lea rdi,[rip+.Lg_legacy_images_json]
 mov rsi,[rsp]
 call .Lrp_append
 test rax,rax
 jz .Lg_parts_bad
 GG rax,panguToolImages
 mov [rsp+72],rax
 mov qword ptr [rsp+40],0
.Lg_legacy_image:
 mov rdi,[rsp+64]
 call json_object_array_length@PLT
 cmp [rsp+40],rax
 jae .Lg_result_images_done
 mov rdi,[rsp+64]
 mov rsi,[rsp+40]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,[rsp+72]
 mov rsi,rax
 call .Lap_ei_append_owned
 test eax,eax
 jnz .Lg_parts_bad
 inc qword ptr [rsp+40]
 jmp .Lg_legacy_image
.Lg_result_images_done:
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov qword ptr [rsp+64],0
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov qword ptr [rsp+16],0
.Lg_part_next:
 inc r13
 jmp .Lg_part
.Lg_parts_ok:
 mov rdx,[rsp+8]
 mov rax,[rsp]
 mov [rdx],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
 jmp .Lg_parts_end
.Lg_parts_bad:
 mov ebp,1
.Lg_parts_end:
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,80
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lg_messages: .asciz "messages"
.Lg_role: .asciz "role"
.Lg_assistant: .asciz "assistant"
.Lg_model: .asciz "model"
.Lg_content: .asciz "content"
.Lg_parts_key: .asciz "parts"
.Lg_contents: .asciz "contents"
.Lg_system: .asciz "system"
.Lg_systemInstruction: .asciz "systemInstruction"
.Lg_max_tokens: .asciz "max_tokens"
.Lg_maxOutputTokens: .asciz "maxOutputTokens"
.Lg_temperature: .asciz "temperature"
.Lg_generationConfig: .asciz "generationConfig"
.Lg_tools: .asciz "tools"
.Lg_functionDeclarations: .asciz "functionDeclarations"
.Lg_name: .asciz "name"
.Lg_description: .asciz "description"
.Lg_input_schema: .asciz "input_schema"
.Lg_parametersJsonSchema: .asciz "parametersJsonSchema"
.Lg_type: .asciz "type"
.Lg_text: .asciz "text"
.Lg_image: .asciz "image"
.Lg_tool_use: .asciz "tool_use"
.Lg_tool_result: .asciz "tool_result"
.Lg_thinking: .asciz "thinking"
.Lg_signature: .asciz "signature"
.Lg_thoughtSignature: .asciz "thoughtSignature"
.Lg_inlineData: .asciz "inlineData"
.Lg_source: .asciz "source"
.Lg_media_type: .asciz "media_type"
.Lg_mimeType: .asciz "mimeType"
.Lg_data: .asciz "data"
.Lg_functionCall: .asciz "functionCall"
.Lg_input: .asciz "input"
.Lg_args: .asciz "args"
.Lg_functionResponse: .asciz "functionResponse"
.Lg_toolName: .asciz "toolName"
.Lg_is_error: .asciz "is_error"
.Lg_output: .asciz "output"
.Lg_error: .asciz "error"
.Lg_response: .asciz "response"
.Lg_empty: .asciz ""
.Lg_stream: .asciz "stream"
.Lg_stream_options: .asciz "stream_options"
.Lg_reasoning_effort: .asciz "reasoning_effort"
.Lg_max_completion_tokens: .asciz "max_completion_tokens"
.Lg_content_json: .asciz "{}"
.Lg_tools_json: .asciz "[{\"functionDeclarations\":[]}]"
.Lg_thought_json: .asciz "{\"thought\":true}"
.Lg_image_json: .asciz "{\"inlineData\":{}}"
.Lg_call_json: .asciz "{\"functionCall\":{}}"
.Lg_result_json: .asciz "{\"functionResponse\":{\"response\":{}}}"

.section .data.rel.ro
.Lg_delete_keys: .quad .Lg_messages,.Lg_system,.Lg_max_tokens,.Lg_model,.Lg_stream,.Lg_stream_options,.Lg_reasoning_effort,.Lg_thinking,.Lg_max_completion_tokens,.Lg_temperature,0

.text
.Lg_copy_signature:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov rdi,rsi
 mov rsi,rdx
 call json_object_object_get@PLT
 mov r12,rax
 mov rdi,rax
 call pg_google_signature_valid
 test eax,eax
 jz .Lg_signature_ok
 mov rdi,r12
 call json_object_get@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lg_thoughtSignature]
 mov rdx,rax
 call .Lrp_set
 jmp .Lg_signature_end
.Lg_signature_ok:
 xor eax,eax
.Lg_signature_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.section .rodata
.Lg_id: .asciz "id"
.Lg_tool_use_id: .asciz "tool_use_id"
.Lg_googleSignature: .asciz "googleSignature"

.section .rodata
.Lg_attached: .asciz "(see attached image)"

.text
/* Gemini <3 carries tool images in a separate user turn. */
.Lg_old_model:
 push rbx
 GG rdi,model
 mov rdi,rax
 call json_object_get_string@PLT
 mov rbx,rax
 test rax,rax
 jz .Lg_old_no
 mov rdi,rax
 lea rsi,[rip+.Lg_gemini_live]
 mov edx,12
 call strncasecmp@PLT
 test eax,eax
 jz .Lg_old_live
 mov rdi,rbx
 lea rsi,[rip+.Lg_gemini_prefix]
 mov edx,7
 call strncasecmp@PLT
 test eax,eax
 jnz .Lg_old_no
 movzx eax,byte ptr [rbx+7]
 jmp .Lg_old_digit
.Lg_old_live:
 movzx eax,byte ptr [rbx+12]
.Lg_old_digit:
 sub eax,'0'
 cmp eax,9
 ja .Lg_old_no
 mov rdi,rbx
 call pg_google_requires_id
 xor eax,1
 pop rbx
 ret
.Lg_old_no:
 xor eax,eax
 pop rbx
 ret
/* Split only the temporary image markers; wire output contains Content/Part. */
.Lg_split_parts:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
 mov qword ptr [rsp],0
.Lg_split_each:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lg_split_adopt
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbp,rax
 GG rax,panguToolImages
 mov [rsp+8],rax
 test rax,rax
 jnz .Lg_split_marker
 test r12,r12
 jnz .Lg_split_array
 lea rdi,[rip+.Lg_user_content_json]
 mov rsi,rbx
 call .Lrp_append
 mov r12,rax
 test rax,rax
 jz .Lg_split_bad
.Lg_split_array:
 cmp qword ptr [rsp],0
 jne .Lg_split_append
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lg_split_bad
.Lg_split_append:
 mov rdi,rbp
 call json_object_get@PLT
 mov rdi,[rsp]
 mov rsi,rax
 call .Lap_ei_append_owned
 test eax,eax
 jnz .Lg_split_bad
 jmp .Lg_split_next
.Lg_split_marker:
 test r12,r12
 jz .Lg_split_bad
 mov rdi,r12
 lea rsi,[rip+.Lg_parts_key]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lg_split_bad
 lea rdi,[rip+.Lg_user_content_json]
 mov rsi,rbx
 call .Lrp_append
 mov r12,rax
 test rax,rax
 jz .Lg_split_bad
 mov rdi,[rsp+8]
 call json_object_get@PLT
 mov rdi,r12
 lea rsi,[rip+.Lg_parts_key]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lg_split_bad
 xor r12d,r12d
.Lg_split_next:
 inc r14
 jmp .Lg_split_each
.Lg_split_adopt:
 test r12,r12
 jz .Lg_split_ok
 mov rdi,r12
 lea rsi,[rip+.Lg_parts_key]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lg_split_bad
.Lg_split_ok:
 xor ebp,ebp
 jmp .Lg_split_end
.Lg_split_bad:
 mov ebp,1
.Lg_split_end:
 mov rdi,[rsp]
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
.Lg_panguToolImages: .asciz "panguToolImages"
.Lg_legacy_images_json: .asciz "{\"panguToolImages\":[{\"text\":\"Tool result image:\"}]}"
.Lg_user_content_json: .asciz "{\"role\":\"user\"}"
.Lg_gemini_live: .asciz "gemini-live-"
.Lg_gemini_prefix: .asciz "gemini-"

/* rewriteConstAsEnum only traverses schema keywords, never default/example data. */
.text
.Lg_const_enum:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov r12d,esi
 mov r13d,edx
 mov qword ptr [rsp],0
 cmp edx,128
 jae .Lg_const_bad
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lg_const_object
 xor r14d,r14d
.Lg_const_array:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lg_const_ok
 mov rdi,rbx
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rax
 xor esi,esi
 lea edx,[r13+1]
 call .Lg_const_enum
 test eax,eax
 jnz .Lg_const_bad
 inc r14
 jmp .Lg_const_array
.Lg_const_object:
 mov rdi,rbx
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lg_const_ok
 mov rdi,rbx
 call json_object_iter_begin@PLT
 mov [rsp+8],rax
 mov rdi,rbx
 call json_object_iter_end@PLT
 mov [rsp+16],rax
.Lg_const_field:
 lea rdi,[rsp+8]
 lea rsi,[rsp+16]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lg_const_rewrite
 lea rdi,[rsp+8]
 call json_object_iter_peek_name@PLT
 mov rbp,rax
 test r12d,r12d
 jnz .Lg_const_child_schema
 lea r14,[rip+.Lg_schema_keys]
.Lg_const_schema_key:
 mov rsi,[r14]
 test rsi,rsi
 jz .Lg_const_maps_begin
 mov rdi,rbp
 call strcmp@PLT
 test eax,eax
 jz .Lg_const_child_schema
 add r14,8
 jmp .Lg_const_schema_key
.Lg_const_maps_begin:
 lea r14,[rip+.Lg_map_keys]
.Lg_const_map_key:
 mov rsi,[r14]
 test rsi,rsi
 jz .Lg_const_next
 mov rdi,rbp
 call strcmp@PLT
 test eax,eax
 jz .Lg_const_child_map
 add r14,8
 jmp .Lg_const_map_key
.Lg_const_child_map:
 mov dword ptr [rsp+32],1
 jmp .Lg_const_child
.Lg_const_child_schema:
 mov dword ptr [rsp+32],0
.Lg_const_child:
 lea rdi,[rsp+8]
 call json_object_iter_peek_value@PLT
 mov rdi,rax
 mov esi,[rsp+32]
 lea edx,[r13+1]
 call .Lg_const_enum
 test eax,eax
 jnz .Lg_const_bad
.Lg_const_next:
 lea rdi,[rsp+8]
 call json_object_iter_next@PLT
 jmp .Lg_const_field
.Lg_const_rewrite:
 test r12d,r12d
 jnz .Lg_const_ok
 mov rdi,rbx
 lea rsi,[rip+.Lst_enum]
 lea rdx,[rsp+24]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Lg_const_ok
 mov rdi,rbx
 lea rsi,[rip+.Lst_const]
 lea rdx,[rsp+24]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lg_const_ok
 mov rdi,[rsp+24]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jnz .Lg_const_convert
 mov rdi,[rsp+24]
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jnz .Lg_const_convert
 mov rdi,[rsp+24]
 mov esi,2
 call json_object_is_type@PLT
 test eax,eax
 jz .Lg_const_ok
.Lg_const_convert:
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lg_const_bad
 mov rdi,[rsp+24]
 call json_object_get@PLT
 mov rdi,[rsp]
 mov rsi,rax
 call .Lap_ei_append_owned
 test eax,eax
 jnz .Lg_const_bad
 mov rdi,rbx
 lea rsi,[rip+.Lst_const]
 call json_object_object_del@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lst_enum]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lg_const_bad
.Lg_const_ok:
 xor ebp,ebp
 jmp .Lg_const_end
.Lg_const_bad:
 mov ebp,1
.Lg_const_end:
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
.Lg_tool_mode:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13d,edx
 mov qword ptr [rsp],0
 GG r12,options
 GG rax,toolChoice
 mov r14,rax
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lg_mode_default
 mov rbp,rax
 mov rdi,rax
 lea rsi,[rip+.Lap_none]
 call strcmp@PLT
 lea r14,[rip+.Lg_NONE]
 test eax,eax
 jz .Lg_mode_create
 mov rdi,rbp
 lea rsi,[rip+.Lg_any]
 call strcmp@PLT
 lea r14,[rip+.Lg_ANY]
 test eax,eax
 jz .Lg_mode_create
 test r13d,r13d
 jnz .Lg_mode_validated
 lea r14,[rip+.Lg_AUTO]
 jmp .Lg_mode_create
.Lg_mode_default:
 test r13d,r13d
 jz .Lg_mode_ok
.Lg_mode_validated:
 lea r14,[rip+.Lg_VALIDATED]
.Lg_mode_create:
 lea rdi,[rip+.Lg_tool_mode_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lg_mode_bad
 GG rax,functionCallingConfig
 mov rdi,rax
 lea rsi,[rip+.Lg_mode]
 mov rdx,r14
 call li_field
 test eax,eax
 jnz .Lg_mode_bad
 mov rdi,rbx
 lea rsi,[rip+.Lg_toolConfig]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lg_mode_bad
.Lg_mode_ok:
 xor ebp,ebp
 jmp .Lg_mode_end
.Lg_mode_bad:
 mov ebp,1
.Lg_mode_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lg_options: .asciz "options"
.Lg_toolChoice: .asciz "toolChoice"
.Lg_strict: .asciz "strict"
.Lg_any: .asciz "any"
.Lg_NONE: .asciz "NONE"
.Lg_ANY: .asciz "ANY"
.Lg_AUTO: .asciz "AUTO"
.Lg_VALIDATED: .asciz "VALIDATED"
.Lg_functionCallingConfig: .asciz "functionCallingConfig"
.Lg_mode: .asciz "mode"
.Lg_toolConfig: .asciz "toolConfig"
.Lg_tool_mode_json: .asciz "{\"functionCallingConfig\":{}}"
.Lg_schema_name_0: .asciz "properties"
.Lg_schema_name_1: .asciz "patternProperties"
.Lg_schema_name_2: .asciz "$defs"
.Lg_schema_name_3: .asciz "definitions"
.Lg_schema_name_4: .asciz "dependentSchemas"
.Lg_schema_name_5: .asciz "dependencies"
.Lg_schema_name_6: .asciz "additionalProperties"
.Lg_schema_name_7: .asciz "unevaluatedProperties"
.Lg_schema_name_8: .asciz "propertyNames"
.Lg_schema_name_9: .asciz "items"
.Lg_schema_name_10: .asciz "prefixItems"
.Lg_schema_name_11: .asciz "additionalItems"
.Lg_schema_name_12: .asciz "unevaluatedItems"
.Lg_schema_name_13: .asciz "contains"
.Lg_schema_name_14: .asciz "allOf"
.Lg_schema_name_15: .asciz "anyOf"
.Lg_schema_name_16: .asciz "oneOf"
.Lg_schema_name_17: .asciz "not"
.Lg_schema_name_18: .asciz "if"
.Lg_schema_name_19: .asciz "then"
.Lg_schema_name_20: .asciz "else"
.Lg_schema_name_21: .asciz "contentSchema"

.section .data.rel.ro
.Lg_map_keys: .quad .Lg_schema_name_0,.Lg_schema_name_1,.Lg_schema_name_2,.Lg_schema_name_3,.Lg_schema_name_4,.Lg_schema_name_5,0
.Lg_schema_keys: .quad .Lg_schema_name_6,.Lg_schema_name_7,.Lg_schema_name_8,.Lg_schema_name_9,.Lg_schema_name_10,.Lg_schema_name_11,.Lg_schema_name_12,.Lg_schema_name_13,.Lg_schema_name_14,.Lg_schema_name_15,.Lg_schema_name_16,.Lg_schema_name_17,.Lg_schema_name_18,.Lg_schema_name_19,.Lg_schema_name_20,.Lg_schema_name_21,0

/* Google native thinking options and simple reasoning-level adaptation. */
.text
.Lgt_options:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 GG r12,modelInfo
 mov r13,rax
 GG rax,reasoning
 test rax,rax
 jz .Lgt_explicit
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lgt_ok
.Lgt_explicit:
 GG r12,options
 mov [rsp+16],rax
 GG rax,thinking
 mov r14,rax
 test rax,rax
 jz .Lgt_simple
 GG r14,enabled
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lgt_disabled
 lea rdi,[rip+.Lgt_thinking_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgt_bad
 GG r14,level
 test rax,rax
 jz .Lgt_explicit_budget
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 call .Lgt_level
 test rax,rax
 jz .Lgt_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lgt_thinkingLevel]
 mov rdx,rax
 call li_field
 test eax,eax
 jnz .Lgt_bad
 jmp .Lgt_adopt
.Lgt_explicit_budget:
 mov rdi,[rsp]
 mov rsi,r14
 lea rdx,[rip+.Lgt_budgetTokens]
 lea rcx,[rip+.Lgt_thinkingBudget]
 call .Lrp_copy
 test eax,eax
 jnz .Lgt_bad
 jmp .Lgt_adopt
.Lgt_simple:
 GG rbx,reasoning_effort
 test rax,rax
 jnz .Lgt_simple_value
 GG "qword ptr [rsp+16]",reasoningEffort
 test rax,rax
 jnz .Lgt_simple_value
 GG r13,reasoning
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lgt_ok
 jmp .Lgt_disabled
.Lgt_simple_value:
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 test rax,rax
 jz .Lgt_bad
 mov rdi,rax
 lea rsi,[rip+.Lap_off]
 call strcasecmp@PLT
 test eax,eax
 jz .Lgt_disabled
 mov rdi,r14
 call .Lgt_level
 mov [rsp+24],rax
 test rax,rax
 jz .Lgt_bad
 lea rdi,[rip+.Lgt_thinking_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgt_bad
 GG r12,model
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lgt_bad
 mov rdi,rax
 call .Lgt_uses_level
 test eax,eax
 jz .Lgt_budget
 mov rdi,[rsp]
 lea rsi,[rip+.Lgt_thinkingLevel]
 mov rdx,[rsp+24]
 call li_field
 test eax,eax
 jnz .Lgt_bad
 jmp .Lgt_adopt
.Lgt_budget:
 GG "qword ptr [rsp+16]",thinkingBudgets
 mov rdi,rax
 mov rsi,r14
 call json_object_object_get@PLT
 test rax,rax
 jz .Lgt_default_budget
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lgt_thinkingBudget]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lgt_bad
 jmp .Lgt_adopt
.Lgt_default_budget:
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lgt_pro25]
 call strstr@PLT
 mov ebp,32768
 test rax,rax
 jnz .Lgt_budget_kind
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lgt_flash25]
 call strstr@PLT
 mov ebp,24576
 test rax,rax
 jnz .Lgt_budget_kind
 mov rdi,-1
 jmp .Lgt_budget_set
.Lgt_budget_kind:
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lgt_MINIMAL]
 call strcmp@PLT
 mov edi,128
 test eax,eax
 jnz .Lgt_budget_low
 mov [rsp+40],rdi
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lgt_lite25]
 call strstr@PLT
 mov rdi,[rsp+40]
 test rax,rax
 jz .Lgt_budget_set
 mov edi,512
 jmp .Lgt_budget_set
.Lgt_budget_low:
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lgt_LOW]
 call strcmp@PLT
 mov edi,2048
 test eax,eax
 jz .Lgt_budget_set
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lgt_MEDIUM]
 call strcmp@PLT
 mov edi,8192
 test eax,eax
 jz .Lgt_budget_set
 mov edi,ebp
.Lgt_budget_set:
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lgt_thinkingBudget]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lgt_bad
 jmp .Lgt_adopt
.Lgt_disabled:
 lea rdi,[rip+.Lgt_disabled_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgt_bad
 GG r13,thinkingLevelMap
 GG rax,off
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lgt_adopt
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lap_off]
 call strcasecmp@PLT
 test eax,eax
 jz .Lgt_adopt
 GG r12,model
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 call .Lgt_uses_level
 test eax,eax
 jz .Lgt_adopt
 mov rdi,r14
 call .Lgt_level
 test rax,rax
 jz .Lgt_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lgt_thinkingLevel]
 mov rdx,rax
 call li_field
 test eax,eax
 jnz .Lgt_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lgt_thinkingBudget]
 call json_object_object_del@PLT
.Lgt_adopt:
 GG rbx,generationConfig
 mov rdi,rax
 lea rsi,[rip+.Lgt_thinkingConfig]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lgt_bad
.Lgt_ok:
 xor ebp,ebp
 jmp .Lgt_end
.Lgt_bad:
 mov ebp,1
.Lgt_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lgt_level:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 test rdi,rdi
 jz .Lgt_level_bad
 lea r12,[rip+.Lgt_level_names]
 xor r13d,r13d
.Lgt_level_each:
 mov rsi,[r12+r13*8]
 test rsi,rsi
 jz .Lgt_level_bad
 mov rdi,rbx
 call strcasecmp@PLT
 test eax,eax
 jz .Lgt_level_found
 inc r13
 jmp .Lgt_level_each
.Lgt_level_found:
 lea rax,[rip+.Lgt_level_values]
 mov rax,[rax+r13*8]
 jmp .Lgt_level_end
.Lgt_level_bad:
 xor eax,eax
.Lgt_level_end:
 pop r13
 pop r12
 pop rbx
 ret
.Lgt_uses_level:
 push rbx
 mov rbx,rdi
 test rdi,rdi
 jz .Lgt_uses_no
 lea rsi,[rip+.Lgt_gemini3]
 call strcasestr@PLT
 test rax,rax
 jz .Lgt_uses_later
 lea rdx,[rax+8]
 cmp byte ptr [rdx],'.'
 jne .Lgt_uses_family
 inc rdx
 movzx eax,byte ptr [rdx]
 sub eax,'0'
 cmp eax,9
 ja .Lgt_uses_later
.Lgt_minor:
 inc rdx
 movzx eax,byte ptr [rdx]
 sub eax,'0'
 cmp eax,9
 jbe .Lgt_minor
.Lgt_uses_family:
 cmp dword ptr [rdx],0x6f72702d
 je .Lgt_uses_yes
 cmp dword ptr [rdx],0x616c662d
 jne .Lgt_uses_later
 cmp word ptr [rdx+4],0x6873
 je .Lgt_uses_yes
.Lgt_uses_later:
 mov rdi,rbx
 lea rsi,[rip+.Lg_gemini_prefix]
 call strcasestr@PLT
 test rax,rax
 jz .Lgt_uses_alias
 movzx edx,byte ptr [rax+7]
 cmp dl,'4'
 jb .Lgt_uses_two_digits
 cmp dl,'9'
 jbe .Lgt_uses_yes
.Lgt_uses_two_digits:
 cmp dl,'1'
 jb .Lgt_uses_alias
 cmp dl,'9'
 ja .Lgt_uses_alias
 movzx edx,byte ptr [rax+8]
 sub edx,'0'
 cmp edx,9
 jbe .Lgt_uses_yes
.Lgt_uses_alias:
 mov rdi,rbx
 lea rsi,[rip+.Lgt_flash_latest]
 call strcasecmp@PLT
 test eax,eax
 jz .Lgt_uses_yes
 mov rdi,rbx
 lea rsi,[rip+.Lgt_lite_latest]
 call strcasecmp@PLT
 test eax,eax
 jz .Lgt_uses_yes
 mov rdi,rbx
 lea rsi,[rip+.Lgt_gemma4]
 call strcasestr@PLT
 test rax,rax
 jnz .Lgt_uses_yes
 mov rdi,rbx
 lea rsi,[rip+.Lgt_gemma_dash4]
 call strcasestr@PLT
 test rax,rax
 jnz .Lgt_uses_yes
.Lgt_uses_no:
 xor eax,eax
 pop rbx
 ret
.Lgt_uses_yes:
 mov eax,1
 pop rbx
 ret
.section .rodata
.Lg_reasoning: .asciz "reasoning"
.Lg_enabled: .asciz "enabled"
.Lg_level: .asciz "level"
.Lg_reasoningEffort: .asciz "reasoningEffort"
.Lg_thinkingBudgets: .asciz "thinkingBudgets"
.Lg_thinkingLevelMap: .asciz "thinkingLevelMap"
.Lg_off: .asciz "off"
.Lgt_budgetTokens: .asciz "budgetTokens"
.Lgt_thinkingConfig: .asciz "thinkingConfig"
.Lgt_thinkingLevel: .asciz "thinkingLevel"
.Lgt_thinkingBudget: .asciz "thinkingBudget"
.Lgt_thinking_json: .asciz "{\"includeThoughts\":true}"
.Lgt_disabled_json: .asciz "{\"thinkingBudget\":0}"
.Lgt_pro25: .asciz "2.5-pro"
.Lgt_flash25: .asciz "2.5-flash"
.Lgt_lite25: .asciz "2.5-flash-lite"
.Lgt_gemini3: .asciz "gemini-3"
.Lgt_flash_latest: .asciz "gemini-flash-latest"
.Lgt_lite_latest: .asciz "gemini-flash-lite-latest"
.Lgt_gemma4: .asciz "gemma4"
.Lgt_gemma_dash4: .asciz "gemma-4"
.Lgt_MINIMAL: .asciz "MINIMAL"
.Lgt_LOW: .asciz "LOW"
.Lgt_MEDIUM: .asciz "MEDIUM"
.Lgt_HIGH: .asciz "HIGH"
.Lgt_minimal: .asciz "minimal"
.Lgt_low: .asciz "low"
.Lgt_medium: .asciz "medium"
.Lgt_high: .asciz "high"
.section .data.rel.ro
.Lgt_level_names: .quad .Lgt_minimal,.Lgt_low,.Lgt_medium,.Lgt_high,0
.Lgt_level_values: .quad .Lgt_MINIMAL,.Lgt_LOW,.Lgt_MEDIUM,.Lgt_HIGH
.section .rodata
.Lg_modelInfo: .asciz "modelInfo"

.section .rodata
.Lrp_model_field: .asciz "model"
.Lrp_azureDeploymentName: .asciz "azureDeploymentName"


.text
.Lbd_request:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov [rsp+48],rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov rdi,rbx
 call .Lbi_ids
 test eax,eax
 jnz .Lbd_bad
 mov rdi,rbx
 lea rsi,[rip+.Lbd_system]
 call json_object_object_get@PLT
 mov r12,rax
 test rax,rax
 jz .Lbd_messages_flow
 xor r13d,r13d
.Lbd_system_flow:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lbd_messages_flow
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rbp,rax
 mov rdi,rbp
 lea rsi,[rip+.Lbd_type]
 call json_object_object_del@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lbd_cache_control]
 call json_object_object_del@PLT
 inc r13
 jmp .Lbd_system_flow
.Lbd_messages_flow:
 mov rdi,rbx
 lea rsi,[rip+.Lbd_messages]
 call json_object_object_get@PLT
 mov r12,rax
 xor r13d,r13d
.Lbd_message_each:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lbd_tools_flow
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rip+.Lbd_content]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,[rsp+48]
 call .Lbd_blocks
 test eax,eax
 jnz .Lbd_bad
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rip+.Lbd_content]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_array_length@PLT
 test rax,rax
 jnz .Lbd_message_kept
 mov rdi,r12
 mov rsi,r13
 mov edx,1
 call json_object_array_del_idx@PLT
 test eax,eax
 jnz .Lbd_bad
 jmp .Lbd_message_each
.Lbd_message_kept:
 inc r13
 jmp .Lbd_message_each
.Lbd_tools_flow:
 mov rdi,rbx
 lea rsi,[rip+.Lbd_tools]
 call json_object_object_get@PLT
 mov r12,rax
 test rax,rax
 jz .Lbd_inference
 xor r13d,r13d
.Lbd_tool_each:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lbd_tool_attach
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rbp,rax
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbd_bad
 mov rdi,qword ptr [rsp]
 mov rsi,rbp
 lea rdx,[rip+.Lbd_input_schema]
 lea rcx,[rip+.Lbd_json]
 call .Lrp_copy
 test eax,eax
 jnz .Lbd_bad
 mov rdi,rbp
 lea rsi,[rip+.Lbd_input_schema]
 call json_object_object_del@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lbd_inputSchema]
 mov rdx,qword ptr [rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lbd_bad
 mov qword ptr [rsp],0
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbd_bad
 mov rdi,rbp
 call json_object_get@PLT
 mov rdi,qword ptr [rsp]
 lea rsi,[rip+.Lbd_toolSpec]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lbd_bad
 mov rdi,r12
 mov rsi,r13
 mov rdx,[rsp]
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Lbd_bad
 mov qword ptr [rsp],0
 inc r13
 jmp .Lbd_tool_each
.Lbd_tool_attach:
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbd_bad
 mov rdi,qword ptr [rsp]
 mov rsi,rbx
 lea rdx,[rip+.Lbd_tools]
 lea rcx,[rip+.Lbd_tools]
 call .Lrp_copy
 test eax,eax
 jnz .Lbd_bad
 mov rdi,rbx
 lea rsi,[rip+.Lbd_toolConfig]
 mov rdx,qword ptr [rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lbd_bad
 mov qword ptr [rsp],0
 mov rdi,rbx
 lea rsi,[rip+.Lbd_tools]
 call json_object_object_del@PLT
.Lbd_inference:
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbd_bad
 mov rdi,qword ptr [rsp]
 mov rsi,rbx
 lea rdx,[rip+.Lbd_max_tokens]
 lea rcx,[rip+.Lbd_maxTokens]
 call .Lrp_copy
 test eax,eax
 jnz .Lbd_bad
 mov rdi,qword ptr [rsp]
 mov rsi,rbx
 lea rdx,[rip+.Lbd_temperature]
 lea rcx,[rip+.Lbd_temperature]
 call .Lrp_copy
 test eax,eax
 jnz .Lbd_bad
 mov rdi,qword ptr [rsp]
 mov rsi,rbx
 lea rdx,[rip+.Lbd_top_p]
 lea rcx,[rip+.Lbd_topP]
 call .Lrp_copy
 test eax,eax
 jnz .Lbd_bad
 mov rdi,rbx
 lea rsi,[rip+.Lbd_inferenceConfig]
 mov rdx,qword ptr [rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lbd_bad
 mov qword ptr [rsp],0
 mov rdi,rbx
 lea rsi,[rip+.Lbd_model]
 call json_object_object_del@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lbd_max_tokens]
 call json_object_object_del@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lbd_temperature]
 call json_object_object_del@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lbd_top_p]
 call json_object_object_del@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lbd_stream]
 call json_object_object_del@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lbd_thinking]
 call json_object_object_del@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lbd_tool_choice]
 call json_object_object_del@PLT
 mov rdi,rbx
 mov rsi,[rsp+48]
 call .Lbc_options
 test eax,eax
 jnz .Lbd_bad
 mov rdi,rbx
 mov rsi,[rsp+48]
 call .Lbp_options
 test eax,eax
 jnz .Lbd_bad
 xor ebp,ebp
 jmp .Lbd_end
.Lbd_bad:
 mov ebp,1
.Lbd_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lbd_blocks:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov [rsp+24],rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+32],0
 mov qword ptr [rsp+48],0
 xor r12d,r12d
.Lbd_block_each:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Lbd_blocks_ok
 mov rdi,rbx
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov r13,rax

 mov rdi,r13
 lea rsi,[rip+.Lbd_type]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 test rax,rax
 jz .Lbd_blocks_bad
 mov rdi,r14
 lea rsi,[rip+.Lbd_text]
 call strcmp@PLT
 test eax,eax
 jz .Lbd_block_text
 mov rdi,r14
 lea rsi,[rip+.Lbd_tool_use]
 call strcmp@PLT
 test eax,eax
 jz .Lbd_block_use
 mov rdi,r14
 lea rsi,[rip+.Lbd_tool_result]
 call strcmp@PLT
 test eax,eax
 jz .Lbd_block_result
 mov rdi,r14
 lea rsi,[rip+.Lbd_thinking]
 call strcmp@PLT
 test eax,eax
 jz .Lbd_block_thinking
 mov rdi,r14
 lea rsi,[rip+.Lbd_image]
 call strcmp@PLT
 test eax,eax
 jz .Lbd_block_image
 mov rdi,r14
 lea rsi,[rip+.Lbd_redacted_type]
 call strcmp@PLT
 test eax,eax
 jz .Lbd_block_redacted
 jmp .Lbd_blocks_bad
.Lbd_block_text:
 mov rdi,r13
 lea rsi,[rip+.Lbd_type]
 call json_object_object_del@PLT
 mov rdi,r13
 lea rsi,[rip+.Lbd_cache_control]
 call json_object_object_del@PLT
 jmp .Lbd_block_next
.Lbd_block_use:
 mov rdi,r13
 mov rsi,r13
 lea rdx,[rip+.Lbd_id]
 lea rcx,[rip+.Lbd_toolUseId]
 call .Lrp_copy
 test eax,eax
 jnz .Lbd_blocks_bad
 mov rdi,r13
 lea rsi,[rip+.Lbd_id]
 call json_object_object_del@PLT
 lea r14,[rip+.Lbd_toolUse]
 jmp .Lbd_block_wrap
.Lbd_block_result:
 mov rdi,r13
 lea rsi,[rip+.Lbd_content]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,[rsp+24]
 call .Lbd_blocks
 test eax,eax
 jnz .Lbd_blocks_bad
 mov rdi,r13
 mov rsi,r13
 lea rdx,[rip+.Lbd_tool_use_id]
 lea rcx,[rip+.Lbd_toolUseId]
 call .Lrp_copy
 test eax,eax
 jnz .Lbd_blocks_bad
 mov rdi,r13
 lea rsi,[rip+.Lbd_tool_use_id]
 call json_object_object_del@PLT
 mov rdi,r13
 lea rsi,[rip+.Lbd_toolName]
 call json_object_object_del@PLT
 mov rdi,r13
 lea rsi,[rip+.Lbd_is_error]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
 lea rdi,[rip+.Lbd_success]
 test eax,eax
 jz .Lbd_result_status
 lea rdi,[rip+.Lbd_error]
.Lbd_result_status:
 call json_object_new_string@PLT
 mov rdi,r13
 lea rsi,[rip+.Lbd_status]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lbd_blocks_bad
 mov rdi,r13
 lea rsi,[rip+.Lbd_is_error]
 call json_object_object_del@PLT
 lea r14,[rip+.Lbd_toolResult]
 jmp .Lbd_block_wrap
.Lbd_block_image:
 mov rdi,r13
 lea rsi,[rip+.Lbd_source]
 call json_object_object_get@PLT
 mov [rsp+16],rax
 mov rdi,rax
 lea rsi,[rip+.Lbd_media_type]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov rbp,rax
 test rax,rax
 jz .Lbd_blocks_bad
 mov rdi,rbp
 lea rsi,[rip+.Lbd_mime_image_jpeg]
 call strcmp@PLT
 lea r14,[rip+.Lbd_format_jpeg]
 test eax,eax
 jz .Lbd_image_format
 mov rdi,rbp
 lea rsi,[rip+.Lbd_mime_image_jpg]
 call strcmp@PLT
 lea r14,[rip+.Lbd_format_jpeg]
 test eax,eax
 jz .Lbd_image_format
 mov rdi,rbp
 lea rsi,[rip+.Lbd_mime_image_png]
 call strcmp@PLT
 lea r14,[rip+.Lbd_format_png]
 test eax,eax
 jz .Lbd_image_format
 mov rdi,rbp
 lea rsi,[rip+.Lbd_mime_image_gif]
 call strcmp@PLT
 lea r14,[rip+.Lbd_format_gif]
 test eax,eax
 jz .Lbd_image_format
 mov rdi,rbp
 lea rsi,[rip+.Lbd_mime_image_webp]
 call strcmp@PLT
 lea r14,[rip+.Lbd_format_webp]
 test eax,eax
 jz .Lbd_image_format
 jmp .Lbd_blocks_bad
.Lbd_image_format:
 call json_object_new_object@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lbd_blocks_bad
 mov rdi,r14
 call json_object_new_string@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lbd_format]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lbd_blocks_bad
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbd_blocks_bad
 mov rdi,rax
 mov rsi,[rsp+16]
 lea rdx,[rip+.Lbd_data]
 lea rcx,[rip+.Lbd_bytes]
 call .Lrp_copy
 test eax,eax
 jnz .Lbd_blocks_bad
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lbd_source]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrp_set
 test eax,eax
 jnz .Lbd_blocks_bad
 mov r13,[rsp+8]
 lea r14,[rip+.Lbd_image]
 jmp .Lbd_wrap_owned
.Lbd_block_redacted:
 mov rdi,r13
 lea rsi,[rip+.Lbd_data]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rsp+32]
 lea rdx,[rsp+40]
 call pg_mimo_base64_decode@PLT
 cmp eax,2
 je .Lbd_blocks_bad
 test eax,eax
 jnz .Lbd_drop_block
 cmp qword ptr [rsp+40],0
 je .Lbd_drop_block
 mov rdi,[rsp+32]
 mov rsi,[rsp+40]
 call pg_mimo_base64_encode@PLT
 mov [rsp+48],rax
 mov rdi,[rsp+32]
 call free@PLT
 mov qword ptr [rsp+32],0
 cmp qword ptr [rsp+48],0
 je .Lbd_blocks_bad
 call json_object_new_object@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lbd_blocks_bad
 mov rdi,rax
 lea rsi,[rip+.Lbd_redactedContent]
 mov rdx,[rsp+48]
 mov qword ptr [rsp+48],0
 call .Lrp_set
 test eax,eax
 jnz .Lbd_blocks_bad
 mov r13,[rsp+8]
 lea r14,[rip+.Lbd_reasoningContent]
 jmp .Lbd_wrap_owned
.Lbd_drop_block:
 mov rdi,[rsp+32]
 call free@PLT
 mov qword ptr [rsp+32],0
 mov rdi,rbx
 mov rsi,r12
 mov edx,1
 call json_object_array_del_idx@PLT
 test eax,eax
 jnz .Lbd_blocks_bad
 jmp .Lbd_block_each
.Lbd_block_thinking:
 mov rdi,r13
 lea rsi,[rip+.Lbd_thinking]
 call json_object_object_get@PLT
 mov [rsp+16],rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 mov ebp,eax
 mov rdi,[rsp+16]
 call json_object_get_string@PLT
 mov rdi,rax
 mov esi,ebp
 call pg_context_blank@PLT
 test eax,eax
 jnz .Lbd_drop_block
 mov rdi,r13
 mov rsi,r13
 lea rdx,[rip+.Lbd_thinking]
 lea rcx,[rip+.Lbd_text]
 call .Lrp_copy
 test eax,eax
 jnz .Lbd_blocks_bad
 mov rdi,r13
 lea rsi,[rip+.Lbd_thinking]
 call json_object_object_del@PLT
 mov rdi,[rsp+24]
 call .Lbd_claude
 test eax,eax
 jz .Lbd_thinking_unsigned
 mov rdi,r13
 lea rsi,[rip+.Lbd_signature]
 call json_object_object_get@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lbd_thinking_as_text
 mov rdi,rax
 call json_object_get_string_len@PLT
 mov ebp,eax
 mov rdi,[rsp+16]
 call json_object_get_string@PLT
 mov rdi,rax
 mov esi,ebp
 call pg_context_blank@PLT
 test eax,eax
 jnz .Lbd_thinking_as_text
 jmp .Lbd_thinking_wrap
.Lbd_thinking_unsigned:
 mov rdi,r13
 lea rsi,[rip+.Lbd_signature]
 call json_object_object_del@PLT
 jmp .Lbd_thinking_wrap
.Lbd_thinking_as_text:
 mov rdi,r13
 lea rsi,[rip+.Lbd_signature]
 call json_object_object_del@PLT
 jmp .Lbd_block_text
.Lbd_thinking_wrap:
 call json_object_new_object@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lbd_blocks_bad
 mov rdi,r13
 lea rsi,[rip+.Lbd_type]
 call json_object_object_del@PLT
 mov rdi,r13
 call json_object_get@PLT
 mov rdi,qword ptr [rsp+8]
 lea rsi,[rip+.Lbd_reasoningText]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lbd_blocks_bad
 mov r13,[rsp+8]
 lea r14,[rip+.Lbd_reasoningContent]
 jmp .Lbd_wrap_owned
.Lbd_block_wrap:
 mov rdi,r13
 lea rsi,[rip+.Lbd_type]
 call json_object_object_del@PLT
 mov rdi,r13
 lea rsi,[rip+.Lbd_cache_control]
 call json_object_object_del@PLT
 mov rdi,r13
 call json_object_get@PLT
 mov [rsp+8],rax
.Lbd_wrap_owned:
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbd_blocks_bad
 mov rdi,rax
 mov rsi,r14
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 call .Lrp_set
 test eax,eax
 jnz .Lbd_blocks_bad
 mov qword ptr [rsp+8],0
 mov rdi,rbx
 mov rsi,r12
 mov rdx,[rsp]
 call json_object_array_put_idx@PLT
 test eax,eax
 jnz .Lbd_blocks_bad
 mov qword ptr [rsp],0
.Lbd_block_next:
 inc r12
 jmp .Lbd_block_each
.Lbd_blocks_ok:
 xor ebp,ebp
 jmp .Lbd_blocks_end
.Lbd_blocks_bad:
 mov ebp,1
.Lbd_blocks_end:
 mov rdi,[rsp+32]
 call free@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lbd_cache_control: .asciz "cache_control"
.Lbd_content: .asciz "content"
.Lbd_error: .asciz "error"
.Lbd_id: .asciz "id"
.Lbd_inferenceConfig: .asciz "inferenceConfig"
.Lbd_inputSchema: .asciz "inputSchema"
.Lbd_input_schema: .asciz "input_schema"
.Lbd_is_error: .asciz "is_error"
.Lbd_json: .asciz "json"
.Lbd_maxTokens: .asciz "maxTokens"
.Lbd_max_tokens: .asciz "max_tokens"
.Lbd_messages: .asciz "messages"
.Lbd_model: .asciz "model"
.Lbd_reasoningContent: .asciz "reasoningContent"
.Lbd_reasoningText: .asciz "reasoningText"
.Lbd_status: .asciz "status"
.Lbd_stream: .asciz "stream"
.Lbd_success: .asciz "success"
.Lbd_system: .asciz "system"
.Lbd_temperature: .asciz "temperature"
.Lbd_text: .asciz "text"
.Lbd_thinking: .asciz "thinking"
.Lbd_toolConfig: .asciz "toolConfig"
.Lbd_toolName: .asciz "toolName"
.Lbd_toolResult: .asciz "toolResult"
.Lbd_toolSpec: .asciz "toolSpec"
.Lbd_toolUse: .asciz "toolUse"
.Lbd_toolUseId: .asciz "toolUseId"
.Lbd_tool_choice: .asciz "tool_choice"
.Lbd_tool_result: .asciz "tool_result"
.Lbd_tool_use: .asciz "tool_use"
.Lbd_tool_use_id: .asciz "tool_use_id"
.Lbd_tools: .asciz "tools"
.Lbd_topP: .asciz "topP"
.Lbd_top_p: .asciz "top_p"
.Lbd_type: .asciz "type"

.text
.Lbd_claude:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 lea rsi,[rip+.Lbd_model]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov r12,rax
 test rax,rax
 jz .Lbd_claude_name
 mov rdi,rax
 lea rsi,[rip+.Lbd_anthropic_dot]
 call strcasestr@PLT
 test rax,rax
 jnz .Lbd_claude_yes
 mov rdi,r12
 lea rsi,[rip+.Lbd_anthropic_slash]
 call strcasestr@PLT
 test rax,rax
 jnz .Lbd_claude_yes
.Lbd_claude_name:
 mov rdi,rbx
 lea rsi,[rip+.Lbd_modelInfo]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lbd_name]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lbd_claude_no
 mov rdi,rax
 lea rsi,[rip+.Lbd_claude_name_key]
 call strcasestr@PLT
 test rax,rax
 setne al
 movzx eax,al
 jmp .Lbd_claude_end
.Lbd_claude_yes:
 mov eax,1
 jmp .Lbd_claude_end
.Lbd_claude_no:
 xor eax,eax
.Lbd_claude_end:
 pop r13
 pop r12
 pop rbx
 ret
.section .rodata
.Lbd_image: .asciz "image"
.Lbd_source: .asciz "source"
.Lbd_bytes: .asciz "bytes"
.Lbd_data: .asciz "data"
.Lbd_format: .asciz "format"
.Lbd_media_type: .asciz "media_type"
.Lbd_signature: .asciz "signature"
.Lbd_modelInfo: .asciz "modelInfo"
.Lbd_anthropic_dot: .asciz "anthropic.claude"
.Lbd_anthropic_slash: .asciz "anthropic/claude"
.Lbd_claude_name_key: .asciz "claude"
.Lbd_mime_image_jpeg: .asciz "image/jpeg"
.Lbd_mime_image_jpg: .asciz "image/jpg"
.Lbd_mime_image_png: .asciz "image/png"
.Lbd_mime_image_gif: .asciz "image/gif"
.Lbd_mime_image_webp: .asciz "image/webp"
.Lbd_format_jpeg: .asciz "jpeg"
.Lbd_format_png: .asciz "png"
.Lbd_format_gif: .asciz "gif"
.Lbd_format_webp: .asciz "webp"

.Lbd_name: .asciz "name"

.macro BCGET obj,key
 mov rdi,\obj
 lea rsi,[rip+.LbcK_\key]
 call json_object_object_get@PLT
.endm
.macro BCSET obj,key,value
 mov rdi,\obj
 lea rsi,[rip+.LbcK_\key]
 mov rdx,\value
 call .Lrp_set
 test eax,eax
 jnz .Lbc_bad
.endm
.text
/* Flags are derived from upstream's normalized model ID and display-name candidates. */
.Lbc_family:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov dword ptr [rsp+24],0
 BCGET rbx,model
 mov r12,rax
 BCGET rbx,modelInfo
 BCGET rax,name
 mov r13,rax
 xor ebp,ebp
.Lbc_candidate:
 mov rdi,r12
 test ebp,ebp
 jz .Lbc_candidate_value
 mov rdi,r13
.Lbc_candidate_value:
 call json_object_get_string@PLT
 test rax,rax
 jz .Lbc_candidate_next
 mov rdi,rax
 call strdup@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbc_family_bad
 mov rdi,rax
 mov rsi,rax
 xor ecx,ecx
.Lbc_normalize:
 movzx edx,byte ptr [rsi]
 inc rsi
 test edx,edx
 jz .Lbc_normalized
 cmp edx,95
 je .Lbc_separator
 cmp edx,46
 je .Lbc_separator
 cmp edx,58
 je .Lbc_separator
 cmp edx,32
 je .Lbc_separator
 cmp edx,9
 jb .Lbc_copy_char
 cmp edx,13
 ja .Lbc_copy_char
.Lbc_separator:
 test ecx,ecx
 jnz .Lbc_normalize
 mov edx,45
 mov ecx,1
 jmp .Lbc_store_char
.Lbc_copy_char:
 xor ecx,ecx
.Lbc_store_char:
 mov [rdi],dl
 inc rdi
 jmp .Lbc_normalize
.Lbc_normalized:
 mov byte ptr [rdi],0
 lea r14,[rip+.Lbc_patterns]
.Lbc_pattern:
 movzx eax,byte ptr [r14]
 test eax,eax
 jz .Lbc_pattern_done
 mov [rsp+16],eax
 mov rdi,[rsp]
 lea rsi,[r14+1]
 call strcasestr@PLT
 test rax,rax
 jz .Lbc_pattern_next
 mov eax,[rsp+16]
 or [rsp+24],eax
.Lbc_pattern_next:
 lea rdi,[r14+1]
 call strlen@PLT
 lea r14,[r14+rax+2]
 jmp .Lbc_pattern
.Lbc_pattern_done:
 test ebp,ebp
 jnz .Lbc_candidate_free
 mov rdi,[rsp]
 lea rsi,[rip+.LbcK_gov_model]
 mov edx,7
 call strncasecmp@PLT
 test eax,eax
 jz .Lbc_model_gov
 mov rdi,[rsp]
 lea rsi,[rip+.LbcK_gov_arn]
 mov edx,15
 call strncasecmp@PLT
 test eax,eax
 jnz .Lbc_candidate_free
.Lbc_model_gov:
 or dword ptr [rsp+24],16
.Lbc_candidate_free:
 mov rdi,[rsp]
 call free@PLT
 mov qword ptr [rsp],0
.Lbc_candidate_next:
 inc ebp
 cmp ebp,2
 jb .Lbc_candidate
 BCGET rbx,options
 BCGET rax,region
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lbc_region_environment
 cmp byte ptr [rax],0
 jne .Lbc_region_check
.Lbc_region_environment:
 lea rdi,[rip+.LbcK_region_env]
 call getenv@PLT
 test rax,rax
 jz .Lbc_region_default
 cmp byte ptr [rax],0
 jne .Lbc_region_check
.Lbc_region_default:
 lea rdi,[rip+.LbcK_region_default_env]
 call getenv@PLT
 test rax,rax
 jz .Lbc_family_ok
.Lbc_region_check:
 mov rdi,rax
 lea rsi,[rip+.LbcK_gov_region]
 mov edx,7
 call strncasecmp@PLT
 test eax,eax
 jnz .Lbc_family_ok
 or dword ptr [rsp+24],16
.Lbc_family_ok:
 mov eax,[rsp+24]
 xor edx,edx
 jmp .Lbc_family_end
.Lbc_family_bad:
 xor eax,eax
 mov edx,1
.Lbc_family_end:
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lbc_options:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,96
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 BCGET r12,options
 mov [rsp+16],rax
 BCGET r12,modelInfo
 mov [rsp+24],rax
 BCGET rax,reasoning
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lbc_ok
 BCGET "qword ptr [rsp+16]",reasoning
 test rax,rax
 jnz .Lbc_level_value
 BCGET "qword ptr [rsp+16]",reasoningEffort
.Lbc_level_value:
 mov [rsp+32],rax
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lbc_ok
 mov rdi,rax
 lea rsi,[rip+.LbcK_off]
 call strcmp@PLT
 test eax,eax
 jz .Lbc_ok
 lea r14,[rip+.Lbc_levels]
 xor r13d,r13d
.Lbc_level:
 mov rdi,[rsp+40]
 mov rsi,r14
 call strcmp@PLT
 test eax,eax
 jz .Lbc_level_ready
 mov rdi,r14
 call strlen@PLT
 lea r14,[r14+rax+1]
 inc r13
 cmp r13,6
 jb .Lbc_level
 jmp .Lbc_bad
.Lbc_level_ready:
 mov [rsp+48],r13
 mov rdi,r12
 call .Lbc_family
 test edx,edx
 jnz .Lbc_bad
 mov [rsp+56],eax
 mov rdi,r12
 call .Lbd_claude
 test eax,eax
 jz .Lbc_openai
 test dword ptr [rsp+56],1
 jnz .Lbc_adaptive
 lea rdi,[rip+.LbcJ_budget]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbc_bad
 BCGET rax,thinking
 mov [rsp+8],rax
 lea rsi,[rip+.LbcK_high]
 cmp r13,4
 jae .Lbc_budget_key
 mov rsi,[rsp+40]
.Lbc_budget_key:
 mov [rsp+72],rsi
 BCGET "qword ptr [rsp+16]",thinkingBudgets
 mov rdi,rax
 mov rsi,[rsp+72]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lbc_budget_default
 mov rdi,rax
 call json_object_get@PLT
 jmp .Lbc_budget_set
.Lbc_budget_default:
 lea rax,[rip+.Lbc_budgets]
 mov rdi,[rax+r13*8]
 call json_object_new_int64@PLT
.Lbc_budget_set:
 BCSET "qword ptr [rsp+8]",budget_tokens,rax
 BCGET "qword ptr [rsp+16]",interleavedThinking
 test rax,rax
 jz .Lbc_interleaved
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lbc_display
.Lbc_interleaved:
 lea rdi,[rip+.LbcJ_interleaved]
 call json_tokener_parse@PLT
 BCSET "qword ptr [rsp]",anthropic_beta,rax
 jmp .Lbc_display
.Lbc_adaptive:
 lea rdi,[rip+.LbcJ_adaptive]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbc_bad
 BCGET rax,thinking
 mov [rsp+8],rax
 test dword ptr [rsp+56],2
 jz .Lbc_adaptive_effort
 test dword ptr [rsp+56],16
 jnz .Lbc_adaptive_effort
 lea rdi,[rip+.LbcJ_binding]
 call json_tokener_parse@PLT
 BCSET "qword ptr [rsp+8]",block_binding,rax
 lea rdi,[rip+.LbcJ_binding_beta]
 call json_tokener_parse@PLT
 BCSET "qword ptr [rsp]",anthropic_beta,rax
.Lbc_adaptive_effort:
 mov ebp,1
 cmp r13,4
 jne .Lbc_mapped_effort
 test dword ptr [rsp+56],2
 jz .Lbc_mapped_effort
 lea rdi,[rip+.LbcK_xhigh]
 jmp .Lbc_effort_string
.Lbc_mapped_effort:
 BCGET "qword ptr [rsp+24]",thinkingLevelMap
 mov rdi,rax
 mov rsi,[rsp+40]
 call json_object_object_get@PLT
 mov r14,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lbc_effort_default
 mov rdi,r14
 call json_object_get@PLT
 jmp .Lbc_effort_set
.Lbc_effort_default:
 lea rdi,[rip+.LbcK_low]
 cmp r13,2
 jb .Lbc_effort_string
 lea rdi,[rip+.LbcK_medium]
 je .Lbc_effort_string
 lea rdi,[rip+.LbcK_high]
 cmp ebp,1
 je .Lbc_effort_string
 cmp r13,4
 jne .Lbc_gpt_max
 lea rdi,[rip+.LbcK_xhigh]
 jmp .Lbc_effort_string
.Lbc_gpt_max:
 cmp r13,5
 jne .Lbc_effort_string
 lea rdi,[rip+.LbcK_max]
.Lbc_effort_string:
 call json_object_new_string@PLT
.Lbc_effort_set:
 mov r14,rax
 mov rax,[rsp]
 lea rsi,[rip+.LbcK_reasoning]
 cmp ebp,1
 jne .Lbc_effort_object
 lea rsi,[rip+.LbcK_output_config]
.Lbc_effort_object:
 mov rdi,rax
 call json_object_object_get@PLT
 BCSET rax,effort,r14
 cmp ebp,1
 jne .Lbc_adopt
.Lbc_display:
 test dword ptr [rsp+56],16
 jnz .Lbc_adopt
 BCGET "qword ptr [rsp+16]",thinkingDisplay
 test rax,rax
 jz .Lbc_display_default
 mov rdi,rax
 call json_object_get@PLT
 jmp .Lbc_display_set
.Lbc_display_default:
 lea rdi,[rip+.LbcK_summarized]
 call json_object_new_string@PLT
.Lbc_display_set:
 BCSET "qword ptr [rsp+8]",display,rax
 jmp .Lbc_adopt
.Lbc_openai:
 test dword ptr [rsp+56],4
 jnz .Lbc_oss
 test dword ptr [rsp+56],8
 jz .Lbc_ok
 lea rdi,[rip+.LbcJ_gpt]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbc_bad
 xor ebp,ebp
 jmp .Lbc_mapped_effort
.Lbc_oss:
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbc_bad
 lea rdi,[rip+.LbcK_low]
 cmp r13,2
 jb .Lbc_oss_effort
 lea rdi,[rip+.LbcK_medium]
 je .Lbc_oss_effort
 lea rdi,[rip+.LbcK_high]
.Lbc_oss_effort:
 call json_object_new_string@PLT
 BCSET "qword ptr [rsp]",reasoning_effort,rax
.Lbc_adopt:
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 BCSET rbx,additionalModelRequestFields,rdx
.Lbc_ok:
 xor ebp,ebp
 jmp .Lbc_end
.Lbc_bad:
 mov ebp,1
.Lbc_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,96
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lbc_patterns:
 .byte 1
 .asciz "opus-4-6"
 .byte 3
 .asciz "opus-4-7"
 .byte 3
 .asciz "opus-4-8"
 .byte 67
 .asciz "opus-5"
 .byte 1
 .asciz "sonnet-4-6"
 .byte 67
 .asciz "sonnet-5"
 .byte 67
 .asciz "haiku-5"
 .byte 67
 .asciz "fable-5"
 .byte 4
 .asciz "gpt-oss"
 .byte 8
 .asciz "gpt-"
 .byte 32
 .asciz "claude"
 .byte 64
 .asciz "-4-"
 .byte 64
 .asciz "claude-3-7-sonnet"
 .byte 64
 .asciz "claude-3-5-haiku"
 .byte 0
.Lbc_levels: .asciz "minimal","low","medium","high","xhigh","max"
.p2align 3
.Lbc_budgets: .quad 1024,2048,8192,16384,16384,16384
.LbcK_model: .asciz "model"
.LbcK_modelInfo: .asciz "modelInfo"
.LbcK_name: .asciz "name"
.LbcK_options: .asciz "options"
.LbcK_region: .asciz "region"
.LbcK_reasoning: .asciz "reasoning"
.LbcK_reasoningEffort: .asciz "reasoningEffort"
.LbcK_off: .asciz "off"
.LbcK_high: .asciz "high"
.LbcK_thinking: .asciz "thinking"
.LbcK_thinkingBudgets: .asciz "thinkingBudgets"
.LbcK_budget_tokens: .asciz "budget_tokens"
.LbcK_interleavedThinking: .asciz "interleavedThinking"
.LbcK_anthropic_beta: .asciz "anthropic_beta"
.LbcK_block_binding: .asciz "block_binding"
.LbcK_thinkingLevelMap: .asciz "thinkingLevelMap"
.LbcK_output_config: .asciz "output_config"
.LbcK_effort: .asciz "effort"
.LbcK_low: .asciz "low"
.LbcK_medium: .asciz "medium"
.LbcK_xhigh: .asciz "xhigh"
.LbcK_max: .asciz "max"
.LbcK_thinkingDisplay: .asciz "thinkingDisplay"
.LbcK_summarized: .asciz "summarized"
.LbcK_display: .asciz "display"
.LbcK_reasoning_effort: .asciz "reasoning_effort"
.LbcK_additionalModelRequestFields: .asciz "additionalModelRequestFields"
.LbcK_gov_model: .asciz "us-gov-"
.LbcK_gov_arn: .asciz "arn-aws-us-gov-"
.LbcK_gov_region: .asciz "us-gov-"
.LbcK_region_env: .asciz "AWS_REGION"
.LbcK_region_default_env: .asciz "AWS_DEFAULT_REGION"
.LbcJ_budget: .asciz "{\"thinking\":{\"type\":\"enabled\",\"budget_tokens\":0}}"
.LbcJ_adaptive: .asciz "{\"thinking\":{\"type\":\"adaptive\"},\"output_config\":{}}"
.LbcJ_gpt: .asciz "{\"reasoning\":{}}"
.LbcJ_binding: .asciz "{\"prefix_mismatch_behavior\":\"drop_block\"}"
.LbcJ_binding_beta: .asciz "[\"thinking-binding-controls-2026-08-01\"]"
.LbcJ_interleaved: .asciz "[\"interleaved-thinking-2025-05-14\"]"

.section .rodata
.Lbd_redacted_type: .asciz "redacted_thinking"
.Lbd_redactedContent: .asciz "redactedContent"

.macro BPGET obj,key
 mov rdi,\obj
 lea rsi,[rip+.LbpK_\key]
 call json_object_object_get@PLT
.endm
.macro BPSET obj,key,value
 mov rdi,\obj
 lea rsi,[rip+.LbpK_\key]
 mov rdx,\value
 call .Lrp_set
 test eax,eax
 jnz .Lbp_bad
.endm
.text
.Lbp_options:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 BPGET r12,options
 mov r13,rax
 BPGET rbx,toolConfig
 mov r14,rax
 test rax,rax
 jz .Lbp_metadata
 BPGET r13,toolChoice
 mov rbp,rax
 test rax,rax
 jz .Lbp_metadata
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jnz .Lbp_named
 mov rdi,rbp
 call json_object_get_string@PLT
 test rax,rax
 jz .Lbp_metadata
 mov rbp,rax
 mov rdi,rax
 lea rsi,[rip+.LbpK_none]
 call strcmp@PLT
 test eax,eax
 jz .Lbp_none
 mov rdi,rbp
 lea rsi,[rip+.LbpK_auto]
 call strcmp@PLT
 lea rdi,[rip+.LbpJ_auto]
 test eax,eax
 jz .Lbp_choice
 mov rdi,rbp
 lea rsi,[rip+.LbpK_any]
 call strcmp@PLT
 test eax,eax
 jnz .Lbp_metadata
 lea rdi,[rip+.LbpJ_any]
.Lbp_choice:
 call json_tokener_parse@PLT
 BPSET r14,toolChoice,rax
 jmp .Lbp_metadata
.Lbp_named:
 BPGET rbp,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lbp_metadata
 mov rdi,rax
 lea rsi,[rip+.LbpK_tool]
 call strcmp@PLT
 test eax,eax
 jnz .Lbp_metadata
 lea rdi,[rip+.LbpJ_named]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbp_bad
 BPGET rax,tool
 mov rdi,rax
 mov rsi,rbp
 lea rdx,[rip+.LbpK_name]
 mov rcx,rdx
 call .Lrp_copy
 test eax,eax
 jnz .Lbp_bad
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 BPSET r14,toolChoice,rdx
 jmp .Lbp_metadata
.Lbp_none:
 mov rdi,rbx
 lea rsi,[rip+.LbpK_toolConfig]
 call json_object_object_del@PLT
.Lbp_metadata:
 mov rdi,rbx
 mov rsi,r13
 lea rdx,[rip+.LbpK_requestMetadata]
 mov rcx,rdx
 call .Lrp_copy
 test eax,eax
 jnz .Lbp_bad
 BPGET r13,cacheRetention
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jnz .Lbp_retention
 lea rdi,[rip+.Lap_cache_env]
 call getenv@PLT
 test rax,rax
 jnz .Lbp_environment_retention
 lea rax,[rip+.LbpK_short]
 jmp .Lbp_retention
.Lbp_environment_retention:
 mov rdi,rax
 lea rsi,[rip+.LbpK_long]
 call strcmp@PLT
 lea rax,[rip+.LbpK_long]
 test eax,eax
 jz .Lbp_retention
 lea rax,[rip+.LbpK_short]
.Lbp_retention:
 mov [rsp+16],rax
 mov rdi,rax
 lea rsi,[rip+.LbpK_none]
 call strcmp@PLT
 test eax,eax
 jz .Lbp_ok
 mov rdi,r12
 call .Lbc_family
 test edx,edx
 jnz .Lbp_bad
 test eax,32
 jz .Lbp_force
 test eax,64
 jz .Lbp_ok
 jmp .Lbp_cache
.Lbp_force:
 lea rdi,[rip+.LbpK_force_env]
 call getenv@PLT
 test rax,rax
 jz .Lbp_ok
 cmp byte ptr [rax],49
 jne .Lbp_ok
 cmp byte ptr [rax+1],0
 jne .Lbp_ok
.Lbp_cache:
 lea r14,[rip+.LbpJ_short]
 mov rdi,[rsp+16]
 lea rsi,[rip+.LbpK_long]
 call strcmp@PLT
 test eax,eax
 jnz .Lbp_system_cache
 lea r14,[rip+.LbpJ_long]
.Lbp_system_cache:
 BPGET rbx,system
 mov [rsp+24],rax
 test rax,rax
 jz .Lbp_user_cache
 mov rdi,rax
 call json_object_array_length@PLT
 test rax,rax
 jz .Lbp_user_cache
 mov rdi,r14
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbp_bad
 mov rdi,[rsp+24]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lbp_bad
 mov qword ptr [rsp],0
.Lbp_user_cache:
 BPGET rbx,messages
 mov r12,rax
 mov rdi,rax
 call json_object_array_length@PLT
 test rax,rax
 jz .Lbp_ok
 mov rdi,r12
 lea rsi,[rax-1]
 call json_object_array_get_idx@PLT
 mov r12,rax
 BPGET rax,role
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lbp_ok
 mov rdi,rax
 lea rsi,[rip+.LbpK_user]
 call strcmp@PLT
 test eax,eax
 jnz .Lbp_ok
 BPGET r12,content
 mov r12,rax
 mov rdi,r14
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbp_bad
 mov rdi,r12
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lbp_bad
 mov qword ptr [rsp],0
.Lbp_ok:
 xor ebp,ebp
 jmp .Lbp_end
.Lbp_bad:
 mov ebp,1
.Lbp_end:
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
.section .rodata
.LbpK_options: .asciz "options"
.LbpK_toolChoice: .asciz "toolChoice"
.LbpK_toolConfig: .asciz "toolConfig"
.LbpK_none: .asciz "none"
.LbpK_auto: .asciz "auto"
.LbpK_any: .asciz "any"
.LbpK_tool: .asciz "tool"
.LbpK_type: .asciz "type"
.LbpK_name: .asciz "name"
.LbpK_requestMetadata: .asciz "requestMetadata"
.LbpK_cacheRetention: .asciz "cacheRetention"
.LbpK_short: .asciz "short"
.LbpK_long: .asciz "long"
.LbpK_system: .asciz "system"
.LbpK_messages: .asciz "messages"
.LbpK_role: .asciz "role"
.LbpK_user: .asciz "user"
.LbpK_content: .asciz "content"
.LbpK_force_env: .asciz "AWS_BEDROCK_FORCE_CACHE"
.LbpJ_auto: .asciz "{\"auto\":{}}"
.LbpJ_any: .asciz "{\"any\":{}}"
.LbpJ_named: .asciz "{\"tool\":{}}"
.LbpJ_short: .asciz "{\"cachePoint\":{\"type\":\"default\"}}"
.LbpJ_long: .asciz "{\"cachePoint\":{\"type\":\"default\",\"ttl\":\"1h\"}}"

.text
/* Normalize UTF-16 units exactly as the upstream non-Unicode regexp, capped at 64. */
.Lbi_normalize:
 push rbx
 sub rsp,80
 mov rbx,rdi
 call json_object_get_string_len@PLT
 mov [rsp+72],rax
 mov rdi,rbx
 call json_object_get_string@PLT
 mov rsi,rax
 mov r8,[rsp+72]
 add r8,rsi
 xor ecx,ecx
.Lbi_byte:
 cmp rsi,r8
 jae .Lbi_ready
 cmp ecx,64
 jae .Lbi_ready
 movzx eax,byte ptr [rsi]
 inc rsi
 cmp eax,128
 jae .Lbi_unicode
 cmp eax,65
 jb .Lbi_lower
 cmp eax,90
 jbe .Lbi_store
.Lbi_lower:
 cmp eax,97
 jb .Lbi_digit
 cmp eax,122
 jbe .Lbi_store
.Lbi_digit:
 cmp eax,48
 jb .Lbi_symbol
 cmp eax,57
 jbe .Lbi_store
.Lbi_symbol:
 cmp eax,45
 je .Lbi_store
 cmp eax,95
 je .Lbi_store
 mov eax,95
 jmp .Lbi_store
.Lbi_unicode:
 mov edx,1
 cmp eax,240
 jb .Lbi_skip_utf8
 mov edx,2
.Lbi_skip_utf8:
 cmp rsi,r8
 jae .Lbi_unicode_store
 mov al,[rsi]
 and al,0xc0
 cmp al,0x80
 jne .Lbi_unicode_store
 inc rsi
 jmp .Lbi_skip_utf8
.Lbi_unicode_store:
 mov byte ptr [rsp+rcx],95
 inc ecx
 dec edx
 jz .Lbi_byte
 cmp ecx,64
 jb .Lbi_unicode_store
 jmp .Lbi_ready
.Lbi_store:
 mov [rsp+rcx],al
 inc ecx
 jmp .Lbi_byte
.Lbi_ready:
 mov rdi,rsp
 mov esi,ecx
 call json_object_new_string_len@PLT
 add rsp,80
 pop rbx
 ret
/* First collect original-to-wire IDs, then apply the same map to result blocks. */
.Lbi_ids:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 xor eax,eax
 mov ecx,8
 mov rdi,rsp
 rep stosq
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbi_bad
 mov rdi,rbx
 lea rsi,[rip+.Lbd_messages]
 call json_object_object_get@PLT
 mov r12,rax
 xor r13d,r13d
.Lbi_message:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lbi_pass_done
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rip+.Lbd_content]
 call json_object_object_get@PLT
 mov [rsp+8],rax
 xor r14d,r14d
.Lbi_block:
 mov rdi,[rsp+8]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lbi_next_message
 mov rdi,[rsp+8]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rbx,rax
 mov rdi,rax
 lea rsi,[rip+.Lbi_foreign]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lbi_result
 cmp dword ptr [rsp+32],0
 jne .Lbi_next
 mov rdi,rbx
 lea rsi,[rip+.Lbi_foreign]
 call json_object_object_del@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lbd_id]
 call json_object_object_get@PLT
 mov [rsp+16],rax
 mov rdi,rax
 call .Lbi_normalize
 mov [rsp+24],rax
 test rax,rax
 jz .Lbi_bad
 mov rdi,[rsp+16]
 call json_object_get_string@PLT
 mov rbp,rax
 mov rdi,[rsp+24]
 call json_object_get@PLT
 mov rdi,[rsp]
 mov rsi,rbp
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lbi_bad
 mov rdx,[rsp+24]
 mov qword ptr [rsp+24],0
 mov rdi,rbx
 lea rsi,[rip+.Lbd_id]
 call .Lrp_set
 test eax,eax
 jnz .Lbi_bad
 jmp .Lbi_next
.Lbi_result:
 cmp dword ptr [rsp+32],0
 je .Lbi_next
 mov rdi,rbx
 lea rsi,[rip+.Lbd_tool_use_id]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lbi_next
 mov rdi,[rsp]
 mov rsi,rax
 call json_object_object_get@PLT
 test rax,rax
 jz .Lbi_next
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lbd_tool_use_id]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lbi_bad
.Lbi_next:
 inc r14
 jmp .Lbi_block
.Lbi_next_message:
 inc r13
 jmp .Lbi_message
.Lbi_pass_done:
 inc dword ptr [rsp+32]
 cmp dword ptr [rsp+32],2
 jae .Lbi_ok
 xor r13d,r13d
 jmp .Lbi_message
.Lbi_ok:
 xor ebp,ebp
 jmp .Lbi_end
.Lbi_bad:
 mov ebp,1
.Lbi_end:
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lbi_foreign: .asciz "panguForeignToolId"

.text
.Lcd_request:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsi
 call pg_mimo_config_codex@PLT
 test eax,eax
 jz .Lcd_request_ok
 mov rdi,rbx
 lea rsi,[rip+.Lcd_max]
 call json_object_object_del@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lcd_input]
 call json_object_object_get@PLT
 mov r13,rax
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lcd_role]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lcd_default_instructions
 mov rdi,rax
 lea rsi,[rip+.Lcd_system]
 call strcmp@PLT
 test eax,eax
 jnz .Lcd_default_instructions
 mov rdi,r14
 lea rsi,[rip+.Lcd_content]
 call json_object_object_get@PLT
 mov rbp,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lcd_instruction_value
 mov rdi,rbp
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcd_text]
 call json_object_object_get@PLT
 mov rbp,rax
.Lcd_instruction_value:
 mov rdi,rbp
 call json_object_get_string@PLT
 mov rbp,rax
 test rax,rax
 jz .Lcd_default_instructions
 cmp byte ptr [rax],0
 je .Lcd_default_instructions
 mov rdi,rbx
 lea rsi,[rip+.Lcd_instructions]
 mov rdx,rbp
 call li_field
 test eax,eax
 jnz .Lcd_request_end
 mov rdi,r13
 xor esi,esi
 mov edx,1
 call json_object_array_del_idx@PLT
 test eax,eax
 jnz .Lcd_request_bad
 jmp .Lcd_request_defaults
.Lcd_default_instructions:
 mov rdi,rbx
 lea rsi,[rip+.Lcd_instructions]
 lea rdx,[rip+.Lcd_default_prompt]
 call li_field
 test eax,eax
 jnz .Lcd_request_end
.Lcd_request_defaults:
 lea rdi,[rip+.Lcd_defaults]
 call json_tokener_parse@PLT
 mov rbp,rax
 test rax,rax
 jz .Lcd_request_bad
 xor r14d,r14d
.Lcd_default_each:
 cmp r14,2
 jne .Lcd_default_copy
 mov rdi,rbx
 lea rsi,[rip+.Lcd_choice]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lcd_default_next
.Lcd_default_copy:
 lea rax,[rip+.Lcd_keys]
 mov r13,[rax+r14*8]
 mov rdi,rbp
 mov rsi,r13
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,rbx
 mov rsi,r13
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lcd_defaults_bad
.Lcd_default_next:
 inc r14
 cmp r14,4
 jb .Lcd_default_each
 mov rdi,rbp
 call json_object_put@PLT
 mov rbp,r12
 mov rdi,r12
 lea rsi,[rip+.Lcd_options]
 call json_object_object_get@PLT
 mov r12,rax
 mov rdi,rbx
 mov rsi,r12
 lea rdx,[rip+.Lcd_service_option]
 lea rcx,[rip+.Lcd_service]
 call .Lrp_copy
 test eax,eax
 jnz .Lcd_request_bad
 mov rdi,rbx
 lea rsi,[rip+.Lcd_text]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,r12
 lea rdx,[rip+.Lcd_verbosity_option]
 lea rcx,[rip+.Lcd_verbosity]
 call .Lrp_copy
 test eax,eax
 jnz .Lcd_request_bad
 mov rdi,rbx
 lea rsi,[rip+.Lrp_reasoning]
 call json_object_object_get@PLT
 mov r13,rax
 test rax,rax
 jz .Lcd_default_reasoning
 mov rdi,rax
 lea rsi,[rip+.Lcd_summary]
 lea rdx,[rip+.Lcd_auto]
 call li_field
 test eax,eax
 jnz .Lcd_request_bad
 mov rdi,r13
 mov rsi,r12
 lea rdx,[rip+.Lcd_summary_option]
 lea rcx,[rip+.Lcd_summary]
 call .Lrp_copy
 jmp .Lcd_request_end
.Lcd_default_reasoning:
 mov rdi,rbp
 lea rsi,[rip+.Lcompat_model_info]
 call json_object_object_get@PLT
 mov rbp,rax
 mov rdi,rax
 lea rsi,[rip+.Lrp_reasoning]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lcd_request_ok
 mov rdi,rbp
 lea rsi,[rip+.Lct_level_map]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lct_off]
 lea rdx,[rsp]
 mov qword ptr [rsp],0
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lcd_default_reasoning_build
 cmp qword ptr [rsp],0
 je .Lcd_request_ok
.Lcd_default_reasoning_build:
 lea rdi,[rip+.Lcd_default_reason_json]
 call json_tokener_parse@PLT
 mov r13,rax
 test rax,rax
 jz .Lcd_request_bad
 mov rdi,rbx
 lea rsi,[rip+.Lrp_reasoning]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lcd_request_bad
 cmp qword ptr [rsp],0
 je .Lcd_request_ok
 mov rdi,[rsp]
 call json_object_get@PLT
 mov rdi,r13
 lea rsi,[rip+.Lrp_effort]
 mov rdx,rax
 call .Lrp_set
 jmp .Lcd_request_end
.Lcd_defaults_bad:
 mov rdi,rbp
 call json_object_put@PLT
.Lcd_request_bad:
 mov eax,1
 jmp .Lcd_request_end
.Lcd_request_ok:
 xor eax,eax
.Lcd_request_end:
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lcd_max: .asciz "max_output_tokens"
.Lcd_input: .asciz "input"
.Lcd_role: .asciz "role"
.Lcd_system: .asciz "system"
.Lcd_content: .asciz "content"
.Lcd_text: .asciz "text"
.Lcd_instructions: .asciz "instructions"
.Lcd_default_prompt: .asciz "You are a helpful assistant."
.Lcd_defaults: .asciz "{\"text\":{\"verbosity\":\"low\"},\"include\":[\"reasoning.encrypted_content\"],\"tool_choice\":\"auto\",\"parallel_tool_calls\":true}"
.Lcd_include: .asciz "include"
.Lcd_choice: .asciz "tool_choice"
.Lcd_parallel: .asciz "parallel_tool_calls"
.section .data.rel.ro
.Lcd_keys: .quad .Lcd_text,.Lcd_include,.Lcd_choice,.Lcd_parallel

.section .rodata
.Lcd_options: .asciz "options"
.Lcd_service_option: .asciz "serviceTier"
.Lcd_service: .asciz "service_tier"
.Lcd_verbosity_option: .asciz "textVerbosity"
.Lcd_verbosity: .asciz "verbosity"
.Lcd_summary_option: .asciz "reasoningSummary"
.Lcd_summary: .asciz "summary"
.Lcd_auto: .asciz "auto"

.section .rodata
.Lcd_default_reason_json: .asciz "{\"effort\":\"none\"}"

.text
/* Model defaults and Pi's context safety margin share the compaction estimator. */
.Lsb_room:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rdi
 mov r12,rdx
 mov rdi,rsi
 lea rsi,[rip+.Lcompat_model_info]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lsb_context]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_int64@PLT
 mov r13,rax
 test rax,rax
 jle .Lsb_room_unlimited
 cmp qword ptr [r12+PG_MM_SES_COUNT],1
 jne .Lsb_room_legacy
 cmp dword ptr [r12+PG_MM_MSG_ROLE],PG_MM_ROLE_CONTEXT_VIEW
 jne .Lsb_room_legacy
 mov rax,[r12+PG_MM_MSG_TEXT]
 jmp .Lsb_room_estimate
.Lsb_room_legacy:
 mov rdi,rbx
 lea rsi,[rip+.Lstr_messages]
 call json_object_object_get@PLT
.Lsb_room_estimate:
 mov rdi,rax
 call pg_context_estimate@PLT
 cmp r13,4096
 jle .Lsb_room_minimum
 sub r13,4096
 cmp rax,r13
 jae .Lsb_room_minimum
 mov rdx,r13
 sub rdx,rax
 mov rax,rdx
 jmp .Lsb_room_end
.Lsb_room_minimum:
 mov eax,1
 jmp .Lsb_room_end
.Lsb_room_unlimited:
 mov rax,-1
.Lsb_room_end:
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lsb_cap:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov [rsp],rdx
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
 mov rbp,rdi
.Lsb_cap_root:
 lea rax,[rip+.Lsb_root_keys]
 mov r14,[rax+r13*8]
 mov rdi,rbx
 mov rsi,r14
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lsb_cap_value
 inc r13
 cmp r13,3
 jb .Lsb_cap_root
 mov rdi,rbx
 lea rsi,[rip+.Lbd_inferenceConfig]
 call json_object_object_get@PLT
 mov rbp,rax
 mov rdi,rax
 lea r14,[rip+.Lbd_maxTokens]
 mov rsi,r14
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lsb_cap_value
 mov rdi,rbx
 lea rsi,[rip+.Lg_generationConfig]
 call json_object_object_get@PLT
 mov rbp,rax
 mov rdi,rax
 lea r14,[rip+.Lg_maxOutputTokens]
 mov rsi,r14
 call json_object_object_get@PLT
 test rax,rax
 jz .Lsb_cap_ok
.Lsb_cap_value:
 mov r13,rax
 lea rax,[rip+.Lrp_max_output_tokens]
 cmp r14,rax
 jne .Lsb_cap_number
 cmp r12,16
 jae .Lsb_cap_number
 mov r12d,16
.Lsb_cap_number:
 mov rdi,r13
 call json_object_get_uint64@PLT
 mov r13,rax
 cmp rax,r12
 jbe .Lsb_thinking_limit
 mov rdi,r12
 call json_object_new_uint64@PLT
 mov rdi,rbp
 mov rsi,r14
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lsb_cap_end
 mov r13,r12
.Lsb_thinking_limit:
 mov rdi,[rsp]
 call pg_mimo_config_anthropic@PLT
 test eax,eax
 jnz .Lsb_shared_thinking
 mov rdi,[rsp]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lsb_cap_ok
.Lsb_shared_thinking:
 mov rdi,[rsp]
 lea rsi,[rip+.Lcompat_model_info]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lopts_maximum_key]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lsb_cap_ok
 mov rdi,rbx
 lea rsi,[rip+.Lstr_thinking]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lsb_thinking_owner
 mov rdi,rbx
 lea rsi,[rip+.LbcK_additionalModelRequestFields]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lstr_thinking]
 call json_object_object_get@PLT
.Lsb_thinking_owner:
 mov rbp,rax
 mov rdi,rax
 lea rsi,[rip+.Lap_budget_tokens]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lsb_cap_ok
 mov rdi,rax
 call json_object_get_uint64@PLT
 xor edx,edx
 sub r13,1024
 cmovb r13,rdx
 cmp rax,r13
 jbe .Lsb_cap_ok
 mov rdi,r13
 call json_object_new_uint64@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lap_budget_tokens]
 mov rdx,rax
 call .Lrp_set
 jmp .Lsb_cap_end
.Lsb_cap_ok:
 xor eax,eax
.Lsb_cap_end:
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lsb_context: .asciz "contextWindow"
.section .data.rel.ro
.Lsb_root_keys: .quad .Lstr_max_tokens,.Lopts_completion_tokens,.Lrp_max_output_tokens

.text
.Lsb_bedrock_adjust:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rdi
 mov rbp,rsi
 mov rdi,rsi
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lsb_adjust_ok
 mov rdi,rbp
 lea rsi,[rip+.Lcompat_model_info]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lopts_maximum_key]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov r12,rax
 test rax,rax
 jz .Lsb_adjust_ok
 mov rdi,rbx
 lea rsi,[rip+.LbcK_additionalModelRequestFields]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lstr_thinking]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lap_budget_tokens]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lsb_adjust_ok
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov r14,rax
 mov rdi,rbx
 lea rsi,[rip+.Lbd_inferenceConfig]
 call json_object_object_get@PLT
 mov rbp,rax
 mov rdi,rax
 lea rsi,[rip+.Lbd_maxTokens]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 add rax,r14
 jc .Lsb_adjust_bad
 cmp rax,r12
 cmova rax,r12
 mov rdi,rax
 call json_object_new_uint64@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lbd_maxTokens]
 mov rdx,rax
 call .Lrp_set
 jmp .Lsb_adjust_end
.Lsb_adjust_ok:
 xor eax,eax
 jmp .Lsb_adjust_end
.Lsb_adjust_bad:
 mov eax,1
.Lsb_adjust_end:
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.section .rodata
.Lap_thinkingBudgets: .asciz "thinkingBudgets"
