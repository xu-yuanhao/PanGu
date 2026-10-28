#define _XOPEN_SOURCE 700
#include "../../src/runtime/settings.h"
#include "../../src/runtime/prompt_templates.h"
#include <assert.h>
#include <ftw.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static json_object *get(json_object *value,const char *key){return json_object_object_get(value,key);}
static void write_file(const char *root,const char *name,const char *text){char path[4096];assert(snprintf(path,sizeof(path),"%s/%s",root,name)>0);FILE *file=fopen(path,"wb");assert(file);assert(fwrite(text,1,strlen(text),file)==strlen(text));assert(!fclose(file));}
static void directory(const char *root,const char *name){char path[4096];assert(snprintf(path,sizeof(path),"%s/%s",root,name)>0);assert(!mkdir(path,0700));}
static int remove_entry(const char *path,const struct stat *metadata,int kind,struct FTW *walk){(void)metadata;(void)kind;(void)walk;return remove(path);}
int main(void){
 json_object *base=json_tokener_parse("{\"nested\":{\"keep\":1,\"replace\":2},\"array\":[1],\"null\":1}"),*over=json_tokener_parse("{\"nested\":{\"replace\":3},\"array\":[2],\"null\":null}"),*merged=0,*expected=json_tokener_parse("{\"nested\":{\"keep\":1,\"replace\":3},\"array\":[2],\"null\":null}");assert(!pg_settings_merge(base,over,&merged));assert(json_object_equal(merged,expected));assert(!json_object_array_add(get(over,"array"),json_object_new_int(9)));assert(json_object_equal(merged,expected));json_object_put(base);json_object_put(over);json_object_put(merged);json_object_put(expected);
 char temporary[]="/tmp/pangu-settings-XXXXXX";assert(mkdtemp(temporary));directory(temporary,"user");directory(temporary,".pangu");directory(temporary,"team");directory(temporary,"user/extras");directory(temporary,"user/templates");char user[4096],project[4096],global_file[4096],project_file[4096];snprintf(user,sizeof(user),"%s/user",temporary);snprintf(project,sizeof(project),"%s/.pangu",temporary);snprintf(global_file,sizeof(global_file),"%s/user/settings.json",temporary);snprintf(project_file,sizeof(project_file),"%s/.pangu/settings.json",temporary);
 write_file(user,"settings.json","\xef\xbb\xbf{\"skills\":[\" extras/guide.md \"],\"prompts\":[\"templates\"],\"nested\":{\"keep\":1,\"replace\":1}}\r\n");write_file(project,"settings.json","{\"skills\":[\"../team\"],\"prompts\":[\"../project-recipe.md\"],\"nested\":{\"replace\":2}}\n");
 write_file(user,"extras/guide.md","---\nname: guide\ndescription: Extra user guide\n---\nGuide body\n");write_file(temporary,"team/SKILL.md","---\nname: team\ndescription: Project team recipe\n---\nTeam body\n");write_file(user,"templates/custom.md","Custom $1\n");write_file(temporary,"project-recipe.md","Project $1\n");
 json_object *settings=0,*paths=0,*catalog=0,*templates=0;assert(!pg_settings_load(global_file,project_file,&settings));assert(!json_object_array_length(get(settings,"diagnostics")));assert(json_object_get_int(get(get(get(settings,"settings"),"nested"),"replace"))==2);assert(json_object_get_int(get(get(get(settings,"settings"),"nested"),"keep"))==1);
 assert(!pg_settings_resource_paths(settings,"skills",user,project,&paths)&&json_object_array_length(paths)==2);assert(!pg_skills_load_config(0,0,paths,&catalog)&&json_object_array_length(get(catalog,"skills"))==2);json_object_put(paths);paths=0;
 assert(!pg_settings_resource_paths(settings,"prompts",user,project,&paths)&&json_object_array_length(paths)==2);assert(!pg_prompt_templates_load_config(0,0,paths,&templates)&&json_object_array_length(get(templates,"templates"))==2);json_object_put(paths);paths=0;
 char output[4096];size_t size=0;assert(!pg_resource_expand(templates,catalog,(pg_slice){"/custom 'quoted value'",22},output,sizeof(output),&size)&&!strcmp(output,"Custom quoted value\n"));
 json_object_put(settings);json_object_put(catalog);json_object_put(templates);settings=0;
 write_file(project,"settings.json","{invalid");assert(!pg_settings_load(global_file,project_file,&settings));assert(json_object_array_length(get(settings,"diagnostics"))==1);assert(json_object_get_int(get(get(get(settings,"settings"),"nested"),"replace"))==1);json_object_put(settings);
 write_file(project,"settings.json","---\n---\n{\"skills\":[]}\n");assert(!pg_settings_load(global_file,project_file,&settings)&&json_object_array_length(get(settings,"diagnostics"))==1);json_object_put(settings);
 assert(!pg_settings_load("/nonexistent-pangu-settings",0,&settings)&&!json_object_array_length(get(settings,"diagnostics")));json_object_put(settings);
 assert(!nftw(temporary,remove_entry,24,FTW_DEPTH|FTW_PHYS));puts("PASS: settings scope merge, detached arrays/null, BOM/error recovery and actual configured skill/template file/directory consumers");
}
