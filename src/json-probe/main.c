#include "src/bootstrap-api/json.h"
#include "src/bootstrap-api/memory.h"
#include "src/json-probe/cases.h"
static pangu_json_parser one,stream,init_probe;
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
static int json_reader_tests(const pangu_memory_api *memory);
static int json_quote_tests(const pangu_memory_api *memory);
static int json_number_tests(const pangu_memory_api *memory);
static int json_writer_tests(const pangu_memory_api *memory);
static int json_equal_tests(const pangu_memory_api *memory);
static volatile pangu_u32 equal_error;
static volatile pangu_u32 format_error;
static int json_format_tests(const pangu_memory_api *memory,pangu_u32 shard);
int json_test(const pangu_memory_api *memory,pangu_u64 shard)
{
 limits.max_bytes=2097152;limits.max_depth=64;limits.reserved=0;
 if(shard>3) return 247;
 if(shard) { int rc=json_format_tests(memory,(pangu_u32)shard);if(rc) { format_error=(pangu_u32)rc;return 246; }return 0; }
 /* First-use storage must be wholly zero, not just the header. Refusal must
  * preserve the dirty sentinel; valid resets may retain old frame contents. */
 init_probe.reserved=1;
 if(pangu_json_init(&init_probe,tokens,512,limits)!=1 || init_probe.abi_version || init_probe.struct_size || init_probe.reserved!=1) return 48;
 init_probe.reserved=0;
 if(pangu_json_init(&init_probe,tokens,512,limits)) return 49;
 init_probe.frames[63].phase=99;
 if(pangu_json_init(&init_probe,tokens,512,limits)) return 50;

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
 int reader_rc=json_reader_tests(memory);if(reader_rc) return 50+reader_rc;
 int quote_rc=json_quote_tests(memory);if(quote_rc) return 100+quote_rc;
 int number_rc=json_number_tests(memory);if(number_rc) return 150+number_rc;
 int writer_rc=json_writer_tests(memory);if(writer_rc) return 190+writer_rc;
 int equal_rc=json_equal_tests(memory);if(equal_rc) { equal_error=(pangu_u32)equal_rc;return 248; }
 int format_rc=json_format_tests(memory,0);if(format_rc) { format_error=(pangu_u32)format_rc;return 246; }
 return 0;
}

_Alignas(8) static unsigned char reader_out[128];
static unsigned char reader_encoded[16];
static pangu_json_result reader_result;
static volatile pangu_u64 reader_case;
static int reader_parse(pangu_text_slice input)
{
 return pangu_json_init(&one,tokens,512,limits) || pangu_json_feed(&one,input,1) || pangu_json_get_result(&one,&reader_result);
}
static void reader_fill(void) { for(pangu_u32 i=0;i<128;i++) reader_out[i]=0xa5; }
static int reader_untouched(void) { for(pangu_u32 i=0;i<128;i++) if(reader_out[i]!=0xa5) return 0;return 1; }
static pangu_u32 reference_utf8(pangu_u32 cp,unsigned char *out)
{
 if(cp<128) { out[0]=(unsigned char)cp;return 1; }
 if(cp<2048) { out[0]=(unsigned char)(192+cp/64);out[1]=(unsigned char)(128+cp%64);return 2; }
 if(cp<65536) { out[0]=(unsigned char)(224+cp/4096);out[1]=(unsigned char)(128+(cp/64)%64);out[2]=(unsigned char)(128+cp%64);return 3; }
 out[0]=(unsigned char)(240+cp/262144);out[1]=(unsigned char)(128+(cp/4096)%64);out[2]=(unsigned char)(128+(cp/64)%64);out[3]=(unsigned char)(128+cp%64);return 4;
}
static int reader_scalar(pangu_u32 first,pangu_u32 second,pangu_u32 pair)
{
 static const unsigned char digits[]="0123456789abcdef";
 reader_encoded[0]='"';reader_encoded[1]='\\';reader_encoded[2]='u';
 for(pangu_u32 i=0;i<4;i++) reader_encoded[3+i]=digits[(first>>((3-i)*4))&15];
 pangu_text_slice input;input.data=reader_encoded;input.length=8;reader_encoded[7]='"';
 if(pair) {
  reader_encoded[7]='\\';reader_encoded[8]='u';
  for(pangu_u32 i=0;i<4;i++) reader_encoded[9+i]=digits[(second>>((3-i)*4))&15];
  reader_encoded[13]='"';input.length=14;
 }
 if(reader_parse(input)) return 1;
 pangu_u64 length=777;reader_fill();pangu_i32 rc=pangu_json_string_decode(input,&reader_result,0,reader_out,128,&length);
 if(!pair && first>=0xd800 && first<=0xdfff) return rc==9 && !length && reader_untouched() ? 0 : 2;
 pangu_u32 cp=pair ? 65536+(first-0xd800)*1024+second-0xdc00 : first;
 unsigned char expected_bytes[4];pangu_u32 n=reference_utf8(cp,expected_bytes);
 if(rc || length!=n) return 3;
 for(pangu_u32 i=0;i<n;i++) if(reader_out[i]!=expected_bytes[i]) return 4;
 if(reader_out[n]!=0xa5) return 5;
 return 0;
}
static int json_reader_tests(const pangu_memory_api *memory)
{
 for(pangu_u64 i=0;i<sizeof(reader_cases)/sizeof(reader_cases[0]);i++) {
  reader_case=i;const struct reader_case *v=&reader_cases[i];pangu_text_slice input;input.data=v->bytes;input.length=v->length;
  if(reader_parse(input)) return 1;
  pangu_u64 n=v->decoded_length;
  pangu_u64 capacities[4];capacities[0]=0;capacities[1]=n ? n-1 : 0;capacities[2]=n;capacities[3]=n+1;
  for(pangu_u32 k=0;k<4;k++) {
   pangu_u64 length=777;reader_fill();pangu_i32 rc=pangu_json_string_decode(input,&reader_result,0,reader_out,capacities[k],&length);
   if(v->status) { if(rc!=(pangu_i32)v->status || length || !reader_untouched()) return 2; }
   else if(capacities[k]<n) { if(rc!=2 || length!=n || !reader_untouched()) return 3; }
   else {
    if(rc || length!=n) return 4;
    for(pangu_u32 b=0;b<n;b++) if(reader_out[b]!=v->decoded[b]) return 5;
    for(pangu_u64 b=n;b<128;b++) if(reader_out[b]!=0xa5) return 6;
   }
  }
  pangu_u64 length=777;pangu_i32 rc=pangu_json_string_decode(input,&reader_result,0,(void *)0,0,&length);
  if(v->status) { if(rc!=(pangu_i32)v->status || length) return 7; }
  else if(rc!=(n ? 2 : 0) || length!=n) return 8;
 }
 for(pangu_u64 i=0;i<sizeof(find_cases)/sizeof(find_cases[0]);i++) {
  reader_case=i;const struct find_case *v=&find_cases[i];pangu_text_slice input,key;input.data=v->bytes;input.length=v->length;key.data=v->key;key.length=v->key_length;
  if(reader_parse(input)) return 9;
  pangu_u64 value=777;pangu_i32 rc=pangu_json_object_find(input,&reader_result,0,key,&value);
  if(rc!=(pangu_i32)v->status) return 10;
  if(rc) { if(value!=~(pangu_u64)0) return 11;continue; }
  if(value>=reader_result.count || reader_result.tokens[value].kind!=v->kind || reader_result.tokens[value].parent!=0) return 12;
  if(v->kind==PANGU_JSON_STRING) {
   pangu_u64 length=777;if(pangu_json_string_decode(input,&reader_result,value,reader_out,128,&length) || length!=v->value_length) return 13;
   for(pangu_u64 j=0;j<length;j++) if(reader_out[j]!=v->value[j]) return 14;
  } else if(v->kind<=PANGU_JSON_NUMBER) {
   const pangu_json_token *t=&reader_result.tokens[value];if(t->end-t->start!=v->value_length) return 15;
   for(pangu_u64 j=0;j<v->value_length;j++) if(input.data[t->start+j]!=v->value[j]) return 16;
  }
 }
 /* Every UTF-16 code unit, including all 2048 isolated surrogate units. */
 for(pangu_u32 cp=0;cp<=65535;cp++) { reader_case=cp;if(reader_scalar(cp,0,0)) return 17; }
 for(pangu_u32 high=0;high<1024;high++) {
  if(reader_scalar(0xd800+high,0xdc00,1) || reader_scalar(0xd800+high,0xdfff,1)) return 18;
 }
 for(pangu_u32 low=0;low<1024;low++) if(reader_scalar(0xd955,0xdc00+low,1)) return 19;
 pangu_text_slice input;input.data=(const unsigned char *)"\"prefix\\ud800\"";input.length=14;
 if(reader_parse(input)) return 20;
 reader_fill();pangu_u64 length=777;
 if(pangu_json_string_decode(input,&reader_result,0,reader_out,128,&length)!=9 || length || !reader_untouched()) return 21;
 input.data=(const unsigned char *)"\"text\"";input.length=6;if(reader_parse(input)) return 22;
 length=777;if(pangu_json_string_decode(input,&reader_result,0,(unsigned char *)input.data,input.length,&length)!=1 || length!=777) return 23;
 if(pangu_json_string_decode(input,&reader_result,99,reader_out,128,&length)!=1 || length!=777) return 24;
 if(pangu_json_string_decode(input,&reader_result,0,reader_out,128,&tokens[0].start)!=1 || tokens[0].start) return 25;
 if(pangu_json_init(&one,tokens,512,limits) || pangu_json_feed(&one,input,0)!=13 || pangu_json_get_result(&one,&reader_result)!=13) return 26;
 if(pangu_json_string_decode(input,&reader_result,0,reader_out,128,&length)!=10 || length!=777) return 27;
 input.data=(const unsigned char *)"{\"x\":1}";input.length=7;if(reader_parse(input)) return 28;
 pangu_text_slice key;key.data=(const unsigned char *)"x";key.length=1;pangu_u64 index=777;
 if(pangu_json_object_find(input,&reader_result,0,key,&reader_result.count)!=1 || reader_result.count!=3) return 29;
 unsigned char bad=0xff;key.data=&bad;key.length=1;
 if(pangu_json_object_find(input,&reader_result,0,key,&index)!=12 || index!=~(pangu_u64)0) return 30;
 /* Exact-capacity UTF-8 output and escaped input both touch page ends. */
 static pangu_mapping in_map,out_map;
 if(memory->map(memory->context,4096,&in_map) || memory->map(memory->context,4096,&out_map)) return 31;
 unsigned char *in=in_map.address,*out=out_map.address;
 const unsigned char *literal=(const unsigned char *)"\"\\uD83D\\uDE00\"";
 for(pangu_u32 i=0;i<14;i++) in[4082+i]=literal[i];
 input.data=in+4082;input.length=14;if(reader_parse(input)) return 32;
 for(pangu_u32 i=0;i<8;i++) out[4088+i]=0xa5;
 length=777;
 if(pangu_json_string_decode(input,&reader_result,0,out+4093,3,&length)!=2 || length!=4) return 33;
 for(pangu_u32 i=0;i<8;i++) if(out[4088+i]!=0xa5) return 34;
 if(pangu_json_string_decode(input,&reader_result,0,out+4092,4,&length) || length!=4 || out[4092]!=0xf0 || out[4093]!=0x9f || out[4094]!=0x98 || out[4095]!=0x80) return 35;
 if(memory->unmap(memory->context,&out_map) || memory->unmap(memory->context,&in_map)) return 36;
 return 0;
}

_Alignas(8) static unsigned char quote_out[2048];
static unsigned char quote_back[256];
static void quote_fill(void) { for(pangu_u32 i=0;i<2048;i++) quote_out[i]=0xa5; }
static int quote_clean(void) { for(pangu_u32 i=0;i<2048;i++) if(quote_out[i]!=0xa5) return 0;return 1; }
static int json_quote_tests(const pangu_memory_api *memory)
{
 for(pangu_u64 i=0;i<sizeof(quote_cases)/sizeof(quote_cases[0]);i++) {
  const struct quote_case *v=&quote_cases[i];pangu_text_slice input={v->bytes,v->length};
  pangu_u64 n=v->quoted_length,length=777,caps[4]={0,n-1,n,n+1};
  if(pangu_json_quote_utf8(input,(void *)0,0,&length)!=2 || length!=n) return 1;
  for(pangu_u32 k=0;k<4;k++) {
   quote_fill();length=777;pangu_i32 rc=pangu_json_quote_utf8(input,quote_out,caps[k],&length);
   if(length!=n) return 2;
   if(caps[k]<n) { if(rc!=2 || !quote_clean()) return 3;continue; }
   if(rc) return 4;
   for(pangu_u64 j=0;j<n;j++) if(quote_out[j]!=v->quoted[j]) return 5;
   if(quote_out[n]!=0xa5) return 6;
   pangu_text_slice encoded={quote_out,n};if(reader_parse(encoded)) return 7;
   if(pangu_json_string_decode(encoded,&reader_result,0,quote_back,256,&length) || length!=input.length) return 8;
   for(pangu_u64 j=0;j<length;j++) if(quote_back[j]!=input.data[j]) return 9;
  }
 }
 static const unsigned char bad[][5]={{255,0,0,0,0},{192,128,0,0,0},{237,160,128,0,0},{244,144,128,128,0},{'a',226,130,0,0}};
 static const pangu_u64 sizes[]={1,2,3,4,3};
 for(pangu_u32 i=0;i<5;i++) { pangu_text_slice input={bad[i],sizes[i]};pangu_u64 length=777;quote_fill();if(pangu_json_quote_utf8(input,quote_out,2048,&length)!=12 || length || !quote_clean()) return 10; }
 pangu_text_slice input={quote_out,1};pangu_u64 length=777;quote_fill();
 if(pangu_json_quote_utf8(input,quote_out,2048,&length)!=1 || length!=777 || !quote_clean()) return 11;
 input.data=(const unsigned char *)"a";
 if(pangu_json_quote_utf8(input,quote_out,2048,(pangu_u64 *)quote_out)!=1 || !quote_clean()) return 12;
 pangu_mapping a,b;a.address=(void *)0;a.length=0;b.address=(void *)0;b.length=0;if(memory->map(memory->context,4096,&a) || memory->map(memory->context,4096,&b)) return 13;
 unsigned char *src=a.address,*dst=b.address;src[4093]='"';src[4094]='\\';src[4095]=0;
 input.data=src+4093;input.length=3;length=777;
 if(pangu_json_quote_utf8(input,dst+4084,12,&length) || length!=12) return 14;
 pangu_text_slice encoded={dst+4084,12};if(reader_parse(encoded)) return 15;
 if(pangu_json_string_decode(encoded,&reader_result,0,quote_back,256,&length) || length!=3 || quote_back[0]!='"' || quote_back[1]!='\\' || quote_back[2]) return 16;
 src[4095]=255;quote_fill();if(pangu_json_quote_utf8(input,quote_out,2048,&length)!=12 || length || !quote_clean()) return 17;
 if(memory->unmap(memory->context,&b) || memory->unmap(memory->context,&a)) return 18;
 return 0;
}

static volatile pangu_u64 number_case_index;
static unsigned char number_over[1048577];
static int json_number_tests(const pangu_memory_api *memory)
{
 for(pangu_u64 i=0;i<sizeof(number_cases)/sizeof(number_cases[0]);i++) {
  number_case_index=i;const struct number_case *v=&number_cases[i];pangu_text_slice input={v->bytes,v->length};pangu_u64 bits=123;
  if(reader_parse(input)) return 1;
  if(pangu_json_number_bits(input,&reader_result,0,&bits) || bits!=v->bits) return 2;
 }
 pangu_text_slice input={(const unsigned char *)"[0,null]",8};pangu_u64 bits=123;
 if(reader_parse(input)) return 3;
 if(pangu_json_number_bits(input,&reader_result,0,&bits)!=9 || bits!=123) return 4;
 if(pangu_json_number_bits(input,&reader_result,2,&bits)!=9 || bits!=123) return 5;
 if(pangu_json_number_bits(input,&reader_result,3,&bits)!=1 || bits!=123) return 6;
 if(pangu_json_number_bits(input,&reader_result,1,&bits) || bits) return 7;
 if(pangu_json_number_bits(input,&reader_result,1,(pangu_u64 *)input.data)!=1) return 8;
 if(pangu_json_number_bits(input,&reader_result,1,(pangu_u64 *)tokens)!=1 || pangu_json_number_bits(input,&reader_result,1,(pangu_u64 *)&reader_result)!=1) return 9;
 reader_result.status=13;bits=123;
 if(pangu_json_number_bits(input,&reader_result,1,&bits)!=10 || bits!=123) return 10;
 /* A million-digit mantissa with compensating exponent must not saturate
  * before cancellation. Remaining input work is bounded by the lexeme quota. */
 const pangu_u64 zeros=1048500;large[0]='1';for(pangu_u64 i=1;i<=zeros;i++) large[i]='0';
 static const unsigned char suffix[]="e-1048500";
 for(pangu_u64 i=0;i<9;i++) large[zeros+1+i]=suffix[i];
 input.data=large;input.length=zeros+10;if(reader_parse(input)) return 11;
 if(pangu_json_number_bits(input,&reader_result,0,&bits) || bits!=0x3ff0000000000000ULL) return 12;
 /* Late nonzero after an exact midpoint: sticky tail must round upward. */
 static const unsigned char half[]="1.00000000000000011102230246251565404236316680908203125";
 for(pangu_u64 i=0;i<55;i++) large[i]=half[i];
 for(pangu_u64 i=55;i<1048575;i++) large[i]='0';large[1048575]='1';input.length=1048576;
 if(reader_parse(input)) return 13;
 if(pangu_json_number_bits(input,&reader_result,0,&bits) || bits!=0x3ff0000000000001ULL) return 14;
 /* Over-quota success at the parser remains a reader quota refusal. */
 number_over[0]='1';for(pangu_u64 i=1;i<1048577;i++) number_over[i]='0';
 input.data=number_over;input.length=1048577;bits=123;
 if(reader_parse(input)) return 20;
 if(pangu_json_number_bits(input,&reader_result,0,&bits)!=2 || bits!=123) return 21;
 /* Exponent saturation remains dominant over every allowed mantissa. */
 large[0]='-';large[1]='1';large[2]='e';for(pangu_u64 i=3;i<1048576;i++) large[i]='9';
 input.data=large;input.length=1048576;
 if(reader_parse(input) || pangu_json_number_bits(input,&reader_result,0,&bits) || bits!=0xfff0000000000000ULL) return 22;
 large[3]='-';if(reader_parse(input) || pangu_json_number_bits(input,&reader_result,0,&bits) || bits!=0x8000000000000000ULL) return 23;
 /* Leading fractional zeros cancel a large, exactly represented exponent. */
 large[0]='0';large[1]='.';for(pangu_u64 i=2;i<1048502;i++) large[i]='0';large[1048502]='1';
 static const unsigned char plus[]="e1048501";for(pangu_u64 i=0;i<8;i++) large[1048503+i]=plus[i];
 input.length=1048511;
 if(reader_parse(input) || pangu_json_number_bits(input,&reader_result,0,&bits) || bits!=0x3ff0000000000000ULL) return 24;

 pangu_mapping mapping;mapping.address=(void *)0;mapping.length=0;
 if(memory->map(memory->context,4096,&mapping)) return 15;
 unsigned char *page=mapping.address;page[4094]='-';page[4095]='0';input.data=page+4094;input.length=2;
 if(reader_parse(input) || pangu_json_number_bits(input,&reader_result,0,&bits) || bits!=0x8000000000000000ULL) return 16;
 page[4095]='1';input.data=page+4095;input.length=1;if(reader_parse(input)) return 17;
 pangu_u64 *out=(pangu_u64 *)(page+4080);
 if(pangu_json_number_bits(input,&reader_result,0,out) || *out!=0x3ff0000000000000ULL) return 18;
 if(memory->unmap(memory->context,&mapping)) return 19;
 return 0;
}

static pangu_json_writer writer;
static unsigned char writer_out[8192],writer_before[8192],writer_state[560];
static unsigned char writer_large[1048578];
static volatile pangu_u64 writer_case_index,writer_capacity,writer_operation;
static pangu_text_slice writer_none;
static void writer_save(pangu_u64 n)
{
 const unsigned char *w=(const unsigned char *)&writer;
 for(pangu_u64 i=0;i<560;i++) writer_state[i]=w[i];
 for(pangu_u64 i=0;i<n;i++) writer_before[i]=writer_out[i];
}
static int writer_same(pangu_u64 n)
{
 const unsigned char *w=(const unsigned char *)&writer;
 for(pangu_u64 i=0;i<560;i++) if(writer_state[i]!=w[i]) return 0;
 for(pangu_u64 i=0;i<n;i++) if(writer_before[i]!=writer_out[i]) return 0;
 return 1;
}
static int writer_reject(pangu_u32 op,pangu_text_slice arg,pangu_i32 expected_rc)
{
 writer_save(8192);pangu_i32 rc=pangu_json_writer_emit(&writer,op,arg);
 return rc==expected_rc && writer_same(8192);
}
static int json_writer_tests(const pangu_memory_api *memory)
{
 for(pangu_u64 i=0;i<sizeof(writer_cases)/sizeof(writer_cases[0]);i++) {
  writer_case_index=i;const struct writer_case *v=&writer_cases[i];pangu_u64 scope=v->length+16;
  for(pangu_u64 cap=0;cap<=(pangu_u64)v->length+1;cap++) {
   writer_capacity=cap;
   for(pangu_u64 j=0;j<scope;j++) writer_out[j]=0xa5;
   if(pangu_json_writer_init(&writer,writer_out,cap,64)) return 1;
   int failed=0;
   for(pangu_u64 j=0;j<v->count;j++) {
    writer_operation=j;const struct writer_op *op=&writer_ops[v->start+j];pangu_text_slice arg;
    arg.data=op->length ? op->bytes : (void *)0;arg.length=op->length;
    writer_save(scope);pangu_i32 rc=pangu_json_writer_emit(&writer,op->op,arg);
    if(rc) { if(rc!=2 || !writer_same(scope)) return 2;failed=1;break; }
   }
   pangu_text_slice out;out.data=(const unsigned char *)1;out.length=777;
   if(cap<v->length) {
    writer_save(scope);if(!failed || pangu_json_writer_finish(&writer,&out)!=13 || out.data!=(const unsigned char *)1 || out.length!=777 || !writer_same(scope)) return 3;
   } else {
    if(failed || pangu_json_writer_finish(&writer,&out) || out.data!=writer_out || out.length!=v->length) return 4;
    for(pangu_u64 j=0;j<out.length;j++) if(out.data[j]!=v->bytes[j]) return 5;
    for(pangu_u64 j=out.length;j<scope;j++) if(writer_out[j]!=0xa5) return 6;
    if(reader_parse(out)) return 7;
    writer_save(scope);if(pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_NULL,writer_none)!=10 || !writer_same(scope)) return 8;
    out.data=(void *)0;out.length=888;if(pangu_json_writer_finish(&writer,&out)!=10 || out.data || out.length!=888 || !writer_same(scope)) return 9;
   }
  }
 }
 if(pangu_json_writer_init(&writer,writer_out,8192,64)) return 10;
 if(!writer_reject(PANGU_JSON_WRITE_END_OBJECT,writer_none,10) || !writer_reject(PANGU_JSON_WRITE_KEY,writer_none,10) || !writer_reject(0,writer_none,1) || !writer_reject(11,writer_none,1)) return 11;
 pangu_text_slice arg={(const unsigned char *)"x",1};
 if(!writer_reject(PANGU_JSON_WRITE_TRUE,arg,1)) return 12;
 if(pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_OBJECT,writer_none)) return 13;
 if(!writer_reject(PANGU_JSON_WRITE_END_ARRAY,writer_none,10) || !writer_reject(PANGU_JSON_WRITE_STRING,arg,10)) return 14;
 if(pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_KEY,arg)) return 15;
 if(!writer_reject(PANGU_JSON_WRITE_KEY,arg,10) || !writer_reject(PANGU_JSON_WRITE_END_OBJECT,writer_none,10)) return 16;
 if(pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_ARRAY,writer_none) || !writer_reject(PANGU_JSON_WRITE_KEY,arg,10)) return 17;
 static const unsigned char *const invalid[]={(const unsigned char *)"1e",(const unsigned char *)"1 2",(const unsigned char *)" 1 ",(const unsigned char *)"NaN",(const unsigned char *)"+1"};
 static const pangu_u64 sizes[]={2,3,3,3,2};
 for(pangu_u64 i=0;i<5;i++) { arg.data=invalid[i];arg.length=sizes[i];if(!writer_reject(PANGU_JSON_WRITE_NUMBER,arg,14)) return 18; }
 arg.data=(const unsigned char *)"true";arg.length=4;if(!writer_reject(PANGU_JSON_WRITE_NUMBER,arg,9)) return 19;
 static const unsigned char bad[]={0xed,0xa0,0x80};arg.data=bad;arg.length=3;if(!writer_reject(PANGU_JSON_WRITE_STRING,arg,12)) return 20;
 arg.data=writer_out;arg.length=1;if(!writer_reject(PANGU_JSON_WRITE_STRING,arg,1)) return 21;
 arg.data=(const unsigned char *)&writer;arg.length=1;if(!writer_reject(PANGU_JSON_WRITE_KEY,arg,1)) return 22;
 writer_save(8192);if(pangu_json_writer_finish(&writer,(pangu_text_slice *)writer_out)!=1 || !writer_same(8192) || pangu_json_writer_finish(&writer,(pangu_text_slice *)&writer)!=1 || !writer_same(8192)) return 23;
 number_over[0]='1';for(pangu_u64 i=1;i<1048577;i++) number_over[i]='0';arg.data=number_over;arg.length=1048577;
 if(!writer_reject(PANGU_JSON_WRITE_NUMBER,arg,2)) return 24;
 /* A rejected operation must not poison later valid completion. */
 arg.data=(const unsigned char *)"-0";arg.length=2;
 if(pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_NUMBER,arg) || pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_END_ARRAY,writer_none) || pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_END_OBJECT,writer_none)) return 25;
 pangu_text_slice out;if(pangu_json_writer_finish(&writer,&out) || reader_parse(out)) return 26;
 if(out.length!=10) return 27;
 static const unsigned char duplicate[]="{\"a\":1,\"a\":2}";
 if(pangu_json_writer_init(&writer,writer_out,8192,64) || pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_OBJECT,writer_none)) return 28;
 for(pangu_u32 i=1;i<=2;i++) {
  arg.data=(const unsigned char *)"a";arg.length=1;if(pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_KEY,arg)) return 29;
  unsigned char n=(unsigned char)('0'+i);arg.data=&n;if(pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_NUMBER,arg)) return 30;
 }
 if(pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_END_OBJECT,writer_none) || pangu_json_writer_finish(&writer,&out) || out.length!=13) return 31;
 for(pangu_u64 i=0;i<13;i++) if(out.data[i]!=duplicate[i]) return 32;
 if(reader_parse(out)) return 33;
 pangu_u64 index,bits;arg.data=(const unsigned char *)"a";arg.length=1;
 if(pangu_json_object_find(out,&reader_result,0,arg,&index) || pangu_json_number_bits(out,&reader_result,index,&bits) || bits!=0x4000000000000000ULL) return 34;
 if(pangu_json_writer_init(&writer,writer_out,8192,64)) return 35;
 for(pangu_u32 i=0;i<64;i++) if(pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_ARRAY,writer_none)) return 36;
 if(!writer_reject(PANGU_JSON_WRITE_ARRAY,writer_none,2) || pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_NULL,writer_none)) return 37;
 for(pangu_u32 i=0;i<64;i++) if(pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_END_ARRAY,writer_none)) return 38;
 if(pangu_json_writer_finish(&writer,&out) || out.length!=132 || reader_parse(out)) return 39;
 /* Large successful string: no temporary proportional allocation. */
 for(pangu_u64 i=0;i<1048576;i++) large[i]='a';arg.data=large;arg.length=1048576;
 if(pangu_json_writer_init(&writer,writer_large,1048578,64) || pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_STRING,arg) || pangu_json_writer_finish(&writer,&out)) return 40;
 if(out.length!=1048578 || out.data[0]!='"' || out.data[1048577]!='"') return 41;
 for(pangu_u64 i=1;i<=1048576;i++) if(out.data[i]!='a') return 42;
 if(reader_parse(out)) return 43;
 pangu_mapping a,b;a.address=(void *)0;a.length=0;b.address=(void *)0;b.length=0;
 if(memory->map(memory->context,4096,&a) || memory->map(memory->context,4096,&b)) return 44;
 unsigned char *page=a.address,*dest=b.address;pangu_json_writer *edge=(pangu_json_writer *)(page+4096-560);
 if(pangu_json_writer_init(edge,dest+4094,2,1) || pangu_json_writer_emit(edge,PANGU_JSON_WRITE_STRING,writer_none) || pangu_json_writer_finish(edge,&out) || out.length!=2 || dest[4094]!='"' || dest[4095]!='"') return 45;
 dest[4095]=0xa5;
 if(pangu_json_writer_init(edge,dest+4095,1,1) || pangu_json_writer_emit(edge,PANGU_JSON_WRITE_STRING,writer_none)!=2 || edge->length || edge->root || dest[4095]!=0xa5) return 46;
 if(memory->unmap(memory->context,&b) || memory->unmap(memory->context,&a)) return 47;
 /* Preserve every strict number lexeme, including -0 and overflow. This
  * intentionally does not promise JSON.stringify(Number(lexeme)) spelling. */
 for(pangu_u64 i=0;i<sizeof(number_cases)/sizeof(number_cases[0]);i++) {
  const struct number_case *v=&number_cases[i];arg.data=v->bytes;arg.length=v->length;
  if(pangu_json_writer_init(&writer,writer_out,v->length-1,64) || !writer_reject(PANGU_JSON_WRITE_NUMBER,arg,2)) return 49;
  if(pangu_json_writer_init(&writer,writer_out,v->length,64) || pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_NUMBER,arg) || pangu_json_writer_finish(&writer,&out) || out.length!=v->length) return 50;
  for(pangu_u64 j=0;j<out.length;j++) if(out.data[j]!=v->bytes[j]) return 51;
  if(reader_parse(out) || pangu_json_number_bits(out,&reader_result,0,&bits) || bits!=v->bits) return 52;
 }
 /* Largest permitted numeric lexeme succeeds and retains every byte. */
 large[0]='1';for(pangu_u64 i=1;i<1048576;i++) large[i]='0';arg.data=large;arg.length=1048576;
 if(pangu_json_writer_init(&writer,writer_large,1048576,64) || pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_NUMBER,arg) || pangu_json_writer_finish(&writer,&out) || out.length!=1048576) return 53;
 for(pangu_u64 i=0;i<out.length;i++) if(out.data[i]!=large[i]) return 54;
 if(reader_parse(out) || pangu_json_number_bits(out,&reader_result,0,&bits) || bits!=0x7ff0000000000000ULL) return 55;
 /* Invalid init arguments preserve the previous state and bytes. */
 writer_save(8192);
 if(pangu_json_writer_init(&writer,writer_out,8192,0)!=1 || !writer_same(8192) || pangu_json_writer_init(&writer,writer_out,8192,65)!=1 || !writer_same(8192) || pangu_json_writer_init(&writer,(unsigned char *)&writer,560,64)!=1 || !writer_same(8192)) return 48;
 return 0;
}

_Alignas(8) static unsigned char format_out[64];
static volatile pangu_u64 format_case_index,format_mode,format_step;
static void format_fill(void) { for(pangu_u32 i=0;i<64;i++) format_out[i]=0xa5; }
static int format_clean(void) { for(pangu_u32 i=0;i<64;i++) if(format_out[i]!=0xa5) return 0;return 1; }
static int json_format_tests(const pangu_memory_api *memory,pangu_u32 shard)
{
 for(pangu_u64 i=shard;i<sizeof(format_cases)/sizeof(format_cases[0]);i+=4) {
  format_case_index=i;const struct format_case *v=&format_cases[i];
  for(pangu_u32 mode=1;mode<=2;mode++) {
   format_mode=mode;const unsigned char *bytes=mode==1 ? v->json : v->string;pangu_u64 n=mode==1 ? v->json_length : v->string_length,length=777;
   format_fill();if(pangu_json_number_text(v->bits,mode,format_out,64,&length) || length!=n) return 1;
   for(pangu_u64 j=0;j<n;j++) if(format_out[j]!=bytes[j]) return 2;
   for(pangu_u64 j=n;j<64;j++) if(format_out[j]!=0xa5) return 3;
   pangu_u64 magnitude=v->bits&0x7fffffffffffffffULL;
   if(mode==1) {
    pangu_text_slice input={format_out,n};if(reader_parse(input)) return 4;
    if(magnitude>=0x7ff0000000000000ULL) { if(reader_result.tokens[0].kind!=PANGU_JSON_NULL) return 5; }
    else { pangu_u64 roundtrip,expected_bits=magnitude ? v->bits : 0;if(pangu_json_number_bits(input,&reader_result,0,&roundtrip) || roundtrip!=expected_bits) return 6; }
   }
   if(i%257==0 || magnitude>=0x7ff0000000000000ULL || !magnitude) {
    format_fill();length=777;
    if(pangu_json_number_text(v->bits,mode,(void *)0,0,&length)!=2 || length!=n) return 7;
    pangu_u64 caps[4]={0,n-1,n,n+1};
    for(pangu_u32 k=0;k<4;k++) {
     format_step=k;length=777;format_fill();pangu_i32 rc=pangu_json_number_text(v->bits,mode,format_out,caps[k],&length);
     if(length!=n) return 8;
     if(caps[k]<n) { if(rc!=2 || !format_clean()) return 9; }
     else {
      if(rc) return 10;
      for(pangu_u64 j=0;j<n;j++) if(format_out[j]!=bytes[j]) return 11;
      for(pangu_u64 j=n;j<64;j++) if(format_out[j]!=0xa5) return 12;
     }
    }
   }
  }
 }
 if(shard) return 0;
 format_fill();pangu_u64 length=777;
 if(pangu_json_number_text(0,0,format_out,64,&length)!=1 || length!=777 || !format_clean()) return 13;
 if(pangu_json_number_text(0,3,format_out,64,&length)!=1 || length!=777 || !format_clean()) return 14;
 if(pangu_json_number_text(0,1,(void *)0,1,&length)!=1 || length!=777 || !format_clean()) return 15;
 if(pangu_json_number_text(0,1,format_out,64,(void *)0)!=1 || !format_clean()) return 16;
 if(pangu_json_number_text(0,1,format_out,64,(pangu_u64 *)format_out)!=1 || !format_clean()) return 17;
 /* Integrate formatted primitive numbers and JS numeric strings with writer. */
 for(pangu_u32 strings=0;strings<2;strings++) {
  if(pangu_json_writer_init(&writer,writer_out,8192,64) || pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_ARRAY,writer_none)) return 18;
  for(pangu_u64 i=0;i<sizeof(format_integration_indices)/sizeof(format_integration_indices[0]);i++) {
   const struct format_case *v=&format_cases[format_integration_indices[i]];
   if(pangu_json_number_text(v->bits,strings ? 2 : 1,format_out,64,&length)) return 19;
   pangu_text_slice input={format_out,length};pangu_u32 op=strings ? PANGU_JSON_WRITE_STRING : ((v->bits&0x7fffffffffffffffULL)>=0x7ff0000000000000ULL ? PANGU_JSON_WRITE_NULL : PANGU_JSON_WRITE_NUMBER);
   if(op==PANGU_JSON_WRITE_NULL) input=writer_none;
   if(pangu_json_writer_emit(&writer,op,input)) return 20;
  }
  pangu_text_slice out;if(pangu_json_writer_emit(&writer,PANGU_JSON_WRITE_END_ARRAY,writer_none) || pangu_json_writer_finish(&writer,&out)) return 21;
  const unsigned char *expected_bytes=strings ? format_integration_strings : format_integration_json;
  pangu_u64 expected_length=strings ? sizeof(format_integration_strings) : sizeof(format_integration_json);
  if(out.length!=expected_length || reader_parse(out)) return 22;
  for(pangu_u64 i=0;i<out.length;i++) if(out.data[i]!=expected_bytes[i]) return 23;
 }
 pangu_mapping a,b;a.address=(void *)0;a.length=0;b.address=(void *)0;b.length=0;
 if(memory->map(memory->context,4096,&a) || memory->map(memory->context,4096,&b)) return 24;
 unsigned char *page=a.address,*length_page=b.address;pangu_u64 *edge_length=(pangu_u64 *)(length_page+4088);
 for(pangu_u64 i=0;i<sizeof(format_integration_indices)/sizeof(format_integration_indices[0]);i++) {
  const struct format_case *v=&format_cases[format_integration_indices[i]];
  for(pangu_u32 mode=1;mode<=2;mode++) {
   pangu_u64 n=mode==1 ? v->json_length : v->string_length;const unsigned char *bytes=mode==1 ? v->json : v->string;
   *edge_length=777;if(pangu_json_number_text(v->bits,mode,page+4096-n,n,edge_length) || *edge_length!=n) return 25;
   for(pangu_u64 j=0;j<n;j++) if(page[4096-n+j]!=bytes[j]) return 26;
   for(pangu_u64 j=0;j<n;j++) page[4096-n+j]=0xa5;
   *edge_length=777;if(pangu_json_number_text(v->bits,mode,page+4097-n,n-1,edge_length)!=2 || *edge_length!=n) return 27;
   for(pangu_u64 j=0;j<n;j++) if(page[4096-n+j]!=0xa5) return 28;
  }
 }
 if(memory->unmap(memory->context,&b) || memory->unmap(memory->context,&a)) return 29;
 return 0;
}

static unsigned char equal_a[32],equal_b[32];
static unsigned char equal_large[1048576];
_Alignas(8) static unsigned char equal_alias_a[8]={'"','a','b','"'},equal_alias_b[8]={'"','a','b','"'};
static volatile pangu_u64 equal_case_index;
static int equal_parse(pangu_text_slice a,pangu_text_slice b)
{
 return pangu_json_init(&one,tokens,512,limits) || pangu_json_feed(&one,a,1) || pangu_json_get_result(&one,&expected)
  || pangu_json_init(&stream,other,512,limits) || pangu_json_feed(&stream,b,1) || pangu_json_get_result(&stream,&observed);
}
static int equal_check(pangu_text_slice a,pangu_u64 ia,pangu_text_slice b,pangu_u64 ib,pangu_u32 wanted)
{
 pangu_u32 value=777;if(pangu_json_string_equal(a,&expected,ia,b,&observed,ib,&value) || value!=wanted) return 1;
 value=777;if(pangu_json_string_equal(b,&observed,ib,a,&expected,ia,&value) || value!=wanted) return 2;
 return 0;
}
static void equal_escape(unsigned char *out,pangu_u32 u,pangu_u32 upper)
{
 const unsigned char *digits=(const unsigned char *)(upper ? "0123456789ABCDEF" : "0123456789abcdef");
 out[0]='"';out[1]='\\';out[2]='u';
 for(pangu_u32 i=0;i<4;i++) out[3+i]=digits[(u>>((3-i)*4))&15];out[7]='"';
}
static int equal_pair(pangu_u32 high,pangu_u32 low)
{
 equal_escape(equal_a,high,0);equal_a[7]='\\';equal_a[8]='u';
 static const unsigned char digits[]="0123456789abcdef";
 for(pangu_u32 i=0;i<4;i++) equal_a[9+i]=digits[(low>>((3-i)*4))&15];equal_a[13]='"';
 equal_b[0]='"';pangu_u32 cp=0x10000+(high-0xd800)*1024+low-0xdc00,n=reference_utf8(cp,equal_b+1);equal_b[n+1]='"';
 pangu_text_slice a={equal_a,14},b={equal_b,n+2};return equal_parse(a,b) || equal_check(a,0,b,0,1);
}
static int json_equal_tests(const pangu_memory_api *memory)
{
 for(pangu_u64 i=0;i<sizeof(equal_cases)/sizeof(equal_cases[0]);i++) {
  equal_case_index=i;const struct equal_case *v=&equal_cases[i];pangu_text_slice a={v->a,v->a_length},b={v->b,v->b_length};
  if(equal_parse(a,b) || equal_check(a,0,b,0,v->equal)) return 1;
  pangu_u32 value=777;if(pangu_json_string_equal(a,&expected,0,a,&expected,0,&value) || value!=1) return 2;
 }
 for(pangu_u32 u=0;u<65536;u++) {
  equal_case_index=u;equal_escape(equal_a,u,0);equal_escape(equal_b,u,1);pangu_text_slice a={equal_a,8},b={equal_b,8};
  if(equal_parse(a,b) || equal_check(a,0,b,0,1)) return 3;
  equal_escape(equal_b,(u+1)&65535,0);if(equal_parse(a,b) || equal_check(a,0,b,0,0)) return 4;
  if(u>=0x20 && u!='"' && u!='\\' && !(u>=0xd800 && u<=0xdfff)) {
   equal_b[0]='"';pangu_u32 n=reference_utf8(u,equal_b+1);equal_b[n+1]='"';b.length=n+2;
   if(equal_parse(a,b) || equal_check(a,0,b,0,1)) return 5;
  }
 }
 for(pangu_u32 high=0xd800;high<=0xdbff;high++) if(equal_pair(high,0xdc00) || equal_pair(high,0xdfff)) return 6;
 for(pangu_u32 low=0xdc00;low<=0xdfff;low++) if(equal_pair(0xd835,low)) return 7;
 static const unsigned char key_json[]="{\"\\ud800\":1}",lone_json[]="\"\\uD800\"";
 pangu_text_slice a={key_json,sizeof(key_json)-1},b={lone_json,sizeof(lone_json)-1};
 if(equal_parse(a,b) || equal_check(a,1,b,0,1)) return 8;
 static const unsigned char nested_a[]="[\"a\",\"b\"]",nested_b[]="{\"k\":\"\\u0062\"}";
 a.data=nested_a;a.length=sizeof(nested_a)-1;b.data=nested_b;b.length=sizeof(nested_b)-1;
 if(equal_parse(a,b) || equal_check(a,2,b,2,1)) return 9;
 a.data=equal_alias_a;a.length=4;b.data=equal_alias_b;b.length=4;if(equal_parse(a,b)) return 10;
 pangu_u32 value=777;
 if(pangu_json_string_equal(a,&expected,1,b,&observed,0,&value)!=1 || value!=777 || pangu_json_string_equal(a,&expected,0,b,&observed,1,&value)!=1 || value!=777) return 11;
 if(pangu_json_string_equal(a,&expected,0,b,&observed,0,(void *)0)!=1) return 12;
 if(pangu_json_string_equal(a,&expected,0,b,&observed,0,(pangu_u32 *)equal_alias_a)!=1 || equal_alias_a[0]!='"' || equal_alias_a[1]!='a') return 13;
 if(pangu_json_string_equal(a,&expected,0,b,&observed,0,(pangu_u32 *)equal_alias_b)!=1 || equal_alias_b[0]!='"' || equal_alias_b[1]!='a') return 14;
 if(pangu_json_string_equal(a,&expected,0,b,&observed,0,(pangu_u32 *)&expected)!=1 || pangu_json_string_equal(a,&expected,0,b,&observed,0,(pangu_u32 *)&observed)!=1 || pangu_json_string_equal(a,&expected,0,b,&observed,0,(pangu_u32 *)tokens)!=1 || pangu_json_string_equal(a,&expected,0,b,&observed,0,(pangu_u32 *)other)!=1 || equal_check(a,0,b,0,1)) return 15;
 observed.status=13;if(pangu_json_string_equal(a,&expected,0,b,&observed,0,&value)!=10 || value!=777) return 16;observed.status=0;
 expected.reserved=1;if(pangu_json_string_equal(a,&expected,0,b,&observed,0,&value)!=10 || value!=777) return 17;expected.reserved=0;
 static const unsigned char number[]="0";a.data=number;a.length=1;if(equal_parse(a,b)) return 18;
 if(pangu_json_string_equal(a,&expected,0,b,&observed,0,&value)!=9 || value!=777) return 19;
 if(pangu_json_string_equal(b,&observed,0,a,&expected,0,&value)!=9 || value!=777) return 20;
 /* Full-size input and late mismatch: no proportional temporary storage. */
 large[0]='"';equal_large[0]='"';for(pangu_u64 i=1;i<1048575;i++) { large[i]='a';equal_large[i]='a'; }
 large[1048575]='"';equal_large[1048575]='"';a.data=large;a.length=1048576;b.data=equal_large;b.length=1048576;
 if(equal_parse(a,b) || equal_check(a,0,b,0,1)) return 21;
 equal_large[1048574]='b';if(equal_parse(a,b) || equal_check(a,0,b,0,0)) return 22;
 pangu_mapping x,y,z;x.address=(void *)0;x.length=0;y.address=(void *)0;y.length=0;z.address=(void *)0;z.length=0;
 if(memory->map(memory->context,4096,&x) || memory->map(memory->context,4096,&y) || memory->map(memory->context,4096,&z)) return 23;
 unsigned char *px=x.address,*py=y.address,*pz=z.address;
 static const unsigned char pair[]="\"\\ud83d\\ude00\"";for(pangu_u32 i=0;i<14;i++) px[4082+i]=pair[i];
 py[4090]='"';py[4091]=0xf0;py[4092]=0x9f;py[4093]=0x98;py[4094]=0x80;py[4095]='"';
 a.data=px+4082;a.length=14;b.data=py+4090;b.length=6;if(equal_parse(a,b)) return 24;
 pangu_u32 *edge=(pangu_u32 *)(pz+4092);*edge=777;
 if(pangu_json_string_equal(a,&expected,0,b,&observed,0,edge) || *edge!=1) return 25;
 if(memory->unmap(memory->context,&z) || memory->unmap(memory->context,&y) || memory->unmap(memory->context,&x)) return 26;
 return 0;
}
