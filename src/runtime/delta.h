#ifndef PANGU_DELTA_H
#define PANGU_DELTA_H
#include "documents.h"
/* Decoded Chord Op tuples over borrowed finite UTF-8 JSON values. Success
   returns a detached owned value (NULL represents JSON null); failure clears
   output. Output is disjoint from both inputs. Paths use string keys or
   nonnegative safe integers; object keys use the json-c C-string vocabulary.
   Text truncation counts UTF-16 units and requires a scalar boundary.
   Inputs and operations remain unchanged, including on failure. */
pg_document_status pg_document_delta_apply(const struct json_object *,const struct json_object *,struct json_object **);
/* Ordered array of decoded operation batches; only the final value is exposed. */
pg_document_status pg_document_delta_batches(const struct json_object *,const struct json_object *,struct json_object **);
typedef enum { PG_REVISION_BASE,PG_REVISION_DELTA } pg_revision_kind;
typedef struct {
 uint64_t sequence,version;
 unsigned kind,reserved;
 const struct json_object *content;
} pg_document_revision;
typedef struct {
 uint64_t created_at,retired_at;
 unsigned current_only,reserved;
 const pg_document_revision *revisions;
 size_t count;
} pg_document_revision_view;
typedef struct {struct json_object *value;uint64_t version;size_t deltas_since_base;} pg_document_snapshot;
/* Ordered committed revisions, borrowed throughout the call. retired_at=0
   means live, at=UINT64_MAX means current. Sequence/version use safe positive
   integers. A base contains a JSON object; a delta contains an Op array.
   PG_DOC_OK with version=0 means outside the incarnation lifetime. JSON null
   may also be a replayed value with a nonzero version. Historical
   reads of current-only records return PG_DOC_SEMANTICS before lifetime checks.
   Output is disjoint, initially unowned; failure clears all output fields. */
pg_document_status pg_document_revision_read(const pg_document_revision_view *,uint64_t at,pg_document_snapshot *);
typedef struct {
 struct json_object *base,*ops;
 unsigned busy,sealed,active,reserved;
} pg_document_change;
typedef struct {struct json_object *value,*ops;} pg_document_prepared;
/* Native explicit-op change over an object/array root. Caller owns zeroed or
   discarded storage. Open detaches the base; append detaches and atomically
   queues an Op batch. Path/operation semantics replay at snapshot/prepare;
   append only admits a finite JSON array. Snapshots own independent values.
   Prepare seals on success or replay failure; success transfers owned value
   and ops to the caller. No change access survives sealing. Discard is
   idempotent and never frees an already transferred Prepared value/ops.
   Serialized use; all output storage is disjoint from inputs and initially
   unowned. Root replacement replay follows the delta API, including JSON null. */
pg_document_status pg_document_change_open(pg_document_change *,const struct json_object *);
pg_document_status pg_document_change_append(pg_document_change *,const struct json_object *);
pg_document_status pg_document_change_snapshot(pg_document_change *,struct json_object **);
pg_document_status pg_document_change_prepare(pg_document_change *,pg_document_prepared *);
pg_document_status pg_document_change_discard(pg_document_change *);
/* Build an owned document.change from a borrowed Prepared: version migration
   selects base even with no ops; equal-version empty ops is an OK no-op with
   *output=NULL; otherwise checkpoint selects base or delta. The checkpoint
   decision is supplied by settlement after Tx validation, not executed here.
   Inputs remain unchanged and output payload is detached. Safe positive IDs
   and versions; stored_version>version returns PG_DOC_NEWER_VERSION. */
pg_document_status pg_document_plan_change(uint64_t id,uint64_t stored_version,uint64_t version,
 const pg_document_prepared *,unsigned checkpoint,struct json_object **);
#endif
