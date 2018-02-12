/**
 * \file   ferret/lib/monitor/monitor.cc
 * \brief  Functions to access sensors from monitors.
 *
 * \date   10/03/2009
 * \author Martin Pohlack <mp26@os.inf.tu-dresden.de>
 * \author Bjoern Doebel <doebel@tudos.org>
 */
/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/ferret/types.h>

#include <stdlib.h>
#include <iostream>    // std::cout
#include <l4/cxx/ipc_stream>    // L4::Ipc::Iostream
#include <l4/re/util/cap_alloc> // L4::Cap
#include <l4/re/dataspace>      // L4Re::Dataspace
#include <l4/re/mem_alloc>      // mem_alloc()
#include <l4/re/rm>             // L4::Rm
#include <l4/re/env>            // L4::Env


//#include <l4/ferret/clock.h>
#include <l4/ferret/comm.h>
#include <l4/ferret/monitor.h>
#include <l4/ferret/monitor_list.h>
#include <l4/ferret/sensordir.h>

#include <l4/ferret/sensors/common.h>
#include <l4/ferret/sensors/list_consumer.h>
//#include <l4/ferret/sensors/dplist_consumer.h>
//#include <l4/ferret/sensors/slist_consumer.h>
//#include <l4/ferret/sensors/ulist_consumer.h>
//#include <l4/ferret/sensors/tbuf.h>
//#include <l4/ferret/sensors/tbuf_consumer.h>

int ferret_attach(l4_cap_idx_t srv,
                  uint16_t major, uint16_t minor,
                  uint16_t instance, void ** addr)
{
	/*
	 * Locally alloc a cap for the new DS
	 */
    L4::Cap<L4Re::Dataspace> ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();

	L4::Ipc::Iostream s(l4_utcb());
	s << L4::Opcode(Ferret::Monitor::Attach);
	s << major << minor << instance;
	s << L4::Ipc::Small_buf(ds.cap(), 0);

	l4_msgtag_t res = s.call(srv, Ferret::Monitor::Protocol);
	if (l4_ipc_error(res, l4_utcb()))
	{
		std::cout << "IPC error in " << __func__ << "()\n";
		return -1;
	}

	/*
	 * unpack return val
	 */
	int ret, err;
	s >> ret;

	/*
	 * Post-processing
	 */
	if (ret == 0)
	{
#if DEBUG
		std::cout << "Successfully attached. Got DS.\n";
#endif
		if (!ds.is_valid())
		{
			std::cout << "No DS mapped during attach.\n";
			return -1;
		}

		err =  L4Re::Env::env()->rm()->attach(addr, ds->size(),
		                                      L4Re::Rm::Search_addr,
		                                      ds);
		if (err < 0)
		{
			std::cout << "Could not attach DS in " << __func__ << "\n";
			std::cout << "Error: " <<  l4sys_errtostr(err) << "\n";
			return -1;
		}

		/*
		 * Sensor-specific processing
		 */
		switch(((ferret_common_t*)*addr)->type)
		{
			case FERRET_LIST:
				ferret_list_init_consumer(addr);
				break;
			case FERRET_SCALAR:
				// fall through
			default:
				break;
		}
	}
	else
	{
		std::cout << "Failure attaching to sensor " << ret << "\n";
	}

	return ret;
}

int ferret_detach(l4_cap_idx_t srv,
                  uint16_t major, uint16_t minor,
                  uint16_t instance, void ** addr)
{
	L4::Ipc::Iostream s(l4_utcb());
	s << L4::Opcode(Ferret::Monitor::Detach);
	s << major << minor << instance;

	l4_msgtag_t res = s.call(srv, Ferret::Monitor::Protocol);
	if (l4_ipc_error(res, l4_utcb()))
	{
		std::cout << "IPC error in " << __func__ << "()\n";
		return -1;
	}

	/*
	 * unpack return val
	 */
	int ret;
	s >> ret;

	if (ret == 0)
	{
		/*
		 * Sensor-specific processing
		 */
		switch(((ferret_common_t*)*addr)->type)
		{
			case FERRET_SCALAR:
				// fall through
			default:
				break;
		}

		// detach dataspace
		L4::Cap<L4Re::Dataspace> ds(((ferret_common_t*)*addr)->ds_cap);

		int err =  L4Re::Env::env()->rm()->detach(*addr, 0);
#if DEBUG
		std::cout << "detach: " << err << "\n";
#endif
		if (err == L4Re::Rm::Detached_ds)
		{
			l4_msgtag_t res = l4_task_unmap(L4RE_THIS_TASK_CAP,
			                                ds.fpage(), L4_FP_ALL_SPACES);
			if (l4_error(res))
			{
				std::cout << "unmap error: " << l4_error(res) << "\n";
				ret = -1;
			}
		}
		else
		{
			std::cout << "Error detaching sensor ds.\n";
			ret = -1;
		}
	}
	else
	{
		std::cout << "Failure detaching sensor: " << ret << "\n";
	}

	return ret;
}

int ferret_list(l4_cap_idx_t srv __attribute__((unused)),
                ferret_monitor_list_entry_t ** entries __attribute__((unused)),
                int count __attribute__((unused)),
                int offset __attribute__((unused)))
{
#if 0
    int ret;
    CORBA_Environment _dice_corba_env = dice_default_environment;
    _dice_corba_env.malloc = (dice_malloc_func)malloc;
    _dice_corba_env.free = (dice_free_func)free;

    // get sensor_dir task id lazily
    if (l4_is_invalid_id(ferret_sensor_dir))
        ferret_sensor_dir = ferret_comm_get_threadid();

    ret = ferret_monitor_list_call(&ferret_sensor_dir, entries, &count, offset,
                                    &_dice_corba_env);
    if (ret < 0)
        return ret;
    else
        return count;
#endif
	return -1;
}
