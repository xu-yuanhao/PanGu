#define _GNU_SOURCE
#include "../../src/runtime/edit_text.h"
#include <assert.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <utf8proc.h>
_Static_assert(PG_EDIT_LIMIT==16,"Pi edit batch limit");
_Static_assert(PG_EDIT_OK==0 && PG_EDIT_INVALID==1 && PG_EDIT_QUOTA==2 && PG_EDIT_NOT_FOUND==3 && PG_EDIT_AMBIGUOUS==4 && PG_EDIT_OVERLAP==5 && PG_EDIT_UNCHANGED==6 && PG_EDIT_NO_MEMORY==7,"status ABI");
_Static_assert(sizeof(pg_text_edit)==32 && offsetof(pg_text_edit,new_text)==16,"edit input ABI");
_Static_assert((UTF8PROC_STABLE|UTF8PROC_COMPAT|UTF8PROC_COMPOSE)==14,"NFKC flags");
_Static_assert(sizeof(utf8proc_ssize_t)==8 && sizeof(utf8proc_int32_t)==4,"Unicode ABI");
static int fail_calloc, fail_map;
static unsigned map_calls;
void *__real_calloc(size_t,size_t);
void *__wrap_calloc(size_t n,size_t size){return fail_calloc?0:__real_calloc(n,size);}
utf8proc_ssize_t __real_utf8proc_map(const utf8proc_uint8_t *,utf8proc_ssize_t,utf8proc_uint8_t **,unsigned int);
utf8proc_ssize_t __wrap_utf8proc_map(const utf8proc_uint8_t *p,utf8proc_ssize_t n,utf8proc_uint8_t **out,unsigned int flags){
 ++map_calls;if(fail_map&&(int)map_calls==fail_map){*out=0;return UTF8PROC_ERROR_NOMEM;}return __real_utf8proc_map(p,n,out,flags);
}
static pg_slice s(const char *p){return (pg_slice){p,strlen(p)};}
static void check(const char *input,const pg_text_edit *edits,size_t count,pg_edit_status expected,const char *result){
 char out[PG_TEXT_LIMIT];memset(out,0x5a,sizeof(out));size_t n=999;
 pg_edit_status rc=pg_edit_text_apply(s(input),edits,count,out,sizeof(out),&n);
 if(rc!=expected){fprintf(stderr,"status %d expected %d for %s\n",rc,expected,input);abort();}
 if(!rc)assert(n==strlen(result)&&!memcmp(out,result,n+1));
 else{assert(!n);for(size_t i=0;i<sizeof(out);++i)assert(out[i]==0x5a);}
}
static void cases(void){
 pg_text_edit edits[]={{s("alpha"),s("ALPHA")},{s("gamma"),s("GAMMA")}};
 check("alpha\nbeta\ngamma\n",edits,2,PG_EDIT_OK,"ALPHA\nbeta\nGAMMA\n");
 pg_text_edit reverse[]={edits[1],edits[0]};check("alpha\nbeta\ngamma\n",reverse,2,PG_EDIT_OK,"ALPHA\nbeta\nGAMMA\n");
 pg_text_edit bom={{"two",3},{"TWO\nnew",7}};check("\xef\xbb\xbfone\r\ntwo\r\n",&bom,1,PG_EDIT_OK,"\xef\xbb\xbfone\r\nTWO\r\nnew\r\n");
 pg_text_edit fuzzy={s("say 'hello' - world"),s("changed")};
 check("keep  \nsay ‘hello’ — world  \nkeep  \n",&fuzzy,1,PG_EDIT_OK,"keep  \nchanged\nkeep  \n");
 pg_text_edit wide={s("ABC"),s("changed")};check("stay\nＡＢＣ\nend  ",&wide,1,PG_EDIT_OK,"stay\nchanged\nend  ");
 pg_text_edit combining={s("é"),s("E")};check("e\xcc\x81",&combining,1,PG_EDIT_OK,"E");
 pg_text_edit same={s("a"),s("a")};check("abc",&same,1,PG_EDIT_UNCHANGED,0);
 pg_text_edit missing={s("not found"),s("x")};check("abc",&missing,1,PG_EDIT_NOT_FOUND,0);
 pg_text_edit duplicate={s("foo"),s("x")};check("foo foo",&duplicate,1,PG_EDIT_AMBIGUOUS,0);
 pg_text_edit folded={s("A"),s("x")};check("A Ａ",&folded,1,PG_EDIT_AMBIGUOUS,0);
 pg_text_edit overlap[]={{s("abc"),s("x")},{s("bcd"),s("y")}};check("abcdef",overlap,2,PG_EDIT_OVERLAP,0);
 pg_text_edit chain[]={{s("a"),s("x")},{s("x"),s("y")}};check("a b",chain,2,PG_EDIT_NOT_FOUND,0);
 pg_text_edit empty={s(""),s("x")};check("a",&empty,1,PG_EDIT_INVALID,0);
 pg_text_edit invalid={{"\xff",1},s("x")};check("a",&invalid,1,PG_EDIT_INVALID,0);
 pg_text_edit nul={{"a\0b",3},s("x")};check("a",&nul,1,PG_EDIT_INVALID,0);
 char out[4096];memset(out,'Q',sizeof(out));size_t n=88;
 pg_text_edit replace={s("a"),s("result")};
 for(size_t cap=0;cap<8;++cap){memset(out,'Q',sizeof(out));n=88;int rc=pg_edit_text_apply(s("a"),&replace,1,out,cap,&n);assert((cap<7&&rc==PG_EDIT_QUOTA&&!n&&out[0]=='Q')||(cap==7&&!rc&&n==6&&!strcmp(out,"result")));}
 memcpy(out,"a",2);assert(!pg_edit_text_apply((pg_slice){out,1},&replace,1,out,sizeof(out),&n)&&n==6&&!strcmp(out,"result"));
 pg_text_edit many[PG_EDIT_LIMIT+1];memset(many,0,sizeof(many));assert(pg_edit_text_apply(s("a"),many,PG_EDIT_LIMIT+1,out,sizeof(out),&n)==PG_EDIT_INVALID&&!n);
 char maximum[4096];memset(maximum,'a',4095);maximum[4095]=0;pg_text_edit del={s("a"),s("")};assert(pg_edit_text_apply(s(maximum),&del,1,out,sizeof(out),&n)==PG_EDIT_AMBIGUOUS);
 long page=sysconf(_SC_PAGESIZE);assert(page>=4096);char *map=mmap(0,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(map!=MAP_FAILED&&!mprotect(map+page,(size_t)page,PROT_NONE));
 char *tail=map+page-1;*tail='a';assert(!pg_edit_text_apply((pg_slice){tail,1},&replace,1,out,sizeof(out),&n)&&n==6);
 assert(!pg_edit_text_apply(s("a"),&replace,1,map+page-7,7,&n)&&n==6);
 *tail=(char)0xc2;assert(pg_edit_text_apply((pg_slice){tail,1},&replace,1,out,sizeof(out),&n)==PG_EDIT_INVALID&&!n);
 assert(!munmap(map,(size_t)page*2));
 fail_calloc=1;check("a",&replace,1,PG_EDIT_NO_MEMORY,0);fail_calloc=0;
 for(int failure=1;failure<=3;++failure){fail_map=failure;map_calls=0;check("Ａ",&folded,1,PG_EDIT_NO_MEMORY,0);}fail_map=0;
 char expansion[4096];size_t used=0;for(unsigned i=0;i<200;++i){memcpy(expansion+used,"ﷺ",3);used+=3;}expansion[used]=0;
 check(expansion,&replace,1,PG_EDIT_NOT_FOUND,0);
 char large[4096];memset(large,'x',4095);large[4095]=0;pg_text_edit grow={s("a"),s(large)};assert(!pg_edit_text_apply(s("a"),&grow,1,out,sizeof(out),&n)&&n==4095&&!memcmp(out,large,sizeof(large)));
 size_t prefix=65536,input_n=prefix+7;char *large_input=malloc(input_n),*large_output=malloc(input_n+2);assert(large_input&&large_output);memset(large_input,'a',prefix);memcpy(large_input+prefix,"target\n",7);pg_text_edit large_edit={s("target"),s("changed")};n=0;assert(!pg_edit_text_apply((pg_slice){large_input,input_n},&large_edit,1,large_output,input_n+2,&n));assert(n==input_n+1&&!memcmp(large_output,large_input,prefix)&&!memcmp(large_output+prefix,"changed\n",8)&&large_output[n]==0);free(large_output);free(large_input);
 puts("edit text: batch/original matching, ambiguity/overlap/no-op refusal, fuzzy NFKC/unchanged lines, BOM/CRLF, alias/quota/page tails OK");
}
static pg_slice value(json_object *o,const char *name){json_object *v=0;assert(json_object_object_get_ex(o,name,&v)&&json_object_is_type(v,json_type_string));return (pg_slice){json_object_get_string(v),(size_t)json_object_get_string_len(v)};}
static int json_cases(void){
 json_object *input=json_object_from_fd(STDIN_FILENO);assert(input&&json_object_is_type(input,json_type_array));json_object *answers=json_object_new_array();assert(answers);
 for(size_t i=0;i<json_object_array_length(input);++i){json_object *item=json_object_array_get_idx(input,i),*array=0;assert(json_object_object_get_ex(item,"edits",&array)&&json_object_is_type(array,json_type_array));size_t count=json_object_array_length(array);pg_text_edit edits[PG_EDIT_LIMIT];memset(edits,0,sizeof(edits));for(size_t j=0;j<count&&j<PG_EDIT_LIMIT;++j){json_object *e=json_object_array_get_idx(array,j);edits[j]=(pg_text_edit){value(e,"oldText"),value(e,"newText")};}
  char out[4096];size_t n=999;pg_edit_status rc=pg_edit_text_apply(value(item,"content"),edits,count,out,sizeof(out),&n);json_object *answer=json_object_new_object();assert(answer);json_object_object_add(answer,"status",json_object_new_int(rc));if(!rc)json_object_object_add(answer,"output",json_object_new_string_len(out,(int)n));assert(!json_object_array_add(answers,answer));
 }
 puts(json_object_to_json_string_ext(answers,JSON_C_TO_STRING_PLAIN));json_object_put(answers);json_object_put(input);return 0;
}
int main(int argc,char **argv){if(argc==2&&!strcmp(argv[1],"--json"))return json_cases();cases();return 0;}
