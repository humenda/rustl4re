/*
 * \brief   DDEKit PCI types
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

namespace DDEKit
{
	class Pci_res
	{
		public:
			static const int ANY_ID;

			explicit Pci_res() { }
			virtual ~Pci_res() { }

			virtual l4_addr_t start() const = 0;
			virtual l4_addr_t end() const = 0;
			virtual unsigned char type() const = 0;

		private:
			Pci_res(Pci_res const &p) { }
			Pci_res& operator =(Pci_res const &) { return *this; }
	};


	class Pci_dev
	{
		public:
			static const int RES_MAX;

			explicit Pci_dev() { }
			virtual ~Pci_dev() { }

			virtual void enable() const = 0;
			virtual void disable() const = 0;
			virtual void set_master() const = 0;
			// XXX: operator[]
			virtual Pci_res const * const resource(unsigned idx) const = 0;
			virtual unsigned short vendor() const = 0;
			virtual unsigned short device() const = 0;
			virtual unsigned short sub_vendor() const = 0;
			virtual unsigned short sub_device() const = 0;
			virtual unsigned dev_class() const = 0;
			virtual unsigned long irq() const = 0;
			virtual char const * const name() const = 0;
			virtual char const * const slot_name() const = 0;
			virtual unsigned char const devfn() const = 0;
			virtual unsigned char const bus() const = 0;
	
			// I'd love to use a template here, but this does not
			// seem to be possible without incorporating L4Io headers.
			virtual unsigned char read_cfg_byte(unsigned pos) const = 0;
			virtual unsigned short read_cfg_word(unsigned pos) const = 0;
			virtual unsigned read_cfg_dword(unsigned pos) const = 0;
			virtual bool write_cfg_byte(unsigned pos, unsigned char val) const = 0;
			virtual bool write_cfg_word(unsigned pos, unsigned short val) const = 0;
			virtual bool write_cfg_dword(unsigned pos, unsigned val) const = 0;

		private:
			Pci_dev(Pci_dev const &) { }
			Pci_dev& operator =(Pci_dev const &) { return *this; }
	};


	class Pci_bus
	{
		public:
			explicit Pci_bus() { }
			virtual ~Pci_bus() { }
			static Pci_bus *init();

			// XXX: operator[]
			virtual Pci_dev const * const get(unsigned idx) const = 0;
			virtual Pci_dev const * const find(Pci_dev const * const start = 0,
											   unsigned short bus = ~0,
			                                   unsigned short slot = ~0,
			                                   unsigned short func = ~0) const = 0;
		private:
			Pci_bus(Pci_bus const &) { }
			Pci_bus& operator =(Pci_bus const &) { return *this; }
	};
}

