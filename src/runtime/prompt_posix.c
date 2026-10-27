#define _XOPEN_SOURCE 700
#include "prompt.h"
#include <errno.h>
#include <git2.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
int pg_prompt_path_canonical(void *context,const char *path,char *out,size_t capacity){
 (void)context;if(!path||!out||capacity<4096)return 1;return realpath(path,out)?0:1;
}
int pg_prompt_git_paths(const char *path,char *worktree,size_t worktree_capacity,char *common,size_t common_capacity){
 if(!path||!worktree||!common||worktree_capacity<4096||common_capacity<4096)return 1;
 *worktree=*common=0;if(git_libgit2_init()<0)return 1;git_repository *repository=0;int result=1;
 if(!git_repository_open_ext(&repository,path,0,0)){const char *root=git_repository_workdir(repository),*shared=git_repository_commondir(repository);if(root&&shared&&!pg_prompt_path_canonical(0,root,worktree,worktree_capacity)&&!pg_prompt_path_canonical(0,shared,common,common_capacity))result=0;}
 git_repository_free(repository);git_libgit2_shutdown();if(result)*worktree=*common=0;return result;
}
static int resource_path(pg_prompt_file_source *source,const char *path,char *out){
 if(!source||!source->directory||!path)return 1;int size=snprintf(out,4096,"%s/%s",source->directory,path);return size<0||size>=4096;
}
int pg_prompt_resource_probe(void *context,const char *path){
 char full[4096];if(resource_path(context,path,full))return -1;struct stat metadata;
 if(stat(full,&metadata))return errno==ENOENT||errno==ENOTDIR?0:-1;return S_ISREG(metadata.st_mode)?1:0;
}
static int authorize(void *context,pg_slice name,pg_slice arguments){(void)context;(void)arguments;return name.size==4&&!memcmp(name.data,"read",4);}
static int execute(void *context,pg_slice name,pg_slice arguments,char *output,size_t capacity,size_t *size){
 pg_prompt_file_source *source=context;*size=0;if(!authorize(context,name,arguments)||!source||!source->complete)return PG_TOOL_ERROR;
 struct json_object *request=json_tokener_parse(arguments.data),*field=0;if(!request)return PG_TOOL_ERROR;
 field=json_object_object_get(request,"path");const char *path=json_object_get_string(field);char full[4096],canonical[4096];
 if(!path||!json_object_is_type(field,json_type_string)||(size_t)json_object_get_string_len(field)!=strlen(path)||resource_path(source,path,full)||pg_prompt_path_canonical(0,full,canonical,sizeof(canonical))){json_object_put(request);return PG_TOOL_ERROR;}
 field=json_object_new_string(canonical+1);if(!field){json_object_put(request);return PG_TOOL_ERROR;}if(json_object_object_add(request,"path",field)){json_object_put(field);json_object_put(request);return PG_TOOL_ERROR;}
 const char *encoded=json_object_to_json_string_ext(request,JSON_C_TO_STRING_PLAIN);int result=PG_TOOL_ERROR;
 if(encoded){pg_slice args={encoded,strlen(encoded)};const pg_tool_definition *reader=source->complete;if(reader->authorize(reader->context,args)==1)result=reader->execute(reader->context,args,output,capacity,size);}
 json_object_put(request);return result;
}
pg_tools pg_prompt_resource_files(pg_prompt_file_source *source){return (pg_tools){source,authorize,execute,0,0,0,0};}
