#include "src/t14-test-platform/platform.h"
typedef __SIZE_TYPE__ size_t;
int memcmp(const void *a,const void *b,size_t n) {const unsigned char *x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return x[i]<y[i]?-1:1;return 0;}
static size_t len(const char *s) {size_t n=0;while(s[n])n++;return n;}
static void write_all(long fd,const char *s,size_t n) {while(n){long z=probe_write(fd,s,n);if(z==-4)continue;if(z<=0 || (size_t)z>n)probe_exit(124);s+=z;n-=(size_t)z;}}
static int number(long fd,int value) {char b[16];unsigned int v=(unsigned int)value;int total=0;if(value<0){write_all(fd,"-",1);v=0-v;total++;}size_t n=0;do{b[n++]=(char)('0'+v%10);v/=10;}while(v);for(size_t i=0;i<n/2;i++){char c=b[i];b[i]=b[n-1-i];b[n-1-i]=c;}write_all(fd,b,n);return total+(int)n;}
int printf(const char *s,...) {__builtin_va_list ap;__builtin_va_start(ap,s);int total=0;for(;*s;s++){if(*s!='%'){write_all(1,s,1);total++;continue;}s++;if(*s=='d')total+=number(1,__builtin_va_arg(ap,int));else if(*s=='s'){const char *text=__builtin_va_arg(ap,const char *);if(!text)probe_exit(123);size_t n=len(text);write_all(1,text,n);total+=(int)n;}else probe_exit(123);}__builtin_va_end(ap);return total;}
int puts(const char *s) {write_all(1,s,len(s));write_all(1,"\n",1);return 0;}
_Noreturn void probe_assert(const char *s,const char *f,int line) {write_all(2,f,len(f));write_all(2,":",1);(void)number(2,line);write_all(2,": ",2);write_all(2,s,len(s));write_all(2,"\n",1);probe_exit(125);}
