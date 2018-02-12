#define L4_RPC_DISABLE_LEGACY_DISPATCH 1
#include <l4/re/event>
#include <l4/re/util/event_svr>
#include <l4/re/util/event_buffer>
#include <l4/re/util/meta>
#include <l4/cxx/ipc_server>
#include <l4/log/log.h>
#include <l4/sys/cxx/ipc_epiface>
#include <asm/server/server.h>
#include <asm/server/input-srv.h>

#include <asm/server/util.h>

class Input : public L4Re::Util::Event_svr<Input>,
              public l4x_srv_epiface_t<Input, L4Re::Event>
{
public:
	l4x_input_srv_ops *ops;

	int init();
	int get_num_streams();
	int get_stream_info(int idx, L4Re::Event_stream_info *);
	int get_stream_info_for_id(l4_umword_t id, L4Re::Event_stream_info *);
	int get_axis_info(l4_umword_t id, unsigned naxes, unsigned const *axis,
	                  L4Re::Event_absinfo *i);

	void add(struct l4x_input_event const *);
	void trigger() const { _irq.trigger(); }

	L4::Ipc_svr::Server_iface *server_iface() const
	{
		return l4x_srv_get_server_data();
	}

	void reset_event_buffer() { _evbuf.reset(); }

private:
	L4Re::Util::Event_buffer _evbuf;
};

int
Input::init()
{
	return L4x_server_util::get_event_buffer(L4_PAGESIZE, &_evbuf, &_ds);
}

void
Input::add(struct l4x_input_event const *e)
{
	_evbuf.put(*reinterpret_cast<L4Re::Event_buffer::Event const *>(e));
}

int
Input::get_num_streams()
{
	return ops->num_streams();
}

int
Input::get_stream_info(int idx, L4Re::Event_stream_info *si)
{
	return ops->stream_info(idx, si);
}

int
Input::get_stream_info_for_id(l4_umword_t id, L4Re::Event_stream_info *si)
{
	return ops->stream_info_for_id(id, si);
}

int
Input::get_axis_info(l4_umword_t id, unsigned naxes, unsigned const *axis,
                     L4Re::Event_absinfo *i)
{
	return ops->axis_info(id, naxes, axis, i);
}

static Input _input;

static Input *input_obj()
{
	return &_input;
}

extern "C" void
l4x_srv_input_init(l4_cap_idx_t thread, struct l4x_input_srv_ops *ops)
{
	L4::Cap<void> c;
	int err;

	if ((err = input_obj()->init()) < 0) {
		LOG_printf("l4x-srv: Input server object initialization failed (%d)\n",
		           err);
		return;
	}


	input_obj()->ops = ops;
	c = l4x_srv_register_name(input_obj(),
	                          L4::Cap<L4::Thread>(thread), "ev");
	if (!c)
		LOG_printf("l4x-srv: Input object registration failed.\n");
}

extern "C" void
l4x_srv_input_add_event(struct l4x_input_event *e)
{
	input_obj()->add(e);
}

extern "C" void
l4x_srv_input_trigger()
{
	if (input_obj()->icu_get_irq(0)->cap())
		input_obj()->trigger();
}

