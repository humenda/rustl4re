/*
 * This file is part of DDEKit.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *               Thomas Friebel <tf13@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <pthread-l4.h>
#include <sys/time.h>
#include <time.h>

struct ddekit_lock 
{
	pthread_mutex_t mtx;
};


static inline pthread_mutex_t *ddekit_lock_to_pthread(struct ddekit_lock *l)
{
	return (pthread_mutex_t *)&l->mtx;
}


enum
{
	one_thousand = 1000,
	one_million  = 1000000,
	one_billion  = 1000000000,
};


/*
 * The DDEKit interface specifies some functions with relative timeouts
 * that need to be converted to absolute ones to be used with libpthread
 * stuff.
 */
static inline struct timespec ddekit_abs_to_from_rel_ms(int ms)
{
	/*
	 * struct timeval:
	 *    tv_sec
	 *    tv_usec
	 *
	 * struct timespec
	 *    tv_sec
	 *    tv_nsec
	 *
	 * Assumption: to parameter is in msec
	 */
	struct timeval v;
	struct timespec abs_to;
	int r = gettimeofday(&v, NULL);
	Assert(r == 0);

	abs_to.tv_sec = v.tv_sec + ms / one_thousand;
	abs_to.tv_nsec = v.tv_usec * one_thousand + (ms % one_thousand) * one_million;

	/*
	 * Adjust nsec value. Pthreads requires it to be < 1.000.000.000
	 */
	if (abs_to.tv_nsec > one_billion) {
		abs_to.tv_nsec -= one_billion;
		abs_to.tv_sec += 1;
	}

	return abs_to;
}
