#include "tools.h"
#include <json-c/json.h>
#include <string.h>
static json_object *object(pg_slice value){
 if(!value.data || !value.size || value.size>=PG_TEXT_LIMIT || memchr(value.data,0,value.size))return 0;
 json_tokener *tok=json_tokener_new();if(!tok)return 0;
 json_object *o=json_tokener_parse_ex(tok,value.data,(int)value.size);
 size_t end=json_tokener_get_parse_end(tok);while(end<value.size && (value.data[end]==' '||value.data[end]=='\t'||value.data[end]=='\r'||value.data[end]=='\n'))++end;
 int failed=json_tokener_get_error(tok)!=json_tokener_success || end!=value.size || !o || !json_object_is_type(o,json_type_object);
 json_tokener_free(tok);if(failed){if(o)json_object_put(o);return 0;}return o;
}
static int name_valid(const char *s){size_t n;if(!s)return 0;for(n=0;n<PG_ID_LIMIT && s[n];++n){char c=s[n];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return 0;}return n>0&&n<PG_ID_LIMIT;}
int pg_tool_registry_init(pg_tool_registry *r,const pg_tool_definition *defs,size_t count){
 if(!r || count>PG_TOOL_LIMIT || (count&&!defs))return 1;
 for(size_t i=0;i<count;++i){const pg_tool_definition *d=&defs[i];
  if(!name_valid(d->name)||!d->description||strlen(d->description)>=PG_TEXT_LIMIT||!d->schema||!d->validate||!d->authorize||!d->execute)return 1;
  json_object *schema=object((pg_slice){d->schema,strlen(d->schema)});if(!schema)return 1;
  json_object *type=0;json_object_object_get_ex(schema,"type",&type);
  int invalid=!type || !json_object_is_type(type,json_type_string) || strcmp(json_object_get_string(type),"object");json_object_put(schema);if(invalid)return 1;
  for(size_t j=0;j<i;++j)if(!strcmp(d->name,defs[j].name))return 1;
 }
 pg_tool_registry candidate={0};candidate.count=count;for(size_t i=0;i<count;++i)candidate.entries[i]=defs[i];*r=candidate;return 0;
}
static const pg_tool_definition *lookup(const pg_tool_registry *r,pg_slice name){if(!r||!name.data)return 0;for(size_t i=0;i<r->count;++i)if(strlen(r->entries[i].name)==name.size&&!memcmp(name.data,r->entries[i].name,name.size))return &r->entries[i];return 0;}
static int available(void *ctx,pg_slice name,pg_slice arguments){(void)arguments;return lookup(ctx,name)!=0;}
static int error(char *out,size_t cap,size_t *size,const char *message){size_t n=strlen(message);if(n>=cap){*size=0;return 1;}memcpy(out,message,n);*size=n;return 1;}
static int execute(void *ctx,pg_slice name,pg_slice arguments,char *out,size_t cap,size_t *size){
 if(!out||!size)return 1;*size=0;
 const pg_tool_definition *d=lookup(ctx,name);if(!d)return error(out,cap,size,"unknown tool");
 json_object *args=object(arguments);if(!args)return error(out,cap,size,"invalid JSON object arguments");json_object_put(args);
 if(!d->validate(d->context,arguments))return error(out,cap,size,"tool argument validation failed");
 if(!d->authorize(d->context,arguments))return error(out,cap,size,"tool permission denied");
 int result=d->execute(d->context,arguments,out,cap,size);
 if(*size>=cap)return error(out,cap,size,"tool output limit exceeded");return result;
}
pg_tools pg_tool_registry_service(pg_tool_registry *r){return (pg_tools){r,available,execute};}
