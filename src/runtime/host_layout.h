#ifndef PANGU_HOST_LAYOUT_H
#define PANGU_HOST_LAYOUT_H
#define PG_H_LIMIT 32
#define PG_H_PLUGINS 0
#define PG_H_COUNT 8
#define PG_H_ENTRIES 16
#define PG_H_ORDER 784
#define PG_H_STATES 912
#define PG_H_SERVICES 1040
#define PG_H_LIVE 1048
#define PG_H_PHASE 1056
#define PG_H_ERROR 1060
#define PG_H_CURRENT 1064
#define PG_H_DISPATCHING 1072
#define PG_H_ERROR_OWNER 1076
#define PG_H_PARENT 1080
#define PG_H_CHILD 1088
#define PG_H_NEXT 1096
#define PG_H_PREVIOUS 1104
#define PG_H_CHILDREN 1112
#define PG_H_BYTES 1120
#define PG_H_ENTRY_BYTES 24
#define PG_H_PLUGIN_BYTES 64
#ifndef __ASSEMBLER__
#include "host.h"
_Static_assert(sizeof(pg_host)==PG_H_BYTES,"host layout");
_Static_assert(sizeof(pg_plugin)==PG_H_PLUGIN_BYTES,"plugin layout");
_Static_assert(sizeof(pg_host_entry)==PG_H_ENTRY_BYTES,"entry layout");
_Static_assert(offsetof(pg_host,entries)==PG_H_ENTRIES,"entries");
_Static_assert(offsetof(pg_host,order)==PG_H_ORDER,"order");
_Static_assert(offsetof(pg_host,states)==PG_H_STATES,"states");
_Static_assert(offsetof(pg_host,service_count)==PG_H_SERVICES,"services");
_Static_assert(offsetof(pg_host,live_count)==PG_H_LIVE,"live");
_Static_assert(offsetof(pg_host,phase)==PG_H_PHASE,"phase");
_Static_assert(offsetof(pg_host,current)==PG_H_CURRENT,"current");
_Static_assert(offsetof(pg_host,parent)==PG_H_PARENT,"parent");
_Static_assert(offsetof(pg_host,first_child)==PG_H_CHILD,"child");
_Static_assert(offsetof(pg_host,next_sibling)==PG_H_NEXT,"next");
_Static_assert(offsetof(pg_host,previous_sibling)==PG_H_PREVIOUS,"previous");
_Static_assert(offsetof(pg_host,child_count)==PG_H_CHILDREN,"children");
_Static_assert(offsetof(pg_plugin,start)==48&&offsetof(pg_plugin,stop)==56,"callbacks");
#endif
#endif
