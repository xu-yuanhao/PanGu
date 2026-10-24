#include "src/bootstrap-api/bootstrap.h"

pangu_i32 pangu_bootstrap_run(const pangu_platform_api *platform, pangu_i32 status)
{
    if (!platform || platform->abi_version != PANGU_PLATFORM_ABI_VERSION ||
        platform->struct_size != PANGU_PLATFORM_STRUCT_SIZE ||
        platform->reserved != 0 || !platform->exit_process || status < 0 || status > 255)
        return PANGU_BOOTSTRAP_INVALID;
    platform->exit_process(platform->context, status);
    return PANGU_BOOTSTRAP_EXIT_RETURNED;
}
