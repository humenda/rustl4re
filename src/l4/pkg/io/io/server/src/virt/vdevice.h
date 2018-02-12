/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#pragma once

#include <l4/sys/types.h>
#include <l4/cxx/avl_set>
#include <string>
#include <vector>
#include <l4/cxx/ipc_stream>
#include <l4/vbus/vbus_types.h>

#include "device.h"
#include "irqs.h"
#include "event_source.h"
#include <cassert>

namespace Hw {
class Dev_feature;
}

class Resource;

namespace Vi {

class Device;

class Dev_feature
{
public:
  virtual ~Dev_feature() = 0;
  virtual bool match_hw_feature(Hw::Dev_feature const *) const = 0;
  virtual int dispatch(l4_umword_t obj, l4_uint32_t func, L4::Ipc::Iostream &ios) = 0;
  virtual Device *host() const = 0;
  virtual void set_host(Device *d) = 0;
  virtual l4_uint32_t interface_type() const = 0;
};

inline Dev_feature::~Dev_feature() {}


class Device : public Generic_device, public Device_tree_mixin<Device>
{
public:
  typedef Device_tree_mixin<Device>::iterator iterator;
  using Device_tree_mixin<Device>::begin;
  using Device_tree_mixin<Device>::end;

  // dispatch helper for server object
  int vdevice_dispatch(l4_umword_t obj, l4_uint32_t func, L4::Ipc::Iostream &ios);

  typedef std::vector<Dev_feature*> Feature_list;

  Feature_list const *features() const { return &_features; }

  void add_feature(Dev_feature *f)
  {
    f->set_host(this);
    _features.push_back(f);
  }

  template< typename FT >
  FT *find_feature()
  {
    for (Feature_list::const_iterator i = _features.begin();
	 i != _features.end(); ++i)
      if (FT *r = dynamic_cast<FT*>(*i))
	return r;

    return 0;
  }


  virtual l4_uint32_t adr() const
  { return l4_uint32_t(~0); }

  virtual ~Device()
  { __devs.erase(l4vbus_device_handle_t(this)); }

  char const *name() const
  { return _name.c_str(); }

  bool name(cxx::String const &n)
  {
    _name = std::string(n.start(), n.end());
    return true;
  }

  bool resource_allocated(Resource const *) const;

  virtual int add_filter(cxx::String const &, cxx::String const &)
  { return -ENODEV; }

  virtual int add_filter(cxx::String const &, unsigned long long)
  { return -ENODEV; }

  virtual int add_filter(cxx::String const &, unsigned long long, unsigned long long)
  { return -ENODEV; }

  virtual void finalize_setup()
  {}

  Device *parent() const { return _dt.parent(); }
  Device *children() const { return _dt.children(); }
  Device *next() const { return _dt.next(); }
  int depth() const { return _dt.depth(); }

  virtual Io::Event_source_infos const *get_event_infos() const
  { return 0; }

  virtual bool dev_notify(Device const *, unsigned, unsigned, unsigned, bool)
  { assert (false); return false; }

  bool notify(unsigned type, unsigned event, unsigned value, bool syn)
  { return get_root()->dev_notify(this, type, event, value, syn); }

  virtual Io_irq_pin::Msi_src *find_msi_src(Msi_src_info si);

  Device() : _name("(noname)")
  { __devs.insert(l4vbus_device_handle_t(this)); }

  void dump(int indent) const;

protected:
  // helper functions
  int get_by_hid(L4::Ipc::Iostream &ios);
  int vbus_get_device(L4::Ipc::Iostream &ios);
  Device *get_dev_by_id(l4vbus_device_handle_t id);

  Device *get_root()
  {
    Device *d;
    for (d = this; d->parent(); d = d->parent())
      ;
    return d;
  }

  std::string _name;

  typedef cxx::Avl_set<l4vbus_device_handle_t> Dev_set;
  static Dev_set __devs;

private:
  Feature_list _features;
};

}
