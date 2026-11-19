/* Pi ai/utils/transcript.ts replay semantics, snapshot d5b3de22. */
#include "internal.h"
#include <pangu/core/transcript.h>
#include <string.h>
static int equal(pangu_slice a,pangu_slice b) { return a.len==b.len && (!a.len || !memcmp(a.data,b.data,a.len)); }
static int add(size_t *size,size_t n) { if (n>SIZE_MAX-*size) return 0; *size+=n; return 1; }
static pangu_status replay(const pangu_core *source,pangu_core *output) {
    size_t text_bytes=0,section_capacity=0,tool_capacity=0;
    int found=0; int64_t timestamp=0;
    for (size_t i=0;i<source->count;++i) {
        const pangu_message *m=source->messages[i]; if (m->role!=PANGU_SYSTEM) continue;
        if (!found) timestamp=m->timestamp_ms;
        found=1;
        if (!add(&section_capacity,m->section_count) || !add(&tool_capacity,m->tools_added_count)) return PANGU_NOMEM;
        size_t len=0,blocks=0;
        for (size_t j=0;j<m->content_count;++j) if (m->content[j].kind==PANGU_TEXT) {
            if ((blocks++ && !add(&len,1)) || !add(&len,m->content[j].data.len)) return PANGU_NOMEM;
        }
        if (len && ((text_bytes && !add(&text_bytes,2)) || !add(&text_bytes,len))) return PANGU_NOMEM;
    }
    if (!found) return PANGU_OK;
    if (section_capacity>SIZE_MAX/sizeof(pangu_system_section) || tool_capacity>SIZE_MAX/sizeof(pangu_tool_declaration)) return PANGU_NOMEM;
    pangu_allocator a=source->memory;
    char *text=text_bytes?a.alloc(a.ctx,text_bytes):NULL;
    pangu_system_section *sections=section_capacity?a.alloc(a.ctx,section_capacity*sizeof(*sections)):NULL;
    pangu_tool_declaration *tools=tool_capacity?a.alloc(a.ctx,tool_capacity*sizeof(*tools)):NULL;
    pangu_status status=PANGU_NOMEM;
    if ((text_bytes && !text) || (section_capacity && !sections) || (tool_capacity && !tools)) goto done;
    size_t text_len=0,section_count=0,tool_count=0;
    for (size_t i=0;i<source->count;++i) {
        const pangu_message *m=source->messages[i]; if (m->role!=PANGU_SYSTEM) continue;
        size_t len=0,blocks=0;
        for (size_t j=0;j<m->content_count;++j) if (m->content[j].kind==PANGU_TEXT) {
            if (blocks++) ++len;
            len+=m->content[j].data.len;
        }
        if (len) {
            if (text_len) { memcpy(text+text_len,"\n\n",2); text_len+=2; }
            blocks=0;
            for (size_t j=0;j<m->content_count;++j) if (m->content[j].kind==PANGU_TEXT) {
                if (blocks++) text[text_len++]='\n';
                pangu_slice v=m->content[j].data;
                if (v.len) memcpy(text+text_len,v.data,v.len);
                text_len+=v.len;
            }
        }
        for (size_t j=0;j<m->section_count;++j) {
            pangu_system_section v=m->sections[j]; size_t at=0;
            while (at<section_count && !equal(sections[at].name,v.name)) ++at;
            if (v.removed) {
                if (at<section_count) { --section_count; memmove(sections+at,sections+at+1,(section_count-at)*sizeof(*sections)); }
            } else { sections[at]=v; if (at==section_count) ++section_count; }
        }
        for (size_t j=0;j<m->tools_removed_count;++j) {
            size_t at=0; while (at<tool_count && !equal(tools[at].name,m->tools_removed[j])) ++at;
            if (at<tool_count) { --tool_count; memmove(tools+at,tools+at+1,(tool_count-at)*sizeof(*tools)); }
        }
        for (size_t j=0;j<m->tools_added_count;++j) {
            pangu_tool_declaration v=m->tools_added[j]; size_t at=0;
            while (at<tool_count && !equal(tools[at].name,v.name)) ++at;
            tools[at]=v; if (at==tool_count) ++tool_count;
        }
    }
    pangu_content block={.kind=PANGU_TEXT,.data={text,text_len}};
    pangu_message m={.role=PANGU_SYSTEM,.content=&block,.content_count=1,
        .sections=sections,.section_count=section_count,.tools_added=tools,.tools_added_count=tool_count,.timestamp_ms=timestamp,
        .presence=PANGU_MESSAGE_CONTENT_STRING};
    status=pangu_core_append_internal(output,&m);
done:
    if (text) a.free(a.ctx,text);
    if (sections) a.free(a.ctx,sections);
    if (tools) a.free(a.ctx,tools);
    return status;
}
pangu_status pangu_core_system_snapshot(const pangu_core *c,pangu_core **out) {
    if (!out) return PANGU_INVALID;
    *out=NULL; if (!c) return PANGU_INVALID;
    pangu_core_config cfg={.allocator=c->memory};
    pangu_core *snapshot=NULL;
    pangu_status s=pangu_core_create(&cfg,&snapshot); if (s!=PANGU_OK) return s;
    s=replay(c,snapshot);
    if (s!=PANGU_OK) pangu_core_destroy(snapshot); else *out=snapshot;
    return s;
}
pangu_status pangu_core_reset(pangu_core *c) {
    if (!c) return PANGU_INVALID;
    if (c->running) return PANGU_BUSY;
    pangu_core baseline={.memory=c->memory};
    pangu_status s=replay(c,&baseline);
    if (s!=PANGU_OK) return s;
    pangu_core_clear(c);
    if (c->messages) c->memory.free(c->memory.ctx,c->messages);
    c->messages=baseline.messages; c->count=baseline.count; c->capacity=baseline.capacity;
    return PANGU_OK;
}

#include "yyjson.h"
static void *json_alloc(void *ctx,size_t n) { pangu_allocator *a=ctx; return a->alloc(a->ctx,n); }
static void json_free(void *ctx,void *p) { pangu_allocator *a=ctx; if (p) a->free(a->ctx,p); }
static void *json_realloc(void *ctx,void *p,size_t old,size_t n) {
    pangu_allocator *a=ctx; void *next=a->alloc(a->ctx,n);
    if (!next) return NULL;
    if (p) { memcpy(next,p,old<n?old:n); a->free(a->ctx,p); }
    return next;
}
typedef struct { yyjson_val *key,*value; uint64_t index; int indexed; } json_entry;
static int index_key(yyjson_val *key,uint64_t *index) {
    const char *text=yyjson_get_str(key); size_t len=yyjson_get_len(key);
    if (!len || len>10 || (len>1 && text[0]=='0')) return 0;
    uint64_t n=0;
    for (size_t i=0;i<len;++i) { if (text[i]<'0' || text[i]>'9') return 0; n=n*10+(unsigned)(text[i]-'0'); }
    *index=n; return n<UINT32_MAX;
}
static int json_string_equal(yyjson_val *a,yyjson_val *b) {
    return yyjson_get_len(a)==yyjson_get_len(b) &&
        !memcmp(yyjson_get_str(a),yyjson_get_str(b),yyjson_get_len(a));
}
/* JSON.parse keeps the last duplicate value at the first key position;
 * JSON.stringify enumerates array-index keys before other string keys. */
static pangu_status entries(pangu_allocator *a,yyjson_val *object,json_entry **out,size_t *count) {
    size_t size=yyjson_obj_size(object); *out=NULL; *count=0;
    if (!size) return PANGU_OK;
    if (size>SIZE_MAX/sizeof(json_entry)) return PANGU_NOMEM;
    json_entry *list=a->alloc(a->ctx,size*sizeof(*list)); if (!list) return PANGU_NOMEM;
    yyjson_obj_iter iter=yyjson_obj_iter_with(object); yyjson_val *key;
    while ((key=yyjson_obj_iter_next(&iter))) {
        size_t at=0; while (at<*count && !json_string_equal(list[at].key,key)) ++at;
        if (at<*count) { list[at].value=yyjson_obj_iter_get_val(key); continue; }
        json_entry e={.key=key,.value=yyjson_obj_iter_get_val(key)};
        e.indexed=index_key(key,&e.index);
        at=*count;
        if (e.indexed) {
            at=0; while (at<*count && list[at].indexed && list[at].index<e.index) ++at;
            memmove(list+at+1,list+at,(*count-at)*sizeof(*list));
        }
        list[at]=e; ++*count;
    }
    *out=list; return PANGU_OK;
}
static pangu_status schema_equal(pangu_allocator *a,pangu_slice left,pangu_slice right,int *same) {
    yyjson_alc alc={json_alloc,json_realloc,json_free,a};
    yyjson_doc *docs[2]={NULL,NULL};
    pangu_slice inputs[2]={left,right}; pangu_status status=PANGU_OK;
    typedef struct { yyjson_val *a,*b; } pair;
    pair *stack=NULL; *same=0;
    for (size_t i=0;i<2;++i) {
        yyjson_read_err error;
        docs[i]=yyjson_read_opts((char *)inputs[i].data,inputs[i].len,0,&alc,&error);
        if (!docs[i]) { status=error.code==YYJSON_READ_ERROR_MEMORY_ALLOCATION?PANGU_NOMEM:PANGU_INVALID; goto done; }
    }
    size_t capacity=yyjson_doc_get_val_count(docs[0]);
    if (capacity>SIZE_MAX/sizeof(*stack)) { status=PANGU_NOMEM; goto done; }
    stack=a->alloc(a->ctx,capacity*sizeof(*stack)); if (!stack) { status=PANGU_NOMEM; goto done; }
    size_t used=1; stack[0]=(pair){yyjson_doc_get_root(docs[0]),yyjson_doc_get_root(docs[1])};
    while (used) {
        pair p=stack[--used];
        if (yyjson_is_num(p.a) && yyjson_is_num(p.b)) { if (yyjson_get_num(p.a)!=yyjson_get_num(p.b)) goto done; continue; }
        if (yyjson_get_type(p.a)!=yyjson_get_type(p.b)) goto done;
        if (yyjson_is_str(p.a)) { if (!json_string_equal(p.a,p.b)) goto done; }
        else if (yyjson_is_bool(p.a)) { if (yyjson_get_bool(p.a)!=yyjson_get_bool(p.b)) goto done; }
        else if (yyjson_is_arr(p.a)) {
            if (yyjson_arr_size(p.a)!=yyjson_arr_size(p.b)) goto done;
            yyjson_arr_iter ia=yyjson_arr_iter_with(p.a),ib=yyjson_arr_iter_with(p.b); yyjson_val *v;
            while ((v=yyjson_arr_iter_next(&ia))) stack[used++]=(pair){v,yyjson_arr_iter_next(&ib)};
        } else if (yyjson_is_obj(p.a)) {
            json_entry *ea=NULL,*eb=NULL; size_t na=0,nb=0;
            status=entries(a,p.a,&ea,&na);
            if (status==PANGU_OK) status=entries(a,p.b,&eb,&nb);
            int match=na==nb;
            if (status==PANGU_OK && match) for (size_t i=0;i<na;++i) {
                if (!json_string_equal(ea[i].key,eb[i].key)) { match=0; break; }
                stack[used++]=(pair){ea[i].value,eb[i].value};
            }
            if (ea) a->free(a->ctx,ea);
            if (eb) a->free(a->ctx,eb);
            if (status!=PANGU_OK || !match) goto done;
        }
    }
    *same=1;
done:
    if (stack) a->free(a->ctx,stack);
    for (size_t i=0;i<2;++i) yyjson_doc_free(docs[i]);
    return status;
}
static pangu_status declaration_equal(pangu_allocator *a,const pangu_tool_declaration *left,
    const pangu_tool_declaration *right,int *same) {
    *same=0;
    if (!equal(left->name,right->name) || !equal(left->description,right->description) ||
        left->has_constrained_sampling!=right->has_constrained_sampling ||
        (left->has_constrained_sampling && left->constrained_sampling!=right->constrained_sampling)) return PANGU_OK;
    return schema_equal(a,left->schema_json,right->schema_json,same);
}
pangu_status pangu_core_declare_tools(pangu_core *c,pangu_core *pending,const pangu_tool *tools,size_t count) {
    if (c->count>SIZE_MAX-pending->count || c->count+pending->count>SIZE_MAX/sizeof(pangu_message *)) return PANGU_NOMEM;
    pangu_allocator a=c->memory;
    size_t total=c->count+pending->count;
    pangu_message **messages=total?a.alloc(a.ctx,total*sizeof(*messages)):NULL;
    if (total && !messages) return PANGU_NOMEM;
    if (c->count) memcpy(messages,c->messages,c->count*sizeof(*messages));
    if (pending->count) memcpy(messages+c->count,pending->messages,pending->count*sizeof(*messages));
    size_t system_index=SIZE_MAX;
    for (size_t i=0;i<pending->count;++i) if (pending->messages[i]->role==PANGU_SYSTEM) system_index=i;
    pangu_message stripped={0};
    if (system_index!=SIZE_MAX) {
        stripped=*pending->messages[system_index];
        stripped.tools_added_count=stripped.tools_removed_count=0;
        stripped.presence&=~(PANGU_MESSAGE_HAS_TOOLS_ADDED|PANGU_MESSAGE_HAS_TOOLS_REMOVED);
        messages[c->count+system_index]=&stripped;
    }
    pangu_core virtual_context={.memory=a,.messages=messages,.count=total};
    pangu_core baseline={.memory=a};
    pangu_status status=replay(&virtual_context,&baseline);
    pangu_tool_declaration *added=NULL; pangu_slice *removed=NULL;
    if (status!=PANGU_OK) goto done;
    const pangu_message *head=baseline.count?baseline.messages[0]:NULL;
    size_t old_count=head?head->tools_added_count:0;
    if (count>SIZE_MAX/sizeof(*added) || old_count>SIZE_MAX/sizeof(*removed)) { status=PANGU_NOMEM; goto done; }
    added=count?a.alloc(a.ctx,count*sizeof(*added)):NULL;
    removed=old_count?a.alloc(a.ctx,old_count*sizeof(*removed)):NULL;
    if ((count && !added) || (old_count && !removed)) { status=PANGU_NOMEM; goto done; }
    size_t added_count=0,removed_count=0;
    for (size_t i=0;i<count;++i) {
        pangu_tool_declaration declaration={.name=tools[i].name,.description=tools[i].description,.schema_json=tools[i].schema_json,.has_constrained_sampling=tools[i].has_constrained_sampling,.constrained_sampling=tools[i].constrained_sampling};
        const pangu_tool_declaration *old=NULL;
        for (size_t j=0;j<old_count;++j) if (equal(head->tools_added[j].name,declaration.name)) old=&head->tools_added[j];
        int same=0;
        if (old) { status=declaration_equal(&a,old,&declaration,&same); if (status!=PANGU_OK) goto done; }
        if (!same) added[added_count++]=declaration;
    }
    for (size_t i=0;i<old_count;++i) {
        const pangu_tool_declaration *old=&head->tools_added[i];
        const pangu_tool *current=NULL;
        for (size_t j=0;j<count;++j) if (equal(tools[j].name,old->name)) current=&tools[j];
        int same=0;
        if (current) {
            pangu_tool_declaration declaration={.name=current->name,.description=current->description,.schema_json=current->schema_json,.has_constrained_sampling=current->has_constrained_sampling,.constrained_sampling=current->constrained_sampling};
            status=declaration_equal(&a,old,&declaration,&same); if (status!=PANGU_OK) goto done;
        }
        if (!same) removed[removed_count++]=old->name;
    }
    const pangu_message *original=system_index!=SIZE_MAX?pending->messages[system_index]:NULL;
    if (!added_count && !removed_count && (!original || (!original->tools_added_count && !original->tools_removed_count))) goto done;
    pangu_message update=original?*original:(pangu_message){.role=PANGU_SYSTEM,
        .timestamp_ms=pangu_core_wallclock_ms(),.presence=PANGU_MESSAGE_CONTENT_STRING};
    update.presence&=~(PANGU_MESSAGE_HAS_TOOLS_ADDED|PANGU_MESSAGE_HAS_TOOLS_REMOVED);
    update.tools_added=added; update.tools_added_count=added_count;
    update.tools_removed=removed; update.tools_removed_count=removed_count;
    if (original) {
        pangu_core replacement={.memory=a};
        status=pangu_core_append_internal(&replacement,&update);
        if (status==PANGU_OK) {
            pending->messages[system_index]=replacement.messages[0];
            a.free(a.ctx,(void *)original);
        }
        if (replacement.messages) a.free(a.ctx,replacement.messages);
    } else {
        size_t at=0; while (at<pending->count && pending->messages[at]->role==PANGU_SYSTEM) ++at;
        status=pangu_core_append_internal(pending,&update);
        if (status==PANGU_OK) {
            pangu_message *created=pending->messages[pending->count-1];
            memmove(pending->messages+at+1,pending->messages+at,(pending->count-at-1)*sizeof(*pending->messages));
            pending->messages[at]=created;
        }
    }
done:
    if (messages) a.free(a.ctx,messages);
    if (added) a.free(a.ctx,added);
    if (removed) a.free(a.ctx,removed);
    pangu_core_clear(&baseline);
    if (baseline.messages) a.free(a.ctx,baseline.messages);
    return status;
}
