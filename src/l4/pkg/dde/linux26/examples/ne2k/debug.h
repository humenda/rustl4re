#pragma once
#ifdef __KERNEL__
#include <l4/sys/types.h>
typedef long pthread_t;
extern pthread_t pthread_self(void);
extern l4_cap_idx_t pthread_l4_cap(pthread_t t);
extern long ddekit_self_cap(l4_cap_idx_t);
#endif

#define FERRET_DEBUG 0

enum LOCS
{
	MAIN_START       =  1,
	OPEN_NW_DEV      =  2,
	START_ARPING     =  3,
	START_HANDLE_REQ =  4,
	STOP_HANDLE_REQ  =  5,
	EI_TX_TIMEOUT_1  =  6,
	EI_TX_TIMEOUT_2  =  7,
	EI_TX_TIMEOUT_3  =  8,
	EI_TX_TIMEOUT_4  =  9,
	EI_START_XMIT_1  = 10,
	EI_START_XMIT_2  = 11,
	EI_START_XMIT_3  = 12,
	EI_START_XMIT_4  = 13,
	EI_START_XMIT_5  = 14,
	EI_IRQ_1         = 15,
	EI_IRQ_2         = 16,
	EI_IRQ_3         = 17,
	EI_STATS_1       = 18,
	EI_STATS_2       = 19,
	ETHDEV_SETUP     = 20,
	EI_OPEN_1        = 21,
	EI_OPEN_2        = 22,
	EI_CLOSE_1       = 23,
	EI_CLOSE_2       = 24,
};

#if FERRET_DEBUG

#include <l4/ferret/client.h>
#include <l4/ferret/clock.h>
#include <l4/ferret/types.h>
#include <l4/ferret/util.h>
#include <l4/ferret/comm.h>
#include <l4/ferret/sensors/list_producer.h>
#include <l4/ferret/sensors/list_producer_wrap.h>

#ifndef __KERNEL__
#include <string.h>
#endif

extern ferret_list_local_t *ne2k_sensor;

static void *mwrap(size_t s)
{
#ifdef __KERNEL__
	return kmalloc(s, GFP_KERNEL);
#else
	return malloc(s);
#endif
}

static void fer_send(void)
{
	ferret_list_entry_common_t * elc;

	int idx = ferret_list_dequeue(ne2k_sensor);
	elc = (ferret_list_entry_common_t *)ferret_list_e4i(ne2k_sensor->glob, idx);
	elc->major = FERRET_MONCON_MAJOR;
	elc->minor = FERRET_MONCON_SERSEND;
	strncpy(elc->data8, "foobar", 7);
	ferret_list_commit(ne2k_sensor, idx);
	printk("Dumped %d\n", idx);
}

static void _init_ferret(void)
{
	printk("Establishing monitoring connection.\n");

	l4_cap_idx_t srv = lookup_sensordir();
	ne2k_sensor = NULL;

	int ret = ferret_create(srv, 42, 1, 0, FERRET_LIST, 0, "64:10000", ne2k_sensor, mwrap);
	printk("ferret_create: %d\n", ret);
}

enum minors
{
	DEFAULT_MINOR = 1,
	LOCK_MINOR    = 2,
};

static inline void __do_post_event(int minor, int id, int thread)
{
	if (ne2k_sensor)
		ferret_list_post_2w(ne2k_sensor, 42, minor, 0, id, thread);
}

static inline void do_post_lock_event(int id)
{
	__do_post_event(LOCK_MINOR, id, ddekit_self_cap(pthread_self()));
}

static inline void do_post_event(int id)
{
	__do_post_event(DEFAULT_MINOR, id, ddekit_self_cap(pthread_self()));
}

#else
static inline void do_post_lock_event(int id) {}
static inline void do_post_event(int id) {}
static void _init_ferret(void) {}
#endif /* FERRET_DEBUG */
