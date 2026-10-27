#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
/* DIR/dirent/stat layouts belong to the platform compiler. Name is borrowed
   until next/close; result is 0=end, 1=entry, -1=platform error. */
int pg_jsonl_directory_next(void *directory,const char **name,unsigned *regular) {
 errno=0;
 struct dirent *entry=readdir(directory);
 if(!entry)return errno?-1:0;
 struct stat info;
 *name=entry->d_name;
 if(fstatat(dirfd(directory),entry->d_name,&info,AT_SYMLINK_NOFOLLOW))return -1;
 *regular=S_ISREG(info.st_mode);
 return 1;
}
