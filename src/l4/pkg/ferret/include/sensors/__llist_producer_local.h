/**
 * \internal
 * \file   ferret/include/sensors/__llist_producer_local.h
 * \brief  locked list producer functions with producer-local part.
 *
 * \date   2007-06-19
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
#include <l4/sys/ipc.h>

EXTERN_C_BEGIN

/**********************************************************************
 * Declarations
 **********************************************************************/

/* Producer local part of the data structure.  Allocated and
 * initialized locally.
 */
typedef struct
{
    ferret_common_t       header;    // copy of the header, all
                                     // sensors should start with
                                     // same header
    PREFIX(t)           * glob;      // pointer to globally shared area
#ifdef FERRET_LLIST_HAVE_LOCAL_LOCK
    PREFIX(local_lock_t)  llock;     // address space specific part of the lock
#endif
} PREFIX(local_t);

/**********************************************************************
 * Basic event poster functions
 **********************************************************************/

#define FERRET_POST_sig(a, b...)                                          \
L4_INLINE void                                                            \
PREFIX(post##a)(PREFIX(local_t) * l, uint16_t maj, uint16_t min,          \
                uint16_t instance, b)

L4_INLINE void
PREFIX(post)(PREFIX(local_t) * l, uint16_t maj, uint16_t min,
             uint16_t instance);

FERRET_POST_sig(_1w, uint32_t d0);
FERRET_POST_sig(_2w, uint32_t d0, uint32_t d1);
FERRET_POST_sig(_3w, uint32_t d0, uint32_t d1, uint32_t d2);
FERRET_POST_sig(_4w, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);

FERRET_POST_sig(_1t,   l4_threadid_t t);
FERRET_POST_sig(_1t1w, l4_threadid_t t, uint32_t d0);
FERRET_POST_sig(_1t2w, l4_threadid_t t, uint32_t d0, uint32_t d1);
FERRET_POST_sig(_1t3w, l4_threadid_t t, uint32_t d0, uint32_t d1, uint32_t d2);
FERRET_POST_sig(_1t4w, l4_threadid_t t, uint32_t d0, uint32_t d1, uint32_t d2,
                uint32_t d3);

/**********************************************************************
 * Experimental stuff
 **********************************************************************/

#define FERRET_POSTX_sig(a, b...)                                         \
L4_INLINE void                                                            \
PREFIX(postX##a)(PREFIX(local_t) * l, uint32_t maj_min,                   \
                uint16_t instance, b)

FERRET_POSTX_sig(_2w, uint32_t d0, uint32_t d1);

/**********************************************************************
 * Conditional macros: here you can dynamically activate / deactivate
 * logging
 **********************************************************************/

#define FERRET_POST_sigc(n, b...)                                         \
    L4_INLINE void PREFIX(post##n##c)(int a, PREFIX(local_t) * l,         \
                                      uint16_t maj, uint16_t min,         \
                                      uint16_t instance, b)

L4_INLINE void PREFIX(post_c)(int a, PREFIX(local_t) * l, uint16_t maj,
                              uint16_t min, uint16_t instance);
FERRET_POST_sigc(_1w, uint32_t d0);
FERRET_POST_sigc(_2w, uint32_t d0, uint32_t d1);
FERRET_POST_sigc(_3w, uint32_t d0, uint32_t d1, uint32_t d2);
FERRET_POST_sigc(_4w, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);

FERRET_POST_sigc(_1t1w, l4_threadid_t t, uint32_t d0);
FERRET_POST_sigc(_1t2w, l4_threadid_t t, uint32_t d0, uint32_t d1);
FERRET_POST_sigc(_1t3w, l4_threadid_t t, uint32_t d0, uint32_t d1, uint32_t d2);
FERRET_POST_sigc(_1t4w, l4_threadid_t t, uint32_t d0, uint32_t d1, uint32_t d2,
                 uint32_t d3);

/**
 * @brief Setup local struct for sensor
 *
 * @param  addr  pointer pointer to global memory area
 * @param  alloc function pointer to memory allocating function
 * @retval addr  pointer to new local area
 */
void PREFIX(init_producer)(void ** addr, void *(*alloc)(size_t size));

/**
 * @brief De-allocate local memory area
 *
 * @param  addr pointer pointer to local memory area
 * @param  free function pointer to memory freeing function
 * @retval addr pointer to referenced global memory
 */
void PREFIX(free_producer)(void ** addr, void (*free)(void *));

/**********************************************************************
 * Implementations
 **********************************************************************/

#include <l4/util/atomic.h>
#include <l4/util/rdtsc.h>

#define FERRET_POST_begin                                                 \
{                                                                         \
    uint32_t index;                                                       \
    ferret_list_entry_common_t * elc;                                     \
                                                                          \
    FERRET_LLIST_LOCAL_LOCK(l);                                           \
    index = l->glob->head.value & l->glob->count_mask;                    \
    elc = (ferret_list_entry_common_t*)                                   \
        (l->glob->data + (index << l->glob->element_size_ld));            \
    elc->timestamp = l4_rdtsc();                                          \
    elc->major     = maj;                                                 \
    elc->minor     = min;                                                 \
    elc->instance  = instance;                                            \
    elc->cpu       = 0;  // fixme

#define FERRET_POSTX_begin                                                \
{                                                                         \
    uint32_t index;                                                       \
    ferret_list_entry_common_t * elc;                                     \
                                                                          \
    FERRET_LLIST_LOCAL_LOCK(l);                                           \
    index = l->glob->head.value & l->glob->count_mask;                    \
    elc = (ferret_list_entry_common_t*)                                   \
        (l->glob->data + (index << l->glob->element_size_ld));            \
    elc->timestamp = l4_rdtsc();                                          \
    elc->maj_min   = maj_min;                                             \
    elc->instance  = instance;                                            \
    elc->cpu       = 0;  // fixme

#define FERRET_POST_end                                                   \
    l->glob->head.value++;                                                \
    FERRET_LLIST_LOCAL_UNLOCK(l);                                         \
}

#define FERRET_POST_assign_1w(o)                                          \
    elc->data32[0 + (o)] = d0;

#define FERRET_POST_assign_2w(o)                                          \
    elc->data32[0 + (o)] = d0;                                            \
    elc->data32[1 + (o)] = d1;

#define FERRET_POST_assign_3w(o)                                          \
    elc->data32[0 + (o)] = d0;                                            \
    elc->data32[1 + (o)] = d1;                                            \
    elc->data32[2 + (o)] = d2;

#define FERRET_POST_assign_4w(o)                                          \
    elc->data32[0 + (o)] = d0;                                            \
    elc->data32[1 + (o)] = d1;                                            \
    elc->data32[2 + (o)] = d2;                                            \
    elc->data32[3 + (o)] = d3;

#define FERRET_POST_assign_1t                                             \
    elc->data64[0] = t.raw;


L4_INLINE void
PREFIX(post)(PREFIX(local_t) * l, uint16_t maj, uint16_t min,
             uint16_t instance)
FERRET_POST_begin
FERRET_POST_end

FERRET_POST_sig(_1w, uint32_t d0)
FERRET_POST_begin
FERRET_POST_assign_1w(0)
FERRET_POST_end

FERRET_POST_sig(_2w, uint32_t d0, uint32_t d1)
FERRET_POST_begin
FERRET_POST_assign_2w(0)
FERRET_POST_end

FERRET_POSTX_sig(_2w, uint32_t d0, uint32_t d1)
FERRET_POSTX_begin
FERRET_POST_assign_2w(0)
FERRET_POST_end

FERRET_POST_sig(_3w, uint32_t d0, uint32_t d1, uint32_t d2)
FERRET_POST_begin
FERRET_POST_assign_3w(0)
FERRET_POST_end

FERRET_POST_sig(_4w, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3)
FERRET_POST_begin
FERRET_POST_assign_4w(0)
FERRET_POST_end

FERRET_POST_sig(_1t, l4_threadid_t t)
FERRET_POST_begin
FERRET_POST_assign_1t
FERRET_POST_end

FERRET_POST_sig(_1t1w, l4_threadid_t t, uint32_t d0)
FERRET_POST_begin
FERRET_POST_assign_1t
FERRET_POST_assign_1w(1)
FERRET_POST_end

FERRET_POST_sig(_1t2w, l4_threadid_t t, uint32_t d0, uint32_t d1)
FERRET_POST_begin
FERRET_POST_assign_1t
FERRET_POST_assign_2w(1)
FERRET_POST_end

FERRET_POST_sig(_1t3w, l4_threadid_t t, uint32_t d0, uint32_t d1, uint32_t d2)
FERRET_POST_begin
FERRET_POST_assign_1t
FERRET_POST_assign_3w(1)
FERRET_POST_end

FERRET_POST_sig(_1t4w, l4_threadid_t t, uint32_t d0, uint32_t d1, uint32_t d2,
                uint32_t d3)
FERRET_POST_begin
FERRET_POST_assign_1t
FERRET_POST_assign_4w(1)
FERRET_POST_end

#define FERRET_POST_bodyc(n, b...)                                        \
    {                                                                     \
        if (a)                                                            \
        {                                                                 \
            PREFIX(post##n)(l, maj, min, instance, b);                    \
        }                                                                 \
    }

L4_INLINE void PREFIX(post_c)(int a, PREFIX(local_t) * l, uint16_t maj, 
                              uint16_t min, uint16_t instance)
{
    if (a)
    {
        PREFIX(post)(l, maj, min, instance);
    }
}

FERRET_POST_sigc (_1w, uint32_t d0)
FERRET_POST_bodyc(_1w, d0)

FERRET_POST_sigc (_2w, uint32_t d0, uint32_t d1)
FERRET_POST_bodyc(_2w, d0, d1)

FERRET_POST_sigc (_3w, uint32_t d0, uint32_t d1, uint32_t d2)
FERRET_POST_bodyc(_3w, d0, d1, d2)

FERRET_POST_sigc (_4w, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3)
FERRET_POST_bodyc(_4w, d0, d1, d2, d3)

FERRET_POST_sigc (_1t1w, l4_threadid_t t, uint32_t d0)
FERRET_POST_bodyc(_1t1w, t, d0)

FERRET_POST_sigc (_1t2w, l4_threadid_t t, uint32_t d0, uint32_t d1)
FERRET_POST_bodyc(_1t2w, t, d0, d1)

FERRET_POST_sigc (_1t3w, l4_threadid_t t, uint32_t d0, uint32_t d1, uint32_t d2)
FERRET_POST_bodyc(_1t3w, t, d0, d1, d2)

FERRET_POST_sigc (_1t4w, l4_threadid_t t, uint32_t d0, uint32_t d1, uint32_t d2,
                  uint32_t d3)
FERRET_POST_bodyc(_1t4w, t, d0, d1, d2, d3)


// cleanup namespace
#undef FERRET_POST_sig
#undef FERRET_POST_sigc
#undef FERRET_POSTX_sig
#undef FERRET_POST_begin
#undef FERRET_POSTX_begin
#undef FERRET_POST_end
#undef FERRET_POST_assign_1w
#undef FERRET_POST_assign_2w
#undef FERRET_POSTX_assign_2w
#undef FERRET_POST_assign_3w
#undef FERRET_POST_assign_4w
#undef FERRET_POST_assign_1t
#undef FERRET_POST_bodyc

EXTERN_C_END
