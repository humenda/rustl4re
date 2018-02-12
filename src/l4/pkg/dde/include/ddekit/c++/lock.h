/*
 * \brief   
 * \author  Thomas Friebel <tf13@os.inf.tu-dresden.de>
 * \author  Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \author  Bjoern Doebel<doebel@os.inf.tu-dresden.de>
 * \date    2008-08-26
 * 
 * DDEKit lock header
 */

/*
 * (c) 2006-2008 Technische Universität Dresden
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING file for details.
 */

#pragma once

#include <l4/sys/compiler.h>

namespace DDEKit
{
	class Lock
	{
		public:
			explicit Lock() { }
			virtual ~Lock() { };
			virtual void lock(void) const = 0;
			virtual void unlock(void) const = 0;
			virtual bool trylock(void) const = 0;

			static DDEKit::Lock *create(const bool init_locked=false);

		private:
			Lock(Lock&) { }
	};
}

