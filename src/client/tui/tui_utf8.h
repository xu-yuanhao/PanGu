/* tui_utf8.h — 最小 UTF-8 字节序列辅助（客户端内部，非公共 ABI）。
 *
 * 只做客户端展示/编辑需要的字节级操作：不校验完整合法性，
 * 非法字节原样交给终端显示；宽字符字素编辑本轮不支持（记录于交接报告）。
 */
#ifndef PANGU_TUI_UTF8_H
#define PANGU_TUI_UTF8_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 首字节推断序列长度：1..4；非法首字节返回 1（按单字节处理）。 */
size_t tui_utf8_seq_len(unsigned char lead);
/* Shared code-point cell width approximation used by input and view. */
unsigned tui_utf8_cp_width(const char *seq, size_t len);
/* External full Unicode lowercase; caller owns output, SIZE_MAX on failure. */
size_t tui_unicode_lower(const char *text, size_t len, char *out, size_t cap);

/* buf[0..len) 末尾不完整序列的字节数（0..3），用于跨事件拼接。 */
size_t tui_utf8_incomplete_tail(const char *buf, size_t len);

/* 返回 buf 末尾最后一个序列起始字节的偏移（退格删除一个 code point 用）。
 * len 为 0 时返回 0。连续字节最多回扫 3 字节。 */
size_t tui_utf8_last_seq_start(const char *buf, size_t len);

/* 在不超过 max_bytes 且不拆断序列的前提下可保留的最大字节数。 */
size_t tui_utf8_truncate_len(const char *buf, size_t len, size_t max_bytes);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_UTF8_H */
