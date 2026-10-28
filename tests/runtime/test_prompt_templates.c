#define _XOPEN_SOURCE 700
#include "../../src/runtime/prompt_templates.h"
#include <assert.h>
#include <ftw.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static pg_slice s(const char *text){return (pg_slice){text,strlen(text)};}
static void write_file(const char *root,const char *name,const char *text){char path[4096];assert(snprintf(path,sizeof(path),"%s/%s",root,name)>0);FILE *file=fopen(path,"wb");assert(file);assert(fwrite(text,1,strlen(text),file)==strlen(text));assert(!fclose(file));}
static int remove_entry(const char *path,const struct stat *metadata,int kind,struct FTW *walk){(void)metadata;(void)kind;(void)walk;return remove(path);}
static void substitution(const char *body,const char *arguments,const char *expected){json_object *args=0;assert(!pg_prompt_args(s(arguments),&args));char out[8192];size_t size=0;assert(!pg_prompt_substitute(s(body),args,out,sizeof(out),&size));assert(size==strlen(expected)&&!strcmp(out,expected));json_object_put(args);}
static int cases(void){char *line=0;size_t capacity=0;while(getline(&line,&capacity,stdin)>=0){json_object *input=json_tokener_parse(line);assert(input);const char *body=json_object_get_string(json_object_object_get(input,"body")),*text=json_object_get_string(json_object_object_get(input,"arguments"));json_object *args=0;assert(!pg_prompt_args(s(text),&args));char out[65536];size_t size=0;int result=pg_prompt_substitute(s(body),args,out,sizeof(out),&size);json_object *reply=json_object_new_object();assert(reply);assert(!json_object_object_add(reply,"arguments",args));assert(!json_object_object_add(reply,"result",json_object_new_string_len(out,(int)size)));assert(!json_object_object_add(reply,"status",json_object_new_int(result)));puts(json_object_to_json_string_ext(reply,JSON_C_TO_STRING_PLAIN));json_object_put(reply);json_object_put(input);}free(line);return 0;}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--cases"))return cases();
 substitution("$1|$2|$3|$@|$ARGUMENTS","'a b' \"\" c","a b||c|a b  c|a b  c");
 substitution("${1:-default}|${2:-two}|${@:-none}","''","default|two|none");
 substitution("${@:2}|${@:0:2}|${@:2:0}|${@:9999999999999999999999}","one two three","two three|one two||");
 substitution("$1 ${2:-$@} $ARGUMENTS","'$2'","$2 $@ $2");
 substitution("$1|$2|$3","one\xe3\x80\x80'二 三'\xc2\xa0\"four","one|二 三|four");
 substitution("$0|$10|${0:-zero}|$ARGUMENTSX","x","||zero|xX");
 json_object *empty=0;assert(!pg_prompt_templates_load("/nonexistent-pangu-template-root",0,&empty)&&!json_object_array_length(json_object_object_get(empty,"templates")));json_object_put(empty);
 char temporary[]="/tmp/pangu-templates-XXXXXX";assert(mkdtemp(temporary));char user[4096],project[4096],skills[4096];snprintf(user,sizeof(user),"%s/user",temporary);snprintf(project,sizeof(project),"%s/project",temporary);snprintf(skills,sizeof(skills),"%s/skills",temporary);assert(!mkdir(user,0700)&&!mkdir(project,0700)&&!mkdir(skills,0700));
 write_file(user,"recipe.md","\xef\xbb\xbf---\r\ndescription: Recipe\r\nargument-hint: '[name]'\r\n---\r\nWrite $1 ${2:-fallback} $@\r\n");write_file(project,"recipe.md","Wrong project duplicate $1");write_file(project,"plain.md","\nFirst plain line\nNext line\n");write_file(project,"bad.md","---\nname: [bad\n---\n");
 char nested[4096];snprintf(nested,sizeof(nested),"%s/project/nested",temporary);assert(!mkdir(nested,0700));write_file(nested,"ignored.md","Not a direct child");
 json_object *catalog=0,*skill_catalog=0;assert(!pg_prompt_templates_load(user,project,&catalog));json_object *templates=json_object_object_get(catalog,"templates");assert(json_object_array_length(templates)==3);assert(json_object_array_length(json_object_object_get(catalog,"diagnostics"))==1);json_object *first=json_object_array_get_idx(templates,0);assert(!strcmp(json_object_get_string(json_object_object_get(first,"name")),"recipe"));assert(!strcmp(json_object_get_string(json_object_object_get(first,"argumentHint")),"[name]"));
 write_file(skills,"SKILL.md","---\nname: manual\ndescription: Hidden by default\ndisable-model-invocation: true\n---\n  Follow the manual skill.  \n");assert(!pg_skills_load(skills,0,&skill_catalog));
 char output[8192];size_t size=0;assert(!pg_resource_expand(catalog,skill_catalog,s("/recipe 'hello world'"),output,sizeof(output),&size));assert(!strcmp(output,"Write hello world fallback hello world"));
 assert(!pg_resource_expand(catalog,skill_catalog,s("/plain"),output,sizeof(output),&size));assert(!strcmp(output,"\nFirst plain line\nNext line\n"));
 assert(!pg_resource_expand(catalog,skill_catalog,s("/unknown x"),output,sizeof(output),&size)&&!strcmp(output,"/unknown x"));assert(!pg_resource_expand(catalog,skill_catalog,s("/recipeX x"),output,sizeof(output),&size)&&!strcmp(output,"/recipeX x"));
 assert(!pg_resource_expand(catalog,skill_catalog,s("/skill:manual argument"),output,sizeof(output),&size));assert(strstr(output,"<skill name=\"manual\" location=\"")&&strstr(output,"References are relative to ")&&strstr(output,"Follow the manual skill.\n</skill>\n\nargument"));
 write_file(skills,"SKILL.md","---\nname: manual\ndescription: Hidden by default\n---\nFresh invocation body\n");assert(!pg_resource_expand(catalog,skill_catalog,s("/skill:manual"),output,sizeof(output),&size)&&strstr(output,"Fresh invocation body"));
 assert(pg_resource_expand(catalog,skill_catalog,s("/recipe x"),output,8,&size)&&!size&&!output[0]);
 json_object_put(catalog);json_object_put(skill_catalog);assert(!nftw(temporary,remove_entry,24,FTW_DEPTH|FTW_PHYS));puts("PASS: prompt templates, quoted arguments, nonrecursive substitution and explicit skill invocation");
}
