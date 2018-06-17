/// Documentation in the Rust version
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <l4/re/env.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/sys/consts.h>
#include <l4/sys/ipc.h>
#include <l4/sys/rcv_endpoint.h>
#include <l4/sys/types.h>
#include <l4/sys/utcb.h>
//#include <l4/sys/cxx/ipc_types>

char *read_message(l4_cap_idx_t ds, long length);


inline void setup_utcb(l4_cap_idx_t ds) {
    l4_cap_idx_t rcv_ds = ds << L4_CAP_SHIFT;
    (*l4_utcb_br()).br[0] = rcv_ds | L4_RCV_ITEM_SINGLE_CAP
            |  L4_RCV_ITEM_LOCAL_ID;
    (*l4_utcb_br()).br[1] = 0;
    (*l4_utcb_br()).bdr = 0;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    auto gate = l4re_env_get_cap("channel");
    if (l4_is_invalid_cap(gate)) {
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
    l4_umword_t l;
    l4_cap_idx_t ds = l4re_util_cap_alloc();
    setup_utcb(ds); // prepare for receiving cap
    l4_msgtag_t tag;
    tag = l4_ipc_wait(l4_utcb(), &l, L4_IPC_NEVER);

    while (1) {
        if ((r = l4_ipc_error(tag, l4_utcb())) == 0) {
            if (l4_is_valid_cap(ds)) {
                long textlen = l4_utcb_mr()->mr[1];
                printf("received cap %lx, text length parameter %lx\n", ds, textlen);
                char *msg = read_message(ds, textlen);
                // print first 20 chars of a non-null terminated string
                printf("%.*s\n", 20, msg);
                l4re_util_cap_free(ds);
            } else {
                printf("errorneous cap received, trying again\n");
            }

            // reserve new cap slot for next receive
            ds = l4re_util_cap_alloc();
        } else {
            printf("broken cap received\n");
            // wait again (slot still available)
        }

        setup_utcb(ds);
        tag = l4_ipc_reply_and_wait(l4_utcb(), l4_msgtag(0, 0, 0, 0), &l,
                L4_IPC_NEVER);
    }
}

char *read_message(l4_cap_idx_t ds_cap, long length) {
    void *virt_addr;
    int page_size, r;
    page_size = l4_round_page(length);
    l4re_ds_stats_t stats;
    if ((r = l4re_ds_info(ds_cap, &stats))) {
        printf("stats info gave error %i\n", r & L4_IPC_ERROR_MASK); 
        //exit(33);
    }
    printf("Ds size info: %lx\n", stats.size);

    printf("attaching memory: %x bytes\n", page_size);
    if ((r = l4re_rm_attach(&virt_addr, page_size, L4RE_RM_SEARCH_ADDR,
            ds_cap, 0, L4_PAGESHIFT))) {
        printf("unable to attach memory from dataspace: %i\n", r);
        exit(92);
    }

    printf("copying string, length: %lx; base: %p\n", length, virt_addr);
    char *s = (char *)malloc(length + 1);
    memcpy(s, (char *)virt_addr, length);
    s[length] = '\0';
    printf("copied\n");
    printf("detaching memory…\n");
    l4re_rm_detach(virt_addr);
    return s;
}
