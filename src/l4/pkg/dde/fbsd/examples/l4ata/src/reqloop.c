#include <l4/dde/fbsd/dde.h>
#include <l4/dde/fbsd/bsd_dde.h>
#include <l4/dde/fbsd/ata.h>
#include <l4/bddf/timebase.h>
#include <l4/log/l4log.h>
#ifdef RTMON
#include <l4/rt_mon/histogram.h>
#endif

#include "types.h"

#define dbg_this 0

/**
 * Helper function for registering a new physical device at bddf.
 * Called after bddf disk initialization and before reading the partition
 * tables. Should release the request loop running.
 */
int l4ata_start_request_loop(device_handle_t device, void *arg) {
	dad_disk_t *dsk = (dad_disk_t*) arg;
	l4ata_disk_info_t *di = dad_l4ata_di(dsk);

	// initialize l4ata_disk structure part 2
	di->bddf_device = device;

	// now request loop can start running
	l4semaphore_up(&di->reqloop_start);

	return 0;
}

/**
 * Request loop function waiting for and handling bddf requests.
 */
void l4ata_request_loop(void *arg) {
	dad_disk_t *dsk = (dad_disk_t *)arg;
	l4ata_disk_info_t *di = dad_l4ata_di(dsk);
	bus_request_t *req;
	unsigned bufsize;
	struct ds_list_element *ds;
	int dsi;

	bsd_dde_prepare_thread("reqloop");

	// wait until start_request_loop is called
	l4semaphore_down(&di->reqloop_start);
	
	// enter loop
	while (1) {
		char *typename;
		
		LOGd(dbg_this, "requesting next disk i/o task");

		req = bus_get_next_req(di->bddf_device);
		switch (req->type) {
			case REQ_TYPE_READ:
				typename = "read";
				break;
			case REQ_TYPE_WRITE:
				typename = "write";
				break;
			case REQ_TYPE_BARRIER:
				typename = "barrier";
				break;
			default:
				typename = "unknown";
				break;
		}

		// mark request handling start time for bddf
		req->start_time = bddf_get_timestamp();

#ifdef RTMON
		rt_mon_hist_start(rtm_request_setup_cpu);
		rt_mon_hist_start(rtm_request_setup_wall);
#endif

		// reject invalid requests
		if (req->start_sect < 0) {
			LOG("error: invalid request (start_sect=%d) for %s", req->start_sect, dsk->name);
			req->end_time = bddf_get_timestamp();
	    	bus_completed_req(di->bddf_device, req, 1);
			continue;
		}
		if ( (req->type!=REQ_TYPE_READ) && (req->type!=REQ_TYPE_WRITE) ) {
			LOG("error: unknown request type (type=%d) for %s", req->type, dsk->name);
			req->end_time = bddf_get_timestamp();
	    	bus_completed_req(di->bddf_device, req, 1);
			continue;
		}

		// test if sum(buffers) is big enough
		bufsize = 0;
		ds = (struct ds_list_element *)req->ds_list;
		for (dsi = req->ds_list_count; dsi; dsi--, ds++)
			bufsize += ds->ds_size;
		if (bufsize < req->sect_num*dsk->sectsize) {
			LOG("error: buffer too small (%d)\nfor request (%d=%d sects of %d bytes) for %s",
					bufsize, req->sect_num*dsk->sectsize, req->sect_num, dsk->sectsize, dsk->name);
			req->end_time = bddf_get_timestamp();
	    	bus_completed_req(di->bddf_device, req, 1);
			continue;
		}

		// handle request
		l4ata_handle_request(dsk, req);

#ifdef RTMON
		rt_mon_hist_end(rtm_request_setup_cpu);
		rt_mon_hist_end(rtm_request_setup_wall);
#endif
	}
}

