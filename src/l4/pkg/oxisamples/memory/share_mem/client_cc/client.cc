#include <cstring>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/sys/err.h>
#include <l4/sys/types.h>
#include <stdio.h>
#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>
#include "../interface.h"

struct ServerWrapper {
    L4::Cap<Shm> channel;

    ServerWrapper() {
        this->channel = L4Re::Env::env()->get_cap<Shm>("channel");
        if (!channel.is_valid()) {
            printf("Could not get server capability!\n");
            throw 0;
        }
    }

    int send(char *message) {
        if (!message)
            return -1;
        unsigned int length = strlen(message);
        void *shared_memory = 0;
        int err;
        unsigned long size_in_bytes = 0;
        for (;size_in_bytes < length; size_in_bytes += L4_PAGESIZE);
        size_in_bytes = l4_trunc_page(size_in_bytes);
        L4::Cap<L4Re::Dataspace> ds_cap;
        if ((err = this->allocate_mem(size_in_bytes, &shared_memory, &ds_cap)) < 0)
            return err;
        strcpy((char *)shared_memory, message);
        int result = channel->witter((l4_uint32_t)size_in_bytes,
                L4::Ipc::Cap<L4Re::Dataspace>(ds_cap));
        if (result < 0)
            printf("Error while sending data to echo server\n");
        if ((err = free_mem(&shared_memory, &ds_cap)) < 0) {
            printf("Error while freeing memory\n");
            return err;
        }
        return result;
    }

    private:
    /// allocate memory, given in bytes in the granularity of pages.
    int allocate_mem(unsigned long size_in_bytes, void **virt_addr, L4::Cap<L4Re::Dataspace> *ds) {
        int r;
        
        // get dataspace capability
        *ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
        if (!ds->is_valid())
            return -L4_ENOMEM;

        // allocate memory to data space
        if ((r = L4Re::Env::env()->mem_alloc()->alloc(size_in_bytes, *ds, 0))) {
            return r;
        }

        // make data space visible for us
        if ((r = L4Re::Env::env()->rm()->attach(virt_addr, size_in_bytes,
                        L4Re::Rm::Search_addr, L4::Ipc::make_cap_rw(*ds), 0)) < 0)
            return r;

        // worked fine;
        return 0;
    }

    // free allocated memory of a dataspace
    int free_mem(void **virt_addr, L4::Cap<L4Re::Dataspace> *ds) {
        int r;

        if ((r = L4Re::Env::env()->rm()->detach(*virt_addr, ds)))
            return r;

        // free data space from memory allocator
        l4_msgtag_t tag = l4_task_unmap(L4RE_THIS_TASK_CAP, ds->fpage(),
                L4_FP_DELETE_OBJ);
        if (l4_msgtag_has_error(tag)) {
            printf("Error freeing memory: %li\n", l4_ipc_error(tag, l4_utcb()));
            return l4_ipc_error(tag, l4_utcb());
        }

        // Release and return capability slot to allocator
        L4Re::Util::cap_alloc.free(*ds, L4Re::Env::env()->task().cap());

        return 0;
    }
};


int main() {
    ServerWrapper s;

    char text_long[] = "Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff. Some random stuff.";

    printf("Sending a string with length %li, starting with: %.14s...\n",
            strlen(text_long), text_long);
    int ret;
    if ((ret = s.send(text_long)) < 0) {
        printf("Error talking to server, code: %i\n", ret);
    }

    return 0;
}

