#include "src/bootstrap-api/pi-arguments.h"
static int range_ok(const void *p,pangu_u64 n) { return (!n || p) && n<=~(pangu_u64)0-(pangu_u64)p; }
static int overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m)
{
 if(!n || !m) return 0;
 return a<=b ? b-a<n : a-b<m;
}
static int whitespace(pangu_u32 c)
{
 return (c>=9 && c<=13) || c==32 || c==0xa0 || c==0x1680 || (c>=0x2000 && c<=0x200a) || c==0x2028 || c==0x2029 || c==0x202f || c==0x205f || c==0x3000 || c==0xfeff;
}
static void clear(pangu_pi_arguments_result *out,pangu_i32 status)
{
 out->input.data=(void *)0;out->input.length=0;
 out->json.tokens=(void *)0;out->json.count=0;out->json.error_offset=0;out->json.status=status;out->json.reserved=0;
 out->path=0;out->reserved=0;out->original_error_offset=0;
}
static pangu_i32 parse(pangu_json_parser *parser,pangu_text_slice input,pangu_json_token *tokens,pangu_u64 capacity,pangu_json_limits limits,pangu_json_result *result)
{
 pangu_i32 rc=pangu_json_init(parser,tokens,capacity,limits);if(rc) return rc;
 rc=pangu_json_feed(parser,input,1);pangu_json_get_result(parser,result);return rc;
}
pangu_i32 pangu_pi_arguments_parse(pangu_text_slice input,pangu_u32 final,unsigned char *scratch,pangu_u64 scratch_capacity,pangu_json_token *tokens,pangu_u64 token_capacity,pangu_json_limits limits,pangu_pi_arguments_result *out)
{
 if(final>1 || !out || !limits.max_bytes || !limits.max_depth || limits.max_depth>64 || limits.reserved || token_capacity>~(pangu_u64)0/48 || !range_ok(input.data,input.length) || !range_ok(scratch,scratch_capacity) || !range_ok(tokens,token_capacity*48) || !range_ok(out,64)) return 1;
 pangu_u64 a[4],n[4];a[0]=(pangu_u64)input.data;n[0]=input.length;a[1]=(pangu_u64)scratch;n[1]=scratch_capacity;a[2]=(pangu_u64)tokens;n[2]=token_capacity*48;a[3]=(pangu_u64)out;n[3]=64;
 for(pangu_u32 i=0;i<4;i++) for(pangu_u32 j=i+1;j<4;j++) if(overlap(a[i],n[i],a[j],n[j])) return 1;
 clear(out,13);if(!final) return 13;
 out->path=PANGU_PI_ARGUMENTS_STRICT;
 if(input.length>limits.max_bytes) { out->json.status=2;return 2; }
 pangu_u64 at=0;pangu_u32 all_white=1;
 while(at<input.length) {
  pangu_u32 scalar;pangu_i32 rc=pangu_text_next(input,&at,&scalar);
  if(rc) { out->json.status=rc;out->json.error_offset=at;return rc; }
  if(!whitespace(scalar)) all_white=0;
 }
 pangu_text_slice selected=input;
 if(all_white) { static const unsigned char empty[]="{}";selected.data=empty;selected.length=2;out->path=PANGU_PI_ARGUMENTS_EMPTY; }
 pangu_json_parser parser;volatile unsigned char *zero=(volatile unsigned char *)&parser;
 for(pangu_u32 i=0;i<sizeof(parser);i++) zero[i]=0;
 pangu_json_result result;pangu_i32 rc=parse(&parser,selected,tokens,token_capacity,limits,&result);
 if((rc==13 || rc==14) && !all_white) {
  out->original_error_offset=result.error_offset;pangu_u64 need;
  pangu_i32 repair=pangu_pi_repair_utf8(input,(void *)0,0,limits.max_bytes,&need);
  if(repair) { out->json.status=repair;return repair; }
  if(need!=input.length) {
   out->path=PANGU_PI_ARGUMENTS_REPAIRED;
   if(need>limits.max_bytes || need>scratch_capacity) { out->json.status=2;return 2; }
   repair=pangu_pi_repair_utf8(input,scratch,scratch_capacity,limits.max_bytes,&need);
   if(repair) { out->json.status=repair;return repair; }
   selected.data=scratch;selected.length=need;rc=parse(&parser,selected,tokens,token_capacity,limits,&result);
  }
 }
 out->json.status=rc;out->json.error_offset=result.error_offset;
 if(rc) return rc;
 if(!result.count || result.tokens[0].kind!=PANGU_JSON_OBJECT) { out->json.status=9;return 9; }
 out->input=selected;out->json.tokens=result.tokens;out->json.count=result.count;return 0;
}
