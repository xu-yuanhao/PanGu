#include "src/bootstrap-api/pi-arguments.h"
#define NONE (~(pangu_u64)0)
static int range_ok(const void *p,pangu_u64 n) { return (!n || p) && n<=NONE-(pangu_u64)p; }
static int overlap(pangu_u64 a,pangu_u64 n,pangu_u64 b,pangu_u64 m)
{
 if(!n || !m) return 0;return a<=b ? b-a<n : a-b<m;
}
static pangu_i32 schema_ok(const pangu_pi_tree_schema *s)
{
 if(!s || !range_ok(s,48) || !s->json.result || !range_ok(s->json.result,32) || !range_ok(s->json.input.data,s->json.input.length)) return 1;
 const pangu_json_result *r=s->json.result;
 if(r->status || r->reserved || !r->tokens || !r->count || r->count>NONE/48 || !range_ok(r->tokens,r->count*48) || r->error_offset!=s->json.input.length) return 10;
 if(s->json.root>=r->count || s->kind_count!=r->count || s->kind_count>NONE/4 || !range_ok(s->kinds,s->kind_count*4) || (r->tokens[s->json.root].flags&PANGU_JSON_KEY)) return 1;
 return 0;
}
static void unpublish(pangu_pi_validation_result *out,pangu_i32 status)
{
 out->arguments.input.data=(void *)0;out->arguments.input.length=0;
 out->arguments.json.tokens=(void *)0;out->arguments.json.count=0;
 out->arguments.json.error_offset=0;out->arguments.json.status=status;out->arguments.json.reserved=0;
}
static void initialize(pangu_pi_validation_result *out)
{
 unpublish(out,13);out->arguments.path=0;out->arguments.reserved=0;out->arguments.original_error_offset=0;
 out->report.status=0;out->report.reason=0;out->report.schema_token=NONE;out->report.value_token=NONE;out->report.work=0;
 out->stage=0;out->reserved=0;out->work=0;
}
static void copy_report(pangu_pi_validation_result *out,const pangu_schema_report *r)
{
 out->report.status=r->status;out->report.reason=r->reason;out->report.schema_token=r->schema_token;out->report.value_token=r->value_token;out->report.work=r->work;
}
static pangu_i32 reparse(pangu_text_slice bytes,const pangu_pi_validation_storage *storage,pangu_json_limits limits,pangu_json_result *result)
{
 pangu_json_parser parser;volatile unsigned char *zero=(volatile unsigned char *)&parser;
 for(pangu_u32 i=0;i<sizeof(parser);i++) zero[i]=0;
 pangu_i32 rc=pangu_json_init(&parser,storage->tokens,storage->token_capacity,limits);if(rc) return rc;
 rc=pangu_json_feed(&parser,bytes,1);pangu_json_get_result(&parser,result);return rc;
}
static pangu_i32 consume(pangu_pi_validation_result *out,pangu_schema_limits *limits,const pangu_schema_report *report,pangu_i32 rc)
{
 copy_report(out,report);
 if(report->work>limits->max_work) { out->report.status=10;out->arguments.json.status=10;return 10; }
 out->work+=report->work;limits->max_work-=report->work;
 if(rc) out->arguments.json.status=rc;return rc;
}
static pangu_i32 budget(pangu_pi_validation_result *out,const pangu_schema_limits *limits)
{
 if(limits->max_work) return 0;
 out->arguments.json.status=2;out->report.status=2;out->report.reason=PANGU_SCHEMA_LIMIT;out->report.work=0;return 2;
}
pangu_i32 pangu_pi_arguments_validate(const pangu_pi_tree_schema *schema,pangu_text_slice input,pangu_u32 final,const pangu_pi_validation_storage *storage,pangu_json_limits json_limits,pangu_schema_limits schema_limits,pangu_pi_validation_result *out)
{
 if(!storage || !range_ok(storage,96) || !out || !range_ok(out,112) || final>1 || !range_ok(input.data,input.length) || !json_limits.max_bytes || !json_limits.max_depth || json_limits.max_depth>64 || json_limits.reserved || !schema_limits.max_work || !schema_limits.max_nodes || !schema_limits.max_depth || schema_limits.max_depth>64) return 1;
 pangu_i32 rc=schema_ok(schema);if(rc) return rc;
 if(!storage->byte_capacity[0] || !storage->byte_capacity[1] || !storage->token_capacity || storage->token_capacity>NONE/48 || storage->drop_capacity>NONE/8) return 1;
 pangu_u64 read[7],rn[7],write[7],wn[7];
 read[0]=(pangu_u64)schema;rn[0]=48;read[1]=(pangu_u64)storage;rn[1]=96;read[2]=(pangu_u64)input.data;rn[2]=input.length;
 read[3]=(pangu_u64)schema->json.result;rn[3]=32;read[4]=(pangu_u64)schema->json.input.data;rn[4]=schema->json.input.length;read[5]=(pangu_u64)schema->json.result->tokens;rn[5]=schema->json.result->count*48;read[6]=(pangu_u64)schema->kinds;rn[6]=schema->kind_count*4;
 write[0]=(pangu_u64)storage->repair;wn[0]=storage->repair_capacity;write[1]=(pangu_u64)storage->bytes[0];wn[1]=storage->byte_capacity[0];write[2]=(pangu_u64)storage->bytes[1];wn[2]=storage->byte_capacity[1];write[3]=(pangu_u64)storage->tokens;wn[3]=storage->token_capacity*48;write[4]=(pangu_u64)storage->scalar;wn[4]=storage->scalar_capacity;write[5]=(pangu_u64)storage->drops;wn[5]=storage->drop_capacity*8;write[6]=(pangu_u64)out;wn[6]=112;
 for(pangu_u32 i=0;i<7;i++) {
  if(!range_ok((const void *)write[i],wn[i])) return 1;
  for(pangu_u32 j=0;j<7;j++) if(overlap(write[i],wn[i],read[j],rn[j])) return 1;
  for(pangu_u32 j=i+1;j<7;j++) if(overlap(write[i],wn[i],write[j],wn[j])) return 1;
 }
 initialize(out);if(!final) return 13;
 out->stage=PANGU_PI_VALIDATE_PARSE;
 pangu_pi_arguments_result parsed;
 rc=pangu_pi_arguments_parse(input,1,storage->repair,storage->repair_capacity,storage->tokens,storage->token_capacity,json_limits,&parsed);
 out->arguments.path=parsed.path;out->arguments.original_error_offset=parsed.original_error_offset;
 if(rc) { out->arguments.json.status=rc;out->arguments.json.error_offset=parsed.json.error_offset;return rc; }
 pangu_json_result current;current.tokens=parsed.json.tokens;current.count=parsed.json.count;current.error_offset=parsed.json.error_offset;current.status=0;current.reserved=0;
 pangu_schema_view value;value.input.data=parsed.input.data;value.input.length=parsed.input.length;value.result=&current;value.root=0;
 pangu_schema_report report;report.status=0;report.reason=0;report.schema_token=NONE;report.value_token=NONE;report.work=0;
 pangu_u64 length=0;out->stage=PANGU_PI_VALIDATE_OPTIONAL;
 rc=pangu_pi_optional_nulls(&schema->json,&value,schema_limits,storage->drops,storage->drop_capacity,storage->bytes[0],storage->byte_capacity[0],&length,&report);
 if(consume(out,&schema_limits,&report,rc)) return out->arguments.json.status;
 pangu_text_slice selected={storage->bytes[0],length};rc=reparse(selected,storage,json_limits,&current);
 if(rc) { out->arguments.json.status=rc;out->arguments.json.error_offset=current.error_offset;return rc; }
 value.input.data=selected.data;value.input.length=selected.length;
 for(pangu_u32 pass=1;pass<=2;pass++) {
  out->stage=pass==1 ? PANGU_PI_VALIDATE_TYPED : PANGU_PI_VALIDATE_PLAIN;
  if(budget(out,&schema_limits)) return 2;
  if(pass==1 && schema->kinds[schema->json.root]==6) { out->arguments.json.status=9;out->report.status=9;out->report.reason=PANGU_SCHEMA_UNSUPPORTED;return 9; }
  pangu_u32 slot=pass==1 ? 1 : 0;
  rc=pangu_pi_tree_convert(schema,&value,pass,schema_limits,storage->scalar,storage->scalar_capacity,storage->bytes[slot],storage->byte_capacity[slot],&length,&report);
  if(consume(out,&schema_limits,&report,rc)) return out->arguments.json.status;
  selected.data=storage->bytes[slot];selected.length=length;rc=reparse(selected,storage,json_limits,&current);
  if(rc) { out->arguments.json.status=rc;out->arguments.json.error_offset=current.error_offset;return rc; }
  value.input.data=selected.data;value.input.length=selected.length;
 }
 out->stage=PANGU_PI_VALIDATE_SCHEMA;if(budget(out,&schema_limits)) return 2;
 rc=pangu_schema_validate(&schema->json,&value,&schema_limits,&report);
 if(consume(out,&schema_limits,&report,rc)) return out->arguments.json.status;
 out->arguments.input.data=value.input.data;out->arguments.input.length=value.input.length;
 out->arguments.json.tokens=current.tokens;out->arguments.json.count=current.count;out->arguments.json.error_offset=current.error_offset;out->arguments.json.status=0;
 out->stage=PANGU_PI_VALIDATE_READY;return 0;
}
