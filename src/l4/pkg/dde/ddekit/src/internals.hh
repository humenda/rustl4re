/*
 * This file is part of DDEKit.
 *
 * (c) 2006-2012 Bjoern Doebel <doebel@os.inf.tu-dresden.de>
 *               Christian Helmuth <ch12@os.inf.tu-dresden.de>
 *               Thomas Friebel <tf13@os.inf.tu-dresden.de>
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#pragma once

#include <l4/sys/consts.h>

namespace DDEKit
{
	class DDEKitObject
	{
		public:
			void * operator new(size_t cbSize)  throw ()
			{
				void *r = ddekit_simple_malloc(cbSize);
				return r;
			}

			void * operator new[](size_t cbSize)  throw ()
			{
				void *r = ddekit_simple_malloc(cbSize);
				return r;
			}

			void operator delete(void *ptr) throw()
			{ ddekit_simple_free(ptr); }

			void operator delete[](void *ptr) throw()
			{ ddekit_simple_free(ptr); }
	};
}

static inline void unpack_devfn(char devfn, unsigned *dev, unsigned *fn)
{
	Assert(dev && fn);

	*dev = devfn >> 3;
	*fn  = devfn & 0x3;
}

#if 0
struct ddekit_pci_res : public DDEKit::DDEKitObject
{
	public:
		explicit ddekit_pci_res(l4io_resource_t *res)
			: _io_res(*res)
		{ }

		l4_addr_t start() const { return _io_res.start; }
		l4_addr_t end() const { return _io_res.end; }
		unsigned char type() const { return _io_res.type; }

	private:
		ddekit_pci_res(ddekit_pci_res const &p __attribute__((unused))) { }
		ddekit_pci_res& operator =(ddekit_pci_res const &) { return *this; }
		l4io_resource_t _io_res;
};

struct ddekit_pci_dev : public DDEKit::DDEKitObject
{
	public:
		ddekit_pci_dev(l4io_pci_dev_t *);
		~ddekit_pci_dev();

		void enable() const;
		void disable() const;
		void set_master() const;

		static const unsigned RES_MAX;

		// XXX: operator[]
		ddekit_pci_res const * resource(unsigned idx) const;
		unsigned short vendor() const     { return _dev.vendor; }
		unsigned short device() const     { return _dev.device; }
		unsigned short sub_vendor() const { return _dev.sub_vendor; }
		unsigned short sub_device() const { return _dev.sub_device; }
		unsigned dev_class() const        { return _dev.dev_class; }
//		unsigned long irq() const         { return _irq; }
		char const * name() const   { return _dev.name; }
		char const * slot_name() const  { return _slot_name; }
		unsigned char header_type() const { return _dev.header_type; }
		unsigned char bus() const { return _dev.bus; }
		unsigned char devfn() const { return _dev.devfn; }
		unsigned num_res() const { return _dev.num_resources; }

		unsigned char read_cfg_byte(unsigned pos) const;
		unsigned short read_cfg_word(unsigned pos) const;
		unsigned read_cfg_dword(unsigned pos) const;
		bool write_cfg_byte(unsigned pos, unsigned char val) const;
		bool write_cfg_word(unsigned pos, unsigned short val) const;
		bool write_cfg_dword(unsigned pos, unsigned val) const;

		void add_res(ddekit_pci_res *r);

	private:
		ddekit_pci_dev(ddekit_pci_dev const &) { }
		ddekit_pci_dev& operator=(ddekit_pci_dev const &) { return *this; }
		unsigned char emulate_config_byte(unsigned pos) const;
		unsigned short emulate_config_word(unsigned pos) const;
		unsigned emulate_config_dword(unsigned pos) const;

		l4io_pci_dev_t _dev;
		l4_cap_idx_t   _cap;
		ddekit_pci_res ** _resources;
		char const *_slot_name;
};
#endif

namespace DDEKit
{
	class Pci_bus : public DDEKitObject
	{
		public:
			explicit Pci_bus();
			~Pci_bus();

			static const unsigned MAX_DEVS;

			// XXX: operator[]
			ddekit_pci_dev const * get(unsigned idx) const;
			ddekit_pci_dev const * find(ddekit_pci_dev const * const start = 0,
									   unsigned short bus = ~0,
			                           unsigned short slot = ~0,
			                           unsigned short func = ~0) const;

		private:
			Pci_bus(Pci_bus const &) { }
			Pci_bus& operator=(Pci_bus const &) { return *this; }
#if 0
			ddekit_pci_dev ** _devices;

			void dump_devs() const
			{
				unsigned idx = 0;
				while (_devices[idx] != 0)
				{
					ddekit_pci_dev *d = _devices[idx];
					ddekit_printf("Device %p -> '%s'\n", d, d->name());
					ddekit_printf("Vendor %x, Device %x\n", d->vendor(), d->device());
					ddekit_printf("Resources: %d\n", d->num_res());
					for (unsigned i = 0; i < d->num_res(); ++i)
					{
						// print resources?
					}

					++idx;
				}
			}
#endif
	};
}
