#include <l4/sys/consts.h>
#include <l4/sys/err.h>
#include <l4/sys/types.h>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/util/rdtsc.h>
#include <algorithm>
#include <utility>
#include <vector>

#include <stdio.h>
#include "../interface.h"

using std::max_element;
using std::min_element;

const char* MY_MESSAGE = "Premature optimization is the root of all evil. Yet we should not pass up our opportunities in that critical 3%.";

#ifdef BENCH_SERIALISATION
#include <l4/sys/ipc.h>
#include <l4/re/c/mem_alloc.h>
#include <l4/re/c/rm.h>
#include <l4/re/c/util/cap_alloc.h>

/// Set up shared memory to access server measurements, send dataspace cap to
/// server.
static void setup_shm(L4::Cap<Bencher>& srv) {
    auto size_in_bytes = l4_round_page(MEASURE_RUNS * sizeof(ServerDispatch)), r = 0UL;
    auto ds = l4re_util_cap_alloc();
    if ((r = l4re_ma_alloc(size_in_bytes, ds, 0)) != 0) {
        printf("Error while requesting memory: %lu", r);
        throw r;
    }

    // attach to region map
    void* virt_addr;
    if ((r = l4re_rm_attach(&virt_addr, size_in_bytes, L4RE_RM_SEARCH_ADDR, ds, 0, L4_PAGESHIFT)) != 0) {
        printf("Error while attaching memory to region map: %lu\n", r);
        throw r;
    }
    // send cap to server without the interface usage to avoid benchmarking
    auto mr = l4_utcb_mr();
    (*mr).mr[0] = size_in_bytes;
    (*mr).mr[1] = L4_ITEM_MAP;
    (*mr).mr[2] = l4_obj_fpage(ds, 0, L4_FPAGE_RWX).raw;
    if ((r = l4_ipc_error(l4_ipc_send(srv.cap(), l4_utcb(),
                        l4_msgtag(9876, 1, 1, 0), L4_IPC_NEVER), l4_utcb())) != 0) {
        printf("Unable to send shared memory dataspace capability to remote server: %lu", r);
        throw r;
    }
    // Allows client-side access to server measurements using a global
    SERVER_MEASUREMENTS = (ServerDispatch *)virt_addr;
    // ^ we do not free our resources :)
}
#endif

template<typename T1, typename T2>
static void format_min_median_max(T1* src1, T2* src2, const char* msg,
        l4_int64_t f(T1, T2)) {
    src1 = src1 + 2;
    src2 = src2 + 2;
    auto v = std::vector<l4_int64_t>();
    for (l4_uint64_t i = 0; i < MEASURE_RUNS - 2; i++) {
        v.push_back(f(src1[i], src2[i]));
    }
    std::sort(v.begin(), v.end());
    printf("%-30s%-10lli%-10lli%-10lli\n", msg, v[0], v[v.size()/2],
            v.back());
}

#ifdef BENCH_SERIALISATION
static void format_min_median_max(const char* msg, l4_int64_t f(ClientCall, ServerDispatch)) {
    return format_min_median_max<ClientCall, ServerDispatch>(
            CLIENT_MEASUREMENTS, SERVER_MEASUREMENTS, msg, f);
}
#endif

#ifdef TEST_CAP
#include <l4/re/dataspace>
#include <l4/sys/capability>
#include <l4/sys/task.h>

// make a DS for the flexpage test, not to be confused with the Ds cap we create
// for sharing memory with the server for measurement exchange
size_t mk_ds(void **shm, L4::Cap<L4Re::Dataspace> &ds);
size_t mk_ds(void **shm, L4::Cap<L4Re::Dataspace> &ds) {
    long size = 8192, r;
    ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    if (!ds.is_valid())
        return -L4_ENOMEM;

    if ((r = L4Re::Env::env()->mem_alloc()->alloc((long)size, ds)))
        return r;

    // make data space visible for us
    if ((r = L4Re::Env::env()->rm()->attach(shm, size,
                    L4Re::Rm::Search_addr, L4::Ipc::make_cap_rw(ds), 0)) < 0)
        return r;
    return size;
}

int free_mem(void *shm, L4::Cap<L4Re::Dataspace> &ds);
int free_mem(void *shm, L4::Cap<L4Re::Dataspace> &ds) {
    int r;
    if ((r = L4Re::Env::env()->rm()->detach(*((l4_addr_t*)shm), &ds))) {
        printf("Error detaching memory: %i\n", r);
        exit(2);
    }

    // free memory at our memory allocator, this is optional
    l4_msgtag_t tag = l4_task_unmap(L4RE_THIS_TASK_CAP, ds.fpage(), L4_FP_DELETE_OBJ);
    if (l4_msgtag_has_error(tag)) {
        printf("Error freeing memory: %li\n", l4_ipc_error(tag, l4_utcb()));
        exit(3);
    }
    // release and return capability slot to allocator
    L4Re::Util::cap_alloc.free(ds, L4Re::Env::env()->task().cap());
    return 0;
}
#endif

#if defined(TEST_STRING) &&  defined(TEST_CAP)
#error "May only build for TEST_STRING or TEST_CAP, not both"
#endif

int main() {
    L4::Cap<Bencher> server = L4Re::Env::env()->get_cap<Bencher>("channel");
#ifdef BENCH_SERIALISATION
    setup_shm(server);
#endif
    int r;

    if (!server.is_valid()) {
        printf("Could not get server capability!\n");
        return 1;
    }

#if !defined(TEST_STRING) && !defined(TEST_CAP)
    printf("[cc] Starting primitive type benchmarking client\n");
    l4_uint32_t val1 = 8;
    l4_uint32_t val2 = 5;
    l4_int32_t res = 0;
#elif defined(TEST_STRING)
    printf("[cc] Starting string benchmarking client\n");
#elif defined(TEST_CAP) // TEST_CAP
    L4::Cap<L4Re::Dataspace> ds;
    void *shm = 0;
    size_t size = mk_ds(&shm, ds);
    printf("[cc] Starting capability benchmarking client\n");
#endif
    std::vector<std::pair<long long, long long>> cycles;
    cycles.reserve(100000);


    for (unsigned int i = 0; i < MEASURE_RUNS; i++) {
        auto start = l4_rdtscp();
#ifdef TEST_STRING
        L4::Ipc::String<const char> knuth = L4::Ipc::String<>(MY_MESSAGE);
        auto out = L4::Ipc::String<char>((char *)malloc((knuth.length + 1) * sizeof(char)));
        if ((r = server->string_ping_pong(knuth, out)))
#elif defined(TEST_CAP)
        if ((r = server->map_cap(ds)))
#else
        if ((r = server->sub(val1, val2, res)))
#endif
        {
            printf("Error talking to server: %i\n", r);
            return 1;
        }
        auto end = l4_rdtscp();
        cycles.push_back(std::make_pair(start, end));
#ifdef TEST_STRING
        free(out.data);
#endif
    }
#ifdef TEST_CAP
    free_mem(&shm, ds);
#endif
    printf("%-30s%-10s%-10s%-10s\n", "Value", "Min", "Median", "Max");

    // global: only cycles required, the other is a dummy arg
    format_min_median_max<std::pair<long long, long long>, void*>(
        cycles.data(), (void **)0, "Global",
        [](std::pair<long long, long long> p, void *c) {
            (void)c; return p.second - p.first; // start of call
        });
#ifdef BENCH_SERIALISATION
    // client IPC method invocation to writing arguments to mr
    format_min_median_max<std::pair<long long, long long>, ClientCall>(
        cycles.data(), CLIENT_MEASUREMENTS, "Call start to writing args",
        [](std::pair<long long, long long> p, ClientCall c) {
            return c.arg_srl_start - p.first;
        });

    // client argument serialisation duration
    format_min_median_max("Writing args to registers", [](ClientCall c, ServerDispatch s) {
        (void)s;return c.arg_srl_end - c.arg_srl_start;
    });
    format_min_median_max("arg srl to IPC call", [](ClientCall c, ServerDispatch s) {
        (void)s;return c.call_start - c.arg_srl_end;
    });
    format_min_median_max("IPC send to server", [](ClientCall c, ServerDispatch s) {
        return s.loop_dispatch - c.call_start;
    });
    format_min_median_max("Srv loop to iface dispatch", [](ClientCall c, ServerDispatch s) {
        (void)c;return s.iface_dispatch - s.loop_dispatch;
    });
    format_min_median_max("Reading opcode", [](ClientCall c, ServerDispatch s) {
        (void)c;return s.opcode_dispatch - s.iface_dispatch;
    });
    format_min_median_max("Reading args", [](ClientCall c, ServerDispatch s) {
        (void)c;return s.exc_user_impl - s.opcode_dispatch;
    });
    format_min_median_max("Exec user impl", [](ClientCall c, ServerDispatch s) {
        (void)c;return s.retval_serialisation_start - s.exc_user_impl;
    });

    format_min_median_max("Write return value", [](ClientCall c, ServerDispatch s) {
        (void)c;return s.result_returned - s.retval_serialisation_start;
    });
    format_min_median_max("IPC reply to client", [](ClientCall c, ServerDispatch s) {
        return c.ret_desrl_start - s.result_returned;
    });

    format_min_median_max("return value desrl", [](ClientCall c, ServerDispatch s) {
        (void)s;return c.ret_desrl_end - c.ret_desrl_start;
    });

    // client ret val read
    format_min_median_max<ClientCall, std::pair<long long, long long>>(
            CLIENT_MEASUREMENTS, cycles.data(), "Ret val read to call end",
            [](ClientCall c, std::pair<long long, long long> p) {
            return p.second - c.ret_desrl_end;
        });
#endif
    return 0;
}
