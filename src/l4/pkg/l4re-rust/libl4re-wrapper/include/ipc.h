// documentation: see original include files :)
#pragma once
#include <pthread-l4.h>
#include <l4/sys/ipc.h>

#include <pthread-l4.h>

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

EXTERN l4_msg_regs_t *l4_utcb_mr_w(void);

EXTERN l4_msgtag_t l4_ipc_call_w(l4_cap_idx_t object,
        l4_utcb_t *utcb, l4_msgtag_t tag,
        l4_timeout_t timeout);

EXTERN l4_umword_t l4_ipc_error_w(l4_msgtag_t tag, l4_utcb_t *utcb);

EXTERN l4_msgtag_t l4_ipc_wait_w(l4_utcb_t *utcb, l4_umword_t *label,
        l4_timeout_t timeout);

EXTERN l4_msgtag_t l4_ipc_wait_w(l4_utcb_t *utcb, l4_umword_t *label,
        l4_timeout_t timeout);


EXTERN l4_utcb_t *l4_utcb_w(void);

EXTERN l4_msgtag_t l4_msgtag_w(long label, unsigned words, unsigned items,
        unsigned flags);

EXTERN l4_msgtag_t l4_ipc_reply_and_wait_w(l4_utcb_t *utcb, l4_msgtag_t tag,
        l4_umword_t *src, l4_timeout_t timeout);

EXTERN l4_fpage_t l4_obj_fpage_w(l4_cap_idx_t obj, unsigned int order, unsigned
        char rights);

/**
 * Wait for a message from a specific source.
 * \ingroup l4_ipc_api
 *
 * \param object   Object to receive a message from.
 * \param timeout  Timeout pair (see #l4_timeout_t, only the receive part
 *                  matters).
 * \param utcb     UTCB of the caller.
 *
 * \return  result tag.
 *
 * This operation waits for a message from the specified object. Messages from
 * other sources are not accepted by this operation. The operation does not
 * include a send phase, this means no message is sent to the object.
 *
 * \note This operation is usually used to receive messages from a specific IRQ
 *       or thread. However, it is not common to use this operation for normal
 *       applications.
 */
EXTERN l4_msgtag_t l4_ipc_receive_w(l4_cap_idx_t object, l4_utcb_t *utcb,
        l4_timeout_t timeout);


