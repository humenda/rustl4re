#include "device"
#include <cstring>
#include <cstdio>
#include <iostream>
#include <boost/format.hpp>

void Ankh::Device_manager::add_device(void *ptr)
{
//	std::cout << "Add device @ " << ptr << "\n";
	_devices.push_back(new Device(ptr));
}


void Ankh::Device_manager::dump_devices()
{
	for (std::list<Device*>::iterator cit = _devices.begin();
		 cit != _devices.end(); ++cit)
	{
		char macbuf_size = 32;
		char mac_buf[macbuf_size];
		char *mac = (*cit)->hw_addr();
		snprintf(mac_buf, macbuf_size, mac_fmt, mac_str(mac));

		/*
		 * Boost::Format strings are similar to printf.
		 * %<position>$<flags>
		 */
		boost::format fmt("%1$-5s IRQ: 0x%2$02X  MAC: %3$-18s MTU: %4$5d");
		fmt % (*cit)->name()
		    % (*cit)->irq()
		    % &mac_buf[0]
		    % (*cit)->mtu();
		std::cout << fmt << "\n";
	}
}


Ankh::Device *Ankh::Device_manager::find_device_by_name(char const * const name,
                                                        bool verbose)
{
	for(std::list<Device *>::iterator it = _devices.begin();
		it != _devices.end(); ++it)
	{
#if 0
		if (verbose)
			std::cout << (*it)->name() << " <-> " << name << "\n";
#endif
		if (!strcmp((*it)->name(), name))
			return *it;
	}
	
	return 0;
}
