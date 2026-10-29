/* Pi Durable harness/prompt.ts and pi-ai utils/transcript.ts, d5b3de22.
   MIT, docs/licenses/pi.md. Ordered maps use json-c's public iterator ABI. */
.intel_syntax noprefix
.macro PF n
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,\n
.endm
.macro PE n
 add rsp,\n
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.endm
.macro PG obj,key
 mov rdi,\obj
 lea rsi,[rip+.Lp_\key]
 call json_object_object_get@PLT
.endm
.macro PI obj,off
 mov rdi,\obj
 call json_object_iter_begin@PLT
 mov [rsp+\off],rax
 mov rdi,\obj
 call json_object_iter_end@PLT
 mov [rsp+\off+8],rax
.endm
.macro PN off,target
 lea rdi,[rsp+\off]
 lea rsi,[rsp+\off+8]
 call json_object_iter_equal@PLT
 test eax,eax
 jnz \target
 lea rdi,[rsp+\off]
 call json_object_iter_peek_name@PLT
 mov [rsp+\off+16],rax
 lea rdi,[rsp+\off]
 call json_object_iter_peek_value@PLT
.endm
.text
/* Add a borrowed immutable value. */
.Lp_set:
 PF 16
 mov rbx,rdi
 mov r12,rsi
 mov rdi,rdx
 call json_object_get@PLT
 mov r13,rax
 mov rdi,rbx
 mov rsi,r12
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jz .Lp_set_end
 mov rdi,r13
 call json_object_put@PLT
 mov eax,5
.Lp_set_end:
 PE 16
/* Patch one ordered map; NULL deletes. */
.Lp_patch:
 PF 32
 mov rbx,rdi
 mov r12,rsi
 test r12,r12
 jz .Lp_patch_ok
 PI r12,0
.Lp_patch_each:
 PN 0,.Lp_patch_ok
 test rax,rax
 jz .Lp_patch_delete
 mov rdi,rbx
 mov rsi,[rsp+16]
 mov rdx,rax
 call .Lp_set
 test eax,eax
 jnz .Lp_patch_end
 jmp .Lp_patch_next
.Lp_patch_delete:
 mov rdi,rbx
 mov rsi,[rsp+16]
 call json_object_object_del@PLT
.Lp_patch_next:
 lea rdi,[rsp]
 call json_object_iter_next@PLT
 jmp .Lp_patch_each
.Lp_patch_ok:
 xor eax,eax
.Lp_patch_end:
 PE 32
/* Compare only ordered keys. */
.Lp_order:
 PF 48
 mov rbx,rdi
 mov r12,rsi
 PI rbx,0
 PI r12,24
.Lp_order_each:
 lea rdi,[rsp]
 lea rsi,[rsp+8]
 call json_object_iter_equal@PLT
 mov ebp,eax
 lea rdi,[rsp+24]
 lea rsi,[rsp+32]
 call json_object_iter_equal@PLT
 test ebp,ebp
 jnz .Lp_order_last
 test eax,eax
 jnz .Lp_order_no
 lea rdi,[rsp]
 call json_object_iter_peek_name@PLT
 mov r14,rax
 lea rdi,[rsp+24]
 call json_object_iter_peek_name@PLT
 mov rdi,r14
 mov rsi,rax
 call strcmp@PLT
 test eax,eax
 jnz .Lp_order_no
 lea rdi,[rsp]
 call json_object_iter_next@PLT
 lea rdi,[rsp+24]
 call json_object_iter_next@PLT
 jmp .Lp_order_each
.Lp_order_last:
 test eax,eax
 setnz al
 movzx eax,al
 jmp .Lp_order_end
.Lp_order_no:
 xor eax,eax
.Lp_order_end:
 PE 48
/* out[2] = removal map and addition map; return 0 minimal, 1 order rewrite,
   5 failure. Tools retain only equal declarations; sections retain keys. */
.Lp_diff:
 PF 80
 mov rbx,rdi
 mov r12,rsi
 mov r13,rcx
 mov [rsp+64],edx
 mov qword ptr [r13],0
 mov qword ptr [r13+8],0
 mov qword ptr [rsp+32],0
 call json_object_new_object@PLT
 mov [r13],rax
 call json_object_new_object@PLT
 mov [r13+8],rax
 call json_object_new_object@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lp_diff_bad
 cmp qword ptr [r13],0
 je .Lp_diff_bad
 cmp qword ptr [r13+8],0
 je .Lp_diff_bad
 PI rbx,0
.Lp_diff_old:
 PN 0,.Lp_diff_desired_start
 mov [rsp+24],rax
 mov rdi,r12
 mov rsi,[rsp+16]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lp_diff_remove
 cmp dword ptr [rsp+64],0
 je .Lp_diff_keep
 mov rdi,[rsp+24]
 mov rsi,rax
 call json_object_equal@PLT
 test eax,eax
 jz .Lp_diff_remove
.Lp_diff_keep:
 mov rdi,[rsp+32]
 mov rsi,[rsp+16]
 mov rdx,[rsp+24]
 call .Lp_set
 jmp .Lp_diff_old_check
.Lp_diff_remove:
 mov rdi,[r13]
 mov rsi,[rsp+16]
 xor edx,edx
 call json_object_object_add@PLT
.Lp_diff_old_check:
 test eax,eax
 jnz .Lp_diff_bad
 lea rdi,[rsp]
 call json_object_iter_next@PLT
 jmp .Lp_diff_old
.Lp_diff_desired_start:
 PI r12,0
.Lp_diff_desired:
 PN 0,.Lp_diff_compare
 mov [rsp+24],rax
 mov rdi,rbx
 mov rsi,[rsp+16]
 call json_object_object_get@PLT
 test rax,rax
 jz .Lp_diff_add
 mov rdi,rax
 mov rsi,[rsp+24]
 call json_object_equal@PLT
 test eax,eax
 jnz .Lp_diff_desired_order
.Lp_diff_add:
 mov rdi,[r13+8]
 mov rsi,[rsp+16]
 mov rdx,[rsp+24]
 call .Lp_set
 test eax,eax
 jnz .Lp_diff_bad
.Lp_diff_desired_order:
 mov rdi,[rsp+32]
 mov rsi,[rsp+16]
 mov rdx,[rsp+24]
 call .Lp_set
 test eax,eax
 jnz .Lp_diff_bad
 lea rdi,[rsp]
 call json_object_iter_next@PLT
 jmp .Lp_diff_desired
.Lp_diff_compare:
 mov rdi,[rsp+32]
 mov rsi,r12
 call .Lp_order
 mov ebp,0
 test eax,eax
 jnz .Lp_diff_end
 mov rdi,[r13]
 call json_object_put@PLT
 call json_object_new_object@PLT
 mov [r13],rax
 test rax,rax
 jz .Lp_diff_bad
 PI rbx,0
.Lp_diff_remove_all:
 PN 0,.Lp_diff_add_all
 mov rdi,[r13]
 mov rsi,[rsp+16]
 xor edx,edx
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lp_diff_bad
 lea rdi,[rsp]
 call json_object_iter_next@PLT
 jmp .Lp_diff_remove_all
.Lp_diff_add_all:
 mov rdi,[r13+8]
 call json_object_put@PLT
 mov qword ptr [r13+8],0
 mov rdi,r12
 lea rsi,[r13+8]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lp_diff_bad
 mov ebp,1
 jmp .Lp_diff_end
.Lp_diff_bad:
 mov ebp,5
.Lp_diff_end:
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov eax,ebp
 PE 80
.globl pg_prompt_replay
.type pg_prompt_replay,@function
pg_prompt_replay:
 PF 64
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [r12],0
 mov qword ptr [rsp+8],0
 lea rdi,[rip+.Lp_state_json]
 call json_tokener_parse@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lp_replay_bad
 PG rax,sections
 mov r13,rax
 PG "qword ptr [rsp]",tools
 mov r14,rax
 PG rbx,messages
 mov rdi,rax
 call json_object_get@PLT
 mov [rsp+8],rax
 test rax,rax
 jz .Lp_replay_bad
 mov qword ptr [rsp+16],0
.Lp_replay_message:
 mov rdi,[rsp+8]
 call json_object_array_length@PLT
 cmp [rsp+16],rax
 jae .Lp_replay_ok
 mov rdi,[rsp+8]
 mov rsi,[rsp+16]
 call json_object_array_get_idx@PLT
 mov rbp,rax
 PG rbp,role
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lp_replay_next
 mov rdi,rax
 lea rsi,[rip+.Lp_system]
 call strcmp@PLT
 test eax,eax
 jnz .Lp_replay_next
 PG rbp,sections
 mov rdi,r13
 mov rsi,rax
 call .Lp_patch
 test eax,eax
 jnz .Lp_replay_bad
 PG rbp,toolsRemoved
 mov [rsp+24],rax
 mov dword ptr [rsp+40],0
 jmp .Lp_replay_tools_start
.Lp_replay_added:
 PG rbp,toolsAdded
 mov [rsp+24],rax
 mov dword ptr [rsp+40],1
.Lp_replay_tools_start:
 mov qword ptr [rsp+32],0
.Lp_replay_tool:
 cmp qword ptr [rsp+24],0
 je .Lp_replay_tools_end
 mov rdi,[rsp+24]
 call json_object_array_length@PLT
 cmp [rsp+32],rax
 jae .Lp_replay_tools_end
 mov rdi,[rsp+24]
 mov rsi,[rsp+32]
 call json_object_array_get_idx@PLT
 mov [rsp+48],rax
 PG rax,name
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lp_replay_bad
 mov rdi,r14
 mov rsi,rax
 cmp dword ptr [rsp+40],0
 jne .Lp_replay_tool_add
 call json_object_object_del@PLT
 jmp .Lp_replay_tool_next
.Lp_replay_tool_add:
 mov rdx,[rsp+48]
 call .Lp_set
 test eax,eax
 jnz .Lp_replay_bad
.Lp_replay_tool_next:
 inc qword ptr [rsp+32]
 jmp .Lp_replay_tool
.Lp_replay_tools_end:
 cmp dword ptr [rsp+40],0
 je .Lp_replay_added
.Lp_replay_next:
 inc qword ptr [rsp+16]
 jmp .Lp_replay_message
.Lp_replay_ok:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rax,[rsp]
 mov [r12],rax
 xor eax,eax
 PE 64
.Lp_replay_bad:
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp]
 call json_object_put@PLT
 mov eax,5
 PE 64
/* Convert a map to an array of declarations (mode 0) or name refs (mode 1). */
.Lp_values:
 PF 48
 mov rbx,rdi
 mov r12,rsi
 mov [rsp+32],edx
 call json_object_new_array@PLT
 mov r13,rax
 test rax,rax
 jz .Lp_values_bad
 PI rbx,0
.Lp_values_each:
 PN 0,.Lp_values_ok
 cmp dword ptr [rsp+32],0
 je .Lp_values_value
 call json_object_new_object@PLT
 mov r14,rax
 test rax,rax
 jz .Lp_values_bad
 mov rdi,[rsp+16]
 call json_object_new_string@PLT
 test rax,rax
 jz .Lp_values_item_bad
 mov rdi,r14
 lea rsi,[rip+.Lp_name]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lp_values_item_bad
 jmp .Lp_values_append
.Lp_values_value:
 mov rdi,rax
 call json_object_get@PLT
 mov r14,rax
.Lp_values_append:
 mov rdi,r13
 mov rsi,r14
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lp_values_item_bad
 lea rdi,[rsp]
 call json_object_iter_next@PLT
 jmp .Lp_values_each
.Lp_values_item_bad:
 mov rdi,r14
 call json_object_put@PLT
.Lp_values_bad:
 mov rdi,r13
 call json_object_put@PLT
 mov eax,5
 PE 48
.Lp_values_ok:
 mov [r12],r13
 xor eax,eax
 PE 48
/* Append an owned native system draft with optional borrowed sections. */
.Lp_draft:
 PF 32
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 lea rdi,[rip+.Lp_draft_json]
 call json_tokener_parse@PLT
 mov r14,rax
 test rax,rax
 jz .Lp_draft_bad
 PG r14,model
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rbp,rax
 mov rdi,r13
 call json_object_new_uint64@PLT
 test rax,rax
 jz .Lp_draft_bad
 mov rdi,rbp
 lea rsi,[rip+.Lp_timestamp]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lp_draft_bad
 test r12,r12
 jz .Lp_draft_append
 mov rdi,rbp
 lea rsi,[rip+.Lp_sections]
 mov rdx,r12
 call .Lp_set
 test eax,eax
 jnz .Lp_draft_bad
.Lp_draft_append:
 mov rdi,rbx
 mov rsi,r14
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lp_draft_bad
 xor eax,eax
 PE 32
.Lp_draft_bad:
 mov rdi,r14
 call json_object_put@PLT
 mov eax,5
 PE 32
.Lp_is_system:
 push rbx
 PG rdi,kind_key
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lp_is_system_no
 mov rdi,rax
 lea rsi,[rip+.Lp_kind]
 call strcmp@PLT
 test eax,eax
 setz al
 movzx eax,al
 pop rbx
 ret
.Lp_is_system_no:
 xor eax,eax
 pop rbx
 ret
.globl pg_prompt_plan
.type pg_prompt_plan,@function
pg_prompt_plan:
 PF 192
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,r8
 mov rbp,rcx
 mov rdi,rsp
 xor eax,eax
 mov ecx,24
 rep stosq
 mov [rsp+40],rbp
 mov qword ptr [r14],0
 mov rdi,rbx
 mov rsi,rsp
 call pg_prompt_replay
 test eax,eax
 jnz .Lp_plan_bad
 PG "qword ptr [rsp]",sections
 mov [rsp+8],rax
 PG "qword ptr [rsp]",tools
 mov [rsp+16],rax
 call json_object_new_object@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lp_plan_bad
 xor ebp,ebp
.Lp_plan_tool_map:
 mov rdi,r13
 call json_object_array_length@PLT
 cmp rbp,rax
 jae .Lp_plan_head
 mov rdi,r13
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov [rsp+152],rax
 PG rax,name
 mov rdi,rax
 call json_object_get_string@PLT
 test rax,rax
 jz .Lp_plan_bad
 mov rdi,[rsp+24]
 mov rsi,rax
 mov rdx,[rsp+152]
 call .Lp_set
 test eax,eax
 jnz .Lp_plan_bad
 inc rbp
 jmp .Lp_plan_tool_map
.Lp_plan_head:
 PG rbx,entries
 mov [rsp+120],rax
 PG rbx,head
 test rax,rax
 jz .Lp_plan_diff
 PG rax,id
 mov rdi,rax
 call json_object_get_uint64@PLT
 mov [rsp+88],rax
 mov dword ptr [rsp+80],1
 xor ebp,ebp
.Lp_plan_after_head:
 mov rdi,[rsp+120]
 call json_object_array_length@PLT
 cmp rbp,rax
 jae .Lp_plan_baseline
 mov rdi,[rsp+120]
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov [rsp+152],rax
 mov rdi,rax
 call .Lp_is_system
 test eax,eax
 jz .Lp_plan_after_next
 PG "qword ptr [rsp+152]",id
 mov rdi,rax
 call json_object_get_uint64@PLT
 cmp rax,[rsp+88]
 ja .Lp_plan_normal
.Lp_plan_after_next:
 inc rbp
 jmp .Lp_plan_after_head
.Lp_plan_normal:
 mov dword ptr [rsp+80],0
.Lp_plan_diff:
 mov rdi,[rsp+8]
 mov rsi,r12
 xor edx,edx
 lea rcx,[rsp+48]
 call .Lp_diff
 cmp eax,5
 je .Lp_plan_bad
 mov [rsp+184],eax
 mov rdi,[rsp+16]
 mov rsi,[rsp+24]
 mov edx,1
 lea rcx,[rsp+64]
 call .Lp_diff
 cmp eax,5
 je .Lp_plan_bad
 jmp .Lp_plan_drafts
.Lp_plan_baseline:
 call json_object_new_object@PLT
 mov [rsp+48],rax
 call json_object_new_object@PLT
 mov [rsp+64],rax
 mov rdi,r12
 lea rsi,[rsp+56]
 xor edx,edx
 call json_object_deep_copy@PLT
 test eax,eax
 jnz .Lp_plan_bad
 mov rdi,[rsp+24]
 call json_object_get@PLT
 mov [rsp+72],rax
 cmp qword ptr [rsp+48],0
 je .Lp_plan_bad
 cmp qword ptr [rsp+64],0
 je .Lp_plan_bad
.Lp_plan_drafts:
 call json_object_new_array@PLT
 mov [rsp+32],rax
 test rax,rax
 jz .Lp_plan_bad
 cmp dword ptr [rsp+80],0
 jne .Lp_plan_full_section
 cmp dword ptr [rsp+184],0
 jne .Lp_plan_reorder_sections
 mov rdi,[rsp+48]
 mov rsi,[rsp+56]
 call .Lp_patch
 test eax,eax
 jnz .Lp_plan_bad
 mov rdi,[rsp+48]
 call json_object_object_length@PLT
 test eax,eax
 jz .Lp_plan_tools
 mov rdi,[rsp+32]
 mov rsi,[rsp+48]
 mov rdx,[rsp+40]
 call .Lp_draft
 test eax,eax
 jnz .Lp_plan_bad
 jmp .Lp_plan_tools
.Lp_plan_reorder_sections:
 mov rdi,[rsp+32]
 mov rsi,[rsp+48]
 mov rdx,[rsp+40]
 call .Lp_draft
 test eax,eax
 jnz .Lp_plan_bad
.Lp_plan_full_section:
 mov rdi,[rsp+32]
 mov rsi,[rsp+56]
 mov rdx,[rsp+40]
 call .Lp_draft
 test eax,eax
 jnz .Lp_plan_bad
.Lp_plan_tools:
 mov rdi,[rsp+64]
 lea rsi,[rsp+160]
 mov edx,1
 call .Lp_values
 test eax,eax
 jnz .Lp_plan_bad
 mov rdi,[rsp+72]
 lea rsi,[rsp+168]
 xor edx,edx
 call .Lp_values
 test eax,eax
 jnz .Lp_plan_bad
 mov rdi,[rsp+160]
 call json_object_array_length@PLT
 mov rbp,rax
 mov rdi,[rsp+168]
 call json_object_array_length@PLT
 or rbp,rax
 jz .Lp_plan_edits
 mov rdi,[rsp+32]
 call json_object_array_length@PLT
 test rax,rax
 jnz .Lp_plan_last
 mov rdi,[rsp+32]
 xor esi,esi
 mov rdx,[rsp+40]
 call .Lp_draft
 test eax,eax
 jnz .Lp_plan_bad
 mov eax,1
.Lp_plan_last:
 lea rsi,[rax-1]
 mov rdi,[rsp+32]
 call json_object_array_get_idx@PLT
 PG rax,model
 mov rdi,rax
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rbp,rax
 mov rdi,[rsp+160]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lp_plan_tools_added
 mov rdi,rbp
 lea rsi,[rip+.Lp_toolsRemoved]
 mov rdx,[rsp+160]
 call .Lp_set
 test eax,eax
 jnz .Lp_plan_bad
.Lp_plan_tools_added:
 mov rdi,[rsp+168]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lp_plan_edits
 mov rdi,rbp
 lea rsi,[rip+.Lp_toolsAdded]
 mov rdx,[rsp+168]
 call .Lp_set
 test eax,eax
 jnz .Lp_plan_bad
.Lp_plan_edits:
 cmp dword ptr [rsp+80],0
 je .Lp_plan_ok
 call json_object_new_array@PLT
 mov [rsp+144],rax
 test rax,rax
 jz .Lp_plan_bad
 xor ebp,ebp
.Lp_plan_omit:
 mov rdi,[rsp+120]
 call json_object_array_length@PLT
 cmp rbp,rax
 jae .Lp_plan_omit_done
 mov rdi,[rsp+120]
 mov rsi,rbp
 call json_object_array_get_idx@PLT
 mov [rsp+152],rax
 mov rdi,rax
 call .Lp_is_system
 test eax,eax
 jz .Lp_plan_omit_next
 lea rdi,[rip+.Lp_edit_json]
 call json_tokener_parse@PLT
 mov [rsp+176],rax
 test rax,rax
 jz .Lp_plan_bad
 PG "qword ptr [rsp+152]",id
 mov rdi,[rsp+176]
 lea rsi,[rip+.Lp_target]
 mov rdx,rax
 call .Lp_set
 test eax,eax
 jnz .Lp_plan_bad
 mov rdi,[rsp+144]
 mov rsi,[rsp+176]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lp_plan_bad
 mov qword ptr [rsp+176],0
.Lp_plan_omit_next:
 inc rbp
 jmp .Lp_plan_omit
.Lp_plan_omit_done:
 mov rdi,[rsp+144]
 call json_object_array_length@PLT
 test rax,rax
 jz .Lp_plan_ok
 mov rdi,[rsp+32]
 xor esi,esi
 call json_object_array_get_idx@PLT
 mov rdi,rax
 lea rsi,[rip+.Lp_edits]
 mov rdx,[rsp+144]
 call .Lp_set
 test eax,eax
 jnz .Lp_plan_bad
.Lp_plan_ok:
 mov rax,[rsp+32]
 mov [r14],rax
 mov qword ptr [rsp+32],0
 xor ebp,ebp
 jmp .Lp_plan_end
.Lp_plan_bad:
 mov ebp,5
.Lp_plan_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov rdi,[rsp+32]
 call json_object_put@PLT
 mov rdi,[rsp+48]
 call json_object_put@PLT
 mov rdi,[rsp+56]
 call json_object_put@PLT
 mov rdi,[rsp+64]
 call json_object_put@PLT
 mov rdi,[rsp+72]
 call json_object_put@PLT
 mov rdi,[rsp+144]
 call json_object_put@PLT
 mov rdi,[rsp+160]
 call json_object_put@PLT
 mov rdi,[rsp+168]
 call json_object_put@PLT
 mov rdi,[rsp+176]
 call json_object_put@PLT
 mov eax,ebp
 PE 192
.globl pg_prompt_tools
.type pg_prompt_tools,@function
pg_prompt_tools:
 PF 32
 mov rbx,rdi
 mov r12,rsi
 mov qword ptr [r12],0
 mov qword ptr [rsp],0
 call json_object_new_array@PLT
 mov r13,rax
 test rax,rax
 jz .Lp_catalog_bad
 xor r14d,r14d
.Lp_catalog_each:
 cmp r14,[rbx+1152]
 jae .Lp_catalog_ok
 imul rbp,r14,72
 add rbp,rbx
 call json_object_new_object@PLT
 mov [rsp],rax
 test rax,rax
 jz .Lp_catalog_bad
 mov rdi,[rbp]
 call json_object_new_string@PLT
 test rax,rax
 jz .Lp_catalog_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lp_name]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lp_catalog_bad
 mov rdi,[rbp+8]
 call json_object_new_string@PLT
 test rax,rax
 jz .Lp_catalog_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lp_description]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lp_catalog_bad
 mov rdi,[rbp+16]
 call json_tokener_parse@PLT
 test rax,rax
 jz .Lp_catalog_bad
 mov rdi,[rsp]
 lea rsi,[rip+.Lp_parameters]
 mov rdx,rax
 call json_object_object_add@PLT
 test eax,eax
 jnz .Lp_catalog_bad
 mov rdi,r13
 mov rsi,[rsp]
 call json_object_array_add@PLT
 test eax,eax
 jnz .Lp_catalog_bad
 mov qword ptr [rsp],0
 inc r14
 jmp .Lp_catalog_each
.Lp_catalog_ok:
 mov [r12],r13
 xor eax,eax
 PE 32
.Lp_catalog_bad:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,r13
 call json_object_put@PLT
 mov eax,5
 PE 32
.section .rodata
.Lp_state_json: .asciz "{\"sections\":{},\"tools\":{}}"
.Lp_draft_json: .asciz "{\"kind\":\"pangu.system\",\"model\":[{\"role\":\"system\",\"content\":\"\"}]}"
.Lp_edit_json: .asciz "{\"target\":0,\"action\":\"omit\"}"
.Lp_sections: .asciz "sections"
.Lp_tools: .asciz "tools"
.Lp_messages: .asciz "messages"
.Lp_role: .asciz "role"
.Lp_system: .asciz "system"
.Lp_kind: .asciz "pangu.system"
.Lp_kind_key: .asciz "kind"
.Lp_name: .asciz "name"
.Lp_description: .asciz "description"
.Lp_parameters: .asciz "parameters"
.Lp_toolsRemoved: .asciz "toolsRemoved"
.Lp_toolsAdded: .asciz "toolsAdded"
.Lp_timestamp: .asciz "timestamp"
.Lp_model: .asciz "model"
.Lp_entries: .asciz "entries"
.Lp_head: .asciz "head"
.Lp_id: .asciz "id"
.Lp_target: .asciz "target"
.Lp_edits: .asciz "edits"
.section .note.GNU-stack,"",@progbits
.text
.globl pg_prompt_configure
.type pg_prompt_configure,@function
pg_prompt_configure:
 PF 48
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov r14,rcx
 mov rdi,rsp
 xor eax,eax
 mov ecx,6
 rep stosq
 mov [rsp+40],r8
 PG r12,promptSections
 mov rbp,rax
 test rbp,rbp
 jz .Lp_config_bad
 PG "qword ptr [rsp+40]",instructions
 test rax,rax
 jz .Lp_config_tools
 mov [rsp+32],rax
 lea rdi,[rip+.Lp_instructions_open]
 call json_object_new_string@PLT
 mov [rsp+16],rax
 test rax,rax
 jz .Lp_config_bad
 lea rdi,[rsp+16]
 mov rsi,[rsp+32]
 xor edx,edx
 call pg_context_append@PLT
 test eax,eax
 jnz .Lp_config_bad
 lea rdi,[rip+.Lp_instructions_close]
 call json_object_new_string@PLT
 mov [rsp+24],rax
 test rax,rax
 jz .Lp_config_bad
 lea rdi,[rsp+16]
 mov rsi,rax
 xor edx,edx
 call pg_context_append@PLT
 test eax,eax
 jnz .Lp_config_bad
 mov rdi,rbp
 lea rsi,[rip+.Lp_instructions]
 mov rdx,[rsp+16]
 call .Lp_set
 test eax,eax
 jnz .Lp_config_bad
.Lp_config_tools:
 mov rdi,r13
 mov rsi,rsp
 call pg_prompt_tools
 test eax,eax
 jnz .Lp_config_bad
 mov rdi,rbx
 mov rsi,rbp
 mov rdx,[rsp]
 mov rcx,r14
 lea r8,[rsp+8]
 call pg_prompt_plan
 test eax,eax
 jnz .Lp_config_bad
 mov rdi,r12
 lea rsi,[rip+.Lp_promptPlan]
 mov rdx,[rsp+8]
 call .Lp_set
 test eax,eax
 jnz .Lp_config_bad
 xor ebp,ebp
 jmp .Lp_config_end
.Lp_config_bad:
 mov ebp,5
.Lp_config_end:
 mov rdi,[rsp]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 call json_object_put@PLT
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+24]
 call json_object_put@PLT
 mov eax,ebp
 PE 48
.section .rodata
.Lp_promptSections: .asciz "promptSections"
.Lp_instructions: .asciz "instructions"
.Lp_promptPlan: .asciz "promptPlan"
.Lp_instructions_open: .asciz "<instructions>\n"
.Lp_instructions_close: .asciz "\n</instructions>"