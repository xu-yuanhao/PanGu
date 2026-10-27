.intel_syntax noprefix
.text
/* Execute one SQL statement; SQL/library details stay inside this provider. */
.Lsql_exec:
 xor edx,edx
 xor ecx,ecx
 xor r8d,r8d
 jmp sqlite3_exec@PLT
.Lsql_adopt:
 ret

.globl pg_record_sqlite_storage
.type pg_record_sqlite_storage,@function
pg_record_sqlite_storage:
 mov rax,rdi
 lea rdx,[rip+.Lsql_commit]
 ret
.size pg_record_sqlite_storage,.-pg_record_sqlite_storage

.Lsql_commit:
 test rdi,rdi
 jz .Lsql_commit_invalid
 cmp dword ptr [rdi+56],0
 jne .Lsql_commit_invalid
 cmp dword ptr [rdi+60],0
 jne .Lsql_commit_invalid
 cmp dword ptr [rdi+64],0
 jne .Lsql_commit_invalid
 cmp dword ptr [rdi+68],0
 jne .Lsql_commit_invalid
 cmp qword ptr [rdi+48],0
 je .Lsql_commit_invalid
 push rbx
 mov rbx,rdi
 mov dword ptr [rbx+56],1
 sub rsp,16
 mov [rsp],rdi
 lea r9,[rip+.Lsql_persist]
 call pg_record_memory_commit_persisted@PLT
 add rsp,16
 mov dword ptr [rbx+56],0
 pop rbx
 ret
.Lsql_commit_invalid:
 test r8,r8
 jz .Lsql_commit_bad_return
 mov qword ptr [r8],0
.Lsql_commit_bad_return:
 mov eax,2
 ret

/* One atomic log append under SQLite's write lock. Unique/contiguous sequence
   rejects concurrent owners before either caller can adopt stale state. */
.Lsql_persist:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 sub rsp,16
 mov rbx,rdi
 mov r12,rdx
 mov rdi,rsi
 mov esi,0
 lea rdx,[rsp+8]
 call json_object_to_json_string_length@PLT
 test rax,rax
 jz .Lsql_persist_fail
 mov r13,rax
 mov rdi,[rbx+48]
 lea rsi,[rip+.Lsql_begin]
 call .Lsql_exec
 test eax,eax
 jnz .Lsql_persist_fail
 mov qword ptr [rsp],0
 mov rdi,[rbx+48]
 lea rsi,[rip+.Lsql_insert]
 mov edx,-1
 mov rcx,rsp
 xor r8d,r8d
 call sqlite3_prepare_v2@PLT
 test eax,eax
 jnz .Lsql_persist_rollback
 mov rdi,[rsp]
 mov esi,1
 mov rdx,r12
 call sqlite3_bind_int64@PLT
 test eax,eax
 jnz .Lsql_persist_finalize
 mov rdi,[rsp]
 mov esi,2
 mov rdx,r13
 mov ecx,-1
 mov r8,-1
 call sqlite3_bind_text@PLT
 test eax,eax
 jnz .Lsql_persist_finalize
 mov rdi,[rsp]
 call sqlite3_step@PLT
 cmp eax,101
 jne .Lsql_persist_finalize
 mov rdi,[rbx+48]
 call sqlite3_changes@PLT
 cmp eax,1
 jne .Lsql_persist_finalize
 mov rdi,[rsp]
 call sqlite3_finalize@PLT
 mov qword ptr [rsp],0
 test eax,eax
 jnz .Lsql_persist_rollback
 mov rdi,[rbx+48]
 lea rsi,[rip+.Lsql_commit_sql]
 call .Lsql_exec
 test eax,eax
 jnz .Lsql_persist_rollback
 xor eax,eax
 jmp .Lsql_persist_return
.Lsql_persist_finalize:
 mov rdi,[rsp]
 call sqlite3_finalize@PLT
 mov qword ptr [rsp],0
.Lsql_persist_rollback:
 mov rdi,[rbx+48]
 lea rsi,[rip+.Lsql_rollback]
 call .Lsql_exec
.Lsql_persist_fail:
 mov dword ptr [rbx+68],1
 mov eax,1
.Lsql_persist_return:
 add rsp,16
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret

.globl pg_record_sqlite_open
.type pg_record_sqlite_open,@function
pg_record_sqlite_open:
 push rbp
 push rbx
 push r12
 push r13
 push r14
 push r15
 sub rsp,40
 mov rbx,rdi
 mov r12,rsi
 mov r13d,edx
 mov qword ptr [rsp],0
 mov qword ptr [rsp+8],0
 mov qword ptr [rsp+16],0
 mov r15d,1
 test rbx,rbx
 jz .Lsql_open_return
 test r12,r12
 jz .Lsql_open_return
 cmp r13d,1
 ja .Lsql_open_return
 cmp qword ptr [rbx+48],0
 jne .Lsql_open_return
 cmp dword ptr [rbx+56],0
 jne .Lsql_open_return
 mov rdi,rbx
 call pg_record_memory_open@PLT
 test eax,eax
 jnz .Lsql_open_return
 mov dword ptr [rbx+56],1
 mov dword ptr [rbx+60],0
 mov [rbx+64],r13d
 mov dword ptr [rbx+68],0
 mov rdi,r12
 lea rsi,[rbx+48]
 mov edx,6
 test r13d,r13d
 jz .Lsql_open_flags
 mov edx,1
.Lsql_open_flags:
 or edx,0x10000
 xor ecx,ecx
 call sqlite3_open_v2@PLT
 mov r15d,2
 test eax,eax
 jnz .Lsql_open_cleanup
 test r13d,r13d
 jnz .Lsql_open_read
 mov rdi,[rbx+48]
 lea rsi,[rip+.Lsql_schema]
 call .Lsql_exec
 test eax,eax
 jnz .Lsql_open_cleanup
.Lsql_open_read:
 /* A single SELECT provides a consistent SQLite read snapshot. */
 mov rdi,[rbx+48]
 lea rsi,[rip+.Lsql_select]
 mov edx,-1
 mov rcx,rsp
 xor r8d,r8d
 call sqlite3_prepare_v2@PLT
 test eax,eax
 jnz .Lsql_open_cleanup
.Lsql_replay:
 mov rdi,[rsp]
 call sqlite3_step@PLT
 cmp eax,101
 je .Lsql_open_done
 mov r15d,3
 cmp eax,100
 jne .Lsql_open_cleanup
 mov rdi,[rsp]
 xor esi,esi
 call sqlite3_column_type@PLT
 cmp eax,1
 jne .Lsql_open_cleanup
 mov rdi,[rsp]
 xor esi,esi
 call sqlite3_column_int64@PLT
 cmp rax,[rbx+32]
 jne .Lsql_open_cleanup
 mov [rsp+24],rax
 mov rdi,[rsp]
 mov esi,1
 call sqlite3_column_type@PLT
 cmp eax,3
 jne .Lsql_open_cleanup
 mov rdi,[rsp]
 mov esi,1
 call sqlite3_column_bytes@PLT
 test eax,eax
 jle .Lsql_open_cleanup
 mov r14d,eax
 mov rdi,[rsp]
 mov esi,1
 call sqlite3_column_text@PLT
 test rax,rax
 jz .Lsql_open_alloc
 mov rbp,rax
 call json_tokener_new@PLT
 test rax,rax
 jz .Lsql_open_alloc
 mov [rsp+8],rax
 mov rdi,rax
 mov esi,0x11
 call json_tokener_set_flags@PLT
 mov rdi,[rsp+8]
 mov rsi,rbp
 mov edx,r14d
 call json_tokener_parse_ex@PLT
 mov [rsp+16],rax
 mov rdi,[rsp+8]
 call json_tokener_get_error@PLT
 test eax,eax
 jnz .Lsql_open_cleanup
 mov rdi,[rsp+8]
 call json_tokener_get_parse_end@PLT
 cmp rax,r14
 jne .Lsql_open_cleanup
 mov rdi,[rsp+8]
 call json_tokener_free@PLT
 mov qword ptr [rsp+8],0
 mov rdi,rbx
 call pg_record_memory_storage@PLT
 /* Storage descriptor return uses rax=context, rdx=commit. */
 mov rdi,rax
 mov rsi,[rsp+16]
 lea rax,[rip+.Lsql_adopt]
 mov r11,rdx
 mov rdx,rax
 xor ecx,ecx
 lea r8,[rsp+32]
 call r11
 test eax,eax
 jnz .Lsql_open_cleanup
 mov rax,[rsp+32]
 cmp rax,[rsp+24]
 jne .Lsql_open_cleanup
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov qword ptr [rsp+16],0
 jmp .Lsql_replay
.Lsql_open_alloc:
 mov r15d,4
 jmp .Lsql_open_cleanup
.Lsql_open_done:
 mov rdi,[rsp]
 call sqlite3_finalize@PLT
 mov qword ptr [rsp],0
 test eax,eax
 jnz .Lsql_open_cleanup
 mov dword ptr [rbx+56],0
 xor r15d,r15d
 jmp .Lsql_open_return
.Lsql_open_cleanup:
 mov rdi,[rsp+16]
 call json_object_put@PLT
 mov rdi,[rsp+8]
 test rdi,rdi
 jz .Lsql_open_statement
 call json_tokener_free@PLT
.Lsql_open_statement:
 mov rdi,[rsp]
 call sqlite3_finalize@PLT
 mov rdi,[rbx+48]
 call sqlite3_close_v2@PLT
 mov qword ptr [rbx+48],0
 mov rdi,rbx
 call pg_record_memory_close@PLT
 mov dword ptr [rbx+56],0
 mov dword ptr [rbx+60],1
.Lsql_open_return:
 mov eax,r15d
 add rsp,40
 pop r15
 pop r14
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_record_sqlite_open,.-pg_record_sqlite_open

.globl pg_record_sqlite_close
.type pg_record_sqlite_close,@function
pg_record_sqlite_close:
 mov eax,1
 test rdi,rdi
 jz .Lsql_close_return
 cmp dword ptr [rdi+56],0
 jne .Lsql_close_return
 cmp dword ptr [rdi+40],0
 jne .Lsql_close_return
 push rbx
 mov rbx,rdi
 mov rdi,[rbx+48]
 call sqlite3_close@PLT
 test eax,eax
 jnz .Lsql_close_io
 mov qword ptr [rbx+48],0
 mov rdi,rbx
 call pg_record_memory_close@PLT
 mov dword ptr [rbx+60],1
 xor eax,eax
 pop rbx
 ret
.Lsql_close_io:
 mov eax,2
 pop rbx
.Lsql_close_return:
 ret
.size pg_record_sqlite_close,.-pg_record_sqlite_close

.section .rodata
.Lsql_schema:
 .asciz "PRAGMA trusted_schema=OFF; PRAGMA synchronous=FULL; CREATE TABLE IF NOT EXISTS pangu_durable_commits_v1(seq INTEGER PRIMARY KEY CHECK(seq>0 AND seq<=9007199254740991), writes TEXT NOT NULL);"
.Lsql_select:
 .asciz "SELECT seq,writes FROM pangu_durable_commits_v1 ORDER BY seq"
.Lsql_begin: .asciz "BEGIN IMMEDIATE"
.Lsql_commit_sql: .asciz "COMMIT"
.Lsql_rollback: .asciz "ROLLBACK"
.Lsql_insert:
 .asciz "INSERT INTO pangu_durable_commits_v1(seq,writes) SELECT ?1,?2 WHERE ?1=(SELECT COALESCE(MAX(seq),0)+1 FROM pangu_durable_commits_v1)"
.section .note.GNU-stack,"",@progbits
