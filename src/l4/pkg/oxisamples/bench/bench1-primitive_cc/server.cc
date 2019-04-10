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

        int op_strpingpong(Bencher::Rights, L4::Ipc::String<> s, String<char> &a) {
            auto base = (char *)malloc(s.length + 1);
            memcpy(base, s.data, s.length);
            a.copy_in(s.data);
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


    int
main()
{
    static Bench_server bench;

    // Register Bench server
    if (!server.registry()->register_obj(&bench, "channel").is_valid()) {
        printf("Could not register my service, is there a 'channel' in the caps table?\n");
        return 1;
    }

    printf("Server starting\n");
    // wait for client requests
    server.loop();

    return 0;
}
