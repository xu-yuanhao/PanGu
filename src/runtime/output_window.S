/* Pi Chord output.ts boundOutput/headRange/tailRange, d5b3de22.
   MIT docs/licenses/pi.md. */
.intel_syntax noprefix
.text
.globl pg_output_bound
.type pg_output_bound,@function
pg_output_bound:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 mov eax,4
 test r9,r9
 jz .Low_end
 cmp r8d,1
 ja .Low_end
 test rsi,rsi
 jz .Low_empty
 test rdi,rdi
 jz .Low_end
 mov rbx,rdi
 mov rbp,rdx
 mov r13,rcx
 xor r10d,r10d
 mov r11,rsi
 test rbp,rbp
 jz .Low_zero
 test r13,r13
 jz .Low_zero
 test r8d,r8d
 jnz .Low_tail
 xor eax,eax
 xor r12d,r12d
.Low_head_lines:
 cmp rax,rsi
 jae .Low_head_bytes
 cmp byte ptr [rbx+rax],10
 jne .Low_head_next
 inc r12
 cmp r12,r13
 jne .Low_head_next
 lea r11,[rax+1]
 jmp .Low_head_bytes
.Low_head_next:
 inc rax
 jmp .Low_head_lines
.Low_head_bytes:
 cmp r11,rbp
 jbe .Low_counts
 mov rax,rbp
.Low_head_newline:
 test rax,rax
 jz .Low_head_character
 dec rax
 cmp byte ptr [rbx+rax],10
 jne .Low_head_newline
 lea r11,[rax+1]
 jmp .Low_counts
.Low_head_character:
 mov r11,rbp
.Low_head_boundary:
 test r11,r11
 jz .Low_counts
 movzx eax,byte ptr [rbx+r11]
 and eax,192
 cmp eax,128
 jne .Low_counts
 dec r11
 jmp .Low_head_boundary
.Low_tail:
 lea rax,[rsi-1]
 cmp byte ptr [rbx+rax],10
 jne .Low_tail_start
 dec rax
.Low_tail_start:
 mov r12d,1
.Low_tail_lines:
 test rax,rax
 js .Low_tail_bytes
 cmp byte ptr [rbx+rax],10
 jne .Low_tail_previous
 cmp r12,r13
 je .Low_tail_line_cut
 inc r12
.Low_tail_previous:
 dec rax
 jmp .Low_tail_lines
.Low_tail_line_cut:
 lea r10,[rax+1]
.Low_tail_bytes:
 mov rax,rsi
 sub rax,r10
 cmp rax,rbp
 jbe .Low_counts
 mov r10,rsi
 sub r10,rbp
 lea rax,[r10-1]
.Low_tail_newline:
 cmp rax,rsi
 jae .Low_tail_boundary
 cmp byte ptr [rbx+rax],10
 je .Low_tail_newline_found
 inc rax
 jmp .Low_tail_newline
.Low_tail_newline_found:
 inc rax
 cmp rax,rsi
 jae .Low_tail_boundary
 mov r10,rax
 jmp .Low_counts
.Low_tail_boundary:
 cmp r10,rsi
 jae .Low_counts
 movzx eax,byte ptr [rbx+r10]
 and eax,192
 cmp eax,128
 jne .Low_counts
 inc r10
 jmp .Low_tail_boundary
.Low_zero:
 test r8d,r8d
 jz .Low_head_zero
 mov r10,rsi
 jmp .Low_counts
.Low_head_zero:
 xor r11d,r11d
.Low_counts:
 xor r12d,r12d
 xor r13d,r13d
 xor eax,eax
.Low_count_each:
 cmp rax,rsi
 jae .Low_count_last
 cmp byte ptr [rbx+rax],10
 jne .Low_count_next
 inc r12
 cmp rax,r10
 jb .Low_count_next
 cmp rax,r11
 jae .Low_count_next
 inc r13
.Low_count_next:
 inc rax
 jmp .Low_count_each
.Low_count_last:
 cmp byte ptr [rbx+rsi-1],10
 je .Low_kept_last
 inc r12
.Low_kept_last:
 cmp r11,r10
 je .Low_store
 cmp byte ptr [rbx+r11-1],10
 je .Low_store
 inc r13
.Low_store:
 mov [r9],r10
 sub r11,r10
 mov [r9+8],r11
 sub rsi,r11
 mov [r9+16],rsi
 sub r12,r13
 mov [r9+24],r12
 xor eax,eax
 jmp .Low_end
.Low_empty:
 pxor xmm0,xmm0
 movdqu [r9],xmm0
 movdqu [r9+16],xmm0
 xor eax,eax
.Low_end:
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_output_bound,.-pg_output_bound
.section .note.GNU-stack,"",@progbits
.text
/* In-place sanitizeOutput over complete UTF-8 text, including embedded NUL. */
.globl pg_output_sanitize
.type pg_output_sanitize,@function
pg_output_sanitize:
 xor eax,eax
 xor ecx,ecx
.Los_each:
 cmp rcx,rsi
 jae .Los_end
 movzx edx,byte ptr [rdi+rcx]
 cmp dl,32
 jae .Los_unicode
 cmp dl,9
 je .Los_keep
 cmp dl,10
 je .Los_keep
 inc rcx
 jmp .Los_each
.Los_unicode:
 cmp dl,239
 jne .Los_keep
 lea r8,[rcx+2]
 cmp r8,rsi
 jae .Los_keep
 cmp byte ptr [rdi+rcx+1],191
 jne .Los_keep
 movzx r8d,byte ptr [rdi+rcx+2]
 cmp r8d,185
 jb .Los_keep
 cmp r8d,187
 ja .Los_keep
 add rcx,3
 jmp .Los_each
.Los_keep:
 mov [rdi+rax],dl
 inc rax
 inc rcx
 jmp .Los_each
.Los_end:
 ret
.size pg_output_sanitize,.-pg_output_sanitize
.text
.macro BF n
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\n
.endm
.macro BE n
 add rsp,\n
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
/* Complete decoded chunks; counters measure unsanitized text. */
.globl pg_output_buffer_push
.type pg_output_buffer_push,@function
pg_output_buffer_push:
 BF 16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 test r13,r13
 jz .Lob_push_ok
 test r12,r12
 jz .Lob_push_bad
 xor r14d,r14d
 xor eax,eax
.Lob_count_chunk:
 cmp rax,r13
 jae .Lob_chunk_counted
 cmp byte ptr [r12+rax],10
 jne .Lob_count_next
 inc r14
.Lob_count_next:
 inc rax
 jmp .Lob_count_chunk
.Lob_chunk_counted:
 cmp dword ptr [rbx+56],0
 jne .Lob_accept_count
 mov rdi,[rbx+8]
 add rdi,r13
 jc .Lob_push_bad
 cmp rdi,2147483646
 ja .Lob_push_bad
 inc rdi
 mov rsi,rdi
 mov rdi,[rbx]
 call realloc@PLT
 test rax,rax
 jz .Lob_push_alloc
 mov [rbx],rax
 mov rdi,rax
 add rdi,[rbx+8]
 mov rsi,r12
 mov rdx,r13
 call memcpy@PLT
 add [rbx+8],r13
 cmp dword ptr [rbx+48],0
 jne .Lob_accept_count
 mov rax,[rbx+8]
 cmp rax,[rbx+32]
 ja .Lob_head_full
 mov rdi,[rbx]
 mov rsi,[rbx+8]
 call .Lob_newlines
 cmp rax,[rbx+40]
 jb .Lob_accept_count
.Lob_head_full:
 mov dword ptr [rbx+56],1
.Lob_accept_count:
 add [rbx+16],r13
 add [rbx+24],r14
 cmp byte ptr [r12+r13-1],10
 sete al
 movzx eax,al
 mov [rbx+52],eax
.Lob_push_ok:
 xor eax,eax
 jmp .Lob_push_end
.Lob_push_alloc:
 mov eax,5
 jmp .Lob_push_end
.Lob_push_bad:
 mov eax,4
.Lob_push_end:
 BE 16
.size pg_output_buffer_push,.-pg_output_buffer_push
.Lob_newlines:
 xor eax,eax
 xor ecx,ecx
.Lob_newline_each:
 cmp rcx,rsi
 jae .Lob_newline_end
 cmp byte ptr [rdi+rcx],10
 jne .Lob_newline_next
 inc rax
.Lob_newline_next:
 inc rcx
 jmp .Lob_newline_each
.Lob_newline_end:
 ret
.globl pg_output_buffer_snapshot
.type pg_output_buffer_snapshot,@function
pg_output_buffer_snapshot:
 BF 64
 mov rbx,rdi
 mov r12,rsi
 pxor xmm0,xmm0
 movdqu [rsi],xmm0
 movdqu [rsi+16],xmm0
 mov qword ptr [rsp+32],0
 mov rdi,[rbx]
 mov rsi,[rbx+8]
 mov rdx,[rbx+32]
 mov rcx,[rbx+40]
 mov r8d,[rbx+48]
 mov r9,rsp
 call pg_output_bound
 test eax,eax
 jnz .Lob_snapshot_end
 mov rdi,[rsp+8]
 inc rdi
 call malloc@PLT
 test rax,rax
 jz .Lob_snapshot_alloc
 mov [rsp+32],rax
 mov rdi,rax
 mov rsi,[rbx]
 add rsi,[rsp]
 mov rdx,[rsp+8]
 call memcpy@PLT
 mov rdi,[rsp+32]
 mov rsi,[rsp+8]
 call pg_output_sanitize
 mov r13,rax
 mov rdx,[rsp+32]
 mov byte ptr [rdx+rax],0
 mov rdi,[rbx]
 mov rsi,[rbx+8]
 call .Lob_newlines
 mov r14,rax
 cmp qword ptr [rbx+8],0
 je .Lob_stored_lines
 mov rdx,[rbx]
 mov rcx,[rbx+8]
 cmp byte ptr [rdx+rcx-1],10
 je .Lob_stored_lines
 inc r14
.Lob_stored_lines:
 sub r14,[rsp+24]
 mov rax,[rbx+24]
 cmp qword ptr [rbx+16],0
 je .Lob_total_lines
 cmp dword ptr [rbx+52],0
 jne .Lob_total_lines
 inc rax
.Lob_total_lines:
 sub rax,r14
 mov [r12+24],rax
 mov rax,[rbx+16]
 sub rax,[rsp+8]
 mov [r12+16],rax
 mov [r12+8],r13
 mov rax,[rsp+32]
 mov [r12],rax
 mov qword ptr [rsp+32],0
 cmp dword ptr [rbx+48],0
 je .Lob_snapshot_ok
 /* Chord tailMargin retains the character before the byte window and the
    newline before the line window; both are needed for the next chunk. */
 mov r13,[rbx+8]
 xor r14d,r14d
 mov rax,[rbx+32]
 cmp r13,rax
 jbe .Lob_margin_lines
 mov r14,r13
 sub r14,rax
 dec r14
 mov rdx,[rbx]
.Lob_margin_boundary:
 test r14,r14
 jz .Lob_margin_lines
 movzx eax,byte ptr [rdx+r14]
 and eax,192
 cmp eax,128
 jne .Lob_margin_lines
 dec r14
 jmp .Lob_margin_boundary
.Lob_margin_lines:
 mov rax,r13
 xor ecx,ecx
 mov rdx,[rbx]
.Lob_margin_line_each:
 test rax,rax
 jz .Lob_margin_compact
 dec rax
 cmp byte ptr [rdx+rax],10
 jne .Lob_margin_line_each
 inc rcx
 cmp rcx,[rbx+40]
 jbe .Lob_margin_line_each
 cmp r14,rax
 cmovb r14,rax
.Lob_margin_compact:
 test r14,r14
 jz .Lob_snapshot_ok
 sub r13,r14
 mov rdi,[rbx]
 mov rsi,rdi
 add rsi,r14
 mov rdx,r13
 call memmove@PLT
 mov [rbx+8],r13
.Lob_snapshot_ok:
 xor eax,eax
 jmp .Lob_snapshot_end
.Lob_snapshot_alloc:
 mov eax,5
.Lob_snapshot_end:
 mov ebp,eax
 mov rdi,[rsp+32]
 call free@PLT
 mov eax,ebp
 BE 64
.size pg_output_buffer_snapshot,.-pg_output_buffer_snapshot
.globl pg_output_buffer_close
.type pg_output_buffer_close,@function
pg_output_buffer_close:
 push rbx
 mov rbx,rdi
 mov rdi,[rdi]
 call free@PLT
 mov rdi,rbx
 xor eax,eax
 mov ecx,8
 rep stosq
 pop rbx
 ret
.size pg_output_buffer_close,.-pg_output_buffer_close
