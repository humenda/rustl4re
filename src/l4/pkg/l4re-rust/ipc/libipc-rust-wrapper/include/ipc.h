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
