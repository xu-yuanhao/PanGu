#ifndef PANGU_INTEROP_GUARD_H
#define PANGU_INTEROP_GUARD_H
typedef unsigned long long pangu_interop_u64;
typedef pangu_interop_u64 (*pangu_interop_guard_fn)(void (*)(void),const pangu_interop_u64 *,unsigned int,unsigned int);
#ifdef __cplusplus
extern "C" {
void pangu_route_cpp_set_guard(pangu_interop_guard_fn) noexcept;
#else
void pangu_route_cpp_set_guard(pangu_interop_guard_fn);
#endif
#ifdef __cplusplus
}
#endif
#endif
