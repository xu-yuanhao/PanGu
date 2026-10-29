#ifndef PANGU_IMAGE_PROCESSOR_H
#define PANGU_IMAGE_PROCESSOR_H
#include "read_tool.h"
/* Pi Durable images.prepare policy. Returns owned prepared JSON, 1 when the
   image cannot fit/decode, 5 on allocation/contract failure. Inputs borrowed. */
int pg_image_prepare(void *,pg_slice,const char *,const pg_read_image_limits *,struct json_object **);
/* External codec handles and encoded malloc bytes are caller-owned. */
typedef struct {unsigned width,height,format,quality;} pg_image_codec_target;
int pg_image_codec_open(pg_slice,void **,unsigned *,unsigned *,unsigned *);
int pg_image_codec_orient(void *,void **,unsigned *,unsigned *);
int pg_image_codec_encode(void *,const pg_image_codec_target *,pg_slice *);
void pg_image_codec_close(void *);
/* Caller supplies ceil(bytes.size/3)*4+1 bytes; output is NUL terminated. */
void pg_image_base64(pg_slice,char *);
#endif
