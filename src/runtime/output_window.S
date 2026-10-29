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
