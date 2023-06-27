#include <l4/l4rust/irq.h>
#include <l4/sys/irq.h>

EXTERN l4_msgtag_t l4_irq_mux_chain_w (l4_cap_idx_t irq, l4_cap_idx_t slave) L4_NOTHROW {
	return l4_irq_mux_chain(irq, slave);
}
EXTERN l4_msgtag_t l4_irq_mux_chain_u_w (l4_cap_idx_t irq, l4_cap_idx_t slave, l4_utcb_t *utcb) L4_NOTHROW {
	return l4_irq_mux_chain_u(irq, slave, utcb);
}
EXTERN l4_msgtag_t l4_irq_detach_w (l4_cap_idx_t irq) L4_NOTHROW {
	return l4_irq_detach(irq);
}
EXTERN l4_msgtag_t l4_irq_detach_u_w (l4_cap_idx_t irq, l4_utcb_t *utcb) L4_NOTHROW {
	return l4_irq_detach_u(irq, utcb);
}
EXTERN l4_msgtag_t l4_irq_trigger_w (l4_cap_idx_t irq) L4_NOTHROW {
	return l4_irq_trigger(irq);
}
EXTERN l4_msgtag_t l4_irq_trigger_u_w (l4_cap_idx_t irq, l4_utcb_t *utcb) L4_NOTHROW {
	return l4_irq_trigger_u(irq, utcb);
}
EXTERN l4_msgtag_t l4_irq_receive_w (l4_cap_idx_t irq, l4_timeout_t to) L4_NOTHROW {
	return l4_irq_receive(irq, to);
}
EXTERN l4_msgtag_t l4_irq_receive_u_w (l4_cap_idx_t irq, l4_timeout_t timeout, l4_utcb_t *utcb) L4_NOTHROW {
	return l4_irq_receive_u(irq, timeout, utcb);
}
EXTERN l4_msgtag_t l4_irq_wait_w (l4_cap_idx_t irq, l4_umword_t *label, l4_timeout_t to) L4_NOTHROW {
	return l4_irq_wait(irq, label, to);
}
EXTERN l4_msgtag_t l4_irq_wait_u_w (l4_cap_idx_t irq, l4_umword_t *label, l4_timeout_t timeout, l4_utcb_t *utcb) L4_NOTHROW {
	return l4_irq_wait_u(irq, label, timeout, utcb);
}
EXTERN l4_msgtag_t l4_irq_unmask_w (l4_cap_idx_t irq) L4_NOTHROW {
	return l4_irq_unmask(irq);
}
EXTERN l4_msgtag_t l4_irq_unmask_u_w (l4_cap_idx_t irq, l4_utcb_t *utcb) L4_NOTHROW {
	return l4_irq_unmask_u(irq, utcb);
}
