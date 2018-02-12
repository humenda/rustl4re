/**
 * \file
 */
/*
 * (c) 2008-2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */


#pragma once

#include <l4/sys/types.h>
#include <l4/sys/linkage.h>
#include <l4/io/types.h>

EXTERN_C_BEGIN

/**
 * \defgroup api_l4io IO interface
 */

/**
 * \internal
 * \brief Request an interrupt.
 * \ingroup api_l4io
 *
 * \param irqnum IRQ number.
 * \param irqcap Capability index to attach the IRQ to.
 * \return 0 on success, <0 on error
 *
 * \note This function is internal because libirq is handling IRQs. This
 *       is rather the interface to get the IRQ.
 */
L4_CV long L4_EXPORT
l4io_request_irq(int irqnum, l4_cap_idx_t irqcap);

/**
 * \brief Request the ICU object of the client.
 *
 * \return Client ICU object, an invalid capability selector is returned if
 *         no ICU is available.
 */
L4_CV l4_cap_idx_t L4_EXPORT
l4io_request_icu(void);

/**
 * \internal
 * \brief Release an interrupt.
 * \ingroup api_l4io
 *
 * \param irqnum IRQ number.
 * \return 0 on success, <0 on error
 *
 * \note This function is internal because libirq is handling IRQs. This
 *       is rather the interface to return the IRQ.
 */
L4_CV long L4_EXPORT
l4io_release_irq(int irqnum, l4_cap_idx_t irq_cap);

/**
 * \brief Request an IO memory region.
 * \ingroup api_l4io
 * \param          phys   Physical address of the I/O memory region
 * \param          size   Size of the region in Bytes, granularity pages.
 * \param          flags  See #l4io_iomem_flags_t
 * \param[in, out] virt   Virtual address where the IO memory region should be
 *                        mapped to. If the caller passes '0' a region in the
 *                        caller's address space is searched and the virtual
 *                        address is returned.
 *
 * \retval 0                  Success.
 * \retval -L4_ENOENT         No area in the caller's address space could be
 *                            found to map the IO memory region.
 * \retval -L4_EPERM          Operation not allowed.
 * \retval -L4_EINVAL         Invalid value.
 * \retval -L4_EADDRNOTAVAIL  The requested virtual address is not available.
 * \retval -L4_ENOMEM         The requested IO memory region could not be
 *                            allocated.
 * \retval <0                 IPC errors.
 *
 * \note This function uses L4Re functionality to reserve a part of the
 *       virtual address space of the caller.
 */
L4_CV long L4_EXPORT
l4io_request_iomem(l4_addr_t phys, unsigned long size, int flags,
                   l4_addr_t *virt);

/**
 * \brief Request an IO memory region and map it to a specified region.
 * \ingroup api_l4io
 * \param phys   Physical address of the I/O memory region
 * \param virt   Virtual address.
 * \param size   Size of the region in Bytes, granularity pages.
 * \param flags  See #l4io_iomem_flags_t
 *
 * \retval 0                  Success.
 * \retval -L4_ENOENT         No area could be found to map the IO memory
 *                            region.
 * \retval -L4_EPERM          Operation not allowed.
 * \retval -L4_EINVAL         Invalid value.
 * \retval -L4_EADDRNOTAVAIL  The requested virtual address is not available.
 * \retval -L4_ENOMEM         The requested IO memory region could not be
 *                            allocated.
 * \retval <0                 IPC errors.
 *
 * \note This function uses L4Re functionality to reserve a part of the
 *       virtual address space of the caller.
 */
L4_CV long L4_EXPORT
l4io_request_iomem_region(l4_addr_t phys, l4_addr_t virt,
                          unsigned long size, int flags);

/**
 * \brief Release an IO memory region.
 * \ingroup api_l4io
 * \param virt  Virtual address of region to free, see #l4io_request_iomem
 * \param size  Size of the region to release.
 * \return 0 on success, <0 on error
 */
L4_CV long L4_EXPORT
l4io_release_iomem(l4_addr_t virt, unsigned long size);

/**
 * \brief Search for a IO memory region.
 * \ingroup api_l4io
 * \param   phys   Physical address to look for
 * \param   size   Size of requested memory area
 * \retval  rstart Start address for region
 * \retval  rsize  Size of region in bytes
 * \return 0 if an IO region was found, <0 if not
 */
L4_CV long L4_EXPORT
l4io_search_iomem_region(l4_addr_t phys, l4_addr_t size,
                         l4_addr_t *rstart, l4_addr_t *rsize);

/**
 * \brief Request an IO port region.
 * \ingroup api_l4io
 * \param portnum  Start of port range to request
 * \param len      Length of range to request
 * \return 0 on success, <0 on error
 *
 * \note X86 architecture only
 */
L4_CV long L4_EXPORT
l4io_request_ioport(unsigned portnum, unsigned len);

/**
 * \brief Release an IO port region.
 * \ingroup api_l4io
 * \param portnum  Start of port range to release
 * \param len      Length of range to request
 * \return 0 on success, <0 on error
 *
 * \note X86 architecture only
 */
L4_CV long L4_EXPORT
l4io_release_ioport(unsigned portnum, unsigned len);


/* -------------- Device handling --------------------- */

/**
 * \brief Get root device handle of the device bus.
 *
 * \return root device handle
 */
L4_INLINE
l4io_device_handle_t l4io_get_root_device(void);

/**
 * \brief Iterate over the device bus.
 *
 * \param  devhandle   Device handle to start iterating at
 * \retval devhandle   Next device handle
 * \retval dev         Device information, may be NULL
 * \retval reshandle   Resource handle.
 * \return 0 on success, error code otherwise
 */
L4_CV int L4_EXPORT
l4io_iterate_devices(l4io_device_handle_t *devhandle,
                     l4io_device_t *dev, l4io_resource_handle_t *reshandle);

/**
 * \brief Find a device by name.
 * \ingroup api_l4io
 *
 * \param  devname    Name of device
 * \retval dev_handle Device handle for found device, can be NULL.
 * \retval dev        Device information, filled by the function, can be NULL.
 * \retval res_handle Resource handle, can be NULL.
 *
 * \return 0 on success, error code otherwise
 */
L4_CV int L4_EXPORT
l4io_lookup_device(const char *devname,
                   l4io_device_handle_t *dev_handle,
                   l4io_device_t *dev, l4io_resource_handle_t *res_handle);

/**
 * \brief Request a specific resource from a device description.
 * \ingroup api_l4io
 *
 * \param  devhandle Device handle.
 * \param  type      Type of resource to request (see \#l4io_resource_types_t)
 * \param  reshandle Resource handle,
 *                   start with handle returned by device functions.
 * \retval reshandle Next resource handle.
 * \retval res       Device descriptor
 *
 * \return 0 on success, error code otherwise, esp. -L4_ENOENT if no more
 *         resources found
 */
L4_CV int L4_EXPORT
l4io_lookup_resource(l4io_device_handle_t devhandle,
                     enum l4io_resource_types_t type,
                     l4io_resource_handle_t *reshandle,
                     l4io_resource_t *res);


/* --------------- Convenience functions ----------------- */

/**
 * \brief Request IO memory.
 * \ingroup api_l4io
 *
 * \param         devhandle  Device handle.
 * \param[in,out] reshandle  Resource handle from which IO memory should be
 *                           requested. Upon successfull completion 'reshandle'
 *                           points to the device's next resource.
 *
 * \retval 0   An error occured. The value of 'reshandle' is undefined.
 * \retval >0  The virtual address of the IO memory mapping.
 */
L4_CV l4_addr_t L4_EXPORT
l4io_request_resource_iomem(l4io_device_handle_t devhandle,
                            l4io_resource_handle_t *reshandle);

/**
 * \brief Request all available IO-port resources.
 * \param res_cb Callback function called for every port resource found,
 *               give NULL for no callback.
 *
 */
L4_CV void L4_EXPORT
l4io_request_all_ioports(void (*res_cb)(l4vbus_resource_t const *res));

/**
 * \brief Check if a resource is available.
 * \ingroup api_l4io
 *
 * \param type  Type of resource
 * \param start Minimal value.
 * \param end   Maximum value.
 */
L4_CV int L4_EXPORT
l4io_has_resource(enum l4io_resource_types_t type,
                  l4vbus_paddr_t start, l4vbus_paddr_t end);

/* ------------------------------------------------------- */
/* Implementations */

L4_INLINE
l4io_device_handle_t l4io_get_root_device(void)
{ return 0; }

EXTERN_C_END
