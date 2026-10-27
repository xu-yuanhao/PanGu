.intel_syntax noprefix
.macro JGET object,key
 mov rdi,\object
 lea rsi,[rip+.Ljl_\key]
 call json_object_object_get@PLT
.endm
.macro JSTR object,key,text
 lea rdi,[rip+.Ljl_\text]
 call json_object_new_string@PLT
 mov rdx,rax
 mov rdi,\object
 lea rsi,[rip+.Ljl_\key]
 call .Ljl_set
.endm
.macro JINT object,key,value
 mov rdi,\value
 call json_object_new_int64@PLT
 mov rdx,rax
 mov rdi,\object
 lea rsi,[rip+.Ljl_\key]
 call .Ljl_set
.endm
.macro JCOPY object,key,source
 mov rdi,\source
 lea rsi,[rip+.Ljl_\key]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get@PLT
 mov rdx,rax
 mov rdi,\object
 lea rsi,[rip+.Ljl_\key]
 call .Ljl_set
.endm
.text
/* Owned insertions release their value on failure. */
.Ljl_set:
 test rdx,rdx
 jz .Ljl_set_missing
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Ljl_set_return
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,1
.Ljl_set_return:
 pop rbx
 ret
.Ljl_set_missing:
 mov eax,1
 ret
.Ljl_array_add:
 push rbx
 mov rbx,rsi
 call json_object_array_add@PLT
 test eax,eax
 jz .Ljl_array_return
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,1
.Ljl_array_return:
 pop rbx
 ret
/* Exact JSON strings, including embedded NUL. */
.Ljl_equal:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Ljl_equal_return
 mov rdi,r12
 call strlen@PLT
 mov [rsp],rax
 mov rdi,rbx
 call json_object_get_string_len@PLT
 cmp rax,[rsp]
 jne .Ljl_equal_no
 mov rdi,rbx
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,r12
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 jmp .Ljl_equal_return
.Ljl_equal_no:
 xor eax,eax
.Ljl_equal_return:
 add rsp,8
 pop r12
 pop rbx
 ret
/* Return owned newline-terminated JSON bytes and length through out[2]. */
.Ljl_encode_line:
 push rbx
 push r12
 push r13
 sub rsp,16
 mov rbx,rsi
 mov qword ptr [rbx],0
 mov qword ptr [rbx+8],0
 xor esi,esi
 lea rdx,[rsp]
 call json_object_to_json_string_length@PLT
 test rax,rax
 jz .Ljl_line_fail
 mov r12,rax
 mov r13,[rsp]
 lea rdi,[r13+1]
 call malloc@PLT
 test rax,rax
 jz .Ljl_line_fail
 mov [rbx],rax
 mov rdi,rax
 mov rsi,r12
 mov rdx,r13
 call memcpy@PLT
 mov byte ptr [rax+r13],10
 inc r13
 mov [rbx+8],r13
 xor eax,eax
 jmp .Ljl_line_return
.Ljl_line_fail:
 mov eax,1
.Ljl_line_return:
 add rsp,16
 pop r13
 pop r12
 pop rbx
 ret
/* store, name, bytes, length. Append ownership never escapes this call. */
.Ljl_append:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rdi
 mov r12,rdx
 mov r13,rcx
 mov rsi,rsi
 mov edi,[rbx+48]
 mov edx,0x80441
 mov ecx,384
 xor eax,eax
 call openat@PLT
 test eax,eax
 js .Ljl_append_fail
 mov ebp,eax
.Ljl_write:
 test r13,r13
 jz .Ljl_write_done
 mov edi,ebp
 mov rsi,r12
 mov rdx,r13
 call write@PLT
 test rax,rax
 jg .Ljl_written
 jz .Ljl_write_failed
 call __errno_location@PLT
 cmp dword ptr [rax],4
 je .Ljl_write
 jmp .Ljl_write_failed
.Ljl_written:
 add r12,rax
 sub r13,rax
 jmp .Ljl_write
.Ljl_write_done:
 cmp dword ptr [rbx+64],0
 je .Ljl_append_close
 mov edi,ebp
 call fsync@PLT
 test eax,eax
 jnz .Ljl_write_failed
.Ljl_append_close:
 mov edi,ebp
 call close@PLT
 test eax,eax
 jnz .Ljl_append_fail
 xor eax,eax
 jmp .Ljl_append_return
.Ljl_write_failed:
 mov edi,ebp
 call close@PLT
.Ljl_append_fail:
 mov eax,1
.Ljl_append_return:
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* A file name is derived only from validated IDs. */
.Ljl_filename:
 /* buffer, kind(0=doc/1=task), id */
 mov rcx,rdx
 lea rdx,[rip+.Ljl_doc_file]
 test esi,esi
 jz .Ljl_filename_format
 lea rdx,[rip+.Ljl_task_file]
.Ljl_filename_format:
 mov esi,64
 xor eax,eax
 jmp snprintf@PLT
/* Standard metadata envelope. Returns owned object, NULL on failure. */
.Ljl_envelope:
 push rbx
 push r12
 push r13
 mov r12,rdi
 mov r13,rsi
 call json_object_new_object@PLT
 test rax,rax
 jz .Ljl_envelope_return
 mov rbx,rax
 JINT rbx,format,1
 test eax,eax
 jnz .Ljl_envelope_bad
 mov rdi,r12
 call json_object_new_string@PLT
 mov rdx,rax
 mov rdi,rbx
 lea rsi,[rip+.Ljl_type]
 call .Ljl_set
 test eax,eax
 jnz .Ljl_envelope_bad
 JINT rbx,seq,r13
 test eax,eax
 jnz .Ljl_envelope_bad
 mov rax,rbx
 jmp .Ljl_envelope_return
.Ljl_envelope_bad:
 mov rdi,rbx
 call json_object_put@PLT
 xor eax,eax
.Ljl_envelope_return:
 pop r13
 pop r12
 pop rbx
 ret

.globl pg_record_jsonl_storage
.type pg_record_jsonl_storage,@function
pg_record_jsonl_storage:
 mov rax,rdi
 lea rdx,[rip+.Ljl_commit]
 ret
.size pg_record_jsonl_storage,.-pg_record_jsonl_storage
.Ljl_commit:
 test rdi,rdi
 jz .Ljl_commit_invalid
 cmp dword ptr [rdi+52],1
 jne .Ljl_commit_invalid
 cmp dword ptr [rdi+56],0
 jne .Ljl_commit_invalid
 cmp dword ptr [rdi+60],0
 jne .Ljl_commit_invalid
 push rbx
 mov rbx,rdi
 mov dword ptr [rbx+56],1
 sub rsp,16
 mov [rsp],rdi
 lea r9,[rip+.Ljl_persist]
 call pg_record_memory_commit_persisted@PLT
 add rsp,16
 mov dword ptr [rbx+56],0
 pop rbx
 ret
.Ljl_commit_invalid:
 test r8,r8
 jz .Ljl_commit_bad_return
 mov qword ptr [r8],0
.Ljl_commit_bad_return:
 mov eax,2
 ret

/* Encoded sidecar entries: filename/bytes/length, three qwords. All encoding
   completes before the first append; marker acknowledgement precedes adoption. */
.Ljl_persist:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,184
 mov rbx,rdi
 mov r12,rsi
 mov [rsp+8],rdx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],0
 mov qword ptr [rsp+32],0
 mov qword ptr [rsp+40],0
 mov qword ptr [rsp+48],0
 mov qword ptr [rsp+64],0
 mov qword ptr [rsp+72],0
 mov rdi,r12
 call json_object_array_length@PLT
 mov [rsp+56],rax
 mov rdi,rax
 mov esi,24
 call calloc@PLT
 mov [rsp+32],rax
 test rax,rax
 jnz .Ljl_persist_array
 cmp qword ptr [rsp+56],0
 jne .Ljl_persist_error
.Ljl_persist_array:
 xor edi,edi
 call json_object_new_array_ext@PLT
 mov [rsp],rax
 test rax,rax
 jz .Ljl_persist_error
 xor r13d,r13d
 xor r14d,r14d
.Ljl_encode_each:
 cmp r13,[rsp+56]
 jae .Ljl_encode_marker
 mov rdi,r12
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rbp,rax
 JGET rbp,type
 mov [rsp+80],rax
 mov rdi,rax
 lea rsi,[rip+.Ljl_task]
 call .Ljl_equal
 test eax,eax
 jnz .Ljl_encode_task
 mov rdi,[rsp+80]
 lea rsi,[rip+.Ljl_create]
 call .Ljl_equal
 test eax,eax
 jnz .Ljl_encode_create
 mov rdi,[rsp+80]
 lea rsi,[rip+.Ljl_change]
 call .Ljl_equal
 test eax,eax
 jnz .Ljl_encode_change
.Ljl_encode_inline:
 mov rdi,rbp
 call json_object_get@PLT
 mov rsi,rax
 mov rdi,[rsp]
 call .Ljl_array_add
 test eax,eax
 jnz .Ljl_persist_error
 jmp .Ljl_encode_next
.Ljl_encode_task:
 JGET rbp,value
 mov [rsp+88],rax
 JGET rax,state
 JGET rax,status
 mov rdi,rax
 lea rsi,[rip+.Ljl_terminal]
 call .Ljl_equal
 test eax,eax
 jnz .Ljl_encode_inline
 JGET "qword ptr [rsp+88]",id
 mov rdi,rax
 call pg_document_safe_number@PLT
 mov [rsp+96],rax
 mov dword ptr [rsp+104],1
 mov rdi,rbp
 call json_object_get@PLT
 mov [rsp+24],rax
 jmp .Ljl_encode_descriptor
.Ljl_encode_create:
 JGET rbp,record
 mov [rsp+88],rax
 JGET rax,id
 mov rdi,rax
 call pg_document_safe_number@PLT
 mov [rsp+96],rax
 mov dword ptr [rsp+104],2
 jmp .Ljl_encode_document
.Ljl_encode_change:
 JGET rbp,id
 mov rdi,rax
 call pg_document_safe_number@PLT
 mov [rsp+96],rax
 mov dword ptr [rsp+104],3
.Ljl_encode_document:
 call json_object_new_object@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Ljl_persist_error
 JSTR "qword ptr [rsp+24]",type,document
 test eax,eax
 jnz .Ljl_persist_error
 JINT "qword ptr [rsp+24]",id,"qword ptr [rsp+96]"
 test eax,eax
 jnz .Ljl_persist_error
 JCOPY "qword ptr [rsp+24]",content,rbp
 test eax,eax
 jnz .Ljl_persist_error
.Ljl_encode_descriptor:
 call json_object_new_object@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Ljl_persist_error
 cmp dword ptr [rsp+104],1
 jne .Ljl_descriptor_document
 JSTR "qword ptr [rsp+16]",type,task_sidecar
 jmp .Ljl_descriptor_type
.Ljl_descriptor_document:
 cmp dword ptr [rsp+104],2
 jne .Ljl_descriptor_change
 JSTR "qword ptr [rsp+16]",type,create
 test eax,eax
 jnz .Ljl_persist_error
 JCOPY "qword ptr [rsp+16]",record,rbp
 jmp .Ljl_descriptor_type
.Ljl_descriptor_change:
 JSTR "qword ptr [rsp+16]",type,change
.Ljl_descriptor_type:
 test eax,eax
 jnz .Ljl_persist_error
 cmp dword ptr [rsp+104],2
 je .Ljl_descriptor_ordinal
 JINT "qword ptr [rsp+16]",id,"qword ptr [rsp+96]"
 test eax,eax
 jnz .Ljl_persist_error
.Ljl_descriptor_ordinal:
 JINT "qword ptr [rsp+16]",ordinal,r14
 test eax,eax
 jnz .Ljl_persist_error
 lea rdi,[rip+.Ljl_record_type]
 mov rsi,[rsp+8]
 call .Ljl_envelope
 mov [rsp+40],rax
 test rax,rax
 jz .Ljl_persist_error
 JINT "qword ptr [rsp+40]",ordinal,r14
 test eax,eax
 jnz .Ljl_persist_error
 mov rdx,[rsp+24]
 mov qword ptr [rsp+24],0
 mov rdi,[rsp+40]
 lea rsi,[rip+.Ljl_payload]
 call .Ljl_set
 test eax,eax
 jnz .Ljl_persist_error
 imul r15,r14,24
 add r15,[rsp+32]
 lea rdi,[rsp+112]
 xor esi,esi
 cmp dword ptr [rsp+104],1
 sete sil
 mov rdx,[rsp+96]
 call .Ljl_filename
 lea rdi,[rsp+112]
 call strdup@PLT
 mov [r15],rax
 test rax,rax
 jz .Ljl_persist_error
 mov rdi,[rsp+40]
 lea rsi,[r15+8]
 call .Ljl_encode_line
 test eax,eax
 jnz .Ljl_persist_error
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov qword ptr [rsp+40],0
 mov rsi,[rsp+16]
 mov qword ptr [rsp+16],0
 mov rdi,[rsp]
 call .Ljl_array_add
 test eax,eax
 jnz .Ljl_persist_error
 inc r14
.Ljl_encode_next:
 inc r13
 jmp .Ljl_encode_each
.Ljl_encode_marker:
 lea rdi,[rip+.Ljl_commit_type]
 mov rsi,[rsp+8]
 call .Ljl_envelope
 mov [rsp+40],rax
 test rax,rax
 jz .Ljl_persist_error
 mov rdx,[rsp]
 mov qword ptr [rsp],0
 mov rdi,[rsp+40]
 lea rsi,[rip+.Ljl_writes]
 call .Ljl_set
 test eax,eax
 jnz .Ljl_persist_error
 mov rdi,[rsp+40]
 lea rsi,[rsp+64]
 call .Ljl_encode_line
 test eax,eax
 jnz .Ljl_persist_error
 xor r13d,r13d
.Ljl_append_each:
 cmp r13,r14
 jae .Ljl_append_marker
 imul r15,r13,24
 add r15,[rsp+32]
 mov rdi,rbx
 mov rsi,[r15]
 mov rdx,[r15+8]
 mov rcx,[r15+16]
 call .Ljl_append
 test eax,eax
 jnz .Ljl_persist_poison
 inc r13
 jmp .Ljl_append_each
.Ljl_append_marker:
 mov rdi,rbx
 lea rsi,[rip+.Ljl_main]
 mov rdx,[rsp+64]
 mov rcx,[rsp+72]
 call .Ljl_append
 test eax,eax
 jnz .Ljl_persist_poison
 xor ebp,ebp
 jmp .Ljl_persist_cleanup
.Ljl_persist_poison:
 mov dword ptr [rbx+60],1
.Ljl_persist_error:
 mov ebp,1
.Ljl_persist_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+64]
 call free@PLT
 xor r13d,r13d
.Ljl_encoded_free:
 cmp r13,[rsp+56]
 jae .Ljl_persist_return
 cmp qword ptr [rsp+32],0
 je .Ljl_persist_return
 imul r15,r13,24
 add r15,[rsp+32]
 mov rdi,[r15]
 call free@PLT
 mov rdi,[r15+8]
 call free@PLT
 inc r13
 jmp .Ljl_encoded_free
.Ljl_persist_return:
 mov rdi,[rsp+32]
 call free@PLT
 mov eax,ebp
 add rsp,184
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* File lines are owned arrays of [parsed JSON, starting byte offset, confirmed]. */
.Ljl_load:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,72
 mov rbx,rdi
 mov r13,rdx
 mov qword ptr [r13],0
 mov [rsp+56],rsi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],0
 mov qword ptr [rsp+32],0
 xor r12d,r12d
 xor r14d,r14d
 xor edi,edi
 call json_object_new_array_ext@PLT
 mov rbp,rax
 mov r15d,4
 test rax,rax
 jz .Ljl_load_cleanup
 mov edi,[rbx+48]
 mov rsi,[rsp+56]
 mov edx,0x80002
 xor eax,eax
 call openat@PLT
 mov r15d,2
 test eax,eax
 js .Ljl_load_missing
 mov [rsp+48],eax
 mov edi,eax
 lea rsi,[rip+.Ljl_read_mode]
 call fdopen@PLT
 test rax,rax
 jz .Ljl_load_fd_error
 mov r12,rax
.Ljl_load_line:
 lea rdi,[rsp]
 lea rsi,[rsp+8]
 mov rdx,r12
 call getline@PLT
 test rax,rax
 js .Ljl_load_eof
 mov [rsp+40],rax
 mov rdx,[rsp]
 cmp byte ptr [rdx+rax-1],10
 jne .Ljl_load_torn
 dec rax
 mov r15d,3
 cmp rax,2147483647
 ja .Ljl_load_cleanup
 call json_tokener_new@PLT
 mov r15d,4
 test rax,rax
 jz .Ljl_load_cleanup
 mov [rsp+24],rax
 mov rdi,rax
 mov esi,0x11
 call json_tokener_set_flags@PLT
 mov rdi,[rsp+24]
 mov rsi,[rsp]
 mov edx,[rsp+40]
 dec edx
 call json_tokener_parse_ex@PLT
 mov [rsp+16],rax
 mov rdi,[rsp+24]
 call json_tokener_get_error@PLT
 mov r15d,3
 test eax,eax
 jnz .Ljl_load_cleanup
 mov rdi,[rsp+24]
 call json_tokener_get_parse_end@PLT
 inc rax
 cmp rax,[rsp+40]
 jne .Ljl_load_cleanup
 mov rdi,[rsp+24]
 call json_tokener_free@PLT
 mov qword ptr [rsp+24],0
 mov edi,3
 call json_object_new_array_ext@PLT
 mov [rsp+32],rax
 mov r15d,4
 test rax,rax
 jz .Ljl_load_cleanup
 mov rdi,rax
 mov rsi,[rsp+16]
 mov qword ptr [rsp+16],0
 call .Ljl_array_add
 test eax,eax
 jnz .Ljl_load_cleanup
 mov rdi,r14
 call json_object_new_int64@PLT
 test rax,rax
 jz .Ljl_load_cleanup
 mov rdi,[rsp+32]
 mov rsi,rax
 call .Ljl_array_add
 test eax,eax
 jnz .Ljl_load_cleanup
 xor edi,edi
 call json_object_new_int@PLT
 test rax,rax
 jz .Ljl_load_cleanup
 mov rdi,[rsp+32]
 mov rsi,rax
 call .Ljl_array_add
 test eax,eax
 jnz .Ljl_load_cleanup
 mov rdi,rbp
 mov rsi,[rsp+32]
 mov qword ptr [rsp+32],0
 call .Ljl_array_add
 test eax,eax
 jnz .Ljl_load_cleanup
 add r14,[rsp+40]
 jmp .Ljl_load_line
.Ljl_load_torn:
 mov rdi,r12
 call fileno@PLT
 mov edi,eax
 mov rsi,r14
 call ftruncate@PLT
 mov r15d,2
 test eax,eax
 jnz .Ljl_load_cleanup
 jmp .Ljl_load_success
.Ljl_load_eof:
 mov rdi,r12
 call ferror@PLT
 mov r15d,2
 test eax,eax
 jnz .Ljl_load_cleanup
.Ljl_load_success:
 mov [r13],rbp
 xor ebp,ebp
 xor r15d,r15d
 jmp .Ljl_load_cleanup
.Ljl_load_missing:
 call __errno_location@PLT
 cmp dword ptr [rax],2
 je .Ljl_load_success
 jmp .Ljl_load_cleanup
.Ljl_load_fd_error:
 mov edi,[rsp+48]
 call close@PLT
.Ljl_load_cleanup:
 mov rdi,[rsp]
 call free@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 test rdi,rdi
 jz .Ljl_load_wrapper
 call json_tokener_free@PLT
.Ljl_load_wrapper:
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,rbp
 call json_object_put@PLT
 test r12,r12
 jz .Ljl_load_return
 mov rdi,r12
 call fclose@PLT
 test eax,eax
 jz .Ljl_load_return
 mov rdi,[r13]
 call json_object_put@PLT
 mov qword ptr [r13],0
 mov r15d,2
.Ljl_load_return:
 mov eax,r15d
 add rsp,72
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Recovery ctx: files map, record map, native file list, main lines, store.
   File node: next/name/borrowed lines/flags/latest-base-seq/latest-base-ordinal. */
.Ljl_file:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov rdi,[rbx]
 call json_object_object_get@PLT
 test rax,rax
 jz .Ljl_file_new
 mov rdi,rax
 call json_object_get_userdata@PLT
 jmp .Ljl_file_return
.Ljl_file_new:
 mov edi,1
 mov esi,48
 call calloc@PLT
 test rax,rax
 jz .Ljl_file_return
 mov r13,rax
 mov rdi,r12
 call strdup@PLT
 mov [r13+8],rax
 test rax,rax
 jz .Ljl_file_fail
 xor edi,edi
 call json_object_new_array_ext@PLT
 mov [r13+16],rax
 test rax,rax
 jz .Ljl_file_fail
 mov rdi,rax
 mov rsi,r13
 xor edx,edx
 call json_object_set_userdata@PLT
 mov rdi,[rbx]
 mov rsi,r12
 mov rdx,[r13+16]
 call .Ljl_set
 test eax,eax
 jnz .Ljl_file_fail
 mov rax,[rbx+16]
 mov [r13],rax
 mov [rbx+16],r13
 mov rax,r13
 jmp .Ljl_file_return
.Ljl_file_fail:
 mov rdi,[r13+8]
 call free@PLT
 mov rdi,r13
 call free@PLT
 xor eax,eax
.Ljl_file_return:
 pop r13
 pop r12
 pop rbx
 ret
/* Filename must be the canonical nonnegative safe-ID sidecar basename. */
.Ljl_sidecar_name:
 push rbx
 sub rsp,96
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov dword ptr [rsp+8],0
 lea rsi,[rip+.Ljl_doc_scan]
 lea rdx,[rsp]
 lea rcx,[rsp+8]
 xor eax,eax
 call sscanf@PLT
 xor r8d,r8d
 cmp eax,1
 jne .Ljl_sidecar_task
 cmp dword ptr [rsp+8],0
 jne .Ljl_sidecar_check
.Ljl_sidecar_task:
 mov dword ptr [rsp+8],0
 mov rdi,rbx
 lea rsi,[rip+.Ljl_task_scan]
 lea rdx,[rsp]
 lea rcx,[rsp+8]
 xor eax,eax
 call sscanf@PLT
 mov r8d,1
 cmp eax,1
 jne .Ljl_sidecar_no
 cmp dword ptr [rsp+8],0
 je .Ljl_sidecar_no
.Ljl_sidecar_check:
 mov rax,9007199254740991
 cmp [rsp],rax
 ja .Ljl_sidecar_no
 lea rdi,[rsp+16]
 mov esi,r8d
 mov rdx,[rsp]
 call .Ljl_filename
 lea rdi,[rsp+16]
 mov rsi,rbx
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 jmp .Ljl_sidecar_return
.Ljl_sidecar_no:
 xor eax,eax
.Ljl_sidecar_return:
 add rsp,96
 pop rbx
 ret
/* Wrapper -> borrowed raw object. */
.Ljl_raw:
 xor esi,esi
 jmp json_object_array_get_idx@PLT
/* Object header validation, expected type at rsi. seq is returned in rdx. */
.Ljl_header:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ljl_header_bad
 JGET rbx,format
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ljl_header_bad
 cmp rax,1
 jne .Ljl_header_bad
 JGET rbx,type
 mov rdi,rax
 mov rsi,r12
 call .Ljl_equal
 test eax,eax
 jz .Ljl_header_bad
 JGET rbx,seq
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ljl_header_bad
 test rax,rax
 jz .Ljl_header_bad
 mov rdx,rax
 mov eax,1
 jmp .Ljl_header_return
.Ljl_header_bad:
 xor eax,eax
.Ljl_header_return:
 pop r13
 pop r12
 pop rbx
 ret
/* Main operation -> type code, ID in rdx, ordinal in rcx; -1 invalid. */
.Ljl_operation:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rdi
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ljl_op_bad
 JGET rbx,type
 mov rbp,rax
 xor r14d,r14d
.Ljl_op_type:
 lea rax,[rip+.Ljl_op_names]
 movsxd rsi,dword ptr [rax+r14*4]
 add rsi,rax
 mov rdi,rbp
 call .Ljl_equal
 test eax,eax
 jnz .Ljl_op_found
 inc r14
 cmp r14,8
 jb .Ljl_op_type
 jmp .Ljl_op_bad
.Ljl_op_found:
 xor r13d,r13d
 cmp r14d,3
 jbe .Ljl_op_value
 cmp r14d,6
 je .Ljl_op_record
 mov rdi,rbx
 jmp .Ljl_op_id
.Ljl_op_value:
 JGET rbx,value
 mov r12,rax
 mov rdi,r12
 cmp r14d,3
 jne .Ljl_op_id_object
 JGET r12,state
 JGET rax,status
 mov rdi,rax
 lea rsi,[rip+.Ljl_terminal]
 call .Ljl_equal
 test eax,eax
 jz .Ljl_op_bad
 mov rdi,r12
 jmp .Ljl_op_id_object
.Ljl_op_record:
 JGET rbx,record
 mov rdi,rax
.Ljl_op_id_object:
 mov r12,rdi
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ljl_op_bad
 mov rdi,r12
.Ljl_op_id:
 JGET rdi,id
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ljl_op_bad
 mov r12,rax
 cmp r14d,5
 jb .Ljl_op_ok
 JGET rbx,ordinal
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ljl_op_bad
 mov r13,rax
.Ljl_op_ok:
 mov rax,r14
 mov rdx,r12
 mov rcx,r13
 jmp .Ljl_op_return
.Ljl_op_bad:
 mov eax,-1
.Ljl_op_return:
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
/* Validate every sidecar, including unconfirmed records. */
.Ljl_record_valid:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 lea rsi,[rip+.Ljl_record_type]
 call .Ljl_header
 test eax,eax
 jz .Ljl_record_return
 JGET rbx,ordinal
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ljl_record_bad
 JGET rbx,payload
 mov r12,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ljl_record_bad
 JGET r12,type
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Ljl_task]
 call .Ljl_equal
 test eax,eax
 jz .Ljl_record_document
 JGET r12,value
 mov r12,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ljl_record_bad
 JGET r12,id
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ljl_record_bad
 JGET r12,state
 mov r12,rax
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Ljl_record_bad
 JGET r12,status
 mov rdi,rax
 lea rsi,[rip+.Ljl_terminal]
 call .Ljl_equal
 xor eax,1
 jmp .Ljl_record_return
.Ljl_record_document:
 mov rdi,r13
 lea rsi,[rip+.Ljl_document]
 call .Ljl_equal
 test eax,eax
 jz .Ljl_record_bad
 JGET r12,id
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ljl_record_bad
 JGET r12,content
 mov r12,rax
 JGET r12,version
 mov rdi,rax
 call pg_document_safe_number@PLT
 test edx,edx
 jz .Ljl_record_bad
 test rax,rax
 jz .Ljl_record_bad
 JGET r12,kind
 mov r13,rax
 mov rdi,rax
 lea rsi,[rip+.Ljl_base]
 call .Ljl_equal
 test eax,eax
 jz .Ljl_record_delta
 JGET r12,value
 mov rdi,rax
 mov esi,4
 call json_object_is_type@PLT
 jmp .Ljl_record_return
.Ljl_record_delta:
 mov rdi,r13
 lea rsi,[rip+.Ljl_delta]
 call .Ljl_equal
 test eax,eax
 jz .Ljl_record_bad
 JGET r12,ops
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 jmp .Ljl_record_return
.Ljl_record_bad:
 xor eax,eax
.Ljl_record_return:
 pop r13
 pop r12
 pop rbx
 ret
/* key buffer, filename, sequence, ordinal */
.Ljl_key:
 mov r9,rcx
 mov r8,rdx
 mov rcx,rsi
 lea rdx,[rip+.Ljl_key_format]
 mov esi,128
 xor eax,eax
 jmp snprintf@PLT
.Ljl_index_files:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,200
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov rbp,[rbx+32]
 mov edi,[rbp+48]
 lea rsi,[rip+.Ljl_dot]
 mov edx,0x90000
 xor eax,eax
 call openat@PLT
 mov r15d,2
 test eax,eax
 js .Ljl_index_return
 mov edi,eax
 mov [rsp+24],eax
 call fdopendir@PLT
 test rax,rax
 jz .Ljl_index_fd_fail
 mov [rsp],rax
.Ljl_directory_each:
 mov rdi,[rsp]
 lea rsi,[rsp+8]
 lea rdx,[rsp+24]
 call pg_jsonl_directory_next@PLT
 mov r15d,2
 test eax,eax
 js .Ljl_index_cleanup
 jz .Ljl_index_success
 cmp dword ptr [rsp+24],0
 je .Ljl_directory_each
 mov rdi,[rsp+8]
 call .Ljl_sidecar_name
 test eax,eax
 jz .Ljl_directory_reclaim
 mov rdi,rbx
 mov rsi,[rsp+8]
 call .Ljl_file
 mov r12,rax
 mov r15d,4
 test rax,rax
 jz .Ljl_index_cleanup
 mov rdi,rbp
 mov rsi,[r12+8]
 lea rdx,[rsp+16]
 call .Ljl_load
 mov r15d,eax
 test eax,eax
 jnz .Ljl_index_cleanup
 mov rdi,[rsp+16]
 mov rsi,r12
 xor edx,edx
 call json_object_set_userdata@PLT
 mov rdi,[rbx]
 mov rsi,[r12+8]
 mov rdx,[rsp+16]
 mov [r12+16],rdx
 mov qword ptr [rsp+16],0
 call .Ljl_set
 mov r15d,4
 test eax,eax
 jnz .Ljl_index_cleanup
 mov qword ptr [rsp+32],0
 mov qword ptr [rsp+40],-1
 xor r13d,r13d
.Ljl_index_record:
 mov rdi,[r12+16]
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Ljl_directory_each
 mov rdi,[r12+16]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov [rsp+48],rax
 mov rdi,rax
 call .Ljl_raw
 mov r14,rax
 mov rdi,rax
 call .Ljl_record_valid
 mov r15d,3
 test eax,eax
 jz .Ljl_index_cleanup
 JGET r14,seq
 mov rdi,rax
 call pg_document_safe_number@PLT
 mov [rsp+56],rax
 cmp rax,[rsp+32]
 jb .Ljl_index_cleanup
 JGET r14,ordinal
 mov rdi,rax
 call pg_document_safe_number@PLT
 mov [rsp+64],rax
 mov rcx,[rsp+56]
 cmp rcx,[rsp+32]
 jne .Ljl_index_ordered
 cmp rax,[rsp+40]
 jbe .Ljl_index_cleanup
.Ljl_index_ordered:
 mov [rsp+32],rcx
 mov [rsp+40],rax
 lea rdi,[rsp+72]
 mov rsi,[r12+8]
 mov rdx,rcx
 mov rcx,rax
 call .Ljl_key
 mov rdi,[rsp+48]
 call json_object_get@PLT
 mov rdx,rax
 mov rdi,[rbx+8]
 lea rsi,[rsp+72]
 call .Ljl_set
 mov r15d,4
 test eax,eax
 jnz .Ljl_index_cleanup
 inc r13
 jmp .Ljl_index_record
.Ljl_directory_reclaim:
 mov rdi,[rsp+8]
 call strlen@PLT
 cmp rax,8
 jb .Ljl_directory_each
 mov r12,rax
 mov rdi,[rsp+8]
 lea rdi,[rdi+rax-8]
 lea rsi,[rip+.Ljl_reclaim_suffix]
 call strcmp@PLT
 test eax,eax
 jnz .Ljl_directory_each
 mov rdi,[rsp+8]
 lea rsi,[r12-8]
 call strndup@PLT
 test rax,rax
 jz .Ljl_index_cleanup
 mov r12,rax
 mov rdi,rax
 call .Ljl_sidecar_name
 mov r13d,eax
 mov rdi,r12
 call free@PLT
 test r13d,r13d
 jz .Ljl_directory_each
 mov edi,[rbp+48]
 mov rsi,[rsp+8]
 xor edx,edx
 call unlinkat@PLT
 jmp .Ljl_directory_each
.Ljl_index_success:
 xor r15d,r15d
 jmp .Ljl_index_cleanup
.Ljl_index_fd_fail:
 mov edi,[rsp+24]
 call close@PLT
 jmp .Ljl_index_return
.Ljl_index_cleanup:
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp]
 call closedir@PLT
 test eax,eax
 jz .Ljl_index_return
 mov r15d,2
.Ljl_index_return:
 mov eax,r15d
 add rsp,200
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* First pass resolves final lifetimes; second locates current-only bases. */
.Ljl_plan:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,216
 mov rbx,rdi
 mov [rsp+56],esi
 mov qword ptr [rsp],0
 xor r12d,r12d
.Ljl_plan_marker:
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ljl_plan_success
 mov rdi,[rbx+24]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call .Ljl_raw
 mov r14,rax
 mov rdi,rax
 lea rsi,[rip+.Ljl_commit_type]
 call .Ljl_header
 test eax,eax
 jz .Ljl_plan_corrupt
 cmp rdx,[rsp]
 jbe .Ljl_plan_corrupt
 mov [rsp],rdx
 mov [rsp+8],rdx
 JGET r14,writes
 mov r14,rax
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Ljl_plan_corrupt
 xor r13d,r13d
.Ljl_plan_op:
 mov rdi,r14
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Ljl_plan_next_marker
 mov rdi,r14
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r15,rax
 mov rdi,rax
 call .Ljl_operation
 test eax,eax
 js .Ljl_plan_corrupt
 mov [rsp+16],eax
 mov [rsp+24],rdx
 mov [rsp+32],rcx
 cmp eax,3
 jb .Ljl_plan_next_op
 lea rdi,[rsp+64]
 xor esi,esi
 cmp eax,3
 je .Ljl_plan_task_file
 cmp eax,5
 jne .Ljl_plan_file
.Ljl_plan_task_file:
 mov esi,1
.Ljl_plan_file:
 call .Ljl_filename
 mov rdi,rbx
 lea rsi,[rsp+64]
 call .Ljl_file
 mov rbp,rax
 test rax,rax
 jz .Ljl_plan_alloc
 cmp dword ptr [rsp+56],0
 jne .Ljl_plan_base
 mov eax,[rsp+16]
 cmp eax,3
 je .Ljl_plan_terminal
 cmp eax,5
 je .Ljl_plan_live
 cmp eax,4
 je .Ljl_plan_retired
 cmp eax,6
 jne .Ljl_plan_next_op
 JGET r15,record
 mov [rsp+40],rax
 JGET rax,scope
 JGET rax,kind
 mov rdi,rax
 lea rsi,[rip+.Ljl_conversation]
 call .Ljl_equal
 test eax,eax
 jz .Ljl_plan_current
 JGET "qword ptr [rsp+40]",history
 mov rdi,rax
 lea rsi,[rip+.Ljl_latest]
 call .Ljl_equal
 test eax,eax
 jz .Ljl_plan_next_op
.Ljl_plan_current:
 or qword ptr [rbp+24],1
 jmp .Ljl_plan_next_op
.Ljl_plan_terminal:
 or qword ptr [rbp+24],4
 jmp .Ljl_plan_next_op
.Ljl_plan_live:
 and qword ptr [rbp+24],-5
 jmp .Ljl_plan_next_op
.Ljl_plan_retired:
 or qword ptr [rbp+24],2
 jmp .Ljl_plan_next_op
.Ljl_plan_base:
 cmp dword ptr [rsp+16],6
 jb .Ljl_plan_next_op
 test qword ptr [rbp+24],1
 jz .Ljl_plan_next_op
 lea rdi,[rsp+64]
 mov rsi,[rbp+8]
 mov rdx,[rsp+8]
 mov rcx,[rsp+32]
 call .Ljl_key
 mov rdi,[rbx+8]
 lea rsi,[rsp+64]
 call json_object_object_get@PLT
 test rax,rax
 jz .Ljl_plan_next_op
 mov rdi,rax
 call .Ljl_raw
 JGET rax,payload
 mov [rsp+40],rax
 JGET rax,type
 mov rdi,rax
 lea rsi,[rip+.Ljl_document]
 call .Ljl_equal
 test eax,eax
 jz .Ljl_plan_next_op
 JGET "qword ptr [rsp+40]",id
 mov rdi,rax
 call pg_document_safe_number@PLT
 cmp rax,[rsp+24]
 jne .Ljl_plan_next_op
 JGET "qword ptr [rsp+40]",content
 JGET rax,kind
 mov rdi,rax
 lea rsi,[rip+.Ljl_base]
 call .Ljl_equal
 test eax,eax
 jz .Ljl_plan_next_op
 mov rax,[rsp+8]
 cmp rax,[rbp+32]
 jb .Ljl_plan_next_op
 ja .Ljl_plan_save_base
 mov rcx,[rsp+32]
 cmp rcx,[rbp+40]
 jbe .Ljl_plan_next_op
.Ljl_plan_save_base:
 mov [rbp+32],rax
 mov rcx,[rsp+32]
 mov [rbp+40],rcx
.Ljl_plan_next_op:
 inc r13
 jmp .Ljl_plan_op
.Ljl_plan_next_marker:
 inc r12
 jmp .Ljl_plan_marker
.Ljl_plan_corrupt:
 mov eax,3
 jmp .Ljl_plan_return
.Ljl_plan_alloc:
 mov eax,4
 jmp .Ljl_plan_return
.Ljl_plan_success:
 xor eax,eax
.Ljl_plan_return:
 add rsp,216
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Confirm once. Missing optional content returns NULL with edx=0. */
.Ljl_confirm:
 push rbx
 push r12
 push r13
 sub rsp,128
 mov rbx,rdi
 mov r12,rsi
 mov r13d,r8d
 mov rdi,rsp
 mov rsi,[r12+8]
 call .Ljl_key
 mov rdi,[rbx+8]
 mov rsi,rsp
 call json_object_object_get@PLT
 test rax,rax
 jz .Ljl_confirm_missing
 mov r12,rax
 mov rdi,rax
 mov esi,2
 call json_object_array_get_idx@PLT
 mov r13,rax
 mov rdi,rax
 call json_object_get_int@PLT
 test eax,eax
 jnz .Ljl_confirm_bad
 mov rdi,r13
 mov esi,1
 call json_object_set_int@PLT
 mov rdi,r12
 call .Ljl_raw
 xor edx,edx
 jmp .Ljl_confirm_return
.Ljl_confirm_missing:
 xor edx,edx
 test r13d,r13d
 jnz .Ljl_confirm_return
.Ljl_confirm_bad:
 mov edx,3
 xor eax,eax
.Ljl_confirm_return:
 add rsp,128
 pop r13
 pop r12
 pop rbx
 ret

.Ljl_truncate:
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdx
 mov edi,[rdi+48]
 mov edx,0x80002
 xor eax,eax
 call openat@PLT
 test eax,eax
 js .Ljl_truncate_bad
 mov r12d,eax
 mov edi,eax
 mov rsi,rbx
 call ftruncate@PLT
 mov ebx,eax
 mov edi,r12d
 call close@PLT
 or eax,ebx
 setne al
 movzx eax,al
 jmp .Ljl_truncate_return
.Ljl_truncate_bad:
 mov eax,1
.Ljl_truncate_return:
 add rsp,8
 pop r12
 pop rbx
 ret
.Ljl_adopt:
 ret
.Ljl_replay:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,232
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+72],0
 mov qword ptr [rsp+80],0
 xor r12d,r12d
.Ljl_replay_marker:
 mov rdi,[rbx+24]
 call json_object_array_length@PLT
 cmp r12,rax
 jae .Ljl_replay_success
 mov rdi,[rbx+24]
 mov rsi,r12
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call .Ljl_raw
 mov r15,rax
 JGET r15,seq
 mov rdi,rax
 call pg_document_safe_number@PLT
 mov [rsp+8],rax
 JGET r15,writes
 mov r14,rax
 xor edi,edi
 call json_object_new_array_ext@PLT
 mov [rsp],rax
 test rax,rax
 jz .Ljl_replay_alloc
 xor r13d,r13d
.Ljl_replay_op:
 mov rdi,r14
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Ljl_replay_commit
 mov rdi,r14
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r15,rax
 mov rdi,rax
 call .Ljl_operation
 mov [rsp+24],eax
 mov [rsp+32],rdx
 mov [rsp+40],rcx
 cmp eax,5
 jb .Ljl_replay_inline
 lea rdi,[rsp+96]
 xor esi,esi
 cmp eax,5
 sete sil
 call .Ljl_filename
 mov rdi,rbx
 lea rsi,[rsp+96]
 call .Ljl_file
 mov rbp,rax
 test rax,rax
 jz .Ljl_replay_alloc
 mov dword ptr [rsp+56],0
 cmp dword ptr [rsp+24],5
 jne .Ljl_replay_doc_optional
 test qword ptr [rbp+24],4
 jz .Ljl_replay_confirm
 mov dword ptr [rsp+56],1
 jmp .Ljl_replay_confirm
.Ljl_replay_doc_optional:
 test qword ptr [rbp+24],1
 jz .Ljl_replay_confirm
 test qword ptr [rbp+24],2
 jnz .Ljl_replay_optional
 mov rax,[rsp+8]
 cmp rax,[rbp+32]
 jb .Ljl_replay_optional
 jne .Ljl_replay_confirm
 mov rax,[rsp+40]
 cmp rax,[rbp+40]
 jae .Ljl_replay_confirm
.Ljl_replay_optional:
 mov dword ptr [rsp+56],1
.Ljl_replay_confirm:
 mov rdi,rbx
 mov rsi,rbp
 mov rdx,[rsp+8]
 mov rcx,[rsp+40]
 mov r8d,[rsp+56]
 call .Ljl_confirm
 test edx,edx
 jnz .Ljl_replay_corrupt
 test rax,rax
 jz .Ljl_replay_absent
 JGET rax,payload
 mov [rsp+64],rax
 JGET rax,type
 mov rdi,rax
 lea rsi,[rip+.Ljl_document]
 cmp dword ptr [rsp+24],5
 jne .Ljl_replay_payload_type
 lea rsi,[rip+.Ljl_task]
.Ljl_replay_payload_type:
 call .Ljl_equal
 test eax,eax
 jz .Ljl_replay_corrupt
 mov rdi,[rsp+64]
 cmp dword ptr [rsp+24],5
 jne .Ljl_replay_payload_id
 JGET rdi,value
 mov rdi,rax
.Ljl_replay_payload_id:
 JGET rdi,id
 mov rdi,rax
 call pg_document_safe_number@PLT
 cmp rax,[rsp+32]
 jne .Ljl_replay_corrupt
 cmp dword ptr [rsp+24],5
 je .Ljl_replay_task
 JGET "qword ptr [rsp+64]",content
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+80],rax
 cmp dword ptr [rsp+24],6
 jne .Ljl_replay_document
 JGET rax,kind
 mov rdi,rax
 lea rsi,[rip+.Ljl_base]
 call .Ljl_equal
 test eax,eax
 jz .Ljl_replay_corrupt
 jmp .Ljl_replay_document
.Ljl_replay_absent:
 cmp dword ptr [rsp+24],5
 jne .Ljl_replay_document
.Ljl_replay_task:
 cmp dword ptr [rsp+56],0
 jne .Ljl_replay_next_op
 call json_object_new_object@PLT
 mov [rsp+72],rax
 test rax,rax
 jz .Ljl_replay_alloc
 JSTR "qword ptr [rsp+72]",type,task
 test eax,eax
 jnz .Ljl_replay_alloc
 JCOPY "qword ptr [rsp+72]",value,"qword ptr [rsp+64]"
 test eax,eax
 jnz .Ljl_replay_alloc
 jmp .Ljl_replay_add_new
.Ljl_replay_document:
 cmp dword ptr [rsp+56],0
 je .Ljl_replay_copy_descriptor
 mov rdi,[rsp+80]
 call json_object_put@PLT
 mov qword ptr [rsp+80],0
 cmp dword ptr [rsp+24],7
 je .Ljl_replay_next_op
 call json_object_new_object@PLT
 mov [rsp+80],rax
 test rax,rax
 jz .Ljl_replay_alloc
 JSTR "qword ptr [rsp+80]",kind,base
 test eax,eax
 jnz .Ljl_replay_alloc
 JINT "qword ptr [rsp+80]",version,1
 test eax,eax
 jnz .Ljl_replay_alloc
 call json_object_new_object@PLT
 mov rdx,rax
 mov rdi,[rsp+80]
 lea rsi,[rip+.Ljl_value]
 call .Ljl_set
 test eax,eax
 jnz .Ljl_replay_alloc
.Ljl_replay_copy_descriptor:
 mov rdi,r15
 lea rsi,[rsp+72]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Ljl_replay_alloc
 mov rdi,[rsp+72]
 lea rsi,[rip+.Ljl_ordinal]
 call json_object_object_del@PLT
 mov rdi,[rsp+72]
 lea rsi,[rip+.Ljl_content]
 mov rdx,[rsp+80]
 mov qword ptr [rsp+80],0
 call .Ljl_set
 test eax,eax
 jnz .Ljl_replay_alloc
.Ljl_replay_add_new:
 mov rsi,[rsp+72]
 mov qword ptr [rsp+72],0
 mov rdi,[rsp]
 call .Ljl_array_add
 test eax,eax
 jnz .Ljl_replay_alloc
 jmp .Ljl_replay_next_op
.Ljl_replay_inline:
 mov rdi,r15
 call json_object_get@PLT
 mov rsi,rax
 mov rdi,[rsp]
 call .Ljl_array_add
 test eax,eax
 jnz .Ljl_replay_alloc
.Ljl_replay_next_op:
 inc r13
 jmp .Ljl_replay_op
.Ljl_replay_commit:
 mov rbp,[rbx+32]
 mov rax,[rsp+8]
 mov [rbp+32],rax
 mov rdi,rbp
 call pg_record_memory_storage@PLT
 mov r11,rdx
 mov rdi,rax
 mov rsi,[rsp]
 lea rdx,[rip+.Ljl_adopt]
 xor ecx,ecx
 lea r8,[rsp+88]
 call r11
 cmp eax,3
 je .Ljl_replay_alloc
 test eax,eax
 jnz .Ljl_replay_corrupt
 mov rdi,[rsp]
 call json_object_put@PLT
 mov qword ptr [rsp],0
 inc r12
 jmp .Ljl_replay_marker
.Ljl_replay_corrupt:
 mov ebp,3
 jmp .Ljl_replay_cleanup
.Ljl_replay_alloc:
 mov ebp,4
 jmp .Ljl_replay_cleanup
.Ljl_replay_success:
 xor ebp,ebp
.Ljl_replay_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+72]
 call json_object_put@PLT
 mov rdi,[rsp+80]
 call json_object_put@PLT
 mov eax,ebp
 add rsp,232
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.Ljl_recover_tails:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov rbx,rdi
 mov r12,[rbx+16]
.Ljl_tail_file:
 test r12,r12
 jz .Ljl_tail_success
 xor r13d,r13d
 mov r14,-1
.Ljl_tail_record:
 mov rdi,[r12+16]
 call json_object_array_length@PLT
 cmp r13,rax
 jae .Ljl_tail_truncate
 mov rdi,[r12+16]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rbp,rax
 mov rdi,rax
 mov esi,2
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_int@PLT
 test eax,eax
 jz .Ljl_tail_unconfirmed
 cmp r14,-1
 jne .Ljl_tail_corrupt
 jmp .Ljl_tail_next
.Ljl_tail_unconfirmed:
 cmp r14,-1
 jne .Ljl_tail_next
 mov rdi,rbp
 mov esi,1
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_int64@PLT
 mov r14,rax
.Ljl_tail_next:
 inc r13
 jmp .Ljl_tail_record
.Ljl_tail_truncate:
 cmp r14,-1
 je .Ljl_tail_next_file
 mov rdi,[rbx+32]
 mov rsi,[r12+8]
 mov rdx,r14
 call .Ljl_truncate
 test eax,eax
 jnz .Ljl_tail_io
.Ljl_tail_next_file:
 mov r12,[r12]
 jmp .Ljl_tail_file
.Ljl_tail_corrupt:
 mov eax,3
 jmp .Ljl_tail_return
.Ljl_tail_io:
 mov eax,2
 jmp .Ljl_tail_return
.Ljl_tail_success:
 xor eax,eax
.Ljl_tail_return:
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Maintenance is best effort after the committed state has recovered. */
.Ljl_reclaim_recovered:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,120
 mov rbx,rdi
 mov r12,[rbx+16]
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov rbp,[rbx+32]
 cmp dword ptr [rbp+64],0
 je .Ljl_reclaim_file_each
 mov edi,[rbp+48]
 lea rsi,[rip+.Ljl_main]
 mov edx,0x80000
 xor eax,eax
 call openat@PLT
 test eax,eax
 js .Ljl_reclaim_done
 mov r13d,eax
 mov edi,eax
 call fsync@PLT
 mov r14d,eax
 mov edi,r13d
 call close@PLT
 test r14d,r14d
 jnz .Ljl_reclaim_done
.Ljl_reclaim_file_each:
 test r12,r12
 jz .Ljl_reclaim_done
 mov rdi,[r12+16]
 call json_object_array_length@PLT
 mov r15,rax
 test rax,rax
 jz .Ljl_reclaim_next_file
 test qword ptr [r12+24],4
 jnz .Ljl_reclaim_remove
 mov rax,[r12+24]
 and eax,3
 cmp eax,3
 je .Ljl_reclaim_remove
 test eax,1
 jz .Ljl_reclaim_next_file
 cmp qword ptr [r12+32],0
 je .Ljl_reclaim_next_file
 xor r13d,r13d
 xor r14d,r14d
.Ljl_reclaim_count:
 cmp r13,r15
 jae .Ljl_reclaim_counted
 mov rdi,[r12+16]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call .Ljl_raw
 JGET rax,seq
 mov rdi,rax
 call pg_document_safe_number@PLT
 cmp rax,[r12+32]
 jb .Ljl_reclaim_count_drop
 ja .Ljl_reclaim_count_next
 mov rdi,[r12+16]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call .Ljl_raw
 JGET rax,ordinal
 mov rdi,rax
 call pg_document_safe_number@PLT
 cmp rax,[r12+40]
 jae .Ljl_reclaim_count_next
.Ljl_reclaim_count_drop:
 inc r14
.Ljl_reclaim_count_next:
 inc r13
 jmp .Ljl_reclaim_count
.Ljl_reclaim_counted:
 test r14,r14
 jz .Ljl_reclaim_next_file
 lea rdi,[rsp+32]
 mov esi,80
 lea rdx,[rip+.Ljl_reclaim_file]
 mov rcx,[r12+8]
 xor eax,eax
 call snprintf@PLT
 mov edi,[rbp+48]
 lea rsi,[rsp+32]
 mov edx,0x80241
 mov ecx,384
 xor eax,eax
 call openat@PLT
 test eax,eax
 js .Ljl_reclaim_next_file
 mov edi,eax
 call close@PLT
 test eax,eax
 jnz .Ljl_reclaim_next_file
 xor r13d,r13d
.Ljl_reclaim_copy:
 cmp r13,r15
 jae .Ljl_reclaim_rename
 mov rdi,[r12+16]
 mov rsi,r13
 call json_object_array_get_idx@PLT
 mov r14,rax
 mov rdi,rax
 mov esi,2
 call json_object_array_get_idx@PLT
 mov rdi,rax
 call json_object_get_int@PLT
 test eax,eax
 jz .Ljl_reclaim_copy_next
 mov rdi,r14
 call .Ljl_raw
 JGET rax,seq
 mov rdi,rax
 call pg_document_safe_number@PLT
 cmp rax,[r12+32]
 jb .Ljl_reclaim_copy_next
 ja .Ljl_reclaim_copy_line
 mov rdi,r14
 call .Ljl_raw
 JGET rax,ordinal
 mov rdi,rax
 call pg_document_safe_number@PLT
 cmp rax,[r12+40]
 jb .Ljl_reclaim_copy_next
.Ljl_reclaim_copy_line:
 mov rdi,r14
 call .Ljl_raw
 mov rdi,rax
 mov rsi,rsp
 call .Ljl_encode_line
 test eax,eax
 jnz .Ljl_reclaim_next_file
 mov rdi,rbp
 lea rsi,[rsp+32]
 mov rdx,[rsp]
 mov rcx,[rsp+8]
 call .Ljl_append
 mov r14d,eax
 mov rdi,[rsp]
 call free@PLT
 mov qword ptr [rsp],0
 test r14d,r14d
 jnz .Ljl_reclaim_next_file
.Ljl_reclaim_copy_next:
 inc r13
 jmp .Ljl_reclaim_copy
.Ljl_reclaim_rename:
 mov edi,[rbp+48]
 lea rsi,[rsp+32]
 mov edx,edi
 mov rcx,[r12+8]
 call renameat@PLT
 jmp .Ljl_reclaim_next_file
.Ljl_reclaim_remove:
 mov edi,[rbp+48]
 mov rsi,[r12+8]
 xor edx,edx
 call unlinkat@PLT
.Ljl_reclaim_next_file:
 mov r12,[r12]
 jmp .Ljl_reclaim_file_each
.Ljl_reclaim_done:
 mov rdi,[rsp]
 call free@PLT
 add rsp,120
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.Ljl_recover:
 push rbx
 push r12
 push r13
 sub rsp,48
 mov rbx,rdi
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],0
 mov [rsp+32],rdi
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Ljl_recover_alloc
 call json_object_new_object@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Ljl_recover_alloc
 mov rdi,rbx
 lea rsi,[rip+.Ljl_main]
 lea rdx,[rsp+24]
 call .Ljl_load
 test eax,eax
 jnz .Ljl_recover_cleanup
 mov rdi,rsp
 call .Ljl_index_files
 test eax,eax
 jnz .Ljl_recover_cleanup
 mov rdi,rsp
 xor esi,esi
 call .Ljl_plan
 test eax,eax
 jnz .Ljl_recover_cleanup
 mov rdi,rsp
 mov esi,1
 call .Ljl_plan
 test eax,eax
 jnz .Ljl_recover_cleanup
 mov rdi,rsp
 call .Ljl_replay
 test eax,eax
 jnz .Ljl_recover_cleanup
 mov rdi,rsp
 call .Ljl_recover_tails
 test eax,eax
 jnz .Ljl_recover_cleanup
 mov rdi,rsp
 call .Ljl_reclaim_recovered
 xor eax,eax
 jmp .Ljl_recover_cleanup
.Ljl_recover_alloc:
 mov eax,4
.Ljl_recover_cleanup:
 mov r13d,eax
 mov r12,[rsp+16]
.Ljl_file_cleanup:
 test r12,r12
 jz .Ljl_recover_json_cleanup
 mov rbx,[r12]
 mov rdi,[r12+8]
 call free@PLT
 mov rdi,r12
 call free@PLT
 mov r12,rbx
 jmp .Ljl_file_cleanup
.Ljl_recover_json_cleanup:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov eax,r13d
 add rsp,48
 pop r13
 pop r12
 pop rbx
 ret

.globl pg_record_jsonl_open
.type pg_record_jsonl_open,@function
pg_record_jsonl_open:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx,rdi
 mov r12,rsi
 mov r13d,edx
 mov r15d,1
 test rbx,rbx
 jz .Ljl_open_return
 test r12,r12
 jz .Ljl_open_return
 cmp r13d,1
 ja .Ljl_open_return
 cmp dword ptr [rbx+52],0
 jne .Ljl_open_return
 cmp dword ptr [rbx+56],0
 jne .Ljl_open_return
 call pg_record_memory_open@PLT
 test eax,eax
 jnz .Ljl_open_return
 mov dword ptr [rbx+48],-1
 mov dword ptr [rbx+56],1
 mov dword ptr [rbx+60],0
 mov [rbx+64],r13d
 mov rdi,r12
 mov esi,448
 call mkdir@PLT
 test eax,eax
 jz .Ljl_open_directory
 call __errno_location@PLT
 cmp dword ptr [rax],17
 jne .Ljl_open_io
.Ljl_open_directory:
 mov rdi,r12
 mov esi,0x90000
 xor eax,eax
 call open@PLT
 test eax,eax
 js .Ljl_open_io
 mov [rbx+48],eax
 mov edi,eax
 mov esi,6
 call flock@PLT
 test eax,eax
 jnz .Ljl_open_io
 mov rdi,rbx
 call .Ljl_recover
 mov r15d,eax
 test eax,eax
 jnz .Ljl_open_cleanup
 mov dword ptr [rbx+52],1
 mov dword ptr [rbx+56],0
 jmp .Ljl_open_return
.Ljl_open_io:
 mov r15d,2
.Ljl_open_cleanup:
 cmp dword ptr [rbx+48],0
 jl .Ljl_open_memory_cleanup
 mov edi,[rbx+48]
 call close@PLT
 mov dword ptr [rbx+48],-1
.Ljl_open_memory_cleanup:
 mov rdi,rbx
 call pg_record_memory_close@PLT
 mov dword ptr [rbx+56],0
.Ljl_open_return:
 mov eax,r15d
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size pg_record_jsonl_open,.-pg_record_jsonl_open

.globl pg_record_jsonl_close
.type pg_record_jsonl_close,@function
pg_record_jsonl_close:
 mov eax,1
 test rdi,rdi
 jz .Ljl_close_return
 cmp dword ptr [rdi+56],0
 jne .Ljl_close_return
 cmp dword ptr [rdi+40],0
 jne .Ljl_close_return
 xor eax,eax
 cmp dword ptr [rdi+52],0
 je .Ljl_close_return
 push rbx
 push r12
 sub rsp,8
 mov rbx,rdi
 mov edi,[rbx+48]
 call close@PLT
 mov r12d,eax
 mov dword ptr [rbx+48],-1
 mov dword ptr [rbx+52],0
 mov rdi,rbx
 call pg_record_memory_close@PLT
 xor eax,eax
 test r12d,r12d
 setne al
 shl eax,1
 add rsp,8
 pop r12
 pop rbx
.Ljl_close_return:
 ret
.size pg_record_jsonl_close,.-pg_record_jsonl_close

.section .rodata
.Ljl_type: .asciz "type"
.Ljl_value: .asciz "value"
.Ljl_record: .asciz "record"
.Ljl_payload: .asciz "payload"
.Ljl_writes: .asciz "writes"
.Ljl_id: .asciz "id"
.Ljl_seq: .asciz "seq"
.Ljl_format: .asciz "format"
.Ljl_ordinal: .asciz "ordinal"
.Ljl_content: .asciz "content"
.Ljl_state: .asciz "state"
.Ljl_status: .asciz "status"
.Ljl_kind: .asciz "kind"
.Ljl_version: .asciz "version"
.Ljl_ops: .asciz "ops"
.Ljl_scope: .asciz "scope"
.Ljl_history: .asciz "history"
.Ljl_task: .asciz "task"
.Ljl_task_sidecar: .asciz "task.sidecar"
.Ljl_document: .asciz "document"
.Ljl_create: .asciz "document.create"
.Ljl_change: .asciz "document.change"
.Ljl_retire: .asciz "document.retire"
.Ljl_conversation: .asciz "conversation"
.Ljl_entry: .asciz "entry"
.Ljl_submission: .asciz "submission"
.Ljl_terminal: .asciz "terminal"
.Ljl_latest: .asciz "latest"
.Ljl_base: .asciz "base"
.Ljl_delta: .asciz "delta"
.Ljl_record_type: .asciz "record"
.Ljl_commit_type: .asciz "commit"
.Ljl_main: .asciz "main.jsonl"
.Ljl_dot: .asciz "."
.Ljl_read_mode: .asciz "r+"
.Ljl_doc_file: .asciz "doc-%llu.jsonl"
.Ljl_task_file: .asciz "task-%llu.jsonl"
.Ljl_doc_scan: .asciz "doc-%llu.jsonl%n"
.Ljl_task_scan: .asciz "task-%llu.jsonl%n"
.Ljl_key_format: .asciz "%s:%llu:%llu"
.Ljl_reclaim_suffix: .asciz ".reclaim"
.Ljl_reclaim_file: .asciz "%s.reclaim"
.Ljl_op_names:
 .long .Ljl_conversation-.Ljl_op_names,.Ljl_entry-.Ljl_op_names,.Ljl_submission-.Ljl_op_names,.Ljl_task-.Ljl_op_names
 .long .Ljl_retire-.Ljl_op_names,.Ljl_task_sidecar-.Ljl_op_names,.Ljl_create-.Ljl_op_names,.Ljl_change-.Ljl_op_names
.section .note.GNU-stack,"",@progbits
