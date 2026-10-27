#define _GNU_SOURCE
#include "model_failure.h"
#include <limits.h>
#include <regex.h>
/* regex_t/regmatch_t are opaque platform layouts. REG_STARTEND preserves the
   explicit provider-buffer length, including consumed SSE NUL separators. */
int pg_model_regex(pg_slice text,const char *pattern){
 if(!text.data||text.size>INT_MAX)return -1;
 regex_t expression;
 if(regcomp(&expression,pattern,REG_EXTENDED|REG_ICASE))return -1;
 regmatch_t range={0,(regoff_t)text.size};
 int result=regexec(&expression,text.data,1,&range,REG_STARTEND);
 regfree(&expression);
 return result==0?1:result==REG_NOMATCH?0:-1;
}
