/**
 * \file   ferret/include/sensors/alist_producer.h
 * \brief  Atomic list producer functions.
 *
 * \date   2007-07-23
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2007-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_SENSORS_ALIST_PRODUCER_H_
#define __FERRET_INCLUDE_SENSORS_ALIST_PRODUCER_H_

#include <l4/sys/compiler.h>
#include <l4/ferret/sensors/alist.h>

EXTERN_C_BEGIN

/**********************************************************************
 * Declarations
 **********************************************************************/

L4_INLINE void
ferret_alist_post(ferret_alist_t * l, uint16_t maj, uint16_t min,
                  uint16_t instance);

L4_INLINE void
ferret_alist_post_2w(ferret_alist_t * l, uint16_t maj, uint16_t min,
                     uint16_t instance, uint32_t d0, uint32_t d1);

L4_INLINE void
ferret_alist_postX_2w(ferret_alist_t * l, uint32_t maj_min,
                      uint16_t instance, uint32_t d0, uint32_t d1);


/**********************************************************************
 * Implementations
 **********************************************************************/

#include <l4/util/atomic.h>
#include <l4/util/rdtsc.h>

//#define KERN_LIB_PAGE_APOST 0xef7ff000
//#define KERN_LIB_PAGE_APOST __ferret_alist_post.bak

L4_INLINE void
ferret_alist_post(ferret_alist_t * l, uint16_t maj, uint16_t min,
                  uint16_t instance)
{
    ferret_list_entry_common_t e;

    e.major     = maj;
    e.minor     = min;
    e.instance  = instance;

    asm volatile (
        "cld                              \n\t"
        "push %%ecx                       \n\t"
        "push %%esi                       \n\t"
        "push %%eax                       \n\t"
//        "call " L4_stringify(KERN_LIB_PAGE_APOST) "-1f+4   \n\t"
//        "1:                               \n\t"
        "call __ferret_alist_post         \n\t"
        "add  $0xc, %%esp                 \n\t"
        :
        : "S"((&(e.timestamp)) + 1), "a"(l), "c"(2)
        : "memory", "edi", "ebx", "edx", "cc"
        );
}

L4_INLINE void
ferret_alist_post_2w(ferret_alist_t * l, uint16_t maj, uint16_t min,
                     uint16_t instance, uint32_t d0, uint32_t d1)
{
    ferret_list_entry_common_t e;

    e.major     = maj;
    e.minor     = min;
    e.instance  = instance;
    e.data32[0] = d0;
    e.data32[1] = d1;

    asm volatile (
        "cld                              \n\t"
        "push %%ecx                       \n\t"
        "push %%esi                       \n\t"
        "push %%eax                       \n\t"
        "call __ferret_alist_post         \n\t"
        "add  $0xc, %%esp                 \n\t"
        :
        : "S"((&(e.timestamp)) + 1), "a"(l), "c"(4)
        : "memory", "edi", "ebx", "edx", "cc"
        );
}

L4_INLINE void
ferret_alist_postX_2w(ferret_alist_t * l, uint32_t maj_min,
                      uint16_t instance, uint32_t d0, uint32_t d1)
{
    ferret_list_entry_common_t e;

    e.maj_min   = maj_min;
    e.instance  = instance;
    e.data32[0] = d0;
    e.data32[1] = d1;

    asm volatile (
        "cld                              \n\t"
        "push %%ecx                       \n\t"
        "push %%esi                       \n\t"
        "push %%eax                       \n\t"
        "call __ferret_alist_post         \n\t"
        "add  $0xc, %%esp                 \n\t"
        :
        : "S"((&(e.timestamp)) + 1), "a"(l), "c"(4)
        : "memory", "edi", "ebx", "edx", "cc"
        );
}


/* Parameter: list pointer .................... %eax (tainted by rdtsc)
 *            pointer to parameters to copy ... %esi (tainted by rep movsl)
 *            word count to copy .............. %ecx (tainted by rep movsl)
 */
asm (
    ".p2align(12)                            \n"
    "__ferret_alist_post.bak:                \n"

    "mov    0x4(%esp),%eax                   \n"
    "mov    0x8(%esp),%esi                   \n"
    "mov    0xc(%esp),%ecx                   \n"

    "mov    0x1c(%eax),%ebx                  \n"  // l->count_mask
    "mov    0x10(%eax),%edx                  \n"  // l->head
    "and    %edx,%ebx                        \n"  // &
    "shl    $0x6,%ebx                        \n"  // *64
    "add    %eax,%ebx                        \n"  // l+offset

    "lea    0x48(%ebx),%edi                  \n"  // dest: &l.data[offset].data
    "rep movsl %ds:(%esi),%es:(%edi)         \n"  // memcpy

    "incl   %edx                             \n"  // ++edx
    "mov    %edx,%ecx                        \n"  // save new head
    "lea    0x10(%eax),%esi                  \n"  // &l->head

    "rdtsc                                   \n"  // get timestamp
    "mov    %eax,0x40(%ebx)                  \n"  // l.data[offset]
    "mov    %edx,0x44(%ebx)                  \n"

    "jmp 1f                                  \n"

    "1:                                      \n"
    "mov    %ecx,(%esi)                      \n"  // save new l->head
    // forward point
    ".p2align(6)                             \n"

    "ret                                     \n"
    );

EXTERN_C_END

#endif
