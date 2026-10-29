/* Pi Durable images/index.ts prepare/encodeWithin, d5b3de22d0e24a3e9c38a9410d3329b099717283; MIT.
   Codec handles and encoded bytes are released on every terminal path. */
/* MIT License

Copyright (c) 2025 Mario Zechner

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
.macro IP_ENTER
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,184
.endm
.macro IP_LEAVE
 add rsp,184
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.text
.Lip_set:
 test rdx,rdx
 jz .Lip_set_bad
 push rbx
 mov rbx,rdx
 call json_object_object_add@PLT
 test eax,eax
 jz .Lip_set_done
 mov rdi,rbx
 call json_object_put@PLT
 mov eax,5
.Lip_set_done:
 pop rbx
 ret
.Lip_set_bad:
 mov eax,5
 ret
.globl pg_image_prepare
.type pg_image_prepare,@function
pg_image_prepare:
 IP_ENTER
 mov rbx,rsi
 mov r12,rdx
 mov r13,rcx
 mov r14,r8
 mov r15,r9
 mov rdi,rsp
 xor eax,eax
 mov ecx,23
 rep stosq
 mov dword ptr [rsp+96],1
 test r15,r15
 jz .Lip_invalid
 mov qword ptr [r15],0
 test rbx,rbx
 jz .Lip_end
 test r12,r12
 jz .Lip_end
 test r13,r13
 jz .Lip_invalid
 test r14,r14
 jz .Lip_invalid
 cmp qword ptr [r14],0
 je .Lip_invalid
 cmp dword ptr [r14+8],0
 je .Lip_invalid
 cmp dword ptr [r14+12],0
 je .Lip_invalid
 mov rdi,rbx
 mov rsi,r12
 lea rdx,[rsp]
 lea rcx,[rsp+56]
 lea r8,[rsp+60]
 lea r9,[rsp+64]
 call pg_image_codec_open@PLT
 test eax,eax
 jnz .Lip_end
 mov rdi,r13
 lea rsi,[rip+.Lip_jpeg]
 call strcmp@PLT
 test eax,eax
 sete al
 movzx eax,al
 mov [rsp+68],eax
 mov eax,[rsp+56]
 mov [rsp+72],eax
 mov eax,[rsp+60]
 mov [rsp+76],eax
 cmp dword ptr [rsp+64],1
 jne .Lip_orient
 mov eax,[rsp+56]
 cmp eax,[r14+8]
 ja .Lip_orient
 mov eax,[rsp+60]
 cmp eax,[r14+12]
 ja .Lip_orient
 mov rdi,r12
 mov rsi,[r14]
 call .Lip_fits
 test eax,eax
 jz .Lip_orient
 xor ebp,ebp
.Lip_inline_each:
 lea rax,[rip+.Lip_inline]
 mov rsi,[rax+rbp*8]
 test rsi,rsi
 jz .Lip_orient
 mov rdi,r13
 call strcmp@PLT
 test eax,eax
 jz .Lip_passthrough
 inc ebp
 jmp .Lip_inline_each
.Lip_passthrough:
 mov [rsp+112],rbx
 mov [rsp+120],r12
 mov [rsp+128],r13
 jmp .Lip_json
.Lip_orient:
 mov rdi,[rsp]
 lea rsi,[rsp+8]
 lea rdx,[rsp+56]
 lea rcx,[rsp+60]
 call pg_image_codec_orient@PLT
 test eax,eax
 jnz .Lip_end
 movsd xmm0,[rip+.Lip_one]
 mov eax,[r14+8]
 cvtsi2sd xmm1,rax
 mov eax,[rsp+56]
 cvtsi2sd xmm2,rax
 divsd xmm1,xmm2
 minsd xmm0,xmm1
 mov eax,[r14+12]
 cvtsi2sd xmm1,rax
 mov eax,[rsp+60]
 cvtsi2sd xmm2,rax
 divsd xmm1,xmm2
 minsd xmm0,xmm1
 mov eax,[rsp+56]
 cvtsi2sd xmm1,rax
 mulsd xmm1,xmm0
 addsd xmm1,[rip+.Lip_half]
 cvttsd2si eax,xmm1
 test eax,eax
 jnz .Lip_width_ready
 mov eax,1
.Lip_width_ready:
 mov [rsp+72],eax
 mov eax,[rsp+60]
 cvtsi2sd xmm1,rax
 mulsd xmm1,xmm0
 addsd xmm1,[rip+.Lip_half]
 cvttsd2si eax,xmm1
 test eax,eax
 jnz .Lip_height_ready
 mov eax,1
.Lip_height_ready:
 mov [rsp+76],eax
.Lip_encode_round:
 mov dword ptr [rsp+80],0
.Lip_encode_candidate:
 mov eax,[rsp+80]
 cmp dword ptr [rsp+68],0
 je .Lip_png_first
 cmp eax,4
 je .Lip_candidate_png
 jmp .Lip_candidate_jpeg
.Lip_png_first:
 test eax,eax
 jz .Lip_candidate_png
 dec eax
.Lip_candidate_jpeg:
 lea rdx,[rip+.Lip_qualities]
 mov eax,[rdx+rax*4]
 mov [rsp+108],eax
 mov dword ptr [rsp+104],1
 lea rax,[rip+.Lip_jpeg]
 mov [rsp+128],rax
 jmp .Lip_encode
.Lip_candidate_png:
 mov dword ptr [rsp+108],0
 mov dword ptr [rsp+104],0
 lea rax,[rip+.Lip_png]
 mov [rsp+128],rax
.Lip_encode:
 mov rdi,[rsp+16]
 call free@PLT
 mov qword ptr [rsp+16],0
 mov eax,[rsp+72]
 mov [rsp+152],eax
 mov eax,[rsp+76]
 mov [rsp+156],eax
 mov rax,[rsp+104]
 mov [rsp+152+8],rax
 mov rdi,[rsp+8]
 lea rsi,[rsp+152]
 lea rdx,[rsp+16]
 call pg_image_codec_encode@PLT
 test eax,eax
 jnz .Lip_next_candidate
 mov rdi,[rsp+24]
 mov rsi,[r14]
 call .Lip_fits
 test eax,eax
 jnz .Lip_encoded
.Lip_next_candidate:
 inc dword ptr [rsp+80]
 cmp dword ptr [rsp+80],5
 jb .Lip_encode_candidate
 mov eax,[rsp+72]
 cmp eax,1
 jne .Lip_shrink
 cmp dword ptr [rsp+76],1
 je .Lip_end
.Lip_shrink:
 mov eax,[rsp+72]
 lea rax,[rax+rax*2]
 shr rax,2
 test eax,eax
 jnz .Lip_shrink_width
 mov eax,1
.Lip_shrink_width:
 mov [rsp+72],eax
 mov eax,[rsp+76]
 lea rax,[rax+rax*2]
 shr rax,2
 test eax,eax
 jnz .Lip_shrink_height
 mov eax,1
.Lip_shrink_height:
 mov [rsp+76],eax
 jmp .Lip_encode_round
.Lip_encoded:
 mov rax,[rsp+16]
 mov [rsp+112],rax
 mov rax,[rsp+24]
 mov [rsp+120],rax
.Lip_json:
 mov dword ptr [rsp+96],5
 mov rax,[rsp+120]
 add rax,2
 jc .Lip_end
 xor edx,edx
 mov ecx,3
 div rcx
 mov rdx,rax
 shr rdx,62
 jnz .Lip_end
 shl rax,2
 cmp rax,2147483647
 ja .Lip_end
 mov [rsp+144],rax
 lea rdi,[rax+1]
 call malloc@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lip_end
 mov rdi,[rsp+112]
 mov rsi,[rsp+120]
 mov rdx,rax
 call pg_image_base64@PLT
 call json_object_new_object@PLT
 mov [rsp+40],rax
 test rax,rax
 jz .Lip_end
 mov rdi,[rsp+32]
 mov esi,[rsp+144]
 call json_object_new_string_len@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lip_data]
 mov rdx,rax
 call .Lip_set
 test eax,eax
 jnz .Lip_end
 mov rdi,[rsp+128]
 call json_object_new_string@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lip_mime]
 mov rdx,rax
 call .Lip_set
 test eax,eax
 jnz .Lip_end
 mov rdi,r13
 mov rsi,[rsp+128]
 call strcmp@PLT
 test eax,eax
 jz .Lip_resized
 mov rdi,r13
 call json_object_new_string@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lip_converted]
 mov rdx,rax
 call .Lip_set
 test eax,eax
 jnz .Lip_end
.Lip_resized:
 mov eax,[rsp+56]
 cmp eax,[rsp+72]
 jne .Lip_resize_json
 mov eax,[rsp+60]
 cmp eax,[rsp+76]
 je .Lip_success
.Lip_resize_json:
 lea rdi,[rsp+48]
 lea rsi,[rip+.Lip_resize_format]
 mov edx,[rsp+56]
 mov ecx,[rsp+60]
 mov r8d,[rsp+72]
 mov r9d,[rsp+76]
 xor eax,eax
 call asprintf@PLT
 test eax,eax
 js .Lip_end
 mov rdi,[rsp+48]
 call json_tokener_parse@PLT
 mov rdi,[rsp+40]
 lea rsi,[rip+.Lip_resize]
 mov rdx,rax
 call .Lip_set
 test eax,eax
 jnz .Lip_end
.Lip_success:
 mov rax,[rsp+40]
 mov [r15],rax
 mov qword ptr [rsp+40],0
 mov dword ptr [rsp+96],0
 jmp .Lip_end
.Lip_invalid:
 mov dword ptr [rsp+96],5
.Lip_end:
 mov rdi,[rsp+40]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call free@PLT
 mov rdi,[rsp+32]
 call free@PLT
 mov rdi,[rsp+16]
 call free@PLT
 mov rdi,[rsp+8]
 call pg_image_codec_close@PLT
 mov rdi,[rsp]
 call pg_image_codec_close@PLT
 mov eax,[rsp+96]
 IP_LEAVE
/* base64Length(size) <= maxBytes with overflow rejection. */
.Lip_fits:
 lea rax,[rdi+2]
 cmp rax,rdi
 jb .Lip_no
 mov r8,rsi
 xor edx,edx
 mov ecx,3
 div rcx
 mov rdx,rax
 shr rdx,62
 jnz .Lip_no
 shl rax,2
 cmp rax,r8
 seta al
 xor al,1
 movzx eax,al
 ret
.Lip_no:
 xor eax,eax
 ret
.section .rodata
.balign 8
.Lip_one: .double 1.0
.Lip_half: .double 0.5
.Lip_png: .asciz "image/png"
.Lip_jpeg: .asciz "image/jpeg"
.Lip_gif: .asciz "image/gif"
.Lip_webp: .asciz "image/webp"
.Lip_data: .asciz "data"
.Lip_mime: .asciz "mimeType"
.Lip_converted: .asciz "convertedFrom"
.Lip_resize: .asciz "resized"
.Lip_resize_format: .asciz "{\"from\":{\"width\":%u,\"height\":%u},\"to\":{\"width\":%u,\"height\":%u}}"
.section .data.rel.ro
.balign 8
.Lip_inline: .quad .Lip_png,.Lip_jpeg,.Lip_gif,.Lip_webp,0
.section .rodata
.Lip_qualities: .long 80,70,55,40
.section .note.GNU-stack,"",@progbits
