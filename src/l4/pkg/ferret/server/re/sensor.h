/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once
#include <l4/ferret/types.h>          // types

#include <l4/cxx/ipc_server>    // L4::Server*
#include <iostream>    // std::cout
#include <l4/cxx/ipc_stream>    // L4::Ipc::Iostream
#include <l4/re/util/cap_alloc> // L4::Cap
#include <l4/re/dataspace>      // L4Re::Dataspace
#include <l4/re/mem_alloc>      // mem_alloc()
#include <l4/re/rm>             // L4::Rm
#include <l4/re/env>            // L4::Env
#include <l4/re/util/object_registry>

#include <l4/ferret/sensordir.h>      // Protocol & Opcodes
#include <l4/ferret/sensors/common.h> // sensor types
#include <l4/ferret/sensors/scalar.h>

#include <cstdlib>
#include <list>

#define DEBUG 0

namespace Ferret
{
	/**
	 * Sensors are the basic server objects managed by the sensor directory.
	 *
	 * A sensor resides in a dataspace. The dataspace contains a common head,
	 * followed by a type-specific head. This head is used to describe the
	 * sensor. The rest of the DS is used to contain the sensor's event
	 * payload, which again, depends on the sensor type.
	 *
	 * The sensor directory does _not_ distinguish sensors of different
	 * types. This is up to clients and monitors. The directory only cares
	 * about their common subset - major, minor and instance numbers.
	 */
	class Sensor
	{
		public:
			uint16_t major() const     { return _major; }
			uint16_t minor() const     { return _minor; }
			uint16_t instance() const  { return _instance; }
			uint16_t type() const      { return _type; }
			unsigned int refcount()    { return _refcount; }
			void inc() { ++_refcount; }
			void dec() { --_refcount; }

			uint32_t elem_size() const { return _elemsize; }
			void elem_size(uint32_t v) { _elemsize = v; }
			uint32_t num_elem() const  { return _num_elem; }
			void num_elem(uint32_t v)  { _num_elem = v; }

			L4::Cap<L4Re::Dataspace> const ds_cap() { return _dataspace; }

			Sensor(uint16_t const major, uint16_t const minor,
			       uint16_t const instance, uint16_t const type,
			       uint32_t e_size, uint32_t e_num);

			~Sensor()
			{
				// No need to rm->detach() -- we did not ever attach this ds
				l4_msgtag_t r = l4_task_unmap(L4RE_THIS_TASK_CAP,
				                              _dataspace.fpage(), L4_FP_ALL_SPACES);
				if (l4_error(r))
					std::cout << "Error unmapping dataspace? (" << l4_error(r) << ")\n";

				L4Re::Util::cap_alloc.free(_dataspace);
			}

		private:
			unsigned int             _refcount; /**< reference count */
			uint16_t                 _major;    /**< major number */
			uint16_t                 _minor;    /**< minor number */
			uint16_t                 _instance; /**< instance number */
			uint16_t                 _type;     /**< sensor type */
			uint32_t                 _elemsize; /**< size of a list element XXX -> subclass! */
			uint32_t                 _num_elem; /**< number of list elements XXX -> subclass! */
			L4::Cap<L4Re::Dataspace> _dataspace; /**< cap to sensor data space */

			L4::Cap<L4Re::Dataspace> get_dataspace(size_t bytes);
	};


	/* 
	 * The sensor registry is used to manage and lookup sensors. It is not a
	 * L4Re IPC server registry, because we do not expose sensor objects to
	 * clients directly (XXX maybe later?).
	 */
	class SensorRegistry
	{
		public:
			SensorRegistry();
			void register_sensor(Sensor *r);
			void unregister_sensor(Sensor *r);
			Sensor *lookup(uint16_t const major, uint16_t const minor, uint16_t const instance) const;

		private:
			std::list<Sensor*> _sensors;
//			std::map<int, Sensor*> _map;
	};

	/**
	 * Server implementation. The sensor directory manages clients (event producers)
	 * and monitors (event consumers).
	 */
	class SensorDir : public L4::Server_object_t<Ding>
	{
		public:
			SensorDir(void);

			/**
			 * Dispatch message
			 */
			int dispatch(l4_umword_t, L4::Ipc::Iostream &ios);

			/**
			 * Dispatch monitor messages.
			 */
			int dispatch_monitor(L4::Ipc::Iostream &ios);

			/**
			 * Dispatch client messages.
			 */
			int dispatch_client(L4::Ipc::Iostream &ios);

			/**
			 * Start the sensor dir server 
			 */
			void start();

			char const * name() const { return "sensordir"; }

		private:
			L4Re::Util::Registry_server<> _server;
			SensorRegistry _reg;
	};

}

std::ostream& operator<<(std::ostream& os, Ferret::Sensor const & obj);
