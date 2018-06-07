#include <stdio.h>
#include <stdlib.h>
#include <l4/sys/rcv_endpoint.h>
#include <l4/re/env.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/sys/ipc.h>
#include <l4/sys/types.h>
#include <l4/sys/utcb.h>
// ToDo: wech
#include <l4/sys/cxx/ipc_types>

int main(int argc, char **argv) {
    auto gate = ::l4re_env_get_cap("channel");
    if(l4_is_invalid_cap(gate)) {
        printf("No IPC Gate found.\n");
        exit(9);
    }
    int r;
    if ((r = l4_ipc_error(l4_rcv_ep_bind_thread(gate, (*l4re_env()).main_thread,
            0b111100), l4_utcb()))) {
        printf("Error while binding IPC gate: %i\n", r);
        exit(10);
    }
    printf("IPC gate received and bound.\n");
    while (1) {
        auto ds = l4re_util_cap_alloc();
        if (l4_is_invalid_cap(ds)) {
            printf("allocating capability failed.\n");
            exit(8);
        }
        l4_umword_t l; // receive label
//        auto rcv_ds = ds << cap::l4_cap_consts_t_L4_CAP_SHIFT;
//        (*l4_utcb_br()).br[0] = rcv_ds
//                | cap::l4_msg_item_consts_t_L4_RCV_ITEM_SINGLE_CAP as u64
//                |  cap::l4_msg_item_consts_t_L4_RCV_ITEM_LOCAL_ID as u64;
//        (*l4_utcb_br()).br[1] = 0;
//        (*l4_utcb_br()).bdr = 0;
        l4_utcb_br()->br[0] = L4::Ipc::Small_buf(ds << L4_CAP_SHIFT,
                L4_RCV_ITEM_LOCAL_ID).raw();
        l4_utcb_br()->br[1] = 0;
        l4_utcb_br()->bdr = 0;

        auto tag = l4_ipc_wait(l4_utcb(), &l, L4_IPC_NEVER);
        if ((r = l4_ipc_error(tag, l4_utcb()))) {
            printf("IPC error %i\n", r);
            continue;
        }
        if (l4_is_invalid_cap(ds)) {
            printf("broken cap received\n");
        } else {
            printf("received good cap, ToDo\n");
        }
        printf("Exiting with 80\n");
        exit(80);
    }
}

