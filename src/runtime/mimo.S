/* mimo.S — MiMo provider 生命周期：model/bind_tools/start/next/cancel/close。
 *
 * 适配器只做投影、传输与协议事件，不持有 Session 提交/恢复/工具执行（契约见
 * agent.h pg_model 与 Pi durable generation 的宿主边界）。start 复用 MIMO_API_KEY/
 * MIMO_MODEL 环境提供默认连接；持久请求配置优先决定模型/端点与请求选项；easy/multi/headers/
 * request 为本实例所有，cancel/close 全量释放，重复 close 幂等，半初始化可安全关闭。
 * next 是非阻塞推进：multi_perform 轮询→HTTP 状态→行拆分（跨接收块换行、CRLF、
 * 未完成末行）→decode_line；transport 完成后按 [DONE]+finish 出终态，工具参数须
 * 是完整 JSON object。curl_easy_setopt/getinfo 为变参：al=0，变参走 rdx。
 * WRITE 回调 receive 做乘法溢出与 body 容量检查，拒绝即向 curl 返回 0 触发传输
 * 错误。布局见 mimo_layout.h。
 */
#include "mimo_layout.h"
.intel_syntax noprefix

.section .rodata
.Lstr_endpoint:     .asciz "https://token-plan-cn.xiaomimimo.com/v1/chat/completions"
.Lstr_https:        .asciz "https"
.Lstr_content_type: .asciz "Content-Type: application/json"
.Lstr_auth_prefix:  .asciz "Authorization: Bearer "
.Lstr_pangu_key: .asciz "PANGU_MODEL_API_KEY"
.Lstr_pangu_endpoint: .asciz "PANGU_MODEL_ENDPOINT"
.Lstr_env_key:      .asciz "MIMO_API_KEY"

.section .text

/* ---- pg_model pg_mimo_model(pg_mimo *m)：清零实例并返回函数表（sret）。 ---- */
.globl pg_mimo_model
.type pg_mimo_model,@function
pg_mimo_model:
 push rdi                             /* 保存 sret 目标；本函数无外部调用 */
 mov rdi, rsi
 xor eax, eax
 mov ecx, PG_MM_BYTES/8
 cld
 rep stosq                           /* *m = {0} */
 pop rdi
 mov qword ptr [rdi + PG_MM_MODEL_CONTEXT], rsi
 lea rax, [rip + li_start]
 mov qword ptr [rdi + PG_MM_MODEL_START], rax
 lea rax, [rip + li_next]
 mov qword ptr [rdi + PG_MM_MODEL_NEXT], rax
 lea rax, [rip + li_cancel]
 mov qword ptr [rdi + PG_MM_MODEL_CANCEL], rax
 lea rax, [rip + li_close]
 mov qword ptr [rdi + PG_MM_MODEL_CLOSE], rax
 mov rax, rdi
 ret
.size pg_mimo_model, .-pg_mimo_model

/* ---- void pg_mimo_bind_tools(pg_mimo *m, const pg_tool_registry *tools) ---- */
.globl pg_mimo_bind_tools
.type pg_mimo_bind_tools,@function
pg_mimo_bind_tools:
 mov qword ptr [rdi + PG_MM_TOOLS], rsi
 ret
.size pg_mimo_bind_tools, .-pg_mimo_bind_tools

.type .Lclear_argument_buffers,@function
.Lclear_argument_buffers:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 call pg_mimo_anthropic_clear
 xor r12d,r12d
.Lclear_arg_loop:
 mov rdi,[rbx+PG_MM_ARGUMENT_OVERFLOW+r12*8]
 mov qword ptr [rbx+PG_MM_ARGUMENT_OVERFLOW+r12*8],0
 mov qword ptr [rbx+PG_MM_ARGUMENT_LENGTH+r12*8],0
 mov qword ptr [rbx+PG_MM_ARGUMENT_CAPACITY+r12*8],0
 call free@PLT
 mov rdi,[rbx+PG_MM_PREPARED_ARGUMENTS+r12*8]
 mov qword ptr [rbx+PG_MM_PREPARED_ARGUMENTS+r12*8],0
 call json_object_put@PLT
 inc r12d
 cmp r12d,PG_MM_CALL_LIMIT
 jb .Lclear_arg_loop
 mov rdi,[rbx+PG_MM_RESPONSE_OUTPUT]
 mov qword ptr [rbx+PG_MM_RESPONSE_OUTPUT],0
 call json_object_put@PLT
 mov rdi,[rbx+PG_MM_RESPONSE_ID]
 mov qword ptr [rbx+PG_MM_RESPONSE_ID],0
 call json_object_put@PLT
 mov rdi,[rbx+PG_MM_RESPONSE_TERMINAL]
 mov qword ptr [rbx+PG_MM_RESPONSE_TERMINAL],0
 mov dword ptr [rbx+PG_MM_RESPONSE_CLOSED],0
 call json_object_put@PLT
 /* Preserve the request metadata and registry between the two buffers. */
 lea rdi,[rbx+PG_MM_ARGUMENTS]
 xor esi,esi
 mov edx,PG_MM_USED-PG_MM_ARGUMENTS
 call memset@PLT
 lea rdi,[rbx+PG_MM_EXTRA]
 xor esi,esi
 mov edx,PG_MM_USAGE-PG_MM_EXTRA
 call memset@PLT
 add rsp,8
 pop r12
 pop rbx
 ret
.size .Lclear_argument_buffers,.-.Lclear_argument_buffers

/* ---- void li_close(void *ctx)：释放 easy/multi/headers/request 后置空。
 * 依赖 curl_slist_free_all/free 的 NULL 安全；重复调用幂等。文件内私有。 */
.type li_close,@function
li_close:
 push rbx
 mov rbx,rdi
 call .Ltransport_close
 mov rdi,rbx
 call .Lclear_argument_buffers
 mov rdi,[rbx+PG_MM_USAGE]
 mov qword ptr [rbx+PG_MM_USAGE],0
 call json_object_put@PLT
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 mov qword ptr [rbx+PG_MM_CONFIGURATION],0
 call json_object_put@PLT
 pop rbx
 ret
.Ltransport_close:
 push rbx                             /* 入口 rsp≡8 → 0，可调用 */
 mov rbx, rdi
 mov rdi, qword ptr [rbx + PG_MM_MULTI]
 mov rsi, qword ptr [rbx + PG_MM_EASY]
 test rdi, rdi
 jz 1f
 test rsi, rsi
 jz 1f
 call curl_multi_remove_handle@PLT
1:
 mov rdi, qword ptr [rbx + PG_MM_EASY]
 test rdi, rdi
 jz 1f
 call curl_easy_cleanup@PLT
1:
 mov rdi, qword ptr [rbx + PG_MM_MULTI]
 test rdi, rdi
 jz 1f
 call curl_multi_cleanup@PLT
1:
 mov rdi, qword ptr [rbx + PG_MM_HEADERS]
 call curl_slist_free_all@PLT
 mov rdi, qword ptr [rbx + PG_MM_REQUEST]
 call free@PLT
 mov qword ptr [rbx + PG_MM_EASY], 0
 mov qword ptr [rbx + PG_MM_MULTI], 0
 mov qword ptr [rbx + PG_MM_HEADERS], 0
 mov qword ptr [rbx + PG_MM_REQUEST], 0
 pop rbx
 ret
.size li_close, .-li_close

.type li_cancel,@function
li_cancel:
 jmp li_close                         /* 尾跳：入口栈型一致 */
.size li_cancel, .-li_cancel

/* ---- size_t li_receive(char *p, size_t size, size_t nmemb, void *ctx)
 * curl WRITE 回调：乘法溢出/容量不足返回 0（curl 判为写错误），成功整块拷贝。 */
.type li_receive,@function
li_receive:
 push rbx
 push r12
 push r13
 push r14
 push r15                             /* 入口 rsp≡8；5 压栈后 ≡0 */
 mov rbx, rcx                         /* m */
 mov r12, rdi                         /* p */
 xor r14d, r14d                       /* n = 0 */
 test rsi, rsi
 jz .Lrx_check
 mov r13, rdx                         /* count */
 mov rax, -1                          /* SIZE_MAX */
 xor edx, edx
 div rsi                              /* rax = SIZE_MAX/size */
 cmp r13, rax
 ja .Lrx_zero                         /* size*count 溢出 */
 mov r14, rsi
 imul r14, r13                        /* n = size*count */
.Lrx_check:
 mov rax,[rbx+PG_MM_USED]
 mov rcx,PG_MM_BODY_LIMIT
 sub rcx,rax
 cmp r14,rcx
 jb .Lrx_capacity
 mov r15,[rbx+PG_MM_CONSUMED]
 test r15,r15
 jz .Lrx_zero
 cmp r15,rax
 ja .Lrx_zero
 sub rax,r15
 mov r13,rax
 lea rdi,[rbx+PG_MM_BODY]
 lea rsi,[rdi+r15]
 mov rdx,rax
 call memmove@PLT
 mov [rbx+PG_MM_USED],r13
 mov qword ptr [rbx+PG_MM_CONSUMED],0
 mov byte ptr [rbx+r13+PG_MM_BODY],0
.Lrx_capacity:
 mov rax, qword ptr [rbx + PG_MM_USED]
 mov rcx, PG_MM_BODY_LIMIT
 sub rcx, rax                         /* 剩余容量（含终止 NUL 位） */
 cmp r14, rcx
 jae .Lrx_zero
 mov rdi,r12
 xor esi,esi
 mov rdx,r14
 call memchr@PLT
 test rax,rax
 jnz .Lrx_zero
 mov rax,[rbx + PG_MM_USED]
 lea rdi, [rbx + rax + PG_MM_BODY]
 mov rsi, r12
 mov rdx, r14
 call memcpy@PLT
 mov rcx, qword ptr [rbx + PG_MM_USED]
 add rcx, r14
 mov qword ptr [rbx + PG_MM_USED], rcx
 mov byte ptr [rbx + rcx + PG_MM_BODY], 0
 mov rax, r14
 jmp .Lrx_out
.Lrx_zero:
 xor eax, eax
.Lrx_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_receive, .-li_receive

/* ---- int li_start(void *ctx, const pg_session *s)
 * 栈帧 544：[0..512) auth 头缓冲。失败路径经 close 语义可安全善后。 */
.type li_start,@function
li_start:
 push rbx
 push r12
 push r13
 push r14
 push r15                             /* 入口 rsp≡8；5 压栈后 ≡0 */
 sub rsp, 544
 mov rbx, rdi                         /* m */
 mov r12, rsi                         /* s */
 call .Ltransport_close               /* 保留已绑定请求配置 */
 mov rdi,rbx
 call .Lclear_argument_buffers
 mov rdi,[rbx+PG_MM_USAGE]
 mov qword ptr [rbx+PG_MM_USAGE],0
 call json_object_put@PLT
 mov qword ptr [rbx + PG_MM_USED], 0
 mov qword ptr [rbx + PG_MM_CONSUMED], 0
 mov dword ptr [rbx + PG_MM_DELIVERED], 0
 mov dword ptr [rbx + PG_MM_FAILED], 0
 mov dword ptr [rbx + PG_MM_TRANSPORT_DONE], 0
 mov dword ptr [rbx + PG_MM_FINISH_REASON], 0
 mov dword ptr [rbx + PG_MM_SAW_DONE], 0
 mov qword ptr [rbx + PG_MM_HTTP], 0
 mov byte ptr [rbx + PG_MM_ARGUMENTS], 0
 mov byte ptr [rbx + PG_MM_ID], 0
 mov byte ptr [rbx + PG_MM_NAME], 0

 lea rdi,[rip+.Lstr_pangu_key]
 call getenv@PLT
 test rax,rax
 jnz .Lst_key_ready
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 test rdi,rdi
 jz .Lst_check_environment_endpoint
 lea rsi,[rip+.Lcfg_endpoint]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lstr_endpoint]
 call strcmp@PLT
 test eax,eax
 jnz .Lst_bad
 jmp .Lst_default_key
.Lst_check_environment_endpoint:
 lea rdi,[rip+.Lstr_pangu_endpoint]
 call getenv@PLT
 test rax,rax
 jnz .Lst_bad
 .Lst_default_key:
 lea rdi, [rip + .Lstr_env_key]
 call getenv@PLT
 test rax, rax
 jz .Lst_bad
.Lst_key_ready:
 mov r13, rax                         /* key */
 mov rdi, rax
 call strlen@PLT
 test rax,rax
 jz .Lst_bad
 cmp rax, 400
 ja .Lst_bad
 mov rdi, r13
 mov esi, 13                          /* '\r' */
 call strchr@PLT
 test rax, rax
 jnz .Lst_bad
 mov rdi, r13
 mov esi, 10                          /* '\n' */
 call strchr@PLT
 test rax, rax
 jnz .Lst_bad
 mov rdi, rbx
 mov rsi, r12
 call pg_mimo_encode_request
 test eax, eax
 jnz .Lst_bad
 /* auth = "Authorization: Bearer " + key + NUL（总长 < 512） */
 lea r14,[rip+.Lstr_auth_prefix]
 mov r15d,PG_MM_AUTH_PREFIX_LEN
 mov rdi,rbx
 call pg_mimo_uses_anthropic
 test eax,eax
 jz .Lauth_prefix_ready
 lea r14,[rip+.Lanthropic_key]
 mov r15d,11
.Lauth_prefix_ready:
 lea rdi,[rsp]
 mov rsi,r14
 mov rdx,r15
 call memcpy@PLT
 mov rdi, r13
 call strlen@PLT
 lea rdi, [rsp + r15]
 mov rsi, r13
 lea rdx, [rax + 1]
 call memcpy@PLT
 xor edi, edi
 lea rsi, [rsp]
 call curl_slist_append@PLT
 mov r13, rax
 mov qword ptr [rbx + PG_MM_HEADERS], rax
 test rax,rax
 jz .Lst_bad
 mov rdi, r13
 lea rsi, [rip + .Lstr_content_type]
 call curl_slist_append@PLT
 test rax,rax
 jz .Lst_bad
 mov r13, rax
 mov qword ptr [rbx + PG_MM_HEADERS], r13
 mov rdi,rbx
 call pg_mimo_uses_anthropic
 test eax,eax
 jz .Lauth_headers_ready
 mov rdi,r13
 lea rsi,[rip+.Lanthropic_version]
 call curl_slist_append@PLT
 test rax,rax
 jz .Lst_bad
 mov r13,rax
 mov [rbx+PG_MM_HEADERS],rax
.Lauth_headers_ready:
 call curl_easy_init@PLT
 mov r14, rax
 mov qword ptr [rbx + PG_MM_EASY], r14
 call curl_multi_init@PLT
 mov r15, rax
 mov qword ptr [rbx + PG_MM_MULTI], r15
 test r14, r14
 jz .Lst_bad                          /* 半初始化：close 释放已建部分 */
 test r15, r15
 jz .Lst_bad
 /* 变参 setopt：整数/指针变参在 rdx，al=0（无向量参数） */
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 test rdi,rdi
 jz .Lst_environment_endpoint
 lea rsi,[rip+.Lcfg_endpoint]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov rdx,rax
 jmp .Lst_endpoint_ready
.Lst_environment_endpoint:
 lea rdi,[rip+.Lstr_pangu_endpoint]
 call getenv@PLT
 test rax,rax
 jz .Lst_default_endpoint
 cmp byte ptr [rax],0
 je .Lst_bad
 mov rdx,rax
 jmp .Lst_endpoint_ready
.Lst_default_endpoint:
 lea rdx,[rip+.Lstr_endpoint]
.Lst_endpoint_ready:
 mov rdi, r14
 mov esi, PG_MM_CURLOPT_URL
 xor eax, eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lst_bad
 mov rdi, r14
 mov esi, PG_MM_CURLOPT_HTTPHEADER
 mov rdx, r13
 xor eax, eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lst_bad
 mov rdi, r14
 mov esi, PG_MM_CURLOPT_POSTFIELDS
 mov rdx, qword ptr [rbx + PG_MM_REQUEST]
 xor eax, eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lst_bad
 mov rdi, r14
 mov esi, PG_MM_CURLOPT_WRITEFUNCTION
 lea rdx, [rip + li_receive]
 xor eax, eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lst_bad
 mov rdi, r14
 mov esi, PG_MM_CURLOPT_WRITEDATA
 mov rdx, rbx
 xor eax, eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lst_bad
 mov rdi, r14
 mov esi, PG_MM_CURLOPT_TIMEOUT
 mov edx, 60
 xor eax, eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lst_bad
 mov rdi, r14
 mov esi, PG_MM_CURLOPT_CONNECTTIMEOUT
 mov edx, 15
 xor eax, eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lst_bad
 mov rdi, r14
 mov esi, PG_MM_CURLOPT_NOSIGNAL
 mov edx, 1
 xor eax, eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lst_bad
 mov rdi, r14
 mov esi, PG_MM_CURLOPT_PROTOCOLS_STR
 lea rdx, [rip + .Lstr_https]
 xor eax, eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lst_bad
 mov rdi, r15
 mov rsi, r14
 call curl_multi_add_handle@PLT
 test eax, eax
 setne al
 movzx eax, al
 jmp .Lst_out
.Lst_bad:
 mov eax, 1
.Lst_out:
 add rsp, 544
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_start, .-li_start

/* ---- int li_next(void *ctx, pg_model_event *event)
 * 栈帧 64：[0]=running(int) [8]=left(int)；调用集合的校验在私有helper中。 ---- */
.type li_next,@function
li_next:
 push rbx
 push r12
 push r13
 push r14
 push r15                             /* 入口 rsp≡8；5 压栈后 ≡0 */
 sub rsp, 64
 mov rbx, rdi                         /* m */
 mov r12, rsi                         /* event */
 mov rdi, r12
 xor eax, eax
 mov ecx, 7
 cld
 rep stosq                           /* *event = {WAIT} */
 cmp dword ptr [rbx+PG_MM_FAILED],0
 jne .Lnx_bad
 cmp dword ptr [rbx + PG_MM_DELIVERED], 0
 jne .Lnx_delivered
 cmp dword ptr [rbx + PG_MM_TRANSPORT_DONE], 0
 jne .Lnx_lines
 mov dword ptr [rsp], 0               /* running */
 mov rdi, qword ptr [rbx + PG_MM_MULTI]
 lea rsi, [rsp]
 call curl_multi_perform@PLT
 test eax, eax
 jnz .Lnx_bad
 mov rdi, qword ptr [rbx + PG_MM_EASY]
 mov esi, PG_MM_CURLINFO_RESPONSE_CODE
 lea rdx, [rbx + PG_MM_HTTP]
 xor eax, eax                         /* getinfo 亦为变参 */
 call curl_easy_getinfo@PLT
 test eax,eax
 jnz .Lnx_bad
 mov rax, qword ptr [rbx + PG_MM_HTTP]
 test rax, rax
 jz .Lnx_running                      /* 首响应头未到 */
.Lnx_running:
 cmp dword ptr [rsp], 0
 je .Lnx_transport_result
 cmp qword ptr [rbx+PG_MM_HTTP],200
 jne .Lnx_wait
 jmp .Lnx_lines
.Lnx_transport_result:
 mov dword ptr [rsp + 8], 0           /* left */
 mov rdi, qword ptr [rbx + PG_MM_MULTI]
 lea rsi, [rsp + 8]
 call curl_multi_info_read@PLT
 test rax, rax
 jz .Lnx_bad
 cmp dword ptr [rax + PG_MM_CURLMSG_MSG], PG_MM_CURLMSG_DONE
 jne .Lnx_bad
 mov ecx,[rax + PG_MM_CURLMSG_RESULT]
 mov [rbx+PG_MM_TRANSPORT_ERROR],ecx
 cmp ecx,PG_MM_CURLE_OK
 jne .Lnx_bad                         /* 传输错误不得伪造成功 */
 cmp qword ptr [rbx + PG_MM_HTTP], 200
 jne .Lnx_bad
 mov dword ptr [rbx + PG_MM_TRANSPORT_DONE], 1
.Lnx_lines:
 mov r14, qword ptr [rbx + PG_MM_CONSUMED]
 cmp r14, qword ptr [rbx + PG_MM_USED]
 jae .Lnx_drain
 lea r13, [rbx + r14 + PG_MM_BODY]    /* 行起点 */
 mov rdx, qword ptr [rbx + PG_MM_USED]
 sub rdx, r14                         /* 剩余字节 */
 mov rdi, r13
 mov esi, 10                          /* '\n' */
 call memchr@PLT
 test rax, rax
 jnz .Lnx_have_line
 cmp dword ptr [rbx + PG_MM_TRANSPORT_DONE], 0
 jne .Lnx_bad                         /* 传输已结束仍无换行：截断 */
 xor eax, eax                         /* 行未完整：WAIT */
 jmp .Lnx_out
.Lnx_have_line:
 mov byte ptr [rax], 0
 cmp rax, r13
 jbe 1f
 cmp byte ptr [rax - 1], 13           /* 剥离 CR */
 jne 1f
 mov byte ptr [rax - 1], 0
1:
 sub rax, rbx
 sub rax, PG_MM_BODY
 inc rax
 mov qword ptr [rbx + PG_MM_CONSUMED], rax
 mov rdi, rbx
 mov rsi, r13
 mov rdx, r12
 call pg_mimo_decode_line
 test eax,eax
 jnz .Lnx_bad
 jmp .Lnx_out
.Lnx_drain:
 cmp dword ptr [rbx + PG_MM_TRANSPORT_DONE], 0
 je .Lnx_wait
 cmp dword ptr [rbx + PG_MM_SAW_DONE], 0
 je .Lnx_bad
 cmp dword ptr [rbx + PG_MM_FINISH_REASON], 0
 je .Lnx_bad
 cmp dword ptr [rbx + PG_MM_FINISH_REASON], 2
 jne .Lnx_done_event
 call .Lcalls_ready
 test eax,eax
 jnz .Lnx_bad
 cmp dword ptr [rbx+PG_MM_CALL_COUNT],1
 jne .Lnx_batch
 mov dword ptr [r12+PG_MM_EVT_KIND],PG_MM_KIND_CALL
 mov rax,[rbx+PG_MM_CALLS]
 mov [r12+8],rax
 mov rax,[rbx+PG_MM_CALLS+8]
 mov [r12+16],rax
 mov rax,[rbx+PG_MM_CALLS+16]
 mov [r12+24],rax
 mov rax,[rbx+PG_MM_CALLS+24]
 mov [r12+32],rax
 mov rax,[rbx+PG_MM_CALLS+32]
 mov [r12+40],rax
 mov rax,[rbx+PG_MM_CALLS+40]
 mov [r12+48],rax
 jmp .Lnx_terminal
.Lnx_batch:
 mov dword ptr [r12+PG_MM_EVT_KIND],PG_MM_KIND_BATCH
 lea rax,[rbx+PG_MM_CALLS]
 mov [r12+8],rax
 mov eax,[rbx+PG_MM_CALL_COUNT]
 mov [r12+16],rax
 jmp .Lnx_terminal
.Lnx_done_event:
 mov dword ptr [r12 + PG_MM_EVT_KIND], PG_MM_KIND_DONE
.Lnx_terminal:
 mov dword ptr [rbx + PG_MM_DELIVERED], 1
 xor eax, eax
 jmp .Lnx_out
.Lnx_delivered:
 mov dword ptr [r12 + PG_MM_EVT_KIND], PG_MM_KIND_DONE
 xor eax, eax
 jmp .Lnx_out
.Lnx_wait:
 xor eax, eax
 jmp .Lnx_out
.Lnx_bad:
 mov dword ptr [rbx+PG_MM_FAILED],1
 mov eax, 1
.Lnx_out:
 add rsp, 64
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_next, .-li_next


.Lcalls_ready:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,32
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 cmp dword ptr [rbx+PG_MM_CALL_COUNT],1
 jb .Lready_bad
 cmp dword ptr [rbx+PG_MM_CALL_COUNT],PG_MM_CALL_LIMIT
 ja .Lready_bad
 xor r13d,r13d
.Lready_call:
 mov rax,r13
 PG_MM_CALL_SLOT r14,rax
 imul r15,r13,PG_MM_CALL_BYTES
 lea r15,[rbx+PG_MM_CALLS+r15]
 mov rax,[rbx+PG_MM_ARGUMENT_OVERFLOW+r13*8]
 test rax,rax
 jnz .Lready_dynamic_arguments
 mov rax,r14
.Lready_dynamic_arguments:
 mov [r15],rax
 mov [rsp+16],rax
 mov rdx,[rbx+PG_MM_ARGUMENT_LENGTH+r13*8]
 mov [r15+8],rdx
 mov [rsp+24],rdx
 lea rdi,[r14+PG_MM_TEXT_LIMIT]
 mov [r15+16],rdi
 call strlen@PLT
 test rax,rax
 jz .Lready_bad
 mov [r15+24],rax
 lea rdi,[r14+PG_MM_TEXT_LIMIT+PG_MM_ID_LIMIT]
 mov [r15+32],rdi
 call strlen@PLT
 test rax,rax
 jz .Lready_bad
 mov [r15+40],rax
 mov rdi,rbx
 call pg_mimo_uses_responses@PLT
 test eax,eax
 jnz .Lready_prepared_arguments
 mov rdi,rbx
 call pg_mimo_uses_anthropic
 test eax,eax
 jz .Lready_chat_arguments
.Lready_prepared_arguments:
 mov rdi,rbx
 mov esi,r13d
 call pg_mimo_prepare_arguments@PLT
 test eax,eax
 jnz .Lready_bad
 jmp .Lready_next
.Lready_chat_arguments:
 call json_tokener_new@PLT
 test rax,rax
 jz .Lready_bad
 mov [rsp],rax
 mov rdi,rax
 mov rsi,[rsp+16]
 mov rdx,[rsp+24]
 call json_tokener_parse_ex@PLT
 mov [rsp+8],rax
 mov rdi,[rsp]
 call json_tokener_get_error@PLT
 test eax,eax
 jnz .Lready_bad
 cmp qword ptr [rsp+8],0
 je .Lready_bad
 mov rdi,[rsp]
 call json_tokener_get_parse_end@PLT
.Lready_tail:
 cmp rax,[rsp+24]
 jae .Lready_type
 mov rdx,[rsp+16]
 movzx ecx,byte ptr [rdx+rax]
 cmp ecx,32
 je .Lready_space
 cmp ecx,9
 je .Lready_space
 cmp ecx,10
 je .Lready_space
 cmp ecx,13
 jne .Lready_bad
.Lready_space:
 inc rax
 jmp .Lready_tail
.Lready_type:
 mov rdi,[rsp+8]
 mov esi,PG_MM_JSON_OBJECT
 call json_object_is_type@PLT
 test eax,eax
 jz .Lready_bad
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov qword ptr [rsp+8],0
 mov rdi,[rsp]
 call json_tokener_free@PLT
 mov qword ptr [rsp],0
.Lready_next:
 inc r13
 mov eax,[rbx+PG_MM_CALL_COUNT]
 cmp r13,rax
 jb .Lready_call
 xor eax,eax
 jmp .Lready_out
.Lready_bad:
 mov rdi,[rsp+8]
 test rdi,rdi
 jz .Lready_token
 call json_object_put@PLT
.Lready_token:
 mov rdi,[rsp]
 test rdi,rdi
 jz .Lready_error
 call json_tokener_free@PLT
.Lready_error:
 mov eax,1
.Lready_out:
 add rsp,32
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.section .note.GNU-stack,"",@progbits

.text
.globl pg_mimo_configure
.type pg_mimo_configure,@function
pg_mimo_configure:
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
 mov ebp,1
 test rdi,rdi
 jz .Lcfg_end
 cmp qword ptr [rdi+PG_MM_EASY],0
 jne .Lcfg_end
 cmp qword ptr [rdi+PG_MM_MULTI],0
 jne .Lcfg_end
 mov rdi,rsi
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lcfg_end
 mov rdi,r12
 lea rsi,[rip+.Lcfg_model]
 call json_object_object_get@PLT
 mov rdi,rax
 mov esi,256
 call .Lcfg_string
 test eax,eax
 jnz .Lcfg_end
 mov rdi,r12
 lea rsi,[rip+.Lcfg_endpoint]
 call json_object_object_get@PLT
 mov rdi,rax
 mov esi,2048
 call .Lcfg_string
 test eax,eax
 jnz .Lcfg_end
 mov rdi,r12
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lcfg_end
 call json_object_new_object@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lcfg_end
 mov rdi,[rsp]
 mov rsi,rax
 call pg_mimo_apply_options@PLT
 test eax,eax
 jnz .Lcfg_end
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call json_object_put@PLT
 mov rax,[rsp]
 mov [rbx+PG_MM_CONFIGURATION],rax
 mov qword ptr [rsp],0
 xor ebp,ebp
.Lcfg_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_mimo_configure,.-pg_mimo_configure
.Lcfg_string:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12d,esi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lcfg_string_bad
 mov rdi,rbx
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lcfg_string_bad
 cmp eax,r12d
 jae .Lcfg_string_bad
 mov r13d,eax
 mov rdi,rbx
 call json_object_get_string@PLT
 mov rdi,rax
 xor esi,esi
 mov edx,r13d
 call memchr@PLT
 test rax,rax
 jnz .Lcfg_string_bad
 xor eax,eax
 jmp .Lcfg_string_end
.Lcfg_string_bad:
 mov eax,1
.Lcfg_string_end:
 pop r13
 pop r12
 pop rbx
 ret
.section .rodata
.Lcfg_model: .asciz "model"
.Lcfg_endpoint: .asciz "endpoint"
.text
/* Resolve absent usage fields as zero; reject fractional/negative/unsafe counts. */
.Lmu_count:
 push rbx
 call json_object_object_get@PLT
 mov rbx,rax
 test rax,rax
 jz .Lmu_zero
 mov rdi,rax
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lmu_invalid
 mov rdi,rbx
 call json_object_get_int64@PLT
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lmu_invalid
 xor edx,edx
 pop rbx
 ret
.Lmu_zero:
 xor eax,eax
 xor edx,edx
 pop rbx
 ret
.Lmu_invalid:
 mov edx,1
 xor eax,eax
 pop rbx
 ret
.globl pg_mimo_complete
.type pg_mimo_complete,@function
pg_mimo_complete:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,80
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
 mov ebp,1
 pxor xmm0,xmm0
 movdqu [rsp],xmm0
 movdqu [rsp+16],xmm0
 movdqu [rsp+32],xmm0
 movdqu [rsp+48],xmm0
 mov qword ptr [rsp+64],0
 test rdi,rdi
 jz .Lmu_end
 test rsi,rsi
 jz .Lmu_end
 mov eax,[rbx+PG_MM_FINISH_REASON]
 lea rdi,[rip+.Lmu_stop]
 cmp eax,1
 je .Lmu_reason
 lea rdi,[rip+.Lmu_tool_use]
 cmp eax,2
 je .Lmu_reason
 lea rdi,[rip+.Lmu_length]
 cmp eax,3
 jne .Lmu_end
.Lmu_reason:
 mov [rsp+72],rdi
 cmp byte ptr [rbx+PG_MM_REASONING],0
 je .Lmu_reason_without_thinking
 lea rdi,[rip+.Lmu_thinking_json]
 call json_tokener_parse@PLT
 mov r13,rax
 test rax,rax
 jz .Lmu_end
 lea rdi,[rbx+PG_MM_REASONING]
 call json_object_new_string@PLT
 mov [rsp+64],rax
 test rax,rax
 jz .Lmu_end
 mov rdi,r13
 lea rsi,[rip+.Lmu_thinking]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lmu_end
 mov qword ptr [rsp+64],0
 mov rdi,r12
 lea rsi,[rip+.Lmu_content]
 call json_object_object_get@PLT
 mov rdi,rax
 xor esi,esi
 mov rdx,r13
 call json_object_array_insert_idx@PLT
 test eax,eax
 jnz .Lmu_end
 xor r13d,r13d
.Lmu_reason_without_thinking:
 mov rdi,[rsp+72]
 call json_object_new_string@PLT
 mov [rsp+64],rax
 test rax,rax
 jz .Lmu_end
 mov rdi,r12
 lea rsi,[rip+.Lmu_stop_reason]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lmu_end
 mov qword ptr [rsp+64],0
 mov r14,[rbx+PG_MM_USAGE]
 test r14,r14
 jz .Lmu_success
 mov rdi,r14
 lea rsi,[rip+.Lmu_prompt]
 call .Lmu_count
 test edx,edx
 jnz .Lmu_end
 mov [rsp],rax
 mov rdi,r14
 lea rsi,[rip+.Lmu_completion]
 call .Lmu_count
 test edx,edx
 jnz .Lmu_end
 mov [rsp+8],rax
 mov rdi,r14
 lea rsi,[rip+.Lmu_prompt_details]
 call json_object_object_get@PLT
 mov [rsp+56],rax
 mov rdi,rax
 lea rsi,[rip+.Lmu_cached]
 lea rdx,[rsp+48]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lmu_deepseek_cache
 cmp qword ptr [rsp+48],0
 je .Lmu_deepseek_cache
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lmu_cached]
 jmp .Lmu_cache_count
.Lmu_deepseek_cache:
 mov rdi,r14
 lea rsi,[rip+.Lmu_hit]
 lea rdx,[rsp+48]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lmu_top_cache
 cmp qword ptr [rsp+48],0
 je .Lmu_top_cache
 mov rdi,r14
 lea rsi,[rip+.Lmu_hit]
 jmp .Lmu_cache_count
.Lmu_top_cache:
 mov rdi,r14
 lea rsi,[rip+.Lmu_cached]
.Lmu_cache_count:
 call .Lmu_count
 test edx,edx
 jnz .Lmu_end
 mov [rsp+16],rax
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lmu_cache_write]
 call .Lmu_count
 test edx,edx
 jnz .Lmu_end
 mov [rsp+24],rax
 mov rdi,r14
 lea rsi,[rip+.Lmu_completion_details]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lmu_reasoning_tokens]
 call .Lmu_count
 test edx,edx
 jnz .Lmu_end
 mov [rsp+32],rax
 mov rax,[rsp]
 sub rax,[rsp+16]
 sub rax,[rsp+24]
 xor ecx,ecx
 test rax,rax
 cmovs rax,rcx
 mov [rsp],rax
 add rax,[rsp+8]
 add rax,[rsp+16]
 add rax,[rsp+24]
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lmu_end
 mov [rsp+40],rax
 call json_object_new_object@PLT
 mov r13,rax
 test rax,rax
 jz .Lmu_end
 xor r14d,r14d
.Lmu_fields:
 mov rdi,[rsp+r14*8]
 call json_object_new_int64@PLT
 mov [rsp+64],rax
 test rax,rax
 jz .Lmu_end
 lea rcx,[rip+.Lmu_keys]
 mov rsi,[rcx+r14*8]
 mov rdi,r13
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lmu_end
 mov qword ptr [rsp+64],0
 inc r14
 cmp r14,6
 jb .Lmu_fields
 mov rdi,r12
 lea rsi,[rip+.Lmu_usage]
 mov rdx,r13
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lmu_end
 xor r13d,r13d
.Lmu_success:
 mov rdi,rbx
 mov rsi,r12
 call pg_mimo_responses_complete
 mov ebp,eax
.Lmu_end:
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov rdi,r13
 call json_object_put@PLT
 mov eax,ebp
 add rsp,80
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_mimo_complete,.-pg_mimo_complete
.section .rodata
.Lmu_stop: .asciz "stop"
.Lmu_tool_use: .asciz "toolUse"
.Lmu_length: .asciz "length"
.Lmu_stop_reason: .asciz "stopReason"
.Lmu_usage: .asciz "usage"
.Lmu_prompt: .asciz "prompt_tokens"
.Lmu_completion: .asciz "completion_tokens"
.Lmu_prompt_details: .asciz "prompt_tokens_details"
.Lmu_completion_details: .asciz "completion_tokens_details"
.Lmu_cached: .asciz "cached_tokens"
.Lmu_hit: .asciz "prompt_cache_hit_tokens"
.Lmu_cache_write: .asciz "cache_write_tokens"
.Lmu_reasoning_tokens: .asciz "reasoning_tokens"
.Lmu_input: .asciz "input"
.Lmu_output: .asciz "output"
.Lmu_cache_read: .asciz "cacheRead"
.Lmu_cache_write_key: .asciz "cacheWrite"
.Lmu_reasoning: .asciz "reasoning"
.Lmu_total: .asciz "totalTokens"
.section .data.rel.ro
.Lmu_keys: .quad .Lmu_input,.Lmu_output,.Lmu_cache_read,.Lmu_cache_write_key,.Lmu_reasoning,.Lmu_total

.section .rodata
.Lmu_thinking: .asciz "thinking"
.Lmu_content: .asciz "content"
.Lmu_thinking_json: .asciz "{\"type\":\"thinking\",\"thinkingSignature\":\"reasoning_content\"}"

.section .rodata
.Lanthropic_key: .asciz "x-api-key: "
.Lanthropic_version: .asciz "anthropic-version: 2023-06-01"
