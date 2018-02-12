/**
 * \file   ferret/lib/client/create.cc
 * \brief  Function to create a new sensor at the directory.
 *
 * \date   2009-03-07
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 * \author Bjoern Doebel   <doebel@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/ferret/types.h>

#include <iostream>    // std::cout
#include <l4/cxx/ipc_stream>    // L4::Ipc::Iostream
#include <l4/re/util/cap_alloc> // L4::Cap
#include <l4/re/dataspace>      // L4Re::Dataspace
#include <l4/re/mem_alloc>      // mem_alloc()
#include <l4/re/rm>             // L4::Rm
#include <l4/re/env>            // L4::Env
#include <l4/sys/kdebug.h>      // enter_kdebug()

#include <string>
#include <vector>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>


#include <l4/ferret/client.h>
#include <l4/ferret/sensors/common.h>
#include <l4/ferret/sensors/scalar.h>
#include <l4/ferret/sensors/scalar_init.h>
#include <l4/ferret/sensors/list_producer.h>
#include <l4/ferret/sensors/list_init.h>
#include <l4/ferret/sensordir.h>

#define DEBUG 0

static int _ferret_create_sensor(l4_cap_idx_t dir_cap,
                                 uint16_t major, uint16_t minor,
                                 uint16_t instance, uint16_t type,
                                 uint32_t flags, const char * config,
                                 void ** addr, void *(*alloc)(size_t s))
{
#if DEBUG
	std::cout << __func__ << "\n";
	std::cout << Client << " .. " << Create << "\n";
	std::cout << "(" << major << "," << minor << "," << instance << ")\n";
	std::cout << type << ", " << flags << "\n";
#endif
	int err;
	unsigned e_size = 0, e_num = 0;

	switch (type)
	{
		case FERRET_SCALAR:
			break;
		case FERRET_LIST:
			{
				std::vector<std::string> v(2);
				boost::split(v, config, boost::is_any_of(":"));
				e_size = atoi(v[0].c_str());
				e_num  = atoi(v[1].c_str());
#if DEBUG
				std::cout << "size = " << e_size << " * " << e_num
				          << " = " << e_size * e_num << "\n";
#endif

			}
	}

	/*
	 * Locally alloc a cap for the new DS
	 */
    L4::Cap<L4Re::Dataspace> ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();

#if DEBUG
	std::cout << "allocated cap: " << ds.cap() << "\n";
#endif

	L4::Ipc::Iostream s(l4_utcb());
	s << L4::Opcode(Ferret::Client::Create);                         // Opcode
	s << major << minor << instance << type << flags; // Parameters

	/*
	 * Sensor-specific parameters
	 */
	switch(type)
	{
		case FERRET_SCALAR:
			// fall through
		case FERRET_LIST:
			s << e_size << e_num;
		default:
			break;
	}

	// recv fpage is final parameter
	s << L4::Ipc::Small_buf(ds.cap(), 0);                  // Recv. fpage for ds cap

	l4_msgtag_t res = s.call(dir_cap, Ferret::Client::Protocol);
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

	/*
	 * Post-processing
	 */
	if (ret == 0)
	{
		if (!ds.is_valid())
		{
			std::cout << "Did not get a cap mapped in create_sensor()\n";
			return -1;
		}

		/*
		 * Fill sensor header
		 */
		err =  L4Re::Env::env()->rm()->attach(addr, ds->size(),
		                                      L4Re::Rm::Search_addr | L4Re::Rm::Eager_map,
		                                      ds);
		if (err < 0)
		{
			std::cout << "Attach DS failed in " << __func__ << "\n";
			return -1;
		}

		ferret_common_t *_head = (ferret_common_t*)(*addr);
		_head->major    = major;
		_head->minor    = minor;
		_head->instance = instance;
		_head->type     = type;
		_head->ds_cap   = ds.cap();
		_head->ds_size  = ds->size();

		/*
		 * Sensor-specific processing
		 */
		switch(type)
		{
			case FERRET_SCALAR:
				err = ferret_scalar_init((ferret_scalar_t*)*addr, config);
				if (err)
					std::cout << "Error configuring scalar sensor\n";
				break;
			case FERRET_LIST:
				ferret_list_init((ferret_list_t*)*addr, config);
				ferret_list_init_producer(addr, alloc);
				break;
			default:
				break;
		}
	}

	return ret;
}

int ferret_create_sensor(l4_cap_idx_t dir_cap,
                         uint16_t major, uint16_t minor,
                         uint16_t instance, uint16_t type,
                         uint32_t flags, const char * config,
                         void ** addr, void *(*alloc)(size_t s))
{
#if 0
    // get ferret_sensor_dir thread_id lazily
    if (l4_is_invalid_id(ferret_sensor_dir))
        ferret_sensor_dir = ferret_comm_get_threadid();
#endif

    return _ferret_create_sensor(dir_cap, major, minor, instance, type,
                                 flags, config, addr, alloc);
}
