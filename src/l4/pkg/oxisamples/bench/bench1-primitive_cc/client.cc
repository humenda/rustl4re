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

int main() {
    L4::Cap<Bencher> server = L4Re::Env::env()->get_cap<Bencher>("channel");
    if (!server.is_valid()) {
        printf("Could not get server capability!\n");
        return 1;
    }

    l4_uint32_t val1 = 8;
    l4_uint32_t val2 = 5;
    l4_int32_t res = 0;
    std::vector<unsigned long> v;
    v.reserve(100000);

    printf("[cc] Starting primitive subtraction test\n");

    for (unsigned int i = 0; i < 100000; i++) {
        auto start = l4_rdtsc();
#ifndef TEST_STRING
        if (server->sub(val1, val2, &res)) {
#else
            // ToDo: not functional
        if (server->strpingpong(....)) {
#endif
            printf("Error talking to server\n");
            return 1;
        }
        auto end = l4_rdtsc();
        v.push_back(end - start);
    }
    std::sort(v.begin(), v.end());

    printf("min: %lu\n", *min_element(std::begin(v), std::end(v)));
    printf("max: %lu\n", *max_element(std::begin(v), std::end(v)));
    int sum = 0;
    for (unsigned int i = 0; i < (unsigned int)v.size(); sum += v[i]);
    printf("avg: %lu\n", sum / v.size());
    printf("median: %lu\n", v[500]);

    return 0;
}
