#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/kern/vfs_bio.c,v 1.444.2.2 2005/01/31 23:26:18 imp Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

static struct mtx bdonelock;

static void bufinit(void *arg) {
	mtx_init(&bdonelock, "bdone lock", NULL, MTX_DEF);
}
SYSINIT(bufinit, SI_SUB_CPU, SI_ORDER_FIRST, bufinit, NULL);

void
biodone(struct bio *bp)
{
	mtx_lock(&bdonelock);
	bp->bio_flags |= BIO_DONE;
	if (bp->bio_done == NULL)
		wakeup(bp);
	mtx_unlock(&bdonelock);
	if (bp->bio_done != NULL)
		bp->bio_done(bp);
}

int
biowait(struct bio *bp, const char *wchan)
{

	mtx_lock(&bdonelock);
	while ((bp->bio_flags & BIO_DONE) == 0)
		msleep(bp, &bdonelock, PRIBIO, wchan, hz / 10);
	mtx_unlock(&bdonelock);
	if (bp->bio_error != 0)
		return (bp->bio_error);
	if (!(bp->bio_flags & BIO_ERROR))
		return (0);
	return (EIO);
}

void
biofinish(struct bio *bp, struct devstat *stat, int error)
{
	
	if (error) {
		bp->bio_error = error;
		bp->bio_flags |= BIO_ERROR;
	}
	if (stat != NULL)
		devstat_end_transaction_bio(stat, bp);
	biodone(bp);
}

