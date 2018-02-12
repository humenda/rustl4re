#include <l4/bddf/timebase.h>

#include <l4/dde/fbsd/dde.h>
#include <l4/dde/fbsd/bsd_dde.h>
#include <l4/log/l4log.h>

#include "types.h"

l4_ssize_t l4libc_heapsize = 400<<10;

// default is not enough
const int l4thread_max_threads = 32;

#ifdef RTMON
// request loop thread time for request setup
rt_mon_histogram_t *rtm_request_duration;
rt_mon_histogram_t *rtm_request_setup_cpu;
rt_mon_histogram_t *rtm_request_setup_wall;
#endif

/**
 * The bus drivers main function.
 */
static int l4ata_main(int argc, char** argv) {
	int err;

	bddf_init_timebase();

#ifdef RTMON
    rtm_request_duration   = rt_mon_hist_create(
			0, 100000, 200, "l4ata/request/duration",
			"duration [us]", "incidence", RT_MON_TSC_TO_US_TIME
	);
    rtm_request_setup_cpu  = rt_mon_hist_create(
			0, 200, 200, "l4ata/request/setup (thread)",
			"duration [us]", "incidence", RT_MON_THREAD_TIME
	);
    rtm_request_setup_wall = rt_mon_hist_create(
			0, 200, 200, "l4ata/request/setup (wall clock)",
			"duration [us]", "incidence", RT_MON_TSC_TO_US_TIME
	);
#endif

	err = l4ata_init_disk();
	if (err) {
		return 0;
	}

	// initialize ddekit
	dde_init();

	// two pages minimum to not cause thrashing
	dde_setup_page_buffer(2);

	// initialize dde_fbsd
	bsd_dde_init();

	l4thread_sleep_forever();
}

init_busdriver(l4ata_main);
