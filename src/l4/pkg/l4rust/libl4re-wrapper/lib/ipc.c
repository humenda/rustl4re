#include <pthread-l4.h>
#include <l4/sys/rcv_endpoint.h>
#include <l4/l4rust/ipc.h>

EXTERN l4_msg_regs_t *l4_utcb_mr_w() {
    return l4_utcb_mr();
}

EXTERN l4_msgtag_t l4_ipc_call_w(l4_cap_idx_t object,
        l4_utcb_t *utcb, l4_msgtag_t tag,
        l4_timeout_t timeout) {
    return l4_ipc_call(object, utcb, tag, timeout);
}

EXTERN l4_msgtag_t l4_ipc_send_w(l4_cap_idx_t object,
        l4_utcb_t *utcb, l4_msgtag_t tag, l4_timeout_t timeout) {
    return l4_ipc_send(object, utcb, tag, timeout);
}


EXTERN long l4_error_w (l4_msgtag_t tag) L4_NOTHROW {
    return l4_error(tag);
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

EXTERN l4_msgtag_t l4_ipc_receive_w(l4_cap_idx_t object, l4_utcb_t *utcb,
        l4_timeout_t timeout) {
    return l4_ipc_receive(object, utcb, timeout);
}


EXTERN l4_msgtag_t l4_ipc_reply_and_wait_w(l4_utcb_t *utcb, l4_msgtag_t tag,
        l4_umword_t *src, l4_timeout_t timeout) {
    return l4_ipc_reply_and_wait(utcb, tag, src, timeout);
}

EXTERN unsigned l4_msgtag_words_w(l4_msgtag_t t) {
    return l4_msgtag_words(t);
}

EXTERN l4_msgtag_t l4_rcv_ep_bind_thread_w(l4_cap_idx_t ep, l4_cap_idx_t thread,
          l4_umword_t label) {
    return l4_rcv_ep_bind_thread(ep, thread, label);
}

EXTERN int l4_sndfpage_add_wu(l4_fpage_t const snd_fpage, unsigned long snd_base,
                l4_msgtag_t *tag, l4_utcb_t *u) {
    return l4_sndfpage_add_u(snd_fpage, snd_base, tag, u);
}
