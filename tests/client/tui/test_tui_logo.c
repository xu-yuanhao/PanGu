#define _POSIX_C_SOURCE 200809L
#include "tui_logo.h"
#include "tui_view.h"
#include <assert.h>
#include <math.h>
#include <json-c/json.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
/* Independent ray-crossing oracle over the supplied SVG's four vertices. */
static double polygons[2][4][2], gap_half, angle;
static int oracle(double x,double y,unsigned phase){
 double close=1-phase/65536.0;
 for(unsigned p=0;p<2;p++){
  double sign=p?-1:1,px=x-sign*gap_half*sin(angle)*close,py=y-sign*gap_half*cos(angle)*close;
  int inside=0;
  for(unsigned i=0,j=3;i<4;j=i++){
   double ax=polygons[p][i][0],ay=polygons[p][i][1],bx=polygons[p][j][0],by=polygons[p][j][1];
   if((ay>py)!=(by>py) && px<(bx-ax)*(py-ay)/(by-ay)+ax)inside=!inside;
  }
  if(inside)return 1;
 }
 return 0;
}
static double sec(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static size_t writes,bytes;
static int sink(void *ctx,const char *p,size_t n){(void)ctx;(void)p;writes++;bytes+=n;return 0;}
int main(void){
 struct json_object *geometry=json_object_from_file("assets/branding/pangu-logo-geometry.json");assert(geometry);
 struct json_object *cut=json_object_object_get(geometry,"cut");
 gap_half=json_object_get_double(json_object_object_get(cut,"perpendicular_gap"))/2;angle=json_object_get_double(json_object_object_get(cut,"angle_degrees"))*3.14159265358979323846/180;
 struct json_object *vertices=json_object_object_get(geometry,"polygon_vertices"),*paths=json_object_object_get(geometry,"paths");
 FILE *svg=fopen("assets/branding/pangu-logo.svg","r");assert(svg);char xml[4096];size_t xmln=fread(xml,1,sizeof(xml)-1,svg);xml[xmln]=0;fclose(svg);
 const char *names[]={"sky","earth"};
 for(unsigned p=0;p<2;p++){
  char attr[512];snprintf(attr,sizeof(attr),"d=\"%s\"",json_object_get_string(json_object_object_get(paths,names[p])));assert(strstr(xml,attr));
  struct json_object *poly=json_object_object_get(vertices,names[p]);assert(json_object_array_length(poly)==4);
  for(unsigned v=0;v<4;v++)for(unsigned axis=0;axis<2;axis++)polygons[p][v][axis]=json_object_get_double(json_object_array_get_idx(json_object_array_get_idx(poly,v),axis));
 }
 json_object_put(geometry);
 unsigned char guarded[1602];memset(guarded,0xa5,sizeof(guarded));
 assert(!tui_logo_raster(guarded+1,41,20,131072,65536,1600));
 assert(!tui_logo_raster(guarded+1,40,20,131072,65536,1599));
 const unsigned aspects[]={98304,131072,163840};const unsigned phases[]={0,32768,65536};
 const unsigned bits[]={1,8,2,16,4,32,64,128};unsigned errors=0;
 for(unsigned a=0;a<3;a++)for(unsigned p=0;p<3;p++){
  unsigned w=40,h=20;assert(tui_logo_raster(guarded+1,w,h,aspects[a],phases[p],1600)==800);
  assert(guarded[0]==0xa5&&guarded[1601]==0xa5);
  double ar=aspects[a]/65536.0,scale=fmin(w/64.0,h*ar/64.0),sx=1/(2*scale),sy=ar/(4*scale);
  double ox=32-w*sx,oy=32-2*h*sy;
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
   unsigned mask=0;
   for(unsigned k=0;k<8;k++){
    unsigned n=0;
    for(unsigned j=0;j<4;j++)n+=(unsigned)oracle(ox+(2*x+(k&1)+((j&1)?.75:.25))*sx,oy+(4*y+k/2+(j>=2?.75:.25))*sy,phases[p]);
    if(n>=2)mask|=bits[k];
   }
   if(mask!=guarded[1+(y*w+x)*2])errors++;
  }
 }
 assert(errors<=4); /* fixed-point quantization only at exact boundary samples */
 assert(!tui_logo_contains(32*65536,32*65536,65536));
 assert(tui_logo_contains(32*65536,31*65536,0));
 double t=sec();for(unsigned i=0;i<100;i++)tui_logo_raster(guarded+1,40,20,131072,i*655,1600);
 printf("logo raster: 100 max frames %.2f ms; oracle cell differences %u\n",(sec()-t)*1000,errors);
 tui_view view;tui_input input;tui_input_init(&input);tui_view_init(&view,(tui_out_sink){NULL,sink},1,&input);
 view.terminal_rows=32;tui_view_banner(&view,"test");
 view.logo_flags=3;view.logo_phase=0;view.logo_start_ms=view.logo_last_ms=0;
 assert(tui_view_logo_tick(&view,1000)==50);
 assert(tui_view_logo_tick(&view,1020)==30);
 assert(tui_view_logo_tick(&view,1050)==50);
 assert(tui_view_logo_tick(&view,3000)==-1&&view.logo_phase==65536);
 size_t old=bytes;for(unsigned i=0;i<100;i++)assert(tui_view_logo_tick(&view,4000+i)==-1);assert(bytes==old);
 view.logo_flags=7;view.logo_phase=0;old=bytes;assert(tui_view_logo_tick(&view,5000)==-1);assert(bytes==old);
 view.logo_flags=3;view.logo_phase=0;view.reading=1;old=bytes;assert(tui_view_logo_tick(&view,6000)==-1);assert(bytes==old);
 view.reading=0;view.transcript_len=1;assert(tui_view_logo_tick(&view,6000)==-1);assert(bytes==old);
 printf("logo timing: completion/static/reduced-motion/reading/history emit no extra frames; writes=%zu bytes=%zu\n",writes,bytes);
 return 0;
}
