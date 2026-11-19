/* Native adaptation of Pi StreamingJsonParser (MIT), snapshot d5b3de22.
 * Each input byte enters the state machine once; a scalar delimiter is then
 * dispatched structurally. yyjson owns containers and the strict final parse.
 * Serialized snapshots charge the whole input size, rather than JS open-path
 * size: C byte slices cannot share completed strings/containers like JS values.
 */
#include "streaming_json.h"
#include "yyjson.h"
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>

typedef struct buffer { struct buffer *next; char *data; size_t len, cap; } buffer;
typedef struct { yyjson_mut_val *value, *key, *identity; unsigned phase; } frame;
typedef struct { yyjson_mut_val *parent, *key, *value; uint64_t hash; } entry;
typedef struct { yyjson_mut_val *parent, *key, *identity; int valid; } slot;
struct pangu_json_stream {
    pangu_allocator a; yyjson_alc alc; yyjson_mut_doc *doc;
    yyjson_mut_val *root, *string_value;
    frame *frames; size_t depth, frame_cap;
    entry *table; size_t table_cap, table_used;
    buffer raw, scalar, *strings, *string;
    slot scalar_slot;
    int in_string, key_string, done, dirty, failed;
    char escape[6]; unsigned escape_len;
    unsigned utf_left; char utf[4]; unsigned utf_len;
    size_t credit; char *snapshot, *final; size_t snapshot_len;
};
static void *alloc(void *ctx,size_t n) { pangu_json_stream *s=ctx; return s->a.alloc(s->a.ctx,n); }
static void release(void *ctx,void *p) { pangu_json_stream *s=ctx; if(p) s->a.free(s->a.ctx,p); }
static void *resize(void *ctx,void *p,size_t old,size_t n) {
    void *q=alloc(ctx,n); if(!q) return NULL;
    if(p) { memcpy(q,p,old<n?old:n); release(ctx,p); } return q;
}
static int reserve(pangu_json_stream *s,void **p,size_t *cap,size_t need,size_t unit) {
    if(need<=*cap) return 1;
    size_t n=*cap?*cap:32;
    while(n<need) { if(n>SIZE_MAX/2) {n=need;break;} n*=2; }
    if(n>SIZE_MAX/unit) {s->failed=1;return 0;}
    void *q=resize(s,*p,*cap*unit,n*unit);
    if(!q) {s->failed=1;return 0;} *p=q; *cap=n; return 1;
}
static int bytes(pangu_json_stream *s,buffer *b,const char *p,size_t n) {
    if(n>SIZE_MAX-b->len-1) {s->failed=1;return 0;}
    if(!reserve(s,(void **)&b->data,&b->cap,b->len+n+1,1)) return 0;
    if(n) memcpy(b->data+b->len,p,n);
    b->len+=n; b->data[b->len]=0; return 1;
}
static yyjson_mut_val *checked(pangu_json_stream *s,yyjson_mut_val *v) { if(!v)s->failed=1;return v; }
static frame *top(pangu_json_stream *s) { return s->depth?&s->frames[s->depth-1]:NULL; }
static slot value_slot(pangu_json_stream *s) {
    frame *f=top(s); slot r={0};
    if(!f) {r.valid=s->root==NULL;return r;}
    r.parent=f->value;r.identity=f->identity;
    if(yyjson_mut_is_arr(f->value)) r.valid=1;
    else if((f->phase==1 || f->phase==2) && f->key) {r.valid=1;r.key=f->key;}
    return r;
}
static uint64_t hash_key(yyjson_mut_val *parent,yyjson_mut_val *key) {
    uint64_t h=(uint64_t)(uintptr_t)parent ^ UINT64_C(1469598103934665603);
    const unsigned char *p=(const unsigned char *)yyjson_mut_get_str(key);
    for(size_t i=0;i<yyjson_mut_get_len(key);i++) h=(h^p[i])*UINT64_C(1099511628211);
    return h;
}
static int same(entry *e,yyjson_mut_val *p,yyjson_mut_val *k,uint64_t h) {
    return e->parent==p && e->hash==h && yyjson_mut_get_len(e->key)==yyjson_mut_get_len(k) &&
        !memcmp(yyjson_mut_get_str(e->key),yyjson_mut_get_str(k),yyjson_mut_get_len(k));
}
static entry *lookup(pangu_json_stream *s,yyjson_mut_val *p,yyjson_mut_val *k) {
    if(!s->table_cap || s->table_used>=s->table_cap/2) {
        size_t n=s->table_cap?s->table_cap*2:64;
        if(n<s->table_cap || n>SIZE_MAX/sizeof(entry)) {s->failed=1;return NULL;}
        entry *t=alloc(s,n*sizeof(*t)); if(!t){s->failed=1;return NULL;}
        memset(t,0,n*sizeof(*t));
        for(size_t i=0;i<s->table_cap;i++) if(s->table[i].parent) {
            size_t j=(size_t)s->table[i].hash&(n-1); while(t[j].parent) j=(j+1)&(n-1); t[j]=s->table[i];
        }
        release(s,s->table); s->table=t;s->table_cap=n;
    }
    uint64_t h=hash_key(p,k); size_t i=(size_t)h&(s->table_cap-1);
    while(s->table[i].parent && !same(&s->table[i],p,k,h)) i=(i+1)&(s->table_cap-1);
    return &s->table[i];
}
/* yyjson mutable value payload replacement preserves its intrusive next link. */
static void assign(yyjson_mut_val *dst,yyjson_mut_val *src) { dst->tag=src->tag; dst->uni=src->uni; }
static yyjson_mut_val *place(pangu_json_stream *s,slot at,yyjson_mut_val *v) {
    if(!v || !at.valid) return v;
    s->dirty=1;
    if(!at.parent) {s->root=v;return v;}
    if(!at.key) {yyjson_mut_arr_append(at.parent,v);return v;}
    entry *e=lookup(s,at.identity,at.key); if(!e)return v;
    if(e->parent) {assign(e->value,v);return e->value;}
    *e=(entry){at.identity,at.key,v,hash_key(at.identity,at.key)};s->table_used++;
    yyjson_mut_obj_add(at.parent,at.key,v);return v;
}
static void ended(pangu_json_stream *s) { frame *f=top(s); if(f)f->phase=3;else if(s->root)s->done=1; }
static int delimiter(unsigned char c) { return c && strchr(" \t\n\r,:{}[]\"",c)!=NULL; }
static int hex(unsigned char c) {return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');}
/* At most 64 bytes are reparsed for pending scalars; complete tokens once. */
static yyjson_mut_val *scalar_value(pangu_json_stream *s,int pending) {
    size_t n=s->scalar.len; char *p=s->scalar.data;
    if(!n || (pending && n>64))return NULL;
    const char *lits[]={"true","false","null"};
    for(int i=0;i<3;i++) if(n<=strlen(lits[i]) && !memcmp(p,lits[i],n) && (pending||n==strlen(lits[i])))
        return checked(s,i==2?yyjson_mut_null(s->doc):yyjson_mut_bool(s->doc,i==0));
    if(pending) {
        size_t k=n; if(k && (p[k-1]=='+'||p[k-1]=='-'))k--;
        if(k && (p[k-1]=='e'||p[k-1]=='E'))n=k-1;
    }
    if(!n || !(p[0]=='-' || (p[0]>='0'&&p[0]<='9')))return NULL;
    yyjson_read_err err; yyjson_doc *d=yyjson_read_opts(p,n,YYJSON_READ_BIGNUM_AS_RAW,&s->alc,&err);
    if(!d) {if(err.code==YYJSON_READ_ERROR_MEMORY_ALLOCATION)s->failed=1;return NULL;}
    yyjson_val *v=yyjson_doc_get_root(d); yyjson_mut_val *r=NULL;
    /* Pi numbers are IEEE-754 doubles, including rounded large integers. */
    if(yyjson_is_num(v))r=checked(s,yyjson_mut_real(s->doc,yyjson_get_num(v)));
    else if(yyjson_is_raw(v)) {
        /* The raw-number flag also catches integers outside uint64_t, even
         * when they still fit a finite double. Use yyjson's numeric conversion
         * before falling back to signed infinity for true double overflow. */
        yyjson_read_err conversion_error;
        yyjson_doc *converted=yyjson_read_opts(p,n,0,&s->alc,&conversion_error);
        if(converted) {
            r=checked(s,yyjson_mut_real(s->doc,yyjson_get_num(yyjson_doc_get_root(converted))));
            yyjson_doc_free(converted);
        } else if(conversion_error.code==YYJSON_READ_ERROR_MEMORY_ALLOCATION)s->failed=1;
        else r=checked(s,yyjson_mut_real(s->doc,p[0]=='-'?-INFINITY:INFINITY));
    }
    yyjson_doc_free(d);return r;
}
static void string_text(pangu_json_stream *s,const char *p,size_t n) {
    buffer *b=s->string;
    if(!bytes(s,b,p,n))return;
    /* Keep a closing quote beyond the logical end. */
    if(!reserve(s,(void **)&b->data,&b->cap,b->len+2,1))return;
    b->data[b->len]='"';b->data[b->len+1]=0;
    if(s->string_value) {
        yyjson_mut_set_raw(s->string_value,b->data,b->len+1);
        s->dirty=1;
    }
}
static void string_byte(pangu_json_stream *s,unsigned char c) {
    if(s->utf_left) {
        if((c&0xc0)==0x80) {
            s->utf[s->utf_len++]=(char)c;
            if(--s->utf_left==0) {
                /* Strict decoder validation; invalid UTF-8 becomes replacement. */
                unsigned char a=(unsigned char)s->utf[0], b=(unsigned char)s->utf[1];
                if((a==0xe0&&b<0xa0)||(a==0xed&&b>=0xa0)||(a==0xf0&&b<0x90)||(a==0xf4&&b>=0x90))
                    string_text(s,"\\ufffd",6);
                else string_text(s,s->utf,s->utf_len);
            }
            return;
        }
        s->utf_left=0;string_text(s,"\\ufffd",6);
    }
    if(c>=0x80) {
        s->utf_len=1;s->utf[0]=(char)c;
        s->utf_left=(c>=0xc2&&c<=0xdf)?1:(c>=0xe0&&c<=0xef)?2:(c>=0xf0&&c<=0xf4)?3:0;
        if(!s->utf_left)string_text(s,"\\ufffd",6);
    } else if(c<32) {
        const char *h="0123456789abcdef";char b[6]={'\\','u','0','0',h[c>>4],h[c&15]};string_text(s,b,6);
    } else {char b=(char)c;string_text(s,&b,1);}
}
static void read_string(pangu_json_stream *s,unsigned char c) {
    if(s->escape_len) {
        if(s->escape_len==1) {
            if(strchr("\"\\/bfnrt",c) && c) {
                char b[2]={'\\',(char)c};s->escape_len=0;string_text(s,b,2);return;
            }
            if(c=='u'){s->escape[s->escape_len++]='u';return;}
        } else if(hex(c)) {
            s->escape[s->escape_len++]=(char)c;
            if(s->escape_len==6){string_text(s,s->escape,6);s->escape_len=0;} return;
        }
        /* Invalid escape: encode the literal backslash and accumulated text. */
        string_text(s,"\\\\",2);
        if(s->escape_len>1)string_text(s,s->escape+1,s->escape_len-1);
        s->escape_len=0;
    }
    if(c=='"') {
        if(s->utf_left){s->utf_left=0;string_text(s,"\\ufffd",6);}
        s->in_string=0;
        if(s->key_string) {
            yyjson_read_err e; yyjson_doc *d=yyjson_read_opts(s->string->data,s->string->len+1,YYJSON_READ_ALLOW_INVALID_UNICODE,&s->alc,&e);
            if(!d && e.code!=YYJSON_READ_ERROR_MEMORY_ALLOCATION) {
                /* Keep isolated UTF-16 code units in internal WTF-8 keys.
                 * yyjson handles decoding ordinary escapes and valid pairs;
                 * the writer below re-escapes isolated units for valid JSON. */
                char *p=s->string->data; size_t read=1,write=1;
                while(read<s->string->len) {
                    if(p[read]!='\\' || read+1>=s->string->len) {p[write++]=p[read++];continue;}
                    if(p[read+1]!='u' || read+6>s->string->len) {
                        p[write++]=p[read++];p[write++]=p[read++];continue;
                    }
                    unsigned u=0;
                    for(size_t j=2;j<6;j++){unsigned c=(unsigned char)p[read+j];u=(u<<4)|(c<='9'?c-'0':(c|32)-'a'+10);}
                    size_t length=6;
                    if(u>=0xd800 && u<=0xdbff && read+12<=s->string->len && p[read+6]=='\\' && p[read+7]=='u') {
                        unsigned lo=0;for(size_t j=8;j<12;j++){unsigned c=(unsigned char)p[read+j];lo=(lo<<4)|(c<='9'?c-'0':(c|32)-'a'+10);}
                        if(lo>=0xdc00 && lo<=0xdfff)length=12;
                    }
                    if(length==6 && u>=0xd800 && u<=0xdfff) {
                        p[write++]=(char)(0xe0|(u>>12));p[write++]=(char)(0x80|((u>>6)&63));p[write++]=(char)(0x80|(u&63));
                    } else {memmove(p+write,p+read,length);write+=length;}
                    read+=length;
                }
                s->string->len=write;p[write]='"';p[write+1]=0;
                d=yyjson_read_opts(p,s->string->len+1,YYJSON_READ_ALLOW_INVALID_UNICODE,&s->alc,&e);
            }
            if(!d){s->failed=1;return;}
            frame *f=top(s); f->key=checked(s,yyjson_val_mut_copy(s->doc,yyjson_doc_get_root(d)));f->phase=1;
            yyjson_doc_free(d);
        }else ended(s);
        s->string_value=NULL;return;
    }
    if(c=='\\'){s->escape[0]='\\';s->escape_len=1;return;}
    string_byte(s,c);
}
static void structural(pangu_json_stream *s,unsigned char c) {
    frame *f=top(s);
    if(c==' '||c=='\t'||c=='\n'||c=='\r')return;
    if(c=='"') {
        buffer *b=alloc(s,sizeof(*b));if(!b){s->failed=1;return;}
        memset(b,0,sizeof(*b));b->next=s->strings;s->strings=b;s->string=b;
        s->in_string=1;s->key_string=f && yyjson_mut_is_obj(f->value) && (f->phase==0||f->phase==3);
        s->string_value=NULL;string_text(s,"\"",1);
        slot at=value_slot(s);
        if(!s->key_string && at.valid && !s->failed) s->string_value=place(s,at,checked(s,yyjson_mut_rawn(s->doc,b->data,b->len+1)));
    } else if(c=='{'||c=='[') {
        yyjson_mut_val *v=checked(s,c=='{'?yyjson_mut_obj(s->doc):yyjson_mut_arr(s->doc));
        if(s->failed)return;
        yyjson_mut_val *identity=v;v=place(s,value_slot(s),v);
        if(!reserve(s,(void **)&s->frames,&s->frame_cap,s->depth+1,sizeof(frame)))return;
        s->frames[s->depth++]=(frame){v,NULL,identity,c=='{'?0:2};s->dirty=1;
    } else if(c=='}'||c==']') {
        if(s->depth){s->depth--;s->dirty=1;ended(s);}
    } else if(c==':') {if(f&&f->phase==1)f->phase=2;}
    else if(c==',') {if(f)f->phase=yyjson_mut_is_arr(f->value)?2:0;}
    else {s->scalar.len=0;s->scalar_slot=value_slot(s);char b=(char)c;bytes(s,&s->scalar,&b,1);if(s->scalar_slot.valid)s->dirty=1;}
}
pangu_status pangu_json_stream_create(pangu_allocator a,pangu_json_stream **out) {
    if(!out)return PANGU_INVALID;
    *out=NULL;if(!a.alloc||!a.free)return PANGU_INVALID;
    pangu_json_stream *s=a.alloc(a.ctx,sizeof(*s));if(!s)return PANGU_NOMEM;
    memset(s,0,sizeof(*s));s->a=a;s->alc=(yyjson_alc){alloc,resize,release,s};
    s->doc=yyjson_mut_doc_new(&s->alc);
    if(!s->doc){a.free(a.ctx,s);return PANGU_NOMEM;}*out=s;return PANGU_OK;
}
void pangu_json_stream_destroy(pangu_json_stream *s) {
    if(!s)return;
    yyjson_mut_doc_free(s->doc);
    buffer *b=s->strings;while(b){buffer *next=b->next;release(s,b->data);release(s,b);b=next;}
    release(s,s->raw.data);release(s,s->scalar.data);release(s,s->frames);release(s,s->table);
    release(s,s->snapshot);release(s,s->final);pangu_allocator a=s->a;a.free(a.ctx,s);
}
pangu_status pangu_json_stream_append(pangu_json_stream *s,pangu_slice input) {
    if(!s||(!input.data&&input.len))return PANGU_INVALID;
    if(s->failed)return PANGU_NOMEM;
    if(!bytes(s,&s->raw,input.data,input.len))return PANGU_NOMEM;
    size_t add=input.len>(SIZE_MAX-256)/8?SIZE_MAX:256+8*input.len;
    s->credit=add>SIZE_MAX-s->credit?SIZE_MAX:s->credit+add;
    for(size_t i=0;i<input.len&&!s->done&&!s->failed;i++) {
        unsigned char c=(unsigned char)input.data[i];
        if(s->in_string){read_string(s,c);continue;}
        if(s->scalar.len) {
            if(!delimiter(c)){char b=(char)c;bytes(s,&s->scalar,&b,1);if(s->scalar_slot.valid)s->dirty=1;continue;}
            yyjson_mut_val *v=scalar_value(s,0);if(v)place(s,s->scalar_slot,v);
            s->scalar.len=0;ended(s);if(s->done)continue;
        }
        structural(s,c);
    }
    return s->failed?PANGU_NOMEM:PANGU_OK;
}
static char *write_json(pangu_json_stream *s,yyjson_mut_val *root,size_t *length) {
    yyjson_write_err error;
    char *raw=yyjson_mut_val_write_opts(root,YYJSON_WRITE_ALLOW_INVALID_UNICODE|YYJSON_WRITE_INF_AND_NAN_AS_NULL,&s->alc,length,&error);
    if(!raw){s->failed=1;return NULL;}
    size_t extra=0;
    for(size_t i=0;i+2<*length;i++) if((unsigned char)raw[i]==0xed &&
        (unsigned char)raw[i+1]>=0xa0 && (unsigned char)raw[i+1]<=0xbf &&
        ((unsigned char)raw[i+2]&0xc0)==0x80) {if(extra>SIZE_MAX-3){s->failed=1;break;}extra+=3;i+=2;}
    if(s->failed || extra>SIZE_MAX-*length-1){release(s,raw);s->failed=1;return NULL;}
    if(!extra)return raw;
    char *encoded=alloc(s,*length+extra+1);
    if(!encoded){release(s,raw);s->failed=1;return NULL;}
    size_t out=0;const char *hex="0123456789abcdef";
    for(size_t i=0;i<*length;i++) {
        unsigned char a=(unsigned char)raw[i];
        if(a==0xed && i+2<*length && (unsigned char)raw[i+1]>=0xa0 && (unsigned char)raw[i+1]<=0xbf &&
            ((unsigned char)raw[i+2]&0xc0)==0x80) {
            unsigned u=((a&15)<<12)|(((unsigned char)raw[i+1]&63)<<6)|((unsigned char)raw[i+2]&63);
            encoded[out++]='\\';encoded[out++]='u';
            for(int shift=12;shift>=0;shift-=4)encoded[out++]=hex[(u>>shift)&15];
            i+=2;
        } else encoded[out++]=raw[i];
    }
    encoded[out]=0;*length=out;release(s,raw);return encoded;
}
pangu_status pangu_json_stream_snapshot(pangu_json_stream *s,pangu_slice *out) {
    if(!s||!out)return PANGU_INVALID;
    *out=(pangu_slice){"{}",2};
    if(s->failed)return PANGU_NOMEM;
    size_t cost=s->raw.len;
    if(s->dirty && (s->done || cost<=s->credit)) {
        if(!s->done)s->credit-=cost;
        yyjson_mut_val *root=s->root, *pending=NULL, *old=NULL;
        yyjson_mut_val saved; memset(&saved,0,sizeof(saved));
        int appended=0;slot at=s->scalar_slot;
        if(s->scalar.len && at.valid)pending=scalar_value(s,1);
        if(pending) {
            if(!at.parent)root=pending;
            else if(!at.key){yyjson_mut_arr_append(at.parent,pending);appended=1;}
            else {
                entry *e=lookup(s,at.identity,at.key);
                if(e && e->parent){old=e->value;saved=*old;assign(old,pending);}
                else if(e){yyjson_mut_obj_add(at.parent,at.key,pending);appended=1;}
            }
        }
        char *json=NULL;size_t len=0;
        if(!s->failed && yyjson_mut_is_obj(root))json=write_json(s,root,&len);
        if(old)assign(old,&saved);
        if(appended) {
            if(at.key)yyjson_mut_obj_remove(at.parent,at.key);
            else yyjson_mut_arr_remove_last(at.parent);
        }
        if(yyjson_mut_is_obj(root) && !json)s->failed=1;
        if(s->failed){release(s,json);return PANGU_NOMEM;}
        release(s,s->snapshot);s->snapshot=json;s->snapshot_len=len;s->dirty=0;
    }
    if(s->snapshot)*out=(pangu_slice){s->snapshot,s->snapshot_len};
    return PANGU_OK;
}
/* Build a validation-only repair. Values retain their original escaped UTF-16
 * code units in the incremental tree; yyjson validates the structural grammar
 * with surrogate code units replaced here, as it rejects lone surrogates. */
static int validation_text(pangu_json_stream *s,buffer *b) {
    int in=0;
    for(size_t i=0;i<s->raw.len;i++) {
        unsigned char c=(unsigned char)s->raw.data[i];
        if(!in){if(c=='"')in=1;if(!bytes(s,b,(const char *)&s->raw.data[i],1))return 0;continue;}
        if(c=='"'){in=0;if(!bytes(s,b,"\"",1))return 0;continue;}
        if(c=='\\') {
            if(i+1<s->raw.len) {
                unsigned char next=(unsigned char)s->raw.data[i+1];
                if(next && strchr("\"\\/bfnrt",next)) {
                    if(!bytes(s,b,s->raw.data+i,2))return 0;
                    i++;continue;
                }
                if(next=='u' && i+6<=s->raw.len && hex(s->raw.data[i+2]) && hex(s->raw.data[i+3]) && hex(s->raw.data[i+4]) && hex(s->raw.data[i+5])) {
                    unsigned first=(unsigned char)s->raw.data[i+2],second=(unsigned char)s->raw.data[i+3];
                    int surrogate=(first=='d'||first=='D') && ((second>='8'&&second<='9')||(second>='a'&&second<='f')||(second>='A'&&second<='F'));
                    if(!bytes(s,b,surrogate?"\\ufffd":s->raw.data+i,6))return 0;
                    i+=5;continue;
                }
            }
            if(!bytes(s,b,"\\\\",2))return 0;
        }else if(c<32) {
            const char *h="0123456789abcdef";char escaped[6]={'\\','u','0','0',h[c>>4],h[c&15]};
            if(!bytes(s,b,escaped,6))return 0;
        }else if(!bytes(s,b,s->raw.data+i,1))return 0;
    }
    return 1;
}
/* ECMAScript String.trim WhiteSpace/LineTerminator set, UTF-8. */
pangu_slice pangu_json_trim(pangu_slice raw) {
    static const char *spaces[]={"\xc2\xa0","\xe1\x9a\x80","\xe2\x80\x80","\xe2\x80\x81",
        "\xe2\x80\x82","\xe2\x80\x83","\xe2\x80\x84","\xe2\x80\x85","\xe2\x80\x86",
        "\xe2\x80\x87","\xe2\x80\x88","\xe2\x80\x89","\xe2\x80\x8a","\xe2\x80\xa8",
        "\xe2\x80\xa9","\xe2\x80\xaf","\xe2\x81\x9f","\xe3\x80\x80","\xef\xbb\xbf"};
    for(int right=0;right<2;++right)while(raw.len){
        unsigned char c=(unsigned char)raw.data[right?raw.len-1:0];
        size_t skip=0;
        if(c==9 || c==10 || c==11 || c==12 || c==13 || c==32)skip=1;
        for(size_t j=0;!skip && j<sizeof(spaces)/sizeof(spaces[0]);++j){
            size_t n=strlen(spaces[j]);
            if(n<=raw.len && !memcmp(raw.data+(right?raw.len-n:0),spaces[j],n))skip=n;
        }
        if(!skip)break;
        if(!right)raw.data+=skip;
        raw.len-=skip;
    }
    return raw;
}
pangu_status pangu_json_stream_finish(pangu_json_stream *s,pangu_slice *arguments,pangu_slice *error) {
    if(!s||!arguments||!error)return PANGU_INVALID;
    *arguments=(pangu_slice){"{}",2};*error=(pangu_slice){0};
    if(s->failed)return PANGU_NOMEM;
    release(s,s->final);s->final=NULL;
    if(!pangu_json_trim((pangu_slice){s->raw.data,s->raw.len}).len)return PANGU_OK;
    buffer repaired={0};
    if(!validation_text(s,&repaired)){release(s,repaired.data);return PANGU_NOMEM;}
    yyjson_read_err err;yyjson_doc *d=yyjson_read_opts(repaired.data,repaired.len,YYJSON_READ_BIGNUM_AS_RAW,&s->alc,&err);
    release(s,repaired.data);
    if(!d && err.code==YYJSON_READ_ERROR_MEMORY_ALLOCATION){s->failed=1;return PANGU_NOMEM;}
    const char *message=NULL;
    if(d && !yyjson_is_obj(yyjson_doc_get_root(d)))message="Tool arguments must be a JSON object";
    else if(!d)message=err.msg?err.msg:"Invalid tool arguments JSON";
    if(message) {
        const char *prefix="\n\nReceived arguments:\n";
        size_t m=strlen(message),p=strlen(prefix);
        if(s->raw.len>SIZE_MAX-m-p-1){s->failed=1;}
        else {
            s->final=alloc(s,m+p+s->raw.len+1);
            if(!s->final)s->failed=1;
            else {memcpy(s->final,message,m);memcpy(s->final+m,prefix,p);memcpy(s->final+m+p,s->raw.data,s->raw.len);s->final[m+p+s->raw.len]=0;
                *error=(pangu_slice){s->final,m+p+s->raw.len};}
        }
    } else {
        size_t len;
        /* The strict read validates the repaired structure. The incremental tree
         * already coalesces duplicate keys with last-value semantics. */
        s->final=write_json(s,s->root,&len);
        if(!s->final)s->failed=1;else *arguments=(pangu_slice){s->final,len};
    }
    yyjson_doc_free(d);return s->failed?PANGU_NOMEM:PANGU_OK;
}
