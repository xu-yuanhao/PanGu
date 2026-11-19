#define _POSIX_C_SOURCE 200809L
#include <pangu/tui/completion.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
typedef struct {
  char *text;
  size_t length, count;
} prefix;
static int candidate(prefix *p, const char *s, size_t n, const char *token,
                     size_t len) {
  if (n < len || memcmp(s, token, len))
    return 1;
  if (!p->count) {
    p->text = malloc(n + 1);
    if (!p->text)
      return 0;
    memcpy(p->text, s, n);
    p->text[n] = 0;
    p->length = n;
  } else {
    size_t i = 0;
    while (i < p->length && i < n && p->text[i] == s[i])
      i++;
    /* Common prefix must end before any partial UTF-8 scalar. */
    while (i && ((unsigned char)p->text[i] & 0xc0) == 0x80)
      i--;
    p->length = i;
    p->text[i] = 0;
  }
  p->count++;
  return 1;
}
char *pg_tui_complete(void *ctx, const char *token, size_t len, size_t *out) {
  if (out)
    *out = 0;
  if (!out || (!token && len) || !len || memchr(token, 0, len))
    return NULL;
  pg_tui_completion_catalog *cat = ctx;
  prefix p = {0};
  if (token[0] == '/' || token[0] == '@') {
    static const char *defaults[] = {"/help", "/quit", "/tree", "/switch", "/branch", "/reset", "/handoff", "/compact", "/usage"};
    for (size_t i = 0; i < 2; i++)
      if (!candidate(&p, defaults[i], strlen(defaults[i]), token, len))
        goto failed;
    if (cat)
      for (size_t i = 0; i < cat->count; i++)
        if (!candidate(&p, cat->entries[i].text, cat->entries[i].length, token,
                       len))
          goto failed;
    /* A leading slash can also be an absolute filesystem path. */
    if (p.count || token[0] == '@')
      goto done;
  }
  size_t slash = len;
  while (slash && token[slash - 1] != '/')
    slash--;
  size_t cwd = cat && cat->cwd ? strlen(cat->cwd) : 0;
  if (cwd > SIZE_MAX - slash - 3)
    goto failed;
  char *dir = malloc(cwd + slash + 3);
  if (!dir)
    goto failed;
  size_t used = 0;
  if (cwd && token[0] != '/') {
    memcpy(dir, cat->cwd, cwd);
    used = cwd;
    dir[used++] = '/';
  }
  if (slash) {
    memcpy(dir + used, token, slash);
    used += slash;
  } else {
    dir[used++] = '.';
  }
  dir[used] = 0;
  uv_fs_t req;
  int rc = uv_fs_scandir(NULL, &req, dir, 0, NULL);
  free(dir);
  if (rc >= 0) {
    uv_dirent_t entry;
    while (uv_fs_scandir_next(&req, &entry) == 0) {
      size_t n = strlen(entry.name);
      if (entry.name[0] == '.' && (slash == len || token[slash] != '.'))
        continue;
      if (slash > SIZE_MAX - n - 2) {
        uv_fs_req_cleanup(&req);
        goto failed;
      }
      char *s = malloc(slash + n + 2);
      if (!s) {
        uv_fs_req_cleanup(&req);
        goto failed;
      }
      if (slash)
        memcpy(s, token, slash);
      memcpy(s + slash, entry.name, n);
      n += slash;
      if (entry.type == UV_DIRENT_DIR)
        s[n++] = '/';
      s[n] = 0;
      int ok = candidate(&p, s, n, token, len);
      free(s);
      if (!ok) {
        uv_fs_req_cleanup(&req);
        goto failed;
      }
    }
  }
  uv_fs_req_cleanup(&req);
done:
  if (!p.count)
    return NULL;
  *out = p.length;
  return p.text;
failed:
  free(p.text);
  return NULL;
}
