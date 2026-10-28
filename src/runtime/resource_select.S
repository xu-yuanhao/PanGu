/* Pi Coding Agent package-manager resource selection native adaptation.
MIT License

Copyright (c) 2025 Mario Zechner
Copyright Julian Gruber <julian@juliangruber.com>
TypeScript port Copyright Isaac Z. Schlueter <i@izs.me>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.*/
.intel_syntax noprefix
.section .rodata
.Lbase: .asciz "base"
.Lpatterns: .asciz "patterns"
.Ldefaults: .asciz "defaults"
.Lskill: .asciz "SKILL.md"
.Lup: .asciz "../"
.Linteger: .asciz "%lld"
.Lpadded: .asciz "%0*lld"
.text
.macro enter bytes=0
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,\bytes+8
.endm
.macro exit bytes=0
 add rsp,\bytes+8
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.globl pg_resource_glob
.type pg_resource_glob,@function
pg_resource_glob:
 xor edx,edx
.Lglob:
 enter 8272
 mov rbx,rdi
 mov r12,rsi
 mov eax,edx
 and eax,0x7fffffff
 mov [rsp+8192],eax
 mov [rsp+8256],edx
 mov qword ptr [rsp+8200],0
 mov qword ptr [rsp+8208],0
 mov qword ptr [rsp+8216],0
 cmp eax,32
 jae .Lglob_error
 test rbx,rbx
 jz .Lglob_error
 test r12,r12
 jz .Lglob_error
 mov rdi,rbx
 call strlen@PLT
 cmp rax,4096
 jae .Lglob_error
 mov [rsp+8224],rax
 mov rdi,r12
 call strlen@PLT
 cmp rax,4096
 jae .Lglob_error
 test dword ptr [rsp+8256],0x80000000
 jnz .Lglob_brace_start
 /* Comments and repeated leading negation follow minimatch. */
 cmp byte ptr [rbx],'#'
 je .Lglob_no
.Lglob_negation:
 cmp byte ptr [rbx],'!'
 jne .Lglob_brace_start
 xor dword ptr [rsp+8200],1
 inc rbx
 jmp .Lglob_negation
.Lglob_brace_start:
 mov rax,rbx
1: cmp byte ptr [rax],0
 je .Lglob_segments
 cmp byte ptr [rax],92
 jne 2f
 cmp byte ptr [rax+1],0
 je .Lglob_segments
 add rax,2
 jmp 1b
2: cmp byte ptr [rax],'{'
 je 3f
 inc rax
 jmp 1b
3:
 mov [rsp+8208],rax
 mov r13,rax
 mov r14d,1
 xor r15d,r15d
.Lglob_brace_end:
 inc r13
 mov al,[r13]
 test al,al
 jz .Lglob_segments
 cmp al,92
 jne 1f
 cmp byte ptr [r13+1],0
 je .Lglob_segments
 inc r13
 jmp .Lglob_brace_end
1: cmp al,'{'
 jne 2f
 inc r14d
 jmp .Lglob_brace_end
2: cmp al,'}'
 jne 3f
 dec r14d
 jz .Lglob_brace_ready
 jmp .Lglob_brace_end
3: cmp al,','
 jne .Lglob_brace_end
 cmp r14d,1
 jne .Lglob_brace_end
 mov r15d,1
 jmp .Lglob_brace_end
.Lglob_brace_ready:
 mov rax,[rsp+8208]
 cmp rax,rbx
 je 1f
 cmp byte ptr [rax-1],'$'
 je .Lglob_segments
1: test r15d,r15d
 jnz .Lglob_brace_options
 mov rdi,rbx
 mov rsi,[rsp+8208]
 mov rdx,r13
 mov rcx,r12
 mov r8d,[rsp+8192]
 call .Lrange
 cmp eax,2
 je .Lglob_segments
 test eax,eax
 js .Lglob_error
 jmp .Lglob_result
.Lglob_brace_options:
 mov [rsp+8216],r13
 mov rax,[rsp+8208]
 inc rax
 mov [rsp+8232],rax
 mov r13,rax
 xor r14d,r14d
.Lglob_alternative:
 mov al,[r13]
 cmp al,92
 jne 1f
 cmp byte ptr [r13+1],0
 je .Lglob_error
 add r13,2
 jmp .Lglob_alternative
1: cmp al,'{'
 jne 2f
 inc r14d
 jmp .Lglob_alternative_next
2: cmp al,'}'
 jne 3f
 test r14d,r14d
 jz .Lglob_alternative_emit
 dec r14d
 jmp .Lglob_alternative_next
3: cmp al,','
 jne .Lglob_alternative_next
 test r14d,r14d
 jnz .Lglob_alternative_next
.Lglob_alternative_emit:
 mov rdi,rsp
 mov rsi,rbx
 mov rdx,[rsp+8208]
 sub rdx,rbx
 mov [rsp+8240],rdx
 call memcpy@PLT
 mov rdx,r13
 sub rdx,[rsp+8232]
 mov r15,rdx
 mov rdi,rsp
 add rdi,[rsp+8240]
 mov rsi,[rsp+8232]
 call memcpy@PLT
 add [rsp+8240],r15
 mov rdi,[rsp+8216]
 inc rdi
 call strlen@PLT
 lea rdx,[rax+1]
 mov rdi,rsp
 add rdi,[rsp+8240]
 mov rsi,[rsp+8216]
 inc rsi
 call memcpy@PLT
 cmp byte ptr [rsp],0
 je .Lglob_alternative_failed
 mov rdi,rsp
 mov rsi,r12
 mov edx,[rsp+8192]
 inc edx
 or edx,0x80000000
 call .Lglob
 test eax,eax
 js .Lglob_error
 jnz .Lglob_yes
.Lglob_alternative_failed:
 cmp r13,[rsp+8216]
 je .Lglob_no
 inc r13
 mov [rsp+8232],r13
 jmp .Lglob_alternative
.Lglob_alternative_next:
 inc r13
 jmp .Lglob_alternative
.Lglob_segments:
 /* Split segments before fnmatch so ** consumes zero or more directories. */
 mov rdi,rbx
 mov rsi,r12
 xor edx,edx
 call .Lsegments
 test eax,eax
 js .Lglob_error
 jmp .Lglob_result
.Lglob_yes:
 mov eax,1
 jmp .Lglob_result
.Lglob_no:
 xor eax,eax
.Lglob_result:
 xor eax,[rsp+8200]
 exit 8272
.Lglob_error:
 mov eax,-1
 exit 8272
.size pg_resource_glob,.-pg_resource_glob
.Lsegments:
 enter 64
 mov rbp,rsp
 mov rbx,rdi
 mov r12,rsi
 mov r13d,edx
 cmp edx,4096
 jae .Lsegments_error
 /* Repeated separators have the same meaning as one separator. */
 cmp byte ptr [rbx],'/'
 jne .Lsegments_normal
 cmp byte ptr [r12],'/'
 jne .Lsegments_no
1:  cmp byte ptr [rbx],'/'
 jne 2f
 inc rbx
 jmp 1b
2: cmp byte ptr [r12],'/'
 jne .Lsegments_normal
 inc r12
 jmp 2b
.Lsegments_normal:
 mov rdi,rbx
 mov esi,'/'
 call strchr@PLT
 mov r14,rax
 test rax,rax
 jnz 1f
 mov rdi,rbx
 call strlen@PLT
 lea r14,[rbx+rax]
1: mov rdi,r12
 mov esi,'/'
 call strchr@PLT
 mov r15,rax
 test rax,rax
 jnz 2f
 mov rdi,r12
 call strlen@PLT
 lea r15,[r12+rax]
2: mov rax,r14
 sub rax,rbx
 cmp rax,2
 jne .Lsegments_literal
 cmp word ptr [rbx],0x2a2a
 jne .Lsegments_literal
 mov rax,r14
 cmp byte ptr [rax],0
 je .Lsegments_globstar_tail
1: inc rax
 cmp byte ptr [rax],'/'
 je 1b
 mov rdi,rax
 mov rsi,r12
 lea edx,[r13+1]
 call .Lsegments
 test eax,eax
 jnz .Lsegments_done
 cmp byte ptr [r12],0
 je .Lsegments_no
 cmp byte ptr [r12],'.'
 je .Lsegments_no
 cmp byte ptr [r15],0
 je .Lsegments_no
 mov rdi,rbx
 lea rsi,[r15+1]
 lea edx,[r13+1]
 call .Lsegments
 jmp .Lsegments_done
.Lsegments_globstar_tail:
 cmp byte ptr [r12],0
 je .Lsegments_yes
 cmp byte ptr [r12],'.'
 je .Lsegments_no
 cmp byte ptr [r15],0
 je .Lsegments_yes
 mov rdi,rbx
 lea rsi,[r15+1]
 lea edx,[r13+1]
 call .Lsegments
 jmp .Lsegments_done
.Lsegments_literal:
 cmp r15,r12
 jne .Lsegments_allocate
 cmp r14,rbx
 je .Lsegments_allocate
 mov rax,rbx
1: cmp rax,r14
 jae .Lsegments_no
 cmp byte ptr [rax],'*'
 jne .Lsegments_allocate
 inc rax
 jmp 1b
.Lsegments_allocate:
 mov rax,r14
 sub rax,rbx
 mov rdx,r15
 sub rdx,r12
 add rax,rdx
 lea rax,[rax*2+17]
 and rax,-16
 mov [rbp],rax
 sub rsp,rax
 mov rdi,rbx
 mov rsi,r14
 sub rsi,rbx
 mov rdx,rsp
 call .Lunits
 test rax,rax
 js .Lsegments_restore_error
 lea rdx,[rsp+rax+1]
 mov [rbp+8],rdx
 mov rdi,r12
 mov rsi,r15
 sub rsi,r12
 call .Lunits
 test rax,rax
 js .Lsegments_restore_error
 mov rdi,rsp
 mov rsi,[rbp+8]
 call pg_resource_fnmatch
 add rsp,[rbp]
 test eax,eax
 js .Lsegments_error
 jnz .Lsegments_no
 mov rax,r15
 sub rax,r12
 cmp rax,1
 je 1f
 cmp rax,2
 jne .Lsegments_after_dot
 cmp word ptr [r12],0x2e2e
 jne .Lsegments_after_dot
 jmp 2f
1: cmp byte ptr [r12],'.'
 jne .Lsegments_after_dot
2: mov rax,rbx
3: cmp rax,r14
 jae .Lsegments_after_dot
 cmp byte ptr [rax],92
 jne 4f
 add rax,2
 jmp 3b
4: cmp byte ptr [rax],'*'
 je .Lsegments_no
 cmp byte ptr [rax],'?'
 je .Lsegments_no
 cmp byte ptr [rax],'['
 je .Lsegments_no
 inc rax
 jmp 3b
.Lsegments_after_dot:
 cmp byte ptr [r14],0
 jne .Lsegments_continue
 cmp byte ptr [r15],0
 je .Lsegments_yes
 mov rax,r15
1: cmp byte ptr [rax],'/'
 jne .Lsegments_no
 inc rax
 cmp byte ptr [rax],0
 jne 1b
 jmp .Lsegments_yes
.Lsegments_continue:
 cmp byte ptr [r15],0
 jne 1f
 jmp .Lsegments_no
1: lea rdi,[r14+1]
2: cmp byte ptr [rdi],'/'
 jne 3f
 inc rdi
 jmp 2b
3: lea rsi,[r15+1]
4: cmp byte ptr [rsi],'/'
 jne 5f
 inc rsi
 jmp 4b
5: lea edx,[r13+1]
 call .Lsegments
 jmp .Lsegments_done
.Lsegments_yes:
 mov eax,1
 jmp .Lsegments_done
.Lsegments_no:
 xor eax,eax
 jmp .Lsegments_done
.Lsegments_restore_error:
 add rsp,[rbp]
.Lsegments_error:
 mov eax,-1
.Lsegments_done:
 exit 64
/* Encode JS UTF-16 units as private-use UTF-8 scalars for libc's wchar matcher.
   ASCII syntax is unchanged; all other code units retain their numeric order. */
.Lunits:
 enter 16
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 xor r14d,r14d
 xor r15d,r15d
.Lunits_loop:
 cmp r14,r12
 jae .Lunits_end
 movzx eax,byte ptr [rbx+r14]
 cmp eax,128
 jae 1f
 mov [r13+r15],al
 inc r14
 inc r15
 jmp .Lunits_loop
1: lea rdi,[rbx+r14]
 mov rsi,r12
 sub rsi,r14
 mov rdx,rsp
 call utf8proc_iterate@PLT
 test rax,rax
 jle .Lunits_error
 add r14,rax
 mov edi,[rsp]
 cmp edi,65535
 jbe 2f
 sub edi,65536
 mov [rsp+8],edi
 shr edi,10
 add edi,0xd800
 call .Lunit_emit
 mov edi,[rsp+8]
 and edi,1023
 add edi,0xdc00
2: call .Lunit_emit
 jmp .Lunits_loop
.Lunits_end:
 mov byte ptr [r13+r15],0
 mov rax,r15
 exit 16
.Lunits_error:
 mov rax,-1
 exit 16
.Lunit_emit:
 add edi,0xf0000
 mov byte ptr [r13+r15],0xf3
 mov eax,edi
 shr eax,12
 and eax,63
 or eax,128
 mov [r13+r15+1],al
 mov eax,edi
 shr eax,6
 and eax,63
 or eax,128
 mov [r13+r15+2],al
 mov eax,edi
 and eax,63
 or eax,128
 mov [r13+r15+3],al
 add r15,4
 ret
.Lrange:
 enter 128
 mov rbx,rdi
 mov r12,rcx
 mov r13,rsi
 mov [rsp+120],rsi
 mov r14,rdx
 mov r15d,r8d
 mov qword ptr [rsp+16],1
 mov qword ptr [rsp+24],0
 mov qword ptr [rsp+40],0
 mov qword ptr [rsp+72],0
 lea rdi,[r13+1]
 movzx eax,byte ptr [rdi]
 cmp eax,'A'
 jb .Lrange_numeric
 cmp eax,'Z'
 jbe .Lrange_alpha
 cmp eax,'a'
 jb .Lrange_numeric
 cmp eax,'z'
 jbe .Lrange_alpha
.Lrange_numeric:
 call .Lrange_number
 test edx,edx
 jz .Lrange_literal
 mov [rsp],rax
 mov rax,[rsp+80]
 sub rax,r13
 dec rax
 mov [rsp+88],rax
 mov rdi,[rsp+80]
 lea rax,[rdi+2]
 cmp rax,r14
 ja .Lrange_literal
 cmp word ptr [rdi],0x2e2e
 jne .Lrange_literal
 add rdi,2
 mov [rsp+96],rdi
 call .Lrange_number
 test edx,edx
 jz .Lrange_literal
 mov [rsp+8],rax
 mov rax,[rsp+80]
 sub rax,[rsp+96]
 mov rcx,[rsp+88]
 cmp rax,rcx
 cmovb rax,rcx
 mov [rsp+32],rax
 jmp .Lrange_step
.Lrange_alpha:
 mov qword ptr [rsp+24],1
 mov [rsp],rax
 cmp word ptr [rdi+1],0x2e2e
 jne .Lrange_literal
 lea rax,[rdi+3]
 cmp rax,r14
 jae .Lrange_literal
 movzx eax,byte ptr [rax]
 cmp eax,'A'
 jb .Lrange_literal
 cmp eax,'Z'
 jbe 1f
 cmp eax,'a'
 jb .Lrange_literal
 cmp eax,'z'
 ja .Lrange_literal
1: mov [rsp+8],rax
 lea rax,[rdi+4]
 mov [rsp+80],rax
.Lrange_step:
 mov rdi,[rsp+80]
 cmp rdi,r14
 je .Lrange_validate
 lea rax,[rdi+2]
 cmp rax,r14
 jae .Lrange_literal
 cmp word ptr [rdi],0x2e2e
 jne .Lrange_literal
 add rdi,2
 call .Lrange_number
 test edx,edx
 jz .Lrange_literal
 cmp [rsp+80],r14
 jne .Lrange_literal
 mov [rsp+16],rax
.Lrange_validate:
 xor r13d,r13d
1: mov rax,[rsp+r13]
 mov rcx,rax
 neg rcx
 test rax,rax
 cmovs rax,rcx
 mov rcx,9007199254740991
 cmp rax,rcx
 ja .Lrange_error
 add r13,8
 cmp r13,24
 jb 1b
 mov rax,[rsp+16]
 test rax,rax
 jns 2f
 neg rax
2: test rax,rax
 jnz 3f
 mov eax,1
3: mov rcx,[rsp]
 cmp rcx,[rsp+8]
 jle 4f
 neg rax
4: mov [rsp+16],rax
 mov edi,8192
 call malloc@PLT
 mov [rsp+72],rax
 test rax,rax
 jz .Lrange_error
 mov qword ptr [rsp+48],0
 mov qword ptr [rsp+56],0
.Lrange_loop:
 cmp qword ptr [rsp+48],100000
 jae .Lrange_no
 mov rax,[rsp]
 cmp qword ptr [rsp+16],0
 jl 1f
 cmp rax,[rsp+8]
 jg .Lrange_no
 jmp 2f
1: cmp rax,[rsp+8]
 jl .Lrange_no
2: mov r13,[rsp+72]
 add r13,4096
 cmp qword ptr [rsp+24],0
 je .Lrange_format
 mov byte ptr [r13],al
 mov byte ptr [r13+1],0
 mov qword ptr [rsp+104],1
 cmp al,92
 jne .Lrange_candidate
 mov byte ptr [r13],0
 mov qword ptr [rsp+104],0
 jmp .Lrange_candidate
.Lrange_format:
 mov rdi,r13
 mov esi,4096
 cmp qword ptr [rsp+40],0
 jne 1f
 lea rdx,[rip+.Linteger]
 mov rcx,[rsp]
 xor eax,eax
 call snprintf@PLT
 jmp 2f
1: lea rdx,[rip+.Lpadded]
 mov ecx,[rsp+32]
 mov r8,[rsp]
 xor eax,eax
 call snprintf@PLT
2: test eax,eax
 js .Lrange_error
 cmp eax,4096
 jae .Lrange_error
 movsxd rax,eax
 mov [rsp+104],rax
.Lrange_candidate:
 mov r13,[rsp+120]
 mov rdx,r13
 sub rdx,rbx
 mov [rsp+112],rdx
 mov rdi,[rsp+72]
 mov rsi,rbx
 call memcpy@PLT
 mov rdi,[rsp+72]
 add rdi,[rsp+112]
 mov rsi,[rsp+72]
 add rsi,4096
 mov rdx,[rsp+104]
 call memcpy@PLT
 mov rdi,r14
 inc rdi
 call strlen@PLT
 add rax,[rsp+104]
 add rax,[rsp+112]
 cmp rax,4096
 jae .Lrange_error
 add [rsp+56],rax
 cmp qword ptr [rsp+56],4000000
 ja .Lrange_no
 mov rdi,[rsp+72]
 add rdi,[rsp+112]
 add rdi,[rsp+104]
 lea rsi,[r14+1]
 mov rdx,rax
 sub rdx,[rsp+112]
 sub rdx,[rsp+104]
 inc rdx
 call memcpy@PLT
 mov rdi,[rsp+72]
 mov rsi,r12
 lea edx,[r15+1]
 or edx,0x80000000
 call .Lglob
 test eax,eax
 js .Lrange_error
 jnz .Lrange_yes
 mov rax,[rsp+16]
 add [rsp],rax
 inc qword ptr [rsp+48]
 jmp .Lrange_loop
.Lrange_literal:
 mov eax,2
 jmp .Lrange_end
.Lrange_yes:
 mov eax,1
 jmp .Lrange_end
.Lrange_no:
 xor eax,eax
 jmp .Lrange_end
.Lrange_error:
 mov eax,-1
.Lrange_end:
 mov r13d,eax
 mov rdi,[rsp+72]
 call free@PLT
 mov eax,r13d
 exit 128
/* Number at rdi; parent range frame holds end pointer and padding state. */
.Lrange_number:
 sub rsp,24
 mov [rsp],rdi
 movzx eax,byte ptr [rdi]
 cmp al,'-'
 jne 1f
 inc rdi
 movzx eax,byte ptr [rdi]
1: cmp al,'0'
 jb 3f
 cmp al,'9'
 ja 3f
 cmp al,'0'
 jne 2f
 movzx eax,byte ptr [rdi+1]
 cmp al,'0'
 jb 2f
 cmp al,'9'
 ja 2f
 mov qword ptr [rsp+72],1
2: mov rdi,[rsp]
 lea rsi,[rsp+112]
 mov edx,10
 call strtoll@PLT
 mov edx,1
 jmp 4f
3: xor edx,edx
4: add rsp,24
 ret
/* Lexical relative path between normalized absolute paths. Output >=12288. */
.Lrelative:
 enter 32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov rdi,rbx
 call strlen@PLT
 mov r14,rax
 mov rdi,r12
 call strlen@PLT
 mov r15,rax
 xor ecx,ecx
 xor edx,edx
.Lrelative_common:
 cmp rcx,r14
 jae .Lrelative_boundary
 cmp rcx,r15
 jae .Lrelative_boundary
 mov al,[rbx+rcx]
 cmp al,[r12+rcx]
 jne .Lrelative_boundary
 cmp al,'/'
 jne 1f
 lea rdx,[rcx+1]
1: inc rcx
 jmp .Lrelative_common
.Lrelative_boundary:
 mov [rsp],rdx
 mov [rsp+8],rdx
 cmp rcx,r14
 jne 2f
 cmp byte ptr [r12+rcx],0
 je 1f
 cmp byte ptr [r12+rcx],'/'
 jne 2f
 mov [rsp],rcx
 inc rcx
 mov [rsp+8],rcx
 jmp .Lrelative_upper
1: mov [rsp],rcx
 mov [rsp+8],rcx
 jmp .Lrelative_upper
2: cmp rcx,r15
 jne .Lrelative_upper
 cmp byte ptr [rbx+rcx],'/'
 jne .Lrelative_upper
 mov [rsp+8],rcx
 inc rcx
 mov [rsp],rcx
.Lrelative_upper:
 xor r14d,r14d
 mov r15,[rsp]
1: cmp byte ptr [rbx+r15],0
 je .Lrelative_suffix
 cmp byte ptr [rbx+r15],'/'
 jne 2f
 inc r15
 jmp 1b
2: mov byte ptr [r13+r14],'.'
 mov byte ptr [r13+r14+1],'.'
 mov byte ptr [r13+r14+2],'/'
 add r14,3
3: cmp byte ptr [rbx+r15],0
 je .Lrelative_suffix
 cmp byte ptr [rbx+r15],'/'
 je 1b
 inc r15
 jmp 3b
.Lrelative_suffix:
 mov rdi,r12
 add rdi,[rsp+8]
 call strlen@PLT
 test rax,rax
 jnz 1f
 test r14,r14
 jz 2f
 dec r14
2: mov byte ptr [r13+r14],0
 jmp 3f
1: lea rdi,[r13+r14]
 mov rsi,r12
 add rsi,[rsp+8]
 lea rdx,[rax+1]
 call memcpy@PLT
3: exit 32
/* Pattern, six borrowed path variants, isSkill, exact. */
.Lvariants:
 enter 0
 mov rbx,rdi
 mov r12,rsi
 mov r13d,edx
 mov r14d,ecx
 xor r15d,r15d
.Lvariants_loop:
 cmp r15,6
 jae .Lvariants_no
 cmp r15,3
 jb 1f
 test r13d,r13d
 jz .Lvariants_no
1: test r14d,r14d
 jz 2f
 cmp r15,1
 je .Lvariants_next
 cmp r15,4
 je .Lvariants_next
 mov rdi,rbx
 mov rsi,[r12+r15*8]
 call strcmp@PLT
 test eax,eax
 jz .Lvariants_yes
 jmp .Lvariants_next
2: mov rdi,rbx
 mov rsi,[r12+r15*8]
 call pg_resource_glob
 test eax,eax
 js .Lvariants_end
 jnz .Lvariants_yes
.Lvariants_next:
 inc r15
 jmp .Lvariants_loop
.Lvariants_yes:
 mov eax,1
 jmp .Lvariants_end
.Lvariants_no:
 xor eax,eax
.Lvariants_end:
 exit 0
.globl pg_resource_selected
.type pg_resource_selected,@function
pg_resource_selected:
 enter 144
 mov rbx,rdi
 mov r12,rsi
 xor ebp,ebp
 test rsi,rsi
 jz .Lselected_yes
 test rbx,rbx
 jz .Lselected_error
 mov rdi,rsi
 lea rsi,[rip+.Lpatterns]
 call json_object_object_get@PLT
 mov r13,rax
 test rax,rax
 jz .Lselected_yes
 mov rdi,rax
 mov esi,5
 call json_object_is_type@PLT
 test eax,eax
 jz .Lselected_error
 mov rdi,r13
 call json_object_array_length@PLT
 test rax,rax
 jz .Lselected_yes
 mov rdi,r12
 lea rsi,[rip+.Lbase]
 call json_object_object_get@PLT
 mov [rsp+64],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lselected_error
 mov rdi,[rsp+64]
 call json_object_get_string_len@PLT
 cmp eax,4096
 jae .Lselected_error
 mov rdi,[rsp+64]
 call json_object_get_string@PLT
 mov r15,rax
 mov rdi,r12
 lea rsi,[rip+.Ldefaults]
 call json_object_object_get@PLT
 mov rdi,rax
 call json_object_get_boolean@PLT
 mov [rsp+56],eax
 mov edi,32768
 call malloc@PLT
 mov rbp,rax
 test rax,rax
 jz .Lselected_error
 mov rdi,rbx
 call strlen@PLT
 cmp rax,4096
 jae .Lselected_error
 lea rdi,[rbp+12288]
 mov rsi,rbx
 lea rdx,[rax+1]
 call memcpy@PLT
 mov rdi,rbx
 mov esi,'/'
 call strrchr@PLT
 test rax,rax
 jnz 1f
 mov rax,rbx
 dec rax
1: inc rax
 mov [rsp+88],rax
 mov rdi,rax
 lea rsi,[rip+.Lskill]
 call strcmp@PLT
 xor edx,edx
 test eax,eax
 sete dl
 mov [rsp+24],edx
 lea rdi,[rbp+12288]
 mov esi,'/'
 call strrchr@PLT
 test rax,rax
 jz .Lselected_error
 lea rcx,[rbp+12288]
 cmp rax,rcx
 jne 2f
 inc rax
2: mov byte ptr [rax],0
 lea rdi,[rbp+12288]
 mov esi,'/'
 call strrchr@PLT
 test rax,rax
 jnz 3f
 lea rax,[rbp+12287]
3: inc rax
 mov [rsp+112],rax
 mov rdi,r15
 mov rsi,rbx
 mov rdx,rbp
 call .Lrelative
 mov rdi,r15
 lea rsi,[rbp+12288]
 lea rdx,[rbp+16384]
 call .Lrelative
 mov [rsp+80],rbp
 mov [rsp+96],rbx
 lea rax,[rbp+16384]
 mov [rsp+104],rax
 lea rax,[rbp+12288]
 mov [rsp+120],rax
 mov qword ptr [rsp],0
 xor r14d,r14d
.Lselected_loop:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp r14,rax
 jae .Lselected_result
 mov rdi,r13
 mov rsi,r14
 call json_object_array_get_idx@PLT
 mov [rsp+64],rax
 mov rdi,rax
 mov esi,6
 call json_object_is_type@PLT
 test eax,eax
 jz .Lselected_error
 mov rdi,[rsp+64]
 call json_object_get_string_len@PLT
 cmp eax,4096
 jae .Lselected_error
 mov [rsp+40],rax
 mov rdi,[rsp+64]
 call json_object_get_string@PLT
 mov [rsp+32],rax
 mov rdi,rax
 call strlen@PLT
 cmp rax,[rsp+40]
 jne .Lselected_error
 mov rax,[rsp+32]
 movzx ecx,byte ptr [rax]
 mov qword ptr [rsp+48],0
 cmp cl,'!'
 je .Lselected_exclude
 cmp cl,'+'
 je .Lselected_force_include
 cmp cl,'-'
 je .Lselected_force_exclude
 cmp dword ptr [rsp+56],0
 jne .Lselected_next
 mov rdi,rax
 mov esi,'*'
 call strchr@PLT
 test rax,rax
 jnz 1f
 mov rdi,[rsp+32]
 mov esi,'?'
 call strchr@PLT
 test rax,rax
 jz .Lselected_next
1: or qword ptr [rsp],1
 mov qword ptr [rsp+8],2
 jmp .Lselected_pattern
.Lselected_exclude:
 mov qword ptr [rsp+8],4
 inc qword ptr [rsp+32]
 dec qword ptr [rsp+40]
 jmp .Lselected_pattern
.Lselected_force_include:
 mov qword ptr [rsp+8],8
 jmp .Lselected_exact
.Lselected_force_exclude:
 mov qword ptr [rsp+8],16
.Lselected_exact:
 mov qword ptr [rsp+48],1
 inc qword ptr [rsp+32]
 dec qword ptr [rsp+40]
 cmp qword ptr [rsp+40],2
 jb .Lselected_pattern
 mov rax,[rsp+32]
 cmp word ptr [rax],0x2f2e
 jne .Lselected_pattern
 add qword ptr [rsp+32],2
 sub qword ptr [rsp+40],2
.Lselected_pattern:
 mov rax,[rsp+32]
 cmp byte ptr [rax],'~'
 jne .Lselected_copy_pattern
 cmp byte ptr [rax+1],0
 je .Lselected_home
 cmp byte ptr [rax+1],'/'
 jne .Lselected_copy_pattern
.Lselected_home:
 mov rdi,rax
 mov rsi,[rsp+40]
 lea rdx,[rbp+28672]
 mov ecx,4096
 call pg_tool_path_resolve
 test eax,eax
 jnz .Lselected_error
 jmp .Lselected_match
.Lselected_copy_pattern:
 lea rdi,[rbp+28672]
 mov rsi,[rsp+32]
 mov rdx,[rsp+40]
 inc rdx
 call memcpy@PLT
.Lselected_match:
 lea rdi,[rbp+28672]
 lea rsi,[rsp+80]
 mov edx,[rsp+24]
 mov ecx,[rsp+48]
 call .Lvariants
 test eax,eax
 js .Lselected_error
 jz .Lselected_next
 mov rax,[rsp+8]
 or [rsp],rax
.Lselected_next:
 inc r14
 jmp .Lselected_loop
.Lselected_result:
 mov rax,[rsp]
 test al,16
 jnz .Lselected_no
 test al,8
 jnz .Lselected_yes
 test al,4
 jnz .Lselected_no
 test al,1
 jz .Lselected_yes
 test al,2
 jnz .Lselected_yes
.Lselected_no:
 xor eax,eax
 jmp .Lselected_end
.Lselected_yes:
 mov eax,1
 jmp .Lselected_end
.Lselected_error:
 mov eax,-1
.Lselected_end:
 mov r12d,eax
 mov rdi,rbp
 call free@PLT
 mov eax,r12d
 exit 144
.size pg_resource_selected,.-pg_resource_selected
.section .note.GNU-stack,"",@progbits
