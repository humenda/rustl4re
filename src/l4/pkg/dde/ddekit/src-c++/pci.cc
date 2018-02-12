#include <l4/dde/ddekit/assert.h>
#include <l4/dde/ddekit/pci.h>
#include <l4/dde/ddekit/printf.h>
#include <l4/dde/ddekit/panic.h>
#include <l4/io/pci.h>
#include <l4/io/dev_set>
#include <l4/io/desc.h>
#include <l4/re/util/cap_alloc>
#include <l4/re/env>

#include <cstdlib>
#include <list>

#include "internal.h"

namespace DDEKit
{
	const int DDEKit::Pci_dev::RES_MAX = 12;
	const int DDEKit::Pci_res::ANY_ID  = 0;

	static inline void unpack_devfn(char devfn, unsigned *dev, unsigned *fn)
	{
		Assert(dev && fn);

		*dev = devfn >> 3;
		*fn  = devfn & 0x3;
	}

	class Pci_res_impl : public Pci_res, public DDEKitObject
	{
		public:
			explicit Pci_res_impl(l4io_resource_t *res)
				: _io_res(*res)
			{
			}

			virtual l4_addr_t start() const { return _io_res.start; }
			virtual l4_addr_t end() const { return _io_res.end; }
			virtual unsigned char type() const { return _io_res.type; }

		private:
			Pci_res_impl(Pci_res_impl const &p) { }
			Pci_res_impl& operator =(Pci_res_impl const &) { return *this; }
			l4io_resource_t _io_res;
	};


	class Pci_dev_impl : public Pci_dev, public DDEKitObject
	{
		public:
			Pci_dev_impl(l4io_pci_dev_t *);
			virtual ~Pci_dev_impl();

			virtual void enable() const;
			virtual void disable() const;
			virtual void set_master() const;

			// XXX: operator[]
			virtual Pci_res const * const resource(unsigned idx) const;
			virtual unsigned short vendor() const     { return _dev.vendor; }
			virtual unsigned short device() const     { return _dev.device; }
			virtual unsigned short sub_vendor() const { return _dev.sub_vendor; }
			virtual unsigned short sub_device() const { return _dev.sub_device; }
			virtual unsigned dev_class() const        { return _dev.dev_class; }
			virtual unsigned long irq() const         { return _irq; }
			virtual char const * const name() const   { return _dev.name; }
			virtual char const * const slot_name() const  { return _slot_name; }
			virtual unsigned char const bus() const { return _dev.bus; }
			virtual unsigned char const devfn() const { return _dev.devfn; }

			virtual unsigned char read_cfg_byte(unsigned pos) const;
			virtual unsigned short read_cfg_word(unsigned pos) const;
			virtual unsigned read_cfg_dword(unsigned pos) const;
			virtual bool write_cfg_byte(unsigned pos, unsigned char val) const;
			virtual bool write_cfg_word(unsigned pos, unsigned short val) const;
			virtual bool write_cfg_dword(unsigned pos, unsigned val) const;

		private:
			Pci_dev_impl(Pci_dev_impl const &) { }
			Pci_dev_impl& operator=(Pci_dev_impl const &) { return *this; }

			l4io_pci_dev_t _dev;
			unsigned long _irq;
			char const *_slot_name;
			Pci_res const* _resources[DDEKit::Pci_dev::RES_MAX];
	};


	class Pci_bus_impl : public Pci_bus, public DDEKitObject
	{
		public:
			explicit Pci_bus_impl();
			virtual ~Pci_bus_impl();

			// XXX: operator[]
			virtual Pci_dev const * const get(unsigned idx) const;
			virtual Pci_dev const * const find(Pci_dev const * const start = 0,
											   unsigned short bus = ~0,
			                                   unsigned short slot = ~0,
			                                   unsigned short func = ~0) const;

		private:
			Pci_bus_impl(Pci_bus_impl const &) { }
			Pci_bus_impl& operator=(Pci_bus_impl const &) { return *this; }
			std::list<Pci_dev_impl*> _devices;
	};
}


DDEKit::Pci_bus_impl::Pci_bus_impl()
{
	using DDEKit::Pci_dev;
	using DDEKit::Pci_dev_impl;

	DDEKit::printf("pci bus constructor\n");

	l4io_pci_dev_handle_t h = 0;

	do
	{
		/* dummy struct - pci_dev_t requires an array of resource_t
		 * to be layed out directly behind it, so l4io is able to store
		 * the device's resources in there.
		 */
		struct {
			l4io_pci_dev_t dev;
			l4io_resource_t res[Pci_dev::RES_MAX];
		} devdata;

		devdata.dev.num_resources = Pci_dev::RES_MAX;

		h = l4io_pci_find_any(h, &devdata.dev);
		if (h != 0)
		{
			DDEKit::printf("'%s'\n", devdata.dev.name);
			DDEKit::printf("\t%u resources\n", devdata.dev.num_resources);
			_devices.push_back(new Pci_dev_impl(&devdata.dev));
		}

	} while (h != 0);
}

DDEKit::Pci_bus_impl::~Pci_bus_impl()
{
	DDEKit::printf("pci bus destructor\n");
	unsigned cnt = 0;
	for (std::list<Pci_dev_impl*>::const_iterator i = _devices.begin();
		 i != _devices.end(); ++i, ++cnt)
		delete *i;
	DDEKit::printf("deleted %d devices.\n", cnt);
}

DDEKit::Pci_bus* DDEKit::Pci_bus::init()
{
	return new Pci_bus_impl();
}

DDEKit::Pci_dev const * const DDEKit::Pci_bus_impl::get(unsigned idx) const
{
	unsigned cnt = 0;

	if (idx < _devices.size())
	{
		for (std::list<Pci_dev_impl*>::const_iterator i = _devices.begin();
			 i != _devices.end(); ++i, ++cnt)
		{
			if (cnt == idx-1)
				return *i;
		}
	}

	return 0;
}

DDEKit::Pci_dev const * const DDEKit::Pci_bus_impl::find(DDEKit::Pci_dev const * const start,
														 unsigned short bus,
                                                         unsigned short slot,
                                                         unsigned short func) const
{
	std::list<Pci_dev_impl*>::const_iterator i = _devices.begin();

	// move to start pos if given
	if (start != 0)
	{
		while (*i != start)
			++i;
	}

	// one more step to begin searching
	++i;

	// find any?
	if (bus == (unsigned short)~0 && slot == (unsigned short)~0 && func == (unsigned short)~0)
		return *i;

	for ( ; i != _devices.end(); ++i)
	{
		unsigned char const dbus = (*i)->bus();
		unsigned dslot, dfunc;
		unpack_devfn((*i)->devfn(), &dslot, &dfunc);
		if (dbus == bus && dslot == slot && dfunc == func)
			return *i;
	}

	return 0;
}


DDEKit::Pci_dev_impl::Pci_dev_impl(l4io_pci_dev_t *dev)
	: _dev(*dev), _irq(0), _slot_name(0)
{
	std::memset(_resources, 0, sizeof(_resources[0]) * Pci_dev::RES_MAX);
	for (unsigned i = 0; i < dev->num_resources; ++i)
	{
		_resources[i] = new Pci_res_impl(&dev->resources[i]);
	}
}


DDEKit::Pci_dev_impl::~Pci_dev_impl()
{
	for (unsigned i = 0; i < _dev.num_resources; ++i)
		delete _resources[i];
	DDEKit::printf("deleted %d resources.\n", _dev.num_resources);
}

DDEKit::Pci_res const * const DDEKit::Pci_dev_impl::resource(unsigned idx) const
{
	if (idx < _dev.num_resources)
		return _resources[idx];

	return 0;
}

void DDEKit::Pci_dev_impl::enable() const
{
	if (l4io_pci_enable(_dev.handle))
		DDEKit::panic("%s failed\n", __FUNCTION__);
}


void DDEKit::Pci_dev_impl::disable()  const
{
	if (l4io_pci_disable(_dev.handle))
		DDEKit::panic("%s failed\n", __FUNCTION__);
}


void DDEKit::Pci_dev_impl::set_master() const
{
	if (l4io_pci_set_master(_dev.handle))
		DDEKit::panic("%s failed\n", __FUNCTION__);
}


unsigned char DDEKit::Pci_dev_impl::read_cfg_byte(unsigned pos) const
{
	unsigned char ret = 0xff;
	int err = l4io_pci_readb_cfg(_dev.handle, pos, &ret);
	if (err)
		DDEKit::panic("%s failed\n", __FUNCTION__);
	return ret;
}


unsigned short DDEKit::Pci_dev_impl::read_cfg_word(unsigned pos) const
{
	unsigned short ret = 0xffff;
	int err = l4io_pci_readw_cfg(_dev.handle, pos, &ret);
	if (err)
		DDEKit::panic("%s failed\n", __FUNCTION__);
	return ret;
}


unsigned DDEKit::Pci_dev_impl::read_cfg_dword(unsigned pos) const
{
	unsigned ret = 0UL;
	int err = l4io_pci_readl_cfg(_dev.handle, pos, &ret);
	if (err)
		DDEKit::panic("%s failed\n", __FUNCTION__);
	return ret;
}


bool DDEKit::Pci_dev_impl::write_cfg_byte(unsigned pos, unsigned char val) const
{
	int err = l4io_pci_writeb_cfg(_dev.handle, pos, val);
	if (err)
		DDEKit::panic("%s failed\n", __FUNCTION__);
	return true;
}


bool DDEKit::Pci_dev_impl::write_cfg_word(unsigned pos, unsigned short val) const
{
	int err = l4io_pci_writew_cfg(_dev.handle, pos, val);
	if (err)
		DDEKit::panic("%s failed\n", __FUNCTION__);
	return true;
}


bool DDEKit::Pci_dev_impl::write_cfg_dword(unsigned pos, unsigned val) const
{
	int err = l4io_pci_writel_cfg(_dev.handle, pos, val);
	if (err)
		DDEKit::panic("%s failed\n", __FUNCTION__);
	return true;
}
