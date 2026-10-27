/* test_tui_abi.c — 汇编实现 ABI/边界专项测试。
 *
 * 覆盖（汇编访存不受 sanitizer 完整保护，故用独立手段）：
 *  1. sigaction/布局常量编译期断言（tui_app.S 使用的平台结构偏移）；
 *  2. ABI 探针：调用前主动破坏 caller-saved 寄存器、置 callee-saved 哨兵，
 *     调用后核对被调用方保存寄存器；调用点带栈金丝雀；
 *  3. 汇编→C 回调的栈对齐检查（sink 回调入口 rsp≡8 mod 16）；
 *  4. 保护页边界：缓冲区尾紧贴 PROT_NONE 页，覆盖满容量/拒绝/回扫/截断；
 *  5. 零长度与上限用例。
 */
#define _GNU_SOURCE /* struct sigaction.sa_restorer 偏移断言需要 */

#include "test_util.h"

#include <signal.h>
#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>

#include "tui_app.h"
#include "tui_input.h"
#include "tui_layout.h"
#include "tui_state.h"
#include "tui_term.h"
#include "tui_utf8.h"
#include "tui_view.h"

/* ---- tui_app.S 使用的 glibc struct sigaction 布局断言 ---- */
TUI_ASSERT_OFF(struct sigaction, sa_handler, TUI_LI_SA_HANDLER);
TUI_ASSERT_OFF(struct sigaction, sa_mask, TUI_LI_SA_MASK);
TUI_ASSERT_OFF(struct sigaction, sa_flags, TUI_LI_SA_FLAGS);
TUI_ASSERT_OFF(struct sigaction, sa_restorer, TUI_LI_SA_RESTORER);
TUI_ASSERT_SIZE(struct sigaction, TUI_LI_SIGACTION_SIZE);

/* ---- ABI 探针（文件级手写汇编）----
 * 约定：先保存 fn 与参数到栈槽，再以垃圾值破坏全部 caller-saved 寄存器、
 * 以哨兵装载 callee-saved，最后从栈槽装载参数发起调用；调用后把观测到的
 * callee-saved 值写入 abi_saved[] 由 C 侧核对。保存寄存器之外另留32字节参数区，调用点保持16字节对齐。 */
__asm__(
".intel_syntax noprefix\n"
".text\n"
".type abi_align_sink,@function\n"
"abi_align_sink:\n"              /* SysV sink.write(ctx, data, len) */
" mov rax, rsp\n"                /* 入口 rsp 应 ≡ 8 (mod 16) */
" and rax, 15\n"
" mov qword ptr [rdi], rax\n"
" xor eax, eax\n"
" ret\n"
".size abi_align_sink, .-abi_align_sink\n"
".type abi_probe1,@function\n"   /* unsigned long abi_probe1(fn, a1) */
"abi_probe1:\n"
" push rbx\n push r12\n push r13\n push r14\n push r15\n sub rsp,32\n"
" mov qword ptr [rsp], rdi\n"
" mov qword ptr [rsp+8], rsi\n"
" mov rbx, 0x0B0B0001\n mov r12, 0x0B0B0002\n mov r13, 0x0B0B0003\n"
" mov r14, 0x0B0B0004\n mov r15, 0x0B0B0005\n"
" mov rcx, 0x0DEAD001\n mov rdx, 0x0DEAD002\n mov rsi, 0x0DEAD003\n"
" mov rdi, 0x0DEAD00A\n mov r8, 0x0DEAD004\n mov r9, 0x0DEAD005\n"
" mov r10, 0x0DEAD006\n mov r11, 0x0DEAD007\n mov rax, 0x0DEAD008\n"
" mov rdi, qword ptr [rsp+8]\n"
" call qword ptr [rsp]\n"
" mov qword ptr [rip+abi_saved+0], rbx\n"
" mov qword ptr [rip+abi_saved+8], r12\n"
" mov qword ptr [rip+abi_saved+16], r13\n"
" mov qword ptr [rip+abi_saved+24], r14\n"
" mov qword ptr [rip+abi_saved+32], r15\n"
" add rsp,32\n pop r15\n pop r14\n pop r13\n pop r12\n pop rbx\n"
" ret\n"
".size abi_probe1, .-abi_probe1\n"
".type abi_probe2,@function\n"   /* unsigned long abi_probe2(fn, a1, a2) */
"abi_probe2:\n"
" push rbx\n push r12\n push r13\n push r14\n push r15\n sub rsp,32\n"
" mov qword ptr [rsp], rdi\n"
" mov qword ptr [rsp+8], rsi\n"
" mov qword ptr [rsp+16], rdx\n"
" mov rbx, 0x0B0B0001\n mov r12, 0x0B0B0002\n mov r13, 0x0B0B0003\n"
" mov r14, 0x0B0B0004\n mov r15, 0x0B0B0005\n"
" mov rcx, 0x0DEAD001\n mov rdx, 0x0DEAD002\n mov rsi, 0x0DEAD003\n"
" mov rdi, 0x0DEAD00A\n mov r8, 0x0DEAD004\n mov r9, 0x0DEAD005\n"
" mov r10, 0x0DEAD006\n mov r11, 0x0DEAD007\n mov rax, 0x0DEAD008\n"
" mov rdi, qword ptr [rsp+8]\n"
" mov rsi, qword ptr [rsp+16]\n"
" call qword ptr [rsp]\n"
" mov qword ptr [rip+abi_saved+0], rbx\n"
" mov qword ptr [rip+abi_saved+8], r12\n"
" mov qword ptr [rip+abi_saved+16], r13\n"
" mov qword ptr [rip+abi_saved+24], r14\n"
" mov qword ptr [rip+abi_saved+32], r15\n"
" add rsp,32\n pop r15\n pop r14\n pop r13\n pop r12\n pop rbx\n"
" ret\n"
".size abi_probe2, .-abi_probe2\n"
".type abi_probe3,@function\n"   /* unsigned long abi_probe3(fn, a1, a2, a3) */
"abi_probe3:\n"
" push rbx\n push r12\n push r13\n push r14\n push r15\n sub rsp,32\n"
" mov qword ptr [rsp], rdi\n"
" mov qword ptr [rsp+8], rsi\n"
" mov qword ptr [rsp+16], rdx\n"
" mov qword ptr [rsp+24], rcx\n"
" mov rbx, 0x0B0B0001\n mov r12, 0x0B0B0002\n mov r13, 0x0B0B0003\n"
" mov r14, 0x0B0B0004\n mov r15, 0x0B0B0005\n"
" mov rcx, 0x0DEAD001\n mov rdx, 0x0DEAD002\n mov rsi, 0x0DEAD003\n"
" mov rdi, 0x0DEAD00A\n mov r8, 0x0DEAD004\n mov r9, 0x0DEAD005\n"
" mov r10, 0x0DEAD006\n mov r11, 0x0DEAD007\n mov rax, 0x0DEAD008\n"
" mov rdi, qword ptr [rsp+8]\n"
" mov rsi, qword ptr [rsp+16]\n"
" mov rdx, qword ptr [rsp+24]\n"
" call qword ptr [rsp]\n"
" mov qword ptr [rip+abi_saved+0], rbx\n"
" mov qword ptr [rip+abi_saved+8], r12\n"
" mov qword ptr [rip+abi_saved+16], r13\n"
" mov qword ptr [rip+abi_saved+24], r14\n"
" mov qword ptr [rip+abi_saved+32], r15\n"
" add rsp,32\n pop r15\n pop r14\n pop r13\n pop r12\n pop rbx\n"
" ret\n"
".size abi_probe3, .-abi_probe3\n"
".type abi_probe_identity,@function\n"
"abi_probe_identity:\n"
" xor eax,eax\n"
" ret\n"
".type abi_probe_selfcheck,@function\n"
"abi_probe_selfcheck:\n"
" push rbp\n"
" push rbx\n"
" push r12\n"
" push r13\n"
" push r14\n"
" push r15\n"
" sub rsp,8\n"
" mov ebx,1\n"
" mov r12d,2\n"
" mov r13d,3\n"
" mov r14d,4\n"
" mov r15d,5\n"
" mov ebp,6\n"
" lea rdi,[rip+abi_probe_identity]\n"
" xor esi,esi\n"
" call abi_probe1\n"
" lea rdi,[rip+abi_probe_identity]\n"
" xor esi,esi\n"
" xor edx,edx\n"
" call abi_probe2\n"
" lea rdi,[rip+abi_probe_identity]\n"
" xor esi,esi\n"
" xor edx,edx\n"
" xor ecx,ecx\n"
" call abi_probe3\n"
" mov eax,1\n"
" cmp rbx,1\n"
" jne 1f\n"
" cmp r12,2\n"
" jne 1f\n"
" cmp r13,3\n"
" jne 1f\n"
" cmp r14,4\n"
" jne 1f\n"
" cmp r15,5\n"
" jne 1f\n"
" cmp rbp,6\n"
" jne 1f\n"
" xor eax,eax\n"
"1:\n"
" add rsp,8\n"
" pop r15\n"
" pop r14\n"
" pop r13\n"
" pop r12\n"
" pop rbx\n"
" pop rbp\n"
" ret\n"
".size abi_probe_selfcheck,.-abi_probe_selfcheck\n"
".section .bss\n"
".align 8\n"
"abi_saved:\n"
".zero 40\n"
".section .text\n"
".att_syntax\n"
);

extern const unsigned long abi_saved[5];
typedef unsigned long (*abi_fn1)(unsigned long);
typedef unsigned long (*abi_fn2)(unsigned long, unsigned long);
typedef unsigned long (*abi_fn3)(unsigned long, unsigned long, unsigned long);
extern unsigned long abi_probe1(abi_fn1 fn, unsigned long a1);
extern unsigned long abi_probe2(abi_fn2 fn, unsigned long a1, unsigned long a2);
extern unsigned long abi_probe3(abi_fn3 fn, unsigned long a1, unsigned long a2, unsigned long a3);
extern unsigned long abi_probe_selfcheck(void);
extern void abi_align_sink(void); /* 特殊签名见上 */

static const unsigned long abi_sentinels[5] = {
	0x0B0B0001UL, 0x0B0B0002UL, 0x0B0B0003UL, 0x0B0B0004UL, 0x0B0B0005UL
};

static void check_sentinels(const char *fn_name)
{
	int i;

	for (i = 0; i < 5; i++)
		T_CHECK(abi_saved[i] == abi_sentinels[i]);
	(void)fn_name;
}

/* 调用点栈金丝雀：探测被调用方冲入调用方栈 */
#define ABI_CANARY_DECL volatile unsigned long can_a_ = 0xA11CEUL, can_b_ = 0xB0B5EUL
#define ABI_CANARY_CHECK()                            \
	do {                                          \
		T_CHECK(can_a_ == 0xA11CEUL);         \
		T_CHECK(can_b_ == 0xB0B5EUL);         \
	} while (0)

static void test_abi_utf8(void)
{
	char buf[8];
	ABI_CANARY_DECL;

	memcpy(buf, "a\xe4\xb8\xad\xf0\x9f", 6);
	T_CHECK(abi_probe1((abi_fn1)tui_utf8_seq_len, 0xE4) == 3);
	check_sentinels("seq_len");
	ABI_CANARY_CHECK();
	T_CHECK(abi_probe2((abi_fn2)tui_utf8_last_seq_start, (unsigned long)buf, 5) == 4); /* F0 是首字节 */
	check_sentinels("last_seq_start");
	ABI_CANARY_CHECK();
	T_CHECK(abi_probe2((abi_fn2)tui_utf8_incomplete_tail, (unsigned long)buf, 6) == 2); /* F0 9f 前缀 */
	check_sentinels("incomplete_tail");
	ABI_CANARY_CHECK();
	T_CHECK(abi_probe3((abi_fn3)tui_utf8_truncate_len, (unsigned long)buf, 6, 4) == 4);
	check_sentinels("truncate_len");
	ABI_CANARY_CHECK();
	T_RUN("abi_utf8");
}

static void test_abi_input(void)
{
	tui_input in;
	ABI_CANARY_DECL;

	(void)abi_probe1((abi_fn1)tui_input_init, (unsigned long)&in);
	check_sentinels("input_init");
	ABI_CANARY_CHECK();
	T_CHECK(abi_probe2((abi_fn2)tui_input_feed, (unsigned long)&in, 'h') == TUI_KEY_CHAR);
	check_sentinels("feed");
	ABI_CANARY_CHECK();
	T_CHECK(abi_probe1((abi_fn1)tui_input_len, (unsigned long)&in) == 1);
	check_sentinels("input_len");
	T_CHECK(abi_probe1((abi_fn1)tui_input_empty, (unsigned long)&in) == 0);
	check_sentinels("input_empty");
	T_CHECK(abi_probe1((abi_fn1)tui_input_text, (unsigned long)&in) == (unsigned long)&in);
	check_sentinels("input_text");
	T_CHECK(abi_probe2((abi_fn2)tui_input_feed, (unsigned long)&in, '\r') == TUI_KEY_ENTER);
	T_CHECK(tui_input_len(&in) == 1); /* 提交不改动内容 */
	(void)abi_probe1((abi_fn1)tui_input_reset, (unsigned long)&in);
	check_sentinels("input_reset");
	T_CHECK(tui_input_len(&in) == 0);
	T_RUN("abi_input");
}

/* 最小 mock ops：state_submit/cancel/poll ABI 探针需要真实函数表 */
static tui_submit_result mock_submit(void *ctx, const char *t, size_t n)
{
	(void)ctx; (void)t; (void)n;
	return TUI_SUBMIT_OK;
}
static int mock_cancel(void *ctx) { (void)ctx; return 1; }
static int mock_poll(void *ctx, tui_event *out)
{
	(void)ctx; (void)out;
	return 0;
}
static tui_runtime_ops abi_ops = { "abi-mock", NULL, mock_submit, mock_cancel, mock_poll, NULL, NULL };

static void test_abi_state(void)
{
	tui_view v;
	tui_state s;
	tui_input in;
	tstr out;
	ABI_CANARY_DECL;

	tui_input_init(&in);
	tui_view_init(&v, tstr_sink(&out), 0, &in);
	tstr_clear(&out);
	(void)abi_probe3((abi_fn3)tui_state_init, (unsigned long)&s, (unsigned long)&abi_ops, (unsigned long)&v);
	check_sentinels("state_init");
	ABI_CANARY_CHECK();
	T_CHECK(abi_probe3((abi_fn3)tui_state_submit, (unsigned long)&s, (unsigned long)"hi", 2) == TUI_SUBMIT_OK);
	check_sentinels("state_submit");
	ABI_CANARY_CHECK();
	T_CHECK(tui_state_running(&s) == 1);
	T_CHECK(abi_probe1((abi_fn1)tui_state_running, (unsigned long)&s) == 1);
	check_sentinels("state_running");
	T_CHECK(abi_probe1((abi_fn1)tui_state_fatal, (unsigned long)&s) == 0);
	check_sentinels("state_fatal");
	T_CHECK(abi_probe1((abi_fn1)tui_state_cancel, (unsigned long)&s) == 1);
	check_sentinels("state_cancel");
	ABI_CANARY_CHECK();
	(void)abi_probe1((abi_fn1)tui_state_poll, (unsigned long)&s);
	check_sentinels("state_poll");
	ABI_CANARY_CHECK();
	T_CHECK(abi_probe1((abi_fn1)tui_state_running, (unsigned long)&s) == 1); /* poll 无事件不补造终态 */
	T_RUN("abi_state");
}

static void test_abi_view_term(void)
{
	tui_view v;
	tui_input in;
	tui_term t;
	tstr out;
	tui_event ev;
	ABI_CANARY_DECL;

	tui_input_init(&in);
	tui_view_init(&v, tstr_sink(&out), 1, &in);
	tstr_clear(&out);
	T_CHECK(v.ansi == 1 && v.editor == &in);
	(void)abi_probe1((abi_fn1)tui_view_redraw_editor, (unsigned long)&v);
	check_sentinels("view_redraw");
	ABI_CANARY_CHECK();
	(void)abi_probe1((abi_fn1)tui_view_finalize_editor, (unsigned long)&v);
	check_sentinels("view_finalize");
	memset(&ev, 0, sizeof(ev));
	tui_view_event(&v, &ev); /* kind=0：无输出（保护页用例另测 kind=2） */
	T_CHECK(out.len > 0);    /* redraw 已产生 "> " */
	tstr_clear(&out);
	memset(&ev, 0, sizeof(ev)); /* kind=0：未知事件，无输出（rsi 必须是合法 ev） */
	(void)abi_probe2((abi_fn2)tui_view_event, (unsigned long)&v, (unsigned long)&ev);
	check_sentinels("view_event_unknown");
	T_CHECK(out.len == 0);
	ABI_CANARY_CHECK();

	(void)abi_probe3((abi_fn3)tui_term_init, (unsigned long)&t, 0, 1);
	check_sentinels("term_init");
	ABI_CANARY_CHECK();
	T_CHECK(t.in_fd == 0 && t.out_fd == 1);
	T_CHECK(abi_probe1((abi_fn1)tui_term_is_interactive, (unsigned long)&t) == 0);
	check_sentinels("term_is_interactive");
	/* fd=-1：EBADF 立即失败路径（写循环不推进、不崩溃） */
	t.out_fd = -1;
	T_CHECK((unsigned)abi_probe3((abi_fn3)tui_term_write, (unsigned long)&t, (unsigned long)"x", 1) == (unsigned)-1); /* int 返回零扩展 */
	check_sentinels("term_write");
	ABI_CANARY_CHECK();
	T_CHECK(strcmp(tui_exit_reason_name(TUI_EXIT_EOF), "EOF") == 0);
	T_CHECK(strcmp(tui_exit_reason_name(TUI_EXIT_SIGNAL), "收到终止信号") == 0);
	T_CHECK(strcmp(tui_exit_reason_name(99), "未知") == 0);
	T_RUN("abi_view_term");
}

/* ---- 汇编→C 回调栈对齐：sink 入口 rsp ≡ 8 (mod 16) ---- */
static int counting_write(void *ctx, const char *data, size_t len)
{
	(void)data;
	*(size_t *)ctx += len;
	return 0;
}

static void test_callback_alignment(void)
{
	tui_view v;
	tui_input in;
	tui_event ev;
	size_t written = 0;
	unsigned long align_at_entry = 0xFFFF;
	tui_out_sink sink;

	tui_input_init(&in);
	sink.ctx = &align_at_entry;
	sink.write = (int (*)(void *, const char *, size_t))abi_align_sink;
	tui_view_init(&v, sink, 1, &in);

	memset(&ev, 0, sizeof(ev));
	ev.kind = TUI_EV_RUN_STARTED;
	tui_view_event(&v, &ev); /* 触发 vputs → sink.write */
	T_CHECK(align_at_entry == 8); /* 入口 rsp ≡ 8 (mod 16) */

	/* 计数 sink：零长度 text_delta 不得产生写 */
	sink.ctx = &written;
	sink.write = counting_write;
	tui_view_init(&v, sink, 0, NULL);
	ev.kind = TUI_EV_TEXT_DELTA;
	ev.text = "x";
	ev.text_len = 0;
	tui_view_event(&v, &ev);
	T_CHECK(written == 0);
	T_RUN("callback_alignment");
}

__asm__(
".intel_syntax noprefix\n"
".type abi_history_callback,@function\n"
"abi_history_callback:\n"
" mov rax,rsp\n and eax,15\n cmp eax,8\n je 1f\n ud2\n"
"1:\n sub rsp,8\n call abi_history_fixture\n add rsp,8\n"
" mov rcx,0xcccccccc\n mov rdx,0xdddddddd\n mov rsi,0x51515151\n mov rdi,0xd1d1d1d1\n"
" mov r8,0x88888888\n mov r9,0x99999999\n mov r10,0xaaaaaaaa\n mov r11,0xbbbbbbbb\n ret\n"
".size abi_history_callback,.-abi_history_callback\n"
".att_syntax prefix\n");
int abi_history_callback(void *,size_t,tui_history_entry *);
int abi_history_fixture(void *ctx,size_t index,tui_history_entry *entry){(void)ctx;if(index)return 0;*entry=(tui_history_entry){.text="控制\x1b]52;bad\a",.text_len=sizeof("控制\x1b]52;bad\a")-1};return 1;}
static void test_abi_history(void){tui_view view;tui_state state;tstr out;tstr_clear(&out);tui_view_init(&view,tstr_sink(&out),0,NULL);tui_runtime_ops ops=abi_ops;ops.history=abi_history_callback;tui_state_init(&state,&ops,&view);T_CHECK(abi_probe1((abi_fn1)tui_state_history,(unsigned long)&state)==1);check_sentinels("history callback");T_CHECK(tstr_contains(&out,"[history] user:"));tui_history_entry entry={.kind=1,.text="\x1b]52;bad\a",.text_len=9};(void)abi_probe2((abi_fn2)tui_view_history,(unsigned long)&view,(unsigned long)&entry);check_sentinels("history view");T_RUN("abi_history_callback_clobbers");}

/* ---- 保护页边界 ---- */
static char *guard_alloc(size_t usable)
{
	size_t pg = 4096;
	char *base = mmap(NULL, 2 * pg, PROT_READ | PROT_WRITE,
			  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	T_CHECK(base != MAP_FAILED);
	T_CHECK(mprotect(base + pg, pg, PROT_NONE) == 0);
	return base + pg - usable; /* usable 字节止于保护页边界 */
}

static void test_guard_utf8(void)
{
	/* 各长度尾部紧贴保护页：回扫/尾测/截断只允许读 [0,len) */
	static const size_t lens[] = { 1, 2, 3, 4, 5, 6, 7 };
	size_t i;
	ABI_CANARY_DECL;

	for (i = 0; i < sizeof(lens) / sizeof(lens[0]); i++) {
		size_t len = lens[i];
		char *b = guard_alloc(len);

		memset(b, 0x61, len);
		b[0] = (char)0xE6; /* 首字节造 3 字节序列，末尾可能出现不完整前缀 */
		(void)tui_utf8_last_seq_start(b, len);
		(void)tui_utf8_incomplete_tail(b, len);
		(void)tui_utf8_truncate_len(b, len, len + 8);
		(void)tui_utf8_truncate_len(b, len, len - 1);
		(void)tui_utf8_last_seq_start(b, 0); /* 零长度 */
		(void)tui_utf8_incomplete_tail(b, 0);
		(void)tui_utf8_truncate_len(b, 0, 0);
	}
	T_RUN("guard_utf8");
	ABI_CANARY_CHECK();
}

static void test_guard_input_full(void)
{
	/* 结构整体贴保护页：任何越过 sizeof(tui_input) 的写触发 SIGSEGV */
	tui_input *in = (tui_input *)guard_alloc(sizeof(tui_input));
	unsigned i;

	tui_input_init(in);
	for (i = 0; i < TUI_INPUT_MAX_BYTES - 1; i++)
		T_CHECK(tui_input_feed(in, 'a') == TUI_KEY_CHAR);
	T_CHECK(tui_input_len(in) == TUI_INPUT_MAX_BYTES - 1); /* 末字节保留 NUL */
	/* 满（有效上限 1023）后拒绝（整字符），节流，不越界写 */
	T_CHECK(tui_input_feed(in, 'b') == TUI_KEY_OVERFLOW);
	T_CHECK(tui_input_feed(in, 'c') == TUI_KEY_IGNORED);
	T_CHECK(tui_input_len(in) == TUI_INPUT_MAX_BYTES - 1);
	/* 满 + 多字节首字节整字符拒绝路径 */
	T_CHECK(tui_input_feed(in, 0xE6) == TUI_KEY_IGNORED); /* 节流后不再报 */
	T_CHECK(tui_input_feed(in, 0xBC) == TUI_KEY_IGNORED);
	T_CHECK(tui_input_feed(in, 0x94) == TUI_KEY_IGNORED);
	T_CHECK(tui_input_len(in) == TUI_INPUT_MAX_BYTES - 1);
	/* 回退与清行 */
	T_CHECK(tui_input_feed(in, 0x7F) == TUI_KEY_BACKSPACE);
	T_CHECK(tui_input_len(in) == TUI_INPUT_MAX_BYTES - 2);
	T_CHECK(tui_input_feed(in, 0x15) == TUI_KEY_CHAR);
	T_CHECK(tui_input_len(in) == 0);
	T_RUN("guard_input_full");
}

static void test_guard_view_text(void)
{
	/* text 尾部贴保护页：text_delta 只允许读 [0, text_len) */
	tstr out;
	tui_view v;
	tui_input in;
	tui_event ev;
	char *b = guard_alloc(8);

	b[0] = 'a';
	b[1] = 'b';
	b[2] = 'c';
	b[3] = (char)0xE6;
	b[4] = (char)0xBC;
	b[5] = (char)0x94;
	b[6] = (char)0xF0;
	b[7] = (char)0x9F;
	tui_input_init(&in);
	tui_view_init(&v, tstr_sink(&out), 0, NULL);
	tstr_clear(&out);
	memset(&ev, 0, sizeof(ev));
	ev.kind = TUI_EV_TEXT_DELTA;
	ev.text = b + 3;
	ev.text_len = 5; /* e6 bc 94 完整 + f0 9f 两字节前缀 */
	tui_view_event(&v, &ev);
	/* md 行缓冲：无换行不输出，完整与不完整序列都在行内 */
	T_CHECK(out.len == 0);
	T_CHECK(v.md.line_len == 5);
	T_CHECK(v.carry_len == 0);
	memset(&ev, 0, sizeof(ev));
	ev.kind = TUI_EV_TEXT_DELTA;
	ev.text = "\n";
	ev.text_len = 1;
	tui_view_event(&v, &ev);
	T_CHECK(find_bytes(out.buf, out.len, "\xe6\xbc\x94", 3) != NULL); /* 完整序列已写出 */
	T_CHECK(find_bytes(out.buf, out.len, "\xf0\x9f", 2) == NULL);     /* 不完整前缀转义文本，不透传 */
	T_RUN("guard_view_text");
}

int main(void)
{
	T_CHECK(abi_probe_selfcheck()==0);
	test_abi_utf8();
	test_abi_input();
	test_abi_state();
	test_abi_view_term();
	test_callback_alignment();
    test_abi_history();
	test_guard_utf8();
	test_guard_input_full();
	test_guard_view_text();
	printf("test_tui_abi: all passed\n");
	return 0;
}
