#define _XOPEN_SOURCE 700
#include "../../src/runtime/skills.h"
#include <assert.h>
#include <ftw.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static void directory(const char *root,const char *name){char path[4096];assert(snprintf(path,sizeof(path),"%s/%s",root,name)>0);assert(!mkdir(path,0700));}
static void put(const char *root,const char *name,const char *text){char path[4096];assert(snprintf(path,sizeof(path),"%s/%s",root,name)>0);FILE *f=fopen(path,"wb");assert(f);assert(fwrite(text,1,strlen(text),f)==strlen(text));assert(!fclose(f));}
static json_object *field(json_object *value,const char *key){return json_object_object_get(value,key);}
static const char *string(json_object *value,const char *key){return json_object_get_string(field(value,key));}
static int remove_entry(const char *path,const struct stat *metadata,int kind,struct FTW *walk){(void)metadata;(void)kind;(void)walk;return remove(path);}
int main(void){
 char temporary[]="/tmp/pangu-skills-XXXXXX";assert(mkdtemp(temporary));directory(temporary,"user");directory(temporary,"project");char user[4096],project[4096];snprintf(user,sizeof(user),"%s/user",temporary);snprintf(project,sizeof(project),"%s/project",temporary);
 directory(user,"alpha");directory(user,"alpha/nested");put(user,"alpha/SKILL.md","\xef\xbb\xbf---\r\nname: alpha\r\ndescription: |\r\n  First & <special> skill.\r\n  Read this on demand.\r\n---\r\nSECRET_BODY\n");put(user,"alpha/nested/SKILL.md","---\nname: wrong-nested\ndescription: hidden by parent\n---\n");
 put(user,"direct.md","---\nname: direct\ndescription: 'quoted " "\"" "value" "\"" "'\n---\n");
 directory(user,".hidden");put(user,".hidden/SKILL.md","---\nname: hidden\ndescription: hidden\n---\n");directory(user,"node_modules");put(user,"node_modules/SKILL.md","---\nname: node\ndescription: hidden\n---\n");
 directory(project,"copy");put(project,"copy/SKILL.md","---\nname: alpha\ndescription: project loses name collision\n---\n");
 directory(project,"disabled");put(project,"disabled/SKILL.md","---\nname: disabled\ndescription: model hidden\ndisable-model-invocation: true\n---\n");
 directory(project,"nested");directory(project,"nested/leaf");put(project,"nested/leaf/SKILL.md","---\ndescription: Parent name fallback\n---\n");put(project,"nested/notes.md","---\nname: wrong-file\ndescription: only root markdown\n---\n");
 directory(project,"invalid");put(project,"invalid/SKILL.md","---\nname: Bad--Name-\ndescription: still load with warnings\n---\n");
 directory(project,"missing");put(project,"missing/SKILL.md","---\nname: missing\n---\n");directory(project,"numeric");put(project,"numeric/SKILL.md","---\ndescription: 123\n---\n");
 directory(project,"empty");put(project,"empty/SKILL.md","---\n---\n");directory(project,"whitespace");put(project,"whitespace/SKILL.md","---\ndescription: '\xc2\xa0\xe3\x80\x80'\n---\n");
 directory(project,"malformed");put(project,"malformed/SKILL.md","---\nname: [unterminated\n---\n");directory(project,"duplicate");put(project,"duplicate/SKILL.md","---\ndescription: first\ndescription: second\n---\n");
 put(project,"notes.md","Ordinary markdown without frontmatter.");char link[4096],target[4096];snprintf(link,sizeof(link),"%s/project/alias.md",temporary);snprintf(target,sizeof(target),"%s/user/alpha/SKILL.md",temporary);assert(!symlink(target,link));snprintf(link,sizeof(link),"%s/project/nested/cycle",temporary);assert(!symlink(project,link));
 json_object *catalog=0;assert(!pg_skills_load(user,project,&catalog)&&catalog);json_object *skills=field(catalog,"skills"),*diagnostics=field(catalog,"diagnostics");assert(json_object_array_length(skills)==5);assert(json_object_array_length(diagnostics)>=8);
 json_object *alpha=json_object_array_get_idx(skills,0);assert(!strcmp(string(alpha,"name"),"alpha"));assert(strstr(string(alpha,"description"),"\n"));assert(!strcmp(string(alpha,"filePath"),target));
 int direct=0,disabled=0,leaf=0,bad=0;for(size_t i=0;i<json_object_array_length(skills);i++){json_object *skill=json_object_array_get_idx(skills,i);const char *name=string(skill,"name");direct+=!strcmp(name,"direct");disabled+=!strcmp(name,"disabled");leaf+=!strcmp(name,"leaf");bad+=!strcmp(name,"Bad--Name-");}assert(direct==1&&disabled==1&&leaf==1&&bad==1);
 char output[8192];size_t size=99;assert(!pg_skills_prompt(catalog,0,output,sizeof(output),&size)&&size==strlen(output));assert(strstr(output,"Use the read tool")&&strstr(output,"First &amp; &lt;special&gt; skill."));assert(strstr(output,"&quot;value&quot;")&&!strstr(output,"model hidden")&&!strstr(output,"SECRET_BODY")&&!strstr(output,"wrong-nested")&&!strstr(output,"wrong-file"));
 assert(!pg_skills_prompt(catalog,1,output,sizeof(output),&size)&&strstr(output,"Use bash to load"));assert(!pg_skills_prompt(catalog,2,output,sizeof(output),&size)&&strstr(output,"Load a skill's file")&&!strstr(output,"Use the read"));assert(!pg_skills_prompt(catalog,3,output,sizeof(output),&size)&&!size&&!output[0]);
 assert(pg_skills_prompt(catalog,0,output,32,&size)&&!size&&!output[0]);json_object_put(catalog);
 assert(!pg_skills_load(0,0,&catalog)&&!json_object_array_length(field(catalog,"skills")));assert(!pg_skills_prompt(catalog,0,output,sizeof(output),&size)&&!size);json_object_put(catalog);
 /* Use the upstream fixtures as a second source of discovery/validation cases. */
 const char *fixtures="references/pi/packages/coding-agent/test/fixtures/skills";assert(!pg_skills_load(fixtures,0,&catalog));assert(json_object_array_length(field(catalog,"skills"))>=10);json_object_put(catalog);
 assert(!nftw(temporary,remove_entry,24,FTW_DEPTH|FTW_PHYS));puts("PASS: skills discovery, metadata, collisions, symlinks, prompt and upstream fixtures");
}
