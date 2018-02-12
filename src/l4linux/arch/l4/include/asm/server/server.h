#ifndef __ASM_L4__GENERIC__SERVER_H__
#define __ASM_L4__GENERIC__SERVER_H__

#include <l4/sys/utcb.h>

struct l4x_svr_ops {
	l4_cap_idx_t (*cap_alloc)(void);
	void (*cap_free)(l4_cap_idx_t cap);
};

struct l4x_srv_object
{
	L4_CV long (*dispatch)(struct l4x_srv_object *, l4_umword_t obj,
	                       l4_utcb_t *msg, l4_msgtag_t *tag);
};

#ifdef __cplusplus

#define C_FUNC extern "C" L4_CV

#include <l4/sys/capability>
#include <l4/log/log.h>
#include <l4/sys/cxx/ipc_epiface>

L4::Cap<void>
l4x_srv_register(l4x_srv_object *o, L4::Cap<L4::Thread> thread);

L4::Cap<void>
l4x_srv_register_name(l4x_srv_object *o,
                      L4::Cap<L4::Thread> thread,
                      const char *service);

template< typename T>
L4_CV
long l4x_srv_generic_dispatch(l4x_srv_object *_this, l4_umword_t obj,
                              l4_utcb_t *msg, l4_msgtag_t *tag)
{
	l4_msgtag_t r = static_cast<T*>(_this)->dispatch(*tag, obj, msg);

	if (r.label() != -L4_ENOREPLY) {
		*tag = l4_ipc_send(L4_INVALID_CAP | L4_SYSF_REPLY, msg, r, L4_IPC_BOTH_TIMEOUT_0);
		if ((*tag).has_error())
			LOG_printf("IPC failed (r=%ld, e=%ld)\n",
			           r.label(), l4_error(*tag));
	}

	return r.label();
}

template<typename CRT>
class l4x_srv_object_tmpl : public l4x_srv_object
{
public:
	l4x_srv_object_tmpl()
	{
		l4x_srv_object::dispatch = &l4x_srv_generic_dispatch<CRT>;
	}
};

template<typename CRT, typename IFACE>
struct l4x_srv_epiface_t :
	L4::Epiface_t<CRT, IFACE, l4x_srv_object_tmpl<CRT> >
{
};

L4::Ipc_svr::Server_iface *l4x_srv_get_server_data();

#else
#define C_FUNC L4_CV
#include <l4/sys/types.h>
#endif

C_FUNC l4_cap_idx_t
l4x_srv_register_c(struct l4x_srv_object *obj, l4_cap_idx_t thread);

C_FUNC l4_cap_idx_t
l4x_srv_register_name_c(struct l4x_srv_object *obj,
                        l4_cap_idx_t thread, const char *service);

C_FUNC void l4x_srv_init(struct l4x_svr_ops const *ops);
C_FUNC void l4x_srv_setup_recv(l4_utcb_t *u);

#endif /* __ASM_L4__GENERIC__SERVER_H__ */
