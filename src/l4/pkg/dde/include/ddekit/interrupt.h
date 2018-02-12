/*
 * \brief   Hardware-interrupt subsystem
 * \date    2007-01-26
 *
 * DDEKit supports registration of one handler function per interrupt. If any
 * specific DDE implementation needs to register more than one handler,
 * multiplexing has to be implemented there!
 */

/*
 * This file is part of DDEKit.
 *
 * (c) 2007-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *               Thomas Friebel <tf13@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <l4/sys/compiler.h>
#include <l4/dde/ddekit/thread.h>

EXTERN_C_BEGIN

#define DDEKIT_IRQ_PRIO         0x11

#define IRQF_TRIGGER_NONE	0x00000000
#define IRQF_TRIGGER_RISING	0x00000001
#define IRQF_TRIGGER_FALLING	0x00000002
#define IRQF_TRIGGER_HIGH	0x00000004
#define IRQF_TRIGGER_LOW	0x00000008
#define IRQF_TRIGGER_MASK	(IRQF_TRIGGER_HIGH | IRQF_TRIGGER_LOW | \
				 IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)
#define IRQF_TRIGGER_PROBE	0x00000010

/**
 * Attach to hardware interrupt
 *
 * \param irq          IRQ number to attach to
 * \param shared       set to 1 if interrupt sharing is supported; set to 0
 *                     otherwise
 * \param thread_init  called just after DDEKit internal init and before any
 *                     other function
 * \param handler      IRQ handler for interrupt irq
 * \param priv         private token (argument for thread_init and handler)
 *
 * \return pointer to interrupt thread created
 */
ddekit_thread_t *ddekit_interrupt_attach(int irq, int shared,
                                         void(*thread_init)(void *),
                                         void(*handler)(void *), void *priv);

/**
 * Detach from a previously attached interrupt.
 *
 * \param irq          IRQ number
 */
void ddekit_interrupt_detach(int irq);

/**
 * Block interrupt.
 *
 * \param irq          IRQ number to block
 */
void ddekit_interrupt_disable(int irq);

/**
 * Enable interrupt.
 *
 * \param irq          IRQ number to block
 */
void ddekit_interrupt_enable(int irq);

/**
 * Set the trigger mode flags
 *
 * \param irq		IRQ number
 * \param flags		
 */
int ddekit_irq_set_type(int irq, unsigned flags);

/**
 * Initialize internal data structures
 */
void ddekit_init_irqs(void);

EXTERN_C_END
