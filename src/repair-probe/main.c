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
static int repair_final_test(const pangu_memory_api *memory,pangu_u64 shard)
{
 if(shard) return 247;
 int rc=repair_lexical_test(memory,0);if(rc) return rc;rc=final_corpus();if(rc) return rc;rc=final_limits_test();if(rc) return rc;rc=final_refusals();if(rc) return rc;return final_pages(memory);
}

static pangu_json_parser null_parsers[4];
static pangu_json_token null_tokens[4][512];
static pangu_json_result null_results[4];
static pangu_schema_view null_views[4];
static pangu_u64 null_drops[512];
_Alignas(8) static unsigned char null_output[2048];
static pangu_schema_limits null_limits={16777216,64,4096};
static int null_parse(pangu_u32 slot,const unsigned char *bytes,pangu_u64 n)
{
 pangu_text_slice input={bytes,n};pangu_json_limits q={2097152,64,0};
 if(pangu_json_init(&null_parsers[slot],null_tokens[slot],512,q) || pangu_json_feed(&null_parsers[slot],input,1) || pangu_json_get_result(&null_parsers[slot],&null_results[slot])) return 1;
 null_views[slot].input=input;null_views[slot].result=&null_results[slot];null_views[slot].root=0;return 0;
}
static pangu_u64 null_find(const pangu_schema_view *a,pangu_u64 object,const pangu_schema_view *keydoc,pangu_u64 key)
{
 pangu_u64 found=~(pangu_u64)0;
 for(pangu_u64 i=object+1;i<a->result->tokens[object].next;) {
  pangu_u32 eq=0;if(pangu_json_string_equal(a->input,a->result,i,keydoc->input,keydoc->result,key,&eq)) return ~(pangu_u64)0;
  if(eq) found=i+1;
  i=a->result->tokens[i+1].next;
 }
 return found;
}
static pangu_u64 null_members(const pangu_schema_view *a,pangu_u64 object)
{
 pangu_u64 count=0;
 for(pangu_u64 i=object+1;i<a->result->tokens[object].next;) { if(null_find(a,object,a,i)==i+1) count++;i=a->result->tokens[i+1].next; }
 return count;
}
/* Test-only semantic equality; bounded recursion, never product code. */
static int null_equal(const pangu_schema_view *a,pangu_u64 ia,const pangu_schema_view *b,pangu_u64 ib,pangu_u32 depth)
{
 if(depth>64) return 0;
 const pangu_json_token *x=&a->result->tokens[ia],*y=&b->result->tokens[ib];if(x->kind!=y->kind) return 0;
 if(x->kind==PANGU_JSON_STRING) { pangu_u32 eq=0;return !pangu_json_string_equal(a->input,a->result,ia,b->input,b->result,ib,&eq) && eq; }
 if(x->kind==PANGU_JSON_NUMBER) { pangu_u64 u,v;return !pangu_json_number_bits(a->input,a->result,ia,&u) && !pangu_json_number_bits(b->input,b->result,ib,&v) && u==v; }
 if(x->kind==PANGU_JSON_ARRAY) {
  if(x->children!=y->children) return 0;
  pangu_u64 i=ia+1,j=ib+1;
  while(i<x->next && j<y->next) { if(!null_equal(a,i,b,j,depth+1)) return 0;i=a->result->tokens[i].next;j=b->result->tokens[j].next; }
  return i==x->next && j==y->next;
 }
 if(x->kind==PANGU_JSON_OBJECT) {
  if(null_members(a,ia)!=null_members(b,ib)) return 0;
  for(pangu_u64 i=ia+1;i<x->next;) {
   pangu_u64 value=i+1,next=a->result->tokens[value].next;
   if(null_find(a,ia,a,i)==value) { pangu_u64 found=null_find(b,ib,a,i);if(found==~(pangu_u64)0 || !null_equal(a,value,b,found,depth+1)) return 0; }
   i=next;
  }
 }
 return 1;
}
static int null_corpus(void)
{
 for(pangu_u64 i=0;i<sizeof(null_cases)/sizeof(null_cases[0]);i++) {
  test_case=i;const null_case *c=&null_cases[i];pangu_u64 need=99,n;pangu_schema_report report;
  if(null_parse(0,c->schema,c->sn) || null_parse(1,c->value,c->vn) || null_parse(2,c->expected,c->en)) return 70;
  if(pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,null_drops,512,(void *)0,0,&need,&report) || report.status) return 71;
  for(pangu_u64 cap=0;cap<=need+1;cap++) {
   test_capacity=cap;n=99;fill(null_output,sizeof(null_output),0xa5);
   pangu_i32 rc=pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,null_drops,512,null_output,cap,&n,&report);
   if(cap<need) { if(rc!=2 || report.status!=2 || n!=99) return 72;for(pangu_u64 j=0;j<sizeof(null_output);j++) if(null_output[j]!=0xa5) return 73; }
   else {
    if(rc || report.status || n!=need || null_parse(3,null_output,n) || !null_equal(&null_views[2],0,&null_views[3],0,1)) return 74;
    for(pangu_u64 j=need;j<sizeof(null_output);j++) if(null_output[j]!=0xa5) return 75;
   }
  }
 }
 return 0;
}
static int null_boundaries(void)
{
 static const unsigned char schema[]="{\"properties\":{\"x\":{\"type\":\"string\"}}}";
 static const unsigned char value[]="{\"x\":null,\"y\":null}";pangu_schema_report report;pangu_u64 n=99;
 if(null_parse(0,schema,sizeof(schema)-1) || null_parse(1,value,sizeof(value)-1)) return 80;
 if(pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,null_drops,512,(void *)0,0,&n,&report)) return 81;
 pangu_u64 work=report.work,need=n;
 for(pangu_u64 budget=1;budget<=work+1;budget++) {
  pangu_schema_limits q=null_limits;q.max_work=budget;n=99;fill(null_output,sizeof(null_output),0xa5);
  pangu_i32 rc=pangu_pi_optional_nulls(&null_views[0],&null_views[1],q,null_drops,512,null_output,2048,&n,&report);
  if(report.work>budget) return 82;
  if(budget<work) { if(rc!=2 || n!=99 || report.status!=2) return 83;for(pangu_u64 j=0;j<sizeof(null_output);j++) if(null_output[j]!=0xa5) return 84; }
  else if(rc || n!=need) return 85;
 }
 n=99;if(pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,(void *)0,0,null_output,2048,&n,&report)!=2 || n!=99) return 86;
 /* Boolean false child must keep null: Pi's cached validator is unavailable. */
 static const unsigned char boolean_schema[]="{\"properties\":{\"x\":false}}";
 if(null_parse(0,boolean_schema,sizeof(boolean_schema)-1)) return 87;
 if(pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,(void *)0,0,null_output,2048,&n,&report) || n!=sizeof(value)-1 || !same(null_output,value,n)) return 88;
 static const unsigned char unsupported[]="{\"properties\":{\"x\":{\"pattern\":\"x\"}}}";
 if(null_parse(0,unsupported,sizeof(unsupported)-1)) return 89;
 n=99;fill(null_output,sizeof(null_output),0xa5);
 if(pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,null_drops,512,null_output,2048,&n,&report)!=9 || n!=99 || null_output[0]!=0xa5) return 90;
 return 0;
}
static int null_refusals(void)
{
 pangu_schema_report report;pangu_u64 n=99;
 fill((unsigned char *)&report,32,0xa5);
 if(pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,null_drops,512,null_output,2048,(pangu_u64 *)null_output,&report)!=1 || n!=99 || ((unsigned char *)&report)[0]!=0xa5) return 91;
 if(pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,null_drops,512,null_output,2048,&n,(pangu_schema_report *)null_drops)!=1) return 92;
 if(pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,null_drops,512,(unsigned char *)null_tokens[1],32,&n,&report)!=1 || ((unsigned char *)&report)[0]!=0xa5) return 93;
 if(pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,null_drops,512,(unsigned char *)null_views[0].input.data,null_views[0].input.length,&n,&report)!=1) return 94;
 if(pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,null_drops,512,null_output,2048,&n,(pangu_schema_report *)&null_views[1])!=1) return 95;
 pangu_i32 saved=null_results[1].status;null_results[1].status=13;
 if(pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,null_drops,512,null_output,2048,&n,&report)!=10 || ((unsigned char *)&report)[0]!=0xa5) return 96;
 null_results[1].status=saved;return 0;
}
static int null_pages(const pangu_memory_api *memory)
{
 pangu_mapping maps[4];for(pangu_u32 i=0;i<4;i++) { maps[i].address=(void *)0;maps[i].length=0;if(memory->map(memory->context,4096,&maps[i])) return 100; }
 static const unsigned char schema[]="{\"properties\":{\"x\":{\"type\":\"string\"}}}";
 static const unsigned char value[]="{\"x\":1,\"\\u0078\":null,\"keep\":\"\\ud800\"}";
 unsigned char *s=(unsigned char *)maps[0].address+4096-(sizeof(schema)-1),*v=(unsigned char *)maps[1].address+4096-(sizeof(value)-1);
 for(pangu_u64 i=0;i<sizeof(schema)-1;i++) s[i]=schema[i];
 for(pangu_u64 i=0;i<sizeof(value)-1;i++) v[i]=value[i];
 if(null_parse(0,s,sizeof(schema)-1) || null_parse(1,v,sizeof(value)-1)) return 101;
 pangu_schema_report *report=(pangu_schema_report *)((unsigned char *)maps[3].address+4096-32);pangu_u64 need,n=99;
 if(pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,null_drops,512,(void *)0,0,&need,report)) return 102;
 unsigned char *out=(unsigned char *)maps[2].address+4096-need;
 if(pangu_pi_optional_nulls(&null_views[0],&null_views[1],null_limits,null_drops,512,out,need,&n,report) || n!=need) return 103;
 static const unsigned char expected[]="{\"keep\":\"\\ud800\"}";
 if(!same(out,expected,sizeof(expected)-1) || n!=sizeof(expected)-1) return 104;
 for(pangu_u32 i=0;i<4;i++) if(memory->unmap(memory->context,&maps[i])) return 105;
 return 0;
}
static int repair_null_test(const pangu_memory_api *memory,pangu_u64 shard)
{
 if(shard) return 247;
 int rc=repair_final_test(memory,0);if(rc) return rc;rc=null_corpus();if(rc) return rc;rc=null_boundaries();if(rc) return rc;rc=null_refusals();if(rc) return rc;return null_pages(memory);
}

_Alignas(8) static unsigned char numeric_scratch[1048578];
static unsigned char numeric_large[1048578];
static int numeric_corpus(void)
{
 for(pangu_u64 i=0;i<sizeof(number_string_cases)/sizeof(number_string_cases[0]);i++) {
  test_case=i;const number_string_case *c=&number_string_cases[i];pangu_text_slice input={c->text,c->length};pangu_u64 bits=99;
  if(pangu_pi_number_string(input,numeric_scratch,c->need,c->length,&bits) || bits!=c->bits) return 110;
  if(c->need) { bits=99;if(pangu_pi_number_string(input,numeric_scratch,c->need-1,c->length,&bits)!=2 || bits!=99) return 111; }
 }
 return 0;
}
static int numeric_boundaries(void)
{
 pangu_text_slice input;input.data=(void *)0;input.length=0;pangu_u64 bits=99;
 if(pangu_pi_number_string(input,(void *)0,0,0,&bits) || bits) return 112;
 for(pangu_u64 i=0;i<sizeof(numeric_large);i++) numeric_large[i]='0';
 input.data=numeric_large;input.length=1048577;
 if(pangu_pi_number_string(input,numeric_scratch,1,1048577,&bits) || bits) return 113;
 numeric_large[0]='1';bits=99;
 if(pangu_pi_number_string(input,numeric_scratch,sizeof(numeric_scratch),1048577,&bits)!=2 || bits!=99) return 114;
 input.length=1048576;if(pangu_pi_number_string(input,numeric_scratch,1048576,1048576,&bits) || bits!=0x7ff0000000000000ULL) return 115;
 bits=99;if(pangu_pi_number_string(input,numeric_scratch,1048576,1048575,&bits)!=2 || bits!=99) return 116;
 /* Huge radix must still reject invalid trailing digits after known overflow. */
 numeric_large[0]='0';numeric_large[1]='x';for(pangu_u64 i=2;i<1048576;i++) numeric_large[i]='f';
 if(pangu_pi_number_string(input,(void *)0,0,1048576,&bits) || bits!=0x7ff0000000000000ULL) return 117;
 numeric_large[1048575]='g';if(pangu_pi_number_string(input,(void *)0,0,1048576,&bits) || bits!=0x7ff8000000000000ULL) return 118;
 static const unsigned char invalid[][4]={{0x80,0,0,0},{0xc0,0x80,0,0},{0xed,0xa0,0x80,0},{0xf4,0x90,0x80,0x80},{0xe2,0x82,0,0}};
 static const pangu_u64 lengths[]={1,2,3,4,2};
 for(pangu_u64 i=0;i<5;i++) { bits=99;input.data=invalid[i];input.length=lengths[i];if(pangu_pi_number_string(input,numeric_scratch,32,32,&bits)!=12 || bits!=99) return 119; }
 return 0;
}
static int numeric_refusals(void)
{
 pangu_text_slice input={(const unsigned char *)"1",1};pangu_u64 bits=99;
 if(pangu_pi_number_string(input,(unsigned char *)input.data,1,1,&bits)!=1 || bits!=99) return 120;
 if(pangu_pi_number_string(input,numeric_scratch,32,1,(pangu_u64 *)numeric_scratch)!=1) return 121;
 if(pangu_pi_number_string(input,numeric_scratch,32,1,(void *)0)!=1) return 122;
 input.data=(const unsigned char *)(~(pangu_u64)0-1);input.length=4;
 if(pangu_pi_number_string(input,numeric_scratch,32,8,&bits)!=1 || bits!=99) return 123;
 return 0;
}
static int numeric_pages(const pangu_memory_api *memory)
{
 pangu_mapping maps[3];for(pangu_u32 i=0;i<3;i++) { maps[i].address=(void *)0;maps[i].length=0;if(memory->map(memory->context,4096,&maps[i])) return 124; }
 static const unsigned char text[]={0xef,0xbb,0xbf,'+','.','5','e','2',0xc2,0xa0};
 unsigned char *s=(unsigned char *)maps[0].address+4096-sizeof(text),*scratch=(unsigned char *)maps[1].address+4096-5;
 for(pangu_u64 i=0;i<sizeof(text);i++) s[i]=text[i];
 pangu_u64 *bits=(pangu_u64 *)((unsigned char *)maps[2].address+4096-8);*bits=99;pangu_text_slice input={s,sizeof(text)};
 if(pangu_pi_number_string(input,scratch,5,4096,bits) || *bits!=0x4049000000000000ULL) return 125;
 *bits=99;if(pangu_pi_number_string(input,scratch,4,4096,bits)!=2 || *bits!=99) return 126;
 for(pangu_u32 i=0;i<3;i++) if(memory->unmap(memory->context,&maps[i])) return 127;
 return 0;
}
static int repair_numeric_test(const pangu_memory_api *memory,pangu_u64 shard)
{
 if(shard) return 247;
 int rc=repair_null_test(memory,0);if(rc) return rc;rc=numeric_corpus();if(rc) return rc;rc=numeric_boundaries();if(rc) return rc;rc=numeric_refusals();if(rc) return rc;return numeric_pages(memory);
}

static pangu_json_parser scalar_parsers[2];
static pangu_json_token scalar_tokens[2][512];
static pangu_json_result scalar_results[2];
static pangu_schema_view scalar_views[2];
_Alignas(8) static unsigned char scalar_workspace[4096];
static unsigned char scalar_expected_text[64];
static int scalar_parse(pangu_u32 slot,const unsigned char *bytes,pangu_u64 n)
{
 pangu_text_slice input={bytes,n};pangu_json_limits q={2097152,64,0};
 if(pangu_json_init(&scalar_parsers[slot],scalar_tokens[slot],512,q) || pangu_json_feed(&scalar_parsers[slot],input,1) || pangu_json_get_result(&scalar_parsers[slot],&scalar_results[slot])) return 1;
 scalar_views[slot].input=input;scalar_views[slot].result=&scalar_results[slot];scalar_views[slot].root=0;return 0;
}
static int scalar_matches(const pangu_pi_scalar_result *value)
{
 if(value->reserved) return 0;
 if(value->kind!=PANGU_JSON_STRING && (value->text.data || value->text.length)) return 0;
 if(value->kind!=PANGU_JSON_NUMBER && value->number_bits) return 0;
 if(!value->kind) return null_equal(&scalar_views[0],scalar_views[0].root,&scalar_views[1],0,1);
 const pangu_json_token *t=&scalar_tokens[1][0];if(t->kind!=value->kind) return 0;
 if(value->kind==PANGU_JSON_NUMBER) { pangu_u64 bits;if((value->number_bits&0x7fffffffffffffffULL)>=0x7ff0000000000000ULL) return 0;return !pangu_json_number_bits(scalar_views[1].input,&scalar_results[1],0,&bits) && bits==value->number_bits; }
 if(value->kind==PANGU_JSON_STRING) { pangu_u64 n;if(pangu_json_string_decode(scalar_views[1].input,&scalar_results[1],0,scalar_expected_text,64,&n)) return 0;return n==value->text.length && same(scalar_expected_text,value->text.data,n); }
 return value->kind<=PANGU_JSON_FALSE;
}
static int scalar_corpus(void)
{
 for(pangu_u64 i=0;i<sizeof(scalar_cases)/sizeof(scalar_cases[0]);i++) {
  test_case=i;const scalar_case *c=&scalar_cases[i];pangu_pi_scalar_result result;
  if(scalar_parse(0,c->source,c->n) || scalar_parse(1,c->expected,c->en)) return 130;
  if(pangu_pi_scalar_convert(&scalar_views[0],c->target,c->mode,scalar_workspace,sizeof(scalar_workspace),c->n,&result) || !scalar_matches(&result)) return 131;
  fill((unsigned char *)&result,32,0xa5);
  pangu_i32 rc=pangu_pi_scalar_convert(&scalar_views[0],c->target,c->mode,(void *)0,0,c->n,&result);
  if(rc) { if(rc!=2) return 132;for(pangu_u64 j=0;j<32;j++) if(((unsigned char *)&result)[j]!=0xa5) return 133; }
  else if(!scalar_matches(&result)) return 134;
 }
 return 0;
}
static int scalar_boundaries(void)
{
 static const unsigned char text[]="\"9007199254740991n\"";pangu_pi_scalar_result result;
 if(scalar_parse(0,text,sizeof(text)-1)) return 135;
 for(pangu_u64 cap=0;cap<=sizeof(text)*2;cap++) {
  fill((unsigned char *)&result,32,0xa5);pangu_i32 rc=pangu_pi_scalar_convert(&scalar_views[0],PANGU_PI_SCALAR_INTEGER,PANGU_PI_TYPED_BASIC,scalar_workspace,cap,128,&result);
  if(cap<17) { if(rc!=2) return 136;for(pangu_u64 j=0;j<32;j++) if(((unsigned char *)&result)[j]!=0xa5) return 137; }
  else if(rc || result.kind!=PANGU_JSON_NUMBER || result.number_bits!=0x433fffffffffffffULL) return 138;
 }
 static const unsigned char object[]="{\"x\":\"1\"}";
 if(scalar_parse(0,object,sizeof(object)-1)) return 139;
 scalar_views[0].root=1;fill((unsigned char *)&result,32,0xa5);
 if(pangu_pi_scalar_convert(&scalar_views[0],1,3,scalar_workspace,4096,128,&result)!=1 || ((unsigned char *)&result)[0]!=0xa5) return 140;
 scalar_views[0].root=2;
 if(pangu_pi_scalar_convert(&scalar_views[0],1,3,scalar_workspace,4096,128,&result) || result.kind!=PANGU_JSON_NUMBER || result.number_bits!=0x3ff0000000000000ULL) return 141;
 scalar_views[0].root=0;fill((unsigned char *)&result,32,0xa5);
 if(pangu_pi_scalar_convert(&scalar_views[0],0,3,scalar_workspace,4096,128,&result)!=1 || ((unsigned char *)&result)[0]!=0xa5) return 142;
 if(pangu_pi_scalar_convert(&scalar_views[0],1,4,scalar_workspace,4096,128,&result)!=1 || ((unsigned char *)&result)[0]!=0xa5) return 143;
 if(pangu_pi_scalar_convert(&scalar_views[0],1,3,scalar_workspace,4096,1,&result)!=2 || ((unsigned char *)&result)[0]!=0xa5) return 144;
 if(pangu_pi_scalar_convert(&scalar_views[0],1,3,scalar_workspace,4096,128,(pangu_pi_scalar_result *)scalar_workspace)!=1) return 145;
 if(pangu_pi_scalar_convert(&scalar_views[0],1,3,(unsigned char *)scalar_tokens[0],32,128,&result)!=1 || ((unsigned char *)&result)[0]!=0xa5) return 146;
 if(pangu_pi_scalar_convert(&scalar_views[0],1,3,scalar_workspace,4096,128,(pangu_pi_scalar_result *)&scalar_views[0])!=1) return 147;
 if(pangu_pi_scalar_convert(&scalar_views[0],1,3,scalar_workspace,4096,128,(pangu_pi_scalar_result *)&scalar_results[0])!=1) return 148;
 scalar_results[0].status=13;
 if(pangu_pi_scalar_convert(&scalar_views[0],1,3,scalar_workspace,4096,128,&result)!=10 || ((unsigned char *)&result)[0]!=0xa5) return 149;
 scalar_results[0].status=0;return 0;
}
static int scalar_pages(const pangu_memory_api *memory)
{
 pangu_mapping maps[3];for(pangu_u32 i=0;i<3;i++) { maps[i].address=(void *)0;maps[i].length=0;if(memory->map(memory->context,4096,&maps[i])) return 150; }
 static const unsigned char text[]="\"\\u0031\"";
 unsigned char *input=(unsigned char *)maps[0].address+4096-(sizeof(text)-1),*work=(unsigned char *)maps[1].address+4096-2;
 for(pangu_u64 i=0;i<sizeof(text)-1;i++) input[i]=text[i];
 if(scalar_parse(0,input,sizeof(text)-1)) return 151;
 pangu_pi_scalar_result *out=(pangu_pi_scalar_result *)((unsigned char *)maps[2].address+4096-32);
 if(pangu_pi_scalar_convert(&scalar_views[0],1,3,work,2,128,out) || out->kind!=PANGU_JSON_NUMBER || out->number_bits!=0x3ff0000000000000ULL) return 152;
 fill((unsigned char *)out,32,0xa5);if(pangu_pi_scalar_convert(&scalar_views[0],1,3,work,1,128,out)!=2) return 153;
 for(pangu_u64 j=0;j<32;j++) if(((unsigned char *)out)[j]!=0xa5) return 154;
 for(pangu_u32 i=0;i<3;i++) if(memory->unmap(memory->context,&maps[i])) return 155;
 return 0;
}
static int repair_scalar_test(const pangu_memory_api *memory,pangu_u64 shard)
{
 if(shard) return 247;
 int rc=repair_numeric_test(memory,0);if(rc) return rc;rc=scalar_corpus();if(rc) return rc;rc=scalar_boundaries();if(rc) return rc;return scalar_pages(memory);
}

_Alignas(8) static unsigned char tree_workspace[8192];
_Alignas(8) static unsigned char tree_output[2][8192],tree_final_scratch[8192];
static pangu_json_token tree_final_tokens[512];
static pangu_u32 tree_kind_workspace[512];
static void tree_schema_init(pangu_pi_tree_schema *s,const pangu_u32 *kinds,pangu_u64 count)
{
 s->json.input.data=null_views[0].input.data;s->json.input.length=null_views[0].input.length;s->json.result=&null_results[0];s->json.root=0;s->kinds=kinds;s->kind_count=count;
}
static int tree_untouched(const unsigned char *p,pangu_u64 n)
{
 for(pangu_u64 i=0;i<n;i++) if(p[i]!=0x5a) return 0;return 1;
}
static int tree_stages(void)
{
 for(pangu_u64 i=0;i<sizeof(tree_cases)/sizeof(tree_cases[0]);i++) {
  const tree_case *c=&tree_cases[i];test_case=i;
  if(null_parse(0,c->schema,c->sn) || null_results[0].count!=c->kn) return 156;
  pangu_pi_tree_schema schema;tree_schema_init(&schema,c->kinds,c->kn);
  for(pangu_u32 pass=1;pass<=2;pass++) {
   const unsigned char *input=pass==1 ? c->normalized : c->typed_mutation,*expected=pass==1 ? c->typed : c->plain;
   pangu_u64 n=pass==1 ? c->nn : c->mn,en=pass==1 ? c->tn : c->pn,need=123456;
   pangu_i32 expected_status=(pangu_i32)(pass==1 ? c->typed_status : c->plain_status);pangu_schema_report report;
   if(null_parse(1,input,n)) return 157;
   fill(tree_output[0],8192,0x5a);
   pangu_i32 rc=pangu_pi_tree_convert(&schema,&null_views[1],pass,null_limits,tree_workspace,8192,tree_output[0],8192,&need,&report);
   if(rc!=expected_status || report.status!=rc) return 158;
   if(rc) { if(need!=123456 || !tree_untouched(tree_output[0],8192)) return 159;continue; }
   if(null_parse(1,tree_output[0],need) || null_parse(2,expected,en) || !null_equal(&null_views[1],0,&null_views[2],0,1)) return 160;
   if(null_parse(1,input,n)) return 161;
   pangu_u64 measured=123456;
   if(pangu_pi_tree_convert(&schema,&null_views[1],pass,null_limits,tree_workspace,8192,(void *)0,0,&measured,&report) || measured!=need) return 162;
   for(pangu_u64 capacity=0;capacity<need;capacity++) {
    test_capacity=capacity;pangu_u64 written=123456;fill(tree_output[0],8192,0x5a);
    if(pangu_pi_tree_convert(&schema,&null_views[1],pass,null_limits,tree_workspace,8192,tree_output[0],capacity,&written,&report)!=2 || written!=123456 || !tree_untouched(tree_output[0],8192)) return 163;
   }
  }
 }
 return 0;
}
static int tree_tool_chain(void)
{
 for(pangu_u64 i=0;i<sizeof(tree_cases)/sizeof(tree_cases[0]);i++) {
  const tree_case *c=&tree_cases[i];if(!c->tool) continue;test_case=i;
  if(null_parse(0,c->schema,c->sn)) return 164;
  pangu_pi_tree_schema schema;tree_schema_init(&schema,c->kinds,c->kn);
  pangu_pi_arguments_result parsed;pangu_text_slice input={c->raw,c->rn};pangu_json_limits limits={8192,64,0};
  if(pangu_pi_arguments_parse(input,1,tree_final_scratch,8192,tree_final_tokens,512,limits,&parsed)) return 165;
  pangu_schema_view value;value.input.data=parsed.input.data;value.input.length=parsed.input.length;value.result=&parsed.json;value.root=0;
  pangu_u64 n=0;pangu_schema_report report;
  if(pangu_pi_optional_nulls(&schema.json,&value,null_limits,null_drops,512,tree_output[0],8192,&n,&report)) return 166;
  if(null_parse(1,tree_output[0],n)) return 167;
  if(pangu_pi_tree_convert(&schema,&null_views[1],1,null_limits,tree_workspace,8192,tree_output[1],8192,&n,&report)) return 168;
  if(null_parse(1,tree_output[1],n)) return 169;
  if(pangu_pi_tree_convert(&schema,&null_views[1],2,null_limits,tree_workspace,8192,tree_output[0],8192,&n,&report)) return 170;
  if(null_parse(1,tree_output[0],n)) return 171;
  pangu_i32 rc=pangu_schema_validate(&schema.json,&null_views[1],&null_limits,&report);
  if(rc!=(c->accepted ? 0 : 15)) return 172;
  if(c->accepted && (null_parse(2,c->public_value,c->en) || !null_equal(&null_views[1],0,&null_views[2],0,1))) return 173;
 }
 return 0;
}
static int tree_setup(void)
{
 static const unsigned char schema[]="{\"type\":\"object\",\"properties\":{\"x\":{\"type\":\"number\"}},\"required\":[\"x\"]}",value[]="{\"x\":\"2\"}";
 if(null_parse(0,schema,sizeof(schema)-1) || null_parse(1,value,sizeof(value)-1)) return 174;
 for(pangu_u64 i=0;i<512;i++) tree_kind_workspace[i]=0;
 tree_kind_workspace[0]=7;
 pangu_u64 props=0,child=0;pangu_text_slice p={(const unsigned char *)"properties",10},x={(const unsigned char *)"x",1};
 if(pangu_json_object_find(null_views[0].input,&null_results[0],0,p,&props) || pangu_json_object_find(null_views[0].input,&null_results[0],props,x,&child)) return 175;
 tree_kind_workspace[child]=1;return 0;
}
static int tree_boundaries(void)
{
 if(tree_setup()) return 176;pangu_pi_tree_schema schema;tree_schema_init(&schema,tree_kind_workspace,null_results[0].count);
 pangu_u64 n=123456;pangu_schema_report report;pangu_schema_limits limits;
 limits.max_work=1;limits.max_depth=64;limits.max_nodes=4096;fill(tree_output[0],8192,0x5a);
 if(pangu_pi_tree_convert(&schema,&null_views[1],1,limits,tree_workspace,8192,tree_output[0],8192,&n,&report)!=2 || n!=123456 || !tree_untouched(tree_output[0],8192)) return 177;
 limits.max_work=16777216;limits.max_depth=1;
 if(pangu_pi_tree_convert(&schema,&null_views[1],1,limits,tree_workspace,8192,tree_output[0],8192,&n,&report)!=2 || n!=123456 || !tree_untouched(tree_output[0],8192)) return 178;
 limits.max_depth=64;limits.max_nodes=1;
 if(pangu_pi_tree_convert(&schema,&null_views[1],1,limits,tree_workspace,8192,tree_output[0],8192,&n,&report)!=2 || n!=123456 || !tree_untouched(tree_output[0],8192)) return 179;
 if(pangu_pi_tree_convert(&schema,&null_views[1],1,null_limits,(unsigned char *)tree_kind_workspace,sizeof(tree_kind_workspace),tree_output[0],8192,&n,&report)!=1) return 180;
 if(pangu_pi_tree_convert(&schema,&null_views[1],1,null_limits,tree_workspace,8192,(unsigned char *)null_views[1].input.data,null_views[1].input.length,&n,&report)!=1) return 181;
 if(pangu_pi_tree_convert(&schema,&null_views[1],1,null_limits,tree_workspace,8192,tree_output[0],8192,(pangu_u64 *)tree_output[0],&report)!=1) return 182;
 if(pangu_pi_tree_convert(&schema,&null_views[1],1,null_limits,tree_workspace,8192,tree_output[0],8192,&n,(pangu_schema_report *)&schema)!=1) return 183;
 if(pangu_pi_tree_convert(&schema,&null_views[1],3,null_limits,tree_workspace,8192,tree_output[0],8192,&n,&report)!=1) return 184;
 if(pangu_pi_tree_convert(&schema,&null_views[1],1,null_limits,tree_workspace,8192,(void *)0,1,&n,&report)!=1) return 185;
 tree_kind_workspace[0]=99;
 if(pangu_pi_tree_convert(&schema,&null_views[1],1,null_limits,tree_workspace,8192,tree_output[0],8192,&n,&report)!=9 || n!=123456 || !tree_untouched(tree_output[0],8192)) return 186;
 tree_kind_workspace[0]=7;schema.kind_count--;
 if(pangu_pi_tree_convert(&schema,&null_views[1],1,null_limits,tree_workspace,8192,tree_output[0],8192,&n,&report)!=1) return 187;
 schema.kind_count++;null_results[1].status=13;
 if(pangu_pi_tree_convert(&schema,&null_views[1],1,null_limits,tree_workspace,8192,tree_output[0],8192,&n,&report)!=10) return 188;
 null_results[1].status=0;
 static const unsigned char duplicate[]="{\"x\":\"bad\",\"\\u0078\":\"2\"}",expected[]="{\"x\":2}";
 if(null_parse(1,duplicate,sizeof(duplicate)-1) || pangu_pi_tree_convert(&schema,&null_views[1],1,null_limits,tree_workspace,8192,tree_output[0],8192,&n,&report) || null_parse(1,tree_output[0],n) || null_parse(2,expected,sizeof(expected)-1) || !null_equal(&null_views[1],0,&null_views[2],0,1)) return 189;
 return 0;
}
static int tree_pages(const pangu_memory_api *memory)
{
 if(tree_setup()) return 190;
 pangu_mapping maps[6];for(pangu_u32 i=0;i<6;i++) { maps[i].address=(void *)0;maps[i].length=0;if(memory->map(memory->context,4096,&maps[i])) return 191; }
 static const unsigned char input[]="{\"x\":\"2\"}",expected[]="{\"x\":2}";
 unsigned char *bytes=(unsigned char *)maps[0].address+4096-(sizeof(input)-1),*scratch=(unsigned char *)maps[1].address+4096-32,*out=(unsigned char *)maps[2].address+4096-(sizeof(expected)-1);
 pangu_u32 *kinds=(pangu_u32 *)((unsigned char *)maps[3].address+4096-null_results[0].count*4);
 pangu_schema_report *report=(pangu_schema_report *)((unsigned char *)maps[4].address+4096-32);pangu_u64 *n=(pangu_u64 *)((unsigned char *)maps[5].address+4096-8);
 for(pangu_u64 i=0;i<sizeof(input)-1;i++) bytes[i]=input[i];for(pangu_u64 i=0;i<null_results[0].count;i++) kinds[i]=tree_kind_workspace[i];
 if(null_parse(1,bytes,sizeof(input)-1)) return 192;
 pangu_pi_tree_schema schema;tree_schema_init(&schema,kinds,null_results[0].count);
 if(pangu_pi_tree_convert(&schema,&null_views[1],1,null_limits,scratch,32,out,sizeof(expected)-1,n,report) || *n!=sizeof(expected)-1 || !same(out,expected,*n)) return 193;
 fill(out,sizeof(expected)-1,0x5a);*n=123456;
 if(pangu_pi_tree_convert(&schema,&null_views[1],1,null_limits,scratch,32,out,sizeof(expected)-2,n,report)!=2 || *n!=123456 || !tree_untouched(out,sizeof(expected)-1)) return 194;
 for(pangu_u32 i=0;i<6;i++) if(memory->unmap(memory->context,&maps[i])) return 195;return 0;
}
static int repair_tree_test(const pangu_memory_api *memory,pangu_u64 shard)
{
 if(shard) return 247;int rc=repair_scalar_test(memory,0);if(rc) return rc;
 rc=tree_stages();if(rc) return rc;rc=tree_tool_chain();if(rc) return rc;rc=tree_boundaries();if(rc) return rc;return tree_pages(memory);
}

static void validation_storage(pangu_pi_validation_storage *s)
{
 s->repair=tree_final_scratch;s->repair_capacity=8192;s->bytes[0]=tree_output[0];s->bytes[1]=tree_output[1];s->byte_capacity[0]=8192;s->byte_capacity[1]=8192;
 s->tokens=tree_final_tokens;s->token_capacity=512;s->scalar=tree_workspace;s->scalar_capacity=8192;s->drops=null_drops;s->drop_capacity=512;
}
static int validation_empty(const pangu_pi_validation_result *r,pangu_i32 rc)
{
 return !r->arguments.input.data && !r->arguments.input.length && !r->arguments.json.tokens && !r->arguments.json.count && r->arguments.json.status==rc && !r->reserved;
}
static int validation_corpus(void)
{
 pangu_pi_validation_storage storage;validation_storage(&storage);pangu_json_limits limits={8192,64,0};
 for(pangu_u64 i=0;i<sizeof(tree_cases)/sizeof(tree_cases[0]);i++) {
  const tree_case *c=&tree_cases[i];if(!c->tool) continue;test_case=i;
  if(null_parse(0,c->schema,c->sn)) return 196;pangu_pi_tree_schema schema;tree_schema_init(&schema,c->kinds,c->kn);
  pangu_pi_validation_result out;pangu_text_slice input={c->raw,c->rn};
  pangu_i32 rc=pangu_pi_arguments_validate(&schema,input,1,&storage,limits,null_limits,&out);
  if(rc!=(c->accepted ? 0 : 15)) return 197;
  if(!rc) {
   if(out.stage!=PANGU_PI_VALIDATE_READY || !out.arguments.json.tokens || out.arguments.json.status || !out.work) return 198;
   pangu_schema_view v;v.input.data=out.arguments.input.data;v.input.length=out.arguments.input.length;v.result=&out.arguments.json;v.root=0;
   if(null_parse(2,c->public_value,c->en) || !null_equal(&v,0,&null_views[2],0,1)) return 199;
  } else if(!validation_empty(&out,rc) || out.stage!=PANGU_PI_VALIDATE_SCHEMA) return 200;
  fill(tree_final_scratch,8192,0x5a);fill(tree_output[0],8192,0x5a);fill(tree_output[1],8192,0x5a);fill(tree_workspace,8192,0x5a);fill((unsigned char *)tree_final_tokens,sizeof(tree_final_tokens),0x5a);fill((unsigned char *)null_drops,sizeof(null_drops),0x5a);fill((unsigned char *)&out,112,0xa5);
  if(pangu_pi_arguments_validate(&schema,input,0,&storage,limits,null_limits,&out)!=13 || !validation_empty(&out,13) || out.stage || out.work) return 201;
  if(!tree_untouched(tree_final_scratch,8192) || !tree_untouched(tree_output[0],8192) || !tree_untouched(tree_output[1],8192) || !tree_untouched(tree_workspace,8192) || !tree_untouched((const unsigned char *)tree_final_tokens,sizeof(tree_final_tokens)) || !tree_untouched((const unsigned char *)null_drops,sizeof(null_drops))) return 202;
 }
 return 0;
}
static int validation_text_corpus(void)
{
 pangu_pi_validation_storage storage;validation_storage(&storage);pangu_json_limits limits={8192,64,0};
 for(pangu_u64 i=0;i<sizeof(validation_text_cases)/sizeof(validation_text_cases[0]);i++) {
  const validation_text_case *c=&validation_text_cases[i];const tree_case *t=&tree_cases[c->index];test_case=i;
  if(null_parse(0,t->schema,t->sn)) return 203;pangu_pi_tree_schema schema;tree_schema_init(&schema,t->kinds,t->kn);
  pangu_pi_validation_result out;pangu_text_slice input={c->text,c->n};pangu_i32 rc=pangu_pi_arguments_validate(&schema,input,1,&storage,limits,null_limits,&out);
  if(c->accepted) {
   if(rc || out.stage!=PANGU_PI_VALIDATE_READY) return 204;pangu_schema_view v;v.input.data=out.arguments.input.data;v.input.length=out.arguments.input.length;v.result=&out.arguments.json;v.root=0;
   if(null_parse(2,c->expected,c->en) || !null_equal(&v,0,&null_views[2],0,1)) return 205;
  } else {
   if(!rc || !validation_empty(&out,rc)) return 206;
   if(c->parse_error) { if(out.stage!=PANGU_PI_VALIDATE_PARSE || (rc!=9 && rc!=12 && rc!=13 && rc!=14)) return 207; }
   else if(rc!=15 || out.stage!=PANGU_PI_VALIDATE_SCHEMA) return 208;
  }
 }
 return 0;
}
static int validation_boundaries(void)
{
 if(tree_setup()) return 209;pangu_pi_tree_schema schema;tree_schema_init(&schema,tree_kind_workspace,null_results[0].count);
 pangu_pi_validation_storage storage;validation_storage(&storage);pangu_json_limits json_limits={8192,64,0};pangu_pi_validation_result out;pangu_text_slice input=null_views[1].input;
 if(pangu_pi_arguments_validate(&schema,input,1,&storage,json_limits,null_limits,&out)) return 210;
 pangu_u64 total=out.work;pangu_schema_limits limits=null_limits;
 for(pangu_u64 work=1;work<total;work++) { test_capacity=work;limits.max_work=work;
  if(pangu_pi_arguments_validate(&schema,input,1,&storage,json_limits,limits,&out)!=2 || !validation_empty(&out,2) || out.work>work) return 211;
 }
 limits.max_work=total;if(pangu_pi_arguments_validate(&schema,input,1,&storage,json_limits,limits,&out) || out.work!=total) return 212;
 for(pangu_u32 slot=0;slot<2;slot++) {
  for(pangu_u64 capacity=1;capacity<32;capacity++) { storage.byte_capacity[slot]=capacity;pangu_i32 rc=pangu_pi_arguments_validate(&schema,input,1,&storage,json_limits,null_limits,&out);if(rc!=0 && (rc!=2 || !validation_empty(&out,2))) return 213; }
  storage.byte_capacity[slot]=8192;
 }
 for(pangu_u64 capacity=1;capacity<8;capacity++) { storage.token_capacity=capacity;pangu_i32 rc=pangu_pi_arguments_validate(&schema,input,1,&storage,json_limits,null_limits,&out);if(rc!=0 && (rc!=2 || !validation_empty(&out,2))) return 214; }storage.token_capacity=512;
 for(pangu_u64 bytes=1;bytes<input.length;bytes++) { json_limits.max_bytes=bytes;if(pangu_pi_arguments_validate(&schema,input,1,&storage,json_limits,null_limits,&out)!=2 || !validation_empty(&out,2)) return 215; }json_limits.max_bytes=8192;
 fill((unsigned char *)&out,112,0xa5);storage.bytes[1]=storage.bytes[0];
 if(pangu_pi_arguments_validate(&schema,input,1,&storage,json_limits,null_limits,&out)!=1) return 216;
 for(pangu_u64 i=0;i<112;i++) if(((unsigned char *)&out)[i]!=0xa5) return 217;
 storage.bytes[1]=tree_output[1];null_results[0].status=13;
 if(pangu_pi_arguments_validate(&schema,input,1,&storage,json_limits,null_limits,&out)!=10) return 218;
 for(pangu_u64 i=0;i<112;i++) if(((unsigned char *)&out)[i]!=0xa5) return 219;
 null_results[0].status=0;
 if(pangu_pi_arguments_validate(&schema,input,1,&storage,json_limits,null_limits,(pangu_pi_validation_result *)tree_output[0])!=1) return 220;
 static const unsigned char invalid[]={0xff};input.data=invalid;input.length=1;
 if(pangu_pi_arguments_validate(&schema,input,1,&storage,json_limits,null_limits,&out)!=12 || !validation_empty(&out,12)) return 221;
 return 0;
}
static int validation_pages(const pangu_memory_api *memory)
{
 if(tree_setup()) return 222;pangu_pi_tree_schema schema;tree_schema_init(&schema,tree_kind_workspace,null_results[0].count);
 pangu_mapping maps[9];for(pangu_u32 i=0;i<9;i++) { maps[i].address=(void *)0;maps[i].length=0;if(memory->map(memory->context,4096,&maps[i])) return 223; }
 pangu_pi_validation_storage *storage=(pangu_pi_validation_storage *)((unsigned char *)maps[0].address+4096-96);
 pangu_pi_validation_result *out=(pangu_pi_validation_result *)((unsigned char *)maps[1].address+4096-112);
 storage->repair=(unsigned char *)maps[2].address+4096-32;storage->repair_capacity=32;
 storage->bytes[0]=(unsigned char *)maps[3].address+4096-9;storage->byte_capacity[0]=9;
 storage->bytes[1]=(unsigned char *)maps[4].address+4096-9;storage->byte_capacity[1]=9;
 storage->tokens=(pangu_json_token *)((unsigned char *)maps[5].address+4096-3*48);storage->token_capacity=3;
 storage->scalar=(unsigned char *)maps[6].address+4096-32;storage->scalar_capacity=32;
 storage->drops=(pangu_u64 *)((unsigned char *)maps[7].address+4096-8);storage->drop_capacity=1;
 static const unsigned char text[]="{\"x\":\"2\"}";unsigned char *bytes=(unsigned char *)maps[8].address+4096-(sizeof(text)-1);for(pangu_u64 i=0;i<sizeof(text)-1;i++) bytes[i]=text[i];
 pangu_text_slice input={bytes,sizeof(text)-1};pangu_json_limits limits={32,64,0};
 if(pangu_pi_arguments_validate(&schema,input,1,storage,limits,null_limits,out) || out->stage!=PANGU_PI_VALIDATE_READY || out->arguments.input.length!=7) return 224;
 storage->byte_capacity[1]=6;
 if(pangu_pi_arguments_validate(&schema,input,1,storage,limits,null_limits,out)!=2 || !validation_empty(out,2)) return 225;
 for(pangu_u32 i=0;i<9;i++) if(memory->unmap(memory->context,&maps[i])) return 226;return 0;
}
int repair_test(const pangu_memory_api *memory,pangu_u64 shard)
{
 if(shard) return 247;int rc=repair_tree_test(memory,0);if(rc) return rc;
 rc=validation_corpus();if(rc) return rc;rc=validation_text_corpus();if(rc) return rc;rc=validation_boundaries();if(rc) return rc;return validation_pages(memory);
}
