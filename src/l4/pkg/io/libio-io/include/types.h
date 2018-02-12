/*
 * (c) 2008-2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
#pragma once

#include <l4/vbus/vbus_types.h>

/**
 * \brief Flags for IO memory.
 * \ingroup api_l4io
 */
enum l4io_iomem_flags_t
{
    L4IO_MEM_NONCACHED = 0, /**< Non-cache memory */
    L4IO_MEM_CACHED    = 1, /**< Cache memory */
    L4IO_MEM_USE_MTRR  = 2, /**< Use MTRR */
    L4IO_MEM_ATTR_MASK = 0xf,

    // combinations
    L4IO_MEM_WRITE_COMBINED = L4IO_MEM_USE_MTRR | L4IO_MEM_CACHED,


    /** Use reserved area for mapping I/O memory. Flag only valid
     *  for l4io_request_iomem_region() */
    L4IO_MEM_USE_RESERVED_AREA = 0x40 << 8,
    /** Eagerly map the I/O memory. Passthrough to the l4re-rm. */
    L4IO_MEM_EAGER_MAP         = 0x80 << 8,
};

/**
 * \brief Device types.
 * \ingroup api_l4io
 */
enum l4io_device_types_t {
  L4IO_DEVICE_INVALID = 0, /**< Invalid type */
  L4IO_DEVICE_PCI,         /**< PCI device */
  L4IO_DEVICE_USB,         /**< USB device */
  L4IO_DEVICE_OTHER,       /**< Any other device without unique IDs */
  L4IO_DEVICE_ANY = ~0     /**< any type */
};

/**
 * \brief Resource types
 * \ingroup api_l4io
 */
enum l4io_resource_types_t {
  L4IO_RESOURCE_INVALID = L4VBUS_RESOURCE_INVALID, /**< Invalid type */
  L4IO_RESOURCE_IRQ     = L4VBUS_RESOURCE_IRQ,         /**< Interrupt resource */
  L4IO_RESOURCE_MEM     = L4VBUS_RESOURCE_MEM,         /**< I/O memory resource */
  L4IO_RESOURCE_PORT    = L4VBUS_RESOURCE_PORT,        /**< I/O port resource (x86 only) */
  L4IO_RESOURCE_ANY = ~0     /**< any type */
};


typedef l4vbus_device_handle_t l4io_device_handle_t;
typedef int l4io_resource_handle_t;

/**
 * \brief Resource descriptor.
 * \ingroup api_l4io
 *
 * For IRQ types, the end field is not used, i.e. only a single
 * interrupt can be described with a l4io_resource_t
 */
typedef l4vbus_resource_t l4io_resource_t;

/** Device descriptor.
 * \ingroup api_l4io
 */
typedef l4vbus_device_t l4io_device_t;
