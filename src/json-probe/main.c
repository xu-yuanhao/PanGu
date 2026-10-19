#include "src/bootstrap-api/json.h"
#include "src/bootstrap-api/memory.h"
#include "src/json-probe/cases.h"
static pangu_json_parser one,stream;
static pangu_json_token tokens[512],other[512];
static pangu_json_result expected,observed;
static volatile pangu_u64 test_case,test_step;
static unsigned char deep[132];
static unsigned char large[1048576];
static pangu_json_limits limits;
static int same_tokens(const pangu_json_result *a,const pangu_json_result *b)
{
 if(a->status!=b->status || a->count!=b->count || a->error_offset!=b->error_offset) return 0;
 for(pangu_u64 i=0;i<a->count;i++) {
  const pangu_json_token *x=&a->tokens[i],*y=&b->tokens[i];
  if(x->kind!=y->kind || x->flags!=y->flags || x->start!=y->start || x->end!=y->end || x->parent!=y->parent || x->next!=y->next || x->children!=y->children) return 0;
 }
 return 1;
}
static int run_case(const unsigned char *bytes,pangu_u64 length,int is_valid)
{
 pangu_text_slice input;input.data=bytes;input.length=length;
 if(pangu_json_init(&one,tokens,512,limits)) return 1;
 pangu_i32 rc=pangu_json_feed(&one,input,1);
 if((rc==0)!=is_valid || rc==1 || rc==2 || rc==10) return 2;
 if(pangu_json_get_result(&one,&expected)!=rc) return 3;
 if(!is_valid && (expected.tokens || expected.count)) return 4;
 if(pangu_json_feed(&one,input,0)!=10) return 5;
 for(pangu_u64 cut=0;cut<=length;cut++) {
  test_step=cut;
  if(pangu_json_init(&stream,other,512,limits)) return 6;
  input.data=bytes;input.length=cut;rc=pangu_json_feed(&stream,input,0);
  pangu_json_get_result(&stream,&observed);
  if(observed.tokens || observed.count) return 7;
  input.data=bytes+cut;input.length=length-cut;
  if(rc==13) rc=pangu_json_feed(&stream,input,1);
  else if(pangu_json_feed(&stream,input,1)!=10) return 8;
  if(pangu_json_get_result(&stream,&observed)!=rc || !same_tokens(&expected,&observed)) return 9;
 }
 /* Every byte is its own non-final fragment, including string UTF-8 bytes. */
 if(pangu_json_init(&stream,other,512,limits)) return 10;
 rc=13;
 for(pangu_u64 i=0;i<length && rc==13;i++) {
  input.data=bytes+i;input.length=1;rc=pangu_json_feed(&stream,input,0);
  if(pangu_json_get_result(&stream,&observed)==0 || observed.tokens || observed.count) return 11;
 }
 if(rc==13) { input.data=(void *)0;input.length=0;rc=pangu_json_feed(&stream,input,1); }
 if(pangu_json_get_result(&stream,&observed)!=rc || !same_tokens(&expected,&observed)) return 12;
 return 0;
}
static const pangu_json_token topology[]={
 {PANGU_JSON_OBJECT,0,0,31,~(pangu_u64)0,9,2},
 {PANGU_JSON_STRING,PANGU_JSON_KEY,1,4,0,2,0},
 {PANGU_JSON_ARRAY,0,5,21,0,7,2},
 {PANGU_JSON_TRUE,0,6,10,2,4,0},
 {PANGU_JSON_OBJECT,0,11,20,2,7,1},
 {PANGU_JSON_STRING,PANGU_JSON_KEY,12,15,4,6,0},
 {PANGU_JSON_STRING,0,16,19,4,7,0},
 {PANGU_JSON_STRING,PANGU_JSON_KEY,22,25,0,8,0},
 {PANGU_JSON_NULL,0,26,30,0,9,0}
};
int json_test(const pangu_memory_api *memory)
{
 limits.max_bytes=2097152;limits.max_depth=64;limits.reserved=0;
 for(pangu_u64 i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  test_case=i;int rc=run_case(cases[i].bytes,cases[i].length,(int)cases[i].valid);if(rc) return rc;
 }
 pangu_text_slice input;input.data=(const unsigned char *)"{\"a\":[true,{\"b\":\"x\"}],\"n\":null}";input.length=31;
 if(pangu_json_init(&one,tokens,512,limits) || pangu_json_feed(&one,input,1) || pangu_json_get_result(&one,&observed)) return 20;
 expected.tokens=topology;expected.count=9;expected.error_offset=31;expected.status=0;
 if(!same_tokens(&expected,&observed)) return 21;
 /* Raw invalid UTF-8 is not converted to JS replacement characters by an oracle. */
 static const unsigned char malformed[][6]={{'"',0xc0,0x80,'"',0,0},{'"',0xed,0xa0,0x80,'"',0},{'"',0xf4,0x90,0x80,0x80,'"'}};
 static const pangu_u64 malformed_lengths[]={4,5,6};
 for(pangu_u32 i=0;i<3;i++) {
  if(run_case(malformed[i],malformed_lengths[i],0) || expected.status!=12 || expected.error_offset!=1) return 22;
 }
 /* No partial result is published when quota is reached, at any split. */
 limits.max_bytes=1;
 if(run_case((const unsigned char *)" 0",2,0)!=2) return 23; /* run_case rejects quota as syntax; test explicitly below. */
 for(pangu_u64 cut=0;cut<=2;cut++) {
  if(pangu_json_init(&one,tokens,512,limits)) return 24;
  input.data=(const unsigned char *)" 0";input.length=cut;pangu_i32 rc=pangu_json_feed(&one,input,0);
  if(rc==13) { input.data+=cut;input.length=2-cut;rc=pangu_json_feed(&one,input,1); }
  if(rc!=2 || pangu_json_get_result(&one,&observed)!=2 || observed.count || observed.tokens || observed.error_offset!=1) return 25;
 }
 limits.max_bytes=2097152;limits.max_depth=2;
 input.data=(const unsigned char *)"[[[]]]";input.length=6;
 if(pangu_json_init(&one,tokens,512,limits) || pangu_json_feed(&one,input,1)!=2 || pangu_json_get_result(&one,&observed)!=2 || observed.error_offset!=2) return 26;
 limits.max_depth=64;
 for(pangu_u32 i=0;i<64;i++) { deep[i]='[';deep[65+i]=']'; }deep[64]='0';
 if(run_case(deep,129,1)) return 27;
 input.data=(const unsigned char *)"{\"a\":1}";input.length=7;
 if(pangu_json_init(&one,tokens,2,limits) || pangu_json_feed(&one,input,1)!=2 || pangu_json_get_result(&one,&observed)!=2 || observed.tokens || observed.count || observed.error_offset!=5) return 28;
 input.data=(const unsigned char *)"{}";input.length=2;
 if(pangu_json_init(&one,(void *)0,0,limits) || pangu_json_feed(&one,input,1)!=2) return 29;
 if(pangu_json_init(&one,tokens,512,limits) || pangu_json_feed(&one,input,0)!=13 || pangu_json_get_result(&one,&observed)!=13 || observed.tokens || observed.count) return 30;
 input.data=(void *)0;input.length=0;
 if(pangu_json_feed(&one,input,1) || pangu_json_get_result(&one,&observed) || observed.count!=1) return 31;
 input.length=1;
 if(pangu_json_init(&one,tokens,512,limits) || pangu_json_feed(&one,input,0)!=1 || one.offset || one.count) return 32;
 input.data=(const unsigned char *)"0";
 if(pangu_json_feed(&one,input,2)!=1 || one.offset || one.count) return 33;
 limits.max_depth=65;if(pangu_json_init(&one,tokens,512,limits)!=1) return 34;limits.max_depth=64;
 /* One-megabyte JSON string in one chunk and 257-byte chunks: two tokens slots suffice. */
 large[0]='"';large[sizeof(large)-1]='"';
 for(pangu_u64 i=1;i<sizeof(large)-1;i++) large[i]='x';
 if(pangu_json_init(&one,tokens,2,limits)) return 35;
 input.data=large;input.length=sizeof(large);
 if(pangu_json_feed(&one,input,1) || pangu_json_get_result(&one,&expected)) return 36;
 if(pangu_json_init(&stream,other,2,limits)) return 37;
 for(pangu_u64 i=0;i<sizeof(large);) {
  pangu_u64 n=sizeof(large)-i;if(n>257) n=257;input.data=large+i;input.length=n;
  if(pangu_json_feed(&stream,input,0)!=13) return 38;i+=n;
 }
 input.data=(void *)0;input.length=0;
 if(pangu_json_feed(&stream,input,1) || pangu_json_get_result(&stream,&observed) || !same_tokens(&expected,&observed) || stream.offset!=sizeof(large)) return 39;
 /* Place both input and token quota directly before inaccessible pages. */
 static pangu_mapping input_mapping,token_mapping;
 if(memory->map(memory->context,4096,&input_mapping) || memory->map(memory->context,4096,&token_mapping)) return 40;
 unsigned char *page=input_mapping.address,*token_page=token_mapping.address;
 const unsigned char *document=(const unsigned char *)"{\"a\":1}";
 for(pangu_u32 i=0;i<7;i++) page[4089+i]=document[i];
 for(pangu_u64 capacity=0;capacity<=3;capacity++) {
  pangu_json_token *guarded=(pangu_json_token *)(token_page+4096-capacity*48);
  unsigned char *canary=(unsigned char *)guarded-16;
  for(pangu_u32 i=0;i<16;i++) canary[i]=0xa5;
  for(pangu_u64 cut=0;cut<=7;cut++) {
   if(pangu_json_init(&one,guarded,capacity,limits)) return 41;
   input.data=page+4089;input.length=cut;pangu_i32 rc=pangu_json_feed(&one,input,0);
   if(rc==13) { input.data+=cut;input.length=7-cut;rc=pangu_json_feed(&one,input,1); }
   if(rc!=(capacity==3 ? 0 : 2) || pangu_json_get_result(&one,&observed)!=rc) return 42;
   if(capacity<3 && (observed.tokens || observed.count)) return 43;
   for(pangu_u32 i=0;i<16;i++) if(canary[i]!=0xa5) return 44;
  }
 }
 page[4095]='"';input.data=page+4095;input.length=1;
 if(pangu_json_init(&one,tokens,512,limits) || pangu_json_feed(&one,input,1)!=13) return 45;
 page[4093]='"';page[4094]=0xe4;page[4095]=0xb8;input.data=page+4093;input.length=3;
 if(pangu_json_init(&one,tokens,512,limits) || pangu_json_feed(&one,input,1)!=12 || pangu_json_get_result(&one,&observed)!=12 || observed.error_offset!=1) return 46;
 if(memory->unmap(memory->context,&token_mapping) || memory->unmap(memory->context,&input_mapping)) return 47;
 return 0;
}
