#include <pangu/services/schema.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>
#define SL(s) ((pangu_slice){s, sizeof(s) - 1})
#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      fprintf(stderr, "schema failure %d\n", __LINE__);                        \
      abort();                                                                 \
    }                                                                          \
  } while (0)
typedef struct { char *text; size_t len; int count; pangu_status status; } output;
static pangu_status capture(void *ctx,pangu_slice s) {
  output *o=ctx; ++o->count;
  o->text=malloc(s.len+1); CHECK(o->text); memcpy(o->text,s.data,s.len); o->text[s.len]=0; o->len=s.len;
  return o->status;
}
static void example(const char *schema,const char *raw,const char *expected) {
  output out={0},error={0}; pg_schema_diagnostics diag={SL("echo"),capture,&error};
  char *input=malloc(strlen(raw)+1); CHECK(input); strcpy(input,raw);
  pangu_status st=pg_schema_normalize(&diag,(pangu_slice){schema,strlen(schema)},(pangu_slice){input,strlen(input)},capture,&out);
  CHECK(!strcmp(input,raw)); free(input);
  if (expected) {
    CHECK(st==PANGU_OK && out.count==1 && !error.count);
    yyjson_doc *a=yyjson_read(out.text,out.len,0), *b=yyjson_read(expected,strlen(expected),0);
    CHECK(a && b && yyjson_equals(yyjson_doc_get_root(a),yyjson_doc_get_root(b)));
    yyjson_doc_free(a); yyjson_doc_free(b);
  } else CHECK(st==PANGU_TOOL_ERROR && !out.count && error.count==1 && strstr(error.text,"Validation failed for tool \"echo\":") && strstr(error.text,"Received arguments:"));
  free(out.text); free(error.text);
}
static void source_cases(void) {
  const char *types[]={"number","number","number","integer","boolean","boolean","boolean","boolean","string","string","null","null","null","[\"number\",\"string\"]","[\"boolean\",\"number\"]","boolean","boolean","null","integer"};
  const char *inputs[]={"\"42\"","true","null","\"42\"","\"true\"","\"false\"","1","0","null","true","\"\"","0","false","\"1\"","\"1\"","\"1\"","\"0\"","\"null\"","\"42.1\""};
  const char *results[]={"42","1","0","42","true","false","true","false","\"\"","\"true\"","null","null","null","\"1\"","1",NULL,NULL,NULL,NULL};
  for(size_t i=0;i<sizeof(types)/sizeof(*types);++i) {
    char schema[300],raw[100],expected[100];
    snprintf(schema,sizeof(schema),"{\"type\":\"object\",\"properties\":{\"value\":{\"type\":%s%s%s}},\"required\":[\"value\"]}",types[i][0]=='['?"":"\"",types[i],types[i][0]=='['?"":"\"");
    snprintf(raw,sizeof(raw),"{\"value\":%s}",inputs[i]);
    snprintf(expected,sizeof(expected),"{\"value\":%s}",results[i]?results[i]:"null");
    example(schema,raw,results[i]?expected:NULL);
  }
  example("{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\"},\"offset\":{\"type\":\"number\"},\"nullable\":{\"anyOf\":[{\"type\":\"string\"},{\"type\":\"null\"}]},\"metadata\":{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"}}}},\"required\":[\"path\",\"metadata\"]}","{\"path\":\"file.txt\",\"offset\":null,\"nullable\":null,\"metadata\":{\"enabled\":null}}","{\"path\":\"file.txt\",\"nullable\":null,\"metadata\":{}}");
  example("{\"type\":\"object\",\"properties\":{\"value\":{\"$ref\":\"#/$defs/value\"}},\"$defs\":{\"value\":{\"anyOf\":[{\"type\":\"number\"},{\"type\":\"null\"}]}}}","{\"value\":null}","{\"value\":null}");
  example("{\"type\":\"object\",\"properties\":{\"value\":{\"oneOf\":[{\"type\":\"number\"},{\"type\":\"null\"}]}},\"required\":[\"value\"]}","{\"value\":null}","{\"value\":null}");
  example("{\"type\":\"object\",\"properties\":{\"value\":{\"anyOf\":[{\"type\":\"number\"},{\"type\":\"null\"}]}},\"required\":[\"value\"]}","{\"value\":\"42\"}","{\"value\":42}");
  example("{\"type\":[\"array\",\"null\"],\"items\":{\"type\":\"string\"}}","null","null");
  example("{\"type\":\"array\",\"items\":[{\"type\":\"integer\"},{\"type\":\"boolean\"}]}","[\"42\",\"false\",7]","[42,false,7]");
  example("{\"type\":\"object\",\"additionalProperties\":{\"type\":\"number\"}}","{\"x\":\"42\",\"y\":true}","{\"x\":42,\"y\":1}");
  example("{\"allOf\":[{\"type\":\"object\",\"properties\":{\"x\":{\"type\":\"number\"}}},{\"properties\":{\"x\":{\"minimum\":2}}}]}","{\"x\":\"3\"}","{\"x\":3}");
  example("{\"type\":\"object\"}","{",NULL);
  output o={.status=PANGU_CANCELLED};
  CHECK(pg_schema_normalize(NULL,SL("{}"),SL("{}"),capture,&o)==PANGU_CANCELLED && o.count==1); free(o.text);
  output diagnostics={0},unused={0};
  pg_schema_diagnostics context={SL("read"),capture,&diagnostics};
  CHECK(pg_schema_normalize(&context,SL("{\"type\":\"object\",\"properties\":{\"metadata\":{\"type\":\"object\",\"required\":[\"enabled\"]}}}"),SL("{\"metadata\":{}}"),capture,&unused)==PANGU_TOOL_ERROR);
  CHECK(!unused.count && diagnostics.count==1 && strstr(diagnostics.text,"metadata.enabled:") && strstr(diagnostics.text,"Received arguments:\n{\n  \"metadata\": {}\n}"));
  free(diagnostics.text);
  diagnostics=(output){.status=PANGU_NOMEM};
  CHECK(pg_schema_normalize(&context,SL("{\"type\":\"number\"}"),SL("{}"),capture,&unused)==PANGU_NOMEM && !unused.count);
  free(diagnostics.text);

}
int main(int argc,char **argv) {
  if (argc==4 && !strcmp(argv[1],"--normalize")) {
    output out={0},err={0}; pg_schema_diagnostics d={SL("echo"),capture,&err};
    pangu_status st=pg_schema_normalize(&d,(pangu_slice){argv[2],strlen(argv[2])},(pangu_slice){argv[3],strlen(argv[3])},capture,&out);
    if (st==PANGU_OK) puts(out.text); else if(err.text) puts(err.text);
    free(out.text); free(err.text); return st==PANGU_OK?0:2;
  }
  source_cases();
  pangu_slice schema =
      SL("{\"type\":\"object\",\"properties\":{\"path\":{\"type\":\"string\","
         "\"minLength\":1},\"n\":{\"type\":\"integer\",\"minimum\":1}},"
         "\"required\":[\"path\"],\"additionalProperties\":false}");
  CHECK(pg_schema_validate(NULL, schema, SL("{\"path\":\"中文\",\"n\":2}")) ==
        PANGU_OK);
  CHECK(pg_schema_validate(NULL, schema, SL("{\"path\":\"\"}")) ==
        PANGU_TOOL_ERROR);
  CHECK(pg_schema_validate(NULL, schema, SL("{\"path\":2}")) ==
        PANGU_TOOL_ERROR);
  CHECK(pg_schema_validate(NULL, schema, SL("{\"path\":\"a\",\"n\":1.5}")) ==
        PANGU_TOOL_ERROR);
  CHECK(
      pg_schema_validate(NULL, schema, SL("{\"path\":\"a\",\"extra\":true}")) ==
      PANGU_TOOL_ERROR);
  CHECK(pg_schema_validate(NULL, schema, SL("{}")) == PANGU_TOOL_ERROR);
  CHECK(pg_schema_validate(NULL,
                           SL("{\"$ref\":\"https://invalid.example/schema\"}"),
                           SL("{}")) == PANGU_TOOL_ERROR);
  CHECK(pg_schema_validate(NULL, SL("{"), SL("{}")) == PANGU_TOOL_ERROR);
  CHECK(pg_schema_validate(NULL, SL("{\"type\":\"integer\"}"), SL("1 2")) ==
        PANGU_TOOL_ERROR);
  CHECK(pg_schema_validate(NULL, SL("{\"type\":\"integer\"}"), SL("1")) ==
        PANGU_OK);
  puts("schema: strict types, bounds, required, extra fields, invalid JSON and "
       "remote ref rejection passed");
  return 0;
}
