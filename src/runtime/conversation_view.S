/* Pi Durable harness/view.ts build and context.ts activeEntries.
   d5b3de22d0e24a3e9c38a9410d3329b099717283; MIT, docs/licenses/pi.md. */
.intel_syntax noprefix
.macro VF n
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\n
.endm
.macro VE n
 add rsp,\n
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.macro VG obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lv_\key]
 call json_object_object_get@PLT
.endm
.text
/* Retain a borrowed field in an owned object, consuming the reference on error. */
.Lv_field:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 call json_object_get@PLT
 mov [rsp],rax
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Lv_field_end
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,5
.Lv_field_end:
 add rsp,8
 pop r12
 pop rbx
 ret
.globl pg_conversation_queue_read
.type pg_conversation_queue_read,@function
pg_conversation_queue_read:
 VF 176
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r13],0
 mov rdi,rsp
 xor eax,eax
 mov ecx,22
 rep stosq
 mov dword ptr [rsp],3
 mov dword ptr [rsp+4],1
 mov qword ptr [rsp+24],256
 mov qword ptr [rsp+32],65
 mov [rsp+40],r12
 call json_object_new_array@PLT
 mov [rsp+144],rax
 test rax,rax
 jz .Lv_inbox_bad
.Lv_inbox_page:
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+112]
 call [rbx+16]
 test eax,eax
 jnz .Lv_inbox_bad
 xor r14d,r14d
.Lv_inbox_each:
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lv_inbox_page_end
 mov rdi,[rsp+112]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+152],rax
 mov rdi,[rsp+144]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lv_inbox_bad
 mov qword ptr [rsp+152],0
 inc r14
 jmp .Lv_inbox_each
.Lv_inbox_page_end:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 cmp dword ptr [rsp+132],0
 je .Lv_inbox_ok
 mov rax,[rsp+120]
 cmp rax,[rsp+16]
 jle .Lv_inbox_bad
 mov [rsp+16],rax
 mov dword ptr [rsp+8],1
 mov eax,[rsp+128]
 mov [rsp+12],eax
 jmp .Lv_inbox_page
.Lv_inbox_ok:
 mov rax,[rsp+144]
 mov [r13],rax
 mov qword ptr [rsp+144],0
 xor ebp,ebp
 jmp .Lv_inbox_end
.Lv_inbox_bad:
 mov ebp,5
.Lv_inbox_end:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov rdi,[rsp+144]
 call json_object_put@PLT
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov eax,ebp
 VE 176
.globl pg_conversation_view_read
.type pg_conversation_view_read,@function
pg_conversation_view_read:
 VF 192
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r13],0
 mov rdi,rsp
 xor eax,eax
 mov ecx,24
 rep stosq
 cmp dword ptr [rbx+24],0
 jne .Lv_view_closed
 mov rdi,rsp
 mov rsi,[rbx]
 call pg_durable_read_on_line_begin@PLT
 test eax,eax
 jnz .Lv_view_busy
 mov rax,[rbx+8]
 mov rdi,[rax]
 xor esi,esi
 mov rdx,r12
 lea rcx,[rsp+16]
 call [rax+8]
 cmp eax,1
 je .Lv_view_missing
 test eax,eax
 jnz .Lv_view_bad
 mov rdi,[rbx+8]
 mov rsi,r12
 lea rdx,[rsp+24]
 call .Lv_active_entries
 test eax,eax
 jnz .Lv_view_bad
 lea rdi,[rip+.Lv_view_json]
 call json_tokener_parse@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lv_view_bad
 mov rdi,rax
 lea rsi,[rip+.Lv_conversation]
 mov rdx,[rsp+16]
 call .Lv_field
 test eax,eax
 jnz .Lv_view_bad
 mov rax,[rsp+24]
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lv_entries]
 mov rdx,rax
 call .Lv_field
 test eax,eax
 jnz .Lv_view_bad
 VG "qword ptr [rsp+32]",docs
 mov r14,rax
 xor ebp,ebp
.Lv_view_document:
 lea rax,[rip+.Lv_definitions]
 mov rax,[rax+rbp*8]
 test rax,rax
 jz .Lv_view_inbox
 mov [rsp+136],rax
 mov rcx,[rax]
 mov [rsp+48],rcx
 mov rcx,[rax+8]
 mov [rsp+56],rcx
 mov [rsp+80],r12
 mov dword ptr [rsp+88],1
 mov rdi,rsp
 mov rsi,rax
 lea rdx,[rsp+48]
 lea rcx,[rsp+96]
 call pg_durable_conversation_document_on_line@PLT
 cmp eax,8
 je .Lv_view_document_next
 test eax,eax
 jnz .Lv_view_bad
 mov rax,[rsp+136]
 mov rdi,r14
 mov rsi,[rax]
 mov rdx,[rsp+104]
 call .Lv_field
 test eax,eax
 jnz .Lv_view_bad
 mov rdi,[rsp+96]
 call json_object_put@PLT
 mov qword ptr [rsp+96],0
 mov rdi,[rsp+104]
 call json_object_put@PLT
 mov qword ptr [rsp+104],0
.Lv_view_document_next:
 inc rbp
 jmp .Lv_view_document
.Lv_view_inbox:
 mov rdi,[rbx+8]
 mov rsi,r12
 lea rdx,[rsp+40]
 call pg_conversation_queue_read
 test eax,eax
 jnz .Lv_view_bad
 mov rdi,[rsp+32]
 lea rsi,[rip+.Lv_inbox_key]
 mov rdx,[rsp+40]
 call .Lv_field
 test eax,eax
 jnz .Lv_view_bad
 mov rax,[rsp+32]
 mov [r13],rax
 mov qword ptr [rsp+32],0
 xor ebp,ebp
 jmp .Lv_view_end
.Lv_view_missing:
 mov ebp,2
 jmp .Lv_view_end
.Lv_view_closed:
 mov ebp,7
 jmp .Lv_view_end
.Lv_view_busy:
 mov ebp,3
 jmp .Lv_view_end
.Lv_view_bad:
 mov ebp,5
.Lv_view_end:
 cmp dword ptr [rsp+8],0
 je .Lv_view_line_end
 mov rdi,rsp
 call pg_durable_read_on_line_end@PLT
.Lv_view_line_end:
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+96]
 call json_object_put@PLT
 mov rdi,[rsp+104]
 call json_object_put@PLT
 mov eax,ebp
 VE 192
.globl pg_conversation_view_service
.type pg_conversation_view_service,@function
pg_conversation_view_service:
 mov [rsi],rdi
 lea rax,[rip+pg_conversation_view_read]
 mov [rsi+8],rax
 mov rax,qword ptr [rip+json_object_put@GOTPCREL]
 mov [rsi+16],rax
 lea rax,[rip+pg_conversation_watch_open]
 mov [rsi+24],rax
 lea rax,[rip+pg_conversation_watch_next]
 mov [rsi+32],rax
 lea rax,[rip+pg_conversation_watch_close]
 mov [rsi+40],rax
 ret
.section .rodata
.Lv_conversation: .asciz "conversation"
.Lv_entries: .asciz "entries"
.Lv_docs: .asciz "docs"
.Lv_inbox_key: .asciz "inbox"
.Lv_live: .asciz "pangu.live"
.Lv_usage: .asciz "pangu.usage"
.Lv_view_json: .asciz "{\"conversation\":{},\"entries\":[],\"docs\":{},\"inbox\":[]}"
.section .data.rel.ro
.balign 8
.Lv_live_definition:
 .quad .Lv_live,10,1
 .long 1,1,2,0
 .quad 0,0
.Lv_usage_definition:
 .quad .Lv_usage,11,1
 .long 1,1,2,0
 .quad 0,0
.Lv_definitions: .quad .Lv_live_definition,.Lv_usage_definition,0
.section .note.GNU-stack,"",@progbits

.text
/* activeEntries: newest marker first, then non-head range in ascending ID. */
.Lv_active_entries:
 VF 192
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov qword ptr [r13],0
 mov rdi,rsp
 xor eax,eax
 mov ecx,24
 rep stosq
 mov dword ptr [rsp],1
 mov dword ptr [rsp+4],2
 mov qword ptr [rsp+24],1
 mov qword ptr [rsp+32],513
 mov [rsp+40],r12
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+112]
 call [rbx+16]
 test eax,eax
 jnz .Lv_entries_bad
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lv_entries_no_head
 mov rdi,[rsp+112]
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+152],rax
.Lv_entries_no_head:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov dword ptr [rsp+4],1
 mov qword ptr [rsp+24],256
 mov qword ptr [rsp+32],1
 call json_object_new_array@PLT
 mov [rsp+144],rax
 test rax,rax
 jz .Lv_entries_bad
 cmp qword ptr [rsp+152],0
 je .Lv_entries_page
 VG "qword ptr [rsp+152]",head
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+64],rax
 or qword ptr [rsp+32],8
 mov rdi,[rsp+152]
 call json_object_get@PLT
 mov [rsp+160],rax
 mov rdi,[rsp+144]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lv_entries_bad
 mov qword ptr [rsp+160],0
.Lv_entries_page:
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+112]
 call [rbx+16]
 test eax,eax
 jnz .Lv_entries_bad
 xor r14d,r14d
.Lv_entries_each:
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lv_entries_next
 mov rdi,[rsp+112]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov [rsp+168],rax
 VG rax,head
 test rax,rax
 jnz .Lv_entries_skip
 mov rdi,[rsp+168]
 call json_object_get@PLT
 mov [rsp+160],rax
 mov rdi,[rsp+144]
 mov rsi,rax
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lv_entries_bad
 mov qword ptr [rsp+160],0
.Lv_entries_skip:
 inc r14
 jmp .Lv_entries_each
.Lv_entries_next:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 cmp dword ptr [rsp+132],0
 je .Lv_entries_ok
 mov rax,[rsp+120]
 cmp rax,[rsp+16]
 jle .Lv_entries_bad
 mov [rsp+16],rax
 mov dword ptr [rsp+8],1
 mov eax,[rsp+128]
 mov [rsp+12],eax
 jmp .Lv_entries_page
.Lv_entries_ok:
 mov rax,[rsp+144]
 mov [r13],rax
 mov qword ptr [rsp+144],0
 xor ebp,ebp
 jmp .Lv_entries_end
.Lv_entries_bad:
 mov ebp,5
.Lv_entries_end:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov rdi,[rsp+144]
 call json_object_put@PLT
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov rdi,[rsp+160]
 call json_object_put@PLT
 mov eax,ebp
 VE 192
.section .rodata
.Lv_head: .asciz "head"

.text
.Lv_watch_snapshot:
 mov rdx,rsi
 mov rsi,[rdi+80]
 mov rdi,[rdi+72]
 jmp pg_conversation_view_read
.globl pg_conversation_watch_open
.type pg_conversation_watch_open,@function
pg_conversation_watch_open:
 VF 16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 cmp dword ptr [rdx+96],0
 jne .Lv_watch_open_invalid
 cmp dword ptr [rbx+24],0
 jne .Lv_watch_open_closed
 mov [r13+72],rbx
 mov [r13+80],r12
 mov rdi,r13
 mov rsi,r13
 lea rdx,[rip+.Lv_watch_snapshot]
 call pg_view_binding_init@PLT
 mov rdi,r13
 lea rsi,[r13+40]
 call pg_view_watch_open@PLT
 test eax,eax
 jnz .Lv_watch_open_error
 cmp dword ptr [rbx+24],0
 jne .Lv_watch_open_late_close
 mov rax,[rbx+16]
 mov [r13+88],rax
 mov [rbx+16],r13
 mov dword ptr [r13+96],1
 xor eax,eax
 jmp .Lv_watch_open_end
.Lv_watch_open_late_close:
 mov eax,7
.Lv_watch_open_error:
 mov [rsp],eax
 mov rdi,r13
 call pg_view_binding_close@PLT
 mov rdi,r13
 xor eax,eax
 mov ecx,13
 rep stosq
 mov eax,[rsp]
 jmp .Lv_watch_open_end
.Lv_watch_open_closed:
 mov eax,7
 jmp .Lv_watch_open_end
.Lv_watch_open_invalid:
 mov eax,1
.Lv_watch_open_end:
 VE 16
.globl pg_conversation_watch_next
.type pg_conversation_watch_next,@function
pg_conversation_watch_next:
 mov qword ptr [rsi],0
 cmp dword ptr [rdi+96],1
 jne .Lv_watch_next_closed
 add rdi,40
 jmp pg_view_watch_next@PLT
.Lv_watch_next_closed:
 mov eax,-7
 ret
.globl pg_conversation_watch_close
.type pg_conversation_watch_close,@function
pg_conversation_watch_close:
 push rbx
 mov rbx,rdi
 cmp dword ptr [rbx+96],0
 je .Lv_watch_close_end
 mov rax,[rbx+72]
 lea rcx,[rax+16]
.Lv_watch_close_find:
 mov rax,[rcx]
 test rax,rax
 jz .Lv_watch_close_release
 cmp rax,rbx
 je .Lv_watch_close_unlink
 lea rcx,[rax+88]
 jmp .Lv_watch_close_find
.Lv_watch_close_unlink:
 mov rax,[rbx+88]
 mov [rcx],rax
.Lv_watch_close_release:
 mov rdi,rbx
 call pg_view_binding_close@PLT
 mov rdi,rbx
 xor eax,eax
 mov ecx,13
 rep stosq
.Lv_watch_close_end:
 pop rbx
 ret
.globl pg_conversation_view_close
.type pg_conversation_view_close,@function
pg_conversation_view_close:
 push rbx
 mov rbx,rdi
 mov dword ptr [rbx+24],1
.Lv_watch_close_all:
 mov rdi,[rbx+16]
 test rdi,rdi
 jz .Lv_watch_close_all_end
 call pg_conversation_watch_close
 jmp .Lv_watch_close_all
.Lv_watch_close_all_end:
 pop rbx
 ret
