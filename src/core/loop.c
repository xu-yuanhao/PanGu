/* Native adapter of Pi agent-loop.ts (d5b3de22), MIT; see Pi-LICENSE. */
#include <pangu/core/loop.h>
#include "internal.h"
#include <string.h>
#include <stddef.h>
typedef union snapshot_block snapshot_block;
union snapshot_block {
    max_align_t alignment;
    struct { size_t capacity; } allocation;
};
typedef struct {
    pangu_allocator upstream;
    snapshot_block *spare;
} snapshot_pool;
static void *snapshot_allocate(void *ctx,size_t n) {
    snapshot_pool *pool=ctx;
    if (n>=1024 && pool->spare && pool->spare->allocation.capacity>=n) {
        snapshot_block *block=pool->spare; pool->spare=NULL; return block+1;
    }
    size_t capacity=n;
    if (n>=1024) {
        capacity=1024;
        while (capacity<n && capacity<=SIZE_MAX/2) capacity*=2;
        if (capacity<n) capacity=n;
    }
    if (capacity>SIZE_MAX-sizeof(snapshot_block)) return NULL;
    snapshot_block *block=pool->upstream.alloc(pool->upstream.ctx,sizeof(*block)+capacity);
    if (!block) return NULL;
    block->allocation.capacity=capacity; return block+1;
}
static void snapshot_release(void *ctx,void *p) {
    if (!p) return;
    snapshot_pool *pool=ctx; snapshot_block *block=(snapshot_block *)p-1;
    if (block->allocation.capacity>=1024 &&
        (!pool->spare || pool->spare->allocation.capacity<block->allocation.capacity)) {
        if (pool->spare) pool->upstream.free(pool->upstream.ctx,pool->spare);
        pool->spare=block;
    } else pool->upstream.free(pool->upstream.ctx,block);
}
typedef struct {
    pangu_core *core, *scratch, *generated;
    snapshot_pool *snapshot_memory;
    const pangu_core *turn_results;
    const pangu_loop_config *config;
    const pangu_content *call;
    int started, final, preserve, silent, skip_thinking;
    int has_duration;
    uint64_t duration_ms;
    int64_t result_timestamp;
    const pangu_assistant_event *assistant_event;
    pangu_tool_sink updates;
    void *update_ctx;
    pangu_status failure;
} receiver;
static pangu_slice str(const char *s) { return (pangu_slice){s, strlen(s)}; }
static void emit(receiver *r, pangu_event_kind kind, const pangu_message *m) {
    pangu_core *c=r->core;
    if (r->silent) return;
    if (kind==PANGU_MESSAGE_START || kind==PANGU_MESSAGE_UPDATE) c->streaming=m;
    if (kind==PANGU_MESSAGE_END) {
        c->streaming=NULL;
    }
    pangu_event e={.kind=kind,.message=m,.tool_call=r->call,.has_duration=kind==PANGU_TOOL_END?r->has_duration:0,.duration_ms=r->duration_ms};
    if (kind==PANGU_MESSAGE_UPDATE) e.assistant_event=r->assistant_event;
    if (kind==PANGU_TURN_END) {
        if (m && m->role==PANGU_ASSISTANT && m->error.len) c->error=m->error;
        e.tool_results=r->turn_results; e.new_messages=r->generated;
        e.message_count=r->generated?r->generated->count:0;
    }
    if (r->config->event) r->config->event(r->config->ctx,c,&e);
    pangu_core_dispatch(c,&e);
}
static pangu_message error_message(pangu_role role, const char *text, pangu_content *b) {
    *b = (pangu_content){.kind=PANGU_TEXT, .data=str(text)};
    return (pangu_message){.role=role, .content=b, .content_count=1,
        .is_error=1, .error=str(text), .stop_reason=PANGU_ERROR,
        .timestamp_ms=pangu_core_wallclock_ms()};
}
static void compact(receiver *r) {
    pangu_core *s=r->scratch;
    if (s->count<2) return;
    for (size_t i=0;i+1<s->count;++i) s->memory.free(s->memory.ctx,s->messages[i]);
    s->messages[0]=s->messages[s->count-1]; s->count=1;
}
static pangu_status receive(void *ctx, const pangu_message *m, int final) {
    receiver *r = ctx;
    if (r->failure != PANGU_OK) return r->failure;
    if (r->call && r->final && !final) return PANGU_OK;
    if (!m || r->final || (r->call ? m->role != PANGU_TOOL_RESULT : m->role != PANGU_ASSISTANT))
        return r->failure = PANGU_INVALID;
    pangu_message copy = *m;
    if (!r->call && final && !r->skip_thinking) {
        copy.thinking_level=r->config->thinking_level;
        copy.presence|=PANGU_MESSAGE_HAS_THINKING_LEVEL;
    }
    if (r->call) { copy.tool_call_id=r->call->id; copy.tool_name=r->call->name; }
    pangu_status s = pangu_core_append(r->scratch, &copy);
    if (s != PANGU_OK) return r->failure=s;
    const pangu_message *saved = pangu_core_message_at(r->scratch, pangu_core_message_count(r->scratch)-1);
    if (r->call) {
        if (!final) {
            if (r->silent && r->updates) {
                pangu_status update=r->updates(r->update_ctx,saved,0);
                if (update!=PANGU_OK) return r->failure=update;
            } else emit(r,PANGU_TOOL_UPDATE,saved);
        }
    } else {
        if (!r->started) { r->started=1; emit(r,PANGU_MESSAGE_START,saved); }
        else if (!final) emit(r,PANGU_MESSAGE_UPDATE,saved);
    }
    if (!r->preserve) compact(r);
    r->final = !!final;
    return PANGU_OK;
}
static pangu_status receive_event(void *ctx,const pangu_message *m,const pangu_assistant_event *e) {
    receiver *r=ctx;
    if (r->failure!=PANGU_OK) return r->failure;
    if (!e || (unsigned)e->kind>PANGU_ASSISTANT_ERROR || !m || m->role!=PANGU_ASSISTANT ||
        (e->delta.len && !e->delta.data) || (e->content.len && !e->content.data))
        return r->failure=PANGU_INVALID;
    int final=e->kind==PANGU_ASSISTANT_DONE || e->kind==PANGU_ASSISTANT_ERROR;
    if (r->final) return r->failure=PANGU_INVALID;
    if (final && ((e->kind==PANGU_ASSISTANT_ERROR && e->reason!=PANGU_ERROR && e->reason!=PANGU_ABORTED) ||
        (e->kind==PANGU_ASSISTANT_DONE && e->reason!=PANGU_STOP && e->reason!=PANGU_LENGTH && e->reason!=PANGU_TOOL_USE && e->reason!=PANGU_DEFERRED)))
        return r->failure=PANGU_INVALID;
    if (!final && e->kind!=PANGU_ASSISTANT_START && !r->started) return PANGU_OK;
    if (!final && e->kind!=PANGU_ASSISTANT_START &&
        (e->content_index>=m->content_count || (e->kind==PANGU_ASSISTANT_TOOL_END && !e->tool_call)))
        return r->failure=PANGU_INVALID;
    pangu_message copy=*m;
    if (final) copy.stop_reason=e->reason;
    r->assistant_event=e;
    pangu_status status=receive(r,&copy,final);
    r->assistant_event=NULL;
    return status;
}
static pangu_status replacement(void *ctx, const pangu_message *m, int final) {
    receiver *r=ctx;
    if (r->failure!=PANGU_OK) return r->failure;
    if (!final) return r->failure=PANGU_INVALID;
    return receive(ctx,m,final);
}
static int equals(pangu_slice a, pangu_slice b) {
    return a.len == b.len && (!a.len || (a.data && b.data && !memcmp(a.data,b.data,a.len)));
}
static void destroy_scratch(receiver *r) {
    pangu_core_destroy(r->scratch); r->scratch=NULL;
    if (r->snapshot_memory) {
        snapshot_pool *pool=r->snapshot_memory;
        if (pool->spare) pool->upstream.free(pool->upstream.ctx,pool->spare);
        pool->upstream.free(pool->upstream.ctx,pool);
        r->snapshot_memory=NULL;
    }
}
static pangu_status scratch(receiver *r) {
    pangu_core_config c = {.allocator=r->core->memory};
    /* Only model temporaries use this cache. Tool results can transfer their
     * Core to a caller, whose allocator lifetime must remain the owner's. */
    if (!r->call) {
        snapshot_pool *pool=r->core->memory.alloc(r->core->memory.ctx,sizeof(*pool));
        if (!pool) return PANGU_NOMEM;
        *pool=(snapshot_pool){.upstream=r->core->memory}; r->snapshot_memory=pool;
        c.allocator=(pangu_allocator){pool,snapshot_allocate,snapshot_release};
    }
    pangu_status status=pangu_core_create(&c,&r->scratch);
    if (status!=PANGU_OK) destroy_scratch(r);
    return status;
}
static const pangu_message *last(receiver *r) {
    return pangu_core_message_at(r->scratch, pangu_core_message_count(r->scratch)-1);
}
typedef struct {
    receiver r;
    const pangu_tool *tool;
    const pangu_message *assistant;
    pangu_content prepared_call;
    char *owned_args,*owned_before_reason;
    int args_set;
    uint64_t started_ns;
    void *operation;
    int immediate, completed, started, pending_id;
    pangu_status status;
} tool_task;
size_t pangu_core_pending_tool_count(const pangu_core *c) {
    if (!c) return 0;
    const tool_task *tasks=c->active_tool_tasks; size_t count=0;
    for (size_t i=0;i<c->active_tool_count;++i) count+=tasks[i].pending_id;
    return count;
}
const pangu_content *pangu_core_pending_tool_at(const pangu_core *c,size_t index) {
    if (!c) return NULL;
    const tool_task *tasks=c->active_tool_tasks;
    for (size_t i=0;i<c->active_tool_count;++i) if (tasks[i].pending_id) {
        if (!index) return tasks[i].r.call;
        --index;
    }
    return NULL;
}
/* Agent.pendingToolCalls is a Set of IDs, not a count of operations. */
static void pending_start(tool_task *task) {
    if(task->r.silent)return;
    pangu_core *c=task->r.core;
    tool_task *tasks=c->active_tool_tasks;
    for (size_t i=0;i<c->active_tool_count;++i)
        if (tasks[i].pending_id && equals(tasks[i].r.call->id,task->r.call->id)) return;
    task->pending_id=1;
}
static void tool_finished(tool_task *task) {
    task->completed=1; task->pending_id=0;
    if(task->r.silent)return;
    pangu_core *c=task->r.core;
    tool_task *tasks=c->active_tool_tasks;
    for (size_t i=0;i<c->active_tool_count;++i)
        if (equals(tasks[i].r.call->id,task->r.call->id)) tasks[i].pending_id=0;
}
static pangu_status arguments(void *ctx,pangu_slice value) {
    tool_task *t=ctx;
    if (t->status!=PANGU_OK) return t->status;
    if (t->args_set || (value.len && !value.data)) return t->status=PANGU_INVALID;
    char *copy=value.len?t->r.core->memory.alloc(t->r.core->memory.ctx,value.len):NULL;
    if (value.len && !copy) return t->status=PANGU_NOMEM;
    if (value.len) memcpy(copy,value.data,value.len);
    if (t->owned_args) t->r.core->memory.free(t->r.core->memory.ctx,t->owned_args);
    t->owned_args=copy; t->prepared_call.data=(pangu_slice){copy,value.len}; t->args_set=1;
    return PANGU_OK;
}
static pangu_status patch(void *ctx,const pangu_tool_patch *p) {
    receiver *r=ctx;
    if (r->failure!=PANGU_OK) return r->failure;
    if (!p || (p->fields & ~63u)) return r->failure=PANGU_INVALID;
    pangu_message m=*last(r);
    if (p->fields&PANGU_PATCH_CONTENT) {
        m.content=p->content; m.content_count=p->content_count;
        if (!(p->fields&PANGU_PATCH_STRUCTURED)) m.structured_json=(pangu_slice){0};
    }
    if (p->fields&PANGU_PATCH_DETAILS) m.details_json=p->details_json;
    if (p->fields&PANGU_PATCH_STRUCTURED) m.structured_json=p->structured_json;
    if (p->fields&PANGU_PATCH_USAGE) {m.usage=p->usage;m.presence|=PANGU_MESSAGE_HAS_USAGE;}
    if (p->fields&PANGU_PATCH_ERROR) m.is_error=p->is_error;
    if (p->fields&PANGU_PATCH_TERMINATE) m.terminate=p->terminate;
    return replacement(r,&m,1);
}
static uint64_t now(receiver *r) {
    return r->config->monotonic_ns?r->config->monotonic_ns(r->config->ctx):pangu_core_monotonic_ns();
}
static pangu_status tool_error(tool_task *task, const char *text, pangu_before_result *before) {
    pangu_core *owner=task->r.core;
    pangu_content b;
    pangu_message m=error_message(PANGU_TOOL_RESULT,text,&b);
    if(owner->failure_detail.len && !before)b.data=m.error=owner->failure_detail;
    if (before) { if (before->reason.len) b.data=before->reason; m.terminate=before->terminate; }
    task->r.final=0;
    pangu_status status=receive(&task->r,&m,1);
    if(owner->failure_detail.data)owner->memory.free(owner->memory.ctx,(void *)owner->failure_detail.data);
    owner->failure_detail=(pangu_slice){0};return status;
}
static pangu_status named_error(tool_task *task,const char *prefix,const char *suffix) {
    pangu_slice name=task->r.call->name;
    size_t left=strlen(prefix),right=strlen(suffix);
    if (name.len>SIZE_MAX-left || right>SIZE_MAX-left-name.len) return PANGU_NOMEM;
    size_t len=left+name.len+right;
    char *text=task->r.core->memory.alloc(task->r.core->memory.ctx,len);
    if (!text) return PANGU_NOMEM;
    memcpy(text,prefix,left); if (name.len) memcpy(text+left,name.data,name.len); memcpy(text+left+name.len,suffix,right);
    pangu_content b={.kind=PANGU_TEXT,.data={text,len}};
    pangu_message m={.role=PANGU_TOOL_RESULT,.content=&b,.content_count=1,.is_error=1};
    pangu_status s=receive(&task->r,&m,1);
    task->r.core->memory.free(task->r.core->memory.ctx,text);
    return s;
}
static pangu_status invalid_arguments_error(tool_task *task) {
    const char prefix[]="Invalid arguments for tool \"",middle[]="\": ";
    pangu_slice name=task->r.call->name,detail=task->r.call->arguments_error;
    size_t fixed=sizeof(prefix)-1+sizeof(middle)-1;
    if(name.len>SIZE_MAX-fixed || detail.len>SIZE_MAX-fixed-name.len)return PANGU_NOMEM;
    size_t bytes=fixed+name.len+detail.len;
    pangu_allocator a=task->r.core->memory;char *text=a.alloc(a.ctx,bytes);
    if(!text)return PANGU_NOMEM;
    size_t at=sizeof(prefix)-1;memcpy(text,prefix,at);
    if(name.len)memcpy(text+at,name.data,name.len);
    at+=name.len;
    memcpy(text+at,middle,sizeof(middle)-1);at+=sizeof(middle)-1;
    if(detail.len)memcpy(text+at,detail.data,detail.len);
    pangu_content content={.kind=PANGU_TEXT,.data={text,bytes}};
    pangu_message result={.role=PANGU_TOOL_RESULT,.content=&content,.content_count=1,.is_error=1};
    task->r.final=0;pangu_status status=receive(&task->r,&result,1);a.free(a.ctx,text);return status;
}
typedef struct {
    tool_task *task;
    pangu_before_update update;
    char *args,*reason;
    int submitted;
    pangu_status failure;
} before_update;
static pangu_status before_sink(void *ctx,const pangu_before_update *update) {
    before_update *p=ctx;
    if (p->failure!=PANGU_OK) return p->failure;
    if (!update || p->submitted || (update->result.reason.len && !update->result.reason.data))
        return p->failure=PANGU_INVALID;
    p->submitted=1;
    pangu_core *c=p->task->r.core;
    if (update->has_arguments) {
        pangu_status status=pangu_core_validate_json(c,update->arguments_json,1);
        if (status!=PANGU_OK) return p->failure=status;
        p->args=c->memory.alloc(c->memory.ctx,update->arguments_json.len);
        if (!p->args) return p->failure=PANGU_NOMEM;
        memcpy(p->args,update->arguments_json.data,update->arguments_json.len);
    }
    if (update->result.reason.len) {
        p->reason=c->memory.alloc(c->memory.ctx,update->result.reason.len);
        if (!p->reason) return p->failure=PANGU_NOMEM;
        memcpy(p->reason,update->result.reason.data,update->result.reason.len);
    }
    p->update=*update; return PANGU_OK;
}
static pangu_status before_updated(tool_task *task,pangu_before_result *out) {
    receiver *r=&task->r;
    pangu_tool_context input={.assistant=task->assistant,.call=r->call,.args_json=task->prepared_call.data};
    before_update p={.task=task};
    pangu_status status=r->config->before_tool_update(r->config->ctx,r->core,&input,before_sink,&p);
    if (p.failure!=PANGU_OK) status=p.failure;
    if (status==PANGU_OK && p.submitted) {
        if (p.update.has_arguments) {
            if (task->owned_args) r->core->memory.free(r->core->memory.ctx,task->owned_args);
            task->owned_args=p.args; p.args=NULL;
            task->prepared_call.data=(pangu_slice){task->owned_args,p.update.arguments_json.len};
        }
        task->owned_before_reason=p.reason; p.reason=NULL;
        *out=p.update.result; out->reason=(pangu_slice){task->owned_before_reason,p.update.result.reason.len};
    }
    if (p.args) r->core->memory.free(r->core->memory.ctx,p.args);
    if (p.reason) r->core->memory.free(r->core->memory.ctx,p.reason);
    return status;
}
static pangu_status preflight(tool_task *task, const pangu_message *assistant) {
    receiver *r=&task->r;
    task->assistant=assistant; task->prepared_call=*r->call;
    pending_start(task); emit(r,PANGU_TOOL_START,NULL);
    for (size_t i=0;i<r->config->tool_count;++i)
        if (equals(r->config->tools[i].name,r->call->name)) { task->tool=&r->config->tools[i]; break; }
    const pangu_tool *t=task->tool;
    const char *error=NULL;int invalid_arguments=0;
    pangu_before_result before={0};
    if (!r->silent && assistant->stop_reason==PANGU_LENGTH) error="Tool arguments may be truncated; re-issue the tool call";
    else if (!t) error="Tool not found";
    else if (!t->validate && !t->validate_arguments && !t->validate_call) error="Missing tool argument validator";
    else {
        pangu_status preparation=PANGU_OK;
        if (t->prepare_arguments) {
            preparation=t->prepare_arguments(t->ctx,r->call->data,arguments,task);
            if (task->status!=PANGU_OK) preparation=task->status;
            if (preparation==PANGU_NOMEM) return preparation;
            if (preparation==PANGU_OK && !task->args_set) preparation=PANGU_INVALID;
        }
        pangu_status validation=preparation;
        if (preparation==PANGU_OK && r->call->arguments_error.len) {
            invalid_arguments=1;validation=PANGU_INVALID;
        } else if (preparation==PANGU_OK) {
            if (t->validate_call || t->validate_arguments) {
                task->args_set=0;
                validation=t->validate_call?
                    t->validate_call(t->ctx,r->core,r->call,t->schema_json,task->prepared_call.data,arguments,task):
                    t->validate_arguments(t->ctx,t->schema_json,task->prepared_call.data,arguments,task);
                if (task->status!=PANGU_OK) validation=task->status;
                if (validation==PANGU_OK && !task->args_set) validation=PANGU_INVALID;
            } else validation=t->validate(t->ctx,t->schema_json,task->prepared_call.data);
        }
        if (validation==PANGU_NOMEM) return validation;
        if (validation!=PANGU_OK) error="Invalid tool arguments";
        else if (r->config->before_tool_update) {
            pangu_status status=before_updated(task,&before);
            if (status==PANGU_NOMEM) return status;
            if (status!=PANGU_OK) error="Tool preflight hook failed";
        } else if (r->config->before_tool_context) {
            pangu_tool_context context={.assistant=assistant,.call=r->call,.args_json=task->prepared_call.data};
            before=r->config->before_tool_context(r->config->ctx,r->core,&context);
        } else if (r->config->before_tool) before=r->config->before_tool(r->config->ctx,r->core,assistant,r->call);
    }
    if (!error && r->core->cancelled) error="Operation aborted";
    else if (!error && before.block) error="Tool execution was blocked";
    if (!error && !(t->execute || (t->start && t->release && r->config->pump)))
        error="Tool has no compatible executor";
    if (error) {
        task->status=PANGU_OK;
        task->immediate=1;
        pangu_status status;
        if (!r->silent && assistant->stop_reason==PANGU_LENGTH)
            status=named_error(task,"Tool call \"","\" was not executed: the response hit the output token limit, so its arguments may be truncated. Re-issue the tool call with complete arguments.");
        else if (!t) status=named_error(task,"Tool "," not found");
        else if(invalid_arguments)status=invalid_arguments_error(task);
        else status=tool_error(task,error,before.block && !r->core->cancelled?&before:NULL);
        if (status!=PANGU_OK) return status;
        tool_finished(task); emit(r,PANGU_TOOL_END,last(r));
    }
    return PANGU_OK;
}
static pangu_status finalize(tool_task *task, pangu_status status) {
    receiver *r=&task->r;
    if (r->has_duration) {
        uint64_t end=now(r),elapsed=end>=task->started_ns?end-task->started_ns:0;
        r->duration_ms=elapsed/UINT64_C(1000000)+(elapsed%UINT64_C(1000000)>=500000);
    }
    if (r->failure!=PANGU_OK) return r->failure;
    if (status==PANGU_NOMEM) return status;
    if (status!=PANGU_OK || !r->final) {
        status=tool_error(task,"Tool execution failed",NULL);
        if (status!=PANGU_OK) return status;
    }
    if (r->config->after_tool_patch || r->config->after_tool) {
        const pangu_message *result=last(r);
        r->final=0; r->preserve=1;
        if (r->config->after_tool_patch) {
            pangu_tool_context context={.assistant=task->assistant,.call=r->call,.args_json=task->prepared_call.data,.result=result};
            status=r->config->after_tool_patch(r->config->ctx,r->core,&context,patch,r);
        } else status=r->config->after_tool(r->config->ctx,r->core,r->call,result,replacement,r);
        r->preserve=0; compact(r);
        if (r->failure!=PANGU_OK) return r->failure;
        if (status==PANGU_NOMEM) return status;
        if (status!=PANGU_OK) {
            status=tool_error(task,"Tool result hook failed",NULL);
            if (status!=PANGU_OK) return status;
        }
    }
    tool_finished(task); emit(r,PANGU_TOOL_END,last(r));
    return PANGU_OK;
}
static pangu_status async_receive(void *ctx, const pangu_message *m, int final) {
    tool_task *task=ctx;
    if (task->completed && !final) return PANGU_OK;
    if (task->completed || task->status!=PANGU_OK) return PANGU_INVALID;
    pangu_status s=receive(&task->r,m,final);
    if (s==PANGU_OK && final) s=finalize(task,PANGU_OK);
    return task->status=s;
}
static pangu_status execute(tool_task *task) {
    receiver *r=&task->r;
    if (r->core->cancelled) {
        pangu_status s=tool_error(task,"Operation aborted",NULL);
        if (s!=PANGU_OK) return s;
        tool_finished(task); emit(r,PANGU_TOOL_END,last(r)); return PANGU_OK;
    }
    const pangu_tool *t=task->tool;
    task->started_ns=now(r); r->has_duration=1;
    if (t->start && t->release && r->config->pump) {
        pangu_status s=t->start(t->ctx,r->core,&task->prepared_call,async_receive,task,&task->operation);
        task->started=s==PANGU_OK || task->operation!=NULL;
        if (task->status!=PANGU_OK) return task->status;
        if (s!=PANGU_OK) {
            if (task->completed) return PANGU_INVALID;
            return finalize(task,s);
        }
        return PANGU_OK;
    }
    return finalize(task,t->execute(t->ctx,r->core,&task->prepared_call,receive,r));
}
static pangu_status await_tasks(tool_task *tasks, size_t count) {
    for (;;) {
        int pending=0;
        for (size_t i=0;i<count;++i) {
            if (tasks[i].status!=PANGU_OK) return tasks[i].status;
            pending |= !tasks[i].completed;
        }
        if (!pending) return PANGU_OK;
        receiver *r=&tasks[0].r;
        pangu_status s=r->config->pump(r->config->ctx,r->core);
        if (s!=PANGU_OK) return s;
    }
}
static pangu_status commit(receiver *r,const pangu_message *message) {
    pangu_status s=pangu_core_append_internal(r->generated,message);
    if (s!=PANGU_OK) return s;
    s=pangu_core_append_internal(r->core,message);
    if (s!=PANGU_OK) {
        pangu_core *g=r->generated;
        g->memory.free(g->memory.ctx,g->messages[--g->count]);
    }
    return s;
}
static pangu_message result_message(receiver *r) {
    pangu_message m=*last(r);
    if (!r->result_timestamp) r->result_timestamp=pangu_core_wallclock_ms();
    m.timestamp_ms=r->result_timestamp;
    m.structured_json=(pangu_slice){0}; m.terminate=0;
    m.has_duration=r->has_duration; m.duration_ms=r->duration_ms;
    return m;
}
static void result_events(receiver *r) {
    pangu_message m=result_message(r);
    emit(r,PANGU_MESSAGE_START,&m); emit(r,PANGU_MESSAGE_END,&m);
}
static pangu_status batch(receiver *base, const pangu_message *assistant,
    size_t *calls, int *all_terminate) {
    size_t count=0;
    int sequential=base->config->tool_execution==PANGU_TOOLS_SEQUENTIAL || assistant->stop_reason==PANGU_LENGTH;
    for (size_t i=0;i<assistant->content_count;++i) {
        const pangu_content *call=&assistant->content[i];
        if (call->kind!=PANGU_TOOL_CALL) continue;
        ++count;
        for (size_t j=0;j<base->config->tool_count;++j) {
            if (!equals(call->name,base->config->tools[j].name)) continue;
            /* Match the same first definition used by preflight and Pi find. */
            if (base->config->tools[j].execution_mode==PANGU_TOOLS_SEQUENTIAL) sequential=1;
            break;
        }
    }
    *calls=0; *all_terminate=1;
    if (!count) return PANGU_OK;
    if (count>SIZE_MAX/sizeof(tool_task)) return PANGU_NOMEM;
    tool_task *tasks=base->core->memory.alloc(base->core->memory.ctx,count*sizeof(*tasks));
    if (!tasks) return PANGU_NOMEM;
    memset(tasks,0,count*sizeof(*tasks));
    base->core->active_tool_tasks=tasks; base->core->active_tool_count=0;
    size_t used=0;
    pangu_status status=PANGU_OK;
    for (size_t i=0;i<assistant->content_count;++i) {
        if (assistant->content[i].kind!=PANGU_TOOL_CALL) continue;
        tool_task *t=&tasks[used++];
        t->r=(receiver){.core=base->core,.config=base->config,.call=&assistant->content[i]};
        status=scratch(&t->r); if (status!=PANGU_OK) goto cleanup;
        base->core->active_tool_count=used;
        status=preflight(t,assistant); if (status!=PANGU_OK) goto cleanup;
        if (sequential) {
            if (!t->immediate) { status=execute(t); if (status!=PANGU_OK) goto cleanup; }
            status=await_tasks(t,1); if (status!=PANGU_OK) goto cleanup;
            result_events(&t->r);
        }
        if (base->core->cancelled) break;
    }
    if (!sequential) {
        for (size_t i=0;i<used;++i) if (!tasks[i].immediate) {
            status=execute(&tasks[i]); if (status!=PANGU_OK) goto cleanup;
        }
        status=await_tasks(tasks,used); if (status!=PANGU_OK) goto cleanup;
        for (size_t i=0;i<used;++i) result_events(&tasks[i].r);
    }
    for (size_t i=0;i<used;++i) {
        pangu_message m=result_message(&tasks[i].r);
        status=commit(base,&m);
        if (status!=PANGU_OK) goto cleanup;
        ++*calls;
        *all_terminate &= !!last(&tasks[i].r)->terminate;
    }
cleanup:
    base->core->active_tool_tasks=NULL; base->core->active_tool_count=0;
    /* Quiesce operations before invalidating their sink contexts. */
    for (size_t i=0;i<used;++i) {
        if (tasks[i].started) tasks[i].tool->release(tasks[i].tool->ctx,tasks[i].operation);
        if (tasks[i].owned_args) base->core->memory.free(base->core->memory.ctx,tasks[i].owned_args);
        if (tasks[i].owned_before_reason) base->core->memory.free(base->core->memory.ctx,tasks[i].owned_before_reason);
        pangu_core_destroy(tasks[i].r.scratch);
    }
    base->core->memory.free(base->core->memory.ctx,tasks);
    return status;
}
pangu_status pangu_core_call_tool(pangu_core *c,const pangu_message *assistant,
    const pangu_content *call,const pangu_loop_config *config,pangu_tool_sink updates,
    void *update_ctx,pangu_tool_outcome *out) {
    if (!out) return PANGU_INVALID;
    *out=(pangu_tool_outcome){0};
    if (!c || !assistant || assistant->role!=PANGU_ASSISTANT || !call || call->kind!=PANGU_TOOL_CALL ||
        !config || (config->tool_count && !config->tools) || (call->data.len && !call->data.data) ||
        (call->name.len && !call->name.data) || (call->id.len && !call->id.data)) return PANGU_INVALID;
    int own_run=!c->running;
    if (own_run) { c->running=1; c->cancelled=0; }
    pangu_slice inherited_detail=c->failure_detail;c->failure_detail=(pangu_slice){0};
    tool_task task={.r={.core=c,.config=config,.call=call,.silent=1,.updates=updates,.update_ctx=update_ctx}};
    pangu_status status=scratch(&task.r);
    if (status==PANGU_OK) status=preflight(&task,assistant);
    if (status==PANGU_OK && !task.immediate) status=execute(&task);
    if (status==PANGU_OK) status=await_tasks(&task,1);
    if (task.started) task.tool->release(task.tool->ctx,task.operation);
    if (status==PANGU_OK) {
        compact(&task.r);
        out->result=task.r.scratch; task.r.scratch=NULL;
        out->has_duration=task.r.has_duration; out->duration_ms=task.r.duration_ms;
    }
    if (task.owned_args) c->memory.free(c->memory.ctx,task.owned_args);
    if (task.owned_before_reason) c->memory.free(c->memory.ctx,task.owned_before_reason);
    pangu_core_destroy(task.r.scratch);
    if(c->failure_detail.data)c->memory.free(c->memory.ctx,(void *)c->failure_detail.data);
    c->failure_detail=inherited_detail;
    if (own_run) c->running=0;
    return status;
}
static pangu_status drain(pangu_core *c,pangu_queue_kind kind,pangu_core *pending) {
    pangu_core *q=c->queues[kind];
    if (!q || !q->count) return PANGU_OK;
    size_t count=c->queue_modes[kind]==PANGU_QUEUE_ALL?q->count:1;
    pangu_status s=pangu_core_move_prefix(q,pending,count);
    if (s==PANGU_OK) c->held_kind=kind;
    return s;
}
static pangu_status pending_artifacts(receiver *r,pangu_core *pending) {
    while (pending->count) {
        pangu_status s=pangu_core_append_internal(r->generated,pending->messages[0]);
        if (s!=PANGU_OK) return s;
        s=pangu_core_move_prefix(pending,r->core,1);
        if (s!=PANGU_OK) {
            pangu_core *g=r->generated;
            g->memory.free(g->memory.ctx,g->messages[--g->count]); return s;
        }
        const pangu_message *m=r->core->messages[r->core->count-1];
        emit(r,PANGU_MESSAGE_START,m); emit(r,PANGU_MESSAGE_END,m);
    }
    return PANGU_OK;
}
typedef struct {
    pangu_core *owner;
    char *bytes;
    size_t len;
    int submitted;
    pangu_status failure;
} resolved_key;
static pangu_status copy_api_key(void *ctx,pangu_slice key) {
    resolved_key *k=ctx;
    if (k->failure!=PANGU_OK) return k->failure;
    if (k->submitted || (key.len && !key.data)) return k->failure=PANGU_INVALID;
    k->submitted=1;
    if (key.len) {
        k->bytes=k->owner->memory.alloc(k->owner->memory.ctx,key.len);
        if (!k->bytes) return k->failure=PANGU_NOMEM;
        memcpy(k->bytes,key.data,key.len); k->len=key.len;
    }
    return PANGU_OK;
}
static void release_api_key(resolved_key *key) {
    if (!key->bytes) return;
    volatile unsigned char *bytes=(volatile unsigned char *)key->bytes;
    for (size_t i=0;i<key->len;++i) bytes[i]=0;
    key->owner->memory.free(key->owner->memory.ctx,key->bytes);
}
static pangu_status request_model(receiver *r,const char **preparation_error) {
    const pangu_loop_config *cfg=r->config;
    pangu_core transformed={.memory=r->core->memory},converted={.memory=r->core->memory};
    resolved_key key={.owner=r->core};
    const pangu_core *input=r->core;
    int modern=cfg->request || cfg->request_events;
    pangu_status status=PANGU_OK;
    if (modern) {
        if (cfg->transform_context) {
            status=cfg->transform_context(cfg->ctx,r->core,input,&transformed);
            if (status!=PANGU_OK) { *preparation_error="Context transform failed"; goto done; }
            input=&transformed;
        }
        if (cfg->convert_to_model) {
            status=cfg->convert_to_model(cfg->ctx,r->core,input,&converted);
            if (status!=PANGU_OK) *preparation_error="Message conversion failed";
        }
        else for (size_t i=0;i<input->count;++i) if (input->messages[i]->role!=PANGU_CUSTOM) {
            status=pangu_core_append(&converted,input->messages[i]);
            if (status!=PANGU_OK) break;
        }
    }
    if (status!=PANGU_OK) goto done;
    if (cfg->get_api_key) {
        status=cfg->get_api_key(cfg->ctx,r->core,cfg->model_config.provider,copy_api_key,&key);
        if (key.failure!=PANGU_OK) status=key.failure;
        if (status!=PANGU_OK) { *preparation_error="API key resolution failed"; goto done; }
    }
    r->core->active_api_key=key.len?(pangu_slice){key.bytes,key.len}:cfg->api_key;
    if (!modern) status=cfg->model(cfg->ctx,r->core,receive,r);
    else {
        void *ctx=cfg->has_request_ctx?cfg->request_ctx:cfg->ctx;
        status=cfg->request_events?cfg->request_events(ctx,r->core,&converted,receive_event,r):
            cfg->request(ctx,r->core,&converted,receive,r);
    }
done:
    r->core->active_api_key=(pangu_slice){0};
    release_api_key(&key);
    pangu_core_clear(&transformed); pangu_core_clear(&converted);
    if (transformed.messages) r->core->memory.free(r->core->memory.ctx,transformed.messages);
    if (converted.messages) r->core->memory.free(r->core->memory.ctx,converted.messages);
    return status;
}

void pangu_core_cancel(pangu_core *c) { if (c && c->running) c->cancelled=1; }
int pangu_core_is_cancelled(const pangu_core *c) { return c && c->cancelled; }
int pangu_core_is_running(const pangu_core *c) { return c && c->running; }
typedef struct {
    pangu_core copy, additions, declarations, model;
    pangu_tool *tools;
    int allow_messages;
    pangu_request_update update;
    pangu_status failure;
    int submitted;
} request_update;
static pangu_status receive_update(void *ctx,const pangu_request_update *update) {
    request_update *u=ctx;
    if (u->failure!=PANGU_OK) return u->failure;
    if (!update || u->submitted || (update->fields & ~63u) ||
        ((update->fields&PANGU_UPDATE_THINKING) && (unsigned)update->thinking_level>PANGU_THINKING_MAX) ||
        ((update->fields&PANGU_UPDATE_TOOLS) && update->tool_count && !update->tools) ||
        ((update->fields&PANGU_UPDATE_MESSAGES) && (!u->allow_messages || !update->messages)) ||
        ((update->fields&PANGU_UPDATE_CONTEXT) && !update->context) ||
        ((update->fields&PANGU_UPDATE_PROVIDER) && !update->request && !update->request_events)) return u->failure=PANGU_INVALID;
    u->submitted=1; u->update=*update;
    if (update->fields&PANGU_UPDATE_MODEL) {
        pangu_message m={.role=PANGU_SYSTEM,.model=update->model.id,.provider=update->model.provider,
            .api=update->model.api,.structured_json=update->model.metadata_json};
        pangu_status status=pangu_core_append_internal(&u->model,&m);
        if (status!=PANGU_OK) return u->failure=status;
    }
    if (update->fields&PANGU_UPDATE_TOOLS) {
        if (update->tool_count>SIZE_MAX/sizeof(*u->tools)) return u->failure=PANGU_NOMEM;
        if (update->tool_count) {
            u->tools=u->copy.memory.alloc(u->copy.memory.ctx,update->tool_count*sizeof(*u->tools));
            if (!u->tools) return u->failure=PANGU_NOMEM;
        }
        for (size_t i=0;i<update->tool_count;++i) {
            const pangu_tool *tool=&update->tools[i];
            pangu_tool_declaration declaration={.name=tool->name,.description=tool->description,.schema_json=tool->schema_json};
            pangu_message m={.role=PANGU_SYSTEM,.tools_added=&declaration,.tools_added_count=1};
            pangu_status s=pangu_core_append_internal(&u->declarations,&m);
            if (s!=PANGU_OK) return u->failure=s;
            const pangu_tool_declaration *saved=u->declarations.messages[i]->tools_added;
            u->tools[i]=*tool; u->tools[i].name=saved->name;
            u->tools[i].description=saved->description; u->tools[i].schema_json=saved->schema_json;
        }
    }
    if (update->fields&PANGU_UPDATE_MESSAGES) for (size_t i=0;i<update->messages->count;++i) {
        pangu_status s=pangu_core_append_internal(&u->additions,update->messages->messages[i]);
        if (s!=PANGU_OK) return u->failure=s;
    }
    if (update->fields&PANGU_UPDATE_CONTEXT) for (size_t i=0;i<update->context->count;++i) {
        pangu_status s=pangu_core_append_internal(&u->copy,update->context->messages[i]);
        if (s!=PANGU_OK) return u->failure=s;
    }
    return PANGU_OK;
}
static pangu_status prepare_update(pangu_core *c,pangu_loop_config *config,pangu_prepare_update_fn hook,pangu_core *additions,pangu_core *declarations,pangu_tool **owned_tools,pangu_core *model) {
    request_update u={.copy={.memory=c->memory},.additions={.memory=c->memory},.declarations={.memory=c->memory},.model={.memory=c->memory},.allow_messages=additions!=NULL};
    pangu_status s=hook(config->ctx,c,receive_update,&u);
    if (u.failure!=PANGU_OK) s=u.failure;
    if (s==PANGU_OK && u.submitted && additions)
        s=pangu_core_move_prefix(&u.additions,additions,u.additions.count);
    if (s==PANGU_OK && u.submitted) {
        if (u.update.fields&PANGU_UPDATE_CONTEXT) {
            pangu_message **old=c->messages; size_t old_count=c->count,old_capacity=c->capacity;
            c->messages=u.copy.messages; c->count=u.copy.count; c->capacity=u.copy.capacity;
            u.copy.messages=old; u.copy.count=old_count; u.copy.capacity=old_capacity;
            c->streaming=NULL; c->error=(pangu_slice){0};
        }
        if (u.update.fields&PANGU_UPDATE_MODEL) {
            pangu_core swap=*model; *model=u.model; u.model=swap;
            const pangu_message *m=model->messages[0];
            config->model_config=(pangu_model_config){m->model,m->provider,m->api,m->structured_json};
        }
        if (u.update.fields&PANGU_UPDATE_THINKING) config->thinking_level=u.update.thinking_level;
        if (u.update.fields&PANGU_UPDATE_TOOLS) {
            pangu_core swap=*declarations; *declarations=u.declarations; u.declarations=swap;
            pangu_tool *old=*owned_tools; *owned_tools=u.tools; u.tools=old;
            config->tools=*owned_tools; config->tool_count=u.update.tool_count;
        }
        if (u.update.fields&PANGU_UPDATE_PROVIDER) {
            config->request=u.update.request; config->request_events=u.update.request_events; config->request_ctx=u.update.request_ctx;
            config->has_request_ctx=1; config->model=NULL;
        }
    }
    pangu_core_clear(&u.model);
    if (u.model.messages) c->memory.free(c->memory.ctx,u.model.messages);
    pangu_core_clear(&u.declarations);
    if (u.declarations.messages) c->memory.free(c->memory.ctx,u.declarations.messages);
    if (u.tools) c->memory.free(c->memory.ctx,u.tools);
    pangu_core_clear(&u.additions);
    if (u.additions.messages) c->memory.free(c->memory.ctx,u.additions.messages);
    pangu_core_clear(&u.copy);
    if (u.copy.messages) c->memory.free(c->memory.ctx,u.copy.messages);
    return s;
}
static const pangu_slice default_categories[]={{"cli-agent",9}};
static const pangu_client_identity default_identity={
    .fields=PANGU_IDENTITY_ATTRIBUTION,.attribution_enabled=1,
    .name={"PanGu",5},.categories=default_categories,.category_count=1
};
const pangu_client_identity *pangu_core_default_identity(void) { return &default_identity; }
const pangu_client_identity *pangu_core_active_identity(const pangu_core *c) {
    return c && c->active_config?&c->active_config->identity:NULL;
}
static int identity_slice_valid(pangu_slice s) { return !s.len || s.data; }
static int identity_valid(const pangu_client_identity *i) {
    if (i->fields & ~(PANGU_IDENTITY_USER_AGENT|PANGU_IDENTITY_ATTRIBUTION|PANGU_IDENTITY_VERSION)) return 0;
    if ((i->fields&PANGU_IDENTITY_USER_AGENT) &&
        ((unsigned)i->user_agent>PANGU_USER_AGENT_CUSTOM || !identity_slice_valid(i->custom_user_agent))) return 0;
    if ((i->fields&PANGU_IDENTITY_VERSION) && !identity_slice_valid(i->version)) return 0;
    if ((i->fields&PANGU_IDENTITY_ATTRIBUTION) && i->attribution_enabled) {
        if (!identity_slice_valid(i->name) || !identity_slice_valid(i->url) ||
            (i->category_count && !i->categories)) return 0;
        for (size_t n=0;n<i->category_count;++n) if (!identity_slice_valid(i->categories[n])) return 0;
    }
    return 1;
}
pangu_slice pangu_core_active_api_key(const pangu_core *c) { return c?c->active_api_key:(pangu_slice){0}; }
pangu_slice pangu_core_active_options(const pangu_core *c) {
    return c && c->active_config?c->active_config->request_options_json:(pangu_slice){0};
}
pangu_status pangu_core_observe_provider_event(pangu_core *c,pangu_slice event_json) {
    if (!c || !c->active_config || (event_json.len && !event_json.data)) return PANGU_INVALID;
    const pangu_loop_config *cfg=c->active_config;
    return cfg->on_provider_event?cfg->on_provider_event(cfg->ctx,c,&cfg->model_config,event_json):PANGU_OK;
}
const pangu_model_config *pangu_core_active_model(const pangu_core *c) {
    return c && c->active_config?&c->active_config->model_config:NULL;
}
size_t pangu_core_active_tool_count(const pangu_core *c) {
    return c && c->active_config?c->active_config->tool_count:0;
}
const pangu_tool *pangu_core_active_tool_at(const pangu_core *c,size_t index) {
    return c && c->active_config && index<c->active_config->tool_count?&c->active_config->tools[index]:NULL;
}
pangu_thinking_level pangu_core_active_thinking(const pangu_core *c) {
    return c && c->active_config?c->active_config->thinking_level:PANGU_THINKING_OFF;
}
pangu_status pangu_core_set_default_stream(pangu_core *c,pangu_request_fn fn,void *ctx) {
    if (!c) return PANGU_INVALID;
    if (c->running) return PANGU_BUSY;
    c->default_request=fn; c->default_request_ctx=fn?ctx:NULL;
    return PANGU_OK;
}
static pangu_status run(pangu_core *c, const pangu_message *prompts, size_t count,
    const pangu_loop_config *cfg,int entry) {
    if (!c || !cfg) return PANGU_INVALID;
    if ((cfg->api_key.len && !cfg->api_key.data) || !identity_valid(&cfg->identity) || (cfg->request_options_json.len && !cfg->request_options_json.data)) return PANGU_INVALID;
    if (!identity_slice_valid(cfg->model_config.id) ||
        !identity_slice_valid(cfg->model_config.provider) ||
        !identity_slice_valid(cfg->model_config.api) ||
        !identity_slice_valid(cfg->model_config.metadata_json)) return PANGU_INVALID;
    pangu_loop_config resolved=*cfg;
    if(cfg->override_fields & ~7u)return PANGU_INVALID;
    if(!entry) {
        if(!(cfg->override_fields&PANGU_RUN_OVERRIDE_MODEL) &&
           !cfg->model_config.id.len && !cfg->model_config.provider.len &&
           !cfg->model_config.api.len && !cfg->model_config.metadata_json.len)
            resolved.model_config=*pangu_core_model(c);
        if(!(cfg->override_fields&PANGU_RUN_OVERRIDE_THINKING) && cfg->thinking_level==PANGU_THINKING_OFF)
            resolved.thinking_level=pangu_core_thinking(c);
        if(!(cfg->override_fields&PANGU_RUN_OVERRIDE_TOOLS) && !cfg->tools && !cfg->tool_count) {
            resolved.tools=pangu_core_tool_at(c,0);
            resolved.tool_count=pangu_core_tool_count(c);
        }
    }
    if (!(resolved.identity.fields&PANGU_IDENTITY_ATTRIBUTION)) {
        resolved.identity.attribution_enabled=1;
        resolved.identity.name=default_identity.name;
        resolved.identity.url=default_identity.url;
        resolved.identity.categories=default_identity.categories;
        resolved.identity.category_count=default_identity.category_count;
        resolved.identity.fields|=PANGU_IDENTITY_ATTRIBUTION;
    }
    if (!resolved.model && !resolved.request && !resolved.request_events && c->default_request) {
        resolved.request=c->default_request;
        resolved.has_request_ctx=1; resolved.request_ctx=c->default_request_ctx;
    }
    cfg=&resolved;
    if ((unsigned)cfg->thinking_level>PANGU_THINKING_MAX) return PANGU_INVALID;
    if ((!cfg->model && !cfg->request && !cfg->request_events) || ((cfg->transform_context || cfg->convert_to_model) && !cfg->request && !cfg->request_events) || (count && !prompts) || (cfg->tool_count && !cfg->tools)) return PANGU_INVALID;
    if (c->running) return PANGU_BUSY;
    for (size_t i=0;i<cfg->tool_count;++i) {
        const pangu_tool *t=&cfg->tools[i];
        if ((t->name.len && !t->name.data) || (t->description.len && !t->description.data) ||
            (t->schema_json.len && !t->schema_json.data)) return PANGU_INVALID;
    }
    int assistant_resume=!entry && !count && c->count && c->messages[c->count-1]->role==PANGU_ASSISTANT;
    if (entry==2 && (!c->count || c->messages[c->count-1]->role==PANGU_ASSISTANT)) return PANGU_INVALID;
    if (!entry && !count) {
        int conversational=0;
        for (size_t i=0;i<c->count;++i) conversational |= c->messages[i]->role!=PANGU_SYSTEM;
        if (!conversational || (assistant_resume && !c->prepared_count && !pangu_core_queue_count(c,PANGU_STEERING) &&
            !pangu_core_queue_count(c,PANGU_FOLLOW_UP))) return PANGU_INVALID;
    }
    c->running=1; c->active_config=cfg; c->cancelled=0; c->error=(pangu_slice){0};
    if(c->retained_error)c->memory.free(c->memory.ctx,c->retained_error);
    c->retained_error=NULL;
    pangu_core pending={.memory=c->memory,.messages=c->held_messages,
        .count=c->held_count,.capacity=c->held_capacity};
    c->held_messages=NULL; c->held_count=c->held_capacity=0;
    pangu_core initial={.memory=c->memory};
    pangu_core generated={.memory=c->memory},prepared={.memory=c->memory,.messages=c->prepared_messages,
        .count=c->prepared_count,.capacity=c->prepared_capacity},tool_declarations={.memory=c->memory},model_state={.memory=c->memory};
    c->prepared_messages=NULL; c->prepared_count=c->prepared_capacity=0;
    pangu_tool *owned_tools=NULL;
    receiver r={.core=c,.config=cfg,.generated=&generated};
    pangu_status status=PANGU_OK;
    int preparation_failed=0;
    const char *preparation_error="Request preparation failed";
    emit(&r,PANGU_AGENT_START,NULL);
    emit(&r,PANGU_TURN_START,NULL);
    for (size_t i=0;i<count;++i) {
        status=pangu_core_append_internal(&initial,&prompts[i]); if (status!=PANGU_OK) goto done;
    }
    if (count || entry==1) {
        status=pangu_core_declare_tools(c,&initial,cfg->tools,cfg->tool_count); if (status!=PANGU_OK) goto done;
        status=pending_artifacts(&r,&initial); if (status!=PANGU_OK) goto done;
    }
    int skip_initial_steer=0;
    if (assistant_resume && !prepared.count) {
        pangu_queue_kind kind=pangu_core_queue_count(c,PANGU_STEERING)?PANGU_STEERING:PANGU_FOLLOW_UP;
        if (pending.count) kind=c->held_kind;
        else { status=drain(c,kind,&pending); if (status!=PANGU_OK) goto done; }
        skip_initial_steer=kind==PANGU_STEERING;
        status=pending_artifacts(&r,&pending); if (status!=PANGU_OK) goto done;
    }
    if (!skip_initial_steer && !pending.count) { status=drain(c,PANGU_STEERING,&pending); if (status!=PANGU_OK) goto done; }
    for (;;) {
        if (prepared.count) {
            /* Prepared messages precede selected queued input. */
            status=pangu_core_move_prefix(&pending,&prepared,pending.count); if (status!=PANGU_OK) goto done;
            pangu_core swap=pending; pending=prepared; prepared=swap;
        }
        status=pangu_core_declare_tools(c,&pending,cfg->tools,cfg->tool_count); if (status!=PANGU_OK) goto done;
        status=pending_artifacts(&r,&pending); if (status!=PANGU_OK) goto done;
        if (cfg->prepare_request_update) {
            status=prepare_update(c,&resolved,cfg->prepare_request_update,NULL,&tool_declarations,&owned_tools,&model_state); if (status!=PANGU_OK) { preparation_failed=1; goto done; }
        } else if (cfg->prepare_request) {
            status=cfg->prepare_request(cfg->ctx,c); if (status!=PANGU_OK) { preparation_failed=1; goto done; }
        }
        r.started=r.final=0; r.failure=PANGU_OK; r.call=NULL;
        status=scratch(&r); if (status!=PANGU_OK) goto done;
        const char *request_preparation_error=NULL;
        status=request_model(&r,&request_preparation_error);
        if (request_preparation_error) { preparation_failed=1; preparation_error=request_preparation_error; goto done; }
        if (r.failure!=PANGU_OK) { status=r.failure; goto done; }
        if (status==PANGU_NOMEM) goto done;
        if (status!=PANGU_OK || !r.final || c->cancelled) {
            const pangu_message *partial=last(&r);
            int aborted=c->cancelled || status==PANGU_CANCELLED;
            /* Stream failures preserve the provider's accumulated message.
             * In particular, proxy ABORTED/ERROR terminals carry useful text,
             * usage and diagnostics and must not become a generic artifact. */
            if (!(r.final && partial && (partial->stop_reason==PANGU_ERROR || partial->stop_reason==PANGU_ABORTED))) {
                pangu_content b;
                pangu_message m=partial?*partial:error_message(PANGU_ASSISTANT,aborted?"Operation aborted":"Model stream failed",&b);
                if (!partial) {
                    m.model=cfg->model_config.id;
                    m.provider=cfg->model_config.provider;
                    m.api=cfg->model_config.api;
                }
                m.stop_reason=aborted?PANGU_ABORTED:PANGU_ERROR;
                if(c->failure_detail.len) {
                    m.error=c->failure_detail;
                    if(!partial)b.data=(pangu_slice){0};
                }
                if (!m.error.len) m.error=str(aborted?"Operation aborted":"Model stream failed");
                r.final=0;
                r.skip_thinking=1;
                status=receive(&r,&m,1);r.skip_thinking=0;
                if (status!=PANGU_OK) goto done;
            }
        }
        status=commit(&r,last(&r)); if (status!=PANGU_OK) goto done;
        const pangu_message *assistant=c->messages[c->count-1];
        emit(&r,PANGU_MESSAGE_END,assistant);
        destroy_scratch(&r);
        int hard=assistant->stop_reason==PANGU_ERROR || assistant->stop_reason==PANGU_ABORTED;
        size_t results_begin=c->count;
        size_t calls=0; int all_terminate=1;
        if (!hard) {
            status=batch(&r,assistant,&calls,&all_terminate);
            if (status!=PANGU_OK) goto done;
        }
        r.call=NULL;
        pangu_core results={.messages=c->messages+results_begin,.count=c->count-results_begin};
        pangu_turn_context turn={.assistant=assistant,.tool_results=&results,.new_messages=&generated};
        r.turn_results=&results;
        pangu_turn_decision d=cfg->finish_turn_context?cfg->finish_turn_context(cfg->ctx,c,&turn):
            cfg->finish_turn?cfg->finish_turn(cfg->ctx,c,assistant):PANGU_TURN_AUTO;
        emit(&r,PANGU_TURN_END,assistant);
        r.turn_results=NULL;
        if (hard) { status=assistant->stop_reason==PANGU_ABORTED?PANGU_CANCELLED:PANGU_MODEL_ERROR; break; }
        if (d==PANGU_TURN_END_RUN) break;
        status=drain(c,PANGU_STEERING,&pending); if (status!=PANGU_OK) goto done;
        int natural=(calls && !all_terminate) || pending.count;
        if (!natural) { status=drain(c,PANGU_FOLLOW_UP,&pending); if (status!=PANGU_OK) goto done; }
        if (!natural && !pending.count && d!=PANGU_TURN_CONTINUE) break;
        if (cfg->prepare_next_turn_update) {
            status=prepare_update(c,&resolved,cfg->prepare_next_turn_update,&prepared,&tool_declarations,&owned_tools,&model_state); if (status!=PANGU_OK) { preparation_failed=1; goto done; }
        } else if (cfg->prepare_next_turn) {
            status=cfg->prepare_next_turn(cfg->ctx,c); if (status!=PANGU_OK) { preparation_failed=1; goto done; }
        }
        if (!pending.count) { status=drain(c,PANGU_STEERING,&pending); if (status!=PANGU_OK) goto done; }
        emit(&r,PANGU_TURN_START,NULL);
    }
done:;
    pangu_message *failed_message=NULL;
    pangu_core failure_view={.memory=c->memory},empty_results={.memory=c->memory};
    if (!entry && preparation_failed && status!=PANGU_NOMEM) {
        int aborted=c->cancelled || status==PANGU_CANCELLED;
        pangu_content text={.kind=PANGU_TEXT};
        pangu_message failure={.role=PANGU_ASSISTANT,.content=&text,.content_count=1,
            .timestamp_ms=pangu_core_wallclock_ms(),
            .model=cfg->model_config.id,.provider=cfg->model_config.provider,.api=cfg->model_config.api,
            .stop_reason=aborted?PANGU_ABORTED:PANGU_ERROR,
            .error=c->failure_detail.len?c->failure_detail:str(aborted?"Operation aborted":preparation_error)};
        pangu_status saved=commit(&r,&failure);
        if (saved==PANGU_OK) {
            failed_message=c->messages[c->count-1];
            failure_view.messages=&failed_message; failure_view.count=1;
            r.call=NULL; r.assistant_event=NULL;
            emit(&r,PANGU_MESSAGE_START,failed_message);
            emit(&r,PANGU_MESSAGE_END,failed_message);
            r.turn_results=&empty_results;
            emit(&r,PANGU_TURN_END,failed_message);
            r.turn_results=NULL;
            if (c->cancelled) status=PANGU_CANCELLED;
        } else status=saved;
    }
    c->prepared_messages=prepared.messages;
    c->prepared_count=prepared.count; c->prepared_capacity=prepared.capacity;
    pangu_core_clear(&initial);
    if (initial.messages) c->memory.free(c->memory.ctx,initial.messages);
    c->streaming=NULL;
    destroy_scratch(&r);
    /* Keep selected but uncommitted inputs owned across allocation/hook
     * failures. They remain at the front of the originating queue when idle. */
    c->held_messages=pending.messages;
    c->held_count=pending.count; c->held_capacity=pending.capacity;
    pangu_event e={.kind=PANGU_AGENT_END,.first_message=0,.message_count=failed_message?1:generated.count,
        .new_messages=failed_message?&failure_view:&generated};
    if (cfg->event) cfg->event(cfg->ctx,c,&e);
    pangu_core_dispatch(c,&e);
    c->active_config=NULL;
    pangu_core_clear(&tool_declarations);
    if (tool_declarations.messages) c->memory.free(c->memory.ctx,tool_declarations.messages);
    if (owned_tools) c->memory.free(c->memory.ctx,owned_tools);
    pangu_core_clear(&model_state);
    if (model_state.messages) c->memory.free(c->memory.ctx,model_state.messages);
    pangu_core_clear(&generated);
    if (generated.messages) c->memory.free(c->memory.ctx,generated.messages);
    if(c->failure_detail.data)c->memory.free(c->memory.ctx,(void *)c->failure_detail.data);
    c->failure_detail=(pangu_slice){0};
    c->running=0;
    return status;
}

pangu_status pangu_core_run(pangu_core *c,const pangu_message *prompts,size_t count,const pangu_loop_config *cfg) {
    return run(c,prompts,count,cfg,0);
}
pangu_status pangu_core_loop(pangu_core *c,const pangu_message *prompts,size_t count,const pangu_loop_config *cfg) {
    return run(c,prompts,count,cfg,1);
}
pangu_status pangu_core_loop_continue(pangu_core *c,const pangu_loop_config *cfg) {
    return run(c,NULL,0,cfg,2);
}
