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
 call .Lsampling_admit
 test eax,eax
 jnz .Linvalid
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
 call pg_mimo_uses_responses
 test eax,eax
 jz .Lserialize_ready
 mov rdi,[rsp]
 call .Lrp_request
 test eax,eax
 jnz .Linvalid
.Lserialize_ready:
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
 mov rdi,[rsp]
 call .Lap_error_images
 test eax,eax
 jnz .Lap_bad
 mov rdi,[rsp]
 call .Lap_normalize_messages
 test eax,eax
 jnz .Lap_bad
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
 test r14,r14
 jz .Lap_thinking_bad
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
 APGET rbx,strict
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lap_schema_legacy
 mov rdi,r12
 call json_object_get@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lap_input_schema]
 mov rdx,rax
 call .Lrp_set
 test eax,eax
 jnz .Lap_schema_bad
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
