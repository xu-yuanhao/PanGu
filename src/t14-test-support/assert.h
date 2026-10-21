#ifndef TEST_ASSERT_H
#define TEST_ASSERT_H
_Noreturn void probe_assert(const char *,const char *,int);
#define assert(x) ((x)?(void)0:probe_assert(#x,__FILE__,__LINE__))
#endif
