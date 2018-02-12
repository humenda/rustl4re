/*
 * \brief   Hardware-interrupt subsystem
 * \author  Thomas Friebel <tf13@os.inf.tu-dresden.de>
 * \author  Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \date    2007-01-26
 *
 * DDEKit supports registration of one handler function per interrupt. If any
 * specific DDE implementation needs to register more than one handler,
 * multiplexing has to be implemented there!
 */

/*
 * (c) 2007-2008 Technische Universität Dresden
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING file for details.
 */


#pragma once

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

#include <l4/dde/ddekit/thread.h>

#define DDEKIT_IRQ_PRIO         0x11

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

EXTERN_C_END
