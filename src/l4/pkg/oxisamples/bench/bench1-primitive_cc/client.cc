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
/*
#include <iterator>
#include <utility>
class ZipIterator: public std::iterator<std::forward_iterator_tag,
            std::pair<ClientCall, ServerDispatch>> {
    l4_uint64_t idx_a;
    l4_uint64_t idx_b;
public:
    ZipIterator() : idx_a(0), idx_b(0) { }
    ~ZipIterator() { }
    ZipIterator& operator=(const ZipIterator& o) {
        idx_a = o.idx_a;
        idx_b = o.idx_b;
        return *this;
    }
    ZipIterator& operator++() {
        idx_a++; idx_b++;
        // no bound checks
        return *this;
    }
    std::pair<ClientCall, ServerDispatch> operator*() const {
        return std::make_pair(CLIENT_MEASUREMENTS[idx_a], SERVER_MEASUREMENTS[idx_b]);
    }
};
*/

template<typename T1, typename T2>
static void format_min_median_max(T1* src1, T2* src2, const char* msg,
        l4_int64_t f(T1, T2)) {
    auto v = std::vector<l4_int64_t>();
    for (l4_uint64_t i = 0; i < MEASURE_RUNS; i++) {
        v.push_back(f(src1[i], src2[i]));
    }
    std::sort(v.begin(), v.end());
    printf("%-30s%-10lli%-10lli%-10lli\n", msg, v[0], v[MEASURE_RUNS/2],
            v.back());
}

static void format_min_median_max(const char* msg, l4_int64_t f(ClientCall, ServerDispatch)) {
    return format_min_median_max<ClientCall, ServerDispatch>(
            CLIENT_MEASUREMENTS, SERVER_MEASUREMENTS, msg, f);
}
#endif

#ifdef TEST_CAP
#include <l4/re/dataspace>
#include <l4/sys/capability>
#include <l4/sys/task.h>

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
#else // TEST_CAP
    L4::Cap<L4Re::Dataspace> ds;
    void *shm = 0;
    size_t size = mk_ds(&shm, ds);
    printf("[cc] Starting capability benchmarking client\n");
#endif
    std::vector<std::pair<long long, long long>> cycles;
    cycles.reserve(100000);


    for (unsigned int i = 0; i < MEASURE_RUNS; i++) {
        auto start = l4_rdtsc();
#ifdef TEST_STRING
        L4::Ipc::String<const char> knuth = L4::Ipc::String<>(MY_MESSAGE);
        auto out = L4::Ipc::String<char>((char *)malloc((knuth.length + 1) * sizeof(char)));
        if ((r = server->str2leet(knuth, out)))
#elif defined(TEST_CAP)
        if ((r = server->check_dataspace(size, ds)))
#else
        if ((r = server->sub(val1, val2, res)))
#endif
        {
            printf("Error talking to server: %i\n", r);
            return 1;
        }
        auto end = l4_rdtsc();
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
    format_min_median_max<std::pair<long long, long long>, ClientCall>(
        cycles.data(), CLIENT_MEASUREMENTS, "Global",
        [](std::pair<long long, long long> p, ClientCall c) {
            (void)c; return p.second - p.first; // start of call
        });
#ifdef BENCH_SERIALISATION
    // client IPC method invocation to writing arguments to mr
    format_min_median_max<std::pair<long long, long long>, ClientCall>(
        cycles.data(), CLIENT_MEASUREMENTS, "IPC arg invocation to writing args",
        [](std::pair<long long, long long> p, ClientCall c) {
            return c.arg_srl_start - p.first;
        });

    // client argument serialisation duration
    format_min_median_max("client argument srl", [](ClientCall c, ServerDispatch s) {
        (void)s;return c.arg_srl_end - c.arg_srl_start;
    });
    format_min_median_max("arg srl -> IPC call", [](ClientCall c, ServerDispatch s) {
        (void)s;return c.call_start - c.arg_srl_end;
    });
    format_min_median_max("arg srl -> IPC call", [](ClientCall c, ServerDispatch s) {
        (void)s;return c.call_start - c.arg_srl_end;
    });
    format_min_median_max("IPC call", [](ClientCall c, ServerDispatch s) {
        (void)s;return c.ret_desrl_start - c.call_start;
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
