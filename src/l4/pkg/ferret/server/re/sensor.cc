/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/ferret/types.h>
#include <l4/ferret/sensors/list.h>
#include <l4/cxx/ipc_server>          // L4::Server*
#include <iostream>

#include "sensor.h"

void * operator new(size_t size) throw(std::bad_alloc)
{
	return malloc(size);
}


void * operator new[](size_t size) throw(std::bad_alloc)
{
	return malloc(size);
}


void operator delete(void *p) throw()
{
	free(p);
}


void operator delete[](void *p) throw()
{
	free(p);
}


/******************************************************************************
 *                                                                            *
 * Sensor                                                                     *
 *                                                                            *
 ******************************************************************************/
Ferret::Sensor::Sensor(uint16_t const major, uint16_t const minor,
                       uint16_t const instance, uint16_t const type,
					   uint32_t e_size = 0, uint32_t e_num = 0)
	: _refcount(0), _major(major), _minor(minor), _instance(instance),
      _type(type), _elemsize(e_size), _num_elem(e_num)
{
	unsigned space_req = 0;

	switch(type)
	{
		case FERRET_SCALAR:
			space_req = sizeof(ferret_scalar_t);
			break;
		case FERRET_LIST:
			space_req = _num_elem * sizeof(ferret_list_index_t);
			space_req += sizeof(ferret_list_t);
			space_req += FERRET_ASSUMED_CACHE_LINE_SIZE - 1;
			space_req -= space_req % FERRET_ASSUMED_CACHE_LINE_SIZE;
			space_req += _num_elem * _elemsize;
			break;
	}

	_dataspace = get_dataspace(space_req);
}


L4::Cap<L4Re::Dataspace> Ferret::Sensor::get_dataspace(size_t const bytes)
{
    L4::Cap<L4Re::Dataspace> ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
	if (!ds.is_valid())
		std::cout << "Dataspace allocation failed.\n";

	int err =  L4Re::Env::env()->mem_alloc()->alloc(bytes, ds, 0);
	if (err < 0) {
		std::cout << "mem_alloc->alloc() failed.\n";
		L4Re::Util::cap_alloc.free(ds);
	}

	return ds;
}


std::ostream& operator<<(std::ostream& os, Ferret::Sensor const & obj)
{
	os << "(" << obj.major() << "," << obj.minor()
	   << "," << obj.instance() << ")";
	switch(obj.type())
	{
		case FERRET_SCALAR:
			os << " scalar";
			break;
		case FERRET_HISTO:
			os << " histo";
			break;
		case FERRET_HISTO_2D:
			os << " histo2D";
			break;
		case FERRET_TBUF:
			os << " tbuf";
			break;
		case FERRET_LIST:
			os << " list";
			break;
		default:
			break;
	}
	return os;
}


/******************************************************************************
 *                                                                            *
 * SensorRegistry                                                             *
 *                                                                            *
 ******************************************************************************/
Ferret::SensorRegistry::SensorRegistry()
	: _sensors()
{
}


void Ferret::SensorRegistry::register_sensor(Ferret::Sensor *s)
{
#if DEBUG
	std::cout << "adding " << *s << "\n";
#endif
	_sensors.push_back(s);
}


void Ferret::SensorRegistry::unregister_sensor(Ferret::Sensor *s)
{
	for (std::list<Sensor*>::iterator i = _sensors.begin();
		 i != _sensors.end(); ++i)
	{
		if ((s->major() == (*i)->major()) &&
			(s->minor() == (*i)->minor()) &&
			(s->instance() == (*i)->instance())) {
			_sensors.erase(i);
			break;
		}
	}
}


Ferret::Sensor *Ferret::SensorRegistry::lookup(uint16_t const major,
                                               uint16_t const minor,
                                               uint16_t const instance) const
{
	for (std::list<Sensor*>::const_iterator i = _sensors.begin();
		 i != _sensors.end(); ++i)
	{
		if ((major == (*i)->major()) &&
			(minor == (*i)->minor()) &&
			(instance == (*i)->instance()))
			return *i;
	}

	return 0;
}


