#ifndef PANGU_COMPILER_MEMORY_H
#define PANGU_COMPILER_MEMORY_H
typedef __SIZE_TYPE__ pangu_size;
void *memcpy(void *,const void *,pangu_size);
void *memmove(void *,const void *,pangu_size);
void *memset(void *,int,pangu_size);
#endif
