#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <sys/stat.h>
/* Public struct stat layout stays with the platform compiler. */
int pg_journal_file_size(int descriptor, uint64_t *size) {
    struct stat info;
    if (fstat(descriptor, &info) || !S_ISREG(info.st_mode) || info.st_size < 0) return 1;
    *size = (uint64_t)info.st_size;
    return 0;
}
