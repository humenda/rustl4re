/**
 * DAD = Dde Ata Driver
 *
 */
#ifndef _dde_fbsd_ata_h
#define _dde_fbsd_ata_h

#include <l4/sys/types.h>

typedef struct dad_disk {
	unsigned sectsize;
	unsigned sects;
	const char *name;
	void *dde_priv;
	void *client_priv;
	struct dad_disk *next;
} dad_disk_t;

typedef struct dad_request {
	dad_disk_t *disk;
	unsigned start;
	unsigned count;
	enum {
		DAD_READ,
		DAD_WRITE,
	} op;
	void *buf;
	l4_uint32_t phys;
	unsigned bufsize;
	void (*done)(struct dad_request *);
	int error;
	void *client_priv;
} dad_request_t;

typedef void (dad_disk_announce_t)(dad_disk_t *);

void dad_set_announce_disk(dad_disk_announce_t fun);

void dad_put_request(dad_request_t *);

#endif
