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
/* Native port of Pi edit-diff (MIT) and jsdiff 8.0.4 (BSD-3-Clause).
BSD 3-Clause License

Copyright (c) 2009-2015, Kevin Decker <kpdecker@gmail.com>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
.intel_syntax noprefix
/* Pi Durable edit-diff and jsdiff 8.0.4 Myers paths. Nodes are arena indices,
   so realloc never invalidates a diagonal's predecessor. */
#define PATH 0
#define PATHN 8
#define BEFORE 16
#define BEFOREN 24
#define AFTER 32
#define AFTERN 40
#define OUT 48
#define TEXT 56
#define OLD 64
#define NEW 72
#define N 80
#define M 88
#define SUM 96
#define VEC 104
#define CENTER 112
#define NODES 120
#define NCAP 128
#define NCOUNT 136
#define OPS 144
#define SORTED 152
#define SELECT 160
#define DISPLAY 168
#define PATCH 176
#define CAP 184
#define DLEN 192
#define PLEN 200
#define WIDTH 208
#define FIRST 216
#define OLDNUM 224
#define NEWNUM 232
#define ACTION 240
#define COMMON 248
#define OINDEX 256
#define NINDEX 264
#define COUNT 272
#define JSON 280
#define END 288
#define START 296
#define FREE 304
#define FRAME 312
.text
/* rdi source, rsi length, rdx normalized storage, rcx token storage. */
.Led_tokens:
 xor r8d,r8d
 xor r9d,r9d
 cmp rsi,3
 jb .Led_normalize
 cmp byte ptr [rdi],0xef
 jne .Led_normalize
 cmp byte ptr [rdi+1],0xbb
 jne .Led_normalize
 cmp byte ptr [rdi+2],0xbf
 jne .Led_normalize
 mov r8d,3
.Led_normalize:
 cmp r8,rsi
 jae .Led_tokenize
 mov al,[rdi+r8]
 inc r8
 cmp al,13
 jne .Led_copy
 cmp r8,rsi
 jae .Led_copy
 cmp byte ptr [rdi+r8],10
 je .Led_normalize
.Led_copy:
 mov [rdx+r9],al
 inc r9
 jmp .Led_normalize
.Led_tokenize:
 xor eax,eax
 xor r8d,r8d
 xor r10d,r10d
.Led_token_loop:
 cmp r8,r9
 jae .Led_token_tail
 inc r8
 cmp byte ptr [rdx+r8-1],10
 jne .Led_token_loop
.Led_token_store:
 lea r11,[rdx+r10]
 mov [rcx],r11
 mov r11,r8
 sub r11,r10
 mov [rcx+8],r11
 add rcx,16
 inc rax
 mov r10,r8
 cmp r8,r9
 jb .Led_token_loop
 jmp .Led_token_done
.Led_token_tail:
 cmp r10,r8
 jb .Led_token_store
.Led_token_done:
 /* split("\n").length for display number width. */
 mov rdx,rax
 test r9,r9
 jz .Led_empty_lines
 mov rcx,[rcx-16]
 movzx esi,byte ptr [rcx+r11-1]
 /* Only the low byte is inspected; avoid reading past the final token. */
 cmp sil,10
 jne .Led_tokens_return
 inc rdx
 jmp .Led_tokens_return
.Led_empty_lines:
 mov edx,1
.Led_tokens_return:
 ret

/* Each diagonal owns one node reference; each node owns its predecessor. */
.Led_node:
 push r12
 mov r12,[rbx+FREE]
 cmp r12,-1
 je .Led_node_extend
 mov rax,r12
 shl rax,5
 add rax,[rbx+NODES]
 mov rdx,[rax]
 mov [rbx+FREE],rdx
 mov rdx,r12
 pop r12
 ret
.Led_node_extend:
 mov r12,[rbx+NCOUNT]
 cmp r12,[rbx+NCAP]
 jb .Led_node_ready
 mov rsi,[rbx+NCAP]
 add rsi,rsi
 jnz .Led_node_grow
 mov esi,64
.Led_node_grow:
 mov [rbx+NCAP],rsi
 shl rsi,5
 mov rdi,[rbx+NODES]
 call realloc@PLT
 test rax,rax
 jz .Led_node_return
 mov [rbx+NODES],rax
.Led_node_ready:
 mov rax,r12
 shl rax,5
 add rax,[rbx+NODES]
 inc qword ptr [rbx+NCOUNT]
 mov rdx,r12
.Led_node_return:
 pop r12
 ret

.globl pg_edit_diff_create
.type pg_edit_diff_create,@function
pg_edit_diff_create:
 mov rax,[rsp+8]
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,FRAME
 mov rbx,rsp
 mov [rbx+PATH],rdi
 mov [rbx+PATHN],rsi
 mov [rbx+BEFORE],rdx
 mov [rbx+BEFOREN],rcx
 mov [rbx+AFTER],r8
 mov [rbx+AFTERN],r9
 mov [rbx+OUT],rax
 lea rdi,[rbx+TEXT]
 xor eax,eax
 mov ecx,(FRAME-TEXT)/8
 rep stosq
 mov qword ptr [rbx+FREE],-1
 mov ebp,1
 mov rax,[rbx+OUT]
 test rax,rax
 jz .Led_exit
 mov qword ptr [rax],0
 cmp qword ptr [rbx+BEFOREN],4096
 jae .Led_exit
 cmp qword ptr [rbx+AFTERN],4096
 jae .Led_exit
 cmp qword ptr [rbx+PATHN],4096
 jae .Led_exit
 mov edi,1
 mov esi,8192
 call calloc@PLT
 mov [rbx+TEXT],rax
 test rax,rax
 jz .Led_alloc
 mov rdi,[rbx+BEFOREN]
 add rdi,[rbx+AFTERN]
 add rdi,2
 mov esi,16
 call calloc@PLT
 mov [rbx+OLD],rax
 test rax,rax
 jz .Led_alloc
 mov rdx,[rbx+BEFOREN]
 inc rdx
 shl rdx,4
 add rax,rdx
 mov [rbx+NEW],rax
 mov rdi,[rbx+BEFORE]
 mov rsi,[rbx+BEFOREN]
 mov rdx,[rbx+TEXT]
 mov rcx,[rbx+OLD]
 call .Led_tokens
 mov [rbx+N],rax
 mov [rbx+WIDTH],rdx
 mov rdi,[rbx+AFTER]
 mov rsi,[rbx+AFTERN]
 mov rdx,[rbx+TEXT]
 add rdx,4096
 mov rcx,[rbx+NEW]
 call .Led_tokens
 mov [rbx+M],rax
 cmp rdx,[rbx+WIDTH]
 cmovb rdx,[rbx+WIDTH]
 mov ecx,1
.Led_width:
 cmp rdx,10
 jb .Led_width_done
 mov rax,rdx
 xor edx,edx
 mov esi,10
 div rsi
 mov rdx,rax
 inc ecx
 jmp .Led_width
.Led_width_done:
 mov [rbx+WIDTH],rcx
 mov rdi,[rbx+N]
 add rdi,[rbx+M]
 mov [rbx+SUM],rdi
 lea rdi,[rdi+rdi+3]
 mov esi,16
 call calloc@PLT
 mov [rbx+VEC],rax
 test rax,rax
 jz .Led_alloc
 mov rcx,[rbx+SUM]
 inc rcx
 shl rcx,4
 add rcx,rax
 mov [rbx+CENTER],rcx
 mov rcx,[rbx+SUM]
 lea rcx,[rcx+rcx+3]
.Led_vec_init:
 mov qword ptr [rax],-2
 add rax,16
 dec rcx
 jnz .Led_vec_init
 mov rdi,[rbx+SUM]
 inc rdi
 mov esi,32
 call calloc@PLT
 mov [rbx+OPS],rax
 test rax,rax
 jz .Led_alloc
 mov rdx,[rbx+SUM]
 inc rdx
 shl rdx,4
 add rax,rdx
 mov [rbx+SORTED],rax
 mov rdi,[rbx+SUM]
 inc rdi
 mov esi,1
 call calloc@PLT
 mov [rbx+SELECT],rax
 test rax,rax
 jz .Led_alloc
 xor r12d,r12d
 xor r13d,r13d
 mov r14,-1
 mov r15,-1
 mov qword ptr [rbx+ACTION],0
 jmp .Led_common_begin
.Led_distance:
 inc r12
 cmp r12,[rbx+SUM]
 ja .Led_alloc
 mov r13,r12
 neg r13
.Led_diagonal:
 mov rax,r13
 shl rax,4
 add rax,[rbx+CENTER]
 mov r14,[rax-16]
 mov r15,[rax-8]
 mov r10,[rax+16]
 /* Eligibility is checked before selecting the farthest old position. */
 cmp r14,-2
 je .Led_choose_add
 lea rcx,[r14+1]
 cmp rcx,[rbx+N]
 jae .Led_choose_add
 cmp r10,-2
 je .Led_choose_remove
 mov rcx,r10
 sub rcx,r13
 test rcx,rcx
 js .Led_choose_remove
 cmp rcx,[rbx+M]
 jae .Led_choose_remove
 cmp r14,r10
 jge .Led_choose_remove
.Led_choose_add:
 cmp r10,-2
 je .Led_no_path
 mov rcx,r10
 sub rcx,r13
 test rcx,rcx
 js .Led_no_path
 cmp rcx,[rbx+M]
 jae .Led_no_path
 mov r14,r10
 mov r15,[rax+24]
 mov qword ptr [rbx+ACTION],43
 jmp .Led_common_begin
.Led_choose_remove:
 inc r14
 mov qword ptr [rbx+ACTION],45
.Led_common_begin:
 mov qword ptr [rbx+COMMON],0
.Led_common:
 lea rax,[r14+1]
 cmp rax,[rbx+N]
 jae .Led_path
 mov rcx,rax
 sub rcx,r13
 cmp rcx,[rbx+M]
 jae .Led_path
 shl rax,4
 shl rcx,4
 add rax,[rbx+OLD]
 add rcx,[rbx+NEW]
 mov rdx,[rax+8]
 cmp rdx,[rcx+8]
 jne .Led_path
 mov rdi,[rax]
 mov rsi,[rcx]
 call memcmp@PLT
 test eax,eax
 jnz .Led_path
 inc r14
 inc qword ptr [rbx+COMMON]
 jmp .Led_common
.Led_path:
 call .Led_node
 test rax,rax
 jz .Led_alloc
 mov [rax],r15
 mov qword ptr [rax+24],1
 cmp r15,-1
 je .Led_node_linked
 mov rcx,r15
 shl rcx,5
 add rcx,[rbx+NODES]
 inc qword ptr [rcx+24]
.Led_node_linked:
 mov rcx,[rbx+COMMON]
 mov [rax+8],rcx
 mov rcx,[rbx+ACTION]
 mov [rax+16],rcx
 mov r15,rdx
 mov rax,r13
 shl rax,4
 add rax,[rbx+CENTER]
 mov [rax],r14
 mov [rax+8],r15
 call .Led_release_previous_diagonal
 lea rax,[r14+1]
 cmp rax,[rbx+N]
 jb .Led_path_next
 sub rax,r13
 cmp rax,[rbx+M]
 jae .Led_reconstruct
.Led_path_next:
 test r12,r12
 jz .Led_distance
 add r13,2
 cmp r13,r12
 jle .Led_diagonal
 jmp .Led_distance
.Led_no_path:
 mov qword ptr [rax],-2
 call .Led_release_previous_diagonal
 jmp .Led_path_next
.Led_reconstruct:
 mov r12,[rbx+SUM]
 mov r13,[rbx+N]
 mov r14,[rbx+M]
.Led_back_node:
 mov rbp,r15
 shl rbp,5
 add rbp,[rbx+NODES]
 mov rcx,[rbp+8]
.Led_back_common:
 test rcx,rcx
 jz .Led_back_change
 dec r12
 dec r13
 dec r14
 mov rax,r12
 shl rax,4
 add rax,[rbx+OPS]
 mov qword ptr [rax],32
 mov [rax+8],r13
 dec rcx
 jmp .Led_back_common
.Led_back_change:
 mov rcx,[rbp+16]
 test rcx,rcx
 jz .Led_back_previous
 dec r12
 mov rax,r12
 shl rax,4
 add rax,[rbx+OPS]
 mov [rax],rcx
 cmp ecx,43
 jne .Led_back_remove
 dec r14
 mov [rax+8],r14
 jmp .Led_back_previous
.Led_back_remove:
 dec r13
 mov [rax+8],r13
.Led_back_previous:
 mov r15,[rbp]
 cmp r15,-1
 jne .Led_back_node
 mov rax,r12
 shl rax,4
 add [rbx+OPS],rax
 mov rax,[rbx+SUM]
 sub rax,r12
 mov [rbx+COUNT],rax
 /* jsdiff buildValues emits removals before additions in a replacement. */
 xor r12d,r12d
 xor r13d,r13d
.Led_sort:
 cmp r12,[rbx+COUNT]
 jae .Led_select_begin
 mov rax,r12
 shl rax,4
 add rax,[rbx+OPS]
 cmp qword ptr [rax],32
 jne .Led_sort_run
 movdqu xmm0,[rax]
 mov rax,r13
 shl rax,4
 add rax,[rbx+SORTED]
 movdqu [rax],xmm0
 inc r12
 inc r13
 jmp .Led_sort
.Led_sort_run:
 mov r14,r12
.Led_run_end:
 cmp r14,[rbx+COUNT]
 jae .Led_run_sort
 mov rax,r14
 shl rax,4
 add rax,[rbx+OPS]
 cmp qword ptr [rax],32
 je .Led_run_sort
 inc r14
 jmp .Led_run_end
.Led_run_sort:
 mov r15d,45
.Led_run_pass:
 mov rcx,r12
.Led_run_copy:
 cmp rcx,r14
 jae .Led_run_next
 mov rax,rcx
 shl rax,4
 add rax,[rbx+OPS]
 cmp [rax],r15
 jne .Led_run_skip
 movdqu xmm0,[rax]
 mov rax,r13
 shl rax,4
 add rax,[rbx+SORTED]
 movdqu [rax],xmm0
 inc r13
.Led_run_skip:
 inc rcx
 jmp .Led_run_copy
.Led_run_next:
 cmp r15d,43
 je .Led_run_sorted
 mov r15d,43
 jmp .Led_run_pass
.Led_run_sorted:
 mov r12,r14
 jmp .Led_sort
.Led_select_begin:
 xor r12d,r12d
 mov r14,[rbx+SORTED]
 mov r15,[rbx+SELECT]
.Led_select_loop:
 cmp r12,[rbx+COUNT]
 jae .Led_render_allocate
 mov rax,r12
 shl rax,4
 cmp qword ptr [r14+rax],32
 je .Led_select_next
 mov byte ptr [r15+r12],1
 mov ecx,1
.Led_select_near:
 cmp ecx,4
 ja .Led_select_next
 mov rdx,r12
 sub rdx,rcx
 js .Led_select_right
 mov byte ptr [r15+rdx],1
.Led_select_right:
 lea rdx,[r12+rcx]
 cmp rdx,[rbx+COUNT]
 jae .Led_select_step
 mov byte ptr [r15+rdx],1
.Led_select_step:
 inc ecx
 jmp .Led_select_near
.Led_select_next:
 inc r12
 jmp .Led_select_loop
.Led_render_allocate:
 mov rdi,[rbx+SUM]
 shl rdi,5
 add rdi,[rbx+BEFOREN]
 add rdi,[rbx+AFTERN]
 mov rax,[rbx+PATHN]
 lea rdi,[rdi+rax*2+1024]
 mov [rbx+CAP],rdi
 mov esi,2
 call calloc@PLT
 mov [rbx+DISPLAY],rax
 test rax,rax
 jz .Led_alloc
 add rax,[rbx+CAP]
 mov [rbx+PATCH],rax
 mov rdi,rax
 mov rsi,[rbx+CAP]
 lea rdx,[rip+.Led_headers]
 mov ecx,[rbx+PATHN]
 mov r8,[rbx+PATH]
 mov r9d,ecx
 sub rsp,16
 mov rax,[rbx+PATH]
 mov [rsp],rax
 xor eax,eax
 call snprintf@PLT
 add rsp,16
 test eax,eax
 js .Led_alloc
 mov [rbx+PLEN],rax
 xor r12d,r12d
 mov qword ptr [rbx+OLDNUM],1
 mov qword ptr [rbx+NEWNUM],1
 xor r13d,r13d
.Led_display_loop:
 cmp r12,[rbx+COUNT]
 jae .Led_patch_begin
 mov rax,r12
 shl rax,4
 add rax,[rbx+SORTED]
 mov r14,[rax]
 mov r15,[rax+8]
 cmp r14d,32
 je .Led_display_selected
 cmp qword ptr [rbx+FIRST],0
 jne .Led_display_selected
 mov rax,[rbx+NEWNUM]
 mov [rbx+FIRST],rax
.Led_display_selected:
 mov rax,[rbx+SELECT]
 cmp byte ptr [rax+r12],0
 jne .Led_display_line
 test r13d,r13d
 jnz .Led_display_advance
 /* No-change input has no display lines or ellipses. */
 mov rax,[rbx+NCOUNT]
 cmp rax,1
 je .Led_display_advance
 mov rdi,[rbx+DISPLAY]
 add rdi,[rbx+DLEN]
 mov rsi,[rbx+CAP]
 sub rsi,[rbx+DLEN]
 lea rdx,[rip+.Led_ellipsis]
 mov ecx,[rbx+WIDTH]
 lea r8,[rip+.Led_empty]
 xor eax,eax
 call snprintf@PLT
 add [rbx+DLEN],rax
 mov r13d,1
 jmp .Led_display_advance
.Led_display_line:
 xor r13d,r13d
 call .Led_token
 /* rax token address; r15 token length without terminal LF. */
 mov rdi,[rbx+DISPLAY]
 add rdi,[rbx+DLEN]
 mov rsi,[rbx+CAP]
 sub rsi,[rbx+DLEN]
 lea rdx,[rip+.Led_numbered]
 mov ecx,r14d
 mov r8d,[rbx+WIDTH]
 mov r9,[rbx+OLDNUM]
 cmp r14d,43
 cmove r9,[rbx+NEWNUM]
 sub rsp,16
 mov [rsp],r15
 mov [rsp+8],rax
 xor eax,eax
 call snprintf@PLT
 add rsp,16
 add [rbx+DLEN],rax
.Led_display_advance:
 cmp r14d,43
 je .Led_display_new
 inc qword ptr [rbx+OLDNUM]
.Led_display_new:
 cmp r14d,45
 je .Led_display_next
 inc qword ptr [rbx+NEWNUM]
.Led_display_next:
 inc r12
 jmp .Led_display_loop
.Led_patch_begin:
 cmp qword ptr [rbx+DLEN],0
 je .Led_patch_reset
 dec qword ptr [rbx+DLEN]
.Led_patch_reset:
 xor r12d,r12d
 mov qword ptr [rbx+OLDNUM],1
 mov qword ptr [rbx+NEWNUM],1
.Led_patch_loop:
 cmp r12,[rbx+COUNT]
 jae .Led_json
 mov rax,[rbx+SELECT]
 cmp byte ptr [rax+r12],0
 jne .Led_hunk
 mov rax,r12
 shl rax,4
 add rax,[rbx+SORTED]
 mov r14,[rax]
 call .Led_advance
 inc r12
 jmp .Led_patch_loop
.Led_hunk:
 mov [rbx+START],r12
 mov r13,r12
 xor r8d,r8d
 xor r9d,r9d
.Led_hunk_count:
 cmp r13,[rbx+COUNT]
 jae .Led_hunk_header
 mov rax,[rbx+SELECT]
 cmp byte ptr [rax+r13],0
 je .Led_hunk_header
 mov rax,r13
 shl rax,4
 add rax,[rbx+SORTED]
 cmp qword ptr [rax],43
 je .Led_hunk_new
 inc r8
.Led_hunk_new:
 cmp qword ptr [rax],45
 je .Led_hunk_count_next
 inc r9
.Led_hunk_count_next:
 inc r13
 jmp .Led_hunk_count
.Led_hunk_header:
 mov [rbx+END],r13
 mov rcx,[rbx+OLDNUM]
 test r8,r8
 jnz .Led_hunk_old_start
 dec rcx
.Led_hunk_old_start:
 mov rax,[rbx+NEWNUM]
 test r9,r9
 jnz .Led_hunk_new_start
 dec rax
.Led_hunk_new_start:
 mov rdi,[rbx+PATCH]
 add rdi,[rbx+PLEN]
 mov rsi,[rbx+CAP]
 sub rsi,[rbx+PLEN]
 lea rdx,[rip+.Led_hunk_format]
 sub rsp,16
 mov [rsp],r9
 mov r9,rax
 xor eax,eax
 call snprintf@PLT
 add rsp,16
 add [rbx+PLEN],rax
.Led_patch_lines:
 cmp r12,[rbx+END]
 jae .Led_patch_loop
 mov rax,r12
 shl rax,4
 add rax,[rbx+SORTED]
 mov r14,[rax]
 mov r15,[rax+8]
 call .Led_token
 mov r13,rdx
 mov rdi,[rbx+PATCH]
 add rdi,[rbx+PLEN]
 mov rsi,[rbx+CAP]
 sub rsi,[rbx+PLEN]
 lea rdx,[rip+.Led_patch_line]
 mov ecx,r14d
 mov r8d,r15d
 mov r9,rax
 xor eax,eax
 call snprintf@PLT
 add [rbx+PLEN],rax
 test r13,r13
 jnz .Led_patch_line_done
 mov rdi,[rbx+PATCH]
 add rdi,[rbx+PLEN]
 lea rsi,[rip+.Led_no_eof]
 mov edx,28
 call memcpy@PLT
 add qword ptr [rbx+PLEN],28
.Led_patch_line_done:
 call .Led_advance
 inc r12
 jmp .Led_patch_lines
.Led_json:
 call json_object_new_object@PLT
 mov [rbx+JSON],rax
 test rax,rax
 jz .Led_alloc
 mov rdi,[rbx+DISPLAY]
 mov esi,[rbx+DLEN]
 call json_object_new_string_len@PLT
 lea rsi,[rip+.Led_diff_key]
 call .Led_json_add
 test eax,eax
 jnz .Led_alloc
 mov rdi,[rbx+PATCH]
 mov esi,[rbx+PLEN]
 call json_object_new_string_len@PLT
 lea rsi,[rip+.Led_patch_key]
 call .Led_json_add
 test eax,eax
 jnz .Led_alloc
 cmp qword ptr [rbx+FIRST],0
 je .Led_success
 mov rdi,[rbx+FIRST]
 call json_object_new_uint64@PLT
 lea rsi,[rip+.Led_first_key]
 call .Led_json_add
 test eax,eax
 jnz .Led_alloc
.Led_success:
 mov rax,[rbx+OUT]
 mov rdx,[rbx+JSON]
 mov [rax],rdx
 mov qword ptr [rbx+JSON],0
 xor ebp,ebp
 jmp .Led_exit
.Led_alloc:
 mov ebp,5
.Led_exit:
 mov rdi,[rbx+JSON]
 call json_object_put@PLT
 mov rdi,[rbx+DISPLAY]
 call free@PLT
 mov rdi,[rbx+SELECT]
 call free@PLT
 mov rdi,[rbx+OPS]
 /* OPS points into its allocation after reconstruction. */
 test rdi,rdi
 jz .Led_free_nodes
 cmp qword ptr [rbx+SORTED],0
 je .Led_free_ops
 mov rdi,[rbx+SORTED]
 mov rax,[rbx+SUM]
 inc rax
 shl rax,4
 sub rdi,rax
.Led_free_ops:
 call free@PLT
.Led_free_nodes:
 mov rdi,[rbx+NODES]
 call free@PLT
 mov rdi,[rbx+VEC]
 call free@PLT
 mov rdi,[rbx+OLD]
 call free@PLT
 mov rdi,[rbx+TEXT]
 call free@PLT
 mov eax,ebp
 add rsp,FRAME
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_edit_diff_create,.-pg_edit_diff_create

.Led_release_previous_diagonal:
 lea rcx,[r13-1]
 shl rcx,4
 add rcx,[rbx+CENTER]
 cmp qword ptr [rcx],-2
 je .Led_release_done
 mov rdi,[rcx+8]
 mov qword ptr [rcx],-2
.Led_release_node:
 cmp rdi,-1
 je .Led_release_done
 mov rax,rdi
 shl rax,5
 add rax,[rbx+NODES]
 dec qword ptr [rax+24]
 jnz .Led_release_done
 mov rdx,[rax]
 mov rcx,[rbx+FREE]
 mov [rax],rcx
 mov [rbx+FREE],rdi
 mov rdi,rdx
 jmp .Led_release_node
.Led_release_done:
 ret

.Led_advance:
 cmp r14d,43
 je .Led_advance_new
 inc qword ptr [rbx+OLDNUM]
.Led_advance_new:
 cmp r14d,45
 je .Led_advance_return
 inc qword ptr [rbx+NEWNUM]
.Led_advance_return:
 ret
/* Return token pointer in rax, content length in r15, trailing-LF flag rdx. */
.Led_token:
 mov rax,r15
 shl rax,4
 mov rcx,[rbx+OLD]
 cmp r14d,43
 cmove rcx,[rbx+NEW]
 add rax,rcx
 mov r15,[rax+8]
 mov rax,[rax]
 xor edx,edx
 cmp byte ptr [rax+r15-1],10
 jne .Led_token_return
 dec r15
 mov edx,1
.Led_token_return:
 ret
.Led_json_add:
 test rax,rax
 jz .Led_json_bad
 push r12
 mov r12,rax
 mov rdi,[rbx+JSON]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Led_json_return
 mov rdi,r12
 call json_object_put@PLT
 mov eax,5
.Led_json_return:
 pop r12
 ret
.Led_json_bad:
 mov eax,5
 ret
.section .rodata
.Led_empty: .asciz ""
.Led_headers: .asciz "--- %.*s\n+++ %.*s\n"
.Led_ellipsis: .asciz " %*s ...\n"
.Led_numbered: .asciz "%c%*zu %.*s\n"
.Led_patch_line: .asciz "%c%.*s\n"
.Led_hunk_format: .asciz "@@ -%zu,%zu +%zu,%zu @@\n"
.Led_no_eof: .ascii "\\ No newline at end of file\n"
.Led_diff_key: .asciz "diff"
.Led_patch_key: .asciz "patch"
.Led_first_key: .asciz "firstChangedLine"
.section .note.GNU-stack,"",@progbits
