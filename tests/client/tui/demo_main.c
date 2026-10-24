/* demo_main.c — 盘古薄 TUI 演示入口（假 Runtime 事件源）。
 *
 * 用法：
 *   pangu-tui-demo --demo <scenario> [--non-interactive] [--tick <ms>]
 *
 * 场景：no_tool, tool_roundtrip, model_error, user_cancel, busy_reject
 *
 * 退出码：0=正常退出（EOF/^D、闲置 ^C、终止信号，均已恢复终端）；
 *         1=用法错误或初始化失败；2=Runtime poll 错误（未补造终态）。
 * 无 TTY 时默认明确拒绝交互模式；--non-interactive 显式选择降级纯文本模式。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fake_runtime.h"
#include "tui_app.h"

static void usage(FILE *out, const char *prog)
{
	char names[128];

	tui_demo_scenario_names(names, sizeof(names));
	fprintf(out, "用法: %s --demo <scenario> [--non-interactive] [--tick <ms>]\n", prog);
	fprintf(out, "场景: %s\n", names);
	fprintf(out, "退出码: 0=正常 1=用法/初始化失败 2=Runtime错误\n");
}

int main(int argc, char **argv)
{
	const char *scenario = NULL;
	int non_interactive = 0;
	unsigned tick_ms = 0;
	const tui_demo_scenario *sc;
	tui_fake_runtime fr;
	tui_app_config cfg;
	int rc;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--demo") == 0 && i + 1 < argc) {
			scenario = argv[++i];
		} else if (strcmp(argv[i], "--non-interactive") == 0) {
			non_interactive = 1;
		} else if (strcmp(argv[i], "--tick") == 0 && i + 1 < argc) {
			tick_ms = (unsigned)atoi(argv[++i]);
		} else {
			usage(stderr, argv[0]);
			return 1;
		}
	}
	if (scenario == NULL) {
		usage(stderr, argv[0]);
		return 1;
	}
	sc = tui_demo_scenario_get(scenario);
	if (sc == NULL) {
		fprintf(stderr, "未知场景: %s\n", scenario);
		usage(stderr, argv[0]);
		return 1;
	}

	if (!non_interactive && !(isatty(0) == 1 && isatty(1) == 1)) {
		fprintf(stderr,
			"stdin/stdout 不是 TTY：拒绝进入交互模式（不会无限等待）。\n"
			"如需降级纯文本模式，请显式使用 --non-interactive。\n");
		return 1;
	}

	tui_fake_runtime_init(&fr, sc);
	memset(&cfg, 0, sizeof(cfg));
	cfg.in_fd = 0;
	cfg.out_fd = 1;
	cfg.ansi = non_interactive ? 0 : 1;
	cfg.ops = tui_fake_runtime_ops(&fr);
	cfg.poll_tick_ms = tick_ms;

	rc = tui_app_run(&cfg);
	if (rc < 0) {
		fprintf(stderr, "初始化失败\n");
		return 1;
	}
	/* 信号退出同样是恢复后的正常路径 */
	return rc == (int)TUI_EXIT_POLL_ERROR ? 2 : 0;
}
