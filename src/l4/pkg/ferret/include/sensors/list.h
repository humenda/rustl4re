/**
 * \file   ferret/include/sensors/list.h
 * \brief  Functions to operate on event lists.
 *
 * \date   21/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#ifndef __FERRET_INCLUDE_SENSORS_LIST_H_
#define __FERRET_INCLUDE_SENSORS_LIST_H_

#include <l4/sys/compiler.h>

#include <l4/ferret/types.h>
#include <l4/ferret/sensors/common.h>


#define FERRET_INDEX_BITS   16
#define FERRET_ELEMENT_BITS 48

#define FERRET_ASSUMED_CACHE_LINE_SIZE 64

typedef union ferret_list_index_s
{
    // fixme: is this really packed? (uint32 and uint64)
    struct __attribute__ ((__packed__))
    {
        uint32_t index:FERRET_INDEX_BITS;
        uint64_t element:FERRET_ELEMENT_BITS;
    } parts;
    low_high_t   lh;
    uint64_t     value;
} ferret_list_index_t;

/* Merge two uint16 values (major and minor) into one single uint32 */
#define MAJ_MIN(a, b) ((uint32_t)(((uint16_t)(b)) << 16 | ((uint16_t)(a))))

// fixme: maybe I should extract the list element to external header
//        file for easy reuse in other list implementations etc.?
/* The common header of all list elements
 */
typedef struct ferret_list_entry_s
{
    ferret_utime_t timestamp;
    unsigned char  data[0];
} ferret_list_entry_t;

/* A basic event type header, used for cross-component interaction.
 * This is the common header for all events to be used with magpie
 * tools.
 */
typedef struct __attribute__ ((__packed__)) ferret_list_entry_common_header_s
{
    ferret_utime_t timestamp;
    union __attribute__ ((__packed__))
    {
        struct __attribute__ ((__packed__))
        {
            uint16_t major;
            uint16_t minor;
        };
        uint32_t     maj_min;
    };
    uint16_t         instance;
    uint8_t           cpu;
} ferret_list_entry_common_header_t;

/* A basic event type, used for cross-component interaction.
 */
typedef struct __attribute__ ((__packed__)) ferret_list_entry_common_s
{
    ferret_utime_t timestamp;
    union __attribute__ ((__packed__))
    {
        struct __attribute__ ((__packed__))
        {
            uint16_t major;
            uint16_t minor;
        };
        uint32_t     maj_min;
    };
    uint16_t       instance;
    uint8_t        cpu;
    uint8_t        _pad;
    union
    {
        uint64_t   data64[6];
        uint32_t   data32[12];
        uint16_t   data16[24];
        uint8_t    data8[48];
    };
} ferret_list_entry_common_t;

/* Like ferret_list_entry_common_t, but specifies common part for all
 * kernel events
 */
typedef struct __attribute__ ((__packed__)) ferret_list_entry_kernel_s
{
    ferret_utime_t timestamp;
    union __attribute__ ((__packed__))
    {
        struct __attribute__ ((__packed__))
        {
            uint16_t major;
            uint16_t minor;
        };
        uint32_t     maj_min;
    };
    uint16_t       instance;
    uint8_t        cpu;
    uint8_t        _pad;
    void *         context;
    uint32_t       eip;
    uint32_t       pmc1;
    uint32_t       pmc2;
    union
    {
        uint64_t   data64[4];
        uint32_t   data32[8];
        uint16_t   data16[16];
        uint8_t    data8[32];
    };
} ferret_list_entry_kernel_t;


/* list data structures are split in to parts, a shared and a local
 * part.  The shared part contains the list elements and common data
 * structures, such as the head and the tail pointer.  The local parts
 * contains address space specific information, such as pointers to
 * the output buffer and index array.
 */

/* Shared part of the data structure.  Allocated and initialized by the
 * sensor directory.
 */
typedef struct
{
    ferret_common_t      header;

    volatile ferret_list_index_t head;
    volatile ferret_list_index_t tail;
    uint32_t             count;          // max. number of elements
    uint32_t             element_size;   // size of one element
    uint32_t             flags;
    uint32_t             element_offset; // offset for real elements
                                         // relative to data
    // data container, align to probable cache line size
    uint8_t data[0] __attribute__ ((aligned (FERRET_ASSUMED_CACHE_LINE_SIZE)));
} ferret_list_t;

#if 0
/**
 * @brief "entry data for index" returns pointer to data in an element
 *        for the index given
 *
 * @param list  list sensor to work on
 * @param index index of element to get
 *
 * @return pointer to element data
 */
L4_INLINE char * ferret_list_ed4i(ferret_list_t * list, int index);
L4_INLINE char * ferret_list_ed4i(ferret_list_t * list, int index)
{
    return ((ferret_list_entry_t *)(list->data + list->element_offset +
                                    index * list->element_size))->data;
}
#endif

/**
 * @brief "entry for index" returns the element pointer for the index
 *        given
 *
 * @param list  list sensor to work on
 * @param index index of element to get
 *
 * @return pointer to element
 */
L4_INLINE ferret_list_entry_t *
ferret_list_e4i(ferret_list_t * list, int index);
L4_INLINE ferret_list_entry_t *
ferret_list_e4i(ferret_list_t * list, int index)
{
    return (ferret_list_entry_t *)(list->data + list->element_offset +
                                   index * list->element_size);
}

#endif
