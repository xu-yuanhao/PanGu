#define _GNU_SOURCE
#include "../../src/runtime/stream_decode_layout.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
static unsigned char input[4096];static char output[12291];
static int digit(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;assert(0);return 0;}
static void hex(const char *p,size_t n){for(size_t i=0;i<n;++i)printf("%02x",(unsigned char)p[i]);}
static void boundary(void){
 pg_stream_decoder d;pg_stream_decoder_init(&d);pg_stream_decoder before=d;size_t n=99;
 assert(pg_stream_decode(&d,(pg_slice){"a",1},0,output,5,&n)&&!n&&!memcmp(&d,&before,sizeof(d)));
 assert(pg_stream_decode(&d,(pg_slice){0,1},0,output,sizeof(output),&n)&&!memcmp(&d,&before,sizeof(d)));
 assert(pg_stream_decode(&d,(pg_slice){"",4097},0,output,sizeof(output),&n));
 assert(!pg_stream_decode(&d,(pg_slice){"\xef\xbb",2},0,output,sizeof(output),&n)&&!n);
 before=d;assert(pg_stream_decode(&d,(pg_slice){"\xbf",1},0,output,5,&n)&&!memcmp(&d,&before,sizeof(d)));
 assert(!pg_stream_decode(&d,(pg_slice){"\xbf",1},0,output,sizeof(output),&n)&&!n);
 assert(!pg_stream_decode(&d,(pg_slice){"\xef\xbb\xbf",3},1,output,sizeof(output),&n)&&n==3&&!memcmp(output,"\xef\xbb\xbf",3));
 assert(!pg_stream_decode(&d,(pg_slice){0,0},1,output,3,&n)&&!n);
 before=d;assert(pg_stream_decode(&d,(pg_slice){"a",1},0,output,sizeof(output),&n)&&!memcmp(&d,&before,sizeof(d)));
 pg_stream_decoder a,b;pg_stream_decoder_init(&a);pg_stream_decoder_init(&b);
 assert(!pg_stream_decode(&a,(pg_slice){"\xe4",1},0,output,sizeof(output),&n)&&!n);
 assert(!pg_stream_decode(&b,(pg_slice){"\xb8\xad",2},0,output,sizeof(output),&n)&&n==6);
 assert(!pg_stream_decode(&a,(pg_slice){"\xb8\xad",2},1,output,sizeof(output),&n)&&n==3&&!memcmp(output,"\xe4\xb8\xad",3));
 long page=sysconf(_SC_PAGESIZE);assert(page>=4096);char *map=mmap(0,(size_t)page*5,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(map!=MAP_FAILED);assert(!mprotect(map+page,(size_t)page,PROT_READ|PROT_WRITE));pg_stream_decoder *tail=(pg_stream_decoder*)(map+2*page-sizeof(d));pg_stream_decoder_init(tail);
 const char *bytes=map+page;memset((char *)bytes,0xff,1024);assert(!pg_stream_decode(tail,(pg_slice){bytes,1024},1,output,sizeof(output),&n)&&n==3072);
 assert(!mprotect(map+3*page,(size_t)page,PROT_READ|PROT_WRITE));
 char *out_tail=map+4*page-3;pg_stream_decoder_init(tail);
 assert(!pg_stream_decode(tail,(pg_slice){"\xe4",1},0,output,sizeof(output),&n)&&!n);
 assert(!pg_stream_decode(tail,(pg_slice){0,0},1,out_tail,3,&n)&&n==3&&!memcmp(out_tail,"\xef\xbf\xbd",3));
 pg_stream_decoder_init(tail);char *input_tail=map+4*page-1;*input_tail=(char)0xff;
 assert(!pg_stream_decode(tail,(pg_slice){input_tail,1},1,out_tail-3,6,&n)&&n==3);
 assert(!munmap(map,(size_t)page*5));
}
int main(int argc,char **argv){
 if(argc==1){boundary();puts("stream decode: BOM, malformed prefixes, per-stream state, final/capacity and guard OK");return 0;}
 assert(argc==2&&!strcmp(argv[1],"--oracle"));char *line=0;size_t cap=0;
 while(getline(&line,&cap,stdin)>0){char *split=strchr(line,'|');assert(split);*split++=0;size_t n=strlen(line)/2;assert(n<=sizeof(input)&&strlen(line)%2==0);for(size_t i=0;i<n;++i)input[i]=(unsigned char)(digit(line[i*2])*16+digit(line[i*2+1]));pg_stream_decoder d;pg_stream_decoder_init(&d);size_t pos=0,written;char *save=0;
  for(char *part=strtok_r(split,",\n",&save);part;part=strtok_r(0,",\n",&save)){size_t count=strtoul(part,0,10);assert(count<=n-pos);assert(!pg_stream_decode(&d,(pg_slice){(char *)input+pos,count},0,output,sizeof(output),&written));hex(output,written);pos+=count;putchar(',');}
  assert(pos==n);assert(!pg_stream_decode(&d,(pg_slice){0,0},1,output,sizeof(output),&written));hex(output,written);putchar('\n');
 }
 free(line);return 0;
}
