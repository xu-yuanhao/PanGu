#ifndef PG_PROFILE_LAYOUT_H
#define PG_PROFILE_LAYOUT_H
#define PG_P_SPEC_BYTES 88
#define PG_P_TOOLS 64
#define PG_P_DEVICE 72
#define PG_P_INODE 80
#ifndef __ASSEMBLER__
#include "profile.h"
#include "tools_layout.h"
_Static_assert(sizeof(pg_profile_spec)==PG_P_SPEC_BYTES,"profile spec size");
_Static_assert(offsetof(pg_profile_spec,provider)==0&&offsetof(pg_profile_spec,model)==16,"profile model fields");
_Static_assert(offsetof(pg_profile_spec,workspace)==32&&offsetof(pg_profile_spec,implementation)==48,"profile strings");
_Static_assert(offsetof(pg_profile_spec,tools)==PG_P_TOOLS,"profile tools");
_Static_assert(offsetof(pg_profile_spec,device)==PG_P_DEVICE&&offsetof(pg_profile_spec,inode)==PG_P_INODE,"profile filesystem identity");
_Static_assert(sizeof(pg_tool_definition)==72&&offsetof(pg_tool_registry,count)==1152,"profile registry layout");
#endif
#endif
