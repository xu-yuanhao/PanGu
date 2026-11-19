/* Small platform boundary: execution duration requires a monotonic clock. */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdint.h>
#include <time.h>
/* UTC provenance clock, independent of duration timing. Android API 26 does
 * not expose C17 timespec_get; its POSIX realtime clock has the same units. */
int64_t pangu_core_wallclock_ms(void) {
    struct timespec value;
#if defined(__ANDROID__)
    if (clock_gettime(CLOCK_REALTIME,&value)) return 0;
#else
    if (timespec_get(&value,TIME_UTC)!=TIME_UTC) return 0;
#endif
    return (int64_t)value.tv_sec*INT64_C(1000)+value.tv_nsec/1000000;
}
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
uint64_t pangu_core_monotonic_ns(void) {
    LARGE_INTEGER frequency,counter;
    if (!QueryPerformanceFrequency(&frequency) || !QueryPerformanceCounter(&counter)) return 0;
    uint64_t value=(uint64_t)counter.QuadPart,scale=(uint64_t)frequency.QuadPart;
    return (value/scale)*UINT64_C(1000000000)+(value%scale)*UINT64_C(1000000000)/scale;
}
#else
uint64_t pangu_core_monotonic_ns(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC,&value)) return 0;
    return (uint64_t)value.tv_sec*UINT64_C(1000000000)+(uint64_t)value.tv_nsec;
}
#endif
