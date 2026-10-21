#ifndef PANGU_INTEROP_CONTRACT_V1_H
#define PANGU_INTEROP_CONTRACT_V1_H
/* Independent fixed service-consumer fixture. No lifecycle/route headers.
 * SysV Linux x86_64, little-endian, C ABI; no C++ objects across the boundary.
 * Methods operate only under an existing route lease. This is not an SDK.
 */
#define PANGU_INTEROP_SERVICE_SIZE 24
#define PANGU_INTEROP_METHOD_OFFSET 16
#ifndef __ASSEMBLER__
#ifdef __cplusplus
#define PANGU_INTEROP_ASSERT static_assert
#define PANGU_INTEROP_NOEXCEPT noexcept
extern "C" {
#else
#define PANGU_INTEROP_ASSERT _Static_assert
#define PANGU_INTEROP_NOEXCEPT
#endif
typedef unsigned int pangu_interop_u32;
typedef int pangu_interop_i32;
typedef pangu_interop_i32 (*pangu_interop_method)(void *,pangu_interop_i32,pangu_interop_i32 *);
typedef struct {pangu_interop_u32 version,struct_size,target,reserved;} pangu_interop_header;
typedef struct {pangu_interop_header header;pangu_interop_method evaluate;} pangu_interop_table;
typedef struct {pangu_interop_i32 value,calls;} pangu_interop_state;
pangu_interop_i32 pangu_route_cpp_invoke(const void *,void *,pangu_interop_u32,pangu_interop_i32,pangu_interop_i32 *) PANGU_INTEROP_NOEXCEPT;
pangu_interop_i32 pangu_route_asm_invoke(const void *,void *,pangu_interop_u32,pangu_interop_i32,pangu_interop_i32 *);
pangu_interop_i32 pangu_route_asm_preserve(const void *,void *,pangu_interop_u32,pangu_interop_i32,pangu_interop_i32 *);
pangu_interop_i32 pangu_route_cpp_evaluate(void *,pangu_interop_i32,pangu_interop_i32 *) PANGU_INTEROP_NOEXCEPT;
void pangu_cpp_provider_observed(void *);
PANGU_INTEROP_ASSERT(sizeof(pangu_interop_u32)==4,"u32");
PANGU_INTEROP_ASSERT(sizeof(pangu_interop_i32)==4,"i32");
PANGU_INTEROP_ASSERT(sizeof(void *)==8,"pointer");
PANGU_INTEROP_ASSERT(sizeof(pangu_interop_header)==16,"header");
PANGU_INTEROP_ASSERT(sizeof(pangu_interop_table)==PANGU_INTEROP_SERVICE_SIZE,"table");
PANGU_INTEROP_ASSERT(__builtin_offsetof(pangu_interop_table,evaluate)==PANGU_INTEROP_METHOD_OFFSET,"method offset");
PANGU_INTEROP_ASSERT(sizeof(pangu_interop_state)==8,"state");
#ifdef __cplusplus
}
#endif
#undef PANGU_INTEROP_ASSERT
#undef PANGU_INTEROP_NOEXCEPT
#endif
#endif
