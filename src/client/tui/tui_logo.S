/* PanGu V2 geometric rasterizer. Canonical geometry: assets/branding/ SVG
 * and geometry.json. Analytic convex half-planes replace Codex's curved-path
 * SDF grid: no image, field cache, heap, noise, or static frame strings.
 * Four deterministic area samples per Braille dot; >=50% turns a dot on.
 * x/y/translation use 16.16 SVG units, products and mapping use 64-bit.
 */
.intel_syntax noprefix
.section .rodata
.Ldot_bits: .byte 1,8,2,16,4,32,64,128
.section .text
.globl tui_logo_contains
.type tui_logo_contains,@function
tui_logo_contains:
 cmp edx, 65536
 jbe .Llc_phase
 mov edx, 65536
.Llc_phase:
 mov ecx, 65536
 sub ecx, edx
 /* Closed top moves +normal*4.5, bottom -normal*4.5. */
 imul r8, rcx, 91133
 sar r8, 16
 imul r9, rcx, 280478
 sar r9, 16
 movsxd rdi, edi
 movsxd rsi, esi
 mov r10, rdi
 mov r11, rsi
 sub rdi, r8
 sub rsi, r9
 call .Llc_top
 test eax, eax
 jnz .Llc_done
 mov rdi, r10
 mov rsi, r11
 add rdi, r8
 add rsi, r9
 cmp rdi, 524288
 jl .Llc_no
 cmp rdi, 3670016
 jg .Llc_no
 cmp rsi, 524288
 jl .Llc_no
 cmp rsi, 3670016
 jg .Llc_no
 imul rcx, rdi, 21294
 sar rcx, 16
 add rcx, rsi
 cmp rcx, 3088647 /* lower intercept 47.129... */
 setge al
 movzx eax, al
 ret
.Llc_top:
 xor eax, eax
 cmp rdi, 524288
 jl .Llc_done
 cmp rdi, 3670016
 jg .Llc_done
 cmp rsi, 524288
 jl .Llc_done
 cmp rsi, 3670016
 jg .Llc_done
 imul rcx, rdi, 21294
 sar rcx, 16
 add rcx, rsi
 cmp rcx, 2468469 /* upper intercept 37.665... */
 setle al
 movzx eax, al
.Llc_done:
 ret
.Llc_no:
 xor eax, eax
 ret
.size tui_logo_contains,.-tui_logo_contains

/* Frame local slots: stepX/Y 0/8,startX/Y16/24,phase32,index40,
 * col48,row56,dot64,sample72,dotCoverage80,mask88,cellCoverage96,
 * sample x104/y112. Bounded 40x20 caller storage, constant 136-byte frame. */
.globl tui_logo_raster
.type tui_logo_raster,@function
tui_logo_raster:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 128
 mov r12, rdi
 mov r13d, esi
 mov r14d, edx
 mov ebx, ecx
 mov [rsp+32], r8d
 test rdi, rdi
 jz .Llr_fail
 test esi, esi
 jz .Llr_fail
 test edx, edx
 jz .Llr_fail
 cmp esi, 40
 ja .Llr_fail
 cmp edx, 20
 ja .Llr_fail
 cmp ecx, 65536
 jb .Llr_fail
 cmp ecx, 262144
 ja .Llr_fail
 mov eax, esi
 imul rax, rdx
 lea rcx, [rax+rax]
 cmp r9, rcx
 jb .Llr_fail
 /* Fit a 64x64 coordinate square in the actual physical region. */
 mov eax, r14d
 imul rax, rbx
 mov ecx, r13d
 shl rcx, 16
 cmp rax, rcx
 jb .Llr_height_fit
 mov eax, 4194304
 lea rcx, [r13+r13]
 xor edx, edx
 div rcx
 mov [rsp], rax
 imul rax, rbx
 shr rax, 17
 mov [rsp+8], rax
 jmp .Llr_origin
.Llr_height_fit:
 mov eax, 4194304
 lea rcx, [r14*4]
 xor edx, edx
 div rcx
 mov [rsp+8], rax
 shl rax, 17
 xor edx, edx
 div rbx
 mov [rsp], rax
.Llr_origin:
 mov rax, [rsp]
 imul rax, r13
 sub rax, 2097152
 neg rax
 mov [rsp+16], rax
 mov rax, [rsp+8]
 imul rax, r14
 shl rax, 1
 sub rax, 2097152
 neg rax
 mov [rsp+24], rax
 mov qword ptr [rsp+40], 0
 mov qword ptr [rsp+56], 0
.Llr_row:
 mov qword ptr [rsp+48], 0
.Llr_col:
 mov qword ptr [rsp+88], 0
 mov qword ptr [rsp+96], 0
 mov qword ptr [rsp+64], 0
.Llr_dot:
 mov qword ptr [rsp+80], 0
 mov qword ptr [rsp+72], 0
.Llr_sample:
 mov rax, [rsp+48]
 shl rax, 1
 mov rcx, [rsp+64]
 and ecx, 1
 add rax, rcx
 imul rax, [rsp]
 add rax, [rsp+16]
 mov rcx, [rsp+72]
 and ecx, 1
 lea rcx, [rcx*2+1]
 imul rcx, [rsp]
 sar rcx, 2
 add rax, rcx
 mov edi, eax
 mov rax, [rsp+56]
 shl rax, 2
 mov rcx, [rsp+64]
 shr rcx, 1
 add rax, rcx
 imul rax, [rsp+8]
 add rax, [rsp+24]
 mov rcx, [rsp+72]
 shr rcx, 1
 lea rcx, [rcx*2+1]
 imul rcx, [rsp+8]
 sar rcx, 2
 add rax, rcx
 mov esi, eax
 mov edx, [rsp+32]
 call tui_logo_contains
 add [rsp+80], rax
 inc qword ptr [rsp+72]
 cmp qword ptr [rsp+72], 4
 jb .Llr_sample
 cmp qword ptr [rsp+80], 2
 jb .Llr_dot_next
 mov rcx, [rsp+64]
 lea rdx, [rip+.Ldot_bits]
 movzx eax, byte ptr [rdx+rcx]
 or [rsp+88], rax
 mov rax, [rsp+80]
 add [rsp+96], rax
.Llr_dot_next:
 inc qword ptr [rsp+64]
 cmp qword ptr [rsp+64], 8
 jb .Llr_dot
 mov rcx, [rsp+40]
 mov rax, [rsp+88]
 mov [r12+rcx*2], al
 /* Coverage of enabled dots only; solid cells have 255, edge cells dim. */
 mov edx, eax
 xor eax, eax
.Llr_pop:
 test edx, edx
 jz .Llr_pop_done
 lea ecx, [rdx-1]
 and edx, ecx
 inc eax
 jmp .Llr_pop
.Llr_pop_done:
 test eax, eax
 jz .Llr_store_shade
 shl eax, 2
 mov ecx, eax
 mov rax, [rsp+96]
 imul eax, 255
 xor edx, edx
 div ecx
.Llr_store_shade:
 mov rcx, [rsp+40]
 mov [r12+rcx*2+1], al
 inc qword ptr [rsp+40]
 inc qword ptr [rsp+48]
 cmp [rsp+48], r13
 jb .Llr_col
 inc qword ptr [rsp+56]
 cmp [rsp+56], r14
 jb .Llr_row
 mov rax, [rsp+40]
 jmp .Llr_out
.Llr_fail:
 xor eax, eax
.Llr_out:
 add rsp, 128
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size tui_logo_raster,.-tui_logo_raster
.section .note.GNU-stack,"",@progbits
