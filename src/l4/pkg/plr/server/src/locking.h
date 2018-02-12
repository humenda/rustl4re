// vi: ft=cpp
/*
 * locking.h --
 *
 *     Global locking stuff
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once
#include "memory"
#include "log"

namespace Romain {
struct Rm_guard
{
	Romain::Region_map *map;
	Rm_guard(Romain::Region_map *m, unsigned id)
		: map(m)
	{
		map->activate(id); }

	~Rm_guard()
	{
		map->release();
	}
};
}
