#include <l4/re/c/rm.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/sys/ipc.h>
#include <l4/sys/consts.h>
#include <l4/sys/types.h>
#include <l4/util/rdtsc.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/sys/rcv_endpoint.h>

#include <stdio.h>
#include <algorithm>

#define MAX_RUNS 1000

// set up shared memory to transmit measurements to the client
static l4_int64_t* setup_shm() {
    auto ds = l4re_util_cap_alloc();
    auto br = l4_utcb_br();
    br->bdr = 0;
    br->br[0] = ds | L4_RCV_ITEM_SINGLE_CAP | L4_RCV_ITEM_LOCAL_ID;
    l4_umword_t label; // gate is already registered
    auto tag = l4_ipc_wait(l4_utcb(), &label, L4_IPC_NEVER);
    if (l4_ipc_error(tag, l4_utcb()))
        throw l4_ipc_error(tag, l4_utcb());
    auto size = l4_utcb_mr()->mr[0];
    void* ds_start;
    // attach received dataspace to virtual memory region and auto the
    // measurement functions point to it
    auto err = l4re_rm_attach(&ds_start, size, L4RE_RM_SEARCH_ADDR, ds, 0, L4_PAGESHIFT);
    if (err != 0)
        printf("error while attaching memory: %i", err);
    // null the memory
    auto shm = (l4_int64_t *)ds_start;
    for (int i = 0; i < MAX_RUNS; shm[i] = 0, i++);
    return shm;
}

int main() {
    printf("Starting server\n");
    auto chan = l4re_env_get_cap("channel");
    // bind gate
    int r;
    if ((r = l4_ipc_error(l4_rcv_ep_bind_thread(chan,
            (*l4re_env()).main_thread, 0xc01dc0ffee), l4_utcb()))) {
        printf("Error while binding IPC gate: %i\n", r);
        exit(9);
    }

    l4_int64_t* srv_stamps = setup_shm();
    l4_umword_t label = 0;
    // warm up cache
    auto tag = l4_ipc_wait(l4_utcb(), &label, L4_IPC_NEVER);
    tag = l4_ipc_reply_and_wait(l4_utcb(), l4_msgtag(0, 0, 0, 0), &label,
            L4_IPC_NEVER);

    for (int i = 0; i < MAX_RUNS; i++) {
        tag = l4_ipc_reply_and_wait(l4_utcb(), l4_msgtag(0, 0, 0, 0), &label,
                L4_IPC_NEVER);
        srv_stamps[i] = l4_rdtscp();
    }
    // final reply needs to be sent to client, didn't figure out how to send reply without wait
    tag = l4_ipc_reply_and_wait(l4_utcb(), l4_msgtag(0, 0, 0, 0), &label,
            L4_IPC_NEVER);
    printf("Done\n");
}
