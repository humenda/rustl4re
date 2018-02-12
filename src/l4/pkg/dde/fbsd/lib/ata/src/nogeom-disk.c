#include <sys/types.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <geom/geom.h>
#include <geom/geom_disk.h>
#include <l4/dde/ddekit/panic.h>

#include <dde_fbsd/sysinit.h>

#include <l4/dde/fbsd/ata.h>

#include "types.h"

#define dbg_this 0

MALLOC_DEFINE(M_NOGEOM, "nogeom disks", "nogeom disks")

static ddekit_lock_t done_lock;

static void initlock(void *arg) {
	ddekit_lock_init(&done_lock);
}
SYSINIT(nogeom_disk_li, SI_DDE_STATLOCK, SI_ORDER_FIRST, initlock, NULL);

/**
 * Taken and slightly modified from geom/geom_disk.c#191
 */
static void my_g_disk_done(struct bio *bp) {
	struct bio *bp2;

	ddekit_lock_lock(&done_lock);
	bp->bio_completed = bp->bio_length - bp->bio_resid;

	bp2 = bp->bio_parent;
	if (bp2->bio_error == 0)
		bp2->bio_error = bp->bio_error;
	bp2->bio_completed += bp->bio_completed;
	g_destroy_bio(bp);
	bp2->bio_inbed++;
	if (bp2->bio_children == bp2->bio_inbed) {
		bp2->bio_resid = bp2->bio_bcount - bp2->bio_completed;
		g_io_deliver(bp2, bp2->bio_error);
	}
	ddekit_lock_unlock(&done_lock);
}

static void issue_bio(struct bio *bp, dad_disk_t *dsk) {
	struct disk *dp;
	
	dp = dsk_di(dsk)->bio_disk;

	bp->bio_disk   = dp;
	bp->bio_pblkno = bp->bio_offset / dp->d_sectorsize;
	bp->bio_bcount = bp->bio_length;

	{
		struct bio *bp2, *bp3;
		off_t off;

		off = 0;
		bp3 = NULL;
		bp2 = g_clone_bio(bp);
		if (bp2 == NULL) {
			g_io_deliver(bp, ENOMEM);
			return;
		}
		do {
			bp2->bio_offset += off;
			bp2->bio_length -= off;
			bp2->bio_data += off;
			if (bp2->bio_length > dp->d_maxsize) {
				/*
				 * XXX: If we have a stripesize we should really
				 * use it here.
				 */
				bp2->bio_length = dp->d_maxsize;
				off += dp->d_maxsize;
				/*
				 * To avoid a race, we need to grab the next bio
				 * before we schedule this one.  See "notes".
				 */
				bp3 = g_clone_bio(bp);
				if (bp3 == NULL)
					bp->bio_error = ENOMEM;
			}
			bp2->bio_done = my_g_disk_done;
			bp2->bio_pblkno = bp2->bio_offset / dp->d_sectorsize;
			bp2->bio_bcount = bp2->bio_length;
			bp2->bio_disk = dp;
			if (dp->d_flags & DISKFLAG_NEEDSGIANT) mtx_lock(&Giant);
			dp->d_strategy(bp2);
			if (dp->d_flags & DISKFLAG_NEEDSGIANT) mtx_unlock(&Giant);
			bp2 = bp3;
			bp3 = NULL;
		} while (bp2 != NULL);
	}
}

struct disk *disk_alloc() {
	struct disk *dp;
	dp = malloc(sizeof *dp, M_NOGEOM, M_WAITOK|M_ZERO);
	return (dp);
}

void disk_destroy(struct disk *dp) {
	free(dp, M_NOGEOM);
}

void disk_create(struct disk *dp, int version) {
	dad_disk_t *dsk;
	dde_disk_info_t *di;
	int namelen;
	char *name;

	di = (dde_disk_info_t *) malloc(sizeof(*di), M_NOGEOM, M_WAITOK|M_ZERO);
	di->issue_bio = issue_bio;
	di->bio_disk = dp;

	namelen = strlen(dp->d_name);
	name = (char *) malloc(namelen+3, M_NOGEOM, M_WAITOK);
	snprintf(name, namelen+3, "%s%d", dp->d_name, dp->d_unit);

	dsk = malloc(sizeof *dsk, M_NOGEOM, M_WAITOK|M_ZERO);
	dsk->sectsize = dp->d_sectorsize;
	dsk->sects    = dp->d_mediasize / dp->d_sectorsize;
	dsk->name     = name;
	dsk->dde_priv = di;

	dad_announce_disk(dsk);
}
