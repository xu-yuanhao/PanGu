#ifndef PANGU_SERVICES_IMAGES_H
#define PANGU_SERVICES_IMAGES_H
#include <pangu/core/core.h>
#include <pangu/services/host.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PG_IMAGES_SERVICE "pangu.images"
/* Synchronous caller-thread preparation. INVALID means not an image; other
 * errors mean a recognized image failed preparation. Input is borrowed bytes.
 * Success returns malloc-owned base64 and a static borrowed MIME slice; caller
 * frees base64 after the model/tool sink has returned. Limits: 2000x2000 pixels,
 * 4.5 MiB base64. Cancellation is observed before/after codec work; unload waits
 * for the acquired service lease. No callbacks or operations survive return. */
typedef struct {
  pangu_status (*prepare)(void *, pangu_core *, pangu_slice, char **,
                           size_t *, pangu_slice *);
} pg_images_service;
/* Optional GdkPixbuf image codec plugin, isolated from Core and the file host.
 * Supports PNG/JPEG/GIF/WebP/BMP when the installed codec supports that format.
 * First frame, orientation applied, proportionally resized and encoded as PNG. */
extern const pg_plugin pg_images_plugin;
#ifdef __cplusplus
}
#endif
#endif
