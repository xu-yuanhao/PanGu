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
 mov rdi,[rbx+PG_MM_GOOGLE_TEXT]
 mov qword ptr [rbx+PG_MM_GOOGLE_TEXT],0
 call json_object_put@PLT
 mov rdi,[rbx+PG_MM_STRICT_DIAGNOSTICS]
 mov qword ptr [rbx+PG_MM_STRICT_DIAGNOSTICS],0
 mov dword ptr [rbx+PG_MM_STRICT_RETRY],0
 mov byte ptr [rbx+PG_MM_RESPONSE_REQUEST_ID],0
 call json_object_put@PLT
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
 mov rdi,[rbx+PG_MM_WIRE_REQUEST]
 call free@PLT
 mov qword ptr [rbx+PG_MM_WIRE_REQUEST],0
 mov qword ptr [rbx+PG_MM_WIRE_SIZE],0
 mov qword ptr [rbx+PG_MM_RETRY_ATTEMPT],0
 mov qword ptr [rbx+PG_MM_RETRY_AT],0
 mov dword ptr [rbx+PG_MM_RETRY_BLOCKED],0
 mov byte ptr [rbx+PG_MM_RETRY_AFTER],0
 mov byte ptr [rbx+PG_MM_RETRY_AFTER_MS],0
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
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jnz .Lrx_copy
 mov rdi,r12
 xor esi,esi
 mov rdx,r14
 call memchr@PLT
 test rax,rax
 jnz .Lrx_zero
.Lrx_copy:
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
 mov qword ptr [rsp+512],0
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

 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lst_explicit_key
 lea rdi,[rip+.Lbe_skip_env]
 call .Lbe_enabled
 test eax,eax
 jnz .Lst_aws_iam
.Lst_explicit_key:
 lea rdi,[rip+.Lstr_pangu_key]
 call getenv@PLT
 test rax,rax
 jnz .Lst_key_ready
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lst_nonaws_key
 lea rdi,[rip+.Lbe_bearer_env]
 call getenv@PLT
 test rax,rax
 jz .Lst_aws_iam
 cmp byte ptr [rax],0
 jne .Lst_key_ready
.Lst_aws_iam:
 mov qword ptr [rsp+512],1
 jmp .Lst_encode
.Lst_nonaws_key:
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
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lst_standard_key
 mov rdi,r13
 call .Lbe_credential
 test eax,eax
 jnz .Lst_bad
 jmp .Lst_encode
.Lst_standard_key:
 mov rdi,r13
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
.Lst_encode:
 mov rdi, rbx
 mov rsi, r12
 call pg_mimo_encode_request
 test eax, eax
 jnz .Lst_bad
 cmp qword ptr [rsp+512],0
 je .Lst_bearer_header
 xor r13d,r13d
 jmp .Lauth_content_type
.Lst_bearer_header:
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lst_standard_bearer
 mov rdi,rbx
 mov rsi,r13
 call .Lbe_bearer_header
 test eax,eax
 jnz .Lst_bad
 mov r13,[rbx+PG_MM_HEADERS]
 jmp .Lauth_content_type
.Lst_standard_bearer:
 /* auth = "Authorization: Bearer " + key + NUL（总长 < 512） */
 lea r14,[rip+.Lstr_auth_prefix]
 mov r15d,PG_MM_AUTH_PREFIX_LEN
 mov rdi,rbx
 call pg_mimo_uses_google
 cmp eax,1
 je .Lauth_google_key
 cmp eax,2
 jne .Lauth_anthropic_prefix
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call .Lge_key_mode
 test eax,eax
 jz .Lauth_prefix_ready
.Lauth_google_key:
 lea r14,[rip+.Lgoogle_key]
 mov r15d,16
 jmp .Lauth_prefix_ready
.Lauth_anthropic_prefix:
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call .Laz_is
 test eax,eax
 jz .Lauth_nonazure
 lea r14,[rip+.Laz_key]
 mov r15d,9
 jmp .Lauth_prefix_ready
.Lauth_nonazure:
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call .Lcp_is
 test eax,eax
 jnz .Lauth_prefix_ready
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
.Lauth_content_type:
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
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call .Lcp_is
 test eax,eax
 jz .Lauth_headers_ready
 mov rdi,r13
 lea rsi,[rip+.Lcp_accept]
 call curl_slist_append@PLT
 test rax,rax
 jz .Lst_bad
 mov r13,rax
 mov [rbx+PG_MM_HEADERS],rax
 mov rdi,r13
 lea rsi,[rip+.Lcp_browser]
 call curl_slist_append@PLT
 test rax,rax
 jz .Lst_bad
 mov r13,rax
 mov [rbx+PG_MM_HEADERS],rax
.Lauth_headers_ready:
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_codex@PLT
 test eax,eax
 jz .Lcd_originator_ready
 mov rdi,[rbx+PG_MM_HEADERS]
 lea rsi,[rip+.Lcd_originator]
 call curl_slist_append@PLT
 test rax,rax
 jz .Lst_bad
 mov [rbx+PG_MM_HEADERS],rax
.Lcd_originator_ready:
 mov rdi,rbx
 call .Lanthropic_beta_headers
 test eax,eax
 jnz .Lst_bad
 mov rdi,rbx
 call .Lanthropic_affinity
 test eax,eax
 jnz .Lst_bad
 mov rdi,rbx
 mov rsi,r12
 call .Lprovider_headers
 test eax,eax
 jnz .Lst_bad
 mov rdi,rbx
 call .Lcd_headers
 test eax,eax
 jnz .Lst_bad
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
 cmp qword ptr [rsp+512],0
 je .Lst_signed_ready
 mov rdi,rbx
 mov rsi,r14
 call .Lbe_sign
 test eax,eax
 jnz .Lst_bad
.Lst_signed_ready:
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lst_http_version_ready
 lea rdi,[rip+.Lbe_http1_env]
 call .Lbe_enabled
 test eax,eax
 jz .Lst_http_version_ready
 mov rdi,r14
 mov esi,PG_MM_CURLOPT_HTTP_VERSION
 mov edx,PG_MM_CURL_HTTP_VERSION_1_1
 xor eax,eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lst_bad
.Lst_http_version_ready:
 mov rdi,rbx
 call .Lcd_compress
 test eax,eax
 jnz .Lst_bad
 mov rdi, r14
 mov esi, PG_MM_CURLOPT_HTTPHEADER
 mov rdx,[rbx+PG_MM_HEADERS]
 xor eax, eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lst_bad
 cmp qword ptr [rbx+PG_MM_WIRE_REQUEST],0
 je .Lcd_plain_post
 mov rdi,r14
 mov esi,PG_MM_CURLOPT_POSTFIELDSIZE
 mov rdx,[rbx+PG_MM_WIRE_SIZE]
 xor eax,eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lst_bad
 mov rdx,[rbx+PG_MM_WIRE_REQUEST]
 jmp .Lcd_post_ready
.Lcd_plain_post:
 mov rdx,[rbx+PG_MM_REQUEST]
.Lcd_post_ready:
 mov rdi,r14
 mov esi,PG_MM_CURLOPT_POSTFIELDS
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
 mov rdi,r14
 mov esi,PG_MM_CURLOPT_HEADERFUNCTION
 lea rdx,[rip+.Lhd_receive]
 xor eax,eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lst_bad
 mov rdi,r14
 mov esi,PG_MM_CURLOPT_HEADERDATA
 mov rdx,rbx
 xor eax,eax
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
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call .Ltm_timeout
 test edx,edx
 jnz .Lst_bad
 mov rdx,rax
 mov rdi, r14
 mov esi, PG_MM_CURLOPT_TIMEOUT_MS
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
 mov rdi,rbx
 call .Lcr_resume
 test eax,eax
 js .Lnx_bad
 jnz .Lnx_wait
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
 jne .Lnx_codex_retry                         /* 传输错误不得伪造成功 */
 cmp qword ptr [rbx + PG_MM_HTTP], 200
 jne .Lnx_strict_retry
 mov dword ptr [rbx + PG_MM_TRANSPORT_DONE], 1
 jmp .Lnx_lines
.Lnx_strict_retry:
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_codex@PLT
 test eax,eax
 jnz .Lnx_codex_retry
 mov rdi,rbx
 call pg_mimo_anthropic_strict_retry
 test eax,eax
 jnz .Lnx_bad
 mov rdi,[rbx+PG_MM_MULTI]
 mov rsi,[rbx+PG_MM_EASY]
 call curl_multi_remove_handle@PLT
 test eax,eax
 jnz .Lnx_bad
 mov rdi,[rbx+PG_MM_EASY]
 mov esi,PG_MM_CURLOPT_POSTFIELDS
 mov rdx,[rbx+PG_MM_REQUEST]
 xor eax,eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lnx_bad
 mov rdi,[rbx+PG_MM_MULTI]
 mov rsi,[rbx+PG_MM_EASY]
 call curl_multi_add_handle@PLT
 test eax,eax
 jnz .Lnx_bad
 mov qword ptr [rbx+PG_MM_USED],0
 mov qword ptr [rbx+PG_MM_CONSUMED],0
 mov byte ptr [rbx+PG_MM_BODY],0
 mov qword ptr [rbx+PG_MM_HTTP],0
 mov dword ptr [rbx+PG_MM_TRANSPORT_ERROR],0
 jmp .Lnx_wait
.Lnx_codex_retry:
 mov rdi,rbx
 call .Lcr_schedule
 test eax,eax
 jnz .Lnx_wait
 jmp .Lnx_bad
.Lnx_lines:
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jnz .Lnx_bedrock_frame
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
.Lnx_bedrock_frame:
 mov r14,[rbx+PG_MM_CONSUMED]
 mov rax,[rbx+PG_MM_USED]
 sub rax,r14
 jz .Lnx_drain
 cmp rax,12
 jb .Lnx_bedrock_partial
 lea r13,[rbx+r14+PG_MM_BODY]
 mov edx,[r13]
 bswap edx
 cmp edx,16
 jb .Lnx_bad
 cmp edx,PG_MM_BODY_LIMIT
 jae .Lnx_bad
 cmp rax,rdx
 jb .Lnx_bedrock_partial
 add r14,rdx
 mov [rbx+PG_MM_CONSUMED],r14
 mov rdi,rbx
 mov rsi,r13
 mov rdx,r12
 call pg_mimo_decode_line
 test eax,eax
 jnz .Lnx_bad
 jmp .Lnx_out
.Lnx_bedrock_partial:
 cmp dword ptr [rbx+PG_MM_TRANSPORT_DONE],0
 jne .Lnx_bad
 jmp .Lnx_wait
.Lnx_drain:
 cmp dword ptr [rbx + PG_MM_TRANSPORT_DONE], 0
 je .Lnx_wait
 cmp dword ptr [rbx + PG_MM_SAW_DONE], 0
 je .Lnx_unfinished
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
.Lnx_unfinished:
 mov rdi,rbx
 call pg_mimo_uses_google
 test eax,eax
 jz .Lnx_bad
 lea rdi,[rip+.Lgoogle_unfinished]
 call json_tokener_parse@PLT
 test rax,rax
 jz .Lnx_bad
 mov r13,rax
 mov rdi,[rbx+PG_MM_RESPONSE_TERMINAL]
 call json_object_put@PLT
 mov [rbx+PG_MM_RESPONSE_TERMINAL],r13
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
 mov rsi,rsp
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lcfg_end
 mov rdi,[rsp]
 call .Lge_prepare
 test eax,eax
 jnz .Lcfg_end
 mov rdi,[rsp]
 call .Laz_prepare
 test eax,eax
 jnz .Lcfg_end
 mov rdi,[rsp]
 call .Lbe_prepare
 test eax,eax
 jnz .Lcfg_end
 mov rdi,[rsp]
 call .Lcp_prepare
 test eax,eax
 jnz .Lcfg_end
 mov rdi,[rsp]
 call .Lcr_validate
 test eax,eax
 jnz .Lcfg_end
 mov rdi,[rsp]
 call .Ltm_timeout
 test edx,edx
 jnz .Lcfg_end
 mov rdi,[rsp]
 lea rsi,[rip+.Lcfg_endpoint]
 call json_object_object_get@PLT
 mov rdi,rax
 mov esi,2048
 call .Lcfg_string
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
 mov rdi,r12
 lea rsi,[rip+.Lhd_stop]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lhd_normal_complete
 mov rdi,rax
 lea rsi,[rip+.Lhd_error]
 call strcmp@PLT
 test eax,eax
 jnz .Lhd_normal_complete
 mov rdi,rbx
 mov rsi,r12
 call .Lhd_failure_complete
 mov ebp,eax
 jmp .Lmu_end
.Lhd_normal_complete:
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
 test eax,eax
 jnz .Lmu_end
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 mov rsi,r12
 call .Lmc_calculate
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

/* Explicit beta header overrides generated features. Null/empty disables it;
   options headers take precedence over model headers, matching getBetaFeatures. */
.text
.Lanthropic_beta_headers:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],0
 mov qword ptr [rsp+32],0
 call pg_mimo_uses_anthropic
 test eax,eax
 jz .Lbeta_ok
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lbeta_modelInfo]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lbeta_headers]
 call json_object_object_get@PLT
 mov [rsp+40],rax
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lbeta_headers]
 call json_object_object_get@PLT
 mov [rsp+48],rax
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lbeta_options]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lbeta_headers]
 call json_object_object_get@PLT
 mov [rsp+56],rax
 xor r12d,r12d
.Lbeta_layer:
 mov rdi,[rsp+40+r12*8]
 test rdi,rdi
 jz .Lbeta_next_layer
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lbeta_bad
 mov rdi,[rsp+40+r12*8]
 call json_object_iter_begin@PLT
 mov [rsp+24],rax
 mov rdi,[rsp+40+r12*8]
 call json_object_iter_end@PLT
 mov [rsp+32],rax
.Lbeta_each:
 lea rdi,[rsp+24]
 lea rsi,[rsp+32]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lbeta_next_layer
 lea rdi,[rsp+24]
 call json_object_iter_peek_name@PLT
 mov rdi,rax
 lea rsi,[rip+.Lbeta_name]
 call strcasecmp@PLT
 test eax,eax
 jnz .Lbeta_next
 lea rdi,[rsp+24]
 call json_object_iter_peek_value@PLT
 mov [rsp+8],rax
 mov qword ptr [rsp+16],1
.Lbeta_next:
 lea rdi,[rsp+24]
 call json_object_iter_next@PLT
 jmp .Lbeta_each
.Lbeta_next_layer:
 inc r12
 cmp r12,3
 jb .Lbeta_layer
 cmp qword ptr [rsp+16],0
 je .Lbeta_default
 mov rdi,[rsp+8]
 test rdi,rdi
 jz .Lbeta_ok
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lbeta_bad
 mov rdi,[rsp+8]
 call json_object_get_string_len@PLT
 mov r13d,eax
 mov rdi,[rsp+8]
 call json_object_get_string@PLT
 mov r14,rax
 mov rdi,rax
 call strlen@PLT
 cmp rax,r13
 jne .Lbeta_bad
 test r13,r13
 jz .Lbeta_ok
 mov rdi,r14
 mov esi,13
 call strchr@PLT
 test rax,rax
 jnz .Lbeta_bad
 mov rdi,r14
 mov esi,10
 call strchr@PLT
 test rax,rax
 jnz .Lbeta_bad
 lea rdi,[r13+17]
 call malloc@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbeta_bad
 mov rdi,rax
 lea rsi,[rip+.Lbeta_prefix]
 mov edx,16
 call memcpy@PLT
 mov rdi,[rsp]
 add rdi,16
 mov rsi,r14
 lea rdx,[r13+1]
 call memcpy@PLT
 jmp .Lbeta_append
.Lbeta_default:
 mov rdi,[rbx+PG_MM_REQUEST]
 call json_tokener_parse@PLT
 mov rbp,rax
 test rax,rax
 jz .Lbeta_bad
 xor r14d,r14d
 mov rdi,rbp
 lea rsi,[rip+.Lbeta_thinking]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lbeta_type]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lbeta_tools
 mov rdi,rax
 lea rsi,[rip+.Lbeta_enabled]
 call strcmp@PLT
 test eax,eax
 jnz .Lbeta_tools
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lbeta_options]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lbeta_interleaved]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lbeta_thinking_on
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Lbeta_tools
.Lbeta_thinking_on:
 or r14d,1
.Lbeta_tools:
 mov rdi,rbp
 lea rsi,[rip+.Lbeta_tools_key]
 call json_object_object_get@PLT
 mov r13,rax
 test rax,rax
 jz .Lbeta_fallbacks
 mov rdi,rax
 call json_object_array_length@PLT
 test rax,rax
 jz .Lbeta_fallbacks
 mov rdi,r13
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rip+.Lbeta_eager]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jnz .Lbeta_fallbacks
 or r14d,2
.Lbeta_fallbacks:
 mov rdi,rbp
 lea rsi,[rip+.Lbeta_fallbacks_key]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lbeta_default_release
 mov rdi,rax
 call json_object_array_length@PLT
 test rax,rax
 jz .Lbeta_default_release
 or r14d,4
.Lbeta_default_release:
 mov rdi,rbp
 call json_object_put@PLT
 test r14d,r14d
 jz .Lbeta_ok
 lea rax,[rip+.Lbeta_feature_headers]
 mov rsi,[rax+r14*8]
 mov rdi,[rbx+PG_MM_HEADERS]
 jmp .Lbeta_send
.Lbeta_append:
 mov rdi,[rbx+PG_MM_HEADERS]
 mov rsi,[rsp]
.Lbeta_send:
 call curl_slist_append@PLT
 test rax,rax
 jz .Lbeta_bad
 mov [rbx+PG_MM_HEADERS],rax
.Lbeta_ok:
 xor ebp,ebp
 jmp .Lbeta_end
.Lbeta_bad:
 mov ebp,1
.Lbeta_end:
 mov rdi,[rsp]
 call free@PLT
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lbeta_modelInfo: .asciz "modelInfo"
.Lbeta_options: .asciz "options"
.Lbeta_headers: .asciz "headers"
.Lbeta_name: .asciz "anthropic-beta"
.Lbeta_prefix: .asciz "anthropic-beta: "
.Lbeta_thinking: .asciz "thinking"
.Lbeta_type: .asciz "type"
.Lbeta_enabled: .asciz "enabled"
.Lbeta_interleaved: .asciz "interleavedThinking"

.section .rodata
.Lbeta_tools_key: .asciz "tools"
.Lbeta_eager: .asciz "eager_input_streaming"
.Lbeta_fallbacks_key: .asciz "fallbacks"
.Lbeta_features_1: .asciz "anthropic-beta: interleaved-thinking-2025-05-14"
.Lbeta_features_2: .asciz "anthropic-beta: fine-grained-tool-streaming-2025-05-14"
.Lbeta_features_3: .asciz "anthropic-beta: interleaved-thinking-2025-05-14,fine-grained-tool-streaming-2025-05-14"
.Lbeta_features_4: .asciz "anthropic-beta: server-side-fallback-2026-07-01"
.Lbeta_features_5: .asciz "anthropic-beta: interleaved-thinking-2025-05-14,server-side-fallback-2026-07-01"
.Lbeta_features_6: .asciz "anthropic-beta: fine-grained-tool-streaming-2025-05-14,server-side-fallback-2026-07-01"
.Lbeta_features_7: .asciz "anthropic-beta: interleaved-thinking-2025-05-14,fine-grained-tool-streaming-2025-05-14,server-side-fallback-2026-07-01"

.section .data.rel.ro
.Lbeta_feature_headers: .quad 0,.Lbeta_features_1,.Lbeta_features_2,.Lbeta_features_3,.Lbeta_features_4,.Lbeta_features_5,.Lbeta_features_6,.Lbeta_features_7

/* PanGu's committed provider session identity supplies cache affinity. */
.text
.Lanthropic_affinity:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov qword ptr [rsp],0
 call pg_mimo_uses_anthropic
 test eax,eax
 jz .Laffinity_ok
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lbeta_options]
 call json_object_object_get@PLT
 mov r12,rax
 mov rdi,rax
 lea rsi,[rip+.Laffinity_retention]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jnz .Laffinity_retention_value
 lea rdi,[rip+.Laffinity_env]
 call getenv@PLT
 test rax,rax
 jz .Laffinity_identity
.Laffinity_retention_value:
 mov rdi,rax
 lea rsi,[rip+.Laffinity_none]
 call strcmp@PLT
 test eax,eax
 jz .Laffinity_ok
.Laffinity_identity:
 mov rdi,r12
 lea rsi,[rip+.Laffinity_sessionId]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Laffinity_identity_found
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Laffinity_sessionId]
 call json_object_object_get@PLT
.Laffinity_identity_found:
 mov r13,rax
 test rax,rax
 jz .Laffinity_ok
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Laffinity_bad
 mov rdi,r13
 call json_object_get_string_len@PLT
 mov [rsp+8],eax
 test eax,eax
 jz .Laffinity_ok
 mov rdi,r13
 call json_object_get_string@PLT
 mov r13,rax
 mov rdi,rax
 call strlen@PLT
 cmp eax,[rsp+8]
 jne .Laffinity_bad
 mov rdi,r13
 mov esi,13
 call strchr@PLT
 test rax,rax
 jnz .Laffinity_bad
 mov rdi,r13
 mov esi,10
 call strchr@PLT
 test rax,rax
 jnz .Laffinity_bad
 xor r14d,r14d
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Laffinity_provider]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Laffinity_endpoint
 mov rdi,rax
 lea rsi,[rip+.Laffinity_openrouter]
 call strcmp@PLT
 test eax,eax
 sete r14b
.Laffinity_endpoint:
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Laffinity_endpoint_key]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Laffinity_compat
 mov rdi,rax
 lea rsi,[rip+.Laffinity_domain]
 call strstr@PLT
 test rax,rax
 jz .Laffinity_compat
 mov r14d,1
.Laffinity_compat:
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lbeta_modelInfo]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Laffinity_compat_key]
 call json_object_object_get@PLT
 mov rbp,rax
 mov rdi,rax
 lea rsi,[rip+.Laffinity_send]
 call json_object_object_get@PLT
 test rax,rax
 jz .Laffinity_default
 mov rdi,rax
 call json_object_get_boolean@PLT
 test eax,eax
 jz .Laffinity_ok
 jmp .Laffinity_format
.Laffinity_default:
 test r14d,r14d
 jz .Laffinity_ok
.Laffinity_format:
 mov rdi,rbp
 lea rsi,[rip+.Laffinity_format_key]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Laffinity_format_ready
 mov rdi,rax
 lea rsi,[rip+.Laffinity_openrouter]
 call strcmp@PLT
 test eax,eax
 sete r14b
.Laffinity_format_ready:
 lea rbp,[rip+.Laffinity_native_prefix]
 test r14d,r14d
 jz .Laffinity_prefix
 lea rbp,[rip+.Laffinity_router_prefix]
.Laffinity_prefix:
 mov rdi,rbp
 call strlen@PLT
 mov [rsp+16],rax
 mov edi,[rsp+8]
 add rdi,rax
 inc rdi
 call malloc@PLT
 mov [rsp],rax
 test rax,rax
 jz .Laffinity_bad
 mov rdi,rax
 mov rsi,rbp
 mov rdx,[rsp+16]
 call memcpy@PLT
 mov rdi,[rsp]
 add rdi,[rsp+16]
 mov rsi,r13
 mov edx,[rsp+8]
 inc rdx
 call memcpy@PLT
 mov rdi,[rbx+PG_MM_HEADERS]
 mov rsi,[rsp]
 call curl_slist_append@PLT
 test rax,rax
 jz .Laffinity_bad
 mov [rbx+PG_MM_HEADERS],rax
.Laffinity_ok:
 xor ebp,ebp
 jmp .Laffinity_end
.Laffinity_bad:
 mov ebp,1
.Laffinity_end:
 mov rdi,[rsp]
 call free@PLT
 mov eax,ebp
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Laffinity_retention: .asciz "cacheRetention"
.Laffinity_env: .asciz "PANGU_CACHE_RETENTION"
.Laffinity_none: .asciz "none"
.Laffinity_sessionId: .asciz "sessionId"
.Laffinity_provider: .asciz "modelProvider"
.Laffinity_openrouter: .asciz "openrouter"
.Laffinity_domain: .asciz "openrouter.ai"
.Laffinity_endpoint_key: .asciz "endpoint"
.Laffinity_compat_key: .asciz "compat"
.Laffinity_send: .asciz "sendSessionAffinityHeaders"
.Laffinity_format_key: .asciz "sessionAffinityFormat"
.Laffinity_native_prefix: .asciz "x-session-affinity: "
.Laffinity_router_prefix: .asciz "x-session-id: "

.section .rodata
.Lgoogle_key: .asciz "x-goog-api-key: "

.section .rodata
.Lgoogle_unfinished: .asciz "{\"error\":{\"message\":\"Google stream ended without a finish reason\"}}"

/* Native credential binding keeps secrets outside the configuration. */
.text
.Lge_key_mode:
 push rbx
 lea rsi,[rip+.Lbeta_options]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lge_authMode]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lge_not_key
 mov rdi,rax
 lea rsi,[rip+.Lge_api_key]
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 pop rbx
 ret
.Lge_not_key:
 xor eax,eax
 pop rbx
 ret
/* Option string with environment fallback; borrowed through this configure. */
.Lge_option:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rsi
 mov r12,rdx
 lea rsi,[rip+.Lbeta_options]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,rbx
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jnz .Lge_option_end
 mov rdi,r12
 call getenv@PLT
.Lge_option_end:
 add rsp,8
 pop r12
 pop rbx
 ret
/* Complete only an absent endpoint on a detached candidate. */
.Lge_prepare:
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
 mov rdi,rbx
 call pg_mimo_config_google
 mov r12d,eax
 test eax,eax
 jz .Lge_ok
 mov rdi,rbx
 lea rsi,[rip+.Lbeta_options]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lge_authMode]
 lea rdx,[rsp+64]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lge_mode_checked
 mov rdi,[rsp+64]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lge_bad
 mov rdi,[rsp+64]
 call json_object_get_string@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lge_api_key]
 call strcmp@PLT
 test eax,eax
 jz .Lge_mode_checked
 cmp r12d,1
 je .Lge_bad
 mov rdi,r14
 lea rsi,[rip+.Lge_bearer]
 call strcmp@PLT
 test eax,eax
 jnz .Lge_bad
.Lge_mode_checked:
 mov rdi,rbx
 lea rsi,[rip+.Lcfg_endpoint]
 lea rdx,[rsp+64]
 call json_object_object_get_ex@PLT
 test eax,eax
 jnz .Lge_ok
 mov rdi,rbx
 lea rsi,[rip+.Lcfg_model]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov rsi,rax
 xor edi,edi
 xor edx,edx
 call curl_easy_escape@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lge_bad
 mov rdi,rbx
 lea rsi,[rip+.Lge_baseUrl]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lge_base_value
 mov rdi,rbx
 lea rsi,[rip+.Lbeta_modelInfo]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lge_baseUrl]
 call json_object_object_get@PLT
.Lge_base_value:
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 test rax,rax
 jz .Lge_default_base
 cmp byte ptr [rax],0
 je .Lge_default_base
 mov rdi,rax
 lea rsi,[rip+.Lge_marker]
 call strstr@PLT
 test rax,rax
 jnz .Lge_default_base
 mov qword ptr [rsp+72],1
 jmp .Lge_base_ready
.Lge_default_base:
 lea r14,[rip+.Lge_google_base]
 cmp r12d,1
 je .Lge_base_ready
 lea r14,[rip+.Lge_vertex_base]
.Lge_base_ready:
 mov rdi,r14
 call strdup@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lge_bad
 mov rdi,rax
 call strlen@PLT
 mov rdx,[rsp+8]
.Lge_trim:
 test rax,rax
 jz .Lge_bad
 cmp byte ptr [rdx+rax-1],'/'
 jne .Lge_route
 dec rax
 mov byte ptr [rdx+rax],0
 jmp .Lge_trim
.Lge_route:
 cmp r12d,1
 je .Lge_version_ready
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lge_version_path]
 call strstr@PLT
 test rax,rax
 jz .Lge_append_version
 movzx eax,byte ptr [rax+2]
 sub eax,'0'
 cmp eax,9
 jbe .Lge_version_ready
.Lge_append_version:
 lea rdi,[rsp+48]
 lea rsi,[rip+.Lge_version_format]
 mov rdx,[rsp+8]
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lge_bad
 mov rdi,[rsp+8]
 call free@PLT
 mov rax,[rsp+48]
 mov [rsp+8],rax
 mov qword ptr [rsp+48],0
.Lge_version_ready:
 cmp r12d,1
 je .Lge_google_route
 mov rdi,rbx
 call .Lge_key_mode
 test eax,eax
 jnz .Lge_publisher_route
 mov rdi,rbx
 lea rsi,[rip+.Lge_project]
 lea rdx,[rip+.Lge_project_env]
 call .Lge_option
 test rax,rax
 jnz .Lge_project_ready
 lea rdi,[rip+.Lge_project_alias]
 call getenv@PLT
.Lge_project_ready:
 test rax,rax
 jz .Lge_bad
 cmp byte ptr [rax],0
 je .Lge_bad
 mov rsi,rax
 xor edi,edi
 xor edx,edx
 call curl_easy_escape@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lge_bad
 mov rdi,rbx
 lea rsi,[rip+.Lge_location]
 lea rdx,[rip+.Lge_location_env]
 call .Lge_option
 test rax,rax
 jz .Lge_bad
 cmp byte ptr [rax],0
 je .Lge_bad
 mov [rsp+56],rax
 mov rsi,rax
 xor edi,edi
 xor edx,edx
 call curl_easy_escape@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lge_bad
 # Use the regional host only when no custom base overrides it.
 cmp qword ptr [rsp+72],0
 jne .Lge_project_route
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lge_vertex_base]
 call strcmp@PLT
 test eax,eax
 jnz .Lge_project_route
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lge_global]
 call strcmp@PLT
 test eax,eax
 jz .Lge_project_route
 mov rdi,[rsp+8]
 call free@PLT
 mov qword ptr [rsp+8],0
 lea rdi,[rsp+8]
 lea rsi,[rip+.Lge_regional_base]
 mov rdx,[rsp+24]
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lge_bad
.Lge_project_route:
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lge_projects_path]
 call strstr@PLT
 test rax,rax
 jnz .Lge_publisher_route
 lea rdi,[rsp+32]
 lea rsi,[rip+.Lge_vertex_format]
 mov rdx,[rsp+8]
 mov rcx,[rsp+16]
 mov r8,[rsp+24]
 mov r9,[rsp]
 xor eax,eax
 call asprintf@PLT
 jmp .Lge_formatted
.Lge_publisher_route:
 lea rdi,[rsp+32]
 lea rsi,[rip+.Lge_publisher_format]
 mov rdx,[rsp+8]
 mov rcx,[rsp]
 xor eax,eax
 call asprintf@PLT
 jmp .Lge_formatted
.Lge_google_route:
 lea rdi,[rsp+32]
 lea rsi,[rip+.Lge_google_format]
 mov rdx,[rsp+8]
 mov rcx,[rsp]
 xor eax,eax
 call asprintf@PLT
.Lge_formatted:
 test eax,eax
 js .Lge_bad
 cmp eax,2048
 jae .Lge_bad
 mov rdi,[rsp+32]
 mov esi,eax
 call json_object_new_string_len@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lge_bad
 mov rdi,rbx
 lea rsi,[rip+.Lcfg_endpoint]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lge_bad
 mov qword ptr [rsp+40],0
.Lge_ok:
 xor ebp,ebp
 jmp .Lge_end
.Lge_bad:
 mov ebp,1
.Lge_end:
 mov rdi,[rsp]
 call curl_free@PLT
 mov rdi,[rsp+16]
 call curl_free@PLT
 mov rdi,[rsp+24]
 call curl_free@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov rdi,[rsp+32]
 call free@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call free@PLT
 mov eax,ebp
 add rsp,80
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lge_authMode: .asciz "authMode"
.Lge_api_key: .asciz "api-key"
.Lge_baseUrl: .asciz "baseUrl"
.Lge_marker: .asciz "{location}"
.Lge_google_base: .asciz "https://generativelanguage.googleapis.com/v1beta"
.Lge_vertex_base: .asciz "https://aiplatform.googleapis.com/v1"
.Lge_global: .asciz "global"
.Lge_regional_base: .asciz "https://%s-aiplatform.googleapis.com/v1"
.Lge_projects_path: .asciz "/projects/"
.Lge_google_format: .asciz "%s/models/%s:streamGenerateContent?alt=sse"
.Lge_publisher_format: .asciz "%s/publishers/google/models/%s:streamGenerateContent?alt=sse"
.Lge_vertex_format: .asciz "%s/projects/%s/locations/%s/publishers/google/models/%s:streamGenerateContent?alt=sse"
.Lge_project: .asciz "project"
.Lge_location: .asciz "location"
.Lge_project_env: .asciz "GOOGLE_CLOUD_PROJECT"
.Lge_project_alias: .asciz "GCLOUD_PROJECT"
.Lge_location_env: .asciz "GOOGLE_CLOUD_LOCATION"

.section .rodata
.Lge_version_path: .asciz "/v"
.Lge_version_format: .asciz "%s/v1"

.section .rodata
.Lge_bearer: .asciz "bearer"

/* Merge explicit provider headers case-insensitively before the transport owns them. */
.text
.Lprovider_headers:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,96
 mov rbx,rdi
 mov r14,rsi
 xor eax,eax
 mov ecx,12
 mov rdi,rsp
 rep stosq
 mov [rsp+80],r14
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lph_bad
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lbeta_modelInfo]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lbeta_headers]
 call json_object_object_get@PLT
 mov [rsp+8],rax
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lbeta_headers]
 call json_object_object_get@PLT
 mov [rsp+16],rax
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lbeta_options]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lbeta_headers]
 call json_object_object_get@PLT
 mov [rsp+24],rax
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 mov [rsp+72],eax
 test eax,eax
 jz .Lph_sources_ready
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
.Lph_sources_ready:
 xor r12d,r12d
.Lph_layer:
 mov rdi,[rsp+8+r12*8]
 test rdi,rdi
 jz .Lph_layer_next
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lph_bad
 mov rdi,[rsp+8+r12*8]
 call json_object_iter_begin@PLT
 mov [rsp+32],rax
 mov rdi,[rsp+8+r12*8]
 call json_object_iter_end@PLT
 mov [rsp+40],rax
.Lph_each:
 lea rdi,[rsp+32]
 lea rsi,[rsp+40]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lph_layer_next
 lea rdi,[rsp+32]
 call json_object_iter_peek_name@PLT
 mov rdi,rax
 call strdup@PLT
 mov [rsp+48],rax
 test rax,rax
 jz .Lph_bad
 mov r13,rax
 cmp byte ptr [r13],0
 je .Lph_bad
.Lph_name:
 movzx ebp,byte ptr [r13]
 test ebp,ebp
 jz .Lph_value
 cmp ebp,65
 jb .Lph_name_check
 cmp ebp,90
 ja .Lph_name_check
 add ebp,32
 mov [r13],bpl
.Lph_name_check:
 lea rdi,[rip+.Lph_tokens]
 mov esi,ebp
 call strchr@PLT
 test rax,rax
 jz .Lph_bad
 inc r13
 jmp .Lph_name
.Lph_value:
 lea rdi,[rsp+32]
 call json_object_iter_peek_value@PLT
 mov r14,rax
 test rax,rax
 jz .Lph_delete
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lph_bad
 mov rdi,r14
 call json_object_get_string_len@PLT
 mov ebp,eax
 mov rdi,r14
 call json_object_get_string@PLT
 mov r13,rax
 mov rdi,rax
 call strlen@PLT
 cmp rax,rbp
 jne .Lph_bad
 mov rdi,r13
 lea rsi,[rip+.Lph_newlines]
 call strpbrk@PLT
 test rax,rax
 jnz .Lph_bad
 mov rdi,r14
 call json_object_get@PLT
 mov [rsp+64],rax
 mov rdi,[rsp]
 mov rsi,[rsp+48]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lph_bad
 mov qword ptr [rsp+64],0
 jmp .Lph_next
.Lph_delete:
 mov rdi,[rsp]
 mov rsi,[rsp+48]
 call json_object_object_del@PLT
.Lph_next:
 mov rdi,[rsp+48]
 call free@PLT
 mov qword ptr [rsp+48],0
 lea rdi,[rsp+32]
 call json_object_iter_next@PLT
 jmp .Lph_each
.Lph_layer_next:
 test r12d,r12d
 jnz .Lcp_layer_done
 mov rdi,rbx
 mov rsi,[rsp+80]
 mov rdx,[rsp]
 call .Lcp_headers
 test eax,eax
 jnz .Lph_bad
.Lcp_layer_done:
 inc r12
 cmp r12,3
 jb .Lph_layer
 mov rdi,[rsp]
 call json_object_iter_begin@PLT
 mov [rsp+32],rax
 mov rdi,[rsp]
 call json_object_iter_end@PLT
 mov [rsp+40],rax
.Lph_emit:
 lea rdi,[rsp+32]
 lea rsi,[rsp+40]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz .Lph_ok
 lea rdi,[rsp+32]
 call json_object_iter_peek_name@PLT
 mov r12,rax
 cmp dword ptr [rsp+72],0
 je .Lph_emit_name
 mov rdi,r12
 call .Lbe_reserved_header
 test eax,eax
 jz .Lph_emit_name
 lea rdi,[rsp+32]
 call json_object_iter_next@PLT
 jmp .Lph_emit
.Lph_emit_name:
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_codex@PLT
 test eax,eax
 jz .Lcd_emit_allowed
 mov rdi,r12
 call .Lcd_reserved
 test eax,eax
 jz .Lcd_emit_allowed
 lea rdi,[rsp+32]
 call json_object_iter_next@PLT
 jmp .Lph_emit
.Lcd_emit_allowed:
 mov rdi,r12
 call strlen@PLT
 mov rbp,rax
 lea rdi,[rsp+32]
 call json_object_iter_peek_value@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 lea rsi,[rip+.Lph_format]
 cmp byte ptr [r14],0
 jne .Lph_format_ready
 lea rsi,[rip+.Lph_empty]
.Lph_format_ready:
 lea rdi,[rsp+56]
 mov rdx,r12
 mov rcx,r14
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lph_bad
 lea r13,[rbx+PG_MM_HEADERS]
.Lph_remove:
 mov r14,[r13]
 test r14,r14
 jz .Lph_append
 mov rdi,[r14]
 mov rsi,r12
 mov rdx,rbp
 call strncasecmp@PLT
 test eax,eax
 jnz .Lph_remove_next
 mov rax,[r14]
 mov al,[rax+rbp]
 cmp al,58
 je .Lph_unlink
 cmp al,59
 jne .Lph_remove_next
.Lph_unlink:
 mov rax,[r14+8]
 mov [r13],rax
 mov qword ptr [r14+8],0
 mov rdi,r14
 call curl_slist_free_all@PLT
 jmp .Lph_remove
.Lph_remove_next:
 lea r13,[r14+8]
 jmp .Lph_remove
.Lph_append:
 mov rdi,[rbx+PG_MM_HEADERS]
 mov rsi,[rsp+56]
 call curl_slist_append@PLT
 test rax,rax
 jz .Lph_bad
 mov [rbx+PG_MM_HEADERS],rax
 mov rdi,[rsp+56]
 call free@PLT
 mov qword ptr [rsp+56],0
 lea rdi,[rsp+32]
 call json_object_iter_next@PLT
 jmp .Lph_emit
.Lph_ok:
 xor ebp,ebp
 jmp .Lph_cleanup
.Lph_bad:
 mov ebp,1
.Lph_cleanup:
 mov rdi,[rsp+48]
 call free@PLT
 mov rdi,[rsp+56]
 call free@PLT
 mov rdi,[rsp+64]
 call json_object_put@PLT
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
.Lph_tokens: .asciz "abcdefghijklmnopqrstuvwxyz0123456789!#$%&'*+-.^_`|~"
.Lph_newlines: .asciz "\r\n"
.Lph_format: .asciz "%s: %s"
.Lph_empty: .asciz "%s;"

.text
.Laz_is:
 push rbx
 mov rbx,rdi
 lea rsi,[rip+.Laz_api_keyname]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Laz_api_value
 mov rdi,rbx
 lea rsi,[rip+.Lbeta_modelInfo]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Laz_api_keyname]
 call json_object_object_get@PLT
.Laz_api_value:
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Laz_no
 mov rdi,rax
 lea rsi,[rip+.Laz_api]
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 pop rbx
 ret
.Laz_no:
 xor eax,eax
 pop rbx
 ret
/* Resolve public Azure connection fields on the detached configuration. */
.Laz_prepare:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 xor eax,eax
 mov ecx,6
 mov rdi,rsp
 rep stosq
 mov rdi,rbx
 call .Laz_is
 test eax,eax
 jz .Laz_ok
 mov rdi,rbx
 lea rsi,[rip+.Lbeta_options]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Laz_deployment]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Laz_map_select
 cmp byte ptr [rax],0
 jne .Laz_deployment_selected
.Laz_map_select:
 mov rdi,rbx
 call .Laz_map_deployment
 mov [rsp+32],rax
 test edx,edx
 jnz .Laz_bad
 test rax,rax
 jz .Laz_endpoint
.Laz_deployment_selected:
 mov rdi,rax
 call json_object_new_string@PLT
 test rax,rax
 jz .Laz_bad
 mov [rsp+24],rax
 mov rdi,rbx
 lea rsi,[rip+.Laz_deployment]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Laz_bad
 mov qword ptr [rsp+24],0
.Laz_endpoint:
 mov rdi,rbx
 lea rsi,[rip+.Lcfg_endpoint]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Laz_ok
 mov rdi,rbx
 lea rsi,[rip+.Laz_base]
 lea rdx,[rip+.Laz_base_env]
 call .Lge_option
 mov rdi,rax
 call .Laz_nonblank
 test rax,rax
 jnz .Laz_copy_base
 lea rdi,[rip+.Laz_base_env]
 call getenv@PLT
 mov rdi,rax
 call .Laz_nonblank
 test rax,rax
 jnz .Laz_copy_base
.Laz_resource:
 mov rdi,rbx
 lea rsi,[rip+.Laz_resource_name]
 lea rdx,[rip+.Laz_resource_env]
 call .Lge_option
 test rax,rax
 jz .Laz_model_base
 cmp byte ptr [rax],0
 je .Laz_model_base
 mov rcx,rax
 lea rdi,[rsp]
 lea rsi,[rip+.Laz_resource_format]
 mov rdx,rcx
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Laz_bad
 jmp .Laz_base_ready
.Laz_model_base:
 mov rdi,rbx
 lea rsi,[rip+.Lge_baseUrl]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Laz_base_value
 mov rdi,rbx
 lea rsi,[rip+.Lbeta_modelInfo]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lge_baseUrl]
 call json_object_object_get@PLT
.Laz_base_value:
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Laz_bad
.Laz_copy_base:
 mov rdi,rax
 call strdup@PLT
 mov [rsp],rax
 test rax,rax
 jz .Laz_bad
.Laz_base_ready:
 mov rdi,[rsp]
 call .Laz_trim_space
 mov r13,rax
 mov rdi,rax
 call strlen@PLT
 mov r12,rax
.Laz_trim:
 test r12,r12
 jz .Laz_bad
 cmp byte ptr [r13+r12-1],47
 jne .Laz_version
 dec r12
 mov byte ptr [r13+r12],0
 jmp .Laz_trim
.Laz_version:
 mov rdi,rbx
 lea rsi,[rip+.Laz_version_option]
 lea rdx,[rip+.Laz_version_env]
 call .Lge_option
 test rax,rax
 jz .Laz_default_version
 cmp byte ptr [rax],0
 jne .Laz_escape_version
.Laz_default_version:
 lea rax,[rip+.Laz_v1]
.Laz_escape_version:
 xor edi,edi
 mov rsi,rax
 xor edx,edx
 call curl_easy_escape@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Laz_bad
 mov rdi,r13
 mov rsi,rax
 call .Laz_route
 mov [rsp+16],rax
 test rax,rax
 jz .Laz_bad
 mov rdi,rax
 call strlen@PLT
 cmp eax,2048
 jae .Laz_bad
 mov rdi,[rsp+16]
 mov esi,eax
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Laz_bad
 mov [rsp+24],rax
 mov rdi,rbx
 lea rsi,[rip+.Lcfg_endpoint]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Laz_bad
 mov qword ptr [rsp+24],0
.Laz_ok:
 xor ebp,ebp
 jmp .Laz_end
.Laz_bad:
 mov ebp,1
.Laz_end:
 mov rdi,[rsp+32]
 call free@PLT
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call curl_free@PLT
 mov rdi,[rsp+16]
 call free@PLT
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
.Laz_api: .asciz "azure-openai-responses"
.Laz_key: .asciz "api-key: "
.Laz_deployment: .asciz "azureDeploymentName"
.Laz_base: .asciz "azureBaseUrl"
.Laz_base_env: .asciz "AZURE_OPENAI_BASE_URL"
.Laz_resource_name: .asciz "azureResourceName"
.Laz_resource_env: .asciz "AZURE_OPENAI_RESOURCE_NAME"
.Laz_version_option: .asciz "azureApiVersion"
.Laz_version_env: .asciz "AZURE_OPENAI_API_VERSION"
.Laz_v1: .asciz "v1"
.Laz_resource_format: .asciz "https://%s.openai.azure.com/openai/v1"
.Laz_endpoint_format: .asciz "%s/responses?api-version=%s"

.Laz_api_keyname: .asciz "api"

.text
/* In-place ASCII trim for URL/config strings; returned pointer is borrowed. */
.Laz_trim_space:
 mov rax,rdi
.Laz_space_head:
 movzx edx,byte ptr [rax]
 cmp edx,32
 je .Laz_space_advance
 cmp edx,9
 jb .Laz_space_tail
 cmp edx,13
 ja .Laz_space_tail
.Laz_space_advance:
 inc rax
 jmp .Laz_space_head
.Laz_space_tail:
 mov rcx,rax
.Laz_space_end:
 cmp byte ptr [rcx],0
 je .Laz_space_back
 inc rcx
 jmp .Laz_space_end
.Laz_space_back:
 cmp rcx,rax
 je .Laz_space_done
 movzx edx,byte ptr [rcx-1]
 cmp edx,32
 je .Laz_space_cut
 cmp edx,9
 jb .Laz_space_done
 cmp edx,13
 ja .Laz_space_done
.Laz_space_cut:
 dec rcx
 mov byte ptr [rcx],0
 jmp .Laz_space_back
.Laz_space_done:
 ret
.Laz_suffix:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 call strlen@PLT
 mov r13,rax
 mov rdi,r12
 call strlen@PLT
 cmp r13,rax
 jb .Laz_suffix_no
 sub r13,rax
 lea rdi,[rbx+r13]
 mov rsi,r12
 call strcasecmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 jmp .Laz_suffix_end
.Laz_suffix_no:
 xor eax,eax
.Laz_suffix_end:
 pop r13
 pop r12
 pop rbx
 ret
/* libcurl owns URL syntax; Assembly owns the Azure path and query policy. */
.Laz_route:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov r12,rdi
 mov r13,rsi
 xor eax,eax
 mov ecx,8
 mov rdi,rsp
 rep stosq
 call curl_url@PLT
 mov [rsp],rax
 test rax,rax
 jz .Laz_route_end
 mov rdi,rax
 xor esi,esi
 mov rdx,r12
 xor ecx,ecx
 call curl_url_set@PLT
 test eax,eax
 jnz .Laz_route_end
 mov rdi,[rsp]
 mov esi,5
 lea rdx,[rsp+8]
 xor ecx,ecx
 call curl_url_get@PLT
 test eax,eax
 jnz .Laz_route_end
 mov rdi,[rsp]
 mov esi,7
 lea rdx,[rsp+16]
 xor ecx,ecx
 call curl_url_get@PLT
 test eax,eax
 jnz .Laz_route_end
 mov rdi,[rsp+8]
 lea rsi,[rip+.Laz_host_openai]
 call .Laz_suffix
 test eax,eax
 jnz .Laz_route_host
 mov rdi,[rsp+8]
 lea rsi,[rip+.Laz_host_cognitive]
 call .Laz_suffix
 test eax,eax
 jnz .Laz_route_host
 mov rdi,[rsp+8]
 lea rsi,[rip+.Laz_host_ai]
 call .Laz_suffix
 test eax,eax
 jz .Laz_route_path
.Laz_route_host:
 mov rdi,[rsp+16]
 call strlen@PLT
 mov rbx,[rsp+16]
.Laz_route_trim:
 test rax,rax
 jz .Laz_route_normalize
 cmp byte ptr [rbx+rax-1],47
 jne .Laz_route_path_test
 dec rax
 mov byte ptr [rbx+rax],0
 jmp .Laz_route_trim
.Laz_route_path_test:
 mov rdi,rbx
 lea rsi,[rip+.Laz_openai_path]
 call strcmp@PLT
 test eax,eax
 jz .Laz_route_normalize
 mov rdi,rbx
 lea rsi,[rip+.Laz_responses_path]
 call strcmp@PLT
 test eax,eax
 jnz .Laz_route_path
.Laz_route_normalize:
 mov rdi,[rsp+16]
 call curl_free@PLT
 lea rdi,[rip+.Laz_v1_path]
 call strdup@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Laz_route_end
 mov rdi,[rsp]
 mov esi,8
 xor edx,edx
 xor ecx,ecx
 call curl_url_set@PLT
 test eax,eax
 jnz .Laz_route_end
.Laz_route_path:
 mov rdi,[rsp+16]
 call strlen@PLT
 mov rbx,[rsp+16]
.Laz_route_path_trim:
 test rax,rax
 jz .Laz_route_join
 cmp byte ptr [rbx+rax-1],47
 jne .Laz_route_join
 dec rax
 mov byte ptr [rbx+rax],0
 jmp .Laz_route_path_trim
.Laz_route_join:
 lea rdi,[rsp+24]
 lea rsi,[rip+.Laz_path_format]
 mov rdx,rbx
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Laz_route_end
 mov rdi,[rsp]
 mov esi,7
 mov rdx,[rsp+24]
 xor ecx,ecx
 call curl_url_set@PLT
 test eax,eax
 jnz .Laz_route_end
 mov rdi,[rsp]
 mov esi,8
 lea rdx,[rsp+56]
 xor ecx,ecx
 call curl_url_get@PLT
 test eax,eax
 jnz .Laz_route_query_ready
 mov rbx,[rsp+56]
.Laz_route_query_each:
 cmp byte ptr [rbx],0
 je .Laz_route_query_set
 mov rdi,rbx
 lea rsi,[rip+.Laz_query_key]
 mov edx,12
 call strncmp@PLT
 test eax,eax
 jnz .Laz_route_query_next
 mov rdi,rbx
 mov esi,38
 call strchr@PLT
 test rax,rax
 jz .Laz_route_query_last
 lea r14,[rax+1]
 mov rdi,r14
 call strlen@PLT
 mov rdi,rbx
 mov rsi,r14
 lea rdx,[rax+1]
 call memmove@PLT
 jmp .Laz_route_query_each
.Laz_route_query_last:
 mov byte ptr [rbx],0
 cmp rbx,[rsp+56]
 je .Laz_route_query_set
 mov byte ptr [rbx-1],0
 jmp .Laz_route_query_set
.Laz_route_query_next:
 mov rdi,rbx
 mov esi,38
 call strchr@PLT
 test rax,rax
 jz .Laz_route_query_set
 lea rbx,[rax+1]
 jmp .Laz_route_query_each
.Laz_route_query_set:
 mov rdi,[rsp]
 mov esi,8
 mov rdx,[rsp+56]
 xor ecx,ecx
 call curl_url_set@PLT
 test eax,eax
 jnz .Laz_route_end
.Laz_route_query_ready:
 lea rdi,[rsp+32]
 lea rsi,[rip+.Laz_query_format]
 mov rdx,r13
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Laz_route_end
 mov rdi,[rsp]
 mov esi,8
 mov rdx,[rsp+32]
 mov ecx,256
 call curl_url_set@PLT
 test eax,eax
 jnz .Laz_route_end
 mov rdi,[rsp]
 xor esi,esi
 lea rdx,[rsp+40]
 xor ecx,ecx
 call curl_url_get@PLT
 test eax,eax
 jnz .Laz_route_end
 mov rdi,[rsp+40]
 call strdup@PLT
 mov [rsp+48],rax
.Laz_route_end:
 mov rdi,[rsp+56]
 call curl_free@PLT
 mov rdi,[rsp]
 call curl_url_cleanup@PLT
 mov rdi,[rsp+8]
 call curl_free@PLT
 mov rdi,[rsp+16]
 call curl_free@PLT
 mov rdi,[rsp+24]
 call free@PLT
 mov rdi,[rsp+32]
 call free@PLT
 mov rdi,[rsp+40]
 call curl_free@PLT
 mov rax,[rsp+48]
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Laz_host_openai: .asciz ".openai.azure.com"
.Laz_host_cognitive: .asciz ".cognitiveservices.azure.com"
.Laz_host_ai: .asciz ".ai.azure.com"
.Laz_openai_path: .asciz "/openai"
.Laz_responses_path: .asciz "/openai/v1/responses"
.Laz_v1_path: .asciz "/openai/v1"
.Laz_path_format: .asciz "%s/responses"
.Laz_query_format: .asciz "api-version=%s"

.text
.Laz_map_deployment:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 xor eax,eax
 mov [rsp],rax
 mov [rsp+8],rax
 mov [rsp+16],rax
 lea rdi,[rip+.Laz_map_env]
 call getenv@PLT
 test rax,rax
 jz .Laz_map_ok
 mov rdi,rax
 call strdup@PLT
 mov [rsp],rax
 test rax,rax
 jz .Laz_map_bad
 mov r14,rax
 mov rdi,rbx
 lea rsi,[rip+.Lcfg_model]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov r12,rax
.Laz_map_entry:
 mov rdi,r14
 mov esi,44
 call strchr@PLT
 xor r13d,r13d
 test rax,rax
 jz .Laz_map_pair
 mov byte ptr [rax],0
 lea r13,[rax+1]
.Laz_map_pair:
 mov rdi,r14
 call .Laz_trim_space
 mov r14,rax
 mov rdi,rax
 mov esi,61
 call strchr@PLT
 test rax,rax
 jz .Laz_map_next
 mov byte ptr [rax],0
 lea rbp,[rax+1]
 cmp byte ptr [r14],0
 je .Laz_map_next
 cmp byte ptr [rbp],0
 je .Laz_map_next
 mov rdi,rbp
 mov esi,61
 call strchr@PLT
 test rax,rax
 jz .Laz_map_trim_key
 mov byte ptr [rax],0
.Laz_map_trim_key:
 mov rdi,r14
 call .Laz_trim_space
 mov rdi,rax
 mov rsi,r12
 call strcmp@PLT
 test eax,eax
 jnz .Laz_map_next
 mov rdi,rbp
 call .Laz_trim_space
 mov [rsp+8],rax
.Laz_map_next:
 mov r14,r13
 test r14,r14
 jnz .Laz_map_entry
 mov rdi,[rsp+8]
 test rdi,rdi
 jz .Laz_map_ok
 cmp byte ptr [rdi],0
 je .Laz_map_ok
 call strdup@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Laz_map_bad
.Laz_map_ok:
 xor ebp,ebp
 jmp .Laz_map_end
.Laz_map_bad:
 mov ebp,1
.Laz_map_end:
 mov rdi,[rsp]
 call free@PLT
 mov rax,[rsp+16]
 mov edx,ebp
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Laz_map_env: .asciz "AZURE_OPENAI_DEPLOYMENT_NAME_MAP"

.text
.Laz_nonblank:
 mov rax,rdi
 test rax,rax
 jz .Laz_nonblank_end
.Laz_nonblank_next:
 movzx edx,byte ptr [rax]
 test edx,edx
 jz .Laz_nonblank_empty
 cmp edx,32
 je .Laz_nonblank_advance
 cmp edx,9
 jb .Laz_nonblank_end
 cmp edx,13
 ja .Laz_nonblank_end
.Laz_nonblank_advance:
 inc rax
 jmp .Laz_nonblank_next
.Laz_nonblank_empty:
 xor eax,eax
.Laz_nonblank_end:
 ret

.section .rodata
.Laz_query_key: .asciz "api-version="

.text
.Lbe_prepare:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 xor eax,eax
 mov ecx,6
 mov rdi,rsp
 rep stosq
 mov rdi,rbx
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lbe_ok
 mov rdi,rbx
 call .Lbr_resolve
 test eax,eax
 jnz .Lbe_bad
 mov rdi,rbx
 lea rsi,[rip+.Lcfg_endpoint]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lbe_ok
 mov rdi,rbx
 lea rsi,[rip+.Lge_baseUrl]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lbe_base_value
 mov rdi,rbx
 lea rsi,[rip+.Lbeta_modelInfo]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lge_baseUrl]
 call json_object_object_get@PLT
.Lbe_base_value:
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lbe_region
 mov rdi,rax
 call strdup@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbe_bad
 jmp .Lbe_model
.Lbe_region:
 mov rdi,rbx
 lea rsi,[rip+.Lbr_region]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
.Lbe_region_ready:
 lea rdi,[rsp]
 lea rsi,[rip+.Lbe_base_format]
 mov rdx,rax
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lbe_bad
.Lbe_model:
 mov rdi,[rsp]
 call strlen@PLT
 mov r13,[rsp]
.Lbe_trim:
 test rax,rax
 jz .Lbe_bad
 cmp byte ptr [r13+rax-1],47
 jne .Lbe_escape
 dec rax
 mov byte ptr [r13+rax],0
 jmp .Lbe_trim
.Lbe_escape:
 mov rdi,rbx
 lea rsi,[rip+.Lcfg_model]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 xor edi,edi
 mov rsi,rax
 xor edx,edx
 call curl_easy_escape@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lbe_bad
 lea rdi,[rsp+16]
 lea rsi,[rip+.Lbe_endpoint_format]
 mov rdx,r13
 mov rcx,rax
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lbe_bad
 cmp eax,2048
 jae .Lbe_bad
 mov rdi,[rsp+16]
 mov esi,eax
 call json_object_new_string_len@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lbe_bad
 mov rdi,rbx
 lea rsi,[rip+.Lcfg_endpoint]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lbe_bad
 mov qword ptr [rsp+24],0
.Lbe_ok:
 xor ebp,ebp
 jmp .Lbe_end
.Lbe_bad:
 mov ebp,1
.Lbe_end:
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call curl_free@PLT
 mov rdi,[rsp+16]
 call free@PLT
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
.Lbe_region_key: .asciz "region"
.Lbe_region_env: .asciz "AWS_REGION"
.Lbe_default_region_env: .asciz "AWS_DEFAULT_REGION"
.Lbe_region_default: .asciz "us-east-1"
.Lbe_base_format: .asciz "https://bedrock-runtime.%s.amazonaws.com"
.Lbe_endpoint_format: .asciz "%s/model/%s/converse-stream"

.text
/* Shared environment credential validation before crossing the libcurl boundary. */
.Lbe_credential:
 push rbx
 mov rbx,rdi
 test rdi,rdi
 jz .Lbe_credential_bad
 call strlen@PLT
 test rax,rax
 jz .Lbe_credential_bad
 cmp rax,8192
 ja .Lbe_credential_bad
 mov rdi,rbx
 lea rsi,[rip+.Lph_newlines]
 call strpbrk@PLT
 test rax,rax
 setne al
 movzx eax,al
 pop rbx
 ret
.Lbe_credential_bad:
 mov eax,1
 pop rbx
 ret
/* Environment switches follow the upstream exact "1" convention. */
.Lbe_enabled:
 sub rsp,8
 call getenv@PLT
 test rax,rax
 jz .Lbe_enabled_no
 cmp byte ptr [rax],49
 jne .Lbe_enabled_no
 cmp byte ptr [rax+1],0
 sete al
 movzx eax,al
 add rsp,8
 ret
.Lbe_enabled_no:
 xor eax,eax
 add rsp,8
 ret
/* libcurl copies USERPWD/SIGV4 strings; temporary credentials never enter JSON. */
.Lbe_sign:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov r12,rsi
 xor eax,eax
 mov ecx,6
 mov rdi,rsp
 rep stosq
 lea rdi,[rip+.Lbe_skip_env]
 call .Lbe_enabled
 test eax,eax
 jz .Lbe_sign_load
 lea r13,[rip+.Lbe_dummy_access]
 lea r14,[rip+.Lbe_dummy_secret]
 jmp .Lbe_sign_validate
.Lbe_sign_load:
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call .Lapf_load
 mov [rsp+24],rax
 test rax,rax
 jz .Lbe_sign_bad
 cmp dword ptr [rax+32],0
 jne .Lbe_sign_bad
 mov r13,[rax]
 mov r14,[rax+8]
.Lbe_sign_validate:
 mov rdi,r13
 call .Lbe_credential
 test eax,eax
 jnz .Lbe_sign_bad
 mov rdi,r13
 mov esi,58
 call strchr@PLT
 test rax,rax
 jnz .Lbe_sign_bad
 mov rdi,r14
 call .Lbe_credential
 test eax,eax
 jnz .Lbe_sign_bad
 lea rdi,[rsp]
 lea rsi,[rip+.Lbe_userpwd_format]
 mov rdx,r13
 mov rcx,r14
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lbe_sign_bad
 mov rdi,r12
 mov esi,PG_MM_CURLOPT_USERPWD
 mov rdx,[rsp]
 xor eax,eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lbe_sign_bad
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lbr_region]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
.Lbe_sign_region:
 mov r13,rax
 mov rdi,rax
 call .Lbe_credential
 test eax,eax
 jnz .Lbe_sign_bad
 lea rdi,[rsp+8]
 lea rsi,[rip+.Lbe_sig_format]
 mov rdx,r13
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lbe_sign_bad
 mov rdi,r12
 mov esi,PG_MM_CURLOPT_AWS_SIGV4
 mov rdx,[rsp+8]
 xor eax,eax
 call curl_easy_setopt@PLT
 test eax,eax
 jnz .Lbe_sign_bad
 mov rax,[rsp+24]
 test rax,rax
 jz .Lbe_sign_ok
 mov rax,[rax+16]
 mov r13,rax
 test rax,rax
 jz .Lbe_sign_ok
 cmp byte ptr [rax],0
 je .Lbe_sign_ok
 mov rdi,rax
 call .Lbe_credential
 test eax,eax
 jnz .Lbe_sign_bad
 lea rdi,[rsp+16]
 lea rsi,[rip+.Lbe_token_format]
 mov rdx,r13
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lbe_sign_bad
 mov rdi,[rbx+PG_MM_HEADERS]
 mov rsi,[rsp+16]
 call curl_slist_append@PLT
 test rax,rax
 jz .Lbe_sign_bad
 mov [rbx+PG_MM_HEADERS],rax
.Lbe_sign_ok:
 xor ebp,ebp
 jmp .Lbe_sign_end
.Lbe_sign_bad:
 mov ebp,1
.Lbe_sign_end:
 mov rdi,[rsp]
 test rdi,rdi
 jz .Lbe_sign_free
 call strlen@PLT
 mov rdi,[rsp]
 mov rdx,rax
 xor esi,esi
 call memset@PLT
.Lbe_sign_free:
 mov rdi,[rsp+24]
 call .Lapf_free
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov rdi,[rsp+16]
 call free@PLT
 mov eax,ebp
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lbe_skip_env: .asciz "AWS_BEDROCK_SKIP_AUTH"
.Lbe_http1_env: .asciz "AWS_BEDROCK_FORCE_HTTP1"
.Lbe_dummy_access: .asciz "dummy-access-key"
.Lbe_dummy_secret: .asciz "dummy-secret-key"
.Lbe_bearer_env: .asciz "AWS_BEARER_TOKEN_BEDROCK"
.Lbe_access_env: .asciz "AWS_ACCESS_KEY_ID"
.Lbe_secret_env: .asciz "AWS_SECRET_ACCESS_KEY"
.Lbe_token_env: .asciz "AWS_SESSION_TOKEN"
.Lbe_profile_key: .asciz "profile"
.Lbe_userpwd_format: .asciz "%s:%s"
.Lbe_sig_format: .asciz "aws:amz:%s:bedrock"
.Lbe_token_format: .asciz "X-Amz-Security-Token: %s"

.text
.Lbe_reserved_header:
 push rbx
 mov rbx,rdi
 lea rsi,[rip+.Lbe_authorization_key]
 call strcmp@PLT
 test eax,eax
 jz .Lbe_reserved_yes
 mov rdi,rbx
 lea rsi,[rip+.Lbe_host_key]
 call strcmp@PLT
 test eax,eax
 jz .Lbe_reserved_yes
 mov rdi,rbx
 lea rsi,[rip+.Lbe_amz_prefix]
 mov edx,6
 call strncmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 pop rbx
 ret
.Lbe_reserved_yes:
 mov eax,1
 pop rbx
 ret
.section .rodata
.Lbe_authorization_key: .asciz "authorization"
.Lbe_host_key: .asciz "host"
.Lbe_amz_prefix: .asciz "x-amz-"

.text
.Lbe_bearer_header:
 push rbp
 push rbx
 push r12
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 lea rdi,[rsp]
 lea rsi,[rip+.Lbe_bearer_format]
 mov rdx,r12
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lbe_bearer_bad
 mov rdi,[rbx+PG_MM_HEADERS]
 mov rsi,[rsp]
 call curl_slist_append@PLT
 test rax,rax
 jz .Lbe_bearer_bad
 mov [rbx+PG_MM_HEADERS],rax
 xor ebp,ebp
 jmp .Lbe_bearer_end
.Lbe_bearer_bad:
 mov ebp,1
.Lbe_bearer_end:
 mov rdi,[rsp]
 call free@PLT
 mov eax,ebp
 add rsp,16
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lbe_bearer_format: .asciz "Authorization: Bearer %s"

.text
/* Owned standard-runtime endpoint region, NULL for a custom endpoint. */
.Lbr_endpoint_region:
 push rbp
 push rbx
 push r12
 sub rsp,32
 mov r12,rdi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 xor ebp,ebp
 test rdi,rdi
 jz .Lbr_host_end
 call curl_url@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbr_host_end
 mov rdi,rax
 xor esi,esi
 mov rdx,r12
 xor ecx,ecx
 call curl_url_set@PLT
 test eax,eax
 jnz .Lbr_host_end
 mov rdi,[rsp]
 mov esi,5
 lea rdx,[rsp+8]
 xor ecx,ecx
 call curl_url_get@PLT
 test eax,eax
 jnz .Lbr_host_end
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lbr_runtime]
 mov edx,16
 call strncasecmp@PLT
 mov rbx,[rsp+8]
 test eax,eax
 jnz .Lbr_host_fips
 add rbx,16
 jmp .Lbr_host_region
.Lbr_host_fips:
 mov rdi,rbx
 lea rsi,[rip+.Lbr_fips]
 mov edx,21
 call strncasecmp@PLT
 test eax,eax
 jnz .Lbr_host_end
 add rbx,21
.Lbr_host_region:
 mov rdi,rbx
 mov esi,46
 call strchr@PLT
 test rax,rax
 jz .Lbr_host_end
 mov [rsp+16],rax
 mov rdi,rax
 lea rsi,[rip+.Lbr_aws_suffix]
 call strcasecmp@PLT
 test eax,eax
 jz .Lbr_host_copy
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lbr_cn_suffix]
 call strcasecmp@PLT
 test eax,eax
 jnz .Lbr_host_end
.Lbr_host_copy:
 mov rsi,[rsp+16]
 sub rsi,rbx
 test rsi,rsi
 jz .Lbr_host_end
 mov [rsp+24],rsi
 mov rdi,rbx
 lea rsi,[rip+.Lbr_region_chars]
 call strspn@PLT
 cmp rax,[rsp+24]
 jne .Lbr_host_end
 mov rdi,rbx
 mov rsi,[rsp+24]
 call strndup@PLT
 mov rbp,rax
.Lbr_host_end:
 mov rdi,[rsp+8]
 call curl_free@PLT
 mov rdi,[rsp]
 call curl_url_cleanup@PLT
 mov rax,rbp
 add rsp,32
 pop r12
 pop rbx
 pop rbp
 ret
/* One fixed public region feeds endpoint and signing for the lifetime of a request config. */
.Lbr_resolve:
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
 mov rdi,rbx
 lea rsi,[rip+.Lge_baseUrl]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lbr_base_value
 mov rdi,rbx
 lea rsi,[rip+.Lbeta_modelInfo]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lge_baseUrl]
 call json_object_object_get@PLT
.Lbr_base_value:
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+8],rax
 mov rdi,rax
 call .Lbr_endpoint_region
 mov [rsp+16],rax
 mov rdi,rbx
 lea rsi,[rip+.Lbe_region_key]
 lea rdx,[rip+.Lbe_region_env]
 call .Lge_option
 test rax,rax
 jz .Lbr_environment_default
 cmp byte ptr [rax],0
 jne .Lbr_configured
.Lbr_environment_default:
 lea rdi,[rip+.Lbe_default_region_env]
 call getenv@PLT
 test rax,rax
 jz .Lbr_configured
 cmp byte ptr [rax],0
 jne .Lbr_configured
 xor eax,eax
.Lbr_configured:
 mov [rsp+24],rax
 mov rdi,rbx
 lea rsi,[rip+.Lcfg_model]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov r12,rax
 mov rdi,rax
 lea rsi,[rip+.Lbr_arn_prefix]
 mov edx,7
 call strncmp@PLT
 test eax,eax
 jnz .Lbr_select_region
 lea r13,[r12+7]
 cmp byte ptr [r13],58
 je .Lbr_arn_partition
 cmp byte ptr [r13],45
 jne .Lbr_select_region
 mov rdi,r13
 mov esi,58
 call strchr@PLT
 test rax,rax
 jz .Lbr_select_region
 mov r13,rax
.Lbr_arn_partition:
 lea rdi,[r13+1]
 lea rsi,[rip+.Lbr_arn_service]
 mov edx,8
 call strncmp@PLT
 test eax,eax
 jnz .Lbr_select_region
 add r13,9
 mov rdi,r13
 mov esi,58
 call strchr@PLT
 test rax,rax
 jz .Lbr_select_region
 sub rax,r13
 test rax,rax
 jz .Lbr_select_region
 mov rdi,r13
 mov rsi,rax
 call strndup@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbr_bad
 jmp .Lbr_store
.Lbr_select_region:
 mov rax,[rsp+24]
 test rax,rax
 jnz .Lbr_copy_region
 lea rdi,[rip+.Lapf_profile_env]
 call getenv@PLT
 test rax,rax
 jz .Lapf_standard_region
 cmp byte ptr [rax],0
 jne .Lapf_resolve_profile_region
.Lapf_standard_region:
 mov rax,[rsp+16]
 test rax,rax
 jnz .Lbr_copy_region
 lea rax,[rip+.Lbe_region_default]
 jmp .Lbr_copy_region
.Lapf_resolve_profile_region:
 mov rdi,rbx
 call .Lapf_load
 test rax,rax
 jz .Lbr_bad
 mov [rsp+40],rax
 mov rax,[rax+24]
 test rax,rax
 jnz .Lbr_copy_region
 lea rax,[rip+.Lbe_region_default]
.Lbr_copy_region:
 mov rdi,rax
 call strdup@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbr_bad
.Lbr_store:
 mov rdi,[rsp]
 call .Lbe_credential
 test eax,eax
 jnz .Lbr_bad
 mov rdi,[rsp]
 call json_object_new_string@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lbr_region]
 mov rdx,rax
 test rdx,rdx
 jz .Lbr_bad
 mov [rsp+32],rdx
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lbr_bad
 mov qword ptr [rsp+32],0
 cmp qword ptr [rsp+16],0
 je .Lbr_ok
 cmp qword ptr [rsp+24],0
 jne .Lapf_region_overrides_catalog
 lea rdi,[rip+.Lapf_profile_env]
 call getenv@PLT
 test rax,rax
 jz .Lbr_ok
 cmp byte ptr [rax],0
 je .Lbr_ok
.Lapf_region_overrides_catalog:
 /* A configured region supersedes the built-in standard endpoint. */
 mov rdi,rbx
 lea rsi,[rip+.Lge_baseUrl]
 call json_object_object_del@PLT
 mov rdi,rbx
 lea rsi,[rip+.Lbeta_modelInfo]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lbr_ok
 mov rdi,rax
 lea rsi,[rip+.Lge_baseUrl]
 call json_object_object_del@PLT
.Lbr_ok:
 xor ebp,ebp
 jmp .Lbr_end
.Lbr_bad:
 mov ebp,1
.Lbr_end:
 mov rdi,[rsp+40]
 call .Lapf_free
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+16]
 call free@PLT
 mov rdi,[rsp+32]
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
.Lbr_region: .asciz "bedrockRegion"
.Lbr_runtime: .asciz "bedrock-runtime."
.Lbr_fips: .asciz "bedrock-runtime-fips."
.Lbr_aws_suffix: .asciz ".amazonaws.com"
.Lbr_cn_suffix: .asciz ".amazonaws.com.cn"
.Lbr_arn_prefix: .asciz "arn:aws"
.Lbr_arn_service: .asciz "bedrock:"

.section .rodata
.Lbr_region_chars: .asciz "abcdefghijklmnopqrstuvwxyz0123456789-"

.text
/* Price metadata is public; canonical usage is finalized before this calculation. */
.Lmc_calculate:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,128
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov rdi,r12
 lea rsi,[rip+.Lmu_usage]
 call json_object_object_get@PLT
 mov r12,rax
 test rax,rax
 jz .Lmc_ok
 mov rdi,rbx
 lea rsi,[rip+.Lbeta_modelInfo]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lmc_cost]
 call json_object_object_get@PLT
 mov r13,rax
 test rax,rax
 jz .Lmc_ok
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lmc_bad
 xor r14d,r14d
.Lmc_counts:
 lea rax,[rip+.Lmc_keys]
 mov rsi,[rax+r14*8]
 mov rdi,r12
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_double@PLT
 movsd [rsp+8+r14*8],xmm0
 inc r14
 cmp r14,5
 jb .Lmc_counts
 movsd xmm0,[rsp+8]
 addsd xmm0,[rsp+24]
 addsd xmm0,[rsp+32]
 movsd [rsp+56],xmm0
 movsd xmm0,[rip+.Lmc_negative]
 movsd [rsp+64],xmm0
 mov rdi,r13
 lea rsi,[rip+.Lmc_tiers]
 call json_object_object_get@PLT
 mov rbx,rax
 xor r14d,r14d
 test rax,rax
 jz .Lmc_rates
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lmc_bad
.Lmc_tier:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lmc_rates
 mov rdi,rbx
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov [rsp+72],rax
 mov rdi,rax
 lea rsi,[rip+.Lmc_threshold]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_double@PLT
 ucomisd xmm0,[rsp+64]
 jbe .Lmc_tier_next
 movsd xmm1,[rsp+56]
 ucomisd xmm1,xmm0
 jbe .Lmc_tier_next
 movsd [rsp+64],xmm0
 mov r13,[rsp+72]
.Lmc_tier_next:
 inc r14
 jmp .Lmc_tier
.Lmc_rates:
 xor r14d,r14d
.Lmc_rate:
 lea rax,[rip+.Lmc_keys]
 mov rsi,[rax+r14*8]
 mov rdi,r13
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_double@PLT
 pxor xmm1,xmm1
 ucomisd xmm0,xmm1
 jb .Lmc_bad
 jp .Lmc_bad
 movsd [rsp+80+r14*8],xmm0
 inc r14
 cmp r14,4
 jb .Lmc_rate
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lmc_bad
 pxor xmm0,xmm0
 movsd [rsp+112],xmm0
 xor r14d,r14d
.Lmc_component:
 movsd xmm0,[rsp+80+r14*8]
 cmp r14,3
 je .Lmc_write
 divsd xmm0,[rip+.Lmc_million]
 mulsd xmm0,[rsp+8+r14*8]
 jmp .Lmc_component_ready
.Lmc_write:
 movsd xmm1,[rsp+32]
 subsd xmm1,[rsp+40]
 mulsd xmm0,xmm1
 movsd xmm1,[rsp+80]
 addsd xmm1,xmm1
 mulsd xmm1,[rsp+40]
 addsd xmm0,xmm1
 divsd xmm0,[rip+.Lmc_million]
.Lmc_component_ready:
 movsd xmm1,[rsp+112]
 addsd xmm1,xmm0
 movsd [rsp+112],xmm1
 call json_object_new_double@PLT
 mov rdx,rax
 lea rax,[rip+.Lmc_keys]
 mov rsi,[rax+r14*8]
 mov rdi,[rsp]
 call .Lmc_set
 test eax,eax
 jnz .Lmc_bad
 inc r14
 cmp r14,4
 jb .Lmc_component
 movsd xmm0,[rsp+112]
 call json_object_new_double@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lmc_total]
 mov rdx,rax
 call .Lmc_set
 test eax,eax
 jnz .Lmc_bad
 xor edi,edi
 call json_object_new_boolean@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lmc_reported]
 mov rdx,rax
 call .Lmc_set
 test eax,eax
 jnz .Lmc_bad
 mov rdi,r12
 lea rsi,[rip+.Lmc_cost]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lmc_set
 mov ebp,eax
 jmp .Lmc_end
.Lmc_ok:
 xor ebp,ebp
 jmp .Lmc_end
.Lmc_bad:
 mov ebp,1
.Lmc_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,128
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lmc_set:
 push rbx
 mov rbx,rdx
 test rdx,rdx
 jz .Lmc_set_bad
 call json_object_object_add@PLT
 test eax,eax
 jz .Lmc_set_end
 mov rdi,rbx
 call json_object_put@PLT
.Lmc_set_bad:
 mov eax,1
.Lmc_set_end:
 pop rbx
 ret
.section .rodata
.Lmc_cost: .asciz "cost"
.Lmc_tiers: .asciz "tiers"
.Lmc_threshold: .asciz "inputTokensAbove"
.Lmc_total: .asciz "total"
.Lmc_reported: .asciz "totalFromProvider"
.Lmc_long: .asciz "cacheWrite1h"
.p2align 3
.Lmc_million: .double 1000000
.Lmc_negative: .double -1
.section .data.rel.ro
.Lmc_keys: .quad .Lmu_input,.Lmu_output,.Lmu_cache_read,.Lmu_cache_write_key,.Lmc_long

.text
/* Positive millisecond request deadline; configuration validation is atomic. */
.Ltm_timeout:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 lea rsi,[rip+.Lbeta_options]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Ltm_key]
 call json_object_object_get@PLT
 mov r12,rax
 test rax,rax
 jz .Ltm_default
 mov rdi,rax
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Ltm_bad
 mov rdi,r12
 call json_object_get_int64@PLT
 test rax,rax
 jle .Ltm_bad
 mov rcx,2147483647
 cmp rax,rcx
 ja .Ltm_bad
 xor edx,edx
 jmp .Ltm_end
.Ltm_default:
 mov eax,60000
 xor edx,edx
.Ltm_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.Ltm_bad:
 xor eax,eax
 mov edx,1
 jmp .Ltm_end
.section .rodata
.Ltm_key: .asciz "timeoutMs"

.text
/* Header callback borrows curl bytes; retain only bounded public request identity. */
.Lhd_receive:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rcx
 mov r12,rdi
 mov rax,rsi
 mul rdx
 test rdx,rdx
 jnz .Lhd_overflow
 mov r13,rax
 cmp rax,5
 jb .Lhd_return
 mov rdi,r12
 lea rsi,[rip+.Lhd_http]
 mov edx,5
 call strncasecmp@PLT
 test eax,eax
 jnz .Lhd_find
 mov byte ptr [rbx+PG_MM_RESPONSE_REQUEST_ID],0
 mov byte ptr [rbx+PG_MM_RETRY_AFTER],0
 mov byte ptr [rbx+PG_MM_RETRY_AFTER_MS],0
 jmp .Lhd_return
.Lhd_find:
 mov rdi,rbx
 mov rsi,r12
 mov rdx,r13
 call .Lcr_header
 xor ebp,ebp
 lea r14,[rip+.Lhd_names]
.Lhd_name:
 mov rdi,[r14+rbp*8]
 test rdi,rdi
 jz .Lhd_return
 call strlen@PLT
 cmp rax,r13
 jae .Lhd_next
 mov rdi,r12
 mov rsi,[r14+rbp*8]
 mov rdx,rax
 call strncasecmp@PLT
 test eax,eax
 jnz .Lhd_next
 mov rdi,[r14+rbp*8]
 call strlen@PLT
 cmp byte ptr [r12+rax],58
 jne .Lhd_next
 lea r12,[r12+rax+1]
 mov r14,r13
 sub r14,rax
 dec r14
.Lhd_trim_head:
 test r14,r14
 jz .Lhd_drop
 cmp byte ptr [r12],32
 je .Lhd_trim_advance
 cmp byte ptr [r12],9
 jne .Lhd_trim_tail
.Lhd_trim_advance:
 inc r12
 dec r14
 jmp .Lhd_trim_head
.Lhd_trim_tail:
 test r14,r14
 jz .Lhd_drop
 mov al,[r12+r14-1]
 cmp al,32
 je .Lhd_trim_cut
 cmp al,9
 je .Lhd_trim_cut
 cmp al,13
 je .Lhd_trim_cut
 cmp al,10
 jne .Lhd_value
.Lhd_trim_cut:
 dec r14
 jmp .Lhd_trim_tail
.Lhd_value:
 cmp r14,200
 ja .Lhd_drop
 mov rdi,r12
 xor esi,esi
 mov rdx,r14
 call memchr@PLT
 test rax,rax
 jnz .Lhd_drop
 lea rdi,[rbx+PG_MM_RESPONSE_REQUEST_ID]
 mov rsi,r12
 mov rdx,r14
 call memcpy@PLT
 mov byte ptr [rbx+PG_MM_RESPONSE_REQUEST_ID+r14],0
 jmp .Lhd_return
.Lhd_drop:
 mov byte ptr [rbx+PG_MM_RESPONSE_REQUEST_ID],0
 jmp .Lhd_return
.Lhd_next:
 inc ebp
 jmp .Lhd_name
.Lhd_return:
 mov rax,r13
 jmp .Lhd_end
.Lhd_overflow:
 xor eax,eax
.Lhd_end:
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lhd_failure_complete:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 cmp byte ptr [rdi+PG_MM_RESPONSE_REQUEST_ID],0
 je .Lhd_complete_ok
 lea rdi,[rip+.Lhd_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lhd_complete_bad
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov r13,rax
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 lea rdi,[rip+.Lhd_generic]
 test eax,eax
 jz .Lhd_type_ready
 lea rdi,[rip+.Lhd_bedrock]
.Lhd_type_ready:
 call json_object_new_string@PLT
 mov rdi,r13
 lea rsi,[rip+.Lhd_type]
 mov rdx,rax
 call .Lmc_set
 test eax,eax
 jnz .Lhd_complete_bad
 xor edi,edi
 lea rsi,[rsp+8]
 call clock_gettime@PLT
 test eax,eax
 jnz .Lhd_complete_bad
 cvtsi2sd xmm0,qword ptr [rsp+8]
 mulsd xmm0,[rip+.Lhd_thousand]
 cvtsi2sd xmm1,qword ptr [rsp+16]
 divsd xmm1,[rip+.Lmc_million]
 addsd xmm0,xmm1
 call json_object_new_double@PLT
 mov rdi,r13
 lea rsi,[rip+.Lhd_timestamp]
 mov rdx,rax
 call .Lmc_set
 test eax,eax
 jnz .Lhd_complete_bad
 mov rdi,r13
 lea rsi,[rip+.Lhd_details]
 call json_object_object_get@PLT
 mov r14,rax
 lea rdi,[rbx+PG_MM_RESPONSE_REQUEST_ID]
 call json_object_new_string@PLT
 mov rdi,r14
 lea rsi,[rip+.Lhd_request_id]
 mov rdx,rax
 call .Lmc_set
 test eax,eax
 jnz .Lhd_complete_bad
 mov rdi,r12
 lea rsi,[rip+.Lhd_diagnostics]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lmc_set
 mov ebp,eax
 jmp .Lhd_complete_end
.Lhd_complete_ok:
 xor ebp,ebp
 jmp .Lhd_complete_end
.Lhd_complete_bad:
 mov ebp,1
.Lhd_complete_end:
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
.Lhd_http: .asciz "HTTP/"
.Lhd_amzn: .asciz "x-amzn-requestid"
.Lhd_openai: .asciz "x-request-id"
.Lhd_anthropic: .asciz "request-id"
.Lhd_stop: .asciz "stopReason"
.Lhd_error: .asciz "error"
.Lhd_json: .asciz "[{\"details\":{}}]"
.Lhd_generic: .asciz "provider_response_failure"
.Lhd_bedrock: .asciz "bedrock_response_failure"
.Lhd_type: .asciz "type"
.Lhd_timestamp: .asciz "timestamp"
.Lhd_details: .asciz "details"
.Lhd_request_id: .asciz "requestId"
.Lhd_diagnostics: .asciz "diagnostics"
.p2align 3
.Lhd_thousand: .double 1000
.section .data.rel.ro
.Lhd_names: .quad .Lhd_amzn,.Lhd_openai,.Lhd_anthropic,0

.text
.Lapf_string_free:
 push rbx
 mov rbx,rdi
 test rdi,rdi
 jz .Lapf_string_done
 call strlen@PLT
 mov rdi,rbx
 mov rdx,rax
 xor esi,esi
 call memset@PLT
 mov rdi,rbx
 call g_free@PLT
.Lapf_string_done:
 pop rbx
 ret
/* Private owned strings: access, secret, token, region; never part of request JSON. */
.Lapf_free:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 test rdi,rdi
 jz .Lapf_free_done
 xor r12d,r12d
.Lapf_free_each:
 mov rdi,[rbx+r12*8]
 test rdi,rdi
 jz .Lapf_free_next
 call .Lapf_string_free
.Lapf_free_next:
 inc r12
 cmp r12,4
 jb .Lapf_free_each
 mov rdi,rbx
 call free@PLT
.Lapf_free_done:
 add rsp,8
 pop r12
 pop rbx
 ret
/* Files use GLib's INI parser; explicit profile wins ambient credentials. */
.Lapf_load:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,80
 mov rbx,rdi
 xor eax,eax
 mov ecx,10
 mov rdi,rsp
 rep stosq
 mov edi,1
 mov esi,40
 call calloc@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lapf_bad
 mov rdi,rbx
 lea rsi,[rip+.Lbeta_options]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lbe_profile_key]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lapf_ambient_profile
 cmp byte ptr [rax],0
 je .Lapf_ambient_profile
 mov dword ptr [rsp+48],1
 jmp .Lapf_profile_ready
.Lapf_ambient_profile:
 lea rdi,[rip+.Lapf_profile_env]
 call getenv@PLT
 test rax,rax
 jz .Lapf_default_profile
 cmp byte ptr [rax],0
 jne .Lapf_profile_ready
.Lapf_default_profile:
 lea rax,[rip+.Lapf_default]
.Lapf_profile_ready:
 mov rdi,rax
 call strdup@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lapf_bad
 xor r12d,r12d
.Lapf_file:
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lapf_default]
 call strcmp@PLT
 test eax,eax
 jz .Lapf_plain_section
 test r12d,r12d
 jnz .Lapf_plain_section
 lea rdi,[rsp+16]
 lea rsi,[rip+.Lapf_section_format]
 mov rdx,[rsp+8]
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lapf_bad
 jmp .Lapf_file_path
.Lapf_plain_section:
 mov rdi,[rsp+8]
 call strdup@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lapf_bad
.Lapf_file_path:
 lea rdi,[rip+.Lapf_config_env]
 test r12d,r12d
 jz .Lapf_path_env
 lea rdi,[rip+.Lapf_credentials_env]
.Lapf_path_env:
 call getenv@PLT
 test rax,rax
 jz .Lapf_home
 cmp byte ptr [rax],0
 je .Lapf_home
 mov rdi,rax
 call strdup@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lapf_bad
 jmp .Lapf_parse
.Lapf_home:
 lea rdi,[rip+.Lapf_home_env]
 call getenv@PLT
 test rax,rax
 jz .Lapf_file_next
 lea rcx,[rip+.Lapf_config_file]
 test r12d,r12d
 jz .Lapf_home_path
 lea rcx,[rip+.Lapf_credentials_file]
.Lapf_home_path:
 lea rdi,[rsp+24]
 lea rsi,[rip+.Lapf_path_format]
 mov rdx,rax
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lapf_bad
.Lapf_parse:
 call g_key_file_new@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lapf_bad
 mov rdi,rax
 mov rsi,[rsp+24]
 xor edx,edx
 xor ecx,ecx
 call g_key_file_load_from_file@PLT
 test eax,eax
 jz .Lapf_file_next
 /* Roles/process/SSO need their own provider rather than accidental static fallback. */
 xor r13d,r13d
.Lapf_unsupported:
 lea rax,[rip+.Lapf_unsupported_keys]
 mov rdx,[rax+r13*8]
 test rdx,rdx
 jz .Lapf_values
 mov rdi,[rsp+32]
 mov rsi,[rsp+16]
 xor ecx,ecx
 call g_key_file_get_value@PLT
 test rax,rax
 jz .Lapf_unsupported_next
 mov rdi,rax
 call g_free@PLT
 mov dword ptr [rsp+52],1
.Lapf_unsupported_next:
 inc r13
 jmp .Lapf_unsupported
.Lapf_values:
 xor r13d,r13d
.Lapf_value:
 mov rdi,[rsp+32]
 mov rsi,[rsp+16]
 lea rax,[rip+.Lapf_keys]
 mov rdx,[rax+r13*8]
 xor ecx,ecx
 call g_key_file_get_value@PLT
 test rax,rax
 jz .Lapf_value_next
 mov r14,rax
 mov rax,[rsp]
 mov rdi,[rax+r13*8]
 call .Lapf_string_free
 mov rax,[rsp]
 mov [rax+r13*8],r14
.Lapf_value_next:
 inc r13
 cmp r13,4
 jb .Lapf_value
.Lapf_file_next:
 mov rdi,[rsp+32]
 test rdi,rdi
 jz .Lapf_file_free
 call g_key_file_unref@PLT
 mov qword ptr [rsp+32],0
.Lapf_file_free:
 mov rdi,[rsp+16]
 call free@PLT
 mov qword ptr [rsp+16],0
 mov rdi,[rsp+24]
 call free@PLT
 mov qword ptr [rsp+24],0
 inc r12
 cmp r12,2
 jb .Lapf_file
 cmp dword ptr [rsp+48],0
 jne .Lapf_explicit
 lea rdi,[rip+.Lbe_access_env]
 call getenv@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lapf_explicit
 cmp byte ptr [rax],0
 je .Lapf_explicit
 lea rdi,[rip+.Lbe_secret_env]
 call getenv@PLT
 test rax,rax
 jz .Lapf_explicit
 cmp byte ptr [rax],0
 je .Lapf_explicit
 mov r14,rax
 xor r13d,r13d
.Lapf_env_value:
 mov rdi,[rsp+40]
 test r13d,r13d
 jz .Lapf_env_copy
 mov rdi,r14
 cmp r13d,1
 je .Lapf_env_copy
 lea rdi,[rip+.Lbe_token_env]
 call getenv@PLT
 mov rdi,rax
.Lapf_env_copy:
 xor ebp,ebp
 test rdi,rdi
 jz .Lapf_env_replace
 call g_strdup@PLT
 mov rbp,rax
 test rax,rax
 jz .Lapf_bad
.Lapf_env_replace:
 mov rax,[rsp]
 mov rdi,[rax+r13*8]
 call .Lapf_string_free
 mov rax,[rsp]
 mov [rax+r13*8],rbp
 inc r13
 cmp r13,3
 jb .Lapf_env_value
 mov dword ptr [rsp+52],0
.Lapf_explicit:
 mov rbx,[rsp]
 mov eax,[rsp+52]
 mov [rbx+32],eax
 mov qword ptr [rsp],0
 jmp .Lapf_end
.Lapf_bad:
 xor ebx,ebx
.Lapf_end:
 mov rdi,[rsp]
 call .Lapf_free
 mov rdi,[rsp+8]
 call free@PLT
 mov rdi,[rsp+16]
 call free@PLT
 mov rdi,[rsp+24]
 call free@PLT
 mov rdi,[rsp+32]
 test rdi,rdi
 jz .Lapf_end_ready
 call g_key_file_unref@PLT
.Lapf_end_ready:
 mov rax,rbx
 add rsp,80
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lapf_profile_env: .asciz "AWS_PROFILE"
.Lapf_config_env: .asciz "AWS_CONFIG_FILE"
.Lapf_credentials_env: .asciz "AWS_SHARED_CREDENTIALS_FILE"
.Lapf_home_env: .asciz "HOME"
.Lapf_default: .asciz "default"
.Lapf_section_format: .asciz "profile %s"
.Lapf_path_format: .asciz "%s/.aws/%s"
.Lapf_config_file: .asciz "config"
.Lapf_credentials_file: .asciz "credentials"
.Lapf_access: .asciz "aws_access_key_id"
.Lapf_secret: .asciz "aws_secret_access_key"
.Lapf_token: .asciz "aws_session_token"
.Lapf_region: .asciz "region"
.Lapf_role: .asciz "role_arn"
.Lapf_process: .asciz "credential_process"
.Lapf_sso: .asciz "sso_session"
.Lapf_web: .asciz "web_identity_token_file"
.section .data.rel.ro
.Lapf_keys: .quad .Lapf_access,.Lapf_secret,.Lapf_token,.Lapf_region
.Lapf_unsupported_keys: .quad .Lapf_role,.Lapf_process,.Lapf_sso,.Lapf_web,0

.text
.Lcp_is:
 push rbx
 mov rbx,rdi
 lea rsi,[rip+.Laffinity_provider]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lcp_provider_value
 mov rdi,rbx
 lea rsi,[rip+.Lbeta_modelInfo]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcp_provider]
 call json_object_object_get@PLT
.Lcp_provider_value:
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lcp_is_done
 mov rdi,rax
 lea rsi,[rip+.Lcp_name]
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
.Lcp_is_done:
 pop rbx
 ret
.Lcp_put:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 call json_object_new_string@PLT
 test rax,rax
 jz .Lcp_put_bad
 mov r13,rax
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Lcp_put_done
 mov rdi,r13
 call json_object_put@PLT
.Lcp_put_bad:
 mov eax,1
.Lcp_put_done:
 pop r13
 pop r12
 pop rbx
 ret
/* Canonical transcript is borrowed for this request; dynamic headers own copies. */
.Lcp_headers:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rsi
 mov rbp,rdx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov rdi,[rdi+PG_MM_CONFIGURATION]
 call .Lcp_is
 test eax,eax
 jz .Lcp_headers_ok
 mov r12,[rbx+PG_MM_SES_COUNT]
 test r12,r12
 jz .Lcp_emit
 cmp dword ptr [rbx+PG_MM_MSG_ROLE],PG_MM_ROLE_CONTEXT_VIEW
 jne .Lcp_legacy
 mov rdi,[rbx+PG_MM_MSG_TEXT]
 lea rsi,[rip+.Lcp_messages]
 call json_object_object_get@PLT
 mov [rsp+16],rax
 mov rdi,rax
 call json_object_array_length@PLT
 mov r12,rax
 xor r13d,r13d
.Lcp_row:
 cmp r13,r12
 jae .Lcp_emit
 mov rdi,[rsp+16]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lcp_role]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lcp_row_next
 mov [rsp+32],rax
 mov rdi,rax
 lea rsi,[rip+.Lcp_user]
 call strcmp@PLT
 mov dword ptr [rsp],eax
 test eax,eax
 jz .Lcp_content
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lcp_tool_result]
 call strcmp@PLT
 test eax,eax
 jnz .Lcp_row_next
.Lcp_content:
 mov rdi,r14
 lea rsi,[rip+.Lcp_content_key]
 call json_object_object_get@PLT
 mov [rsp+24],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lcp_row_next
 mov rdi,[rsp+24]
 call json_object_array_length@PLT
 mov r14,rax
 mov qword ptr [rsp+40],0
.Lcp_block:
 cmp [rsp+40],r14
 jae .Lcp_row_next
 mov rdi,[rsp+24]
 mov rsi,[rsp+40]
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcp_type]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lcp_block_next
 mov rdi,rax
 lea rsi,[rip+.Lcp_image]
 call strcmp@PLT
 test eax,eax
 jnz .Lcp_block_next
 mov dword ptr [rsp+8],1
.Lcp_block_next:
 inc qword ptr [rsp+40]
 jmp .Lcp_block
.Lcp_row_next:
 inc r13
 jmp .Lcp_row
.Lcp_legacy:
 dec r12
 imul r12,PG_MM_MSG_BYTES
 mov eax,[rbx+r12+PG_MM_MSG_ROLE]
 cmp eax,PG_MM_ROLE_USER
 je .Lcp_emit
 cmp eax,PG_MM_ROLE_USER_VIEW
 je .Lcp_emit
 mov dword ptr [rsp],1
.Lcp_emit:
 lea rdx,[rip+.Lcp_user]
 cmp dword ptr [rsp],0
 je .Lcp_initiator
 lea rdx,[rip+.Lcp_agent]
.Lcp_initiator:
 mov rdi,rbp
 lea rsi,[rip+.Lcp_initiator_key]
 call .Lcp_put
 test eax,eax
 jnz .Lcp_headers_end
 mov rdi,rbp
 lea rsi,[rip+.Lcp_intent_key]
 lea rdx,[rip+.Lcp_intent]
 call .Lcp_put
 test eax,eax
 jnz .Lcp_headers_end
 cmp dword ptr [rsp+8],0
 je .Lcp_headers_ok
 mov rdi,rbp
 lea rsi,[rip+.Lcp_vision_key]
 lea rdx,[rip+.Lcp_true]
 call .Lcp_put
 jmp .Lcp_headers_end
.Lcp_headers_ok:
 xor eax,eax
.Lcp_headers_end:
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lcp_accept: .asciz "accept: application/json"
.Lcp_browser: .asciz "anthropic-dangerous-direct-browser-access: true"
.Lcp_name: .asciz "github-copilot"
.Lcp_provider: .asciz "provider"
.Lcp_messages: .asciz "messages"
.Lcp_role: .asciz "role"
.Lcp_user: .asciz "user"
.Lcp_agent: .asciz "agent"
.Lcp_tool_result: .asciz "toolResult"
.Lcp_content_key: .asciz "content"
.Lcp_type: .asciz "type"
.Lcp_image: .asciz "image"
.Lcp_initiator_key: .asciz "x-initiator"
.Lcp_intent_key: .asciz "openai-intent"
.Lcp_intent: .asciz "conversation-edits"
.Lcp_vision_key: .asciz "copilot-vision-request"
.Lcp_true: .asciz "true"

.text
.Lcp_prepare:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 call pg_mimo_config_codex@PLT
 mov [rsp+8],eax
 test eax,eax
 jnz .Lcp_prepare_active
 mov rdi,rbx
 call .Lcp_is
 test eax,eax
 jz .Lcp_prepare_ok
.Lcp_prepare_active:
 mov rdi,rbx
 lea rsi,[rip+.Lcfg_endpoint]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lcp_prepare_ok
 mov rdi,rbx
 lea rsi,[rip+.Lge_baseUrl]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lcp_base_value
 mov rdi,rbx
 lea rsi,[rip+.Lbeta_modelInfo]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lge_baseUrl]
 call json_object_object_get@PLT
.Lcp_base_value:
 mov rdi,rax
 call json_object_get_string@PLT
 mov r12,rax
 test rax,rax
 jnz .Lcp_base_ready
 lea r12,[rip+.Lcp_base]
 cmp dword ptr [rsp+8],0
 je .Lcp_base_ready
 lea r12,[rip+.Lcd_base]
.Lcp_base_ready:
 mov rdi,r12
 call strlen@PLT
 mov rbp,rax
.Lcp_base_trim:
 test rbp,rbp
 jz .Lcp_prepare_bad
 cmp byte ptr [r12+rbp-1],47
 jne .Lcp_base_path
 dec rbp
 jmp .Lcp_base_trim
.Lcp_base_path:
 cmp dword ptr [rsp+8],0
 jne .Lcd_base_path
 lea r13,[rip+.Lcp_chat_path]
 mov rdi,rbx
 call pg_mimo_config_responses@PLT
 test eax,eax
 jz .Lcp_anthropic_path
 lea r13,[rip+.Lcp_responses_path]
 jmp .Lcp_url
.Lcp_anthropic_path:
 mov rdi,rbx
 call pg_mimo_config_anthropic@PLT
 test eax,eax
 jz .Lcp_url
 lea r13,[rip+.Lcp_messages_path]
 jmp .Lcp_url
.Lcd_base_path:
 lea r13,[rip+.Lcd_full_path]
 cmp rbp,16
 jb .Lcd_short_path
 lea rdi,[r12+rbp-16]
 lea rsi,[rip+.Lcd_full_path]
 mov edx,16
 call strncmp@PLT
 test eax,eax
 jnz .Lcd_short_path
 lea r13,[rip+.Lcd_empty]
 jmp .Lcp_url
.Lcd_short_path:
 cmp rbp,6
 jb .Lcp_url
 lea rdi,[r12+rbp-6]
 lea rsi,[rip+.Lcd_codex_suffix]
 mov edx,6
 call strncmp@PLT
 test eax,eax
 jnz .Lcp_url
 lea r13,[rip+.Lcp_responses_path]
.Lcp_url:
 lea rdi,[rsp]
 lea rsi,[rip+.Lcp_url_format]
 mov edx,ebp
 mov rcx,r12
 mov r8,r13
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lcp_prepare_bad
 mov rdi,rbx
 lea rsi,[rip+.Lcfg_endpoint]
 mov rdx,[rsp]
 call .Lcp_put
 mov r14d,eax
 jmp .Lcp_prepare_end
.Lcp_prepare_bad:
 mov r14d,1
 jmp .Lcp_prepare_end
.Lcp_prepare_ok:
 xor r14d,r14d
.Lcp_prepare_end:
 mov rdi,[rsp]
 call free@PLT
 mov eax,r14d
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lcp_base: .asciz "https://api.individual.githubcopilot.com"
.Lcp_chat_path: .asciz "/chat/completions"
.Lcp_responses_path: .asciz "/responses"
.Lcp_messages_path: .asciz "/v1/messages"
.Lcp_url_format: .asciz "%.*s%s"

.text
.Lcd_headers:
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
 xor ebp,ebp
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_codex@PLT
 test eax,eax
 jz .Lcd_headers_cleanup
 mov ebp,1
 lea rdi,[rip+.Lstr_pangu_key]
 call getenv@PLT
 test rax,rax
 jz .Lcd_headers_cleanup
 mov r12,rax
 mov rdi,rax
 mov esi,46
 call strchr@PLT
 test rax,rax
 jz .Lcd_headers_cleanup
 lea r13,[rax+1]
 mov rdi,r13
 mov esi,46
 call strchr@PLT
 test rax,rax
 jz .Lcd_headers_cleanup
 mov r14,rax
 lea rdi,[rax+1]
 mov esi,46
 call strchr@PLT
 test rax,rax
 jnz .Lcd_headers_cleanup
 mov rdi,r13
 mov rsi,r14
 sub rsi,r13
 call json_object_new_string_len@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lcd_headers_cleanup
 mov rdi,rax
 lea rsi,[rsp+8]
 lea rdx,[rsp+16]
 call pg_mimo_base64_decode@PLT
 test eax,eax
 jnz .Lcd_headers_cleanup
 mov rdi,[rsp+8]
 mov rsi,[rsp+16]
 call strndup@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lcd_headers_cleanup
 mov rdi,rax
 call json_tokener_parse@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lcd_headers_cleanup
 mov rdi,rax
 lea rsi,[rip+.Lcd_claim]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcd_account]
 call json_object_object_get@PLT
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lcd_headers_cleanup
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lcd_claim]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcd_account]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 mov r13,rax
 mov rdi,rax
 call .Lbe_credential
 test eax,eax
 jnz .Lcd_headers_cleanup
 lea rdi,[rsp+40]
 lea rsi,[rip+.Lcd_account_format]
 mov rdx,r13
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lcd_headers_cleanup
 mov rdi,[rbx+PG_MM_HEADERS]
 mov rsi,[rsp+40]
 call curl_slist_append@PLT
 test rax,rax
 jz .Lcd_headers_cleanup
 mov [rbx+PG_MM_HEADERS],rax
 xor r14d,r14d
.Lcd_header_each:
 lea rax,[rip+.Lcd_headers_table]
 mov rsi,[rax+r14*8]
 mov rdi,[rbx+PG_MM_HEADERS]
 call curl_slist_append@PLT
 test rax,rax
 jz .Lcd_headers_cleanup
 mov [rbx+PG_MM_HEADERS],rax
 inc r14
 cmp r14,2
 jb .Lcd_header_each
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call .Lcd_session_key
 mov [rsp+48],rax
 test rax,rax
 jz .Lcd_session_ready
 mov rdi,rax
 call .Lbe_credential
 test eax,eax
 jnz .Lcd_headers_cleanup
 xor r14d,r14d
.Lcd_session_each:
 mov rdi,[rsp+40]
 call free@PLT
 mov qword ptr [rsp+40],0
 lea rax,[rip+.Lcd_session_formats]
 mov rsi,[rax+r14*8]
 lea rdi,[rsp+40]
 mov rdx,[rsp+48]
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lcd_headers_cleanup
 lea r13,[rbx+PG_MM_HEADERS]
.Lcd_session_remove:
 mov r12,[r13]
 test r12,r12
 jz .Lcd_session_append
 mov rdi,[r12]
 lea rax,[rip+.Lcd_session_names]
 mov rsi,[rax+r14*8]
 lea rax,[rip+.Lcd_session_lengths]
 mov rdx,[rax+r14*8]
 call strncasecmp@PLT
 test eax,eax
 jnz .Lcd_session_keep
 lea rax,[rip+.Lcd_session_lengths]
 mov rdx,[rax+r14*8]
 mov rax,[r12]
 cmp byte ptr [rax+rdx],58
 jne .Lcd_session_keep
 mov rax,[r12+8]
 mov [r13],rax
 mov qword ptr [r12+8],0
 mov rdi,r12
 call curl_slist_free_all@PLT
 jmp .Lcd_session_remove
.Lcd_session_keep:
 lea r13,[r12+8]
 jmp .Lcd_session_remove
.Lcd_session_append:
 mov rdi,[rbx+PG_MM_HEADERS]
 mov rsi,[rsp+40]
 call curl_slist_append@PLT
 test rax,rax
 jz .Lcd_headers_cleanup
 mov [rbx+PG_MM_HEADERS],rax
 inc r14
 cmp r14,2
 jb .Lcd_session_each
.Lcd_session_ready:
 xor ebp,ebp
.Lcd_headers_cleanup:
 mov rdi,[rsp+48]
 call free@PLT
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov rdi,[rsp+24]
 call free@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call free@PLT
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lcd_claim: .asciz "https://api.openai.com/auth"
.Lcd_account: .asciz "chatgpt_account_id"
.Lcd_account_format: .asciz "chatgpt-account-id: %s"
.Lcd_beta: .asciz "OpenAI-Beta: responses=experimental"
.Lcd_accept: .asciz "accept: text/event-stream"
.Lcd_originator: .asciz "originator: pangu"
.section .data.rel.ro
.Lcd_headers_table: .quad .Lcd_beta,.Lcd_accept,.Lcd_originator

.section .rodata
.Lcd_base: .asciz "https://chatgpt.com/backend-api"
.Lcd_full_path: .asciz "/codex/responses"
.Lcd_codex_suffix: .asciz "/codex"
.Lcd_empty: .asciz ""

.text
.Lcd_reserved:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 xor r12d,r12d
.Lcd_reserved_each:
 lea rax,[rip+.Lcd_reserved_keys]
 mov rsi,[rax+r12*8]
 mov rdi,rbx
 call strcasecmp@PLT
 test eax,eax
 jz .Lcd_reserved_yes
 inc r12
 cmp r12,6
 jb .Lcd_reserved_each
 xor eax,eax
 jmp .Lcd_reserved_end
.Lcd_reserved_yes:
 mov eax,1
.Lcd_reserved_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.section .rodata
.Lcd_auth_key: .asciz "authorization"
.Lcd_account_key: .asciz "chatgpt-account-id"
.Lcd_beta_key: .asciz "openai-beta"
.Lcd_accept_key: .asciz "accept"
.Lcd_content_key: .asciz "content-type"
.Lcd_encoding_key: .asciz "content-encoding"
.section .data.rel.ro
.Lcd_reserved_keys: .quad .Lcd_auth_key,.Lcd_account_key,.Lcd_beta_key,.Lcd_accept_key,.Lcd_content_key,.Lcd_encoding_key

.text
/* Owned bounded session identity; absent identity and disabled cache return NULL. */
.Lcd_session_key:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 lea rsi,[rip+.Lbeta_options]
 call json_object_object_get@PLT
 mov r12,rax
 mov rdi,rax
 lea rsi,[rip+.Lcd_retention_key]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lcd_session_lookup
 mov rdi,rax
 lea rsi,[rip+.Lcd_none]
 call strcmp@PLT
 test eax,eax
 jz .Lcd_session_absent
.Lcd_session_lookup:
 mov rdi,r12
 lea rsi,[rip+.Laffinity_sessionId]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lcd_session_value
 mov rdi,rbx
 lea rsi,[rip+.Laffinity_sessionId]
 call json_object_object_get@PLT
.Lcd_session_value:
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lcd_session_absent
 mov rbx,rax
 xor r12d,r12d
 xor r13d,r13d
.Lcd_session_char:
 movzx eax,byte ptr [rbx+r12]
 test al,al
 jz .Lcd_session_copy
 and eax,192
 cmp eax,128
 je .Lcd_session_next
 cmp r13,64
 jae .Lcd_session_copy
 inc r13
.Lcd_session_next:
 inc r12
 jmp .Lcd_session_char
.Lcd_session_copy:
 test r12,r12
 jz .Lcd_session_absent
 mov rdi,rbx
 mov rsi,r12
 call strndup@PLT
 jmp .Lcd_session_return
.Lcd_session_absent:
 xor eax,eax
.Lcd_session_return:
 pop r13
 pop r12
 pop rbx
 ret
.section .rodata
.Lcd_session_format: .asciz "session-id: %s"
.Lcd_request_format: .asciz "x-client-request-id: %s"
.Lcd_retention_key: .asciz "cacheRetention"
.Lcd_none: .asciz "none"
.section .data.rel.ro
.Lcd_session_formats: .quad .Lcd_session_format,.Lcd_request_format

.section .rodata
.Lcd_session_name: .asciz "session-id"
.Lcd_request_name: .asciz "x-client-request-id"
.Lcd_session_lengths: .quad 10,19
.section .data.rel.ro
.Lcd_session_names: .quad .Lcd_session_name,.Lcd_request_name

.text
.Lcd_compress:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rdi
 xor r14d,r14d
 mov rdi,[rdi+PG_MM_CONFIGURATION]
 call pg_mimo_config_codex@PLT
 test eax,eax
 jz .Lcd_compress_ok
 mov rdi,[rbx+PG_MM_REQUEST]
 call strlen@PLT
 mov r12,rax
 mov rdi,rax
 call ZSTD_compressBound@PLT
 mov r13,rax
 mov rdi,rax
 call malloc@PLT
 mov r14,rax
 test rax,rax
 jz .Lcd_compress_ok
 mov rdi,r14
 mov rsi,r13
 mov rdx,[rbx+PG_MM_REQUEST]
 mov rcx,r12
 mov r8d,3
 call ZSTD_compress@PLT
 mov r12,rax
 mov rdi,rax
 call ZSTD_isError@PLT
 test eax,eax
 jnz .Lcd_compress_fallback
 mov [rbx+PG_MM_WIRE_REQUEST],r14
 mov [rbx+PG_MM_WIRE_SIZE],r12
 mov rdi,[rbx+PG_MM_HEADERS]
 lea rsi,[rip+.Lcd_encoding]
 call curl_slist_append@PLT
 test rax,rax
 jz .Lcd_compress_bad
 mov [rbx+PG_MM_HEADERS],rax
.Lcd_compress_ok:
 xor eax,eax
 jmp .Lcd_compress_end
.Lcd_compress_fallback:
 mov rdi,r14
 call free@PLT
 jmp .Lcd_compress_ok
.Lcd_compress_bad:
 mov eax,1
.Lcd_compress_end:
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lcd_encoding: .asciz "content-encoding: zstd"

.text
/* Cooperative request-local retries retain the exact encoded/signed body. */
.Lcr_now:
 mov edi,1
 jmp .Lcr_clock
.Lcr_wall_now:
 xor edi,edi
.Lcr_clock:
 sub rsp,24
 mov rsi,rsp
 call clock_gettime@PLT
 test eax,eax
 jnz .Lcr_now_bad
 imul rcx,[rsp],1000
 mov rax,[rsp+8]
 xor edx,edx
 mov esi,1000000
 div rsi
 add rax,rcx
 add rsp,24
 ret
.Lcr_now_bad:
 mov rax,-1
 add rsp,24
 ret
/* Borrowed config and key; negative or noninteger options fail closed. */
.Lcr_integer:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 lea rsi,[rip+.Lbeta_options]
 call json_object_object_get@PLT
 mov rdi,rax
 mov rsi,r12
 call json_object_object_get@PLT
 mov rbx,rax
 test rax,rax
 jz .Lcr_integer_default
 mov rdi,rax
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lcr_integer_bad
 mov rdi,rbx
 call json_object_get_int64@PLT
 test rax,rax
 js .Lcr_integer_bad
 jmp .Lcr_integer_return
.Lcr_integer_default:
 mov rax,r13
 jmp .Lcr_integer_return
.Lcr_integer_bad:
 mov rax,-1
.Lcr_integer_return:
 pop r13
 pop r12
 pop rbx
 ret
.Lcr_schedule:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rdi
 mov rdi,[rdi+PG_MM_CONFIGURATION]
 call pg_mimo_config_codex@PLT
 test eax,eax
 jz .Lcr_schedule_no
 cmp qword ptr [rbx+PG_MM_HTTP],200
 je .Lcr_schedule_no
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lcr_max_retries]
 xor edx,edx
 call .Lcr_integer
 test rax,rax
 js .Lcr_schedule_no
 cmp [rbx+PG_MM_RETRY_ATTEMPT],rax
 jae .Lcr_schedule_no
 cmp dword ptr [rbx+PG_MM_TRANSPORT_ERROR],0
 jne .Lcr_schedule_delay
 cmp qword ptr [rbx+PG_MM_HTTP],429
 jne .Lcr_schedule_status
 xor r12d,r12d
.Lcr_quota_each:
 lea rax,[rip+.Lcr_quota_words]
 mov rsi,[rax+r12*8]
 test rsi,rsi
 jz .Lcr_schedule_delay
 lea rdi,[rbx+PG_MM_BODY]
 call strcasestr@PLT
 test rax,rax
 jnz .Lcr_schedule_no
 inc r12
 jmp .Lcr_quota_each
.Lcr_schedule_status:
 mov rax,[rbx+PG_MM_HTTP]
 cmp rax,500
 je .Lcr_schedule_delay
 cmp rax,502
 je .Lcr_schedule_delay
 cmp rax,503
 je .Lcr_schedule_delay
 cmp rax,504
 je .Lcr_schedule_delay
 xor r12d,r12d
.Lcr_transient_each:
 lea rax,[rip+.Lcr_transient_pairs]
 mov rdx,r12
 shl rdx,4
 mov rsi,[rax+rdx]
 test rsi,rsi
 jz .Lcr_schedule_no
 mov rdx,[rax+rdx+8]
 lea rdi,[rbx+PG_MM_BODY]
 call .Lcr_match_pair
 test rax,rax
 jnz .Lcr_schedule_delay
 inc r12
 jmp .Lcr_transient_each
.Lcr_schedule_delay:
 mov rcx,[rbx+PG_MM_RETRY_ATTEMPT]
 cmp rcx,53
 jae .Lcr_schedule_no
 mov r12d,1000
 shl r12,cl
 cmp dword ptr [rbx+PG_MM_TRANSPORT_ERROR],0
 jne .Lcr_default_delay
 mov rdi,rbx
 call .Lcr_header_delay
 test rax,rax
 js .Lcr_default_delay
 mov r12,rax
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lcr_max_delay]
 mov edx,60000
 call .Lcr_integer
 test rax,rax
 js .Lcr_schedule_no
 jz .Lcr_default_delay
 cmp r12,rax
 ja .Lcr_schedule_block
.Lcr_default_delay:
 call .Lcr_now
 cmp rax,-1
 je .Lcr_schedule_no
 add r12,rax
 jc .Lcr_schedule_no
 mov rdi,[rbx+PG_MM_MULTI]
 mov rsi,[rbx+PG_MM_EASY]
 call curl_multi_remove_handle@PLT
 test eax,eax
 jnz .Lcr_schedule_no
 mov [rbx+PG_MM_RETRY_AT],r12
 inc qword ptr [rbx+PG_MM_RETRY_ATTEMPT]
 mov eax,1
 jmp .Lcr_schedule_end
.Lcr_schedule_block:
 mov dword ptr [rbx+PG_MM_RETRY_BLOCKED],1
.Lcr_schedule_no:
 xor eax,eax
.Lcr_schedule_end:
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lcr_resume:
 push rbx
 mov rbx,rdi
 cmp qword ptr [rdi+PG_MM_RETRY_AT],0
 je .Lcr_resume_active
 call .Lcr_now
 cmp rax,-1
 je .Lcr_resume_bad
 cmp rax,[rbx+PG_MM_RETRY_AT]
 jb .Lcr_resume_wait
 mov qword ptr [rbx+PG_MM_USED],0
 mov qword ptr [rbx+PG_MM_CONSUMED],0
 mov byte ptr [rbx+PG_MM_BODY],0
 mov byte ptr [rbx+PG_MM_RESPONSE_REQUEST_ID],0
 mov byte ptr [rbx+PG_MM_RETRY_AFTER],0
 mov byte ptr [rbx+PG_MM_RETRY_AFTER_MS],0
 mov qword ptr [rbx+PG_MM_HTTP],0
 mov dword ptr [rbx+PG_MM_TRANSPORT_ERROR],0
 mov rdi,[rbx+PG_MM_MULTI]
 mov rsi,[rbx+PG_MM_EASY]
 call curl_multi_add_handle@PLT
 test eax,eax
 jnz .Lcr_resume_bad
 mov qword ptr [rbx+PG_MM_RETRY_AT],0
.Lcr_resume_active:
 xor eax,eax
 jmp .Lcr_resume_end
.Lcr_resume_wait:
 mov eax,1
 jmp .Lcr_resume_end
.Lcr_resume_bad:
 mov eax,-1
.Lcr_resume_end:
 pop rbx
 ret
.section .rodata
.Lcr_max_retries: .asciz "maxRetries"
.Lcr_q1: .asciz "GoUsageLimitError"
.Lcr_q2: .asciz "FreeUsageLimitError"
.Lcr_q3: .asciz "Monthly usage limit reached"
.Lcr_q4: .asciz "available balance"
.Lcr_q5: .asciz "insufficient_quota"
.Lcr_q6: .asciz "out of budget"
.Lcr_q7: .asciz "quota exceeded"
.Lcr_q8: .asciz "billing"
.section .data.rel.ro
.Lcr_quota_words: .quad .Lcr_q1,.Lcr_q2,.Lcr_q3,.Lcr_q4,.Lcr_q5,.Lcr_q6,.Lcr_q7,.Lcr_q8,0


.text
.Lcr_header:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
.Lcr_header_each:
 lea rax,[rip+.Lcr_header_names]
 mov rdi,[rax+r14*8]
 call strlen@PLT
 mov rbp,rax
 cmp r13,rax
 jb .Lcr_header_next
 lea rax,[rip+.Lcr_header_names]
 mov rsi,[rax+r14*8]
 mov rdi,r12
 mov rdx,rbp
 call strncasecmp@PLT
 test eax,eax
 jnz .Lcr_header_next
 sub r13,rbp
 cmp r13,127
 ja .Lcr_header_done
 lea rdi,[rbx+PG_MM_RETRY_AFTER]
 test r14d,r14d
 jz .Lcr_header_copy
 lea rdi,[rbx+PG_MM_RETRY_AFTER_MS]
.Lcr_header_copy:
 mov rbx,rdi
 lea rsi,[r12+rbp]
 mov rdx,r13
 call memcpy@PLT
 mov byte ptr [rbx+r13],0
 jmp .Lcr_header_done
.Lcr_header_next:
 inc r14
 cmp r14,2
 jb .Lcr_header_each
.Lcr_header_done:
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Number headers share JavaScript finite-number and nonnegative delay semantics. */
.Lcr_number:
 push rbx
 sub rsp,16
 mov rbx,rdi
 lea rsi,[rip+.Lcr_space]
 call strspn@PLT
 cmp byte ptr [rbx+rax],0
 jne .Lcr_number_parse
 pxor xmm0,xmm0
 xor eax,eax
 jmp .Lcr_number_end
.Lcr_number_parse:
 mov rdi,rbx
 mov qword ptr [rsp],0
 lea rsi,[rsp]
 call strtod@PLT
 movsd [rsp+8],xmm0
 mov rdi,[rsp]
 cmp rdi,rbx
 je .Lcr_number_bad
 lea rsi,[rip+.Lcr_space]
 call strspn@PLT
 mov rdi,[rsp]
 cmp byte ptr [rdi+rax],0
 jne .Lcr_number_bad
 movsd xmm0,[rsp+8]
 movq rax,xmm0
 shr rax,52
 and eax,2047
 cmp eax,2047
 je .Lcr_number_bad
 pxor xmm1,xmm1
 maxsd xmm0,xmm1
 xor eax,eax
 jmp .Lcr_number_end
.Lcr_number_bad:
 mov eax,1
.Lcr_number_end:
 add rsp,16
 pop rbx
 ret
.Lcr_header_delay:
 push rbp
 push rbx
 sub rsp,8
 mov rbx,rdi
 lea rdi,[rbx+PG_MM_RETRY_AFTER_MS]
 cmp byte ptr [rdi],0
 je .Lcr_seconds_delay
 call .Lcr_number
 test eax,eax
 jz .Lcr_delay_convert
.Lcr_seconds_delay:
 lea rdi,[rbx+PG_MM_RETRY_AFTER]
 cmp byte ptr [rdi],0
 je .Lcr_delay_absent
 call .Lcr_number
 test eax,eax
 jnz .Lcr_date_delay
 mulsd xmm0,[rip+.Lcr_thousand]
.Lcr_delay_convert:
 cvttsd2si rax,xmm0
 test rax,rax
 js .Lcr_delay_absent
 jmp .Lcr_delay_end
.Lcr_date_delay:
 lea rdi,[rbx+PG_MM_RETRY_AFTER]
 xor esi,esi
 call curl_getdate@PLT
 cmp rax,-1
 je .Lcr_delay_absent
 imul rbp,rax,1000
 jo .Lcr_delay_absent
 call .Lcr_wall_now
 cmp rax,-1
 je .Lcr_delay_absent
 sub rbp,rax
 mov rax,rbp
 xor edx,edx
 test rax,rax
 cmovs rax,rdx
 jmp .Lcr_delay_end
.Lcr_delay_absent:
 mov rax,-1
.Lcr_delay_end:
 add rsp,8
 pop rbx
 pop rbp
 ret
.section .rodata
.Lcr_after: .asciz "retry-after:"
.Lcr_after_ms: .asciz "retry-after-ms:"
.Lcr_max_delay: .asciz "maxRetryDelayMs"
.Lcr_space: .asciz " \t\r\n"
.p2align 3
.Lcr_thousand: .double 1000
.section .data.rel.ro
.Lcr_header_names: .quad .Lcr_after,.Lcr_after_ms

.text
.Lcr_validate:
 push rbx
 mov rbx,rdi
 call pg_mimo_config_codex@PLT
 test eax,eax
 jz .Lcr_validate_ok
 mov rdi,rbx
 lea rsi,[rip+.Lcr_max_retries]
 xor edx,edx
 call .Lcr_integer
 test rax,rax
 js .Lcr_validate_bad
 mov rdi,rbx
 lea rsi,[rip+.Lcr_max_delay]
 mov edx,60000
 call .Lcr_integer
 test rax,rax
 js .Lcr_validate_bad
.Lcr_validate_ok:
 xor eax,eax
 pop rbx
 ret
.Lcr_validate_bad:
 mov eax,1
 pop rbx
 ret

.text
/* Fixed upstream alternatives: prefix, optional non-newline byte, suffix. */
.Lcr_match_pair:
 push rbx
 push r12
 push r13
 push r14
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rsi
 call strlen@PLT
 mov r14,rax
.Lcr_match_find:
 mov rdi,rbx
 mov rsi,r12
 call strcasestr@PLT
 test rax,rax
 jz .Lcr_match_end
 lea rbx,[rax+1]
 lea rdi,[rax+r14]
 mov [rsp],rdi
 mov rsi,r13
 mov rdi,r13
 call strlen@PLT
 mov rdx,rax
 mov rdi,[rsp]
 mov rsi,r13
 call strncasecmp@PLT
 test eax,eax
 jz .Lcr_match_yes
 mov rdi,[rsp]
 cmp byte ptr [rdi],0
 je .Lcr_match_find
 cmp byte ptr [rdi],10
 je .Lcr_match_find
 inc rdi
 mov [rsp],rdi
 mov rdi,r13
 call strlen@PLT
 mov rdx,rax
 mov rdi,[rsp]
 mov rsi,r13
 call strncasecmp@PLT
 test eax,eax
 jnz .Lcr_match_find
.Lcr_match_yes:
 mov eax,1
.Lcr_match_end:
 add rsp,8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.section .rodata
.Lcr_rate: .asciz "rate"
.Lcr_limit: .asciz "limit"
.Lcr_overloaded: .asciz "overloaded"
.Lcr_empty: .asciz ""
.Lcr_service: .asciz "service"
.Lcr_unavailable: .asciz "unavailable"
.Lcr_upstream: .asciz "upstream"
.Lcr_connect: .asciz "connect"
.Lcr_connection: .asciz "connection"
.Lcr_refused: .asciz "refused"
.section .data.rel.ro
.Lcr_transient_pairs: .quad .Lcr_rate,.Lcr_limit,.Lcr_overloaded,.Lcr_empty,.Lcr_service,.Lcr_unavailable,.Lcr_upstream,.Lcr_connect,.Lcr_connection,.Lcr_refused,0,0
