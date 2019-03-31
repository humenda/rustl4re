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


    for (unsigned int i = 0; i < 100000; i++) {
#ifdef TEST_STRING
        L4::Ipc::String<const char> knuth = L4::Ipc::String<>(MY_MESSAGE);
        auto out = L4::Ipc::String<char>((char *)malloc((knuth.length + 1) * sizeof(char)));
#endif
        auto start = l4_rdtsc();
#ifndef TEST_STRING
        if (server->sub(val1, val2, &res))
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
    std::sort(cycles.begin(), cycles.end());

    printf("min: %lu\n", *min_element(std::begin(cycles), std::end(cycles)));
    printf("max: %lu\n", *max_element(std::begin(cycles), std::end(cycles)));
    int sum = 0;
    for (unsigned int i = 0; i < (unsigned int)cycles.size(); i++, sum += cycles[i]);
    printf("avg: %lu\n", sum / cycles.size());
    printf("median: %lu\n", cycles[500]);

    return 0;
}
