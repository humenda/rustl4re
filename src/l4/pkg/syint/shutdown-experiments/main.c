#include <stdio.h>
#include <stdlib.h>
#include <l4/re/env.h>
#include <l4/sys/platform_control.h>
#include <l4/sys/types.h>
#include <l4/sys/ipc.h>

void shutdown(void);

void shutdown() {
    l4_cap_idx_t pfc = l4re_env_get_cap("pfc");
    if (l4_is_invalid_cap(pfc)) {
        printf("Invalid platform control capability, insufficient task rights?\n");
        exit(1);
    }
    l4_msgtag_t tag = l4_platform_ctl_system_shutdown(pfc, 0);
    if (l4_msgtag_has_error(tag)) {
        printf("Error while shutting down: %li\n",
                    l4_ipc_error(tag, l4_utcb()));
        exit(9);
    }
}

int main(int argc, const char *argv[])
{
    printf("Hey! Bye!\n");
    shutdown();
    printf("test\n");
    exit(0);
}
