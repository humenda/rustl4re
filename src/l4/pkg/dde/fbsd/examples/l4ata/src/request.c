#include <l4/sys/types.h>
#include <l4/bddf/busdriver_types.h>
#include <l4/bddf/busdriver_if_ipc.h>
#include <l4/bddf/timebase.h>

#include <l4/log/l4log.h>
#include <l4/l4rm/l4rm.h>
#include <l4/env/errno.h>
#include <l4/dm_mem/dm_mem.h>

#include <malloc.h>

#include "types.h"

#define dbg_this 0

// buffers must be aligned to be accepted by disk driver
#define BIO_ALIGN 16

#define bio_get_bi(bp)         ((struct bio_info *) bp->bio_driver1)
#define bio_set_bi(bp, value)  (bp->bio_driver1 = (void *) value)

/**
 * Do common (buffered vs. direct) bio done handling.
 */
static void dadrq_done_common(dad_request_t *dadrq) {
	l4ata_bio_info_t *bi = dadrq_l4ata_bi(dadrq);
	l4ata_req_info_t *ri = bi->ri;
#ifdef RTMON
	Time_t usec;
#endif

	if (dbg_this)
		LOG_printf("done:                        dadrq=%p\n", dadrq);

	l4lock_lock(&ri->lock);
	ri->outstanding--;
	if ( (! ri->error) && (dadrq->error) )
		ri->error = 1;
	if (! ri->outstanding) {
		l4lock_unlock(&ri->lock);
		ri->req->end_time = bddf_get_timestamp();
#ifdef RTMON
		usec = bddf_get_time_difference(ri->req->start_time, ri->req->end_time)/1000;
		rt_mon_hist_insert_data(rtm_request_duration, usec, 1);
#endif
		if (dbg_this)
			LOG_printf("complete:     req=%p with error=%x\n", ri->req, ri->error);
	    bus_completed_req(dad_l4ata_di(ri->dsk)->bddf_device, ri->req, ri->error);
		free(ri);
	} else {
		l4lock_unlock(&ri->lock);
	}
	free(dadrq);
	free(bi);
}

static void dadrq_done_direct(dad_request_t *dadrq) {
	// detach dataspace
	l4rm_detach(dadrq->buf);

	// do common handling
	dadrq_done_common(dadrq);
}

static void start_direct_bio(l4ata_bio_info_t *bi, int start_sect, int sect_count) {
	int err;
	struct ds_list_element *ds = bi->first_ds;
	int write = (bi->ri->req->type==REQ_TYPE_WRITE) ? 1 : 0;
	dad_request_t *dadrq;

	// setup dad request struct
	dadrq = (dad_request_t *)malloc(sizeof(*dadrq));
	if (!dadrq) {
		LOG_Error("allocating dad_request_t failed");
		return;
	}
	dadrq->disk    = bi->ri->dsk;
	dadrq->start   = start_sect;
	dadrq->count   = sect_count;
	dadrq->op      = write ? DAD_WRITE : DAD_READ;
	dadrq->buf     = NULL; // initialized later
	dadrq->phys    = ds->phys_addr;
	dadrq->bufsize = sect_count*bi->ri->dsk->sectsize;
	dadrq->done    = dadrq_done_direct;
	dadrq->client_priv = bi;

	// attach ds
	err = l4rm_attach(&ds->ds_id, ds->ds_size, ds->ds_offset,
			(write?L4DM_RO:L4DM_RW), &dadrq->buf);
	if (err) {
		LOG_Error("attaching dataspace failed");
		free(dadrq);
		return;
	}

	// increment outstanding counter
	bi->ri->outstanding++;

	// print info
	if (dbg_this)
		LOG_printf("new direct:   req=%p dadrq=%p start=%d count=%d\n", bi->ri->req, dadrq, start_sect, sect_count);

	// issue request to driver
	dad_put_request(dadrq);
}

static int copy_buffer(int tobuffer, struct ds_list_element *first, void *buffer, int count) {
	struct ds_list_element *ds;
	int left=count;
	int err;
	int dsi;

	ds = first;
	for (dsi=0; ; dsi++, ds++) {
		void *mapped;
		unsigned tocopy;

		// map DS
		err = l4rm_attach(&ds->ds_id, ds->ds_size, ds->ds_offset,
				(tobuffer?L4DM_RO:L4DM_RW)|L4RM_MAP, &mapped);
		if (err) {
			LOG_Error("attaching dataspace failed");
			return 1;
		}
		// copy contents
		tocopy = ds->ds_size;
		if (tocopy > left) tocopy = left;
		if (tobuffer)
			memcpy(buffer, mapped, tocopy);
		else
			memcpy(mapped, buffer, tocopy);
		left   -= tocopy;
		buffer += tocopy;
		// unmap DS
		l4rm_detach(mapped);

		// stop copying if nothing left
		if (! left) break;
	}
	return 0; // no error
}

static void dadrq_done_buffered(dad_request_t *dadrq) {
	l4ata_bio_info_t *bi = dadrq_l4ata_bi(dadrq);
	l4ata_req_info_t *ri = bi->ri;

	// do post-read copying
	if ( ! (ri->error || dadrq->error) )
		if (dadrq->op == DAD_READ)
			copy_buffer(/* to_buffer */ 0, bi->first_ds, dadrq->buf, dadrq->bufsize);

	// free shadow buffer
	l4dm_mem_release(dadrq->buf);

	// do common handling
	dadrq_done_common(dadrq);
}

static void start_buffered_bio(l4ata_bio_info_t *bi, int start_sect, int sect_count) {
	int err;
	int write = (bi->ri->req->type==REQ_TYPE_WRITE) ? 1 : 0;
	dad_request_t *dadrq;
	l4dm_mem_addr_t dm_paddr;
	l4_size_t tmp;

	// setup dad request struct
	dadrq = (dad_request_t *)malloc(sizeof(*dadrq));
	if (!dadrq) {
		LOG_Error("allocating dad_request_t failed");
		return;
	}
	dadrq->disk    = bi->ri->dsk;
	dadrq->start   = start_sect;
	dadrq->count   = sect_count;
	dadrq->op      = write ? DAD_WRITE : DAD_READ;
	dadrq->buf     = NULL; // initialized later
	dadrq->phys    = 0;    // initialized later
	dadrq->bufsize = sect_count*bi->ri->dsk->sectsize;
	dadrq->done    = dadrq_done_buffered;
	dadrq->client_priv = bi;

	
	// allocate shadow buffer
	dadrq->buf =
		l4dm_mem_allocate(dadrq->bufsize, L4DM_CONTIGUOUS|L4DM_PINNED|L4RM_MAP|L4RM_LOG2_ALIGNED);
	if (! dadrq->buf) {
		LOG_Error("could not allocate shadow buffer");
		free(dadrq);
		return;
	}
	// get physical address
	err = l4dm_mem_phys_addr(dadrq->buf, dadrq->bufsize, &dm_paddr, 1, &tmp);
	if (err != 1) {
		LOG_Error("error getting physical address of shadow buffer!\n");
		free(dadrq->buf);
		free(dadrq);
		return;
	}
	dadrq->phys  = dm_paddr.addr;

	// precopy on write
	if (write)
		copy_buffer(/* to_buffer */ 1, bi->first_ds, dadrq->buf, dadrq->bufsize);
	
	// increment outstanding counter
	bi->ri->outstanding++;

	// print info
	if (dbg_this)
		LOG_printf("new buffered: req=%p dadrq=%p start=%d count=%d\n", bi->ri->req, dadrq, start_sect, sect_count);

	// issue request to driver
	dad_put_request(dadrq);
}

L4_INLINE int bio_direct_possible(struct ds_list_element *ds, int sectsize, int bioalign, int checksize);
L4_INLINE int bio_direct_possible(struct ds_list_element *ds, int sectsize, int bioalign, int checksize) {
	if (checksize) {
		// size matters! ;)
		if (ds->ds_size & (sectsize - 1))
			return 0;
		if (ds->ds_size & (bioalign - 1))
			return 0;
	}
	if (ds->ds_offset & (bioalign - 1))
		return 0;
	return 1;
}

void l4ata_handle_request(dad_disk_t *dsk, bus_request_t *req) {
	l4ata_req_info_t *ri;
	l4ata_bio_info_t *bi;
	int next_sect;
	int sects_todo;
	int buf_bio_bytes=0;
	struct ds_list_element *ds;

	// allocate private request data
	ri = malloc(sizeof(l4ata_req_info_t));
	ri->lock = L4LOCK_UNLOCKED;
	ri->req = req;
	ri->dsk = dsk;
	ri->error = 0;
	ri->outstanding = 0;

	// iterate over ds-s
	l4lock_lock(&ri->lock);
	next_sect  = req->start_sect;
	sects_todo = req->sect_num;
	bi = NULL;
	for (
		ds = (struct ds_list_element *)req->ds_list;
		sects_todo;
		ds++
	) {
		// do not check ds size when buffer bigger than needed
		int ds_sects = ds->ds_size / dsk->sectsize;
		int checksize = (ds_sects >= sects_todo) ? 0 : 1;
		// do direct if possible
		if (bio_direct_possible(ds, dsk->sectsize, BIO_ALIGN, checksize)) {
			if (bi) {
				// send dangling bio
				int sects = buf_bio_bytes / dsk->sectsize;
				start_buffered_bio(bi, next_sect, sects);
				bi = NULL;
				next_sect  += sects;
				sects_todo -= sects;
			}

			// do a direct bio
			if (ds_sects > sects_todo)
				ds_sects = sects_todo;
			bi = malloc(sizeof(l4ata_bio_info_t));
			bi->ri = ri;
			bi->first_ds = ds;
			bi->ds_count = 1;
			start_direct_bio(bi, next_sect, ds_sects);
			next_sect  += ds_sects;
			sects_todo -= ds_sects;
			bi = NULL;
		} else {
			if (!bi) {
				// create a new buffered bio_info
				bi = malloc(sizeof(l4ata_bio_info_t));
				// initialize bio_info fields
				bi->ri = ri;
				bi->first_ds = ds;
				bi->ds_count = 1;
				// reset buffer size
				buf_bio_bytes = 0;
			}
add_ds_to_buffered:
			// increment ds count
			bi->ds_count++;
			// accumulate buf_bio_bytes
			buf_bio_bytes += ds->ds_size;
			// stop processing ds-s if buffer is big enough for request
			if (buf_bio_bytes/dsk->sectsize >= sects_todo)
				break;
			// continue adding ds-s if bio_bytes not sector aligned
			if (buf_bio_bytes & (dsk->sectsize-1)) {
				ds++;
				goto add_ds_to_buffered;
			}
		}
	}
	// send dangling bio
	if (bi) {
		int sects = buf_bio_bytes / dsk->sectsize;
		start_buffered_bio(bi, next_sect, sects);
		bi = NULL;
		next_sect  += sects;
		sects_todo -= sects;
	}
	l4lock_unlock(&ri->lock);
}

/**
 * Generic requests to the bus driver not involving bio's.
 */
int
l4ata_handle_generic_request(
	device_handle_t device,
	int description,
	l4_int64_t in_value,
	int in_buffer_type,
	int in_buffer_size,
	const void * in_buffer,
	int out_buffer_type,
	int * out_buffer_size,
	void ** out_buffer,
	l4_int64_t * return_value)
{
	LOG_Enter("");
	*out_buffer_size = 0;
	return 0;
}

