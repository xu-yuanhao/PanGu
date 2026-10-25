/* test_tools_asm.c — 工具注册表/read 工具汇编实现的 ABI 与边界专项。
 *
 * 覆盖：
 *  1. 汇编回调 ABI：validate/authorize/execute 以嵌入汇编实现，入口主动破坏
 *     全部 caller-saved 寄存器，并验证 execute 的 cap/size 栈传参
 *     （[rsp+8]/[rsp+16]）与各寄存器实参；
 *  2. ABI 探针：callee-saved 哨兵保持、聚合 sret 返回（pg_tools、
 *     pg_tool_definition）、调用点栈金丝雀；
 *  3. 注册表边界：名称 1/63/64、描述 4095/4096、16 上限、失败初始化的
 *     事务性（原 registry 不变）、空 registry、object schema 各拒绝分支；
 *  4. 参数 JSON 严格性：尾随垃圾/嵌入 NUL（显式长度切片）/非 object/空白尾随；
 *  5. read 边界：容量 0/1/2、恰好容量-1、超一字节、零字节文件、深层嵌套、
 *     空中间分量、UTF-8 组件、关闭根、保护页输出缓冲。
 */
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "read_tool_layout.h"
#include "tools.h"
#include "tools_layout.h"

/* ---- 汇编回调：入口破坏 caller-saved，验证寄存器/栈实参 ---- */
__asm__(
".intel_syntax noprefix\n"
".text\n"
".type abi_asm_validate,@function\n"
"abi_asm_validate:\n"            /* (ctx, args.data, args.size) */
" push rdi\n push rsi\n push rdx\n"
" /* 参数已存栈：现在破坏全部 caller-saved */\n"
" mov rcx, 0x0DEAD001\n mov rdx, 0x0DEAD002\n mov rsi, 0x0DEAD003\n"
" mov rdi, 0x0DEAD00A\n mov r8, 0x0DEAD004\n mov r9, 0x0DEAD005\n"
" mov r10, 0x0DEAD006\n mov r11, 0x0DEAD007\n mov rax, 0x0DEAD008\n"
" pop rcx\n pop r11\n pop r10\n" /* LIFO：rcx=size(orig rdx) r11=data r10=ctx */
" mov rdx, [rip + abi_expected_ctx]\n"
" cmp r10, rdx\n jne .Lav_bad\n"
" mov rdx, [rip + abi_expected_args]\n"
" cmp r11, rdx\n jne .Lav_bad\n"
" mov rdx, [rip + abi_expected_size]\n"
" cmp rcx, rdx\n jne .Lav_bad\n"
" mov eax, 1\n ret\n"
".Lav_bad:\n mov eax, 0\n ret\n"
".size abi_asm_validate, .-abi_asm_validate\n"
".type abi_asm_authorize,@function\n"
"abi_asm_authorize:\n"
" push rdi\n"
" mov rcx, 0x0DEAD001\n mov rdx, 0x0DEAD002\n mov rsi, 0x0DEAD003\n"
" mov rdi, 0x0DEAD00A\n mov r8, 0x0DEAD004\n mov r9, 0x0DEAD005\n"
" mov r10, 0x0DEAD006\n mov r11, 0x0DEAD007\n mov rax, 0x0DEAD008\n"
" pop r10\n"
" mov rcx, [rip + abi_expected_ctx]\n"
" cmp r10, rcx\n jne .Laa_bad\n"
" mov eax, 1\n ret\n"
".Laa_bad:\n mov eax, 0\n ret\n"
".size abi_asm_authorize, .-abi_asm_authorize\n"
".type abi_asm_execute,@function\n"
"abi_asm_execute:\n"             /* (ctx, args.data, args.size, out=rcx, cap=r8, size*=r9) */
" push rdi\n push rsi\n push rdx\n push rcx\n push r8\n push r9\n push rax\n"
" mov rcx, 0x0DEAD001\n mov rdx, 0x0DEAD002\n mov rsi, 0x0DEAD003\n"
" mov rdi, 0x0DEAD00A\n mov r8, 0x0DEAD004\n mov r9, 0x0DEAD005\n"
" mov r10, 0x0DEAD006\n mov r11, 0x0DEAD007\n mov rax, 0x0DEAD008\n"
" pop rax\n"
" pop r10\n"  /* orig r9=size* */
" pop r11\n"  /* orig r8=cap */
" pop rax\n"  /* orig rcx=out */
" pop rdx\n"  /* orig rdx=size */
" pop rsi\n"  /* orig rsi=data */
" pop rdi\n"  /* orig rdi=ctx */
" mov r9, [rip + abi_expected_sizep]\n"
" cmp r10, r9\n jne .Lax_bad\n"
" mov r9, [rip + abi_expected_cap]\n"
" cmp r11, r9\n jne .Lax_bad\n"
" mov r9, [rip + abi_expected_ctx]\n"
" cmp rdi, r9\n jne .Lax_bad\n"
" mov r9, [rip + abi_expected_out]\n"
" cmp rax, r9\n jne .Lax_bad\n"
" /* 写出标记并设置 *size=5 */\n"
" mov byte ptr [rax], 0x5A\n"
" mov byte ptr [rax + 1], 0x59\n"
" mov byte ptr [rax + 2], 0x58\n"
" mov byte ptr [rax + 3], 0x57\n"
" mov byte ptr [rax + 4], 0x56\n"
" mov qword ptr [r10], 5\n"
" xor eax, eax\n ret\n"
".Lax_bad:\n mov eax, 0x7F\n ret\n"
".size abi_asm_execute, .-abi_asm_execute\n"
".section .data\n"
".align 8\n"
"abi_expected_ctx:  .quad 0\n"
"abi_expected_args: .quad 0\n"
"abi_expected_size: .quad 0\n"
"abi_expected_cap:  .quad 0\n"
"abi_expected_sizep:.quad 0\n"
"abi_expected_out:  .quad 0\n"
".section .text\n"
".section .note.GNU-stack,\"\",@progbits\n"
".att_syntax\n"
);

extern const void *abi_expected_ctx, *abi_expected_args, *abi_expected_size;
extern const void *abi_expected_cap, *abi_expected_sizep, *abi_expected_out;
int abi_asm_validate(void *, pg_slice);
int abi_asm_authorize(void *, pg_slice);
int abi_asm_execute(void *, pg_slice, char *, size_t, size_t *);

static pg_slice s(const char *v) { return (pg_slice){v, strlen(v)}; }

/* ---- ABI 探针：callee-saved 哨兵 + caller-saved 垃圾（三参版本） ---- */
__asm__(
".intel_syntax noprefix\n"
".text\n"
".type abi_probe3,@function\n"
"abi_probe3:\n"                   /* (fn, a1, a2, a3) → fn 返回值 */
" push rbx\n push r12\n push r13\n push r14\n push r15\n"
" sub rsp, 32\n"                   /* scratch：不与保存槽重叠 */
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
" add rsp, 32\n"
" pop r15\n pop r14\n pop r13\n pop r12\n pop rbx\n"
" ret\n"
".size abi_probe3, .-abi_probe3\n"
".section .bss\n"
".align 8\n"
"abi_saved:\n"
".zero 40\n"
".section .text\n"
".section .note.GNU-stack,\"\",@progbits\n"
".att_syntax\n"
);

extern const unsigned long abi_saved[5];
typedef unsigned long (*abi_fn3)(unsigned long, unsigned long, unsigned long);
extern unsigned long abi_probe3(abi_fn3, unsigned long, unsigned long, unsigned long);

static const unsigned long sentinels[5] = {
	0x0B0B0001UL, 0x0B0B0002UL, 0x0B0B0003UL, 0x0B0B0004UL, 0x0B0B0005UL
};

static void check_sentinels(void)
{
	int i;

	for (i = 0; i < 5; i++)
		assert(abi_saved[i] == sentinels[i]);
}

static int ok_validate(void *ctx, pg_slice args) { (void)ctx; (void)args; return 1; }
static int ok_authorize(void *ctx, pg_slice args) { (void)ctx; (void)args; return 1; }
static int ok_execute(void *ctx, pg_slice args, char *out, size_t cap, size_t *size)
{
	(void)ctx; (void)args;
	assert(cap >= 4);
	memcpy(out, "done", 4);
	*size = 4;
	return 0;
}

static void test_asm_callbacks_and_stack_args(void)
{
	static const char schema[] = "{\"type\":\"object\"}";
	char out[64];
	size_t n = 0;
	pg_tool_registry r;
	pg_tools svc;
	pg_tool_definition d = {"abi", "desc", schema, (void *)0x1234,
				abi_asm_validate, abi_asm_authorize, abi_asm_execute,0,0};
	const char *args = "{\"x\":1}";
	volatile unsigned long canary_a = 0xA11CE, canary_b = 0xB0B5E;

	assert(!pg_tool_registry_init(&r, &d, 1));
	svc = pg_tool_registry_service(&r);
	abi_expected_ctx = (void *)0x1234;
	abi_expected_args = args;
	abi_expected_size = (void *)strlen(args);
	abi_expected_cap = (void *)sizeof(out);
	abi_expected_sizep = &n;
	abi_expected_out = out;
	assert(!svc.execute(svc.context, s("abi"), s(args), out, sizeof(out), &n));
	assert(n == 5 && out[0] == 0x5A && out[4] == 0x56);
	assert(canary_a == 0xA11CE && canary_b == 0xB0B5E); /* 调用点栈金丝雀 */
	puts("ok: asm callbacks, register args, stack-passed cap/size");
}

static void test_sret_aggregates(void)
{
	pg_tool_registry r;
	pg_tool_definition d = {"sret", "d", "{\"type\":\"object\"}", NULL,
				ok_validate, ok_authorize, ok_execute,0,0};
	pg_tools a, b;
	pg_read_tool reader;
	pg_tool_definition rd;

	reader.directory = -1;
	assert(!pg_tool_registry_init(&r, &d, 1));
	a = pg_tool_registry_service(&r);
	assert(a.context == &r && a.authorize && a.execute);
	b = pg_tool_registry_service(&r);
	assert(a.context == b.context && a.authorize == b.authorize && a.execute == b.execute);
	rd = pg_read_tool_definition(&reader);
	assert(strcmp(rd.name, "read") == 0 && rd.context == &reader);
	assert(strcmp(rd.schema,
	       "{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"],\"additionalProperties\":false}") == 0);
	assert(rd.validate && rd.authorize && rd.execute);
	/* ABI 探针 + 哨兵 */
	assert(abi_probe3((abi_fn3)pg_read_tool_open, (unsigned long)&reader,
	                   (unsigned long)"/nonexistent-pangu-abi-probe", 0) == 1);
	check_sentinels();
	pg_read_tool_close(&reader);
	assert(abi_probe3((abi_fn3)ok_validate, 0, 0, 0) == 1);
	check_sentinels();
	puts("ok: sret aggregates, ABI probe sentinels");
}

static void test_registry_boundaries(void)
{
	pg_tool_definition defs[PG_TOOL_LIMIT + 1];
	pg_tool_registry keep;
	char long_name[PG_ID_LIMIT + 2], big[PG_TEXT_LIMIT + 2];
	size_t i;
	static const char schema[] = "{\"type\":\"object\"}";
	int probe_ctx = 0;

	memset(defs, 0, sizeof(defs));
	for (i = 0; i <= PG_ID_LIMIT; i++)
		long_name[i] = 'a';
	long_name[PG_ID_LIMIT + 1] = '\0';
	memset(big, 'b', sizeof(big));
	for (i = 0; i < PG_TOOL_LIMIT; i++) {
		defs[i].name = "t0";
		defs[i].description = "d";
		defs[i].schema = schema;
		defs[i].context = &probe_ctx;
		defs[i].validate = ok_validate;
		defs[i].authorize = ok_authorize;
		defs[i].execute = ok_execute;
	}
	defs[0].name = "t-0_1"; /* 合法字符集 */
	assert(!pg_tool_registry_init(&keep, defs, 1));
	assert(keep.count == 1 && strcmp(keep.entries[0].name, "t-0_1") == 0);
	/* 失败初始化保持原 registry 不变（事务性） */
	defs[1] = defs[0];
	assert(pg_tool_registry_init(&keep, defs, 2) != 0);
	assert(keep.count == 1 && strcmp(keep.entries[0].name, "t-0_1") == 0);
	defs[1].name = "t1";
	assert(!pg_tool_registry_init(&keep, defs, 2));
	assert(keep.count == 2);
	/* 16 项成功、17 项失败 */
	for (i = 2; i < PG_TOOL_LIMIT; i++) {
		char *n = malloc(8);
		snprintf(n, 8, "t%zu", i);
		defs[i].name = n;
	}
	assert(!pg_tool_registry_init(&keep, defs, PG_TOOL_LIMIT));
	assert(keep.count == PG_TOOL_LIMIT);
	defs[PG_TOOL_LIMIT] = defs[0];
	defs[PG_TOOL_LIMIT].name = "extra";
	assert(pg_tool_registry_init(&keep, defs, PG_TOOL_LIMIT + 1) != 0);
	assert(keep.count == PG_TOOL_LIMIT); /* 上限拒绝也是事务性 */
	/* 名称边界 */
	assert(pg_tool_registry_init(&keep, defs, 0) == 0); /* count==0 清空 */
	assert(keep.count == 0);
	defs[0].name = long_name; /* 64 字符 */
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	long_name[63] = '\0'; /* 63 字符（上限内：长度 < PG_ID_LIMIT） */
	assert(!pg_tool_registry_init(&keep, defs, 1));
	assert(strcmp(keep.entries[0].name, long_name) == 0);
	defs[0].name = "";
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	defs[0].name = "bad name";
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	defs[0].name = "bad/name";
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	/* 描述边界 */
	defs[0].name = "t0";
	big[PG_TEXT_LIMIT - 1] = '\0';
	defs[0].description = big;
	assert(!pg_tool_registry_init(&keep, defs, 1));
	big[PG_TEXT_LIMIT - 1] = 'b';
	big[PG_TEXT_LIMIT] = '\0';
	defs[0].description = big;
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	defs[0].description = NULL;
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	/* schema 拒绝分支 */
	defs[0].description = "d";
	defs[0].schema = "{\"type\":\"array\"}";
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	defs[0].schema = "[1,2]";
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	defs[0].schema = "{}x";
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	defs[0].schema = "{\"type\":1}";
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	defs[0].schema = "{\"type\":\"object\\u0000extra\"}";
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	defs[0].schema = "{\"type\":\"object\"}\n\t ";
	assert(!pg_tool_registry_init(&keep, defs, 1)); /* 合法 schema + 尾随空白允许 */
	/* 必需回调 */
	defs[0].schema = schema;
	defs[0].validate = NULL;
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	defs[0].validate = ok_validate;
	defs[0].authorize = NULL;
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	defs[0].authorize = ok_authorize;
	defs[0].execute = NULL;
	assert(pg_tool_registry_init(&keep, defs, 1) != 0);
	defs[0].execute = ok_execute;
	/* 目标别名：defs 与 registry 同址（被替换 C 经本地 candidate 支持） */
	{
		pg_tool_registry alias;

		memset(&alias, 0, sizeof(alias));
		defs[0].name = "alias";
		alias.entries[0] = defs[0];
		assert(!pg_tool_registry_init(&alias, alias.entries, 1));
		assert(alias.count == 1 && strcmp(alias.entries[0].name, "alias") == 0);
		assert(alias.entries[0].validate == ok_validate);
		assert(!alias.entries[1].name && !alias.entries[1].validate); /* 尾区清零 */
	}
	/* NULL registry / defs 组合 */
	assert(pg_tool_registry_init(NULL, defs, 1) != 0);
	assert(pg_tool_registry_init(&keep, NULL, 1) != 0);
	assert(pg_tool_registry_init(&keep, NULL, 0) == 0);
	assert(keep.count == 0);
	puts("ok: registry limits, transactional failure, name/description/schema boundaries");
}

static void test_argument_strictness(void)
{
	pg_tool_definition d = {"echo", "d", "{\"type\":\"object\"}", NULL,
				ok_validate, ok_authorize, ok_execute,0,0};
	pg_tool_registry r;
	pg_tools svc;
	char out[64];
	size_t n;
	static const char *bad[] = {
		"", "   ", "{}x", "{} }", "[]", "[1]", "5", "\"s\"", "null", "true",
		"false", "{", "}{", "{\"a\":1}}",
	};
	size_t i;
	char *big;
	pg_slice embedded;

	assert(!pg_tool_registry_init(&r, &d, 1));
	svc = pg_tool_registry_service(&r);
	for (i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
		assert(svc.execute(svc.context, s("echo"), s(bad[i]), out, sizeof(out), &n) != 0);
	/* 合法形态：对象 + 尾随空白 */
	assert(!svc.execute(svc.context, s("echo"), s("{\"a\":1} \n\t "), out, sizeof(out), &n));
	assert(n == 4 && !memcmp(out, "done", 4));
	/* 嵌入 NUL 的显式长度切片 */
	embedded.data = "{\"a\":1}\0\"x\"";
	embedded.size = 11;
	assert(svc.execute(svc.context, s("echo"), embedded, out, sizeof(out), &n) != 0);
	/* size 超上限 */
	big = calloc(1, PG_TEXT_LIMIT + 16);
	memset(big, ' ', PG_TEXT_LIMIT + 8);
	assert(svc.execute(svc.context, s("echo"), (pg_slice){big, PG_TEXT_LIMIT + 8}, out, sizeof(out), &n) != 0);
	free(big);
	/* out/size 空指针直接拒绝 */
	assert(svc.execute(svc.context, s("echo"), s("{}"), NULL, sizeof(out), &n) != 0);
	assert(svc.execute(svc.context, s("echo"), s("{}"), out, sizeof(out), NULL) != 0);
	/* 查找：前缀不匹配、更长名与嵌入 NUL 名 → 未知工具拒绝 */
	assert(svc.execute(svc.context, s("ech"), s("{}"), out, sizeof(out), &n) != 0);
	assert(svc.execute(svc.context, s("echo2"), s("{}"), out, sizeof(out), &n) != 0);
	assert(svc.execute(svc.context, (pg_slice){"echo\0x", 6}, s("{}"), out, sizeof(out), &n) != 0);
	assert(svc.authorize(svc.context, s("echo"), s("{}")));
	assert(!svc.authorize(svc.context, (pg_slice){NULL, 0}, s("{}")));
	puts("ok: argument JSON strictness, lookup boundaries");
}

static void test_read_boundaries(void)
{
	char root[] = "/tmp/pangu-read-abi-XXXXXX";
	char path[512];
	pg_read_tool reader;
	pg_tool_definition d;
	pg_tool_registry r;
	pg_tools svc;
	char out[128];
	size_t n;
	FILE *f;
	int i;

	assert(mkdtemp(root));
	reader.directory = -1;
	assert(!pg_read_tool_open(&reader, root));
	d = pg_read_tool_definition(&reader);
	assert(!pg_tool_registry_init(&r, &d, 1));
	svc = pg_tool_registry_service(&r);

	/* 容量 0/1：固定 invalid read path（与被替换 C 一致） */
	snprintf(path, sizeof(path), "%s/a", root);
	f = fopen(path, "w");
	assert(f);
	fputs("xy", f);
	fclose(f);
	assert(svc.execute(svc.context, s("read"), s("{\"path\":\"a\"}"), out, 0, &n) != 0);
	assert(svc.execute(svc.context, s("read"), s("{\"path\":\"a\"}"), out, 1, &n) != 0);
	/* 容量 2：1 字节文件恰好放下 */
	f = fopen(path, "w");
	fputs("x", f);
	fclose(f);
	assert(!svc.execute(svc.context, s("read"), s("{\"path\":\"a\"}"), out, 2, &n));
	assert(n == 1 && out[0] == 'x');
	/* 容量 2：2 字节文件放不下（成功要求文件 ≤ cap-1）→ 超限拒绝 */
	f = fopen(path, "w");
	fputs("xy", f);
	fclose(f);
	assert(svc.execute(svc.context, s("read"), s("{\"path\":\"a\"}"), out, 2, &n) != 0);
	/* 容量 128：127 字节恰好、128 字节超一字节拒绝 */
	{
		char buf[128];
		memset(buf, 'c', sizeof(buf));
		f = fopen(path, "w");
		fwrite(buf, 1, 127, f);
		fclose(f);
		assert(!svc.execute(svc.context, s("read"), s("{\"path\":\"a\"}"), out, 128, &n));
		assert(n == 127);
		f = fopen(path, "w");
		fwrite(buf, 1, 128, f);
		fclose(f);
		assert(svc.execute(svc.context, s("read"), s("{\"path\":\"a\"}"), out, 128, &n) != 0);
	}
	/* 零字节文件：成功且 n==0 */
	f = fopen(path, "w");
	fclose(f);
	assert(!svc.execute(svc.context, s("read"), s("{\"path\":\"a\"}"), out, 128, &n));
	assert(n == 0);
	/* 深层嵌套 8 级 + UTF-8 组件 */
	{
		char deep[512];
		snprintf(deep, sizeof(deep), "%s/深度", root);
		assert(!mkdir(deep, 0700));
		for (i = 0; i < 7; i++) {
			char next[512];
			snprintf(next, sizeof(next), "%s/d%d", deep, i);
			assert(!mkdir(next, 0700));
			strcpy(deep, next);
		}
		snprintf(path, sizeof(path), "%s/文件.txt", deep);
		f = fopen(path, "w");
		fputs("你好，盘古\n", f);
		fclose(f);
		{
			char q[512];
			snprintf(q, sizeof(q), "{\"path\":\"深度/d0/d1/d2/d3/d4/d5/d6/文件.txt\"}");
			assert(!svc.execute(svc.context, s("read"), s(q), out, 128, &n));
			assert(n == strlen("你好，盘古\n") && !memcmp(out, "你好，盘古\n", n));
		}
		/* 反向清理 d6..d0 与 深度/文件.txt */
		{
			char cur[512];
			strcpy(cur, deep);
			snprintf(path, sizeof(path), "%s/文件.txt", cur);
			unlink(path);
			for (i = 6; i >= 0; i--) {
				rmdir(cur);
				/* cur 去掉末级 /dN */
				char *slash = strrchr(cur, '/');
				assert(slash);
				*slash = '\0';
			}
			rmdir(cur); /* 深度 */
		}
	}
	/* 空中间分量与多余分隔拒绝 */
	assert(svc.execute(svc.context, s("read"), s("{\"path\":\"a//b\"}"), out, 128, &n) != 0);
	assert(svc.execute(svc.context, s("read"), s("{\"path\":\"/a\"}"), out, 128, &n) != 0);
	assert(svc.execute(svc.context, s("read"), s("{\"path\":\"a/./b\"}"), out, 128, &n) != 0);
	assert(svc.execute(svc.context, s("read"), s("{\"path\":\".\"}"), out, 128, &n) != 0);
	assert(svc.execute(svc.context, s("read"), s("{\"path\":\"..\"}"), out, 128, &n) != 0);
	assert(svc.execute(svc.context, s("read"), s("{\"path\":\"\"}"), out, 128, &n) != 0);
	/* 多成员/类型错误/非字符串 path */
	assert(svc.execute(svc.context, s("read"), s("{\"path\":\"a\",\"x\":1}"), out, 128, &n) != 0);
	assert(svc.execute(svc.context, s("read"), s("{\"path\":1}"), out, 128, &n) != 0);
	assert(svc.execute(svc.context, s("read"), s("[\"a\"]"), out, 128, &n) != 0);
	/* 根目录本体（目录而非普通文件）拒绝 */
	assert(svc.execute(svc.context, s("read"), s("{\"path\":\".\"}"), out, 128, &n) != 0);
	/* 关闭根：authorize 前不可达（execute 先 parse 后 authorize）→ 根不可用 */
	pg_read_tool_close(&reader);
	assert(svc.execute(svc.context, s("read"), s("{\"path\":\"a\"}"), out, 128, &n) != 0);
	pg_read_tool_close(&reader); /* 幂等 */
	puts("ok: read capacity/depth/UTF-8/refusal boundaries");
}

static void test_guard_page_outputs(void)
{
	char root[] = "/tmp/pangu-read-grd-XXXXXX";
	char path[512];
	pg_read_tool reader;
	pg_tool_definition d;
	pg_tool_registry r;
	pg_tools svc;
	size_t pg = 4096;
	char *base, *out;
	size_t n;
	FILE *f;

	assert(mkdtemp(root));
	snprintf(path, sizeof(path), "%s/f", root);
	f = fopen(path, "w");
	fputs("abc", f);
	fclose(f);
	reader.directory = -1;
	assert(!pg_read_tool_open(&reader, root));
	d = pg_read_tool_definition(&reader);
	assert(!pg_tool_registry_init(&r, &d, 1));
	svc = pg_tool_registry_service(&r);

	/* 输出缓冲尾紧贴保护页：容量 4，文件 3 字节（fits）与 4 字节（超限拒绝） */
	base = mmap(NULL, 2 * pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	assert(base != MAP_FAILED);
	assert(!mprotect(base + pg, pg, PROT_NONE));
	out = base + pg - 8; /* 8 字节可用区，尾贴保护页 */
	/* 3 字节文件 + 容量 4：恰好 cap-1 放下 */
	assert(!svc.execute(svc.context, s("read"), s("{\"path\":\"f\"}"), out, 4, &n));
	assert(n == 3 && out[0] == 'a');
	/* 4 字节文件：容量 4 放不下（严格小于）→ 超限拒绝，至多写 3 字节不触保护页 */
	f = fopen(path, "w");
	fputs("abcd", f);
	fclose(f);
	assert(svc.execute(svc.context, s("read"), s("{\"path\":\"f\"}"), out, 4, &n) != 0);
	/* 4 字节文件 + 容量 5：成功且 n==4，写入仍在 8 字节区内 */
	assert(!svc.execute(svc.context, s("read"), s("{\"path\":\"f\"}"), out, 5, &n));
	assert(n == 4 && out[3] == 'd');
	/* 参数切片尾贴保护页（严格 JSON 只读 [0,size)） */
	{
		char *abuf = mmap(NULL, 2 * pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		assert(abuf != MAP_FAILED);
		assert(!mprotect(abuf + pg, pg, PROT_NONE));
		memcpy(abuf + pg - 15, "{\"path\":\"f\"}   ", 15);
		assert(!svc.execute(svc.context, s("read"), (pg_slice){abuf + pg - 15, 15}, out, 5, &n));
		assert(n == 4);
		munmap(abuf, 2 * pg);
	}
	munmap(base, 2 * pg);
	pg_read_tool_close(&reader);
	puts("ok: guard page output/argument buffers");
}

int main(void)
{
	test_asm_callbacks_and_stack_args();
	test_sret_aggregates();
	test_registry_boundaries();
	test_argument_strictness();
	test_read_boundaries();
	test_guard_page_outputs();
	puts("PASS: tools/read assembly ABI, registry transaction, strictness and boundary suite");
	return 0;
}
