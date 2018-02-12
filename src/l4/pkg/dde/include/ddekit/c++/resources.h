/*
 * \brief   DDEKit I/O resource accessors
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

#include <l4/sys/types.h>
#include <l4/sys/compiler.h>

namespace DDEKit
{
class ResourceManager
{
	public:
		explicit ResourceManager() { }
		virtual ~ResourceManager() { }

		virtual bool request_ioports(l4_addr_t start, l4_addr_t count) = 0;
		virtual bool release_ioports(l4_addr_t start, l4_addr_t count) = 0;
		virtual bool request_iomem(l4_addr_t start, l4_addr_t size, l4_addr_t *vaddr) = 0;
		virtual bool release_iomem(l4_addr_t start, l4_addr_t size) = 0;
		virtual bool irq_attach(int irq, bool shared, void (*thread_init)(void*),
		                        void (*handler)(void *), void *data) = 0;
		virtual bool irq_detach(int irq) = 0;
		virtual bool irq_disable(int irq) = 0;
		virtual bool irq_enable(int irq) = 0;

		static inline ResourceManager *resman();

	private:
		ResourceManager(ResourceManager const &rm) { }
		ResourceManager& operator=(ResourceManager const &) { return *this; }
};

}
