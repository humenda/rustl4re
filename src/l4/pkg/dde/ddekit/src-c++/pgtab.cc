#include <l4/dde/ddekit/pgtab.h>
#include <l4/dde/ddekit/memory.h>
#include <l4/dde/ddekit/printf.h>
#include <list>

#include "internal.h"

namespace DDEKit
{
	class RegionManager_impl : public RegionManager, public DDEKitObject
	{
			public:
				explicit RegionManager_impl();
				virtual ~RegionManager_impl();

				virtual void add(l4_addr_t virt, l4_addr_t phys, long pages);
				virtual void remove(l4_addr_t virt);
				virtual l4_addr_t virt_to_phys(l4_addr_t virt) const;
				virtual l4_addr_t phys_to_virt(l4_addr_t pyhs) const;

			private:
				struct Region
				{
					l4_addr_t virt;
					l4_addr_t phys;
					long size;

					Region(l4_addr_t start, l4_size_t sz, l4_addr_t p = 0)
						: virt(start), phys(p), size(sz)
					{ }
				};

				std::list<Region> _l;
				std::list<Region>::const_iterator vlookup(l4_addr_t virt) const;
				std::list<Region>::const_iterator plookup(l4_addr_t virt) const;

				RegionManager_impl(RegionManager_impl const &) { }
				RegionManager_impl &operator=(RegionManager_impl const &) { return *this; }
	};
}

DDEKit::RegionManager *DDEKit::RegionManager::create()
{
	return new DDEKit::RegionManager_impl();
}


DDEKit::RegionManager_impl::RegionManager_impl()
{
}

DDEKit::RegionManager_impl::~RegionManager_impl()
{
}


std::list<DDEKit::RegionManager_impl::Region>::const_iterator
DDEKit::RegionManager_impl::vlookup(l4_addr_t virt) const
{
	// we return an iterator here, so the result of lookup()
	// may also be used as input to remove
	using namespace DDEKit;

	std::list<RegionManager_impl::Region>::const_iterator i = _l.begin();
	for ( ; i != _l.end(); ++i)
	{
		RegionManager_impl::Region const &r = *i;
		if (r.virt <= virt && virt < r.virt + r.size)
			return i;
	}

	return _l.end();
}


std::list<DDEKit::RegionManager_impl::Region>::const_iterator
DDEKit::RegionManager_impl::plookup(l4_addr_t phys) const
{
	// we return an iterator here, so the result of lookup()
	// may also be used as input to remove
	using namespace DDEKit;

	std::list<RegionManager_impl::Region>::const_iterator i = _l.begin();
	for ( ; i != _l.end(); ++i)
	{
		RegionManager_impl::Region const &r = *i;
		if (r.phys <= phys && phys < r.phys + r.size)
			return i;
	}

	return _l.end();
}


void DDEKit::RegionManager_impl::add(l4_addr_t virt, l4_addr_t phys, long pages)
{
	_l.push_back(RegionManager_impl::Region(virt, pages * L4_PAGESIZE, phys));
}

void DDEKit::RegionManager_impl::remove(l4_addr_t virt)
{
	std::list<DDEKit::RegionManager_impl::Region>::const_iterator i = vlookup(virt);
	if (i != _l.end())
		_l.erase(i);
	else
		DDEKit::printf("%s: lookup failed, removing nothing.\n", __FUNCTION__);
}


l4_addr_t DDEKit::RegionManager_impl::virt_to_phys(l4_addr_t const virt) const
{
	std::list<DDEKit::RegionManager_impl::Region>::const_iterator i = vlookup(virt);
	l4_addr_t offset = virt - (*i).virt;
	return (*i).phys + offset;
}


l4_addr_t DDEKit::RegionManager_impl::phys_to_virt(l4_addr_t const phys) const
{
	std::list<DDEKit::RegionManager_impl::Region>::const_iterator i = plookup(phys);
	l4_addr_t offset = phys - (*i).phys;
	return (*i).virt + offset;
}

