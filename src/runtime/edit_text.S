.intel_syntax noprefix
#define ORIGINAL 0
#define FUZZY 8
#define EDITED 16
#define FINAL 24
#define ARENA 32
#define NEEDLE 40
#define RECORDS 48
#define ON 56
#define FN 64
#define EN 72
#define AN 80
#define COUNT 88
#define USED 96
#define CRLF 100
#define BOM 104
#define EDIT_CAP 112
#define FINAL_CAP 120
#define WORK_BYTES 136
#define OLD 0
#define OLD_N 8
#define NEW 16
#define NEW_N 24
#define AT 32
#define LENGTH 40
#define BS 48
#define BE 56
#define OS 64
#define OE 72
#define RECORD_BYTES 80
.text

/* UTF-8 validation and CRLF/CR -> LF. Returns byte length or -1. */
.type .Llf,@function
.Llf:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
 xor r15d,r15d
 test r12,r12
 jz .Llf_ok
 test rbx,rbx
 jz .Llf_bad
.Llf_loop:
 cmp r14,r12
 jae .Llf_ok
 movzx eax,byte ptr [rbx+r14]
 test eax,eax
 jz .Llf_bad
 cmp eax,128
 jae .Llf_unicode
 inc r14
 cmp eax,13
 jne .Llf_ascii
 cmp r14,r12
 jae .Llf_cr
 cmp byte ptr [rbx+r14],10
 jne .Llf_cr
 inc r14
.Llf_cr:
 mov eax,10
.Llf_ascii:
 mov [r13+r15],al
 inc r15
 jmp .Llf_loop
.Llf_unicode:
 lea rdi,[rbx+r14]
 mov rsi,r12
 sub rsi,r14
 mov rdx,rsp
 call utf8proc_iterate@PLT
 test rax,rax
 jle .Llf_bad
 mov rcx,rax
.Llf_bytes:
 mov al,[rbx+r14]
 mov [r13+r15],al
 inc r14
 inc r15
 dec rcx
 jnz .Llf_bytes
 jmp .Llf_loop
.Llf_ok:
 mov rax,r15
 jmp .Llf_return
.Llf_bad:
 mov rax,-1
.Llf_return:
 add rsp,16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size .Llf,.-.Llf

/* NFKC via utf8proc, then Pi's trimEnd/quote/dash/space mapping in place. */
.type .Lfuzzy,@function
.Lfuzzy:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,48
 mov [rsp+16],rdx
 test rdx,rdx
 jz .Lfuzzy_invalid
 mov qword ptr [rdx],0
 mov qword ptr [rsp],0
 lea rdx,[rsp]
 mov ecx,14 /* STABLE|COMPAT|COMPOSE, asserted in the C boundary tests. */
 call utf8proc_map@PLT
 cmp rax,-1
 je .Lfuzzy_no_memory
 test rax,rax
 js .Lfuzzy_invalid
 mov rbx,[rsp]
 mov r12,rax
 mov rdi,rbx
 lea rsi,[r12+1]
 call realloc@PLT
 test rax,rax
 jz .Lfuzzy_realloc_failed
 mov rbx,rax
 xor r13d,r13d
 xor r14d,r14d
 mov qword ptr [rsp+24],0 /* last non-whitespace byte */
.Lfuzzy_loop:
 cmp r13,r12
 jae .Lfuzzy_done
 lea rdi,[rbx+r13]
 mov rsi,r12
 sub rsi,r13
 lea rdx,[rsp+8]
 call utf8proc_iterate@PLT
 test rax,rax
 jle .Lfuzzy_free_bad
 mov r15,rax
 mov eax,[rsp+8]
 cmp eax,10
 je .Lfuzzy_newline
 call .Lwhitespace
 mov [rsp+32],eax
 mov eax,[rsp+8]
 cmp eax,0x2018
 jb .Lfuzzy_double
 cmp eax,0x201b
 jbe .Lfuzzy_single
.Lfuzzy_double:
 cmp eax,0x201c
 jb .Lfuzzy_dash
 cmp eax,0x201f
 jbe .Lfuzzy_quote
.Lfuzzy_dash:
 cmp eax,0x2212
 je .Lfuzzy_hyphen
 cmp eax,0x2010
 jb .Lfuzzy_space
 cmp eax,0x2015
 jbe .Lfuzzy_hyphen
.Lfuzzy_space:
 cmp eax,0xa0
 je .Lfuzzy_blank
 cmp eax,0x202f
 je .Lfuzzy_blank
 cmp eax,0x205f
 je .Lfuzzy_blank
 cmp eax,0x3000
 je .Lfuzzy_blank
 cmp eax,0x2002
 jb .Lfuzzy_copy
 cmp eax,0x200a
 jbe .Lfuzzy_blank
.Lfuzzy_copy:
 xor ecx,ecx
.Lfuzzy_copy_loop:
 mov al,[rbx+r13]
 mov [rbx+r14],al
 inc r13
 inc r14
 inc rcx
 cmp rcx,r15
 jb .Lfuzzy_copy_loop
 jmp .Lfuzzy_mark
.Lfuzzy_single:
 mov eax,39
 jmp .Lfuzzy_ascii
.Lfuzzy_quote:
 mov eax,34
 jmp .Lfuzzy_ascii
.Lfuzzy_hyphen:
 mov eax,45
 jmp .Lfuzzy_ascii
.Lfuzzy_blank:
 mov eax,32
.Lfuzzy_ascii:
 mov [rbx+r14],al
 inc r14
 add r13,r15
.Lfuzzy_mark:
 cmp dword ptr [rsp+32],0
 jne .Lfuzzy_loop
 mov [rsp+24],r14
 jmp .Lfuzzy_loop
.Lfuzzy_newline:
 mov r14,[rsp+24]
 mov byte ptr [rbx+r14],10
 inc r14
 mov [rsp+24],r14
 add r13,r15
 jmp .Lfuzzy_loop
.Lfuzzy_done:
 mov r14,[rsp+24]
 mov byte ptr [rbx+r14],0
 mov rax,[rsp+16]
 mov [rax],rbx
 mov rax,r14
 jmp .Lfuzzy_return
.Lfuzzy_free_bad:
 mov rdi,rbx
 call utf8proc_free@PLT
.Lfuzzy_invalid:
 mov rax,-1
 jmp .Lfuzzy_return
.Lfuzzy_no_memory:
 mov rax,-3
 jmp .Lfuzzy_return
.Lfuzzy_realloc_failed:
 mov rdi,rbx
 call utf8proc_free@PLT
 mov rax,-3
.Lfuzzy_return:
 add rsp,48
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size .Lfuzzy,.-.Lfuzzy

/* ECMAScript WhiteSpace/LineTerminator, excluding LF handled above. */
.Lwhitespace:
 cmp eax,9
 jb .Lnot_space
 cmp eax,13
 jbe .Lis_space
 cmp eax,32
 je .Lis_space
 cmp eax,0xa0
 je .Lis_space
 cmp eax,0x1680
 je .Lis_space
 cmp eax,0x2000
 jb .Lother_space
 cmp eax,0x200a
 jbe .Lis_space
.Lother_space:
 cmp eax,0x2028
 je .Lis_space
 cmp eax,0x2029
 je .Lis_space
 cmp eax,0x202f
 je .Lis_space
 cmp eax,0x205f
 je .Lis_space
 cmp eax,0x3000
 je .Lis_space
 cmp eax,0xfeff
 je .Lis_space
.Lnot_space:
 xor eax,eax
 ret
.Lis_space:
 mov eax,1
 ret

/* Count non-overlapping matches, stopping at two. Empty needle follows JS split. */
.type .Loccurrences,@function
.Loccurrences:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 xor r15d,r15d
 test r14,r14
 jz .Locc_empty
.Locc_loop:
 mov rdi,rbx
 mov rsi,r12
 mov rdx,r13
 mov rcx,r14
 call memmem@PLT
 test rax,rax
 jz .Locc_done
 inc r15
 cmp r15,2
 jae .Locc_done
 add rax,r14
 sub rax,rbx
 sub r12,rax
 add rbx,rax
 jmp .Locc_loop
.Locc_empty:
 test r12,r12
 jz .Locc_empty_done
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rsp
 call utf8proc_iterate@PLT
 test rax,rax
 jle .Locc_empty_done
 add rbx,rax
 sub r12,rax
 inc r15
 cmp dword ptr [rsp],0xffff
 jbe .Locc_units
 inc r15
.Locc_units:
 cmp r15,3
 jb .Locc_empty
.Locc_empty_done:
 dec r15
.Locc_done:
 mov rax,r15
 add rsp,16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size .Loccurrences,.-.Loccurrences

.Lline_count:
 xor eax,eax
 xor ecx,ecx
 test rsi,rsi
 jz .Lline_count_done
.Lline_count_loop:
 cmp byte ptr [rdi+rcx],10
 jne .Lline_count_next
 inc rax
.Lline_count_next:
 inc rcx
 cmp rcx,rsi
 jb .Lline_count_loop
 cmp byte ptr [rdi+rsi-1],10
 je .Lline_count_done
 inc rax
.Lline_count_done:
 ret

/* Find touched line spans in the fuzzy base and corresponding original lines. */
.Lspans:
 xor r8d,r8d
 xor r9d,r9d
 mov qword ptr [rsi+BS],-1
.Lspan_loop:
 cmp r8,[rdi+FN]
 jae .Lspan_bad
 mov r10,r8
 mov r11,r9
.Lspan_base_end:
 cmp r10,[rdi+FN]
 jae .Lspan_original_end
 mov rax,[rdi+FUZZY]
 cmp byte ptr [rax+r10],10
 lea r10,[r10+1]
 jne .Lspan_base_end
.Lspan_original_end:
 cmp r11,[rdi+ON]
 jae .Lspan_check
 mov rax,[rdi+ORIGINAL]
 cmp byte ptr [rax+r11],10
 lea r11,[r11+1]
 jne .Lspan_original_end
.Lspan_check:
 cmp qword ptr [rsi+BS],-1
 jne .Lspan_finish
 mov rax,[rsi+AT]
 cmp rax,r8
 jb .Lspan_bad
 cmp rax,r10
 jae .Lspan_advance
 mov [rsi+BS],r8
 mov [rsi+OS],r9
.Lspan_finish:
 mov rax,[rsi+AT]
 add rax,[rsi+LENGTH]
 cmp r10,rax
 jb .Lspan_advance
 mov [rsi+BE],r10
 mov [rsi+OE],r11
 xor eax,eax
 ret
.Lspan_advance:
 mov r8,r10
 mov r9,r11
 jmp .Lspan_loop
.Lspan_bad:
 mov eax,1
 ret

/* Append to the private edited buffer, never the caller's output. */
.Lappend:
 push rbx
 mov rbx,rdi
 mov rax,[rbx+EN]
 mov rcx,rax
 add rcx,rdx
 jc .Lappend_bad
 cmp rcx,[rbx+EDIT_CAP]
 jae .Lappend_bad
 mov rdi,[rbx+EDITED]
 add rdi,rax
 add [rbx+EN],rdx
 call memcpy@PLT
 xor eax,eax
 pop rbx
 ret
.Lappend_bad:
 mov eax,1
 pop rbx
 ret

.globl pg_edit_text_apply
.type pg_edit_text_apply,@function
pg_edit_text_apply:
 mov rax,[rsp+8]
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,200
 mov [rsp],rdi
 mov [rsp+8],rsi
 mov [rsp+16],rax
 mov r12,rdx
 mov r13,rcx
 mov r14,r8
 mov r15,r9
 xor ebx,ebx
 mov dword ptr [rsp+136],1
 test rax,rax
 jz .Lreturn
 mov qword ptr [rax],0
 test r14,r14
 jz .Lreturn
 test r12,r12
 jz .Lreturn
 test r13,r13
 jz .Lreturn
 cmp r13,16
 ja .Lreturn
 test rsi,rsi
 jz .Lallocate
 test rdi,rdi
 jz .Lreturn
.Lallocate:
 mov edi,1
 mov esi,WORK_BYTES
 call calloc@PLT
 test rax,rax
 jz .Lno_memory
 mov rbx,rax
 mov [rbx+COUNT],r13
 mov rax,[rsp+8]
 inc rax
 jz .Lquota
 mov rdi,rax
 call malloc@PLT
 test rax,rax
 jz .Lno_memory
 mov [rbx+ORIGINAL],rax
 mov rdi,r13
 imul rdi,rdi,RECORD_BYTES
 call malloc@PLT
 test rax,rax
 jz .Lno_memory
 mov [rbx+RECORDS],rax
 mov qword ptr [rsp+24],0
 mov qword ptr [rsp+32],0
 xor ebp,ebp
.Lmeasure_edits:
 cmp rbp,r13
 jae .Lmeasure_done
 mov rax,rbp
 shl rax,5
 lea rax,[r12+rax]
 mov rcx,[rax+8]
 add [rsp+24],rcx
 jc .Lquota
 mov rcx,[rax+24]
 add [rsp+24],rcx
 jc .Lquota
 add [rsp+32],rcx
 jc .Lquota
 inc rbp
 jmp .Lmeasure_edits
.Lmeasure_done:
 mov rax,[rsp+24]
 inc rax
 jz .Lquota
 mov rdi,rax
 call malloc@PLT
 test rax,rax
 jz .Lno_memory
 mov [rbx+ARENA],rax
 mov rax,[rsp+8]
 add rax,[rsp+32]
 jc .Lquota
 inc rax
 jz .Lquota
 mov [rbx+EDIT_CAP],rax
 mov rdi,rax
 call malloc@PLT
 test rax,rax
 jz .Lno_memory
 mov [rbx+EDITED],rax
 mov rax,[rbx+EDIT_CAP]
 dec rax
 add rax,rax
 jc .Lquota
 add rax,4
 jc .Lquota
 mov [rbx+FINAL_CAP],rax
 mov rdi,rax
 call malloc@PLT
 test rax,rax
 jz .Lno_memory
 mov [rbx+FINAL],rax
 mov rdi,[rsp]
 mov rsi,[rsp+8]
 cmp rsi,3
 jb .Ldetect
 cmp byte ptr [rdi],0xef
 jne .Ldetect
 cmp byte ptr [rdi+1],0xbb
 jne .Ldetect
 cmp byte ptr [rdi+2],0xbf
 jne .Ldetect
 mov dword ptr [rbx+BOM],1
 add rdi,3
 sub rsi,3
 mov [rsp],rdi
 mov [rsp+8],rsi
.Ldetect:
 test rsi,rsi
 jz .Lnormalize_content
 mov rdx,rsi
 mov esi,10
 call memchr@PLT
 test rax,rax
 jz .Lnormalize_content
 cmp rax,[rsp]
 jbe .Lnormalize_content
 cmp byte ptr [rax-1],13
 jne .Lnormalize_content
 mov dword ptr [rbx+CRLF],1
.Lnormalize_content:
 mov rdi,[rsp]
 mov rsi,[rsp+8]
 mov rdx,[rbx+ORIGINAL]
 call .Llf
 test rax,rax
 js .Lreturn
 mov [rbx+ON],rax
 mov qword ptr [rsp+24],0 /* combined raw edit bytes */
 xor ebp,ebp
.Lnormalize_edit:
 cmp rbp,r13
 jae .Lfuzzy_content
 mov rax,rbp
 shl rax,5
 lea r10,[r12+rax]
 mov rax,[r10+8]
 add rax,[r10+24]
 jc .Lquota
 add [rsp+24],rax
 jc .Lquota
 mov r11,[rbx+RECORDS]
 mov rax,rbp
 imul rax,RECORD_BYTES
 add r11,rax
 mov [rsp+32],r10
 mov [rsp+40],r11
 mov rdi,[r10]
 mov rsi,[r10+8]
 mov rdx,[rbx+ARENA]
 add rdx,[rbx+AN]
 mov [r11+OLD],rdx
 call .Llf
 test rax,rax
 jle .Lreturn
 mov r11,[rsp+40]
 mov [r11+OLD_N],rax
 add [rbx+AN],rax
 mov r10,[rsp+32]
 mov rdi,[r10+16]
 mov rsi,[r10+24]
 mov rdx,[rbx+ARENA]
 add rdx,[rbx+AN]
 mov [r11+NEW],rdx
 call .Llf
 test rax,rax
 js .Lreturn
 mov r11,[rsp+40]
 mov [r11+NEW_N],rax
 add [rbx+AN],rax
 inc rbp
 jmp .Lnormalize_edit
.Lfuzzy_content:
 mov rdi,[rbx+ORIGINAL]
 mov rsi,[rbx+ON]
 lea rdx,[rbx+FUZZY]
 call .Lfuzzy
 test rax,rax
 js .Lunicode_error
 mov [rbx+FN],rax
 xor ebp,ebp
.Linitial_match:
 cmp rbp,r13
 jae .Lmatch_begin
 imul rax,rbp,RECORD_BYTES
 add rax,[rbx+RECORDS]
 mov [rsp+40],rax
 mov rdx,[rax+OLD]
 mov rcx,[rax+OLD_N]
 mov rdi,[rbx+ORIGINAL]
 mov rsi,[rbx+ON]
 call memmem@PLT
 test rax,rax
 jnz .Linitial_next
 mov rax,[rsp+40]
 mov rdi,[rax+OLD]
 mov rsi,[rax+OLD_N]
 lea rdx,[rbx+NEEDLE]
 call .Lfuzzy
 test rax,rax
 js .Lunicode_error
 mov rcx,rax
 mov rdi,[rbx+FUZZY]
 mov rsi,[rbx+FN]
 mov rdx,[rbx+NEEDLE]
 call memmem@PLT
 mov [rsp+80],rax
 mov rdi,[rbx+NEEDLE]
 call utf8proc_free@PLT
 mov qword ptr [rbx+NEEDLE],0
 cmp qword ptr [rsp+80],0
 je .Lnot_found
 mov dword ptr [rbx+USED],1
.Linitial_next:
 inc rbp
 jmp .Linitial_match
.Lmatch_begin:
 xor ebp,ebp
.Lmatch:
 cmp rbp,r13
 jae .Lsort_begin
 imul rax,rbp,RECORD_BYTES
 add rax,[rbx+RECORDS]
 mov [rsp+40],rax
 mov rdi,[rax+OLD]
 mov rsi,[rax+OLD_N]
 lea rdx,[rbx+NEEDLE]
 call .Lfuzzy
 test rax,rax
 js .Lunicode_error
 mov [rsp+32],rax
 mov rdi,[rbx+FUZZY]
 mov rsi,[rbx+FN]
 mov rdx,[rbx+NEEDLE]
 mov rcx,rax
 call .Loccurrences
 cmp rax,1
 jg .Lmatch_ambiguous
 mov rdi,[rbx+ORIGINAL]
 mov rsi,[rbx+ON]
 cmp dword ptr [rbx+USED],0
 je .Lmatch_base
 mov rdi,[rbx+FUZZY]
 mov rsi,[rbx+FN]
.Lmatch_base:
 mov [rsp+24],rdi
 mov rax,[rsp+40]
 mov rdx,[rax+OLD]
 mov rcx,[rax+OLD_N]
 call memmem@PLT
 test rax,rax
 jnz .Lmatch_exact_found
 mov rdi,[rsp+24]
 mov rsi,[rbx+FN]
 mov rdx,[rbx+NEEDLE]
 mov rcx,[rsp+32]
 call memmem@PLT
 mov [rsp+80],rax
 mov rdi,[rbx+NEEDLE]
 call utf8proc_free@PLT
 mov qword ptr [rbx+NEEDLE],0
 mov rax,[rsp+80]
 test rax,rax
 jz .Lnot_found
 mov r10,[rsp+40]
 mov r11,[rsp+32]
 jmp .Lmatched
.Lmatch_exact_found:
 mov [rsp+80],rax
 mov rdi,[rbx+NEEDLE]
 call utf8proc_free@PLT
 mov qword ptr [rbx+NEEDLE],0
 mov rax,[rsp+80]
 mov r10,[rsp+40]
 mov r11,[r10+OLD_N]
.Lmatched:
 sub rax,[rsp+24]
 mov [r10+AT],rax
 mov [r10+LENGTH],r11
 inc rbp
 jmp .Lmatch
.Lmatch_ambiguous:
 mov rdi,[rbx+NEEDLE]
 call utf8proc_free@PLT
 mov qword ptr [rbx+NEEDLE],0
 jmp .Lambiguous
.Lsort_begin:
 mov ebp,1
.Lsort:
 cmp rbp,r13
 jae .Loverlap_begin
 imul rax,rbp,RECORD_BYTES
 mov rsi,[rbx+RECORDS]
 add rsi,rax
 lea rdi,[rsp+48]
 mov ecx,10
 rep movsq
 mov [rsp+24],rbp
.Linsert:
 mov rax,[rsp+24]
 test rax,rax
 jz .Linsert_done
 dec rax
 imul rax,rax,RECORD_BYTES
 mov rsi,[rbx+RECORDS]
 add rsi,rax
 mov rdx,[rsi+AT]
 cmp rdx,[rsp+48+AT]
 jbe .Linsert_done
 lea rdi,[rsi+RECORD_BYTES]
 mov ecx,10
 rep movsq
 dec qword ptr [rsp+24]
 jmp .Linsert
.Linsert_done:
 imul rax,[rsp+24],RECORD_BYTES
 mov rdi,[rbx+RECORDS]
 add rdi,rax
 lea rsi,[rsp+48]
 mov ecx,10
 rep movsq
 inc rbp
 jmp .Lsort
.Loverlap_begin:
 mov ebp,1
.Loverlap_check:
 cmp rbp,r13
 jae .Lbuild_begin
 imul rax,rbp,RECORD_BYTES
 add rax,[rbx+RECORDS]
 mov rcx,[rax+AT-RECORD_BYTES]
 add rcx,[rax+LENGTH-RECORD_BYTES]
 cmp rcx,[rax+AT]
 ja .Loverlap
 inc rbp
 jmp .Loverlap_check
.Lbuild_begin:
 mov qword ptr [rsp+24],0 /* original cursor */
 cmp dword ptr [rbx+USED],0
 jne .Lbuild_fuzzy
 xor ebp,ebp
.Lbuild_exact:
 cmp rbp,r13
 jae .Lbuild_tail
 imul rax,rbp,RECORD_BYTES
 add rax,[rbx+RECORDS]
 mov [rsp+40],rax
 mov rdx,[rax+AT]
 sub rdx,[rsp+24]
 mov rsi,[rbx+ORIGINAL]
 add rsi,[rsp+24]
 mov rdi,rbx
 call .Lappend
 test eax,eax
 jnz .Lquota
 mov rax,[rsp+40]
 mov rsi,[rax+NEW]
 mov rdx,[rax+NEW_N]
 mov rdi,rbx
 call .Lappend
 test eax,eax
 jnz .Lquota
 mov rax,[rsp+40]
 mov rcx,[rax+AT]
 add rcx,[rax+LENGTH]
 mov [rsp+24],rcx
 inc rbp
 jmp .Lbuild_exact
.Lbuild_fuzzy:
 mov rdi,[rbx+ORIGINAL]
 mov rsi,[rbx+ON]
 call .Lline_count
 mov [rsp+32],rax
 mov rdi,[rbx+FUZZY]
 mov rsi,[rbx+FN]
 call .Lline_count
 cmp rax,[rsp+32]
 jne .Lreturn
 xor ebp,ebp
.Lspan_each:
 cmp rbp,r13
 jae .Lgroups_begin
 imul rsi,rbp,RECORD_BYTES
 add rsi,[rbx+RECORDS]
 mov rdi,rbx
 call .Lspans
 test eax,eax
 jnz .Lreturn
 inc rbp
 jmp .Lspan_each
.Lgroups_begin:
 xor ebp,ebp
.Lgroup:
 cmp rbp,r13
 jae .Lbuild_tail
 imul rax,rbp,RECORD_BYTES
 add rax,[rbx+RECORDS]
 mov rcx,[rax+BS]
 mov [rsp+32],rcx
 mov rcx,[rax+BE]
 mov [rsp+40],rcx
 mov rcx,[rax+OS]
 mov [rsp+48],rcx
 mov rcx,[rax+OE]
 mov [rsp+56],rcx
 mov [rsp+64],rbp
 inc rbp
.Lextend_group:
 cmp rbp,r13
 jae .Lemit_group
 imul rax,rbp,RECORD_BYTES
 add rax,[rbx+RECORDS]
 mov rcx,[rax+BS]
 cmp rcx,[rsp+40]
 jae .Lemit_group
 mov rcx,[rax+BE]
 cmp rcx,[rsp+40]
 jbe .Lextend_next
 mov [rsp+40],rcx
 mov rcx,[rax+OE]
 mov [rsp+56],rcx
.Lextend_next:
 inc rbp
 jmp .Lextend_group
.Lemit_group:
 mov [rsp+72],rbp
 mov rdx,[rsp+48]
 sub rdx,[rsp+24]
 mov rsi,[rbx+ORIGINAL]
 add rsi,[rsp+24]
 mov rdi,rbx
 call .Lappend
 test eax,eax
 jnz .Lquota
 mov rbp,[rsp+64]
.Lemit_edit:
 cmp rbp,[rsp+72]
 jae .Lemit_group_tail
 imul rax,rbp,RECORD_BYTES
 add rax,[rbx+RECORDS]
 mov [rsp+80],rax
 mov rdx,[rax+AT]
 sub rdx,[rsp+32]
 mov rsi,[rbx+FUZZY]
 add rsi,[rsp+32]
 mov rdi,rbx
 call .Lappend
 test eax,eax
 jnz .Lquota
 mov rax,[rsp+80]
 mov rsi,[rax+NEW]
 mov rdx,[rax+NEW_N]
 mov rdi,rbx
 call .Lappend
 test eax,eax
 jnz .Lquota
 mov rax,[rsp+80]
 mov rcx,[rax+AT]
 add rcx,[rax+LENGTH]
 mov [rsp+32],rcx
 inc rbp
 jmp .Lemit_edit
.Lemit_group_tail:
 mov rdx,[rsp+40]
 sub rdx,[rsp+32]
 mov rsi,[rbx+FUZZY]
 add rsi,[rsp+32]
 mov rdi,rbx
 call .Lappend
 test eax,eax
 jnz .Lquota
 mov rcx,[rsp+56]
 mov [rsp+24],rcx
 jmp .Lgroup
.Lbuild_tail:
 mov rdx,[rbx+ON]
 sub rdx,[rsp+24]
 mov rsi,[rbx+ORIGINAL]
 add rsi,[rsp+24]
 mov rdi,rbx
 call .Lappend
 test eax,eax
 jnz .Lquota
 mov rdx,[rbx+EN]
 cmp rdx,[rbx+ON]
 jne .Lrestore
 mov rdi,[rbx+EDITED]
 mov rsi,[rbx+ORIGINAL]
 call memcmp@PLT
 test eax,eax
 jz .Lunchanged
.Lrestore:
 xor ecx,ecx
 cmp dword ptr [rbx+BOM],0
 je .Lrestore_begin
 mov rax,[rbx+FINAL]
 mov byte ptr [rax],0xef
 mov byte ptr [rax+1],0xbb
 mov byte ptr [rax+2],0xbf
 mov ecx,3
.Lrestore_begin:
 xor edx,edx
.Lrestore_loop:
 cmp rdx,[rbx+EN]
 jae .Lpublish
 mov rax,[rbx+EDITED]
 mov al,[rax+rdx]
 cmp al,10
 jne .Lrestore_byte
 cmp dword ptr [rbx+CRLF],0
 je .Lrestore_byte
 cmp rcx,[rbx+FINAL_CAP]
 jae .Lquota
 mov rsi,[rbx+FINAL]
 mov byte ptr [rsi+rcx],13
 inc rcx
.Lrestore_byte:
 cmp rcx,[rbx+FINAL_CAP]
 jae .Lquota
 mov rsi,[rbx+FINAL]
 mov [rsi+rcx],al
 inc rcx
 inc rdx
 jmp .Lrestore_loop
.Lpublish:
 cmp rcx,r15
 jae .Lquota
 mov [rsp+24],rcx
 mov rdi,r14
 mov rsi,[rbx+FINAL]
 mov rdx,rcx
 call memcpy@PLT
 mov rax,[rsp+24]
 mov byte ptr [r14+rax],0
 mov rcx,[rsp+16]
 mov [rcx],rax
 mov dword ptr [rsp+136],0
 jmp .Lreturn
.Lunicode_error:
 cmp rax,-3
 je .Lno_memory
 cmp rax,-2
 jne .Lreturn
.Lquota:
 mov dword ptr [rsp+136],2
 jmp .Lreturn
.Lnot_found:
 mov dword ptr [rsp+136],3
 jmp .Lreturn
.Lambiguous:
 mov dword ptr [rsp+136],4
 jmp .Lreturn
.Loverlap:
 mov dword ptr [rsp+136],5
 jmp .Lreturn
.Lunchanged:
 mov dword ptr [rsp+136],6
 jmp .Lreturn
.Lno_memory:
 mov dword ptr [rsp+136],7
.Lreturn:
 test rbx,rbx
 jz .Lreturn_result
 mov rdi,[rbx+NEEDLE]
 call utf8proc_free@PLT
 mov rdi,[rbx+FUZZY]
 call utf8proc_free@PLT
 mov rdi,[rbx+RECORDS]
 call free@PLT
 mov rdi,[rbx+ARENA]
 call free@PLT
 mov rdi,[rbx+FINAL]
 call free@PLT
 mov rdi,[rbx+EDITED]
 call free@PLT
 mov rdi,[rbx+ORIGINAL]
 call free@PLT
 mov rdi,rbx
 call free@PLT
.Lreturn_result:
 mov eax,[rsp+136]
 add rsp,200
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_edit_text_apply,.-pg_edit_text_apply
.section .note.GNU-stack,"",@progbits
