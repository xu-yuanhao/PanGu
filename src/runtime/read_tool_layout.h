#ifndef PG_READ_TOOL_LAYOUT_H
#define PG_READ_TOOL_LAYOUT_H
/* read_tool.S 与 C 测试共享的布局/平台常量；Linux x86-64 公开 ABI，
 * C 侧断言与 <fcntl.h>/<sys/stat.h> 真实值一致。 */
#define PG_RTL_DIRECTORY 0
#define PG_RTL_RESULT 8
#define PG_RTL_IMAGES 16
#define PG_RTL_IMAGE_LIMITS 24
#define PG_RTL_IMAGE_BYTES 24
#define PG_RTL_IMAGE_WIDTH 32
#define PG_RTL_IMAGE_HEIGHT 36
#define PG_RTL_IMAGE_VISION 40
#define PG_RTL_READ_TOOL_BYTES 48
#define PG_RTL_SCAN_LIMIT 1048576
#define PG_RTL_PAGE_LINES 2000
#define PG_RTL_OUTPUT_MAX 4096

/* open/openat 标志（<fcntl.h>，断言对齐） */
#define PG_RTL_O_RDONLY 0
#define PG_RTL_O_DIRECTORY 65536
#define PG_RTL_O_NOFOLLOW 131072
#define PG_RTL_O_CLOEXEC 524288
#define PG_RTL_O_NONBLOCK 2048
#define PG_RTL_F_DUPFD_CLOEXEC 1030

/* struct stat（<sys/stat.h>）：仅普通文件判定所需字段 */
#define PG_RTL_STAT_MODE 24
#define PG_RTL_STAT_BYTES 144
#define PG_RTL_S_IFMT 0170000
#define PG_RTL_S_IFREG 0100000

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "read_tool.h"
#define PG_RTL_ASSERT_OFF(t, f, v) _Static_assert(offsetof(t, f) == (v), #t "." #f " layout drifted")
#define PG_RTL_ASSERT_SIZE(t, v) _Static_assert(sizeof(t) == (v), #t " size drifted")
PG_RTL_ASSERT_OFF(pg_read_tool, directory, PG_RTL_DIRECTORY);
PG_RTL_ASSERT_OFF(pg_read_tool, result, PG_RTL_RESULT);
PG_RTL_ASSERT_OFF(pg_read_tool, images, PG_RTL_IMAGES);
PG_RTL_ASSERT_OFF(pg_read_tool, image_limits, PG_RTL_IMAGE_LIMITS);
PG_RTL_ASSERT_OFF(pg_read_tool, vision, PG_RTL_IMAGE_VISION);
PG_RTL_ASSERT_SIZE(pg_read_image_limits,16);
PG_RTL_ASSERT_SIZE(pg_read_tool, PG_RTL_READ_TOOL_BYTES);
_Static_assert(O_RDONLY == PG_RTL_O_RDONLY, "O_RDONLY drifted");
_Static_assert(O_DIRECTORY == PG_RTL_O_DIRECTORY, "O_DIRECTORY drifted");
_Static_assert(O_NOFOLLOW == PG_RTL_O_NOFOLLOW, "O_NOFOLLOW drifted");
_Static_assert(O_CLOEXEC == PG_RTL_O_CLOEXEC, "O_CLOEXEC drifted");
_Static_assert(O_NONBLOCK == PG_RTL_O_NONBLOCK, "O_NONBLOCK drifted");
_Static_assert(F_DUPFD_CLOEXEC == PG_RTL_F_DUPFD_CLOEXEC, "F_DUPFD_CLOEXEC drifted");
PG_RTL_ASSERT_OFF(struct stat, st_mode, PG_RTL_STAT_MODE);
PG_RTL_ASSERT_SIZE(struct stat, PG_RTL_STAT_BYTES);
_Static_assert(S_IFMT == PG_RTL_S_IFMT && S_IFREG == PG_RTL_S_IFREG, "stat mode bits drifted");
#endif
#endif
