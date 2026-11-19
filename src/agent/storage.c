#include "internal.h"
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
static const char initial[] = "{\"format\":\"pangu.session\",\"version\":1,"
                              "\"seq\":0,\"messages\":[],\"documents\":{}}\n";
static int closefd(int fd) {
  uv_fs_t r;
  int n = uv_fs_close(NULL, &r, fd, NULL);
  uv_fs_req_cleanup(&r);
  return n;
}
static int unlinkpath(const char *p) {
  uv_fs_t r;
  int n = uv_fs_unlink(NULL, &r, p, NULL);
  uv_fs_req_cleanup(&r);
  return n;
}
static int writeall(int fd, const char *p, size_t n) {
  size_t offset = 0;
  while (offset < n) {
    size_t chunk = n - offset;
    if (chunk > INT_MAX)
      chunk = INT_MAX;
    uv_buf_t b = uv_buf_init((char *)p + offset, (unsigned)chunk);
    uv_fs_t r;
    int z = uv_fs_write(NULL, &r, fd, &b, 1, (int64_t)offset, NULL);
    uv_fs_req_cleanup(&r);
    if (z <= 0)
      return -1;
    offset += (size_t)z;
  }
  uv_fs_t r;
  int z = uv_fs_fsync(NULL, &r, fd, NULL);
  uv_fs_req_cleanup(&r);
  return z;
}
static pg_session_status file_read(pangu_session *s,
                                   pg_session_diagnostic *diag) {
  uv_fs_t r;
  int fd = uv_fs_open(NULL, &r, s->path, O_RDONLY, 0, NULL);
  uv_fs_req_cleanup(&r);
  if (fd < 0) {
    if (diag)
      diag->native_code = fd;
    return fd == UV_ENOENT ? PG_SESSION_NOT_FOUND : PG_SESSION_IO;
  }
  int n = uv_fs_fstat(NULL, &r, fd, NULL);
  uint64_t len = n < 0 ? 0 : r.statbuf.st_size;
  uv_fs_req_cleanup(&r);
  if (n < 0 || len > SIZE_MAX - 1) {
    closefd(fd);
    return PG_SESSION_IO;
  }
  s->log = malloc((size_t)len + 1);
  if (!s->log) {
    closefd(fd);
    return PG_SESSION_NOMEM;
  }
  size_t off = 0;
  while (off < len) {
    size_t chunk = (size_t)len - off;
    if (chunk > INT_MAX)
      chunk = INT_MAX;
    uv_buf_t b = uv_buf_init(s->log + off, (unsigned)chunk);
    n = uv_fs_read(NULL, &r, fd, &b, 1, (int64_t)off, NULL);
    uv_fs_req_cleanup(&r);
    if (n <= 0) {
      closefd(fd);
      return PG_SESSION_IO;
    }
    off += (size_t)n;
  }
  closefd(fd);
  s->log_len = off;
  s->log[off] = 0;
  return PG_SESSION_OK;
}
pg_session_status pg_store_open(pangu_session *s, int create,
                                pg_session_diagnostic *diag) {
  if (create) {
    uv_fs_t r;
    int fd =
        uv_fs_open(NULL, &r, s->path, O_WRONLY | O_CREAT | O_EXCL, 0600, NULL);
    uv_fs_req_cleanup(&r);
    if (fd < 0) {
      if (diag)
        diag->native_code = fd;
      return PG_SESSION_IO;
    }
    int rc = s->backend == PG_STORE_JSONL
                 ? writeall(fd, initial, sizeof(initial) - 1)
                 : 0;
    int cc = closefd(fd);
    if (rc < 0 || cc < 0) {
      unlinkpath(s->path);
      return PG_SESSION_IO;
    }
  }
  if (s->backend == PG_STORE_JSONL)
    return file_read(s, diag);
  int rc = sqlite3_open_v2(
      s->path, &s->db,
      s->readonly ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE, NULL);
  if (rc != SQLITE_OK) {
    if (diag)
      diag->native_code = rc;
    return rc == SQLITE_CANTOPEN ? PG_SESSION_NOT_FOUND : PG_SESSION_IO;
  }
  if (create) {
    rc = sqlite3_exec(s->db,
                      "PRAGMA journal_mode=DELETE; PRAGMA synchronous=FULL; "
                      "CREATE TABLE pangu_session(id INTEGER PRIMARY KEY "
                      "CHECK(id=1),snapshot TEXT NOT NULL);",
                      NULL, NULL, NULL);
    if (rc != SQLITE_OK)
      return PG_SESSION_IO;
    pg_session_status z = pg_store_write(s, initial, sizeof(initial) - 1);
    if (z != PG_SESSION_OK)
      return z;
  }
  sqlite3_stmt *q = NULL;
  rc = sqlite3_prepare_v2(
      s->db, "SELECT snapshot FROM pangu_session WHERE id=1", -1, &q, NULL);
  if (rc != SQLITE_OK) {
    sqlite3_finalize(q);
    return PG_SESSION_CORRUPT;
  }
  rc = sqlite3_step(q);
  if (rc != SQLITE_ROW) {
    sqlite3_finalize(q);
    return PG_SESSION_CORRUPT;
  }
  int len = sqlite3_column_bytes(q, 0);
  const void *p = sqlite3_column_blob(q, 0);
  s->log = malloc((size_t)len + 1);
  if (!s->log) {
    sqlite3_finalize(q);
    return PG_SESSION_NOMEM;
  }
  memcpy(s->log, p, (size_t)len);
  s->log[len] = 0;
  s->log_len = (size_t)len;
  sqlite3_finalize(q);
  return PG_SESSION_OK;
}
pg_session_status pg_store_write(pangu_session *s, const char *data,
                                 size_t len) {
  if (s->backend == PG_STORE_SQLITE) {
    if (sqlite3_exec(s->db, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK)
      return PG_SESSION_IO;
    sqlite3_stmt *q = NULL;
    int rc = sqlite3_prepare_v2(
        s->db, "INSERT OR REPLACE INTO pangu_session VALUES(1,?)", -1, &q,
        NULL);
    if (rc == SQLITE_OK)
      rc = sqlite3_bind_text64(q, 1, data, (sqlite3_uint64)len, SQLITE_STATIC,
                               SQLITE_UTF8);
    if (rc == SQLITE_OK)
      rc = sqlite3_step(q);
    sqlite3_finalize(q);
    if (rc == SQLITE_DONE &&
        sqlite3_exec(s->db, "COMMIT", NULL, NULL, NULL) == SQLITE_OK)
      return PG_SESSION_OK;
    sqlite3_exec(s->db, "ROLLBACK", NULL, NULL, NULL);
    return PG_SESSION_IO;
  }
  /* Build a replacement log in the same directory. rename is the adoption
   * boundary: a failed pre-rename write leaves the old log byte-for-byte
   * intact. fsync of the file protects process-crash reopen; parent-directory
   * fsync / power-loss guarantees remain an explicit platform follow-up. */
  size_t plen = strlen(s->path);
  if (plen > SIZE_MAX - 40)
    return PG_SESSION_NOMEM;
  char *temp = malloc(plen + 40);
  if (!temp)
    return PG_SESSION_NOMEM;
  snprintf(temp, plen + 40, "%s.tmp.%llu", s->path,
           (unsigned long long)uv_hrtime());
  uv_fs_t r;
  int fd = uv_fs_open(NULL, &r, temp, O_WRONLY | O_CREAT | O_EXCL, 0600, NULL);
  uv_fs_req_cleanup(&r);
  if (fd < 0) {
    free(temp);
    return PG_SESSION_IO;
  }
  int rc = writeall(fd, data, len);
  int cc = closefd(fd);
  if (rc >= 0 && cc >= 0) {
    rc = uv_fs_rename(NULL, &r, temp, s->path, NULL);
    uv_fs_req_cleanup(&r);
  } else
    rc = -1;
  if (rc < 0)
    unlinkpath(temp);
  free(temp);
  return rc < 0 ? PG_SESSION_IO : PG_SESSION_OK;
}
