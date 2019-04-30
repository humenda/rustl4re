#include <stdio.h>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/sys/cxx/ipc_epiface>
#include <string.h>
#include <stdlib.h>

#include "../interface.h"

using L4::Ipc::String;

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

class Bench_server : public L4::Epiface_t<Bench_server, Bencher>
{
    public:
        int op_sub(Bencher::Rights, l4_uint32_t a, l4_uint32_t b, l4_int32_t &res) {
            res = (l4_int32_t)a - b;
            return 0;
        }

        int op_str_ping_pong(Bencher::Rights, L4::Ipc::String<> s, String<char> &a) {
            char msg[256];
            if (s.length > 256)
                return -L4_EMSGTOOLONG;
            memcpy(msg, s.data, s.length);
            a.copy_in((char *)&msg);
            return 0;
        }

        int op_string_ping_pong(Bencher::Rights, L4::Ipc::String<> s, String<char> &a) {
            char msg[256];
            if (s.length > 256)
                return -L4_EMSGTOOLONG;
            memcpy(msg, s.data, s.length);
            a.copy_in((char *)&msg);
            return 0;

        }

        int op_check_dataspace(Bencher::Rights, l4_uint64_t size, L4::Ipc::Snd_fpage const &fpage) {
            int r = 0;
            L4Re::Dataspace::Stats stats;
            if (!fpage.cap_received()) {
                printf("no data space capability received, sorry\n");
                return -L4_EINVAL;
            }
            L4::Cap<L4Re::Dataspace> ds = server_iface()->rcv_cap<L4Re::Dataspace>(0);
            if (!ds.is_valid()) {
                printf("invalid data space capability\n");
                return -666;
            }
            if ((r = ds->info(&stats))) 
                return r;
            return (stats.size == size) == 0;
        }
};

#ifdef BENCH_SERIALISATION
#include <l4/re/c/rm.h>
#include <l4/re/c/util/cap_alloc.h>
static void setup_shm() {
    auto ds = l4re_util_cap_alloc();
    auto br = l4_utcb_br();
    br->bdr = 0;
    br->br[0] = ds | L4_RCV_ITEM_SINGLE_CAP | L4_RCV_ITEM_LOCAL_ID;
    long unsigned int label; // gate is already registered
    auto tag = l4_ipc_wait(l4_utcb(), &label, L4_IPC_NEVER);
    if (l4_ipc_error(tag, l4_utcb()))
        throw l4_ipc_error(tag, l4_utcb());
    auto size = l4_utcb_mr()->mr[0];
    void* ds_start;
    // attach received dataspace to virtual memory region and let the
    // measurement functions point to it
    auto err = l4re_rm_attach(&ds_start, size, L4RE_RM_SEARCH_ADDR, ds, 0, L4_PAGESHIFT);
    if (err != 0)
        printf("error while attaching memory: %i", err);
    SERVER_MEASUREMENTS = (ServerDispatch* )ds_start;
}
#endif

int main() {
    static Bench_server bench;

    // Register Bench server
    if (!server.registry()->register_obj(&bench, "channel").is_valid()) {
        printf("Could not register my service, is there a 'channel' in the caps table?\n");
        return 1;
    }
#ifdef BENCH_SERIALISATION
    setup_shm();
#endif

    printf("Server starting\n");
    // wait for client requests
    server.loop();

    return 0;
}
