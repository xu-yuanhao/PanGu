#ifndef PANGU_DOCUMENTS_H
#define PANGU_DOCUMENTS_H
#include "../contracts/client.h"
#include <stdint.h>
typedef enum { PG_DOC_SESSION,PG_DOC_CONVERSATION,PG_DOC_TASK } pg_document_scope;
typedef struct {
 pg_slice kind,key;
 int64_t owner;
 unsigned scope,has_key;
} pg_document_address;
typedef enum { PG_DOC_OK,PG_DOC_INVALID,PG_DOC_CAPACITY,PG_DOC_ALLOCATION,
 PG_DOC_SEMANTICS,PG_DOC_NEWER_VERSION,PG_DOC_MIGRATION_REQUIRED,PG_DOC_MIGRATION_FAILED,PG_DOC_MISSING,PG_DOC_STORAGE_FAILED } pg_document_status;
/* Slices borrow valid UTF-8, may contain NUL, lengths <=INT_MAX. Session owner
   is zero; conversation/task owners use the signed JS safe-integer range.
   has_key distinguishes an absent family key from an empty family key. */
int pg_document_address_valid(const pg_document_address *);
int pg_document_address_equal(const pg_document_address *,const pg_document_address *);
/* Durable addressId JSON tuple. Output may alias input; size is disjoint from
   all inputs/output. Failure sets size=0 and leaves output untouched. Capacity
   includes a trailing NUL, excluded from size. No retained JSON allocation. */
pg_document_status pg_document_address_encode(const pg_document_address *,char *,size_t,size_t *);
struct json_object;
#define PG_DOCUMENT_DEPTH_LIMIT 256
/* Finite acyclic json-c values, at most 256 nested containers. Shared children
   are allowed. Caller serializes access; objects use ordinary JSON values. */
int pg_document_value_valid(const struct json_object *);
typedef enum { PG_DOC_LATEST=1,PG_DOC_REWINDABLE } pg_document_history;
typedef enum { PG_DOC_CURRENT=1,PG_DOC_INITIAL,PG_DOC_AS_OF } pg_document_fork;
typedef struct {
 pg_slice kind;
 uint64_t version;
 unsigned scope,history,fork,family;
 void *context;
 /* Input is a private borrowed copy; output is an owned reference, even if
    identical to input. Returned values may be retained with another reference. */
 int (*migrate)(void *,struct json_object *,uint64_t,struct json_object **);
} pg_document_definition;
typedef struct {
 pg_document_address address;
 uint64_t id;
 unsigned history,fork;
} pg_document_record;
pg_document_status pg_document_check(const pg_document_definition *,const pg_document_record *,uint64_t stored_version);
/* Input is borrowed, immutable; success yields a detached owned JSON object.
   Output pointer must be disjoint from inputs. Failure leaves *output=NULL. */
pg_document_status pg_document_materialize(const pg_document_definition *,const pg_document_record *,uint64_t,
                                         struct json_object *,struct json_object **output);
/* Decode a typed StoredDocument.record. Slices borrow the JSON record;
   output is disjoint and cleared on failure. IDs are positive safe integers. */
pg_document_status pg_document_record_decode(const struct json_object *,pg_document_record *);
typedef struct {
 void *context;
 /* 0 owned result, 1 missing, other values storage failure. */
 int (*find)(void *,const pg_document_address *,uint64_t at,struct json_object **);
 int (*read)(void *,uint64_t id,uint64_t at,struct json_object **);
} pg_document_storage;
typedef struct {
 struct json_object *record_owner,*value;
 const pg_document_definition *definition;
 pg_document_record record;
 uint64_t stored_version,value_version,deltas_since_base;
 unsigned busy,active;
} pg_document_loaded;
/* Fresh zeroed or closed caller storage. Owns detached record/value; Definition
   remains borrowed until close. Find/read select current. Missing find returns
   MISSING; missing content after a successful find is STORAGE_FAILED.
   Migration changes value_version while stored_version retains the source.
   Fields are API-managed; consumers borrow them read-only while active. This
   is a loaded handle; Session cache/revision arbitration owns its lifetime. */
pg_document_status pg_document_loaded_open(pg_document_loaded *,const pg_document_storage *,const pg_document_definition *,const pg_document_address *);
pg_document_status pg_document_loaded_close(pg_document_loaded *);
#endif
