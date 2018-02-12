#include <l4/sys/types.h>
#include <l4/bddf/busdriver_types.h>
#include <l4/bddf/busdriver_if_ipc.h>
#include <l4/dde/fbsd/dde.h>
#include <l4/dde/fbsd/ata.h>
#include <l4/log/l4log.h>

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "types.h"

#define DISKNAME_LEN 30

#define dbg_this 0

/**
 * The root bus device in the bddf hierarchy.
 */
device_handle_t l4ata_bddf_root_device;

/**
 * Register disk at bddf.
 */
void l4ata_register_disk(dad_disk_t *dsk) {
	l4thread_t reqloop_thread;
	int err;
	char diskname[DISKNAME_LEN];
	static int devidx=0;
	l4ata_disk_info_t *di;
	
	// print some info
	if (dbg_this) {
		LOG_printf("\n");
		LOG_Enter("name=%s", dsk->name);
		LOG_printf("%llu GB: %u sects with %u Bytes per sector\n",
			((unsigned long long) dsk->sects * dsk->sectsize)>>30, dsk->sects, dsk->sectsize
		);
		LOG_printf("\n");
	}

	// initialize l4ata_disk structure part 2
	di = (l4ata_disk_info_t *) malloc(sizeof(*di));
	if (!di) {
		LOG_Error("couldn't allocate l4ata_disk_t");
		goto err_exit1;
	}
	di->reqloop_start = L4SEMAPHORE_LOCKED;
	dsk->client_priv = di;

	// create request loop thread
	reqloop_thread = l4thread_create_named(l4ata_request_loop, ".reqloop", dsk, L4THREAD_CREATE_ASYNC);
	if (reqloop_thread <= 0) {
		LOG_Error("couldn't create queue thread (%i)", reqloop_thread);
		goto err_exit2;
	}

	// setup with linux style name (hda instead of ad0)
	if ( strncmp(dsk->name, "ad", 2)==0 && (dsk->name[2]>='0') && (dsk->name[2]<='3') ) {
		snprintf(diskname, DISKNAME_LEN, "hd%c", 'a'-'0'+dsk->name[2]);
	} else if ( strncmp(dsk->name, "hd", 2) != 0) {
		snprintf(diskname, DISKNAME_LEN, "hd%c", 'e'+devidx);
		devidx++;
	}
// the old version: call the first device hda, the second hdb and so on... 
//	snprintf(diskname, DISKNAME_LEN, "hd%c", 'a'+devidx);
//	devidx++;

	// register device
	err = bus_register_phys_device(
			l4ata_bddf_root_device, diskname, dsk->sectsize, dsk->sects,
			l4ata_start_request_loop, dsk, reqloop_thread
	);
	if (err < 0) {
		LOG_Error("couldn't register physical device (%i)", err);
		goto err_exit3;
	}

	return;

err_exit3:
	l4thread_shutdown(reqloop_thread);
err_exit2:
	free(di);
err_exit1:
	return;
}

int l4ata_init_disk() {
	l4ata_bddf_root_device = bus_register_busdriver("ide", l4ata_handle_generic_request);
	if (l4ata_bddf_root_device < 0) {
		LOG_Error("Failed to register at BDDF");
		return 1;
	} else {
		LOG("successfully registered as \"ide\" at bddf");
	}

	dad_set_announce_disk(l4ata_register_disk);
	return 0;
}

