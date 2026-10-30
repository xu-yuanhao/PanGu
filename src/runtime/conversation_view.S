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
 mov rsi,[rsp+16]
 call .Lcn_add_name
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

.text
/* Add consumes an owned leaf on either result. */
.Lcn_owned:
 test rdx,rdx
 jz .Lcn_owned_bad
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Lcn_owned_end
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,5
.Lcn_owned_end:
 pop rbx
 ret
.Lcn_owned_bad:
 mov eax,5
 ret
/* Latest metadata is independent of active context heads, including clears. */
.Lcn_add_name:
 VF 144
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rsp
 xor eax,eax
 mov ecx,18
 rep stosq
 mov dword ptr [rsp],1
 mov dword ptr [rsp+4],2
 mov qword ptr [rsp+24],256
 mov qword ptr [rsp+32],1
 VG r12,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+40],rax
.Lcn_name_page:
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+112]
 call [rbx+16]
 mov ebp,6
 test eax,eax
 jnz .Lcn_name_end
 xor r14d,r14d
.Lcn_name_each:
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lcn_name_next_page
 mov rdi,[rsp+112]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Lcn_kind_key]
 call json_object_object_get@PLT
 mov [rsp+136],rax
 mov rdi,rax
 call json_object_get_string_len@PLT
 cmp eax,18
 jne .Lcn_name_next
 mov rdi,[rsp+136]
 call json_object_get_string@PLT
 mov rdi,rax
 lea rsi,[rip+.Lcn_kind]
 mov edx,18
 call memcmp@PLT
 test eax,eax
 jnz .Lcn_name_next
 mov rdi,r13
 lea rsi,[rip+.Lcn_name]
 call json_object_object_get@PLT
 mov r13,rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 xor ebp,ebp
 test eax,eax
 jz .Lcn_name_end
 mov rdi,r13
 call json_object_get_string_len@PLT
 test eax,eax
 jz .Lcn_name_end
 mov rdi,r12
 lea rsi,[rip+.Lcn_name]
 mov rdx,r13
 call .Lv_field
 mov ebp,eax
 jmp .Lcn_name_end
.Lcn_name_next:
 inc r14
 jmp .Lcn_name_each
.Lcn_name_next_page:
 xor ebp,ebp
 cmp dword ptr [rsp+132],0
 je .Lcn_name_end
 mov ebp,6
 mov rax,[rsp+120]
 cmp dword ptr [rsp+8],0
 je .Lcn_name_cursor
 cmp rax,[rsp+16]
 jae .Lcn_name_end
.Lcn_name_cursor:
 mov [rsp+16],rax
 mov dword ptr [rsp+8],1
 mov dword ptr [rsp+12],2
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov qword ptr [rsp+112],0
 jmp .Lcn_name_page
.Lcn_name_end:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov eax,ebp
 VE 144
.globl pg_conversation_info
.type pg_conversation_info,@function
pg_conversation_info:
 VF 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rsp
 xor eax,eax
 mov ecx,6
 rep stosq
 mov ebp,4
 test r13,r13
 jz .Lcn_info_end
 mov qword ptr [r13],0
 test rbx,rbx
 jz .Lcn_info_end
 test r12,r12
 jz .Lcn_info_end
 mov rax,9007199254740991
 cmp r12,rax
 ja .Lcn_info_end
 mov ebp,7
 cmp dword ptr [rbx+24],0
 jne .Lcn_info_end
 mov rdi,rsp
 mov rsi,[rbx]
 call pg_durable_read_on_line_begin@PLT
 mov ebp,3
 test eax,eax
 jnz .Lcn_info_end
 mov rax,[rbx+8]
 mov rdi,[rax]
 xor esi,esi
 mov rdx,r12
 lea rcx,[rsp+16]
 call [rax+8]
 mov ebp,2
 cmp eax,1
 je .Lcn_info_release
 mov ebp,6
 test eax,eax
 jnz .Lcn_info_release
 mov rdi,[rbx+8]
 mov rsi,[rsp+16]
 call .Lcn_add_name
 mov ebp,eax
 test eax,eax
 jnz .Lcn_info_release
 mov rax,[rsp+16]
 mov [r13],rax
 mov qword ptr [rsp+16],0
.Lcn_info_release:
 mov rdi,rsp
 call pg_durable_read_on_line_end@PLT
.Lcn_info_end:
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov eax,ebp
 VE 48
.globl pg_conversation_list
.type pg_conversation_list,@function
pg_conversation_list:
 VF 176
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rsp
 xor eax,eax
 mov ecx,22
 rep stosq
 mov ebp,4
 test r13,r13
 jz .Lcn_list_end
 mov qword ptr [r13],0
 test rbx,rbx
 jz .Lcn_list_end
 mov qword ptr [rsp+24],64
 test r12,r12
 jz .Lcn_query_ready
 cmp dword ptr [r12+44],0
 jne .Lcn_list_end
 cmp dword ptr [r12+32],2
 ja .Lcn_list_end
 cmp dword ptr [r12+36],1
 ja .Lcn_list_end
 cmp dword ptr [r12+40],2
 ja .Lcn_list_end
 mov rax,9007199254740991
 cmp [r12],rax
 ja .Lcn_list_end
 cmp [r12+8],rax
 ja .Lcn_list_end
 cmp [r12+16],rax
 ja .Lcn_list_end
 mov rax,[r12+24]
 test rax,rax
 jz .Lcn_list_end
 mov [rsp+24],rax
 mov eax,[r12+32]
 mov [rsp+4],eax
 mov eax,[r12+36]
 mov [rsp+8],eax
 mov eax,[r12+40]
 mov [rsp+12],eax
 mov rax,[r12+16]
 mov [rsp+16],rax
 mov rax,[r12]
 mov [rsp+48],rax
 test rax,rax
 jz .Lcn_query_task
 or qword ptr [rsp+32],2
.Lcn_query_task:
 mov rax,[r12+8]
 mov [rsp+56],rax
 test rax,rax
 jz .Lcn_query_ready
 or qword ptr [rsp+32],4
.Lcn_query_ready:
 mov ebp,7
 cmp dword ptr [rbx+24],0
 jne .Lcn_list_end
 lea rdi,[rsp+160]
 mov rsi,[rbx]
 call pg_durable_read_on_line_begin@PLT
 mov ebp,3
 test eax,eax
 jnz .Lcn_list_end
 mov rax,[rbx+8]
 mov rdi,[rax]
 mov rsi,rsp
 lea rdx,[rsp+112]
 call [rax+16]
 mov ebp,4
 cmp eax,2
 je .Lcn_list_release
 mov ebp,6
 test eax,eax
 jnz .Lcn_list_release
 cmp dword ptr [rsp],1
 je .Lcn_list_page
 xor r14d,r14d
.Lcn_list_name:
 mov rdi,[rsp+112]
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lcn_list_page
 mov rdi,[rsp+112]
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov rsi,rax
 mov rdi,[rbx+8]
 call .Lcn_add_name
 mov ebp,eax
 test eax,eax
 jnz .Lcn_list_release
 inc r14
 jmp .Lcn_list_name
.Lcn_list_page:
 lea rdi,[rip+.Lcn_page]
 call json_tokener_parse@PLT
 mov [rsp+144],rax
 mov ebp,5
 test rax,rax
 jz .Lcn_list_release
 mov rdi,rax
 lea rsi,[rip+.Lcn_items]
 mov rdx,[rsp+112]
 call .Lv_field
 test eax,eax
 jnz .Lcn_list_release
 cmp dword ptr [rsp+132],0
 je .Lcn_list_success
 call json_object_new_object@PLT
 mov [rsp+152],rax
 test rax,rax
 jz .Lcn_list_release
 mov rdi,[rsp+120]
 call json_object_new_uint64@PLT
 mov rdx,rax
 mov rdi,[rsp+152]
 lea rsi,[rip+.Lcn_after]
 call .Lcn_owned
 test eax,eax
 jnz .Lcn_list_release
 mov edi,[rsp+128]
 call json_object_new_int@PLT
 mov rdx,rax
 mov rdi,[rsp+152]
 lea rsi,[rip+.Lcn_order]
 call .Lcn_owned
 test eax,eax
 jnz .Lcn_list_release
 mov rdi,[rsp+144]
 lea rsi,[rip+.Lcn_cursor]
 mov rdx,[rsp+152]
 call .Lv_field
 test eax,eax
 jnz .Lcn_list_release
.Lcn_list_success:
 mov rax,[rsp+144]
 mov [r13],rax
 mov qword ptr [rsp+144],0
 xor ebp,ebp
.Lcn_list_release:
 lea rdi,[rsp+160]
 call pg_durable_read_on_line_end@PLT
.Lcn_list_end:
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov rdi,[rsp+144]
 call json_object_put@PLT
 mov rdi,[rsp+152]
 call json_object_put@PLT
 mov eax,ebp
 VE 176
.section .rodata
.Lv_id: .asciz "id"
.Lcn_kind: .asciz "pangu.session-info"
.Lcn_kind_key: .asciz "kind"
.Lcn_name: .asciz "name"
.Lcn_items: .asciz "items"
.Lcn_cursor: .asciz "cursor"
.Lcn_after: .asciz "after"
.Lcn_order: .asciz "order"
.Lcn_page: .asciz "{\"items\":[],\"cursor\":null}"
.section .note.GNU-stack,"",@progbits

.text
/* Coding Agent appendSessionInfo: CR/LF runs -> space, ECMAScript trim. */
.Lcn_normalize:
 VF 64
 mov rbx,rdi
 mov r12,rsi
 mov [rsp],rdx
 mov qword ptr [rdx],0
 xor r14d,r14d
 mov eax,0x7fffffff
 cmp r12,rax
 ja .Lcn_normal_invalid
 test r12,r12
 jz .Lcn_normal_allocate
 test rbx,rbx
 jz .Lcn_normal_invalid
.Lcn_normal_allocate:
 lea rdi,[r12+1]
 call malloc@PLT
 mov r14,rax
 test rax,rax
 jz .Lcn_normal_bad
 xor r13d,r13d
 xor ebp,ebp
 mov qword ptr [rsp+8],0
 mov dword ptr [rsp+16],0
.Lcn_normal_each:
 cmp r13,r12
 jae .Lcn_normal_finish
 mov al,[rbx+r13]
 cmp al,13
 je .Lcn_normal_newline
 cmp al,10
 je .Lcn_normal_newline
 mov dword ptr [rsp+16],0
 lea rdi,[rbx+r13]
 mov rsi,r12
 sub rsi,r13
 lea rdx,[rsp+32]
 call utf8proc_iterate@PLT
 test rax,rax
 jle .Lcn_normal_invalid
 mov [rsp+24],rax
 lea rdi,[rbx+r13]
 mov rsi,rax
 call pg_context_blank@PLT
 mov [rsp+40],eax
 test eax,eax
 jz .Lcn_normal_copy
 test rbp,rbp
 jz .Lcn_normal_skip
.Lcn_normal_copy:
 lea rdi,[r14+rbp]
 lea rsi,[rbx+r13]
 mov rdx,[rsp+24]
 call memcpy@PLT
 add rbp,[rsp+24]
 cmp dword ptr [rsp+40],0
 jne .Lcn_normal_skip
 mov [rsp+8],rbp
.Lcn_normal_skip:
 add r13,[rsp+24]
 jmp .Lcn_normal_each
.Lcn_normal_newline:
 cmp dword ptr [rsp+16],0
 jne .Lcn_normal_next_newline
 mov dword ptr [rsp+16],1
 test rbp,rbp
 jz .Lcn_normal_next_newline
 mov byte ptr [r14+rbp],32
 inc rbp
.Lcn_normal_next_newline:
 inc r13
 jmp .Lcn_normal_each
.Lcn_normal_finish:
 mov rdi,r14
 mov esi,[rsp+8]
 call json_object_new_string_len@PLT
 mov rcx,[rsp]
 mov [rcx],rax
 mov r12d,5
 test rax,rax
 jz .Lcn_normal_end
 xor r12d,r12d
 jmp .Lcn_normal_end
.Lcn_normal_invalid:
 mov r12d,4
 jmp .Lcn_normal_end
.Lcn_normal_bad:
 mov r12d,5
.Lcn_normal_end:
 mov rdi,r14
 call free@PLT
 mov eax,r12d
 VE 64
.globl pg_conversation_rename
.type pg_conversation_rename,@function
pg_conversation_rename:
 VF 128
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rdi,rsp
 xor eax,eax
 mov ecx,16
 rep stosq
 mov ebp,4
 test rbx,rbx
 jz .Lcn_rename_end
 test r12,r12
 jz .Lcn_rename_end
 mov rax,9007199254740991
 cmp r12,rax
 ja .Lcn_rename_end
 mov rax,[rbx]
 test rax,rax
 jz .Lcn_rename_end
 mov ebp,7
 cmp dword ptr [rax+172],1
 jne .Lcn_rename_end
 mov rdi,rax
 call pg_durable_session_idle@PLT
 mov ebp,3
 test eax,eax
 jz .Lcn_rename_end
 mov rdi,r13
 mov rsi,r14
 lea rdx,[rsp+112]
 call .Lcn_normalize
 mov ebp,eax
 test eax,eax
 jnz .Lcn_rename_end
 lea rdi,[rip+.Lcn_name_draft]
 call json_tokener_parse@PLT
 mov [rsp+104],rax
 mov ebp,5
 test rax,rax
 jz .Lcn_rename_end
 mov rdi,rax
 lea rsi,[rip+.Lcn_name]
 mov rdx,[rsp+112]
 call .Lv_field
 test eax,eax
 jnz .Lcn_rename_end
 mov rdi,[rbx]
 mov rsi,rsp
 lea rdx,[rsp+48]
 mov rcx,[rbx+8]
 mov r8,[rbx+16]
 call pg_durable_session_begin@PLT
 mov ebp,3
 test eax,eax
 jnz .Lcn_rename_end
 mov rdi,rsp
 xor esi,esi
 mov rdx,r12
 lea rcx,[rsp+96]
 call pg_record_tx_read@PLT
 mov ebp,2
 cmp eax,7
 je .Lcn_rename_abort
 mov ebp,6
 test eax,eax
 jnz .Lcn_rename_abort
 mov rdi,rsp
 mov rsi,r12
 mov rdx,[rsp+104]
 xor ecx,ecx
 mov r8,[rbx+24]
 lea r9,[rsp+120]
 call pg_record_tx_append_entry@PLT
 mov ebp,5
 test eax,eax
 jnz .Lcn_rename_abort
 mov rdi,[rbx]
 xor esi,esi
 xor edx,edx
 xor ecx,ecx
 call pg_durable_session_settle@PLT
 xor ebp,ebp
 test eax,eax
 jz .Lcn_rename_end
 mov ebp,6
 jmp .Lcn_rename_end
.Lcn_rename_abort:
 mov rdi,[rbx]
 call pg_durable_session_abort@PLT
.Lcn_rename_end:
 mov rdi,[rsp+96]
 call json_object_put@PLT
 mov rdi,[rsp+104]
 call json_object_put@PLT
 mov rdi,[rsp+112]
 call json_object_put@PLT
 mov rdi,[rsp+120]
 call json_object_put@PLT
 mov eax,ebp
 VE 128
.section .rodata
.Lcn_name_draft: .asciz "{\"kind\":\"pangu.session-info\"}"
.section .note.GNU-stack,"",@progbits

.text
.globl pg_conversation_history_service
.type pg_conversation_history_service,@function
pg_conversation_history_service:
 mov [rsi],rdi
 lea rax,[rip+pg_conversation_entries]
 mov [rsi+8],rax
 mov rax,qword ptr [rip+json_object_put@GOTPCREL]
 mov [rsi+16],rax
 ret
.globl pg_conversation_entries
.type pg_conversation_entries,@function
pg_conversation_entries:
 VF 176
 mov rbx,rdi
 mov r12,rdx
 mov r13,rcx
 mov r14,rsi
 mov rdi,rsp
 xor eax,eax
 mov ecx,22
 rep stosq
 mov ebp,4
 test r13,r13
 jz .Lcn_list_end
 mov qword ptr [r13],0
 test rbx,rbx
 jz .Lcn_list_end
 test r14,r14
 jz .Lcn_list_end
 mov rax,9007199254740991
 cmp r14,rax
 ja .Lcn_list_end
 mov dword ptr [rsp],1
 mov dword ptr [rsp+4],2
 mov qword ptr [rsp+24],64
 mov qword ptr [rsp+32],1
 mov [rsp+40],r14
 test r12,r12
 jz .Lcn_query_ready
 cmp dword ptr [r12+44],0
 jne .Lcn_list_end
 cmp dword ptr [r12+32],2
 ja .Lcn_list_end
 cmp dword ptr [r12+36],1
 ja .Lcn_list_end
 cmp dword ptr [r12+40],2
 ja .Lcn_list_end
 mov rax,9007199254740991
 cmp [r12],rax
 ja .Lcn_list_end
 cmp [r12+8],rax
 ja .Lcn_list_end
 cmp [r12+16],rax
 ja .Lcn_list_end
 mov rax,[r12+24]
 test rax,rax
 jz .Lcn_list_end
 mov [rsp+24],rax
 mov eax,[r12+32]
 mov [rsp+4],eax
 mov eax,[r12+36]
 mov [rsp+8],eax
 mov eax,[r12+40]
 mov [rsp+12],eax
 mov rax,[r12+16]
 mov [rsp+16],rax
 mov rax,[r12]
 mov [rsp+64],rax
 test rax,rax
 jz .Lce_max
 or qword ptr [rsp+32],8
.Lce_max:
 mov rax,[r12+8]
 mov [rsp+72],rax
 test rax,rax
 jz .Lcn_query_ready
 or qword ptr [rsp+32],16
 jmp .Lcn_query_ready
.section .note.GNU-stack,"",@progbits
