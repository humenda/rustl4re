#ifndef _dde_fbsd_ata_types
#define _dde_fbsd_ata_types

#include <sys/types.h>
#include <sys/param.h>
#include <sys/bio.h>

typedef struct dde_disk_info {
	// function to call for putting a bio in queue
	void (*issue_bio)(struct bio *bp, dad_disk_t *dsk);
	// private information for geom backed disk
	struct g_consumer *geom_consumer;
	// private information for nogeom backed disk
	struct disk *bio_disk;
} dde_disk_info_t;

#define dsk_di(dsk) ((dde_disk_info_t *)(dsk->dde_priv))

#define bio_get_drq(bp)         ((dad_request_t *) bp->bio_caller1)
#define bio_set_drq(bp, value)  (bp->bio_caller1 = (void *) value)

void dad_announce_disk(dad_disk_t *dsk);

#endif
