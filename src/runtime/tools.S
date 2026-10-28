/* tools.S — 工具注册表与分派服务。
 *
 * 契约见 tools.h：初始化事务性（先全量验证再提交，失败不改原 registry）；
 * 16 项上限、名称字符集、重复拒绝、完整 JSON object schema 且 type=object；
 * 执行次序：查找 → 完整 JSON object 参数检查 → validate → authorize → execute，
 * 任一步失败不再触发后续回调；available 仅可见性查询，不构成授权。
 * json-c/libc 经 PLT 调用；pg_tools 聚合经 sret 返回。布局见 tools_layout.h。
 */
#include "tools_layout.h"
.intel_syntax noprefix

.section .rodata
.Lstr_type:       .asciz "type"
.Lstr_object_val: .asciz "object"
.Lmsg_unknown:    .asciz "unknown tool"
.Lmsg_bad_args:   .asciz "invalid JSON object arguments"
.Lmsg_validate:   .asciz "tool argument validation failed"
.Lmsg_authorize:  .asciz "tool permission denied"
.Lmsg_limit:      .asciz "tool output limit exceeded"
.Lmsg_busy: .asciz "tool operation is active"
.Lmsg_protocol: .asciz "invalid tool operation status"
.Lmsg_no_pending: .asciz "no pending tool operation"

.section .text

/* ---- int li_name_valid(const char *s)：1..63 个 [A-Za-z0-9_-] 字符 ---- */
.type li_name_valid,@function
li_name_valid:
 test rdi, rdi
 jz .Lnv_bad
 xor ecx, ecx
.Lnv_loop:
 cmp ecx, PG_TL_ID_LIMIT
 jae .Lnv_bad
 movzx eax, byte ptr [rdi + rcx]
 test al, al
 jz .Lnv_end
 cmp al, 'a'
 jb .Lnv_upper
 cmp al, 'z'
 jbe .Lnv_ok
.Lnv_upper:
 cmp al, 'A'
 jb .Lnv_digit
 cmp al, 'Z'
 jbe .Lnv_ok
.Lnv_digit:
 cmp al, '0'
 jb .Lnv_symbol
 cmp al, '9'
 jbe .Lnv_ok
.Lnv_symbol:
 cmp al, '_'
 je .Lnv_ok
 cmp al, '-'
 je .Lnv_ok
 jmp .Lnv_bad
.Lnv_ok:
 inc ecx
 jmp .Lnv_loop
.Lnv_end:
 test ecx, ecx
 jz .Lnv_bad
 mov eax, 1
 ret
.Lnv_bad:
 xor eax, eax
 ret
.size li_name_valid, .-li_name_valid

/* ---- json_object *li_parse_object(const char *data, size_t size)
 * 严格完整 JSON object：非空、长度上限、无嵌入 NUL、尾随空白外无残余。 ---- */
.type li_parse_object,@function
li_parse_object:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi /* data */
 mov r12, rsi /* size */
 test rbx, rbx
 jz .Lpo_null
 test r12, r12
 jz .Lpo_null
 cmp r12, PG_TL_TEXT_LIMIT
 jae .Lpo_null
 mov rdi, rbx
 xor esi, esi
 mov rdx, r12
 call memchr@PLT
 test rax, rax
 jnz .Lpo_null
 call json_tokener_new@PLT
 test rax, rax
 jz .Lpo_null
 mov r13, rax /* tok */
 mov rdi, r13
 mov rsi, rbx
 mov edx, r12d /* size < PG_TEXT_LIMIT，int 安全 */
 call json_tokener_parse_ex@PLT
 mov r14, rax /* o */
 mov rdi, r13
 call json_tokener_get_parse_end@PLT
 mov r15, rax /* end */
.Lpo_ws:
 cmp r15, r12
 jae .Lpo_ws_done
 movzx eax, byte ptr [rbx + r15]
 cmp al, ' '
 je .Lpo_ws_next
 cmp al, 9
 je .Lpo_ws_next
 cmp al, 10
 je .Lpo_ws_next
 cmp al, 13
 je .Lpo_ws_next
 jmp .Lpo_ws_done
.Lpo_ws_next:
 inc r15
 jmp .Lpo_ws
.Lpo_ws_done:
 mov rdi, r13
 call json_tokener_get_error@PLT
 test eax, eax
 jnz .Lpo_fail
 cmp r15, r12
 jne .Lpo_fail
 test r14, r14
 jz .Lpo_fail
 mov rdi, r14
 mov esi, PG_TL_JSON_OBJECT
 call json_object_is_type@PLT
 test eax, eax
 jz .Lpo_fail
 mov rdi, r13
 call json_tokener_free@PLT
 mov rax, r14
 jmp .Lpo_out
.Lpo_fail:
 mov rdi, r13
 call json_tokener_free@PLT
 test r14, r14
 jz .Lpo_null
 mov rdi, r14
 call json_object_put@PLT
.Lpo_null:
 xor eax, eax
.Lpo_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_parse_object, .-li_parse_object

/* ---- const pg_tool_definition *li_lookup(registry*, name.data, name.size) ---- */
.type li_lookup,@function
li_lookup:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov r14, rsi /* name.data */
 mov r15, rdx /* name.size */
 test rdi, rdi
 jz .Llk_null
 test r14, r14
 jz .Llk_null
 mov rbx, rdi /* registry 基址 */
 mov r13, qword ptr [rdi + PG_TL_REG_COUNT]
 xor r12d, r12d /* i */
.Llk_loop:
 cmp r12, r13
 jae .Llk_null
 mov rax, r12
 imul rax, rax, PG_TL_DEF_BYTES
 mov rdi, qword ptr [rbx + rax] /* entries[i].name */
 call strlen@PLT
 cmp rax, r15
 jne .Llk_next
 mov rax, r12
 imul rax, rax, PG_TL_DEF_BYTES
 mov rdi, r14
 mov rsi, qword ptr [rbx + rax]
 mov rdx, r15
 call memcmp@PLT
 test eax, eax
 jz .Llk_found
.Llk_next:
 inc r12
 jmp .Llk_loop
.Llk_found:
 mov rax, r12
 imul rax, rax, PG_TL_DEF_BYTES
 add rax, rbx
 jmp .Llk_out
.Llk_null:
 xor eax, eax
.Llk_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_lookup, .-li_lookup

/* ---- int li_error(out, cap, size*, msg)：写错误消息，恒返回 1 ---- */
.type li_error,@function
li_error:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov r14, rcx
 mov rdi, r14
 call strlen@PLT
 cmp rax, r12
 jae .Lerr_big
 mov r15, rax /* n：跨 memcpy 保存 */
 mov rdi, rbx
 mov rsi, r14
 mov rdx, r15
 call memcpy@PLT
 mov qword ptr [r13], r15
 mov eax, 1
 jmp .Lerr_out
.Lerr_big:
 mov qword ptr [r13], 0
 mov eax, 1
.Lerr_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_error, .-li_error

/* ---- int pg_tool_registry_init(registry*, defs*, count) → 0 成功
 * 栈帧：[rsp]=schema 对象 [rsp+8]=重复 j [rsp+16]=schema 串。 ---- */
.globl pg_tool_registry_init
.type pg_tool_registry_init,@function
pg_tool_registry_init:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 1216 /* scratch[32], detached registry[1184] */
 mov rbx, rdi /* registry，全程占用 */
 mov r12, rsi
 mov r13, rdx
 test rbx, rbx
 jz .Linit_bad
 cmp r13, PG_TL_LIMIT
 ja .Linit_bad
 test r13, r13
 jz .Linit_commit /* count==0 合法：提交空 registry */
 test r12, r12
 jz .Linit_bad
 xor r14d, r14d /* i */
.Linit_loop:
 cmp r14, r13
 jae .Linit_commit
 mov r15, r14
 imul r15, r15, PG_TL_DEF_BYTES
 add r15, r12 /* d = &defs[i] */
 mov rdi, qword ptr [r15 + PG_TL_DEF_NAME]
 call li_name_valid
 test eax, eax
 jz .Linit_bad
 mov rdi, qword ptr [r15 + PG_TL_DEF_DESCRIPTION]
 test rdi, rdi
 jz .Linit_bad
 call strlen@PLT
 cmp rax, PG_TL_TEXT_LIMIT
 jae .Linit_bad
 cmp qword ptr [r15 + PG_TL_DEF_SCHEMA], 0
 je .Linit_bad
 cmp qword ptr [r15 + PG_TL_DEF_VALIDATE], 0
 je .Linit_bad
 cmp qword ptr [r15 + PG_TL_DEF_AUTHORIZE], 0
 je .Linit_bad
 cmp dword ptr [r15 + PG_TL_DEF_REPLAY_SAFE],1
 ja .Linit_bad
 cmp qword ptr [r15 + PG_TL_DEF_EXECUTE], 0
 je .Linit_bad
 mov rax,[r15+PG_TL_DEF_ASYNC]
 test rax,rax
 jz .Linit_async_valid
 cmp qword ptr [rax],0
 je .Linit_bad
 cmp qword ptr [rax+8],0
 je .Linit_bad
 cmp qword ptr [rax+16],0
 je .Linit_bad
.Linit_async_valid:
 mov rax, qword ptr [r15 + PG_TL_DEF_SCHEMA]
 mov [rsp+16], rax
 mov rdi, rax
 call strlen@PLT
 mov rdi, [rsp+16]
 mov rsi, rax
 call li_parse_object
 test rax, rax
 jz .Linit_bad
 mov [rsp], rax
 mov qword ptr [rsp+8], 0 /* json_object *type = 0 */
 mov rdi, [rsp]
 lea rsi, [rip + .Lstr_type]
 lea rdx, [rsp+8]
 call json_object_object_get_ex@PLT
 mov rax, [rsp+8]
 test rax, rax
 jz .Linit_schema_bad
 mov rdi, rax
 mov esi, PG_TL_JSON_STRING
 call json_object_is_type@PLT
 test eax, eax
 jz .Linit_schema_bad
 mov rdi, [rsp+8]
 call json_object_get_string_len@PLT
 cmp eax,6
 jne .Linit_schema_bad
 mov rdi, [rsp+8]
 call json_object_get_string@PLT
 mov rdi, rax
 lea rsi, [rip + .Lstr_object_val]
 call strcmp@PLT
 test eax, eax
 jnz .Linit_schema_bad
 mov rdi, [rsp]
 call json_object_put@PLT
 /* 重复名检查：j < i 时 strcmp(name_i, name_j)==0 → 拒绝 */
 mov qword ptr [rsp], 0
.Linit_dup:
 mov rax, [rsp]
 cmp rax, r14
 jae .Linit_next
 mov rcx, rax
 imul rcx, rcx, PG_TL_DEF_BYTES
 mov rdi, qword ptr [r15 + PG_TL_DEF_NAME]
 mov rsi, qword ptr [r12 + rcx]
 call strcmp@PLT
 test eax, eax
 jz .Linit_bad
 inc qword ptr [rsp]
 jmp .Linit_dup
.Linit_schema_bad:
 mov rdi, [rsp]
 call json_object_put@PLT
 jmp .Linit_bad
.Linit_next:
 inc r14
 jmp .Linit_loop
.Linit_commit:
 /* 在独立栈缓冲构造完整 registry 后发布，允许 defs 与目标重叠。 */
 lea rdi, [rsp + 32]
 mov ecx, PG_TL_REG_BYTES/8
 xor eax, eax
 cld
 rep stosq /* candidate = {0}：串指令，无调用 */
 test r13, r13
 jz .Linit_no_entries
 lea rdi, [rsp + 32]
 mov rsi, r12
 mov rdx, r13
 imul rdx, rdx, PG_TL_DEF_BYTES
 call memcpy@PLT
.Linit_no_entries:
 mov qword ptr [rsp + 32 + PG_TL_REG_COUNT], r13
 mov rdi, rbx /* 发布：*r = candidate */
 lea rsi, [rsp + 32]
 mov edx, PG_TL_REG_BYTES
 call memcpy@PLT
 xor eax, eax
 add rsp, 1216
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Linit_bad:
 mov eax, 1
 add rsp, 1216
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_tool_registry_init, .-pg_tool_registry_init

/* ---- pg_tools pg_tool_registry_service(pg_tool_registry *r)
 * 32 字节聚合：sret，rdi=目标缓冲 rsi=r。 ---- */
.globl pg_tool_registry_service
.type pg_tool_registry_service,@function
pg_tool_registry_service:
 mov qword ptr [rdi + PG_TL_SVC_CONTEXT], rsi
 lea rax, [rip + li_available]
 mov qword ptr [rdi + PG_TL_SVC_AUTHORIZE], rax
 lea rax, [rip + li_execute]
 mov qword ptr [rdi + PG_TL_SVC_EXECUTE], rax
 lea rax,[rip+li_replay_safe]
 mov [rdi+PG_TL_SVC_REPLAY_SAFE],rax
 lea rax,[rip+li_next]
 mov [rdi+PG_TL_SVC_NEXT],rax
 lea rax,[rip+li_cancel]
 mov [rdi+PG_TL_SVC_CANCEL],rax
 lea rax,[rip+li_close]
 mov [rdi+PG_TL_SVC_CLOSE],rax
 mov rax, rdi
 ret
.size pg_tool_registry_service, .-pg_tool_registry_service

/* Pure lookup; absent/invalid policy is unsafe. */
.type li_replay_safe,@function
li_replay_safe:
 sub rsp,8
 call li_lookup
 add rsp,8
 test rax,rax
 jz .Lreplay_absent
 cmp dword ptr [rax+PG_TL_DEF_REPLAY_SAFE],1
 sete al
 movzx eax,al
 ret
.Lreplay_absent:
 xor eax,eax
 ret
.size li_replay_safe,.-li_replay_safe

/* ---- int li_available(ctx, name.data, name.size, args.data, args.size)
 * 可见性查询：仅查找存在，不构成授权。 ---- */
.type li_available,@function
li_available:
 push rax /* 16 字节对齐（入口 rsp≡8） */
 call li_lookup
 pop rcx
 test rax, rax
 setne al
 movzx eax, al
 ret
.size li_available, .-li_available

/* ---- int li_execute(ctx, name.data, name.size, args.data, args.size,
 *                      out, cap=[ caller rsp+8 ], size*=[ caller rsp+16 ]) ---- */
.type li_execute,@function
li_execute:
 mov r10, [rsp+8] /* cap */
 mov r11, [rsp+16] /* size */
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 48 /* [rsp]=args.data [rsp+8]=args.size [rsp+16]=name.data [rsp+24]=name.size */
 mov rbx, rdi /* registry */
 mov r12, r9 /* out */
 mov r13, r10 /* cap */
 mov r14, r11 /* size */
 mov [rsp], rcx
 mov [rsp+8], r8
 mov [rsp+16], rsi
 mov [rsp+24], rdx
 test r12, r12
 jz .Lexec_bad_direct
 test r14, r14
 jz .Lexec_bad_direct
 mov qword ptr [r14], 0
 cmp qword ptr [rbx+PG_TL_REG_ACTIVE],0
 jne .Lexec_busy
 mov rdi, rbx
 mov rsi, [rsp+16]
 mov rdx, [rsp+24]
 call li_lookup
 mov r15, rax
 test r15, r15
 jz .Lexec_unknown
 mov rdi, [rsp]
 mov rsi, [rsp+8]
 call li_parse_object
 test rax, rax
 jz .Lexec_bad_args
 mov rdi, rax
 call json_object_put@PLT
 mov rdi, qword ptr [r15 + PG_TL_DEF_CONTEXT]
 mov rsi, [rsp]
 mov rdx, [rsp+8]
 call [r15 + PG_TL_DEF_VALIDATE]
 test eax, eax
 jz .Lexec_validate_failed
 mov rdi, qword ptr [r15 + PG_TL_DEF_CONTEXT]
 mov rsi, [rsp]
 mov rdx, [rsp+8]
 call [r15 + PG_TL_DEF_AUTHORIZE]
 test eax, eax
 jz .Lexec_denied
 cmp qword ptr [r15+PG_TL_DEF_ASYNC],0
 je .Lexec_sync
 cmp dword ptr [rbx+PG_TL_REG_DEPTH],0
 jne .Lexec_busy
 mov [rbx+PG_TL_REG_ACTIVE],r15
 mov dword ptr [rbx+PG_TL_REG_STATE],1
 mov dword ptr [rbx+PG_TL_REG_CANCEL],0
 jmp .Lexec_invoke
.Lexec_sync:
 inc dword ptr [rbx+PG_TL_REG_DEPTH]
.Lexec_invoke:
 mov rdi, qword ptr [r15 + PG_TL_DEF_CONTEXT]
 mov rsi, [rsp]
 mov rdx, [rsp+8]
 mov rcx, r12
 mov r8, r13
 mov r9, r14
 call [r15 + PG_TL_DEF_EXECUTE]
 cmp qword ptr [r15+PG_TL_DEF_ASYNC],0
 jne .Lexec_check_result
 dec dword ptr [rbx+PG_TL_REG_DEPTH]
.Lexec_check_result:
 call li_operation_result
 jmp .Lexec_out
.Lexec_bad_direct:
 mov eax, 1
 jmp .Lexec_out
.Lexec_busy:
 mov rdi,r12
 mov rsi,r13
 mov rdx,r14
 lea rcx,[rip+.Lmsg_busy]
 call li_error
 jmp .Lexec_out
.Lexec_unknown:
 mov rdi, r12
 mov rsi, r13
 mov rdx, r14
 lea rcx, [rip + .Lmsg_unknown]
 call li_error
 jmp .Lexec_out
.Lexec_bad_args:
 mov rdi, r12
 mov rsi, r13
 mov rdx, r14
 lea rcx, [rip + .Lmsg_bad_args]
 call li_error
 jmp .Lexec_out
.Lexec_validate_failed:
 mov rdi, r12
 mov rsi, r13
 mov rdx, r14
 lea rcx, [rip + .Lmsg_validate]
 call li_error
 jmp .Lexec_out
.Lexec_denied:
 mov rdi, r12
 mov rsi, r13
 mov rdx, r14
 lea rcx, [rip + .Lmsg_authorize]
 call li_error
 jmp .Lexec_out
.Lexec_limit:
 mov rdi, r12
 mov rsi, r13
 mov rdx, r14
 lea rcx, [rip + .Lmsg_limit]
 call li_error
.Lexec_out:
 add rsp, 48
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_execute, .-li_execute


/* All callers share result validation; active ownership survives until close=0. */
li_operation_result:
 push r15
 mov r15d,eax
 cmp qword ptr [r14],r13
 jae .Lop_limit
 cmp r15d,4
 je .Lop_final
 cmp r15d,1
 jbe .Lop_final
 cmp r15d,3
 ja .Lop_invalid
 cmp qword ptr [rbx+PG_TL_REG_ACTIVE],0
 je .Lop_invalid
 cmp r15d,2
 jne .Lop_return
 cmp qword ptr [r14],0
 jne .Lop_invalid
 jmp .Lop_return
.Lop_final:
 cmp qword ptr [rbx+PG_TL_REG_ACTIVE],0
 je .Lop_return
 mov dword ptr [rbx+PG_TL_REG_STATE],2
.Lop_return:
 mov eax,r15d
 pop r15
 ret
.Lop_limit:
 lea r15,[rip+.Lmsg_limit]
 jmp .Lop_fail
.Lop_invalid:
 lea r15,[rip+.Lmsg_protocol]
.Lop_fail:
 cmp qword ptr [rbx+PG_TL_REG_ACTIVE],0
 je .Lop_message
 mov dword ptr [rbx+PG_TL_REG_STATE],2
 mov rdi,rbx
 call li_cancel
.Lop_message:
 mov rdi,r12
 mov rsi,r13
 mov rdx,r14
 mov rcx,r15
 call li_error
 pop r15
 ret

li_next:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 test r12,r12
 jz .Lnext_bad
 test r14,r14
 jz .Lnext_bad
 mov qword ptr [r14],0
 mov r15,[rbx+PG_TL_REG_ACTIVE]
 test r15,r15
 jz .Lnext_empty
 cmp dword ptr [rbx+PG_TL_REG_STATE],1
 jne .Lnext_empty
 mov rax,[r15+PG_TL_DEF_ASYNC]
 mov rdi,[r15+PG_TL_DEF_CONTEXT]
 mov rsi,r12
 mov rdx,r13
 mov rcx,r14
 call [rax]
 call li_operation_result
 jmp .Lnext_return
.Lnext_empty:
 mov rdi,r12
 mov rsi,r13
 mov rdx,r14
 lea rcx,[rip+.Lmsg_no_pending]
 call li_error
 jmp .Lnext_return
.Lnext_bad:
 mov eax,1
.Lnext_return:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret

li_cancel:
 mov rax,[rdi+PG_TL_REG_ACTIVE]
 test rax,rax
 jz .Lcancel_return
 cmp dword ptr [rdi+PG_TL_REG_CANCEL],0
 jne .Lcancel_return
 mov dword ptr [rdi+PG_TL_REG_CANCEL],1
 mov rcx,[rax+PG_TL_DEF_ASYNC]
 mov rdi,[rax+PG_TL_DEF_CONTEXT]
 jmp [rcx+8]
.Lcancel_return:
 ret

li_close:
 push rbx
 mov rbx,rdi
 mov rax,[rbx+PG_TL_REG_ACTIVE]
 test rax,rax
 jz .Lclose_idle
 mov dword ptr [rbx+PG_TL_REG_STATE],3
 mov rcx,[rax+PG_TL_DEF_ASYNC]
 mov rdi,[rax+PG_TL_DEF_CONTEXT]
 call [rcx+16]
 test eax,eax
 jz .Lclose_clear
 cmp eax,1
 jle .Lclose_return
 mov eax,-1
 jmp .Lclose_return
.Lclose_clear:
 mov qword ptr [rbx+PG_TL_REG_ACTIVE],0
 mov qword ptr [rbx+PG_TL_REG_STATE],0
.Lclose_idle:
 xor eax,eax
.Lclose_return:
 pop rbx
 ret

.section .note.GNU-stack,"",@progbits

.text
.globl pg_tool_registry_check_json
.type pg_tool_registry_check_json,@function
pg_tool_registry_check_json:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rsi
 mov r12,rdi
 mov rdi,rbx
 lea rsi,[rip+.Lcheck_name]
 call json_object_object_get@PLT
 mov r13,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lcheck_missing
 mov rdi,r13
 call json_object_get_string@PLT
 mov rbp,rax
 mov rdi,r13
 call json_object_get_string_len@PLT
 mov rdi,r12
 mov rsi,rbp
 mov edx,eax
 call li_lookup
 mov r14,rax
 test rax,rax
 jz .Lcheck_missing
 mov rdi,rbx
 lea rsi,[rip+.Lcheck_arguments]
 call json_object_object_get@PLT
 mov r13,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lcheck_invalid
 mov rdi,r13
 mov esi,256
 call json_object_to_json_string_ext@PLT
 mov rbp,rax
 test rax,rax
 jz .Lcheck_invalid
 mov rdi,rax
 call strlen@PLT
 mov r13,rax
 mov rdi,rbp
 mov rsi,r13
 call li_parse_object
 test rax,rax
 jz .Lcheck_invalid
 mov rdi,rax
 call json_object_put@PLT
 mov rdi,[r14+PG_TL_DEF_CONTEXT]
 mov rsi,rbp
 mov rdx,r13
 call [r14+PG_TL_DEF_VALIDATE]
 test eax,eax
 setnz al
 movzx eax,al
 jmp .Lcheck_end
.Lcheck_missing:
 mov eax,-1
 jmp .Lcheck_end
.Lcheck_invalid:
 xor eax,eax
.Lcheck_end:
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lcheck_name: .asciz "name"
.Lcheck_arguments: .asciz "arguments"
