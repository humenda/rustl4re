/*
 * main.cc --
 * 
 *     Main program entry point.
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <cstdio>
#include <cstring>

#include "exceptions"
#include "log"
#include "manager"

#include <l4/re/l4aux.h>
#include <l4/util/util.h>
#include <l4/sys/debugger.h>

using L4Re::chksys;
using L4Re::chkcap;

Romain::Log::LogLevel Romain::Log::maxLog = Romain::Log::INFO;
l4_umword_t Romain::Log::logFlags         = Romain::Log::None;
bool Romain::Log::withtime                = false;
bool Romain::Log::logLocalTSC             = false;
bool Romain::Log::replicaLogBuf           = false;

namespace Romain {
	l4re_aux_t* l4re_aux;
}

static void setup_aux(l4_mword_t argc, const char **argv)
{
	l4_umword_t *auxp = (l4_umword_t*)&argv[argc] + 1;
	while (*auxp) {
		++auxp;
	}
	++auxp;
	Romain::l4re_aux = 0;
	while (*auxp) {
		if (*auxp == 0xf0) {
			Romain::l4re_aux = (l4re_aux_t*)auxp[1];
		}
		auxp += 2;
	}
}

namespace Romain {
	Romain::InstanceManager *_the_instance_manager = NULL;
	Measurements::EventBuf  *globalLogBuf          = NULL;
}

static int _main(l4_mword_t argc, char const **argv)
{
	(void)argc; (void)argv;
	INFO() << "FILE: " << argv[1];

	sleep(2);

	struct timeval tv;
	gettimeofday(&tv, NULL);
	srandom(tv.tv_usec);

	setup_aux(argc, argv);

	Romain::_the_instance_manager = new Romain::InstanceManager(argc-1, &argv[1]);
	DEBUG() << "creating instances";
	Romain::_the_instance_manager->create_instances();
	DEBUG() << "running instances";
	Romain::_the_instance_manager->run_instances();

	l4_sleep_forever();

	return 0;
}


static void sanity_check_cmdline(l4_mword_t argc, char const **argv)
{
	(void)argv;
	_check( argc < 2, "No file name given.");
}


int main(int argc, char const**argv)
{
	INFO() << std::hex << &argc;
	INFO() << "startup";
	l4_mword_t err = -1;
	try {
		DEBUG() << "argc " << argc;
		sanity_check_cmdline(argc, argv);
		err = _main(argc, argv);
	}
	catch (Argument_error &e) {
		std::cerr << "Argument error: " << e.str() << "\n";
	}
	catch (L4::Runtime_error& e) {
		std::cerr << "Uncaught exception: " << e.str() << "\n";
	}
	catch (...) {
		std::cerr << "FATAL uncaught exception of unknown type\n";
	}
	std::cerr << "Terminating.\n";
	return err;
}
