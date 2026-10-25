#ifndef PG_TOOLS_LAYOUT_H
#define PG_TOOLS_LAYOUT_H
/* tools.S 与 C 测试共享的布局/枚举常量；数值只有这一份，C 侧断言与
 * tools.h/契约头的真实布局一致，漂移即编译失败。 */
#define PG_TL_LIMIT 16
#define PG_TL_TEXT_LIMIT 4096
#define PG_TL_ID_LIMIT 64

/* pg_slice（src/contracts/client.h） */
#define PG_TL_SLICE_DATA 0
#define PG_TL_SLICE_SIZE 8
#define PG_TL_SLICE_BYTES 16

/* pg_tool_definition（tools.h）；聚合按 MEMORY 经 sret 指针返回 */
#define PG_TL_DEF_NAME 0
#define PG_TL_DEF_DESCRIPTION 8
#define PG_TL_DEF_SCHEMA 16
#define PG_TL_DEF_CONTEXT 24
#define PG_TL_DEF_VALIDATE 32
#define PG_TL_DEF_AUTHORIZE 40
#define PG_TL_DEF_EXECUTE 48
#define PG_TL_DEF_REPLAY_SAFE 56
#define PG_TL_DEF_ASYNC 64
#define PG_TL_DEF_BYTES 72

/* pg_tool_registry（tools.h） */
#define PG_TL_REG_ENTRIES 0
#define PG_TL_REG_COUNT 1152
#define PG_TL_REG_ACTIVE 1160
#define PG_TL_REG_STATE 1168
#define PG_TL_REG_CANCEL 1172
#define PG_TL_REG_DEPTH 1176
#define PG_TL_REG_BYTES 1184

/* pg_tools 服务函数表（agent.h）；56 字节聚合经 sret 返回 */
#define PG_TL_SVC_CONTEXT 0
#define PG_TL_SVC_AUTHORIZE 8
#define PG_TL_SVC_EXECUTE 16
#define PG_TL_SVC_REPLAY_SAFE 24
#define PG_TL_SVC_NEXT 32
#define PG_TL_SVC_CANCEL 40
#define PG_TL_SVC_CLOSE 48
#define PG_TL_SVC_BYTES 56

/* json-c 公开枚举值（<json-c/json.h>，断言对齐） */
#define PG_TL_JSON_OBJECT 4
#define PG_TL_JSON_STRING 6
#define PG_TL_TOKENER_SUCCESS 0

#ifndef __ASSEMBLER__
#include <stddef.h>
#include "agent.h"
#include "tools.h"
#include <json-c/json.h>
#define PG_TL_ASSERT_OFF(t, f, v) _Static_assert(offsetof(t, f) == (v), #t "." #f " layout drifted")
#define PG_TL_ASSERT_SIZE(t, v) _Static_assert(sizeof(t) == (v), #t " size drifted")
PG_TL_ASSERT_OFF(pg_slice, data, PG_TL_SLICE_DATA);
PG_TL_ASSERT_OFF(pg_slice, size, PG_TL_SLICE_SIZE);
PG_TL_ASSERT_SIZE(pg_slice, PG_TL_SLICE_BYTES);
PG_TL_ASSERT_OFF(pg_tool_definition, name, PG_TL_DEF_NAME);
PG_TL_ASSERT_OFF(pg_tool_definition, description, PG_TL_DEF_DESCRIPTION);
PG_TL_ASSERT_OFF(pg_tool_definition, schema, PG_TL_DEF_SCHEMA);
PG_TL_ASSERT_OFF(pg_tool_definition, context, PG_TL_DEF_CONTEXT);
PG_TL_ASSERT_OFF(pg_tool_definition, validate, PG_TL_DEF_VALIDATE);
PG_TL_ASSERT_OFF(pg_tool_definition, authorize, PG_TL_DEF_AUTHORIZE);
PG_TL_ASSERT_OFF(pg_tool_definition, execute, PG_TL_DEF_EXECUTE);
PG_TL_ASSERT_OFF(pg_tool_definition, replay_safe, PG_TL_DEF_REPLAY_SAFE);
PG_TL_ASSERT_OFF(pg_tool_definition, async, PG_TL_DEF_ASYNC);
PG_TL_ASSERT_SIZE(pg_tool_definition, PG_TL_DEF_BYTES);
PG_TL_ASSERT_OFF(pg_tool_registry, entries, PG_TL_REG_ENTRIES);
PG_TL_ASSERT_OFF(pg_tool_registry, count, PG_TL_REG_COUNT);
PG_TL_ASSERT_OFF(pg_tool_registry, active, PG_TL_REG_ACTIVE);
PG_TL_ASSERT_OFF(pg_tool_registry, operation_state, PG_TL_REG_STATE);
PG_TL_ASSERT_OFF(pg_tool_registry, cancel_sent, PG_TL_REG_CANCEL);
PG_TL_ASSERT_OFF(pg_tool_registry, dispatch_depth, PG_TL_REG_DEPTH);
PG_TL_ASSERT_SIZE(pg_tool_registry, PG_TL_REG_BYTES);
PG_TL_ASSERT_OFF(pg_tools, context, PG_TL_SVC_CONTEXT);
PG_TL_ASSERT_OFF(pg_tools, authorize, PG_TL_SVC_AUTHORIZE);
PG_TL_ASSERT_OFF(pg_tools, execute, PG_TL_SVC_EXECUTE);
PG_TL_ASSERT_OFF(pg_tools, replay_safe, PG_TL_SVC_REPLAY_SAFE);
PG_TL_ASSERT_OFF(pg_tools, next, PG_TL_SVC_NEXT);
PG_TL_ASSERT_OFF(pg_tools, cancel, PG_TL_SVC_CANCEL);
PG_TL_ASSERT_OFF(pg_tools, close, PG_TL_SVC_CLOSE);
PG_TL_ASSERT_SIZE(pg_tools, PG_TL_SVC_BYTES);
PG_TL_ASSERT_SIZE(pg_tool_async, 24);
PG_TL_ASSERT_OFF(pg_tool_async, next, 0);
PG_TL_ASSERT_OFF(pg_tool_async, cancel, 8);
PG_TL_ASSERT_OFF(pg_tool_async, close, 16);
_Static_assert(PG_TOOL_OK==0&&PG_TOOL_ERROR==1&&PG_TOOL_WAIT==2&&PG_TOOL_OUTPUT==3,"tool statuses");
_Static_assert(PG_TOOL_LIMIT == PG_TL_LIMIT, "tool limit drifted");
_Static_assert(PG_TEXT_LIMIT == PG_TL_TEXT_LIMIT, "text limit drifted");
_Static_assert(PG_ID_LIMIT == PG_TL_ID_LIMIT, "id limit drifted");
_Static_assert(json_type_object == PG_TL_JSON_OBJECT, "json_type_object drifted");
_Static_assert(json_type_string == PG_TL_JSON_STRING, "json_type_string drifted");
_Static_assert(json_tokener_success == PG_TL_TOKENER_SUCCESS, "tokener success drifted");
#endif
#endif
