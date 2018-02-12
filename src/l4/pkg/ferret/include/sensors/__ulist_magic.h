/**
 * \file   ferret/include/sensors/__ulist_magic.h
 * \brief  Template magic for Fiasco's user semaphore lists
 *
 * \date   2007-06-15
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#undef  FERRET_LLIST_LOCK
#undef  FERRET_LLIST_UNLOCK
#undef  FERRET_LLIST_LOCAL_LOCK
#undef  FERRET_LLIST_LOCAL_UNLOCK
#undef  FERRET_LLIST_INIT
#undef  FERRET_LLIST_DEINIT

#define FERRET_LLIST_LOCAL_LOCK(l)                                            \
    l4_usem_down((l)->llock, &((l)->glob->lock));
#define FERRET_LLIST_LOCAL_UNLOCK(l)                                          \
    l4_usem_up((l)->llock, &((l)->glob->lock));
// fixme: this should be the global init part
#define FERRET_LLIST_INIT(l, d)                                               \
    do                                                                        \
    {                                                                         \
        int _sem = ferret_local_names_reserve();                              \
        if (_sem == 0)                                                        \
        {                                                                     \
            enter_kdebug("PANIC, out of local names for kernel semaphores");  \
        }                                                                     \
        l4_usem_new(_sem, 1, &((l)->lock));                                   \
        *d = (char *)_sem;                                                    \
    } while (0)
#define FERRET_LLIST_DEINIT(l, d)

#define FERRET_LLIST_LOCAL_INIT(l)                                            \
    do                                                                        \
    {                                                                         \
        int ksem, ret;                                                        \
        ksem = ferret_local_names_reserve();                                  \
        if (ksem == 0)                                                        \
        {                                                                     \
            Panic("Could not get local ksem entry!");                         \
        }                                                                     \
        ret = ferret_fpages_request(ksem, (l)->header.major,                  \
                                    (l)->header.minor,                        \
                                    (l)->header.instance);                    \
        if (ret != 0)                                                         \
        {                                                                     \
            Panic("fpages call failed!");                                     \
        }                                                                     \
        (l)->llock = ksem;                                                    \
    } while (0)

#undef  FERRET_LLIST_HAVE_LOCK
#undef  ferret_ulist_lock_t
#undef  FERRET_LLIST_HAVE_LOCAL_LOCK
#undef  ferret_ulist_local_lock_t

#define FERRET_LLIST_HAVE_LOCK
#define ferret_ulist_lock_t          l4_u_semaphore_t

#define FERRET_LLIST_HAVE_LOCAL_LOCK
#define ferret_ulist_local_lock_t    unsigned long

#undef  PREFIX
#define PREFIX(prefix) ferret_ulist_ ## prefix
