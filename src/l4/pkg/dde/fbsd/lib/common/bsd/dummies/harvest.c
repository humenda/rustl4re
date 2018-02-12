/**
 * FreeBSD implementation in
 * dev/random/harvest.c
 */

#include <sys/types.h>
#include <sys/random.h>

struct harvest_select harvest = { 0, 0, 0, 0 };

void random_harvest(void *entropy, u_int count, u_int bits, u_int frac, enum esource origin) {
}
