/* tui_md.h — 流式行级 Markdown 渲染（纯状态，无终端 I/O，经 sink 输出）。
 *
 * 移植 Pi packages/tui/src/components/markdown.ts 的必要块级行为：段落、
 * 标题、列表（嵌套/有序/任务）、引用、围栏代码块、分隔线、管道表格、
 * 链接与流式未闭合块。内联支持代码/粗体/斜体/删除线/链接与反斜杠转义。
 *
 * 流式模型：tui_md_text 逐字节累积源行，行完整即渲染输出（滚动回显无法
 * 重写已滚出内容，故不重排同段落的软换行——每源行独立按终端列宽折行，
 * 与 Pi 整段重排的差异记录于 TODO）。表格需缓冲整块；含 | 的行先作候选，
 * 下一行是分隔行才成立，否则候选按普通段落输出。
 *
 * 流式未闭合块：围栏内疑似部分闭栏行（仅若干开栏字符且更短）暂扣不发，
 * 后续行到达即按内容补发，run 结束仍未完成则丢弃（对应 Pi
 * trimPartialClosingFences 防闪烁行为）；未闭合围栏/表格/尾行在
 * tui_md_flush 收尾输出。
 *
 * 安全：所有输出经转义（可打印/完整 UTF-8 原样，控制字节与非法序列
 * 输出 \xHH 文本），SGR 仅由渲染器自身产生。ANSI 模式行尾为 \r\n，
 * 降级模式为 \n。缓冲有界：源行 512 字节（满则按码点边界拆分输出），
 * 表格 16 行 × 256 字节，超出后表格先渲染、后续行按原文输出。
 */
#ifndef PANGU_TUI_MD_H
#define PANGU_TUI_MD_H

#include <stddef.h>

#include "tui_sink.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TUI_MD_LINE_MAX  512u
#define TUI_MD_TABLE_ROWS 16u
#define TUI_MD_ROW_MAX   256u

typedef struct tui_md {
	tui_out_sink sink;   /* 输出目的地（借用视图 sink，不拥有） */
	int ansi;            /* 1=ANSI 样式/\r\n；0=降级纯文本 */
	unsigned cols;       /* 折行宽度，最小 1 */
	char line[TUI_MD_LINE_MAX]; /* 当前源行累积（无 \n） */
	unsigned line_len;
	unsigned fence;      /* 1=围栏代码块内 */
	unsigned fence_char; /* '`' 或 '~' */
	unsigned fence_len;  /* 开栏标记长度 */
	char hold[TUI_MD_LINE_MAX]; /* 围栏内疑似部分闭栏行 */
	unsigned hold_len;   /* 0=无暂扣 */
	char cand[TUI_MD_LINE_MAX]; /* 表头候选行（含 |） */
	unsigned cand_len;   /* 0=无候选 */
	unsigned table_on;   /* 1=表格行缓冲中 */
	unsigned last_blank; /* 上一行是空行（连续空行折叠） */
	unsigned wrote_any;  /* 流内已输出过内容（抑制首空行） */
	unsigned table_rows; /* 已缓冲表格行数（含表头） */
	unsigned table_lens[TUI_MD_TABLE_ROWS]; /* 各行字节数 */
	char table[TUI_MD_TABLE_ROWS][TUI_MD_ROW_MAX];
} tui_md;

/* sink 借用自视图；cols 最小按 1。 */
void tui_md_init(tui_md *md, tui_out_sink sink, int ansi, unsigned cols);
void tui_md_set_cols(tui_md *md, unsigned cols);

/* 增量文本（可任意拆分，含拆断的 UTF-8）；返回本次调用是否产生输出。 */
int tui_md_text(tui_md *md, const char *bytes, size_t len);

/* 行未完时先按段落行输出（工具/状态行打断前调用）。 */
void tui_md_break(tui_md *md);

/* run 终态收尾：输出尾行、渲染未闭合表格、丢弃暂扣的部分闭栏。 */
void tui_md_flush(tui_md *md);

#ifdef __cplusplus
}
#endif

#endif /* PANGU_TUI_MD_H */
