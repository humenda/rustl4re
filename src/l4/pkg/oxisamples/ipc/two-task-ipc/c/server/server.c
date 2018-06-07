#include <stdio.h>
#include <l4/sys/ipc.h>
#include <l4/re/env.h>
#include <l4/sys/ipc_gate.h>

int main(int argc, char **argv) {
    int r;
    (void)argc;
    (void)argv;
    l4_umword_t gatelabel = 0b11111100;
    l4_umword_t label;
    l4_cap_idx_t gate = l4re_env_get_cap("channel");
    if (l4_is_invalid_cap(gate)) {
        printf("No IPC Gate found.\n");
        return 9;
    }
    if ((r = l4_ipc_error(l4_rcv_ep_bind_thread(gate, (*l4re_env()).main_thread,
            gatelabel), l4_utcb()))) {
        printf("Error while binding IPC gate: %i\n", r);
        return 8;
    }
    printf("IPC gate received and bound.\n");

    // square server
    // Wait for requests from any thread. No timeout, i.e. wait forever.
    printf("waiting for incoming connections\n");
    l4_msgtag_t tag = l4_ipc_wait(l4_utcb(), &label, L4_IPC_NEVER);
    while (1) {
        // Check if we had any IPC failure, if yes, print the error code and
        // just wait again.
        if ((r = l4_ipc_error(tag, l4_utcb()))) {
            printf("server: IPC error: %i\n", r);
            continue;
        }

        // the IPC was ok, now take the value out of message register 0 of the
        // UTCB and store the square of it back to it.
        int val = (*l4_utcb_mr()).mr[0];
        (*l4_utcb_mr()).mr[0] = val * val;
        printf("new value: %li\n", (*l4_utcb_mr()).mr[0]);

        // send reply and wait again for new messages.
        // The '1' in msgtag indicates that 1 word transfered from first
        // register
        tag = l4_ipc_reply_and_wait(l4_utcb(), l4_msgtag(0, 1, 0, 0), &label,
                L4_IPC_NEVER);
    }
}


