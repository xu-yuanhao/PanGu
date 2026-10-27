/* mimo_stream.S — OpenAI SSE 单行解码与片段累积（decode_line）。
 *
 * 输入是已去除 CR/LF 的完整一行：空行/":" 注释→WAIT 0；"data:" 前缀外→1；
 * "[DONE]" 仅在 finish_reason 之后接受一次。JSON 须完整（parse_end 后仅
 * 空白），单 choice（长度恰 1）、tool_calls 按整数index 0..7分别累积；id/name/
 * arguments 为增量追加（null 视为缺席），text 为当前分片整串替换。解码出的
 * 字符串含嵌入 NUL、超出定长容量即拒绝。finish_reason：stop 需无工具 id，
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

/* ---- int pg_mimo_decode_line(pg_mimo *m, const char *line, pg_model_event *event)
 * 栈帧 112 字节：[0]=tok [8]=choices [16]=choice [24]=delta [32]=data
 * [40]=calls [48]=content [56]=reason [64]=call [72]=index，[80]=delta个数，[88]=迭代，[96]=slot，[104]=fn/内层迭代。 ---- */
.globl pg_mimo_decode_line
.type pg_mimo_decode_line,@function
pg_mimo_decode_line:
 push rbx
 push r12
 push r13
 push r14
 push r15                             /* 入口 rsp≡8；5 压栈后 ≡0 */
 sub rsp, 112
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
 mov rsi,r12
 mov edx,PG_MM_TEXT_LIMIT
 call li_accumulate
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
 add rsp, 112
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
