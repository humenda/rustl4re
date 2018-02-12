/**
 * \file   ferret/lib/comm/comm.cc
 * \brief  Helper stuff for finding the sensor directory.
 *
 * \date   10/03/2009
 * \author Bjoern Doebel <doebel@tudos.org>
 */
/*
 * (c) 2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdlib.h>

#include <l4/ferret/types.h>
#include <l4/ferret/comm.h>
#include <l4/re/c/util/cap_alloc.h>
#include <l4/re/c/namespace.h>
#include <iostream>          // std::cout

l4_cap_idx_t lookup_sensordir(void)
{
	l4_cap_idx_t srv = l4re_env_get_cap("ferret");
	if (l4_is_invalid_cap(srv))
		std::cout << "Ups...\n";
	else
		std::cout << "Found 'ferret' in ns.\n";

	return srv;
}
