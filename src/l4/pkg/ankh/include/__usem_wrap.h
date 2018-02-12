/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/re/c/util/cap_alloc.h>
#include <l4/sys/semaphore.h>
#include <l4/re/env.h>
#include <l4/sys/factory.h>
#include <l4/sys/task.h>

__BEGIN_DECLS

/**
 * Memory alignment requirement for L4 kernel semaphores.
 *
 * Make sure that your memory is aligned to this size when
 * allocating memory for a kernel semaphore dynamically.
 */
#define USEM_LOCK_ALIGN	(2*sizeof(l4_mword_t))

/**
 * Allocate memory that is aligned to USEM_LOCK_ALIGN.
 *
 * #type must contain a member #baseptrname where the
 * pointer to the original base is stored.
 *
 * \param ptr result pointer
 * \param type type to be allocated
 * \param baseptrname name of the base pointer slot
 *        in type
 * \param malloc_fn function for allocating memory
 */
#define USEM_ALLOC_ALIGNED(ptr, type, baseptrname, malloc_fn) \
	do { \
		void *_tmp = (malloc_fn)(sizeof(type) + USEM_LOCK_ALIGN); \
		long _mask = ~(USEM_LOCK_ALIGN - 1); \
		(ptr) = (type *)((char*)_tmp + USEM_LOCK_ALIGN); \
		(ptr) = (type *)((long)(ptr) & _mask); \
		(ptr)->baseptrname = _tmp; \
	} while (0);

L4_INLINE int
 __do_init_lock(l4_u_semaphore_t *sem, l4_cap_idx_t *cap, int init_val);

L4_INLINE int
__init_lock_locked(l4_u_semaphore_t *sem, l4_cap_idx_t *cap);

L4_INLINE int
__init_lock_unlocked(l4_u_semaphore_t *sem, l4_cap_idx_t *cap);

L4_INLINE int
__lock(l4_u_semaphore_t *sem, l4_cap_idx_t cap);

L4_INLINE int
__lock_timed(l4_u_semaphore_t *sem, l4_cap_idx_t cap, l4_timeout_s to);

L4_INLINE int
__unlock(l4_u_semaphore_t *sem, l4_cap_idx_t cap);

L4_INLINE int
__lock_free(l4_cap_idx_t cap);

L4_INLINE int
__locked(l4_u_semaphore_t *s);

/**
 * Initialize u_semaphore.
 *
 * \param sem	semaphore ptr
 * \param cap   cap ptr
 * \param init_val initial counter value
 *
 * \retval cap  semaphore cap
 * \retval sem  semaphore
 * \return 1 for success,  0 on error
 */
L4_INLINE int __do_init_lock(l4_u_semaphore_t *sem,
                             l4_cap_idx_t *cap, int init_val)
{
	*cap = l4re_util_cap_alloc();
	if (l4_is_invalid_cap(*cap))
		return 0;

	l4_msgtag_t ret = l4_factory_create_semaphore(l4re_env()->factory, *cap);
	if (l4_error(ret))
		return 0;

	l4_usem_init(init_val ,sem);

	return 1;
}


/**
 * Initialize u_semaphore in locked state
 *
 * \param sem	semaphore ptr
 * \param cap   cap ptr
 * \param init_val initial counter value
 *
 * \retval cap  semaphore cap
 * \retval sem  semaphore
 * \return 1 for success,  0 on error
 */
L4_INLINE int __init_lock_locked(l4_u_semaphore_t *sem,
                                 l4_cap_idx_t *cap)
{
	return __do_init_lock(sem, cap, 0);
}


/**
 * Initialize u_semaphore in unlocked state
 *
 * \param sem	semaphore ptr
 * \param cap   cap ptr
 * \param init_val initial counter value
 *
 * \retval cap  semaphore cap
 * \retval sem  semaphore
 * \return 1 for success,  0 on error
 */
L4_INLINE int __init_lock_unlocked(l4_u_semaphore_t *sem,
                                   l4_cap_idx_t *cap)
{
	return __do_init_lock(sem, cap, 1);
}


/**
 * Lock u_semaphore.
 *
 * \param sem semaphore
 * \param cap semaphore cap
 *
 * \return 1 success, 0 error
 */
L4_INLINE int __lock(l4_u_semaphore_t *sem, l4_cap_idx_t cap)
{
	return (l4_usem_down(cap, sem).raw == L4_USEM_OK);
}


/**
 * Lock u_semaphore with timeout.
 *
 * \param sem semaphore
 * \param cap semaphore cap
 * \param to  timeout
 * \return 1 success, 0 error
 */
L4_INLINE int __lock_timed(l4_u_semaphore_t *sem,
                           l4_cap_idx_t cap, l4_timeout_s to)
{
	return (l4_usem_down_to(cap, sem, to).raw == L4_USEM_OK);
}


/**
 * Unlock u_semaphore.
 *
 * \param sem semaphore
 * \param cap semaphore cap
 *
 * \return 1 success, 0 error
 */
L4_INLINE int __unlock(l4_u_semaphore_t *sem, l4_cap_idx_t cap)
{
	return (l4_usem_up(cap, sem).raw == L4_USEM_OK);
}


/**
 * Delete u_semaphore.
 *
 * \param cap semaphore cap
 *
 * \return 1 success, 0 error
 */
L4_INLINE int __lock_free(l4_cap_idx_t cap)
{
  l4_msgtag_t ret = l4_task_unmap(L4RE_THIS_TASK_CAP,
                                  l4_obj_fpage(cap, 0, L4_FPAGE_RWX),
                                  L4_FP_ALL_SPACES);

  l4re_util_cap_free(cap);

  return !l4_msgtag_has_error(ret);
}


/*
 * Determine whether a usem is locked.
 *
 * \param s u_semaphore to check.
 * \return 1 if locked, 0 if not
 */
L4_INLINE int __locked(l4_u_semaphore_t *s)
{
  return (s->counter < 1);
}

__END_DECLS
