#include "tools_layout.h"
.intel_syntax noprefix
#define P_ROOT 0
#define P_INNER 8
#define P_PATH 16
#define P_PATH_N 24
#define P_COUNT 32
#define P_EDITS 40
#define P_CAP 48
#define P_BYTES 56
#define FIRST_LINE 56
#define CONTENT 64
#define CONTENT_CAP 72
#define RESULT 80
#define RESULT_CAP 88
#define IO_SIZE 96
#define TREE 104
#define ENCODED 112
#define ENCODED_N 120
#define FRAME 152
.section .rodata
.Lname: .asciz "edit"
.Ldescription: .asciz "Edit one file using a relative or absolute POSIX path using edits: an array of {oldText,newText}. Paths support home expansion, file URLs and lexical dot segments. Targets must be unique, non-overlapping regions of the original file. Merge changes to the same block. Preserves BOM, line endings and untouched line blocks."
.Lschema: .asciz "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"edits\":{\"type\":\"array\",\"minItems\":1,\"items\":{\"type\":\"object\",\"properties\":{\"oldText\":{\"type\":\"string\"},\"newText\":{\"type\":\"string\"}},\"required\":[\"oldText\",\"newText\"],\"additionalProperties\":false}}},\"required\":[\"path\",\"edits\"],\"additionalProperties\":false}"
.Lpath: .asciz "path"
.Ledits: .asciz "edits"
.Lold: .asciz "oldText"
.Lnew: .asciz "newText"
.Lcontent: .asciz "content"
.Lread: .asciz "read"
.Lwrite: .asciz "write"
.Linvalid: .asciz "Edit arguments invalid"
.Lread_error: .asciz "Could not read file for edit"
.Lwrite_error: .asciz "Edit write failed; file may have partially changed"
.Lquota: .asciz "Edit output or arguments exceed native bounds"
.Lmissing: .asciz "Edit target not found"
.Lambiguous: .asciz "Edit target is ambiguous; provide more context"
.Loverlap: .asciz "Edit targets overlap; merge them"
.Lnochange: .asciz "Edit produced no changes"
.Lmemory: .asciz "Edit memory allocation failed"
.Lok: .asciz "Successfully replaced %zu block(s) in %s."
.Lsummary_error: .asciz "Edit succeeded; could not format change summary"
.Lread_limit: .asciz "read output limit exceeded"
.set LREAD_LIMIT_N, 26
.text

/* Strict bounded JSON parse; caller owns the returned object. */
.type .Ljson,@function
.Ljson:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov rbx,rdi
 mov r12,rsi
 xor r13d,r13d
 xor r14d,r14d
 test rbx,rbx
 jz .Ljson_bad
 test r12,r12
 jz .Ljson_bad
 cmp r12,PG_TL_ARGUMENT_LIMIT
 jae .Ljson_bad
 xor esi,esi
 mov rdx,r12
 call memchr@PLT
 test rax,rax
 jnz .Ljson_bad
 call json_tokener_new@PLT
 test rax,rax
 jz .Ljson_bad
 mov r13,rax
 mov rdi,rax
 mov esi,17
 call json_tokener_set_flags@PLT
 mov rdi,r13
 mov rsi,rbx
 mov edx,r12d
 call json_tokener_parse_ex@PLT
 mov r14,rax
 test rax,rax
 jz .Ljson_bad
 mov rdi,r13
 call json_tokener_get_error@PLT
 test eax,eax
 jnz .Ljson_bad
 mov rdi,r13
 call json_tokener_get_parse_end@PLT
 mov r15,rax
.Ljson_tail:
 cmp r15,r12
 jae .Ljson_good
 movzx eax,byte ptr [rbx+r15]
 cmp eax,32
 je .Ljson_next
 cmp eax,9
 je .Ljson_next
 cmp eax,10
 je .Ljson_next
 cmp eax,13
 jne .Ljson_bad
.Ljson_next:
 inc r15
 jmp .Ljson_tail
.Ljson_good:
 mov rdi,r13
 call json_tokener_free@PLT
 mov rax,r14
 jmp .Ljson_return
.Ljson_bad:
 test r14,r14
 jz .Ljson_free_token
 mov rdi,r14
 call json_object_put@PLT
.Ljson_free_token:
 test r13,r13
 jz .Ljson_zero
 mov rdi,r13
 call json_tokener_free@PLT
.Ljson_zero:
 xor eax,eax
.Ljson_return:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size .Ljson,.-.Ljson

/* Borrow one decoded UTF-8 string into a slice, refusing embedded NUL. */
.Lstring:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 test rbx,rbx
 jz .Lstring_bad
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lstring_bad
 mov rdi,rbx
 call json_object_get_string_len@PLT
 mov r13d,eax
 cmp r13,PG_TL_ARGUMENT_LIMIT
 jae .Lstring_bad
 mov rdi,rbx
 call json_object_get_string@PLT
 mov [r12],rax
 mov [r12+8],r13
 mov rdi,rax
 xor esi,esi
 mov rdx,r13
 call memchr@PLT
 test rax,rax
 jnz .Lstring_bad
 xor eax,eax
 jmp .Lstring_return
.Lstring_bad:
 mov eax,1
.Lstring_return:
 pop r13
 pop r12
 pop rbx
 ret

/* Append old/new JSON strings without modifying the owned source trees. */
.Lpair:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rdx
 mov r13,rsi
 mov rax,[rbx+P_COUNT]
 cmp rax,[rbx+P_CAP]
 jb .Lpair_slot
 mov rsi,[rbx+P_CAP]
 add rsi,rsi
 jnz .Lpair_grow
 mov esi,8
.Lpair_grow:
 mov [rbx+P_CAP],rsi
 shl rsi,5
 mov rdi,[rbx+P_EDITS]
 call realloc@PLT
 test rax,rax
 jz .Lpair_bad
 mov [rbx+P_EDITS],rax
.Lpair_slot:
 mov rdi,r13
 mov rax,[rbx+P_COUNT]
 shl rax,5
 mov r13,[rbx+P_EDITS]
 add r13,rax
 mov rsi,r13
 call .Lstring
 test eax,eax
 jnz .Lpair_bad
 cmp qword ptr [r13+8],0
 je .Lpair_bad
 mov rdi,r12
 lea rsi,[r13+16]
 call .Lstring
 test eax,eax
 jnz .Lpair_bad
 inc qword ptr [rbx+P_COUNT]
 xor eax,eax
 jmp .Lpair_return
.Lpair_bad:
 mov eax,1
.Lpair_return:
 pop r13
 pop r12
 pop rbx
 ret

.Lobject_pair:
 push rbx
 push r12
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 mov rdi,r12
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lobject_shape_bad
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov rdi,r12
 lea rsi,[rip+.Lold]
 mov rdx,rsp
 call json_object_object_get_ex@PLT
 mov rdi,r12
 lea rsi,[rip+.Lnew]
 lea rdx,[rsp+8]
 call json_object_object_get_ex@PLT
 mov rdi,[rsp]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lobject_shape_bad
 mov rdi,[rsp+8]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lobject_shape_bad
 mov rdi,r12
 call json_object_object_length@PLT
 cmp eax,2
 jne .Lobject_bad
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,[rsp+8]
 call .Lpair
 jmp .Lobject_return
.Lobject_bad:
 mov eax,1
 jmp .Lobject_return
.Lobject_shape_bad:
 mov eax,2
.Lobject_return:
 add rsp,24
 pop r12
 pop rbx
 ret

/* Parsed state owns the edit vector and ROOT/INNER; slices borrow the trees. */
.Lparsed_close:
 push rbx
 mov rbx,rdi
 mov rdi,[rbx+P_INNER]
 test rdi,rdi
 jz .Lclose_root
 call json_object_put@PLT
.Lclose_root:
 mov rdi,[rbx+P_ROOT]
 test rdi,rdi
 jz .Lclose_done
 call json_object_put@PLT
.Lclose_done:
 mov rdi,[rbx+P_EDITS]
 mov qword ptr [rbx+P_EDITS],0
 call free@PLT
 pop rbx
 ret

.Lparse:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,80
 mov rbx,rdx
 mov r8,rdi
 mov r9,rsi
 mov rdi,rbx
 xor eax,eax
 mov ecx,P_BYTES/8
 rep stosq
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov qword ptr [rsp+24],1
 mov qword ptr [rsp+32],0
 mov rdi,r8
 mov rsi,r9
 call .Ljson
 mov [rbx+P_ROOT],rax
 mov r12,rax
 test rax,rax
 jz .Lparse_bad
 mov rdi,r12
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Lparse_bad
 mov rdi,r12
 call pg_tool_path_arguments@PLT
 test eax,eax
 jnz .Lparse_bad
 mov qword ptr [rsp+40],0
 mov rdi,r12
 lea rsi,[rip+.Lpath]
 lea rdx,[rsp+40]
 call json_object_object_get_ex@PLT
 mov rdi,[rsp+40]
 lea rsi,[rbx+P_PATH]
 call .Lstring
 test eax,eax
 jnz .Lparse_bad
 cmp qword ptr [rbx+P_PATH_N],4096
 jae .Lparse_bad
 mov rdi,r12
 lea rsi,[rip+.Lold]
 mov rdx,rsp
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lget_new
 inc qword ptr [rsp+24]
.Lget_new:
 mov rdi,r12
 lea rsi,[rip+.Lnew]
 lea rdx,[rsp+8]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lget_edits
 inc qword ptr [rsp+24]
.Lget_edits:
 mov rdi,r12
 lea rsi,[rip+.Ledits]
 lea rdx,[rsp+16]
 call json_object_object_get_ex@PLT
 test eax,eax
 jz .Lcheck_members
 inc qword ptr [rsp+24]
.Lcheck_members:
 mov rdi,r12
 call json_object_object_length@PLT
 cmp rax,[rsp+24]
 jne .Lparse_bad
 cmp qword ptr [rsp],0
 je .Lparse_edits
 cmp qword ptr [rsp+8],0
 je .Lparse_edits
 mov rdi,[rsp]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lparse_edits
 mov rdi,[rsp+8]
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lparse_edits
 mov qword ptr [rsp+32],1
.Lparse_edits:
 mov r13,[rsp+16]
 test r13,r13
 jz .Llegacy
 mov rdi,r13
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lparsed_edits_type
 mov rdi,r13
 call json_object_get_string_len@PLT
 mov r14d,eax
 mov rdi,r13
 call json_object_get_string@PLT
 mov rdi,rax
 mov rsi,r14
 call .Ljson
 mov [rbx+P_INNER],rax
 mov r13,rax
 test r13,r13
 jz .Llegacy
.Lparsed_edits_type:
 mov rdi,r13
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jnz .Larray
 mov rdi,r13
 mov esi,4
 call json_object_is_type@PLT
 test eax,eax
 jz .Llegacy
 mov rdi,rbx
 mov rsi,r13
 call .Lobject_pair
 cmp eax,2
 je .Llegacy
 test eax,eax
 jnz .Lparse_bad
 jmp .Llegacy
.Larray:
 mov rdi,r13
 call json_object_array_length@PLT
 mov r14,rax
 xor r15d,r15d
.Larray_item:
 cmp r15,r14
 jae .Llegacy
 mov rdi,r13
 mov rsi,r15
 call json_object_array_get_idx@PLT
 mov rdi,rbx
 mov rsi,rax
 call .Lobject_pair
 test eax,eax
 jnz .Lparse_bad
 inc r15
 jmp .Larray_item
.Llegacy:
 cmp qword ptr [rsp+32],0
 je .Lparse_count
 mov rdi,rbx
 mov rsi,[rsp]
 mov rdx,[rsp+8]
 call .Lpair
 test eax,eax
 jnz .Lparse_bad
.Lparse_count:
 cmp qword ptr [rbx+P_COUNT],0
 je .Lparse_bad
 xor eax,eax
 jmp .Lparse_return
.Lparse_bad:
 mov eax,1
.Lparse_return:
 add rsp,80
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret

.Lvalidate:
 push rbx
 sub rsp,64
 mov rdi,rsi
 mov rsi,rdx
 mov rdx,rsp
 call .Lparse
 mov ebx,eax
 mov rdi,rsp
 call .Lparsed_close
 mov eax,ebx
 xor eax,1
 add rsp,64
 pop rbx
 ret

.Lauthorize:
 xor eax,eax
 test rdi,rdi
 jz .Lauthorize_return
 cmp qword ptr [rdi+8],0
 je .Lauthorize_return
 cmp qword ptr [rdi+16],0
 je .Lauthorize_return
 mov eax,1
.Lauthorize_return:
 ret

/* Add one string with local ownership until the parent accepts it. */
.Lfield:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 mov esi,ecx
 call json_object_new_string_len@PLT
 test rax,rax
 jz .Lfield_bad
 mov r13,rax
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Lfield_return
 mov rdi,r13
 call json_object_put@PLT
.Lfield_bad:
 mov eax,1
.Lfield_return:
 pop r13
 pop r12
 pop rbx
 ret

/* Build a read request or a write request from detached path/result bytes. */
.Lrequest:
 push rbx
 push r12
 push r13
 push r14
 push r15
 mov r12,rdi
 mov r13,rsi
 mov r14,rdx
 mov r15d,ecx
 call json_object_new_object@PLT
 mov rbx,rax
 test rax,rax
 jz .Lrequest_return
 mov rdi,rbx
 lea rsi,[rip+.Lpath]
 mov rdx,[r12+P_PATH]
 mov rcx,[r12+P_PATH_N]
 call .Lfield
 test eax,eax
 jnz .Lrequest_bad
 test r15d,r15d
 jz .Lrequest_good
 mov rdi,rbx
 lea rsi,[rip+.Lcontent]
 mov rdx,r13
 mov rcx,r14
 call .Lfield
 test eax,eax
 jnz .Lrequest_bad
.Lrequest_good:
 mov rax,rbx
 jmp .Lrequest_return
.Lrequest_bad:
 mov rdi,rbx
 call json_object_put@PLT
 xor eax,eax
.Lrequest_return:
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret

/* (editor,name,len,out,args,argslen,size*) -> authorized service execution. */
.Lio:
 mov rax,[rsp+8]
 mov r10,[rsp+16]
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,24
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov r15,r8
 mov [rsp],r9
 mov [rsp+8],rax
 mov [rsp+16],r10
 mov rdi,[rbx]
 mov rcx,r15
 mov r8,r9
 call [rbx+8]
 test eax,eax
 jz .Lio_denied
 mov rdi,[rbx]
 mov rsi,r12
 mov rdx,r13
 mov rcx,r15
 mov r8,[rsp]
 mov r9,r14
 mov rax,[rsp+8]
 mov r10,[rsp+16]
 push rax
 push r10
 call [rbx+16]
 add rsp,16
 jmp .Lio_return
.Lio_denied:
 mov eax,1
.Lio_return:
 add rsp,24
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.Lexecute:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,FRAME
 mov rbp,rsp
 mov rbx,rdi
 mov r12,rcx
 mov r13,r8
 mov r14,r9
 mov qword ptr [rbp+CONTENT],0
 mov qword ptr [rbp+CONTENT_CAP],0
 mov qword ptr [rbp+RESULT],0
 mov qword ptr [rbp+RESULT_CAP],0
 mov qword ptr [rbp+TREE],0
 push rsi
 push rdx
 mov rdi,rbx
 call pg_edit_tool_clear
 pop rdx
 pop rsi
 mov qword ptr [rbp+TREE],0
 lea r15,[rip+.Linvalid]
 test r12,r12
 jz .Ldirect_failure
 test r14,r14
 jz .Ldirect_failure
 mov qword ptr [r14],0
 cmp r13,64
 jb .Ldirect_failure
 mov rdi,rsi
 mov rsi,rdx
 mov rdx,rbp
 call .Lparse
 test eax,eax
 jnz .Lcleanup
 mov rdi,rbx
 call .Lauthorize
 test eax,eax
 jz .Lcleanup
 mov rdi,rbp
 xor esi,esi
 xor edx,edx
 xor ecx,ecx
 call .Lrequest
 mov [rbp+TREE],rax
 test rax,rax
 jz .Lmemory_error
 call .Lserialize
 cmp eax,2
 je .Lmemory_error
 test eax,eax
 jnz .Lquota_error
 mov qword ptr [rbp+CONTENT_CAP],65536
 mov rdi,65536
 call malloc@PLT
 test rax,rax
 jz .Lmemory_error
 mov [rbp+CONTENT],rax
.Lread_retry:
 mov qword ptr [rbp+IO_SIZE],0
 lea rax,[rbp+IO_SIZE]
 mov rdx,[rbp+CONTENT_CAP]
 push rdx
 push rax
 mov rdi,rbx
 lea rsi,[rip+.Lread]
 mov edx,4
 mov rcx,[rbp+CONTENT]
 mov r8,[rbp+ENCODED]
 mov r9,[rbp+ENCODED_N]
 call .Lio
 add rsp,16
 test eax,eax
 jz .Lread_complete
	mov edx,LREAD_LIMIT_N
	cmp qword ptr [rbp+IO_SIZE],rdx
 jne .Lread_failed
 mov rdi,[rbp+CONTENT]
 lea rsi,[rip+.Lread_limit]
 mov edx,LREAD_LIMIT_N
 call memcmp@PLT
 test eax,eax
 jnz .Lread_failed
 mov rax,[rbp+CONTENT_CAP]
	movabs r10,0x7fffffffffffffff
	cmp rax,r10
 ja .Lquota_error
 shl rax,1
 mov [rbp+CONTENT_CAP],rax
 mov rdi,[rbp+CONTENT]
 mov rsi,rax
 call realloc@PLT
 test rax,rax
 jz .Lmemory_error
 mov [rbp+CONTENT],rax
 jmp .Lread_retry
.Lread_failed:
 lea r15,[rip+.Lread_error]
 test eax,eax
 jmp .Lcleanup
.Lread_complete:
	mov rax,[rbp+IO_SIZE]
	cmp rax,[rbp+CONTENT_CAP]
	jae .Lquota_error
 mov rdi,[rbp+TREE]
 call json_object_put@PLT
 mov qword ptr [rbp+TREE],0
 mov rax,[rbp+IO_SIZE]
 mov [rbp+FIRST_LINE],rax
 xor r10d,r10d
 xor r11d,r11d
.Lresult_measure:
 cmp r11,[rbp+P_COUNT]
 jae .Lresult_measure_done
 mov rax,r11
 shl rax,5
 mov rdx,[rbp+P_EDITS]
 add rdx,rax
 add r10,[rdx+24]
 jc .Lquota_error
 inc r11
 jmp .Lresult_measure
.Lresult_measure_done:
 add r10,[rbp+FIRST_LINE]
 jc .Lquota_error
 add r10,r10
 jc .Lquota_error
 add r10,4
 jc .Lquota_error
 mov [rbp+RESULT_CAP],r10
 mov rdi,r10
 call malloc@PLT
 test rax,rax
 jz .Lmemory_error
 mov [rbp+RESULT],rax
 lea rax,[rbp+IO_SIZE]
 push 0
 push rax
 mov rdi,[rbp+CONTENT]
 mov rsi,[rbp+FIRST_LINE]
 mov rdx,[rbp+P_EDITS]
 mov rcx,[rbp+P_COUNT]
 mov r8,[rbp+RESULT]
 mov r9,[rbp+RESULT_CAP]
 call pg_edit_text_apply
 add rsp,16
 cmp eax,0
 je .Lwrite_request
 lea r15,[rip+.Linvalid]
 cmp eax,2
 je .Lquota_error
 lea r15,[rip+.Lmissing]
 cmp eax,3
 je .Lcleanup
 lea r15,[rip+.Lambiguous]
 cmp eax,4
 je .Lcleanup
 lea r15,[rip+.Loverlap]
 cmp eax,5
 je .Lcleanup
 lea r15,[rip+.Lnochange]
 cmp eax,6
 je .Lcleanup
 lea r15,[rip+.Lmemory]
 cmp eax,7
 je .Lcleanup
 lea r15,[rip+.Linvalid]
 jmp .Lcleanup
.Lwrite_request:
 mov rdi,[rbp+P_PATH]
 mov rsi,[rbp+P_PATH_N]
 mov rdx,[rbp+CONTENT]
 mov rcx,[rbp+FIRST_LINE]
 mov r8,[rbp+RESULT]
 mov r9,[rbp+IO_SIZE]
 sub rsp,16
 lea rax,[rbx+56]
 mov [rsp],rax
 call pg_edit_diff_create@PLT
 add rsp,16
 test eax,eax
 jnz .Lmemory_error
 mov rdi,[rbx+56]
 lea rsi,[rip+.Lfirst_key]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rbp+FIRST_LINE],rax
 mov rdi,rbp
	mov rsi,[rbp+RESULT]
 mov rdx,[rbp+IO_SIZE]
 mov ecx,1
 call .Lrequest
 mov [rbp+TREE],rax
 test rax,rax
 jz .Lmemory_error
 call .Lserialize
 cmp eax,2
 je .Lmemory_error
 test eax,eax
 jnz .Lquota_error
 mov qword ptr [rbp+IO_SIZE],0
 lea rax,[rbp+IO_SIZE]
 push 4096
 push rax
 mov rdi,rbx
 lea rsi,[rip+.Lwrite]
 mov edx,5
 mov rcx,[rbp+CONTENT]
 mov r8,[rbp+ENCODED]
 mov r9,[rbp+ENCODED_N]
 call .Lio
 add rsp,16
 lea r15,[rip+.Lwrite_error]
 test eax,eax
 jnz .Lcleanup
 cmp qword ptr [rbp+IO_SIZE],4096
 jae .Lcleanup
 lea r15,[rip+.Lok]
 jmp .Lcleanup
.Lmemory_error:
 lea r15,[rip+.Lmemory]
 jmp .Lcleanup
.Lquota_error:
 lea r15,[rip+.Lquota]
.Lcleanup:
 mov rdi,[rbp+CONTENT]
 call free@PLT
 mov qword ptr [rbp+CONTENT],0
 mov rdi,[rbp+RESULT]
 call free@PLT
 mov qword ptr [rbp+RESULT],0
 lea rax,[rip+.Lok]
 cmp r15,rax
 je .Lcleanup_keep_details
 mov rdi,rbx
 call pg_edit_tool_clear
.Lcleanup_keep_details:
 mov rdi,[rbp+TREE]
 test rdi,rdi
 jz .Lcleanup_parsed
 call json_object_put@PLT
.Lcleanup_parsed:
 lea rax,[rip+.Lok]
 cmp r15,rax
 jne .Lcleanup_failure
 mov rdi,r12
 mov rsi,r13
 mov rdx,r15
 mov rcx,[rbp+P_COUNT]
 mov r8,[rbp+P_PATH]
 xor eax,eax
 call snprintf@PLT
 mov [rbp+IO_SIZE],rax
 mov rdi,rbp
 call .Lparsed_close
 mov rax,[rbp+IO_SIZE]
 test eax,eax
 js .Lsummary_failed
 cmp rax,r13
 jae .Lsummary_failed
 mov [r14],rax
 xor eax,eax
 jmp .Lexecute_return
.Lcleanup_failure:
 mov rdi,rbp
 call .Lparsed_close
 jmp .Lcopy_message
.Lsummary_failed:
 mov rdi,rbx
 call pg_edit_tool_clear
 lea r15,[rip+.Lsummary_error]
.Lcopy_message:
 mov rdi,r15
 call strlen@PLT
 mov rbx,rax
 mov rdi,r12
 mov rsi,r15
 lea rdx,[rax+1]
 call memcpy@PLT
 mov [r14],rbx
 lea rax,[rip+.Lok]
 cmp r15,rax
 setne al
 movzx eax,al
 jmp .Lexecute_return
.Ldirect_failure:
 mov eax,1
.Lexecute_return:
 add rsp,FRAME
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

/* Main frame is in rbp; JSON string remains borrowed from TREE. */
.Lserialize:
 sub rsp,8
 mov rdi,[rbp+TREE]
 xor esi,esi
 call json_object_to_json_string_ext@PLT
 mov [rbp+ENCODED],rax
 test rax,rax
 jz .Lserialize_memory
 mov rdi,rax
 call strlen@PLT
	movabs rdx,0x7fffffff
	cmp rax,rdx
	ja .Lserialize_bad
 mov [rbp+ENCODED_N],rax
 xor eax,eax
 add rsp,8
 ret
.Lserialize_bad:
 mov eax,1
 add rsp,8
 ret
.Lserialize_memory:
 mov eax,2
 add rsp,8
 ret

.globl pg_edit_tool_definition
.type pg_edit_tool_definition,@function
pg_edit_tool_definition:
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
.size pg_edit_tool_definition,.-pg_edit_tool_definition
.section .note.GNU-stack,"",@progbits

.text
.globl pg_edit_tool_clear
.type pg_edit_tool_clear,@function
pg_edit_tool_clear:
 push rbx
 mov rbx,rdi
 mov rdi,[rbx+56]
 mov qword ptr [rbx+56],0
 call json_object_put@PLT
 pop rbx
 ret
.size pg_edit_tool_clear,.-pg_edit_tool_clear
.section .rodata
.Lfirst_key: .asciz "firstChangedLine"
