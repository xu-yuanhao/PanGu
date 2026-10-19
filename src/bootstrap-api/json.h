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
#endif
#endif
