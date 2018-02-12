/**
 * \file
 * \brief IRQ handling routines.
 */
/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Henning Schild <hschild@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/compiler.h>
#include <l4/sys/types.h>

__BEGIN_DECLS

/**
 * \defgroup l4irq_api IRQ handling library
 *
 * \defgroup l4irq_api_irq Interface using direct functionality.
 * \ingroup l4irq_api
 */

struct l4irq_t;
typedef struct l4irq_t l4irq_t;

/**
 * \brief Attach/connect to IRQ.
 * \ingroup l4irq_api_irq
 *
 * \param  irqnum          IRQ number to request
 * \return Pointer to l4irq_t structure, 0 on error
 *
 * This l4irq_attach has to be called in the same thread as
 * l4irq_wait and caller has to be a pthread thread.
 */
L4_CV l4irq_t *
l4irq_attach(int irqnum);

/**
 * \brief Attach/connect to IRQ using given type.
 * \ingroup l4irq_api_irq
 *
 * \param  irqnum          IRQ number to request
 * \param  mode            Interrupt type, \see L4_irq_mode
 * \return Pointer to l4irq_t structure, 0 on error
 *
 * This l4irq_attach has to be called in the same thread as
 * l4irq_wait and caller has to be a pthread thread.
 */
L4_CV l4irq_t *
l4irq_attach_ft(int irqnum, unsigned mode);

/**
 * \brief Attach/connect to IRQ.
 * \ingroup l4irq_api_irq
 *
 * \param  irqnum          IRQ number to request
 * \param  to_thread       Attach IRQ to this specified thread.
 * \return Pointer to l4irq_t structure, 0 on error
 *
 * The pointer to the IRQ structure is used as a label in the IRQ object.
 */
L4_CV l4irq_t *
l4irq_attach_thread(int irqnum, l4_cap_idx_t to_thread);

/**
 * \brief Attach/connect to IRQ using given type.
 * \ingroup l4irq_api_irq
 *
 * \param  irqnum          IRQ number to request
 * \param  to_thread       Attach IRQ to this specified thread.
 * \param  mode            Interrupt type, \see L4_irq_mode
 * \return Pointer to l4irq_t structure, 0 on error
 *
 * The pointer to the IRQ structure is used as a label in the IRQ object.
 */
L4_CV l4irq_t *
l4irq_attach_thread_ft(int irqnum, l4_cap_idx_t to_thread,
                       unsigned mode);

/**
 * \brief Wait for specified IRQ.
 * \ingroup l4irq_api_irq
 *
 * \param irq  IRQ data structure
 * \return 0 on success, != 0 on error
 */
L4_CV long
l4irq_wait(l4irq_t *irq);

/**
 * \brief Unmask a specific IRQ and wait for any attached IRQ.
 * \ingroup l4irq_api_irq
 *
 * \param  unmask_irq  IRQ data structure for unmask.
 * \retval ret_irq     Received interrupt.
 * \return 0 on success, != 0 on error
 */
L4_CV long
l4irq_unmask_and_wait_any(l4irq_t *unmask_irq, l4irq_t **ret_irq);

/**
 * \brief Wait for any attached IRQ.
 * \ingroup l4irq_api_irq
 *
 * \retval irq     Received interrupt.
 * \return 0 on success, != 0 on error
 */
L4_CV long
l4irq_wait_any(l4irq_t **irq);

/**
 * \brief Unmask a specific IRQ.
 * \ingroup l4irq_api_irq
 *
 * \param irq  IRQ data structure
 * \return 0 on success, != 0 on error
 *
 * This function is useful if a thread wants to wait for multiple IRQs using
 * l4_ipc_wait.
 */
L4_CV long
l4irq_unmask(l4irq_t *irq);

/**
 * \brief Detach from IRQ.
 * \ingroup l4irq_api_irq
 *
 * \param irq  IRQ data structure
 * \return 0 on success, != 0 on error
 */
L4_CV long
l4irq_detach(l4irq_t *irq);



/***********************************************************************/
/**
 * \defgroup l4irq_api_async Interface for asynchronous ISR handlers.
 * \ingroup l4irq_api
 *
 * This interface has just two (main) functions. l4irq_request to install a
 * handler for an interrupt and l4irq_release to uninstall the handler again
 * and release all resources associated with it.
 */

/**
 * \brief Attach asychronous ISR handler to IRQ.
 * \ingroup l4irq_api_async
 *
 * \param irqnum          IRQ number to request
 * \param isr_handler     Handler routine that is called when an interrupt triggers
 * \param isr_data        Pointer given as argument to isr_handler
 * \param irq_thread_prio L4 thread priority of the ISR handler. Give -1 for same priority as creator.
 * \param mode            Interrupt type, \see L4_irq_mode
 *
 * \return Pointer to l4irq_t structure, 0 on error
 */
L4_CV l4irq_t *
l4irq_request(int irqnum, void (*isr_handler)(void *), void *isr_data,
              int irq_thread_prio, unsigned mode);

/**
 * \brief Release asynchronous ISR handler and free resources.
 * \ingroup l4irq_api_async
 *
 * \param irq  IRQ data structure
 * \return 0 sucess, != 0 failure
 */
L4_CV long
l4irq_release(l4irq_t *irq);



/***********************************************************************/
/***********************************************************************/

/**
 * \defgroup l4irq_api_irq_cap Interface using direct functionality.
 * \ingroup l4irq_api_irq
 */

/**
 * \brief Attach/connect to IRQ.
 * \ingroup l4irq_api_irq_cap
 *
 * \param  irqcap          IRQ capability
 * \return Pointer to l4irq_t structure, 0 on error
 *
 * This l4irq_attach has to be called in the same thread as
 * l4irq_wait and caller has to be a pthread thread.
 */
L4_CV l4irq_t *
l4irq_attach_cap(l4_cap_idx_t irqcap);

/**
 * \brief Attach/connect to IRQ using given type.
 * \ingroup l4irq_api_irq_cap
 *
 * \param  irqcap          IRQ capability
 * \param  mode            Interrupt type, \see L4_irq_mode
 * \return Pointer to l4irq_t structure, 0 on error
 *
 * This l4irq_attach has to be called in the same thread as
 * l4irq_wait and caller has to be a pthread thread.
 */
L4_CV l4irq_t *
l4irq_attach_cap_ft(l4_cap_idx_t irqcap, unsigned mode);

/**
 * \brief Attach/connect to IRQ.
 * \ingroup l4irq_api_irq_cap
 *
 * \param  irqcap          IRQ capability
 * \param  to_thread       Attach IRQ to this thread.
 * \return Pointer to l4irq_t structure, 0 on error
 *
 * The pointer to the IRQ structure is used as a label in the IRQ object.
 */
L4_CV l4irq_t *
l4irq_attach_thread_cap(l4_cap_idx_t irqcap, l4_cap_idx_t to_thread);

/**
 * \brief Attach/connect to IRQ using given type.
 * \ingroup l4irq_api_irq_cap
 *
 * \param  irqcap          IRQ capability
 * \param  to_thread       Attach IRQ to this thread.
 * \param  mode            Interrupt type, \see L4_irq_mode
 * \return Pointer to l4irq_t structure, 0 on error
 *
 * The pointer to the IRQ structure is used as a label in the IRQ object.
 */
L4_CV l4irq_t *
l4irq_attach_thread_cap_ft(l4_cap_idx_t irqcap, l4_cap_idx_t to_thread,
                           unsigned mode);

/***********************************************************************/
/**
 * \defgroup l4irq_api_async_cap Interface for asynchronous ISR handlers with a given IRQ capability.
 * \ingroup l4irq_api_async
 *
 * This group is just an enhanced version to l4irq_request() which takes a
 * capability object instead of a plain number.
 */

/**
 * \brief Attach asychronous ISR handler to IRQ.
 * \ingroup l4irq_api_async_cap
 *
 * \param irqcap          IRQ capability
 * \param isr_handler     Handler routine that is called when an interrupt triggers
 * \param isr_data        Pointer given as argument to isr_handler
 * \param irq_thread_prio L4 thread priority of the ISR handler. Give -1 for same priority as creator.
 * \param mode            Interrupt type, \see L4_irq_mode
 *
 * \return Pointer to l4irq_t structure, 0 on error
 */
L4_CV l4irq_t *
l4irq_request_cap(l4_cap_idx_t irqcap,
                  void (*isr_handler)(void *), void *isr_data,
                  int irq_thread_prio, unsigned mode);

__END_DECLS
