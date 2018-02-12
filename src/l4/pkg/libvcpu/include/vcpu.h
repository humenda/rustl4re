/*
 * (c) 2010 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 *
 * As a special exception, you may use this file as part of a free software
 * library without restriction.  Specifically, if other files instantiate
 * templates or use macros or inline functions from this file, or you compile
 * this file and link it with other files to produce an executable, this
 * file does not by itself cause the resulting executable to be covered by
 * the GNU General Public License.  This exception does not however
 * invalidate any other reasons why the executable file might be covered by
 * the GNU General Public License.
 */
#pragma once

#include <l4/sys/vcpu.h>
#include <l4/sys/utcb.h>

__BEGIN_DECLS

/**
 * \defgroup api_libvcpu vCPU Support Library
 * \brief vCPU handling functionality.
 *
 * This library provides convenience functionality on top of the l4sys vCPU
 * interface to ease programming. It wraps commonly used code and abstracts
 * architecture depends parts as far as reasonable.
 */

/**
 * \defgroup api_libvcpu_ext Extended vCPU support
 * \ingroup api_libvcpu
 * \brief extended vCPU handling functionality.
 */

typedef void (*l4vcpu_event_hndl_t)(l4_vcpu_state_t *vcpu);
typedef void (*l4vcpu_setup_ipc_t)(l4_utcb_t *utcb);

/**
 * \brief Disable a vCPU for event delivery.
 * \ingroup api_libvcpu
 *
 * \param vcpu  Pointer to vCPU area.
 */
L4_CV L4_INLINE
void
l4vcpu_irq_disable(l4_vcpu_state_t *vcpu) L4_NOTHROW;

/**
 * \brief Disable a vCPU for event delivery and return previous state.
 * \ingroup api_libvcpu
 *
 * \param vcpu  Pointer to vCPU area.
 *
 * \return IRQ state before disabling IRQs.
 */
L4_CV L4_INLINE
unsigned
l4vcpu_irq_disable_save(l4_vcpu_state_t *vcpu) L4_NOTHROW;

/**
 * \brief Enable a vCPU for event delivery.
 * \ingroup api_libvcpu
 *
 * \param vcpu             Pointer to vCPU area.
 * \param utcb             Utcb pointer of the calling vCPU.
 * \param do_event_work_cb Call-back function that is called in case an
 *                         event (such as an interrupt) is pending.
 * \param setup_ipc        Function call-back that is called right before
 *                         any IPC operation, and before event delivery is
 *                         enabled.
 */
L4_CV L4_INLINE
void
l4vcpu_irq_enable(l4_vcpu_state_t *vcpu, l4_utcb_t *utcb,
                  l4vcpu_event_hndl_t do_event_work_cb,
                  l4vcpu_setup_ipc_t setup_ipc) L4_NOTHROW;

/**
 * \brief Restore a previously saved IRQ/event state.
 * \ingroup api_libvcpu
 *
 * \param vcpu             Pointer to vCPU area.
 * \param s                IRQ state to be restored.
 * \param utcb             Utcb pointer of the calling vCPU.
 * \param do_event_work_cb Call-back function that is called in case an
 *                         event (such as an interrupt) is pending after
 *                         enabling.
 * \param setup_ipc        Function call-back that is called right before
 *                         any IPC operation, and before event delivery is
 *                         enabled.
 */
L4_CV L4_INLINE
void
l4vcpu_irq_restore(l4_vcpu_state_t *vcpu, unsigned s,
                   l4_utcb_t *utcb,
                   l4vcpu_event_hndl_t do_event_work_cb,
                   l4vcpu_setup_ipc_t setup_ipc) L4_NOTHROW;

/**
 * \internal
 * \ingroup api_libvcpu
 *
 * \param vcpu             Pointer to vCPU area.
 * \param utcb             Utcb pointer of the calling vCPU.
 * \param to               Timeout to do IPC operation with.
 * \param do_event_work_cb Call-back function that is called in case an
 *                         event (such as an interrupt) is pending after
 *                         enabling.
 * \param setup_ipc        Function call-back that is called right before
 *                         any IPC operation.
 */
L4_CV L4_INLINE
void
l4vcpu_wait(l4_vcpu_state_t *vcpu, l4_utcb_t *utcb,
            l4_timeout_t to,
            l4vcpu_event_hndl_t do_event_work_cb,
            l4vcpu_setup_ipc_t setup_ipc) L4_NOTHROW;

/**
 * \brief Wait for event.
 * \ingroup api_libvcpu
 *
 * \param vcpu             Pointer to vCPU area.
 * \param utcb             Utcb pointer of the calling vCPU.
 * \param do_event_work_cb Call-back function that is called when the vCPU
 *                         awakes and needs to handle an event/IRQ.
 * \param setup_ipc        Function call-back that is called right before
 *                         any IPC operation.
 *
 * Note that event delivery remains disabled after this function returns.
 */
L4_CV L4_INLINE
void
l4vcpu_wait_for_event(l4_vcpu_state_t *vcpu, l4_utcb_t *utcb,
                      l4vcpu_event_hndl_t do_event_work_cb,
                      l4vcpu_setup_ipc_t setup_ipc) L4_NOTHROW;


/**
 * \brief Print the state of a vCPU.
 * \ingroup api_libvcpu
 *
 * \param vcpu   Pointer to vCPU area.
 * \param prefix A prefix for each line printed.
 */
L4_CV void
l4vcpu_print_state(const l4_vcpu_state_t *vcpu, const char *prefix) L4_NOTHROW;

/**
 * \internal
 */
L4_CV void
l4vcpu_print_state_arch(const l4_vcpu_state_t *vcpu, const char *prefix) L4_NOTHROW;


/**
 * \brief Return whether the entry reason was an IRQ/IPC message.
 * \ingroup api_libvcpu
 *
 * \param vcpu Pointer to vCPU area.
 *
 * return 0 if not, !=0 otherwise.
 */
L4_CV L4_INLINE
int
l4vcpu_is_irq_entry(l4_vcpu_state_t const *vcpu) L4_NOTHROW;

/**
 * \brief Return whether the entry reason was a page fault.
 * \ingroup api_libvcpu
 *
 * \param vcpu Pointer to vCPU area.
 *
 * return 0 if not, !=0 otherwise.
 */
L4_CV L4_INLINE
int
l4vcpu_is_page_fault_entry(l4_vcpu_state_t const *vcpu) L4_NOTHROW;

/**
 * \brief Allocate state area for an extended vCPU.
 * \ingroup api_libvcpu_ext
 *
 * \retval vcpu      Allocated vcpu-state area.
 * \retval ext_state Allocated extended vcpu-state area.
 * \param  task      Task to use for allocation.
 * \param  regmgr    Region manager to use for allocation.
 *
 * \return 0 for success, error code otherwise
 */
L4_CV int
l4vcpu_ext_alloc(l4_vcpu_state_t **vcpu, l4_addr_t *ext_state,
                 l4_cap_idx_t task, l4_cap_idx_t regmgr) L4_NOTHROW;

/* ===================================================================== */
/* Implementations */

#include <l4/sys/ipc.h>
#include <l4/vcpu/vcpu_arch.h>

L4_CV L4_INLINE
void
l4vcpu_irq_disable(l4_vcpu_state_t *vcpu) L4_NOTHROW
{
  vcpu->state &= ~L4_VCPU_F_IRQ;
  l4_barrier();
}

L4_CV L4_INLINE
unsigned
l4vcpu_irq_disable_save(l4_vcpu_state_t *vcpu) L4_NOTHROW
{
  unsigned s = vcpu->state;
  l4vcpu_irq_disable(vcpu);
  return s;
}

L4_CV L4_INLINE
void
l4vcpu_wait(l4_vcpu_state_t *vcpu, l4_utcb_t *utcb,
            l4_timeout_t to,
            l4vcpu_event_hndl_t do_event_work_cb,
            l4vcpu_setup_ipc_t setup_ipc) L4_NOTHROW
{
  l4vcpu_irq_disable(vcpu);
  setup_ipc(utcb);
  vcpu->i.tag = l4_ipc_wait(utcb, &vcpu->i.label, to);
  if (L4_LIKELY(!l4_msgtag_has_error(vcpu->i.tag)))
    do_event_work_cb(vcpu);
}

L4_CV L4_INLINE
void
l4vcpu_irq_enable(l4_vcpu_state_t *vcpu, l4_utcb_t *utcb,
                  l4vcpu_event_hndl_t do_event_work_cb,
                  l4vcpu_setup_ipc_t setup_ipc) L4_NOTHROW
{
  if (!(vcpu->state & L4_VCPU_F_IRQ))
    {
      setup_ipc(utcb);
      l4_barrier();
    }

  while (1)
    {
      vcpu->state |= L4_VCPU_F_IRQ;
      l4_barrier();

      if (L4_LIKELY(!(vcpu->sticky_flags & L4_VCPU_SF_IRQ_PENDING)))
        break;

      l4vcpu_wait(vcpu, utcb, L4_IPC_BOTH_TIMEOUT_0,
                  do_event_work_cb, setup_ipc);
    }
}

L4_CV L4_INLINE
void
l4vcpu_irq_restore(l4_vcpu_state_t *vcpu, unsigned s,
                   l4_utcb_t *utcb,
                   l4vcpu_event_hndl_t do_event_work_cb,
                   l4vcpu_setup_ipc_t setup_ipc) L4_NOTHROW
{
  if (s & L4_VCPU_F_IRQ)
    l4vcpu_irq_enable(vcpu, utcb, do_event_work_cb, setup_ipc);
  else if (vcpu->state & L4_VCPU_F_IRQ)
    l4vcpu_irq_disable(vcpu);
}

L4_CV L4_INLINE
void
l4vcpu_wait_for_event(l4_vcpu_state_t *vcpu, l4_utcb_t *utcb,
                      l4vcpu_event_hndl_t do_event_work_cb,
                      l4vcpu_setup_ipc_t setup_ipc) L4_NOTHROW
{
  l4vcpu_wait(vcpu, utcb, L4_IPC_NEVER, do_event_work_cb, setup_ipc);
}

__END_DECLS
