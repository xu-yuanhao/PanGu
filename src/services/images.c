#include <pangu/services/images.h>
#include <pangu/core/loop.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdlib.h>
#include <string.h>
static int recognized(pangu_slice b) {
  return (b.len >= 8 && !memcmp(b.data, "\x89PNG\r\n\x1a\n", 8)) ||
    (b.len >= 3 && !memcmp(b.data, "\xff\xd8\xff", 3)) ||
    (b.len >= 6 && (!memcmp(b.data, "GIF87a", 6) || !memcmp(b.data, "GIF89a", 6))) ||
    (b.len >= 12 && !memcmp(b.data, "RIFF", 4) && !memcmp(b.data+8, "WEBP", 4)) ||
    (b.len >= 2 && !memcmp(b.data, "BM", 2));
}
static void size_prepared(GdkPixbufLoader *loader, int width, int height, gpointer ctx) {
  (void)ctx;
  if (width > 2000 || height > 2000) {
    double ratio = 2000.0 / (width > height ? width : height);
    int w = (int)(width * ratio), h = (int)(height * ratio);
    gdk_pixbuf_loader_set_size(loader, w > 0 ? w : 1, h > 0 ? h : 1);
  }
}
static pangu_status prepare(void *ctx, pangu_core *owner, pangu_slice input,
                            char **output, size_t *length, pangu_slice *mime) {
  (void)ctx;
  *output = NULL; *length = 0; *mime = (pangu_slice){0};
  if (!recognized(input)) return PANGU_INVALID;
  if (pangu_core_is_cancelled(owner)) return PANGU_CANCELLED;
  GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
  if (!loader) return PANGU_NOMEM;
  g_signal_connect(loader, "size-prepared", G_CALLBACK(size_prepared), NULL);
  GError *error = NULL;
  gboolean wrote = gdk_pixbuf_loader_write(loader, (const guchar *)input.data, input.len, &error);
  if (error) { g_error_free(error); error = NULL; }
  gboolean closed = gdk_pixbuf_loader_close(loader, &error);
  if (error) { g_error_free(error); error = NULL; }
  pangu_status st = PANGU_TOOL_ERROR;
  GdkPixbuf *image = wrote && closed ? gdk_pixbuf_loader_get_pixbuf(loader) : NULL;
  if (image) {
    GdkPixbuf *oriented = gdk_pixbuf_apply_embedded_orientation(image);
    if (!oriented) { st = PANGU_NOMEM; goto done; }
    gchar *png = NULL;
    gsize png_len = 0;
    gboolean saved = gdk_pixbuf_save_to_buffer(oriented, &png, &png_len, "png", &error, NULL);
    g_object_unref(oriented);
    if (error) { g_error_free(error); error = NULL; }
    if (saved && png_len <= (4718592u / 4u) * 3u) {
      gchar *encoded = g_base64_encode((const guchar *)png, png_len);
      if (encoded) {
        size_t n = strlen(encoded);
        *output = malloc(n+1);
        if (*output) { memcpy(*output, encoded, n+1); *length = n; *mime = (pangu_slice){"image/png", 9}; st = PANGU_OK; }
        else st = PANGU_NOMEM;
        g_free(encoded);
      } else st = PANGU_NOMEM;
    }
    g_free(png);
  }
done:
  g_object_unref(loader);
  if (pangu_core_is_cancelled(owner)) { free(*output); *output = NULL; st = PANGU_CANCELLED; }
  return st;
}
static const pg_images_service service = {prepare};
static pg_status install(pg_scope *scope, const void *cfg, size_t n) {
  (void)cfg;
  if (n) return PG_INVALID;
  return pg_service_register(scope, PG_NAME(PG_IMAGES_SERVICE), 1, &service, NULL);
}
const pg_plugin pg_images_plugin = {1, sizeof(pg_plugin), {"pangu.images.gdk", 16}, NULL, 0, install};
