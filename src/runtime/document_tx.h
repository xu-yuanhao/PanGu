#ifndef PANGU_DOCUMENT_TX_H
#define PANGU_DOCUMENT_TX_H
#include "records.h"
#include "delta.h"
typedef struct {
 pg_document_loaded *loaded;
 pg_document_change *change;
 pg_document_prepared prepared;
 pg_document_adoption adoption;
 struct json_object *write;
 size_t write_index;
 void *context;
 /* Runs after batch validation, only for nonempty same-version edits.
    Borrowed value/ops are read-only. Return 0 delta, 1 base, other error. */
 int (*checkpoint)(void *,const struct json_object *,const struct json_object *,uint64_t deltas_since_base);
 unsigned kind,reserved;
} pg_document_tx_entry;
/* Loaded handles and edits are borrowed, disjoint, unique per incarnation and
   alive for the whole synchronous mutation line. Entry internal fields start
   zeroed. Seal and prepare all edits, validate all generated plans, then invoke
   checkpoint predicates and prepare adoption before Storage admission.
   Storage confirmation applies all loaded values before the Session's existing
   adopt hook; its existing publish hook follows normal settlement. Hooks and
   context are restored on return. No-op edits cause no Storage commit.
   After a valid admission every result discards prepared state and seals edits;
   argument/precondition refusal leaves caller state unchanged. Creation,
   retirement and deferred holds are handled by the surrounding Doc transaction.
   Callers must finish through this entry once loaded edits were acquired. */
pg_record_tx_status pg_document_tx_commit(pg_record_tx *,pg_document_tx_entry *,size_t);
#endif
