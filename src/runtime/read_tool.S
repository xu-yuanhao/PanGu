/* POSIX read backend: caller-owned cwd fd; bounded UTF-8 line output.
   openat resolves relative/absolute paths, dot segments and symbolic links.
   The opened descriptor is checked as a regular file before reading. */
#include "read_tool_layout.h"
#include "tools_layout.h"
.intel_syntax noprefix

.section .rodata
.Lsnapshot_schema: .asciz "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"],\"additionalProperties\":false}"
.Lsnapshot_desc: .asciz "Read a complete bounded UTF-8 file snapshot for an internal file consumer."
.Lstr_path:      .asciz "path"
.Lstr_offset: .asciz "offset"
.Lstr_line_limit: .asciz "limit"
.Lread_more: .asciz "\n\n[read: more lines; use offset=%llu to continue]"
.Lmsg_offset: .asciz "read offset is beyond end of file"
.Lutf8_bom: .byte 0xef,0xbb,0xbf
.Lread_name:     .asciz "read"
.Lread_desc: .asciz "Read text files and inline PNG, JPEG, GIF or WebP images using a relative or absolute path. Optional offset is a 1-based line and limit is a positive line count. Output is bounded to 2000 lines and 50 KiB; follow the next offset when truncated. Paths support home expansion, file URLs and lexical dot segments before POSIX resolution."
.Lread_schema: .asciz "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"offset\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":2147483647},\"limit\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":2147483647}},\"required\":[\"path\"],\"additionalProperties\":false}"
.Lmsg_invalid:   .asciz "invalid read path"
.Lmsg_root:      .asciz "read root unavailable"
.Lmsg_refused:   .asciz "read refused or unavailable"
.Lmsg_limit:     .asciz "read output limit exceeded"
.Lmsg_utf8:      .asciz "invalid UTF-8 file is unsupported"
.Lmsg_binary:    .asciz "binary file is unsupported"

.section .text

/* Strict JSON path/offset/limit decoding into caller storage; embedded NUL
   is refused and the byte length is bounded before any filesystem call. */
.type li_parse_path,@function
li_parse_path:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,4096
 test byte ptr [rsp],0
 sub rsp,48
 mov [rsp+4112],rcx
 mov [rsp+4128],r8d
 mov qword ptr [rcx],1
 mov qword ptr [rcx+8],PG_RTL_PAGE_LINES
 mov qword ptr [rcx+16],0
 mov qword ptr [rsp+4120],1
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
 xor ecx,ecx
.Lpp_escapes:
 cmp rcx,r15
 jae .Lpp_copy_json
 cmp byte ptr [r13+rcx],92
 jne .Lpp_escape_next
 lea rax,[rcx+5]
 cmp rax,r15
 jae .Lpp_escape_skip
 cmp byte ptr [r13+rcx+1],'u'
 jne .Lpp_escape_skip
 cmp dword ptr [r13+rcx+2],0x30303030
 je .Lpp_bad
.Lpp_escape_skip:
 inc rcx
.Lpp_escape_next:
 inc rcx
 jmp .Lpp_escapes
.Lpp_copy_json:
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
 cmp eax,1
 jb .Lpp_bad_put
 cmp eax,3
 ja .Lpp_bad_put
 mov [rsp+4128],rax
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
 mov rdi, rax
 xor esi, esi
 mov rdx, r12
 call memchr@PLT
 test rax, rax
 jnz .Lpp_bad_put
 mov rdi,[rsp+4104]
 mov rsi,r12
 mov rdx,rbx
 mov ecx,4096
 cmp dword ptr [rsp+4128],0
 jne .Lpp_resolve
 call pg_tool_path_prepare@PLT
 jmp .Lpp_prepared
.Lpp_resolve:
 call pg_tool_path_resolve@PLT
.Lpp_prepared:
 test eax,eax
 jnz .Lpp_bad_put
 jmp .Lpp_ok
.Lpp_bad:
 mov eax, 1 /* 无对象需要释放 */
 jmp .Lpp_ret
.Lpp_bad_put:
 mov rdi, r14
 call json_object_put@PLT
 mov eax, 1
 jmp .Lpp_ret
.Lpp_ok:
 xor r13d,r13d
.Lpp_option:
 lea rsi,[rip+.Lstr_offset]
 test r13,r13
 jz .Lpp_option_get
 lea rsi,[rip+.Lstr_line_limit]
.Lpp_option_get:
 mov rdi,r14
 lea rdx,[rsp+4096]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lpp_option_next
 inc qword ptr [rsp+4120]
 mov rcx,[rsp+4112]
 or qword ptr [rcx+16],2
 mov rdi,[rsp+4096]
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lpp_bad_put
 mov rdi,[rsp+4096]
 call json_object_get_int64@PLT
 cmp rax,1
 jl .Lpp_bad_put
 cmp rax,2147483647
 ja .Lpp_bad_put
 mov rcx,[rsp+4112]
 mov [rcx+r13*8],rax
 test r13,r13
 jz .Lpp_option_next
 or qword ptr [rcx+16],1
 cmp rax,PG_RTL_PAGE_LINES
 jbe .Lpp_option_next
 mov qword ptr [rcx+8],PG_RTL_PAGE_LINES
 and qword ptr [rcx+16],-2
.Lpp_option_next:
 inc r13
 cmp r13,2
 jb .Lpp_option
 mov rax,[rsp+4120]
 cmp rax,[rsp+4128]
 jne .Lpp_bad_put
 mov rdi, r14
 call json_object_put@PLT
 xor eax, eax
.Lpp_ret:
 add rsp, 4144
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
 xor r9d,r9d
 jmp .Lvalidate_mode
.Lvalidate_snapshot:
 mov r9d,1
.Lvalidate_mode:
 push rbx
 push r12
 push rax
 sub rsp,4096
 test byte ptr [rsp],0
 sub rsp,32
 mov [rsp+4120],r9d
 mov r8, rdx
 mov rdx, rsp
 mov rdi, rsi
 mov rsi, r8
 lea rcx,[rsp+4096]
 mov r8d,[rsp+4120]
 call li_parse_path
 test eax,eax
 jnz .Lvalidate_return
 cmp dword ptr [rsp+4120],0
 je .Lvalidate_return
 test qword ptr [rsp+4112],2
 jz .Lvalidate_return
 mov eax,1
.Lvalidate_return:
 test eax, eax
 setz al
 movzx eax, al
 add rsp, 4128
 pop rcx
 pop r12
 pop rbx
 ret
.size li_validate, .-li_validate

/* ---- int li_authorize(void *ctx, args.data, args.size)：根 fd 有效即可 ---- */
.type li_authorize,@function
li_authorize:
 push rdi
 mov rdi,[rdi+PG_RTL_RESULT]
 call json_object_put@PLT
 pop rdi
 mov qword ptr [rdi+PG_RTL_RESULT],0
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
 mov byte ptr [rbx+r15],0
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
 * 栈帧4368：[0]=path/chunk[4096]，[4096]=stat，[4248]=path cursor，
 * [4256]=fd，[4264]=output bytes，[4272..4288]=options，
 * [4296..4336]=line/scan/chunk cursors。寄存器：rbx=ctx→part、r12=out、
 * r13=capacity、r14=size*、r15=dir fd。 ---- */
.type li_execute_read,@function
li_execute_read:
 xor r10d,r10d
 jmp .Lexecute_mode
.Lexecute_snapshot:
 mov r10d,1
.Lexecute_mode:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,4096
 test byte ptr [rsp],0
 sub rsp,272
 mov [rsp+4360],r10d
 mov rbx, rdi
 mov r12, rcx
 mov r13, r8
 cmp r13,PG_RTL_OUTPUT_MAX
 jbe .Lrd_capacity
 mov r13d,PG_RTL_OUTPUT_MAX
.Lrd_capacity:
 mov r14, r9
 mov [rsp+4240],rsi
 mov [rsp+4248],rdx
 mov rdi,[rbx+PG_RTL_RESULT]
 call json_object_put@PLT
 mov qword ptr [rbx+PG_RTL_RESULT],0
 mov rsi,[rsp+4240]
 mov rdx,[rsp+4248]
 cmp r13, 2
 jb .Lrd_invalid
 mov r8, rdx
 mov rdx, rsp
 mov rdi, rsi
 mov rsi, r8
 lea rcx,[rsp+4272]
 mov r8d,[rsp+4360]
 call li_parse_path
 test eax,eax
 jnz .Lrd_invalid
 cmp dword ptr [rsp+4360],0
 je .Lrd_open_root
 test qword ptr [rsp+4288],2
 jnz .Lrd_invalid
.Lrd_open_root:
 mov edi, dword ptr [rbx + PG_RTL_DIRECTORY]
 mov esi, PG_RTL_F_DUPFD_CLOEXEC
 xor edx, edx
 xor eax, eax
 call fcntl@PLT
 mov r15, rax
 test r15, r15
 js .Lrd_root_gone
 mov qword ptr [rsp + 4256], -1
 mov edi,r15d
 mov rsi,rsp
 mov edx,0x80800 /* O_RDONLY|O_CLOEXEC|O_NONBLOCK */
 xor eax,eax
 call openat@PLT
 movsxd rax,eax
 mov [rsp+4256],rax
 mov edi,r15d
 call close@PLT
 mov r15,-1
 mov rcx,[rsp+4256]
 test rcx,rcx
 js .Lrd_refused
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
 cmp dword ptr [rsp+4360],0
 jne .Lrd_text_file
 mov rdi,rbx
 mov rsi,[rsp+4256]
 mov rdx,[rsp+4096+48]
 call .Lread_image
 cmp eax,-1
 je .Lrd_text_file
 mov [rsp+4344],eax
 mov rdi,[rsp+4256]
 call close@PLT
 mov qword ptr [r14],0
 mov byte ptr [r12],0
 mov eax,[rsp+4344]
 jmp .Lrd_out
.Lrd_text_file:
 /* Pi's TextDecoder omits a leading UTF-8 BOM for reads beginning at byte 0. */
 cmp dword ptr [rsp+4360],0
 jne .Lrd_bom_ready
.Lrd_bom_read:
 mov rdi,[rsp+4256]
 lea rsi,[rsp+4344]
 mov edx,3
 xor ecx,ecx
 xor eax,eax
 call pread@PLT
 test rax,rax
 jge .Lrd_bom_read_done
 call __errno_location@PLT
 cmp dword ptr [rax],4
 je .Lrd_bom_read
 jmp .Lrd_refused
.Lrd_bom_read_done:
 cmp rax,3
 jne .Lrd_bom_ready
 lea rsi,[rsp+4344]
 lea rdi,[rip+.Lutf8_bom]
 mov edx,3
 call memcmp@PLT
 test eax,eax
 jne .Lrd_bom_ready
.Lrd_bom_seek:
 mov rdi,[rsp+4256]
 mov esi,3
 xor edx,edx
 call lseek@PLT
 cmp rax,-1
 jne .Lrd_bom_ready
 call __errno_location@PLT
 cmp dword ptr [rax],4
 je .Lrd_bom_seek
 jmp .Lrd_refused
.Lrd_bom_ready:
 mov qword ptr [rsp+4344],0 /* final complete/partial status */
 cmp qword ptr [rsp+4272],1
 je .Lrd_initial_status
 mov qword ptr [rsp+4344],4
.Lrd_initial_status:
 mov qword ptr [rsp+4264],0 /* output bytes */
 mov qword ptr [rsp+4296],1 /* current source line */
 mov qword ptr [rsp+4304],0 /* complete output lines */
 mov qword ptr [rsp+4312],0 /* last full-line output boundary */
 mov qword ptr [rsp+4320],0 /* bytes scanned */
 mov qword ptr [rsp+4328],0 /* chunk length */
 mov qword ptr [rsp+4336],0 /* chunk cursor */
.Lrd_next_byte:
 mov rax,[rsp+4336]
 cmp rax,[rsp+4328]
 jb .Lrd_have_byte
 call .Lrd_chunk
 test rax,rax
 js .Lrd_chunk_error
 jz .Lrd_eof
 xor eax,eax
.Lrd_have_byte:
 movzx ecx,byte ptr [rsp+rax]
 inc qword ptr [rsp+4336]
 mov rdx,[rsp+4296]
 cmp rdx,[rsp+4272]
 jb .Lrd_skip_byte
 test cl,cl
 jz .Lrd_binary_close
 mov rax,[rsp+4264]
 lea rdx,[r13-1]
 cmp rax,rdx
 jae .Lrd_page_full
 mov [r12+rax],cl
 inc qword ptr [rsp+4264]
 cmp cl,10
 jne .Lrd_next_byte
 inc qword ptr [rsp+4296]
 inc qword ptr [rsp+4304]
 mov rax,[rsp+4264]
 mov [rsp+4312],rax
 mov rax,[rsp+4304]
 cmp dword ptr [rsp+4360],0
 jne .Lrd_next_byte
 cmp rax,[rsp+4280]
 jb .Lrd_next_byte
 mov rax,[rsp+4336]
 cmp rax,[rsp+4328]
 jb .Lrd_more
 call .Lrd_chunk
 test rax,rax
 js .Lrd_chunk_error
 jnz .Lrd_more
 test qword ptr [rsp+4288],1
 jz .Lrd_success
 dec qword ptr [rsp+4264]
 mov qword ptr [rsp+4344],4
 jmp .Lrd_success
.Lrd_skip_byte:
 cmp cl,10
 jne .Lrd_next_byte
 inc qword ptr [rsp+4296]
 jmp .Lrd_next_byte
.Lrd_eof:
 mov rax,[rsp+4296]
 cmp rax,[rsp+4272]
 jb .Lrd_offset_close
 jmp .Lrd_success
.Lrd_page_full:
 cmp dword ptr [rsp+4360],0
 jne .Lrd_limit
 mov rax,[rsp+4312]
 test rax,rax
 jz .Lrd_limit
 mov [rsp+4264],rax
.Lrd_more:
 mov qword ptr [rsp+4344],4
.Lrd_reserve:
 cmp qword ptr [rsp+4304],0
 je .Lrd_limit
 lea rdi,[rsp+4096]
 mov esi,96
 lea rdx,[rip+.Lread_more]
 mov rcx,[rsp+4272]
 add rcx,[rsp+4304]
 xor eax,eax
 call snprintf@PLT
 test eax,eax
 js .Lrd_limit
 cmp eax,96
 jae .Lrd_limit
 cdqe
 mov [rsp+4352],rax
 add rax,[rsp+4264]
 dec rax
 cmp rax,r13
 jb .Lrd_hint
 dec qword ptr [rsp+4304]
 mov rax,[rsp+4264]
 dec rax
.Lrd_back_line:
 test rax,rax
 jz .Lrd_limit
 dec rax
 cmp byte ptr [r12+rax],10
 jne .Lrd_back_line
 inc rax
 mov [rsp+4264],rax
 jmp .Lrd_reserve
.Lrd_hint:
 dec qword ptr [rsp+4264]
 mov rax,[rsp+4264]
 lea rdi,[r12+rax]
 lea rsi,[rsp+4096]
 mov rdx,[rsp+4352]
 call memcpy@PLT
 mov rax,[rsp+4352]
 add [rsp+4264],rax
.Lrd_success:
 mov rdi,[rsp+4256]
 call close@PLT
 mov qword ptr [rsp+4256],-1
 mov rdi,r12
 mov rsi,[rsp+4264]
 call li_utf8
 test eax,eax
 jz .Lrd_utf8_bad
 mov rax,[rsp+4264]
 mov byte ptr [r12+rax],0
 mov [r14],rax
 mov eax,[rsp+4344]
 jmp .Lrd_out
.Lrd_binary_close:
 lea rbx,[rip+.Lmsg_binary]
 jmp .Lrd_close_message
.Lrd_offset_close:
 lea rbx,[rip+.Lmsg_offset]
 jmp .Lrd_close_message
.Lrd_chunk_error:
 jmp .Lrd_refused
.Lrd_close_message:
 mov rdi,[rsp+4256]
 call close@PLT
 mov qword ptr [rsp+4256],-1
 mov rdi,r12
 mov rsi,r13
 mov rdx,r14
 mov rcx,rbx
 call li_fail
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
 add rsp, 4368
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret

.Lrd_chunk:
 push rbx
 mov rbx,rsp
 mov edx,4096
.Lrd_chunk_size:
 mov rdi,[rbx+16+4256]
 lea rsi,[rbx+16]
 call read@PLT
 test rax,rax
 js .Lrd_chunk_return
 add [rbx+16+4320],rax
 mov [rbx+16+4328],rax
 mov qword ptr [rbx+16+4336],0
 jmp .Lrd_chunk_return
.Lrd_chunk_return:
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
 mov qword ptr [rbx+PG_RTL_RESULT],0
 mov qword ptr [rbx+PG_RTL_IMAGES],0
 mov qword ptr [rbx+PG_RTL_IMAGE_BYTES],4718592
 mov dword ptr [rbx+PG_RTL_IMAGE_WIDTH],2000
 mov dword ptr [rbx+PG_RTL_IMAGE_HEIGHT],2000
 mov dword ptr [rbx+PG_RTL_IMAGE_VISION],1
 mov dword ptr [rbx+44],0
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
 * 64 字节聚合：sret，rdi=目标缓冲 rsi=r。 ---- */
.globl pg_read_tool_definition
.type pg_read_tool_definition,@function
pg_read_tool_definition:
 lea rax, [rip + .Lread_name]
 mov qword ptr [rdi + PG_TL_DEF_NAME], rax
 lea rax, [rip + .Lread_desc]
 cmp qword ptr [rsi+PG_RTL_IMAGES],0
 je .Lread_description_ready
 lea rax,[rip+.Lread_image_desc]
.Lread_description_ready:
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
 mov qword ptr [rdi+PG_TL_DEF_REPLAY_SAFE],1
 mov qword ptr [rdi+PG_TL_DEF_ASYNC],0
 mov qword ptr [rdi+PG_TL_DEF_OUTPUT_LIMITS],0
 mov rax, rdi
 ret
.size pg_read_tool_definition, .-pg_read_tool_definition

.globl pg_read_snapshot_definition
.type pg_read_snapshot_definition,@function
pg_read_snapshot_definition:
 push rbx
 mov rbx,rdi
 call pg_read_tool_definition
 lea rax,[rip+.Lsnapshot_schema]
 mov [rbx+PG_TL_DEF_SCHEMA],rax
 lea rax,[rip+.Lsnapshot_desc]
 mov [rbx+PG_TL_DEF_DESCRIPTION],rax
 lea rax,[rip+.Lvalidate_snapshot]
 mov [rbx+PG_TL_DEF_VALIDATE],rax
 lea rax,[rip+.Lexecute_snapshot]
 mov [rbx+PG_TL_DEF_EXECUTE],rax
 mov rax,rbx
 pop rbx
 ret
.size pg_read_snapshot_definition,.-pg_read_snapshot_definition

.section .note.GNU-stack,"",@progbits

/* Pi Durable tools/read.ts and tools/image.ts, d5b3de22; MIT.
   The opened regular file stays owned by the caller. */
.macro RI_ENTER
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,392
.endm
.macro RI_LEAVE
 add rsp,392
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.text
.globl pg_read_tool_snapshot
.type pg_read_tool_snapshot,@function
pg_read_tool_snapshot:
 test rdi,rdi
 jz .Lris_bad
 test rsi,rsi
 jz .Lris_bad
 push rbx
 mov rbx,rsi
 mov qword ptr [rsi],0
 mov rdi,[rdi+PG_RTL_RESULT]
 test rdi,rdi
 jz .Lris_empty
 xor edx,edx
 call json_object_deep_copy@PLT
 pop rbx
 ret
.Lris_empty:
 mov qword ptr [rbx],0
 xor eax,eax
 pop rbx
 ret
.Lris_bad:
 mov eax,5
 ret
.Lread_image:
 RI_ENTER
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
 xor ebp,ebp
 mov qword ptr [rsp+360],0
 mov qword ptr [rsp+384],0
 mov qword ptr [rsp+344],0
 mov edi,r12d
 mov rsi,rsp
 mov edx,32
 xor ecx,ecx
 call pread@PLT
 test rax,rax
 js .Lrim_not_image
 mov rdi,rsp
 mov rsi,rax
 call .Lrim_format
 test eax,eax
 jz .Lrim_not_image
 cmp qword ptr [rbx+PG_RTL_IMAGES],0
 jne .Lrim_size_ready
 cmp eax,5
 je .Lrim_unsupported
 call .Lrim_size_fits
 test eax,eax
 jz .Lrim_unsupported
.Lrim_size_ready:
 mov dword ptr [rsp+336],0
.Lrim_attempt:
 mov edi,r12d
 lea rsi,[rsp+32]
 call fstat@PLT
 test eax,eax
 jnz .Lrim_unsupported
 mov r13,[rsp+32+48]
 cmp qword ptr [rbx+PG_RTL_IMAGES],0
 jne .Lrim_allocate
 call .Lrim_size_fits
 test eax,eax
 jz .Lrim_unsupported
.Lrim_allocate:
 lea rdi,[r13+1]
 test rdi,rdi
 jz .Lrim_unsupported
 call malloc@PLT
 mov r14,rax
 test rax,rax
 jz .Lrim_unsupported
 xor r15d,r15d
.Lrim_read:
 cmp r15,r13
 jae .Lrim_check
 mov edi,r12d
 lea rsi,[r14+r15]
 mov rdx,r13
 sub rdx,r15
 mov rcx,r15
 call pread@PLT
 test rax,rax
 jle .Lrim_unsupported
 add r15,rax
 jmp .Lrim_read
.Lrim_check:
 mov edi,r12d
 lea rsi,[rsp+176]
 call fstat@PLT
 test eax,eax
 jnz .Lrim_unsupported
 mov rax,[rsp+32+48]
 cmp rax,[rsp+176+48]
 jne .Lrim_changed
 mov rax,[rsp+32+88]
 cmp rax,[rsp+176+88]
 jne .Lrim_changed
 mov rax,[rsp+32+96]
 cmp rax,[rsp+176+96]
 jne .Lrim_changed
 mov rdi,r14
 mov rsi,r13
 call .Lrim_format
 test eax,eax
 jz .Lrim_unsupported
 mov [rsp+376],eax
 cmp qword ptr [rbx+PG_RTL_IMAGES],0
 jne .Lrim_process
 cmp eax,5
 je .Lrim_unsupported
 test edx,edx
 jz .Lrim_unsupported
 test ecx,ecx
 jz .Lrim_unsupported
 cmp edx,[rbx+PG_RTL_IMAGE_WIDTH]
 ja .Lrim_unsupported
 cmp ecx,[rbx+PG_RTL_IMAGE_HEIGHT]
 ja .Lrim_unsupported
 lea rdi,[rip+.Lrim_mimes]
 dec eax
 movsxd r15,dword ptr [rdi+rax*4]
 add r15,rdi
 lea rax,[r13+2]
 xor edx,edx
 mov ecx,3
 div rcx
 shl rax,2
 mov [rsp+328],rax
 lea rdi,[rax+1]
 call malloc@PLT
 mov rbp,rax
 test rax,rax
 jz .Lrim_unsupported
 mov rdi,r14
 mov rsi,r13
 mov rdx,rbp
 call pg_image_base64
 jmp .Lrim_build
.Lrim_process:
 lea rdi,[rip+.Lrim_mimes]
 mov eax,[rsp+376]
 dec eax
 movsxd rcx,dword ptr [rdi+rax*4]
 add rcx,rdi
 mov rax,[rbx+PG_RTL_IMAGES]
 mov rdi,[rax]
 mov rsi,r14
 mov rdx,r13
 lea r8,[rbx+PG_RTL_IMAGE_LIMITS]
 lea r9,[rsp+384]
 call [rax+8]
 test eax,eax
 jnz .Lrim_unsupported
 mov rdi,[rsp+384]
 lea rsi,[rip+.Lrim_data]
 call json_object_object_get@PLT
 mov [rsp+320],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lrim_unsupported
 mov rdi,[rsp+320]
 call json_object_get_string_len@PLT
 cmp rax,[rbx+PG_RTL_IMAGE_BYTES]
 ja .Lrim_unsupported
 mov rdi,[rsp+384]
 lea rsi,[rip+.Lrim_mime]
 call json_object_object_get@PLT
 mov [rsp+328],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lrim_unsupported
 mov rdi,[rsp+328]
 call json_object_get_string@PLT
 mov r15,rax
.Lrim_build:
 mov edi,r12d
 lea rsi,[rsp+176]
 call fstat@PLT
 test eax,eax
 jnz .Lrim_unsupported
 mov rax,[rsp+32+48]
 cmp rax,[rsp+176+48]
 jne .Lrim_changed
 mov rax,[rsp+32+88]
 cmp rax,[rsp+176+88]
 jne .Lrim_changed
 mov rax,[rsp+32+96]
 cmp rax,[rsp+176+96]
 jne .Lrim_changed
 lea rdi,[rip+.Lrim_success]
 call json_tokener_parse@PLT
 mov [rsp+360],rax
 test rax,rax
 jz .Lrim_unsupported
 mov rdi,rax
 lea rsi,[rip+.Lrim_output]
 call json_object_object_get@PLT
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov [rsp+368],rax
 cmp qword ptr [rsp+384],0
 jne .Lrim_prepared_data
 mov rdi,rbp
 mov esi,[rsp+328]
 call json_object_new_string_len@PLT
 jmp .Lrim_data_ready
.Lrim_prepared_data:
 mov rdi,[rsp+320]
 call json_object_get@PLT
.Lrim_data_ready:
 mov rdx,rax
 mov rdi,[rsp+368]
 lea rsi,[rip+.Lrim_data]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lrim_unsupported
 mov rdi,r15
 call json_object_new_string@PLT
 mov rdx,rax
 mov rdi,[rsp+368]
 lea rsi,[rip+.Lrim_mime]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lrim_unsupported
 lea rdi,[rsp+344]
 lea rsi,[rip+.Lrim_note_format]
 mov rdx,r15
 lea rcx,[rip+.Lrim_no_note]
 cmp dword ptr [rbx+PG_RTL_IMAGE_VISION],0
 jne .Lrim_note_ready
 lea rcx,[rip+.Lrim_nonvision]
.Lrim_note_ready:
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lrim_unsupported
 cmp qword ptr [rsp+384],0
 je .Lrim_notes_done
 mov rdi,[rsp+384]
 lea rsi,[rip+.Lrim_converted]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lrim_resize_note
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lrim_unsupported
 mov rcx,rax
 mov r12,[rsp+344]
 mov qword ptr [rsp+344],0
 lea rdi,[rsp+344]
 lea rsi,[rip+.Lrim_converted_format]
 mov rdx,r12
 mov r8,r15
 xor eax,eax
 call asprintf@PLT
 mov [rsp+376],eax
 mov rdi,r12
 call free@PLT
 cmp dword ptr [rsp+376],0
 jl .Lrim_unsupported
.Lrim_resize_note:
 mov rdi,[rsp+384]
 lea rsi,[rip+.Lrim_resized]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lrim_notes_done
 mov r12,rax
 mov rdi,rax
 lea rsi,[rip+.Lrim_from]
 call json_object_object_get@PLT
 mov r15,rax
 mov rdi,rax
 lea rsi,[rip+.Lrim_width]
 call .Lrim_dimension
 test rax,rax
 jz .Lrim_unsupported
 mov [rsp],rax
 mov rdi,r15
 lea rsi,[rip+.Lrim_height]
 call .Lrim_dimension
 test rax,rax
 jz .Lrim_unsupported
 mov [rsp+8],rax
 mov rdi,r12
 lea rsi,[rip+.Lrim_to]
 call json_object_object_get@PLT
 mov r15,rax
 mov rdi,rax
 lea rsi,[rip+.Lrim_width]
 call .Lrim_dimension
 test rax,rax
 jz .Lrim_unsupported
 mov [rsp+16],rax
 mov rdi,r15
 lea rsi,[rip+.Lrim_height]
 call .Lrim_dimension
 test rax,rax
 jz .Lrim_unsupported
 mov [rsp+24],rax
 mov r12,[rsp+344]
 mov qword ptr [rsp+344],0
 cvtsi2sd xmm0,qword ptr [rsp]
 cvtsi2sd xmm1,qword ptr [rsp+16]
 divsd xmm0,xmm1
 lea rdi,[rsp+344]
 lea rsi,[rip+.Lrim_resized_format]
 mov rdx,r12
 mov rcx,[rsp]
 mov r8,[rsp+8]
 mov r9,[rsp+16]
 mov rax,[rsp+24]
 mov [rsp],rax
 mov eax,1
 call asprintf@PLT
 mov [rsp+376],eax
 mov rdi,r12
 call free@PLT
 cmp dword ptr [rsp+376],0
 jl .Lrim_unsupported
.Lrim_notes_done:
 mov rdi,[rsp+344]
 call json_object_new_string@PLT
 mov [rsp+368],rax
 mov rdi,[rsp+360]
 lea rsi,[rip+.Lrim_diagnostics]
 call json_object_object_get@PLT
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rip+.Lrim_message]
 mov rdx,[rsp+368]
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lrim_unsupported
 mov rax,[rsp+360]
 mov [rbx+PG_RTL_RESULT],rax
 xor eax,eax
 mov [rsp+352],eax
 jmp .Lrim_finish
.Lrim_changed:
 mov rdi,[rsp+384]
 call json_object_put@PLT
 mov qword ptr [rsp+384],0
 mov rdi,rbp
 call free@PLT
 xor ebp,ebp
 mov rdi,r14
 call free@PLT
 xor r14d,r14d
 inc dword ptr [rsp+336]
 cmp dword ptr [rsp+336],1
 jbe .Lrim_attempt
.Lrim_unsupported:
 mov rdi,[rsp+360]
 call json_object_put@PLT
 lea rdi,[rip+.Lrim_failure]
 call json_tokener_parse@PLT
 mov [rbx+PG_RTL_RESULT],rax
 mov dword ptr [rsp+352],1
 jmp .Lrim_finish
.Lrim_not_image:
 mov dword ptr [rsp+352],-1
.Lrim_finish:
 mov rdi,[rsp+344]
 call free@PLT
 mov rdi,[rsp+384]
 call json_object_put@PLT
 mov rdi,r14
 call free@PLT
 mov rdi,rbp
 call free@PLT
 mov eax,[rsp+352]
 RI_LEAVE
/* Returns the recognized inline format and header dimensions. */
.Lrim_format:
 xor eax,eax
 xor edx,edx
 xor ecx,ecx
 cmp rsi,3
 jb .Lrif_end
 cmp word ptr [rdi],0xd8ff
 jne .Lrif_png
 cmp byte ptr [rdi+2],0xff
 jne .Lrif_png
 cmp rsi,4
 jb .Lrif_end
 cmp byte ptr [rdi+3],0xf7
 je .Lrif_end
 mov eax,2
 mov r8d,2
.Lrif_jpeg:
 lea r9,[r8+9]
 cmp r9,rsi
 ja .Lrif_end
 cmp byte ptr [rdi+r8],0xff
 jne .Lrif_end
 movzx r9d,byte ptr [rdi+r8+1]
 cmp r9d,0xff
 jne .Lrif_marker
 inc r8
 jmp .Lrif_jpeg
.Lrif_marker:
 cmp r9d,0xc0
 jb .Lrif_segment
 cmp r9d,0xcf
 ja .Lrif_segment
 cmp r9d,0xc4
 je .Lrif_segment
 cmp r9d,0xc8
 je .Lrif_segment
 cmp r9d,0xcc
 je .Lrif_segment
 movzx edx,word ptr [rdi+r8+7]
 rol dx,8
 movzx ecx,word ptr [rdi+r8+5]
 rol cx,8
 ret
.Lrif_segment:
 movzx r9d,word ptr [rdi+r8+2]
 rol r9w,8
 lea r8,[r8+r9+2]
 jmp .Lrif_jpeg
.Lrif_png:
 cmp rsi,16
 jb .Lrif_gif
 mov r8,0x0a1a0a0d474e5089
 cmp [rdi],r8
 jne .Lrif_gif
 cmp dword ptr [rdi+8],0x0d000000
 jne .Lrif_end
 cmp dword ptr [rdi+12],0x52444849
 jne .Lrif_end
 mov eax,1
 cmp rsi,24
 jb .Lrif_end
 mov edx,[rdi+16]
 bswap edx
 mov ecx,[rdi+20]
 bswap ecx
 mov r8d,8
.Lrif_png_chunk:
 lea r9,[r8+8]
 cmp r9,rsi
 ja .Lrif_end
 cmp dword ptr [rdi+r8+4],0x4c546361
 je .Lrif_unrecognized
 cmp dword ptr [rdi+r8+4],0x54414449
 je .Lrif_end
 mov r9d,[rdi+r8]
 bswap r9d
 lea r8,[r8+r9+12]
 cmp r8,rsi
 jbe .Lrif_png_chunk
 ret
.Lrif_gif:
 cmp rsi,6
 jb .Lrif_bmp
 cmp dword ptr [rdi],0x38464947
 jne .Lrif_webp
 cmp word ptr [rdi+4],0x6137
 je .Lrif_gif_found
 cmp word ptr [rdi+4],0x6139
 jne .Lrif_webp
.Lrif_gif_found:
 mov eax,3
 cmp rsi,10
 jb .Lrif_end
 movzx edx,word ptr [rdi+6]
 movzx ecx,word ptr [rdi+8]
 ret
.Lrif_webp:
 cmp rsi,12
 jb .Lrif_bmp
 cmp dword ptr [rdi],0x46464952
 jne .Lrif_bmp
 cmp dword ptr [rdi+8],0x50424557
 jne .Lrif_bmp
 mov eax,4
 cmp rsi,30
 jb .Lrif_end
 cmp dword ptr [rdi+12],0x20385056
 jne .Lrif_lossless
 movzx edx,word ptr [rdi+26]
 and edx,0x3fff
 movzx ecx,word ptr [rdi+28]
 and ecx,0x3fff
 ret
.Lrif_lossless:
 cmp dword ptr [rdi+12],0x4c385056
 jne .Lrif_extended
 mov edx,[rdi+21]
 mov ecx,edx
 and edx,0x3fff
 inc edx
 shr ecx,14
 and ecx,0x3fff
 inc ecx
 ret
.Lrif_extended:
 cmp dword ptr [rdi+12],0x58385056
 jne .Lrif_end
 mov edx,[rdi+24]
 and edx,0xffffff
 inc edx
 mov ecx,[rdi+26]
 shr ecx,8
 inc ecx
 ret
.Lrif_bmp:
 cmp word ptr [rdi],0x4d42
 jne .Lrif_unrecognized
 mov eax,5
 ret
.Lrif_unrecognized:
 xor eax,eax
.Lrif_end:
 ret
.globl pg_image_base64
.type pg_image_base64,@function
pg_image_base64:
 xor r8d,r8d
 xor r9d,r9d
 lea r10,[rip+.Lrim_alphabet]
.Lrib_loop:
 cmp r8,rsi
 jae .Lrib_done
 movzx eax,byte ptr [rdi+r8]
 shl eax,16
 lea rcx,[r8+1]
 cmp rcx,rsi
 jae .Lrib_encode
 movzx ecx,byte ptr [rdi+r8+1]
 shl ecx,8
 or eax,ecx
 lea rcx,[r8+2]
 cmp rcx,rsi
 jae .Lrib_encode
 movzx ecx,byte ptr [rdi+r8+2]
 or eax,ecx
.Lrib_encode:
 mov ecx,eax
 shr ecx,18
 mov cl,[r10+rcx]
 mov [rdx+r9],cl
 mov ecx,eax
 shr ecx,12
 and ecx,63
 mov cl,[r10+rcx]
 mov [rdx+r9+1],cl
 mov byte ptr [rdx+r9+2],'='
 mov byte ptr [rdx+r9+3],'='
 lea rcx,[r8+1]
 cmp rcx,rsi
 jae .Lrib_next
 mov ecx,eax
 shr ecx,6
 and ecx,63
 mov cl,[r10+rcx]
 mov [rdx+r9+2],cl
 lea rcx,[r8+2]
 cmp rcx,rsi
 jae .Lrib_next
 and eax,63
 mov al,[r10+rax]
 mov [rdx+r9+3],al
.Lrib_next:
 add r8,3
 add r9,4
 jmp .Lrib_loop
.Lrib_done:
 mov byte ptr [rdx+r9],0
 ret
.section .rodata
.Lrim_mimes:
 .long .Lrim_png-.Lrim_mimes,.Lrim_jpeg-.Lrim_mimes,.Lrim_gif-.Lrim_mimes,.Lrim_webp-.Lrim_mimes,.Lrim_bmp-.Lrim_mimes
.Lrim_png: .asciz "image/png"
.Lrim_jpeg: .asciz "image/jpeg"
.Lrim_gif: .asciz "image/gif"
.Lrim_webp: .asciz "image/webp"
.Lrim_alphabet: .asciz "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
.Lrim_output: .asciz "output"
.Lrim_data: .asciz "data"
.Lrim_mime: .asciz "mimeType"
.Lrim_success: .asciz "{\"output\":[{\"type\":\"image\"}],\"diagnostics\":[{\"severity\":\"info\",\"code\":\"image\",\"message\":\"Read image file.\"}]}"
.Lrim_failure: .asciz "{\"output\":[],\"diagnostics\":[{\"severity\":\"error\",\"code\":\"unsupported_image\",\"message\":\"Image cannot be sent within inline format, size and dimension limits without an image processor, or changed while being read.\"}]}"

.Lrim_bmp: .asciz "image/bmp"

.text
.Lrim_size_fits:
 lea rax,[r13+2]
 cmp rax,r13
 jb .Lrim_size_no
 xor edx,edx
 mov ecx,3
 div rcx
 mov r11,rax
 shr rax,62
 jnz .Lrim_size_no
 mov rax,r11
 shl rax,2
 cmp rax,[rbx+PG_RTL_IMAGE_BYTES]
 seta al
 xor al,1
 movzx eax,al
 ret
.Lrim_size_no:
 xor eax,eax
 ret

.text
.globl pg_read_tool_configure_images
.type pg_read_tool_configure_images,@function
pg_read_tool_configure_images:
 RI_ENTER
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 test rbx,rbx
 jz .Lricfg_bad
 test r13,r13
 jz .Lricfg_defaults
 cmp qword ptr [r13+8],0
 je .Lricfg_bad
.Lricfg_defaults:
 mov qword ptr [rsp],4718592
 mov dword ptr [rsp+8],2000
 mov dword ptr [rsp+12],2000
 mov dword ptr [rsp+16],1
 mov rdi,r12
 lea rsi,[rip+.Lricfg_model]
 call json_object_object_get@PLT
 test rax,rax
 cmovnz r12,rax
 mov rdi,r12
 lea rsi,[rip+.Lricfg_input]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lricfg_limits
 mov r14,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lricfg_bad
 mov dword ptr [rsp+16],0
 xor r15d,r15d
.Lricfg_input_each:
 mov rdi,r14
 call json_object_array_length@PLT
 cmp r15,rax
 jae .Lricfg_limits
 mov rdi,r14
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lricfg_bad
 mov rdi,rax
 lea rsi,[rip+.Lricfg_image]
 call strcmp@PLT
 test eax,eax
 jnz .Lricfg_input_next
 mov dword ptr [rsp+16],1
.Lricfg_input_next:
 inc r15
 jmp .Lricfg_input_each
.Lricfg_limits:
 mov rdi,r12
 lea rsi,[rip+.Lricfg_limits_key]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lricfg_images]
 call json_object_object_get@PLT
 mov rdi,rax
 lea rsi,[rip+.Lricfg_resize]
 call json_object_object_get@PLT
 mov r12,rax
 test rax,rax
 jz .Lricfg_commit
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lricfg_bad
 xor r14d,r14d
.Lricfg_limit_each:
 lea rax,[rip+.Lricfg_keys]
 movsxd rsi,dword ptr [rax+r14*4]
 add rsi,rax
 mov rdi,r12
 call json_object_object_get@PLT
 test rax,rax
 jz .Lricfg_limit_next
 mov r15,rax
 mov rdi,rax
 mov esi,3
 call json_object_is_type@PLT
 test eax,eax
 jz .Lricfg_bad
 mov rdi,r15
 call json_object_get_int64@PLT
 test rax,rax
 jle .Lricfg_bad
 test r14,r14
 jnz .Lricfg_dimension
 mov rdx,0x3fffffffffffffff
 cmp rax,rdx
 ja .Lricfg_bad
 mov [rsp],rax
 jmp .Lricfg_limit_next
.Lricfg_dimension:
 mov edx,0xffffffff
 cmp rax,rdx
 ja .Lricfg_bad
 mov [rsp+r14*4+4],eax
.Lricfg_limit_next:
 inc r14
 cmp r14,3
 jb .Lricfg_limit_each
.Lricfg_commit:
 movdqu xmm0,[rsp]
 movdqu [rbx+PG_RTL_IMAGE_LIMITS],xmm0
 mov eax,[rsp+16]
 mov [rbx+PG_RTL_IMAGE_VISION],eax
 mov [rbx+PG_RTL_IMAGES],r13
 xor eax,eax
 RI_LEAVE
.Lricfg_bad:
 mov eax,5
 RI_LEAVE
.section .rodata
.Lricfg_model: .asciz "modelInfo"
.Lricfg_input: .asciz "input"
.Lricfg_image: .asciz "image"
.Lricfg_limits_key: .asciz "inputLimits"
.Lricfg_images: .asciz "images"
.Lricfg_resize: .asciz "resize"
.Lricfg_maxbytes: .asciz "maxBytes"
.Lricfg_width: .asciz "maxWidth"
.Lricfg_height: .asciz "maxHeight"
.Lricfg_keys: .long .Lricfg_maxbytes-.Lricfg_keys,.Lricfg_width-.Lricfg_keys,.Lricfg_height-.Lricfg_keys

.section .rodata
.Lrim_note_format: .asciz "Read image file [%s].%s"
.Lrim_no_note: .asciz ""
.Lrim_nonvision: .asciz " The current model does not support images; it sees a placeholder instead."
.Lrim_diagnostics: .asciz "diagnostics"
.Lrim_message: .asciz "message"

.text
.Lrim_dimension:
 sub rsp,8
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_int64@PLT
 test rax,rax
 jle .Lrim_dimension_bad
 mov edx,0xffffffff
 cmp rax,rdx
 ja .Lrim_dimension_bad
 add rsp,8
 ret
.Lrim_dimension_bad:
 xor eax,eax
 add rsp,8
 ret
.section .rodata
.Lrim_converted: .asciz "convertedFrom"
.Lrim_resized: .asciz "resized"
.Lrim_from: .asciz "from"
.Lrim_to: .asciz "to"
.Lrim_width: .asciz "width"
.Lrim_height: .asciz "height"
.Lrim_converted_format: .asciz "%s Converted from %s to %s."
.Lrim_resized_format: .asciz "%s Resized from %llux%llu to %llux%llu. Multiply coordinates by %.2f to map them to the original."

.section .rodata
.Lread_image_desc: .asciz "Read text files and image attachments (jpg, png, gif, webp, bmp). Images are decoded, oriented and prepared within model limits. Optional offset/limit select text lines; text output is bounded to 2000 lines and 50 KiB. Use the next offset to continue."
