#include <sys/types.h>
#include <sys/param.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <geom/geom.h>
#include <sys/systm.h>

#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/pgtab.h>

#include <l4/dde/fbsd/ata.h>

#include "types.h"

#define dbg_this 0

static void bio_done(struct bio *bp) {
	dad_request_t *drq = bio_get_drq(bp);

	if (dbg_this)
		printf("done bio=%p for drq=%p\n", bp, drq);

	// set error value on drq
	drq->error = bp->bio_error;

	// clear PTEs
	dde_clear_ptes(drq->buf, PTE_TYPE_OTHER);

	// call done function
	drq->done(drq);

	// clean up request
	g_destroy_bio(bp);
}

void dad_put_request(dad_request_t *drq) {
	int pages, offs_in_p;
	struct bio *bp;

	// alloc the new bio
	bp = g_alloc_bio();
	if (! bp) {
		dde_debug("out of memory");
		drq->error = 1;
		drq->done(drq);
		return;
	}

	// setup bio
	bio_set_drq(bp, drq);
	bp->bio_done   = bio_done;
	bp->bio_cmd    = (drq->op == DAD_READ) ? BIO_READ : BIO_WRITE;
	bp->bio_data   = drq->buf;
	bp->bio_offset = 1LL*drq->start * drq->disk->sectsize;
	bp->bio_length = drq->bufsize;

	// round up page count
	offs_in_p = (int) drq->buf & PAGE_MASK;
	pages = ((offs_in_p + drq->bufsize) +PAGE_SIZE-1) >> PAGE_SHIFT;
	// set PTEs
	dde_set_ptes(drq->buf, drq->phys, pages, PTE_TYPE_OTHER);

	if (dbg_this)
		printf("send bio=%p for drq=%p (start=%d)\n", bp, drq, drq->start);
	// issue request to geom
	dsk_di(drq->disk)->issue_bio(bp, drq->disk);
}

