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
 * [80]=next/prev/registry [88]=registry count [96]=leaf [104]=owner bits。
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
 sub rsp, 128
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
 mov qword ptr [rsp + 48], 0           /* i = 0 */
.Lmsg_loop:
 mov rax, qword ptr [rsp + 48]
 cmp rax, qword ptr [r12 + PG_MM_SES_COUNT]
 jae .Lmsg_done
 imul rax, rax, PG_MM_MSG_BYTES
 lea r13, [r12 + rax]                  /* p = &messages[i] */
 mov r14, r13                          /* text = p */
 mov rax, qword ptr [r13 + PG_MM_MSG_SIZE]
 cmp rax, PG_MM_TEXT_LIMIT
 jae .Linvalid
 mov eax, dword ptr [r13 + PG_MM_MSG_ROLE]
 cmp eax, PG_MM_ROLE_TOOL_RESULT       /* 无符号比较同时拒绝负值/越界 */
 ja .Linvalid
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
 cmp qword ptr [r13 + PG_MM_MSG_SIZE], 0
 jne .Lrole_dispatch
 jmp .Lmsg_next
.Lrole_dispatch:
 mov eax,dword ptr [r13 + PG_MM_MSG_ROLE]
 cmp eax, PG_MM_ROLE_TOOL_CALL
 jne .Lplain_message
 /* --- tool_call：id/name 有界非空，后随 tool_result 且 call_id 相等 --- */
 lea rdi, [r13 + PG_MM_MSG_CALL_ID]
 xor esi, esi
 mov edx, PG_MM_ID_LIMIT
 call memchr@PLT
 test rax, rax
 jz .Linvalid
 cmp byte ptr [r13 + PG_MM_MSG_CALL_ID], 0
 je .Linvalid
 lea rdi, [r13 + PG_MM_MSG_TOOL_NAME]
 xor esi, esi
 mov edx, PG_MM_ID_LIMIT
 call memchr@PLT
 test rax, rax
 jz .Linvalid
 cmp byte ptr [r13 + PG_MM_MSG_TOOL_NAME], 0
 je .Linvalid
 mov rax, qword ptr [rsp + 48]
 inc rax
 cmp rax, qword ptr [r12 + PG_MM_SES_COUNT]
 jae .Linvalid
 imul rcx, rax, PG_MM_MSG_BYTES
 lea rdx, [r12 + rcx]                  /* next = &messages[i+1] */
 mov qword ptr [rsp + 80], rdx
 cmp dword ptr [rdx + PG_MM_MSG_ROLE], PG_MM_ROLE_TOOL_RESULT
 jne .Linvalid
 lea rdi, [rdx + PG_MM_MSG_CALL_ID]
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
 /* tool_result：前一条必须是配对 tool_call */
 cmp qword ptr [rsp + 48], 0
 je .Linvalid
 mov rax, qword ptr [rsp + 48]
 dec rax
 imul rax, rax, PG_MM_MSG_BYTES
 lea rdx, [r12 + rax]                  /* prev */
 mov qword ptr [rsp + 80], rdx
 cmp dword ptr [rdx + PG_MM_MSG_ROLE], PG_MM_ROLE_TOOL_CALL
 jne .Linvalid
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
 cmp eax, PG_MM_ROLE_TOOL_RESULT
 je .Lbuild_tool
 mov rdi, r15
 lea rsi, [rip + .Lstr_role]
 lea rdx, [rip + .Lstr_assistant]
 call li_field
 test eax,eax
 jnz .Linvalid
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
 add rsp, 128
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_mimo_encode_request, .-pg_mimo_encode_request

.section .note.GNU-stack,"",@progbits
