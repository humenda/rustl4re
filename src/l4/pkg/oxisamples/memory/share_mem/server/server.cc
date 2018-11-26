struct Witter: L4::Kobject_t<Witter, L4::Kobject, 0x666, L4::Type_info::Demand_t<1>>
{
  L4_INLINE_RPC(int, witter, (l4_uint32_t length, L4::Ipc::Cap<L4Re::Dataspace>));
  typedef L4::Typeid::Rpcs<witter_t> Rpcs;
};

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

class WitterServer: public L4::Epiface_t<WitterServer, Witter> {
public:
    int op_witter(L4::Typeid::Rights<Witter>& _ign,
            l4_uint32_t size_in_bytes, L4::Ipc::Snd_fpage const &fpage) {
        if (!fpage.cap_received()) {
            printf("no data space capability received, sorry\n");
            return -L4_EINVAL;
        }
        printf("stopping for now, ToDo\n");
        return 0;
        int err;

        L4::Cap<L4Re::Dataspace> ds_cap = server_iface()->rcv_cap<L4Re::Dataspace>(0);
        if (!ds_cap.is_valid()) {
            printf("invalid data space capability\n");
            return -666;
        }
        void *virt_addr = 0;
        // reserve area to allow attaching a data space; flags = 0 must be set

        if ((err = L4Re::Env::env()->rm()->attach(&virt_addr, size_in_bytes,
                        L4Re::Rm::Search_addr, ds_cap)) < 0)
            return err;

        printf("Received %s\n", (char *)virt_addr);
        L4Re::Env::env()->rm()->detach(virt_addr, &ds_cap);
        return 0;
    }
};


int main() {
    static EchoServer echo;

    // Register echo server
    if (!server.registry()->register_obj(&echo, "echo_server").is_valid()) {
        printf("Could not register my service, is there a 'echo_server' in the caps table?\n");
        return 1;
    }

    printf("Welcome to the echo server.\nSend me, whatever you'd like seeing echoed.\n");

    // Wait for client requests
    server.loop();

    return 0;
}

