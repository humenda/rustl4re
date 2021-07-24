/*
 * (c) 2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *          Alexander Warg <warg@os.inf.tu-dresden.de>,
 *          Torsten Frenzel <frenzel@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
/**
 * \file vbus.h
 * Description of the vbus C API
 */
#pragma once

#include <l4/sys/compiler.h>
#include <l4/vbus/vbus_types.h>
#include <l4/sys/types.h>

/** Constants for device nodes */
enum {
  L4VBUS_NULL = (l4_mword_t)-1,    /**< NULL device */
  L4VBUS_ROOT_BUS = 0,             /**< Root device on the vbus */
};

/** Flags that can be used with the ICU on a vbus device */
enum l4vbus_icu_src_types {
    /**
     * Flag to denote that the value should be interpreted as a device handle.
     *
     * This flag may be used in the `source` parameter in l4_icu_msi_info() to
     * denote that the ICU should interpret the source ID as a device handle.
     */
    L4VBUS_ICU_SRC_DEV_HANDLE = 1ULL << 63
};

/**
 * \defgroup l4vbus_module L4 Vbus functions
 * \{
 * C interface of the Vbus API.
 *
 * The virtual bus (Vbus) is a hierarchical (tree) structure of device nodes
 * where each device has a set of resources attached to it. Each virtual bus
 * provides an Icu (\ref l4_icu_api) for interrupt handling.
 *
 * The Vbus interface allows a client to find and query devices present on his
 * virtual bus. After obtaining a device handle for a specific device the
 * client can enumerate its resources.
 *
 * \includefile{l4/vbus/vbus.h}
 *
 * Refer to \ref L4vbus for the C++ API.
 */

__BEGIN_DECLS

/**
 * \copybrief L4vbus::Device::device_by_hid()
 * \param  vbus         Capability of the system bus
 * \param  parent       Handle to the parent to start the search
 *
 * \copydetails L4vbus::Device::device_by_hid()
 */
int L4_CV
l4vbus_get_device_by_hid(l4_cap_idx_t vbus, l4vbus_device_handle_t parent,
                         l4vbus_device_handle_t *child, char const *hid,
                         int depth, l4vbus_device_t *devinfo);

/**
 * \copybrief L4vbus::Device::next_device()
 * \param           vbus     Capability of the system bus
 * \param           parent   Handle to the parent device (use #L4VBUS_ROOT_BUS
 *                           for the system bus)
 * \param[in, out]  child    Handle of the device that precedes the device
 *                           that shall be returned. To start from the
 *                           beginning, `child` must be initialized with
 *                           #L4VBUS_NULL. If a device is found, its handle is
 *                           returned through this parameter.
 * \param           depth    Depth to look for
 * \param[out]      devinfo  Device information (might be NULL)
 *
 * \return 0 on success, else failure
 */
int L4_CV
l4vbus_get_next_device(l4_cap_idx_t vbus, l4vbus_device_handle_t parent,
                       l4vbus_device_handle_t *child, int depth,
                       l4vbus_device_t *devinfo);

/**
 * \copybrief L4vbus::Device::device()
 * \param vbus          Capability of the vbus to which the device is
 *                      connected.
 * \param dev           Device handle of the device from which to retrieve the
 *                      details.
 * \param[out] devinfo  Information structure which contains details about
 *                      the device. The pointer might be NULL.
 *
 * \retval 0           Success.
 * \retval -L4_ENODEV  No device with the given device handle `dev` could be
 *                     found.
 */
int L4_CV
l4vbus_get_device(l4_cap_idx_t vbus, l4vbus_device_handle_t dev,
                  l4vbus_device_t *devinfo);

/**
 * \copybrief L4vbus::Device::get_resource()
 * \param vbus  Capability of the vbus to which the device is connected.
 * \param dev   Device handle of the device on the vbus. The device handle can
 *              be obtained by using the l4vbus_get_device_by_hid() and
 *              l4vbus_get_next_device() functions.
 * \copydetails L4vbus::Device::get_resource()
 */
int L4_CV
l4vbus_get_resource(l4_cap_idx_t vbus, l4vbus_device_handle_t dev,
                    int res_idx, l4vbus_resource_t *res);


/**
 * \copybrief L4vbus::Device::is_compatible()
 * \param  vbus Capability of the system bus
 * \param  dev  device handle for which the CID shall be tested
 * \copydetails L4vbus::Device::is_compatible()
 */
int L4_CV
l4vbus_is_compatible(l4_cap_idx_t vbus, l4vbus_device_handle_t dev,
                     char const *cid);

/**
 * \brief Get the HID (hardware identifier) of a device
 *
 * \param  vbus         Capability of the system bus
 * \param  dev          Handle of the device
 * \param  hid          Pointer to a buffer for the HID string
 * \param  max_len      The size of the buffer (\a hid)
 *
 * \return the length of the HID string on success, else failure
 */
int L4_CV
l4vbus_get_hid(l4_cap_idx_t vbus, l4vbus_device_handle_t dev, char *hid,
               unsigned long max_len);

/**
 * Request an IO port resource.
 *
 * \param     vbus  Capability of the system bus.
 * \param[in] res   The IO port resource to be requested from the bus.
 *
 * \retval 0           Success.
 * \retval -L4_EINVAL  Resource is not an IO port resource.
 * \retval -L4_ENOENT  No matching IO port resource found.
 *
 * If any IO port resource is found that contains the requested IO port range
 * the IO ports are obtained.
 */
int L4_CV
l4vbus_request_ioport(l4_cap_idx_t vbus, l4vbus_resource_t const *res);

/**
 * Request a resource of a specific type
 *
 * \param     vbus   Capability of the system bus.
 * \param[in] res    Descriptor of the resource.
 * \param     flags  Ignored.
 *
 * \return 0 on success, else failure.
 *
 * If any resource is found that contains the requested type and resource
 * address range the resource is obtained.
 *
 * \deprecated This function is deprecated since Q3 2019.
 *             Use l4vbus_request_ioport() instead.
 */
int L4_CV
L4_DEPRECATED("use l4vbus_request_ioport")
l4vbus_request_resource(l4_cap_idx_t vbus, l4vbus_resource_t const *res,
                        int flags);

/**
 * Flags for l4vbus_assign_dma_domain().
 */
enum L4vbus_dma_domain_assign_flags
{
  /** Unbind the given DMA space from the DMA domain. */
  L4VBUS_DMAD_UNBIND = 0,
  /** Bind the given DMA space to the DMA domain. */
  L4VBUS_DMAD_BIND   = 1,
  /** The given DMA space is an L4Re::Dma_space */
  L4VBUS_DMAD_L4RE_DMA_SPACE = 0,
  /** The given DMA space is a kernel DMA space (L4::Task) */
  L4VBUS_DMAD_KERNEL_DMA_SPACE = 2,
};

/**
 * Bind or unbind a kernel DMA space (L4::Task) or a L4Re::Dma_space to a
 * DMA domain.
 * \param vbus       Capability of the system bus
 * \param domain_id  DMA domain ID (resource address of DMA domain found on
 *                   the vBUS). If the value is ~0U the DMA space of the whole
 *                   vBUS is used.
 * \param flags      A combination of #L4vbus_dma_domain_assign_flags.
 * \param dma_space  The DMA space capability to bind or unbind, this must
 *                   either be an L4Re::Dma_space or a kernel DMA space
 *                   (L4::Task created with L4_PROTO_DMA_SPACE) and the type
 *                   must be reflected in the `flags`.
 *
 * \retval 0           Operation completed successfully.
 * \retval -L4_ENOENT  The vbus does not support a global DMA domain or no DMA
 *                     domain could be found.
 * \retval -L4_EINVAL  Invalid argument used.
 * \retval -L4_EBUSY   DMA domain is already active, this means another DMA
 *                     space is already assigned.
 */
int L4_CV
l4vbus_assign_dma_domain(l4_cap_idx_t vbus, unsigned domain_id,
                         unsigned flags, l4_cap_idx_t dma_space);

/**
 * Release a previously requested IO port resource.
 *
 * \param     vbus  Capability of the system bus.
 * \param[in] res   The IO port resource to be released from the bus.
 *
 * \return >=0 on success, <0 on error.
 */
int L4_CV
l4vbus_release_ioport(l4_cap_idx_t vbus, l4vbus_resource_t const *res);

/**
 * Release a previously requested resource.
 *
 * \param     vbus  Capability of the system bus.
 * \param[in] res   The resource to be released from the bus.
 *
 * \return >=0 on success, <0 on error.
 *
 * \deprecated This function is deprecated since Q3 2019.
 *             Use l4vbus_release_ioport() instead.
 */
int L4_CV
L4_DEPRECATED("use l4vbus_release_ioport")
l4vbus_release_resource(l4_cap_idx_t vbus, l4vbus_resource_t const *res);

/**
 * \brief Get capability of ICU.
 *
 * \param  vbus         Capability of the system bus.
 * \param  icu          ICU device handle.
 * \param  cap          Capability slot for the capability.
 *
 * \return 0 on success, else failure
 */
int L4_CV
l4vbus_vicu_get_cap(l4_cap_idx_t vbus, l4vbus_device_handle_t icu,
                    l4_cap_idx_t cap);

__END_DECLS

/** \} */
