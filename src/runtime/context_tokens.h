#ifndef PANGU_CONTEXT_TOKENS_H
#define PANGU_CONTEXT_TOKENS_H
#include <stdint.h>
#include <stddef.h>
struct json_object;
/* Borrow validated canonical messages/UTF-8. Estimates allocate no owned result. */
uint64_t pg_context_message_tokens(const struct json_object *);
uint64_t pg_context_utf16_units(const char *,size_t);
uint64_t pg_context_usage_tokens(const struct json_object *usage);
/* Array, or PanGu {messages,systemPrompt}; usable provider usage covers the prefix. */
uint64_t pg_context_estimate(const struct json_object *context);
#endif
