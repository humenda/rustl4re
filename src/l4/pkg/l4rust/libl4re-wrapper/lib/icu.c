#include <l4/l4rust/icu.h>

EXTERN l4_msgtag_t l4_icu_bind_w (l4_cap_idx_t icu, unsigned irqnum, l4_cap_idx_t irq) {
	return l4_icu_bind(icu, irqnum, irq);
}

EXTERN l4_msgtag_t l4_icu_bind_u_w (l4_cap_idx_t icu, unsigned irqnum, l4_cap_idx_t irq, l4_utcb_t *utcb) {
	return l4_icu_bind_u(icu, irqnum, irq, utcb);
}

EXTERN l4_msgtag_t l4_icu_unbind_w (l4_cap_idx_t icu, unsigned irqnum, l4_cap_idx_t irq) {
	return l4_icu_unbind(icu, irqnum, irq);
}

EXTERN l4_msgtag_t l4_icu_unbind_u_w (l4_cap_idx_t icu, unsigned irqnum, l4_cap_idx_t irq, l4_utcb_t *utcb) {
	return l4_icu_unbind_u(icu, irqnum, irq, utcb);
}

EXTERN l4_msgtag_t l4_icu_set_mode_w (l4_cap_idx_t icu, unsigned irqnum, l4_umword_t mode) {
	return l4_icu_set_mode(icu, irqnum, mode);
}

EXTERN l4_msgtag_t l4_icu_set_mode_u_w (l4_cap_idx_t icu, unsigned irqnum, l4_umword_t mode, l4_utcb_t *utcb) {
	return l4_icu_set_mode_u(icu, irqnum, mode, utcb);
}

EXTERN l4_msgtag_t l4_icu_info_w (l4_cap_idx_t icu, l4_icu_info_t *info) {
	return l4_icu_info(icu, info);
}

EXTERN l4_msgtag_t l4_icu_info_u_w (l4_cap_idx_t icu, l4_icu_info_t *info, l4_utcb_t *utcb) {
	return l4_icu_info_u(icu, info, utcb);
}

EXTERN l4_msgtag_t l4_icu_msi_info_w (l4_cap_idx_t icu, unsigned irqnum, l4_uint64_t source, l4_icu_msi_info_t *msi_info) {
	return l4_icu_msi_info(icu, irqnum, source, msi_info);
}

EXTERN l4_msgtag_t l4_icu_msi_info_u_w (l4_cap_idx_t icu, unsigned irqnum, l4_uint64_t source, l4_icu_msi_info_t *msi_info, l4_utcb_t *utcb) {
	return l4_icu_msi_info_u(icu, irqnum, source, msi_info, utcb);
}

EXTERN l4_msgtag_t l4_icu_unmask_w (l4_cap_idx_t icu, unsigned irqnum, l4_umword_t *label, l4_timeout_t to) {
	return l4_icu_unmask(icu, irqnum, label, to);
}

EXTERN l4_msgtag_t l4_icu_unmask_u_w (l4_cap_idx_t icu, unsigned irqnum, l4_umword_t *label, l4_timeout_t to, l4_utcb_t *utcb) {
	return l4_icu_unmask_u(icu, irqnum, label, to, utcb);
}

EXTERN l4_msgtag_t l4_icu_mask_w (l4_cap_idx_t icu, unsigned irqnum, l4_umword_t *label, l4_timeout_t to) {
	return l4_icu_mask(icu, irqnum, label, to);
}

EXTERN l4_msgtag_t l4_icu_mask_u_w (l4_cap_idx_t icu, unsigned irqnum, l4_umword_t *label, l4_timeout_t to, l4_utcb_t *utcb) {
	return l4_icu_mask_u(icu, irqnum, label, to, utcb);
}
