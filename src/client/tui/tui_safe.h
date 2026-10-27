/* tui_safe.h — 安全输出原语：控制字节转义 + ANSI 换行翻译（视图/工具/diff 共享）。
 *
 * 从 tui_view.S 的内部通道提取（原地维护一个实现）；视图的 li_* 包装转发到此。
 * 契约：
 *  - tui_raw_write：不转义；ansi=1 时 '\n' 译为 "\r\n"（分段写出），ansi=0 原样。
 *  - tui_safe_write：在 raw 基础上转义不可打印内容——C0 控制字节（除 \t \n）、
 *    DEL、C1、非法/不完整 UTF-8 序列输出 "\xHH" 文本；U+0080–U+009F 输出
 *    "\u00XX"；完整合法 UTF-8 原样。SGR 等可打印 ASCII 原样通过。
 *  - sink 借用（ctx 归宿主）；无状态，不持有任何缓冲。
 */
#ifndef PANGU_TUI_SAFE_H
#define PANGU_TUI_SAFE_H

#include <stddef.h>

#include "tui_sink.h"

#ifdef __cplusplus
extern "C" {
#endif

size_t tui_safe_seq_len(const char *data, size_t len);

void tui_raw_write(const tui_out_sink *sink, int ansi, const char *data, size_t len);
void tui_safe_write(const tui_out_sink *sink, int ansi, const char *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_SAFE_H */
