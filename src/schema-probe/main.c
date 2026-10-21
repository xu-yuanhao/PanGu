#include "src/bootstrap-api/schema.h"
#include "src/bootstrap-api/memory.h"
#include "src/schema-probe/cases.h"
static pangu_json_parser parser[2];
static pangu_json_token tokens[2][1024];
static pangu_json_result results[2];
static pangu_schema_view views[2];
static pangu_schema_limits limits={16777216,64,4096};
static volatile pangu_u64 test_case,test_step;
static unsigned char large[1048578],deep_schema[1024],deep_value[256];
static pangu_u64 length(const char *s) { pangu_u64 n=0;while(s[n]) n++;return n; }
static int same(const void *x,const void *y,pangu_u64 n)
{
 const unsigned char *a=x,*b=y;for(pangu_u64 i=0;i<n;i++) if(a[i]!=b[i]) return 0;return 1;
}
static void fill(void *x,pangu_u64 n,unsigned char b) { unsigned char *a=x;for(pangu_u64 i=0;i<n;i++) a[i]=b; }
static pangu_u64 hash(const void *x,pangu_u64 n)
{
 const unsigned char *p=x;pangu_u64 h=1469598103934665603ULL;for(pangu_u64 i=0;i<n;i++) h=(h^p[i])*1099511628211ULL;return h;
}
static int parse(pangu_u32 slot,const unsigned char *bytes,pangu_u64 n)
{
 pangu_json_limits q={2097152,64,0};pangu_text_slice s={bytes,n};
 if(pangu_json_init(&parser[slot],tokens[slot],1024,q) || pangu_json_feed(&parser[slot],s,1) || pangu_json_get_result(&parser[slot],&results[slot])) return 1;
 views[slot].input=s;views[slot].result=&results[slot];views[slot].root=0;return 0;
}
static int strings(const char *s,const char *v)
{
 return parse(0,(const unsigned char *)s,length(s)) || parse(1,(const unsigned char *)v,length(v));
}
static int cases_test(void)
{
 for(pangu_u64 i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
  test_case=i;const schema_case *c=&cases[i];pangu_schema_report report;
  if(parse(0,c->schema,c->schema_length) || parse(1,c->value,c->value_length)) return 1;
  pangu_u64 a=hash(c->schema,c->schema_length),b=hash(c->value,c->value_length),t=hash(tokens,sizeof(tokens));
  pangu_i32 expect=c->status==15 ? 0 : c->status;
  if(pangu_schema_check(&views[0],&limits,&report)!=expect || report.status!=expect || report.work>limits.max_work) return 2;
  if(pangu_schema_validate(&views[0],&views[1],&limits,&report)!=c->status || report.status!=c->status || report.work>limits.max_work) return 3;
  if(a!=hash(c->schema,c->schema_length) || b!=hash(c->value,c->value_length) || t!=hash(tokens,sizeof(tokens))) return 4;
 }
 return 0;
}
static int quotas(void)
{
 pangu_schema_report report;pangu_schema_limits q=limits;
 if(strings("{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"string\"}},\"required\":[\"a\"],\"additionalProperties\":false}","{\"a\":\"x\"}")) return 10;
 if(pangu_schema_validate(&views[0],&views[1],&q,&report)) return 11;
 pangu_u64 work=report.work;
 for(pangu_u64 i=1;i<=work+1;i++) {
  test_step=i;q.max_work=i;pangu_i32 expected=i<work ? 2 : 0;
  if(pangu_schema_validate(&views[0],&views[1],&q,&report)!=expected || report.status!=expected || report.work>i || (expected && report.reason!=PANGU_SCHEMA_LIMIT)) return 12;
 }
 q=limits;q.max_nodes=1;if(pangu_schema_check(&views[0],&q,&report)!=2) return 13;
 q.max_nodes=2;if(pangu_schema_check(&views[0],&q,&report)) return 14;
 /* 63 nested items plus a boolean leaf = 64 logical Schema/value frames. */
 pangu_u64 n=0,m=0;
 for(pangu_u32 i=0;i<63;i++) { const char *p="{\"items\":";for(pangu_u64 j=0;p[j];j++) deep_schema[n++]=(unsigned char)p[j];deep_value[m++]='['; }
 const char *p="true";for(pangu_u32 i=0;i<4;i++) deep_schema[n++]=(unsigned char)p[i];deep_value[m++]='0';
 for(pangu_u32 i=0;i<63;i++) { deep_schema[n++]='}';deep_value[m++]=']'; }
 if(parse(0,deep_schema,n) || parse(1,deep_value,m)) return 15;
 q=limits;q.max_nodes=64;if(pangu_schema_validate(&views[0],&views[1],&q,&report)) return 16;
 q.max_nodes=63;if(pangu_schema_check(&views[0],&q,&report)!=2) return 17;
 q=limits;q.max_depth=63;if(pangu_schema_validate(&views[0],&views[1],&q,&report)!=2) return 18;
 /* One MiB string, same bytes decoded from a const Schema and raw value. */
 large[0]='"';for(pangu_u64 i=1;i<1048577;i++) large[i]='x';large[1048577]='"';
 if(strings("{\"type\":\"string\"}","null") || parse(1,large,sizeof(large))) return 19;
 q=limits;if(pangu_schema_validate(&views[0],&views[1],&q,&report)) return 20;
 return 0;
}
static int refusals(void)
{
 pangu_schema_report report,before;pangu_schema_limits q=limits;
 if(strings("{\"type\":\"string\"}","\"x\"")) return 30;
 fill(&report,sizeof(report),0xa5);before=report;
 if(pangu_schema_validate((void *)0,&views[1],&q,&report)!=1 || !same(&report,&before,32)) return 31;
 if(pangu_schema_validate(&views[0],(void *)0,&q,&report)!=1 || !same(&report,&before,32)) return 32;
 for(pangu_u32 i=0;i<4;i++) {
  q=limits;if(i==0) q.max_work=0;if(i==1) q.max_nodes=0;if(i==2) q.max_depth=0;if(i==3) q.max_depth=65;
  if(pangu_schema_check(&views[0],&q,&report)!=1 || !same(&report,&before,32)) return 33;
 }
 q=limits;views[0].root=results[0].count;
 if(pangu_schema_check(&views[0],&q,&report)!=1 || !same(&report,&before,32)) return 34;
 views[0].root=0;results[0].status=13;
 if(pangu_schema_check(&views[0],&q,&report)!=10 || !same(&report,&before,32)) return 35;
 results[0].status=0;
 for(pangu_u32 i=0;i<2;i++) {
  pangu_u64 saved=hash(&views[i],32);
  if(pangu_schema_validate(&views[0],&views[1],&q,(pangu_schema_report *)&views[i])!=1 || saved!=hash(&views[i],32)) return 36;
  saved=hash(&results[i],32);
  if(pangu_schema_validate(&views[0],&views[1],&q,(pangu_schema_report *)&results[i])!=1 || saved!=hash(&results[i],32)) return 37;
  saved=hash(tokens[i],sizeof(tokens[i]));
  if(pangu_schema_validate(&views[0],&views[1],&q,(pangu_schema_report *)tokens[i])!=1 || saved!=hash(tokens[i],sizeof(tokens[i]))) return 38;
  if(pangu_schema_validate(&views[0],&views[1],&q,(pangu_schema_report *)views[i].input.data)!=1) return 39;
 }
 pangu_u64 saved=hash(&q,sizeof(q));
 if(pangu_schema_check(&views[0],&q,(pangu_schema_report *)&q)!=1 || saved!=hash(&q,sizeof(q))) return 40;
 /* Use a parsed nested subtree as either Schema or value. */
 if(strings("{\"s\":{\"type\":\"integer\"}}","[2]")) return 41;
 views[0].root=2;views[1].root=1;
 if(pangu_schema_validate(&views[0],&views[1],&q,&report)) return 42;
 return 0;
}
static int pages(const pangu_memory_api *memory)
{
 pangu_mapping maps[3];
 for(pangu_u32 i=0;i<3;i++) { maps[i].address=(void *)0;maps[i].length=0; }
 for(pangu_u32 i=0;i<3;i++) if(memory->map(memory->context,4096,&maps[i])) return 50;
 const char *s="{\"type\":\"object\",\"properties\":{\"a\":{\"enum\":[\"\\ud800\",\"😀\"]}},\"required\":[\"a\"],\"additionalProperties\":false}";
 const char *v="{\"a\":\"\\ud83d\\ude00\"}";
 pangu_u64 n=length(s),m=length(v);unsigned char *a=(unsigned char *)maps[0].address+4096-n,*b=(unsigned char *)maps[1].address+4096-m;
 for(pangu_u64 i=0;i<n;i++) a[i]=(unsigned char)s[i];
 for(pangu_u64 i=0;i<m;i++) b[i]=(unsigned char)v[i];
 if(parse(0,a,n) || parse(1,b,m)) return 51;
 pangu_schema_report *report=(pangu_schema_report *)((unsigned char *)maps[2].address+4096-32);
 if(pangu_schema_validate(&views[0],&views[1],&limits,report) || report->status) return 52;
 for(pangu_u32 i=0;i<3;i++) if(memory->unmap(memory->context,&maps[i])) return 53;
 return 0;
}
int schema_test(const pangu_memory_api *memory,pangu_u64 shard)
{
 if(shard) return 247;
 int rc=cases_test();if(rc) return rc;rc=quotas();if(rc) return rc;rc=refusals();if(rc) return rc;return pages(memory);
}
