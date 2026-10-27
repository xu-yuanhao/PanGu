#define _POSIX_C_SOURCE 200809L
#include "tool_path.h"
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
/* libc owns the fallback record; the serialized caller copies it immediately. */
const char *pg_tool_path_home(void){const char *home=getenv("HOME");if(home&&*home)return home;struct passwd *entry=getpwuid(getuid());return entry?entry->pw_dir:0;}
