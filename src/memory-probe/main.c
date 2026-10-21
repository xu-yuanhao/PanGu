#include "src/bootstrap-api/memory.h"
/* Test composition alone selects providers; run_checks uses only an injected API. */
static pangu_i32 run_checks(const pangu_memory_api *api)
{
 pangu_mapping m, second, bad;
 m.address=(void *)0;m.length=0;second.address=(void *)0;second.length=0;
 if (api->abi_version!=1 || api->struct_size!=40 || api->reserved) return 1;
 if (api->map(api->context,1,(void *)0)!=1) return 2;
 if (api->map(api->context,0,&m)!=1) return 3;
 if (api->map(api->context,~(pangu_u64)0,&m)!=1) return 4;
 if (m.address || m.length) return 5;
 if (api->map(api->context,(pangu_u64)1 << 63,&m)!=2 || m.address || m.length) return 20;
 if (api->unmap(api->context,(void *)0)!=1) return 6;
 if (api->unmap(api->context,&m)!=1) return 7;
 if (api->map(api->context,1,&m)!=0 || m.length!=4096 || !m.address) return 8;
 volatile unsigned char *bytes=m.address;
 for (pangu_u64 i=0;i<m.length;i++) if (bytes[i]!=0) return 9;
 bytes[0]=17; bytes[m.length-1]=29;
 if (bytes[0]!=17 || bytes[m.length-1]!=29) return 10;
 bad.address=(unsigned char *)m.address+1;bad.length=m.length;
 if (api->unmap(api->context,&bad)!=1 || bytes[0]!=17) return 21;
 bad.address=m.address;bad.length=m.length-1;
 if (api->unmap(api->context,&bad)!=1 || bytes[0]!=17) return 22;
 bad.address=(void *)(~(pangu_u64)4095);bad.length=4096;
 if (api->unmap(api->context,&bad)!=1) return 23;

 void *original=m.address;
 if (api->map(api->context,8192,&m)!=5 || m.address!=original || m.length!=4096) return 11;
 if (api->map(api->context,4097,&second)!=0 || second.length!=8192 || second.address==m.address) return 12;
 volatile unsigned char *other=second.address;other[0]=31;other[8191]=37;
 if (bytes[0]!=17 || bytes[4095]!=29) return 13;
 if (api->unmap(api->context,&second)!=0 || second.address || second.length) return 14;
 if (bytes[0]!=17) return 15;
 if (api->unmap(api->context,&m)!=0 || m.address || m.length) return 16;
 if (api->unmap(api->context,&m)!=1) return 17;
 for (pangu_u32 i=0;i<64;i++) {
  if (api->map(api->context,17,&m)!=0) return 18;
  ((volatile unsigned char *)m.address)[0]=42;
  if (api->unmap(api->context,&m)!=0 || m.address || m.length) return 19;
 }
 return 0;
}
int memory_test(const pangu_memory_api *api)
{
 return run_checks(api);
}
