#include <l4/sys/capability>
#include <l4/sys/typeinfo_svr>
#include <l4/sys/thread>
#include <l4/sys/ipc_gate>
#include <l4/sys/factory>
#include <l4/cxx/ipc_stream>
#include <l4/cxx/ipc_server>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>

#include <l4/log/log.h>

#include <asm/server/server.h>

L4::Cap<void>
l4x_srv_register(l4x_srv_object *o, L4::Cap<L4::Thread> thread)
{
	L4::Cap<L4::Ipc_gate> cap
		= L4Re::Util::cap_alloc.alloc<L4::Ipc_gate>();
	if (!cap)
		return cap;

	l4_msgtag_t t;
	t = L4Re::Env::env()->factory()->create_gate(cap, thread,
	                                             (l4_umword_t)o);
	if (l4_error(t)) {
		L4Re::Util::cap_alloc.free(cap);
		return L4::Cap<void>::Invalid;
	}

	int err = l4_error(cap->bind_thread(thread, (l4_umword_t)o));
	if (err < 0) {
		L4Re::Util::cap_alloc.free(cap);
		return L4::Cap<void>::Invalid;
	}

	return cap;
}

L4::Cap<void>
l4x_srv_register_name(l4x_srv_object *o,
                      L4::Cap<L4::Thread> thread, const char *service)
{
	L4::Cap<L4::Ipc_gate> cap
		= L4Re::Env::env()->get_cap<L4::Ipc_gate>(service);
	if (!cap)
		return cap;

	int err = l4_error(cap->bind_thread(thread, (l4_umword_t)o));
	if (err < 0)
		return L4::Cap<void>::Invalid;

	return cap;
}

C_FUNC l4_cap_idx_t
l4x_srv_register_c(struct l4x_srv_object *obj, l4_cap_idx_t thread)
{
	return l4x_srv_register(obj, L4::Cap<L4::Thread>(thread)).cap();
}

C_FUNC l4_cap_idx_t
l4x_srv_register_name_c(struct l4x_srv_object *obj,
                        l4_cap_idx_t thread, const char *service)
{
	return l4x_srv_register_name(obj, L4::Cap<L4::Thread>(thread),
	                             service).cap();
}

static L4::Cap<L4::Kobject> _rcv_cap;

C_FUNC void
l4x_srv_setup_recv(l4_utcb_t *u)
{
	l4_utcb_br_u(u)->br[0] = _rcv_cap.cap() | L4_RCV_ITEM_SINGLE_CAP
	                         | L4_RCV_ITEM_LOCAL_ID;
	l4_utcb_br_u(u)->bdr = 0;
}

static struct l4x_svr_ops const *_server_ops;

C_FUNC void
l4x_srv_init(struct l4x_svr_ops const *ops)
{
	_server_ops = ops;
	_rcv_cap = L4Re::Util::cap_alloc.alloc<L4::Kobject>();
	if (!_rcv_cap)
		LOG_printf("l4x_srv: rcv-cap alloc failed\n");
	l4x_srv_setup_recv(l4_utcb());
}


class L4x_server : public L4::Ipc_svr::Br_manager_no_buffers
{
public:
	L4::Cap<void> get_rcv_cap(int index) const
	{
		if (index != 0)
			return L4::Cap<void>::Invalid;
		return _rcv_cap;
	}

	int realloc_rcv_cap(int index)
	{
		if (index != 0)
			return -L4_EINVAL;

		l4_cap_idx_t c = _server_ops->cap_alloc();
		if (l4_is_invalid_cap(c))
			return -L4_ENOMEM;

		_rcv_cap = L4::Cap<L4::Kobject>(c);
		return 0;
	}
};

static L4x_server _l4x_server;

L4::Ipc_svr::Server_iface *l4x_srv_get_server_data()
{ return &_l4x_server; }


