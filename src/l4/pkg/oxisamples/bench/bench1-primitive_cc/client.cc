#include <l4/sys/err.h>
#include <l4/sys/types.h>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/util/rdtsc.h>
#include <algorithm>
#include <vector>

#include <stdio.h>
#include "../interface.h"

using std::max_element;
using std::min_element;

const char* MY_MESSAGE = "Premature optimization is the root of all evil. Yet we should not pass up our opportunities in that critical 3%.";

#ifdef BENCH_SERIALISATION
void format_min_median_max(const char* msg, l4_int64_t f(ClientCall));
void format_min_median_max(const char* msg, l4_int64_t f(ClientCall)) {
    auto v = std::vector<l4_int64_t>();
    for (l4_uint64_t i = 0; i < MEASURE_RUNS; i++) {
        v.push_back(f(CLIENT_MEASUREMENTS[i]));
    }
   std::sort(v.begin(), v.end());
    printf("%-25s%-10lli%-10lli%-10lli\n", msg, v[0], v[MEASURE_RUNS/2],
            v.back());
}
#endif

int main() {
    L4::Cap<Bencher> server = L4Re::Env::env()->get_cap<Bencher>("channel");
    if (!server.is_valid()) {
        printf("Could not get server capability!\n");
        return 1;
    }

#ifndef TEST_STRING
    printf("[cc] Starting primitive type benchmarking client\n");
    l4_uint32_t val1 = 8;
    l4_uint32_t val2 = 5;
    l4_int32_t res = 0;
#else
    printf("[cc] Starting string benchmarking client\n");
#endif
    std::vector<unsigned long> cycles;
    cycles.reserve(100000);


    for (unsigned int i = 0; i < MEASURE_RUNS; i++) {
        auto start = l4_rdtsc();
#ifdef TEST_STRING
        L4::Ipc::String<const char> knuth = L4::Ipc::String<>(MY_MESSAGE);
        auto out = L4::Ipc::String<char>((char *)malloc((knuth.length + 1) * sizeof(char)));
#endif
#ifndef TEST_STRING
        if (server->sub(val1, val2, res))
#else
        if (server->strpingpong(knuth, out))
#endif
        {
            printf("Error talking to server\n");
            return 1;
        }
        auto end = l4_rdtsc();
        cycles.push_back(end - start);
#ifdef TEST_STRING
        if (out.data[0] == 'P') {
            printf("error, got string: %s\n", out.data);
        }
        free(out.data);
#endif
    }
    printf("srl := serialisation\n");
    printf("%-25s%-10s%-10s%-10s\n", "Value", "Min", "Median", "Max");
    std::sort(cycles.begin(), cycles.end());

    printf("%-25s%-10lu%-10lu%-10lu\n", "Global",
            *min_element(std::begin(cycles), std::end(cycles)),
            cycles[MEASURE_RUNS / 2],
            *max_element(std::begin(cycles), std::end(cycles)));
    // client argument serialisation duration
    format_min_median_max("client argument srl", [](ClientCall c) {
        return c.arg_srl_end - c.arg_srl_start;
    });
    format_min_median_max("arg srl -> IPC call", [](ClientCall c) {
        return c.call_start - c.arg_srl_end;
    });
    format_min_median_max("arg srl -> IPC call", [](ClientCall c) {
        return c.call_start - c.arg_srl_end;
    });
    format_min_median_max("IPC call", [](ClientCall c) {
        return c.ret_desrl_start - c.call_start;
    });
    format_min_median_max("return value desrl", [](ClientCall c) {
        return c.ret_desrl_end - c.ret_desrl_start;
    });

    return 0;
}
