/* Pi agent.ts PendingMessageQueue and subscription lifecycle, d5b3de22. */
#include "internal.h"
#include <string.h>
void pangu_core_state_init(pangu_core *c) {
    static const char metadata[]="{\"name\":\"unknown\",\"baseUrl\":\"\",\"reasoning\":false,\"input\":[],\"cost\":{\"input\":0,\"output\":0,\"cacheRead\":0,\"cacheWrite\":0},\"contextWindow\":0,\"maxTokens\":0}";
    c->state_model=(pangu_model_config){.id={"unknown",7},.provider={"unknown",7},
        .api={"unknown",7},.metadata_json={metadata,sizeof(metadata)-1}};
}
const pangu_model_config *pangu_core_model(const pangu_core *c) { return c?&c->state_model:NULL; }
pangu_thinking_level pangu_core_thinking(const pangu_core *c) { return c?c->state_thinking:PANGU_THINKING_OFF; }
size_t pangu_core_tool_count(const pangu_core *c) { return c?c->state_tool_count:0; }
const pangu_tool *pangu_core_tool_at(const pangu_core *c,size_t i) { return c && i<c->state_tool_count?c->state_tools+i:NULL; }
static int state_size(size_t *n,pangu_slice s) {
    if ((s.len && !s.data) || s.len>SIZE_MAX-*n || (s.data && s.len==SIZE_MAX-*n))return 0;
    *n+=s.len+(s.data!=NULL); return 1;
}
static pangu_slice state_copy(pangu_slice s,char **p) {
    if (!s.data)return s;
    pangu_slice result={*p,s.len};
    if(s.len)memcpy(*p,s.data,s.len);
    (*p)[s.len]=0; *p+=s.len+1; return result;
}
pangu_status pangu_core_set_model(pangu_core *c,const pangu_model_config *model) {
    if(!c)return PANGU_INVALID;
    if(c->running)return PANGU_BUSY;
    if(!model)return PANGU_INVALID;
    size_t bytes=0;
    if(!state_size(&bytes,model->id) || !state_size(&bytes,model->provider) ||
       !state_size(&bytes,model->api) || !state_size(&bytes,model->metadata_json))return PANGU_INVALID;
    char *storage=bytes?c->memory.alloc(c->memory.ctx,bytes):NULL,*cursor=storage;
    if(bytes && !storage)return PANGU_NOMEM;
    pangu_model_config next=*model;
    next.id=state_copy(next.id,&cursor); next.provider=state_copy(next.provider,&cursor);
    next.api=state_copy(next.api,&cursor); next.metadata_json=state_copy(next.metadata_json,&cursor);
    if(c->state_model_storage)c->memory.free(c->memory.ctx,c->state_model_storage);
    c->state_model=next; c->state_model_storage=storage; return PANGU_OK;
}
pangu_status pangu_core_set_thinking(pangu_core *c,pangu_thinking_level level) {
    if(!c)return PANGU_INVALID;
    if(c->running)return PANGU_BUSY;
    if((unsigned)level>PANGU_THINKING_MAX)return PANGU_INVALID;
    c->state_thinking=level; return PANGU_OK;
}
pangu_status pangu_core_set_tools(pangu_core *c,const pangu_tool *tools,size_t count) {
    if(!c)return PANGU_INVALID;
    if(c->running)return PANGU_BUSY;
    if((count && !tools) || count>SIZE_MAX/sizeof(*tools))return PANGU_INVALID;
    size_t bytes=count*sizeof(*tools);
    for(size_t i=0;i<count;++i)
        if(!state_size(&bytes,tools[i].name) || !state_size(&bytes,tools[i].description) ||
           !state_size(&bytes,tools[i].schema_json))return PANGU_INVALID;
    pangu_tool *next=bytes?c->memory.alloc(c->memory.ctx,bytes):NULL;
    if(bytes && !next)return PANGU_NOMEM;
    char *cursor=count?(char *)(next+count):NULL;
    for(size_t i=0;i<count;++i) {
        next[i]=tools[i]; next[i].name=state_copy(tools[i].name,&cursor);
        next[i].description=state_copy(tools[i].description,&cursor);
        next[i].schema_json=state_copy(tools[i].schema_json,&cursor);
    }
    if(c->state_tools)c->memory.free(c->memory.ctx,c->state_tools);
    c->state_tools=next; c->state_tool_count=count; return PANGU_OK;
}
pangu_status pangu_core_replace_messages(pangu_core *c,const pangu_message *messages,size_t count) {
    if(!c)return PANGU_INVALID;
    if(c->running)return PANGU_BUSY;
    if((count && !messages) || count>SIZE_MAX/sizeof(*messages))return PANGU_INVALID;
    pangu_core staged={.memory=c->memory}; pangu_status status=PANGU_OK;
    for(size_t i=0;i<count;++i) {
        status=pangu_core_append_internal(&staged,messages+i);
        if(status!=PANGU_OK)goto fail;
    }
    char *error=NULL;
    if(c->error.len) {
        error=c->memory.alloc(c->memory.ctx,c->error.len);
        if(!error) { status=PANGU_NOMEM; goto fail; }
        memcpy(error,c->error.data,c->error.len);
    }
    for(size_t i=0;i<c->count;++i)c->memory.free(c->memory.ctx,c->messages[i]);
    if(c->messages)c->memory.free(c->memory.ctx,c->messages);
    if(c->retained_error)c->memory.free(c->memory.ctx,c->retained_error);
    c->retained_error=error; c->error=(pangu_slice){error,c->error.len};
    c->messages=staged.messages; c->count=staged.count; c->capacity=staged.capacity;
    return PANGU_OK;
fail:
    pangu_core_clear(&staged);
    if(staged.messages)c->memory.free(c->memory.ctx,staged.messages);
    return status;
}
/* An owned diagnostic bridges C status returns to source exception messages. */
pangu_status pangu_core_report_error(pangu_core *c,pangu_slice detail) {
    if(!c || !c->running || (detail.len && !detail.data))return PANGU_INVALID;
    char *copy=NULL;
    if(detail.len) {
        copy=c->memory.alloc(c->memory.ctx,detail.len);
        if(!copy)return PANGU_NOMEM;
        memcpy(copy,detail.data,detail.len);
    }
    if(c->failure_detail.data)c->memory.free(c->memory.ctx,(void *)c->failure_detail.data);
    c->failure_detail=(pangu_slice){copy,detail.len};return PANGU_OK;
}
struct pangu_subscription {
    pangu_subscription *next;
    pangu_event_fn callback;
    void *ctx;
    uint64_t id;
    int active;
};
static int queue_valid(pangu_queue_kind kind) { return (unsigned)kind<=PANGU_FOLLOW_UP; }
pangu_status pangu_core_enqueue(pangu_core *c,pangu_queue_kind kind,const pangu_message *m) {
    if (!c || !queue_valid(kind) || !m) return PANGU_INVALID;
    if (!c->queues[kind]) {
        pangu_core_config cfg={.allocator=c->memory};
        pangu_status s=pangu_core_create(&cfg,&c->queues[kind]);
        if (s!=PANGU_OK) return s;
    }
    return pangu_core_append(c->queues[kind],m);
}
pangu_status pangu_core_queue_mode(pangu_core *c,pangu_queue_kind kind,pangu_queue_mode mode) {
    if (!c || !queue_valid(kind) || (unsigned)mode>PANGU_QUEUE_ALL) return PANGU_INVALID;
    c->queue_modes[kind]=mode; return PANGU_OK;
}
pangu_status pangu_core_get_queue_mode(const pangu_core *c,pangu_queue_kind kind,pangu_queue_mode *out) {
    if (!c || !out || !queue_valid(kind)) return PANGU_INVALID;
    *out=c->queue_modes[kind]; return PANGU_OK;
}
size_t pangu_core_peek_queued_count(const pangu_core *c) {
    if (!c) return 0;
    pangu_queue_kind kind=pangu_core_queue_count(c,PANGU_STEERING)?PANGU_STEERING:PANGU_FOLLOW_UP;
    size_t count=pangu_core_queue_count(c,kind);
    return c->queue_modes[kind]==PANGU_QUEUE_ONE && count?1:count;
}
const pangu_message *pangu_core_peek_queued_at(const pangu_core *c,size_t index) {
    if (!c || index>=pangu_core_peek_queued_count(c)) return NULL;
    pangu_queue_kind kind=pangu_core_queue_count(c,PANGU_STEERING)?PANGU_STEERING:PANGU_FOLLOW_UP;
    return pangu_core_queue_at(c,kind,index);
}
size_t pangu_core_queue_count(const pangu_core *c,pangu_queue_kind kind) {
    return c && queue_valid(kind)?pangu_core_message_count(c->queues[kind])+
        (c->held_kind==kind?c->held_count:0):0;
}
const pangu_message *pangu_core_queue_at(const pangu_core *c,pangu_queue_kind kind,size_t i) {
    if (!c || !queue_valid(kind)) return NULL;
    if (c->held_kind==kind) {
        if (i<c->held_count) return c->held_messages[i];
        i-=c->held_count;
    }
    return pangu_core_message_at(c->queues[kind],i);
}
pangu_status pangu_core_clear_queue(pangu_core *c,pangu_queue_kind kind) {
    if (!c || !queue_valid(kind)) return PANGU_INVALID;
    if (c->held_kind==kind) {
        for (size_t i=0;i<c->held_count;++i) c->memory.free(c->memory.ctx,c->held_messages[i]);
        c->held_count=0;
    }
    return c->queues[kind]?pangu_core_clear(c->queues[kind]):PANGU_OK;
}
pangu_status pangu_core_truncate_queue(pangu_core *c,pangu_queue_kind kind,size_t count) {
    if (!c || !queue_valid(kind) || count>pangu_core_queue_count(c,kind)) return PANGU_INVALID;
    size_t held=c->held_kind==kind?c->held_count:0;
    size_t keep_held=count<held?count:held;
    if (c->held_kind==kind) {
        for (size_t i=keep_held;i<c->held_count;++i) c->memory.free(c->memory.ctx,c->held_messages[i]);
        c->held_count=keep_held;
    }
    pangu_core *queue=c->queues[kind];
    if (queue) {
        size_t keep=count-keep_held;
        for (size_t i=keep;i<queue->count;++i) queue->memory.free(queue->memory.ctx,queue->messages[i]);
        queue->count=keep;
    }
    return PANGU_OK;
}
size_t pangu_core_prepared_count(const pangu_core *c) { return c?c->prepared_count:0; }
const pangu_message *pangu_core_prepared_at(const pangu_core *c,size_t index) {
    return c && index<c->prepared_count?c->prepared_messages[index]:NULL;
}
static void sweep(pangu_core *c) {
    if (c->dispatch_depth) return;
    pangu_subscription **link=&c->subscriptions;
    c->subscription_tail=NULL;
    while (*link) {
        pangu_subscription *s=*link;
        if (!s->active) { *link=s->next; c->memory.free(c->memory.ctx,s); }
        else { c->subscription_tail=s; link=&s->next; }
    }
}
pangu_status pangu_core_subscribe(pangu_core *c,pangu_event_fn callback,void *ctx,uint64_t *id) {
    if (!c || !callback || !id) return PANGU_INVALID;
    *id=0;
    for (pangu_subscription *s=c->subscriptions;s;s=s->next)
        if (s->active && s->callback==callback && s->ctx==ctx) { *id=s->id; return PANGU_OK; }
    if (c->next_subscription==UINT64_MAX) return PANGU_NOMEM;
    pangu_subscription *s=c->memory.alloc(c->memory.ctx,sizeof(*s));
    if (!s) return PANGU_NOMEM;
    *s=(pangu_subscription){.callback=callback,.ctx=ctx,.id=++c->next_subscription,.active=1};
    if (c->subscription_tail) c->subscription_tail->next=s;
    else c->subscriptions=s;
    c->subscription_tail=s; *id=s->id; return PANGU_OK;
}
pangu_status pangu_core_unsubscribe(pangu_core *c,uint64_t id) {
    if (!c || !id) return PANGU_INVALID;
    for (pangu_subscription *s=c->subscriptions;s;s=s->next) if (s->id==id) { s->active=0; break; }
    sweep(c); return PANGU_OK;
}
void pangu_core_dispatch(pangu_core *c,const pangu_event *event) {
    ++c->dispatch_depth;
    for (pangu_subscription *s=c->subscriptions;s;s=s->next)
        if (s->active) s->callback(s->ctx,c,event);
    --c->dispatch_depth; sweep(c);
}
void pangu_core_state_destroy(pangu_core *c) {
    if(c->state_model_storage)c->memory.free(c->memory.ctx,c->state_model_storage);
    if(c->state_tools)c->memory.free(c->memory.ctx,c->state_tools);
    for (size_t i=0;i<2;++i) pangu_core_destroy(c->queues[i]);
    pangu_subscription *s=c->subscriptions;
    while (s) { pangu_subscription *next=s->next; c->memory.free(c->memory.ctx,s); s=next; }
}
pangu_status pangu_core_wait_idle(const pangu_core *c) {
    return !c?PANGU_INVALID:c->running?PANGU_BUSY:PANGU_OK;
}
const pangu_message *pangu_core_streaming_message(const pangu_core *c) { return c?c->streaming:NULL; }
pangu_slice pangu_core_error(const pangu_core *c) { return c?c->error:(pangu_slice){0}; }
