#include "src/compiler-memory-api/memory.h"
void *memcpy(void *d,const void *s,pangu_size n){volatile unsigned char *o=d;const volatile unsigned char *i=s;for(pangu_size j=0;j<n;j++)o[j]=i[j];return d;}
void *memset(void *d,int c,pangu_size n){volatile unsigned char *o=d;for(pangu_size j=0;j<n;j++)o[j]=(unsigned char)c;return d;}
void *memmove(void *d,const void *s,pangu_size n){volatile unsigned char *o=d;const volatile unsigned char *i=s;pangu_size x=(pangu_size)d,y=(pangu_size)s;if(x>y && x-y<n){while(n){n--;o[n]=i[n];}}else{for(pangu_size j=0;j<n;j++)o[j]=i[j];}return d;}
