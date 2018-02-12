/**
 * \file   ferret/include/sensors/list_producer_wrap.h
 * \brief  Convenience wrapper functions for creating events
 *
 * \date   2006-06-13
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_SENSORS_LIST_PRODUCER_WRAP_H_
#define __FERRET_INCLUDE_SENSORS_LIST_PRODUCER_WRAP_H_

#include <l4/sys/compiler.h>
#include <l4/ferret/sensors/list.h>
#include <l4/ferret/sensors/list_producer.h>

EXTERN_C_BEGIN

/**********************************************************************
 * Declarations
 **********************************************************************/

/**********************************************************************
 * Basic event poster functions
 **********************************************************************/

#define FERRET_POST_sig(a, b...)                                          \
L4_INLINE void                                                            \
ferret_list_post##a(ferret_list_local_t * l, uint16_t maj, uint16_t min,  \
                    uint16_t instance, b)

L4_INLINE void
ferret_list_post(ferret_list_local_t * l, uint16_t maj, uint16_t min,
                 uint16_t instance);

FERRET_POST_sig(_1w, uint32_t d0);
FERRET_POST_sig(_2w, uint32_t d0, uint32_t d1);
FERRET_POST_sig(_3w, uint32_t d0, uint32_t d1, uint32_t d2);
FERRET_POST_sig(_4w, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);

/* No special handling for thread IDs anymore -- there _are_ non in Re. */
#if 0
FERRET_POST_sig(_1t,   l4_threadid_t t);
FERRET_POST_sig(_1t1w, l4_threadid_t t, uint32_t d0);
FERRET_POST_sig(_1t2w, l4_threadid_t t, uint32_t d0, uint32_t d1);
FERRET_POST_sig(_1t3w, l4_threadid_t t, uint32_t d0, uint32_t d1, uint32_t d2);
FERRET_POST_sig(_1t4w, l4_threadid_t t, uint32_t d0, uint32_t d1, uint32_t d2,
                uint32_t d3);
#endif

/**********************************************************************
 * Experimental stuff
 **********************************************************************/

#define FERRET_POSTX_sig(a, b...)                                         \
L4_INLINE void                                                            \
ferret_list_postX##a(ferret_list_local_t * l, uint32_t maj_min,           \
                     uint16_t instance, b)

FERRET_POSTX_sig(_2w, uint32_t d0, uint32_t d1);

/**********************************************************************
 * Conditional macros: here you can dynamically activate / deactivate
 * logging
 **********************************************************************/

#define FERRET_POST_sigc_body(n, a, b...)                                 \
    do                                                                    \
    {                                                                     \
        if (a)                                                            \
        {                                                                 \
            ferret_list_post##n(b);                                       \
        }                                                                 \
    } while (0)

#define ferret_list_post_c(a, b...)   FERRET_POST_sigc_body(   , a, b)
#define ferret_list_post_1wc(a, b...) FERRET_POST_sigc_body(_1w, a, b)
#define ferret_list_post_2wc(a, b...) FERRET_POST_sigc_body(_2w, a, b)
#define ferret_list_post_3wc(a, b...) FERRET_POST_sigc_body(_3w, a, b)
#define ferret_list_post_4wc(a, b...) FERRET_POST_sigc_body(_4w, a, b)

#if 0
#define ferret_list_post_1tc(a, b...)   FERRET_POST_sigc_body(_1t  , a, b)
#define ferret_list_post_1t1wc(a, b...) FERRET_POST_sigc_body(_1t1w, a, b)
#define ferret_list_post_1t2wc(a, b...) FERRET_POST_sigc_body(_1t2w, a, b)
#define ferret_list_post_1t3wc(a, b...) FERRET_POST_sigc_body(_1t3w, a, b)
#define ferret_list_post_1t4wc(a, b...) FERRET_POST_sigc_body(_1t4w, a, b)
#endif

/**********************************************************************
 * Implementations
 **********************************************************************/

#define FERRET_POST_begin                                                 \
{                                                                         \
    int index;                                                            \
    ferret_list_entry_common_t * elc;                                     \
                                                                          \
    index = ferret_list_dequeue(l);                                       \
    elc = (ferret_list_entry_common_t *)ferret_list_e4i(l->glob, index);  \
    elc->major     = maj;                                                 \
    elc->minor     = min;                                                 \
    elc->instance  = instance;                                            \
    elc->cpu       = 0;  // fixme

#define FERRET_POSTX_begin                                                \
{                                                                         \
    int index;                                                            \
    ferret_list_entry_common_t * elc;                                     \
                                                                          \
    index = ferret_list_dequeue(l);                                       \
    elc = (ferret_list_entry_common_t *)ferret_list_e4i(l->glob, index);  \
    elc->maj_min   = maj_min;                                             \
    elc->instance  = instance;                                            \
    elc->cpu       = 0;  // fixme

#define FERRET_POST_end                                                   \
    ferret_list_commit(l, index);                                         \
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

#if 0
#define FERRET_POST_assign_1t                                             \
    elc->data64[0] = t.raw;
#endif


L4_INLINE void
ferret_list_post(ferret_list_local_t * l, uint16_t maj, uint16_t min,
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

#if 0
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
#endif

// cleanup namespace
#undef FERRET_POST_sig
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

EXTERN_C_END

#endif
