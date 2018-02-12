/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/ferret/types.h>
#include <iostream>          // std::cout
#include <l4/util/backtrace.h>        // l4util_backtrace()

#include "sensor.h"

int main(/*int argc, char **argv*/)
{
	Ferret::SensorDir dir;

	dir.start();

	std::cout << "Server loop exited!\n";

	std::cout << "===== Backtrace =====\n";
	int len = 16;
	void *array[len];
	int r = l4util_backtrace(&array[0], len);
	for (int i=0; i < r; ++i)
		std::cout << "\t" << array[i] << "\n";
	std::cout << "=====================\n";

	return 0;
}
