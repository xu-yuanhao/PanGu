#ifndef PANGU_AGENT_TEST_SLICES_H
#define PANGU_AGENT_TEST_SLICES_H
#include <pangu/core/core.h>
#include <string.h>
/* Core/resource slices need not have a trailing NUL. Only needle is a C string.
 */
static inline int slice_contains(pangu_slice haystack, const char *needle) {
  size_t n = strlen(needle);
  if (n > haystack.len)
    return 0;
  if (!n)
    return 1;
  for (size_t i = 0; i <= haystack.len - n; i++)
    if (!memcmp(haystack.data + i, needle, n))
      return 1;
  return 0;
}
#endif
