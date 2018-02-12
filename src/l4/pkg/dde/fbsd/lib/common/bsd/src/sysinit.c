/**
 * Implementation of SYSINIT's helpers. Written from scratch.
 *
 * \author Thomas Friebel <tf13@os.inf.tu-dresden.de>
 */
#include <sys/queue.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <dde_fbsd/sysinit.h>

#define dbg_this 0

/** List of SYSINITs.
 */
static TAILQ_HEAD(, sysinit) sysinits = TAILQ_HEAD_INITIALIZER(sysinits);

/**
 * This function is called by the SYSINIT macro to register a SYSINIT.
 * Creates a sorted list of all SYSINITs.
 */
void bsd_register_sysinit(struct sysinit *new) {
	struct sysinit *entry;

	if (TAILQ_EMPTY(&sysinits)) {
		TAILQ_INSERT_HEAD(&sysinits, new, next);
	} else {
		TAILQ_FOREACH(entry, &sysinits, next) {
			if (new->subsystem > entry->subsystem)
				continue;
			if ( (new->subsystem == entry->subsystem) && (new->order > entry->order) )
				continue;
			TAILQ_INSERT_BEFORE(entry, new, next);
			break;
		}
		if (!entry) {
			TAILQ_INSERT_TAIL(&sysinits, new, next);
		}
	}
}

/**
 * Lets the SYSINITs register themselves and call all entries sorted by order.
 */
void bsd_call_sysinits() {
	struct sysinit *entry;

	// let the SYSINITs register
	ddekit_do_initcalls();

	// call every list entry
	while ((entry = TAILQ_FIRST(&sysinits))) {
		if (dbg_this) printf("SYSINIT(0x%08x, 0x%08x, \"%s\")\n", entry->subsystem, entry->order, entry->name);
		entry->func(entry->udata);
		TAILQ_REMOVE(&sysinits, entry, next);
	}
}

