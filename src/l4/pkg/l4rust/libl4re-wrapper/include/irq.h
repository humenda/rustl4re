#pragma once

#include <l4/sys/icu.h>

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

EXTERN l4_msgtag_t l4_irq_mux_chain_w (l4_cap_idx_t irq, l4_cap_idx_t slave) L4_NOTHROW;
EXTERN l4_msgtag_t l4_irq_mux_chain_u_w (l4_cap_idx_t irq, l4_cap_idx_t slave, l4_utcb_t *utcb) L4_NOTHROW;
EXTERN l4_msgtag_t l4_irq_detach_w (l4_cap_idx_t irq) L4_NOTHROW;
EXTERN l4_msgtag_t l4_irq_detach_u_w (l4_cap_idx_t irq, l4_utcb_t *utcb) L4_NOTHROW;
EXTERN l4_msgtag_t l4_irq_trigger_w (l4_cap_idx_t irq) L4_NOTHROW;
EXTERN l4_msgtag_t l4_irq_trigger_u_w (l4_cap_idx_t irq, l4_utcb_t *utcb) L4_NOTHROW;
EXTERN l4_msgtag_t l4_irq_receive_w (l4_cap_idx_t irq, l4_timeout_t to) L4_NOTHROW;
EXTERN l4_msgtag_t l4_irq_receive_u_w (l4_cap_idx_t irq, l4_timeout_t timeout, l4_utcb_t *utcb) L4_NOTHROW;
EXTERN l4_msgtag_t l4_irq_wait_w (l4_cap_idx_t irq, l4_umword_t *label, l4_timeout_t to) L4_NOTHROW;
EXTERN l4_msgtag_t l4_irq_wait_u_w (l4_cap_idx_t irq, l4_umword_t *label, l4_timeout_t timeout, l4_utcb_t *utcb) L4_NOTHROW;
EXTERN l4_msgtag_t l4_irq_unmask_w (l4_cap_idx_t irq) L4_NOTHROW;
EXTERN l4_msgtag_t l4_irq_unmask_u_w (l4_cap_idx_t irq, l4_utcb_t *utcb) L4_NOTHROW;
