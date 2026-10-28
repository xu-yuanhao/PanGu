#!/usr/bin/env bash
# tools/client/tui/build.sh — 构建 TUI 演示程序与单元测试（仅本模块，不接全局构建）。
# 核心（tui_utf8/input/state/view/app + term 组合原语）为手写 .S；
# tui_term.c 保留 termios/ioctl 平台结构与 libunistring 外部 ABI 胶水（c-bridge/platform-layout）。
# 用法: build.sh [run-tests]
set -euo pipefail
cd "$(dirname "$0")/../../.."   # 仓库根 (tools/client/tui → 仓库根)
OUT=${PANGU_TUI_BUILD_DIR:-${TMPDIR:-/tmp}/pangu-tui-build-${UID:-$(id -u)}}
SRC=src/client/tui
TST=tests/client/tui
CC=${CC:-cc}
CFLAGS="-std=c11 -Wall -Wextra -Wshadow -g -O1"
INC="-I$SRC"

mkdir -p "$OUT"

ASM_SRC="$SRC/tui_utf8.S $SRC/tui_term.S $SRC/tui_safe.S $SRC/tui_input.S $SRC/tui_view.S $SRC/tui_md.S $SRC/tui_tool.S $SRC/tui_logo.S $SRC/tui_diff.S $SRC/tui_state.S $SRC/tui_app.S"
C_SRC="$SRC/tui_term.c"

echo "== 组装/编译核心（产品客户端，libc + 动态 libunistring Unicode 胶水） =="
# shellcheck disable=SC2086
for assembly in $ASM_SRC; do
	$CC $CFLAGS $INC -c "$assembly" -o "$OUT/$(basename "${assembly%.S}").o"
done
$CC $CFLAGS $INC -c $C_SRC -o "$OUT/tui_term_cglue.o"

echo "== 编译演示程序（核心 + 假 Runtime + demo main） =="
# shellcheck disable=SC2086
for fixture in fake_runtime demo_main; do
	$CC $CFLAGS $INC -c "$TST/$fixture.c" -o "$OUT/$fixture.o"
done
$CC "$OUT"/tui_utf8.o "$OUT"/tui_term.o "$OUT"/tui_term_cglue.o "$OUT"/tui_safe.o "$OUT"/tui_input.o \
	"$OUT"/tui_logo.o "$OUT"/tui_view.o "$OUT"/tui_md.o "$OUT"/tui_tool.o "$OUT"/tui_diff.o "$OUT"/tui_state.o "$OUT"/tui_app.o \
	"$OUT"/fake_runtime.o "$OUT"/demo_main.o -Wl,--no-as-needed -ljson-c -o "$OUT/pangu-tui-demo"

echo "== 编译单元测试 =="
# shellcheck disable=SC2086
$CC $CFLAGS $INC -Wl,--no-as-needed -ljson-c -o "$OUT/test_tui_utf8" "$TST/test_tui_utf8.c" "$OUT/tui_utf8.o"
# shellcheck disable=SC2086
$CC $CFLAGS $INC -Wl,--no-as-needed -ljson-c -o "$OUT/test_tui_input" "$TST/test_tui_input.c" "$OUT/tui_input.o" "$OUT/tui_utf8.o" "$OUT/tui_term_cglue.o"
# shellcheck disable=SC2086
$CC $CFLAGS $INC -Wl,--no-as-needed -ljson-c -o "$OUT/test_tui_md" "$TST/test_tui_md.c" "$OUT/tui_md.o" "$OUT/tui_logo.o" "$OUT/tui_view.o" "$OUT/tui_tool.o" "$OUT/tui_safe.o" "$OUT/tui_diff.o" "$OUT/tui_utf8.o" "$OUT/tui_input.o" "$OUT/tui_term_cglue.o"
# shellcheck disable=SC2086
$CC $CFLAGS $INC -Wl,--no-as-needed -ljson-c -o "$OUT/test_tui_tool" "$TST/test_tui_tool.c" "$OUT/tui_tool.o" "$OUT/tui_safe.o" "$OUT/tui_utf8.o"
# shellcheck disable=SC2086
$CC $CFLAGS $INC -Wl,--no-as-needed -ljson-c -Wl,--wrap=malloc,--wrap=free -o "$OUT/test_tui_diff" "$TST/test_tui_diff.c" "$OUT/tui_diff.o" "$OUT/tui_safe.o" "$OUT/tui_utf8.o"
# shellcheck disable=SC2086
$CC $CFLAGS $INC -Wl,--no-as-needed -ljson-c -o "$OUT/test_tui_view" "$TST/test_tui_view.c" "$OUT/tui_logo.o" "$OUT/tui_view.o" "$OUT/tui_md.o" "$OUT/tui_tool.o" "$OUT/tui_safe.o" "$OUT/tui_diff.o" "$OUT/tui_utf8.o" "$OUT/tui_input.o" "$OUT/tui_term_cglue.o"
# shellcheck disable=SC2086
$CC $CFLAGS $INC -Wl,--no-as-needed -ljson-c -o "$OUT/test_tui_state" "$TST/test_tui_state.c" "$OUT/tui_state.o" "$OUT/tui_logo.o" "$OUT/tui_view.o" "$OUT/tui_md.o" "$OUT/tui_tool.o" "$OUT/tui_safe.o" "$OUT/tui_diff.o" "$OUT/tui_utf8.o" "$OUT/tui_input.o" "$OUT/tui_term_cglue.o"
# shellcheck disable=SC2086
$CC $CFLAGS $INC -Wl,--no-as-needed -ljson-c -o "$OUT/test_fake_runtime" "$TST/test_fake_runtime.c" "$OUT/fake_runtime.o"
# ABI/边界/哨兵专项：寄存器保存与破坏、栈金丝雀、保护页边界、平台结构断言
# shellcheck disable=SC2086
$CC -D_POSIX_C_SOURCE=200809L -Wno-cast-function-type $CFLAGS $INC -I"$TST" -Wl,--no-as-needed -ljson-c -o "$OUT/test_tui_abi" \
	"$TST/test_tui_abi.c" "$OUT/tui_utf8.o" "$OUT/tui_input.o" "$OUT/tui_state.o" \
	"$OUT/tui_logo.o" "$OUT/tui_view.o" "$OUT/tui_md.o" "$OUT/tui_tool.o" "$OUT/tui_diff.o" "$OUT/tui_safe.o" \
	"$OUT/tui_app.o" "$OUT/tui_term.o" "$OUT/tui_term_cglue.o"

# Geometric oracle, frame deadline and idle-write verification.
$CC $CFLAGS $INC -o "$OUT/test_tui_logo" "$TST/test_tui_logo.c" "$OUT/tui_logo.o" "$OUT/tui_view.o" "$OUT/tui_md.o" "$OUT/tui_tool.o" "$OUT/tui_safe.o" "$OUT/tui_diff.o" "$OUT/tui_utf8.o" "$OUT/tui_input.o" "$OUT/tui_term_cglue.o" -ljson-c -lm

echo "构建完成: $OUT/"
echo "  $OUT/pangu-tui-demo --demo <no_tool|tool_roundtrip|model_error|user_cancel|busy_reject>"
if [ "${1:-}" = "run-tests" ]; then
	"$OUT/test_tui_utf8" && \
		"$OUT/test_tui_input" && \
		"$OUT/test_tui_md" && \
		"$OUT/test_tui_tool" && \
		"$OUT/test_tui_diff" && \
		"$OUT/test_tui_view" && \
		"$OUT/test_tui_state" && \
		"$OUT/test_fake_runtime" && \
		"$OUT/test_tui_abi" && \
        "$OUT/test_tui_logo"
fi
