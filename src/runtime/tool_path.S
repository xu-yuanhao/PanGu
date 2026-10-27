/* Pi Durable path-utils.ts/Node ExecutionEnv; MIT: docs/licenses/pi.md. */
#include "tool_path.h"
.intel_syntax noprefix
.text
.globl pg_tool_path_prepare
.type pg_tool_path_prepare,@function
pg_tool_path_prepare:
 xor r8d,r8d
 jmp .Lpath_begin
.globl pg_tool_path_resolve
.type pg_tool_path_resolve,@function
pg_tool_path_resolve:
 mov r8d,1
.Lpath_begin:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,8264
 mov rbx,rdi
 mov r15,rsi
 mov r12,rdx
 mov r13,rcx
 mov [rsp+8260],r8d
 mov qword ptr [rsp+8192],0
 mov qword ptr [rsp+8200],0
 mov qword ptr [rsp+8208],0
 mov qword ptr [rsp+8216],0
 test r12,r12
 jz .Lpath_bad
 test r13,r13
 jz .Lpath_bad
 mov byte ptr [r12],0
 test rbx,rbx
 jz .Lpath_bad
 test r15,r15
 jz .Lpath_bad
 cmp r15,4096
 jae .Lpath_bad
 mov rdi,rbx
 xor esi,esi
 mov rdx,r15
 call memchr@PLT
 test rax,rax
 jnz .Lpath_bad
 cmp dword ptr [rsp+8260],0
 jne .Lpath_raw_copy
 cmp byte ptr [rbx],64
 jne .Lpath_copy_begin
 inc rbx
 dec r15
.Lpath_raw_copy:
 mov rdi,rsp
 mov rsi,rbx
 mov rdx,r15
 call memcpy@PLT
 mov r14,r15
 jmp .Lpath_copied
.Lpath_copy_begin:
 xor r14d,r14d
.Lpath_copy:
 test r15,r15
 jz .Lpath_copied
 movzx eax,byte ptr [rbx]
 mov edx,1
 cmp r15,2
 jb .Lpath_copy_byte
 cmp ax,0xc2
 jne .Lpath_space3
 cmp byte ptr [rbx+1],0xa0
 jne .Lpath_copy_byte
 mov eax,32
 mov edx,2
 jmp .Lpath_copy_byte
.Lpath_space3:
 cmp r15,3
 jb .Lpath_copy_byte
 cmp al,0xe2
 jne .Lpath_space3000
 cmp byte ptr [rbx+1],0x80
 jne .Lpath_space205f
 mov cl,[rbx+2]
 cmp cl,0x80
 jb .Lpath_copy_byte
 cmp cl,0x8a
 jbe .Lpath_space_found
 cmp cl,0xaf
 je .Lpath_space_found
 jmp .Lpath_copy_byte
.Lpath_space205f:
 cmp byte ptr [rbx+1],0x81
 jne .Lpath_copy_byte
 cmp byte ptr [rbx+2],0x9f
 je .Lpath_space_found
 jmp .Lpath_copy_byte
.Lpath_space3000:
 cmp al,0xe3
 jne .Lpath_copy_byte
 cmp byte ptr [rbx+1],0x80
 jne .Lpath_copy_byte
 cmp byte ptr [rbx+2],0x80
 jne .Lpath_copy_byte
.Lpath_space_found:
 mov eax,32
 mov edx,3
.Lpath_copy_byte:
 mov [rsp+r14],al
 inc r14
 add rbx,rdx
 sub r15,rdx
 jmp .Lpath_copy
.Lpath_copied:
 mov byte ptr [rsp+r14],0
 mov rbx,rsp
 test r14,r14
 jnz .Lpath_expansion
 mov byte ptr [rbx],46
 mov byte ptr [rbx+1],0
 mov r14d,1
.Lpath_expansion:
 cmp byte ptr [rbx],126
 jne .Lpath_url
 cmp r14,1
 je .Lpath_home
 cmp byte ptr [rbx+1],47
 jne .Lpath_url
.Lpath_home:
 call pg_tool_path_home@PLT
 test rax,rax
 jz .Lpath_bad
 mov [rsp+8240],rax
 mov rdi,rax
 mov esi,4096
 call strnlen@PLT
 test rax,rax
 jz .Lpath_bad
 cmp rax,4096
 jae .Lpath_bad
 mov [rsp+8248],rax
 lea rcx,[rax+r14-1]
 cmp rcx,4096
 jae .Lpath_bad
 lea rdi,[rsp+4096]
 mov rsi,[rsp+8240]
 mov rdx,rax
 call memcpy@PLT
 mov rax,[rsp+8248]
 lea rdi,[rsp+rax+4096]
 lea rsi,[rbx+1]
 mov rdx,r14
 call memcpy@PLT
 lea rbx,[rsp+4096]
 jmp .Lpath_lexical
.Lpath_url:
 cmp r14,7
 jb .Lpath_lexical
 mov rdi,rbx
 lea rsi,[rip+.Lpath_file_scheme]
 mov edx,7
 call memcmp@PLT
 test eax,eax
 jnz .Lpath_lexical
 mov r15d,7
 cmp byte ptr [rbx+7],47
 je .Lpath_url_percent
.Lpath_url_authority:
 cmp r15,r14
 jae .Lpath_url_parse
 mov al,[rbx+r15]
 cmp al,47
 je .Lpath_url_percent
 cmp al,63
 je .Lpath_url_parse
 cmp al,35
 je .Lpath_url_parse
 inc r15
 jmp .Lpath_url_authority
.Lpath_url_percent:
 cmp r15,r14
 jae .Lpath_url_parse
 cmp byte ptr [rbx+r15],63
 je .Lpath_url_parse
 cmp byte ptr [rbx+r15],35
 je .Lpath_url_parse
 cmp byte ptr [rbx+r15],37
 jne .Lpath_url_percent_next
 lea rax,[r15+2]
 cmp rax,r14
 jae .Lpath_lexical
 movzx edi,byte ptr [rbx+r15+1]
 call .Lpath_hex
 test eax,eax
 js .Lpath_lexical
 mov [rsp+8224],eax
 movzx edi,byte ptr [rbx+r15+2]
 call .Lpath_hex
 test eax,eax
 js .Lpath_lexical
 mov edx,[rsp+8224]
 shl edx,4
 or eax,edx
 test eax,eax
 jz .Lpath_bad
 cmp eax,47
 je .Lpath_lexical
 add r15,2
.Lpath_url_percent_next:
 inc r15
 jmp .Lpath_url_percent
.Lpath_url_parse:
 call curl_url@PLT
 mov [rsp+8192],rax
 test rax,rax
 jz .Lpath_bad
 /* HTTP grammar supplies an authority's empty path and decoded host; the
    file contract below accepts only localhost without userinfo or a port. */
 xor r15d,r15d
 cmp byte ptr [rbx+7],47
 je .Lpath_url_set
 mov dword ptr [rbx],0x70747468
 mov r15d,1
.Lpath_url_set:
 mov rdi,[rsp+8192]
 xor esi,esi
 mov rdx,rbx
 mov ecx,2096
 call curl_url_set@PLT
 test r15d,r15d
 jz .Lpath_url_set_result
 mov dword ptr [rbx],0x656c6966
.Lpath_url_set_result:
 test eax,eax
 jnz .Lpath_lexical
 mov rdi,[rsp+8192]
 mov esi,5
 lea rdx,[rsp+8208]
 xor ecx,ecx
 call curl_url_get@PLT
 test eax,eax
 jnz .Lpath_url_path
 mov rdi,[rsp+8208]
 cmp byte ptr [rdi],0
 je .Lpath_url_path
 lea rsi,[rip+.Lpath_localhost]
 call strcasecmp@PLT
 test eax,eax
 jnz .Lpath_lexical
.Lpath_url_path:
 mov rdi,[rsp+8192]
 mov esi,6
 lea rdx,[rsp+8216]
 xor ecx,ecx
 call curl_url_get@PLT
 test eax,eax
 jz .Lpath_lexical
 mov rdi,[rsp+8192]
 mov esi,7
 lea rdx,[rsp+8200]
 mov ecx,64
 call curl_url_get@PLT
 test eax,eax
 jnz .Lpath_lexical
 mov rdi,[rsp+8200]
 call strlen@PLT
 cmp rax,4096
 jae .Lpath_bad
 mov r15,rax
 xor r14d,r14d
.Lpath_url_utf8:
 cmp r14,r15
 jae .Lpath_url_valid
 mov rdi,[rsp+8200]
 add rdi,r14
 mov rsi,r15
 sub rsi,r14
 lea rdx,[rsp+8256]
 call utf8proc_iterate@PLT
 test rax,rax
 jle .Lpath_lexical
 add r14,rax
 jmp .Lpath_url_utf8
.Lpath_url_valid:
 mov rbx,[rsp+8200]
.Lpath_lexical:
 xor r14d,r14d
 xor r15d,r15d
 mov qword ptr [rsp+8232],0
 cmp byte ptr [rbx],47
 jne .Lpath_segment
 cmp r13,2
 jb .Lpath_bad
 mov byte ptr [r12],47
 mov r14d,1
 mov r15d,1
 mov qword ptr [rsp+8232],1
.Lpath_segment:
 mov [rsp+8224],r15
.Lpath_scan:
 mov al,[rbx+r15]
 test al,al
 jz .Lpath_boundary
 cmp al,47
 je .Lpath_boundary
 inc r15
 jmp .Lpath_scan
.Lpath_boundary:
 mov rdx,r15
 sub rdx,[rsp+8224]
 test rdx,rdx
 jz .Lpath_next
 mov rcx,[rsp+8224]
 cmp rdx,1
 jne .Lpath_parent_check
 cmp byte ptr [rbx+rcx],46
 je .Lpath_next
.Lpath_parent_check:
 cmp rdx,2
 jne .Lpath_append
 cmp word ptr [rbx+rcx],0x2e2e
 jne .Lpath_append
 cmp r14,[rsp+8232]
 jbe .Lpath_above_root
 mov r10,r14
.Lpath_last_start:
 test r10,r10
 jz .Lpath_last_found
 cmp byte ptr [r12+r10-1],47
 je .Lpath_last_found
 dec r10
 jmp .Lpath_last_start
.Lpath_last_found:
 mov rax,r14
 sub rax,r10
 cmp rax,2
 jne .Lpath_pop
 cmp word ptr [r12+r10],0x2e2e
 je .Lpath_append
.Lpath_pop:
 mov r14,r10
 test r14,r14
 jz .Lpath_next
 dec r14
 cmp r14,[rsp+8232]
 jae .Lpath_next
 mov r14,[rsp+8232]
 jmp .Lpath_next
.Lpath_above_root:
 cmp qword ptr [rsp+8232],1
 je .Lpath_next
.Lpath_append:
 xor r10d,r10d
 test r14,r14
 jz .Lpath_append_room
 cmp byte ptr [r12+r14-1],47
 je .Lpath_append_room
 mov r10d,1
.Lpath_append_room:
 lea rax,[r14+rdx]
 add rax,r10
 cmp rax,r13
 jae .Lpath_bad
 cmp rax,4096
 jae .Lpath_bad
 test r10,r10
 jz .Lpath_append_bytes
 mov byte ptr [r12+r14],47
 inc r14
.Lpath_append_bytes:
 lea rdi,[r12+r14]
 lea rsi,[rbx+rcx]
 mov [rsp+8248],rdx
 call memcpy@PLT
 add r14,[rsp+8248]
.Lpath_next:
 cmp byte ptr [rbx+r15],0
 je .Lpath_done
 inc r15
 jmp .Lpath_segment
.Lpath_done:
 test r14,r14
 jnz .Lpath_terminate
 cmp r13,2
 jb .Lpath_bad
 mov byte ptr [r12],46
 mov r14d,1
.Lpath_terminate:
 mov byte ptr [r12+r14],0
 xor ebp,ebp
 jmp .Lpath_cleanup
.Lpath_bad:
 mov ebp,1
 test r12,r12
 jz .Lpath_cleanup
 test r13,r13
 jz .Lpath_cleanup
 mov byte ptr [r12],0
.Lpath_cleanup:
 mov rdi,[rsp+8200]
 call curl_free@PLT
 mov rdi,[rsp+8208]
 call curl_free@PLT
 mov rdi,[rsp+8216]
 call curl_free@PLT
 mov rdi,[rsp+8192]
 call curl_url_cleanup@PLT
 mov eax,ebp
 add rsp,8264
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_tool_path_prepare,.-pg_tool_path_prepare
.size pg_tool_path_resolve,.-pg_tool_path_resolve
.globl pg_tool_path_arguments
.type pg_tool_path_arguments,@function
pg_tool_path_arguments:
 push rbx
 push r12
 sub rsp,4104
 mov rbx,rdi
 lea rsi,[rip+.Lpath_key]
 call json_object_object_get@PLT
 mov r12,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lpath_arguments_bad
 mov rdi,r12
 call json_object_get_string_len@PLT
 mov [rsp+4096],rax
 mov rdi,r12
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,[rsp+4096]
 mov rdx,rsp
 mov ecx,4096
 call pg_tool_path_prepare
 test eax,eax
 jnz .Lpath_arguments_bad
 mov rdi,rsp
 call json_object_new_string@PLT
 test rax,rax
 jz .Lpath_arguments_bad
 mov r12,rax
 mov rdi,rbx
 lea rsi,[rip+.Lpath_key]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Lpath_arguments_end
 mov rdi,r12
 call json_object_put@PLT
.Lpath_arguments_bad:
 mov eax,1
.Lpath_arguments_end:
 add rsp,4104
 pop r12
 pop rbx
 ret
.size pg_tool_path_arguments,.-pg_tool_path_arguments
.section .rodata
.Lpath_key: .asciz "path"
.Lpath_file_scheme: .asciz "file://"
.Lpath_localhost: .asciz "localhost"
.section .note.GNU-stack,"",@progbits

.text
.Lpath_hex:
 mov eax,edi
 sub eax,48
 cmp eax,9
 jbe .Lpath_hex_done
 mov eax,edi
 or eax,32
 sub eax,97
 cmp eax,5
 ja .Lpath_hex_bad
 add eax,10
 ret
.Lpath_hex_bad:
 mov eax,-1
.Lpath_hex_done:
 ret
