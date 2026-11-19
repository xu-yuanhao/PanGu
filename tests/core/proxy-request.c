#include <pangu/core/proxy_request.h>
#include "yyjson.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do { if (!(x)) abort(); } while (0)
#define S(x) (pangu_slice){x,sizeof(x)-1}
typedef struct { size_t calls,fail,live; } memory;
static void *allocate(void *ctx,size_t n) { memory *m=ctx; if (++m->calls==m->fail) return NULL; void *p=malloc(n); if (p) ++m->live; return p; }
static void release(void *ctx,void *p) { if (p) { --((memory *)ctx)->live; free(p); } }
static void check(pangu_core *encoded) {
    pangu_slice s=pangu_core_message_at(encoded,0)->content[0].data;
    yyjson_doc *doc=yyjson_read(s.data,s.len,0); CHECK(doc);
    yyjson_val *root=yyjson_doc_get_root(doc),*model=yyjson_obj_get(root,"model");
    CHECK(!strcmp(yyjson_get_str(yyjson_obj_get(model,"id")),"new"));
    CHECK(yyjson_get_uint(yyjson_obj_get(model,"contextWindow"))==4096);
    yyjson_val *messages=yyjson_obj_get(yyjson_obj_get(root,"context"),"messages");
    CHECK(yyjson_arr_size(messages)==4);
    yyjson_val *system=yyjson_arr_get(messages,0),*assistant=yyjson_arr_get(messages,2),*result=yyjson_arr_get(messages,3);
    CHECK(yyjson_is_null(yyjson_obj_get(yyjson_obj_get(system,"sections"),"remove")));
    CHECK(yyjson_is_obj(yyjson_obj_get(yyjson_arr_get(yyjson_obj_get(system,"toolsAdded"),0),"parameters")));
    yyjson_val *blocks=yyjson_obj_get(assistant,"content"),*call=yyjson_arr_get(blocks,2);
    CHECK(yyjson_is_false(yyjson_obj_get(yyjson_arr_get(blocks,1),"redacted")));
    yyjson_val *u=yyjson_obj_get(assistant,"usage");
    CHECK(yyjson_is_uint(yyjson_obj_get(u,"cacheWrite1h")) && yyjson_get_uint(yyjson_obj_get(u,"cacheWrite1h"))==0);
    CHECK(yyjson_is_uint(yyjson_obj_get(u,"reasoning")) && yyjson_get_uint(yyjson_obj_get(u,"reasoning"))==0);
    CHECK(yyjson_is_false(yyjson_obj_get(yyjson_obj_get(u,"cost"),"totalFromProvider")));
    CHECK(yyjson_is_null(yyjson_obj_get(yyjson_obj_get(u,"cost"),"input")));
    CHECK(yyjson_is_null(yyjson_obj_get(yyjson_obj_get(u,"cost"),"output")));
    CHECK(yyjson_is_null(yyjson_obj_get(yyjson_obj_get(u,"cost"),"total")));
    CHECK(yyjson_is_arr(yyjson_obj_get(assistant,"diagnostics")));
    CHECK(yyjson_is_obj(yyjson_obj_get(assistant,"deferred")));
    CHECK(yyjson_is_false(yyjson_obj_get(assistant,"endTurn")));
    CHECK(yyjson_is_obj(yyjson_obj_get(result,"nestedCalls")));
    CHECK(!strcmp(yyjson_get_str(yyjson_obj_get(call,"namespace")),"dynamic_tools"));
    CHECK(yyjson_get_uint(yyjson_obj_get(yyjson_obj_get(call,"arguments"),"value"))==1);
    CHECK(yyjson_get_bool(yyjson_obj_get(result,"isError")));
    CHECK(!yyjson_obj_get(result,"structuredContent"));
    yyjson_val *options=yyjson_obj_get(root,"options");
    CHECK(yyjson_obj_size(options)==2 && yyjson_get_real(yyjson_obj_get(options,"temperature"))==0.5);
    CHECK(!yyjson_obj_get(options,"authToken") && !yyjson_obj_get(options,"signal"));
    yyjson_doc_free(doc);
}
typedef struct { int calls,events,reject,trace; const char *session; } options_fixture;
static pangu_status options_observer(void *ctx,pangu_core *owner,const pangu_model_config *model,pangu_slice json) {
    options_fixture *f=ctx; CHECK(pangu_core_is_running(owner) && model->id.len==4 && !memcmp(model->id.data,"mock",4));
    yyjson_doc *doc=yyjson_read(json.data,json.len,0); CHECK(doc);
    CHECK(yyjson_get_real(yyjson_obj_get(yyjson_doc_get_root(doc),"request_cost"))==0.01);
    yyjson_doc_free(doc); ++f->events;
    if (f->trace) puts("event:0.01:mock");
    return f->reject?PANGU_MODEL_ERROR:PANGU_OK;
}
static pangu_status options_provider(void *ctx,pangu_core *owner,const pangu_core *request,pangu_model_sink sink,void *target) {
    options_fixture *f=ctx; ++f->calls;
    pangu_core *encoded=NULL;
    pangu_status status=pangu_proxy_active_request_json(owner,request,&encoded);
    if (status!=PANGU_OK) return status;
    pangu_slice body=pangu_core_message_at(encoded,0)->content[0].data;
    yyjson_doc *doc=yyjson_read(body.data,body.len,0); CHECK(doc);
    yyjson_val *root=yyjson_doc_get_root(doc),*options=yyjson_obj_get(root,"options");
    CHECK(!strcmp(yyjson_get_str(yyjson_obj_get(options,"sessionId")),f->session));
    CHECK(yyjson_obj_size(options)==2 && yyjson_get_real(yyjson_obj_get(options,"temperature"))==0.5);
    yyjson_val *messages=yyjson_obj_get(yyjson_obj_get(root,"context"),"messages");
    CHECK(yyjson_arr_size(messages)>0);
    if (yyjson_arr_size(messages)>1) {
        yyjson_val *usage=yyjson_obj_get(yyjson_arr_get(messages,1),"usage");
        CHECK(!yyjson_obj_get(usage,"cacheWrite1h") && !yyjson_obj_get(usage,"reasoning"));
        CHECK(!yyjson_obj_get(yyjson_obj_get(usage,"cost"),"totalFromProvider"));
    }
    /* Direct providers retain extension fields; proxy serialization filters them. */
    pangu_slice raw=pangu_core_active_options(owner);
    yyjson_doc *raw_doc=yyjson_read(raw.data,raw.len,0); CHECK(raw_doc);
    CHECK(yyjson_get_bool(yyjson_obj_get(yyjson_doc_get_root(raw_doc),"customOption")));
    yyjson_doc_free(raw_doc);
    if (f->trace) printf("session:%s\n",f->session);
    yyjson_doc_free(doc); pangu_core_destroy(encoded);
    status=pangu_core_observe_provider_event(owner,S("{\"request_cost\":0.01}"));
    if (status!=PANGU_OK) return status;
    pangu_message m={.role=PANGU_ASSISTANT,.stop_reason=PANGU_STOP}; return sink(target,&m,1);
}
static void provider_options(int trace) {
    pangu_core *owner=NULL,*encoded=(pangu_core *)1; CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
    CHECK(pangu_proxy_active_request_json(owner,owner,&encoded)==PANGU_INVALID && !encoded);
    CHECK(!pangu_core_active_options(owner).len && pangu_core_observe_provider_event(owner,S("{}"))==PANGU_INVALID);
    options_fixture f={.trace=trace,.session="session-abc"};
    pangu_loop_config cfg={.ctx=&f,.request=options_provider,.on_provider_event=options_observer,
        .model_config={.id=S("mock")},.request_options_json=S("{\"sessionId\":\"session-abc\",\"temperature\":0.5,\"customOption\":true,\"authToken\":\"local-only\"}")};
    pangu_message prompt={.role=PANGU_USER};
    CHECK(pangu_core_run(owner,&prompt,1,&cfg)==PANGU_OK);
    f.session="session-def";
    cfg.request_options_json=S("{\"sessionId\":\"session-def\",\"temperature\":0.5,\"customOption\":true}");
    CHECK(pangu_core_run(owner,&prompt,1,&cfg)==PANGU_OK && f.calls==2 && f.events==2);
    if (!trace) {
        f.reject=1; CHECK(pangu_core_run(owner,&prompt,1,&cfg)==PANGU_MODEL_ERROR && f.events==3);
        f.reject=0; cfg.on_provider_event=NULL;
        CHECK(pangu_core_run(owner,&prompt,1,&cfg)==PANGU_OK && f.events==3);
        cfg.request_options_json=(pangu_slice){NULL,1};
        CHECK(pangu_core_run(owner,&prompt,1,&cfg)==PANGU_INVALID);
        cfg.request_options_json=S("not-json");
        CHECK(pangu_core_run(owner,&prompt,1,&cfg)==PANGU_MODEL_ERROR);
    }
    CHECK(!pangu_core_active_options(owner).len);
    pangu_core_destroy(owner);
}

/* Full wire fixture, deliberately uses presence-bearing zero/false/null values.
 * --trace-wire emits one request body per case; no generated timestamps. */
static const char *wire_options[]={
    "{}",
    "{\"temperature\":0,\"samplingParams\":{\"top_p\":0,\"nested\":[null,false,\"世界\"]},\"maxTokens\":0,\"reasoning\":\"off\",\"cacheRetention\":\"none\",\"sessionId\":\"s\\u0000世界\",\"headers\":{\"x-test\":\"世界\"},\"metadata\":{\"zero\":0,\"false\":false,\"empty\":\"\"},\"transport\":\"sse\",\"thinkingBudgets\":{\"minimal\":0,\"high\":4096},\"maxRetryDelayMs\":0,\"apiKey\":\"fixture\",\"authToken\":\"fixture\",\"proxyUrl\":\"https://invalid.example\",\"signal\":{},\"onPayload\":true,\"onResponse\":true,\"customOption\":true}",
    "{\"temperature\":null,\"samplingParams\":null,\"maxTokens\":null,\"reasoning\":null,\"cacheRetention\":null,\"sessionId\":null,\"headers\":null,\"metadata\":null,\"transport\":null,\"thinkingBudgets\":null,\"maxRetryDelayMs\":null}"
};
static void wire_matrix(int trace) {
    const char *keys[]={"temperature","samplingParams","maxTokens","reasoning","cacheRetention","sessionId","headers","metadata","transport","thinkingBudgets","maxRetryDelayMs"};
    for(size_t mode=0;mode<sizeof(wire_options)/sizeof(wire_options[0]);++mode) {
        pangu_core *owner=NULL,*encoded=NULL;
        CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        char text[]="a\0世界\n\"\\";
        pangu_content blocks[]={
            {.kind=PANGU_TEXT,.data={text,sizeof(text)-1},.signature=S("text-sig")},
            {.kind=PANGU_IMAGE,.data=S("AAAA"),.mime_type=S("image/png")}};
        pangu_system_section sections[]={{.name=S("empty"),.value=S("")},{.name=S("removed"),.removed=1}};
        pangu_tool_declaration tools[]={
            {.name=S("lookup"),.description=S("世界"),.schema_json=S("{\"type\":\"object\",\"properties\":{\"items\":{\"type\":\"array\",\"items\":{\"anyOf\":[{\"type\":\"null\"},{\"type\":\"string\",\"enum\":[\"\",\"世界\"]}]}}},\"required\":[\"items\"],\"additionalProperties\":false}"),.has_constrained_sampling=1},
            {.name=S("other"),.description=S(""),.schema_json=S("{\"type\":\"object\"}"),.has_constrained_sampling=1,.constrained_sampling=1}};
        pangu_slice removed[]={S("old"),S("世界")};
        pangu_message m={.role=PANGU_SYSTEM,.content=blocks,.content_count=1,.sections=sections,.section_count=2,
            .tools_added=tools,.tools_added_count=2,.tools_removed=removed,.tools_removed_count=2,.timestamp_ms=17};
        CHECK(pangu_core_append(owner,&m)==PANGU_OK);
        m=(pangu_message){.role=PANGU_USER,.content=blocks,.content_count=2,.timestamp_ms=18};
        CHECK(pangu_core_append(owner,&m)==PANGU_OK);
        pangu_content assistant[]={blocks[0],{.kind=PANGU_THINKING,.data=S("reason"),.signature=S("think-sig"),.has_redacted=1,.redacted=1},
            {.kind=PANGU_TOOL_CALL,.id=S("one"),.name=S("lookup"),.data=S("{\"items\":[null,\"\",\"世界\"],\"zero\":0,\"flag\":false}"),
                .signature=S("thought-sig"),.tool_namespace=S("dynamic_tools"),.arguments_error=S("fixture-error")}};
        m=(pangu_message){.role=PANGU_ASSISTANT,.content=assistant,.content_count=3,.timestamp_ms=19,
            .model=S("mock"),.api=S("fixture"),.provider=S("mock"),.stop_reason=PANGU_LENGTH,.thinking_level=PANGU_THINKING_MAX,
            .has_duration=1,.duration_ms=0,.has_end_turn=1,.end_turn=1,.error=S("fixture-error"),
            .usage={.input=1,.output=2,.cache_read=3,.cache_write=4,.total=10,.input_cost=.1,.output_cost=.2,.cache_read_cost=.3,.cache_write_cost=.4,.total_cost=1,
                .has_cache_write_1h=1,.cache_write_1h=4,.has_reasoning=1,.reasoning=2,.has_total_from_provider=1,.total_from_provider=1}};
        CHECK(pangu_core_append(owner,&m)==PANGU_OK);
        m=(pangu_message){.role=PANGU_TOOL_RESULT,.content=blocks,.content_count=2,.tool_call_id=S("one"),.tool_name=S("lookup"),
            .presence=PANGU_MESSAGE_HAS_USAGE,.timestamp_ms=20,.has_duration=1,.duration_ms=7,.details_json=S("[null,false,{\"value\":0}]"),.structured_json=S("{\"local\":true}")};
        CHECK(pangu_core_append(owner,&m)==PANGU_OK);
        /* append must have copied stack data before caller mutation. */
        memset(text,'!',sizeof(text));
        pangu_model_config model={.id=S("mock"),.provider=S("mock"),.api=S("fixture"),
            .metadata_json=S("{\"id\":\"old\",\"provider\":\"old\",\"api\":\"old\",\"name\":\"世界\",\"baseUrl\":\"https://invalid.example/v1\",\"reasoning\":true,\"input\":[\"text\",\"image\"],\"cost\":{\"input\":0},\"contextWindow\":4096,\"maxTokens\":512,\"compat\":{\"nested\":[false,null]}}")};
        pangu_slice options={wire_options[mode],strlen(wire_options[mode])};
        CHECK(pangu_proxy_request_json(owner,owner,&model,options,&encoded)==PANGU_OK);
        CHECK(pangu_core_message_count(owner)==4);
        /* Default allocator has no owner context: output survives source destroy. */
        pangu_core_destroy(owner);
        pangu_slice body=pangu_core_message_at(encoded,0)->content[0].data;
        yyjson_doc *doc=yyjson_read(body.data,body.len,0),*input=yyjson_read(options.data,options.len,0); CHECK(doc && input);
        yyjson_val *root=yyjson_doc_get_root(doc),*out=yyjson_obj_get(root,"options"),*in=yyjson_doc_get_root(input);
        CHECK(yyjson_obj_size(root)==3 && yyjson_obj_size(out)==(mode?11u:0u));
        yyjson_doc *model_doc=yyjson_read(model.metadata_json.data,model.metadata_json.len,0); CHECK(model_doc);
        yyjson_val *encoded_model=yyjson_obj_get(root,"model"),*source_model=yyjson_doc_get_root(model_doc);
        const char *model_keys[]={"name","baseUrl","reasoning","input","cost","contextWindow","maxTokens","compat"};
        for(size_t i=0;i<sizeof(model_keys)/sizeof(model_keys[0]);++i)
            CHECK(yyjson_equals(yyjson_obj_get(encoded_model,model_keys[i]),yyjson_obj_get(source_model,model_keys[i])));
        CHECK(!strcmp(yyjson_get_str(yyjson_obj_get(encoded_model,"id")),"mock"));
        CHECK(!strcmp(yyjson_get_str(yyjson_obj_get(encoded_model,"api")),"fixture"));
        CHECK(!strcmp(yyjson_get_str(yyjson_obj_get(encoded_model,"provider")),"mock"));
        yyjson_doc_free(model_doc);
        for(size_t i=0;i<11;++i) {
            yyjson_val *a=yyjson_obj_get(out,keys[i]),*b=yyjson_obj_get(in,keys[i]);
            CHECK((!a && !b) || (a && b && yyjson_equals(a,b)));
        }
        yyjson_val *messages=yyjson_obj_get(yyjson_obj_get(root,"context"),"messages"); CHECK(yyjson_arr_size(messages)==4);
        yyjson_val *sys=yyjson_arr_get(messages,0),*user=yyjson_arr_get(messages,1),*as=yyjson_arr_get(messages,2),*result=yyjson_arr_get(messages,3);
        yyjson_val *decl=yyjson_obj_get(sys,"toolsAdded");
        CHECK(yyjson_is_false(yyjson_obj_get(yyjson_arr_get(decl,0),"constrainedSampling")));
        CHECK(yyjson_is_true(yyjson_obj_get(yyjson_arr_get(decl,1),"constrainedSampling")));
        CHECK(yyjson_arr_size(yyjson_obj_get(sys,"toolsRemoved"))==2);
        for(size_t i=0;i<2;++i) {
            yyjson_doc *schema=yyjson_read(tools[i].schema_json.data,tools[i].schema_json.len,0); CHECK(schema);
            CHECK(yyjson_equals(yyjson_obj_get(yyjson_arr_get(decl,i),"parameters"),yyjson_doc_get_root(schema)));
            yyjson_doc_free(schema);
        }
        CHECK(yyjson_is_null(yyjson_obj_get(yyjson_obj_get(sys,"sections"),"removed")));
        CHECK(yyjson_get_len(yyjson_obj_get(yyjson_obj_get(sys,"sections"),"empty"))==0);
        CHECK(yyjson_arr_size(yyjson_obj_get(result,"content"))==2);
        CHECK(!strcmp(yyjson_get_str(yyjson_obj_get(yyjson_arr_get(yyjson_obj_get(result,"content"),1),"mimeType")),"image/png"));
        CHECK(yyjson_is_true(yyjson_obj_get(yyjson_arr_get(yyjson_obj_get(as,"content"),1),"redacted")));
        CHECK(yyjson_is_false(yyjson_obj_get(yyjson_obj_get(yyjson_arr_get(decl,0),"parameters"),"additionalProperties")));
        yyjson_val *str=yyjson_obj_get(yyjson_arr_get(yyjson_obj_get(user,"content"),0),"text");
        CHECK(yyjson_get_len(str)==sizeof(text)-1 && !memcmp(yyjson_get_str(str),"a\0世界\n\"\\",sizeof(text)-1));
        CHECK(yyjson_is_true(yyjson_obj_get(as,"endTurn")) && yyjson_get_uint(yyjson_obj_get(as,"durationMs"))==0);
        CHECK(!strcmp(yyjson_get_str(yyjson_obj_get(as,"thinkingLevel")),"max"));
        yyjson_val *call=yyjson_arr_get(yyjson_obj_get(as,"content"),2);
        CHECK(!strcmp(yyjson_get_str(yyjson_obj_get(call,"thoughtSignature")),"thought-sig"));
        CHECK(!strcmp(yyjson_get_str(yyjson_obj_get(call,"argumentsError")),"fixture-error"));
        CHECK(yyjson_is_true(yyjson_obj_get(yyjson_obj_get(yyjson_obj_get(as,"usage"),"cost"),"totalFromProvider")));
        CHECK(yyjson_arr_size(yyjson_obj_get(result,"details"))==3 && !yyjson_obj_get(result,"structuredContent"));
        CHECK(yyjson_get_uint(yyjson_obj_get(result,"durationMs"))==7);
        if(trace) { CHECK(fwrite(body.data,1,body.len,stdout)==body.len); putchar('\n'); }
        yyjson_doc_free(input); yyjson_doc_free(doc); pangu_core_destroy(encoded);
    }
}

/* Only content optional strings distinguish absent NULL/0 from present pointer/0.
 * Keep this separate from --trace-wire so its three-request contract is stable. */
static void content_presence(int trace) {
    for(int present=0;present<2;++present) {
        pangu_core *owner=NULL,*encoded=NULL;
        CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        char empty='x'; /* Not NUL terminated: length, not byte value, defines empty. */
        pangu_slice optional=present?(pangu_slice){&empty,0}:(pangu_slice){NULL,0};
        pangu_content blocks[]={
            {.kind=PANGU_TEXT,.data=S("text"),.signature=optional},
            {.kind=PANGU_THINKING,.data=S("reason"),.signature=optional},
            {.kind=PANGU_TOOL_CALL,.id=S("one"),.name=S("lookup"),.data=S("{}"),
                .signature=optional,.tool_namespace=optional,.arguments_error=optional}};
        pangu_message message={.role=PANGU_ASSISTANT,.content=blocks,.content_count=3,
            .model=S("mock"),.provider=S("mock"),.api=S("fixture"),.timestamp_ms=23,
            .stop_reason=PANGU_TOOL_USE,.thinking_level=PANGU_THINKING_OFF,
            .presence=PANGU_MESSAGE_HAS_THINKING_LEVEL};
        CHECK(pangu_core_append(owner,&message)==PANGU_OK);
        empty='y';
        const pangu_message *copied=pangu_core_message_at(owner,0);
        const pangu_slice fields[]={copied->content[0].signature,copied->content[1].signature,
            copied->content[2].signature,copied->content[2].tool_namespace,copied->content[2].arguments_error};
        for(size_t i=0;i<sizeof(fields)/sizeof(fields[0]);++i) {
            CHECK(fields[i].len==0 && !!fields[i].data==present);
            if(present) CHECK(fields[i].data!=&empty);
        }
        pangu_model_config model={.id=S("mock"),.provider=S("mock"),.api=S("fixture")};
        CHECK(pangu_proxy_request_json(owner,owner,&model,S("{}"),&encoded)==PANGU_OK);
        pangu_core_destroy(owner);
        pangu_slice body=pangu_core_message_at(encoded,0)->content[0].data;
        yyjson_doc *doc=yyjson_read(body.data,body.len,0); CHECK(doc);
        yyjson_val *messages=yyjson_obj_get(yyjson_obj_get(yyjson_doc_get_root(doc),"context"),"messages");
        CHECK(yyjson_arr_size(messages)==1);
        yyjson_val *content=yyjson_obj_get(yyjson_arr_get(messages,0),"content");
        CHECK(yyjson_arr_size(content)==3);
        const char *keys[]={"textSignature","thinkingSignature","thoughtSignature","namespace","argumentsError"};
        const size_t indices[]={0,1,2,2,2};
        for(size_t i=0;i<sizeof(keys)/sizeof(keys[0]);++i) {
            yyjson_val *v=yyjson_obj_get(yyjson_arr_get(content,indices[i]),keys[i]);
            if(present) CHECK(yyjson_is_str(v) && yyjson_get_len(v)==0);
            else CHECK(!v);
        }
        if(trace) { CHECK(fwrite(body.data,1,body.len,stdout)==body.len); putchar('\n'); }
        yyjson_doc_free(doc); pangu_core_destroy(encoded);
    }
}
/* Three complete requests: absent, explicit zero/empty, legacy nonzero.
 * Every row exercises append ownership and output lifetime after source destroy. */
static void message_presence(int trace) {
    for (int mode=0;mode<3;++mode) {
        pangu_core *owner=NULL,*encoded=NULL;
        CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        char empty='x',text[]="a\0世界";
        pangu_slice opt=mode==0?(pangu_slice){0}:mode==1?(pangu_slice){&empty,0}:(pangu_slice){text,sizeof(text)-1};
        pangu_content block={.kind=PANGU_TEXT,.data={text,sizeof(text)-1}};
        pangu_message messages[]={
            {.role=PANGU_SYSTEM,.timestamp_ms=31},
            {.role=PANGU_USER,.timestamp_ms=32},
            {.role=PANGU_ASSISTANT,.timestamp_ms=33,.model=S("mock"),.provider=S("mock"),.api=S("fixture"),
             .stop_reason=PANGU_STOP,.response_model=opt,.response_id=opt,.raw_stop_reason=opt,
             .provider_thinking_level=opt,.error=opt},
            {.role=PANGU_TOOL_RESULT,.timestamp_ms=34,.tool_call_id=S("one"),.tool_name=S("lookup")}};
        if (mode==1) {
            messages[0].presence=PANGU_MESSAGE_HAS_SECTIONS|PANGU_MESSAGE_HAS_TOOLS_ADDED|PANGU_MESSAGE_HAS_TOOLS_REMOVED|PANGU_MESSAGE_CONTENT_STRING;
            messages[1].presence=PANGU_MESSAGE_CONTENT_STRING;
            messages[2].presence=PANGU_MESSAGE_HAS_THINKING_LEVEL;
            messages[3].presence=PANGU_MESSAGE_HAS_USAGE;
        }
        if (mode==2) {
            messages[0].presence=messages[1].presence=PANGU_MESSAGE_CONTENT_STRING;
            messages[0].content=messages[1].content=&block;
            messages[0].content_count=messages[1].content_count=1;
            messages[2].thinking_level=PANGU_THINKING_HIGH;
            messages[3].usage.input=1;
        }
        for(size_t i=0;i<4;++i) {
            CHECK(pangu_core_append(owner,&messages[i])==PANGU_OK);
            CHECK(pangu_core_message_at(owner,i)->presence==messages[i].presence);
        }
        const pangu_message *copied=pangu_core_message_at(owner,2);
        const pangu_slice fields[]={copied->response_model,copied->response_id,copied->raw_stop_reason,copied->provider_thinking_level,copied->error};
        for(size_t i=0;i<5;++i) {
            CHECK(!!fields[i].data==(mode!=0) && fields[i].len==opt.len);
            if(mode) CHECK(fields[i].data!=opt.data);
        }
        memset(text,'!',sizeof(text)); empty='y';
        pangu_model_config model={.id=S("mock"),.provider=S("mock"),.api=S("fixture")};
        CHECK(pangu_proxy_request_json(owner,owner,&model,S("{}"),&encoded)==PANGU_OK);
        pangu_core_destroy(owner);
        pangu_slice body=pangu_core_message_at(encoded,0)->content[0].data;
        yyjson_doc *doc=yyjson_read(body.data,body.len,0); CHECK(doc);
        yyjson_val *ms=yyjson_obj_get(yyjson_obj_get(yyjson_doc_get_root(doc),"context"),"messages");
        CHECK(yyjson_arr_size(ms)==4);
        yyjson_val *sys=yyjson_arr_get(ms,0),*as=yyjson_arr_get(ms,2),*result=yyjson_arr_get(ms,3);
        const char *containers[]={"sections","toolsAdded","toolsRemoved"};
        for(size_t i=0;i<3;++i) {
            yyjson_val *v=yyjson_obj_get(sys,containers[i]);
            if(mode==1) CHECK(i==0?(yyjson_is_obj(v)&&yyjson_obj_size(v)==0):(yyjson_is_arr(v)&&yyjson_arr_size(v)==0));
            else CHECK(!v);
        }
        for(size_t i=0;i<2;++i) {
            yyjson_val *v=yyjson_obj_get(yyjson_arr_get(ms,i),"content");
            if(mode==0) CHECK(yyjson_is_arr(v)&&yyjson_arr_size(v)==0);
            else CHECK(yyjson_is_str(v)&&yyjson_get_len(v)==(mode==1?0:sizeof(text)-1));
            if(mode==2) CHECK(!memcmp(yyjson_get_str(v),"a\0世界",sizeof(text)-1));
        }
        const char *keys[]={"responseModel","responseId","rawStopReason","providerThinkingLevel","errorMessage"};
        for(size_t i=0;i<5;++i) {
            yyjson_val *v=yyjson_obj_get(as,keys[i]);
            if(!mode) CHECK(!v);
            else CHECK(yyjson_is_str(v)&&yyjson_get_len(v)==opt.len);
            if(mode==2) CHECK(!memcmp(yyjson_get_str(v),"a\0世界",sizeof(text)-1));
        }
        yyjson_val *level=yyjson_obj_get(as,"thinkingLevel"),*u=yyjson_obj_get(result,"usage");
        if(!mode) CHECK(!level&&!u);
        else {
            CHECK(yyjson_is_str(level)&&!strcmp(yyjson_get_str(level),mode==1?"off":"high"));
            CHECK(yyjson_is_obj(u)&&yyjson_get_uint(yyjson_obj_get(u,"input"))==(mode==2?1u:0u));
        }
        CHECK(yyjson_is_obj(yyjson_obj_get(as,"usage")));
        if(trace) { CHECK(fwrite(body.data,1,body.len,stdout)==body.len); putchar('\n'); }
        yyjson_doc_free(doc); pangu_core_destroy(encoded);
    }
}

static void invalid_message_presence(void) {
    pangu_core *owner=NULL;
    CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
    /* Every unknown bit must fail atomically, not just the first unused bit. */
    for(unsigned bit=6;bit<32;++bit) {
        pangu_message m={.role=PANGU_USER,.presence=UINT32_C(1)<<bit};
        CHECK(pangu_core_append(owner,&m)==PANGU_INVALID);
        CHECK(pangu_core_message_count(owner)==0);
    }
    const uint32_t allowed[]={60u,32u,3u,2u};
    for(int role=PANGU_SYSTEM;role<=PANGU_TOOL_RESULT;++role)for(unsigned bit=0;bit<6;++bit) {
        uint32_t flag=UINT32_C(1)<<bit;
        if(allowed[role]&flag)continue;
        pangu_message m={.role=(pangu_role)role,.presence=flag};
        CHECK(pangu_core_append(owner,&m)==PANGU_INVALID);
        CHECK(pangu_core_message_count(owner)==0);
    }
    pangu_content blocks[]={{.kind=PANGU_TEXT,.data=S("one")},{.kind=PANGU_TEXT,.data=S("two")}};
    for(int role=PANGU_SYSTEM;role<=PANGU_TOOL_RESULT;++role) {
        pangu_message m={.role=(pangu_role)role,.presence=PANGU_MESSAGE_CONTENT_STRING};
        if(role==PANGU_SYSTEM||role==PANGU_USER) {
            m.content=blocks; m.content_count=2;
            CHECK(pangu_core_append(owner,&m)==PANGU_INVALID);
            m.content_count=1; blocks[0].kind=PANGU_IMAGE;
            CHECK(pangu_core_append(owner,&m)==PANGU_INVALID);
            blocks[0].kind=PANGU_TEXT;
            blocks[0].signature=S("");
            CHECK(pangu_core_append(owner,&m)==PANGU_INVALID);
            blocks[0].signature=(pangu_slice){0};
        } else CHECK(pangu_core_append(owner,&m)==PANGU_INVALID);
        CHECK(pangu_core_message_count(owner)==0);
    }
    pangu_core_destroy(owner);
    /* Optional usage flags alone also make a toolResult usage present. */
    for(int field=0;field<3;++field) {
        pangu_core *encoded=NULL; CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        pangu_message m={.role=PANGU_TOOL_RESULT};
        if(field==0)m.usage.has_cache_write_1h=1;
        if(field==1)m.usage.has_reasoning=1;
        if(field==2)m.usage.has_total_from_provider=1;
        CHECK(pangu_core_append(owner,&m)==PANGU_OK);
        pangu_model_config model={0};
        CHECK(pangu_proxy_request_json(owner,owner,&model,S("{}"),&encoded)==PANGU_OK);
        pangu_core_destroy(owner);
        pangu_slice body=pangu_core_message_at(encoded,0)->content[0].data;
        yyjson_doc *doc=yyjson_read(body.data,body.len,0); CHECK(doc);
        yyjson_val *ms=yyjson_obj_get(yyjson_obj_get(yyjson_doc_get_root(doc),"context"),"messages");
        yyjson_val *u=yyjson_obj_get(yyjson_arr_get(ms,0),"usage"); CHECK(yyjson_is_obj(u));
        if(field==0)CHECK(yyjson_is_uint(yyjson_obj_get(u,"cacheWrite1h")));
        if(field==1)CHECK(yyjson_is_uint(yyjson_obj_get(u,"reasoning")));
        if(field==2)CHECK(yyjson_is_false(yyjson_obj_get(yyjson_obj_get(u,"cost"),"totalFromProvider")));
        yyjson_doc_free(doc); pangu_core_destroy(encoded);
    }
}
int main(int argc,char **argv) {
    if (argc==3 && !strcmp(argv[1],"--request-json")) {
        pangu_core *owner=NULL,*encoded=NULL;
        CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
        pangu_content text={.kind=PANGU_TEXT,.data=S("世界")};
        pangu_message user={.role=PANGU_USER,.content=&text,.content_count=1,.timestamp_ms=17};
        CHECK(pangu_core_append(owner,&user)==PANGU_OK);
        pangu_message response={.role=PANGU_ASSISTANT,.model=S("mock"),.provider=S("mock"),.api=S("mock"),
            .response_model=S("mock-revision"),.response_id=S("response\0世界"),
            .raw_stop_reason=S("provider-stop"),.stop_reason=PANGU_STOP,
            .presence=PANGU_MESSAGE_HAS_THINKING_LEVEL,
            .diagnostics_json=S("[{\"type\":\"fixture\",\"timestamp\":17,\"details\":{\"retry\":false}}]"),
            .deferred_json=S("{\"provider\":\"fixture\",\"api\":\"mock\",\"modelId\":\"mock\",\"id\":\"handle\",\"pollAfterMs\":0,\"data\":null}"),
            .has_end_turn=1,
            .usage={.input_cost=INFINITY,.output_cost=-INFINITY,.total_cost=NAN}};
        CHECK(pangu_core_append(owner,&response)==PANGU_OK);
        pangu_message nested={.role=PANGU_TOOL_RESULT,.tool_call_id=S("nested"),.tool_name=S("echo"),
            .presence=PANGU_MESSAGE_HAS_USAGE,
            .nested_calls_json=S("{\"calls\":[{\"id\":\"one\",\"name\":\"read\",\"arguments\":{\"path\":\"a.txt\"},\"status\":\"ok\",\"durationMs\":0}],\"complete\":false}")};
        CHECK(pangu_core_append(owner,&nested)==PANGU_OK);
        pangu_model_config model={.id=S("mock"),.provider=S("mock"),.api=S("mock"),
            .metadata_json=S("{\"contextWindow\":4096,\"maxTokens\":512,\"input\":[\"text\",\"image\"]}")};
        CHECK(pangu_proxy_request_json(owner,owner,&model,(pangu_slice){argv[2],strlen(argv[2])},&encoded)==PANGU_OK);
        pangu_slice body=pangu_core_message_at(encoded,0)->content[0].data;
        CHECK(fwrite(body.data,1,body.len,stdout)==body.len);
        pangu_core_destroy(encoded); pangu_core_destroy(owner); return 0;
    }
    if (argc==2 && !strcmp(argv[1],"--trace-options")) { provider_options(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-wire")) { wire_matrix(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-content-presence")) { content_presence(1); return 0; }
    if (argc==2 && !strcmp(argv[1],"--trace-message-presence")) { message_presence(1); return 0; }
    message_presence(0);
    invalid_message_presence();
    content_presence(0);
    wire_matrix(0);
    provider_options(0);
    int completed=0;
    for (size_t fail=1;fail<300;++fail) {
        memory mem={0}; pangu_core_config cfg={.allocator={&mem,allocate,release}};
        pangu_core *owner=NULL,*encoded=NULL; CHECK(pangu_core_create(&cfg,&owner)==PANGU_OK);
        pangu_system_section section={.name=S("remove"),.removed=1};
        pangu_tool_declaration tool={.name=S("lookup"),.schema_json=S("{}")};
        pangu_message m={.role=PANGU_SYSTEM,.sections=&section,.section_count=1,.tools_added=&tool,.tools_added_count=1};
        CHECK(pangu_core_append(owner,&m)==PANGU_OK);
        pangu_content image={.kind=PANGU_IMAGE,.data=S("AAAA"),.mime_type=S("image/png")};
        m=(pangu_message){.role=PANGU_USER,.content=&image,.content_count=1}; CHECK(pangu_core_append(owner,&m)==PANGU_OK);
        pangu_content blocks[]={{.kind=PANGU_TEXT,.data=S("世界")},{.kind=PANGU_THINKING,.data=S("reason"),.signature=S("sig"),.has_redacted=1},
            {.kind=PANGU_TOOL_CALL,.id=S("one"),.name=S("lookup"),.data=S("{\"value\":1}"),.tool_namespace=S("dynamic_tools")}};
        m=(pangu_message){.role=PANGU_ASSISTANT,.content=blocks,.content_count=3,.stop_reason=PANGU_TOOL_USE,
            .thinking_level=PANGU_THINKING_HIGH,.provider_thinking_level=S("high"),.usage={.has_cache_write_1h=1,.has_reasoning=1,.has_total_from_provider=1,
                .input_cost=INFINITY,.output_cost=-INFINITY,.total_cost=NAN},
            .diagnostics_json=S("[]"),.deferred_json=S("{}"),.has_end_turn=1}; CHECK(pangu_core_append(owner,&m)==PANGU_OK);
        m=(pangu_message){.role=PANGU_TOOL_RESULT,.tool_call_id=S("one"),.tool_name=S("lookup"),.is_error=1,
            .details_json=S("{\"partial\":true}"),.structured_json=S("{\"private\":true}"),
            .nested_calls_json=S("{\"calls\":[],\"complete\":true}")}; CHECK(pangu_core_append(owner,&m)==PANGU_OK);
        pangu_model_config model={.id=S("new"),.provider=S("mock"),.api=S("fixture"),.metadata_json=S("{\"id\":\"old\",\"contextWindow\":4096}")};
        mem.fail=mem.calls+fail;
        pangu_status status=pangu_proxy_request_json(owner,owner,&model,S("{\"temperature\":0.5,\"maxTokens\":100,\"authToken\":\"local\",\"signal\":{}}"),&encoded);
        CHECK(status==PANGU_OK || status==PANGU_NOMEM);
        if (status==PANGU_OK) { check(encoded); completed=1; } else CHECK(!encoded);
        pangu_core_destroy(encoded); pangu_core_destroy(owner); CHECK(!mem.live);
        if (completed) break;
    }
    CHECK(completed);
    pangu_core *owner=NULL,*encoded=NULL; CHECK(pangu_core_create(NULL,&owner)==PANGU_OK);
    pangu_content call={.kind=PANGU_TOOL_CALL,.data=S("{invalid}")};
    pangu_message m={.role=PANGU_ASSISTANT,.content=&call,.content_count=1};
    CHECK(pangu_core_append(owner,&m)==PANGU_OK);
    pangu_model_config model={0};
    CHECK(pangu_proxy_request_json(owner,owner,&model,(pangu_slice){0},&encoded)==PANGU_INVALID && !encoded);
    const pangu_slice invalid_metadata[]={S("{"),S("null"),S("true"),S("3"),S("\"text\""),(pangu_slice){0}};
    for(int field=0;field<3;++field)for(size_t bad=0;bad<sizeof(invalid_metadata)/sizeof(invalid_metadata[0]);++bad){
        CHECK(pangu_core_clear(owner)==PANGU_OK);
        m=(pangu_message){.role=field==2?PANGU_TOOL_RESULT:PANGU_ASSISTANT};
        pangu_slice value=invalid_metadata[bad];
        if(!value.len)value=field==0?S("{}"):S("[]");
        if(field==0)m.diagnostics_json=value;
        if(field==1)m.deferred_json=value;
        if(field==2)m.nested_calls_json=value;
        CHECK(pangu_core_append(owner,&m)==PANGU_OK);
        CHECK(pangu_proxy_request_json(owner,owner,&model,(pangu_slice){0},&encoded)==PANGU_INVALID && !encoded);
        CHECK(pangu_core_message_count(owner)==1);
    }
    pangu_core_destroy(owner); return 0;
}
