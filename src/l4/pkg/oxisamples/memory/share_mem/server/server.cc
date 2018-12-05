#include <cstdio>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>
#include <l4/sys/cxx/ipc_epiface>

struct Witter: L4::Kobject_t<Witter, L4::Kobject, 0x44, L4::Type_info::Demand_t<1>>
{
  L4_INLINE_RPC(int, witter, (l4_uint32_t length, L4::Ipc::Cap<L4Re::Dataspace>));
  typedef L4::Typeid::Rpcs<witter_t> Rpcs;
};

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;
class WitterServer: public L4::Epiface_t<WitterServer, Witter> {
public:
    int op_witter(L4::Typeid::Rights<Witter>& _ign,
            l4_uint32_t size_in_bytes, L4::Ipc::Snd_fpage const &fpage) {
        printf("janz oben, erwarte %x bytes\n", size_in_bytes);
        if (!fpage.cap_received()) {
            printf("no data space capability received, sorry\n");
            return -L4_EINVAL;
        }
        auto mr = l4_utcb_mr();
        int err;
        L4::Cap<L4Re::Dataspace> ds_cap = server_iface()->rcv_cap<L4Re::Dataspace>(0);
        if (!ds_cap.is_valid()) {
            printf("invalid data space capability\n");
            return -666;
        }
        printf("popeling stuff out of mr\n");
        printf("%lx;%lx;%lx;%lx;%lx\n", mr->mr[0], mr->mr[1], mr->mr[2], mr->mr[3], mr->mr[4]);
        void *virt_addr = 0;
        // reserve area to allow attaching a data space; flags = 0 must be set

        if ((err = L4Re::Env::env()->rm()->attach(&virt_addr, size_in_bytes,
                        L4Re::Rm::Search_addr, ds_cap)) < 0) {
            printf("error while attaching 0x%x bytes from cap idx 0x%x: %d\n",
                    size_in_bytes, ds_cap.cap(), err);
            return err;
        }

        printf("Received string of length %i:\n%s\n", size_in_bytes, (char *)virt_addr);
        L4Re::Env::env()->rm()->detach(virt_addr, &ds_cap);
        return 0;
    }
};


int main() {
    static WitterServer witter_srv;

    // Register echo server
    if (!server.registry()->register_obj(&witter_srv, "channel").is_valid()) {
        printf("Could not register my service, is there a 'witter_server' in the caps table?\n");
        return 1;
    }

    printf("Start wittering here, I'll echo it right back.\n");

    // Wait for client requests
    server.loop();
    return 0;
}

