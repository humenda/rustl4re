/*
 * \brief   Memory region management
 * \author  Bjoern Doebel <doebel@tudos.org>
 * \date    2008-09-21
 */

/*
 * (c) 2008 Technische Universität Dresden
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING file for details.
 */


#pragma once

#include <l4/sys/compiler.h>
#include <l4/sys/types.h>

namespace DDEKit
{
	class RegionManager
	{
		public:
			explicit RegionManager() { }
			virtual ~RegionManager() { }

			virtual void add(l4_addr_t virt, l4_addr_t phys, long pages) = 0;
			virtual void remove(l4_addr_t virt) = 0;
			virtual l4_addr_t virt_to_phys(l4_addr_t const virt) const = 0;
			virtual l4_addr_t phys_to_virt(l4_addr_t const pyhs) const = 0;

			static RegionManager *create();

		private:
			RegionManager(RegionManager const &) { }
			RegionManager &operator=(RegionManager const &) { return *this; }
	};

	inline RegionManager *rm();
}

