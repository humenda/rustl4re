#include <stdio.h>
#include <l4/sys/types.h>
#include <l4/re/env.h>
#include <l4/sys/__timeout.h>
#include <l4/sys/ipc.h>
#include <l4/sys/ipc_gate.h>
//#include <l4/thread/thread.h>

int main(int _argc, char **_argv) {
    l4_cap_idx_t gate = l4re_env_get_cap("channel");
    if (l4_is_invalid_cap(gate)) {
        printf("No IPC Gate found.\n");
        return 2;
    }
    int counter = 1, r;
    while (1) {
        // dump value in UTCB
        (*l4_utcb_mr()).mr[0] = counter;
        l4_msgtag_t tag = l4_ipc_call(gate, l4_utcb(), l4_msgtag(0, 1, 0, 0),
                L4_IPC_NEVER);
                        //l4_timeout_t { raw: 0 });
        // ToDo: ^ try L4_TIMEOUT_NEVER
        printf("data sent\n");
        if ((r = l4_ipc_error(tag, l4_utcb())) != 0) {
            printf("IPC error: %i\n", r);
        }
        //l4thread_sleep(2000);
        counter++;
    }
}


