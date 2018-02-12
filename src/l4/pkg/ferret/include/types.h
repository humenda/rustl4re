/**
 * \file   ferret/include/types.h
 * \brief  Global types for ferret.
 *
 * \date   04/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_TYPES_H_
#define __FERRET_INCLUDE_TYPES_H_

#include <l4/ferret/maj_min.h>
#include <l4/re/c/dataspace.h>

#ifdef __cplusplus
#define __STDC_LIMIT_MACROS
#endif

#if defined (__KERNEL__)
#include <linux/types.h>
#else
#include <stdint.h>
#include <sys/types.h>
#endif

EXTERN_C_BEGIN
/*********************************************************************
 * Simple types
 *********************************************************************/

typedef struct
{
    uint32_t low;  ///< Low 32 Bits
    uint32_t high; ///< High 32 Bits
} low_high_t;

typedef int64_t  ferret_time_t;
typedef uint64_t ferret_utime_t;

#ifndef __KERNEL__
// XXX: This is only used by histogram code _and_ the MAX/MIN macros are
//      missing in Linux kernel code - why not move this somewhere else?
static const ferret_time_t FERRET_TIME_MIN = INT64_MIN;
static const ferret_time_t FERRET_TIME_MAX = INT64_MAX;
#endif

#define FERRET_DIR_NAME "FerretDir"

#define FERRET_ROOT_INSTANCE 0

/*********************************************************************
 * Structure types
 *********************************************************************/

enum SensorType
{
	FERRET_UNDEFINED = 0,
	FERRET_SCALAR    = 1,
	FERRET_HISTO     = 2,
	FERRET_HISTO_2D  = 3,
	FERRET_LIST      = 4,
	FERRET_TBUF      = 5,
	FERRET_HISTO64   = 6,
	/* XXX BjoernD, 2009-03-02: We turn off these special kinds of sensors for now
	 *                          until we know which of these we really want.
	 *                          (Suppose, this was only for Martin's measurements.)
	 */
#if 0
	FERRET_DPLIST    = 7,  // list protected with Delayed Preemption
	FERRET_SLIST     = 8,  // list protected with L4Env Semaphores
	FERRET_ULIST     = 9,  // list protected with Fiasco's User Semaphores
	FERRET_ALIST     = 10, // list protected with Fiasco-supported
	                          // atomic roll-back sections
	FERRET_VLIST     = 11, // list protected with Fiasco-supported
	                       // atomic roll-back sections, supports
	                       // variable element sizes
#endif
};

/*********************************************************************
 * Flags
 *********************************************************************/

enum
{
	FERRET_PERMANENT  = 0x00000001, // keep this sensor in the
	                                // directory, even if its usage
	                                // count drops to zero
	FERRET_SUPERPAGES = 0x00010000, // use superpages for sensor
	                                // dataspace if it would be
	                                // larger than one page
};

EXTERN_C_END
#endif
