/**
 * \file   ferret/include/client.h
 * \brief  Interface for client applications to sensor directory
 *
 * \date   2005-11-07
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_CLIENT_H_
#define __FERRET_INCLUDE_CLIENT_H_

#include <l4/sys/compiler.h>

#include <l4/ferret/types.h>

EXTERN_C_BEGIN

// macro to hide the casting to void ** of addr
#define \
    ferret_create(cap, major, minor, instance, type, flags, config, addr, alloc) \
    ferret_create_sensor((cap), (major), (minor), (instance), (type), (flags),     \
                         (config), (void **)&(addr), (alloc))

// macro to hide the casting to void ** of addr
#define \
    ferret_create_dir(major, minor, instance, type, flags,                  \
                      config, addr, alloc, dir)                             \
    ferret_create_sensor_dir((major), (minor), (instance), (type), (flags), \
                             (config), (void **)&(addr), (alloc), dir)

/**
 * @brief Allocate a new sensor
 *
 * @param  dir_cap  cap to sensor dir
 * @param  major    major identifier for sensor
 * @param  minor    minor identifier for sensor
 * @param  instance instance identifier for sensor
 * @param  type     type of sensor (as defined in ferret/include/types.h)
 * @param  flags    special flags to use for this sensor (as defined in
 *                  ferret/include/types.h)
 * @param  config   sensor specific flags to parameterize this sensor
 * @param  addr     address to map sensor memory to (NULL -> l4rm chooses)
 * @retval addr     address of mapped sensor memory
 * @param  alloc    function pointer to memory allocation function
 *                  (malloc() signature)
 *
 * @return 0 on succes, != 0 otherwise
 */
L4_CV int ferret_create_sensor(l4_cap_idx_t dir_cap,
                               uint16_t major, uint16_t minor,
                               uint16_t instance, uint16_t type,
                               uint32_t flags, const char * config,
                               void ** addr, void *(*alloc)(size_t size));

/**
 * @brief Allocate a new sensor with given thread id for sensor
 *        directory for non-L4Env task
 *
 * @param  major    major identifier for sensor
 * @param  minor    minor identifier for sensor
 * @param  instance instance identifier for sensor
 * @param  type     type of sensor (as defined in ferret/include/types.h)
 * @param  flags    special flags to use for this sensor (as defined in
 *                  ferret/include/types.h)
 * @param  config   sensor specific flags to parameterize this sensor
 * @param  addr     address to map sensor memory to (must be != NULL)
 * @retval addr     address of mapped sensor memory
 * @param  alloc    function pointer to memory allocation function
 *                  (malloc() signature)
 * @param  dir      explicitly given thread id for sensor directory
 *
 * @return 0 on succes, != 0 otherwise
 *
 * Use this function if you cannot ask names (e.g., because you are
 * names) but otherwise know the thread id of the sensor directory.
 * Also this functions does not use l4rm_* to attach the dataspace
 * locally, but l4dm_attache_ds, i.e., you must choose the destination
 * address yourself.
 *
 */
L4_CV int ferret_create_sensor_dir(uint16_t major, uint16_t minor,
                                   uint16_t instance, uint16_t type,
                                   uint32_t flags, const char * config,
                                   void ** addr, void *(*alloc)(size_t size),
                                   l4_cap_idx_t dir);

L4_CV int ferret_free_sensor(uint16_t major, uint16_t minor,
                             uint16_t instance, void * addr,
                             void (*free)(void *));
L4_CV int ferret_create_instance(void);

EXTERN_C_END

#endif
