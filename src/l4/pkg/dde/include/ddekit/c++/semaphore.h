/*
 * \brief   Semaphore primitives
 * \author  Thomas Friebel <tf13@os.inf.tu-dresden.de>
 * \author  Christian Helmuth <ch12@os.inf.tu-dresden.de>
 * \author  Bjoern Doebel<doebel@os.inf.tu-dresden.de>
 * \date    2008-08-26
 */

/*
 * (c) 2006-2008 Technische Universität Dresden
 * This file is part of TUD:OS, which is distributed under the terms of the
 * GNU General Public License 2. Please see the COPYING file for details.
 */

#pragma once

/** \defgroup DDEKit_synchronization */

#include <l4/sys/compiler.h>

namespace DDEKit
{
	class Semaphore
	{
		public:
			explicit Semaphore() { }
			virtual ~Semaphore() { }
			static Semaphore* create(int const value);

			virtual bool down(int const timeout = -1) = 0;
			virtual bool trydown(void) = 0;
			virtual void up(void) = 0;

		private:
			Semaphore& operator=(const Semaphore&) { return *this; }
			Semaphore(const Semaphore&) { }
	};
}
