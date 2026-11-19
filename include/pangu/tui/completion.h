#ifndef PANGU_TUI_COMPLETION_H
#define PANGU_TUI_COMPLETION_H
#include <pangu/tui/chat.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Optional caller-owned completion catalog, borrowed during the callback only.
 * Entries carry lengths and include their '/' or '@' prefix. NULL catalog uses
 * /help and /quit. File paths use libuv on the poll caller thread. cwd is a
 * NUL-terminated UTF-8 directory, NULL means process cwd. Results are malloc
 * owned and contain the longest common prefix (unique directories end in /).
 * Unknown resource tokens are left unchanged; no filesystem mutation. */
typedef struct {
  const char *text;
  size_t length;
} pg_tui_completion_entry;
typedef struct {
  const pg_tui_completion_entry *entries;
  size_t count;
  const char *cwd;
} pg_tui_completion_catalog;
char *pg_tui_complete(void *catalog, const char *token, size_t length,
                      size_t *result_length);
#ifdef __cplusplus
}
#endif
#endif
