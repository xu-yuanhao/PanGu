/* Pi d5b3de22 ai/utils/overflow.ts; PCRE2 preserves JS ASCII classes,
 * Unicode whitespace and non-dotall behavior. No pattern changes in references. */
#include <pangu/core/overflow.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdlib.h>
#include <string.h>
static const char *const patterns[]={
    "prompt (?:is )?too long",
    "prompt exceeds max length",
    "request_too_large",
    "input is too long for requested model",
    "exceeds the context window",
    "exceeds (?:the )?(?:model'?s )?maximum context length(?: of [\\d,]+ tokens?|[\\x{9}-\\x{D}\\x{20}\\x{A0}\\x{1680}\\x{2000}-\\x{200A}\\x{2028}\\x{2029}\\x{202F}\\x{205F}\\x{3000}\\x{FEFF}]*\\([\\d,]+\\))",
    "input token count[^\\r\\n\\x{2028}\\x{2029}]*exceeds the maximum",
    "maximum prompt length is \\d+",
    "reduce the length of the messages",
    "maximum context length is \\d+ tokens",
    "exceeds (?:the )?maximum allowed input length of [\\d,]+ tokens?",
    "input \\(\\d+ tokens\\) is longer than the model'?s context length \\(\\d+ tokens\\)",
    "exceeds the limit of \\d+",
    "exceeds the available context size",
    "greater than the context length",
    "context window exceeds limit",
    "exceeded model token limit",
    "too large for model with \\d+ maximum context length",
    "prompt has [\\d,]+ tokens?, but the configured context size is [\\d,]+ tokens?",
    "model_context_window_exceeded",
    "prompt too long; exceeded (?:max )?context length",
    "range of input length should be",
    "context[_ ]length[_ ]exceeded",
    "too many tokens",
    "token limit exceeded",
    "^(?:[\\w -]+ API error \\(413\\):|413(?:[\\x{9}-\\x{D}\\x{20}\\x{A0}\\x{1680}\\x{2000}-\\x{200A}\\x{2028}\\x{2029}\\x{202F}\\x{205F}\\x{3000}\\x{FEFF}]|:|$))",
    "^(Throttling error|Service unavailable):",
    "rate limit",
    "too many requests",
    "^4(?:00|13)[\\x{9}-\\x{D}\\x{20}\\x{A0}\\x{1680}\\x{2000}-\\x{200A}\\x{2028}\\x{2029}\\x{202F}\\x{205F}\\x{3000}\\x{FEFF}]*(?:status code)?[\\x{9}-\\x{D}\\x{20}\\x{A0}\\x{1680}\\x{2000}-\\x{200A}\\x{2028}\\x{2029}\\x{202F}\\x{205F}\\x{3000}\\x{FEFF}]*\\(no body\\)",
};
#define PATTERN_COUNT (sizeof(patterns)/sizeof(patterns[0]))
#define POSITIVE_COUNT 26u
struct pangu_overflow_classifier {
    pangu_allocator memory;
    pcre2_general_context *general;
    pcre2_code *code[PATTERN_COUNT];
};
static void *allocate_default(void *ctx,size_t n) { (void)ctx; return malloc(n); }
static void release_default(void *ctx,void *p) { (void)ctx; free(p); }
static void *allocate_regex(PCRE2_SIZE n,void *ctx) {
    pangu_overflow_classifier *c=ctx; return c->memory.alloc(c->memory.ctx,n);
}
static void release_regex(void *p,void *ctx) {
    pangu_overflow_classifier *c=ctx; if(p)c->memory.free(c->memory.ctx,p);
}
void pangu_overflow_destroy(pangu_overflow_classifier *c) {
    if(!c)return;
    for(size_t i=0;i<PATTERN_COUNT;++i)pcre2_code_free(c->code[i]);
    pcre2_general_context_free(c->general);
    c->memory.free(c->memory.ctx,c);
}
pangu_status pangu_overflow_create(const pangu_allocator *allocator,pangu_overflow_classifier **out) {
    if(!out)return PANGU_INVALID;
    *out=NULL;
    pangu_allocator memory=allocator?*allocator:(pangu_allocator){0};
    if(!!memory.alloc!=!!memory.free)return PANGU_INVALID;
    if(!memory.alloc)memory=(pangu_allocator){NULL,allocate_default,release_default};
    pangu_overflow_classifier *c=memory.alloc(memory.ctx,sizeof(*c));
    if(!c)return PANGU_NOMEM;
    memset(c,0,sizeof(*c));c->memory=memory;
    c->general=pcre2_general_context_create(allocate_regex,release_regex,c);
    if(!c->general){pangu_overflow_destroy(c);return PANGU_NOMEM;}
    pcre2_compile_context *compile=pcre2_compile_context_create(c->general);
    if(!compile){pangu_overflow_destroy(c);return PANGU_NOMEM;}
    pcre2_set_compile_extra_options(compile,PCRE2_EXTRA_CASELESS_RESTRICT);
    pangu_status status=PANGU_OK;
    for(size_t i=0;i<PATTERN_COUNT;++i){
        int error;PCRE2_SIZE offset;
        c->code[i]=pcre2_compile((PCRE2_SPTR)patterns[i],PCRE2_ZERO_TERMINATED,
            PCRE2_CASELESS|PCRE2_UTF,&error,&offset,compile);
        if(!c->code[i]){status=error==PCRE2_ERROR_HEAP_FAILED?PANGU_NOMEM:PANGU_INVALID;break;}
    }
    pcre2_compile_context_free(compile);
    if(status!=PANGU_OK){pangu_overflow_destroy(c);return status;}
    *out=c;return PANGU_OK;
}
static pangu_status matches(pangu_overflow_classifier *c,pcre2_match_data *data,size_t index,pangu_slice input,int *out) {
    int result=pcre2_match(c->code[index],(PCRE2_SPTR)input.data,input.len,0,0,data,NULL);
    if(result==PCRE2_ERROR_NOMATCH){*out=0;return PANGU_OK;}
    if(result<0)return result==PCRE2_ERROR_NOMEMORY?PANGU_NOMEM:PANGU_INVALID;
    *out=1;return PANGU_OK;
}
pangu_status pangu_message_is_context_overflow(pangu_overflow_classifier *c,const pangu_message *m,uint64_t capacity,int *out) {
    if(!c || !m || !out || m->role!=PANGU_ASSISTANT || (unsigned)m->stop_reason>PANGU_DEFERRED ||
        (m->error.len && !m->error.data) || (m->provider.len && !m->provider.data))return PANGU_INVALID;
    if(m->stop_reason==PANGU_ERROR && m->error.len){
        pcre2_match_data *data=pcre2_match_data_create(1,c->general);
        if(!data)return PANGU_NOMEM;
        pangu_status status=PANGU_OK;int hit=0,excluded=0;
        for(size_t i=POSITIVE_COUNT;i<POSITIVE_COUNT+3;++i){
            status=matches(c,data,i,m->error,&excluded);
            if(status!=PANGU_OK || excluded)break;
        }
        if(status==PANGU_OK && !excluded){
            for(size_t i=0;i<POSITIVE_COUNT;++i){
                status=matches(c,data,i,m->error,&hit);
                if(status!=PANGU_OK || hit)break;
            }
            if(status==PANGU_OK && !hit && m->provider.len==8 && !memcmp(m->provider.data,"cerebras",8))
                status=matches(c,data,PATTERN_COUNT-1,m->error,&hit);
        }
        pcre2_match_data_free(data);
        if(status!=PANGU_OK)return status;
        if(hit){*out=1;return PANGU_OK;}
    }
    double input=(double)m->usage.input+(double)m->usage.cache_read;
    *out=capacity && ((m->stop_reason==PANGU_STOP && input>(double)capacity) ||
        (m->stop_reason==PANGU_LENGTH && !m->usage.output && input>=(double)capacity*0.99));
    return PANGU_OK;
}
int pangu_message_is_recoverable_length(const pangu_message *m,uint64_t desired) {
    return m && m->role==PANGU_ASSISTANT && m->stop_reason==PANGU_LENGTH && desired && (double)m->usage.output<(double)desired;
}
