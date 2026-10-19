#ifndef PANGU_JSON_H
#define PANGU_JSON_H
#include "src/bootstrap-api/text.h"
#ifndef __ASSEMBLER__
enum pangu_json_kind { PANGU_JSON_NULL=1,PANGU_JSON_TRUE,PANGU_JSON_FALSE,PANGU_JSON_NUMBER,PANGU_JSON_STRING,PANGU_JSON_ARRAY,PANGU_JSON_OBJECT };
enum { PANGU_JSON_KEY=1,PANGU_JSON_ESCAPED=2 };
typedef struct pangu_json_token {
 pangu_u32 kind,flags;
 pangu_u64 start,end,parent,next,children;
} pangu_json_token;
typedef struct pangu_json_frame { pangu_u64 token;pangu_u32 phase,reserved; } pangu_json_frame;
typedef struct pangu_json_limits { pangu_u64 max_bytes;pangu_u32 max_depth,reserved; } pangu_json_limits;
typedef struct pangu_json_parser {
 pangu_u32 abi_version,struct_size;
 pangu_json_token *tokens;
 pangu_u64 capacity,count,offset,max_bytes,error_offset,active_index;
 pangu_u32 depth,max_depth,root_phase,mode;
 pangu_i32 status;
 pangu_u32 sealed,number_state,literal_position,unicode_left,utf8_have,utf8_need,key;
 unsigned char utf8[4];pangu_u32 reserved;
 pangu_json_frame frames[64];
} pangu_json_parser;
typedef struct pangu_json_result {
 const pangu_json_token *tokens;
 pangu_u64 count,error_offset;
 pangu_i32 status;pangu_u32 reserved;
} pangu_json_result;
_Static_assert(sizeof(pangu_json_token)==48,"JSON token layout");
_Static_assert(sizeof(pangu_json_limits)==16,"JSON limits layout");
_Static_assert(sizeof(pangu_json_parser)==1144,"JSON parser layout");
_Static_assert(sizeof(pangu_json_result)==32,"JSON result layout");
/* Zero-init parser storage once; init resets it and invalidates prior results.
 * Caller owns parser and nonoverlapping token storage. Feed input is readable
 * for the call only; offsets refer to the caller's retained logical byte stream.
 * Token lexemes include quotes/brackets. parent==UINT64_MAX means root.
 * next skips a subtree; children counts array values / object members.
 * Keys are string tokens flagged KEY; duplicate keys remain in source order.
 * Escaped UTF-16 code units (including lone surrogates) remain raw lexemes.
 * No unescaping, numeric conversion, repair, Schema validation or tool execution.
 * 0 final success, 13 need-more/incomplete, 14 syntax, 12 string UTF-8,
 * 2 byte/depth/token quota, 1 arguments, 10 feed after terminal state.
 * final must be 0 or 1. Nonfinal feeds never publish results, even for '{}'.
 * A final incomplete document is a terminal failure (13), not executable.
 * On failure/incomplete, result exposes NULL tokens and count zero; internal
 * token storage is dirty. Init or caller mutation invalidates every raw result.
 */
pangu_i32 pangu_json_init(pangu_json_parser *,pangu_json_token *,pangu_u64,pangu_json_limits);
pangu_i32 pangu_json_feed(pangu_json_parser *,pangu_text_slice,pangu_u32);
pangu_i32 pangu_json_get_result(const pangu_json_parser *,pangu_json_result *);
/* Quote one strict UTF-8 byte slice as a JSON string, without trailing NUL.
 * Input must remain unchanged during the call. No allocation or numeric/Schema
 * conversion. Output and length storage must not overlap input or each other.
 * Invalid arguments (1) leave outputs untouched. With valid storage, UTF-8 or
 * overflow failures (12/11) set length zero; capacity failure (2) reports required
 * length. Every failure leaves output bytes unchanged. NULL output/capacity zero
 * supports measuring; even empty input requires two bytes for quotes.
 */
pangu_i32 pangu_json_quote_utf8(pangu_text_slice,unsigned char *,pangu_u64,pangu_u64 *);
/* Reader calls accept only a successful result and the corresponding unchanged
 * complete input. All borrowed storage must remain valid; no authentication of
 * arbitrary caller-forged tokens or detection of reinit through raw pointers.
 * Decode is lossless UTF-8, no NUL terminator. Two passes: output bytes unchanged
 * on failure. Capacity failure (2) reports required length; other runtime failures
 * report length zero after argument/result checks. Rejected arguments/results
 * leave outputs untouched; callers must check the return status.
 * Lone escaped UTF-16 surrogate: 9 unsupported (raw token remains available).
 * Decode output/length and find output must not overlap input, tokens or result;
 * find output must not overlap key. NULL output is valid only at capacity zero.
 * Find compares decoded UTF-8 keys, last matching member wins, skips subtrees.
 * A legal non-UTF-8-representable key cannot match a UTF-8 query. Missing is 3
 * with index UINT64_MAX; present null returns 0 and a NULL-kind token index.
 * Readers allocate nothing and do not perform Schema conversion/Pi repair.
 */
pangu_i32 pangu_json_string_decode(pangu_text_slice,const pangu_json_result *,pangu_u64,unsigned char *,pangu_u64,pangu_u64 *);
pangu_i32 pangu_json_object_find(pangu_text_slice,const pangu_json_result *,pangu_u64,pangu_text_slice,pangu_u64 *);

/* Convert a NUMBER token from a successful result and matching immutable input
 * to IEEE-754 binary64 bits (not a native floating operation). Nearest, ties to
 * even; preserve negative zero; overflow produces signed infinity as JSON.parse.
 * Schema finite-number checks are separate. No Number(string) coercion/repair.
 * Maximum numeric lexeme 1 MiB; 2 quota, 9 wrong token kind, 10 invalid result,
 * 14 malformed numeric lexeme, 1 arguments/storage overlap. Failure leaves bits
 * unchanged. Output must be disjoint from source, result and token storage.
 * Fixed private integer scratch, no allocation, libc or floating point state.
 */
pangu_i32 pangu_json_number_bits(pangu_text_slice,const pangu_json_result *,pangu_u64,pangu_u64 *);
enum pangu_json_write_op {
 PANGU_JSON_WRITE_OBJECT=1,PANGU_JSON_WRITE_END_OBJECT,PANGU_JSON_WRITE_ARRAY,
 PANGU_JSON_WRITE_END_ARRAY,PANGU_JSON_WRITE_KEY,PANGU_JSON_WRITE_STRING,
 PANGU_JSON_WRITE_NUMBER,PANGU_JSON_WRITE_TRUE,PANGU_JSON_WRITE_FALSE,PANGU_JSON_WRITE_NULL
};
typedef struct pangu_json_write_frame { pangu_u32 kind,phase; } pangu_json_write_frame;
typedef struct pangu_json_writer {
 pangu_u32 abi_version,struct_size;
 unsigned char *data;pangu_u64 capacity,length;
 pangu_u32 depth,max_depth,root,sealed;
 pangu_json_write_frame frames[64];
} pangu_json_writer;
_Static_assert(sizeof(pangu_json_writer)==560,"JSON writer layout");
/* Caller owns writer and its disjoint fixed output bytes. Init resets metadata,
 * not output bytes, and invalidates prior views. No allocation, NUL or IO.
 * KEY/STRING take strict UTF-8; NUMBER takes a complete strict numeric lexeme
 * <=1 MiB, preserving its spelling, not binary64 formatting/JSON.stringify.
 * Other operations require NULL/zero argument. Explicit emission order and
 * duplicate keys are preserved. Container depth is caller-limited to 1..64.
 * Each rejected call leaves writer and every output byte unchanged: 1 invalid
 * arguments/overlap, 2 output/depth/numeric quota, 9 non-number literal,
 * 10 invalid state/order/sealed, 12 UTF-8, 14 numeric syntax. Source must stay
 * immutable/disjoint from writer and entire output storage during a call.
 * Finish publishes one complete root as a borrowed slice and seals emission;
 * 13 incomplete leaves state/view unchanged. Failure never publishes a result.
 * Finish view storage must be disjoint from writer/output. Caller-provided
 * memory validity/lifetime is required; these checks are not native isolation.
 */
pangu_i32 pangu_json_writer_init(pangu_json_writer *,unsigned char *,pangu_u64,pangu_u32);
pangu_i32 pangu_json_writer_emit(pangu_json_writer *,pangu_u32,pangu_text_slice);
pangu_i32 pangu_json_writer_finish(pangu_json_writer *,pangu_text_slice *);
#endif
#endif
