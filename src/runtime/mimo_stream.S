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
 RSGET rax,type
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
 mov rdi,rbx
 mov rsi,[rsp+8]
 mov rdx,r13
 call .Lchat_object
 test eax,eax
 jnz .Lrs_bad
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
