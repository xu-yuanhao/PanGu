/* tui_diff.h — edit 工具 diff/patch 的行级渲染（纯函数，经 sink 输出）。
 *
 * 移植 Pi modes/interactive/components/diff.ts 的行为（快照 d5b3de2）：
 *  - 行格式 ^([+-\s])(\s*\d*)\s(.*)$：前缀 + 行号 + 内容；不匹配行按上下文呈现。
 *  - TAB → 3 空格；上下文暗色、删除红、新增绿（ANSI 基本色，替代 Pi 主题
 *    token，记录于交接区）。
 *  - 连续"删除组+新增组"且恰为 1:1 时行内变更反色；Pi 用 diffWords 词级
 *    LCS，原生以公共前后缀裁剪近似（差异记录于交接区）。
 *  - 组超过 16 行或单行内容超 255 字节时按码点边界截断，退化为逐行着色。
 *  - 与 Pi 的差异：diff 文本以 \n 结尾时不输出尾部空行。
 */
#ifndef PANGU_TUI_DIFF_H
#define PANGU_TUI_DIFF_H

#include <stddef.h>

#include "tui_sink.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TUI_DIFF_RUN_LINES 16u
#define TUI_DIFF_LINE_MAX  256u

void tui_diff_render(const tui_out_sink *sink, int ansi, const char *text, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_DIFF_H */
