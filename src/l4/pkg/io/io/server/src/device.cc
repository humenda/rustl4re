/*
 * (c) 2011 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include "device.h"
#include "debug.h"

#include <typeinfo>
#include <cassert>
#include <set>

std::string
Generic_device::get_full_path() const
{
  if (!parent())
    return std::string("/") + name();

  return parent()->get_full_path() + "/" + name();
}


// The resource setup works in two phases:
//
// (1) The REQUEST phase tries to map preconfigured child resources into
//     provided resources of their parents.
// (2) Child resources which are not yet configured are configured in the
//     ALLOCATE phase.
//
// The rationale behind this strategy is to try hard to not change any setup
// done by previous instances in the boot chain (BIOS, EFI, firmware, etc).
// Changing such an existing setup can lead to a hanging system if firmware
// (e.g. System Management Mode on x86 systems) continues to use the old setup.

bool
Device::alloc_child_resource(Resource *r, Device *cld)
{
  bool found_as = false;
  // The first run requires exact match between client and parent resource.
  // The second run allows to assign a prefetchable MMIO client region to a
  // non-prefetchable MMIO parent resource.
  bool exact = true;
  auto const *rl = resources();
  while (true)
    {
      for (auto *br: *rl)
        {
          if (!br)
            continue;

          if (br->disabled())
            continue;

          if (!br->provided())
            continue;

          if (!br->compatible(r, exact))
            continue;

          found_as = true;

          if (parent() && !parent()->resource_allocated(br))
            {
              br->provided()->assign(br, r);
              d_printf(DBG_ALL, "assigned resource: ");
              if (dlevel(DBG_ALL))
                r->dump();

              return true;
            }
          else if (br->provided()->alloc(br, this, r, cld, false))
            {
              r->enable();
              d_printf(DBG_ALL, "allocated resource: ");
              if (dlevel(DBG_ALL))
                r->dump();

              return true;
            }
        }

      if (exact)
        exact = false;
      else
        {
          if (!found_as && parent())
            return parent()->alloc_child_resource(r, cld);

          d_printf(DBG_ERR, "ERROR: could not reserve resource\n");
          if (dlevel(DBG_ERR))
            r->dump();

          r->disable();
          return false;
        }
    }
}


void
Device::request_resource(Resource *r)
{
  Device *p = parent();

  // Are we the root device?
  if (!p)
    return;

  if (r->empty())
    return;

  if (p->resource_allocated(r))
    return; // already allocated

  if (r->disabled())
    return;

  if (!p->request_child_resource(r, this) && r->fixed_addr())
    {
      d_printf(DBG_WARN, "warning: inconsistent fixed resource @ device: %s\n",
               get_full_path().c_str());
      if (dlevel(DBG_WARN))
        {
          dump(2);
          r->dump(2);
        }
    }
}

void
Device::request_resources()
{
  Resource_list const *rl = resources();

  // Are we the root device?
  if (!parent())
    return;

  for (Resource_list::const_iterator r = rl->begin();
      r != rl->end(); ++r)
    if (*r)
      request_resource(*r);
}


void
Device::request_child_resources()
{
  for (iterator dev = begin(0); dev != end(); ++dev)
    {
      // First, try to map all our resources of our child (dev) into
      // provided resources of ourselves
      (*dev)->request_resources();

      // Second, recurse down to our child (dev)
      (*dev)->request_child_resources();
    }
}


// sorted set of (resource, device) pairs
namespace {

struct Res_dev
{
  Resource *r;
  Device *d;

  Res_dev() = default;
  Res_dev(Resource *r, Device *d) : r(r), d(d) {}
};

static bool res_cmp(Res_dev const &l, Res_dev const &r)
{ return l.r->alignment() > r.r->alignment(); }

typedef std::multiset<Res_dev, bool (*)(Res_dev const &l, Res_dev const &r)> UAD;


static bool _allocate_pending_resources(Device *dev, UAD *to_allocate)
{
  Device *p = dev->parent();
  assert (p);
  auto const *rl = dev->resources();
  for (auto *r: *rl)
    {
      if (!r)
        continue;

      if (r->empty())
        continue;

      if (p->resource_allocated(r))
        continue;

      if (r->fixed_addr())
        continue;

      if (0)
        {
          printf("unallocated resource: %s ", typeid(*r).name());
          r->dump(0);
        }

      to_allocate->insert(Res_dev(r, dev));
    }
  return true;
}

}

/**
 * Allocate all unallocated child resources.
 *
 * The allocation of a resource includes its relocation so that PCI device
 * resources are located inside the respective resource window of their PCI
 * bridge. Allocated resources are fixed until the device is removed.
 *
 * It is not possible for IO clients to perform a resource relocation on a
 * physical device as physical devices are accessible by clients only through
 * respective proxy devices. The proxy device for PCI devices emulates accesses
 * to the BARs.
 */
void
Device::allocate_pending_resources()
{
  allocate_pending_child_resources();
  Device *p = parent();

  if (!p)
    return;

  UAD to_allocate(res_cmp);

  _allocate_pending_resources(this, &to_allocate);

  for (UAD::const_iterator i = to_allocate.begin(); i != to_allocate.end(); ++i)
    p->alloc_child_resource((*i).r, this);
}

void
Device::allocate_pending_child_resources()
{
  UAD to_allocate(res_cmp);

  for (iterator dev = begin(0); dev != end(); ++dev)
    {
      dev->allocate_pending_child_resources();
      _allocate_pending_resources(*dev, &to_allocate);
    }

  for (UAD::const_iterator i = to_allocate.begin(); i != to_allocate.end(); ++i)
    alloc_child_resource((*i).r, (*i).d);
}


bool
Device::request_child_resource(Resource *r, Device *cld)
{
  bool found_as = false;
  // The first run requires exact match between client and parent resource.
  // The second run allows to assign a prefetchable MMIO client region to a
  // non-prefetchable MMIO parent resource.
  bool exact = true;
  auto const *rl = resources();
  while (true)
    {
      // scan through all our resources and try to find a
      // provided resource that is consumed by resource 'r'
      for (auto *br: *rl)
	{
          if (!br)
            continue;

	  if (br->disabled())
	    continue;

	  if (!br->provided())
	    continue;

	  if (!br->compatible(r, exact))
	    continue;

	  found_as = true;

	  if (br->provided()->request(br, this, r, cld))
	    return true;
	}

      if (exact)
	exact = false;
      else if (!found_as && parent())
	{
	  // If there is no proper resource provided by
	  // ourselves that fits resource r, than try to
	  // go up the hierarchy to our parent node
          if (0)
            {
              printf("try parent resource request...\n");
              r->dump();
            }
	  return parent()->request_child_resource(r, cld);
	}
      else
	return false;
    }
}

int
Generic_device::register_property(std::string const &n, Property *prop)
{
  Property *&p = _properties[n];
  if (p)
    {
      d_printf(DBG_ERR, "internal error: %s:"
                        "property %s is already registered\n", name(), n.c_str());
      return -EEXIST;
    }

  p = prop;

  return 0;
}

Property *
Generic_device::property(std::string const &name)
{
  return _properties[name];
}

