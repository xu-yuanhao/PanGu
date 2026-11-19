/* Native message ownership for Pi agent-core semantics, snapshot d5b3de22.
 * See third-party/Pi-LICENSE for upstream attribution. */
#include <pangu/core/core.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "internal.h"
static void *allocate(void *ctx, size_t n) { (void)ctx; return malloc(n); }
static void release(void *ctx, void *p) { (void)ctx; free(p); }
static int valid(pangu_slice s) { return s.data || !s.len; }
static int add_size(size_t *n, size_t more) {
    if (more > SIZE_MAX - *n) return 0;
    *n += more; return 1;
}
static pangu_slice copy_slice(pangu_slice s, char **cursor) {
    /* An explicit empty optional string must survive ownership transfer.
     * The non-dereferenced end pointer belongs to the same owned allocation. */
    pangu_slice result = {s.data?*cursor:NULL, s.len};
    if (s.len) { result.data = *cursor; memcpy(*cursor, s.data, s.len); *cursor += s.len; }
    return result;
}
pangu_status pangu_core_create(const pangu_core_config *config, pangu_core **out) {
    if (!out) return PANGU_INVALID;
    *out = NULL;
    pangu_allocator a = config ? config->allocator : (pangu_allocator){0};
    if (!!a.alloc != !!a.free) return PANGU_INVALID;
    if (!a.alloc) a = (pangu_allocator){NULL, allocate, release};
    pangu_core *c = a.alloc(a.ctx, sizeof(*c));
    if (!c) return PANGU_NOMEM;
    *c = (pangu_core){.memory = a};
    pangu_core_state_init(c);
    *out = c; return PANGU_OK;
}
pangu_status pangu_core_clear(pangu_core *c) {
    if (!c) return PANGU_INVALID;
    if (c->running) return PANGU_BUSY;
    if(c->failure_detail.data)c->memory.free(c->memory.ctx,(void *)c->failure_detail.data);
    c->failure_detail=(pangu_slice){0};
    for (size_t i = 0; i < c->count; ++i) c->memory.free(c->memory.ctx, c->messages[i]);
    for (size_t i=0;i<c->held_count;++i) c->memory.free(c->memory.ctx,c->held_messages[i]);
    c->held_count=0;
    for (size_t i=0;i<c->prepared_count;++i) c->memory.free(c->memory.ctx,c->prepared_messages[i]);
    c->prepared_count=0;
    c->count = 0; c->streaming=NULL; c->error=(pangu_slice){0};
    if (c->retained_error) c->memory.free(c->memory.ctx,c->retained_error);
    c->retained_error=NULL;
    for (size_t i=0;i<2;++i) if (c->queues[i]) pangu_core_clear(c->queues[i]);
    return PANGU_OK;
}
void pangu_core_destroy(pangu_core *c) {
    if (!c || c->running) return;
    pangu_core_clear(c);
    pangu_core_state_destroy(c);
    if (c->prepared_messages) c->memory.free(c->memory.ctx,c->prepared_messages);
    if (c->held_messages) c->memory.free(c->memory.ctx,c->held_messages);
    if (c->messages) c->memory.free(c->memory.ctx, c->messages);
    c->memory.free(c->memory.ctx, c);
}
size_t pangu_core_message_count(const pangu_core *c) { return c ? c->count : 0; }
const pangu_message *pangu_core_message_at(const pangu_core *c, size_t i) {
    return c && i < c->count ? c->messages[i] : NULL;
}
pangu_status pangu_core_append_internal(pangu_core *c, const pangu_message *m) {
    if (!c || !m || (m->content_count && !m->content) ||
        (unsigned)m->role > PANGU_CUSTOM || (unsigned)m->stop_reason > PANGU_DEFERRED)
        return PANGU_INVALID;
    uint32_t allowed=m->role==PANGU_SYSTEM?60u:m->role==PANGU_USER?32u:
        m->role==PANGU_ASSISTANT?3u:m->role==PANGU_TOOL_RESULT?2u:0u;
    if(m->presence & ~allowed)return PANGU_INVALID;
    if(m->presence&PANGU_MESSAGE_CONTENT_STRING) {
        if(m->content_count>1 || (m->content_count &&
           (m->content[0].kind!=PANGU_TEXT || m->content[0].signature.data || m->content[0].signature.len)))return PANGU_INVALID;
    }
    if (m->content_count > (SIZE_MAX - sizeof(*m)) / sizeof(pangu_content)) return PANGU_INVALID;
    size_t bytes = sizeof(*m) + m->content_count * sizeof(pangu_content);
#define ARRAY_SIZE(field, count, type) \
    if ((m->count && !m->field) || m->count>(SIZE_MAX-bytes)/sizeof(type)) return PANGU_INVALID; \
    bytes+=m->count*sizeof(type)
    ARRAY_SIZE(sections,section_count,pangu_system_section);
    ARRAY_SIZE(tools_added,tools_added_count,pangu_tool_declaration);
    ARRAY_SIZE(tools_removed,tools_removed_count,pangu_slice);
#undef ARRAY_SIZE
    for (size_t i=0;i<m->section_count;++i) {
        if (!valid(m->sections[i].name) || !valid(m->sections[i].value) ||
            !add_size(&bytes,m->sections[i].name.len) || !add_size(&bytes,m->sections[i].value.len)) return PANGU_INVALID;
    }
    for (size_t i=0;i<m->tools_added_count;++i) {
        const pangu_tool_declaration *t=&m->tools_added[i];
        if (!valid(t->name) || !valid(t->description) || !valid(t->schema_json) ||
            !add_size(&bytes,t->name.len) || !add_size(&bytes,t->description.len) ||
            !add_size(&bytes,t->schema_json.len)) return PANGU_INVALID;
    }
    for (size_t i=0;i<m->tools_removed_count;++i)
        if (!valid(m->tools_removed[i]) || !add_size(&bytes,m->tools_removed[i].len)) return PANGU_INVALID;
    pangu_slice fields[] = {m->custom_role, m->tool_call_id, m->tool_name, m->model,
        m->provider, m->api, m->error, m->details_json, m->structured_json, m->provider_thinking_level,
        m->response_model, m->response_id, m->raw_stop_reason,
        m->diagnostics_json, m->deferred_json, m->nested_calls_json};
    for (size_t i = 0; i < sizeof(fields)/sizeof(fields[0]); ++i)
        if (!valid(fields[i]) || !add_size(&bytes, fields[i].len)) return PANGU_INVALID;
    for (size_t i = 0; i < m->content_count; ++i) {
        const pangu_content *b = &m->content[i];
        if ((unsigned)b->kind > PANGU_JSON) return PANGU_INVALID;
        pangu_slice f[] = {b->data, b->mime_type, b->id, b->name, b->signature, b->tool_namespace, b->arguments_error};
        for (size_t j = 0; j < sizeof(f)/sizeof(f[0]); ++j)
            if (!valid(f[j]) || !add_size(&bytes, f[j].len)) return PANGU_INVALID;
    }
    pangu_message *owned = c->memory.alloc(c->memory.ctx, bytes);
    if (!owned) return PANGU_NOMEM;
    *owned = *m;
    pangu_content *blocks = (pangu_content *)(owned + 1);
    pangu_system_section *sections=(pangu_system_section *)(blocks+m->content_count);
    pangu_tool_declaration *tools=(pangu_tool_declaration *)(sections+m->section_count);
    pangu_slice *removed=(pangu_slice *)(tools+m->tools_added_count);
    char *cursor=(char *)(removed+m->tools_removed_count);
    owned->sections=m->section_count?sections:NULL;
    owned->tools_added=m->tools_added_count?tools:NULL;
    owned->tools_removed=m->tools_removed_count?removed:NULL;
    for (size_t i=0;i<m->section_count;++i) {
        sections[i]=m->sections[i];
        sections[i].name=copy_slice(sections[i].name,&cursor);
        sections[i].value=copy_slice(sections[i].value,&cursor);
    }
    for (size_t i=0;i<m->tools_added_count;++i) {
        tools[i]=m->tools_added[i];
        tools[i].name=copy_slice(tools[i].name,&cursor);
        tools[i].description=copy_slice(tools[i].description,&cursor);
        tools[i].schema_json=copy_slice(tools[i].schema_json,&cursor);
    }
    for (size_t i=0;i<m->tools_removed_count;++i) removed[i]=copy_slice(m->tools_removed[i],&cursor);
    owned->content = m->content_count ? blocks : NULL;
#define COPY(field) owned->field = copy_slice(m->field, &cursor)
    COPY(custom_role); COPY(tool_call_id); COPY(tool_name); COPY(model); COPY(provider);
    COPY(api); COPY(error); COPY(details_json); COPY(structured_json); COPY(provider_thinking_level);
    COPY(response_model); COPY(response_id); COPY(raw_stop_reason);
    COPY(diagnostics_json); COPY(deferred_json); COPY(nested_calls_json);
#undef COPY
    for (size_t i = 0; i < m->content_count; ++i) {
        blocks[i] = m->content[i];
#define COPY(field) blocks[i].field = copy_slice(m->content[i].field, &cursor)
        COPY(data); COPY(mime_type); COPY(id); COPY(name); COPY(signature); COPY(tool_namespace); COPY(arguments_error);
#undef COPY
    }
    if (c->count == c->capacity) {
        size_t next = c->capacity ? c->capacity * 2 : 8;
        if (next < c->capacity || next > SIZE_MAX / sizeof(*c->messages)) {
            c->memory.free(c->memory.ctx, owned); return PANGU_NOMEM;
        }
        pangu_message **list = c->memory.alloc(c->memory.ctx, next * sizeof(*list));
        if (!list) { c->memory.free(c->memory.ctx, owned); return PANGU_NOMEM; }
        if (c->count) memcpy(list, c->messages, c->count * sizeof(*list));
        if (c->messages) c->memory.free(c->memory.ctx, c->messages);
        c->messages = list; c->capacity = next;
    }
    c->messages[c->count++] = owned;
    return PANGU_OK;
}

pangu_status pangu_core_append(pangu_core *c, const pangu_message *m) {
    if (c && c->running) return PANGU_BUSY;
    return pangu_core_append_internal(c, m);
}

pangu_status pangu_core_move_prefix(pangu_core *source,pangu_core *dest,size_t count) {
    if (!source || !dest || source==dest || count>source->count) return PANGU_INVALID;
    if (!count) return PANGU_OK;
    if (count>SIZE_MAX-dest->count) return PANGU_NOMEM;
    size_t next=dest->count+count;
    if (next>dest->capacity) {
        if (next>SIZE_MAX/sizeof(*dest->messages)) return PANGU_NOMEM;
        pangu_message **messages=dest->memory.alloc(dest->memory.ctx,next*sizeof(*messages));
        if (!messages) return PANGU_NOMEM;
        if (dest->count) memcpy(messages,dest->messages,dest->count*sizeof(*messages));
        if (dest->messages) dest->memory.free(dest->memory.ctx,dest->messages);
        dest->messages=messages; dest->capacity=next;
    }
    memcpy(dest->messages+dest->count,source->messages,count*sizeof(*dest->messages));
    dest->count+=count; source->count-=count;
    memmove(source->messages,source->messages+count,source->count*sizeof(*source->messages));
    return PANGU_OK;
}
