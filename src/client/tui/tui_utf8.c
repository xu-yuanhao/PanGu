#include "tui_utf8.h"

size_t tui_utf8_seq_len(unsigned char lead)
{
	if (lead < 0x80u)
		return 1;
	if ((lead & 0xE0u) == 0xC0u)
		return 2;
	if ((lead & 0xF0u) == 0xE0u)
		return 3;
	if ((lead & 0xF8u) == 0xF0u)
		return 4;
	return 1; /* 非法首字节按单字节放行，由终端决定如何显示 */
}

size_t tui_utf8_incomplete_tail(const char *buf, size_t len)
{
	size_t start = tui_utf8_last_seq_start(buf, len);
	if (start == 0 && len == 0)
		return 0;
	if (start >= len)
		return 0;
	{
		size_t seq = tui_utf8_seq_len((unsigned char)buf[start]);
		size_t have = len - start;
		if (seq > 1 && have < seq)
			return have; /* 末尾是不完整序列前缀 */
	}
	return 0;
}

size_t tui_utf8_last_seq_start(const char *buf, size_t len)
{
	size_t i;
	if (len == 0)
		return 0;
	i = len - 1;
	/* 连续字节 10xxxxxx 最多回扫 3 次（4 字节序列） */
	while (i > 0 && ((unsigned char)buf[i] & 0xC0u) == 0x80u && (len - i) < 4)
		i--;
	return i;
}

size_t tui_utf8_truncate_len(const char *buf, size_t len, size_t max_bytes)
{
	size_t keep = 0;
	if (len <= max_bytes)
		return len;
	while (keep < max_bytes) {
		size_t seq = tui_utf8_seq_len((unsigned char)buf[keep]);
		if (keep + seq > max_bytes)
			break;
		keep += seq;
	}
	return keep;
}
