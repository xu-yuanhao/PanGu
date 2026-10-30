/* mimo_stream.S — OpenAI SSE 单行解码与片段累积（decode_line）。
 *
 * 输入是已去除 CR/LF 的完整一行：空行/":" 注释→WAIT 0；"data:" 前缀外→1；
 * "[DONE]" 仅在 finish_reason 之后接受一次。JSON 须完整（parse_end 后仅
 * 空白），单 choice（长度恰 1）、tool_calls 按整数index 0..7分别累积；id/name/
 * arguments 为增量追加（null 视为缺席），text 为当前分片整串替换。工具参数
 * 动态累积并限制在 128 KiB；其他字符串含嵌入 NUL、超出定长容量即拒绝。
 * finish_reason：stop 需无工具 id，
 * tool_calls 需所有索引连续、id/name齐备且ID唯一；length/end为文本终态。
 * usage可在空choices分片中更新；已finish后拒绝新的choice，DONE后拒绝数据。
 * event 借用 m 内缓冲，有效期至下一次 decode/生命周期调用。布局见
 * mimo_layout.h；json-c 经 PLT 调用，caller-saved 不跨调用保存活跃值。
 */
#include "mimo_layout.h"
.intel_syntax noprefix

.section .rodata
.Lstr_data:          .asciz "data:"
.Lstr_done:          .asciz "[DONE]"
.Lstr_choices:       .asciz "choices"
.Lstr_delta:         .asciz "delta"
.Lstr_tool_calls:    .asciz "tool_calls"
.Lstr_content:       .asciz "content"
.Lstr_finish_reason: .asciz "finish_reason"
.Lstr_index:         .asciz "index"
.Lstr_function:      .asciz "function"
.Lstr_id:            .asciz "id"
.Lstr_name:          .asciz "name"
.Lstr_arguments:     .asciz "arguments"
.Lstr_stop:          .asciz "stop"
.Lstr_reason_calls:  .asciz "tool_calls"

.section .text

/* ---- json_object *li_get(obj, key)：缺失/obj 为空返回 0。文件内私有。 ---- */
.type li_get,@function
li_get:
 test rdi, rdi
 jz .Lget_null
 push rax                             /* 入口 rsp≡8 → 0，可调用 */
 mov qword ptr [rsp], 0
 mov rdx, rsp
 call json_object_object_get_ex@PLT
 mov rax, qword ptr [rsp]
 pop rcx
 ret
.Lget_null:
 xor eax, eax
 ret
.size li_get, .-li_get

/* ---- int li_extract(o, out, cap)：字符串分片整串替换，嵌入 NUL/超界拒绝。 */
.type li_extract,@function
li_extract:
 test rdi, rdi
 jz .Lex_fail
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov r13, rdi                         /* o */
 mov rbx, rsi                         /* out */
 mov r12, rdx                         /* cap */
 mov esi, PG_MM_JSON_STRING
 call json_object_is_type@PLT
 test eax, eax
 jz .Lex_bad
 mov rdi, r13
 call json_object_get_string_len@PLT
 mov r14d, eax                        /* n（>=0） */
 cmp r14, r12
 jae .Lex_bad
 mov rdi, r13
 call json_object_get_string@PLT
 mov r15, rax                         /* s */
 mov rdi, r15
 xor esi, esi
 mov rdx, r14
 call memchr@PLT
 test rax, rax
 jnz .Lex_bad
 mov rdi, rbx
 mov rsi, r15
 lea rdx, [r14 + 1]
 call memcpy@PLT
 xor eax, eax
 jmp .Lex_out
.Lex_bad:
 mov eax, 1
 jmp .Lex_out_pop
.Lex_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Lex_fail:
 mov eax, 1
 ret
.Lex_out_pop:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_extract, .-li_extract

/* ---- int li_accumulate(value, out, capacity)：null→0 缺席；字符串追加。 */
.type li_accumulate,@function
li_accumulate:
 test rdi, rdi
 jz .Lac_zero
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 16                          /* [rsp]=n；5压栈≡0 再减 16 仍≡0 */
 mov r13, rdi                         /* value */
 mov rbx, rsi                         /* out */
 mov r12, rdx                         /* capacity */
 mov esi, PG_MM_JSON_NULL
 call json_object_is_type@PLT
 test eax, eax
 jnz .Lac_ok
 mov rdi, r13
 mov esi, PG_MM_JSON_STRING
 call json_object_is_type@PLT
 test eax, eax
 jz .Lac_bad
 mov rdi, rbx
 call strlen@PLT
 mov r14, rax                         /* old */
 mov rdi, r13
 call json_object_get_string_len@PLT
 mov eax, eax                         /* int → 无符号扩展 */
 mov qword ptr [rsp], rax             /* n */
 mov rax, r12
 sub rax, r14                         /* capacity - old */
 cmp qword ptr [rsp], rax
 jae .Lac_bad
 mov rdi, r13
 call json_object_get_string@PLT
 mov r15, rax                         /* s */
 mov rdi, r15
 xor esi, esi
 mov rdx, qword ptr [rsp]
 call memchr@PLT
 test rax, rax
 jnz .Lac_bad
 lea rdi, [rbx + r14]
 mov rsi, r15
 mov rdx, qword ptr [rsp]
 inc rdx                              /* 含终止 NUL */
 call memcpy@PLT
.Lac_ok:
 xor eax, eax
 jmp .Lac_out
.Lac_bad:
 mov eax, 1
.Lac_out:
 add rsp, 16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Lac_zero:
 xor eax, eax
 ret
.size li_accumulate, .-li_accumulate

/* ---- int li_accumulate_argument(value, pg_mimo *, index)：有界追加工具JSON。 ---- */
.type li_accumulate_argument,@function
li_accumulate_argument:
 test rdi,rdi
 jz .Laa_absent
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,48
 mov r12,rdi
 mov rbx,rsi
 mov r13d,edx
 cmp r13d,PG_MM_CALL_LIMIT
 jae .Laa_bad
 mov esi,PG_MM_JSON_NULL
 call json_object_is_type@PLT
 test eax,eax
 jnz .Laa_ok
 mov rdi,r12
 mov esi,PG_MM_JSON_STRING
 call json_object_is_type@PLT
 test eax,eax
 jz .Laa_bad
 mov rdi,r12
 call json_object_get_string_len@PLT
 test eax,eax
 js .Laa_bad
 mov r14d,eax
 mov rdi,r12
 call json_object_get_string@PLT
 mov r15,rax
 test r15,r15
 jz .Laa_bad
 mov rdi,r15
 xor esi,esi
 mov rdx,r14
 call memchr@PLT
 test rax,rax
 jnz .Laa_bad
 mov rax,[rbx+PG_MM_ARGUMENT_LENGTH+r13*8]
 mov [rsp],rax
 mov rdx,r14
 add rdx,rax
 jc .Laa_bad
 cmp rdx,PG_TL_ARGUMENT_LIMIT
 jae .Laa_bad
 mov [rsp+8],rdx
 mov rax,r13
 PG_MM_CALL_SLOT r10,rax
 mov [rsp+16],r10
 mov rax,[rbx+PG_MM_ARGUMENT_OVERFLOW+r13*8]
 mov [rsp+24],rax
 test rax,rax
 jnz .Laa_dynamic
 cmp qword ptr [rsp+8],PG_MM_TEXT_LIMIT
 jae .Laa_grow
 mov rdi,[rsp+16]
 add rdi,[rsp]
 mov rsi,r15
 mov rdx,r14
 inc rdx
 call memcpy@PLT
 mov rax,[rsp+8]
 mov [rbx+PG_MM_ARGUMENT_LENGTH+r13*8],rax
 xor eax,eax
 jmp .Laa_out
.Laa_dynamic:
 mov rax,[rsp+8]
 inc rax
 mov [rsp+32],rax
 mov r10,[rbx+PG_MM_ARGUMENT_CAPACITY+r13*8]
 cmp r10,rax
 jae .Laa_append
.Laa_grow:
 mov rax,[rsp+8]
 inc rax
 mov [rsp+32],rax
 mov r10,[rbx+PG_MM_ARGUMENT_CAPACITY+r13*8]
 test r10,r10
 jnz .Laa_grow_loop
 mov r10,PG_MM_TEXT_LIMIT*2
.Laa_grow_loop:
 cmp r10,[rsp+32]
 jae .Laa_allocate
 shl r10,1
 jmp .Laa_grow_loop
.Laa_allocate:
 mov [rsp+40],r10
 mov rdi,[rsp+24]
 test rdi,rdi
 jnz .Laa_reallocate
 mov rdi,r10
 call malloc@PLT
 test rax,rax
 jz .Laa_bad
 mov [rsp+24],rax
 mov rdi,rax
 mov rsi,[rsp+16]
 mov rdx,[rsp]
 inc rdx
 call memcpy@PLT
 mov rax,[rsp+24]
 mov [rbx+PG_MM_ARGUMENT_OVERFLOW+r13*8],rax
 mov rax,[rsp+40]
 mov [rbx+PG_MM_ARGUMENT_CAPACITY+r13*8],rax
 jmp .Laa_append
.Laa_reallocate:
 mov rsi,[rsp+40]
 call realloc@PLT
 test rax,rax
 jz .Laa_bad
 mov [rbx+PG_MM_ARGUMENT_OVERFLOW+r13*8],rax
 mov rdx,[rsp+40]
 mov [rbx+PG_MM_ARGUMENT_CAPACITY+r13*8],rdx
.Laa_append:
 mov rdi,[rbx+PG_MM_ARGUMENT_OVERFLOW+r13*8]
 add rdi,[rsp]
 mov rsi,r15
 mov rdx,r14
 inc rdx
 call memcpy@PLT
 mov rax,[rsp+8]
 mov [rbx+PG_MM_ARGUMENT_LENGTH+r13*8],rax
 xor eax,eax
 jmp .Laa_out
.Laa_bad:
 mov eax,1
.Laa_out:
 add rsp,48
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Laa_ok:
 xor eax,eax
 jmp .Laa_out
.Laa_absent:
 xor eax,eax
 ret
.size li_accumulate_argument,.-li_accumulate_argument

/* ---- int pg_mimo_decode_line(pg_mimo *m, const char *line, pg_model_event *event)
 * 栈帧 112 字节：[0]=tok [8]=choices [16]=choice [24]=delta [32]=data
 * [40]=calls [48]=content [56]=reason [64]=call [72]=index，[80]=delta个数，[88]=迭代，[96]=slot，[104]=fn，[112]=call index。 ---- */
.globl pg_mimo_decode_line
.type pg_mimo_decode_line,@function
pg_mimo_decode_line:
 push rdi
 push rsi
 push rdx
 mov rdi,[rdi+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 pop rdx
 pop rsi
 pop rdi
 test eax,eax
 jnz .Lbt_decode
 push rdi
 push rsi
 push rdx
 call pg_mimo_uses_responses
 pop rdx
 pop rsi
 pop rdi
 test eax,eax
 jnz .Lrs_decode
 push rdi
 push rsi
 push rdx
 call pg_mimo_uses_anthropic
 pop rdx
 pop rsi
 pop rdi
 test eax,eax
 jnz .Lrs_decode
 push rdi
 push rsi
 push rdx
 call pg_mimo_uses_google
 pop rdx
 pop rsi
 pop rdi
 test eax,eax
 jnz .Lrs_decode
.Lchat_decode:
 push rbx
 push r12
 push r13
 push r14
 push r15                             /* 入口 rsp≡8；5 压栈后 ≡0 */
 sub rsp, 128
 mov rbx, rdi                         /* m */
 mov r12, rsi                         /* line */
 mov r13, rdx                         /* event */
 mov rdi, r13                         /* *event = {WAIT} */
 xor eax, eax
 mov ecx, 7
 cld
 rep stosq
 movzx eax, byte ptr [r12]
 test al, al
 jz .Ldl_ok                           /* 空行 */
 cmp al, ':'
 je .Ldl_ok                           /* 注释行 */
 mov rdi, r12
 lea rsi, [rip + .Lstr_data]
 mov edx, 5
 call strncmp@PLT
 test eax, eax
 jnz .Ldl_bad
 lea rax, [r12 + 5]
 cmp byte ptr [rax], ' '
 jne 1f
 inc rax
1:
 mov qword ptr [rsp + 32], rax        /* data */
 mov rdi, rax
 lea rsi, [rip + .Lstr_done]
 call strcmp@PLT
 test eax, eax
 jnz .Ldl_parse
 /* [DONE]：仅在已有 finish_reason 且未见过时接受 */
 cmp dword ptr [rbx + PG_MM_FINISH_REASON], 0
 je .Ldl_bad
 cmp dword ptr [rbx + PG_MM_SAW_DONE], 0
 jne .Ldl_bad
 mov dword ptr [rbx + PG_MM_SAW_DONE], 1
 jmp .Ldl_ok
.Ldl_parse:
 cmp dword ptr [rbx + PG_MM_SAW_DONE], 0
 jne .Ldl_bad                         /* 终态后再收数据 */
 call json_tokener_new@PLT
 test rax, rax
 jz .Ldl_bad
 mov qword ptr [rsp], rax             /* tok */
 mov rdi, qword ptr [rsp + 32]
 call strlen@PLT
 mov rdi, qword ptr [rsp]
 mov rsi, qword ptr [rsp + 32]
 mov rdx, rax                         /* payload length is bounded by the receive buffer */
 call json_tokener_parse_ex@PLT
 mov r14, rax                         /* root */
 mov rdi, qword ptr [rsp]
 call json_tokener_get_error@PLT
 xor r15d, r15d                       /* failed */
 test eax, eax
 jz 1f
 mov r15d, 1
1:
 mov rdi, qword ptr [rsp]
 call json_tokener_get_parse_end@PLT
 mov rcx, rax                         /* parsed */
.Ldl_ws:
 mov rax, qword ptr [rsp + 32]
 movzx edx, byte ptr [rax + rcx]
 cmp dl, ' '
 je .Ldl_ws_next
 cmp dl, 9
 je .Ldl_ws_next
 jmp .Ldl_ws_done
.Ldl_ws_next:
 inc rcx
 jmp .Ldl_ws
.Ldl_ws_done:
 mov rax, qword ptr [rsp + 32]
 cmp byte ptr [rax + rcx], 0
 je 1f
 mov r15d, 1                          /* JSON 后残余 */
1:
 mov rdi, qword ptr [rsp]
 call json_tokener_free@PLT
.Ldl_decoded:
 mov rdi,r14
 lea rsi,[rip+.Lstr_usage]
 call li_get
 test rax,rax
 jz .Lusage_absent
 mov r12,rax
 mov rdi,rax
 mov esi,PG_MM_JSON_OBJECT
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldl_bad_put
 mov rdi,r12
 call json_object_get@PLT
 mov r12,rax
 mov rdi,[rbx+PG_MM_USAGE]
 call json_object_put@PLT
 mov [rbx+PG_MM_USAGE],r12
.Lusage_absent:
 mov rdi, r14
 lea rsi, [rip + .Lstr_choices]
 call li_get
 mov qword ptr [rsp + 8], rax         /* choices */
 test r15d, r15d
 jnz .Ldl_bad_put
 test rax, rax
 jz .Ldl_bad_put
 mov rdi, rax
 mov esi, PG_MM_JSON_ARRAY
 call json_object_is_type@PLT
 test eax, eax
 jz .Ldl_bad_put
 mov rdi, qword ptr [rsp + 8]
 call json_object_array_length@PLT
 test rax, rax
 jz .Ldl_empty                        /* usage 空分片 → WAIT */
 cmp rax, 1
 jne .Ldl_bad_put                     /* 仅单 choice */
 cmp dword ptr [rbx + PG_MM_FINISH_REASON], 0
 jne .Ldl_bad_put                     /* 不得重复 finish */
 mov rdi, qword ptr [rsp + 8]
 xor esi, esi
 call json_object_array_get_idx@PLT
 mov qword ptr [rsp + 16], rax        /* choice */
 mov rdi, rax
 lea rsi, [rip + .Lstr_delta]
 call li_get
 mov qword ptr [rsp + 24], rax        /* delta */
 mov rdi, rax
 lea rsi, [rip + .Lstr_tool_calls]
 call li_get
 mov qword ptr [rsp + 40], rax        /* calls */
 mov rdi, qword ptr [rsp + 24]
 lea rsi, [rip + .Lstr_content]
 call li_get
 mov qword ptr [rsp + 48], rax        /* content */
 mov rdi, qword ptr [rsp + 16]
 lea rsi, [rip + .Lstr_finish_reason]
 call li_get
 mov qword ptr [rsp + 56], rax        /* reason */
 cmp qword ptr [rsp + 40], 0
 je .Ldl_content
 mov rdi, qword ptr [rsp + 40]
 mov esi, PG_MM_JSON_ARRAY
 call json_object_is_type@PLT
 test eax, eax
 jz .Ldl_failed
 mov rdi, qword ptr [rsp + 40]
 call json_object_array_length@PLT
 test rax,rax
 jz .Ldl_failed
 cmp rax,PG_MM_CALL_LIMIT
 ja .Ldl_failed
 mov [rsp+80],rax
 mov qword ptr [rsp+88],0
.Lcall_delta:
 mov rdi,[rsp+40]
 mov rsi,[rsp+88]
 call json_object_array_get_idx@PLT
 mov [rsp+64],rax
 mov rdi,rax
 lea rsi,[rip+.Lstr_index]
 call li_get
 test rax,rax
 jz .Ldl_failed
 mov [rsp+72],rax
 mov rdi,rax
 mov esi,PG_MM_JSON_INT
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldl_failed
 mov rdi,[rsp+72]
 call json_object_get_int64@PLT
 cmp rax,PG_MM_CALL_LIMIT-1
 ja .Ldl_failed
 lea ecx,[eax+1]
 cmp ecx,[rbx+PG_MM_CALL_COUNT]
 jbe .Lcall_counted
 mov [rbx+PG_MM_CALL_COUNT],ecx
.Lcall_counted:
 PG_MM_CALL_SLOT r12,rax
 mov [rsp+112],eax
 mov [rsp+96],r12
 mov rdi,[rsp+64]
 lea rsi,[rip+.Lstr_function]
 call li_get
 mov [rsp+104],rax
 test rax,rax
 jz .Lcall_fields
 mov rdi,rax
 mov esi,PG_MM_JSON_OBJECT
 call json_object_is_type@PLT
 test eax,eax
 jz .Ldl_failed
.Lcall_fields:
 mov rdi,[rsp+64]
 lea rsi,[rip+.Lstr_id]
 call li_get
 mov rdi,rax
 lea rsi,[r12+PG_MM_TEXT_LIMIT]
 mov edx,PG_MM_ID_LIMIT
 call li_accumulate
 or r15d,eax
 mov rdi,[rsp+104]
 lea rsi,[rip+.Lstr_name]
 call li_get
 mov rdi,rax
 lea rsi,[r12+PG_MM_TEXT_LIMIT+PG_MM_ID_LIMIT]
 mov edx,PG_MM_ID_LIMIT
 call li_accumulate
 or r15d,eax
 mov rdi,[rsp+104]
 lea rsi,[rip+.Lstr_arguments]
 call li_get
 mov rdi,rax
 mov rsi,rbx
 mov edx,[rsp+112]
 call li_accumulate_argument
 or r15d,eax
 inc qword ptr [rsp+88]
 mov rax,[rsp+88]
 cmp rax,[rsp+80]
 jb .Lcall_delta
.Ldl_content:
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lstr_reasoning_content]
 call li_get
 mov rdi,rax
 lea rsi,[rbx+PG_MM_REASONING]
 mov edx,PG_MM_TEXT_LIMIT
 call li_accumulate
 or r15d,eax
 cmp qword ptr [rsp + 48], 0
 je .Ldl_reason
 mov rdi, qword ptr [rsp + 48]
 mov esi, PG_MM_JSON_NULL
 call json_object_is_type@PLT
 test eax, eax
 jnz .Ldl_reason                      /* null 缺席 */
 mov rdi, qword ptr [rsp + 48]
 lea rsi, [rbx + PG_MM_TEXT]
 mov edx, PG_MM_TEXT_LIMIT
 call li_extract
 or r15d, eax
 test r15d, r15d
 jnz .Ldl_reason
 cmp byte ptr [rbx + PG_MM_TEXT], 0
 je .Ldl_reason
 mov dword ptr [r13 + PG_MM_EVT_KIND], PG_MM_KIND_TEXT
 lea rax, [rbx + PG_MM_TEXT]
 mov qword ptr [r13 + PG_MM_EVT_TEXT + PG_MM_SLICE_DATA], rax
 mov rdi, rax
 call strlen@PLT
 mov qword ptr [r13 + PG_MM_EVT_TEXT + PG_MM_SLICE_SIZE], rax
.Ldl_reason:
 cmp qword ptr [rsp + 56], 0
 je .Ldl_finish
 mov rdi, qword ptr [rsp + 56]
 mov esi, PG_MM_JSON_NULL
 call json_object_is_type@PLT
 test eax, eax
 jnz .Ldl_finish
 mov rdi, qword ptr [rsp + 56]
 mov esi, PG_MM_JSON_STRING
 call json_object_is_type@PLT
 test eax, eax
 jz .Ldl_failed
 mov rdi,rbx
 mov rsi,[rsp+56]
 call .Lchat_stop_data
 test eax,eax
 jnz .Ldl_failed
 mov rdi,qword ptr [rsp + 56]
 call json_object_get_string_len@PLT
 mov [rsp + 72],eax
 cmp eax,3
 je .Lreason_length_ok
 cmp eax,6
 je .Lreason_length_ok
 cmp eax,4
 je .Lreason_length_ok
 cmp eax,10
 jne .Ldl_failed
.Lreason_length_ok:
 mov rdi, qword ptr [rsp + 56]
 call json_object_get_string@PLT
 mov r12, rax                         /* r */
 cmp dword ptr [rsp+72],6
 jne .Lreason_end_alias
 mov rdi,r12
 lea rsi,[rip+.Lstr_length]
 call strcmp@PLT
 test eax,eax
 jnz .Ldl_failed
 cmp dword ptr [rbx+PG_MM_CALL_COUNT],0
 jne .Ldl_failed
 mov dword ptr [rbx+PG_MM_FINISH_REASON],3
 jmp .Ldl_finish
.Lreason_end_alias:
 cmp dword ptr [rsp+72],3
 jne .Lreason_normal
 mov rdi,r12
 lea rsi,[rip+.Lstr_end]
 call strcmp@PLT
 test eax,eax
 jnz .Ldl_failed
 cmp dword ptr [rbx+PG_MM_CALL_COUNT],0
 jne .Ldl_failed
 mov dword ptr [rbx+PG_MM_FINISH_REASON],1
 jmp .Ldl_finish
.Lreason_normal:
 mov rdi, r12
 lea rsi, [rip + .Lstr_stop]
 call strcmp@PLT
 test eax, eax
 jnz 1f
 cmp dword ptr [rsp + 72],4
 jne .Ldl_failed
 cmp dword ptr [rbx+PG_MM_CALL_COUNT],0
 jne .Ldl_failed
 cmp byte ptr [rbx + PG_MM_ID], 0     /* stop 不允许带工具 id */
 jne .Ldl_failed
 mov dword ptr [rbx + PG_MM_FINISH_REASON], 1
 jmp .Ldl_finish
1:
 mov rdi, r12
 lea rsi, [rip + .Lstr_reason_calls]
 call strcmp@PLT
 test eax, eax
 jnz .Ldl_failed                      /* 其余 finish 理由拒绝 */
 cmp dword ptr [rsp + 72],10
 jne .Ldl_failed
 cmp dword ptr [rbx+PG_MM_CALL_COUNT],0
 je .Ldl_failed
 cmp dword ptr [rbx+PG_MM_CALL_COUNT],PG_MM_CALL_LIMIT
 ja .Ldl_failed
 mov qword ptr [rsp+88],0
.Lfinish_call:
 mov rax,[rsp+88]
 PG_MM_CALL_SLOT r12,rax
 cmp byte ptr [r12+PG_MM_TEXT_LIMIT],0
 je .Ldl_failed
 cmp byte ptr [r12+PG_MM_TEXT_LIMIT+PG_MM_ID_LIMIT],0
 je .Ldl_failed
 mov qword ptr [rsp+104],0
.Lfinish_unique:
 mov rax,[rsp+104]
 cmp rax,[rsp+88]
 jae .Lfinish_next
 PG_MM_CALL_SLOT rdx,rax
 lea rdi,[r12+PG_MM_TEXT_LIMIT]
 lea rsi,[rdx+PG_MM_TEXT_LIMIT]
 call strcmp@PLT
 test eax,eax
 jz .Ldl_failed
 inc qword ptr [rsp+104]
 jmp .Lfinish_unique
.Lfinish_next:
 inc qword ptr [rsp+88]
 mov eax,[rbx+PG_MM_CALL_COUNT]
 cmp [rsp+88],rax
 jb .Lfinish_call
 mov dword ptr [rbx + PG_MM_FINISH_REASON], 2
.Ldl_finish:
 test r14, r14
 jz 1f
 mov rdi, r14
 call json_object_put@PLT
1:
 mov eax, r15d
 jmp .Ldl_out
.Ldl_failed:
 mov r15d, 1
 jmp .Ldl_finish
.Ldl_empty:
 mov rdi, r14
 test rdi, rdi
 jz .Ldl_ok
 call json_object_put@PLT
 jmp .Ldl_ok
.Ldl_bad_put:
 test r14, r14
 jz .Ldl_bad
 mov rdi, r14
 call json_object_put@PLT
.Ldl_bad:
 mov eax, 1
 jmp .Ldl_out
.Ldl_ok:
 xor eax, eax
.Ldl_out:
 add rsp, 128
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_mimo_decode_line, .-pg_mimo_decode_line

.section .note.GNU-stack,"",@progbits

.section .rodata
.Lstr_usage: .asciz "usage"
.Lstr_length: .asciz "length"
.Lstr_end: .asciz "end"

.section .rodata
.Lstr_reasoning_content: .asciz "reasoning_content"

.macro RSGET obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lrs_\key]
 call li_get
.endm
.macro RSCOPY dst,src,from,to
 mov rdi,\dst
 mov rsi,\src
 lea rdx,[rip+.Lrs_\from]
 lea rcx,[rip+.Lrs_\to]
 call .Lrs_copy
 test eax,eax
 jnz .Lrs_bad
.endm
.text
.Lrs_copy:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rcx
 mov rdi,rsi
 mov rsi,rdx
 call li_get
 test rax,rax
 jz .Lrs_copy_end
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call .Lrs_set
.Lrs_copy_end:
 add rsp,8
 pop r12
 pop rbx
 ret
/* The normalized object borrows its caller's tree; the common accumulator
   releases one retained reference, without serializing and parsing it again. */
.Lchat_object:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,128
 mov rbx,rdi
 mov r14,rsi
 mov r13,rdx
 xor r15d,r15d
 mov rdi,r14
 call json_object_get@PLT
 jmp .Ldl_decoded
/* Normalize Responses items onto the existing bounded native event accumulator.
   output_index is sparse: only function calls consume a dense tool slot. */
.Lrs_decode:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,112
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rsp
 xor eax,eax
 mov ecx,14
 rep stosq
 mov rdi,r13
 mov ecx,7
 rep stosq
 cmp byte ptr [r12],0
 je .Lrs_ok
 cmp byte ptr [r12],':'
 je .Lrs_ok
 mov rdi,r12
 lea rsi,[rip+.Lrs_event_prefix]
 mov edx,6
 call strncmp@PLT
 test eax,eax
 jz .Lrs_ok
 mov rdi,r12
 lea rsi,[rip+.Lstr_data]
 mov edx,5
 call strncmp@PLT
 test eax,eax
 jnz .Lrs_bad
 mov rdi,rbx
 call pg_mimo_uses_google
 test eax,eax
 jnz .Lgo_parse_allowed
 cmp dword ptr [rbx+PG_MM_SAW_DONE],0
 jne .Lrs_bad
.Lgo_parse_allowed:
 add r12,5
 cmp byte ptr [r12],32
 jne .Lrs_parse
 inc r12
.Lrs_parse:
 call json_tokener_new@PLT
 mov [rsp+80],rax
 test rax,rax
 jz .Lrs_bad
 mov rdi,rax
 mov esi,17
 call json_tokener_set_flags@PLT
 mov rdi,r12
 call strlen@PLT
 lea edx,[eax+1]
 mov rdi,[rsp+80]
 mov rsi,r12
 call json_tokener_parse_ex@PLT
 mov [rsp],rax
 mov rdi,[rsp+80]
 call json_tokener_get_error@PLT
 test eax,eax
 jnz .Lrs_bad
 mov rdi,[rsp+80]
 call json_tokener_get_parse_end@PLT
 cmp byte ptr [r12+rax],0
 jne .Lrs_bad
 mov rdi,rbx
 call pg_mimo_uses_google
 test eax,eax
 jz .Lgo_parsed_not_google
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r13
 call .Lgo_event
 mov ebp,eax
 jmp .Lrs_cleanup
.Lgo_parsed_not_google:
 RSGET "qword ptr [rsp]",type
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+56],rax
 test rax,rax
 jz .Lrs_bad
 mov rdi,rbx
 call pg_mimo_uses_anthropic
 test eax,eax
 jz .Lrs_openai_event
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r13
 call .Lan_event
 mov ebp,eax
 jmp .Lrs_cleanup
.Lrs_openai_event:
 lea rdi,[rip+.Lrs_base]
 call json_tokener_parse@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lrs_bad
 RSGET rax,choices
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov [rsp+16],rax
 RSGET rax,delta
 mov [rsp+24],rax
 lea r14,[rip+.Lrs_events]
 xor ebp,ebp
.Lrs_find:
 mov rdi,[rsp+56]
 mov rsi,[r14+rbp*8]
 test rsi,rsi
 jz .Lrs_ok
 call strcmp@PLT
 test eax,eax
 jz .Lrs_dispatch
 inc ebp
 jmp .Lrs_find
.Lrs_dispatch:
 cmp ebp,0
 je .Lrs_text_handler
 cmp ebp,1
 je .Lrs_text_handler
 cmp ebp,2
 je .Lrs_thinking_handler
 cmp ebp,3
 je .Lrs_thinking_handler
 cmp ebp,4
 je .Lrs_item_handler
 cmp ebp,5
 je .Lrs_arguments_handler
 cmp ebp,6
 je .Lrs_arguments_handler
 cmp ebp,7
 je .Lrs_item_handler
 cmp ebp,8
 je .Lrs_terminal
 cmp ebp,9
 je .Lrs_terminal
 RSGET "qword ptr [rsp]",response
 test rax,rax
 jnz .Lrs_failure_terminal
 mov rax,[rsp]
.Lrs_failure_terminal:
 mov rdi,rbx
 mov rsi,rax
 call .Lrt_save
 jmp .Lrs_bad
.Lrs_text_handler:
 RSCOPY "qword ptr [rsp+24]","qword ptr [rsp]",delta,content
 jmp .Lrs_emit
.Lrs_thinking_handler:
 RSCOPY "qword ptr [rsp+24]","qword ptr [rsp]",delta,reasoning_content
 jmp .Lrs_emit
.Lrs_item_handler:
 RSGET "qword ptr [rsp]",item
 mov [rsp+40],rax
 cmp ebp,7
 jne .Lrs_item_type
 mov rdi,rbx
 mov rsi,[rsp]
 call .Lrc_capture_item
 test eax,eax
 jnz .Lrs_bad
.Lrs_item_type:
 RSGET "qword ptr [rsp+40]",type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lrs_bad
 mov rdi,rax
 lea rsi,[rip+.Lrs_function_call]
 call strcmp@PLT
 test eax,eax
 jnz .Lrs_ok
.Lrs_arguments_handler:
 RSGET "qword ptr [rsp]",output_index
 mov rdi,rax
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lrs_bad
 RSGET "qword ptr [rsp]",output_index
 mov rdi,rax
 call json_object_get_int64@PLT
 test rax,rax
 js .Lrs_bad
 mov [rsp+96],rax
 xor r14d,r14d
.Lrs_slot:
 cmp r14d,[rbx+PG_MM_CALL_COUNT]
 jae .Lrs_new_slot
 cmp rax,[rbx+PG_MM_RESPONSE_INDEX+r14*8]
 je .Lrs_slot_ready
 inc r14d
 jmp .Lrs_slot
.Lrs_new_slot:
 cmp ebp,4
 je .Lrs_create_slot
 cmp ebp,7
 jne .Lrs_bad
.Lrs_create_slot:
 cmp r14d,PG_MM_CALL_LIMIT
 jae .Lrs_bad
 mov [rbx+PG_MM_RESPONSE_INDEX+r14*8],rax
 .Lrs_slot_ready:
 cmp ebp,5
 je .Lrt_closed_delta
 cmp ebp,6
 jne .Lrt_slot_open
.Lrt_closed_delta:
 mov eax,[rbx+PG_MM_RESPONSE_CLOSED]
 bt eax,r14d
 jc .Lrs_ok
.Lrt_slot_open:
 mov [rsp+64],r14
 lea rdi,[rip+.Lrs_calls_json]
 call json_tokener_parse@PLT
 test rax,rax
 jz .Lrs_bad
 mov [rsp+32],rax
 mov rdi,[rsp+24]
 lea rsi,[rip+.Lrs_tool_calls]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lrs_bad
 mov rdi,[rsp+32]
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov [rsp+32],rax
 mov rdi,r14
 call json_object_new_int64@PLT
 test rax,rax
 jz .Lrs_bad
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lrs_index]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lrs_bad
 RSGET "qword ptr [rsp+32]",function
 mov [rsp+88],rax
 cmp ebp,4
 je .Lrs_added_handler
 cmp ebp,7
 je .Lrs_item_done_handler
 cmp ebp,6
 je .Lrs_replace_arguments
 RSCOPY "qword ptr [rsp+88]","qword ptr [rsp]",delta,arguments
 jmp .Lrs_emit
.Lrs_added_handler:
 /* Only a new slot may introduce identity; done events replace arguments. */
 cmp r14d,[rbx+PG_MM_CALL_COUNT]
 jb .Lrs_bad
 RSCOPY "qword ptr [rsp+32]","qword ptr [rsp+40]",call_id,id
 RSCOPY "qword ptr [rsp+88]","qword ptr [rsp+40]",name,name
 RSCOPY "qword ptr [rsp+88]","qword ptr [rsp+40]",arguments,arguments
 jmp .Lrs_emit
.Lrs_item_done_handler:
 cmp r14d,[rbx+PG_MM_CALL_COUNT]
 jb .Lrs_item_existing
 RSCOPY "qword ptr [rsp+32]","qword ptr [rsp+40]",call_id,id
 RSCOPY "qword ptr [rsp+88]","qword ptr [rsp+40]",name,name
.Lrs_item_existing:
 RSGET "qword ptr [rsp+40]",arguments
 jmp .Lrs_replace_value
.Lrs_replace_arguments:
 RSGET "qword ptr [rsp]",arguments
.Lrs_replace_value:
 test rax,rax
 jz .Lrs_bad
 mov [rsp+96],rax
 mov qword ptr [rbx+PG_MM_ARGUMENT_LENGTH+r14*8],0
 mov rax,r14
 PG_MM_CALL_SLOT rdx,rax
 mov byte ptr [rdx],0
 mov rax,[rbx+PG_MM_ARGUMENT_OVERFLOW+r14*8]
 test rax,rax
 jz .Lrs_replace_field
 mov byte ptr [rax],0
.Lrs_replace_field:
 mov rdi,[rsp+96]
 call json_object_get@PLT
 mov rdi,[rsp+88]
 lea rsi,[rip+.Lrs_arguments]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lrs_bad
 jmp .Lrs_emit
.Lrs_terminal:
 RSGET "qword ptr [rsp]",response
 mov r12,rax
 test rax,rax
 jz .Lrs_bad
 mov rdi,rbx
 mov rsi,r12
 call .Lrt_save
 mov rdi,rbx
 mov rsi,r12
 call .Lrc_capture_terminal
 test eax,eax
 jnz .Lrs_bad
 RSGET r12,usage
 mov [rsp+88],rax
 test rax,rax
 jz .Lrs_finish
 lea rdi,[rip+.Lrs_empty]
 call json_tokener_parse@PLT
 test rax,rax
 jz .Lrs_bad
 mov r14,rax
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lrs_usage]
 mov rdx,r14
 call .Lrs_set
 test eax,eax
 jnz .Lrs_bad
 RSCOPY r14,"qword ptr [rsp+88]",input_tokens,prompt_tokens
 RSCOPY r14,"qword ptr [rsp+88]",output_tokens,completion_tokens
 RSCOPY r14,"qword ptr [rsp+88]",input_tokens_details,prompt_tokens_details
 RSCOPY r14,"qword ptr [rsp+88]",output_tokens_details,completion_tokens_details
.Lrs_finish:
 lea r14,[rip+.Lstr_stop]
 cmp dword ptr [rbx+PG_MM_CALL_COUNT],0
 je .Lrs_finish_reason
 lea r14,[rip+.Lstr_reason_calls]
.Lrs_finish_reason:
 cmp ebp,9
 jne .Lrs_finish_field
 RSGET r12,incomplete_details
 RSGET rax,reason
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lrs_bad
 mov rdi,rax
 lea rsi,[rip+.Lrs_max_output_tokens]
 call strcmp@PLT
 test eax,eax
 jnz .Lrs_bad
 lea r14,[rip+.Lstr_length]
.Lrs_finish_field:
 cmp ebp,9
 je .Lrt_finish_ready
 mov ecx,[rbx+PG_MM_CALL_COUNT]
 mov eax,1
 shl eax,cl
 dec eax
 cmp eax,[rbx+PG_MM_RESPONSE_CLOSED]
 jne .Lrs_bad
.Lrt_finish_ready:
 mov rdi,r14
 call json_object_new_string@PLT
 test rax,rax
 jz .Lrs_bad
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lrs_finish_reason_key]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lrs_bad
 mov qword ptr [rsp+72],1
.Lrs_emit:
 cmp ebp,9
 jne .Lrt_emit_common
 /* Keep incomplete calls out of the executable terminal path. */
 call json_object_new_array@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lrs_choices]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lrs_bad
 mov dword ptr [rbx+PG_MM_FINISH_REASON],3
.Lrt_emit_common:
 mov rdi,rbx
 mov rsi,[rsp+8]
 mov rdx,r13
 call .Lchat_object
 test eax,eax
 jnz .Lrs_bad
 cmp ebp,7
 jne .Lrt_emitted
 mov ecx,[rsp+64]
 mov eax,1
 shl eax,cl
 or [rbx+PG_MM_RESPONSE_CLOSED],eax
.Lrt_emitted:
 cmp qword ptr [rsp+72],0
 je .Lrs_ok
 mov dword ptr [rbx+PG_MM_SAW_DONE],1
.Lrs_ok:
 xor ebp,ebp
 jmp .Lrs_cleanup
.Lrs_bad:
 mov ebp,1
.Lrs_cleanup:
 mov rdi,[rsp+80]
 test rdi,rdi
 jz .Lrs_put
 call json_tokener_free@PLT
.Lrs_put:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,112
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lrs_event_prefix: .asciz "event:"
.Lrs_base: .asciz "{\"choices\":[{\"delta\":{}}]}"
.Lrs_calls_json: .asciz "[{\"index\":0,\"function\":{}}]"
.Lrs_empty: .asciz "{}"
.Lrs_type: .asciz "type"
.Lrs_choices: .asciz "choices"
.Lrs_delta: .asciz "delta"
.Lrs_content: .asciz "content"
.Lrs_reasoning_content: .asciz "reasoning_content"
.Lrs_item: .asciz "item"
.Lrs_function_call: .asciz "function_call"
.Lrs_output_index: .asciz "output_index"
.Lrs_tool_calls: .asciz "tool_calls"
.Lrs_index: .asciz "index"
.Lrs_function: .asciz "function"
.Lrs_call_id: .asciz "call_id"
.Lrs_id: .asciz "id"
.Lrs_name: .asciz "name"
.Lrs_arguments: .asciz "arguments"
.Lrs_response: .asciz "response"
.Lrs_usage: .asciz "usage"
.Lrs_input_tokens: .asciz "input_tokens"
.Lrs_output_tokens: .asciz "output_tokens"
.Lrs_input_tokens_details: .asciz "input_tokens_details"
.Lrs_output_tokens_details: .asciz "output_tokens_details"
.Lrs_prompt_tokens: .asciz "prompt_tokens"
.Lrs_completion_tokens: .asciz "completion_tokens"
.Lrs_prompt_tokens_details: .asciz "prompt_tokens_details"
.Lrs_completion_tokens_details: .asciz "completion_tokens_details"
.Lrs_incomplete_details: .asciz "incomplete_details"
.Lrs_reason: .asciz "reason"
.Lrs_max_output_tokens: .asciz "max_output_tokens"
.Lrs_finish_reason_key: .asciz "finish_reason"
.Lrs_text: .asciz "response.output_text.delta"
.Lrs_refusal: .asciz "response.refusal.delta"
.Lrs_thinking: .asciz "response.reasoning_summary_text.delta"
.Lrs_reasoning_text: .asciz "response.reasoning_text.delta"
.Lrs_added: .asciz "response.output_item.added"
.Lrs_arg_delta: .asciz "response.function_call_arguments.delta"
.Lrs_arg_done: .asciz "response.function_call_arguments.done"
.Lrs_item_done: .asciz "response.output_item.done"
.Lrs_completed: .asciz "response.completed"
.Lrs_incomplete: .asciz "response.incomplete"
.Lrs_failed: .asciz "response.failed"
.Lrs_error: .asciz "error"
.section .data.rel.ro
.Lrs_events: .quad .Lrs_text,.Lrs_refusal,.Lrs_thinking,.Lrs_reasoning_text,.Lrs_added,.Lrs_arg_delta,.Lrs_arg_done,.Lrs_item_done,.Lrs_completed,.Lrs_incomplete,.Lrs_failed,.Lrs_error,0

.text
.Lrs_set:
 push rbx
 mov rbx,rdx
 test rdx,rdx
 jz .Lrs_set_bad
 call json_object_object_add@PLT
 test eax,eax
 jz .Lrs_set_end
 mov rdi,rbx
 call json_object_put@PLT
.Lrs_set_bad:
 mov eax,1
.Lrs_set_end:
 pop rbx
 ret

.text
.Lrc_capture_item:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
 mov rdi,r12
 lea rsi,[rip+.Lrs_output_index]
 call li_get
 mov rbp,rax
 mov rdi,rax
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lrc_capture_bad
 mov rdi,rbp
 call json_object_get_int64@PLT
 test rax,rax
 js .Lrc_capture_bad
 mov [rsp],rax
 cmp qword ptr [rbx+PG_MM_RESPONSE_OUTPUT],0
 jne .Lrc_capture_slot
 call json_object_new_array@PLT
 mov [rbx+PG_MM_RESPONSE_OUTPUT],rax
 test rax,rax
 jz .Lrc_capture_bad
.Lrc_capture_slot:
 call json_object_new_object@PLT
 mov r13,rax
 test rax,rax
 jz .Lrc_capture_bad
 mov rdi,rbp
 call json_object_get@PLT
 mov rdi,r13
 lea rsi,[rip+.Lrc_index]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lrc_capture_bad
 mov rdi,r13
 mov rsi,r12
 lea rdx,[rip+.Lrc_item]
 mov rcx,rdx
 call .Lrs_copy
 test eax,eax
 jnz .Lrc_capture_bad
 xor r14d,r14d
.Lrc_capture_find:
 mov rdi,[rbx+PG_MM_RESPONSE_OUTPUT]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lrc_capture_insert
 mov rdi,[rbx+PG_MM_RESPONSE_OUTPUT]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rip+.Lrc_index]
 call li_get
 mov rdi,rax
 call json_object_get_int64@PLT
 cmp rax,[rsp]
 ja .Lrc_capture_insert
 je .Lrc_capture_replace
 inc r14
 jmp .Lrc_capture_find
.Lrc_capture_insert:
 mov rdi,[rbx+PG_MM_RESPONSE_OUTPUT]
 mov rsi,r14
 mov rdx,r13
 call json_object_array_insert_idx@PLT
 jmp .Lrc_capture_adopt
.Lrc_capture_replace:
 mov rdi,[rbx+PG_MM_RESPONSE_OUTPUT]
 mov rsi,r14
 mov rdx,r13
 call json_object_array_put_idx@PLT
.Lrc_capture_adopt:
 test eax,eax
 jnz .Lrc_capture_bad
 xor r13d,r13d
 xor ebp,ebp
 jmp .Lrc_capture_end
.Lrc_capture_bad:
 mov ebp,1
.Lrc_capture_end:
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
.Lrc_capture_terminal:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsi
 lea rsi,[rip+.Lrc_id]
 call li_get
 test rax,rax
 jz .Lrc_capture_output
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp],rax
 mov rdi,[rbx+PG_MM_RESPONSE_ID]
 call json_object_put@PLT
 mov rax,[rsp]
 mov [rbx+PG_MM_RESPONSE_ID],rax
.Lrc_capture_output:
 mov rdi,r12
 lea rsi,[rip+.Lrc_output]
 call li_get
 mov r12,rax
 test rax,rax
 jz .Lrc_terminal_ok
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lrc_terminal_bad
 mov rdi,r12
 call json_object_array_length@PLT
 test rax,rax
 jz .Lrc_terminal_ok
 mov rdi,rbx
 mov rsi,r12
 call .Lrc_merge_output
 test eax,eax
 jnz .Lrc_terminal_bad
.Lrc_terminal_ok:
 xor eax,eax
 jmp .Lrc_terminal_end
.Lrc_terminal_bad:
 mov eax,1
.Lrc_terminal_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.macro RCGET obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lrc_\key]
 call li_get
.endm
.macro RCCOPY dst,src,from,to
 mov rdi,\dst
 mov rsi,\src
 lea rdx,[rip+.Lrc_\from]
 lea rcx,[rip+.Lrc_\to]
 call .Lrs_copy
 test eax,eax
 jnz .Lrc_bad
.endm
.Lrc_append:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rsi
 call json_tokener_parse@PLT
 mov r12,rax
 test rax,rax
 jz .Lrc_append_end
 mov rdi,rbx
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jz .Lrc_append_ok
 mov rdi,r12
 call json_object_put@PLT
 xor r12d,r12d
.Lrc_append_ok:
 mov rax,r12
.Lrc_append_end:
 add rsp,8
 pop r12
 pop rbx
 ret
/* Final item payloads, including terminal-only encrypted reasoning, become
   detached canonical content before the Generation transaction adopts it. */
.globl pg_mimo_responses_complete
.type pg_mimo_responses_complete,@function
pg_mimo_responses_complete:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov r12,[rdi+PG_MM_RESPONSE_OUTPUT]
 mov rdi,rsp
 xor eax,eax
 mov ecx,8
 rep stosq
 mov [rsp+8],rsi
 mov rdi,rbx
 call pg_mimo_uses_responses@PLT
 test eax,eax
 jnz .Lrc_responses
 mov rdi,rbx
 call pg_mimo_uses_anthropic
 test eax,eax
 jnz .Lrc_anthropic
 mov rdi,rbx
 call pg_mimo_uses_google
 test eax,eax
 jnz .Lrc_anthropic
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jnz .Lrc_anthropic
 mov rdi,[rsp+8]
 mov rsi,[rbx+PG_MM_RESPONSE_TERMINAL]
 lea rdx,[rip+.Lstr_finish_reason]
 lea rcx,[rip+.Lrt_raw_stop]
 call .Lrs_copy
 test eax,eax
 jnz .Lrc_bad
 jmp .Lrc_ok
.Lrc_anthropic:
 mov rdi,rbx
 mov rsi,[rsp+8]
 call .Lan_complete
 mov ebp,eax
 jmp .Lrc_cleanup
.Lrc_responses:
 test r12,r12
 jz .Lrc_metadata
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lrc_bad
 xor r13d,r13d
.Lrc_each:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lrc_publish
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 RCGET rax,item
 mov r14,rax
 RCGET r14,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lrc_bad
 mov [rsp+56],rax
 mov rdi,rax
 lea rsi,[rip+.Lrc_function_call]
 call strcmp@PLT
 test eax,eax
 jz .Lrc_tool
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lrc_reasoning]
 call strcmp@PLT
 test eax,eax
 jz .Lrc_reason
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lrc_message]
 call strcmp@PLT
 test eax,eax
 jnz .Lrc_bad
 lea rdi,[rip+.Lrc_text_json]
 mov qword ptr [rsp+48],0
 jmp .Lrc_textual
.Lrc_reason:
 lea rdi,[rip+.Lrc_think_json]
 mov qword ptr [rsp+48],1
.Lrc_textual:
 mov rsi,[rsp]
 call .Lrc_append
 mov rbp,rax
 test rax,rax
 jz .Lrc_bad
 lea rdi,[rip+.Lrc_empty]
 call json_object_new_string@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lrc_bad
 RCGET r14,content
 cmp qword ptr [rsp+48],0
 je .Lrc_parts_ready
 RCGET r14,summary
 test rax,rax
 jz .Lrc_reason_content
 mov [rsp+32],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lrc_bad
 mov rdi,[rsp+32]
 call json_object_array_length@PLT
 test rax,rax
 jnz .Lrc_parts_loaded
.Lrc_reason_content:
 RCGET r14,content
.Lrc_parts_ready:
 mov [rsp+32],rax
.Lrc_parts_loaded:
 mov qword ptr [rsp+40],0
 cmp qword ptr [rsp+32],0
 je .Lrc_joined
 mov rdi,[rsp+32]
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lrc_bad
.Lrc_part:
 mov rdi,[rsp+32]
 call json_object_array_length@PLT
 cmp [rsp+40],rax
 jae .Lrc_joined
 mov rdi,[rsp+32]
 mov rsi,[rsp+40]
 call json_object_array_get_idx@PLT
 mov [rsp+24],rax
 RCGET rax,text
 test rax,rax
 jnz .Lrc_part_text
 RCGET "qword ptr [rsp+24]",refusal
 test rax,rax
 jz .Lrc_part_next
.Lrc_part_text:
 mov rsi,rax
 lea rdi,[rsp+16]
 mov edx,[rsp+48]
 add edx,edx
 call pg_context_append@PLT
 test eax,eax
 jnz .Lrc_bad
.Lrc_part_next:
 inc qword ptr [rsp+40]
 jmp .Lrc_part
.Lrc_joined:
 mov qword ptr [rsp+24],0
 lea rdi,[rip+.Lrc_text]
 cmp qword ptr [rsp+48],0
 je .Lrc_text_field
 lea rdi,[rip+.Lrc_thinking]
.Lrc_text_field:
 mov [rsp+56],rdi
 mov rdi,[rsp+16]
 call json_object_get@PLT
 mov rdi,rbp
 mov rsi,[rsp+56]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lrc_bad
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov qword ptr [rsp+16],0
 cmp qword ptr [rsp+48],0
 jne .Lrc_reason_signature
 lea rdi,[rip+.Lrc_sig_json]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lrc_bad
 RCCOPY "qword ptr [rsp+16]",r14,id,id
 RCCOPY "qword ptr [rsp+16]",r14,phase,phase
 mov rdi,[rsp+16]
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 mov rdi,rax
 call json_object_new_string@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lrc_textSignature]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lrc_bad
 jmp .Lrc_next_release
.Lrc_reason_signature:
 mov rdi,r14
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 mov rdi,rax
 call json_object_new_string@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lrc_thinkingSignature]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lrc_bad
 jmp .Lrc_next_release
.Lrc_tool:
 lea rdi,[rip+.Lrc_tool_json]
 mov rsi,[rsp]
 call .Lrc_append
 mov rbp,rax
 test rax,rax
 jz .Lrc_bad
 RCCOPY rbp,r14,call_id,id
 RCCOPY rbp,r14,name,name
 RCCOPY rbp,r14,id,responseItemId
 RCCOPY rbp,r14,namespace,namespace
 RCGET r14,arguments
 mov [rsp+24],rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 mov [rsp+32],rax
 mov rdi,[rsp+24]
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,[rsp+32]
 lea rdx,[rsp+16]
 call pg_mimo_arguments_parse
 test eax,eax
 jnz .Lrc_bad
 RCCOPY rbp,"qword ptr [rsp+16]",arguments,arguments
 RCCOPY rbp,"qword ptr [rsp+16]",argumentsError,argumentsError
.Lrc_next_release:
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov qword ptr [rsp+16],0
 inc r13
 jmp .Lrc_each
.Lrc_publish:
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lrc_content]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrs_set
 test eax,eax
 jnz .Lrc_bad
.Lrc_metadata:
 mov r14,[rbx+PG_MM_CONFIGURATION]
 RCCOPY "qword ptr [rsp+8]",r14,model,model
 RCCOPY "qword ptr [rsp+8]",r14,modelProvider,provider
 RCGET "qword ptr [rsp+8]",provider
 test rax,rax
 jnz .Lrc_api_metadata
 RCGET r14,modelInfo
 mov r14,rax
 RCCOPY "qword ptr [rsp+8]",r14,provider,provider
.Lrc_api_metadata:
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lrc_api]
 call json_object_object_get@PLT
 test rax,rax
 jnz .Lrc_configured_api
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 lea rsi,[rip+.Lrc_modelInfo]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lrc_api]
 call json_object_object_get@PLT
.Lrc_configured_api:
 mov rdi,rax
 call json_object_get_string@PLT
 lea rdi,[rip+.Lrc_api_value]
 test rax,rax
 cmovnz rdi,rax
 call json_object_new_string@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lrc_api]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lrc_bad
 mov rdi,rbx
 mov rsi,[rsp+8]
 call .Lrt_stop_data
 test eax,eax
 jnz .Lrc_bad
 mov rdi,[rbx+PG_MM_RESPONSE_ID]
 test rdi,rdi
 jz .Lrc_ok
 call json_object_get@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lrc_responseId]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lrc_bad
.Lrc_ok:
 xor ebp,ebp
 jmp .Lrc_cleanup
.Lrc_bad:
 mov ebp,1
.Lrc_cleanup:
 mov rdi,[rsp]
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
.size pg_mimo_responses_complete,.-pg_mimo_responses_complete
.section .rodata
.Lrc_id: .asciz "id"
.Lrc_output: .asciz "output"
.Lrc_type: .asciz "type"
.Lrc_function_call: .asciz "function_call"
.Lrc_reasoning: .asciz "reasoning"
.Lrc_message: .asciz "message"
.Lrc_content: .asciz "content"
.Lrc_summary: .asciz "summary"
.Lrc_text: .asciz "text"
.Lrc_refusal: .asciz "refusal"
.Lrc_thinking: .asciz "thinking"
.Lrc_phase: .asciz "phase"
.Lrc_textSignature: .asciz "textSignature"
.Lrc_thinkingSignature: .asciz "thinkingSignature"
.Lrc_call_id: .asciz "call_id"
.Lrc_name: .asciz "name"
.Lrc_responseItemId: .asciz "responseItemId"
.Lrc_namespace: .asciz "namespace"
.Lrc_arguments: .asciz "arguments"
.Lrc_model: .asciz "model"
.Lrc_modelProvider: .asciz "modelProvider"
.Lrc_provider: .asciz "provider"
.Lrc_api: .asciz "api"
.Lrc_api_value: .asciz "openai-responses"
.Lrc_responseId: .asciz "responseId"
.Lrc_empty: .asciz ""
.Lrc_text_json: .asciz "{\"type\":\"text\"}"
.Lrc_think_json: .asciz "{\"type\":\"thinking\"}"
.Lrc_tool_json: .asciz "{\"type\":\"toolCall\"}"
.Lrc_sig_json: .asciz "{\"v\":1}"

.text
.Lrc_inherit:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rsi,rdx
 call li_get
 test rax,rax
 jz .Lrc_inherit_copy
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lrc_inherit_string
 mov rdi,rbx
 mov rsi,r13
 call li_get
 mov rdi,rax
 call json_object_array_length@PLT
 jmp .Lrc_inherit_length
.Lrc_inherit_string:
 mov rdi,rbx
 mov rsi,r13
 call li_get
 mov rdi,rax
 call json_object_get_string_len@PLT
.Lrc_inherit_length:
 test rax,rax
 jnz .Lrc_inherit_done
.Lrc_inherit_copy:
 mov rdi,rbx
 mov rsi,r12
 mov rdx,r13
 mov rcx,r13
 call .Lrs_copy
 jmp .Lrc_inherit_end
.Lrc_inherit_done:
 xor eax,eax
.Lrc_inherit_end:
 pop r13
 pop r12
 pop rbx
 ret
/* A terminal reasoning item may contain only the encrypted backfill. Keep
   the completed summary/content while adopting that terminal signature. */
.Lrc_merge_output:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,[rdi+PG_MM_RESPONSE_OUTPUT]
 mov r12,rsi
 test rbx,rbx
 jz .Lrc_merge_ok
 xor r13d,r13d
.Lrc_merge_each:
 mov rdi,rbx
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Lrc_merge_ok
 mov rdi,rbx
 mov rsi,r13
 call json_object_array_get_idx@PLT
 RCGET rax,item
 mov r14,rax
 RCGET r14,type
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lrc_merge_next
 mov rdi,rax
 lea rsi,[rip+.Lrc_reasoning]
 call strcmp@PLT
 test eax,eax
 jnz .Lrc_merge_next
 RCGET r14,id
 mov [rsp+16],rax
 mov qword ptr [rsp+8],0
.Lrc_merge_find:
 mov rdi,r12
 call json_object_array_length@PLT
 cmp [rsp+8],rax
 jae .Lrc_merge_next
 mov rdi,r12
 mov rsi,[rsp+8]
 call json_object_array_get_idx@PLT
 mov rbp,rax
 RCGET rbp,id
 mov rdi,rax
 mov rsi,[rsp+16]
 call json_object_equal@PLT
 test eax,eax
 jnz .Lrc_merge_fields
 inc qword ptr [rsp+8]
 jmp .Lrc_merge_find
.Lrc_merge_fields:
 mov rdi,r14
 mov rsi,rbp
 lea rdx,[rip+.Lrc_encrypted_content]
 call .Lrc_inherit
 test eax,eax
 jnz .Lrc_merge_bad
.Lrc_merge_next:
 inc r13
 jmp .Lrc_merge_each
.Lrc_merge_ok:
 xor eax,eax
 jmp .Lrc_merge_end
.Lrc_merge_bad:
 mov eax,1
.Lrc_merge_end:
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lrc_encrypted_content: .asciz "encrypted_content"

.section .rodata
.Lrc_modelInfo: .asciz "modelInfo"

.section .rodata
.Lrc_index: .asciz "index"
.Lrc_item: .asciz "item"

.text
.Lrt_save:
 push rbx
 mov rbx,rdi
 mov rdi,rsi
 call json_object_get@PLT
 mov rdi,[rbx+PG_MM_RESPONSE_TERMINAL]
 mov [rbx+PG_MM_RESPONSE_TERMINAL],rax
 call json_object_put@PLT
 pop rbx
 ret
.globl pg_mimo_failure_data
.type pg_mimo_failure_data,@function
pg_mimo_failure_data:
 test rdi,rdi
 jz .Lrt_failure_invalid
 test rsi,rsi
 jz .Lrt_failure_invalid
 test rdx,rdx
 jz .Lrt_failure_invalid
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor ebp,ebp
 mov qword ptr [rsi],0
 mov qword ptr [rsi+8],0
 mov qword ptr [rdx],0
 mov qword ptr [rdx+8],0
 mov r14,[rbx+PG_MM_RESPONSE_TERMINAL]
 test r14,r14
 jz .Lrt_failure_absent
 mov rdi,r14
 lea rsi,[rip+.Lrt_error]
 call li_get
 test rax,rax
 jnz .Lrt_failure_payload
 mov rdi,r14
 lea rsi,[rip+.Lrs_incomplete_details]
 call li_get
 test rax,rax
 jz .Lrt_failure_serialize
.Lrt_failure_payload:
 mov r14,rax
.Lrt_failure_serialize:
 mov rdi,r14
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Lrt_failure_absent
 mov [r13],rax
 mov rdi,rax
 call strlen@PLT
 mov [r13+8],rax
 mov rdi,r14
 lea rsi,[rip+.Lrt_message]
 call li_get
 test rax,rax
 jnz .Lrt_failure_string
 mov rdi,r14
 lea rsi,[rip+.Lrs_reason]
 call li_get
 test rax,rax
 jnz .Lrt_failure_string
 /* A completed response cannot authorize a still-open function call. */
 mov ecx,[rbx+PG_MM_CALL_COUNT]
 test ecx,ecx
 jz .Lrt_failure_end
 mov eax,1
 shl eax,cl
 dec eax
 cmp eax,[rbx+PG_MM_RESPONSE_CLOSED]
 je .Lrt_failure_end
 lea rax,[rip+.Lrt_unfinished]
 mov [r12],rax
 mov [r13],rax
 mov rdi,rax
 call strlen@PLT
 mov [r12+8],rax
 mov [r13+8],rax
 jmp .Lrt_failure_end
.Lrt_failure_string:
 mov r14,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lrt_failure_end
 mov rdi,r14
 call json_object_get_string@PLT
 mov [r12],rax
 mov rdi,r14
 call json_object_get_string_len@PLT
 mov [r12+8],rax
 jmp .Lrt_failure_end
.Lrt_failure_absent:
 mov ebp,1
.Lrt_failure_end:
 mov eax,ebp
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.Lrt_failure_invalid:
 mov eax,1
 ret
.size pg_mimo_failure_data,.-pg_mimo_failure_data
.section .rodata
.Lrt_error: .asciz "error"
.Lrt_message: .asciz "message"
.Lrt_unfinished: .asciz "OpenAI Responses stream completed with an unfinished tool call"

.text
.Lrt_stop_data:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,[rdi+PG_MM_RESPONSE_TERMINAL]
 mov r12,rsi
 mov qword ptr [rsp],0
 xor ebp,ebp
 test rbx,rbx
 jz .Lrt_stop_end
 mov rdi,r12
 mov rsi,rbx
 lea rdx,[rip+.Lrt_status]
 lea rcx,[rip+.Lrt_raw_stop]
 call .Lrs_copy
 test eax,eax
 jnz .Lrt_stop_bad
 mov rdi,rbx
 lea rsi,[rip+.Lrt_status]
 call li_get
 mov rdi,rax
 call json_object_get_string@PLT
 mov r13,rax
 test rax,rax
 jz .Lrt_stop_end
 mov rdi,rax
 lea rsi,[rip+.Lrt_incomplete]
 call strcmp@PLT
 test eax,eax
 jnz .Lrt_stop_end
 mov rdi,rbx
 lea rsi,[rip+.Lrs_incomplete_details]
 call li_get
 mov rdi,rax
 lea rsi,[rip+.Lrs_reason]
 call li_get
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lrt_stop_end
 mov rcx,rax
 lea rdi,[rsp]
 lea rsi,[rip+.Lrt_reason_format]
 mov rdx,r13
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lrt_stop_bad
 mov rdi,[rsp]
 call json_object_new_string@PLT
 mov r14,rax
 mov rdi,[rsp]
 call free@PLT
 mov qword ptr [rsp],0
 mov rdi,r12
 lea rsi,[rip+.Lrt_raw_stop]
 mov rdx,r14
 call .Lrs_set
 test eax,eax
 jnz .Lrt_stop_bad
 jmp .Lrt_stop_end
.Lrt_stop_bad:
 mov ebp,1
.Lrt_stop_end:
 mov rdi,[rsp]
 call free@PLT
 mov eax,ebp
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lrt_status: .asciz "status"
.Lrt_raw_stop: .asciz "rawStopReason"
.Lrt_incomplete: .asciz "incomplete"
.Lrt_reason_format: .asciz "%s.%s"

.text
/* Strict finished-object parse. rdx: 0 value, 1 syntax/type, 2 allocation. */
.Lpa_object:
 push rbx
 push r12
 push r13
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 call json_tokener_new@PLT
 test rax,rax
 jz .Lpa_object_alloc
 mov [rsp],rax
 mov rdi,rax
 mov esi,17
 call json_tokener_set_flags@PLT
 mov rdi,[rsp]
 mov rsi,rbx
 mov edx,r12d
 call json_tokener_parse_ex@PLT
 mov [rsp+8],rax
 mov rdi,[rsp]
 call json_tokener_get_error@PLT
 test eax,eax
 jnz .Lpa_object_bad
 mov rdi,[rsp]
 call json_tokener_get_parse_end@PLT
 cmp rax,r12
 jne .Lpa_object_bad
 mov rdi,[rsp+8]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lpa_object_bad
 mov r13,[rsp+8]
 mov rdi,[rsp]
 call json_tokener_free@PLT
 mov rax,r13
 xor edx,edx
 jmp .Lpa_object_end
.Lpa_object_bad:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp]
 call json_tokener_free@PLT
 xor eax,eax
 mov edx,1
 jmp .Lpa_object_end
.Lpa_object_alloc:
 xor eax,eax
 mov edx,2
.Lpa_object_end:
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret

/* Pi repairJson/parseToolCallArguments: repair string literals only; a finished
   malformed object becomes an empty argument object with the original error. */
.globl pg_mimo_arguments_parse
.type pg_mimo_arguments_parse,@function
pg_mimo_arguments_parse:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,56
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsp
 xor eax,eax
 mov ecx,7
 rep stosq
 mov [rsp],rdx
 test rdx,rdx
 jz .Lpa_bad
 mov qword ptr [rdx],0
 cmp r12,PG_TL_ARGUMENT_LIMIT
 jae .Lpa_bad
 test r12,r12
 jz .Lpa_empty
 test rbx,rbx
 jz .Lpa_bad
 mov rdi,rbx
 mov rsi,r12
 call pg_context_blank@PLT
 test eax,eax
 jnz .Lpa_empty
 mov rdi,rbx
 mov rsi,r12
 call .Lpa_object
 mov [rsp+16],rax
 test edx,edx
 jz .Lpa_envelope
 cmp edx,2
 je .Lpa_bad
 lea rdi,[r12+r12*2]
 add rdi,rdi
 inc rdi
 call malloc@PLT
 mov [rsp+24],rax
 mov r13,rax
 test rax,rax
 jz .Lpa_bad
 xor r14d,r14d
 xor r15d,r15d
 xor ebp,ebp
.Lpa_repair:
 cmp r14,r12
 jae .Lpa_repaired
 movzx eax,byte ptr [rbx+r14]
 test ebp,ebp
 jz .Lpa_copy
 cmp eax,92
 je .Lpa_escape
 cmp eax,32
 jb .Lpa_control
.Lpa_copy:
 mov [r13+r15],al
 inc r15
 cmp eax,34
 jne .Lpa_advance
 xor ebp,1
.Lpa_advance:
 inc r14
 jmp .Lpa_repair
.Lpa_escape:
 lea rcx,[r14+1]
 cmp rcx,r12
 jae .Lpa_bad_escape
 movzx eax,byte ptr [rbx+rcx]
 cmp eax,'u'
 je .Lpa_unicode
 cmp eax,34
 je .Lpa_valid_escape
 cmp eax,92
 je .Lpa_valid_escape
 cmp eax,'/'
 je .Lpa_valid_escape
 cmp eax,'b'
 je .Lpa_valid_escape
 cmp eax,'f'
 je .Lpa_valid_escape
 cmp eax,'n'
 je .Lpa_valid_escape
 cmp eax,'r'
 je .Lpa_valid_escape
 cmp eax,'t'
 je .Lpa_valid_escape
 jmp .Lpa_bad_escape
.Lpa_valid_escape:
 mov byte ptr [r13+r15],92
 mov [r13+r15+1],al
 add r15,2
 add r14,2
 jmp .Lpa_repair
.Lpa_unicode:
 lea rcx,[r14+5]
 cmp rcx,r12
 jae .Lpa_bad_escape
 mov ecx,2
.Lpa_hex_check:
 lea r8,[r14+rcx]
 movzx eax,byte ptr [rbx+r8]
 cmp eax,'0'
 jb .Lpa_bad_escape
 cmp eax,'9'
 jbe .Lpa_hex_next
 or eax,32
 cmp eax,'a'
 jb .Lpa_bad_escape
 cmp eax,'f'
 ja .Lpa_bad_escape
.Lpa_hex_next:
 inc ecx
 cmp ecx,6
 jb .Lpa_hex_check
 mov eax,[rbx+r14]
 mov [r13+r15],eax
 mov ax,[rbx+r14+4]
 mov [r13+r15+4],ax
 add r15,6
 add r14,6
 jmp .Lpa_repair
.Lpa_bad_escape:
 mov word ptr [r13+r15],0x5c5c
 add r15,2
 jmp .Lpa_advance
.Lpa_control:
 mov dword ptr [r13+r15],0x3030755c
 lea r8,[rip+.Lpa_hex]
 mov ecx,eax
 shr ecx,4
 mov dl,[r8+rcx]
 mov [r13+r15+4],dl
 and eax,15
 mov dl,[r8+rax]
 mov [r13+r15+5],dl
 add r15,6
 jmp .Lpa_advance
.Lpa_repaired:
 mov byte ptr [r13+r15],0
 mov rdi,r13
 mov rsi,r15
 call .Lpa_object
 mov [rsp+16],rax
 cmp edx,2
 je .Lpa_bad
 test edx,edx
 jz .Lpa_envelope
 mov dword ptr [rsp+48],1
 jmp .Lpa_envelope
.Lpa_empty:
 mov qword ptr [rsp+16],0
.Lpa_envelope:
 mov rdi,[rsp+24]
 call free@PLT
 mov qword ptr [rsp+24],0
 lea rdi,[rip+.Lpa_json]
 call json_tokener_parse@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lpa_bad
 cmp qword ptr [rsp+16],0
 je .Lpa_error_field
 mov rdi,rax
 lea rsi,[rip+.Lrs_arguments]
 mov rdx,[rsp+16]
 mov qword ptr [rsp+16],0
 call .Lrs_set
 test eax,eax
 jnz .Lpa_bad
.Lpa_error_field:
 cmp dword ptr [rsp+48],0
 je .Lpa_success
 lea rdi,[r12+PA_ERROR_BYTES+1]
 call malloc@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lpa_bad
 mov rdi,rax
 lea rsi,[rip+.Lpa_error]
 mov edx,OFFSET PA_ERROR_BYTES
 call memcpy@PLT
 mov rdi,[rsp+24]
 add rdi,OFFSET PA_ERROR_BYTES
 mov rsi,rbx
 mov rdx,r12
 call memcpy@PLT
 mov rdi,[rsp+24]
 lea rsi,[r12+PA_ERROR_BYTES]
 call json_object_new_string_len@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lpa_argumentsError]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lpa_bad
.Lpa_success:
 mov rax,[rsp+8]
 mov rdx,[rsp]
 mov [rdx],rax
 mov qword ptr [rsp+8],0
 xor ebp,ebp
 jmp .Lpa_cleanup
.Lpa_bad:
 mov ebp,1
.Lpa_cleanup:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call free@PLT
 mov eax,ebp
 add rsp,56
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_mimo_arguments_parse,.-pg_mimo_arguments_parse
.section .rodata
.Lpa_hex: .ascii "0123456789abcdef"
.Lpa_json: .asciz "{\"arguments\":{}}"
.Lpa_argumentsError: .asciz "argumentsError"
.Lpa_error: .ascii "Invalid JSON tool arguments\n\nReceived arguments:\n"
.Lpa_error_end:

.section .rodata
.Lrc_argumentsError: .asciz "argumentsError"

.text
.globl pg_mimo_prepare_arguments
.type pg_mimo_prepare_arguments,@function
pg_mimo_prepare_arguments:
 push rbx
 push r12
 push r13
 sub rsp,16
 mov rbx,rdi
 mov r12d,esi
 mov qword ptr [rsp],0
 cmp esi,PG_MM_CALL_LIMIT
 jae .Lpr_bad
 mov rax,r12
 PG_MM_CALL_SLOT r13,rax
 mov rdi,[rbx+PG_MM_ARGUMENT_OVERFLOW+r12*8]
 test rdi,rdi
 cmovz rdi,r13
 mov rsi,[rbx+PG_MM_ARGUMENT_LENGTH+r12*8]
 mov rdx,rsp
 call pg_mimo_arguments_parse
 test eax,eax
 jnz .Lpr_bad
 mov rdi,[rbx+PG_MM_PREPARED_ARGUMENTS+r12*8]
 call json_object_put@PLT
 mov rax,[rsp]
 mov [rbx+PG_MM_PREPARED_ARGUMENTS+r12*8],rax
 mov qword ptr [rsp],0
 mov rdi,rax
 lea rsi,[rip+.Lrs_arguments]
 call li_get
 mov rdi,rax
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Lpr_bad
 imul r12,r12,PG_MM_CALL_BYTES
 lea r13,[rbx+PG_MM_CALLS+r12]
 mov [r13],rax
 mov rdi,rax
 call strlen@PLT
 cmp rax,PG_TL_ARGUMENT_LIMIT
 jae .Lpr_bad
 mov [r13+8],rax
 xor eax,eax
 jmp .Lpr_end
.Lpr_bad:
 mov eax,1
.Lpr_end:
 mov [rsp+8],eax
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,[rsp+8]
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret
.size pg_mimo_prepare_arguments,.-pg_mimo_prepare_arguments

.set PA_ERROR_BYTES, .Lpa_error_end-.Lpa_error

/* Pi anthropic-messages.ts: ordered open blocks, final argument parsing and
   message_stop barrier. Each block owns two printbufs and a canonical item. */
.macro ANGET obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lan_\key]
 call li_get
.endm
.macro ANCOPY dst,src,from,to
 mov rdi,\dst
 mov rsi,\src
 lea rdx,[rip+.Lan_\from]
 lea rcx,[rip+.Lan_\to]
 call .Lrs_copy
 test eax,eax
 jnz .Lan_bad
.endm
.text
.globl pg_mimo_anthropic_clear
.type pg_mimo_anthropic_clear,@function
pg_mimo_anthropic_clear:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 xor r12d,r12d
.Lan_clear_each:
 cmp r12,[rbx+PG_MM_ANTHROPIC_COUNT]
 jae .Lan_clear_done
 imul r13,r12,40
 add r13,[rbx+PG_MM_ANTHROPIC_BLOCKS]
 mov rdi,[r13+16]
 call printbuf_free@PLT
 mov rdi,[r13+24]
 call printbuf_free@PLT
 mov rdi,[r13+32]
 call json_object_put@PLT
 inc r12
 jmp .Lan_clear_each
.Lan_clear_done:
 mov rdi,[rbx+PG_MM_ANTHROPIC_BLOCKS]
 call free@PLT
 mov rdi,[rbx+PG_MM_ANTHROPIC_USAGE]
 call json_object_put@PLT
 mov qword ptr [rbx+PG_MM_ANTHROPIC_BLOCKS],0
 mov qword ptr [rbx+PG_MM_ANTHROPIC_COUNT],0
 mov qword ptr [rbx+PG_MM_ANTHROPIC_CAPACITY],0
 mov qword ptr [rbx+PG_MM_ANTHROPIC_USAGE],0
 pop r13
 pop r12
 pop rbx
 ret
.size pg_mimo_anthropic_clear,.-pg_mimo_anthropic_clear
/* Append a JSON string with explicit length; printbuf grows with the stream. */
.Lan_append:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lan_append_bad
 mov rdi,r12
 call json_object_get_string_len@PLT
 mov [rsp],eax
 mov rdi,r12
 call json_object_get_string@PLT
 mov rdi,rbx
 mov rsi,rax
 mov edx,[rsp]
 call printbuf_memappend@PLT
 test eax,eax
 setl al
 movzx eax,al
 jmp .Lan_append_end
.Lan_append_bad:
 mov eax,1
.Lan_append_end:
 add rsp,8
 pop r12
 pop rbx
 ret
/* event frame: normalized Chat root/choice/delta, incoming block, slot and
   temporary owner. Only normalized text/tool deltas enter the common decoder. */
.Lan_event:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,96
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rsp
 xor eax,eax
 mov ecx,12
 rep stosq
 ANGET r12,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+80],rax
 test rax,rax
 jz .Lan_bad
 lea r14,[rip+.Lan_events]
 xor ebp,ebp
.Lan_find_event:
 mov rsi,[r14+rbp*8]
 test rsi,rsi
 jz .Lan_ok
 mov rdi,[rsp+80]
 call strcmp@PLT
 test eax,eax
 jz .Lan_dispatch
 inc ebp
 jmp .Lan_find_event
.Lan_dispatch:
 cmp ebp,6
 je .Lan_error
 cmp ebp,5
 je .Lan_message_stop
 cmp ebp,4
 je .Lan_dispatch_delta
 cmp dword ptr [rbx+PG_MM_FINISH_REASON],0
 jne .Lan_bad
 .Lan_dispatch_delta:
 lea rdi,[rip+.Lrs_base]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lan_bad
 ANGET rax,choices
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov [rsp+8],rax
 ANGET rax,delta
 mov [rsp+16],rax
 cmp ebp,0
 je .Lan_message_start
 cmp ebp,4
 je .Lan_message_delta
 ANGET r12,index
 mov [rsp+88],rax
 mov rdi,rax
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lan_bad
 mov rdi,[rsp+88]
 call json_object_get_int64@PLT
 test rax,rax
 js .Lan_bad
 mov [rsp+24],rax
 xor r14d,r14d
.Lan_find_block:
 cmp r14,[rbx+PG_MM_ANTHROPIC_COUNT]
 jae .Lan_missing_block
 imul rcx,r14,40
 add rcx,[rbx+PG_MM_ANTHROPIC_BLOCKS]
 cmp [rcx],rax
 je .Lan_found_block
 inc r14
 jmp .Lan_find_block
.Lan_missing_block:
 cmp ebp,1
 jne .Lan_bad
 mov rax,[rbx+PG_MM_ANTHROPIC_COUNT]
 cmp rax,[rbx+PG_MM_ANTHROPIC_CAPACITY]
 jb .Lan_allocate_block
 lea rsi,[rax+8]
 mov [rsp+72],rsi
 imul rsi,rsi,40
 mov rdi,[rbx+PG_MM_ANTHROPIC_BLOCKS]
 call realloc@PLT
 test rax,rax
 jz .Lan_bad
 mov [rbx+PG_MM_ANTHROPIC_BLOCKS],rax
 mov rax,[rsp+72]
 mov [rbx+PG_MM_ANTHROPIC_CAPACITY],rax
.Lan_allocate_block:
 imul r14,[rbx+PG_MM_ANTHROPIC_COUNT],40
 add r14,[rbx+PG_MM_ANTHROPIC_BLOCKS]
 mov rdi,r14
 xor esi,esi
 mov edx,40
 call memset@PLT
 inc qword ptr [rbx+PG_MM_ANTHROPIC_COUNT]
 mov rax,[rsp+24]
 mov [r14],rax
 call printbuf_new@PLT
 mov [r14+16],rax
 test rax,rax
 jz .Lan_bad
 call printbuf_new@PLT
 mov [r14+24],rax
 test rax,rax
 jz .Lan_bad
 ANGET r12,content_block
 mov [rsp+32],rax
 ANGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+80],rax
 test rax,rax
 jz .Lan_bad
 mov rdi,rax
 lea rsi,[rip+.Lan_text]
 call strcmp@PLT
 test eax,eax
 jz .Lan_start_text
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lan_thinking]
 call strcmp@PLT
 test eax,eax
 jz .Lan_start_thinking
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lan_redacted_thinking]
 call strcmp@PLT
 test eax,eax
 jz .Lan_start_redacted
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lan_tool_use]
 call strcmp@PLT
 test eax,eax
 jnz .Lan_bad
 mov dword ptr [r14+8],3
 mov eax,[rbx+PG_MM_CALL_COUNT]
 cmp eax,PG_MM_CALL_LIMIT
 jae .Lan_bad
 mov rcx,[rsp+24]
 mov [rbx+PG_MM_RESPONSE_INDEX+rax*8],rcx
 mov [rsp+48],rax
 lea rdi,[rip+.Lrc_tool_json]
 call json_tokener_parse@PLT
 mov [r14+32],rax
 test rax,rax
 jz .Lan_bad
 ANCOPY rax,"qword ptr [rsp+32]",id,id
 ANCOPY "qword ptr [r14+32]","qword ptr [rsp+32]",name,name
 jmp .Lan_tool_delta
.Lan_start_text:
 mov dword ptr [r14+8],1
 lea rdi,[rip+.Lrc_text_json]
 call json_tokener_parse@PLT
 mov [r14+32],rax
 test rax,rax
 jz .Lan_bad
 ANGET "qword ptr [rsp+32]",text
 mov [rsp+56],rax
 jmp .Lan_text_delta
.Lan_start_thinking:
 mov dword ptr [r14+8],2
 lea rdi,[rip+.Lrc_think_json]
 call json_tokener_parse@PLT
 mov [r14+32],rax
 test rax,rax
 jz .Lan_bad
 ANGET "qword ptr [rsp+32]",signature
 test rax,rax
 jz .Lan_initial_thinking
 mov rdi,[r14+24]
 mov rsi,rax
 call .Lan_append
 test eax,eax
 jnz .Lan_bad
.Lan_initial_thinking:
 ANGET "qword ptr [rsp+32]",thinking
 test rax,rax
 jz .Lan_ok
 mov rdi,[r14+16]
 mov rsi,rax
 call .Lan_append
 test eax,eax
 jnz .Lan_bad
 jmp .Lan_ok
.Lan_start_redacted:
 mov dword ptr [r14+8],4
 lea rdi,[rip+.Lan_redacted_json]
 call json_tokener_parse@PLT
 mov [r14+32],rax
 test rax,rax
 jz .Lan_bad
 ANGET "qword ptr [rsp+32]",data
 mov rdi,[r14+24]
 mov rsi,rax
 call .Lan_append
 test eax,eax
 jnz .Lan_bad
 mov rdi,[r14+16]
 lea rsi,[rip+.Lan_redacted]
 mov edx,20
 call printbuf_memappend@PLT
 test eax,eax
 js .Lan_bad
 jmp .Lan_ok
.Lan_found_block:
 mov r14,rcx
 cmp ebp,1
 je .Lan_bad
 cmp dword ptr [r14+12],0
 jne .Lan_bad
 cmp ebp,3
 je .Lan_block_stop
 ANGET r12,delta
 mov [rsp+32],rax
 ANGET rax,type
 mov rdi,rax
 call json_object_get_string@PLT
 mov [rsp+80],rax
 test rax,rax
 jz .Lan_bad
 mov rdi,rax
 lea rsi,[rip+.Lan_text_delta_type]
 call strcmp@PLT
 test eax,eax
 jnz .Lan_other_delta
 cmp dword ptr [r14+8],1
 jne .Lan_bad
 ANGET "qword ptr [rsp+32]",text
 mov [rsp+56],rax
.Lan_text_delta:
 cmp qword ptr [rsp+56],0
 je .Lan_ok
 mov rdi,[r14+16]
 mov rsi,[rsp+56]
 call .Lan_append
 test eax,eax
 jnz .Lan_bad
 mov rdi,rbx
 call pg_mimo_uses_google
 test eax,eax
 jnz .Lan_ok
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jnz .Lan_ok
 mov rdi,[rsp+56]
 call json_object_get@PLT
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lstr_content]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lan_bad
 jmp .Lan_emit
.Lan_other_delta:
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lan_input_json_delta]
 call strcmp@PLT
 test eax,eax
 jz .Lan_tool_delta_find
 cmp dword ptr [r14+8],2
 je .Lbb_reasoning_delta
 cmp dword ptr [r14+8],4
 jne .Lan_bad
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lan_bad
.Lbb_reasoning_delta:
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lan_thinking_delta]
 call strcmp@PLT
 test eax,eax
 jnz .Lan_signature_delta
 ANGET "qword ptr [rsp+32]",thinking
 mov rdi,[r14+16]
 jmp .Lan_append_delta
.Lan_signature_delta:
 cmp dword ptr [r14+8],4
 je .Lan_ok
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lan_signature_delta_type]
 call strcmp@PLT
 test eax,eax
 jnz .Lan_ok
 ANGET "qword ptr [rsp+32]",signature
 mov rdi,[r14+24]
.Lan_append_delta:
 mov rsi,rax
 call .Lan_append
 test eax,eax
 jnz .Lan_bad
 jmp .Lan_ok
.Lan_tool_delta_find:
 cmp dword ptr [r14+8],3
 jne .Lan_bad
 xor ecx,ecx
 mov rax,[r14]
.Lan_tool_slot:
 cmp ecx,[rbx+PG_MM_CALL_COUNT]
 jae .Lan_bad
 cmp [rbx+PG_MM_RESPONSE_INDEX+rcx*8],rax
 je .Lan_tool_slot_found
 inc ecx
 jmp .Lan_tool_slot
.Lan_tool_slot_found:
 mov [rsp+48],rcx
 ANGET "qword ptr [rsp+32]",partial_json
 mov [rsp+56],rax
 mov rdi,[r14+16]
 mov rsi,rax
 call .Lan_append
 test eax,eax
 jnz .Lan_bad
.Lan_tool_delta:
 lea rdi,[rip+.Lrs_calls_json]
 call json_tokener_parse@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lan_bad
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov [rsp+64],rax
 mov rdi,[rsp+48]
 call json_object_new_int64@PLT
 mov rdi,[rsp+64]
 lea rsi,[rip+.Lan_index]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lan_bad
 cmp ebp,1
 jne .Lan_tool_arguments_delta
 ANCOPY "qword ptr [rsp+64]","qword ptr [rsp+32]",id,id
 ANGET "qword ptr [rsp+64]",function
 ANCOPY rax,"qword ptr [rsp+32]",name,name
 jmp .Lan_tool_attach
.Lan_tool_arguments_delta:
 ANGET "qword ptr [rsp+64]",function
 mov [rsp+64],rax
 ANCOPY rax,"qword ptr [rsp+32]",partial_json,arguments
.Lan_tool_attach:
 mov rdi,[rsp+16]
 lea rsi,[rip+.Lan_tool_calls]
 mov rdx,[rsp+40]
 mov qword ptr [rsp+40],0
 call .Lrs_set
 test eax,eax
 jnz .Lan_bad
 jmp .Lan_emit
.Lan_block_stop:
 cmp dword ptr [r14+8],3
 je .Lan_finish_tool
 mov rax,[r14+16]
 mov rdi,[rax]
 mov esi,[rax+8]
 call json_object_new_string_len@PLT
 lea rsi,[rip+.Lan_text]
 cmp dword ptr [r14+8],1
 je .Lan_finish_text_key
 lea rsi,[rip+.Lan_thinking]
.Lan_finish_text_key:
 mov rdi,[r14+32]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lan_bad
 cmp dword ptr [r14+8],1
 jne .Lan_block_signature
 mov rax,[r14+24]
 cmp dword ptr [rax+8],0
 je .Lan_block_closed
.Lan_block_signature:
 cmp dword ptr [r14+8],4
 jne .Lbb_signature_text
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lbb_signature_text
 mov rax,[r14+24]
 mov rdi,[rax]
 mov esi,[rax+8]
 call pg_mimo_base64_encode
 jmp .Lbb_signature_ready
.Lbb_signature_text:
 mov rax,[r14+24]
 mov rdi,[rax]
 mov esi,[rax+8]
 call json_object_new_string_len@PLT
.Lbb_signature_ready:
 mov rdi,[r14+32]
 lea rsi,[rip+.Lan_thinkingSignature]
 cmp dword ptr [r14+8],1
 jne .Lgo_signature_key_ready
 lea rsi,[rip+.Lgo_textSignature]
.Lgo_signature_key_ready:
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lan_bad
 jmp .Lan_block_closed
.Lan_finish_tool:
 mov rax,[r14+16]
 mov rdi,[rax]
 mov esi,[rax+8]
 lea rdx,[rsp+40]
 call pg_mimo_arguments_parse
 test eax,eax
 jnz .Lan_bad
 ANCOPY "qword ptr [r14+32]","qword ptr [rsp+40]",arguments,arguments
 ANCOPY "qword ptr [r14+32]","qword ptr [rsp+40]",argumentsError,argumentsError
.Lan_block_closed:
 mov dword ptr [r14+12],1
 jmp .Lan_ok
.Lan_message_start:
 cmp qword ptr [rbx+PG_MM_RESPONSE_ID],0
 jne .Lan_bad
 ANGET r12,message
 mov [rsp+32],rax
 ANGET rax,id
 test rax,rax
 jz .Lan_bad
 mov rdi,rax
 call json_object_get@PLT
 mov [rbx+PG_MM_RESPONSE_ID],rax
 ANGET "qword ptr [rsp+32]",usage
 jmp .Lan_update_usage
.Lan_message_delta:
 cmp dword ptr [rbx+PG_MM_FINISH_REASON],0
 jne .Lan_delta_usage
 ANGET r12,delta
 mov [rsp+32],rax
 ANGET rax,stop_reason
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lan_delta_usage
 mov [rsp+80],rax
 mov rdi,rbx
 mov rsi,[rsp+32]
 call .Lrt_save
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lbt_reason_other
 mov rdi,[rsp+80]
 lea rsi,[rip+.LbtK_context_limit]
 call strcmp@PLT
 test eax,eax
 jz .Lan_length
.Lbt_reason_other:
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lan_max_tokens]
 call strcmp@PLT
 test eax,eax
 jz .Lan_length
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lan_tool_use]
 call strcmp@PLT
 test eax,eax
 jz .Lan_finish_calls
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lan_end_turn]
 call strcmp@PLT
 test eax,eax
 jz .Lan_finish_stop
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lan_stop_sequence]
 call strcmp@PLT
 test eax,eax
 jz .Lan_finish_stop
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lan_pause_turn]
 call strcmp@PLT
 test eax,eax
 jz .Lan_finish_stop
 mov rdi,rbx
 mov rsi,r12
 mov rdx,[rsp+32]
 call .Lan_stop_failure
 jmp .Lan_bad
.Lan_finish_stop:
 lea rdi,[rip+.Lstr_stop]
 jmp .Lan_finish_value
.Lan_finish_calls:
 lea rdi,[rip+.Lstr_reason_calls]
 jmp .Lan_finish_value
.Lan_length:
 /* A truncated tool must remain non-executable. */
 mov dword ptr [rbx+PG_MM_FINISH_REASON],3
 jmp .Lan_delta_usage
.Lan_finish_value:
 call json_object_new_string@PLT
 mov rdi,[rsp+8]
 lea rsi,[rip+.Lstr_finish_reason]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lan_bad
.Lan_delta_usage:
 ANGET r12,usage
.Lan_update_usage:
 mov rdi,rbx
 mov rsi,rax
 call .Lan_update_usage_object
 test eax,eax
 jnz .Lan_bad
 cmp dword ptr [rbx+PG_MM_FINISH_REASON],0
 jne .Lan_ok
 jmp .Lan_emit
.Lan_message_stop:
 cmp dword ptr [rbx+PG_MM_FINISH_REASON],0
 je .Lan_bad
 xor r14d,r14d
.Lan_all_closed:
 cmp r14,[rbx+PG_MM_ANTHROPIC_COUNT]
 jae .Lan_stopped
 imul rax,r14,40
 add rax,[rbx+PG_MM_ANTHROPIC_BLOCKS]
 cmp dword ptr [rax+12],0
 je .Lan_bad
 inc r14
 jmp .Lan_all_closed
.Lan_stopped:
 mov dword ptr [rbx+PG_MM_SAW_DONE],1
 jmp .Lan_ok
.Lan_error:
 mov rdi,rbx
 mov rsi,r12
 call .Lrt_save
 jmp .Lan_bad
.Lan_emit:
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r13
 call .Lchat_object
 test eax,eax
 jnz .Lan_bad
.Lan_ok:
 xor ebp,ebp
 jmp .Lan_cleanup
.Lan_bad:
 mov ebp,1
.Lan_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,96
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Merge only reported counters, then expose the common usage representation.
   Anthropic input_tokens excludes cache reads and writes. */
.Lan_update_usage_object:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp+48],0
 mov qword ptr [rsp+56],0
 cmp qword ptr [rbx+PG_MM_ANTHROPIC_USAGE],0
 jne .Lan_usage_merge
 call json_object_new_object@PLT
 mov [rbx+PG_MM_ANTHROPIC_USAGE],rax
 test rax,rax
 jz .Lan_usage_bad
.Lan_usage_merge:
 xor r13d,r13d
 lea r14,[rip+.Lan_usage_keys]
.Lan_usage_each:
 cmp r13,4
 jae .Lan_usage_build
 mov rdi,r12
 mov rsi,[r14+r13*8]
 call li_get
 test rax,rax
 jz .Lan_usage_saved
 mov [rsp+48],rax
 mov rdi,rax
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lan_usage_bad
 mov rdi,[rsp+48]
 call json_object_get_int64@PLT
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lan_usage_bad
 mov rdi,[rsp+48]
 call json_object_get@PLT
 mov rdi,[rbx+PG_MM_ANTHROPIC_USAGE]
 mov rsi,[r14+r13*8]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lan_usage_bad
.Lan_usage_saved:
 mov rdi,[rbx+PG_MM_ANTHROPIC_USAGE]
 mov rsi,[r14+r13*8]
 call li_get
 mov rdi,rax
 call json_object_get_int64@PLT
 mov [rsp+r13*8],rax
 inc r13
 jmp .Lan_usage_each
.Lan_usage_build:
 ANGET r12,cache_creation
 ANGET rax,ephemeral_1h_input_tokens
 mov rdi,rbx
 mov rsi,rax
 lea rdx,[rip+.Lan_cacheWrite1h]
 call .Lan_optional_counter
 test eax,eax
 jnz .Lan_usage_bad
 ANGET r12,output_tokens_details
 ANGET rax,thinking_tokens
 mov rdi,rbx
 mov rsi,rax
 lea rdx,[rip+.Lan_reasoning]
 call .Lan_optional_counter
 test eax,eax
 jnz .Lan_usage_bad
 lea rdi,[rip+.Lan_usage_json]
 call json_tokener_parse@PLT
 mov [rsp+56],rax
 test rax,rax
 jz .Lan_usage_bad
 mov rdi,[rsp]
 add rdi,[rsp+16]
 jc .Lan_usage_bad
 add rdi,[rsp+24]
 jc .Lan_usage_bad
 mov rax,9007199254740991
 cmp rdi,rax
 ja .Lan_usage_bad
 call json_object_new_int64@PLT
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lrs_prompt_tokens]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lan_usage_bad
 mov rdi,[rsp+8]
 call json_object_new_int64@PLT
 mov rdi,[rsp+56]
 lea rsi,[rip+.Lrs_completion_tokens]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lan_usage_bad
 RSGET "qword ptr [rsp+56]",prompt_tokens_details
 mov rbp,rax
 mov rdi,[rsp+16]
 call json_object_new_int64@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lan_cached_tokens]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lan_usage_bad
 mov rdi,[rsp+24]
 call json_object_new_int64@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lan_cache_write_tokens]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lan_usage_bad
 mov rdi,[rbx+PG_MM_USAGE]
 call json_object_put@PLT
 mov rax,[rsp+56]
 mov [rbx+PG_MM_USAGE],rax
 mov qword ptr [rsp+56],0
 xor ebp,ebp
 jmp .Lan_usage_end
.Lan_usage_bad:
 mov ebp,1
.Lan_usage_end:
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,64
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Final canonical content is detached from the decoder before persistence. */
.Lan_complete:
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
 call json_object_new_array@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lan_complete_bad
 xor r13d,r13d
.Lan_complete_each:
 cmp r13,[rbx+PG_MM_ANTHROPIC_COUNT]
 jae .Lan_complete_adopt
 imul r14,r13,40
 add r14,[rbx+PG_MM_ANTHROPIC_BLOCKS]
 cmp dword ptr [r14+12],0
 je .Lan_complete_bad
 mov rdi,[r14+32]
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lan_complete_bad
 mov rdi,[rsp]
 mov rsi,[rsp+8]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lan_complete_bad
 mov qword ptr [rsp+8],0
 inc r13
 jmp .Lan_complete_each
.Lan_complete_adopt:
 mov rdi,r12
 lea rsi,[rip+.Lrc_content]
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 call .Lrs_set
 test eax,eax
 jnz .Lan_complete_bad
 mov r14,[rbx+PG_MM_CONFIGURATION]
 mov rdi,r12
 mov rsi,r14
 lea rdx,[rip+.Lrc_model]
 mov rcx,rdx
 call .Lrs_copy
 test eax,eax
 jnz .Lan_complete_bad
 mov rdi,r12
 mov rsi,r14
 lea rdx,[rip+.Lrc_modelProvider]
 lea rcx,[rip+.Lrc_provider]
 call .Lrs_copy
 test eax,eax
 jnz .Lan_complete_bad
 ANGET r12,provider
 test rax,rax
 jnz .Lan_complete_api
 RCGET r14,modelInfo
 mov rsi,rax
 mov rdi,r12
 lea rdx,[rip+.Lrc_provider]
 mov rcx,rdx
 call .Lrs_copy
 test eax,eax
 jnz .Lan_complete_bad
.Lan_complete_api:
 mov rdi,[rbx+PG_MM_CONFIGURATION]
 call pg_mimo_config_bedrock@PLT
 test eax,eax
 jz .Lbt_complete_other
 lea rdi,[rip+.LbtK_api]
 jmp .Lgo_complete_api_ready
.Lbt_complete_other:
 mov rdi,rbx
 call pg_mimo_uses_google
 lea rdi,[rip+.Lan_api_value]
 test eax,eax
 jz .Lgo_complete_api_ready
 lea rdi,[rip+.Lgo_generative]
 cmp eax,2
 jne .Lgo_complete_api_ready
 lea rdi,[rip+.Lgo_vertex]
.Lgo_complete_api_ready:
 call json_object_new_string@PLT
 mov rdi,r12
 lea rsi,[rip+.Lrc_api]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lan_complete_bad
 mov rdi,r12
 mov rsi,[rbx+PG_MM_RESPONSE_TERMINAL]
 lea rdx,[rip+.Lan_stop_reason]
 lea rcx,[rip+.Lrt_raw_stop]
 call .Lrs_copy
 test eax,eax
 jnz .Lan_complete_bad
 mov rdi,[rbx+PG_MM_RESPONSE_ID]
 test rdi,rdi
 jz .Lan_complete_ok
 mov rdi,rbx
 call pg_mimo_uses_google
 test eax,eax
 jz .Lgo_complete_id
 mov rdi,[rbx+PG_MM_RESPONSE_ID]
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lan_complete_ok
.Lgo_complete_id:
 mov rdi,[rbx+PG_MM_RESPONSE_ID]
 call json_object_get@PLT
 mov rdi,r12
 lea rsi,[rip+.Lrc_responseId]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lan_complete_bad
.Lan_complete_ok:
 ANGET r12,usage
 mov r14,rax
 test rax,rax
 jz .Lan_complete_usage_done
 mov rdi,r14
 mov rsi,[rbx+PG_MM_ANTHROPIC_USAGE]
 lea rdx,[rip+.Lan_cacheWrite1h]
 mov rcx,rdx
 call .Lrs_copy
 test eax,eax
 jnz .Lan_complete_bad
 mov rdi,r14
 mov rsi,[rbx+PG_MM_ANTHROPIC_USAGE]
 lea rdx,[rip+.Lan_reasoning]
 mov rcx,rdx
 call .Lrs_copy
 test eax,eax
 jnz .Lan_complete_bad
.Lan_complete_usage_done:
 mov rdi,rbx
 call pg_mimo_uses_google
 test eax,eax
 jz .Lgo_complete_usage_done
 ANGET r12,usage
 mov r14,rax
 test rax,rax
 jz .Lgo_complete_usage_done
 mov rdi,[rbx+PG_MM_ANTHROPIC_USAGE]
 lea rsi,[rip+.Lgo_totalTokenCount]
 call li_get
 test rax,rax
 jz .Lgo_zero_total
 mov rdi,rax
 call json_object_get@PLT
 jmp .Lgo_set_total
.Lgo_zero_total:
 xor edi,edi
 call json_object_new_int64@PLT
.Lgo_set_total:
 mov rdi,r14
 lea rsi,[rip+.Lgo_totalTokens]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lan_complete_bad
.Lgo_complete_usage_done:
 mov rdi,[rbx+PG_MM_STRICT_DIAGNOSTICS]
 test rdi,rdi
 jz .Lan_complete_diagnostics_done
 lea rsi,[rsp+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lan_complete_bad
 mov rdi,r12
 lea rsi,[rip+.Lan_diagnostics]
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 call .Lrs_set
 test eax,eax
 jnz .Lan_complete_bad
.Lan_complete_diagnostics_done:
 xor ebp,ebp
 jmp .Lan_complete_end
.Lan_complete_bad:
 mov ebp,1
.Lan_complete_end:
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
.Lan_type: .asciz "type"
.Lan_index: .asciz "index"
.Lan_content_block: .asciz "content_block"
.Lan_delta: .asciz "delta"
.Lan_text: .asciz "text"
.Lan_thinking: .asciz "thinking"
.Lan_redacted_thinking: .asciz "redacted_thinking"
.Lan_tool_use: .asciz "tool_use"
.Lan_signature: .asciz "signature"
.Lan_data: .asciz "data"
.Lan_id: .asciz "id"
.Lan_name: .asciz "name"
.Lan_function: .asciz "function"
.Lan_arguments: .asciz "arguments"
.Lan_argumentsError: .asciz "argumentsError"
.Lan_partial_json: .asciz "partial_json"
.Lan_tool_calls: .asciz "tool_calls"
.Lan_text_delta_type: .asciz "text_delta"
.Lan_input_json_delta: .asciz "input_json_delta"
.Lan_thinking_delta: .asciz "thinking_delta"
.Lan_signature_delta_type: .asciz "signature_delta"
.Lan_thinkingSignature: .asciz "thinkingSignature"
.Lan_provider: .asciz "provider"
.Lan_message: .asciz "message"
.Lan_usage: .asciz "usage"
.Lan_stop_reason: .asciz "stop_reason"
.Lan_max_tokens: .asciz "max_tokens"
.Lan_end_turn: .asciz "end_turn"
.Lan_stop_sequence: .asciz "stop_sequence"
.Lan_pause_turn: .asciz "pause_turn"
.Lan_api_value: .asciz "anthropic-messages"
.Lan_redacted: .asciz "[Reasoning redacted]"
.Lan_redacted_json: .asciz "{\"type\":\"thinking\",\"redacted\":true}"
.Lan_usage_json: .asciz "{\"prompt_tokens_details\":{}}"
.Lan_cached_tokens: .asciz "cached_tokens"
.Lan_cache_write_tokens: .asciz "cache_write_tokens"
.Lan_input_tokens: .asciz "input_tokens"
.Lan_output_tokens: .asciz "output_tokens"
.Lan_cache_read_input_tokens: .asciz "cache_read_input_tokens"
.Lan_cache_creation_input_tokens: .asciz "cache_creation_input_tokens"
.Lan_start: .asciz "message_start"
.Lan_block_start: .asciz "content_block_start"
.Lan_block_delta: .asciz "content_block_delta"
.Lan_evt_block_stop: .asciz "content_block_stop"
.Lan_message_delta_type: .asciz "message_delta"
.Lan_message_stop_type: .asciz "message_stop"
.Lan_error_type: .asciz "error"
.section .data.rel.ro
.Lan_events: .quad .Lan_start,.Lan_block_start,.Lan_block_delta,.Lan_evt_block_stop,.Lan_message_delta_type,.Lan_message_stop_type,.Lan_error_type,0
.Lan_usage_keys: .quad .Lan_input_tokens,.Lan_output_tokens,.Lan_cache_read_input_tokens,.Lan_cache_creation_input_tokens
.section .rodata
.Lan_choices: .asciz "choices"

.text
/* Missing optional usage does not erase previously reported values. */
.Lan_optional_counter:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 test rsi,rsi
 jz .Lan_counter_ok
 mov rdi,rsi
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lan_counter_bad
 mov rdi,r12
 call json_object_get_int64@PLT
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lan_counter_bad
 mov rdi,r12
 call json_object_get@PLT
 mov rdi,[rbx+PG_MM_ANTHROPIC_USAGE]
 mov rsi,r13
 mov rdx,rax
 call .Lrs_set
 jmp .Lan_counter_end
.Lan_counter_ok:
 xor eax,eax
 jmp .Lan_counter_end
.Lan_counter_bad:
 mov eax,1
.Lan_counter_end:
 pop r13
 pop r12
 pop rbx
 ret
.section .rodata
.Lan_cache_creation: .asciz "cache_creation"
.Lan_ephemeral_1h_input_tokens: .asciz "ephemeral_1h_input_tokens"
.Lan_output_tokens_details: .asciz "output_tokens_details"
.Lan_thinking_tokens: .asciz "thinking_tokens"
.Lan_cacheWrite1h: .asciz "cacheWrite1h"
.Lan_reasoning: .asciz "reasoning"

/* Preserve stop details in the provider failure body, and expose the same
   user-facing explanation as Anthropic mapStopReason. */
.text
.Lan_stop_failure:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [rsp],0
 ANGET r13,stop_reason
 mov rdi,rax
 call json_object_get_string@PLT
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lan_refusal_reason]
 call strcmp@PLT
 test eax,eax
 jnz .Lan_stop_sensitive
 ANGET r13,stop_details
 ANGET rax,explanation
 mov rbp,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lan_stop_refusal_default
 mov rdi,rbp
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lan_stop_refusal_default
 mov rdi,rbp
 call json_object_get@PLT
 jmp .Lan_stop_message
.Lan_stop_refusal_default:
 lea rdi,[rip+.Lan_refusal_message]
 call json_object_new_string@PLT
 jmp .Lan_stop_message
.Lan_stop_sensitive:
 mov rdi,r14
 lea rsi,[rip+.Lan_sensitive_reason]
 call strcmp@PLT
 test eax,eax
 jnz .Lan_stop_unknown
 lea rdi,[rip+.Lan_sensitive_message]
 call json_object_new_string@PLT
 jmp .Lan_stop_message
.Lan_stop_unknown:
 mov rdi,r14
 call strlen@PLT
 lea rdi,[rax+32]
 call malloc@PLT
 mov rbp,rax
 test rax,rax
 jz .Lan_stop_failure_end
 mov rdi,rbx
 call pg_mimo_uses_google
 lea rsi,[rip+.Lan_unknown_format]
 test eax,eax
 jz .Lgo_stop_error_format
 lea rsi,[rip+.Lgo_error_format]
.Lgo_stop_error_format:
 mov rdi,rbp
 mov rdx,r14
 xor eax,eax
 call sprintf@PLT
 mov rdi,rbp
 call json_object_new_string@PLT
 mov [rsp+8],rax
 mov rdi,rbp
 call free@PLT
 mov rax,[rsp+8]
.Lan_stop_message:
 mov [rsp+8],rax
 test rax,rax
 jz .Lan_stop_failure_end
 lea rdi,[rip+.Lan_error_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lan_stop_release_message
 ANGET rax,error_key
 mov rdi,rax
 mov rsi,r12
 lea rdx,[rip+.Lan_delta]
 lea rcx,[rip+.Lan_providerDelta]
 call .Lrs_copy
 test eax,eax
 jnz .Lan_stop_release_message
 ANGET "qword ptr [rsp]",error_key
 mov rdi,rax
 lea rsi,[rip+.Lan_message]
 mov rdx,[rsp+8]
 mov qword ptr [rsp+8],0
 call .Lrs_set
 test eax,eax
 jnz .Lan_stop_release_message
 mov rdi,rbx
 mov rsi,[rsp]
 call .Lrt_save
.Lan_stop_release_message:
 mov rdi,[rsp+8]
 call json_object_put@PLT
.Lan_stop_failure_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 add rsp,32
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.section .rodata
.Lan_error_key: .asciz "error"
.Lan_providerDelta: .asciz "providerDelta"
.Lan_refusal_reason: .asciz "refusal"
.Lan_sensitive_reason: .asciz "sensitive"
.Lan_stop_details: .asciz "stop_details"
.Lan_explanation: .asciz "explanation"
.Lan_refusal_message: .asciz "The model refused to complete the request"
.Lan_sensitive_message: .asciz "Provider stopped with: sensitive"
.Lan_unknown_format: .asciz "Unhandled stop reason: %s"
.Lan_error_json: .asciz "{\"error\":{}}"

/* Chat finish reasons remain available in successful canonical messages and
   failed provider bodies; Durable can retain the original provider cause. */
.text
.Lchat_stop_data:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 xor r14d,r14d
 mov rdi,rbx
 call pg_mimo_uses_responses
 test eax,eax
 jnz .Lchat_stop_skip
 mov rdi,rbx
 call pg_mimo_uses_anthropic
 test eax,eax
 jnz .Lchat_stop_skip
 mov rdi,rbx
 call pg_mimo_uses_google
 test eax,eax
 jnz .Lchat_stop_skip
 lea rdi,[rip+.Lan_error_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lchat_stop_bad
 mov rdi,r12
 call json_object_get@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lstr_finish_reason]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lchat_stop_bad
 call printbuf_new@PLT
 mov r14,rax
 test rax,rax
 jz .Lchat_stop_bad
 mov rdi,rax
 lea rsi,[rip+.Lchat_finish_prefix]
 mov edx,24
 call printbuf_memappend@PLT
 test eax,eax
 js .Lchat_stop_bad
 mov rdi,r12
 call json_object_get_string_len@PLT
 mov [rsp+8],eax
 mov rdi,r12
 call json_object_get_string@PLT
 mov rdi,r14
 mov rsi,rax
 mov edx,[rsp+8]
 call printbuf_memappend@PLT
 test eax,eax
 js .Lchat_stop_bad
 ANGET "qword ptr [rsp]",error_key
 mov r13,rax
 mov rdi,r12
 call json_object_get@PLT
 mov rdi,r13
 lea rsi,[rip+.Lrt_raw_stop]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lchat_stop_bad
 mov rdi,[r14]
 mov esi,[r14+8]
 call json_object_new_string_len@PLT
 mov rdi,r13
 lea rsi,[rip+.Lrt_message]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lchat_stop_bad
 mov rdi,rbx
 mov rsi,[rsp]
 call .Lrt_save
.Lchat_stop_skip:
 xor ebp,ebp
 jmp .Lchat_stop_end
.Lchat_stop_bad:
 mov ebp,1
.Lchat_stop_end:
 mov rdi,r14
 call printbuf_free@PLT
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
.Lchat_finish_prefix: .asciz "Provider finish_reason: "

.section .rodata
.Lan_diagnostics: .asciz "diagnostics"

.macro GO obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lgo_\key]
 call li_get
.endm
/* Google's part stream feeds the existing ordered native block accumulator. */
.text
.Lgo_event:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,128
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rsp
 xor eax,eax
 mov ecx,16
 rep stosq
 mov rdi,[rbx+PG_MM_GOOGLE_TEXT]
 mov qword ptr [rbx+PG_MM_GOOGLE_TEXT],0
 call json_object_put@PLT
 call printbuf_new@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lgo_bad
 GO r12,error
 test rax,rax
 jz .Lgo_start
 mov rdi,rbx
 mov rsi,r12
 call .Lrt_save
 jmp .Lgo_bad
.Lgo_start:
 cmp qword ptr [rbx+PG_MM_RESPONSE_ID],0
 jne .Lgo_candidate
 lea rdi,[rip+.Lgo_start_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgo_bad
 GO rax,message
 mov r14,rax
 GO r12,responseId
 test rax,rax
 jz .Lgo_start_send
 mov rdi,rax
 call json_object_get@PLT
 mov rdi,r14
 lea rsi,[rip+.Lan_id]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_bad
.Lgo_start_send:
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r13
 call .Lan_event
 test eax,eax
 jnz .Lgo_bad
 mov rdi,[rsp]
 call json_object_put@PLT
 mov qword ptr [rsp],0
.Lgo_candidate:
 GO r12,candidates
 test rax,rax
 jz .Lgo_candidate_missing
 mov [rsp+16],rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lgo_bad
 mov rdi,[rsp+16]
 xor esi,esi
 call json_object_array_get_idx@PLT
.Lgo_candidate_missing:
 mov [rsp+16],rax
 GO rax,content
 GO rax,parts
 mov [rsp+24],rax
 test rax,rax
 jz .Lgo_parts_valid
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lgo_bad
.Lgo_parts_valid:
 mov qword ptr [rsp+32],0
.Lgo_part:
 mov rdi,[rsp+24]
 test rdi,rdi
 jz .Lgo_finish
 call json_object_array_length@PLT
 cmp [rsp+32],rax
 jae .Lgo_finish
 cmp dword ptr [rbx+PG_MM_FINISH_REASON],0
 jne .Lgo_bad
 mov rdi,[rsp+24]
 mov rsi,[rsp+32]
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov [rsp+40],rax
 GO rax,functionCall
 mov [rsp+56],rax
 test rax,rax
 jnz .Lgo_call
 GO r14,text
 mov [rsp+48],rax
 test rax,rax
 jz .Lgo_part_next
 GO r14,thought
 mov rdi,rax
 call json_object_get_boolean@PLT
 add eax,1
 mov [rsp+64],eax
 /* Coalesce adjacent text/thinking chunks just as the SDK adapter does. */
 mov rdi,rbx
 mov esi,eax
 mov rdx,r13
 call .Lgo_open_text
 test eax,eax
 jnz .Lgo_bad
 lea rdi,[rip+.Lgo_delta_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgo_bad
 mov rax,[rbx+PG_MM_ANTHROPIC_COUNT]
 dec rax
 mov rdi,[rsp]
 lea rsi,[rip+.Lan_index]
 mov [rsp+72],rax
 mov rdi,rax
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lan_index]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_bad
 GO "qword ptr [rsp]",delta
 mov rbp,rax
 lea rsi,[rip+.Lan_text_delta_type]
 cmp dword ptr [rsp+64],1
 je .Lgo_delta_type
 lea rsi,[rip+.Lan_thinking_delta]
.Lgo_delta_type:
 mov rdi,rsi
 call json_object_new_string@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lan_type]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_bad
 mov rdi,[rsp+48]
 call json_object_get@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lan_text]
 cmp dword ptr [rsp+64],1
 je .Lgo_delta_field
 lea rsi,[rip+.Lan_thinking]
.Lgo_delta_field:
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_bad
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r13
 call .Lan_event
 test eax,eax
 jnz .Lgo_bad
 mov rdi,[rsp]
 call json_object_put@PLT
 mov qword ptr [rsp],0
 cmp dword ptr [rsp+64],1
 jne .Lgo_part_signature
 mov rdi,[rsp+8]
 mov rsi,[rsp+48]
 call .Lan_append
 test eax,eax
 jnz .Lgo_bad
 jmp .Lgo_part_signature
.Lgo_call:
 mov rdi,rbx
 mov rsi,r13
 call .Lgo_close_block
 test eax,eax
 jnz .Lgo_bad
 GO "qword ptr [rsp+56]",id
 mov [rsp+80],rax
 test rax,rax
 jz .Lgo_generate_id
 mov rdi,rax
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lgo_generate_id
 mov rdi,[rsp+80]
 call json_object_get_string@PLT
 mov [rsp+88],rax
 xor ebp,ebp
.Lgo_unique_id:
 cmp ebp,[rbx+PG_MM_CALL_COUNT]
 jae .Lgo_id_ready
 mov eax,ebp
 PG_MM_CALL_SLOT rdx,rax
 lea rdi,[rdx+PG_MM_TEXT_LIMIT]
 mov rsi,[rsp+88]
 call strcmp@PLT
 test eax,eax
 jz .Lgo_generate_id
 inc ebp
 jmp .Lgo_unique_id
.Lgo_generate_id:
 lea rdi,[rsp+96]
 xor esi,esi
 call gettimeofday@PLT
 test eax,eax
 jnz .Lgo_bad
 mov rax,[rsp+104]
 xor edx,edx
 mov ecx,1000
 div rcx
 imul rbp,[rsp+96],1000
 add rbp,rax
 mov rax,1
 lock xadd [rip+.Lgo_call_counter],rax
 inc rax
 mov [rsp+112],rax
 GO "qword ptr [rsp+56]",name
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lgo_bad
 lea rdi,[rbx+PG_MM_TEXT]
 mov esi,PG_MM_TEXT_LIMIT
 lea rdx,[rip+.Lgo_id_format]
 mov ecx,24
 mov r8,rax
 mov r9,rbp
 mov rax,[rsp+112]
 mov [rsp+120],rax
 # Seventh printf argument is passed on the stack.
 sub rsp,16
 mov [rsp],rax
 xor eax,eax
 call snprintf@PLT
 add rsp,16
 lea rdi,[rbx+PG_MM_TEXT]
 call json_object_new_string@PLT
 mov [rsp+80],rax
 mov [rsp+88],rax
 test rax,rax
 jz .Lgo_bad
 jmp .Lgo_id_owned
.Lgo_id_ready:
 mov rdi,[rsp+80]
 call json_object_get@PLT
 mov [rsp+88],rax
.Lgo_id_owned:
 lea rdi,[rip+.Lgo_call_start_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgo_bad
 mov rdi,[rbx+PG_MM_ANTHROPIC_COUNT]
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lan_index]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_bad
 GO "qword ptr [rsp]",content_block
 mov rbp,rax
 mov rdi,rax
 lea rsi,[rip+.Lan_id]
 mov rdx,[rsp+88]
 mov qword ptr [rsp+88],0
 call .Lrs_set
 test eax,eax
 jnz .Lgo_bad
 mov rdi,rbp
 mov rsi,[rsp+56]
 lea rdx,[rip+.Lan_name]
 mov rcx,rdx
 call .Lrs_copy
 test eax,eax
 jnz .Lgo_bad
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r13
 call .Lan_event
 test eax,eax
 jnz .Lgo_bad
 mov rdi,[rsp]
 call json_object_put@PLT
 mov qword ptr [rsp],0
 GO "qword ptr [rsp+56]",args
 test rax,rax
 jz .Lgo_empty_args
 mov rdi,rax
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 test rax,rax
 jz .Lgo_bad
 jmp .Lgo_args_string
.Lgo_empty_args:
 lea rax,[rip+.Lrs_empty]
.Lgo_args_string:
 mov rdi,rax
 call json_object_new_string@PLT
 mov [rsp+88],rax
 test rax,rax
 jz .Lgo_bad
 lea rdi,[rip+.Lgo_call_delta_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgo_bad
 mov rdi,[rbx+PG_MM_ANTHROPIC_COUNT]
 dec rdi
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lan_index]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_bad
 GO "qword ptr [rsp]",delta
 mov rdi,rax
 lea rsi,[rip+.Lan_partial_json]
 mov rdx,[rsp+88]
 mov qword ptr [rsp+88],0
 call .Lrs_set
 test eax,eax
 jnz .Lgo_bad
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r13
 call .Lan_event
 test eax,eax
 jnz .Lgo_bad
 mov rdi,[rsp]
 call json_object_put@PLT
 mov qword ptr [rsp],0
 mov rdi,rbx
 mov rsi,r13
 call .Lgo_close_block
 test eax,eax
 jnz .Lgo_bad
.Lgo_part_signature:
 GO "qword ptr [rsp+40]",thoughtSignature
 mov rbp,rax
 test rax,rax
 jz .Lgo_part_next
 mov rdi,rax
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lgo_part_next
 mov rax,[rbx+PG_MM_ANTHROPIC_COUNT]
 test rax,rax
 jz .Lgo_bad
 dec rax
 imul rax,rax,40
 add rax,[rbx+PG_MM_ANTHROPIC_BLOCKS]
 mov r14,rax
 cmp dword ptr [rax+8],3
 jne .Lgo_text_signature
 mov rdi,rbp
 call json_object_get@PLT
 mov rdi,[r14+32]
 lea rsi,[rip+.Lgo_thoughtSignature]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_bad
 jmp .Lgo_part_next
.Lgo_text_signature:
 mov rax,[r14+24]
 mov dword ptr [rax+8],0
 mov rdx,[rax]
 mov byte ptr [rdx],0
 mov rdi,rax
 mov rsi,rbp
 call .Lan_append
 test eax,eax
 jnz .Lgo_bad
.Lgo_part_next:
 inc qword ptr [rsp+32]
 jmp .Lgo_part
.Lgo_finish:
 GO "qword ptr [rsp+16]",finishReason
 mov [rsp+48],rax
 test rax,rax
 jz .Lgo_usage
 cmp dword ptr [rbx+PG_MM_FINISH_REASON],0
 jne .Lgo_usage
 mov rdi,rbx
 mov rsi,r13
 call .Lgo_close_block
 test eax,eax
 jnz .Lgo_bad
 lea rdi,[rip+.Lgo_finish_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgo_bad
 GO rax,delta
 mov rbp,rax
 mov rdi,[rsp+48]
 call json_object_get_string@PLT
 test rax,rax
 jz .Lgo_bad
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Lgo_STOP]
 call strcmp@PLT
 test eax,eax
 jnz .Lgo_length
 lea rdi,[rip+.Lan_end_turn]
 cmp dword ptr [rbx+PG_MM_CALL_COUNT],0
 je .Lgo_finish_reason
 lea rdi,[rip+.Lan_tool_use]
 jmp .Lgo_finish_reason
.Lgo_length:
 mov rdi,r14
 lea rsi,[rip+.Lgo_MAX_TOKENS]
 call strcmp@PLT
 test eax,eax
 jnz .Lgo_reason_error
 lea rdi,[rip+.Lan_max_tokens]
.Lgo_finish_reason:
 call json_object_new_string@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lan_stop_reason]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_bad
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r13
 call .Lan_event
 test eax,eax
 jnz .Lgo_bad
 mov rdi,[rsp+48]
 call json_object_get@PLT
 mov rdi,[rbx+PG_MM_RESPONSE_TERMINAL]
 lea rsi,[rip+.Lan_stop_reason]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_bad
 mov rdi,[rsp]
 call json_object_put@PLT
 lea rdi,[rip+.Lgo_stop_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgo_bad
 mov rdi,rbx
 mov rsi,rax
 mov rdx,r13
 call .Lan_event
 test eax,eax
 jnz .Lgo_bad
 jmp .Lgo_usage
.Lgo_reason_error:
 mov rdi,[rsp+48]
 call json_object_get@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lan_stop_reason]
 mov rdx,rax
 call .Lrs_set
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,rbp
 call .Lan_stop_failure
 jmp .Lgo_bad
.Lgo_usage:
 mov rdi,rbx
 mov rsi,r12
 call .Lgo_usage_data
 test eax,eax
 jnz .Lgo_bad
 mov rax,[rsp+8]
 mov edx,[rax+8]
 test edx,edx
 jz .Lgo_ok
 mov [rsp+112],rdx
 mov rdi,[rax]
 mov esi,edx
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Lgo_bad
 mov [rbx+PG_MM_GOOGLE_TEXT],rax
 mov rdi,rax
 call json_object_get_string@PLT
 mov dword ptr [r13+PG_MM_EVT_KIND],PG_MM_KIND_TEXT
 mov [r13+8],rax
 mov rax,[rsp+112]
 mov [r13+16],rax
.Lgo_ok:
 xor ebp,ebp
 jmp .Lgo_end
.Lgo_bad:
 mov ebp,1
.Lgo_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+88]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call printbuf_free@PLT
 mov eax,ebp
 add rsp,128
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Close the sole open text/thinking/call block before switching kinds. */
.Lgo_close_block:
 push rbx
 push r12
 push r13
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [rsp],0
 mov rax,[rbx+PG_MM_ANTHROPIC_COUNT]
 test rax,rax
 jz .Lgo_close_ok
 dec rax
 imul rcx,rax,40
 add rcx,[rbx+PG_MM_ANTHROPIC_BLOCKS]
 cmp dword ptr [rcx+12],0
 jne .Lgo_close_ok
 mov r13,rax
 lea rdi,[rip+.Lgo_close_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgo_close_bad
 mov rdi,r13
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lan_index]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_close_bad
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r12
 call .Lan_event
 mov r13d,eax
 jmp .Lgo_close_end
.Lgo_close_ok:
 xor r13d,r13d
 jmp .Lgo_close_end
.Lgo_close_bad:
 mov r13d,1
.Lgo_close_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,r13d
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret
.Lgo_open_text:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12d,esi
 mov r13,rdx
 mov qword ptr [rsp],0
 mov rax,[rbx+PG_MM_ANTHROPIC_COUNT]
 test rax,rax
 jz .Lgo_open_new
 dec rax
 imul rax,rax,40
 add rax,[rbx+PG_MM_ANTHROPIC_BLOCKS]
 cmp dword ptr [rax+12],0
 jne .Lgo_open_new
 cmp [rax+8],r12d
 je .Lgo_open_ok
.Lgo_open_new:
 mov rdi,rbx
 mov rsi,r13
 call .Lgo_close_block
 test eax,eax
 jnz .Lgo_open_bad
 lea rdi,[rip+.Lgo_text_start_json]
 cmp r12d,1
 je .Lgo_open_template
 lea rdi,[rip+.Lgo_thought_start_json]
.Lgo_open_template:
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgo_open_bad
 mov rdi,[rbx+PG_MM_ANTHROPIC_COUNT]
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lan_index]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_open_bad
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r13
 call .Lan_event
 mov r14d,eax
 jmp .Lgo_open_end
.Lgo_open_ok:
 xor r14d,r14d
 jmp .Lgo_open_end
.Lgo_open_bad:
 mov r14d,1
.Lgo_open_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,r14d
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.Lgo_count:
 push rbx
 call li_get
 mov rbx,rax
 test rax,rax
 jz .Lgo_count_zero
 mov rdi,rax
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lgo_count_bad
 mov rdi,rbx
 call json_object_get_int64@PLT
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lgo_count_bad
 xor edx,edx
 pop rbx
 ret
.Lgo_count_zero:
 xor eax,eax
 xor edx,edx
 pop rbx
 ret
.Lgo_count_bad:
 xor eax,eax
 mov edx,1
 pop rbx
 ret
.Lgo_usage_data:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,64
 mov rbx,rdi
 mov rdi,rsi
 lea rsi,[rip+.Lgo_usageMetadata]
 call li_get
 mov r12,rax
 mov qword ptr [rsp],0
 test rax,rax
 jz .Lgo_usage_ok
 xor r13d,r13d
 lea r14,[rip+.Lgo_count_keys]
.Lgo_usage_count:
 mov rdi,r12
 mov rsi,[r14+r13*8]
 call .Lgo_count
 test edx,edx
 jnz .Lgo_usage_bad
 mov [rsp+8+r13*8],rax
 inc r13
 cmp r13,5
 jb .Lgo_usage_count
 lea rdi,[rip+.Lgo_usage_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lgo_usage_bad
 mov rdi,[rsp+8]
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lrs_prompt_tokens]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_usage_bad
 mov rdi,[rsp+16]
 add rdi,[rsp+24]
 mov rax,9007199254740991
 cmp rdi,rax
 ja .Lgo_usage_bad
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.Lrs_completion_tokens]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_usage_bad
 RSGET "qword ptr [rsp]",prompt_tokens_details
 mov rbp,rax
 mov rdi,[rsp+32]
 call json_object_new_int64@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lan_cached_tokens]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_usage_bad
 RSGET "qword ptr [rsp]",completion_tokens_details
 mov rbp,rax
 mov rdi,[rsp+24]
 call json_object_new_int64@PLT
 mov rdi,rbp
 lea rsi,[rip+.Lgo_reasoning_tokens]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lgo_usage_bad
 mov rdi,[rbx+PG_MM_USAGE]
 call json_object_put@PLT
 mov rax,[rsp]
 mov [rbx+PG_MM_USAGE],rax
 mov qword ptr [rsp],0
 mov rdi,r12
 call json_object_get@PLT
 mov r12,rax
 mov rdi,[rbx+PG_MM_ANTHROPIC_USAGE]
 call json_object_put@PLT
 mov [rbx+PG_MM_ANTHROPIC_USAGE],r12
.Lgo_usage_ok:
 xor ebp,ebp
 jmp .Lgo_usage_end
.Lgo_usage_bad:
 mov ebp,1
.Lgo_usage_end:
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
.Lgo_error: .asciz "error"
.Lgo_message: .asciz "message"
.Lgo_responseId: .asciz "responseId"
.Lgo_candidates: .asciz "candidates"
.Lgo_content: .asciz "content"
.Lgo_parts: .asciz "parts"
.Lgo_functionCall: .asciz "functionCall"
.Lgo_text: .asciz "text"
.Lgo_thought: .asciz "thought"
.Lgo_delta: .asciz "delta"
.Lgo_id: .asciz "id"
.Lgo_name: .asciz "name"
.Lgo_content_block: .asciz "content_block"
.Lgo_args: .asciz "args"
.Lgo_thoughtSignature: .asciz "thoughtSignature"
.Lgo_finishReason: .asciz "finishReason"
.Lgo_STOP: .asciz "STOP"
.Lgo_MAX_TOKENS: .asciz "MAX_TOKENS"
.Lgo_usageMetadata: .asciz "usageMetadata"
.Lgo_promptTokenCount: .asciz "promptTokenCount"
.Lgo_candidatesTokenCount: .asciz "candidatesTokenCount"
.Lgo_thoughtsTokenCount: .asciz "thoughtsTokenCount"
.Lgo_cachedContentTokenCount: .asciz "cachedContentTokenCount"
.Lgo_totalTokenCount: .asciz "totalTokenCount"
.Lgo_reasoning_tokens: .asciz "reasoning_tokens"
.Lgo_textSignature: .asciz "textSignature"
.Lgo_generative: .asciz "google-generative-ai"
.Lgo_vertex: .asciz "google-vertex"
.Lgo_start_json: .asciz "{\"type\":\"message_start\",\"message\":{\"id\":\"\",\"usage\":{}}}"
.Lgo_delta_json: .asciz "{\"type\":\"content_block_delta\",\"delta\":{}}"
.Lgo_call_start_json: .asciz "{\"type\":\"content_block_start\",\"content_block\":{\"type\":\"tool_use\",\"input\":{}}}"
.Lgo_call_delta_json: .asciz "{\"type\":\"content_block_delta\",\"delta\":{\"type\":\"input_json_delta\"}}"
.Lgo_finish_json: .asciz "{\"type\":\"message_delta\",\"delta\":{}}"
.Lgo_stop_json: .asciz "{\"type\":\"message_stop\"}"
.Lgo_close_json: .asciz "{\"type\":\"content_block_stop\"}"
.Lgo_text_start_json: .asciz "{\"type\":\"content_block_start\",\"content_block\":{\"type\":\"text\",\"text\":\"\"}}"
.Lgo_thought_start_json: .asciz "{\"type\":\"content_block_start\",\"content_block\":{\"type\":\"thinking\",\"thinking\":\"\"}}"
.Lgo_usage_json: .asciz "{\"prompt_tokens_details\":{},\"completion_tokens_details\":{}}"
.Lgo_id_format: .asciz "%.*s_%llu_%llu"
.section .data.rel.ro
.Lgo_count_keys: .quad .Lgo_promptTokenCount,.Lgo_candidatesTokenCount,.Lgo_thoughtsTokenCount,.Lgo_cachedContentTokenCount,.Lgo_totalTokenCount
.section .bss
.balign 8
.Lgo_call_counter: .zero 8

.section .rodata
.Lgo_totalTokens: .asciz "totalTokens"

.section .rodata
.Lgo_error_format: .asciz "Provider stopped with: %s"

.text
/* IEEE CRC-32 for AWS event-stream prelude and message checksums. */
.Lbt_crc:
 mov eax,-1
.Lbt_crc_byte:
 test rsi,rsi
 jz .Lbt_crc_done
 movzx edx,byte ptr [rdi]
 xor eax,edx
 mov ecx,8
.Lbt_crc_bit:
 mov edx,eax
 and edx,1
 neg edx
 and edx,0xedb88320
 shr eax,1
 xor eax,edx
 dec ecx
 jnz .Lbt_crc_bit
 inc rdi
 dec rsi
 jmp .Lbt_crc_byte
.Lbt_crc_done:
 not eax
 ret
.macro BTGET obj,key
 mov rdi,\obj
 lea rsi,[rip+.LbtK_\key]
 call json_object_object_get@PLT
.endm
.macro BTCOPY dst,src,old,new
 mov rdi,\dst
 mov rsi,\src
 lea rdx,[rip+.LbtK_\old]
 lea rcx,[rip+.LbtK_\new]
 call .Lrs_copy
 test eax,eax
 jnz .Lbt_bad
.endm
/* A complete validated transport frame; payload and normalized events are owned here. */
.Lbt_decode:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,192
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rsp
 xor eax,eax
 mov ecx,24
 rep stosq
 mov rdi,[rbx+PG_MM_GOOGLE_TEXT]
 mov qword ptr [rbx+PG_MM_GOOGLE_TEXT],0
 call json_object_put@PLT
 mov r14d,[r12]
 bswap r14d
 mov [rsp+48],r14
 mov ebp,[r12+4]
 bswap ebp
 mov [rsp+56],rbp
 mov eax,r14d
 sub eax,16
 cmp ebp,eax
 ja .Lbt_bad
 mov rdi,r12
 mov esi,8
 call .Lbt_crc
 mov edx,[r12+8]
 bswap edx
 cmp eax,edx
 jne .Lbt_bad
 mov rdi,r12
 lea rsi,[r14-4]
 call .Lbt_crc
 mov edx,[r12+r14-4]
 bswap edx
 cmp eax,edx
 jne .Lbt_bad
 lea rbp,[r12+12]
 mov r14,[rsp+56]
 add r14,rbp
.Lbt_header:
 cmp rbp,r14
 je .Lbt_payload
 ja .Lbt_bad
 movzx eax,byte ptr [rbp]
 test eax,eax
 jz .Lbt_bad
 mov [rsp+64],rax
 lea rdx,[rbp+rax+2]
 cmp rdx,r14
 ja .Lbt_bad
 movzx ecx,byte ptr [rdx-1]
 cmp ecx,7
 jne .Lbt_header_scalar
 lea rax,[rdx+2]
 cmp rax,r14
 ja .Lbt_bad
 movzx eax,word ptr [rdx]
 rol ax,8
 movzx eax,ax
 lea rcx,[rdx+rax+2]
 cmp rcx,r14
 ja .Lbt_bad
 mov [rsp+72],rcx
 mov [rsp+80],rax
 mov [rsp+88],rdx
 cmp qword ptr [rsp+64],13
 je .Lbt_message_header
 cmp qword ptr [rsp+64],11
 jne .Lbt_header_next
 lea rdi,[rbp+1]
 lea rsi,[rip+.LbtK_event_header]
 mov edx,11
 call memcmp@PLT
 test eax,eax
 jnz .Lbt_header_next
 cmp dword ptr [rsp+104],0
 jne .Lbt_bad
 mov dword ptr [rsp+104],1
 cmp qword ptr [rsp+80],63
 ja .Lbt_bad
 lea rdi,[rsp+112]
 mov rsi,[rsp+88]
 add rsi,2
 mov rdx,[rsp+80]
 call memcpy@PLT
 jmp .Lbt_header_next
.Lbt_message_header:
 lea rdi,[rbp+1]
 lea rsi,[rip+.LbtK_message_header]
 mov edx,13
 call memcmp@PLT
 test eax,eax
 jnz .Lbt_header_next
 cmp dword ptr [rsp+108],0
 jne .Lbt_bad
 mov dword ptr [rsp+108],2
 cmp qword ptr [rsp+80],5
 jne .Lbt_header_next
 mov rdi,[rsp+88]
 add rdi,2
 lea rsi,[rip+.LbtK_event_value]
 mov edx,5
 call memcmp@PLT
 test eax,eax
 jnz .Lbt_header_next
 mov dword ptr [rsp+108],1
.Lbt_header_next:
 mov rbp,[rsp+72]
 jmp .Lbt_header
.Lbt_header_scalar:
 cmp ecx,1
 jbe .Lbt_header_bool
 cmp ecx,2
 je .Lbt_header_byte
 cmp ecx,3
 je .Lbt_header_short
 cmp ecx,4
 je .Lbt_header_int
 cmp ecx,5
 je .Lbt_header_long
 cmp ecx,8
 je .Lbt_header_long
 cmp ecx,9
 je .Lbt_header_uuid
 cmp ecx,6
 jne .Lbt_bad
 lea rax,[rdx+2]
 cmp rax,r14
 ja .Lbt_bad
 movzx eax,word ptr [rdx]
 rol ax,8
 movzx eax,ax
 lea rbp,[rdx+rax+2]
 jmp .Lbt_header
.Lbt_header_bool:
 mov rbp,rdx
 jmp .Lbt_header
.Lbt_header_byte:
 lea rbp,[rdx+1]
 jmp .Lbt_header
.Lbt_header_short:
 lea rbp,[rdx+2]
 jmp .Lbt_header
.Lbt_header_int:
 lea rbp,[rdx+4]
 jmp .Lbt_header
.Lbt_header_long:
 lea rbp,[rdx+8]
 jmp .Lbt_header
.Lbt_header_uuid:
 lea rbp,[rdx+16]
 jmp .Lbt_header
.Lbt_payload:
 call json_tokener_new@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbt_bad
 mov rdi,rax
 mov esi,17
 call json_tokener_set_flags@PLT
 mov rdi,[rsp]
 mov rsi,r14
 mov rdx,[rsp+48]
 sub rdx,[rsp+56]
 sub rdx,16
 mov [rsp+96],rdx
 call json_tokener_parse_ex@PLT
 mov [rsp+8],rax
 mov rdi,[rsp]
 call json_tokener_get_error@PLT
 test eax,eax
 jnz .Lbt_bad
 mov rdi,[rsp]
 call json_tokener_get_parse_end@PLT
 cmp rax,[rsp+96]
 jne .Lbt_bad
 mov rdi,[rsp+8]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lbt_bad
 cmp dword ptr [rsp+108],1
 je .Lbt_dispatch
 mov rdi,rbx
 mov rsi,[rsp+8]
 call .Lrt_save
 jmp .Lbt_bad
.Lbt_dispatch:
 lea rdi,[rsp+112]
 lea rsi,[rip+.LbtK_messageStart]
 call strcmp@PLT
 test eax,eax
 jz .Lbt_message_start
 lea rdi,[rsp+112]
 lea rsi,[rip+.LbtK_contentBlockStart]
 call strcmp@PLT
 test eax,eax
 jz .Lbt_block_start
 lea rdi,[rsp+112]
 lea rsi,[rip+.LbtK_contentBlockDelta]
 call strcmp@PLT
 test eax,eax
 jz .Lbt_block_delta
 lea rdi,[rsp+112]
 lea rsi,[rip+.LbtK_contentBlockStop]
 call strcmp@PLT
 test eax,eax
 jz .Lbt_block_stop
 lea rdi,[rsp+112]
 lea rsi,[rip+.LbtK_messageStop]
 call strcmp@PLT
 test eax,eax
 jz .Lbt_message_stop
 lea rdi,[rsp+112]
 lea rsi,[rip+.LbtK_metadata]
 call strcmp@PLT
 test eax,eax
 jz .Lbt_metadata
 mov rdi,rbx
 mov rsi,[rsp+8]
 call .Lrt_save
 jmp .Lbt_bad
.Lbt_message_start:
 BTGET "qword ptr [rsp+8]",role
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lbt_bad
 mov rdi,rax
 lea rsi,[rip+.LbtK_assistant]
 call strcmp@PLT
 test eax,eax
 jnz .Lbt_bad
 lea rdi,[rip+.LbtJ_start]
 jmp .Lbt_parse_emit
.Lbt_block_start:
 BTGET "qword ptr [rsp+8]",start
 BTGET rax,toolUse
 mov [rsp+24],rax
 test rax,rax
 jz .Lbt_ok
 lea rdi,[rip+.LbtJ_tool]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lbt_bad
 BTCOPY rax,"qword ptr [rsp+8]",contentBlockIndex,index
 BTGET "qword ptr [rsp+16]",content_block
 mov r14,rax
 BTCOPY r14,"qword ptr [rsp+24]",toolUseId,id
 BTCOPY r14,"qword ptr [rsp+24]",name,name
 jmp .Lbt_emit
.Lbt_block_delta:
 BTGET "qword ptr [rsp+8]",delta
 mov [rsp+24],rax
 BTGET rax,text
 test rax,rax
 jnz .Lbt_text
 BTGET "qword ptr [rsp+24]",toolUse
 test rax,rax
 jnz .Lbt_input
 BTGET "qword ptr [rsp+24]",reasoningContent
 test rax,rax
 jz .Lbt_bad
 jmp .Lbt_reasoning
.Lbt_text:
 mov [rsp+40],rax
 mov rdi,rbx
 mov rsi,[rsp+8]
 mov rdx,r13
 lea rcx,[rip+.LbtJ_start_text]
 call .Lbt_ensure_text
 test eax,eax
 jnz .Lbt_bad
 lea rdi,[rip+.LbtJ_text]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lbt_bad
 BTCOPY rax,"qword ptr [rsp+8]",contentBlockIndex,index
 BTGET "qword ptr [rsp+16]",delta
 BTCOPY rax,"qword ptr [rsp+24]",text,text
 mov rdi,rbx
 mov rsi,[rsp+16]
 mov rdx,r13
 call .Lan_event
 test eax,eax
 jnz .Lbt_bad
 mov rdi,[rsp+40]
 call json_object_get@PLT
 mov [rbx+PG_MM_GOOGLE_TEXT],rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 mov [r13+16],rax
 mov rdi,[rsp+40]
 call json_object_get_string@PLT
 mov [r13+8],rax
 mov dword ptr [r13],PG_MM_KIND_TEXT
 jmp .Lbt_ok
.Lbt_reasoning:
 mov [rsp+24],rax
 BTGET rax,redactedContent
 test rax,rax
 jnz .Lbt_redacted
.Lbt_normal_reasoning:
 mov rdi,rbx
 mov rsi,[rsp+8]
 mov rdx,r13
 lea rcx,[rip+.LbtJ_start_thinking]
 call .Lbt_ensure_text
 test eax,eax
 jnz .Lbt_bad
 BTGET "qword ptr [rsp+24]",text
 test rax,rax
 jz .Lbt_reasoning_signature
 lea rdi,[rip+.LbtJ_thinking]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lbt_bad
 BTCOPY rax,"qword ptr [rsp+8]",contentBlockIndex,index
 BTGET "qword ptr [rsp+16]",delta
 BTCOPY rax,"qword ptr [rsp+24]",text,thinking
 mov rdi,rbx
 mov rsi,[rsp+16]
 mov rdx,r13
 call .Lan_event
 test eax,eax
 jnz .Lbt_bad
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov qword ptr [rsp+16],0
.Lbt_reasoning_signature:
 BTGET "qword ptr [rsp+24]",signature
 test rax,rax
 jz .Lbt_ok
 lea rdi,[rip+.LbtJ_signature]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lbt_bad
 BTCOPY rax,"qword ptr [rsp+8]",contentBlockIndex,index
 BTGET "qword ptr [rsp+16]",delta
 BTCOPY rax,"qword ptr [rsp+24]",signature,signature
 jmp .Lbt_emit
.Lbt_redacted:
 mov rdi,rax
 lea rsi,[rsp+176]
 lea rdx,[rsp+184]
 call pg_mimo_base64_decode
 test eax,eax
 jnz .Lbt_bad
 cmp qword ptr [rsp+184],0
 je .Lbt_normal_reasoning
 mov rdi,rbx
 mov rsi,[rsp+8]
 mov rdx,r13
 lea rcx,[rip+.LbtJ_start_thinking]
 call .Lbt_ensure_text
 test eax,eax
 jnz .Lbt_bad
 BTGET "qword ptr [rsp+8]",contentBlockIndex
 mov rdi,rax
 call json_object_get_int64@PLT
 xor ecx,ecx
.Lbt_redacted_find:
 cmp rcx,[rbx+PG_MM_ANTHROPIC_COUNT]
 jae .Lbt_bad
 imul r14,rcx,40
 add r14,[rbx+PG_MM_ANTHROPIC_BLOCKS]
 cmp [r14],rax
 je .Lbt_redacted_slot
 inc rcx
 jmp .Lbt_redacted_find
.Lbt_redacted_slot:
 cmp dword ptr [r14+12],0
 jne .Lbt_bad
 BTGET "qword ptr [rsp+24]",text
 test rax,rax
 jz .Lbt_redacted_transition
 mov rsi,rax
 mov rdi,[r14+16]
 call .Lan_append
 test eax,eax
 jnz .Lbt_bad
.Lbt_redacted_transition:
 cmp dword ptr [r14+8],4
 je .Lbt_redacted_append
 cmp dword ptr [r14+8],2
 jne .Lbt_bad
 mov dword ptr [r14+8],4
 mov rax,[r14+24]
 mov dword ptr [rax+8],0
 mov rax,[rax]
 mov byte ptr [rax],0
 mov edi,1
 call json_object_new_boolean@PLT
 mov rdi,[r14+32]
 lea rsi,[rip+.LbtK_redacted]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lbt_bad
 mov rdi,[r14+16]
 lea rsi,[rip+.Lan_redacted]
 mov edx,20
 call printbuf_memappend@PLT
 test eax,eax
 js .Lbt_bad
.Lbt_redacted_append:
 mov rdi,[r14+24]
 mov rsi,[rsp+176]
 mov rdx,[rsp+184]
 call printbuf_memappend@PLT
 test eax,eax
 js .Lbt_bad
 jmp .Lbt_ok
.Lbt_input:
 mov [rsp+24],rax
 lea rdi,[rip+.LbtJ_input]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lbt_bad
 BTCOPY rax,"qword ptr [rsp+8]",contentBlockIndex,index
 BTGET "qword ptr [rsp+16]",delta
 BTCOPY rax,"qword ptr [rsp+24]",input,partial_json
 jmp .Lbt_emit
.Lbt_block_stop:
 lea rdi,[rip+.LbtJ_stop_block]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lbt_bad
 BTCOPY rax,"qword ptr [rsp+8]",contentBlockIndex,index
 jmp .Lbt_emit
.Lbt_message_stop:
 mov rdi,rbx
 mov rsi,r13
 call .Lbt_flush
 test eax,eax
 jnz .Lbt_bad
 lea rdi,[rip+.LbtJ_stop]
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lbt_bad
 BTGET rax,delta
 BTCOPY rax,"qword ptr [rsp+8]",stopReason,stop_reason
 mov rdi,rbx
 mov rsi,[rsp+16]
 mov rdx,r13
 call .Lan_event
 test eax,eax
 jnz .Lbt_bad
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov qword ptr [rsp+16],0
 lea rdi,[rip+.LbtJ_done]
 jmp .Lbt_parse_emit
.Lbt_metadata:
 BTGET "qword ptr [rsp+8]",usage
 mov [rsp+24],rax
 call json_object_new_object@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lbt_bad
 BTCOPY rax,"qword ptr [rsp+24]",inputTokens,input_tokens
 BTCOPY "qword ptr [rsp+16]","qword ptr [rsp+24]",outputTokens,output_tokens
 BTCOPY "qword ptr [rsp+16]","qword ptr [rsp+24]",cacheReadInputTokens,cache_read_input_tokens
 BTCOPY "qword ptr [rsp+16]","qword ptr [rsp+24]",cacheWriteInputTokens,cache_creation_input_tokens
 mov rdi,rbx
 mov rsi,[rsp+16]
 call .Lan_update_usage_object
 test eax,eax
 jnz .Lbt_bad
 jmp .Lbt_ok
.Lbt_parse_emit:
 call json_tokener_parse@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lbt_bad
.Lbt_emit:
 mov rdi,rbx
 mov rsi,[rsp+16]
 mov rdx,r13
 call .Lan_event
 test eax,eax
 jnz .Lbt_bad
.Lbt_ok:
 xor ebp,ebp
 jmp .Lbt_cleanup
.Lbt_bad:
 mov ebp,1
.Lbt_cleanup:
 mov rdi,[rsp+176]
 call free@PLT
 mov rdi,[rsp]
 call json_tokener_free@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,192
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Bedrock starts text on its first delta. Reuse the existing block index table. */
.Lbt_ensure_text:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov qword ptr [rsp],0
 BTGET r12,contentBlockIndex
 mov rdi,rax
 call json_object_get_int64@PLT
 xor ecx,ecx
.Lbt_ensure_find:
 cmp rcx,[rbx+PG_MM_ANTHROPIC_COUNT]
 jae .Lbt_ensure_create
 imul rdx,rcx,40
 add rdx,[rbx+PG_MM_ANTHROPIC_BLOCKS]
 cmp [rdx],rax
 je .Lbt_ensure_ok
 inc rcx
 jmp .Lbt_ensure_find
.Lbt_ensure_create:
 mov rdi,r14
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbt_ensure_bad
 mov rdi,rax
 mov rsi,r12
 lea rdx,[rip+.LbtK_contentBlockIndex]
 lea rcx,[rip+.LbtK_index]
 call .Lrs_copy
 test eax,eax
 jnz .Lbt_ensure_bad
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r13
 call .Lan_event
 test eax,eax
 jnz .Lbt_ensure_bad
.Lbt_ensure_ok:
 xor ebp,ebp
 jmp .Lbt_ensure_end
.Lbt_ensure_bad:
 mov ebp,1
.Lbt_ensure_end:
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
.Lbt_flush:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
 mov qword ptr [rsp],0
.Lbt_flush_each:
 cmp r13,[rbx+PG_MM_ANTHROPIC_COUNT]
 jae .Lbt_flush_ok
 imul r14,r13,40
 add r14,[rbx+PG_MM_ANTHROPIC_BLOCKS]
 cmp dword ptr [r14+12],0
 jne .Lbt_flush_next
 lea rdi,[rip+.LbtJ_stop_block]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbt_flush_bad
 mov rdi,[r14]
 call json_object_new_int64@PLT
 mov rdi,[rsp]
 lea rsi,[rip+.LbtK_index]
 mov rdx,rax
 call .Lrs_set
 test eax,eax
 jnz .Lbt_flush_bad
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,r12
 call .Lan_event
 test eax,eax
 jnz .Lbt_flush_bad
 mov rdi,[rsp]
 call json_object_put@PLT
 mov qword ptr [rsp],0
.Lbt_flush_next:
 inc r13
 jmp .Lbt_flush_each
.Lbt_flush_ok:
 xor ebp,ebp
 jmp .Lbt_flush_end
.Lbt_flush_bad:
 mov ebp,1
.Lbt_flush_end:
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
.LbtK_api: .asciz "bedrock-converse-stream"
.LbtK_event_header: .asciz ":event-type"
.LbtK_messageStart: .asciz "messageStart"
.LbtK_contentBlockStart: .asciz "contentBlockStart"
.LbtK_contentBlockDelta: .asciz "contentBlockDelta"
.LbtK_contentBlockStop: .asciz "contentBlockStop"
.LbtK_messageStop: .asciz "messageStop"
.LbtK_metadata: .asciz "metadata"
.LbtK_role: .asciz "role"
.LbtK_assistant: .asciz "assistant"
.LbtK_start: .asciz "start"
.LbtK_toolUse: .asciz "toolUse"
.LbtK_toolUseId: .asciz "toolUseId"
.LbtK_name: .asciz "name"
.LbtK_content_block: .asciz "content_block"
.LbtK_contentBlockIndex: .asciz "contentBlockIndex"
.LbtK_index: .asciz "index"
.LbtK_id: .asciz "id"
.LbtK_delta: .asciz "delta"
.LbtK_text: .asciz "text"
.LbtK_input: .asciz "input"
.LbtK_partial_json: .asciz "partial_json"
.LbtK_stopReason: .asciz "stopReason"
.LbtK_stop_reason: .asciz "stop_reason"
.LbtK_usage: .asciz "usage"
.LbtK_inputTokens: .asciz "inputTokens"
.LbtK_outputTokens: .asciz "outputTokens"
.LbtK_cacheReadInputTokens: .asciz "cacheReadInputTokens"
.LbtK_cacheWriteInputTokens: .asciz "cacheWriteInputTokens"
.LbtK_input_tokens: .asciz "input_tokens"
.LbtK_output_tokens: .asciz "output_tokens"
.LbtK_cache_read_input_tokens: .asciz "cache_read_input_tokens"
.LbtK_cache_creation_input_tokens: .asciz "cache_creation_input_tokens"
.LbtJ_start: .asciz "{\"type\":\"message_start\",\"message\":{\"id\":\"\",\"usage\":{}}}"
.LbtJ_tool: .asciz "{\"type\":\"content_block_start\",\"content_block\":{\"type\":\"tool_use\",\"input\":{}}}"
.LbtJ_text: .asciz "{\"type\":\"content_block_delta\",\"delta\":{\"type\":\"text_delta\"}}"
.LbtJ_input: .asciz "{\"type\":\"content_block_delta\",\"delta\":{\"type\":\"input_json_delta\"}}"
.LbtJ_stop_block: .asciz "{\"type\":\"content_block_stop\"}"
.LbtJ_stop: .asciz "{\"type\":\"message_delta\",\"delta\":{}}"
.LbtJ_done: .asciz "{\"type\":\"message_stop\"}"
.LbtJ_start_text: .asciz "{\"type\":\"content_block_start\",\"content_block\":{\"type\":\"text\",\"text\":\"\"}}"

.section .rodata
.LbtK_message_header: .asciz ":message-type"
.LbtK_event_value: .asciz "event"

.section .rodata
.LbtK_context_limit: .asciz "model_context_window_exceeded"

.section .rodata
.LbtK_reasoningContent: .asciz "reasoningContent"
.LbtK_thinking: .asciz "thinking"
.LbtK_signature: .asciz "signature"
.LbtJ_start_thinking: .asciz "{\"type\":\"content_block_start\",\"content_block\":{\"type\":\"thinking\",\"thinking\":\"\"}}"
.LbtJ_thinking: .asciz "{\"type\":\"content_block_delta\",\"delta\":{\"type\":\"thinking_delta\"}}"
.LbtJ_signature: .asciz "{\"type\":\"content_block_delta\",\"delta\":{\"type\":\"signature_delta\"}}"

.section .rodata
.LbtK_redactedContent: .asciz "redactedContent"

.text
/* Decode atob-compatible JSON Base64 into caller-owned bytes. 1 invalid, 2 allocation failure. */
.globl pg_mimo_base64_decode
.type pg_mimo_base64_decode,@function
pg_mimo_base64_decode:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r12],0
 mov qword ptr [r13],0
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lbb_invalid
 mov rdi,rbx
 call json_object_get_string_len@PLT
 cmp eax,2147483643
 ja .Lbb_invalid
 mov [rsp+16],rax
 lea rdi,[rax+4]
 call malloc@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lbb_native
 mov rdi,rbx
 call json_object_get_string@PLT
 mov rbx,rax
 xor r14d,r14d
 xor ebp,ebp
 xor ecx,ecx
.Lbb_clean:
 cmp rcx,[rsp+16]
 jae .Lbb_padding
 movzx eax,byte ptr [rbx+rcx]
 inc rcx
 cmp eax,32
 je .Lbb_clean
 cmp eax,9
 je .Lbb_clean
 cmp eax,10
 je .Lbb_clean
 cmp eax,12
 je .Lbb_clean
 cmp eax,13
 je .Lbb_clean
 cmp eax,61
 je .Lbb_equal
 test ebp,ebp
 jnz .Lbb_invalid
 cmp eax,65
 jb .Lbb_lower
 cmp eax,90
 jbe .Lbb_store
.Lbb_lower:
 cmp eax,97
 jb .Lbb_digit
 cmp eax,122
 jbe .Lbb_store
.Lbb_digit:
 cmp eax,48
 jb .Lbb_symbol
 cmp eax,57
 jbe .Lbb_store
.Lbb_symbol:
 cmp eax,43
 je .Lbb_store
 cmp eax,47
 jne .Lbb_invalid
 jmp .Lbb_store
.Lbb_equal:
 inc ebp
 cmp ebp,2
 ja .Lbb_invalid
.Lbb_store:
 mov rdx,[rsp]
 mov [rdx+r14],al
 inc r14
 jmp .Lbb_clean
.Lbb_padding:
 mov eax,r14d
 and eax,3
 test ebp,ebp
 jz .Lbb_unpadded
 test eax,eax
 jnz .Lbb_invalid
 jmp .Lbb_decode
.Lbb_unpadded:
 cmp eax,1
 je .Lbb_invalid
 test eax,eax
 jz .Lbb_decode
 mov rdx,[rsp]
.Lbb_pad:
 mov byte ptr [rdx+r14],61
 inc r14
 inc ebp
 test r14d,3
 jnz .Lbb_pad
.Lbb_decode:
 mov rax,[rsp]
 mov byte ptr [rax+r14],0
 lea rdi,[r14+1]
 call malloc@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lbb_native
 mov rdi,rax
 mov rsi,[rsp]
 mov edx,r14d
 call EVP_DecodeBlock@PLT
 test eax,eax
 js .Lbb_invalid
 sub eax,ebp
 js .Lbb_invalid
 mov [r13],rax
 mov rax,[rsp+8]
 mov [r12],rax
 mov qword ptr [rsp+8],0
 xor ebp,ebp
 jmp .Lbb_end
.Lbb_invalid:
 mov ebp,1
 jmp .Lbb_end
.Lbb_native:
 mov ebp,2
.Lbb_end:
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+8]
 call free@PLT
 mov eax,ebp
 add rsp,48
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_mimo_base64_decode,.-pg_mimo_base64_decode
.globl pg_mimo_base64_encode
.type pg_mimo_base64_encode,@function
pg_mimo_base64_encode:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 cmp rsi,1610612733
 ja .Lbb_encode_bad
 lea rax,[rsi+2]
 xor edx,edx
 mov ecx,3
 div rcx
 lea rdi,[rax*4+1]
 call malloc@PLT
 mov r13,rax
 test rax,rax
 jz .Lbb_encode_bad
 mov rdi,rax
 mov rsi,rbx
 mov edx,r12d
 call EVP_EncodeBlock@PLT
 test eax,eax
 js .Lbb_encode_release
 mov rdi,r13
 mov esi,eax
 call json_object_new_string_len@PLT
 mov rbx,rax
 jmp .Lbb_encode_free
.Lbb_encode_release:
 xor ebx,ebx
.Lbb_encode_free:
 mov rdi,r13
 call free@PLT
 mov rax,rbx
 jmp .Lbb_encode_end
.Lbb_encode_bad:
 xor eax,eax
.Lbb_encode_end:
 pop r13
 pop r12
 pop rbx
 ret
.size pg_mimo_base64_encode,.-pg_mimo_base64_encode

.section .rodata
.LbtK_redacted: .asciz "redacted"
