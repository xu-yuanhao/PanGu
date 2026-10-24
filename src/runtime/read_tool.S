/* read_tool.S — 工作目录只读 read 工具。
 *
 * 契约见 read_tool.h：调用方信任并持有根目录 fd；仅相对路径；绝对路径、空分量、
 * "."/".."、嵌入 NUL、任意层符号链接、非普通文件、超限与二进制内容全部拒绝。
 * 路径按分量逐级 openat（中间目录 O_DIRECTORY、全程 O_NOFOLLOW|O_CLOEXEC|
 * O_NONBLOCK），不拼接字符串、不 realpath；错误路径逐 fd 关闭。JSON 语义：
 * 恰好一个 "path" 字符串成员。fstat/struct stat 经公开 libc 与布局断言常量。
 */
#include "read_tool_layout.h"
#include "tools_layout.h"
.intel_syntax noprefix

.section .rodata
.Lstr_path:      .asciz "path"
.Lread_name:     .asciz "read"
.Lread_desc:     .asciz "Read one small regular text file relative to the configured workspace. Absolute paths, symlinks, dot segments and oversized files are refused."
.Lread_schema:   .asciz "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"],\"additionalProperties\":false}"
.Lmsg_invalid:   .asciz "invalid read path"
.Lmsg_root:      .asciz "read root unavailable"
.Lmsg_refused:   .asciz "read refused or unavailable"
.Lmsg_limit:     .asciz "read output limit exceeded"
.Lmsg_utf8:      .asciz "invalid UTF-8 file is unsupported"
.Lmsg_binary:    .asciz "binary file is unsupported"

.section .text

/* ---- int li_parse_path(args.data, args.size, char path[PG_TEXT_LIMIT])
 * 返回 1=非法。文本经 NUL 终止副本交给 json_tokener_parse；
 * 恰好一个成员 "path"，字符串 1..4095 字节、非 '/' 开头、无嵌入 NUL，
 * 逐分量拒绝空段与 "."/".."。栈帧：[rsp]=text[4096] [rsp+4096]=json_object* p。 */
.type li_parse_path,@function
li_parse_path:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 4112
 mov rbx, rdx /* path 目标 */
 mov r13, rdi /* args.data */
 mov r15, rsi /* args.size */
 test r13, r13
 jz .Lpp_bad
 test r15, r15
 jz .Lpp_bad
 cmp r15, PG_TL_TEXT_LIMIT
 jae .Lpp_bad
 mov rdi, r13
 xor esi, esi
 mov rdx, r15
 call memchr@PLT
 test rax, rax
 jnz .Lpp_bad
 mov rdi, rsp
 mov rsi, r13
 mov rdx, r15
 call memcpy@PLT
 mov byte ptr [rsp + r15], 0
 mov rdi, rsp
 call json_tokener_parse@PLT
 mov r14, rax
 test r14, r14
 jz .Lpp_bad
 mov rdi, r14
 mov esi, PG_TL_JSON_OBJECT
 call json_object_is_type@PLT
 test eax, eax
 jz .Lpp_bad_put
 mov rdi, r14
 call json_object_object_length@PLT
 cmp eax, 1
 jne .Lpp_bad_put
 mov qword ptr [rsp + 4096], 0
 mov rdi, r14
 lea rsi, [rip + .Lstr_path]
 lea rdx, [rsp + 4096]
 call json_object_object_get_ex@PLT
 test eax, eax
 jz .Lpp_bad_put
 mov rdi, [rsp + 4096]
 mov esi, PG_TL_JSON_STRING
 call json_object_is_type@PLT
 test eax, eax
 jz .Lpp_bad_put
 mov rdi, [rsp + 4096]
 call json_object_get_string_len@PLT
 mov r12, rax /* n */
 test r12, r12
 jz .Lpp_bad_put
 cmp r12, PG_TL_TEXT_LIMIT
 jae .Lpp_bad_put
 mov rdi, [rsp + 4096]
 call json_object_get_string@PLT
 mov [rsp + 4104], rax /* s：栈槽保存（r14 保持为 json 对象） */
 cmp byte ptr [rax], '/'
 je .Lpp_bad_put
 mov rdi, rax
 xor esi, esi
 mov rdx, r12
 call memchr@PLT
 test rax, rax
 jnz .Lpp_bad_put
 lea rdi, [rbx]
 mov rsi, [rsp + 4104]
 lea rdx, [r12 + 1] /* 连同终止符一起复制（与被替换 C 一致） */
 call memcpy@PLT
 /* 分量扫描：空段与 "."/".." 拒绝；i==n 处理末段 */
 xor r8d, r8d /* start */
 xor r9d, r9d /* i */
.Lpp_scan:
 cmp r9, r12
 je .Lpp_bound
 cmp byte ptr [rbx + r9], '/'
 je .Lpp_bound
 inc r9
 jmp .Lpp_scan
.Lpp_bound:
 mov rax, r9
 sub rax, r8 /* len = i - start */
 test rax, rax
 jz .Lpp_bad_put
 cmp rax, 1
 jne .Lpp_not_dot
 cmp byte ptr [rbx + r8], '.'
 je .Lpp_bad_put
.Lpp_not_dot:
 cmp rax, 2
 jne .Lpp_next
 cmp byte ptr [rbx + r8], '.'
 jne .Lpp_next
 cmp byte ptr [rbx + r8 + 1], '.'
 je .Lpp_bad_put
.Lpp_next:
 lea r8, [r9 + 1]
 cmp r9, r12
 je .Lpp_ok
 inc r9
 jmp .Lpp_scan
.Lpp_bad:
 mov eax, 1 /* 无对象需要释放 */
 jmp .Lpp_ret
.Lpp_bad_put:
 mov rdi, r14
 call json_object_put@PLT
 mov eax, 1
 jmp .Lpp_ret
.Lpp_ok:
 mov rdi, r14
 call json_object_put@PLT
 xor eax, eax
.Lpp_ret:
 add rsp, 4112
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_parse_path, .-li_parse_path

/* ---- int li_validate(void *ctx, args.data, args.size) ---- */
.type li_validate,@function
li_validate:
 push rbx
 push r12
 push rax
 sub rsp, 4112
 mov r8, rdx
 mov rdx, rsp
 mov rdi, rsi
 mov rsi, r8
 call li_parse_path
 test eax, eax
 setz al
 movzx eax, al
 add rsp, 4112
 pop rcx
 pop r12
 pop rbx
 ret
.size li_validate, .-li_validate

/* ---- int li_authorize(void *ctx, args.data, args.size)：根 fd 有效即可 ---- */
.type li_authorize,@function
li_authorize:
 mov ecx, dword ptr [rdi + PG_RTL_DIRECTORY]
 xor eax, eax
 test ecx, ecx
 setge al
 ret
.size li_authorize, .-li_authorize

/* ---- int li_fail(out, capacity, size*, msg)：先清零，装得下才写消息；恒 1 ---- */
.type li_fail,@function
li_fail:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov r14, rcx
 mov qword ptr [r13], 0
 mov rdi, r14
 call strlen@PLT
 cmp rax, r12
 jae .Lrf_done
 mov r15, rax
 mov rdi, rbx
 mov rsi, r14
 mov rdx, r15
 call memcpy@PLT
 mov qword ptr [r13], r15
.Lrf_done:
 mov eax, 1
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_fail, .-li_fail

/* ---- int li_utf8(const unsigned char *p, size_t n)：严格 UTF-8 校验 ---- */
.type li_utf8,@function
li_utf8:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx, rdi
 mov r12, rsi
 xor r13d, r13d /* i */
.Lu_loop:
 cmp r13, r12
 jae .Lu_ok
 movzx eax, byte ptr [rbx + r13]
 inc r13
 cmp al, 128
 jb .Lu_loop
 cmp al, 0xc2
 jb .Lu_bad
 cmp al, 0xdf
 jbe .Lu_seq2
 cmp al, 0xe0
 jb .Lu_bad
 cmp al, 0xef
 jbe .Lu_seq3
 cmp al, 0xf0
 jb .Lu_bad
 cmp al, 0xf4
 jbe .Lu_seq4
 jmp .Lu_bad
.Lu_seq2:
 mov ecx, 1 /* need */
 and eax, 31
 mov r14, rax /* cp */
 mov r15d, 0x80 /* min */
 jmp .Lu_conts
.Lu_seq3:
 mov ecx, 2
 and eax, 15
 mov r14, rax
 mov r15d, 0x800
 jmp .Lu_conts
.Lu_seq4:
 mov ecx, 3
 and eax, 7
 mov r14, rax
 mov r15d, 0x10000
.Lu_conts:
 mov rdx, r12
 sub rdx, r13 /* 剩余字节 */
 cmp rcx, rdx
 ja .Lu_bad
.Lu_cont_loop:
 test ecx, ecx
 jz .Lu_check_cp
 movzx eax, byte ptr [rbx + r13]
 inc r13
 and eax, 0xC0
 cmp eax, 0x80
 jne .Lu_bad
 movzx eax, byte ptr [rbx + r13 - 1]
 and eax, 63
 shl r14, 6
 or r14, rax
 dec ecx
 jmp .Lu_cont_loop
.Lu_check_cp:
 cmp r14, r15 /* cp < min：过短编码 */
 jb .Lu_bad
 mov rax, 0x10FFFF
 cmp r14, rax
 ja .Lu_bad
 mov rax, 0xD800
 cmp r14, rax
 jb .Lu_loop
 mov rax, 0xDFFF
 cmp r14, rax
 jbe .Lu_bad
 jmp .Lu_loop
.Lu_ok:
 mov eax, 1
 jmp .Lu_out
.Lu_bad:
 xor eax, eax
.Lu_out:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_utf8, .-li_utf8

/* ---- int li_execute_read(ctx, args.data, args.size, out, capacity, size*)
 * 栈帧（sub 4272）：[rsp]=path[4096] [4096]=stat[144] [4240]=extra
 * [4248]=p [4256]=file fd [4264]=used。寄存器：rbx=ctx→part、r12=out、
 * r13=capacity、r14=size*、r15=dir fd。 ---- */
.type li_execute_read,@function
li_execute_read:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 4272
 mov rbx, rdi
 mov r12, rcx
 mov r13, r8
 mov r14, r9
 cmp r13, 2
 jb .Lrd_invalid
 mov r8, rdx
 mov rdx, rsp
 mov rdi, rsi
 mov rsi, r8
 call li_parse_path
 test eax, eax
 jnz .Lrd_invalid
 mov edi, dword ptr [rbx + PG_RTL_DIRECTORY]
 mov esi, PG_RTL_F_DUPFD_CLOEXEC
 xor edx, edx
 xor eax, eax
 call fcntl@PLT
 mov r15, rax
 test r15, r15
 js .Lrd_root_gone
 mov qword ptr [rsp + 4256], -1
 mov rbx, rsp /* part */
 mov qword ptr [rsp + 4248], rsp /* p */
.Lrd_walk:
 mov rax, [rsp + 4248]
 movzx ecx, byte ptr [rax]
 cmp cl, '/'
 je .Lrd_sep
 test cl, cl
 jz .Lrd_last
 inc qword ptr [rsp + 4248]
 jmp .Lrd_walk
.Lrd_sep:
 mov byte ptr [rax], 0
 mov byte ptr [rsp + 4241], 0 /* last 标志存栈：跨 openat/close 调用 */
 mov edx, 0xB0800 /* O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK|O_DIRECTORY */
 jmp .Lrd_openat
.Lrd_last:
 mov byte ptr [rax], 0
 mov byte ptr [rsp + 4241], 1
 mov edx, 0xA0800 /* 末端文件不带 O_DIRECTORY */
.Lrd_openat:
 mov rdi, r15
 mov rsi, rbx
 call openat@PLT
 movsxd rax, eax
 mov [rsp + 4256], rax /* opened：栈槽跨 close（syscall 摧毁 rcx） */
 mov edi, r15d
 call close@PLT
 mov r15, -1
 mov rcx, [rsp + 4256]
 test rcx, rcx
 js .Lrd_refused
 cmp byte ptr [rsp + 4241], 0
 jne .Lrd_got_file
 mov r15, rcx /* opened 变为新的目录 fd */
 mov qword ptr [rsp + 4256], -1 /* file 槽恢复 -1 */
 mov rax, [rsp + 4248]
 lea rbx, [rax + 1]
 jmp .Lrd_walk
.Lrd_got_file: /* [rsp+4256] 已持有文件 fd */
 mov rdi, rcx
 lea rsi, [rsp + 4096]
 call fstat@PLT
 test eax, eax
 jnz .Lrd_refused
 mov eax, dword ptr [rsp + 4096 + PG_RTL_STAT_MODE]
 and eax, PG_RTL_S_IFMT
 cmp eax, PG_RTL_S_IFREG
 jne .Lrd_refused
 mov qword ptr [rsp + 4264], 0
.Lrd_read:
 mov rax, [rsp + 4264]
 lea rcx, [r13 - 1]
 cmp rax, rcx
 jae .Lrd_read_done
 mov rdi, [rsp + 4256]
 lea rsi, [r12 + rax]
 mov rdx, rcx
 sub rdx, rax
 call read@PLT
 test rax, rax
 js .Lrd_refused
 jz .Lrd_read_done
 add qword ptr [rsp + 4264], rax
 jmp .Lrd_read
.Lrd_read_done:
 mov rdi, [rsp + 4256]
 lea rsi, [rsp + 4240]
 mov edx, 1
 call read@PLT
 test rax, rax
 jnz .Lrd_limit
 mov rdi, [rsp + 4256]
 call close@PLT
 mov qword ptr [rsp + 4256], -1
 mov rdi, r12
 mov rsi, [rsp + 4264]
 call li_utf8
 test eax, eax
 jz .Lrd_utf8_bad
 mov rdi, r12
 xor esi, esi
 mov rdx, [rsp + 4264]
 call memchr@PLT
 test rax, rax
 jnz .Lrd_binary_bad
 mov rax, [rsp + 4264]
 mov [r14], rax
 xor eax, eax
 jmp .Lrd_out
.Lrd_limit:
 mov rdi, [rsp + 4256]
 call close@PLT
 mov qword ptr [rsp + 4256], -1
 mov rdi, r12
 mov rsi, r13
 mov rdx, r14
 lea rcx, [rip + .Lmsg_limit]
 call li_fail
 jmp .Lrd_out
.Lrd_utf8_bad:
 mov rdi, r12
 mov rsi, r13
 mov rdx, r14
 lea rcx, [rip + .Lmsg_utf8]
 call li_fail
 jmp .Lrd_out
.Lrd_binary_bad:
 mov rdi, r12
 mov rsi, r13
 mov rdx, r14
 lea rcx, [rip + .Lmsg_binary]
 call li_fail
 jmp .Lrd_out
.Lrd_invalid:
 mov rdi, r12
 mov rsi, r13
 mov rdx, r14
 lea rcx, [rip + .Lmsg_invalid]
 call li_fail
 jmp .Lrd_out
.Lrd_root_gone:
 mov rdi, r12
 mov rsi, r13
 mov rdx, r14
 lea rcx, [rip + .Lmsg_root]
 call li_fail
 jmp .Lrd_out
.Lrd_refused:
 test r15, r15
 js .Lrd_refused_file
 mov edi, r15d
 call close@PLT
.Lrd_refused_file:
 mov rdi, [rsp + 4256]
 test rdi, rdi
 js .Lrd_refused_msg
 call close@PLT
.Lrd_refused_msg:
 mov rdi, r12
 mov rsi, r13
 mov rdx, r14
 lea rcx, [rip + .Lmsg_refused]
 call li_fail
.Lrd_out:
 add rsp, 4272
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_execute_read, .-li_execute_read

/* ---- int pg_read_tool_open(pg_read_tool *r, const char *root) ---- */
.globl pg_read_tool_open
.type pg_read_tool_open,@function
pg_read_tool_open:
 test rdi, rdi
 jz .Lro_bad
 test rsi, rsi
 jz .Lro_bad
 push rbx /* r 跨 open 调用保存于 callee-saved */
 mov rbx, rdi
 mov rdi, rsi
 mov esi, 0xB0000 /* O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW（flags 为第2参） */
 xor eax, eax
 call open@PLT
 mov dword ptr [rbx + PG_RTL_DIRECTORY], eax
 pop rbx
 test eax, eax
 js .Lro_bad
 xor eax, eax
 ret
.Lro_bad:
 mov eax, 1
 ret
.size pg_read_tool_open, .-pg_read_tool_open

/* ---- void pg_read_tool_close(pg_read_tool *r)：幂等 ---- */
.globl pg_read_tool_close
.type pg_read_tool_close,@function
pg_read_tool_close:
 test rdi, rdi
 jz .Lrc_ret
 mov ecx, dword ptr [rdi + PG_RTL_DIRECTORY]
 test ecx, ecx
 js .Lrc_ret
 push rax
 mov [rsp], rdi
 mov edi, ecx
 call close@PLT
 pop rcx
 mov dword ptr [rcx + PG_RTL_DIRECTORY], -1
.Lrc_ret:
 ret
.size pg_read_tool_close, .-pg_read_tool_close

/* ---- pg_tool_definition pg_read_tool_definition(pg_read_tool *r)
 * 56 字节聚合：sret，rdi=目标缓冲 rsi=r。 ---- */
.globl pg_read_tool_definition
.type pg_read_tool_definition,@function
pg_read_tool_definition:
 lea rax, [rip + .Lread_name]
 mov qword ptr [rdi + PG_TL_DEF_NAME], rax
 lea rax, [rip + .Lread_desc]
 mov qword ptr [rdi + PG_TL_DEF_DESCRIPTION], rax
 lea rax, [rip + .Lread_schema]
 mov qword ptr [rdi + PG_TL_DEF_SCHEMA], rax
 mov qword ptr [rdi + PG_TL_DEF_CONTEXT], rsi
 lea rax, [rip + li_validate]
 mov qword ptr [rdi + PG_TL_DEF_VALIDATE], rax
 lea rax, [rip + li_authorize]
 mov qword ptr [rdi + PG_TL_DEF_AUTHORIZE], rax
 lea rax, [rip + li_execute_read]
 mov qword ptr [rdi + PG_TL_DEF_EXECUTE], rax
 mov rax, rdi
 ret
.size pg_read_tool_definition, .-pg_read_tool_definition

.section .note.GNU-stack,"",@progbits
