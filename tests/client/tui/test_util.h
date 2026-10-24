/* test_util.h — 测试断言与内存输出 sink（仅测试使用）。 */
#ifndef PANGU_TUI_TEST_UTIL_H
#define PANGU_TUI_TEST_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tui_view.h"

#define T_CHECK(cond)                                                         \
	do {                                                                  \
		if (!(cond)) {                                                \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			exit(1);                                              \
		}                                                             \
	} while (0)

#define T_RUN(name)                                      \
	do {                                             \
		fprintf(stderr, "ok: %s\n", name);       \
	} while (0)

typedef struct tstr {
	char buf[1 << 16];
	size_t len;
} tstr;

static inline int tstr_write(void *ctx, const char *data, size_t len)
{
	tstr *s = ctx;

	if (s->len + len >= sizeof(s->buf))
		return -1;
	memcpy(s->buf + s->len, data, len);
	s->len += len;
	s->buf[s->len] = '\0';
	return 0;
}

static inline void tstr_clear(tstr *s)
{
	s->len = 0;
	s->buf[0] = '\0';
}

static inline int tstr_contains(const tstr *s, const char *needle)
{
	return strstr(s->buf, needle) != NULL;
}

static inline tui_out_sink tstr_sink(tstr *s)
{
	tui_out_sink sink;

	sink.ctx = s;
	sink.write = tstr_write;
	return sink;
}

/* 字节串查找（避免依赖 memmem/GNU 扩展） */
static inline const char *find_bytes(const char *hay, size_t n, const char *needle, size_t m)
{
	size_t i;

	if (m == 0 || n < m)
		return NULL;
	for (i = 0; i + m <= n; i++) {
		if (memcmp(hay + i, needle, m) == 0)
			return hay + i;
	}
	return NULL;
}

/* 把字符串按字节喂入编辑器 */
static inline void feed_bytes(tui_input *in, const char *s)
{
	while (*s)
		(void)tui_input_feed(in, (unsigned char)*s++);
}

#endif /* PANGU_TUI_TEST_UTIL_H */
