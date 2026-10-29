#include "image_processor.h"
#include <vips/vips.h>
#include <stdlib.h>
#include <string.h>

static int ready(void){
 static gsize initialized;
 if(g_once_init_enter(&initialized)){
  int failed=vips_init("PanGu");g_once_init_leave(&initialized,failed?2:1);
 }
 return initialized==1;
}
int pg_image_codec_open(pg_slice bytes,void **out,unsigned *width,unsigned *height,unsigned *orientation){
 if(!out||!width||!height||!orientation)return 1;*out=0;
 if(!bytes.data||!bytes.size||!ready())return 1;
 VipsImage *loaded=vips_image_new_from_buffer(bytes.data,bytes.size,"","access",VIPS_ACCESS_RANDOM,"fail_on",VIPS_FAIL_ON_ERROR,NULL);
 if(!loaded){vips_error_clear();return 1;}
 VipsImage *image=vips_image_copy_memory(loaded);g_object_unref(loaded);
 if(!image){vips_error_clear();return 1;}
 int w=vips_image_get_width(image),h=vips_image_get_height(image);
 if(w<=0||h<=0){g_object_unref(image);return 1;}
 *width=(unsigned)w;*height=(unsigned)h;int rotation=vips_image_get_orientation(image);*orientation=rotation>=1&&rotation<=8?(unsigned)rotation:1;*out=image;return 0;
}
int pg_image_codec_orient(void *source,void **out,unsigned *width,unsigned *height){
 if(!source||!out||!width||!height)return 1;*out=0;
 VipsImage *image=0;if(vips_autorot(source,&image,NULL)){vips_error_clear();return 1;}
 *width=(unsigned)vips_image_get_width(image);*height=(unsigned)vips_image_get_height(image);*out=image;return 0;
}
int pg_image_codec_encode(void *source,const pg_image_codec_target *target,pg_slice *out){
 if(!source||!target||!out||!target->width||!target->height)return 1;*out=(pg_slice){0};
 VipsImage *scaled=0;void *buffer=0;size_t size=0;
 if(vips_resize(source,&scaled,(double)target->width/vips_image_get_width(source),"vscale",(double)target->height/vips_image_get_height(source),"kernel",VIPS_KERNEL_LANCZOS3,NULL)){vips_error_clear();return 1;}
 int failed=target->format?vips_jpegsave_buffer(scaled,&buffer,&size,"Q",(int)target->quality,"strip",TRUE,NULL):vips_pngsave_buffer(scaled,&buffer,&size,"strip",TRUE,NULL);g_object_unref(scaled);
 if(failed){g_free(buffer);vips_error_clear();return 1;}
 void *copy=malloc(size);if(!copy){g_free(buffer);return 5;}memcpy(copy,buffer,size);g_free(buffer);*out=(pg_slice){copy,size};return 0;
}
void pg_image_codec_close(void *image){if(image)g_object_unref(image);}
