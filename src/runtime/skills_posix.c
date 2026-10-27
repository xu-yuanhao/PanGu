#define _XOPEN_SOURCE 700
#include "skills.h"
#include <dirent.h>
#include <errno.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <yaml.h>
static int add_owned(struct json_object *object,const char *key,struct json_object *value){
 if(!value)return 5;if(!json_object_object_add(object,key,value))return 0;json_object_put(value);return 5;
}
/* Directory entries expose host layouts; selection and traversal live in ASM. */
int pg_skills_directory(const char *path,struct json_object **out){
 *out=0;struct dirent **entries=0;int count=scandir(path,&entries,0,alphasort);
 if(count<0)return errno==ENOENT||errno==ENOTDIR?0:1;
 struct json_object *array=json_object_new_array();int result=array?0:5;
 for(int i=0;i<count;i++){
  if(!result){char full[4096],canonical[4096];struct stat metadata;int n=snprintf(full,sizeof(full),"%s/%s",path,entries[i]->d_name);
   if(n<0||n>=(int)sizeof(full))result=1;
   else if(!stat(full,&metadata)&&realpath(full,canonical)){
    struct json_object *entry=json_object_new_object();
    if(!entry)result=5;
    else if(add_owned(entry,"name",json_object_new_string(entries[i]->d_name))||add_owned(entry,"path",json_object_new_string(full))||add_owned(entry,"canonical",json_object_new_string(canonical))||add_owned(entry,"kind",json_object_new_int(S_ISREG(metadata.st_mode)?1:S_ISDIR(metadata.st_mode)?2:0))){json_object_put(entry);result=5;}
    else if(json_object_array_add(array,entry)){json_object_put(entry);result=5;}
   }
  }
  free(entries[i]);
 }
 free(entries);if(result)json_object_put(array);else *out=array;return result;
}
/* libyaml owns node buffers. Only these public scalar fields cross this bridge. */
static struct json_object *scalar(yaml_node_t *node,int *error){
 if(node->type!=YAML_SCALAR_NODE)return 0;
 const char *text=(const char *)node->data.scalar.value;size_t size=node->data.scalar.length;
 if(size>2147483646||strlen(text)!=size)return 0;
 if(node->data.scalar.style==YAML_PLAIN_SCALAR_STYLE){
  if(!strcmp(text,"true")||!strcmp(text,"True")||!strcmp(text,"TRUE"))return (*error=5),json_object_new_boolean(1);
  if(!strcmp(text,"false")||!strcmp(text,"False")||!strcmp(text,"FALSE"))return (*error=5),json_object_new_boolean(0);
  if(!strcmp(text,"null")||!strcmp(text,"Null")||!strcmp(text,"NULL")||!strcmp(text,"~")||!*text)return 0;
  struct json_tokener *parser=json_tokener_new();if(!parser){*error=5;return 0;}json_tokener_set_flags(parser,JSON_TOKENER_STRICT);
  struct json_object *number=json_tokener_parse_ex(parser,text,(int)size+1);int valid=json_tokener_get_error(parser)==json_tokener_success&&(json_object_is_type(number,json_type_int)||json_object_is_type(number,json_type_double));json_tokener_free(parser);
  if(valid)return number;json_object_put(number);
 }
 *error=5;return json_object_new_string_len(text,(int)size);
}
int pg_skills_yaml(const char *data,size_t size,struct json_object **out){
 *out=0;yaml_parser_t parser;if(!yaml_parser_initialize(&parser))return 5;
 yaml_parser_set_input_string(&parser,(const unsigned char *)data,size);yaml_document_t document;
 if(!yaml_parser_load(&parser,&document)){yaml_parser_delete(&parser);return 1;}
 yaml_node_t *root=yaml_document_get_root_node(&document);struct json_object *fields=json_object_new_object(),*keys=json_object_new_object();int result=fields&&keys?0:5;
 if(!result&&root&&root->type==YAML_MAPPING_NODE){
  for(yaml_node_pair_t *pair=root->data.mapping.pairs.start;pair<root->data.mapping.pairs.top&&!result;pair++){
   yaml_node_t *key=yaml_document_get_node(&document,pair->key),*value=yaml_document_get_node(&document,pair->value);
   if(key->type!=YAML_SCALAR_NODE){result=1;break;}const char *name=(const char *)key->data.scalar.value;
   if(json_object_object_get(keys,name)){result=1;break;}
   struct json_object *flag=json_object_new_boolean(1);if(!flag||json_object_object_add(keys,name,flag)){json_object_put(flag);result=5;break;}
   if(!strcmp(name,"name")||!strcmp(name,"description")||!strcmp(name,"disable-model-invocation")){
    int error=0;struct json_object *field=scalar(value,&error);if(!field&&error)result=error;else if(field&&json_object_object_add(fields,name,field)){json_object_put(field);result=5;}
   }
  }
 }
 json_object_put(keys);yaml_document_delete(&document);yaml_parser_delete(&parser);
 if(result)json_object_put(fields);else *out=fields;return result;
}
