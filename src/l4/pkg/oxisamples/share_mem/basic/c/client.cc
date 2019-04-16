#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <l4/re/c/dataspace.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/sys/ipc.h>

#define PANIC(x) printf(x);exit(1)
#define PANIC1(x,y) printf(x, y);exit(1)
#define PANIC2(x,y,z) printf(x, y,z);exit(1)

l4_cap_idx_t allocate_ds(void **virt_addr, long size_in_bytes);
void free_ds(void *base, l4_cap_idx_t ds);
void send_cap(l4_cap_idx_t ds, long textlen, l4_cap_idx_t dst);

l4_cap_idx_t allocate_ds(void **base, long size_in_bytes) {
    void *virt_addr;
    int err;
    // allocate ds capability slot
    l4_cap_idx_t ds = l4re_util_cap_alloc();
    if (l4_is_invalid_cap(ds)) {
        PANIC("Unable to acquire capability slot for l4mem\n");
    }
    /*
    if ((err = l4re_ds_allocate(ds, 0, size_in_bytes))) {
        printf("IPC error %d, ", (err & L4_IPC_ERROR_MASK));
        PANIC2("failed to allocate %lx bytes for a dataspace with cap index %lx\n",
                size_in_bytes, ds);
    }
    */
    // map memory into ds
    if ((err = l4re_ma_alloc(size_in_bytes, ds, 0)) != 0) {
        PANIC1("Unable to allocate memory for l4mem: %i", err);
    }
    // attach to region map
    if ((err = l4re_rm_attach(&virt_addr, size_in_bytes,
              L4RE_RM_SEARCH_ADDR, ds, 0, L4_PAGESHIFT)) != 0) {
        l4re_util_cap_free_um(ds);
        PANIC1("Allocation failed with exit code %i", err);
    }
    l4re_ds_stats_t stats;
    l4re_ds_info(ds, &stats);
    if (stats.size != l4_round_page(size_in_bytes)) {
        PANIC2("memory allocation failed: got %lx, required %lx",
                 stats.size, l4_round_page(size_in_bytes));
    }

    // Todo: is there a more elegant solution? direct pointer assignment doesn't
    // work and is lost after return
    *base = virt_addr;
    return ds;
}

void free_ds(void *base, l4_cap_idx_t ds) {
    int err;
    if ((err = l4re_rm_detach(base)) != 0) {
        PANIC1("allocation failed with %i\n", err);
    }
    l4re_util_cap_free_um(ds);
}

void send_cap(l4_cap_idx_t ds, long textlen, l4_cap_idx_t dst) {
    int err;
    auto mr = l4_utcb_mr();
    printf("cap info: index %lx, destination: %lx, memory size: %lx",
             ds, dst, textlen);
    // if flex pages and data words are sent, flex pages need to be the last
    // items
    (*mr).mr[0] = textlen;
    // sending a capability requires to registers, one containing the action,
    // the other the flexpage
    (*mr).mr[1] = L4_ITEM_MAP;
    (*mr).mr[2] = l4_obj_fpage(ds, 0, L4_FPAGE_RWX).raw;
    if ((err = l4_ipc_error(l4_ipc_call(dst, l4_utcb(),
            l4_msgtag(0, 1, 1, 0), L4_IPC_NEVER), l4_utcb())) != 0) {
        PANIC1("IPC error while sending capability: %i", err);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    auto server = l4re_env_get_cap("channel");
    if (l4_is_invalid_cap(server)) {
        PANIC("No IPC Gate found.");
    }

    // write a thousand sentences
    char *text = (char *)malloc(23 * 1000 * sizeof(char) + 1), *ptr = text;
    for (int i=0; i<1000;
            strcpy(ptr, "This is useless text. "), i++, ptr += 22);
    ptr++;
    ptr[0] = '\0';
    long page_bytes = l4_round_page(strlen(text));
    void *base = NULL;
    l4_cap_idx_t ds = allocate_ds(&base, page_bytes);
    printf("allocated %lx B for a string with %lx B, mapped to %p\n",
            page_bytes, strlen(text), base);
    printf("copying text into dataspace, mapped at %p\n", base);
    strcpy((char *)base, text);

    printf("sending cap %lx\n", ds);
    send_cap(ds, strlen(text), server);
    free_ds(base, ds);
    printf("sending successful\n");
    printf("bye\n");
}
