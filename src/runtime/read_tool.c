#define _POSIX_C_SOURCE 200809L
#include "read_tool.h"
#include <json-c/json.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
static int parse(pg_slice args,char *path){
 if(!args.data||!args.size||args.size>=PG_TEXT_LIMIT||memchr(args.data,0,args.size))return 1;
 char text[PG_TEXT_LIMIT];memcpy(text,args.data,args.size);text[args.size]=0;
 json_object *o=json_tokener_parse(text),*p=0;
 if(!o || !json_object_is_type(o,json_type_object)){if(o)json_object_put(o);return 1;}
 int invalid=json_object_object_length(o)!=1 || !json_object_object_get_ex(o,"path",&p) || !json_object_is_type(p,json_type_string);
 if(!invalid){size_t n=(size_t)json_object_get_string_len(p);const char *s=json_object_get_string(p);
  invalid=!n||n>=PG_TEXT_LIMIT||s[0]=='/'||memchr(s,0,n)!=0;
  if(!invalid){memcpy(path,s,n+1);size_t start=0;
   for(size_t i=0;i<=n;++i)if(i==n||path[i]=='/'){
    size_t len=i-start;if(!len || (len==1&&path[start]=='.') || (len==2&&path[start]=='.'&&path[start+1]=='.')){invalid=1;break;}start=i+1;
   }
  }
 }
 json_object_put(o);return invalid;
}
static int validate(void *ctx,pg_slice args){char path[PG_TEXT_LIMIT];(void)ctx;return !parse(args,path);}
static int authorize(void *ctx,pg_slice args){(void)args;return ((pg_read_tool*)ctx)->directory>=0;}
static int fail(char *out,size_t capacity,size_t *size,const char *s){size_t n=strlen(s);*size=0;if(n<capacity){memcpy(out,s,n);*size=n;}return 1;}
static int utf8(const unsigned char *p,size_t n){
 for(size_t i=0;i<n;){unsigned c=p[i++],need=0,cp=0,min=0;
  if(c<128)continue;
  if(c>=0xc2 && c<=0xdf){need=1;cp=c&31;min=0x80;}
  else if(c>=0xe0 && c<=0xef){need=2;cp=c&15;min=0x800;}
  else if(c>=0xf0 && c<=0xf4){need=3;cp=c&7;min=0x10000;}else return 0;
  if(need>n-i)return 0;
  while(need--){unsigned b=p[i++];if((b&0xc0)!=0x80)return 0;cp=(cp<<6)|(b&63);}
  if(cp<min || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))return 0;
 }return 1;
}
static int execute(void *ctx,pg_slice args,char *out,size_t capacity,size_t *size){
 pg_read_tool *r=ctx;char path[PG_TEXT_LIMIT];int dir=-1,file=-1;size_t used=0;
 if(capacity<2 || parse(args,path))return fail(out,capacity,size,"invalid read path");
 dir=fcntl(r->directory,F_DUPFD_CLOEXEC,0);if(dir<0)return fail(out,capacity,size,"read root unavailable");
 char *part=path;
 for(char *p=path;;++p)if(*p=='/'||!*p){int last=!*p;*p=0;
  int opened=openat(dir,part,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK|(last?0:O_DIRECTORY));
  close(dir);dir=-1;if(opened<0)goto refused;
  if(last){file=opened;break;}dir=opened;part=p+1;
 }
 struct stat info;if(fstat(file,&info)||!S_ISREG(info.st_mode))goto refused;
 while(used<capacity-1){ssize_t n=read(file,out+used,capacity-1-used);if(n<0)goto refused;if(!n)break;used+=(size_t)n;}
 {char extra;ssize_t n=read(file,&extra,1);if(n!=0){close(file);return fail(out,capacity,size,"read output limit exceeded");}}
 close(file);if(!utf8((const unsigned char*)out,used))return fail(out,capacity,size,"invalid UTF-8 file is unsupported");if(memchr(out,0,used))return fail(out,capacity,size,"binary file is unsupported");*size=used;return 0;
refused:
 if(dir>=0)close(dir);if(file>=0)close(file);return fail(out,capacity,size,"read refused or unavailable");
}
int pg_read_tool_open(pg_read_tool *r,const char *root){if(!r||!root)return 1;r->directory=open(root,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);return r->directory<0;}
void pg_read_tool_close(pg_read_tool *r){if(r&&r->directory>=0){close(r->directory);r->directory=-1;}}
pg_tool_definition pg_read_tool_definition(pg_read_tool *r){return (pg_tool_definition){"read","Read one small regular text file relative to the configured workspace. Absolute paths, symlinks, dot segments and oversized files are refused.","{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"}},\"required\":[\"path\"],\"additionalProperties\":false}",r,validate,authorize,execute};}
