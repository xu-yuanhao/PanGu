#define _GNU_SOURCE
#include "resource_select.h"
#include <fnmatch.h>
#include <locale.h>
int pg_resource_fnmatch(const char *pattern,const char *path){
 locale_t locale=newlocale(LC_CTYPE_MASK,"C.UTF-8",(locale_t)0);if(!locale)return -1;
 locale_t previous=uselocale(locale);if(!previous){freelocale(locale);return -1;}
 int status=fnmatch(pattern,path,FNM_PATHNAME|FNM_PERIOD|FNM_EXTMATCH);
 uselocale(previous);freelocale(locale);return status;
}
