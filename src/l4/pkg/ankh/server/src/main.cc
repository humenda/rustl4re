#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <time.h>
#include <boost/format.hpp>

#include <l4/re/env>
#include <l4/re/env>
#include <l4/re/namespace>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/re/util/meta>
#include <l4/cxx/ipc_server>
#include <l4/cxx/std_exc_io>
#include <l4/dde/linux26/dde26.h>

#include <l4/sys/debugger.h>
#include <l4/ankh/protocol>
#include <l4/ankh/packet_analyzer.h>
#include "session"
#include "device"
#include "linux_glue.h"

static L4Re::Util::Registry_server<> server;

int Ankh::ServerSession::dispatch(l4_umword_t, L4::Ipc::Iostream &ios)
{
	l4_msgtag_t t;
	ios >> t;
	int ret = -1;

	if (t.label() != Ankh::Svc::Protocol)
		return -L4_EBADPROTO;

	L4::Opcode op;
	ios >> op;

	switch(op)
	{
		case Ankh::Svc::Activate:
			ret = shm_create();
			activate();
			break;

		case Ankh::Svc::Deactivate:
			deactivate();
			break;

		default:
			std::cout << "Unknown op: " << op << "\n";
			ret = -L4_ENOSYS;
	}

	return ret;
}



class Ankh_server : public L4::Server_object_t<L4::Factory>
{
	public: 
		int dispatch(l4_umword_t, L4::Ipc::Iostream &ios);
};


int Ankh_server::dispatch(l4_umword_t, L4::Ipc::Iostream &ios)
{
	l4_msgtag_t t;
	ios >> t;

	switch (t.label()) {
		/*
		 * Used by Ned to find out our supported protocols
		 */
		case L4::Meta::Protocol:
			return dispatch_meta_request(ios);

		/*
		 * Factory protocol to create new sessions
		 */
		case L4::Factory::Protocol:
			{
				unsigned long size = 250;
				char buf[size];
				memset(buf, 0, size);

				unsigned op;
				ios >> op;
				if (op != 0)
					return -L4_EINVAL;

				L4::Ipc::Varg opt;
				ios.get(&opt);

				if (!opt.is_of<char const *>()) {
					std::cerr << "[ERR] Parameter needs to be a string constant.\n";
					return -L4_EINVAL;
				}

				strncpy(buf, opt.value<char const*>(), size);
				unsigned len = opt.length();
				buf[len] = 0;

				Ankh::ServerSession *ret = Ankh::Session_factory::get()->create(buf);
				if (ret != 0)
				{
					server.registry()->register_obj(ret);
					ios << ret->obj_cap();
					return L4_EOK;
				}
				else
				{
					std::cerr << "Error creating Ankh session object.\n";
					return -L4_ENOMEM;
				}
			}
		default:
			std::cout << "Unknown protocol: " << t.label() << "\n";
			return -L4_ENOSYS;
	}

	return L4_EOK;
}

EXTERN_C void netdev_add(void *ptr)
{
	Ankh::Device_manager::dev_mgr()->add_device(ptr);
}


EXTERN_C void enable_ux_self()
{
	l4_thread_control_start();
	l4_thread_control_ux_host_syscall(1);
	l4_thread_control_commit(pthread_l4_cap(pthread_self()));
}


EXTERN_C int packet_deliver(void *packet, unsigned len, char const * const dev, unsigned local)
{
#if 0
	static int callcount = 0;
	if (++callcount >= 7500) {
	    L4Re::Env::env()->parent()->signal(0, 1);
	}
#endif

	assert(packet != 0);
	unsigned cnt = 0;
	int c = l4_debugger_global_id(pthread_l4_cap(pthread_self()));

#if 0
	Ankh::Util::print_mac(static_cast<unsigned char*>(packet));
	std::cout << " <- ";
	Ankh::Util::print_mac(static_cast<unsigned char*>(packet)+6);
	std::cout << std::endl;
#endif

	Ankh::ServerSession *s = Ankh::Session_factory::get()->find_session_for_mac(
	                               static_cast<const char * const>(packet), dev);
	while (s != 0)
	{
		if (cnt == 0) {
			if (s->debug()) {
				packet_analyze(static_cast<char*>(packet), len);
			}
		}
		if (local)
			s->dev()->lock();
		s->deliver(static_cast<char*>(packet), len);
		if (local)
			s->dev()->unlock();
		++cnt;
		s = Ankh::Session_factory::get()->find_session_for_mac(
		          static_cast<const char * const>(packet), dev, s);
	}

	return cnt;
}

int main()
{
	Ankh_server ankh;

	int num_devs = open_network_devices(1);
	std::cout << "Opened " << num_devs << " network devices.\n";
	Ankh::Device_manager::dev_mgr()->dump_devices();

	server.registry()->register_obj(&ankh, "ankh_service");
	std::cout << "Registered @ registry.\n";

	std::cout << "Gooood Morning Ankh-Morpoooooork! TID = 0x" << std::hex << l4_debugger_global_id(pthread_l4_cap(pthread_self())) << std::dec << "\n";
	l4_debugger_set_object_name(pthread_l4_cap(pthread_self()), "ankh.main");

	try
	{
		server.loop();
	}
	catch (L4::Base_exception const &e)
	{
		std::cerr << "Error: " << e << '\n';
		return -1;
	}
	catch (std::exception const &e)
	{
		std::cerr << "Error: " << e.what() << '\n';
	}

	return 0;
}
