/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "sensor.h"
#include <iostream>
#include <l4/re/error_helper>

/******************************************************************************
 *                                                                            *
 * SensorDir                                                                  *
 *                                                                            *
 ******************************************************************************/
Ferret::SensorDir::SensorDir(void)
	: _server(), _reg()
{
        L4Re::chkcap(_server.registry()->register_obj(this, name()), "registering seonsordir", 0);
}


void Ferret::SensorDir::start()
{
	std::cout << "Server loop\n";
	_server.loop();
}


int Ferret::SensorDir::dispatch(l4_umword_t, L4::Ipc::Iostream &ios)
{
	l4_msgtag_t tag;
	ios >> tag;

	/* 
	 * Label sanity check
	 */
	if ((tag.label() != Client::Protocol) && (tag.label() != Monitor::Protocol))
		return L4_EBADPROTO;

	/*
	 * Redirect to corresponding dispatch fn.
	 */
	switch (tag.label())
	{
		case Client::Protocol:
			return dispatch_client(ios);
			break;
		case Monitor::Protocol:
			return dispatch_monitor(ios);
			break;
		default:
			return -L4_EINVAL;
	}
}


int Ferret::SensorDir::dispatch_monitor(L4::Ipc::Iostream &ios)
{
	L4::Opcode opcode;
	int ret = -L4_ENOSYS;
	uint16_t maj, min, inst;
	Sensor *s;

	ios >> opcode;
	ios >> maj >> min >> inst;

#if DEBUG
	std::cout << "(" << maj << ", " << min << ", " << inst << ")\n";
#endif
	switch(opcode)
	{
		case Monitor::Attach:
			s = _reg.lookup(maj, min, inst);
			if (s) {
				std::cout << "Found " << *s << "\n";
				s->inc();
				unsigned flags = L4_FPAGE_RO;
				ios << 0 << L4::Ipc::Snd_fpage(s->ds_cap().fpage(flags));
				ret = 0;
			}
			else {
				std::cout << "Could not find sensor for (" << maj
				         << "," << min << "," << inst << ")\n";
				ios << -L4_ENOENT;
				ret = -L4_ENOENT;
			}
			break;
		case Monitor::Detach:
			s = _reg.lookup(maj, min, inst);
			if (s) {
				s->dec();
				if (s->refcount() == 0)	{
					_reg.unregister_sensor(s);
					delete s;
				}
				ios << 0;
				ret = 0;
			}
			else {
				std::cout << "Could not find sensor for (" << maj
				         << "," << min << "," << inst << ")\n";
				ios << -L4_ENOENT;
				ret = -L4_ENOENT;
			}
			break;
		case Monitor::List:
			std::cout << "list\n";
			ios << ret;
			break;
	}

	return ret;
}


int Ferret::SensorDir::dispatch_client(L4::Ipc::Iostream &ios)
{
	L4::Opcode opcode;
	uint16_t maj, min, inst, typ;
	uint32_t flags;
	Sensor *sensor = 0;

	ios >> opcode;
	ios >> maj >> min >> inst >> typ >> flags;

#if DEBUG
	std::cout << "(" << maj << "," << min << "," << inst << ")\n";
	std::cout << typ << ", " << flags << "\n";
#endif

	int ret = -L4_ENOSYS;
	switch(opcode)
	{
		case Client::Create:
			{
				unsigned e_size = 0, e_num = 0;
				switch(typ)
				{
					case FERRET_LIST:
						ios >> e_size >> e_num;
#if DEBUG
						std::cout << e_num << " elements of size " << e_size << "\n";
#endif
					default:
						break;
				}
				sensor = _reg.lookup(maj, min, inst);
				// is this a re-open?
				if (sensor)	{
					if (sensor->type() != typ) {
						std::cout << "type mismatch!\n";
						ios << -L4_EEXIST;
						sensor = 0;
					}
					else if (typ == FERRET_LIST && (sensor->elem_size() != e_size ||
							 sensor->num_elem()  != e_num)) {
						std::cout << "DS size mismatch.\n";
						ios << -L4_EEXIST;
						sensor = 0;
					}
					else {
						// reopen ok, inc use count
						sensor->inc();
					}
				}
				else {
					switch(typ)
					{
						case FERRET_SCALAR:
							// fall through
						case FERRET_LIST:
							sensor = new Sensor(maj, min, inst, typ, e_size, e_num);
							break;
						default:
							break;
					}

					if (sensor)	{
						_reg.register_sensor(sensor);
						sensor->inc();
					}
				}

#if DEBUG
				if (sensor) {
					std::cout << *sensor << "\n";
					std::cout << "sensor ds size: " << sensor->ds_cap()->size() << "\n";
					std::cout << "sensor cap: " << sensor->ds_cap().fpage().raw << "\n";
				}
				else
					std::cout << "Invalid sensor request.\n";
#endif

				if (sensor) {
					/*
					 * ATTENTION! For mapping a data space writable, the
					 *   'X' bit in the fpage flags needs to be set. Reason:
					 *   those bits are overloaded for capabilities and have
					 *   a meaning different from the original one:
					 *
					 *   R - needs to be set anyway, otherwise nothing is mapped
					 *   W - 'isolate' bit. If set, the cap can potentially be used
					 *       to map other writable objects through it.
					 *   X - 'user-defined' bit that depends on the kind of object
					 *       this cap refers to. For data spaces, it means writable.
					 */
					unsigned flags = L4_FPAGE_RWX;
					ios << 0 << L4::Ipc::Snd_fpage(sensor->ds_cap().fpage(flags));
				}
			}

			ret = 0;
			break;

		case Client::Free:
			sensor = _reg.lookup(maj, min, inst);
			sensor->dec();
			if (sensor->refcount() == 0) {
				_reg.unregister_sensor(sensor);
				delete sensor;
			}

			ios << 0;

			break;

		case Client::NewInstance:
			// XXX create a new instance of a sensor
			break;
		default:
			break;
	}

	return ret;
}
