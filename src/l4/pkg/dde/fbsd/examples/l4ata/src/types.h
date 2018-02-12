#ifndef _types_h
#define _types_h

#include <l4/bddf/busdriver_types.h>
#include <l4/bddf/busdriver_if_ipc.h>
#include <l4/lock/lock.h>
#include <l4/dde/fbsd/ata.h>

#ifdef RTMON
#include <l4/rt_mon/histogram.h>
extern rt_mon_histogram_t *rtm_request_duration;
extern rt_mon_histogram_t *rtm_request_setup_cpu;
extern rt_mon_histogram_t *rtm_request_setup_wall;
#endif

typedef struct l4ata_disk_info {
	device_handle_t bddf_device;
	l4semaphore_t reqloop_start;
} l4ata_disk_info_t;

typedef struct l4ata_req_info {
	// lock for securing outstanding counter
	l4lock_t lock;
	// outstanding bio counter
	unsigned outstanding;
	// set to 1 on bio error
	int error;
	// associated bddf device
	dad_disk_t *dsk;
	// associated bddf request
	bus_request_t *req;
} l4ata_req_info_t;

typedef struct l4ata_bio_info {
	// pointer to request info
   l4ata_req_info_t *ri;
   // first ds associated with this bio
   struct ds_list_element *first_ds;
   // number of ds-s associated with this bio (continious from start_ds)
   int ds_count;
} l4ata_bio_info_t;

#define dad_l4ata_di(dsk) ((l4ata_disk_info_t*)(dsk->client_priv))
#define dadrq_l4ata_bi(dadrq) ((l4ata_bio_info_t*)(dadrq->client_priv))

/* in disk.c: */
int  l4ata_init_disk(void);
void l4ata_register_disk(dad_disk_t *dsk);

/* in reqloop.c */
void l4ata_request_loop(void *arg);
int  l4ata_start_request_loop(device_handle_t, void *);

/* in request.c */
bus_generic_req_fn l4ata_handle_generic_request;
void l4ata_handle_request(dad_disk_t *dsk, bus_request_t *req);

#endif // _types_h
