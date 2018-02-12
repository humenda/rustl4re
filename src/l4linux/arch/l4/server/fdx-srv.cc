#define L4_RPC_DISABLE_LEGACY_DISPATCH 1
#include <l4/sys/cxx/ipc_epiface>
#include <l4/log/log.h>
#include <l4/re/event>
#include <l4/libfdx/fdx>

#include <l4/re/util/event_buffer>
#include <l4/re/util/event_svr>
#include <l4/re/util/meta>
#include <l4/re/util/unique_cap>

#include <asm/server/server.h>
#include <asm/server/fdx-srv.h>
#include <asm/server/util.h>

#include <cassert>
#include <stdlib.h>

#include <l4/sys/factory>

class Fdx_factory : public l4x_srv_epiface_t<Fdx_factory, L4::Factory>
{
public:
	explicit Fdx_factory(l4x_fdx_srv_factory_ops *ops)
	: ops(ops)
	{}

	void *operator new(size_t size, void *p)
	{
		assert(size == sizeof(Fdx_factory));
		return p;
	}

	int op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &obj,
	              long proto, L4::Ipc::Varg_list_ref args);

private:
	l4x_fdx_srv_factory_ops *ops;

	int get_unsigned(L4::Ipc::Varg o, const char *param, unsigned &result);
};

int Fdx_factory::get_unsigned(L4::Ipc::Varg o, const char *param,
                              unsigned &result)
{
	char buf[12];
	unsigned param_len = strlen(param);

	if (o.length() > param_len
	    && !strncmp(o.value<char const *>(), param, param_len)) {
		unsigned l = o.length() - param_len;
		memcpy(buf, o.value<char const *>() + param_len, l);
		buf[l] = 0;
		result = strtoul(buf, 0, 0);
		return 1;
	}
	return 0;
}

int Fdx_factory::op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &obj,
                           long, L4::Ipc::Varg_list_ref _args)
{
	L4::Ipc::Varg_list<> args(_args);
	l4x_fdx_srv_factory_create_data data;
	data.opt_flags = 0;

	for (L4::Ipc::Varg o = args.next(); !o.is_nil(); o = args.next()) {
		if (!o.is_of<char const *>()) {
			LOG_printf("blk-srv: Invalid option\n");
			continue;
		}

		if (get_unsigned(o, "uid=", data.uid)) {
			data.opt_flags |= L4X_FDX_SRV_FACTORY_HAS_UID;
		} else if (get_unsigned(o, "gid=", data.gid)) {
			data.opt_flags |= L4X_FDX_SRV_FACTORY_HAS_GID;
		} else if (get_unsigned(o, "openflags_mask=", data.openflags_mask)) {
			data.opt_flags |= L4X_FDX_SRV_FACTORY_HAS_OPENFLAGS_MASK;
		} else if (o.length() == 6
		           && !strncmp(o.value<char const *>(), "nogrow", 6)) {
			data.opt_flags |= L4X_FDX_SRV_FACTORY_HAS_FLAG_NOGROW;
		} else if (o.length() > 9
		           && !strncmp(o.value<char const *>(), "basepath=", 9)) {
			data.basepath_len = o.length() - 9 - 1;
			data.basepath = o.value<char const *>() + 9;
			data.opt_flags |= L4X_FDX_SRV_FACTORY_HAS_BASEPATH;
		} else if (o.length() > 11
		           && !strncmp(o.value<char const *>(), "filterpath=", 11)) {
			data.filterpath_len = o.length() - 11 - 1;
			data.filterpath = o.value<char const *>() + 11;
			data.opt_flags |= L4X_FDX_SRV_FACTORY_HAS_FILTERPATH;
		} else if (o.length() > 11
		           && !strncmp(o.value<char const *>(), "clientname=", 11)) {
			data.clientname_len = o.length() - 11 - 1;
			data.clientname = o.value<char const *>() + 11;
			data.opt_flags |= L4X_FDX_SRV_FACTORY_HAS_CLIENTNAME;
		} else {
			LOG_printf("blk-srv: Unknown option '%.*s'\n",
			           o.length(), o.value<char const *>());
		}
	}

	l4_cap_idx_t client_cap;
	int r = ops->create(&data, &client_cap);
	if (r < 0)
		return r;
	obj = L4::Ipc::make_cap(L4::Cap<L4::Kobject>(client_cap),
	                        L4_CAP_FPAGE_RWSD);

	return 0;
}

C_FUNC int
l4x_fdx_factory_create(l4_cap_idx_t thread,
                       struct l4x_fdx_srv_factory_ops *ops,
                       void *objmem)
{

	Fdx_factory *fact = new (objmem) Fdx_factory(ops);

	L4::Cap<void> c;
	c = l4x_srv_register_name(fact, L4::Cap<L4::Thread>(thread), "fdx");
	if (!c) {
		LOG_printf("l4x-fdx-srv: 'fdx' object registration failed.\n");
		return -L4_ENODEV;
	}

	return 0;
}

C_FUNC unsigned
l4x_fdx_factory_objsize()
{
	return sizeof(Fdx_factory);
}



class Fdx_server : public L4Re::Util::Event_svr<Fdx_server>,
                   public l4x_srv_epiface_t<Fdx_server, Fdx>,
                   public l4fdx_srv_struct
{
public:
	l4x_fdx_srv_ops *ops;

	enum {
		Size_shm = 512 << 10,
	};

	int init();

	void reset_event_buffer() {}

	unsigned long shm_base() const
	{ return (unsigned long)_shm_base; }
	void add(struct l4fdx_result_t *);
	void trigger() const
	{
		if (l4_ipc_error(_irq.cap()->trigger(), l4_utcb()))
			LOG_printf("l4fdx-srv: Notification failed.\n");
	}

	L4::Ipc_svr::Server_iface *server_iface() const
	{ return l4x_srv_get_server_data(); }

	l4x_fdx_srv_data srv_data;

	void *operator new(size_t size, void *p)
	{
		assert(size == sizeof(Fdx_server));
		return p;
	}

	int op_request_open(Fdx::Rights, unsigned req_id, L4::Ipc::String<> const &path,
	                    int flags, unsigned mode);
	int op_request_read(Fdx::Rights, unsigned req_id, unsigned fid,
	                    unsigned long long offset,
	                    unsigned sz, unsigned shm_offset);
	int op_request_write(Fdx::Rights, unsigned req_id, unsigned fid,
	                     unsigned long long offset,
	                     unsigned sz, unsigned shm_offset);
	int op_request_close(Fdx::Rights, unsigned req_id, unsigned fid);
	int op_getshm(Fdx::Rights, L4::Ipc::Cap<L4Re::Dataspace> &ds)
	{
		ds = L4::Ipc::make_cap(_shm_ds, L4_CAP_FPAGE_RW);
		return 0;
	}

	int op_ping(Fdx::Rights)
	{
		return 0;
	}

	int op_request_fstat(Fdx::Rights, unsigned req_id, unsigned fid,
	                     unsigned shm_offset);
	int alloc_shm(L4::Cap<L4Re::Dataspace> *ds,
                      void **addr, unsigned size);
	void free_shm(void *addr);

	typedef L4Re::Util::Event_buffer_t<l4fdx_result_payload_t> Evbuf;
	Evbuf _evbuf;
	L4::Cap<L4Re::Dataspace> _shm_ds;
	void *_shm_base;
};

int
Fdx_server::alloc_shm(L4::Cap<L4Re::Dataspace> *ds,
                      void **addr, unsigned size)
{
	auto d = L4Re::Util::make_unique_cap<L4Re::Dataspace>();
	if (!d.is_valid())
		return -L4_ENOMEM;

	int r = L4Re::Env::env()->mem_alloc()->alloc(size, d.get());
	if (r < 0)
		return r;

	*addr = 0;
	r = L4Re::Env::env()->rm()->attach(addr, size,
	                                   L4Re::Rm::Search_addr,
	                                   L4::Ipc::make_cap_rw(d.get()));
	if (r < 0)
		return r;

	*ds = d.release();
	return 0;
}

void
Fdx_server::free_shm(void *addr)
{
	int r;

	L4::Cap<L4Re::Dataspace> ds;

	r = L4Re::Env::env()->rm()->detach(addr, &ds);

	if (r == L4Re::Rm::Detached_ds)
		L4Re::Util::cap_alloc.free(ds, L4Re::This_task);
}

int
Fdx_server::init()
{
	int r = L4x_server_util::get_event_buffer(L4_PAGESIZE,
	                                          &_evbuf, &_ds);
	if (r)
		return r;

	r = alloc_shm(&_shm_ds, &_shm_base, Size_shm);
	if (r)
		goto out_free_ev;

	return 0;

out_free_ev:
	L4x_server_util::free_event_buffer(&_evbuf, _ds);
	return r;
}

int
Fdx_server::op_request_open(Fdx::Rights, unsigned req_id, L4::Ipc::String<> const &path,
                            int flags, unsigned mode)
{
	return ops->open(this, req_id, path.data, path.length, flags, mode);
}

int
Fdx_server::op_request_close(Fdx::Rights, unsigned req_id, unsigned fid)
{
	return ops->close(this, req_id, fid);
}

int
Fdx_server::op_request_read(Fdx::Rights, unsigned req_id, unsigned fid,
                            unsigned long long start,
                            unsigned size, unsigned shm_offset)
{
	return ops->read(this, req_id, fid, start, size, shm_offset);
}

int
Fdx_server::op_request_write(Fdx::Rights, unsigned req_id, unsigned fid,
                             unsigned long long start,
                             unsigned size, unsigned shm_offset)
{
	return ops->write(this, req_id, fid, start, size, shm_offset);
}

int
Fdx_server::op_request_fstat(Fdx::Rights, unsigned req_id, unsigned fid,
                             unsigned shm_offset)
{
	return ops->fstat(this, req_id, fid, shm_offset);
}

void
Fdx_server::add(struct l4fdx_result_t *e)
{
	if (L4_UNLIKELY(!_evbuf.put(*reinterpret_cast<Evbuf::Event const *>(e))))
		LOG_printf("l4fdx-srv: Posting message failed.\n");
}

static l4fdx_srv_obj
__l4x_fdx_srv_create(l4_cap_idx_t thread, struct l4x_fdx_srv_ops *ops,
                     struct l4fdx_client *client, void *objmem,
                     l4_cap_idx_t *client_cap, const char *capname)
{
	long err;

	Fdx_server *fdx = new (objmem) Fdx_server();

	if ((err = fdx->init()) < 0) {
		LOG_printf("l4x-fdx-srv: Initialization failed (%ld)\n", err);
		return (l4fdx_srv_obj)err;
	}

	fdx->ops = ops;

	fdx->srv_data.shm_base = fdx->shm_base();
	fdx->srv_data.shm_size = fdx->Size_shm;

	fdx->client = client;

	L4::Cap<void> c;

	if (capname)
		c = l4x_srv_register_name(fdx, L4::Cap<L4::Thread>(thread),
		                          capname);
	else
		c = l4x_srv_register(fdx, L4::Cap<L4::Thread>(thread));
	if (!c) {
		LOG_printf("l4x-fdx-srv: Object registration failed.\n");
		return (l4fdx_srv_obj)-L4_ENODEV;
	}

	if (client_cap)
		*client_cap = c.cap();

	return fdx;
}

extern "C" l4fdx_srv_obj
l4x_fdx_srv_create_name(l4_cap_idx_t thread, const char *capname,
                        struct l4x_fdx_srv_ops *ops,
                        struct l4fdx_client *client,
                        void *objmem)
{
	return __l4x_fdx_srv_create(thread, ops, client, objmem,
	                            0, capname);

}

extern "C" l4fdx_srv_obj
l4x_fdx_srv_create(l4_cap_idx_t thread,
                   struct l4x_fdx_srv_ops *ops,
                   struct l4fdx_client *client,
                   void *objmem,
                   l4_cap_idx_t *client_cap)
{
	return __l4x_fdx_srv_create(thread, ops, client, objmem,
	                            client_cap, 0);
}

C_FUNC unsigned
l4x_fdx_srv_objsize()
{
	return sizeof(Fdx_server);
}

static inline Fdx_server *cast_to(l4fdx_srv_obj o)
{
	return static_cast<Fdx_server *>(o);
}

C_FUNC void
l4x_fdx_srv_add_event(l4fdx_srv_obj fdxobjp, struct l4fdx_result_t *e)
{
	cast_to(fdxobjp)->add(e);
}

C_FUNC void
l4x_fdx_srv_trigger(l4fdx_srv_obj fdxobjp)
{
	cast_to(fdxobjp)->trigger();
}

C_FUNC struct l4x_fdx_srv_data *
l4x_fdx_srv_get_srv_data(l4fdx_srv_obj fdxobjp)
{
	return &cast_to(fdxobjp)->srv_data;
}
