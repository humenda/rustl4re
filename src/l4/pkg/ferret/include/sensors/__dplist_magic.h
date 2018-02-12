/**
 * \file   ferret/include/sensors/__dplist_magic.h
 * \brief  Template magic for dplists
 *
 * \date   2007-06-11
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

#ifdef L4_SUPPORT_DP
#  define FERRET_LLIST_LOCK(a) l4_utcb_delay_preemption_set()
#  define FERRET_LLIST_UNLOCK(a)                                          \
      do                                                                  \
      {                                                                   \
          l4_utcb_delay_preemption_unset();                               \
          if (L4_UNLIKELY(l4_utcb_delay_preemption_triggered()))         \
              l4_yield();                                                 \
      } while (0)
#else
#  define FERRET_LLIST_LOCK(a)
#  define FERRET_LLIST_UNLOCK(a)
#  warning No Delayed Preemption support found, dplist will not work.
#endif
#define FERRET_LLIST_INIT(l, d)
#define FERRET_LLIST_DEINIT(l, d)

#define FERRET_LLIST_LOCAL_LOCK(a)   FERRET_LLIST_LOCK(a)
#define FERRET_LLIST_LOCAL_UNLOCK(a) FERRET_LLIST_UNLOCK(a)

#undef  FERRET_LLIST_HAVE_LOCK
#undef  ferret_dplist_lock_t
#undef  FERRET_LLIST_HAVE_LOCAL_LOCK
#undef  ferret_dplist_local_lock_t

#undef  PREFIX
#define PREFIX(a) ferret_dplist_ ## a
