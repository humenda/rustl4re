/**
 * \file   ferret/include/sensors/llist.h
 * \brief  locked list generic types.
 *
 * \date   2007-06-12
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_SENSORS_LLIST_H_
#define __FERRET_INCLUDE_SENSORS_LLIST_H_

#include <l4/ferret/types.h>

#define FERRET_LLIST_MAX_COUNT   16*1024*1024  // arbitrary, but large limit
#define FERRET_LLIST_MAX_ELEMENT 4096          // arbitrary, but large limit

/* reuse:
    - ferret_list_entry_t
    - ferret_list_entry_common_header_t
    - ferret_list_entry_common_t
    - ferret_list_entry_kernel_t
 */

/* Fixme: This should merely be a wrapper that sets up
 *        SET_DELAY_PREEMPTION and UNSET_DELAY_PREEMPTION (resp. LOCK and
 *        UNLOCK) to a specific implementation.  We can than include a
 *        template which only uses the macros defined here.
 *
 *        We also need more macros for the lock objects:
 *         - INIT, DESTROY, TYPE, ...
 */

/* 64 bit type with direct access to low 32 bit part
 */
typedef union ferret_llist_index_s
{
    low_high_t lh;
    uint64_t   value;
} ferret_llist_index_t;

#endif
