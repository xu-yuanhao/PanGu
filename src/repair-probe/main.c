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
static int repair_lexical_test(const pangu_memory_api *memory,pangu_u64 shard)
{
 if(shard) return 247;
 int rc=corpus();if(rc) return rc;rc=boundaries();if(rc) return rc;return pages(memory);
}

static pangu_json_token final_tokens[512],fresh_tokens[512];
static pangu_json_parser fresh_parser;
_Alignas(8) static unsigned char final_scratch[2048];
static pangu_pi_arguments_result final_result;
static pangu_json_limits final_limits={2097152,64,0};
static unsigned char final_deep[256];
static int final_corpus(void)
{
 for(pangu_u64 i=0;i<sizeof(final_cases)/sizeof(final_cases[0]);i++) {
  test_case=i;const final_case *c=&final_cases[i];pangu_text_slice input={c->raw,c->n};
  pangu_i32 rc=pangu_pi_arguments_parse(input,1,final_scratch,sizeof(final_scratch),final_tokens,512,final_limits,&final_result);
  if((rc==0)!=c->accepted || rc!=final_result.json.status || final_result.path!=c->path) return 30;
  if(rc) { if(final_result.input.data || final_result.input.length || final_result.json.tokens || final_result.json.count) return 31; }
  else {
   if(final_result.input.length!=c->m || !same(final_result.input.data,c->selected,c->m) || !final_result.json.count || final_result.json.tokens!=final_tokens || final_tokens[0].kind!=PANGU_JSON_OBJECT) return 32;
   pangu_text_slice selected={c->selected,c->m};pangu_json_result expected;
   if(pangu_json_init(&fresh_parser,fresh_tokens,512,final_limits) || pangu_json_feed(&fresh_parser,selected,1) || pangu_json_get_result(&fresh_parser,&expected)) return 33;
   if(expected.count!=final_result.json.count || !same((const unsigned char *)fresh_tokens,(const unsigned char *)final_tokens,expected.count*48)) return 34;
  }
  /* Reusing the output after success must clear publication when nonfinal. */
  if(pangu_pi_arguments_parse(input,0,final_scratch,sizeof(final_scratch),final_tokens,512,final_limits,&final_result)!=13 || final_result.input.data || final_result.json.tokens || final_result.json.count || final_result.json.status!=13 || final_result.path) return 35;
 }
 return 0;
}
static int final_limits_test(void)
{
 pangu_text_slice input={(const unsigned char *)"{\"x\":\"\\q\"}",10};pangu_json_limits q=final_limits;
 /* Exact source has 10 bytes, repaired source 11; output is never published
  * when fallback storage/normalized byte quota is inadequate. */
 for(pangu_u64 cap=0;cap<=12;cap++) {
  pangu_i32 rc=pangu_pi_arguments_parse(input,1,final_scratch,cap,final_tokens,512,q,&final_result);
  if(cap<11) { if(rc!=2 || final_result.json.tokens || final_result.input.data || final_result.path!=2) return 40; }
  else if(rc || final_result.path!=2 || final_result.input.length!=11) return 41;
 }
 q.max_bytes=10;if(pangu_pi_arguments_parse(input,1,final_scratch,2048,final_tokens,512,q,&final_result)!=2 || final_result.json.tokens || final_result.path!=2) return 42;
 input.data=(const unsigned char *)"{}";input.length=2;q=final_limits;
 if(pangu_pi_arguments_parse(input,1,(void *)0,0,final_tokens,512,q,&final_result) || final_result.input.data!=input.data || final_result.path!=1) return 43;
 if(pangu_pi_arguments_parse(input,1,(void *)0,0,(void *)0,0,q,&final_result)!=2 || final_result.json.tokens || final_result.path!=1) return 44;
 input.data=(const unsigned char *)"[]";
 if(pangu_pi_arguments_parse(input,1,(void *)0,0,final_tokens,512,q,&final_result)!=9 || final_result.json.tokens) return 45;
 input.data=(void *)0;input.length=0;
 if(pangu_pi_arguments_parse(input,1,(void *)0,0,final_tokens,512,q,&final_result) || final_result.path!=3 || final_result.input.length!=2) return 46;
 q.max_bytes=1;if(pangu_pi_arguments_parse(input,1,(void *)0,0,final_tokens,512,q,&final_result)!=2 || final_result.json.tokens || final_result.path!=3) return 47;
 q=final_limits;
 for(pangu_u32 depth=64;depth<=65;depth++) {
  pangu_u64 n=0;final_deep[n++]='{';final_deep[n++]='"';final_deep[n++]='x';final_deep[n++]='"';final_deep[n++]=':';
  for(pangu_u32 i=1;i<depth;i++) final_deep[n++]='[';
  final_deep[n++]='0';for(pangu_u32 i=1;i<depth;i++) final_deep[n++]=']';final_deep[n++]='}';input.data=final_deep;input.length=n;
  pangu_i32 rc=pangu_pi_arguments_parse(input,1,final_scratch,2048,final_tokens,512,q,&final_result);
  if((depth==64 && rc) || (depth==65 && (rc!=2 || final_result.json.tokens || final_result.path!=1))) return 48;
 }
 return 0;
}
static int final_refusals(void)
{
 pangu_text_slice input={(const unsigned char *)"{}",2};pangu_json_limits q=final_limits;
 pangu_pi_arguments_result before;fill((unsigned char *)&final_result,64,0xa5);fill((unsigned char *)&before,64,0xa5);
 if(pangu_pi_arguments_parse(input,2,final_scratch,2048,final_tokens,512,q,&final_result)!=1 || !same((const unsigned char *)&before,(const unsigned char *)&final_result,64)) return 50;
 if(pangu_pi_arguments_parse(input,1,(unsigned char *)final_tokens,64,final_tokens,512,q,&final_result)!=1 || !same((const unsigned char *)&before,(const unsigned char *)&final_result,64)) return 51;
 if(pangu_pi_arguments_parse(input,1,final_scratch,2048,final_tokens,512,q,(pangu_pi_arguments_result *)final_scratch)!=1) return 52;
 if(pangu_pi_arguments_parse(input,1,(unsigned char *)input.data,2,final_tokens,512,q,&final_result)!=1 || !same((const unsigned char *)&before,(const unsigned char *)&final_result,64)) return 53;
 if(pangu_pi_arguments_parse(input,1,final_scratch,2048,final_tokens,512,q,(pangu_pi_arguments_result *)final_tokens)!=1) return 54;
 static const unsigned char bad[]={0xed,0xa0,0x80};input.data=bad;input.length=3;
 if(pangu_pi_arguments_parse(input,1,final_scratch,2048,final_tokens,512,q,&final_result)!=12 || final_result.json.tokens || final_result.input.data) return 55;
 return 0;
}
static int final_pages(const pangu_memory_api *memory)
{
 pangu_mapping maps[3];for(pangu_u32 i=0;i<3;i++) { maps[i].address=(void *)0;maps[i].length=0;if(memory->map(memory->context,4096,&maps[i])) return 60; }
 const unsigned char raw[]={'{','"','x','"',':','"','\\','q','"','}'};
 unsigned char *s=(unsigned char *)maps[0].address+4096-sizeof(raw),*scratch=(unsigned char *)maps[1].address+4096-11;
 for(pangu_u64 i=0;i<sizeof(raw);i++) s[i]=raw[i];
 pangu_text_slice input={s,sizeof(raw)};pangu_pi_arguments_result *out=(pangu_pi_arguments_result *)((unsigned char *)maps[2].address+4096-64);
 if(pangu_pi_arguments_parse(input,1,scratch,11,final_tokens,512,final_limits,out) || out->input.data!=scratch || out->input.length!=11 || out->path!=2) return 61;
 if(pangu_pi_arguments_parse(input,0,scratch,11,final_tokens,512,final_limits,out)!=13 || out->json.tokens || out->input.data) return 62;
 for(pangu_u32 i=0;i<3;i++) if(memory->unmap(memory->context,&maps[i])) return 63;
 return 0;
}
int repair_test(const pangu_memory_api *memory,pangu_u64 shard)
{
 if(shard) return 247;
 int rc=repair_lexical_test(memory,0);if(rc) return rc;rc=final_corpus();if(rc) return rc;rc=final_limits_test();if(rc) return rc;rc=final_refusals();if(rc) return rc;return final_pages(memory);
}
