#include "src/bootstrap-api/pi-arguments.h"
#include "src/bootstrap-api/memory.h"
#include "src/repair-probe/cases.h"
static volatile pangu_u64 test_case,test_capacity;
_Alignas(8) static unsigned char output[1024];
static unsigned char large[1048576],expanded[6291456];
static void fill(unsigned char *p,pangu_u64 n,unsigned char b) { for(pangu_u64 i=0;i<n;i++) p[i]=b; }
static int same(const unsigned char *a,const unsigned char *b,pangu_u64 n) { for(pangu_u64 i=0;i<n;i++) if(a[i]!=b[i]) return 0;return 1; }
static int corpus(void)
{
 for(pangu_u64 i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  test_case=i;const repair_case *c=&cases[i];pangu_text_slice input={c->raw,c->n};pangu_u64 n=99;
  if(pangu_pi_repair_utf8(input,(void *)0,0,c->n,&n) || n!=c->m) return 1;
  for(pangu_u64 cap=0;cap<=c->m+1;cap++) {
   test_capacity=cap;fill(output,sizeof(output),0xa5);n=99;pangu_i32 rc=pangu_pi_repair_utf8(input,output,cap,c->n,&n);
   if(cap<c->m) { if(rc!=2 || n!=99) return 2;for(pangu_u64 j=0;j<sizeof(output);j++) if(output[j]!=0xa5) return 3; }
   else { if(rc || n!=c->m || !same(output,c->expected,c->m)) return 4;for(pangu_u64 j=c->m;j<sizeof(output);j++) if(output[j]!=0xa5) return 5; }
  }
 }
 return 0;
}
static int boundaries(void)
{
 _Alignas(8) unsigned char raw[16]={'"','\\','q','"'};pangu_text_slice input={raw,4};pangu_u64 n=99;
 fill(output,sizeof(output),0xa5);
 if(pangu_pi_repair_utf8(input,output,16,3,&n)!=2 || n!=99 || output[0]!=0xa5) return 10;
 if(pangu_pi_repair_utf8(input,raw,16,16,&n)!=1 || n!=99 || raw[1]!='\\') return 11;
 if(pangu_pi_repair_utf8(input,output,16,16,(pangu_u64 *)output)!=1 || output[0]!=0xa5) return 12;
 if(pangu_pi_repair_utf8(input,output,16,16,(pangu_u64 *)raw)!=1 || raw[0]!='"') return 13;
 if(pangu_pi_repair_utf8(input,output,16,16,(void *)0)!=1) return 14;
 static const unsigned char invalid[][4]={{0x80,0,0,0},{0xc0,0x80,0,0},{0xed,0xa0,0x80,0},{0xf4,0x90,0x80,0x80},{0xe2,0x82,0,0}};
 static const pangu_u64 lengths[]={1,2,3,4,2};
 for(pangu_u64 i=0;i<5;i++) { input.data=invalid[i];input.length=lengths[i];if(pangu_pi_repair_utf8(input,output,16,16,&n)!=12 || n!=99 || output[0]!=0xa5) return 15; }
 /* Worst per-byte expansion: raw NUL inside an unfinished string. This is
  * lexical success only; final JSON parsing MUST reject the missing quote. */
 fill(large,sizeof(large),0);large[0]='"';input.data=large;input.length=sizeof(large);
 pangu_u64 need=1+6*(sizeof(large)-1);
 if(pangu_pi_repair_utf8(input,(void *)0,0,sizeof(large),&n) || n!=need) return 16;
 n=99;if(pangu_pi_repair_utf8(input,expanded,need-1,sizeof(large),&n)!=2 || n!=99) return 17;
 if(pangu_pi_repair_utf8(input,expanded,need,sizeof(large),&n) || n!=need || expanded[0]!='"') return 18;
 for(pangu_u64 i=1;i<need;i+=6) if(!same(expanded+i,(const unsigned char *)"\\u0000",6)) return 19;
 return 0;
}
static int pages(const pangu_memory_api *memory)
{
 pangu_mapping a,b;a.address=(void *)0;a.length=0;b.address=(void *)0;b.length=0;
 if(memory->map(memory->context,4096,&a) || memory->map(memory->context,4096,&b)) return 20;
 const unsigned char text[]={'"','\\','u','1','2','3','4','\\','q',0,'"'};
 unsigned char *in=(unsigned char *)a.address+4096-sizeof(text);for(pangu_u64 i=0;i<sizeof(text);i++) in[i]=text[i];
 pangu_text_slice input={in,sizeof(text)};pangu_u64 need,n=99;
 if(pangu_pi_repair_utf8(input,(void *)0,0,4096,&need)) return 21;
 unsigned char *out=(unsigned char *)b.address+4096-need;
 if(pangu_pi_repair_utf8(input,out,need,4096,&n) || n!=need) return 22;
 if(pangu_pi_repair_utf8(input,out,need-1,4096,&n)!=2 || n!=need) return 23;
 if(memory->unmap(memory->context,&a) || memory->unmap(memory->context,&b)) return 24;
 return 0;
}
int repair_test(const pangu_memory_api *memory,pangu_u64 shard)
{
 if(shard) return 247;
 int rc=corpus();if(rc) return rc;rc=boundaries();if(rc) return rc;return pages(memory);
}
