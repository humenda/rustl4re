/*
 * (c) 2011 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/compiler.h>
#include <l4/sys/types.h>
#include <l4/vbus/vbus_types.h>

/**
 * \ingroup l4vbus_module
 * \defgroup l4vbus_gpio_module L4vbus GPIO functions
 * \{
 */

__BEGIN_DECLS

/**
 * \brief Constants for generic GPIO functions
 */
enum L4vbus_gpio_generic_func
{
  L4VBUS_GPIO_SETUP_INPUT  = 0x100, ///< Set GPIO pin to input
  L4VBUS_GPIO_SETUP_OUTPUT = 0x200, ///< Set GPIO pin to output
  L4VBUS_GPIO_SETUP_IRQ    = 0x300, ///< Set GPIO pin to IRQ
};

/**
 * \brief Constants for generic GPIO pull up/down resistor configuration
 */
enum L4vbus_gpio_pull_modes
{
  L4VBUS_GPIO_PIN_PULL_NONE = 0x100,  ///< No pull up or pull down resistors
  L4VBUS_GPIO_PIN_PULL_UP   = 0x200,  ///< enable pull up resistor
  L4VBUS_GPIO_PIN_PULL_DOWN = 0x300,  ///< enable pull down resistor
};

/**
 * \copybrief L4vbus::Gpio_pin::setup()
 * \param vbus      V-BUS capability
 * \param handle    Device handle for the GPIO chip
 * \param pin       GPIO pin number
 * \copydetails L4vbus::Gpio_pin::setup()
 */
int L4_CV
l4vbus_gpio_setup(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                  unsigned pin, unsigned mode, int value);

/**
 * \copybrief L4vbus::Gpio_pin::config_pull()
 * \param vbus    V-BUS capability
 * \param handle  Device handle for the GPIO chip
 * \param pin     GPIO pin number
 * \copydetails L4vbus::Gpio_pin::config_pull()
 */
int L4_CV
l4vbus_gpio_config_pull(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                        unsigned pin, unsigned mode);

/**
 * \copybrief L4vbus::Gpio_pin::config_pad()
 * \param vbus    V-BUS capability
 * \param handle  Device handle for the GPIO chip
 * \param pin     GPIO pin number
 * \copydetails L4vbus::Gpio_pin::config_pad()
 */
int L4_CV
l4vbus_gpio_config_pad(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                       unsigned pin, unsigned func, unsigned value);

/**
 * \copybrief L4vbus::Gpio_pin::config_get()
 * \param vbus    V-BUS capability
 * \param handle  Device handle for the GPIO chip
 * \param pin     GPIO pin number
 * \copydetails L4vbus::Gpio_pin::config_get()
 */
int L4_CV
l4vbus_gpio_config_get(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                       unsigned pin, unsigned func, unsigned *value);

/**
 * \copybrief L4vbus::Gpio_pin::get()
 * \param vbus    V-BUS capability
 * \param handle  Device handle for the GPIO chip
 * \param pin     GPIO pin number to read from
 * \copydetails L4vbus::Gpio_pin::get()
 */
int L4_CV
l4vbus_gpio_get(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                unsigned pin);

/**
 * \copybrief L4vbus::Gpio_pin::set()
 * \param vbus    V-BUS capability
 * \param handle  Device handle for the GPIO chip
 * \param pin     GPIO pin number to write to
 * \copydetails L4vbus::Gpio_pin::set()
 */
int L4_CV
l4vbus_gpio_set(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                unsigned pin, int value);

/**
 * \copybrief L4vbus::Gpio_module::setup()
 * \param vbus    V-BUS capability
 * \param handle  Device handle for the GPIO chip
 * \param offset  Pin corresponding to the LSB in \a mask.
 *                Note: allowed may be hardware specific.
 * \param mask    Mask of GPIO pins to configure. A bit set to 1 configures
 *                this pin. A maximum of 32 pins can be configured at once.
 *                The real number depends on the hardware and the driver
 *                implementation.
 * \copydetails L4vbus::Gpio_module::setup()
 */
int L4_CV
l4vbus_gpio_multi_setup(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                        unsigned offset, unsigned mask,
                        unsigned mode, unsigned value);

/**
 * \copybrief L4vbus::Gpio_module::config_pad()
 * \param vbus    V-BUS capability
 * \param handle  Device handle for the GPIO chip
 * \param offset  Pin corresponding to the LSB in \a mask.
 *                Note: allowed may be hardware specific.
 * \param mask    Mask of GPIO pins to configure. A bit set to 1 configures
 *                this pin. A maximum of 32 pins can be configured at once.
 *                The real number depends on the hardware and the driver
 *                implementation.
 * \copydetails L4vbus::Gpio_module::config_pad()
 */
int L4_CV
l4vbus_gpio_multi_config_pad(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                              unsigned offset, unsigned mask,
                              unsigned func, unsigned value);

/**
 * \copybrief L4vbus::Gpio_module::get()
 * \param vbus    V-BUS capability
 * \param handle  Device handle for the GPIO chip
 * \copydetails L4vbus::Gpio_module::get()
 */
int L4_CV
l4vbus_gpio_multi_get(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                      unsigned offset, unsigned *data);

/**
 * \copybrief L4vbus::Gpio_module::set()
 * \param vbus    V-BUS capability
 * \param handle  Device handle for the GPIO chip
 * \param offset  Pin corresponding to the LSB in \a data.
 *                Note: allowed may be hardware specific.
 * \copydetails L4vbus::Gpio_module::set()
 */
int L4_CV
l4vbus_gpio_multi_set(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                      unsigned offset, unsigned mask, unsigned data);

/**
 * \copybrief L4vbus::Gpio_pin::to_irq()
 * \param vbus    V-BUS capability
 * \param handle  Device handle for the GPIO chip
 * \param pin     GPIO pin to create an IRQ for.
 * \copydetails L4vbus::Gpio_pin::to_irq()
 */
int L4_CV
l4vbus_gpio_to_irq(l4_cap_idx_t vbus, l4vbus_device_handle_t handle,
                   unsigned pin);

/**\}*/

__END_DECLS
