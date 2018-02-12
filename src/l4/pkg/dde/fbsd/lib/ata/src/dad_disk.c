#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <machine/stdarg.h>

#include <dde_fbsd/sysinit.h>

#include <l4/dde/ddekit/panic.h>
#include <l4/dde/ddekit/lock.h>
#include <l4/dde/ddekit/printf.h>

#include <l4/dde/fbsd/ata.h>

#include "types.h"

#define dbg_this 0

static dad_disk_announce_t *disk_announce = NULL;

static int buffer_announcements = 1;
static dad_disk_t *ann_buffer = NULL;
static ddekit_lock_t ann_buf_lock;

static void initlock(void *arg) {
	ddekit_lock_init(&ann_buf_lock);
}
SYSINIT(dad_disk_li, SI_DDE_STATLOCK, SI_ORDER_FIRST, initlock, NULL);

void dad_set_announce_disk(dad_disk_announce_t fun) {
	disk_announce = fun;
}

void dad_announce_disk(dad_disk_t *dsk) {
	ddekit_lock_lock(&ann_buf_lock);
	if (buffer_announcements) {
		if (dbg_this)
			ddekit_printf("buffering announcement of %s (%d sects with %d Bytes)\n", dsk->name, dsk->sects, dsk->sectsize);
		dsk->next = NULL;
		if (! ann_buffer)
			ann_buffer = dsk;
		else {
			dad_disk_t *d = ann_buffer;
			while (d->next) d=d->next;
			d->next = dsk;
		}
	} else {
		if (disk_announce) {
			if (dbg_this)
				ddekit_printf("announcing %s (%d sects with %d Bytes)\n", dsk->name, dsk->sects, dsk->sectsize);
			disk_announce(dsk);
		} else
			ddekit_panic("disk found, but no announcing function set yet");
	}
	ddekit_lock_unlock(&ann_buf_lock);
}

static void do_buffered_announcements(void) {
	dad_disk_t *dsk;

	buffer_announcements = 0;
	ddekit_lock_lock(&ann_buf_lock);
	while ((dsk=ann_buffer)) {
		ann_buffer = ann_buffer->next;
		ddekit_lock_unlock(&ann_buf_lock);
		if (dbg_this)
			ddekit_printf("announcing %s (%d sects with %d Bytes)\n", dsk->name, dsk->sects, dsk->sectsize);
		disk_announce(dsk);
		ddekit_lock_lock(&ann_buf_lock);
	}
	ddekit_lock_unlock(&ann_buf_lock);
}
SYSINIT(dad_disk_dba, SI_DDE_LAST, SI_ORDER_ANY, do_buffered_announcements, NULL);

