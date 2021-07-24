/* SPDX-License-Identifier: GPL-2.0-only or License-Ref-kk-custom */
/*
 * Copyright (C) 2013-2020 Kernkonzept GmbH.
 * Author(s): Alexander Warg <alexander.warg@kernkonzept.com>
 *            Matthias Lange <matthias.lange@kernkonzept.com>
 *
 */
#pragma once

/**
 * \defgroup l4virtio L4 VIRTIO Interface
 */

/**
 * \defgroup l4virtio_transport L4 VIRTIO Transport Layer
 * \ingroup l4virtio
 *
 * L4 specific VIRTIO Transport layer.
 *
 * The L4 specific VIRTIO Transport layer is based on L4Re::Dataspace as shared
 * memory and L4::Irq for signaling. The VIRTIO configuration space is mostly
 * based on a shared memory implementation too and accompanied by two IPC
 * functions to synchronize the configuration between device and driver.
 *
 * \{
 */

#include <l4/sys/utcb.h>
#include <l4/sys/ipc.h>
#include <l4/sys/types.h>

/** L4-VIRTIO protocol number */
enum L4_virtio_protocol
{
  L4VIRTIO_PROTOCOL = 0,
};

enum L4virtio_magic
{
  L4VIRTIO_MAGIC = 0x74726976
};

enum L4virtio_vendor
{
  L4VIRTIO_VENDOR_KK = 0x44
};

/** L4-VIRTIO opcodes */
enum L4_virtio_opcodes
{
  L4VIRTIO_OP_SET_STATUS  = 0,  /**< Set status register in device config */
  L4VIRTIO_OP_CONFIG_QUEUE,     /**< Set queue config in device config */
  L4VIRTIO_OP_REGISTER_IFACE,   /**< Register a transport driver to the device */
  L4VIRTIO_OP_REGISTER_DS,      /**< Register a data space as transport memory */
};

/** Virtio device IDs as reported in the driver's config space. */
enum L4virtio_device_ids
{
  L4VIRTIO_ID_NET           = 1,      /**< Virtual ethernet card. */
  L4VIRTIO_ID_BLOCK         = 2,      /**< General block device. */
  L4VIRTIO_ID_CONSOLE       = 3,      /**< Simple device for data IO via ports. */
  L4VIRTIO_ID_RNG           = 4,      /**< Entropy source. */
  L4VIRTIO_ID_BALLOON       = 5,      /**< Memory balooning device. */
  L4VIRTIO_ID_RPMSG         = 7,      /**< Device using rpmsg protocol. */
  L4VIRTIO_ID_SCSI          = 8,      /**< SCSI host device. */
  L4VIRTIO_ID_9P            = 9,      /**< Device using 9P transport protocol. */
  L4VIRTIO_ID_RPROC_SERIAL  = 11,     /**< Rproc serial device. */
  L4VIRTIO_ID_CAIF          = 12,     /**< Device using CAIF network protocol. */
  L4VIRTIO_ID_GPU           = 16,     /**< GPU */
  L4VIRTIO_ID_INPUT         = 18,     /**< Input */
  L4VIRTIO_ID_VSOCK         = 19,     /**< Vsock transport */
  L4VIRTIO_ID_CRYPTO        = 20,     /**< Crypto */

  L4VIRTIO_ID_SOCK          = 0x9999, /**< Unofficial socket device. */
};

/** Virtio device status bits. */
enum L4virtio_device_status
{
  L4VIRTIO_STATUS_ACKNOWLEDGE = 1,   /**< Guest OS has found device. */
  L4VIRTIO_STATUS_DRIVER      = 2,   /**< Guest OS knows how to drive device. */
  L4VIRTIO_STATUS_DRIVER_OK   = 4,   /**< Driver is set up. */
  L4VIRTIO_STATUS_FEATURES_OK = 8,   /**< Driver has acknowledged feature set. */
  L4VIRTIO_STATUS_DEVICE_NEEDS_RESET = 0x40, /**< Device detected fatal error. */
  L4VIRTIO_STATUS_FAILED      = 0x80 /**< Driver detected fatal error. */
};

/** L4virtio-specific feature bits. */
enum L4virtio_feature_bits
{
  /// Virtio protocol version 1 supported. Must be 1 for L4virtio.
  L4VIRTIO_FEATURE_VERSION_1  = 32,
  /// Status and queue config are set via cmd field instead of via IPC.
  L4VIRTIO_FEATURE_CMD_CONFIG = 224
};

/**
 * VIRTIO IRQ status codes (l4virtio_config_hdr_t::irq_status).
 * \note l4virtio_config_hdr_t::irq_status is currently unused.
 */
enum L4_virtio_irq_status
{
  L4VIRTIO_IRQ_STATUS_VRING  = 1, /**< VRING IRQ pending flag */
  L4VIRTIO_IRQ_STATUS_CONFIG = 2, /**< CONFIG IRQ pending flag */
};

/**
 * Virtio commands for device configuration.
 */
enum L4_virtio_cmd
{
  L4VIRTIO_CMD_NONE       = 0x00000000, ///< No command pending
  L4VIRTIO_CMD_SET_STATUS = 0x01000000, ///< Set the status register
  L4VIRTIO_CMD_CFG_QUEUE  = 0x02000000, ///< Configure a queue
  L4VIRTIO_CMD_MASK       = 0xff000000, ///< Mask to get command bits
};

/**
 * L4-VIRTIO config header, provided in shared data space.
 */
typedef struct l4virtio_config_hdr_t
{
  l4_uint32_t magic;          /**< magic value (must be 'virt'). */
  l4_uint32_t version;        /**< VIRTIO version */
  l4_uint32_t device;         /**< device ID */
  l4_uint32_t vendor;         /**< vendor ID */

  l4_uint32_t dev_features;   /**< device features windows selected by device_feature_sel */
  l4_uint32_t dev_features_sel;
  l4_uint32_t _res1[2];

  l4_uint32_t driver_features;
  l4_uint32_t driver_features_sel;

  /* some L4virtio specific members ... */
  l4_uint32_t num_queues;     /**< number of virtqueues */
  l4_uint32_t queues_offset;  /**< offset of virtqueue config array */

  /* must start at 0x30 (per virtio-mmio layout) */
  l4_uint32_t queue_sel;
  l4_uint32_t queue_num_max;
  l4_uint32_t queue_num;
  l4_uint32_t _res3[2];
  l4_uint32_t queue_ready;
  l4_uint32_t _res4[2];

  l4_uint32_t queue_notify;
  l4_uint32_t _res5[3];

  l4_uint32_t irq_status;
  l4_uint32_t irq_ack;
  l4_uint32_t _res6[2];

  /**
   * Device status register (read-only). The register must be written
   * using l4virtio_set_status().
   *
   * must be at offset 0x70 (virtio-mmio)
   */
  l4_uint32_t status;
  l4_uint32_t _res7[3];

  l4_uint64_t queue_desc;
  l4_uint32_t _res8[2];
  l4_uint64_t queue_avail;
  l4_uint32_t _res9[2];
  l4_uint64_t queue_used;

  /* use the unused space here for device and driver feature bitmaps */
  l4_uint32_t dev_features_map[8];
  l4_uint32_t driver_features_map[8];

  l4_uint32_t _res10[2];

  /** W: Event index to be used for config notifications (device to driver) */
  l4_uint32_t cfg_driver_notify_index;
  /** R: Event index to be used for config notifications (driver to device) */
  l4_uint32_t cfg_device_notify_index;

  /** L4 specific command register polled by the driver iff supported */
  l4_uint32_t cmd;
  l4_uint32_t generation;
} l4virtio_config_hdr_t;

/**
 * Queue configuration entry. An array of such entries is available at the
 * l4virtio_config_hdr_t::queues_offset in the config data space.
 *
 * Consistency rules for the queue config are:
 *  - A driver might read `num_max` at any time.
 *  - A driver must write to `num`, `desc_addr`, `avail_addr`, and `used_addr`
 *    only when `ready` is zero (0). Values in these fields are validated and
 *    used by the device only after successfully setting `ready` to one (1),
 *    either by the IPC or by L4VIRTIO_CMD_CFG_QUEUE.
 *  - The value of `device_notify_index` is valid only when `ready` is one.
 *  - The driver might write to `device_notify_index` at any time, however
 *    the change is guaranteed to take effect after a successful
 *    L4VIRTIO_CMD_CFG_QUEUE or after a config_queue IPC. Note, the change
 *    might also have immediate effect, depending on the device
 *    implementation.
 */
typedef struct l4virtio_config_queue_t
{
  /** R: maximum number of descriptors supported by this queue*/
  l4_uint16_t num_max;
  /** RW: number of descriptors configured for this queue */
  l4_uint16_t num;

  /** RW: queue ready flag (read-write) */
  l4_uint16_t ready;

  /** W: Event index to be used for device notifications (device to driver) */
  l4_uint16_t driver_notify_index;

  l4_uint64_t desc_addr;  /**< W: address of descriptor table */
  l4_uint64_t avail_addr; /**< W: address of available ring */
  l4_uint64_t used_addr;  /**< W: address of used ring */

  /** R: Event index to be used by the driver (driver to device) */
  l4_uint16_t device_notify_index;
} l4virtio_config_queue_t;

EXTERN_C_BEGIN

/**
 * Get the pointer to the first queue config.
 * \param cfg  Pointer to the config header.
 * \return pointer to queue config of queue 0.
 */
L4_INLINE l4virtio_config_queue_t *
l4virtio_config_queues(l4virtio_config_hdr_t const *cfg)
{
  return (l4virtio_config_queue_t *)(((l4_addr_t)cfg) + cfg->queues_offset);
}

/**
 * Get the pointer to the device configuration.
 * \param cfg  Pointer to the config header.
 * \return pointer to device configuration structure.
 */
L4_INLINE void *
l4virtio_device_config(l4virtio_config_hdr_t const *cfg)
{
  return (void *)(((l4_addr_t)cfg) + 0x100);
}

/**
 * Set the given feature bit in a feature map.
 */
L4_INLINE void
l4virtio_set_feature(l4_uint32_t *feature_map, unsigned feat)
{
  unsigned idx = feat / 32;

  if (idx < 8)
    feature_map[idx] |= 1UL << (feat % 32);
}

/**
 * Clear the given feature bit in a feature map.
 */
L4_INLINE void
l4virtio_clear_feature(l4_uint32_t *feature_map, unsigned feat)
{
  unsigned idx = feat / 32;

  if (idx < 8)
    feature_map[idx] &= ~(1UL << (feat % 32));
}

/**
 * Check if the given bit in a feature map is set.
 */
L4_INLINE unsigned
l4virtio_get_feature(l4_uint32_t *feature_map, unsigned feat)
{
  unsigned idx = feat / 32;

  if (idx >= 8)
    return 0;

  return feature_map[idx] & (1UL << (feat % 32));
}

/**
 * \param cap    Capability to the VIRTIO host
 *
 * \copydoc L4virtio::Device::set_status
 */
L4_CV int
l4virtio_set_status(l4_cap_idx_t cap, unsigned status) L4_NOTHROW;

/**
 * \param cap    Capability to the VIRTIO host.
 *
 * \copydoc L4virtio::Device::config_queue
 */
L4_CV int
l4virtio_config_queue(l4_cap_idx_t cap, unsigned queue) L4_NOTHROW;

/**
 * \param cap     Capability to the VIRTIO host
 *
 * \copydoc L4virtio::Device::register_ds
 */
L4_CV int
l4virtio_register_ds(l4_cap_idx_t cap, l4_cap_idx_t ds_cap,
                     l4_uint64_t base, l4_umword_t offset,
                     l4_umword_t size) L4_NOTHROW;

/**
 * \param cap     Capability to the L4-VIRTIO host
 *
 * \copydoc L4virtio::Device::register_iface
 */
L4_CV int
l4virtio_register_iface(l4_cap_idx_t cap, l4_cap_idx_t guest_irq,
                        l4_cap_idx_t host_irq, l4_cap_idx_t config_ds) L4_NOTHROW;

/**
 * \param cap     Capability to the L4-VIRTIO host
 *
 * \copydoc L4virtio::Device::device_config
 */
L4_CV int
l4virtio_device_config_ds(l4_cap_idx_t cap, l4_cap_idx_t config_ds,
                          l4_addr_t *ds_offset) L4_NOTHROW;

/**
 * \param cap     Capability to the L4-VIRTIO host
 *
 * \copydoc L4virtio::Device::device_notification_irq
 */
L4_CV int
l4virtio_device_notification_irq(l4_cap_idx_t cap, unsigned index,
                                 l4_cap_idx_t irq) L4_NOTHROW;

EXTERN_C_END

/**\}*/
