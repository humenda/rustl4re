#include <l4/re/c/rm.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/sys/ipc.h>
#include <l4/sys/consts.h>
#include <l4/sys/types.h>
#include <l4/util/rdtsc.h>
#include <stdio.h>

#include <algorithm>

#define MAX_RUNS 1000
l4_int64_t* SERVER_MEASUREMENTS;

static void* setup_measurements(l4_cap_idx_t srv) {
    auto size = l4_round_page(MAX_RUNS * sizeof(l4_int64_t));
    // allocate memory via dataspace manager, attach it
    auto ds = l4re_util_cap_alloc();
    // allocate memory
    int r = l4re_ma_alloc(size, ds, 0);
    if (r != 0) {
        printf("Error while allocating memory: %i\n", r);
        exit(9);
    }
    // attach to region map
    void* virt_addr;
    r = l4re_rm_attach(&virt_addr, size, L4RE_RM_SEARCH_ADDR, ds, 0,
            L4_PAGESHIFT);
    if (r != 0) { // error, free ds
        printf("Error while attaching memory to region map: %i\n", r);
        exit(10);
    }

    // send cap to server
    auto mr = l4_utcb_mr();
    (*mr).mr[0] = size;
    (*mr).mr[1] = L4_ITEM_MAP;
    (*mr).mr[2] = l4_obj_fpage(ds, 0, L4_FPAGE_RWX).raw;
    r = l4_ipc_error(l4_ipc_send(srv, l4_utcb(), l4_msgtag(9876, 1, 1, 0), L4_IPC_NEVER), l4_utcb());
    if (r != 0) {
        printf("Error while sending ds cap: %i\n", r);
        exit(1);
    }
    return virt_addr;
}

int main() {
    l4_cap_idx_t server = l4re_env_get_cap("channel");

    l4_int64_t* srv_stamps = (l4_int64_t*)setup_measurements(server);

    // cold cache call (twice)
    l4_ipc_call(server, l4_utcb(), l4_msgtag(1, 1, 0, 0), L4_IPC_NEVER);
    l4_ipc_call(server, l4_utcb(), l4_msgtag(1, 1, 0, 0), L4_IPC_NEVER);
    l4_int64_t client_measurements[MAX_RUNS];
    for (int i=0; i < MAX_RUNS; i++) {
        auto tag = l4_msgtag(1, 1, 0, 0);
        (*l4_utcb_mr()).mr[0] = i;
        client_measurements[i] = l4_rdtsc();
        l4_ipc_call(server, l4_utcb(), tag, L4_IPC_NEVER);
    }
    l4_int64_t delta[MAX_RUNS];
    for (int i=0; i<MAX_RUNS; i++)
        delta[i] = srv_stamps[i] - client_measurements[i];
    std::sort(delta, &delta[MAX_RUNS]);
    printf("Min: %lli, median: %lli, max: %lli\n",
            *std::min(&delta[0], &delta[MAX_RUNS]),
            delta[MAX_RUNS/2],
            *std::max(&delta[0], &delta[MAX_RUNS]));
}
