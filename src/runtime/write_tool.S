#include "write_tool_layout.h"
#include "tools_layout.h"
.intel_syntax noprefix
.section .rodata
.Lname: .asciz "write"
.Ldescription: .asciz "Write UTF-8 content to a relative or absolute regular file, creating parent directories. Overwrites existing content. Paths support home expansion, file URLs and lexical dot segments before following links. An interrupted write may have partially run."
.Lschema: .asciz "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"}},\"required\":[\"path\",\"content\"],\"additionalProperties\":false}"
.Lpath: .asciz "path"
.Lcontent: .asciz "content"
.Lok: .asciz "Successfully wrote to %s"
.Lfailed: .asciz "Write failed; file or parent directories may have changed"
.section .text

/* Parse detached bounded arguments. Content is returned as owned bytes. */
.type .Lparse,@function
.Lparse:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,64
 mov rbx,rdi
 mov r12,rsi
 mov [rsp+32],rdx
 mov [rsp+40],rcx
 mov [rsp+48],r8
 mov [rsp+56],r9d
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+24],0
 mov rax,[rsp+40]
 mov qword ptr [rax],0
 mov rax,[rsp+48]
 mov qword ptr [rax],0
 test rbx,rbx
 jz .Lparse_bad
 test r12,r12
 jz .Lparse_bad
	cmp r12,0x7fffffff
	ja .Lparse_bad
 mov rdi,rbx
 xor esi,esi
 mov rdx,r12
 call memchr@PLT
 test rax,rax
 jnz .Lparse_bad
 /* json-c exposes keys as C strings; reject decoded NUL in keys only. */
 xor ecx,ecx
 xor r8d,r8d
 xor r9d,r9d
.Lkey_scan:
 cmp rcx,r12
 jae .Lkey_scan_done
 movzx eax,byte ptr [rbx+rcx]
 test r8d,r8d
 jnz .Lkey_string
 cmp al,34
 jne .Lkey_scan_next
 mov r8d,1
 xor r9d,r9d
 jmp .Lkey_scan_next
.Lkey_string:
 cmp al,92
 jne .Lkey_quote
 lea rdx,[rcx+5]
 cmp rdx,r12
 jae .Lkey_escape_skip
 cmp byte ptr [rbx+rcx+1],'u'
 jne .Lkey_escape_skip
 cmp dword ptr [rbx+rcx+2],0x30303030
 jne .Lkey_escape_skip
 mov r9d,1
.Lkey_escape_skip:
 inc rcx
 jmp .Lkey_scan_next
.Lkey_quote:
 cmp al,34
 jne .Lkey_scan_next
 xor r8d,r8d
 test r9d,r9d
 jz .Lkey_scan_next
 lea rdx,[rcx+1]
.Lkey_space:
 cmp rdx,r12
 jae .Lkey_scan_next
 movzx eax,byte ptr [rbx+rdx]
 cmp al,32
 je .Lkey_space_next
 cmp al,9
 je .Lkey_space_next
 cmp al,10
 je .Lkey_space_next
 cmp al,13
 jne .Lkey_colon
.Lkey_space_next:
 inc rdx
 jmp .Lkey_space
.Lkey_colon:
 cmp al,58
 je .Lparse_bad
.Lkey_scan_next:
 inc rcx
 jmp .Lkey_scan
.Lkey_scan_done:
 call json_tokener_new@PLT
 test rax,rax
 jz .Lparse_bad
 mov [rsp],rax
 mov rdi,rax
 mov esi,PG_WTL_JSON_FLAGS
 call json_tokener_set_flags@PLT
 mov rdi,[rsp]
 mov rsi,rbx
 mov edx,r12d
 call json_tokener_parse_ex@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lparse_bad
 mov rdi,[rsp]
 call json_tokener_get_error@PLT
 test eax,eax
 jnz .Lparse_bad
 mov rdi,[rsp]
 call json_tokener_get_parse_end@PLT
.Ltail:
 cmp rax,r12
 jae .Lobject
 movzx ecx,byte ptr [rbx+rax]
 cmp ecx,32
 je .Ltail_next
 cmp ecx,9
 je .Ltail_next
 cmp ecx,10
 je .Ltail_next
 cmp ecx,13
 jne .Lparse_bad
.Ltail_next:
 inc rax
 jmp .Ltail
.Lobject:
 mov rdi,[rsp+8]
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lparse_bad
 mov rdi,[rsp+8]
 call json_object_object_length@PLT
 cmp eax,2
 jne .Lparse_bad
 xor r15d,r15d
.Lfield:
 lea rsi,[rip+.Lpath]
 test r15d,r15d
 jz .Lget
 lea rsi,[rip+.Lcontent]
.Lget:
 mov rdi,[rsp+8]
 lea rdx,[rsp+16]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lparse_bad
 mov rdi,[rsp+16]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lparse_bad
 mov rdi,[rsp+16]
 call json_object_get_string_len@PLT
 mov r13d,eax
 test r15d,r15d
 jz .Lpath_length
.Lpath_length:
 test r15d,r15d
 jnz .Lstring_length_ready
 cmp r13,4096
 jae .Lparse_bad
.Lstring_length_ready:
 mov rdi,[rsp+16]
 call json_object_get_string@PLT
 mov r14,rax
 test r15d,r15d
 jnz .Lcopy_content
 test r13,r13
 jz .Lparse_bad
 mov rdi,r14
 xor esi,esi
 mov rdx,r13
 call memchr@PLT
 test rax,rax
 jnz .Lparse_bad
.Lcopy_path:
 mov rdi,[rsp+32]
 add rdi,4112
 mov rsi,r14
 lea rdx,[r13+1]
 call memcpy@PLT
 mov rdi,r14
 mov rsi,r13
 mov rdx,[rsp+32]
 mov ecx,4096
 cmp dword ptr [rsp+56],0
 jne .Lwrite_resolve
 call pg_tool_path_prepare@PLT
 jmp .Lwrite_prepared
.Lwrite_resolve:
 call pg_tool_path_resolve@PLT
.Lwrite_prepared:
 test eax,eax
 jnz .Lparse_bad
 inc r15d
 jmp .Lfield
.Lcopy_content:
 mov rdi,r13
 inc rdi
 call malloc@PLT
 test rax,rax
 jz .Lparse_bad
 mov [rsp+24],rax
 mov rdi,rax
 mov rsi,r14
 mov rdx,r13
 call memcpy@PLT
 mov rax,[rsp+24]
 mov byte ptr [rax+r13],0
 mov rdi,[rsp+40]
 mov [rdi],rax
 mov qword ptr [rsp+24],0
 mov rax,[rsp+48]
 mov [rax],r13
 xor ebx,ebx
 jmp .Lparse_cleanup
.Lparse_bad:
 mov ebx,1
.Lparse_cleanup:
 mov rdi,[rsp+24]
 call free@PLT
 mov rdi,[rsp+8]
 test rdi,rdi
 jz .Lparse_token
 call json_object_put@PLT
.Lparse_token:
 mov rdi,[rsp]
 test rdi,rdi
 jz .Lparse_return
 call json_tokener_free@PLT
.Lparse_return:
 mov eax,ebx
 add rsp,64
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size .Lparse,.-.Lparse

.type .Lvalidate,@function
.Lvalidate:
 xor r10d,r10d
 jmp .Lvalidate_path_mode
.Lvalidate_file:
 mov r10d,1
.Lvalidate_path_mode:
 sub rsp,8216
 mov [rsp+8208],r10d
 mov qword ptr [rsp+4096],0
 mov rdi,rsi
 mov rsi,rdx
 mov rdx,rsp
 lea rcx,[rsp+4096]
 lea r8,[rsp+4104]
 mov r9d,[rsp+8208]
 call .Lparse
 xor eax,1
 mov [rsp+4112],eax
 mov rdi,[rsp+4096]
 call free@PLT
 mov eax,[rsp+4112]
 add rsp,8216
 ret
.size .Lvalidate,.-.Lvalidate

.type .Lauthorize,@function
.Lauthorize:
 xor eax,eax
 test rdi,rdi
 jz .Lauth_return
 cmp dword ptr [rdi],0
 setge al
.Lauth_return:
 ret
.size .Lauthorize,.-.Lauthorize

/* Stack: path[4096], content pointer/length, stat[144], saved outputs. */
.type .Lexecute,@function
.Lexecute:
 xor r10d,r10d
 jmp .Lexecute_path_mode
.Lexecute_file:
 mov r10d,1
.Lexecute_path_mode:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,8400
 mov qword ptr [rsp+4096],0
 mov rbx,rdi
 mov [rsp+8352],rcx
 mov [rsp+8360],r8
 mov [rsp+8368],r9
 mov [rsp+8384],r10d
 mov r12d,-1
 mov r13d,-1
 test rcx,rcx
 jz .Lno_output
 test r9,r9
 jz .Lno_output
 mov qword ptr [r9],0
 cmp r8,64
 jb .Lno_output
 mov rdi,rsi
 mov rsi,rdx
 mov rdx,rsp
 lea rcx,[rsp+4096]
 lea r8,[rsp+4104]
 mov r9d,[rsp+8384]
 call .Lparse
 test eax,eax
 jnz .Lwrite_error
 lea rdi,[rsp+4112]
 call strlen@PLT
 add rax,23 /* success prefix and terminator */
 cmp rax,[rsp+8360]
 ja .Lwrite_error
 test rbx,rbx
 jz .Lwrite_error
 cmp dword ptr [rbx],0
 jl .Lwrite_error
 mov r14,rsp
 cmp byte ptr [rsp],47
 jne .Lwrite_relative
 lea rdi,[rip+.Lfilesystem_root]
 mov esi,PG_WTL_OPEN_PARENT
 xor eax,eax
 call open@PLT
 inc r14
 jmp .Lwrite_root_ready
.Lwrite_relative:
 mov edi,[rbx]
 mov esi,PG_RTL_F_DUPFD_CLOEXEC
 xor edx,edx
 xor eax,eax
 call fcntl@PLT
.Lwrite_root_ready:
 test eax,eax
 js .Lwrite_error
 mov r12d,eax
.Lcomponent:
 mov rdi,r14
 mov esi,'/'
 call strchr@PLT
 test rax,rax
 jz .Lfile
 mov r15,rax
 cmp rax,r14
 je .Lwrite_empty_component
 mov byte ptr [r15],0
 mov edi,r12d
 mov rsi,r14
 mov edx,PG_WTL_OPEN_PARENT
 xor eax,eax
 call openat@PLT
 test eax,eax
 jns .Lopened_parent
 call __errno_location@PLT
 cmp dword ptr [rax],2
 jne .Lwrite_error
 mov edi,r12d
 mov rsi,r14
 mov edx,0755
 call mkdirat@PLT
 /* A concurrent creator is accepted only after opening a real directory. */
 mov edi,r12d
 mov rsi,r14
 mov edx,PG_WTL_OPEN_PARENT
 xor eax,eax
 call openat@PLT
 test eax,eax
 js .Lwrite_error
.Lopened_parent:
 mov ebx,eax
 mov edi,r12d
 call fsync@PLT
 test eax,eax
 jnz .Lparent_sync_error
 mov edi,r12d
 call close@PLT
 mov r12d,ebx
 lea r14,[r15+1]
 jmp .Lcomponent
.Lwrite_empty_component:
 lea r14,[r15+1]
 jmp .Lcomponent
.Lparent_sync_error:
 mov edi,ebx
 call close@PLT
 jmp .Lwrite_error
.Lfile:
 mov edi,r12d
 mov rsi,r14
 mov edx,PG_WTL_OPEN_FILE
 mov ecx,0600
 xor eax,eax
 call openat@PLT
 test eax,eax
 js .Lwrite_error
 mov r13d,eax
 mov edi,r13d
 lea rsi,[rsp+8208]
 call fstat@PLT
 test eax,eax
 jnz .Lwrite_error
 mov eax,[rsp+8208+PG_RTL_STAT_MODE]
 and eax,PG_RTL_S_IFMT
 cmp eax,PG_RTL_S_IFREG
 jne .Lwrite_error
 mov edi,r13d
 xor esi,esi
 call ftruncate@PLT
 test eax,eax
 jnz .Lwrite_error
 xor r14d,r14d
.Lwrite_more:
 cmp r14,[rsp+4104]
 jae .Lsync
 mov edi,r13d
 mov rax,[rsp+4096]
 lea rsi,[rax+r14]
 mov rdx,[rsp+4104]
 sub rdx,r14
 call write@PLT
 test rax,rax
 js .Lwrite_errno
 jz .Lwrite_error
 add r14,rax
 jmp .Lwrite_more
.Lwrite_errno:
 call __errno_location@PLT
 cmp dword ptr [rax],4
 je .Lwrite_more
 jmp .Lwrite_error
.Lsync:
 mov edi,r13d
 call fsync@PLT
 test eax,eax
 jnz .Lwrite_error
 mov edi,r12d
 call fsync@PLT
 test eax,eax
 jnz .Lwrite_error
 xor ebx,ebx
 jmp .Lclose_all
.Lwrite_error:
 mov ebx,1
.Lclose_all:
 cmp r13d,0
 jl .Lclose_parent
 mov edi,r13d
 call close@PLT
 test eax,eax
 jz .Lclose_parent
 mov ebx,1
.Lclose_parent:
 cmp r12d,0
 jl .Lmessage
 mov edi,r12d
 call close@PLT
.Lmessage:
 lea r14,[rip+.Lok]
 test ebx,ebx
 jnz .Lmessage_failed
 mov rdi,[rsp+8352]
 mov rsi,[rsp+8360]
 lea rdx,[rip+.Lok]
 lea rcx,[rsp+4112]
 xor eax,eax
 call snprintf@PLT
 test eax,eax
 js .Lmessage_overflow
 cmp rax,[rsp+8360]
 jae .Lmessage_overflow
 mov rdx,[rsp+8368]
 mov [rdx],rax
 xor eax,eax
 jmp .Lexecute_return
.Lmessage_overflow:
 mov eax,1
 jmp .Lexecute_return
.Lmessage_failed:
 lea r14,[rip+.Lfailed]
.Lmessage_copy:
 mov rdi,r14
 call strlen@PLT
 mov r15,rax
 mov rdi,[rsp+8352]
 mov rsi,r14
 lea rdx,[rax+1]
 call memcpy@PLT
 mov rax,[rsp+8368]
 mov [rax],r15
 mov eax,ebx
 jmp .Lexecute_return
.Lno_output:
 mov eax,1
.Lexecute_return:
 mov [rsp+8376],eax
 mov rdi,[rsp+4096]
 call free@PLT
 mov eax,[rsp+8376]
 add rsp,8400
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size .Lexecute,.-.Lexecute

.globl pg_write_tool_definition
.type pg_write_tool_definition,@function
pg_write_tool_definition:
 lea rax,[rip+.Lname]
 mov [rdi+PG_TL_DEF_NAME],rax
 lea rax,[rip+.Ldescription]
 mov [rdi+PG_TL_DEF_DESCRIPTION],rax
 lea rax,[rip+.Lschema]
 mov [rdi+PG_TL_DEF_SCHEMA],rax
 mov [rdi+PG_TL_DEF_CONTEXT],rsi
 lea rax,[rip+.Lvalidate]
 mov [rdi+PG_TL_DEF_VALIDATE],rax
 lea rax,[rip+.Lauthorize]
 mov [rdi+PG_TL_DEF_AUTHORIZE],rax
 lea rax,[rip+.Lexecute]
 mov [rdi+PG_TL_DEF_EXECUTE],rax
 mov qword ptr [rdi+PG_TL_DEF_REPLAY_SAFE],0
 mov qword ptr [rdi+PG_TL_DEF_ASYNC],0
 mov qword ptr [rdi+PG_TL_DEF_OUTPUT_LIMITS],0
 mov rax,rdi
 ret
.size pg_write_tool_definition,.-pg_write_tool_definition
.section .note.GNU-stack,"",@progbits

.section .rodata
.Lfilesystem_root: .asciz "/"

.text
.globl pg_write_file_definition
.type pg_write_file_definition,@function
pg_write_file_definition:
 push rbx
 mov rbx,rdi
 call pg_write_tool_definition
 lea rax,[rip+.Lvalidate_file]
 mov [rbx+PG_TL_DEF_VALIDATE],rax
 lea rax,[rip+.Lexecute_file]
 mov [rbx+PG_TL_DEF_EXECUTE],rax
 mov rax,rbx
 pop rbx
 ret
.size pg_write_file_definition,.-pg_write_file_definition
