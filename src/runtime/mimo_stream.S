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
 cmp dword ptr [rbx+PG_MM_SAW_DONE],0
 jne .Lrs_bad
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
 lea rdi,[rip+.Lrc_api_value]
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
 mov edx,19
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
 jne .Lan_bad
 mov rdi,[rsp+80]
 lea rsi,[rip+.Lan_thinking_delta]
 call strcmp@PLT
 test eax,eax
 jnz .Lan_signature_delta
 ANGET "qword ptr [rsp+32]",thinking
 mov rdi,[r14+16]
 jmp .Lan_append_delta
.Lan_signature_delta:
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
 je .Lan_block_closed
 mov rax,[r14+24]
 mov rdi,[rax]
 mov esi,[rax+8]
 call json_object_new_string_len@PLT
 mov rdi,[r14+32]
 lea rsi,[rip+.Lan_thinkingSignature]
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
 lea rdi,[rip+.Lan_api_value]
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
 mov rdi,rax
 lea rsi,[rip+.Lan_unknown_format]
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
