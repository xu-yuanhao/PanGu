#include "src/bootstrap-api/memory.h"
#include "src/t14-test-platform/platform.h"
pangu_i32 guard_fault(pangu_u64 right) {
 pangu_mapping mapping={0};if(pangu_memory_map(0,4096,&mapping))return 11;
 volatile unsigned char *bytes=mapping.address;bytes[0]=0;bytes[4095]=0;
 const char *marker=right?"guard-right-ready\n":"guard-left-ready\n";pangu_u64 length=right?18:17;
 if(probe_write(1,marker,length)!=(long)length)return 12;
 volatile unsigned char *fault=(volatile unsigned char *)((pangu_u64)mapping.address+(right?4096:~(pangu_u64)0));
 return *fault;
}
