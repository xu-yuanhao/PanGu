/* StdinBuffer behavior tests ported from Pi packages/tui/test/
 * stdin-buffer.test.ts, snapshot d5b3de22. Uses the flush API instead of
 * timers: Pi tests flush pending data after a timeout; here the timeout
 * path is exercised through pangu_tui_buffer_flush(). */
#include <pangu/tui/buffer.h>
#include <pangu/tui/keys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                   \
      failures++;                                                              \
    }                                                                          \
  } while (0)

typedef struct {
  char sequences[256][16];
  size_t sequence_count;
  char pastes[16][128];
  size_t paste_count;
} recorder;

static void on_sequence(void *ctx, const uint8_t *data, size_t len) {
  recorder *r = ctx;
  if (r->sequence_count >= 256)
    return;
  if (len > 15)
    len = 15;
  memcpy(r->sequences[r->sequence_count], data, len);
  r->sequences[r->sequence_count][len] = 0;
  r->sequence_count++;
}

static void on_paste(void *ctx, const uint8_t *data, size_t len) {
  recorder *r = ctx;
  if (r->paste_count >= 16)
    return;
  if (len > 127)
    len = 127;
  memcpy(r->pastes[r->paste_count], data, len);
  r->pastes[r->paste_count][len] = 0;
  r->paste_count++;
}

static void feed(pangu_tui_buffer *b, recorder *r, const char *data) {
  pangu_tui_buffer_process(b, (const uint8_t *)data, strlen(data));
}

static void check_sequences(const char *label, recorder *r,
                            const char **expected, size_t count) {
  if (r->sequence_count != count) {
    printf("FAIL %s: expected %zu sequences, got %zu\n", label, count,
           r->sequence_count);
    failures++;
    return;
  }
  for (size_t i = 0; i < count; ++i) {
    if (strcmp(r->sequences[i], expected[i]) != 0) {
      printf("FAIL %s: sequence %zu: expected \"%s\", got \"%s\"\n", label, i,
             expected[i], r->sequences[i]);
      failures++;
    }
  }
}

static void reset(recorder *r) { memset(r, 0, sizeof *r); }

static void test_regular_characters(void) {
  recorder r;
  reset(&r);
  pangu_tui_buffer *b = pangu_tui_buffer_create();
  pangu_tui_buffer_set_callbacks(b, on_sequence, on_paste, NULL, &r);

  feed(b, &r, "a");
  const char *expected1[] = {"a"};
  check_sequences("single char", &r, expected1, 1);

  reset(&r);
  feed(b, &r, "abc");
  const char *expected2[] = {"a", "b", "c"};
  check_sequences("multiple chars", &r, expected2, 3);

  reset(&r);
  feed(b, &r, "hello \xe4\xb8\x96\xe7\x95\x8c");
  const char *expected3[] = {
      "h", "e", "l", "l", "o", " ", "\xe4\xb8\x96", "\xe7\x95\x8c"};
  check_sequences("unicode", &r, expected3, 8);

  pangu_tui_buffer_destroy(b);
}

static void test_complete_sequences(void) {
  recorder r;
  reset(&r);
  pangu_tui_buffer *b = pangu_tui_buffer_create();
  pangu_tui_buffer_set_callbacks(b, on_sequence, on_paste, NULL, &r);

  feed(b, &r, "\x1b[<35;20;5m");
  const char *expected1[] = {"\x1b[<35;20;5m"};
  check_sequences("mouse SGR", &r, expected1, 1);

  reset(&r);
  feed(b, &r, "\x1b[A");
  const char *expected2[] = {"\x1b[A"};
  check_sequences("arrow", &r, expected2, 1);

  reset(&r);
  feed(b, &r, "\x1b[11~");
  const char *expected3[] = {"\x1b[11~"};
  check_sequences("f1", &r, expected3, 1);

  reset(&r);
  feed(b, &r,
       "\x1b"
       "a");
  const char *expected4[] = {"\x1b"
                             "a"};
  check_sequences("meta", &r, expected4, 1);

  reset(&r);
  feed(b, &r, "\x1bOA");
  const char *expected5[] = {"\x1bOA"};
  check_sequences("ss3", &r, expected5, 1);

  pangu_tui_buffer_destroy(b);
}

static void test_partial_sequences(void) {
  recorder r;
  reset(&r);
  pangu_tui_buffer *b = pangu_tui_buffer_create();
  pangu_tui_buffer_set_callbacks(b, on_sequence, on_paste, NULL, &r);

  feed(b, &r, "\x1b");
  CHECK(r.sequence_count == 0);
  CHECK(pangu_tui_buffer_pending(b) == 1);
  feed(b, &r, "[<35");
  CHECK(r.sequence_count == 0);
  CHECK(pangu_tui_buffer_pending(b) == 5);
  feed(b, &r, ";20;5m");
  const char *expected[] = {"\x1b[<35;20;5m"};
  check_sequences("split mouse", &r, expected, 1);
  CHECK(pangu_tui_buffer_pending(b) == 0);

  reset(&r);
  feed(b, &r, "\x1b[");
  CHECK(r.sequence_count == 0);
  feed(b, &r, "1;");
  CHECK(r.sequence_count == 0);
  feed(b, &r, "5H");
  const char *expected2[] = {"\x1b[1;5H"};
  check_sequences("split CSI", &r, expected2, 1);

  /* Split across many single chunks. */
  reset(&r);
  feed(b, &r, "\x1b");
  feed(b, &r, "[");
  feed(b, &r, "<");
  feed(b, &r, "3");
  feed(b, &r, "5");
  feed(b, &r, ";");
  feed(b, &r, "2");
  feed(b, &r, "0");
  feed(b, &r, ";");
  feed(b, &r, "5");
  feed(b, &r, "m");
  const char *expected3[] = {"\x1b[<35;20;5m"};
  check_sequences("byte-by-byte mouse", &r, expected3, 1);

  /* Flush an incomplete sequence. */
  reset(&r);
  feed(b, &r, "\x1b[<35");
  CHECK(r.sequence_count == 0);
  pangu_tui_buffer_flush(b);
  const char *expected4[] = {"\x1b[<35"};
  check_sequences("timeout flush", &r, expected4, 1);

  /* Lone ESC flushes separately; a later CR is its own event (Pi: escape
   * key instead of alt+enter when the split outlives the timeout). */
  reset(&r);
  feed(b, &r, "\x1b");
  pangu_tui_buffer_flush(b);
  feed(b, &r, "\r");
  const char *expected5[] = {"\x1b", "\r"};
  check_sequences("lone esc then CR", &r, expected5, 2);
  CHECK(pangu_tui_matches_key((const uint8_t *)r.sequences[0],
                              strlen(r.sequences[0]), "escape"));

  pangu_tui_buffer_destroy(b);
}

static void test_mixed_content(void) {
  recorder r;
  reset(&r);
  pangu_tui_buffer *b = pangu_tui_buffer_create();
  pangu_tui_buffer_set_callbacks(b, on_sequence, on_paste, NULL, &r);

  feed(b, &r, "abc\x1b[A");
  const char *expected1[] = {"a", "b", "c", "\x1b[A"};
  check_sequences("text then escape", &r, expected1, 4);

  reset(&r);
  feed(b, &r, "\x1b[Aabc");
  const char *expected2[] = {"\x1b[A", "a", "b", "c"};
  check_sequences("escape then text", &r, expected2, 4);

  reset(&r);
  feed(b, &r, "\x1b[A\x1b[B\x1b[C");
  const char *expected3[] = {"\x1b[A", "\x1b[B", "\x1b[C"};
  check_sequences("multiple sequences", &r, expected3, 3);

  reset(&r);
  feed(b, &r, "abc\x1b[<35");
  const char *expected4[] = {"a", "b", "c"};
  check_sequences("partial with text", &r, expected4, 3);
  CHECK(pangu_tui_buffer_pending(b) == 5);
  feed(b, &r, ";20;5m");
  const char *expected5[] = {"a", "b", "c", "\x1b[<35;20;5m"};
  check_sequences("complete after partial", &r, expected5, 4);

  pangu_tui_buffer_destroy(b);
}

static void test_kitty_events(void) {
  recorder r;
  reset(&r);
  pangu_tui_buffer *b = pangu_tui_buffer_create();
  pangu_tui_buffer_set_callbacks(b, on_sequence, on_paste, NULL, &r);

  feed(b, &r, "\x1b[97u");
  const char *expected1[] = {"\x1b[97u"};
  check_sequences("kitty press", &r, expected1, 1);

  reset(&r);
  feed(b, &r, "\x1b[97;1:3u");
  const char *expected2[] = {"\x1b[97;1:3u"};
  check_sequences("kitty release", &r, expected2, 1);

  reset(&r);
  feed(b, &r, "\x1b[97u\x1b[97;1:3u");
  const char *expected3[] = {"\x1b[97u", "\x1b[97;1:3u"};
  check_sequences("batched press+release", &r, expected3, 2);

  reset(&r);
  feed(b, &r, "\x1b[104u\x1b[104;1:3u\x1b[105u\x1b[105;1:3u");
  const char *expected4[] = {"\x1b[104u", "\x1b[104;1:3u", "\x1b[105u",
                             "\x1b[105;1:3u"};
  check_sequences("rapid typing", &r, expected4, 4);

  reset(&r);
  feed(b, &r, "\x1b[1;1:1A");
  const char *expected5[] = {"\x1b[1;1:1A"};
  check_sequences("kitty arrow event", &r, expected5, 1);

  reset(&r);
  feed(b, &r, "\x1b[3;1:3~");
  const char *expected6[] = {"\x1b[3;1:3~"};
  check_sequences("kitty delete release", &r, expected6, 1);

  /* WezTerm escape key: ESC + ESC + CSI split into standalone ESC and CSI. */
  reset(&r);
  feed(b, &r, "\x1b\x1b[27;129:3u");
  const char *expected7[] = {"\x1b", "\x1b[27;129:3u"};
  check_sequences("wezterm escape key", &r, expected7, 2);

  reset(&r);
  feed(b, &r, "\x1b\x1b[27;1:3u");
  const char *expected8[] = {"\x1b", "\x1b[27;1:3u"};
  check_sequences("wezterm escape no numlock", &r, expected8, 2);

  reset(&r);
  feed(b, &r, "\x1b\x1b");
  const char *expected9[] = {"\x1b\x1b"};
  check_sequences("double esc alone", &r, expected9, 1);

  /* Duplicate raw char after kitty printable is dropped. */
  reset(&r);
  feed(b, &r, "\x1b[64u@");
  const char *expected10[] = {"\x1b[64u"};
  check_sequences("kitty printable duplicate", &r, expected10, 1);

  reset(&r);
  feed(b, &r, "\x1b[64u");
  feed(b, &r, "@");
  const char *expected11[] = {"\x1b[64u"};
  check_sequences("kitty duplicate across chunks", &r, expected11, 1);

  reset(&r);
  feed(b, &r, "\x1b[97ub");
  const char *expected12[] = {"\x1b[97u", "b"};
  check_sequences("non-matching char kept", &r, expected12, 2);

  reset(&r);
  feed(b, &r, "\x1b[64;3u@");
  const char *expected13[] = {"\x1b[64;3u", "@"};
  check_sequences("modified printable keeps raw", &r, expected13, 2);

  pangu_tui_buffer_destroy(b);
}

static void test_mouse_events(void) {
  recorder r;
  reset(&r);
  pangu_tui_buffer *b = pangu_tui_buffer_create();
  pangu_tui_buffer_set_callbacks(b, on_sequence, on_paste, NULL, &r);

  feed(b, &r, "\x1b[<0;10;5M");
  const char *expected1[] = {"\x1b[<0;10;5M"};
  check_sequences("mouse press", &r, expected1, 1);

  reset(&r);
  feed(b, &r, "\x1b[<0;10;5m");
  const char *expected2[] = {"\x1b[<0;10;5m"};
  check_sequences("mouse release", &r, expected2, 1);

  reset(&r);
  feed(b, &r, "\x1b[<35;20;5m");
  const char *expected3[] = {"\x1b[<35;20;5m"};
  check_sequences("mouse move", &r, expected3, 1);

  reset(&r);
  feed(b, &r, "\x1b[<3");
  feed(b, &r, "5;1");
  feed(b, &r, "5;");
  feed(b, &r, "10m");
  const char *expected4[] = {"\x1b[<35;15;10m"};
  check_sequences("split mouse", &r, expected4, 1);

  reset(&r);
  feed(b, &r, "\x1b[<35;1;1m\x1b[<35;2;2m\x1b[<35;3;3m");
  const char *expected5[] = {"\x1b[<35;1;1m", "\x1b[<35;2;2m", "\x1b[<35;3;3m"};
  check_sequences("multiple mouse", &r, expected5, 3);

  /* Old-style mouse: ESC[M + 3 bytes. */
  reset(&r);
  feed(b, &r, "\x1b[M abc");
  const char *expected6[] = {"\x1b[M ab", "c"};
  check_sequences("old-style mouse", &r, expected6, 2);

  reset(&r);
  feed(b, &r, "\x1b[M");
  CHECK(pangu_tui_buffer_pending(b) == 3);
  feed(b, &r, " a");
  CHECK(pangu_tui_buffer_pending(b) == 5);
  feed(b, &r, "b");
  const char *expected7[] = {"\x1b[M ab"};
  check_sequences("split old-style mouse", &r, expected7, 1);

  pangu_tui_buffer_destroy(b);
}

static void test_bracketed_paste(void) {
  recorder r;
  reset(&r);
  pangu_tui_buffer *b = pangu_tui_buffer_create();
  pangu_tui_buffer_set_callbacks(b, on_sequence, on_paste, NULL, &r);

  feed(b, &r, "\x1b[200~hello world\x1b[201~");
  CHECK(r.sequence_count == 0);
  CHECK(r.paste_count == 1 && strcmp(r.pastes[0], "hello world") == 0);

  reset(&r);
  feed(b, &r, "\x1b[200~");
  CHECK(r.paste_count == 0);
  feed(b, &r, "hello ");
  CHECK(r.paste_count == 0);
  feed(b, &r, "world\x1b[201~");
  CHECK(r.paste_count == 1 && strcmp(r.pastes[0], "hello world") == 0);
  CHECK(r.sequence_count == 0);

  reset(&r);
  feed(b, &r, "a");
  feed(b, &r, "\x1b[200~pasted\x1b[201~");
  feed(b, &r, "b");
  const char *expected[] = {"a", "b"};
  check_sequences("input around paste", &r, expected, 2);
  CHECK(r.paste_count == 1 && strcmp(r.pastes[0], "pasted") == 0);

  reset(&r);
  feed(b, &r, "\x1b[200~line1\nline2\nline3\x1b[201~");
  CHECK(r.paste_count == 1 && strcmp(r.pastes[0], "line1\nline2\nline3") == 0);

  reset(&r);
  feed(b, &r,
       "\x1b[200~Hello \xe4\xb8\x96\xe7\x95\x8c \xf0\x9f\x8e\x89\x1b[201~");
  CHECK(r.paste_count == 1);
  CHECK(strcmp(r.pastes[0],
               "Hello \xe4\xb8\x96\xe7\x95\x8c \xf0\x9f\x8e\x89") == 0);

  pangu_tui_buffer_destroy(b);
}

static void test_clear_and_flush(void) {
  recorder r;
  reset(&r);
  pangu_tui_buffer *b = pangu_tui_buffer_create();
  pangu_tui_buffer_set_callbacks(b, on_sequence, on_paste, NULL, &r);

  feed(b, &r, "\x1b[<35");
  CHECK(pangu_tui_buffer_pending(b) == 5);
  pangu_tui_buffer_clear(b);
  CHECK(pangu_tui_buffer_pending(b) == 0);
  pangu_tui_buffer_flush(b);
  CHECK(r.sequence_count == 0);
  CHECK(pangu_tui_buffer_pending(b) == 0);

  /* Empty flush is a no-op. */
  pangu_tui_buffer_flush(b);
  CHECK(r.sequence_count == 0);

  pangu_tui_buffer_destroy(b);
}

static void test_long_sequences(void) {
  recorder r;
  reset(&r);
  pangu_tui_buffer *b = pangu_tui_buffer_create();
  pangu_tui_buffer_set_callbacks(b, on_sequence, on_paste, NULL, &r);

  char long_seq[128];
  size_t used = 0;
  memcpy(long_seq, "\x1b[", 2);
  used = 2;
  for (int i = 0; i < 50; ++i) {
    long_seq[used++] = '1';
    long_seq[used++] = ';';
  }
  long_seq[used++] = 'H';
  long_seq[used] = 0;
  feed(b, &r, long_seq);
  CHECK(r.sequence_count == 1);

  pangu_tui_buffer_destroy(b);
}

typedef struct {
  size_t length, events;
  unsigned long checksum;
} large_paste;
static void big_paste(void *ctx, const uint8_t *data, size_t n) {
  large_paste *r = ctx;
  r->events++;
  r->length = n;
  for (size_t i = 0; i < n; i++)
    r->checksum += data[i];
}
static void test_clock_and_large_paste(void) {
  recorder r;
  reset(&r);
  pangu_tui_buffer *b = pangu_tui_buffer_create();
  CHECK(b != NULL);
  pangu_tui_buffer_set_callbacks(b, on_sequence, on_paste, NULL, &r);
  pangu_tui_buffer_tick(b, 100);
  feed(b, &r, "\x1b");
  pangu_tui_buffer_tick(b, 109);
  CHECK(r.sequence_count == 0);
  pangu_tui_buffer_tick(b, 110);
  CHECK(r.sequence_count == 1 && r.sequences[0][0] == 27);
  pangu_tui_buffer_clear(b);
  large_paste big = {0};
  pangu_tui_buffer_set_callbacks(b, NULL, big_paste, NULL, &big);
  const size_t size = 200000;
  char *payload = malloc(size);
  CHECK(payload != NULL);
  memset(payload, 'x', size);
  pangu_tui_buffer_process(b, (const uint8_t *)"\x1b[200~", 6);
  pangu_tui_buffer_process(b, (const uint8_t *)payload, size);
  pangu_tui_buffer_process(b, (const uint8_t *)"\x1b[20", 4);
  pangu_tui_buffer_process(b, (const uint8_t *)"1~", 2);
  CHECK(big.events == 1 && big.length == size &&
        big.checksum == size * (unsigned long)'x');
  CHECK(!pangu_tui_buffer_failed(b));
  free(payload);
  pangu_tui_buffer_destroy(b);
}
int main(void) {
  test_clock_and_large_paste();
  test_regular_characters();
  test_complete_sequences();
  test_partial_sequences();
  test_mixed_content();
  test_kitty_events();
  test_mouse_events();
  test_bracketed_paste();
  test_clear_and_flush();
  test_long_sequences();
  if (failures) {
    printf("buffer: %d failures\n", failures);
    return 1;
  }
  printf("buffer: all tests passed\n");
  return 0;
}
