#include <pthread-l4.h>
#include <l4/l4re-rust/ipc.h>

EXTERN l4_msg_regs_t *l4_utcb_mr_w() {
    return l4_utcb_mr();
}

EXTERN l4_msgtag_t l4_ipc_call_w(l4_cap_idx_t object,
        l4_utcb_t *utcb, l4_msgtag_t tag,
        l4_timeout_t timeout) {
    return l4_ipc_call(object, utcb, tag, timeout);
}

EXTERN l4_umword_t l4_ipc_error_w(l4_msgtag_t tag, l4_utcb_t *utcb) {
    return l4_ipc_error(tag, utcb);
}

EXTERN l4_msgtag_t l4_ipc_wait_w(l4_utcb_t *utcb, l4_umword_t *label,
        l4_timeout_t timeout) {
    return l4_ipc_wait(utcb, label, timeout);
}
l4_utcb_t *l4_utcb_w() {
    return l4_utcb();
}

EXTERN l4_msgtag_t l4_msgtag_w(long label, unsigned words, unsigned items,
        unsigned flags) {
    return l4_msgtag(label, words, items, flags);
}

EXTERN l4_msgtag_t l4_ipc_reply_and_wait_w(l4_utcb_t *utcb, l4_msgtag_t tag,
        l4_umword_t *src, l4_timeout_t timeout) {
    return l4_ipc_reply_and_wait(utcb, tag, src, timeout);
}

