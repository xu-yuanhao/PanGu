typedef __SIZE_TYPE__ size_t;
int memcmp(const void *a,const void *b,size_t n){const unsigned char *x=a,*y=b;for(size_t j=0;j<n;j++) if(x[j]!=y[j])return x[j]<y[j]?-1:1;return 0;}
#include "src/t14-test-platform/platform.h"
static void write_all(long fd,const char *s,size_t n){while(n){long r=probe_write(fd,s,n);if(r==-4)continue;if(r<=0 || (size_t)r>n)probe_exit(124);s+=r;n-=(size_t)r;}}
static size_t len(const char *s){size_t n=0;while(s[n])n++;return n;}
static void number(long fd,int value){char b[16];unsigned int v=(unsigned int)value;if(value<0){write_all(fd,"-",1);v=0-v;}size_t n=0;do{b[n++]=(char)('0'+v%10);v/=10;}while(v);for(size_t i=0;i<n/2;i++){char c=b[i];b[i]=b[n-1-i];b[n-1-i]=c;}write_all(fd,b,n);}
int printf(const char *s,...){__builtin_va_list ap;__builtin_va_start(ap,s);for(;*s;s++){if(*s=='%'){s++;if(*s!='d')probe_exit(123);number(1,__builtin_va_arg(ap,int));}else write_all(1,s,1);}__builtin_va_end(ap);return 0;}
_Noreturn void probe_assert(const char *s,const char *f,int line){write_all(2,f,len(f));write_all(2,":",1);number(2,line);write_all(2,": ",2);write_all(2,s,len(s));write_all(2,"\n",1);probe_exit(125);}
