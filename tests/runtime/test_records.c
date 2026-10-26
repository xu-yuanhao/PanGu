#define _GNU_SOURCE
#include "../../src/runtime/records.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <json-c/json.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
_Static_assert(sizeof(pg_record_session)==88,"record Session");
_Static_assert(offsetof(pg_record_storage,commit)==8,"storage callback");
_Static_assert(offsetof(pg_record_session,sequence)==16&&offsetof(pg_record_session,context)==24,"sequence/context");
_Static_assert(offsetof(pg_record_session,adopt)==32&&offsetof(pg_record_session,publish)==40,"consumer hooks");
_Static_assert(offsetof(pg_record_session,failed)==52&&offsetof(pg_record_session,read_only)==56,"latch/readonly");
_Static_assert(offsetof(pg_record_session,adopt_count)==60&&offsetof(pg_record_session,adopt_error)==64,"adoption state");
_Static_assert(offsetof(pg_record_session,busy)==48,"busy");
_Static_assert(offsetof(pg_record_session,pending_sequence)==72,"pending sequence");
_Static_assert(offsetof(pg_record_session,pending_batch)==80,"pending batch");
typedef struct {pg_record_session session;struct json_object *input,*stored;unsigned calls,adoptions,publications,mode;} fixture;
static int adopt(void *ctx,const struct json_object *batch,uint64_t seq){fixture *f=ctx;assert(f->calls==1&&!f->publications);++f->adoptions;assert(seq==1&&f->stored&&batch!=f->input);assert(pg_records_commit(&f->session,f->input)==PG_RECORDS_INVALID);return f->mode==6;}
static void publish(void *ctx,const struct json_object *batch,uint64_t seq){fixture *f=ctx;++f->publications;assert(f->adoptions==1&&f->session.sequence==seq&&batch!=f->input);assert(pg_records_commit(&f->session,f->input)==PG_RECORDS_INVALID);}
static int commit(void *ctx,const struct json_object *batch,pg_records_adopt accepted,void *owner,uint64_t *seq){fixture *f=ctx;++f->calls;assert(!f->adoptions&&!f->publications&&f->session.busy);assert(pg_records_commit(&f->session,f->input)==PG_RECORDS_INVALID);
 if(f->mode==1)return 1;
 assert(!json_object_deep_copy((struct json_object*)batch,&f->stored,0));
 if(f->mode!=2)accepted(owner,f->mode==5?0:1);
 if(f->mode==3)accepted(owner,1);
 if(f->mode==7)assert(f->session.sequence==1);
 *seq=f->mode==4?2:1;
 return f->mode==7?1:0;
}
#ifdef PG_RECORD_FAULTS
static unsigned copy_failure,copy_skip;
int __real_json_object_deep_copy(struct json_object *,struct json_object **,json_c_shallow_copy_fn *);
int __wrap_json_object_deep_copy(struct json_object *src,struct json_object **dst,json_c_shallow_copy_fn *fn){
 if(copy_failure&&copy_skip){--copy_skip;return __real_json_object_deep_copy(src,dst,fn);}
 if(copy_failure){unsigned mode=copy_failure;copy_failure=0;if(mode==2)*dst=json_object_new_object();return -1;}
 return __real_json_object_deep_copy(src,dst,fn);
}
#endif
static int simple_commit(void *ctx,const struct json_object *writes,pg_records_adopt adopt,void *owner,uint64_t *seq){(void)ctx;(void)writes;adopt(owner,1);*seq=1;return 0;}
static void submission_changes(void){
 const char *states[]={"queued","placed","done","unanswered"};const char *types[]={"input","write"};
 for(unsigned type=0;type<2;++type)for(unsigned state=0;state<4;++state)for(unsigned change=1;change<4;++change){
 struct json_object *current=json_object_new_object(),*next=json_object_new_object(),*result=0;assert(current&&next);json_object_object_add(current,"type",json_object_new_string(types[type]));json_object_object_add(current,"status",json_object_new_string(states[state]));json_object_object_add(next,"status",json_object_new_string(states[change]));json_object_object_add(next,"entry",json_object_new_int(4));json_object_object_add(next,"answer",json_object_new_string("answer"));
 int allowed=state>=2||(change==1?state==0:change==2?state==1:1);pg_records_status status=pg_records_submission_change(current,next,&result);assert((status==PG_RECORDS_OK)==allowed);
 if(allowed){const char *expected=state>=2?states[state]:change==1?(type?"done":"placed"):states[change];assert(result!=current&&!strcmp(json_object_get_string(json_object_object_get(result,"status")),expected));if(state>=2)assert(json_object_equal(result,current));else if(change==1)assert(!json_object_object_get(result,"answer"));}else assert(!result);
 json_object_put(result);result=0;
#ifdef PG_RECORD_FAULTS
 if(allowed)for(unsigned skip=0;skip<((state<2)?2u:1u);++skip)for(unsigned mode=1;mode<=2;++mode){copy_failure=mode;copy_skip=skip;assert(pg_records_submission_change(current,next,&result)==PG_RECORDS_ALLOCATION&&!result&&!copy_failure);}
#endif
 json_object_put(current);json_object_put(next);
 }
}
int main(void){
 submission_changes();
 for(unsigned mode=0;mode<=7;++mode){fixture f={.mode=mode};f.input=json_tokener_parse("[{\"type\":\"entry\",\"value\":{\"id\":2}},{\"type\":\"document.retire\",\"id\":3}]");assert(f.input);f.session=(pg_record_session){.storage={&f,commit},.context=&f,.adopt=adopt,.publish=publish};
 pg_records_status result=pg_records_commit(&f.session,f.input);assert(!f.session.busy&&!f.session.pending_batch);
 if(!mode)assert(result==PG_RECORDS_OK&&f.session.sequence==1&&f.publications==1);
 else {assert(result==(mode==1||mode==7?PG_RECORDS_STORAGE_FAILED:PG_RECORDS_ADOPTION_FAILED));assert(f.session.failed&&(f.publications==0));assert(pg_records_commit(&f.session,f.input)==result&&f.calls==1);}
 if(mode==2||mode==5||mode==6||mode==1)assert(!f.session.sequence);else assert(f.session.sequence==1);
 json_object_put(f.input);json_object_put(f.stored);
 }
 fixture f={0};f.session.storage=(pg_record_storage){&f,commit};struct json_object *empty=json_object_new_array();f.session.read_only=1;assert(!pg_records_commit(&f.session,empty)&&!f.calls&&!f.session.failed);json_object_put(empty);
 f.input=json_tokener_parse("[{\"type\":\"entry\"}]");assert(pg_records_commit(&f.session,f.input)==PG_RECORDS_READ_ONLY&&!f.session.failed&&!f.calls);assert(pg_records_commit(&f.session,0)==PG_RECORDS_INVALID&&!f.session.failed);json_object_put(f.input);
#ifdef PG_RECORD_FAULTS
 for(unsigned mode=1;mode<=2;++mode){fixture fault_fixture={0};fault_fixture.input=json_tokener_parse("[{\"type\":\"entry\",\"value\":{\"id\":2}}]");fault_fixture.session.storage=(pg_record_storage){&fault_fixture,commit};copy_failure=mode;assert(pg_records_commit(&fault_fixture.session,fault_fixture.input)==PG_RECORDS_ALLOCATION&&!fault_fixture.session.failed&&!fault_fixture.session.busy&&!fault_fixture.calls);json_object_put(fault_fixture.input);}
#endif
 long page=sysconf(_SC_PAGESIZE);char *memory=mmap(0,(size_t)page*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);assert(memory!=MAP_FAILED&&!mprotect(memory+page,(size_t)page,PROT_NONE));pg_record_session *tail=(pg_record_session*)(memory+page-sizeof(pg_record_session));memset(tail,0,sizeof(*tail));tail->storage.commit=simple_commit;struct json_object *batch=json_tokener_parse("[{\"type\":\"entry\",\"value\":{\"id\":2}}]");assert(batch&&!pg_records_commit(tail,batch)&&tail->sequence==1);json_object_put(batch);assert(!munmap(memory,(size_t)page*2));
 puts("PASS: atomic detached batch boundary, adopt-before-resolve/publication, empty/read-only, failure latch, duplicate/missing/mismatched adoption and reentry");return 0;
}
