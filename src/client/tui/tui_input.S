/* tui_input.S — UTF-8 多行编辑、分片按键解析与安全括号粘贴，手写汇编。
 * 不执行终端 I/O；未知转义整序列安全消费（CAN/SUB/Ctrl-C 可中止），
 * 不吞掉其后的正常文本。粘贴期间不产生提交/取消/退出/光标动作；
 * CRLF/CR 折叠为 LF，控制字节过滤，溢出仍消费到结束标记。
 * SysV AMD64：in 指针在 rbx，当前字节在 r12b。
 */
#include "tui_layout.h"
.intel_syntax noprefix
.section .text

/* Prefix replacement for native local commands, preserving text after cursor.
 * Current consumers: /hotkeys and /quit. No Runtime operation is advertised here.
 * The command table grows only when its product handler exists. */
.section .rodata
.Lcomplete_help: .ascii "/hotkeys"
.Lcomplete_quit: .ascii "/quit"
.section .text
.type li_complete_command,@function
li_complete_command:
 push rbx
 push r12
 push r13
 push r14
 sub rsp, 8
 mov rbx, rdi
 cmp dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 jne .Lcc_none
 mov r12, [rbx + TUI_LI_INPUT_CURSOR]
 test r12, r12
 jz .Lcc_none
 cmp byte ptr [rbx], '/'
 jne .Lcc_none
 cmp r12, 1
 jne .Lcc_prefix
 cmp dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 2
 je .Lcc_selected
 jmp .Lcc_list
.Lcc_selected:
 lea r13, [rip + .Lcomplete_help]
 mov r14d, 8
 cmp dword ptr [rbx + TUI_LI_INPUT_COMPLETION_SEL], 0
 je .Lcc_replace
 lea r13, [rip + .Lcomplete_quit]
 mov r14d, 5
 jmp .Lcc_replace
.Lcc_prefix:
 lea r13, [rip + .Lcomplete_help]
 mov r14d, 8
 cmp byte ptr [rbx + 1], 'h'
 je .Lcc_match
 lea r13, [rip + .Lcomplete_quit]
 mov r14d, 5
 cmp byte ptr [rbx + 1], 'q'
 jne .Lcc_none
.Lcc_match:
 cmp r12, r14
 jae .Lcc_none
 mov rdi, rbx
 mov rsi, r13
 mov rdx, r12
 call memcmp@PLT
 test eax, eax
 jne .Lcc_none
.Lcc_replace:
 mov rax, [rbx + TUI_LI_INPUT_LEN]
 sub rax, r12
 add rax, r14
 cmp rax, TUI_LI_INPUT_MAX_BYTES - 1
 ja .Lcc_overflow
 mov [rsp], rax
 lea rdi, [rbx + r14]
 lea rsi, [rbx + r12]
 mov rdx, [rbx + TUI_LI_INPUT_LEN]
 sub rdx, r12
 call memmove@PLT
 mov rdi, rbx
 mov rsi, r13
 mov rdx, r14
 call memcpy@PLT
 mov rax, [rsp]
 mov [rbx + TUI_LI_INPUT_LEN], rax
 mov byte ptr [rbx + rax], 0
 mov [rbx + TUI_LI_INPUT_CURSOR], r14
 mov dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 0
 mov dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX], -1
 mov dword ptr [rbx + TUI_LI_INPUT_PREF_VALID], 0
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 mov eax, TUI_LI_KEY_CHAR
 jmp .Lcc_done
.Lcc_list:
 mov dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 1
 mov dword ptr [rbx + TUI_LI_INPUT_COMPLETION_SEL], 0
 mov eax, TUI_LI_KEY_COMPLETIONS
 jmp .Lcc_done
.Lcc_overflow:
 call li_overflow
 jmp .Lcc_done
.Lcc_none:
 mov eax, TUI_LI_KEY_IGNORED
.Lcc_done:
 add rsp, 8
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_complete_command,.-li_complete_command

/* Full Unicode lowercase plus substring, via a platform-owned library bridge.
 * Bounded 4x byte workspaces cover the editor's 1023-byte source capacity.
 * Returns 0 no match, 1 match, -1 conversion/library/allocation failure. */
.type li_case_contains,@function
li_case_contains:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 8208
 mov rbx, rdi
 mov r12, rsi
 mov r13, rdx
 mov r14, rcx
 mov rdx, rsp
 mov ecx, 4096
 call tui_unicode_lower@PLT
 cmp rax, -1
 je .Lcase_failure
 mov [rsp + 8192], rax
 mov rdi, r13
 mov rsi, r14
 lea rdx, [rsp + 4096]
 mov ecx, 4096
 call tui_unicode_lower@PLT
 cmp rax, -1
 je .Lcase_failure
 mov rdi, rsp
 mov rsi, [rsp + 8192]
 lea rdx, [rsp + 4096]
 mov rcx, rax
 call memmem@PLT
 test rax, rax
 setnz al
 movzx eax, al
 jmp .Lcase_done
.Lcase_failure:
 mov rax, -1
.Lcase_done:
 add rsp, 8208
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_case_contains,.-li_case_contains

/* Reverse substring recall: buf is an independently edited query while active;
 * history_index points to a borrowed preview in owned history storage. The saved
 * draft remains separate until accept/cancel. esi=+1 older, -1 newer. */
.type li_history_search,@function
li_history_search:
 push rbx
 push r12
 push r13
 push r14
 sub rsp, 24
 mov r14d, esi
 mov rbx, rdi
 mov dword ptr [rbx + TUI_LI_INPUT_SEARCH_FAILED], 0
 cmp dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 jne .Lhs_incomplete
 cmp dword ptr [rbx + TUI_LI_INPUT_SEARCH_ACTIVE], 0
 jne .Lhs_scan_start
 mov rdx, [rbx + TUI_LI_INPUT_LEN]
 mov [rbx + TUI_LI_INPUT_DRAFT_LEN], rdx
 mov rax, [rbx + TUI_LI_INPUT_CURSOR]
 mov [rbx + TUI_LI_INPUT_DRAFT_CURSOR], rax
 lea rdi, [rbx + TUI_LI_INPUT_DRAFT]
 mov rsi, rbx
 call memcpy@PLT
 mov dword ptr [rbx + TUI_LI_INPUT_SEARCH_ACTIVE], 1
 mov dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 0
 mov dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX], -1
 mov qword ptr [rbx + TUI_LI_INPUT_LEN], 0
 mov qword ptr [rbx + TUI_LI_INPUT_CURSOR], 0
 jmp .Lhs_none
.Lhs_scan_start:
 cmp qword ptr [rbx + TUI_LI_INPUT_LEN], 0
 je .Lhs_none
 mov r12d, [rbx + TUI_LI_INPUT_HISTORY_INDEX]
 add r12d, r14d
.Lhs_scan:
 test r12d, r12d
 js .Lhs_none
 cmp r12d, [rbx + TUI_LI_INPUT_HISTORY_COUNT]
 jae .Lhs_none
 mov rax, r12
 shl rax, 10
 lea r13, [rbx + rax + TUI_LI_INPUT_HISTORY]
 mov rdi, r13
 mov esi, [rbx + r12*4 + TUI_LI_INPUT_HISTORY_LENS]
 mov rdx, rbx
 mov rcx, [rbx + TUI_LI_INPUT_LEN]
 call li_case_contains
 cmp rax, -1
 je .Lhs_failed
 test rax, rax
 jnz .Lhs_unique
.Lhs_advance:
 add r12d, r14d
 jmp .Lhs_scan
/* Native bounded history has <=100 entries. Keep the newest occurrence of
 * each full text, matching the reference seen-text search result semantics. */
.Lhs_unique:
 mov qword ptr [rsp], 0
 mov eax, [rbx + r12*4 + TUI_LI_INPUT_HISTORY_LENS]
 mov [rsp + 8], eax
.Lhs_unique_scan:
 mov rcx, [rsp]
 cmp rcx, r12
 jae .Lhs_found
 mov edx, [rbx + rcx*4 + TUI_LI_INPUT_HISTORY_LENS]
 cmp edx, [rsp + 8]
 jne .Lhs_unique_next
 shl rcx, 10
 lea rdi, [rbx + rcx + TUI_LI_INPUT_HISTORY]
 mov rsi, r13
 call memcmp@PLT
 test eax, eax
 jz .Lhs_advance
.Lhs_unique_next:
 inc qword ptr [rsp]
 jmp .Lhs_unique_scan
.Lhs_found:
 mov [rbx + TUI_LI_INPUT_HISTORY_INDEX], r12d
 mov eax, TUI_LI_KEY_SEARCH
 jmp .Lhs_done
.Lhs_none:
 mov eax, TUI_LI_KEY_SEARCH
 jmp .Lhs_done
.Lhs_failed:
 mov dword ptr [rbx + TUI_LI_INPUT_SEARCH_FAILED], 1
 jmp .Lhs_none
.Lhs_incomplete:
 mov eax, TUI_LI_KEY_IGNORED
.Lhs_done:
 add rsp, 24
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_history_search,.-li_history_search

/* String.trim whitespace, exact UTF-8 bytes; never reads beyond rsi.
 * Excludes U+0085/U+180E/U+200B as JavaScript does. Returns byte count or zero. */
.type li_trim_space,@function
li_trim_space:
 xor eax, eax
 test rsi, rsi
 jz .Lts_ret
 movzx ecx, byte ptr [rdi]
 cmp ecx, 32
 je .Lts_one
 sub ecx, 9
 cmp ecx, 4
 jbe .Lts_one
 cmp rsi, 2
 jb .Lts_ret
 cmp word ptr [rdi], 0xa0c2 /* U+00A0 */
 je .Lts_two
 cmp rsi, 3
 jb .Lts_ret
 movzx ecx, word ptr [rdi]
 movzx edx, byte ptr [rdi + 2]
 cmp ecx, 0x9ae1 /* U+1680 */
 jne .Lts_2000
 cmp edx, 0x80
 je .Lts_three
.Lts_2000:
 cmp ecx, 0x80e2
 jne .Lts_205f
 cmp edx, 0x80
 jb .Lts_ret
 cmp edx, 0x8a /* U+2000..U+200A */
 jbe .Lts_three
 cmp edx, 0xa8 /* U+2028/29 */
 je .Lts_three
 cmp edx, 0xa9
 je .Lts_three
 cmp edx, 0xaf /* U+202F */
 je .Lts_three
 ret
.Lts_205f:
 cmp ecx, 0x81e2
 jne .Lts_3000
 cmp edx, 0x9f
 je .Lts_three
 ret
.Lts_3000:
 cmp ecx, 0x80e3
 jne .Lts_bom
 cmp edx, 0x80
 je .Lts_three
 ret
.Lts_bom:
 cmp ecx, 0xbbef
 jne .Lts_ret
 cmp edx, 0xbf
 je .Lts_three
.Lts_ret:
 ret
.Lts_one:
 mov eax, 1
 ret
.Lts_two:
 mov eax, 2
 ret
.Lts_three:
 mov eax, 3
 ret
.size li_trim_space,.-li_trim_space

/* Pi editor.addToHistory: newest first, 100 entries, consecutive dedup.
 * ECMAScript WhiteSpace/LineTerminator edge characters are trimmed; all stored bytes are owned by this input.
 * Caller invokes this only after submit acceptance, before resetting the draft. */
.globl tui_input_remember
.type tui_input_remember,@function
tui_input_remember:
 mov rsi, rdi
 mov rdx, [rdi + TUI_LI_INPUT_LEN]
 jmp tui_input_history_add
.size tui_input_remember,.-tui_input_remember

/* Borrowed startup prompt; reject unsafe/oversize bytes rather than truncating. */
.globl tui_input_history_add
.type tui_input_history_add,@function
tui_input_history_add:
 cmp rdx, TUI_LI_INPUT_MAX_BYTES - 1
 ja .Lha_reject
 test rdx, rdx
 jz .Lha_reject
 test rsi, rsi
 jz .Lha_reject
 xor ecx, ecx
.Lha_scan:
 cmp rcx, rdx
 jae .Lha_valid
 movzx eax, byte ptr [rsi + rcx]
 inc rcx
 cmp eax, 10
 je .Lha_scan
 cmp eax, 32
 jb .Lha_reject
 cmp eax, 127
 je .Lha_reject
 cmp eax, 128
 jb .Lha_scan
 cmp eax, 194
 jb .Lha_reject
 cmp eax, 244
 ja .Lha_reject
 mov r8d, 1
 cmp eax, 224
 jb .Lha_lead
 mov r8d, 2
 cmp eax, 240
 jb .Lha_lead
 mov r8d, 3
.Lha_lead:
 lea r9, [rcx + r8]
 cmp r9, rdx
 ja .Lha_reject
 movzx r10d, byte ptr [rsi + rcx]
 cmp eax, 194
 jne .Lha_min3
 cmp r10d, 160 /* encoded C1 controls */
 jb .Lha_reject
.Lha_min3:
 cmp eax, 224
 jne .Lha_surrogate
 cmp r10d, 160
 jb .Lha_reject
.Lha_surrogate:
 cmp eax, 237
 jne .Lha_min4
 cmp r10d, 159
 ja .Lha_reject
.Lha_min4:
 cmp eax, 240
 jne .Lha_max4
 cmp r10d, 144
 jb .Lha_reject
.Lha_max4:
 cmp eax, 244
 jne .Lha_cont
 cmp r10d, 143
 ja .Lha_reject
.Lha_cont:
 movzx eax, byte ptr [rsi + rcx]
 sub eax, 128
 cmp eax, 63
 ja .Lha_reject
 inc rcx
 dec r8d
 jnz .Lha_cont
 jmp .Lha_scan
.Lha_reject:
 ret
.Lha_valid:
 push rbx
 push r12
 push r13
 sub rsp, 16
 mov rbx, rdi
 mov r13, rdx
 mov r12, rsi
.Lhr_left:
 test r13, r13
 jz .Lhr_done
 mov rdi, r12
 mov rsi, r13
 call li_trim_space
 test eax, eax
 jz .Lhr_right
 add r12, rax
 sub r13, rax
 jmp .Lhr_left
.Lhr_right:
 mov rdi, r12
 mov rsi, r13
 call tui_utf8_last_seq_start@PLT
 mov [rsp], rax
 lea rdi, [r12 + rax]
 mov rsi, r13
 sub rsi, rax
 call li_trim_space
 test eax, eax
 jz .Lhr_dedup
 mov r13, [rsp]
 test r13, r13
 jnz .Lhr_right
 jmp .Lhr_done
.Lhr_dedup:
 cmp dword ptr [rbx + TUI_LI_INPUT_HISTORY_COUNT], 0
 je .Lhr_shift
 cmp r13d, dword ptr [rbx + TUI_LI_INPUT_HISTORY_LENS]
 jne .Lhr_shift
 lea rdi, [rbx + TUI_LI_INPUT_HISTORY]
 mov rsi, r12
 mov rdx, r13
 call memcmp@PLT
 test eax, eax
 jz .Lhr_done
.Lhr_shift:
 mov eax, dword ptr [rbx + TUI_LI_INPUT_HISTORY_COUNT]
 cmp eax, 100
 jb .Lhr_count
 mov eax, 99
.Lhr_count:
 lea edx, [eax + 1]
 mov dword ptr [rbx + TUI_LI_INPUT_HISTORY_COUNT], edx
 mov edx, eax
 shl rdx, 10
 lea rdi, [rbx + TUI_LI_INPUT_HISTORY + 1024]
 lea rsi, [rbx + TUI_LI_INPUT_HISTORY]
 call memmove@PLT
 mov edx, dword ptr [rbx + TUI_LI_INPUT_HISTORY_COUNT]
 dec edx
 shl edx, 2
 lea rdi, [rbx + TUI_LI_INPUT_HISTORY_LENS + 4]
 lea rsi, [rbx + TUI_LI_INPUT_HISTORY_LENS]
 call memmove@PLT
 lea rdi, [rbx + TUI_LI_INPUT_HISTORY]
 mov rsi, r12
 mov rdx, r13
 call memcpy@PLT
 mov dword ptr [rbx + TUI_LI_INPUT_HISTORY_LENS], r13d
.Lhr_done:
 add rsp, 16
 pop r13
 pop r12
 pop rbx
 ret
.size tui_input_history_add,.-tui_input_history_add

/* Dedicated navigation implementation, also used by first/last-line arrows.
 * direction -1 older, +1 newer. Captures exact draft bytes and byte cursor. */
.type li_history_nav,@function
li_history_nav:
 push rbx
 push r12
 push r13
 sub rsp, 16
 mov dword ptr [rsp], esi
 mov rbx, rdi
 cmp dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 jne .Lhn_none
 mov r12d, dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX]
 sub r12d, esi
 cmp r12d, -1
 jl .Lhn_none
 cmp r12d, dword ptr [rbx + TUI_LI_INPUT_HISTORY_COUNT]
 jge .Lhn_none
 cmp dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX], -1
 jne .Lhn_load
 mov rdx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 mov qword ptr [rbx + TUI_LI_INPUT_DRAFT_LEN], rdx
 mov rax, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 mov qword ptr [rbx + TUI_LI_INPUT_DRAFT_CURSOR], rax
 lea rdi, [rbx + TUI_LI_INPUT_DRAFT]
 mov rsi, rbx
 call memcpy@PLT
.Lhn_load:
 mov dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX], r12d
 cmp r12d, -1
 je .Lhn_draft
 mov r13d, dword ptr [rbx + r12*4 + TUI_LI_INPUT_HISTORY_LENS]
 shl r12, 10
 lea rsi, [rbx + r12 + TUI_LI_INPUT_HISTORY]
 mov qword ptr [rbx + TUI_LI_INPUT_CURSOR], 0
 cmp dword ptr [rsp], 1
 jne .Lhn_copy
 mov qword ptr [rbx + TUI_LI_INPUT_CURSOR], r13
 jmp .Lhn_copy
.Lhn_draft:
 mov r13, qword ptr [rbx + TUI_LI_INPUT_DRAFT_LEN]
 lea rsi, [rbx + TUI_LI_INPUT_DRAFT]
 mov rax, qword ptr [rbx + TUI_LI_INPUT_DRAFT_CURSOR]
 mov qword ptr [rbx + TUI_LI_INPUT_CURSOR], rax
.Lhn_copy:
 mov qword ptr [rbx + TUI_LI_INPUT_LEN], r13
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 mov rdi, rbx
 mov rdx, r13
 call memcpy@PLT
 mov byte ptr [rbx + r13], 0
 mov eax, TUI_LI_KEY_CURSOR
 jmp .Lhn_done
.Lhn_none:
 mov eax, TUI_LI_KEY_IGNORED
.Lhn_done:
 add rsp, 16
 pop r13
 pop r12
 pop rbx
 ret
.size li_history_nav,.-li_history_nav

/* Build the same code-point visual rows as view (including its two-cell prompt).
 * At most 1023 code points plus EOF: 1024 borrowed boundary records on stack.
 * Preferred display column survives short rows; all selected offsets are UTF-8
 * boundaries. No I/O, heap allocation or persistent copy of layout records. */
.type li_vertical,@function
li_vertical:
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp, 16448
 mov rbx, rdi
 mov [rsp + 16400], esi
 cmp dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 jne .Lvv_none
 xor r12d, r12d
 xor r13d, r13d
 mov r14d, 2
 xor r15d, r15d
.Lvv_build:
 cmp r12, [rbx + TUI_LI_INPUT_LEN]
 jae .Lvv_record_end
 cmp byte ptr [rbx + r12], 10
 je .Lvv_record_nl
 movzx edi, byte ptr [rbx + r12]
 call tui_utf8_seq_len@PLT
 mov [rsp + 16404], eax
 lea rdi, [rbx + r12]
 mov esi, eax
 call tui_utf8_cp_width@PLT
 mov [rsp + 16408], eax
 test r14d, r14d
 jz .Lvv_record_char
 add eax, r14d
 cmp eax, dword ptr [rbx + TUI_LI_INPUT_COLUMNS]
 jbe .Lvv_record_char
 inc r13d
 xor r14d, r14d
.Lvv_record_char:
 mov edx, 0
 jmp .Lvv_record
.Lvv_record_nl:
 mov edx, 1
 jmp .Lvv_record
.Lvv_record_end:
 mov edx, 2
.Lvv_record:
 mov rax, r15
 shl rax, 4
 mov [rsp + rax], r12
 mov [rsp + rax + 8], r13d
 mov [rsp + rax + 12], r14d
 cmp r12, [rbx + TUI_LI_INPUT_CURSOR]
 jne .Lvv_not_cursor
 mov [rsp + 16392], r15
.Lvv_not_cursor:
 inc r15
 cmp edx, 2
 je .Lvv_built
 cmp edx, 1
 je .Lvv_next_row
 mov eax, [rsp + 16404]
 add r12, rax
 add r14d, [rsp + 16408]
 jmp .Lvv_build
.Lvv_next_row:
 inc r12
 inc r13d
 xor r14d, r14d
 jmp .Lvv_build
.Lvv_built:
 mov rax, [rsp + 16392]
 shl rax, 4
 mov ecx, [rsp + rax + 8]
 mov edx, [rsp + rax + 12]
 test ecx, ecx
 jnz .Lvv_content_col
 sub edx, 2 /* prompt is decoration, not an editor content column */
.Lvv_content_col:
 add ecx, [rsp + 16400]
 test ecx, ecx
 js .Lvv_above
 cmp ecx, r13d
 ja .Lvv_below
 mov [rsp + 16412], ecx
 cmp dword ptr [rbx + TUI_LI_INPUT_PREF_VALID], 0
 jne .Lvv_preferred
 mov [rbx + TUI_LI_INPUT_PREF_COL], edx
 mov dword ptr [rbx + TUI_LI_INPUT_PREF_VALID], 1
.Lvv_preferred:
 mov edx, [rbx + TUI_LI_INPUT_PREF_COL]
 mov qword ptr [rsp + 16424], -1
 xor r12d, r12d
.Lvv_select:
 cmp r12, r15
 jae .Lvv_selected
 mov rax, r12
 shl rax, 4
 mov ecx, [rsp + rax + 8]
 cmp ecx, [rsp + 16412]
 jne .Lvv_select_next
 cmp qword ptr [rsp + 16424], -1
 je .Lvv_take
 mov ecx, [rsp + rax + 12]
 cmp dword ptr [rsp + rax + 8], 0
 jne .Lvv_candidate_col
 sub ecx, 2
.Lvv_candidate_col:
 cmp ecx, edx
 ja .Lvv_select_next
.Lvv_take:
 mov rcx, [rsp + rax]
 mov [rsp + 16424], rcx
.Lvv_select_next:
 inc r12
 jmp .Lvv_select
.Lvv_selected:
 mov rax, [rsp + 16424]
 jmp .Lvv_set
.Lvv_above:
 cmp qword ptr [rbx + TUI_LI_INPUT_CURSOR], 0
 je .Lvv_history
 cmp dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX], -1
 jne .Lvv_history
 xor eax, eax
 jmp .Lvv_edge
.Lvv_below:
 cmp dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX], -1
 jne .Lvv_history
 mov rax, [rbx + TUI_LI_INPUT_LEN]
.Lvv_edge:
 mov dword ptr [rbx + TUI_LI_INPUT_PREF_VALID], 0
.Lvv_set:
 mov [rbx + TUI_LI_INPUT_CURSOR], rax
 mov eax, TUI_LI_KEY_CURSOR
 jmp .Lvv_done
.Lvv_history:
 mov dword ptr [rbx + TUI_LI_INPUT_PREF_VALID], 0
 mov rdi, rbx
 mov esi, [rsp + 16400]
 call li_history_nav
 jmp .Lvv_done
.Lvv_none:
 mov eax, TUI_LI_KEY_IGNORED
.Lvv_done:
 add rsp, 16448
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 ret
.size li_vertical,.-li_vertical

/* void tui_input_init(tui_input *in) — 清 len..结构尾（56 字节，buf 不要求清空） */
.globl tui_input_init
.type tui_input_init,@function
tui_input_init:
 .cfi_startproc
 mov dword ptr [rdi + TUI_LI_INPUT_SEARCH_ACTIVE], 0
 mov qword ptr [rdi + TUI_LI_INPUT_COMPLETION_OPEN], 0
 mov dword ptr [rdi + TUI_LI_INPUT_COLUMNS], 80
 mov dword ptr [rdi + TUI_LI_INPUT_PREF_VALID], 0
 mov dword ptr [rdi + TUI_LI_INPUT_HISTORY_INDEX], -1
 lea rdi, [rdi + TUI_LI_INPUT_LEN]
 xor eax, eax
 mov ecx, 7
 cld
 rep stosq
 ret
 .cfi_endproc
.size tui_input_init, .-tui_input_init

/* 提交后清草稿；粘贴/转义激活时调用方不得提交（Enter 只在普通模式产生）。 */
.globl tui_input_reset
.type tui_input_reset,@function
tui_input_reset:
 .cfi_startproc
 mov dword ptr [rdi + TUI_LI_INPUT_SEARCH_ACTIVE], 0
 mov qword ptr [rdi + TUI_LI_INPUT_COMPLETION_OPEN], 0
 mov dword ptr [rdi + TUI_LI_INPUT_PREF_VALID], 0
 mov dword ptr [rdi + TUI_LI_INPUT_HISTORY_INDEX], -1
 mov qword ptr [rdi + TUI_LI_INPUT_LEN], 0
 mov qword ptr [rdi + TUI_LI_INPUT_CURSOR], 0
 mov dword ptr [rdi + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 mov dword ptr [rdi + TUI_LI_INPUT_UTF8_PENDING], 0
 mov dword ptr [rdi + TUI_LI_INPUT_PASTE_REJECT], 0
 mov dword ptr [rdi + TUI_LI_INPUT_ENTER_CR], 0
 ret
 .cfi_endproc
.size tui_input_reset, .-tui_input_reset

.globl tui_input_text
.type tui_input_text,@function
tui_input_text:
 .cfi_startproc
 mov rax, rdi
 ret
 .cfi_endproc
.size tui_input_text, .-tui_input_text

.globl tui_input_len
.type tui_input_len,@function
tui_input_len:
 .cfi_startproc
 mov rax, qword ptr [rdi + TUI_LI_INPUT_LEN]
 ret
 .cfi_endproc
.size tui_input_len, .-tui_input_len

.globl tui_input_cursor
.type tui_input_cursor,@function
tui_input_cursor:
 .cfi_startproc
 mov rax, qword ptr [rdi + TUI_LI_INPUT_CURSOR]
 ret
 .cfi_endproc
.size tui_input_cursor, .-tui_input_cursor

.globl tui_input_empty
.type tui_input_empty,@function
tui_input_empty:
 .cfi_startproc
 xor eax, eax
 cmp qword ptr [rdi + TUI_LI_INPUT_LEN], 0
 sete al
 ret
 .cfi_endproc
.size tui_input_empty, .-tui_input_empty

/* int tui_input_incomplete(const tui_input *in)：粘贴或转义序列尚未完成 */
.globl tui_input_incomplete
.type tui_input_incomplete,@function
tui_input_incomplete:
 .cfi_startproc
 xor eax, eax
 cmp dword ptr [rdi + TUI_LI_INPUT_PASTE], 0
 jne .Lincomplete_yes
 cmp dword ptr [rdi + TUI_LI_INPUT_ESC_STATE], 0
 jne .Lincomplete_yes
 cmp dword ptr [rdi + TUI_LI_INPUT_UTF8_PENDING], 0
 jne .Lincomplete_yes
 ret
.Lincomplete_yes:
 mov eax, 1
 ret
 .cfi_endproc
.size tui_input_incomplete, .-tui_input_incomplete

/* Discard only the incomplete codepoint immediately before the cursor, retaining
 * the complete suffix. Used before movement or replacement by a fresh byte. */
.type li_drop_partial,@function
li_drop_partial:
 cmp dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 je .Ldrop_ret
 push r12
 push r13
 sub rsp, 8
 mov r13, [rbx + TUI_LI_INPUT_CURSOR]
 mov rdi, rbx
 mov rsi, r13
 call tui_utf8_last_seq_start@PLT
 mov r12, rax
 lea rdi, [rbx + r12]
 lea rsi, [rbx + r13]
 mov rdx, [rbx + TUI_LI_INPUT_LEN]
 sub rdx, r13
 call memmove@PLT
 sub r13, r12
 sub [rbx + TUI_LI_INPUT_LEN], r13
 mov [rbx + TUI_LI_INPUT_CURSOR], r12
 mov dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 add rsp, 8
 pop r13
 pop r12
.Ldrop_ret:
 ret
.size li_drop_partial,.-li_drop_partial

/* A continuation must obey UTF-8 scalar bounds, including first-continuation
 * restrictions and encoded C1 controls. rbx=input, r12b=current byte. */
.type li_valid_cont,@function
li_valid_cont:
 sub rsp, 8
 movzx eax, r12b
 sub eax, 128
 cmp eax, 63
 ja .Lcont_bad
 mov rdi, rbx
 mov rsi, [rbx + TUI_LI_INPUT_CURSOR]
 call tui_utf8_last_seq_start@PLT
 mov rcx, [rbx + TUI_LI_INPUT_CURSOR]
 sub rcx, rax
 cmp rcx, 1
 jne .Lcont_good
 movzx eax, byte ptr [rbx + rax]
 cmp eax, 194
 jne .Lcont_e0
 cmp r12b, 160
 jb .Lcont_bad
.Lcont_e0:
 cmp eax, 224
 jne .Lcont_ed
 cmp r12b, 160
 jb .Lcont_bad
.Lcont_ed:
 cmp eax, 237
 jne .Lcont_f0
 cmp r12b, 159
 ja .Lcont_bad
.Lcont_f0:
 cmp eax, 240
 jne .Lcont_f4
 cmp r12b, 144
 jb .Lcont_bad
.Lcont_f4:
 cmp eax, 244
 jne .Lcont_good
 cmp r12b, 143
 ja .Lcont_bad
.Lcont_good:
 mov eax, 1
 add rsp, 8
 ret
.Lcont_bad:
 xor eax, eax
 add rsp, 8
 ret
.size li_valid_cont,.-li_valid_cont

/* 插入 r12b 到光标处；完整 UTF-8 字符在最后一字节才移动光标。
 * rax：0 成功，-1 容量不足。栈对齐：入口 ≡8，sub 8 后调用 libc。 */
.type li_insert,@function
li_insert:
 cmp r12b, 128
 jae .Linsert_keep_partial
 sub rsp, 8
 call li_drop_partial
 add rsp, 8
.Linsert_keep_partial:
 mov dword ptr [rbx + TUI_LI_INPUT_PREF_VALID], 0
 mov dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX], -1
 sub rsp, 8
 mov rax, qword ptr [rbx + TUI_LI_INPUT_LEN]
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 cmp rax, TUI_LI_INPUT_MAX_BYTES - 1
 jae .Linsert_fail
 lea rdi, [rbx + rcx + 1]
 lea rsi, [rbx + rcx]
 mov rdx, rax
 sub rdx, rcx
 call memmove@PLT
 mov rax, qword ptr [rbx + TUI_LI_INPUT_LEN]
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 mov [rbx + rcx], r12b
 inc rax
 mov qword ptr [rbx + TUI_LI_INPUT_LEN], rax
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 cmp dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 jne .Linsert_continuation
 movzx edi, r12b
 call tui_utf8_seq_len@PLT
 dec eax
 mov dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], eax
 /* 光标是字节偏移：每个已插入字节都前移，多字节拆分输入仍保序；
  * utf8_pending 只约束下一字节必须为连续字节。 */
 inc qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 xor eax, eax
 add rsp, 8
 ret
.Linsert_continuation:
 dec dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING]
 inc qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 xor eax, eax
 add rsp, 8
 ret
.Linsert_fail:
 mov eax, -1
 add rsp, 8
 ret
.size li_insert, .-li_insert

/* 溢出提示节流：同一连续溢出期间只报一次 OVERFLOW。 */
.type li_overflow,@function
li_overflow:
 cmp dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 jne .Loverflow_ignored
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 1
 mov eax, TUI_LI_KEY_OVERFLOW
 ret
.Loverflow_ignored:
 mov eax, TUI_LI_KEY_IGNORED
 ret
.size li_overflow, .-li_overflow

/* Backspace：删除光标前一个 code point（start..cursor），光标落到 start。 */
.type li_backspace,@function
li_backspace:
 mov dword ptr [rbx + TUI_LI_INPUT_PREF_VALID], 0
 mov dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX], -1
 push r13
 push r14
 sub rsp, 8
 mov r13, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 test r13, r13
 jz .Lback_noop
 mov rdi, rbx
 mov rsi, r13
 call tui_utf8_last_seq_start@PLT
 mov r14, rax /* 被删字符起始 */
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 lea rdi, [rbx + r14]
 lea rsi, [rbx + r13]
 mov rdx, rcx
 sub rdx, r13
 call memmove@PLT
 mov rax, r13
 sub rax, r14 /* 删除字节数 */
 sub qword ptr [rbx + TUI_LI_INPUT_LEN], rax
 mov [rbx + TUI_LI_INPUT_CURSOR], r14
 mov dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 0
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 mov dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0 /* 半字符恰在 cursor 前，一并删除 */
 mov eax, TUI_LI_KEY_BACKSPACE
 add rsp, 8
 pop r14
 pop r13
 ret
.Lback_noop:
 mov eax, TUI_LI_KEY_IGNORED
 add rsp, 8
 pop r14
 pop r13
 ret
.size li_backspace, .-li_backspace

/* Delete：删除光标后一个 code point（cursor..cursor+seq）。
 * 钳制到 len：粘贴截断可能留下不完整序列尾。成功时清 utf8_pending
 * （不变式：pending>0 ⇒ 半字符恰在 cursor 处）。 */
.type li_delete,@function
li_delete:
 mov dword ptr [rbx + TUI_LI_INPUT_PREF_VALID], 0
 mov dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX], -1
 push r13
 push r14
 sub rsp, 8
 mov r13, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 cmp r13, qword ptr [rbx + TUI_LI_INPUT_LEN]
 jae .Ldel_noop
 movzx edi, byte ptr [rbx + r13]
 call tui_utf8_seq_len@PLT
 mov r14, rax
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 sub rcx, r13
 cmp r14, rcx
 jbe .Ldel_len_ok
 mov r14, rcx
.Ldel_len_ok:
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 sub rcx, r13
 sub rcx, r14
 lea rdi, [rbx + r13]
 mov rsi, rdi
 add rsi, r14
 mov rdx, rcx
 call memmove@PLT
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 sub rcx, r14
 mov qword ptr [rbx + TUI_LI_INPUT_LEN], rcx
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 mov dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 mov eax, TUI_LI_KEY_CHAR
 add rsp, 8
 pop r14
 pop r13
 ret
.Ldel_noop:
 mov eax, TUI_LI_KEY_IGNORED
 add rsp, 8
 pop r14
 pop r13
 ret
.size li_delete, .-li_delete

/* 粘贴内容消费（rbx=in，当前字节 r12b，返回键值）：
 *  1) 被拒整字符的剩余连续字节吞到序列完；
 *  2) 候选 ESC[201~ 增量匹配（任意分片），未匹配前缀整体丢弃；
 *  3) CR 折叠：CRLF→单 LF，孤立 CR 也补成 LF（容量不足按溢出处理）；
 *  4) 其余控制字节过滤，可打印/多字节内容整字符插入，放不下整字符拒绝。 */
.type li_paste,@function
li_paste:
 cmp dword ptr [rbx + TUI_LI_INPUT_PASTE_REJECT], 0
 je .Lp_match
 mov eax, r12d
 and eax, 0xC0
 cmp eax, 0x80
 jne .Lp_reject_done
 dec dword ptr [rbx + TUI_LI_INPUT_PASTE_REJECT]
 jmp .Lpaste_ignored
.Lp_reject_done:
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_REJECT], 0
.Lp_match:
 mov ecx, dword ptr [rbx + TUI_LI_INPUT_PASTE_MATCH]
 test ecx, ecx
 jz .Lp_no_match
 lea rax, [rip + .Lpaste_end]
 movzx eax, byte ptr [rax + rcx]
 cmp r12b, al
 jne .Lp_match_fail
 inc ecx
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_MATCH], ecx
 cmp ecx, 6
 je .Lp_end
 jmp .Lpaste_ignored
.Lp_match_fail:
 /* 已缓冲的未匹配前缀整体丢弃；当前字节重新按内容处理 */
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_MATCH], 0
.Lp_no_match:
 cmp r12b, 0x1B
 jne .Lp_content_entry
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_MATCH], 1
 jmp .Lpaste_ignored
.Lp_content_entry:
 cmp dword ptr [rbx + TUI_LI_INPUT_PASTE_CR], 0
 je .Lp_content
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_CR], 0
 cmp r12b, 10
 je .Lp_crlf_lf
 push r12
 mov r12b, 10
 call li_insert
 pop r12
 test eax, eax
 jnz li_overflow /* 折叠 LF 放不下：当前字节一并丢弃 */
 jmp .Lp_content
.Lp_crlf_lf:
 call li_insert
 test eax, eax
 jnz li_overflow
 jmp .Lpaste_ignored
.Lp_content:
 cmp r12b, 13
 jne .Lp_not_cr
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_CR], 1
 jmp .Lpaste_ignored
.Lp_not_cr:
 cmp r12b, 10
 je .Lp_insert
 cmp r12b, 32
 jb .Lpaste_ignored /* 控制字节过滤（含 ^C/^D/ESC 序列字节） */
 cmp r12b, 127
 je .Lpaste_ignored
 cmp r12b, 128
 jb .Lp_ascii
 cmp dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 jne .Lp_cont
 mov eax, r12d
 and eax, 0xC0
 cmp eax, 0x80
 je .Lpaste_ignored /* 孤立连续字节过滤 */
 cmp r12b, 128
 jb .Lpaste_lead_ok
 cmp r12b, 194
 jb .Lpaste_ignored
 cmp r12b, 244
 ja .Lpaste_ignored
.Lpaste_lead_ok:
 movzx edi, r12b
 call tui_utf8_seq_len@PLT
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 add rcx, rax
 cmp rcx, TUI_LI_INPUT_MAX_BYTES - 1
 ja .Lp_reject_lead
 jmp .Lp_insert
.Lp_cont:
 sub rsp, 8
 call li_valid_cont
 add rsp, 8
 test eax, eax
 jnz .Lp_insert
 sub rsp, 8
 call li_drop_partial
 add rsp, 8
 jmp .Lpaste_ignored
 jmp .Lp_insert
.Lp_ascii:
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 cmp rcx, TUI_LI_INPUT_MAX_BYTES - 1
 jae li_overflow
.Lp_insert:
 call li_insert
 test eax, eax
 jnz li_overflow
.Lpaste_ignored:
 mov eax, TUI_LI_KEY_IGNORED
 ret
.Lp_reject_lead:
 /* 整字符拒绝：剩余连续字节仍要吞掉，溢出提示走节流 */
 dec eax
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_REJECT], eax
 jmp li_overflow
.Lp_end:
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE], 0
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_MATCH], 0
 cmp dword ptr [rbx + TUI_LI_INPUT_PASTE_CR], 0
 je .Lp_end_ok
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_CR], 0
 push r12
 mov r12b, 10
 call li_insert
 pop r12
 test eax, eax
 jnz li_overflow /* 尾部折叠 LF 溢出：粘贴仍结束，提示已节流 */
.Lp_end_ok:
 mov eax, TUI_LI_KEY_PASTE
 ret
.size li_paste, .-li_paste

.section .rodata
.Lpaste_end: .ascii "\033[201~"
.section .text

/* tui_key_kind tui_input_feed(tui_input *in, unsigned char b) — rdi=in sil=b → eax */
.globl tui_input_feed
.type tui_input_feed,@function
tui_input_feed:
 .cfi_startproc
 push rbx
 push r12
 push r13
 mov rbx, rdi
 movzx r12d, sil
 cmp dword ptr [rbx + TUI_LI_INPUT_PASTE], 0
 jne .Lfeed_paste
 mov eax, dword ptr [rbx + TUI_LI_INPUT_ESC_STATE]
 test eax, eax
 jnz .Lfeed_esc
 cmp r12b, 0x1B
 jne .Lfeed_plain
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 1
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM], 0
 mov dword ptr [rbx + TUI_LI_INPUT_ENTER_CR], 0
 jmp .Lfeed_ignored

.Lfeed_paste:
 call li_paste
 jmp .Lfeed_out

/* ---- 转义序列：Ctrl-C 任意时刻可中止；CAN/SUB 静默中止 ---- */
.Lfeed_esc:
 cmp eax, 1
 je .Lesc_one
 cmp eax, 2
 je .Lesc_csi
 cmp eax, 3
 je .Lesc_ss3
 cmp eax, 4
 je .Lesc_string
 cmp eax, 5
 je .Lesc_st
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 jmp .Lfeed_ignored
.Lesc_abort_ctrl_c:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 mov eax, TUI_LI_KEY_CTRL_C
 jmp .Lfeed_out
.Lesc_abort:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 jmp .Lfeed_ignored
.Lesc_restart:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 1
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM], 0
 jmp .Lfeed_ignored
.Lesc_one:
 cmp r12b, 0x03
 je .Lesc_abort_ctrl_c
 cmp r12b, 0x1B
 je .Lfeed_ignored /* ESC ESC：保持状态 1，重新等待下一字节 */
 cmp r12b, '['
 je .Lesc_start_csi
 cmp r12b, 'O'
 je .Lesc_start_ss3
 cmp r12b, ']'
 je .Lesc_start_string
 cmp r12b, 'P'
 je .Lesc_start_string
 cmp r12b, '_'
 je .Lesc_start_string
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 cmp r12b, 13
 je .Lesc_alt_newline
 cmp r12b, 10
 je .Lesc_alt_newline
 jmp .Lfeed_ignored /* Meta+键：整组合吞掉 */
.Lesc_alt_newline:
 mov r12b, 10
 call li_insert
 test eax, eax
 jnz .Lfeed_overflow
 mov eax, TUI_LI_KEY_CHAR
 jmp .Lfeed_out
.Lesc_start_csi:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 2
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM], 0
 jmp .Lfeed_ignored
.Lesc_start_ss3:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 3
 jmp .Lfeed_ignored
.Lesc_start_string:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 4
 jmp .Lfeed_ignored
.Lesc_ss3:
 cmp r12b, 0x03
 je .Lesc_abort_ctrl_c
 cmp r12b, 0x1B
 je .Lesc_restart
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 cmp r12b, 'A'
 je .Lfeed_up
 cmp r12b, 'B'
 je .Lfeed_down
 cmp r12b, 'D'
 je .Lfeed_left
 cmp r12b, 'C'
 je .Lfeed_right
 cmp r12b, 'H'
 je .Lfeed_home
 cmp r12b, 'F'
 je .Lfeed_end
 cmp r12b, 'P'
 je .Lfeed_help
 cmp r12b, 'M'
 je .Lfeed_enter
 jmp .Lfeed_ignored
.Lesc_string:
 cmp r12b, 7
 je .Lesc_string_done /* BEL 结束 */
 cmp r12b, 0x03
 je .Lesc_abort_ctrl_c
 cmp r12b, 0x18
 je .Lesc_abort
 cmp r12b, 0x1A
 je .Lesc_abort
 cmp r12b, 0x1B
 jne .Lfeed_ignored /* 内容字节全部消费，无上限：靠 BEL/ST/CAN/^C 收尾 */
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 5
 jmp .Lfeed_ignored
.Lesc_string_done:
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 jmp .Lfeed_ignored
.Lesc_st:
 cmp r12b, 0x03
 je .Lesc_abort_ctrl_c
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 cmp r12b, 0x5C /* '\'：ST 正常结束 */
 je .Lfeed_ignored
 cmp r12b, 0x1B
 jne .Lfeed_ignored
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 1
 jmp .Lfeed_ignored
.Lesc_csi:
 cmp r12b, 0x03
 je .Lesc_abort_ctrl_c
 cmp r12b, 0x18
 je .Lesc_abort
 cmp r12b, 0x1A
 je .Lesc_abort
 cmp r12b, 0x1B
 je .Lesc_restart
 cmp r12b, '0'
 jb .Lcsi_mid
 cmp r12b, '9'
 ja .Lcsi_mid
 /* 数字参数：超大量数字只消费不再累加（防溢出） */
 mov r13d, dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM]
 cmp r13d, 100000
 jae .Lfeed_ignored
 imul r13d, r13d, 10
 movzx eax, r12b
 sub eax, '0'
 add r13d, eax
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM], r13d
 jmp .Lfeed_ignored
.Lcsi_mid:
 cmp r12b, 0x20
 jb .Lesc_abort /* 其余 C0：中止序列，字节按丢弃 */
 cmp r12b, 0x3F
 jbe .Lfeed_ignored /* 中间字节/参数分隔：继续消费 */
 cmp r12b, 0x7F
 jae .Lesc_abort
 /* 0x40–0x7E：终结字节 → 分派 */
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_STATE], 0
 mov r13d, dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM]
 mov dword ptr [rbx + TUI_LI_INPUT_ESC_PARAM], 0
 cmp r12b, '~'
 je .Lcsi_tilde
 cmp r13d, 0
 jne .Lfeed_ignored
 cmp r12b, 'A'
 je .Lfeed_up
 cmp r12b, 'B'
 je .Lfeed_down
 cmp r12b, 'D'
 je .Lfeed_left
 cmp r12b, 'C'
 je .Lfeed_right
 cmp r12b, 'H'
 je .Lfeed_home
 cmp r12b, 'F'
 je .Lfeed_end
 jmp .Lfeed_ignored
.Lcsi_tilde:
 cmp r13d, 5
 je .Lfeed_page_up
 cmp r13d, 6
 je .Lfeed_page_down
 cmp r13d, 11
 je .Lfeed_help
 mov eax, TUI_LI_KEY_IGNORED
 cmp r13d, 200
 je .Lpaste_begin
 cmp r13d, 201
 je .Lfeed_out /* 游离结束标记：忽略 */
 cmp r13d, 3
 je .Lfeed_delete
 cmp r13d, 1
 je .Lfeed_home
 cmp r13d, 7
 je .Lfeed_home
 cmp r13d, 4
 je .Lfeed_end
 cmp r13d, 8
 je .Lfeed_end
 jmp .Lfeed_out
.Lpaste_begin:
 mov dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 0
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE], 1
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_MATCH], 0
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_CR], 0
 mov dword ptr [rbx + TUI_LI_INPUT_PASTE_REJECT], 0
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 jmp .Lfeed_ignored /* utf8_pending 保留：粘贴前的半字符可由内容补全 */

/* ---- 普通按键 ---- */
.Lfeed_plain:
 cmp dword ptr [rbx + TUI_LI_INPUT_ENTER_CR], 0
 je .Lplain_not_lf
 mov dword ptr [rbx + TUI_LI_INPUT_ENTER_CR], 0
 cmp r12b, 10
 je .Lfeed_ignored /* CRLF 的 LF：不二次提交 */
.Lplain_not_lf:
 cmp r12b, 18
 je .Lsearch_next
 cmp dword ptr [rbx + TUI_LI_INPUT_SEARCH_ACTIVE], 0
 je .Lsearch_plain
 cmp r12b, 19
 je .Lsearch_previous
 cmp r12b, 3
 je .Lsearch_cancel
 cmp r12b, 4
 je .Lsearch_cancel
 cmp r12b, 9
 je .Lfeed_ignored
 cmp r12b, 13
 je .Lsearch_accept_cr
 cmp r12b, 10
 je .Lsearch_accept
.Lsearch_plain:
 cmp dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 0
 je .Lplain_no_menu
 cmp r12b, 13
 je .Lmenu_accept_cr
 cmp r12b, 10
 je .Lmenu_accept
 cmp r12b, 3
 je .Lmenu_close
 cmp r12b, 9
 je .Lmenu_move
 mov dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 0
.Lplain_no_menu:
 cmp r12b, 9
 je .Lfeed_complete
 cmp r12b, 13
 je .Lfeed_enter_cr
 cmp r12b, 10
 je .Lfeed_enter
 cmp r12b, 8
 je .Lfeed_backspace
 cmp r12b, 127
 je .Lfeed_backspace
 cmp r12b, 3
 je .Lfeed_ctrl_c
 cmp r12b, 4
 je .Lfeed_ctrl_d
 cmp r12b, 21
 je .Lfeed_ctrl_u
 cmp r12b, 15
 je .Lfeed_ctrl_o
 cmp r12b, 32
 jb .Lfeed_ignored
 cmp dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 jne .Lplain_cont
 mov eax, r12d
 and eax, 0xC0
 cmp eax, 0x80
 je .Lfeed_ignored /* 孤立连续字节 */
 cmp r12b, 128
 jb .Lplain_lead_ok
 cmp r12b, 194
 jb .Lfeed_ignored
 cmp r12b, 244
 ja .Lfeed_ignored
.Lplain_lead_ok:
 movzx edi, r12b
 call tui_utf8_seq_len@PLT
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
 add rcx, rax
 cmp rcx, TUI_LI_INPUT_MAX_BYTES - 1
 ja .Lfeed_overflow
 jmp .Lplain_insert
.Lplain_cont:
 cmp r12b, 128
 jb .Lplain_insert
 call li_valid_cont
 test eax, eax
 jnz .Lplain_insert
 call li_drop_partial
 jmp .Lfeed_ignored
.Lplain_insert:
 call li_insert
 test eax, eax
 jnz .Lfeed_overflow
 mov eax, TUI_LI_KEY_CHAR
 jmp .Lfeed_out

/* ---- 光标移动（清 utf8_pending：半字符留在原地成为惰性数据） ---- */
.Lfeed_left:
 call li_drop_partial
 mov rsi, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 test rsi, rsi
 jz .Lfeed_ignored
 mov rdi, rbx
 call tui_utf8_last_seq_start@PLT
 mov [rbx + TUI_LI_INPUT_CURSOR], rax
 jmp .Lfeed_cursor
.Lfeed_right:
 call li_drop_partial
 mov r13, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 cmp r13, qword ptr [rbx + TUI_LI_INPUT_LEN]
 jae .Lfeed_ignored
 movzx edi, byte ptr [rbx + r13]
 call tui_utf8_seq_len@PLT
 add qword ptr [rbx + TUI_LI_INPUT_CURSOR], rax
 jmp .Lfeed_cursor
.Lfeed_home:
 call li_drop_partial
 mov rax, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
.Lhome_loop:
 test rax, rax
 jz .Lhome_set
 cmp byte ptr [rbx + rax - 1], 10
 je .Lhome_set
 dec rax
 jmp .Lhome_loop
.Lhome_set:
 mov qword ptr [rbx + TUI_LI_INPUT_CURSOR], rax
 jmp .Lfeed_cursor
.Lfeed_end:
 call li_drop_partial
 mov rax, qword ptr [rbx + TUI_LI_INPUT_CURSOR]
 mov rcx, qword ptr [rbx + TUI_LI_INPUT_LEN]
.Lend_loop:
 cmp rax, rcx
 jae .Lend_set
 cmp byte ptr [rbx + rax], 10
 je .Lend_set
 inc rax
 jmp .Lend_loop
.Lend_set:
 mov qword ptr [rbx + TUI_LI_INPUT_CURSOR], rax
 jmp .Lfeed_cursor
.Lfeed_up:
 cmp dword ptr [rbx + TUI_LI_INPUT_SEARCH_ACTIVE], 0
 jne .Lsearch_next
 cmp dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 0
 jne .Lmenu_move
 mov esi, -1
 jmp .Lfeed_vertical
.Lfeed_down:
 cmp dword ptr [rbx + TUI_LI_INPUT_SEARCH_ACTIVE], 0
 jne .Lsearch_previous
 cmp dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 0
 jne .Lmenu_move
 mov esi, 1
.Lfeed_vertical:
 mov rdi, rbx
 call li_vertical
 jmp .Lfeed_out
.Lfeed_cursor:
 mov dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 0
 mov dword ptr [rbx + TUI_LI_INPUT_PREF_VALID], 0
 mov dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 mov eax, TUI_LI_KEY_CURSOR
 jmp .Lfeed_out

.Lfeed_enter_cr:
 mov dword ptr [rbx + TUI_LI_INPUT_ENTER_CR], 1
.Lfeed_enter:
 mov eax, TUI_LI_KEY_ENTER
 jmp .Lfeed_out
.Lfeed_backspace:
 call li_backspace
 jmp .Lfeed_out
.Lfeed_delete:
 call li_delete
 jmp .Lfeed_out
.Lfeed_ctrl_c:
 mov eax, TUI_LI_KEY_CTRL_C
 jmp .Lfeed_out
.Lfeed_ctrl_d:
 cmp qword ptr [rbx + TUI_LI_INPUT_LEN], 0
 jne .Lfeed_backspace /* 非空 ^D：保留现有退格语义 */
 mov eax, TUI_LI_KEY_CTRL_D
 jmp .Lfeed_out
.Lfeed_ctrl_u:
 mov dword ptr [rbx + TUI_LI_INPUT_PREF_VALID], 0
 mov dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX], -1
 mov qword ptr [rbx + TUI_LI_INPUT_LEN], 0
 mov qword ptr [rbx + TUI_LI_INPUT_CURSOR], 0
 mov dword ptr [rbx + TUI_LI_INPUT_OVERFLOW_LATCH], 0
 mov dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 mov eax, TUI_LI_KEY_CHAR
 jmp .Lfeed_out
.Lsearch_next:
 mov rdi, rbx
 mov esi, 1
 call li_history_search
 jmp .Lfeed_out
.Lsearch_previous:
 mov rdi, rbx
 mov esi, -1
 call li_history_search
 jmp .Lfeed_out
.Lsearch_cancel:
 mov rdx, [rbx + TUI_LI_INPUT_DRAFT_LEN]
 mov rax, [rbx + TUI_LI_INPUT_DRAFT_CURSOR]
 mov [rbx + TUI_LI_INPUT_CURSOR], rax
 lea rsi, [rbx + TUI_LI_INPUT_DRAFT]
 jmp .Lsearch_copy_accept
.Lsearch_accept_cr:
 mov dword ptr [rbx + TUI_LI_INPUT_ENTER_CR], 1
.Lsearch_accept:
 movsxd rax, dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX]
 test rax, rax
 js .Lsearch_refresh
 mov edx, [rbx + rax*4 + TUI_LI_INPUT_HISTORY_LENS]
 mov [rbx + TUI_LI_INPUT_CURSOR], rdx
 shl rax, 10
 lea rsi, [rbx + rax + TUI_LI_INPUT_HISTORY]
.Lsearch_copy_accept:
 mov [rbx + TUI_LI_INPUT_LEN], rdx
 mov rdi, rbx
 call memcpy@PLT
 mov dword ptr [rbx + TUI_LI_INPUT_SEARCH_ACTIVE], 0
 mov dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX], -1
 mov dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 mov eax, TUI_LI_KEY_CURSOR
 jmp .Lfeed_out
.Lsearch_refresh:
 mov eax, TUI_LI_KEY_SEARCH
 jmp .Lfeed_out
.Lmenu_accept_cr:
 mov dword ptr [rbx + TUI_LI_INPUT_ENTER_CR], 1
.Lmenu_accept:
 mov dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 2
 mov rdi, rbx
 call li_complete_command
 mov dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 0
 jmp .Lfeed_out
.Lmenu_close:
 mov dword ptr [rbx + TUI_LI_INPUT_COMPLETION_OPEN], 0
 mov eax, TUI_LI_KEY_CURSOR
 jmp .Lfeed_out
.Lmenu_move:
 xor dword ptr [rbx + TUI_LI_INPUT_COMPLETION_SEL], 1
 mov eax, TUI_LI_KEY_COMPLETIONS
 jmp .Lfeed_out
.Lfeed_complete:
 mov rdi, rbx
 call li_complete_command
 jmp .Lfeed_out
.Lfeed_page_up:
 mov eax, TUI_LI_KEY_PAGE_UP
 jmp .Lfeed_out
.Lfeed_page_down:
 mov eax, TUI_LI_KEY_PAGE_DOWN
 jmp .Lfeed_out
.Lfeed_help:
 mov eax, TUI_LI_KEY_HELP
 jmp .Lfeed_out
.Lfeed_ctrl_o:
 mov eax, TUI_LI_KEY_CTRL_O
 jmp .Lfeed_out
.Lfeed_overflow:
 call li_overflow
 jmp .Lfeed_out
.Lfeed_ignored:
 mov eax, TUI_LI_KEY_IGNORED
.Lfeed_out:
 cmp dword ptr [rbx + TUI_LI_INPUT_SEARCH_ACTIVE], 0
 je .Lfeed_return
 cmp eax, TUI_LI_KEY_CHAR
 je .Lsearch_edit
 cmp eax, TUI_LI_KEY_BACKSPACE
 je .Lsearch_edit
 cmp eax, TUI_LI_KEY_PASTE
 jne .Lfeed_return
.Lsearch_edit:
 cmp dword ptr [rbx + TUI_LI_INPUT_UTF8_PENDING], 0
 jne .Lfeed_return
 mov dword ptr [rbx + TUI_LI_INPUT_HISTORY_INDEX], -1
 mov rdi, rbx
 mov esi, 1
 call li_history_search
.Lfeed_return:
 pop r13
 pop r12
 pop rbx
 ret
 .cfi_endproc
.size tui_input_feed, .-tui_input_feed


/* A timed standalone ESC only cancels active native controls. The byte parser
 * itself has no wall clock and preserves arbitrary CSI/paste split points. */
.globl tui_input_escape_timeout
.type tui_input_escape_timeout,@function
tui_input_escape_timeout:
 cmp dword ptr [rdi + TUI_LI_INPUT_PASTE], 0
 jne .Lescape_noop
 cmp dword ptr [rdi + TUI_LI_INPUT_ESC_STATE], 1
 jne .Lescape_noop
 mov eax, [rdi + TUI_LI_INPUT_SEARCH_ACTIVE]
 or eax, [rdi + TUI_LI_INPUT_COMPLETION_OPEN]
 jz .Lescape_noop
 mov dword ptr [rdi + TUI_LI_INPUT_ESC_STATE], 0
 mov esi, 3
 jmp tui_input_feed
.Lescape_noop:
 mov eax, TUI_LI_KEY_IGNORED
 ret
.size tui_input_escape_timeout,.-tui_input_escape_timeout

.section .note.GNU-stack,"",@progbits
