/**
 * \file   ferret/include/sensors/__slist_magic.h
 * \brief  Template magic for L4Env Semaphore lists
 *
 * \date   2007-06-13
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

#define FERRET_LLIST_LOCK(l)         l4semaphore_down(&((l)->lock))
#define FERRET_LLIST_UNLOCK(l)       l4semaphore_up(&((l)->lock))
#define FERRET_LLIST_LOCAL_LOCK(l)   l4semaphore_down(&((l)->glob->lock))
#define FERRET_LLIST_LOCAL_UNLOCK(l) l4semaphore_up(&((l)->glob->lock))
#define FERRET_LLIST_INIT(l, d)      (l)->lock = L4SEMAPHORE_INIT(1)
#define FERRET_LLIST_DEINIT(l, d)

#undef  FERRET_LLIST_HAVE_LOCK
#undef  ferret_slist_lock_t
#undef  FERRET_LLIST_HAVE_LOCAL_LOCK
#undef  ferret_slist_local_lock_t

#define FERRET_LLIST_HAVE_LOCK
#define ferret_slist_lock_t       l4semaphore_t

#undef  PREFIX
#define PREFIX(a) ferret_slist_ ## a
