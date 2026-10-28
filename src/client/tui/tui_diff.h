/* tui_diff.h — edit 工具 diff/patch 的行级渲染（纯函数，经 sink 输出）。
 *
 * 移植 Pi modes/interactive/components/diff.ts 的行为（快照 d5b3de2）：
 *  - 行格式 ^([+-\s])(\s*\d*)\s(.*)$：前缀 + 行号 + 内容；不匹配行按上下文呈现。
 *    空白按 ECMAScript Unicode 集合逐标量解析；内容拒绝 CR/LF/U+2028/U+2029。
 *    上下文前缀规范为一个空格，保留行号字段的原始字节。
 *  - TAB → 3 空格；上下文暗色、删除红、新增绿（ANSI 基本色，替代 Pi 主题
 *    token，记录于交接区）。
 *  - 连续"删除组+新增组"且恰为 1:1 时行内变更反色；Pi 用 diffWords 词级
 *    LCS；原生分词相同时采用新文本空白，无共同 token 时整段替换。
 *    有共同 token 的增删先取公共前后缀，再将词内部边界移至 token 边缘；
 *    多个变更段和空白归并仍为近似。
 *    分词依据 jsdiff 8.0.4，源码保留其 BSD-3-Clause 通知，剩余差异见交接区。
 *  - 连续删除/新增组按完整借用源区间扫描，无行数上限；真实 1:1 组
 *    动态分配完整 TAB 展开内容，不限制源行长；分配失败仍完整逐行着色。
 *    组输出 TAB 以有界流式 span 展开。
 *  - 与 Pi 的差异：diff 文本以 \n 结尾时不输出尾部空行。
 */
#ifndef PANGU_TUI_DIFF_H
#define PANGU_TUI_DIFF_H

#include <stddef.h>

#include "tui_sink.h"

#ifdef __cplusplus
extern "C" {
#endif

void tui_diff_render(const tui_out_sink *sink, int ansi, const char *text, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_DIFF_H */
