/**
 * \internal
 * \file   ferret/include/sensors/__llist_consumer.h
 * \brief  locked list consumer functions.
 *
 * \date   2007-05-16
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#if FERRET_LLIST_MAGIC != ahK6eeNa
#error Do not directly include this file, use a proper wrapper!
#endif
#undef FERRET_LLIST_MAGIC

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

/* Consumer local part of the data structure.  Allocated and
 * initialized locally.
 */
typedef struct
{
    ferret_common_t        header;    // copy of the header, all
                                      // sensors should start with
                                      // same header
    ferret_llist_index_t   next_read; // event to read next
    uint64_t               lost;      // number of events lost so far
    PREFIX(t)            * glob;      // pointer to globally shared area
    char                 * out_buf;   // pointer to output element buffer
#ifdef FERRET_LLIST_HAVE_LOCAL_LOCK
    PREFIX(local_lock_t)   llock;     // address space specific part of the lock
#endif
} PREFIX(moni_t);

/**
 * @brief 
 *
 * @param  list locked list sensor to get elements from
 * @param  el   pointer to memory area to copy retrieved element to
 *
 * @return - = 0 ok
 *         - < 0 errorcode
 */
int PREFIX(get)(PREFIX(moni_t) * list, ferret_list_entry_t * el);

/**
 * @brief Setup local struct for sensor
 *
 * @param  addr pointer pointer to global memory area
 * @retval addr pointer to new local area
 */
void PREFIX(init_consumer)(void ** addr);

/**
 * @brief De-allocate locale memory area
 *
 * @param addr  pointer pointer to local memory area
 * @retval addr pointer to global memory
 */
void PREFIX(free_consumer)(void ** addr);

EXTERN_C_END
