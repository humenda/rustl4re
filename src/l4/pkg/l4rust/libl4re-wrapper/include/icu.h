#pragma once

#include <l4/sys/icu.h>

#ifdef __cplusplus
#define EXTERN extern "C"
#else
#define EXTERN
#endif

EXTERN l4_msgtag_t l4_icu_bind_w (l4_cap_idx_t icu, unsigned irqnum, l4_cap_idx_t irq);
EXTERN l4_msgtag_t l4_icu_bind_u_w (l4_cap_idx_t icu, unsigned irqnum, l4_cap_idx_t irq, l4_utcb_t *utcb);
EXTERN l4_msgtag_t l4_icu_unbind_w (l4_cap_idx_t icu, unsigned irqnum, l4_cap_idx_t irq);
EXTERN l4_msgtag_t l4_icu_unbind_u_w (l4_cap_idx_t icu, unsigned irqnum, l4_cap_idx_t irq, l4_utcb_t *utcb);
EXTERN l4_msgtag_t l4_icu_set_mode_w (l4_cap_idx_t icu, unsigned irqnum, l4_umword_t mode);
EXTERN l4_msgtag_t l4_icu_set_mode_u_w (l4_cap_idx_t icu, unsigned irqnum, l4_umword_t mode, l4_utcb_t *utcb);
EXTERN l4_msgtag_t l4_icu_info_w (l4_cap_idx_t icu, l4_icu_info_t *info);
EXTERN l4_msgtag_t l4_icu_info_u_w (l4_cap_idx_t icu, l4_icu_info_t *info, l4_utcb_t *utcb);
EXTERN l4_msgtag_t l4_icu_msi_info_w (l4_cap_idx_t icu, unsigned irqnum, l4_uint64_t source, l4_icu_msi_info_t *msi_info);
EXTERN l4_msgtag_t l4_icu_msi_info_u_w (l4_cap_idx_t icu, unsigned irqnum, l4_uint64_t source, l4_icu_msi_info_t *msi_info, l4_utcb_t *utcb);
EXTERN l4_msgtag_t l4_icu_unmask_w (l4_cap_idx_t icu, unsigned irqnum, l4_umword_t *label, l4_timeout_t to);
EXTERN l4_msgtag_t l4_icu_unmask_u_w (l4_cap_idx_t icu, unsigned irqnum, l4_umword_t *label, l4_timeout_t to, l4_utcb_t *utcb);
EXTERN l4_msgtag_t l4_icu_mask_w (l4_cap_idx_t icu, unsigned irqnum, l4_umword_t *label, l4_timeout_t to);
EXTERN l4_msgtag_t l4_icu_mask_u_w (l4_cap_idx_t icu, unsigned irqnum, l4_umword_t *label, l4_timeout_t to, l4_utcb_t *utcb);
