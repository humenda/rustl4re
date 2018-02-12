#include <iostream>

#include <l4/util/util.h>
#include <l4/util/rdtsc.h>

#include "constants.h"
#include <l4/plr/measurements.h>

int main()
{
	std::cout << "starting Hello (C++ version)" << std::endl;

	Measurements::EventBuf *eb = reinterpret_cast<Measurements::EventBuf*>(Romain::REPLICA_LOG_ADDRESS);
	std::cout << "evbuf @ " << std::hex << eb << " " << eb->index << " " << eb->size << std::endl;
	
	for (unsigned i = 0; i < 5; ++i) {
		std::cout << "creating event ... " << &eb->index << std::endl;
		Measurements::GenericEvent* ev = eb->next();
		std::cout << "  next @ " << std::hex << ev << std::endl;
		ev->header.tsc  = l4_rdtsc();
		ev->header.vcpu = 0x1234;
		ev->header.type = Measurements::Foo;
		ev->data.foo.start = true;
		
		std::cout << "HELLO" << std::endl;

		ev = eb->next();
		ev->header.tsc  = l4_rdtsc();
		ev->header.vcpu = 0x1234;
		ev->header.type = Measurements::Foo;
		ev->data.foo.start = false;

		l4_sleep(1000);
	}

	return 0;
}
