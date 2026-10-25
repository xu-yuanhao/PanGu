#include "stream_decode_layout.h"
.intel_syntax noprefix
.text
.globl pg_stream_decoder_init
.type pg_stream_decoder_init,@function
pg_stream_decoder_init:
 test rdi,rdi
 jz .Linit_ret
 xor eax,eax
 mov ecx,7
 rep stosd
 mov dword ptr [rdi-PG_D_SIZE+PG_D_LOWER],128
 mov dword ptr [rdi-PG_D_SIZE+PG_D_UPPER],191
.Linit_ret:
 ret
.size pg_stream_decoder_init,.-pg_stream_decoder_init

.globl pg_stream_decode
.type pg_stream_decode,@function
pg_stream_decode:
 test rdi,rdi
 jz .Lbad
 test r8,r8
 jz .Lbad
 mov r10,[rsp+8]
 test r10,r10
 jz .Lbad
 mov qword ptr [r10],0
 cmp rdx,4096
 ja .Lbad
 cmp ecx,1
 ja .Lbad
 test rdx,rdx
 jz .Linput_valid
 test rsi,rsi
 jz .Lbad
.Linput_valid:
 lea rax,[rdx+rdx*2+3]
 cmp r9,rax
 jb .Lbad
 cmp dword ptr [rdi+PG_D_FINISHED],0
 je .Lfresh
 test rdx,rdx
 jnz .Lbad
 test ecx,ecx
 jz .Lbad
 xor eax,eax
 ret
.Lfresh:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,16
 mov rbx,rdi
 mov r12,rsi
 lea r13,[rsi+rdx]
 mov r14,r8
 mov r15,r8
 mov [rsp],r10
 mov [rsp+8],ecx
.Lbyte:
 cmp r12,r13
 jae .Lend
 movzx eax,byte ptr [r12]
 mov ecx,[rbx+PG_D_COUNT]
 test ecx,ecx
 jnz .Lcontinuation
 inc r12
 cmp eax,128
 jb .Lascii
 cmp eax,194
 jb .Lreplace
 cmp eax,244
 ja .Lreplace
 mov [rbx],al
 mov dword ptr [rbx+PG_D_COUNT],1
 mov dword ptr [rbx+PG_D_LOWER],128
 mov dword ptr [rbx+PG_D_UPPER],191
 cmp eax,224
 jae .Lthree
 mov dword ptr [rbx+PG_D_NEEDED],2
 jmp .Lbyte
.Lthree:
 cmp eax,240
 jae .Lfour
 mov dword ptr [rbx+PG_D_NEEDED],3
 cmp eax,224
 jne .Lnot_e0
 mov dword ptr [rbx+PG_D_LOWER],160
.Lnot_e0:
 cmp eax,237
 jne .Lbyte
 mov dword ptr [rbx+PG_D_UPPER],159
 jmp .Lbyte
.Lfour:
 mov dword ptr [rbx+PG_D_NEEDED],4
 cmp eax,240
 jne .Lnot_f0
 mov dword ptr [rbx+PG_D_LOWER],144
.Lnot_f0:
 cmp eax,244
 jne .Lbyte
 mov dword ptr [rbx+PG_D_UPPER],143
 jmp .Lbyte
.Lcontinuation:
 cmp eax,[rbx+PG_D_LOWER]
 jb .Lprefix_error
 cmp eax,[rbx+PG_D_UPPER]
 ja .Lprefix_error
 mov [rbx+rcx],al
 inc r12
 inc ecx
 mov [rbx+PG_D_COUNT],ecx
 mov dword ptr [rbx+PG_D_LOWER],128
 mov dword ptr [rbx+PG_D_UPPER],191
 cmp ecx,[rbx+PG_D_NEEDED]
 jb .Lbyte
 mov dword ptr [rbx+PG_D_COUNT],0
 cmp dword ptr [rbx+PG_D_STARTED],0
 jne .Lemit_sequence
 mov dword ptr [rbx+PG_D_STARTED],1
 cmp ecx,3
 jne .Lemit_sequence
 cmp word ptr [rbx],0xbbef
 jne .Lemit_sequence
 cmp byte ptr [rbx+2],0xbf
 je .Lbyte
.Lemit_sequence:
 xor edx,edx
.Lcopy:
 mov al,[rbx+rdx]
 mov [r14],al
 inc r14
 inc edx
 cmp edx,ecx
 jb .Lcopy
 jmp .Lbyte
.Lascii:
 mov dword ptr [rbx+PG_D_STARTED],1
 mov [r14],al
 inc r14
 jmp .Lbyte
.Lprefix_error:
 mov dword ptr [rbx+PG_D_COUNT],0
.Lreplace:
 mov dword ptr [rbx+PG_D_STARTED],1
 mov word ptr [r14],0xbfef
 mov byte ptr [r14+2],0xbd
 add r14,3
 jmp .Lbyte
.Lend:
 cmp dword ptr [rsp+8],0
 je .Lreturn
 mov dword ptr [rbx+PG_D_FINISHED],1
 cmp dword ptr [rbx+PG_D_COUNT],0
 je .Lreturn
 mov dword ptr [rbx+PG_D_COUNT],0
 mov dword ptr [rbx+PG_D_STARTED],1
 mov word ptr [r14],0xbfef
 mov byte ptr [r14+2],0xbd
 add r14,3
.Lreturn:
 sub r14,r15
 mov rax,[rsp]
 mov [rax],r14
 xor eax,eax
 add rsp,16
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.Lbad:
 mov eax,1
 ret
.size pg_stream_decode,.-pg_stream_decode
.section .note.GNU-stack,"",@progbits
