#include "journal_layout.h"
#include "session_layout.h"
.intel_syntax noprefix
.text
# FNV-1a detects accidental corruption; it is not authentication.
.Lhash:
    mov rax,0xcbf29ce484222325
    mov r8,0x100000001b3
    xor ecx,ecx
.Lhash_loop:
    cmp rcx,rsi
    jae .Lhash_end
    movzx edx,byte ptr [rdi+rcx]
    xor rax,rdx
    imul rax,r8
    inc rcx
    jmp .Lhash_loop
.Lhash_end:
    ret
.Lfield_length:
    xor eax,eax
.Lfield_loop:
    cmp byte ptr [rdi+rax],0
    je .Lfield_end
    inc eax
    cmp eax,64
    jb .Lfield_loop
.Lfield_end:
    ret

# Encode only explicit wire fields, never native padding or pointers.
.Lencode:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp,24
    mov rbx,rdi
    mov r12,rdx
    mov rax,PG_J_MAGIC
    mov [r12],rax
    mov dword ptr [r12+8],1
    mov [r12+16],rsi
    mov eax,[rbx]
    cmp eax,5
    ja .Lencode_bad
    mov [r12+24],eax
    mov dword ptr [r12+28],0
    lea r13,[r12+40]
    cmp eax,5
    je .Lencode_submission
    test eax,eax
    jz .Lencode_messages
    cmp eax,4
    je .Lencode_profile
    cmp eax,1
    jne .Lencode_checkpoint_only
    mov rcx,[rbx+32]
    cmp rcx,4096
    jae .Lencode_bad
    mov rsi,[rbx+24]
    test rcx,rcx
    jz .Lencode_finish
    test rsi,rsi
    jz .Lencode_bad
    mov rdi,r13
    rep movsb
    mov r13,rdi
    jmp .Lencode_finish
.Lencode_profile:
    cmp qword ptr [rbx+40],0
    jne .Lencode_bad
    mov rcx,[rbx+32]
    test rcx,rcx
    jz .Lencode_bad
    cmp rcx,4096
    ja .Lencode_bad
    mov rsi,[rbx+24]
    test rsi,rsi
    jz .Lencode_bad
    mov dword ptr [r12+8],3
    mov rdi,r13
    rep movsb
    mov r13,rdi
    jmp .Lencode_finish
.Lencode_submission:
    cmp qword ptr [rbx+16],1
    jne .Lencode_bad
    cmp qword ptr [rbx+40],0
    je .Lencode_bad
    mov rcx,[rbx+32]
    test rcx,rcx
    jz .Lencode_bad
    cmp rcx,63
    ja .Lencode_bad
    mov rsi,[rbx+24]
    test rsi,rsi
    jz .Lencode_bad
    mov dword ptr [r12+8],4
    mov [r13],ecx
    lea rdi,[r13+4]
.Lencode_key:
    lodsb
    test al,al
    jz .Lencode_bad
    stosb
    dec rcx
    jnz .Lencode_key
    mov r13,rdi
.Lencode_messages:
    mov r14,[rbx+16]
    test r14,r14
    jz .Lencode_bad
    cmp r14,2
    ja .Lencode_bad
    mov [r12+28],r14d
    mov r15,[rbx+8]
    test r15,r15
    jz .Lencode_bad
.Lencode_one:
    mov eax,[r15]
    cmp eax,3
    ja .Lencode_bad
    mov [r13],eax
    mov eax,[r15+PG_MSG_ERROR]
    cmp eax,1
    ja .Lencode_bad
    mov [r13+4],eax
    mov rax,[r15+PG_MSG_SIZE]
    cmp rax,4096
    jae .Lencode_bad
    cmp byte ptr [r15+PG_MSG_TEXT+rax],0
    jne .Lencode_bad
    mov [r13+8],eax
    lea rdi,[r15+PG_MSG_ID]
    call .Lfield_length
    cmp eax,64
    jae .Lencode_bad
    mov [r13+12],ax
    mov [rsp],rax
    lea rdi,[r15+PG_MSG_NAME]
    call .Lfield_length
    cmp eax,64
    jae .Lencode_bad
    mov [r13+14],ax
    mov [rsp+8],rax
    lea rdi,[r13+16]
    lea rsi,[r15+PG_MSG_TEXT]
    mov rcx,[r15+PG_MSG_SIZE]
    rep movsb
    lea rsi,[r15+PG_MSG_ID]
    mov rcx,[rsp]
    rep movsb
    lea rsi,[r15+PG_MSG_NAME]
    mov rcx,[rsp+8]
    rep movsb
    mov r13,rdi
    add r15,PG_MSG_BYTES
    dec r14
    jnz .Lencode_one
    jmp .Lencode_finish
.Lencode_checkpoint_only:
    cmp dword ptr [rbx],3
    jne .Lencode_finish
    cmp qword ptr [rbx+40],0
    je .Lencode_bad
.Lencode_finish:
    mov rsi,[rbx+40]
    test rsi,rsi
    jz .Lencode_no_checkpoint
    cmp dword ptr [r12+8],4
    je .Lencode_checkpoint
    mov dword ptr [r12+8],2
.Lencode_checkpoint:
    mov rdi,r13
    mov ecx,5
    rep movsq
    mov r13,rdi
.Lencode_no_checkpoint:
    mov rsi,r13
    sub rsi,r12
    lea eax,[rsi+16]
    mov [r12+12],eax
    mov rdi,r12
    mov esi,32
    call .Lhash
    mov [r12+32],rax
    mov rsi,r13
    sub rsi,r12
    mov rdi,r12
    call .Lhash
    mov [r13],rax
    mov rax,PG_J_MARKER
    mov [r13+8],rax
    lea rax,[r13+16]
    sub rax,r12
    jmp .Lencode_return
.Lencode_bad:
    xor eax,eax
.Lencode_return:
    add rsp,24
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

# Decode into detached caller scratch. Session validation precedes adoption.
.Ldecode:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp,24
    mov rbx,rdi
    mov r12,rsi
    mov r13,rcx
    mov r14,r8
    mov [rsp+16],r9
    cmp r12,56
    jb .Ldecode_bad
    cmp r12,PG_J_RECORD_MAX
    ja .Ldecode_bad
    mov rax,PG_J_MAGIC
    cmp [rbx],rax
    jne .Ldecode_bad
    cmp dword ptr [rbx+8],1
    jb .Ldecode_bad
    cmp dword ptr [rbx+8],4
    ja .Ldecode_bad
    cmp [rbx+12],r12d
    jne .Ldecode_bad
    cmp [rbx+16],rdx
    jne .Ldecode_bad
    mov rax,PG_J_MARKER
    cmp [rbx+r12-8],rax
    jne .Ldecode_bad
    mov rdi,rbx
    mov esi,32
    call .Lhash
    cmp [rbx+32],rax
    jne .Ldecode_bad
    mov rdi,rbx
    lea rsi,[r12-16]
    call .Lhash
    cmp [rbx+r12-16],rax
    jne .Ldecode_bad
    lea rax,[rbx+r12-16]
    mov [rsp],rax
    mov rdi,r13
    xor eax,eax
    mov ecx,6
    rep stosq
    cmp dword ptr [rbx+8],4
    je .Ldecode_checkpoint
    cmp dword ptr [rbx+8],2
    jne .Ldecode_no_checkpoint
.Ldecode_checkpoint:
    cmp r12,96
    jb .Ldecode_bad
    sub qword ptr [rsp],40
    mov rsi,[rsp]
    mov rdi,[rsp+16]
    test rdi,rdi
    jz .Ldecode_bad
    mov [r13+40],rdi
    mov ecx,5
    rep movsq
.Ldecode_no_checkpoint:
    mov eax,[rbx+24]
    cmp eax,5
    ja .Ldecode_bad
    mov [r13],eax
    lea r15,[rbx+40]
    cmp eax,5
    je .Ldecode_submission
    test eax,eax
    jz .Ldecode_messages
    cmp dword ptr [rbx+28],0
    jne .Ldecode_bad
    cmp eax,4
    je .Ldecode_profile
    cmp eax,1
    je .Ldecode_system
    cmp eax,3
    jne .Ldecode_reset
    cmp dword ptr [rbx+8],2
    jne .Ldecode_bad
.Ldecode_reset:
    cmp r15,[rsp]
    jne .Ldecode_bad
    jmp .Ldecode_good
.Ldecode_profile:
    cmp dword ptr [rbx+8],3
    jne .Ldecode_bad
    mov rax,[rsp]
    sub rax,r15
    test rax,rax
    jz .Ldecode_bad
    cmp rax,4096
    ja .Ldecode_bad
    mov [r13+24],r15
    mov [r13+32],rax
    jmp .Ldecode_good
.Ldecode_system:
    mov rax,[rsp]
    sub rax,r15
    cmp rax,4096
    jae .Ldecode_bad
    mov [r13+24],r15
    mov [r13+32],rax
    jmp .Ldecode_good
.Ldecode_submission:
    cmp dword ptr [rbx+8],4
    jne .Ldecode_bad
    cmp dword ptr [rbx+28],1
    jne .Ldecode_bad
    lea rax,[r15+4]
    cmp rax,[rsp]
    ja .Ldecode_bad
    mov edx,[r15]
    test edx,edx
    jz .Ldecode_bad
    cmp edx,63
    ja .Ldecode_bad
    mov [r13+24],rax
    mov [r13+32],rdx
    add rax,rdx
    cmp rax,[rsp]
    ja .Ldecode_bad
    mov r15,rax
.Ldecode_messages:
    mov ebp,[rbx+28]
    test ebp,ebp
    jz .Ldecode_bad
    cmp ebp,2
    ja .Ldecode_bad
    mov [r13+16],rbp
    mov [r13+8],r14
.Ldecode_one:
    lea rax,[r15+16]
    cmp rax,[rsp]
    ja .Ldecode_bad
    mov eax,[r15+8]
    cmp eax,4096
    jae .Ldecode_bad
    movzx ecx,word ptr [r15+12]
    cmp ecx,64
    jae .Ldecode_bad
    movzx edx,word ptr [r15+14]
    cmp edx,64
    jae .Ldecode_bad
    add rax,rcx
    add rax,rdx
    lea rax,[r15+rax+16]
    cmp rax,[rsp]
    ja .Ldecode_bad
    mov [rsp+8],rax
    mov rdi,r14
    xor eax,eax
    mov ecx,PG_MSG_BYTES/8
    rep stosq
    mov eax,[r15]
    cmp eax,3
    ja .Ldecode_bad
    mov [r14],eax
    mov eax,[r15+4]
    cmp eax,1
    ja .Ldecode_bad
    mov [r14+PG_MSG_ERROR],eax
    mov eax,[r15+8]
    mov [r14+PG_MSG_SIZE],rax
    lea rdi,[r14+PG_MSG_TEXT]
    lea rsi,[r15+16]
    mov rcx,rax
    rep movsb
    lea rdi,[r14+PG_MSG_ID]
    movzx ecx,word ptr [r15+12]
    rep movsb
    lea rdi,[r14+PG_MSG_NAME]
    movzx ecx,word ptr [r15+14]
    rep movsb
    mov r15,[rsp+8]
    add r14,PG_MSG_BYTES
    dec ebp
    jnz .Ldecode_one
    cmp r15,[rsp]
    jne .Ldecode_bad
.Ldecode_good:
    xor eax,eax
    jmp .Ldecode_return
.Ldecode_bad:
    mov eax,1
.Ldecode_return:
    add rsp,24
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret

.globl pg_journal_commit
.type pg_journal_commit,@function
pg_journal_commit:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp,PG_J_COMMIT_STACK
    mov rbx,rdi
    mov r12,rsi
    mov r13,rdx
    mov r14,rcx
    test rbx,rbx
    jz .Lcommit_reject
    test r12,r12
    jz .Lcommit_reject
    test r14,r14
    jz .Lcommit_reject
    cmp dword ptr [rbx+PG_J_FILE],0
    jl .Lcommit_reject
    cmp dword ptr [rbx+PG_J_FAILED],0
    jne .Lcommit_reject
    cmp dword ptr [rbx+PG_J_BUSY],0
    jne .Lcommit_reject
    cmp [rbx+PG_J_SEQUENCE],r13
    jne .Lcommit_reject
    cmp r13,-1
    je .Lcommit_reject
    mov rdi,r12
    lea rsi,[r13+1]
    mov rdx,rsp
    call .Lencode
    test rax,rax
    jz .Lcommit_reject
    mov r15,rax
    add rax,[rbx+PG_J_END]
    jc .Lcommit_reject
    cmp rax,PG_J_LIMIT
    ja .Lcommit_reject
    mov [rsp+PG_J_RECORD_MAX],rax
    mov dword ptr [rbx+PG_J_BUSY],1
    mov edi,[rbx+PG_J_FILE]
    lea rsi,[rsp+PG_J_RECORD_MAX+8]
    call pg_journal_file_size@PLT
    test eax,eax
    jnz .Lcommit_uncertain
    mov rax,[rsp+PG_J_RECORD_MAX+8]
    cmp [rbx+PG_J_END],rax
    jne .Lcommit_uncertain
    xor ebp,ebp
.Lwrite_loop:
    mov edi,[rbx+PG_J_FILE]
    lea rsi,[rsp+rbp]
    mov rdx,r15
    sub rdx,rbp
    mov rcx,[rbx+PG_J_END]
    add rcx,rbp
    call pwrite@PLT
    test rax,rax
    jg .Lwrite_progress
    jz .Lcommit_uncertain
    call __errno_location@PLT
    cmp dword ptr [rax],4
    je .Lwrite_loop
    jmp .Lcommit_uncertain
.Lwrite_progress:
    add rbp,rax
    cmp rbp,r15
    jb .Lwrite_loop
.Lsync_commit:
    mov edi,[rbx+PG_J_FILE]
    call fsync@PLT
    test eax,eax
    jz .Lcommit_confirm
    call __errno_location@PLT
    cmp dword ptr [rax],4
    je .Lsync_commit
    jmp .Lcommit_uncertain
.Lcommit_confirm:
    inc r13
    mov [rbx+PG_J_SEQUENCE],r13
    mov rax,[rsp+PG_J_RECORD_MAX]
    mov [rbx+PG_J_END],rax
    mov [r14],r13
    mov dword ptr [rbx+PG_J_BUSY],0
    xor eax,eax
    jmp .Lcommit_return
.Lcommit_uncertain:
    mov dword ptr [rbx+PG_J_FAILED],1
    mov dword ptr [rbx+PG_J_BUSY],0
    mov eax,2
    jmp .Lcommit_return
.Lcommit_reject:
    mov eax,1
.Lcommit_return:
    add rsp,PG_J_COMMIT_STACK
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
.size pg_journal_commit,.-pg_journal_commit

.globl pg_journal_close
.type pg_journal_close,@function
pg_journal_close:
    test rdi,rdi
    jz .Lclose_return
    push rbx
    mov rbx,rdi
    mov edi,[rbx+PG_J_FILE]
    mov dword ptr [rbx+PG_J_FILE],-1
    test edi,edi
    js .Lclose_directory
    call close@PLT
.Lclose_directory:
    mov edi,[rbx+PG_J_DIRECTORY]
    mov dword ptr [rbx+PG_J_DIRECTORY],-1
    test edi,edi
    js .Lclosed
    call close@PLT
.Lclosed:
    mov qword ptr [rbx+PG_J_FAILED],0
    mov qword ptr [rbx+PG_J_SEQUENCE],0
    mov qword ptr [rbx+PG_J_END],0
    pop rbx
.Lclose_return:
    ret
.size pg_journal_close,.-pg_journal_close

.globl pg_journal_open
.type pg_journal_open,@function
pg_journal_open:
    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    sub rsp,PG_J_OPEN_STACK
    mov rbx,rdi
    mov r14d,esi
    mov r15,rdx
    mov r12,rcx
    mov r13,r8
    test rbx,rbx
    jz .Lopen_reject
    test r12,r12
    jz .Lopen_reject
    test r13,r13
    jz .Lopen_reject
    test r15,r15
    jz .Lopen_reject
    cmp dword ptr [rbx+PG_J_FILE],-1
    jne .Lopen_reject
    cmp dword ptr [rbx+PG_J_DIRECTORY],-1
    jne .Lopen_reject
    cmp qword ptr [r12+PG_S_COUNT],0
    jne .Lopen_reject
    cmp qword ptr [r12+PG_S_SYSTEM_SIZE],0
    jne .Lopen_reject
    cmp qword ptr [r13+PG_SC_SEQ],0
    jne .Lopen_reject
    cmp qword ptr [r13+PG_SC_BUSY],0
    jne .Lopen_reject
    cmp qword ptr [r13+PG_SC_RUN],0
    jne .Lopen_reject
    cmp qword ptr [r13+PG_SC_SUB_COUNT],0
    jne .Lopen_reject
    cmp qword ptr [r13+PG_SC_PROFILE_SIZE],0
    jne .Lopen_reject
    xor ecx,ecx
.Lname_loop:
    movzx eax,byte ptr [r15+rcx]
    test eax,eax
    jz .Lname_end
    cmp eax,47
    je .Lopen_reject
    inc ecx
    cmp ecx,256
    jb .Lname_loop
    jmp .Lopen_reject
.Lname_end:
    test ecx,ecx
    jz .Lopen_reject
    cmp byte ptr [r15],46
    jne .Lname_valid
    cmp ecx,1
    je .Lopen_reject
    cmp ecx,2
    jne .Lname_valid
    cmp byte ptr [r15+1],46
    je .Lopen_reject
.Lname_valid:
    mov edi,r14d
    mov esi,PG_J_DUPFD
    xor edx,edx
    xor eax,eax
    call fcntl@PLT
    test eax,eax
    js .Lopen_reject
    mov [rbx+PG_J_DIRECTORY],eax
    mov edi,eax
    mov rsi,r15
    mov edx,PG_J_CREATE_FLAGS
    mov ecx,384
    xor eax,eax
    call openat@PLT
    test eax,eax
    jns .Lopened_file
    call __errno_location@PLT
    cmp dword ptr [rax],17
    jne .Lopen_fail
    mov edi,[rbx+PG_J_DIRECTORY]
    mov rsi,r15
    mov edx,PG_J_OPEN_FLAGS
    xor eax,eax
    call openat@PLT
    test eax,eax
    js .Lopen_fail
.Lopened_file:
    mov [rbx+PG_J_FILE],eax
    mov edi,eax
    mov esi,6
    call flock@PLT
    test eax,eax
    jnz .Lopen_fail
    mov edi,[rbx+PG_J_FILE]
    lea rsi,[rsp+PG_JO_SIZE]
    call pg_journal_file_size@PLT
    test eax,eax
    jnz .Lopen_fail
    cmp qword ptr [rsp+PG_JO_SIZE],PG_J_LIMIT
    ja .Lopen_fail
    mov rdi,rbx
    add rdi,8
    xor eax,eax
    mov ecx,3
    rep stosq
    lea rdi,[rsp+PG_JO_CONTROL]
    mov ecx,PG_SC_BYTES/8
    rep stosq
    lea rax,[rip+pg_session_volatile_commit]
    mov [rsp+PG_JO_CONTROL+8],rax
    xor ebp,ebp
.Lrecover_loop:
    mov rax,[rsp+PG_JO_SIZE]
    sub rax,rbp
    jz .Lrecover_synced
    cmp rax,40
    jb .Ltorn_tail
    xor r14d,r14d
    mov qword ptr [rsp+PG_JO_READ],40
.Lread_frame:
    mov edi,[rbx+PG_J_FILE]
    lea rsi,[rsp+r14]
    mov rdx,[rsp+PG_JO_READ]
    sub rdx,r14
    lea rcx,[rbp+r14]
    call pread@PLT
    test rax,rax
    jg .Lread_progress
    jz .Lopen_fail
    call __errno_location@PLT
    cmp dword ptr [rax],4
    je .Lread_frame
    jmp .Lopen_fail
.Lread_progress:
    add r14,rax
    cmp r14,[rsp+PG_JO_READ]
    jb .Lread_frame
    cmp qword ptr [rsp+PG_JO_READ],40
    jne .Lread_complete
    mov rax,PG_J_MAGIC
    cmp [rsp],rax
    jne .Lopen_fail
    cmp dword ptr [rsp+8],1
    jb .Lopen_fail
    cmp dword ptr [rsp+8],4
    ja .Lopen_fail
    mov rdi,rsp
    mov esi,32
    call .Lhash
    cmp [rsp+32],rax
    jne .Lopen_fail
    cmp dword ptr [rsp+24],5
    ja .Lopen_fail
    mov eax,[rsp+12]
    cmp eax,56
    jb .Lopen_fail
    cmp eax,PG_J_RECORD_MAX
    ja .Lopen_fail
    mov edx,56
    cmp dword ptr [rsp+8],4
    je .Lheader_v4
    cmp dword ptr [rsp+8],2
    je .Lheader_v2
    cmp dword ptr [rsp+8],3
    je .Lheader_v3
    cmp eax,PG_J_V1_RECORD_MAX
    ja .Lopen_fail
    cmp dword ptr [rsp+24],3
    jae .Lopen_fail
    jmp .Lheader_size_known
.Lheader_v3:
    cmp dword ptr [rsp+24],4
    jne .Lopen_fail
    jmp .Lheader_size_known
.Lheader_v4:
    cmp dword ptr [rsp+24],5
    jne .Lopen_fail
    add edx,40
    jmp .Lheader_size_known
.Lheader_v2:
    cmp dword ptr [rsp+24],4
    jae .Lopen_fail
    cmp dword ptr [rsp+24],1
    je .Lopen_fail
    cmp dword ptr [rsp+24],2
    je .Lopen_fail
    add edx,40
.Lheader_size_known:
    sub eax,edx
    jc .Lopen_fail
    mov edx,[rsp+24]
    cmp edx,5
    je .Lheader_submission
    test edx,edx
    jz .Lheader_append
    cmp dword ptr [rsp+28],0
    jne .Lopen_fail
    cmp edx,4
    je .Lheader_profile
    cmp edx,1
    je .Lheader_system
    test eax,eax
    jne .Lopen_fail
    jmp .Lheader_valid
.Lheader_submission:
    cmp dword ptr [rsp+28],1
    jne .Lopen_fail
    cmp eax,21
    jb .Lopen_fail
    jmp .Lheader_valid
.Lheader_profile:
    test eax,eax
    jz .Lopen_fail
    cmp eax,4096
    ja .Lopen_fail
    jmp .Lheader_valid
.Lheader_system:
    cmp eax,4095
    ja .Lopen_fail
    jmp .Lheader_valid
.Lheader_append:
    mov edx,[rsp+28]
    test edx,edx
    jz .Lopen_fail
    cmp edx,2
    ja .Lopen_fail
    shl edx,4
    cmp eax,edx
    jb .Lopen_fail
.Lheader_valid:
    mov eax,[rsp+12]
    mov rdx,[rsp+PG_JO_CONTROL+PG_SC_SEQ]
    inc rdx
    cmp [rsp+16],rdx
    jne .Lopen_fail
    mov rdx,[rsp+PG_JO_SIZE]
    sub rdx,rbp
    cmp rax,rdx
    ja .Ltorn_tail
    mov [rsp+PG_JO_READ],rax
    jmp .Lread_frame
.Lread_complete:
    mov rdi,rsp
    mov rsi,[rsp+PG_JO_READ]
    mov rdx,[rsp+PG_JO_CONTROL+PG_SC_SEQ]
    inc rdx
    lea rcx,[rsp+PG_JO_CHANGE]
    lea r8,[rsp+PG_JO_MESSAGES]
    lea r9,[rsp+PG_JO_CHECKPOINT]
    call .Ldecode
    test eax,eax
    jnz .Lopen_fail
    mov rdi,r12
    lea rsi,[rsp+PG_JO_CONTROL]
    lea rdx,[rsp+PG_JO_CHANGE]
    call pg_session_commit@PLT
    test eax,eax
    jnz .Lopen_fail
    add rbp,[rsp+PG_JO_READ]
    jmp .Lrecover_loop
.Ltorn_tail:
    mov edi,[rbx+PG_J_FILE]
    mov rsi,rbp
    call ftruncate@PLT
    test eax,eax
    jnz .Lopen_fail
.Lrecover_synced:
    mov edi,[rbx+PG_J_FILE]
    call fsync@PLT
    test eax,eax
    jnz .Lopen_fail
    mov edi,[rbx+PG_J_DIRECTORY]
    call fsync@PLT
    test eax,eax
    jnz .Lopen_fail
    mov rax,[rsp+PG_JO_CONTROL+PG_SC_SEQ]
    mov [rbx+PG_J_SEQUENCE],rax
    mov [rbx+PG_J_END],rbp
    mov [r13+PG_SC_SEQ],rax
    lea rsi,[rsp+PG_JO_CONTROL+PG_SC_RUN]
    lea rdi,[r13+PG_SC_RUN]
    mov ecx,5
    rep movsq
    mov rax,[rsp+PG_JO_CONTROL+PG_SC_PROFILE_SIZE]
    mov [r13+PG_SC_PROFILE_SIZE],rax
    lea rsi,[rsp+PG_JO_CONTROL+PG_SC_PROFILE]
    lea rdi,[r13+PG_SC_PROFILE]
    mov ecx,512
    rep movsq
    lea rsi,[rsp+PG_JO_CONTROL+PG_SC_SUB_COUNT]
    lea rdi,[r13+PG_SC_SUB_COUNT]
    mov ecx,(PG_SC_BYTES-PG_SC_SUB_COUNT)/8
    rep movsq
    mov [r13],rbx
    lea rax,[rip+pg_journal_commit]
    mov [r13+8],rax
    xor eax,eax
    jmp .Lopen_return
.Lopen_fail:
    mov rdi,rbx
    call pg_journal_close
    mov rdi,r12
    xor eax,eax
    mov ecx,PG_S_BYTES/8
    rep stosq
.Lopen_reject:
    mov eax,1
.Lopen_return:
    add rsp,PG_J_OPEN_STACK
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    ret
.size pg_journal_open,.-pg_journal_open
.section .note.GNU-stack,"",@progbits
