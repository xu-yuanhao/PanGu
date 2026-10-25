#ifndef PG_MIMO_LAYOUT_H
#define PG_MIMO_LAYOUT_H
/* mimo_projection.S / mimo_stream.S / mimo.S 与 C 测试共享的布局/枚举常量。
 * 数值只有这一份；C 侧 _Static_assert 对齐 session.h/agent.h/tools.h/mimo.h
 * 与本机公开 libcurl/json-c 头，漂移即编译失败，不手抄私有结构。
 *
 * pg_mimo 公开布局冻结（mimo.h）：字段顺序/尺寸不得改动。
 * curl_easy_setopt/curl_easy_getinfo 为变参函数：整数/指针变参在 rdx/rcx/r8，
 * 调用前须置 al=0（无向量寄存器参数），不能按普通三参函数猜测。 */
#define PG_MM_MESSAGE_LIMIT 64
#define PG_MM_TEXT_LIMIT 4096
#define PG_MM_ID_LIMIT 64
#define PG_MM_BODY_LIMIT 131072

/* pg_slice（src/contracts/client.h） */
#define PG_MM_SLICE_DATA 0
#define PG_MM_SLICE_SIZE 8
#define PG_MM_SLICE_BYTES 16

/* pg_role（session.h） */
#define PG_MM_ROLE_USER 0
#define PG_MM_ROLE_ASSISTANT 1
#define PG_MM_ROLE_TOOL_CALL 2
#define PG_MM_ROLE_TOOL_RESULT 3

/* pg_message（session.h） */
#define PG_MM_MSG_ROLE 0
#define PG_MM_MSG_SIZE 8
#define PG_MM_MSG_TEXT 16
#define PG_MM_MSG_CALL_ID 4112
#define PG_MM_MSG_TOOL_NAME 4176
#define PG_MM_MSG_IS_ERROR 4240
#define PG_MM_MSG_BYTES 4248

/* pg_session 消息/系统视图（session.h） */
#define PG_MM_SES_MESSAGES 0
#define PG_MM_SES_COUNT 271872
#define PG_MM_SES_SYSTEM_PROMPT 271880
#define PG_MM_SES_SYSTEM_SIZE 275976
#define PG_MM_SES_BYTES 275984

/* pg_model_event（agent.h） */
#define PG_MM_EVT_KIND 0
#define PG_MM_EVT_TEXT 8
#define PG_MM_EVT_CALL_ID 24
#define PG_MM_EVT_TOOL_NAME 40
#define PG_MM_EVT_BYTES 56

/* pg_model_kind（agent.h） */
#define PG_MM_KIND_WAIT 0
#define PG_MM_KIND_TEXT 1
#define PG_MM_KIND_CALL 2
#define PG_MM_KIND_DONE 3
#define PG_MM_KIND_ERROR 4

/* pg_model 函数表（agent.h）；40 字节聚合经 sret 返回 */
#define PG_MM_MODEL_CONTEXT 0
#define PG_MM_MODEL_START 8
#define PG_MM_MODEL_NEXT 16
#define PG_MM_MODEL_CANCEL 24
#define PG_MM_MODEL_CLOSE 32
#define PG_MM_MODEL_BYTES 40

/* pg_tool_definition / pg_tool_registry（tools.h；仅消费字段） */
#define PG_MM_DEF_NAME 0
#define PG_MM_DEF_DESCRIPTION 8
#define PG_MM_DEF_SCHEMA 16
#define PG_MM_DEF_BYTES 64
#define PG_MM_REG_ENTRIES 0
#define PG_MM_REG_COUNT 1024
#define PG_MM_REG_BYTES 1032

/* pg_mimo（mimo.h；公开布局冻结） */
#define PG_MM_EASY 0
#define PG_MM_MULTI 8
#define PG_MM_HEADERS 16
#define PG_MM_BODY 24
#define PG_MM_TEXT 131096
#define PG_MM_ARGUMENTS 135192
#define PG_MM_ID 139288
#define PG_MM_NAME 139352
#define PG_MM_USED 139416
#define PG_MM_CONSUMED 139424
#define PG_MM_DELIVERED 139432
#define PG_MM_FAILED 139436
#define PG_MM_TRANSPORT_DONE 139440
#define PG_MM_FINISH_REASON 139444
#define PG_MM_SAW_DONE 139448
#define PG_MM_HTTP 139456
#define PG_MM_REQUEST 139464
#define PG_MM_TOOLS 139472
#define PG_MM_BYTES 139480

/* json-c 公开枚举（<json-c/json.h>） */
#define PG_MM_JSON_NULL 0
#define PG_MM_JSON_INT 3
#define PG_MM_JSON_OBJECT 4
#define PG_MM_JSON_ARRAY 5
#define PG_MM_JSON_STRING 6
#define PG_MM_TOKENER_SUCCESS 0
#define PG_MM_TO_JSON_PLAIN 0

/* libcurl 公开常量（<curl/curl.h> CURLOPTTYPE_LONG/OBJECTPOINT/FUNCTIONPOINT 展开） */
#define PG_MM_CURLOPT_TIMEOUT 13
#define PG_MM_CURLOPT_CONNECTTIMEOUT 78
#define PG_MM_CURLOPT_NOSIGNAL 99
#define PG_MM_CURLOPT_WRITEDATA 10001
#define PG_MM_CURLOPT_URL 10002
#define PG_MM_CURLOPT_POSTFIELDS 10015
#define PG_MM_CURLOPT_HTTPHEADER 10023
#define PG_MM_CURLOPT_WRITEFUNCTION 20011
#define PG_MM_CURLOPT_PROTOCOLS_STR 10318
#define PG_MM_CURLINFO_RESPONSE_CODE 0x200002
#define PG_MM_CURLM_OK 0
#define PG_MM_CURLMSG_DONE 1
#define PG_MM_CURLE_OK 0

/* struct CURLMsg（<curl/multi.h>）：msg(int)/easy_handle/union{whatever,result} */
#define PG_MM_CURLMSG_MSG 0
#define PG_MM_CURLMSG_EASY 8
#define PG_MM_CURLMSG_RESULT 16
#define PG_MM_CURLMSG_BYTES 24

/* Authorization 头前缀长度（mimo.S 按长度 memcpy，不跑 strlen） */
#define PG_MM_AUTH_PREFIX_LEN 22

#ifndef __ASSEMBLER__
#include <stddef.h>
#include "mimo.h"
#include <json-c/json.h>
#include <curl/multi.h>
#define PG_MM_ASSERT_OFF(t, f, v) _Static_assert(offsetof(t, f) == (v), #t "." #f " layout drifted")
#define PG_MM_ASSERT_SIZE(t, v) _Static_assert(sizeof(t) == (v), #t " size drifted")
PG_MM_ASSERT_OFF(pg_slice, data, PG_MM_SLICE_DATA);
PG_MM_ASSERT_OFF(pg_slice, size, PG_MM_SLICE_SIZE);
PG_MM_ASSERT_SIZE(pg_slice, PG_MM_SLICE_BYTES);
PG_MM_ASSERT_OFF(pg_message, role, PG_MM_MSG_ROLE);
PG_MM_ASSERT_OFF(pg_message, size, PG_MM_MSG_SIZE);
PG_MM_ASSERT_OFF(pg_message, text, PG_MM_MSG_TEXT);
PG_MM_ASSERT_OFF(pg_message, call_id, PG_MM_MSG_CALL_ID);
PG_MM_ASSERT_OFF(pg_message, tool_name, PG_MM_MSG_TOOL_NAME);
PG_MM_ASSERT_OFF(pg_message, is_error, PG_MM_MSG_IS_ERROR);
PG_MM_ASSERT_SIZE(pg_message, PG_MM_MSG_BYTES);
PG_MM_ASSERT_OFF(pg_session, messages, PG_MM_SES_MESSAGES);
PG_MM_ASSERT_OFF(pg_session, count, PG_MM_SES_COUNT);
PG_MM_ASSERT_OFF(pg_session, system_prompt, PG_MM_SES_SYSTEM_PROMPT);
PG_MM_ASSERT_OFF(pg_session, system_size, PG_MM_SES_SYSTEM_SIZE);
PG_MM_ASSERT_SIZE(pg_session, PG_MM_SES_BYTES);
PG_MM_ASSERT_OFF(pg_model_event, kind, PG_MM_EVT_KIND);
PG_MM_ASSERT_OFF(pg_model_event, text, PG_MM_EVT_TEXT);
PG_MM_ASSERT_OFF(pg_model_event, call_id, PG_MM_EVT_CALL_ID);
PG_MM_ASSERT_OFF(pg_model_event, tool_name, PG_MM_EVT_TOOL_NAME);
PG_MM_ASSERT_SIZE(pg_model_event, PG_MM_EVT_BYTES);
PG_MM_ASSERT_OFF(pg_model, context, PG_MM_MODEL_CONTEXT);
PG_MM_ASSERT_OFF(pg_model, start, PG_MM_MODEL_START);
PG_MM_ASSERT_OFF(pg_model, next, PG_MM_MODEL_NEXT);
PG_MM_ASSERT_OFF(pg_model, cancel, PG_MM_MODEL_CANCEL);
PG_MM_ASSERT_OFF(pg_model, close, PG_MM_MODEL_CLOSE);
PG_MM_ASSERT_SIZE(pg_model, PG_MM_MODEL_BYTES);
PG_MM_ASSERT_OFF(pg_tool_definition, name, PG_MM_DEF_NAME);
PG_MM_ASSERT_OFF(pg_tool_definition, description, PG_MM_DEF_DESCRIPTION);
PG_MM_ASSERT_OFF(pg_tool_definition, schema, PG_MM_DEF_SCHEMA);
PG_MM_ASSERT_SIZE(pg_tool_definition, PG_MM_DEF_BYTES);
PG_MM_ASSERT_OFF(pg_tool_registry, entries, PG_MM_REG_ENTRIES);
PG_MM_ASSERT_OFF(pg_tool_registry, count, PG_MM_REG_COUNT);
PG_MM_ASSERT_SIZE(pg_tool_registry, PG_MM_REG_BYTES);
PG_MM_ASSERT_OFF(pg_mimo, easy, PG_MM_EASY);
PG_MM_ASSERT_OFF(pg_mimo, multi, PG_MM_MULTI);
PG_MM_ASSERT_OFF(pg_mimo, headers, PG_MM_HEADERS);
PG_MM_ASSERT_OFF(pg_mimo, body, PG_MM_BODY);
PG_MM_ASSERT_OFF(pg_mimo, text, PG_MM_TEXT);
PG_MM_ASSERT_OFF(pg_mimo, arguments, PG_MM_ARGUMENTS);
PG_MM_ASSERT_OFF(pg_mimo, id, PG_MM_ID);
PG_MM_ASSERT_OFF(pg_mimo, name, PG_MM_NAME);
PG_MM_ASSERT_OFF(pg_mimo, used, PG_MM_USED);
PG_MM_ASSERT_OFF(pg_mimo, consumed, PG_MM_CONSUMED);
PG_MM_ASSERT_OFF(pg_mimo, delivered, PG_MM_DELIVERED);
PG_MM_ASSERT_OFF(pg_mimo, failed, PG_MM_FAILED);
PG_MM_ASSERT_OFF(pg_mimo, transport_done, PG_MM_TRANSPORT_DONE);
PG_MM_ASSERT_OFF(pg_mimo, finish_reason, PG_MM_FINISH_REASON);
PG_MM_ASSERT_OFF(pg_mimo, saw_done, PG_MM_SAW_DONE);
PG_MM_ASSERT_OFF(pg_mimo, http, PG_MM_HTTP);
PG_MM_ASSERT_OFF(pg_mimo, request, PG_MM_REQUEST);
PG_MM_ASSERT_OFF(pg_mimo, tools, PG_MM_TOOLS);
PG_MM_ASSERT_SIZE(pg_mimo, PG_MM_BYTES);
_Static_assert(PG_MESSAGE_LIMIT == PG_MM_MESSAGE_LIMIT, "message limit drifted");
_Static_assert(PG_TEXT_LIMIT == PG_MM_TEXT_LIMIT, "text limit drifted");
_Static_assert(PG_ID_LIMIT == PG_MM_ID_LIMIT, "id limit drifted");
_Static_assert(sizeof(((pg_mimo *)0)->body) == PG_MM_BODY_LIMIT, "body limit drifted");
_Static_assert(PG_USER == PG_MM_ROLE_USER && PG_ASSISTANT == PG_MM_ROLE_ASSISTANT &&
               PG_TOOL_CALL == PG_MM_ROLE_TOOL_CALL && PG_TOOL_RESULT == PG_MM_ROLE_TOOL_RESULT,
               "pg_role drifted");
_Static_assert(PG_MODEL_WAIT == PG_MM_KIND_WAIT && PG_MODEL_TEXT == PG_MM_KIND_TEXT &&
               PG_MODEL_CALL == PG_MM_KIND_CALL && PG_MODEL_DONE == PG_MM_KIND_DONE &&
               PG_MODEL_ERROR == PG_MM_KIND_ERROR, "pg_model_kind drifted");
_Static_assert(json_type_null == PG_MM_JSON_NULL && json_type_int == PG_MM_JSON_INT &&
               json_type_object == PG_MM_JSON_OBJECT && json_type_array == PG_MM_JSON_ARRAY &&
               json_type_string == PG_MM_JSON_STRING, "json_type drifted");
_Static_assert(json_tokener_success == PG_MM_TOKENER_SUCCESS, "tokener success drifted");
_Static_assert(JSON_C_TO_STRING_PLAIN == PG_MM_TO_JSON_PLAIN, "plain serialization drifted");
_Static_assert(CURLOPT_TIMEOUT == PG_MM_CURLOPT_TIMEOUT, "CURLOPT_TIMEOUT drifted");
_Static_assert(CURLOPT_CONNECTTIMEOUT == PG_MM_CURLOPT_CONNECTTIMEOUT, "CURLOPT_CONNECTTIMEOUT drifted");
_Static_assert(CURLOPT_NOSIGNAL == PG_MM_CURLOPT_NOSIGNAL, "CURLOPT_NOSIGNAL drifted");
_Static_assert(CURLOPT_WRITEDATA == PG_MM_CURLOPT_WRITEDATA, "CURLOPT_WRITEDATA drifted");
_Static_assert(CURLOPT_URL == PG_MM_CURLOPT_URL, "CURLOPT_URL drifted");
_Static_assert(CURLOPT_POSTFIELDS == PG_MM_CURLOPT_POSTFIELDS, "CURLOPT_POSTFIELDS drifted");
_Static_assert(CURLOPT_HTTPHEADER == PG_MM_CURLOPT_HTTPHEADER, "CURLOPT_HTTPHEADER drifted");
_Static_assert(CURLOPT_WRITEFUNCTION == PG_MM_CURLOPT_WRITEFUNCTION, "CURLOPT_WRITEFUNCTION drifted");
_Static_assert(CURLOPT_PROTOCOLS_STR == PG_MM_CURLOPT_PROTOCOLS_STR, "CURLOPT_PROTOCOLS_STR drifted");
_Static_assert((long)CURLINFO_RESPONSE_CODE == (long)PG_MM_CURLINFO_RESPONSE_CODE, "CURLINFO_RESPONSE_CODE drifted");
_Static_assert(CURLM_OK == PG_MM_CURLM_OK, "CURLM_OK drifted");
_Static_assert(CURLMSG_DONE == PG_MM_CURLMSG_DONE, "CURLMSG_DONE drifted");
_Static_assert(CURLE_OK == PG_MM_CURLE_OK, "CURLE_OK drifted");
PG_MM_ASSERT_OFF(struct CURLMsg, msg, PG_MM_CURLMSG_MSG);
PG_MM_ASSERT_OFF(struct CURLMsg, easy_handle, PG_MM_CURLMSG_EASY);
PG_MM_ASSERT_OFF(struct CURLMsg, data, PG_MM_CURLMSG_RESULT);
PG_MM_ASSERT_SIZE(struct CURLMsg, PG_MM_CURLMSG_BYTES);
_Static_assert(sizeof("Authorization: Bearer ") - 1 == PG_MM_AUTH_PREFIX_LEN, "auth prefix length drifted");
#endif
#endif
