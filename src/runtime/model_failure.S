.intel_syntax noprefix
.text
.globl pg_model_retryable
.type pg_model_retryable,@function
pg_model_retryable:
 push rbp
 push rbx
 push r12
 push r13
 sub rsp,8
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 mov ebp,ecx
 lea rdx,[rip+.Lmf_excluded]
 call pg_model_regex@PLT
 test eax,eax
 jnz .Lmf_no
 cmp r13,413
 je .Lmf_no
 mov rdi,rbx
 mov rsi,r12
 lea rdx,[rip+.Lmf_overflow_exempt]
 call pg_model_regex@PLT
 test eax,eax
 js .Lmf_no
 jnz .Lmf_classify_transient
 mov rdi,rbx
 mov rsi,r12
 lea rdx,[rip+.Lmf_overflow]
 call pg_model_regex@PLT
 test eax,eax
 jnz .Lmf_no
.Lmf_classify_transient:
 mov rdi,rbx
 mov rsi,r12
 lea rdx,[rip+.Lmf_transient]
 call pg_model_regex@PLT
 test eax,eax
 js .Lmf_no
 jnz .Lmf_yes
 cmp r13,429
 je .Lmf_yes
 cmp r13,500
 je .Lmf_yes
 cmp r13,502
 je .Lmf_yes
 cmp r13,503
 je .Lmf_yes
 cmp r13,504
 je .Lmf_yes
 cmp r13,520
 je .Lmf_yes
 cmp r13,524
 je .Lmf_yes
 /* libcurl resolve/connect/partial/timeout/send/receive/HTTP2 failures. */
 lea rdx,[rip+.Lmf_transport]
 xor ecx,ecx
.Lmf_transport_loop:
 cmp ecx,10
 jae .Lmf_no
 cmp ebp,[rdx+rcx*4]
 je .Lmf_yes
 inc ecx
 jmp .Lmf_transport_loop
.Lmf_no:
 xor eax,eax
 jmp .Lmf_end
.Lmf_yes:
 mov eax,1
.Lmf_end:
 add rsp,8
 pop r13
 pop r12
 pop rbx
 pop rbp
 ret
.size pg_model_retryable,.-pg_model_retryable
.text
.globl pg_model_context_overflow
.type pg_model_context_overflow,@function
pg_model_context_overflow:
 push rbx
 push r12
 push r13
 mov rbx,rdi
 mov r12,rsi
 mov r13,rdx
 lea rdx,[rip+.Lmf_overflow_exempt]
 call pg_model_regex@PLT
 test eax,eax
 jnz .Lmf_context_no
 cmp r13,413
 je .Lmf_context_yes
 mov rdi,rbx
 mov rsi,r12
 lea rdx,[rip+.Lmf_overflow]
 call pg_model_regex@PLT
 test eax,eax
 jg .Lmf_context_yes
.Lmf_context_no:
 xor eax,eax
 jmp .Lmf_context_end
.Lmf_context_yes:
 mov eax,1
.Lmf_context_end:
 pop r13
 pop r12
 pop rbx
 ret
.size pg_model_context_overflow,.-pg_model_context_overflow
.section .rodata
.Lmf_overflow_exempt: .asciz "^(Throttling error|Service unavailable):|rate limit|too many requests"
.Lmf_overflow: .asciz "prompt (is )?too long|prompt exceeds max length|request_too_large|input is too long for requested model|exceeds the context window|exceeds (the )?(model.?s )?maximum context length|input token count.*exceeds the maximum|maximum prompt length is [0-9]+|reduce the length of the messages|maximum context length is [0-9]+ tokens|exceeds (the )?maximum allowed input length|input \\([0-9]+ tokens\\) is longer than the model.?s context length|exceeds the limit of [0-9]+|exceeds the available context size|greater than the context length|context window exceeds limit|request exceeded model token limit|configured context size is|too large for model with|Range of input length should be|context[_ ]length[_ ]exceeded|too many tokens|token limit exceeded"
.Lmf_transport: .long 5,6,7,16,18,28,52,55,56,92
/* Pi pi-ai/utils/retry.ts, POSIX ERE spellings for JS boundaries/groups. */
.Lmf_excluded: .asciz "GoUsageLimitError|FreeUsageLimitError|Monthly usage limit reached|available balance|insufficient_quota|out of budget|quota exceeded|billing|subscription_sharing_usage_limit_exceeded|subscription_sharing_workplace_individual_usage_limit_exceeded"
.Lmf_transient: .asciz "overloaded|server_busy|servers are currently busy|currently experiencing high demand|model is at capacity|rate.?limit|too many requests|(^|[^a-z0-9])(429|500|502|503|504|520|524)([^a-z0-9]|$)|service.?(is.?)?(temporarily.?)?unavailable|server.?error|internal.?error|provider.?returned.?(an.?)?error|exceeded request buffer limit while retrying upstream|unable to forward upstream|upstream.?request.?failed|upstream_stream_read_error|stream was interrupted|http/?2 stream failed|network.?error|connection.?error|connection.?refused|connection.?lost|other side closed|fetch failed|request to [^[:space:]]+ failed, reason:|getaddrinfo|ENOTFOUND|EAI_AGAIN|upstream.?connect|reset before headers|socket hang up|socket connection was closed|timed? out|timeout|terminated|websocket.?closed|websocket.?error|ended without|stream ended before message_stop|stream ended before a terminal response event|http2 request did not get a response|stream disconnected before completion|pending stream has been canceled|retry delay|you can retry your request|try your request again|please retry your request|ResourceExhausted|subscription_sharing_usage_unavailable|subscription_sharing_user_unavailable"
.section .note.GNU-stack,"",@progbits
